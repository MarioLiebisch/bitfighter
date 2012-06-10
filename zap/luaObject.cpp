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

#include "luaObject.h"
#include "luaUtil.h"
#include "luaGameInfo.h"
#include "tnlLog.h"           // For logprintf

#include "item.h"             // For getItem()
#include "flagItem.h"         // For getItem()
#include "robot.h"            // For getItem()
#include "NexusGame.h"        // For getItem()
#include "soccerGame.h"       // For getItem()
#include "projectile.h"       // For getItem()
#include "loadoutZone.h"
#include "goalZone.h"
#include "teleporter.h"
#include "speedZone.h"
#include "EngineeredItem.h"   // For getItem()
#include "PickupItem.h"       // For getItem()
#include "playerInfo.h"       // For playerInfo def
#include "UIMenuItems.h"      // For MenuItem def
#include "config.h"
#include "CoreGame.h"         // For getItem()
#include "ClientInfo.h"

#include "stringUtils.h"      // For joindir  
#include "lua.h"


namespace Zap
{

// Constructor
LuaObject::LuaObject()
{
   // Do nothing
}

// Destructor
LuaObject::~LuaObject()
{
   // Do nothing
}


bool LuaObject::shouldLuaGarbageCollectThisObject()
{
   return true;
}


// Returns a point to calling Lua function
S32 LuaObject::returnPoint(lua_State *L, const Point &point)
{
   return returnVec(L, point.x, point.y);
}


// Returns an existing LuaPoint to calling Lua function XXX not used?
template<class T>
S32 LuaObject::returnVal(lua_State *L, T value, bool letLuaDelete)
{
   Lunar<MenuItem>::push(L, value, letLuaDelete);     // true will allow Lua to delete this object when it goes out of scope
   return 1;
}


// Returns an int to a calling Lua function
S32 LuaObject::returnInt(lua_State *L, S32 num)
{
   lua_pushinteger(L, num);
   return 1;
}


// If we have a ship, return it, otherwise return nil
S32 LuaObject::returnShip(lua_State *L, Ship *ship)
{
   if(ship)
   {
      ship->push(L);
      return 1;
   }

   return returnNil(L);
}


S32 LuaObject::returnPlayerInfo(lua_State *L, Ship *ship)
{
   return returnPlayerInfo(L, ship->getClientInfo()->getPlayerInfo());
}


S32 LuaObject::returnPlayerInfo(lua_State *L, LuaPlayerInfo *playerInfo)
{
   playerInfo->push(L);
   return 1;
}


// Returns a vector to a calling Lua function
S32 LuaObject::returnVec(lua_State *L, F32 x, F32 y)
{
   lua_pushvec(L, x, y);
   return 1;
}


// Returns a float to a calling Lua function
S32 LuaObject::returnFloat(lua_State *L, F32 num)
{
   lua_pushnumber(L, num);
   return 1;
}


// Returns a boolean to a calling Lua function
S32 LuaObject::returnBool(lua_State *L, bool boolean)
{
   lua_pushboolean(L, boolean);
   return 1;
}


// Returns a string to a calling Lua function
S32 LuaObject::returnString(lua_State *L, const char *str)
{
   lua_pushstring(L, str);
   return 1;
}


// Returns nil to calling Lua function
S32 LuaObject::returnNil(lua_State *L)
{
   lua_pushnil(L);
   return 1;
}


void LuaObject::clearStack(lua_State *L)
{
   lua_settop(L, 0);
}


// Assume that table is at the top of the stack
void LuaObject::setfield (lua_State *L, const char *key, F32 value)
{
   lua_pushnumber(L, value);
   lua_setfield(L, -2, key);
}


// Make sure we got the number of args we wanted
void LuaObject::checkArgCount(lua_State *L, S32 argsWanted, const char *methodName)
{
   S32 args = lua_gettop(L);

   if(args != argsWanted)     // Problem!
   {
      char msg[256];
      dSprintf(msg, sizeof(msg), "%s called with %d args, expected %d", methodName, args, argsWanted);
      logprintf(LogConsumer::LogError, msg);

      throw LuaException(msg);
   }
}


// Pop integer off stack, check its type, do bounds checking, and return it
lua_Integer LuaObject::getInt(lua_State *L, S32 index, const char *methodName, S32 minVal, S32 maxVal)
{
   lua_Integer val = getInt(L, index, methodName);

   if(val < minVal || val > maxVal)
   {
      char msg[256];
      dSprintf(msg, sizeof(msg), "%s called with out-of-bounds arg: %d (val=%d)", methodName, index, val);
      logprintf(LogConsumer::LogError, msg);

      throw LuaException(msg);
   }

   return val;
}


// Returns defaultVal if there is an invalid or missing value on the stack
lua_Integer LuaObject::getInt(lua_State *L, S32 index, const char *methodName, S32 defaultVal)
{
   if(!lua_isnumber(L, index))
      return defaultVal;
   // else
   return lua_tointeger(L, index);
}


// Pop integer off stack, check its type, and return it (no bounds check)
lua_Integer LuaObject::getInt(lua_State *L, S32 index, const char *methodName)
{
   if(!lua_isnumber(L, index))
   {
      char msg[256];
      dSprintf(msg, sizeof(msg), "%s expected numeric arg at position %d", methodName, index);
      logprintf(LogConsumer::LogError, msg);

      throw LuaException(msg);
   }

   return lua_tointeger(L, index);
}


// Pop a number off stack, convert to float, and return it (no bounds check)
F32 LuaObject::getFloat(lua_State *L, S32 index, const char *methodName)
{
   if(!lua_isnumber(L, index))
   {
      char msg[256];
      dSprintf(msg, sizeof(msg), "%s expected numeric arg at position %d", methodName, index);
      logprintf(LogConsumer::LogError, msg);

      throw LuaException(msg);
   }

   return (F32) lua_tonumber(L, index);
}


// Pop a boolean off stack, and return it
bool LuaObject::getBool(lua_State *L, S32 index, const char *methodName)
{
   if(!lua_isboolean(L, index))
   {
      char msg[256];
      dSprintf(msg, sizeof(msg), "%s expected boolean arg at position %d", methodName, index);
      logprintf(LogConsumer::LogError, msg);

      throw LuaException(msg);
   }

   return (bool) lua_toboolean(L, index);
}


// Pop a boolean off stack, and return it
bool LuaObject::getBool(lua_State *L, S32 index, const char *methodName, bool defaultVal)
{
   if(!lua_isboolean(L, index))
      return defaultVal;
   // else
   return (bool) lua_toboolean(L, index);
}


// Pop a string or string-like object off stack, check its type, and return it
const char *LuaObject::getString(lua_State *L, S32 index, const char *methodName, const char *defaultVal)
{
   if(!lua_isstring(L, index))
      return defaultVal;
   // else
   return lua_tostring(L, index);
}


// Pop a string or string-like object off stack, check its type, and return it
const char *LuaObject::getString(lua_State *L, S32 index, const char *methodName)
{
   if(!lua_isstring(L, index))
   {
      char msg[256];
      dSprintf(msg, sizeof(msg), "%s expected string arg at position %d", methodName, index);
      logprintf(LogConsumer::LogError, msg);

      throw LuaException(msg);
   }

   return lua_tostring(L, index);
}


MenuItem *LuaObject::pushMenuItem (lua_State *L, MenuItem *menuItem)
{
  MenuItem *menuItemUserData = (MenuItem *)lua_newuserdata(L, sizeof(MenuItem));

  *menuItemUserData = *menuItem;
  luaL_getmetatable(L, "MenuItem");
  lua_setmetatable(L, -2);

  return menuItemUserData;
}


// Pulls values out of the table at specified index as strings, and puts them all into strings vector
void LuaObject::getStringVectorFromTable(lua_State *L, S32 index, const char *methodName, Vector<string> &strings)
{
   strings.clear();

   if(!lua_istable(L, index))
   {
      char msg[256];
      dSprintf(msg, sizeof(msg), "%s expected table arg (which I wanted to convert to a string vector) at position %d", methodName, index);
      logprintf(LogConsumer::LogError, msg);

      throw LuaException(msg);
   }

   // The following block loosely based on http://www.gamedev.net/topic/392970-lua-table-iteration-in-c---basic-walkthrough/

   lua_pushvalue(L, index);	// Push our table onto the top of the stack
   lua_pushnil(L);            // lua_next (below) will start the iteration, it needs nil to be the first key it pops

   // The table was pushed onto the stack at -1 (recall that -1 is equivalent to lua_gettop)
   // The lua_pushnil then pushed the table to -2, where it is currently located
   while(lua_next(L, -2))     // -2 is our table
   {
      // Grab the value at the top of the stack
      if(!lua_isstring(L, -1))
      {
         char msg[256];
         dSprintf(msg, sizeof(msg), "%s expected a table of strings -- invalid value at stack position %d, table element %d", 
                                    methodName, index, strings.size() + 1);
         logprintf(LogConsumer::LogError, msg);

         throw LuaException(msg);
      }

      strings.push_back(lua_tostring(L, -1));

      lua_pop(L, 1);    // We extracted that value, pop it off so we can push the next element
   }

   // We've got all the elements in the table, so clear it off the stack
   lua_pop(L, 1);
}

/* // not used?
static ToggleMenuItem *getMenuItem(lua_State *L, S32 index)
{
  ToggleMenuItem *pushedMenuItem;

  luaL_checktype(L, index, LUA_TUSERDATA);      // Confirm the item at index is a full userdata
  pushedMenuItem = (ToggleMenuItem *)luaL_checkudata(L, index, "ToggleMenuItem");
  if(pushedMenuItem == NULL)
     luaL_typerror(L, index, "ToggleMenuItem");

  //MenuItem im = *pushedMenuItem;
  //if(!pushedMenuItem)
  //  luaL_error(L, "null menuItem");

  return pushedMenuItem;
}
*/

// Pulls values out of the table at specified, verifies that they are MenuItems, and adds them to the menuItems vector
bool LuaObject::getMenuItemVectorFromTable(lua_State *L, S32 index, const char *methodName, Vector<MenuItem *> &menuItems)
{
#ifdef ZAP_DEDICATED
      throw LuaException("Dedicated server should not use MenuItem");
#else
   if(!lua_istable(L, index))
   {
      char msg[256];
      dSprintf(msg, sizeof(msg), "%s expected table arg (which I wanted to convert to a menuItem vector) at position %d", methodName, index);
      logprintf(LogConsumer::LogError, msg);

      throw LuaException(msg);
   }

   // The following block (very) loosely based on http://www.gamedev.net/topic/392970-lua-table-iteration-in-c---basic-walkthrough/

   lua_pushvalue(L, index);	// Push our table onto the top of the stack                                               -- table table
   lua_pushnil(L);            // lua_next (below) will start the iteration, it needs nil to be the first key it pops    -- table table nil

   // The table was pushed onto the stack at -1 (recall that -1 is equivalent to lua_gettop)
   // The lua_pushnil then pushed the table to -2, where it is currently located
   while(lua_next(L, -2))     // -2 is our table
   {
      UserData *ud = static_cast<UserData *>(lua_touserdata(L, -1));

      if(!ud)                // Weeds out simple values, wrong userdata types still pass here
      {
         char msg[1024];
         dSprintf(msg, sizeof(msg), "%s expected a MenuItem at position %d", methodName, menuItems.size() + 1);

         throw LuaException(msg);
      }

      // We have a userdata
      LuaObject *obj = ud->objectPtr;                       // Extract the pointer
      MenuItem *menuItem = dynamic_cast<MenuItem *>(obj);   // Cast it to a MenuItem

      if(!menuItem)                                         // Cast failed -- not a MenuItem... we got some bad args
      {
         // TODO: This does not report a line number, for some reason...
         // Reproduce with code like this in a plugin
         //function getArgs()
         //   local items = { }  -- Create an empty table to hold our menu items
         //   
         //   -- Create the menu items we need for this script, adding them to our items table
         //   table.insert(items, ToggleMenuItem:new("Run mode:", { "One", "Two", "Mulitple" }, 1, false, "Specify run mode" ))
         //   table.insert(items, Point:new(1,2))
         //
         //   return "Menu title", items
         //end

         char msg[256];
         dSprintf(msg, sizeof(msg), "%s expected a MenuItem at position %d", methodName, menuItems.size() + 1);
         logprintf(LogConsumer::LogError, msg);

         throw LuaException(msg);
      }

      menuItems.push_back(menuItem);                        // Add the MenuItem to our list
      lua_pop(L, 1);                                        // We extracted that value, pop it off so we can push the next element
   }

   // We've got all the elements in the table, so clear it off the stack
   lua_pop(L, 1);

#endif
   return true;
}


// Pop a vec object off stack, check its type, and return it
Point LuaObject::getVec(lua_State *L, S32 index, const char *methodName)
{
   if(!lua_isvec(L, index))
   {
      char msg[256];
      dSprintf(msg, sizeof(msg), "%s expected vector arg at position %d", methodName, index);
      logprintf(LogConsumer::LogError, msg);

      throw LuaException(msg);
   }

   const F32 *fff = lua_tovec(L, index);
   return Point(fff[0], fff[1]);
}


// Pop a point object off stack, or grab two numbers and create a point from that
Point LuaObject::getPointOrXY(lua_State *L, S32 index, const char *methodName)
{
   S32 args = lua_gettop(L);
   if(args == 1)
   {
      return getVec(L, index, methodName);
   }
   else if(args == 2)
   {
      F32 x = getFloat(L, index, methodName);
      F32 y = getFloat(L, index + 1, methodName);
      return Point(x, y);
   }

   // Uh oh...
   char msg[256];
   dSprintf(msg, sizeof(msg), "%s expected either a point or a pair of numbers at position %d", methodName, index);
   logprintf(LogConsumer::LogError, msg);

   throw LuaException(msg);
}


// Make a nice looking string representation of the object at the specified index
static string stringify(lua_State *L, S32 index)
{
   int t = lua_type(L, index);
   //TNLAssert(t >= -1 && t <= LUA_TTHREAD, "Invalid type number!");
   if(t > LUA_TTHREAD || t < -1)
      return "Invalid object type id " + itos(t);

   switch (t) 
   {
      case LUA_TSTRING:   
         return "string: " + string(lua_tostring(L, index));
      case LUA_TBOOLEAN:  
         return "boolean: " + lua_toboolean(L, index) ? "true" : "false";
      case LUA_TNUMBER:    
         return "number: " + itos(S32(lua_tonumber(L, index)));
      default:             
         return lua_typename(L, t);
   }
}


// May interrupt a table traversal if this is called in the middle
void LuaObject::dumpTable(lua_State *L, S32 tableIndex, const char *msg)
{
   bool hasMsg = (strcmp(msg, "") != 0);
   logprintf("Dumping table at index %d %s%s%s", tableIndex, hasMsg ? "[" : "", msg, hasMsg ? "]" : "");

   TNLAssert(lua_type(L, tableIndex) == LUA_TTABLE || dumpStack(L), "No table at specified index!");

   // Compensate for other stuff we'll be putting on the stack
   if(tableIndex < 0)
      tableIndex -= 1;
                                                            // -- ... table  <=== arrive with table and other junk (perhaps) on the stack
   lua_pushnil(L);      // First key                        // -- ... table nil
   while(lua_next(L, tableIndex) != 0)                      // -- ... table nextkey table[nextkey]      
   {
      string key = stringify(L, -2);                  
      string val = stringify(L, -1);                  

      logprintf("%s - %s", key.c_str(), val.c_str());        
      lua_pop(L, 1);                                        // -- ... table key (Pop value; keep key for next iter.)
   }
}


bool LuaObject::dumpStack(lua_State* L, const char *msg)
{
    int top = lua_gettop(L);

    bool hasMsg = (strcmp(msg, "") != 0);
    logprintf("\nTotal in stack: %d %s%s%s", top, hasMsg ? "[" : "", msg, hasMsg ? "]" : "");

    for(S32 i = 1; i <= top; i++)
    {
      string val = stringify(L, i);
      logprintf("%d : %s", i, val.c_str());
    }

    return false;
 }


////////////////////////////////////////
////////////////////////////////////////

// Declare and Initialize statics:
lua_State *LuaScriptRunner::L = NULL;
bool  LuaScriptRunner::mScriptingDirSet = false;
string LuaScriptRunner::mScriptingDir;

deque<string> LuaScriptRunner::mCachedScripts;

void LuaScriptRunner::clearScriptCache()
{
	while(mCachedScripts.size() != 0)
	{
		deleteScript(mCachedScripts.front().c_str());
		mCachedScripts.pop_front();
	}
}


// Constructor
LuaScriptRunner::LuaScriptRunner()
{
   static U32 mNextScriptId = 0;

   // Initialize all subscriptions to unsubscribed -- bits will automatically subscribe to onTick later
   for(S32 i = 0; i < EventManager::EventTypes; i++)
      mSubscriptions[i] = false;

   mScriptId = "script" + itos(mNextScriptId++);
}


// Destructor
LuaScriptRunner::~LuaScriptRunner()
{
   // Make sure we're unsubscribed to all those events we subscribed to.  Don't want to
   // send an event to a dead bot, after all...
   for(S32 i = 0; i < EventManager::EventTypes; i++)
      if(mSubscriptions[i])
         EventManager::get()->unsubscribeImmediate(getScriptId(), (EventManager::EventType)i);

   // And delete the script's environment table from the Lua instance
   deleteScript(getScriptId());
}


void LuaScriptRunner::setScriptingDir(const string &scriptingDir)
{
   mScriptingDir = scriptingDir;
   mScriptingDirSet = true;
}


lua_State *LuaScriptRunner::getL()
{
   TNLAssert(L, "L not yet instantiated!");
   return L;
}


void LuaScriptRunner::shutdown()
{
   if(L)
   {
      lua_close(L);
      L = NULL;
   }
}


const char *LuaScriptRunner::getScriptId()
{
   return mScriptId.c_str();
}


// Sets the environment for the function on the top of the stack to that associated with name
// Starts with a function on the stack
void LuaScriptRunner::setEnvironment()
{                                    
   // Grab the script's environment table from the registry, place it on the stack
   lua_getfield(L, LUA_REGISTRYINDEX, getScriptId());    // Push REGISTRY[scriptId] onto stack           -- function, table
   lua_setfenv(L, -2);                                   // Set that table to be the env for function    -- function
}


// Retrieve the environment from the registry, and put the requested function from that environment onto the stack
void LuaScriptRunner::loadFunction(lua_State *L, const char *scriptId, const char *functionName)
{
   try
   {
      lua_getfield(L, LUA_REGISTRYINDEX, scriptId);   // Push REGISTRY[scriptId] onto the stack                -- table
      lua_getfield(L, -1, functionName);              // And get the requested function from the environment   -- table, function

      lua_remove(L, -2);                              // Remove table                                          -- function
   }

   catch(LuaException &e)
   {
      // TODO: Should be logError()!
      logprintf(LogConsumer::LogError, "Error accessing %s function: %s.  Aborting script.", functionName, e.what());
      LuaObject::clearStack(L);
   }
}


bool LuaScriptRunner::loadAndRunGlobalFunction(lua_State *L, const char *key)
{
   lua_getfield(L, LUA_REGISTRYINDEX, key);     // Get function out of the registry      -- functionName()
   setEnvironment();                            // Set the environment for the code
   S32 err = lua_pcall(L, 0, 0, 0);             // Run it                                 -- <<empty stack>>

   if(err != 0)
   {
      logError("Failed to load startup functions %s: %s", key, lua_tostring(L, -1));

      lua_pop(L, 1);             // Remove error message from stack
      TNLAssert(lua_gettop(L) == 0 || LuaObject::dumpStack(L), "Stack not cleared!");

      return false;
   }

   return true;
}



// Loads script from file into a Lua chunk, then runs it.  This has the effect of loading all our functions into the local environment,
// defining any globals, and executing any "loose" code not defined in a function.
bool LuaScriptRunner::loadScript()
{
   static const S32 MAX_CACHE_SIZE = 2;      // For now -- can be bigger when we know this works

#ifdef ZAP_DEDICATED
   bool cacheScripts = true;
#else
   bool cacheScripts = gServerGame && !gServerGame->isTestServer();
#endif

   TNLAssert(lua_gettop(L) == 0 || LuaObject::dumpStack(L), "Stack dirty!");

   if(!cacheScripts)
      loadCompileScript(mScriptName.c_str());
   else  
   {
      bool found = false;

      // Check if script is in our cache
      S32 cacheSize = (S32)mCachedScripts.size();

      for(S32 i = 0; i < cacheSize; i++)
         if(mCachedScripts[i] == mScriptName)
         {
            found = true;
            break;
         }

      if(!found)     // Script is not (yet) cached
      {
         if(cacheSize > MAX_CACHE_SIZE)
         {
            // Remove oldest script from the cache
            deleteScript(mCachedScripts.front().c_str());
            mCachedScripts.pop_front();
         }

         // Load new script into cache using full name as registry key
         loadCompileSaveScript(mScriptName.c_str(), mScriptName.c_str());
         mCachedScripts.push_back(mScriptName);
      }

      lua_getfield(L, LUA_REGISTRYINDEX, mScriptName.c_str());    // Load script from cache
   }

   if(lua_gettop(L) == 0)     // Script compile error?
   {
      logError("Error compiling script -- aborting.");
      return false;
   }

   // So, however we got here, the script we want to run is now sitting on top of the stack
   TNLAssert((lua_gettop(L) == 1 && lua_isfunction(L, 1)) || LuaObject::dumpStack(L), "Expected a single function on the stack!");

   setEnvironment();

   S32 error = lua_pcall(L, 0, 0, 0);     // Passing 0 args, expecting none back

   if(!error)
   {
      TNLAssert(lua_gettop(L) == 0 || LuaObject::dumpStack(L), "Stack not cleared!");
      return true;
   }

   logError("%s -- Aborting.", lua_tostring(L, -1));

   lua_pop(L, 1);       // Remove error message from stack

   TNLAssert(lua_gettop(L) == 0 || LuaObject::dumpStack(L), "Stack not cleared!");
   return false;
}


// Don't forget to update the eventManager after running a robot's main function!
// return false if failed
bool LuaScriptRunner::runMain()
{
   return runMain(mScriptArgs);
}


bool LuaScriptRunner::runMain(const Vector<string> &args)
{
   TNLAssert(lua_gettop(L) == 0 || LuaObject::dumpStack(L), "Stack dirty!");
   try 
   {
      // Retrieve the bot's getName function, and put it on top of the stack
      bool ok = retrieveFunction("_main");     

      TNLAssert(ok, "_main function not found -- is lua_helper_functions corrupt?");

      if(!ok)
      {      
         const char *msg = "Function _main() could not be found! Your scripting environment appears corrupted.  Consider reinstalling Bitfighter.";
         logError(msg);
         throw LuaException(msg);
      }

      setLuaArgs(args);

      S32 error = lua_pcall(L, 0, 0, 0);     // Passing no args, expecting none back

      if(error != 0)
         throw LuaException(lua_tostring(L, -1));

      TNLAssert(lua_gettop(L) == 0 || LuaObject::dumpStack(L), "Stack not cleared!");
   }

   catch(LuaException &e)
   {
      logError("Error encountered while attempting to run script's main() function: %s.  Aborting script.", e.what());
      LuaObject::clearStack(L);
      return false;
   }

   return true;
}


// Get a function from the currently running script, and place it on top of the stack.  Returns true if it works, false
// if the specified function could not be found.  If this fails will remove the non-function from the stack.
bool LuaScriptRunner::retrieveFunction(const char *functionName)
{
   lua_getfield(L, LUA_REGISTRYINDEX, getScriptId());    // Put script's environment table onto stack  -- table
   lua_pushstring(L, functionName);                      //                                            -- table, functionName
   lua_gettable(L, -2);                                  // Push requested function onto the stack     -- table, fn
   lua_remove(L, lua_gettop(L) - 1);                     // Remove environment table from stack        -- fn

   // Check if the top stack item is indeed a function (as we would expect)
   if(lua_isfunction(L, -1))
      return true;      // If so, return true

   // else
   lua_pop(L, 1);
   return false;
}


bool LuaScriptRunner::startLua(ScriptType scriptType)
{
   // Start Lua and get everything configured if we haven't already done so
   if(!L)
   {
      TNLAssert(mScriptingDirSet, "Must set scripting folder before starting Lua interpreter!");

      // Prepare the Lua global environment
      L = lua_open();     // Create a new Lua interpreter; will be shutdown in the destructor

      if(!L)
      {  
         // Failure here is likely to be something systemic, something bad.  Like smallpox.
         logError("Could not create Lua interpreter.  Aborting script.");     
         return false;
      }

      if(!configureNewLuaInstance())
      {
         logError("Could not configure Lua interpreter.  I cannot run any scripts until the problem is resolved.");
         lua_close(L);
         return false;
      }
   }
   
   return prepareEnvironment();     // Bots and Levelgens each override this -- sets vars in the created environment
}


// Prepare a new Lua environment for use
bool LuaScriptRunner::configureNewLuaInstance()
{
   registerClasses();

   lua_atpanic(L, luaPanicked);  // Register our panic function 

   // Set scads of global vars in the Lua instance that mimic the use of the enums we use everywhere.
   // These will be copied into the script's environment when we run createEnvironment.
   setEnums(L);                  

#ifdef USE_PROFILER
   init_profiler(L);
#endif

   luaL_openlibs(L);    // Load the standard libraries
   luaopen_vec(L);      // For vector math (lua-vec)

   setModulePath();

   /*luaL_dostring(L, "local env = setmetatable({}, {__index=function(t,k) if k=='_G' then return nil else return _G[k] end})");*/

   // Define a function for copying the global environment to create a private environment for our scripts to run in
   // TODO: This needs to be a deep copy
   luaL_dostring(L, " function table.copy(t)"
                    "    local u = { }"
                    "    for k, v in pairs(t) do u[k] = v end"
                    "    return setmetatable(u, getmetatable(t))"
                    " end");

   // Load our helper functions and store copies of the compiled code in the registry where we can use them for starting new scripts
   return(loadCompileSaveHelper("lua_helper_functions.lua",      LUA_HELPER_FUNCTIONS_KEY)   &&
          loadCompileSaveHelper("robot_helper_functions.lua",    ROBOT_HELPER_FUNCTIONS_KEY) &&
          loadCompileSaveHelper("levelgen_helper_functions.lua", LEVELGEN_HELPER_FUNCTIONS_KEY));
}


bool LuaScriptRunner::loadCompileSaveHelper(const string &scriptName, const char *registryKey)
{
   return loadCompileSaveScript(joindir(mScriptingDir, scriptName).c_str(), registryKey);
}


// Load script from specified file, compile it, and store it in the registry
bool LuaScriptRunner::loadCompileSaveScript(const char *filename, const char *registryKey)
{
   if(!loadCompileScript(filename))
      return false;

   lua_setfield(L, LUA_REGISTRYINDEX, registryKey);      // Save compiled code in registry

   return true;
}


// Load script and place on top of the stack
bool LuaScriptRunner::loadCompileScript(const char *filename)
{
   S32 err = luaL_loadfile(L, filename);     

   if(err == 0)
      return true;

   logprintf(LogConsumer::LogError, "Error loading script %s: %s.", filename, lua_tostring(L, -1));
   lua_pop(L, 1);    // Remove error message from stack
   return false;
}


// Delete script's environment from the registry -- actually set the registry entry to nil so the table can be collected
void LuaScriptRunner::deleteScript(const char *name)
{
   // If a script is not found, or there is some other problem with the bot (or levelgen), we might get here before our L has been
   // set up.  If L hasn't been defined, there's no point in mucking with the registry, right?
   if(L)    
   {
      lua_pushnil(L);                                       //                             -- nil
      lua_setfield(L, LUA_REGISTRYINDEX, name);             // REGISTRY[scriptId] = nil    -- <<empty stack>>
   }
}


// Methods that should be abstract but can't be because luaW requires this class to be instantiable
bool LuaScriptRunner::prepareEnvironment()              { TNLAssert(false, "Unimplemented method!"); return false; }
void LuaScriptRunner::logError(const char *format, ...) { TNLAssert(false, "Unimplemented method!"); }


static string getStackTraceLine(lua_State *L, S32 level)
{
	lua_Debug ar;

	memset(&ar, 0, sizeof(ar));
	if(!lua_getstack(L, level + 1, &ar) || !lua_getinfo(L, "Snl", &ar))
		return "";

	char str[512];
	dSprintf(str, sizeof(str), "at %s(%s:%i)", ar.name, ar.source, ar.currentline);

   return str;    // Implicitly converted to string to avoid passing pointer to deleted buffer
}


void LuaScriptRunner::printStackTrace(lua_State *L)
{
   const int MAX_TRACE_LEN = 20;

   for(S32 level = 0; level < MAX_TRACE_LEN; level++)
   {
	   string str = getStackTraceLine(L, level);
	   if(str == "")
		   break;

	   if(level == 0)
		   logprintf("Stack trace:");

	   logprintf("   %s", str.c_str());
   }
}


// Register classes needed by all script runners
void LuaScriptRunner::registerClasses()
{
   LuaW_Registrar::registerClasses(L);

   // Lunar managed objects
   Lunar<LuaUtil>::Register(L);

   Lunar<LuaGameInfo>::Register(L);
   Lunar<LuaTeamInfo>::Register(L);
   Lunar<LuaPlayerInfo>::Register(L);

   Lunar<LuaWeaponInfo>::Register(L);
   Lunar<LuaModuleInfo>::Register(L);

   Lunar<Teleporter>::Register(L);

   Lunar<GoalZone>::Register(L);
   Lunar<LoadoutZone>::Register(L);
   Lunar<NexusObject>::Register(L);
}


// Hand off any script arguments to Lua, by packing them in the arg table, which is where Lua traditionally stores cmd line args.
// By Lua convention, we'll put the name of the script into the 0th element.
void LuaScriptRunner::setLuaArgs(const Vector<string> &args)
{
   lua_getfield(L, LUA_REGISTRYINDEX, getScriptId()); // Put script's env table onto the stack  -- ..., env_table
   lua_pushliteral(L, "arg");                         //                                        -- ..., env_table, "arg"
   lua_createtable(L, args.size() + 1, 0);            // Create table with predefined slots     -- ..., env_table, "arg", table

   lua_pushstring(L, mScriptName.c_str());            //                                        -- ..., env_table, "arg", table, scriptName
   lua_rawseti(L, -2, 0);                             //                                        -- ..., env_table, "arg", table

   for(S32 i = 0; i < args.size(); i++)
   {
      lua_pushstring(L, args[i].c_str());             //                                        -- ..., env_table, "arg", table, string
      lua_rawseti(L, -2, i + 1);                      //                                        -- ..., env_table, "arg", table
   }
   
   lua_settable(L, -3);                               // Save it: env_table["arg"] = table      -- ..., env_table
   lua_pop(L, 1);                                     // Remove environment table from stack    -- ...
}


// Set up paths so that we can use require to load code in our scripts 
void LuaScriptRunner::setModulePath()   
{
   TNLAssert(lua_gettop(L) == 0 || LuaObject::dumpStack(L), "Stack dirty!");

   lua_pushliteral(L, "package");                           // -- "package"
   lua_gettable(L, LUA_GLOBALSINDEX);                       // -- table (value of package global)

   lua_pushliteral(L, "path");                              // -- table, "path"
   lua_pushstring(L, (mScriptingDir + "/?.lua").c_str());   // -- table, "path", mScriptingDir + "/?.lua"
   lua_settable(L, -3);                                     // -- table
   lua_pop(L, 1);                                           // -- <<empty stack>>

   TNLAssert(lua_gettop(L) == 0 || LuaObject::dumpStack(L), "Stack not cleared!");
}


// Advance timers by deltaT
void LuaScriptRunner::tickTimer(U32 deltaT)
{
   TNLAssert(false, "Not (yet) implemented!");
}


// Since all calls to lua are now done in protected mode, via lua_pcall, if we get here, we've probably encountered 
// a fatal error such as running out of memory.  Best just to shut the whole thing down.
int LuaScriptRunner::luaPanicked(lua_State *L)
{
   string msg = lua_tostring(L, 1);

   logprintf("Fatal error running Lua code: %s.  Possibly out of memory?  Shutting down Bitfighter.", msg.c_str());

   throw LuaException(msg);
   //shutdownBitfighter();

   return 0;
}


int LuaScriptRunner::subscribe(lua_State *L)
{
   // Stack will have a bot or levelgen object at position 1, and the event at position 2
   // Get the event off the stack
   static const char *methodName = "subscribe()";
   LuaObject::checkArgCount(L, 2, methodName);

   lua_Integer eventType = LuaObject::getInt(L, -1, methodName);

   if(eventType < 0 || eventType >= EventManager::EventTypes)
      return 0;

   LuaObject::clearStack(L);

   EventManager::get()->subscribe(getScriptId(), (EventManager::EventType)eventType);
   mSubscriptions[eventType] = true;

   return 0;
}


int LuaScriptRunner::unsubscribe(lua_State *L)
{
   // Stack will have a bot or levelgen object at position 1, and the event at position 2
   // Get the event off the stack
   static const char *methodName = "LuaUtil:unsubscribe()";
   LuaObject::checkArgCount(L, 2, methodName);

   lua_Integer eventType = LuaObject::getInt(L, -1, methodName);
   if(eventType < 0 || eventType >= EventManager::EventTypes)
      return 0;

   LuaObject::clearStack(L);

   EventManager::get()->unsubscribe(getScriptId(), (EventManager::EventType)eventType);
   mSubscriptions[eventType] = false;
   return 0;
}


static int subscribe_bot(lua_State *L)
{
   Robot *robot = luaW_check<Robot>(L, 1);
   robot->subscribe(L);
   return 0;
}


static int unsubscribe_bot(lua_State *L)
{
   Robot *robot = luaW_check<Robot>(L, 1);
   robot->unsubscribe(L);
   return 0;
}


#define setEnumName(number, name) { lua_pushinteger(L, number);             lua_setglobal(L, name); }
#define setEnum(name)             { lua_pushinteger(L, name);               lua_setglobal(L, #name); }
#define setGTEnum(name)           { lua_pushinteger(L, GameType::name);     lua_setglobal(L, #name); }
#define setEventEnum(name)        { lua_pushinteger(L, EventManager::name); lua_setglobal(L, #name); }

// Set scads of global vars in the Lua instance that mimic the use of the enums we use everywhere
void LuaScriptRunner::setEnums(lua_State *L)
{
   setEnumName(BarrierTypeNumber, "BarrierType");
   setEnumName(PlayerShipTypeNumber, "ShipType");
   setEnumName(LineTypeNumber, "LineType");
   setEnumName(ResourceItemTypeNumber, "ResourceItemType");
   setEnumName(TextItemTypeNumber, "TextItemType");
   setEnumName(LoadoutZoneTypeNumber, "LoadoutZoneType");
   setEnumName(TestItemTypeNumber, "TestItemType");
   setEnumName(FlagTypeNumber, "FlagType");
   setEnumName(BulletTypeNumber, "BulletType");
   setEnumName(BurstTypeNumber, "BurstType");
   setEnumName(MineTypeNumber, "MineType");
   setEnumName(NexusTypeNumber, "NexusType");
   setEnumName(BotNavMeshZoneTypeNumber, "BotNavMeshZoneType");
   setEnumName(RobotShipTypeNumber, "RobotType");
   setEnumName(TeleportTypeNumber, "TeleportType");
   setEnumName(GoalZoneTypeNumber, "GoalZoneType");
   setEnumName(AsteroidTypeNumber, "AsteroidType");
   setEnumName(RepairItemTypeNumber, "RepairItemType");
   setEnumName(EnergyItemTypeNumber, "EnergyItemType");
   setEnumName(SoccerBallItemTypeNumber, "SoccerBallItemType");
   setEnumName(WormTypeNumber, "WormType");
   setEnumName(TurretTypeNumber, "TurretType");
   setEnumName(ForceFieldTypeNumber, "ForceFieldType");
   setEnumName(ForceFieldProjectorTypeNumber, "ForceFieldProjectorType");
   setEnumName(SpeedZoneTypeNumber, "SpeedZoneType");
   setEnumName(PolyWallTypeNumber, "PolyWallType");            // a little unsure about this one         
   setEnumName(ShipSpawnTypeNumber, "ShipSpawnType");
   setEnumName(FlagSpawnTypeNumber, "FlagSpawnType");
   setEnumName(AsteroidSpawnTypeNumber, "AsteroidSpawnType");
   setEnumName(WallItemTypeNumber, "WallItemType");            // a little unsure about this one
   setEnumName(WallEdgeTypeNumber, "WallEdgeType");            // not at all sure about this one
   setEnumName(WallSegmentTypeNumber, "WallSegmentType");      // not at all sure about this one
   setEnumName(SlipZoneTypeNumber, "SlipZoneType");
   setEnumName(SpyBugTypeNumber, "SpyBugType");
   setEnumName(CoreTypeNumber, "CoreType");

   // Modules
   setEnum(ModuleShield);
   setEnum(ModuleBoost);
   setEnum(ModuleSensor);
   setEnum(ModuleRepair);
   setEnum(ModuleEngineer);
   setEnum(ModuleCloak);
   setEnum(ModuleArmor);

   // Weapons
   setEnum(WeaponPhaser);
   setEnum(WeaponBounce);
   setEnum(WeaponTriple);
   setEnum(WeaponBurst);
   setEnum(WeaponMine);
   setEnum(WeaponSpyBug);
   setEnum(WeaponTurret);

   // Game Types
   setEnum(BitmatchGame);
   setEnum(CoreGame);
   setEnum(CTFGame);
   setEnum(HTFGame);
   setEnum(NexusGame);
   setEnum(RabbitGame);
   setEnum(RetrieveGame);
   setEnum(SoccerGame);
   setEnum(ZoneControlGame);

   // Scoring Events
   setGTEnum(KillEnemy);
   setGTEnum(KillSelf);
   setGTEnum(KillTeammate);
   setGTEnum(KillEnemyTurret);
   setGTEnum(KillOwnTurret);
   setGTEnum(KilledByAsteroid);
   setGTEnum(KilledByTurret);
   setGTEnum(CaptureFlag);
   setGTEnum(CaptureZone);
   setGTEnum(UncaptureZone);
   setGTEnum(HoldFlagInZone);
   setGTEnum(RemoveFlagFromEnemyZone);
   setGTEnum(RabbitHoldsFlag);
   setGTEnum(RabbitKilled);
   setGTEnum(RabbitKills);
   setGTEnum(ReturnFlagsToNexus);
   setGTEnum(ReturnFlagToZone);
   setGTEnum(LostFlag);
   setGTEnum(ReturnTeamFlag);
   setGTEnum(ScoreGoalEnemyTeam);
   setGTEnum(ScoreGoalHostileTeam);
   setGTEnum(ScoreGoalOwnTeam);

   // Event handler events
   setEventEnum(TickEvent);
   setEventEnum(ShipSpawnedEvent);
   setEventEnum(ShipKilledEvent);
   setEventEnum(MsgReceivedEvent);
   setEventEnum(PlayerJoinedEvent);
   setEventEnum(PlayerLeftEvent);
   setEventEnum(NexusOpenedEvent);
   setEventEnum(NexusClosedEvent);

   setEnum(EngineeredTurret);
   setEnum(EngineeredForceField);

   // A few other misc constants -- in Lua, we reference the teams as first team == 1, so neutral will be 0 and hostile -1
   lua_pushinteger(L, 0);  lua_setglobal(L, "NeutralTeamIndx");
   lua_pushinteger(L, -1); lua_setglobal(L, "HostileTeamIndx");

   // Some generic functions that we can put here rather than binding them to an object
   lua_register(L, "subscribe_bot",   subscribe_bot);
   lua_register(L, "unsubscribe_bot", unsubscribe_bot);


}

//const char *LuaScriptRunner::luaClassName = "LuaScriptRunner";
//const luaL_reg LuaScriptRunner::luaMethods[] =
//{
//   { "subscribe",   luaW_doMethod<LuaScriptRunner, &LuaScriptRunner::subscribe>   },
//   { "unsubscribe", luaW_doMethod<LuaScriptRunner, &LuaScriptRunner::unsubscribe> },
//};

//REGISTER_LUA_CLASS(LuaScriptRunner);

#undef setEnumName
#undef setEnum
#undef setGTEnum
#undef setEventEnum

};

