//-----------------------------------------------------------------------------------
//
// Bitfighter - A multiplayer vector graphics space game
// Based on Zap demo released for Torque Network Library by GarageGames.com
//
// Derivative work copyright (C) 2008-2009 Chris Eykamp
// Original work copyright (C) 2004 GarageGames.com, Inc.
// Other code copyright as noted
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful (and fun!),
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//------------------------------------------------------------------------------------

#include "EventManager.h"
#include "playerInfo.h"          // For RobotPlayerInfo constructor
#include "robot.h"

//#include "../lua/luaprofiler-2.0.2/src/luaprofiler.h"      // For... the profiler!
#include "oglconsole.h"

#ifndef ZAP_DEDICATED
#  include "UI.h"
#endif

#include <math.h>


#define hypot _hypot    // Kill some warnings

namespace Zap
{

// Statics:
bool EventManager::anyPending = false; 
Vector<const char *> EventManager::subscriptions[EventTypes];
Vector<const char *> EventManager::pendingSubscriptions[EventTypes];
Vector<const char *> EventManager::pendingUnsubscriptions[EventTypes];
bool EventManager::mConstructed = false;  // Prevent duplicate instantiation


struct EventDef {
   const char *name;
   const char *function;
};


static const EventDef eventDefs[] = {
   // Name           // Lua function
   { "Tick",         "onTick" },
   { "ShipSpawned",  "onShipSpawned" },
   { "ShipKilled",   "onShipKilled" },
   { "PlayerJoined", "onPlayerJoined" },
   { "PlayerLeft",   "onPlayerLeft" },
   { "MsgReceived",  "onMsgReceived" },
   { "NexusOpened",  "onNexusOpened" },
   { "NexusClosed"   "onNexusClosed" }
};


static EventManager *eventManager = NULL;   // Singleton event manager, one copy is used by all listeners


// C++ constructor
EventManager::EventManager()
{
   TNLAssert(!mConstructed, "There is only one EventManager to rule them all!");

   mIsPaused = false;
   mStepCount = -1;
   mConstructed = true;
}


// Lua constructor
EventManager::EventManager(lua_State *L)
{
   TNLAssert(false, "Should never be called!");
}


void EventManager::shutdown()
{
   if(eventManager)
   {
      delete eventManager;
      eventManager = NULL;
   }
}


// Provide access to the single EventManager instance; lazily initialized
EventManager *EventManager::get()
{
   if(!eventManager)
      eventManager = new EventManager();      // Deleted in cleanup, which is called from Game destuctor

   return eventManager;
}


void EventManager::subscribe(const char *subscriber, EventType eventType)
{
   // First, see if we're already subscribed
   if(isSubscribed(subscriber, eventType) || isPendingSubscribed(subscriber, eventType))
      return;

   lua_State *L = LuaScriptRunner::getL();
   TNLAssert(lua_gettop(L) == 0 || LuaObject::dumpStack(L), "Stack dirty!");

   // Make sure the script has the proper event listener
   LuaScriptRunner::loadFunction(L, subscriber, eventDefs[eventType].function);     // -- function

   if(!lua_isfunction(L, -1))
   {
      logprintf(LogConsumer::LogError, "Error subscribing to %s event: couldn't find handler function.  Unsubscribing.", eventDefs[eventType].name);
      LuaObject::clearStack(L);

      return;
   }

   removeFromPendingUnsubscribeList(subscriber, eventType);
   pendingSubscriptions[eventType].push_back(subscriber);
   anyPending = true;

   lua_pop(L, 1);    // Remove function from stack                                  -- <<empty stack>>

   TNLAssert(lua_gettop(L) == 0 || LuaObject::dumpStack(L), "Stack not cleared!");
}


void EventManager::unsubscribe(const char *subscriber, EventType eventType)
{
   if((isSubscribed(subscriber, eventType) || isPendingSubscribed(subscriber, eventType)) && !isPendingUnsubscribed(subscriber, eventType))
   {
      removeFromPendingSubscribeList(subscriber, eventType);
      pendingUnsubscriptions[eventType].push_back(subscriber);
      anyPending = true;
   }
}


void EventManager::removeFromPendingSubscribeList(const char *subscriber, EventType eventType)
{
   for(S32 i = 0; i < pendingSubscriptions[eventType].size(); i++)
      if(strcmp(pendingSubscriptions[eventType][i], subscriber) == 0)
      {
         pendingSubscriptions[eventType].erase_fast(i);
         return;
      }
}


void EventManager::removeFromPendingUnsubscribeList(const char *subscriber, EventType eventType)
{
   for(S32 i = 0; i < pendingUnsubscriptions[eventType].size(); i++)
      if(strcmp(pendingUnsubscriptions[eventType][i], subscriber) == 0)
      {
         pendingUnsubscriptions[eventType].erase_fast(i);
         return;
      }
}


void EventManager::removeFromSubscribedList(const char *subscriber, EventType eventType)
{
   for(S32 i = 0; i < subscriptions[eventType].size(); i++)
      if(strcmp(subscriptions[eventType][i], subscriber) == 0)
      {
         subscriptions[eventType].erase_fast(i);
         return;
      }
}


// Unsubscribe an event bypassing the pending unsubscribe queue, when we know it will be OK
void EventManager::unsubscribeImmediate(const char *subscriber, EventType eventType)
{
   removeFromSubscribedList(subscriber, eventType);
   removeFromPendingSubscribeList(subscriber, eventType);
   removeFromPendingUnsubscribeList(subscriber, eventType);    // Probably not really necessary...
}


// Check if we're subscribed to an event
bool EventManager::isSubscribed(const char *subscriber, EventType eventType)
{
   for(S32 i = 0; i < subscriptions[eventType].size(); i++)
      if(strcmp(subscriptions[eventType][i], subscriber) == 0)
         return true;

   return false;
}


bool EventManager::isPendingSubscribed(const char *subscriber, EventType eventType)
{
   for(S32 i = 0; i < pendingSubscriptions[eventType].size(); i++)
      if(strcmp(pendingSubscriptions[eventType][i], subscriber) == 0)
         return true;

   return false;
}


bool EventManager::isPendingUnsubscribed(const char *subscriber, EventType eventType)
{
   for(S32 i = 0; i < pendingUnsubscriptions[eventType].size(); i++)
      if(strcmp(pendingUnsubscriptions[eventType][i], subscriber) == 0)
         return true;

   return false;
}


// Process all pending subscriptions and unsubscriptions
void EventManager::update()
{
   if(anyPending)
   {
      for(S32 i = 0; i < EventTypes; i++)
         for(S32 j = 0; j < pendingUnsubscriptions[i].size(); j++)     // Unsubscribing first means less searching!
            removeFromSubscribedList(pendingUnsubscriptions[i][j], (EventType) i);

      for(S32 i = 0; i < EventTypes; i++)
         for(S32 j = 0; j < pendingSubscriptions[i].size(); j++)     
            subscriptions[i].push_back(pendingSubscriptions[i][j]);

      for(S32 i = 0; i < EventTypes; i++)
      {
         pendingSubscriptions[i].clear();
         pendingUnsubscriptions[i].clear();
      }
      anyPending = false;
   }
}


// onNexusOpened, onNexusClosed
void EventManager::fireEvent(EventType eventType)
{
   if(suppressEvents(eventType))   
      return;

   lua_State *L = LuaScriptRunner::getL();

   for(S32 i = 0; i < subscriptions[eventType].size(); i++)
   {
      try
      {
         LuaScriptRunner::loadFunction(L, subscriptions[eventType][i], eventDefs[eventType].function);

         if(lua_pcall(L, 0, 0, 0) != 0)
            throw LuaException(lua_tostring(L, -1));
      }
      catch(LuaException &e)
      {
         handleEventFiringError(subscriptions[eventType][i], eventType, e.what());
         return;
      }
   }
}


// onTick
void EventManager::fireEvent(EventType eventType, U32 deltaT)
{
   if(suppressEvents(eventType))   
      return;

   if(eventType == TickEvent)
      mStepCount--;   

   lua_State *L = LuaScriptRunner::getL();

   for(S32 i = 0; i < subscriptions[eventType].size(); i++)
   {
      try   
      {
         LuaScriptRunner::loadFunction(L, subscriptions[eventType][i], eventDefs[eventType].function);
         lua_pushinteger(L, deltaT);

         if(lua_pcall(L, 1, 0, 0) != 0)
            throw LuaException(lua_tostring(L, -1));
      }
      catch(LuaException &e)
      {
         LuaObject::clearStack(L);
         handleEventFiringError(subscriptions[eventType][i], eventType, e.what());
         return;
      }
   }
}


// onShipSpawned, onShipKilled
void EventManager::fireEvent(EventType eventType, Ship *ship)
{
   if(suppressEvents(eventType))   
      return;

   lua_State *L = LuaScriptRunner::getL();

   for(S32 i = 0; i < subscriptions[eventType].size(); i++)
   {
      try
      {
         LuaScriptRunner::loadFunction(L, subscriptions[eventType][i], eventDefs[eventType].function);
         ship->push(L);

         if(lua_pcall(L, 1, 0, 0) != 0)
            throw LuaException(lua_tostring(L, -1));
      }
      catch(LuaException &e)
      {
         handleEventFiringError(subscriptions[eventType][i], eventType, e.what());
         return;
      }
   }
}


// Note that player can be NULL, in which case we'll pass nil to the listeners
// callerId will be NULL when player sends message
void EventManager::fireEvent(const char *callerId, EventType eventType, const char *message, LuaPlayerInfo *player, bool global)
{
   if(suppressEvents(eventType))   
      return;

   lua_State *L = LuaScriptRunner::getL();

   for(S32 i = 0; i < subscriptions[eventType].size(); i++)
   {
      if(callerId && strcmp(callerId, subscriptions[eventType][i]) == 0)    // Don't alert bot about own message!
         continue;

      try
      {
         LuaScriptRunner::loadFunction(L, subscriptions[eventType][i], eventDefs[eventType].function);
         lua_pushstring(L, message);

         if(player)
            player->push(L);
         else
            lua_pushnil(L);

         lua_pushboolean(L, global);

         if(lua_pcall(L, 3, 0, 0) != 0)
            throw LuaException(lua_tostring(L, -1));
      }
      catch(LuaException &e)
      {
         handleEventFiringError(subscriptions[eventType][i], eventType, e.what());
         return;
      }
   }
}


// onPlayerJoined, onPlayerLeft
void EventManager::fireEvent(const char *callerId, EventType eventType, LuaPlayerInfo *player)
{
   if(suppressEvents(eventType))   
      return;

   lua_State *L = LuaScriptRunner::getL();

   for(S32 i = 0; i < subscriptions[eventType].size(); i++)
   {
      if(strcmp(callerId, subscriptions[eventType][i]) == 0)    // Don't trouble bot with own joinage or leavage!
         continue;

      try   
      {
         LuaScriptRunner::loadFunction(L, subscriptions[eventType][i], eventDefs[eventType].function);
         player->push(L);

         if(lua_pcall(L, 1, 0, 0) != 0)
            throw LuaException(lua_tostring(L, -1));
      }
      catch(LuaException &e)
      {
         handleEventFiringError(subscriptions[eventType][i], eventType, e.what());
         return;
      }
   }
}


void EventManager::handleEventFiringError(const char *subscriber, EventType eventType, const char *errorMsg)
{
   // Figure out which, if any, bot caused the error
   Robot *robot = Robot::findBot(subscriber);

   if(robot)
   {
      robot->logError("Robot error handling event %s: %s. Shutting bot down.", eventDefs[eventType].name, errorMsg);
      delete robot;
   }
   else
      logprintf(LogConsumer::LogError, "Error firing event %s: %s", eventDefs[eventType].name, errorMsg);
}


// If true, events will not fire!
bool EventManager::suppressEvents(EventType eventType)
{
   if(subscriptions[eventType].size() == 0)
      return true;

   return mIsPaused && mStepCount <= 0;    // Paused bots should still respond to events as long as stepCount > 0
}


void EventManager::setPaused(bool isPaused)
{
   mIsPaused = isPaused;
}


void EventManager::togglePauseStatus()
{
   mIsPaused = !mIsPaused;
}


bool EventManager::isPaused()
{
   return mIsPaused;
}


// Each firing of TickEvent is considered a step
void EventManager::addSteps(S32 steps)
{
   if(mIsPaused)           // Don't add steps if not paused to avoid hitting pause and having bot still run a few steps
      mStepCount = steps;     
}


};
