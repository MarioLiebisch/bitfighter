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

#include "robot.h"
#include "playerInfo.h"          // For RobotPlayerInfo constructor
#include "goalZone.h"
#include "loadoutZone.h"
#include "soccerGame.h"          // For lua object defs
#include "huntersGame.h"         // For lua object defs
#include "EngineeredItem.h"      // For lua object defs
#include "PickupItem.h"          // For lua object defs
#include "teleporter.h"          // For lua object defs

#include "../lua/luaprofiler-2.0.2/src/luaprofiler.h"      // For... the profiler!
#include "BotNavMeshZone.h"      // For BotNavMeshZone class definition
#include "luaUtil.h"
#include "oglconsole.h"


#ifndef ZAP_DEDICATED

#include "UI.h"

#endif

#include <math.h>


#define hypot _hypot    // Kill some warnings

namespace Zap
{

const bool QUIT_ON_SCRIPT_ERROR = true;


bool Robot::mIsPaused = false;
S32 Robot::mStepCount = -1;

extern ServerGame *gServerGame;




static EventManager eventManager;                // Singleton event manager, one copy is used by all bots


// Constructor
LuaRobot::LuaRobot(lua_State *L) : LuaShip((Robot *)lua_touserdata(L, 1))
{
   //lua_atpanic(L, luaPanicked);                  // Register our panic function
   thisRobot = (Robot *)lua_touserdata(L, 1);    // Register our robot
   thisRobot->mLuaRobot = this;

   // Initialize all subscriptions to unsubscribed
   for(S32 i = 0; i < EventManager::EventTypes; i++)
      subscriptions[i] = false;

   setEnums(L);      // Set scads of global vars in the Lua instance that mimic the use of the enums we use everywhere

   // A few misc constants -- in Lua, we reference the teams as first team == 1, so neutral will be 0 and hostile -1
   lua_pushinteger(L, 0); lua_setglobal(L, "NeutralTeamIndx");
   lua_pushinteger(L, -1); lua_setglobal(L, "HostileTeamIndx");
}


// Destructor
LuaRobot::~LuaRobot()
{
   // Make sure we're unsubscribed to all those events we subscribed to.  Don't want to
   // send an event to a dead bot, after all...
   for(S32 i = 0; i < EventManager::EventTypes; i++)
      if(subscriptions[i])
         eventManager.unsubscribeImmediate(thisRobot->getL(), (EventManager::EventType)i);

   logprintf(LogConsumer::LogLuaObjectLifecycle, "Deleted Lua Robot Object (%p)\n", this);
}


// Define the methods we will expose to Lua
// Methods defined here need to be defined in the LuaRobot in robot.h

Lunar<LuaRobot>::RegType LuaRobot::methods[] = {
   method(LuaRobot, getClassID),

   method(LuaRobot, getCPUTime),
   method(LuaRobot, getTime),

   // These inherited from LuaShip
   method(LuaRobot, isAlive),

   method(LuaRobot, getLoc),
   method(LuaRobot, getRad),
   method(LuaRobot, getVel),
   method(LuaRobot, getTeamIndx),

   method(LuaRobot, isModActive),
   method(LuaRobot, getEnergy),
   method(LuaRobot, getHealth),
   method(LuaRobot, hasFlag),
   method(LuaRobot, getFlagCount),

   method(LuaRobot, getAngle),
   method(LuaRobot, getActiveWeapon),
   method(LuaRobot, getMountedItems),

   method(LuaRobot, getCurrLoadout),
   method(LuaRobot, getReqLoadout),
   // End inherited methods

   method(LuaRobot, getZoneCenter),
   method(LuaRobot, getGatewayFromZoneToZone),
   method(LuaRobot, getZoneCount),
   method(LuaRobot, getCurrentZone),

   method(LuaRobot, setAngle),
   method(LuaRobot, setAnglePt),
   method(LuaRobot, getAnglePt),
   method(LuaRobot, hasLosPt),

   method(LuaRobot, getWaypoint),

   method(LuaRobot, setThrust),
   method(LuaRobot, setThrustPt),
   method(LuaRobot, setThrustToPt),

   method(LuaRobot, fire),
   method(LuaRobot, setWeapon),
   method(LuaRobot, setWeaponIndex),
   method(LuaRobot, hasWeapon),

   method(LuaRobot, activateModule),
   method(LuaRobot, activateModuleIndex),
   method(LuaRobot, setReqLoadout),

   method(LuaRobot, subscribe),
   method(LuaRobot, unsubscribe),

   method(LuaRobot, globalMsg),
   method(LuaRobot, teamMsg),

   method(LuaRobot, getActiveWeapon),

   method(LuaRobot, findItems),
   method(LuaRobot, findGlobalItems),
   method(LuaRobot, getFiringSolution),
   method(LuaRobot, getInterceptCourse),     // Doesn't work well...

   method(LuaRobot, engineerDeployObject),
   method(LuaRobot, dropItem),
   method(LuaRobot, copyMoveFromObject),

   {0,0}    // End method list
};


#define setEnumName(number, name) { lua_pushinteger(L, number); lua_setglobal(L, name); }

// Set scads of global vars in the Lua instance that mimic the use of the enums we use everywhere
void LuaRobot::setEnums(lua_State *L)
{
   setEnumName(BarrierTypeNumber, "BarrierType");
   setEnumName(PlayerShipTypeNumber, "ShipType");
   setEnumName(LineTypeNumber, "LineType");
   setEnumName(ResourceItemTypeNumber, "ResourceItem");
   setEnumName(TextItemTypeNumber, "TextItemType");
   setEnumName(LoadoutZoneTypeNumber, "LoadoutZoneType");
   setEnumName(TestItemTypeNumber, "TestItem");
   setEnumName(FlagTypeNumber, "FlagType");
   setEnumName(BulletTypeNumber, "BulletType");
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
   setEnumName(PolyWallTypeNumber, "PolyWallType");
   setEnumName(ShipSpawnTypeNumber, "ShipSpawnType");
   setEnumName(FlagSpawnTypeNumber, "FlagSpawnType");
   setEnumName(AsteroidSpawnTypeNumber, "AsteroidSpawnType");
   setEnumName(WallItemTypeNumber, "WallItemType");
   setEnumName(WallEdgeTypeNumber, "WallEdgeType");
   setEnumName(WallSegmentTypeNumber, "WallSegmentType");
   setEnumName(SlipZoneTypeNumber, "SlipZoneType");
   setEnumName(SpyBugTypeNumber, "SpyBugType");

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
   setGTEnum(BitmatchGame);
   setGTEnum(CTFGame);
   setGTEnum(HTFGame);
   setGTEnum(NexusGame);
   setGTEnum(RabbitGame);
   setGTEnum(RetrieveGame);
   setGTEnum(SoccerGame);
   setGTEnum(ZoneControlGame);

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
   setEventEnum(ShipSpawnedEvent);
   setEventEnum(ShipKilledEvent);
   setEventEnum(MsgReceivedEvent);
   setEventEnum(PlayerJoinedEvent);
   setEventEnum(PlayerLeftEvent);

   setEnum(EngineeredTurret);
   setEnum(EngineeredForceField);
}

#undef setEnumName


S32 LuaRobot::getClassID(lua_State *L)
{
   return returnInt(L, RobotShipTypeNumber);
}


// Return CPU time... use for timing things
S32 LuaRobot::getCPUTime(lua_State *L)
{
   return returnInt(L, gServerGame->getCurrentTime());
}


// Turn to angle a (in radians, or toward a point)
S32 LuaRobot::setAngle(lua_State *L)
{
   static const char *methodName = "Robot:setAngle()";

   if(lua_isnumber(L, 1))
   {
      checkArgCount(L, 1, methodName);

      Move move = thisRobot->getCurrentMove();
      move.angle = getFloat(L, 1, methodName);
      thisRobot->setCurrentMove(move);
   }
   else  // Could be a point?
   {
      checkArgCount(L, 1, methodName);
      Point point = getPoint(L, 1, methodName);

      Move move = thisRobot->getCurrentMove();
      move.angle = thisRobot->getAnglePt(point);
      thisRobot->setCurrentMove(move);
   }

   return 0;
}


// Turn towards point XY
S32 LuaRobot::setAnglePt(lua_State *L)
{
   static const char *methodName = "Robot:setAnglePt()";
   checkArgCount(L, 1, methodName);
   Point point = getPoint(L, 1, methodName);

   Move move = thisRobot->getCurrentMove();
   move.angle = thisRobot->getAnglePt(point);
   thisRobot->setCurrentMove(move);

   return 0;
}

// Get angle toward point
S32 LuaRobot::getAnglePt(lua_State *L)
{
   static const char *methodName = "Robot:getAnglePt()";
   checkArgCount(L, 1, methodName);
   Point point = getPoint(L, 1, methodName);

   lua_pushnumber(L, thisRobot->getAnglePt(point));
   return 1;
}


// Thrust at velocity v toward angle a
S32 LuaRobot::setThrust(lua_State *L)
{
   static const char *methodName = "Robot:setThrust()";
   checkArgCount(L, 2, methodName);
   F32 vel = getFloat(L, 1, methodName);
   F32 ang = getFloat(L, 2, methodName);

   Move move = thisRobot->getCurrentMove();

   move.x = vel * cos(ang);
   move.y = vel * sin(ang);

   thisRobot->setCurrentMove(move);

   return 0;
}


bool calcInterceptCourse(GameObject *target, Point aimPos, F32 aimRadius, S32 aimTeam, F32 aimVel, F32 aimLife, bool ignoreFriendly, F32 &interceptAngle)
{
   Point offset = target->getActualPos() - aimPos;    // Account for fact that robot doesn't fire from center
   offset.normalize(aimRadius * 1.2f);    // 1.2 is a fudge factor to prevent robot from not shooting because it thinks it will hit itself
   aimPos += offset;

   if(isShipType(target->getObjectTypeNumber()))
   {
      Ship *potential = (Ship*)target;

      // Is it dead or cloaked?  If so, ignore
      if(!potential->isVisible() || potential->hasExploded)
         return false;
   }

   if(ignoreFriendly && target->getTeam() == aimTeam)      // Is target on our team?
      return false;                                        // ...if so, skip it!

   // Calculate where we have to shoot to hit this...
   Point Vs = target->getActualVel();

   Point d = target->getActualPos() - aimPos;

   F32 t;      // t is set in next statement
   if(!FindLowestRootInInterval(Vs.dot(Vs) - aimVel * aimVel, 2 * Vs.dot(d), d.dot(d), aimLife * 0.001f, t))
      return false;

   Point leadPos = target->getActualPos() + Vs * t;

   // Calculate distance
   Point delta = (leadPos - aimPos);

   // Make sure we can see it...
   Point n;
   TestFunc testFunc = isWallType;

   if( !(isShipType(target->getObjectTypeNumber())) )  // If the target isn't a ship, take forcefields into account
      testFunc = isFlagCollideableType;

   if(target->findObjectLOS(testFunc, MoveObject::ActualState, aimPos, target->getActualPos(), t, n))
      return false;

   // See if we're gonna clobber our own stuff...
   target->disableCollision();
   Point delta2 = delta;
   delta2.normalize(aimLife * aimVel / 1000);
   GameObject *hitObject = target->findObjectLOS((TestFunc)isWithHealthType, 0, aimPos, aimPos + delta2, t, n);
   target->enableCollision();

   if(ignoreFriendly && hitObject && hitObject->getTeam() == aimTeam)
      return false;

   interceptAngle = delta.ATAN2();

   return true;
}


// Given an object, which angle do we need to be at to fire to hit it?
// Returns nil if a workable solution can't be found
// Logic adapted from turret aiming algorithm
// Note that bot WILL fire at teammates if you ask it to!
S32 LuaRobot::getFiringSolution(lua_State *L)
{
   static const char *methodName = "Robot:getFiringSolution()";
   checkArgCount(L, 2, methodName);
   U32 type = (U32)getInt(L, 1, methodName);
   GameObject *target = getItem(L, 2, type, methodName)->getGameObject();

   WeaponInfo weap = gWeapons[thisRobot->getSelectedWeapon()];    // Robot's active weapon

   F32 interceptAngle;

   if(calcInterceptCourse(target, thisRobot->getActualPos(), thisRobot->getRadius(), thisRobot->getTeam(), (F32)weap.projVelocity, (F32)weap.projLiveTime, false, interceptAngle))
      return returnFloat(L, interceptAngle);

   return returnNil(L);
}


// Given an object, what angle do we need to fly toward in order to collide with an object?  This
// works a lot like getFiringSolution().
S32 LuaRobot::getInterceptCourse(lua_State *L)
{
   static const char *methodName = "Robot:getInterceptCourse()";
   checkArgCount(L, 2, methodName);
   U32 type = (U32)getInt(L, 1, methodName);
   GameObject *target = getItem(L, 2, type, methodName)->getGameObject();

   WeaponInfo weap = gWeapons[thisRobot->getSelectedWeapon()];    // Robot's active weapon

   F32 interceptAngle;
   bool ok = calcInterceptCourse(target, thisRobot->getActualPos(), thisRobot->getRadius(), thisRobot->getTeam(), 256, 3000, false, interceptAngle);
   if(!ok)
      return returnNil(L);

   return returnFloat(L, interceptAngle);
}


// Thrust at velocity v toward point x,y
S32 LuaRobot::setThrustPt(lua_State *L)      // (number, point)
{
   static const char *methodName = "Robot:setThrustPt()";
   checkArgCount(L, 2, methodName);
   F32 vel = getFloat(L, 1, methodName);
   Point point = getPoint(L, 2, methodName);

   F32 ang = thisRobot->getAnglePt(point) - 0 * FloatHalfPi;

   Move move = thisRobot->getCurrentMove();

   move.x = vel * cos(ang);
   move.y = vel * sin(ang);

   thisRobot->setCurrentMove(move);

  return 0;
}


// Thrust toward specified point, but slow speed so that we land directly on that point if it is within range
S32 LuaRobot::setThrustToPt(lua_State *L)
{
   static const char *methodName = "Robot:setThrustToPt()";
   checkArgCount(L, 1, methodName);
   Point point = getPoint(L, 1, methodName);

   F32 ang = thisRobot->getAnglePt(point) - 0 * FloatHalfPi;

   Move move = thisRobot->getCurrentMove();

   F32 dist = thisRobot->getActualPos().distanceTo(point);

   F32 vel = dist / ((F32) move.time);      // v = d / t, t is in ms

   move.x = vel * cos(ang);
   move.y = vel * sin(ang);

   thisRobot->setCurrentMove(move);

  return 0;
}


// Get the coords of the center of mesh zone z
S32 LuaRobot::getZoneCenter(lua_State *L)
{
   static const char *methodName = "Robot:getZoneCenter()";
   checkArgCount(L, 1, methodName);
   S32 z = (S32)getInt(L, 1, methodName);


   BotNavMeshZone *zone = dynamic_cast<BotNavMeshZone *>(gServerGame->getBotZoneDatabase()->getObjectByIndex(z));

   if(zone)
      return returnPoint(L, zone->getCenter());
   // else
   return returnNil(L);
}


// Get the coords of the gateway to the specified zone.  Returns point, nil if requested zone doesn't border current zone.
S32 LuaRobot::getGatewayFromZoneToZone(lua_State *L)
{
   static const char *methodName = "Robot:getGatewayFromZoneToZone()";
   checkArgCount(L, 2, methodName);
   S32 from = (S32)getInt(L, 1, methodName);
   S32 to = (S32)getInt(L, 2, methodName);

   BotNavMeshZone *zone = dynamic_cast<BotNavMeshZone *>(gServerGame->getBotZoneDatabase()->getObjectByIndex(from));

   // Is requested zone a neighbor?
   if(zone)
      for(S32 i = 0; i < zone->mNeighbors.size(); i++)
         if(zone->mNeighbors[i].zoneID == to)
            return returnPoint(L, Rect(zone->mNeighbors[i].borderStart, zone->mNeighbors[i].borderEnd).getCenter());

   // Did not find requested neighbor, or zone index was invalid... returning nil
   return returnNil(L);
}


// Get the zone this robot is currently in.  If not in a zone, return nil
S32 LuaRobot::getCurrentZone(lua_State *L)
{
   S32 zone = thisRobot->getCurrentZone(gServerGame);
   return (zone == U16_MAX) ? returnNil(L) : returnInt(L, zone);
}


// Get a count of how many nav zones we have
S32 LuaRobot::getZoneCount(lua_State *L)
{
   return returnInt(L, gServerGame->getBotZoneDatabase()->getObjectCount());
}


// Fire current weapon if possible
S32 LuaRobot::fire(lua_State *L)
{
   Move move = thisRobot->getCurrentMove();
   move.fire = true;
   thisRobot->setCurrentMove(move);

   return 0;
}


// Can robot see point P?
S32 LuaRobot::hasLosPt(lua_State *L)      // Now takes a point or an x,y
{
   static const char *methodName = "Robot:hasLosPt()";
   //checkArgCount(L, 1, methodName);
   Point point = getPointOrXY(L, 1, methodName);

   return returnBool(L, thisRobot->canSeePoint(point));
}


// Set weapon to index
S32 LuaRobot::setWeaponIndex(lua_State *L)
{
   static const char *methodName = "Robot:setWeaponIndex()";
   checkArgCount(L, 1, methodName);
   U32 weap = (U32)getInt(L, 1, methodName, 1, ShipWeaponCount);    // Acceptable range = (1, ShipWeaponCount)
   thisRobot->selectWeapon(weap - 1);     // Correct for the fact that index in C++ is 0 based

   return 0;
}


// Set weapon to specified weapon, if we have it
S32 LuaRobot::setWeapon(lua_State *L)
{
   static const char *methodName = "Robot:setWeapon()";
   checkArgCount(L, 1, methodName);
   U32 weap = (U32)getInt(L, 1, methodName, 0, WeaponCount - 1);

   for(S32 i = 0; i < ShipWeaponCount; i++)
      if((U32)thisRobot->getWeapon(i) == weap)
      {
         thisRobot->selectWeapon(i);
         break;
      }

   // If we get here without having found our weapon, then nothing happens.  Better luck next time!
   return 0;
}


// Do we have a given weapon in our current loadout?
S32 LuaRobot::hasWeapon(lua_State *L)
{
   static const char *methodName = "Robot:hasWeapon()";
   checkArgCount(L, 1, methodName);
   U32 weap = (U32)getInt(L, 1, methodName, 0, WeaponCount - 1);

   for(S32 i = 0; i < ShipWeaponCount; i++)
      if((U32)thisRobot->getWeapon(i) == weap)
         return returnBool(L, true);      // We have it!

   return returnBool(L, false);           // We don't!
}


// Activate module this cycle --> takes module index
S32 LuaRobot::activateModuleIndex(lua_State *L)
{
   static const char *methodName = "Robot:activateModuleIndex()";

   checkArgCount(L, 1, methodName);
   U32 indx = (U32)getInt(L, 1, methodName, 0, ShipModuleCount);

   thisRobot->activateModulePrimary(indx);

   return 0;
}


// Activate module this cycle --> takes module enum.
// If specified module is not part of the loadout, does nothing.
S32 LuaRobot::activateModule(lua_State *L)
{
   static const char *methodName = "Robot:activateModule()";

   checkArgCount(L, 1, methodName);
   ShipModule mod = (ShipModule) getInt(L, 1, methodName, 0, ModuleCount - 1);

   for(S32 i = 0; i < ShipModuleCount; i++)
      if(thisRobot->getModule(i) == mod)
      {
         thisRobot->activateModulePrimary(i);
         break;
      }

   return 0;
}


// Sets loadout to specified --> takes 2 modules, 3 weapons
S32 LuaRobot::setReqLoadout(lua_State *L)
{
   checkArgCount(L, 1, "Robot:setReqLoadout()");

   LuaLoadout *loadout = Lunar<LuaLoadout>::check(L, 1);
   Vector<U32> vec;

   for(S32 i = 0; i < ShipModuleCount + ShipWeaponCount; i++)
      vec.push_back(loadout->getLoadoutItem(i));

   //thisRobot->setLoadout(vec); Robots cheat with this line, skipping loadout zone.
   thisRobot->getOwner()->sRequestLoadout(vec);

   return 0;
}


// Send message to all players
S32 LuaRobot::globalMsg(lua_State *L)
{
   static const char *methodName = "Robot:globalMsg()";
   checkArgCount(L, 1, methodName);

   const char *message = getString(L, 1, methodName);

   GameType *gt = gServerGame->getGameType();
   if(gt)
   {
      gt->s2cDisplayChatMessage(true, thisRobot->getName(), message);

      // Fire our event handler
      Robot::getEventManager().fireEvent(thisRobot->getL(), EventManager::MsgReceivedEvent, message, thisRobot->getPlayerInfo(), true);
   }

   return 0;
}


// Send message to team (what happens when neutral/hostile robot does this???)
S32 LuaRobot::teamMsg(lua_State *L)
{
   static const char *methodName = "Robot:teamMsg()";
   checkArgCount(L, 1, methodName);

   const char *message = getString(L, 1, methodName);

   GameType *gt = gServerGame->getGameType();
   if(gt)
   {
      gt->s2cDisplayChatMessage(true, thisRobot->getName(), message);

      // Fire our event handler
      Robot::getEventManager().fireEvent(thisRobot->getL(), EventManager::MsgReceivedEvent, message, thisRobot->getPlayerInfo(), false);
   }

   return 0;
}


S32 LuaRobot::getTime(lua_State *L)
{
   return returnInt(L, thisRobot->getCurrentMove().time);
}


// Return list of all items of specified type within normal visible range... does no screening at this point
S32 LuaRobot::findItems(lua_State *L)
{
   Point pos = thisRobot->getActualPos();
   Rect queryRect(pos, pos);
   queryRect.expand(gServerGame->computePlayerVisArea(thisRobot));

   return doFindItems(L, &queryRect);
}


// Same but gets all visible items from whole game... out-of-scope items will be ignored
S32 LuaRobot::findGlobalItems(lua_State *L)
{
   return doFindItems(L);
}


S32 LuaRobot::doFindItems(lua_State *L, Rect *scope)
{
   S32 index = 1;
   S32 pushed = 0;      // Count of items actually pushed onto the stack

   fillVector.clear();

   while(lua_isnumber(L, index))
   {
      GridDatabase *gridDB;
      U8 number = (U8)lua_tointeger(L, index);


      if(number == BotNavMeshZoneTypeNumber)
         gridDB = ((ServerGame *)thisRobot->getGame())->getBotZoneDatabase();
      else
         gridDB = thisRobot->getGame()->getGameObjDatabase();

      if(scope)    // Get other objects on screen-visible area only
         gridDB->findObjects(number, fillVector, *scope);
      else
         gridDB->findObjects(number, fillVector);

      index++;
   }

   clearStack(L);

   lua_createtable(L, fillVector.size(), 0);    // Create a table, with enough slots pre-allocated for our data

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      if(isShipType(fillVector[i]->getObjectTypeNumber()))      // Skip cloaked ships & robots!
      {
         Ship *ship = dynamic_cast<Ship *>(fillVector[i]);
         if(ship)
         {

         if(dynamic_cast<Robot *>(fillVector[i]) == thisRobot)             // Do not find self
            continue;

         // Ignore ship/robot if it's dead or cloaked
         if(!ship->isVisible() || ship->hasExploded)
            continue;
         }
      }

      GameObject *obj = dynamic_cast<GameObject *>(fillVector[i]);
      obj->push(L);
      pushed++;      // Increment pushed before using it because Lua uses 1-based arrays
      lua_rawseti(L, 1, pushed);
   }

   return 1;
}

// Get next waypoint to head toward when traveling from current location to x,y
// Note that this function will be called frequently by various robots, so any
// optimizations will be helpful.
S32 LuaRobot::getWaypoint(lua_State *L)  // Takes a luavec or an x,y
{
   static const char *methodName = "Robot:getWaypoint()";

   Point target = getPointOrXY(L, 1, methodName);

   // If we can see the target, go there directly
   if(thisRobot->canSeePoint(target))
   {
      thisRobot->flightPlan.clear();
      return returnPoint(L, target);
   }

   // TODO: cache destination point; if it hasn't moved, then skip ahead.

   U16 targetZone = BotNavMeshZone::findZoneContaining(gServerGame, target);       // Where we're going  ===> returns zone id

   if(targetZone == U16_MAX)       // Our target is off the map.  See if it's visible from any of our zones, and, if so, go there
   {
      targetZone = findClosestZone(target);

      if(targetZone == U16_MAX)
         return returnNil(L);
   }

   // Make sure target is still in the same zone it was in when we created our flightplan.
   // If we're not, our flightplan is invalid, and we need to skip forward and build a fresh one.
   if(thisRobot->flightPlan.size() > 0 && targetZone == thisRobot->flightPlanTo)
   {
      // In case our target has moved, replace final point of our flightplan with the current target location
      thisRobot->flightPlan[0] = target;

      // First, let's scan through our pre-calculated waypoints and see if we can see any of them.
      // If so, we'll just head there with no further rigamarole.  Remember that our flightplan is
      // arranged so the closest points are at the end of the list, and the target is at index 0.
      Point dest;
      bool found = false;
      bool first = true;

      while(thisRobot->flightPlan.size() > 0)
      {
         Point last = thisRobot->flightPlan.last();

         // We'll assume that if we could see the point on the previous turn, we can
         // still see it, even though in some cases, the turning of the ship around a
         // protruding corner may make it technically not visible.  This will prevent
         // rapidfire recalcuation of the path when it's not really necessary.

         // removed if(first) ... Problems with Robot get stuck after pushed from burst or mines.
         // To save calculations, might want to avoid (thisRobot->canSeePoint(last))
         if(thisRobot->canSeePoint(last))
         {
            dest = last;
            found = true;
            first = false;
            thisRobot->flightPlan.pop_back();   // Discard now possibly superfluous waypoint
         }
         else
            break;
      }

      // If we found one, that means we found a visible waypoint, and we can head there...
      if(found)
      {
         thisRobot->flightPlan.push_back(dest);    // Put dest back at the end of the flightplan
         return returnPoint(L, dest);
      }
   }

   // We need to calculate a new flightplan
   thisRobot->flightPlan.clear();

   U16 currentZone = thisRobot->getCurrentZone(gServerGame);     // Where we are

   if(currentZone == U16_MAX)      // We don't really know where we are... bad news!  Let's find closest visible zone and go that way.
      currentZone = findClosestZone(thisRobot->getActualPos());

   if(currentZone == U16_MAX)      // That didn't go so well...
      return returnNil(L);

   // We're in, or on the cusp of, the zone containing our target.  We're close!!
   if(currentZone == targetZone)
   {
      Point p;
      thisRobot->flightPlan.push_back(target);

      if(!thisRobot->canSeePoint(target))           // Possible, if we're just on a boundary, and a protrusion's blocking a ship edge
      {
         BotNavMeshZone *zone = dynamic_cast<BotNavMeshZone *>(gServerGame->getBotZoneDatabase()->getObjectByIndex(targetZone));

         p = zone->getCenter();
         thisRobot->flightPlan.push_back(p);
      }
      else
         p = target;

      return returnPoint(L, p);
   }

   // If we're still here, then we need to find a new path.  Either our original path was invalid for some reason,
   // or the path we had no longer applied to our current location
   thisRobot->flightPlanTo = targetZone;

   // check cache for path first
   pair<S32,S32> pathIndex = pair<S32,S32>(currentZone, targetZone);


   // TODO: Cache this block -- data will not change throughout game... 
   Vector<DatabaseObject *> objects;
   gServerGame->getBotZoneDatabase()->findObjects(BotNavMeshZoneTypeNumber, objects);

   Vector<BotNavMeshZone *> zones;
   zones.resize(objects.size());
   for(S32 i = 0; i < objects.size(); i++)
      zones[i] = dynamic_cast<BotNavMeshZone *>(objects[i]);
   // END BLOCK


   if(gServerGame->getGameType()->cachedBotFlightPlans.find(pathIndex) == gServerGame->getGameType()->cachedBotFlightPlans.end())
   {
      // Not found so calculate flight plan
      thisRobot->flightPlan = AStar::findPath(zones, currentZone, targetZone, target);

      // Add to cache
      gServerGame->getGameType()->cachedBotFlightPlans[pathIndex] = thisRobot->flightPlan;
   }
   else
   {
      thisRobot->flightPlan = gServerGame->getGameType()->cachedBotFlightPlans[pathIndex];
   }

   if(thisRobot->flightPlan.size() > 0)
      return returnPoint(L, thisRobot->flightPlan.last());
   else
      return returnNil(L);    // Out of options, end of the road
}


// Another helper function: returns id of closest zone to a given point
U16 LuaRobot::findClosestZone(const Point &point)
{
   U16 closestZone = U16_MAX;

   // First, do a quick search for zone based on the buffer; should be 99% of the cases

   // Search radius is just slightly larger than twice the zone buffers added to objects like barriers
   S32 searchRadius = 2 * BotNavMeshZone::BufferRadius + 1;

   Vector<DatabaseObject*> objects;
   Rect rect = Rect(point.x + searchRadius, point.y + searchRadius, point.x - searchRadius, point.y - searchRadius);

   gServerGame->getBotZoneDatabase()->findObjects(BotNavMeshZoneTypeNumber, objects, rect);

   for(S32 i = 0; i < objects.size(); i++)
   {
      BotNavMeshZone *zone = dynamic_cast<BotNavMeshZone *>(objects[i]);
      Point center = zone->getCenter();

      if(gServerGame->getGameObjDatabase()->pointCanSeePoint(center, point))  // This is an expensive test
      {
         closestZone = zone->getZoneId();
         break;
      }
   }

   // Target must be outside extents of the map, find nearest zone if a straight line was drawn
   if (closestZone == U16_MAX)
   {
      Point extentsCenter = gServerGame->getWorldExtents().getCenter();

      F32 collisionTimeIgnore;
      Point surfaceNormalIgnore;

      DatabaseObject* object = gServerGame->getBotZoneDatabase()->findObjectLOS(BotNavMeshZoneTypeNumber,
            MoveObject::ActualState, point, extentsCenter, collisionTimeIgnore, surfaceNormalIgnore);

      BotNavMeshZone *zone = dynamic_cast<BotNavMeshZone *>(object);

      if (zone != NULL)
         closestZone = zone->getZoneId();
   }

   return closestZone;
}


S32 LuaRobot::findAndReturnClosestZone(lua_State *L, const Point &point)
{
   U16 closest = findClosestZone(point);

   if(closest != U16_MAX)
   {
      BotNavMeshZone *zone = dynamic_cast<BotNavMeshZone *>(gServerGame->getBotZoneDatabase()->getObjectByIndex(closest));
      return returnPoint(L, zone->getCenter());
   }
   else
      return returnNil(L);    // Really stuck
}


S32 LuaRobot::subscribe(lua_State *L)
{
   // Get the event off the stack
   static const char *methodName = "Robot:subscribe()";
   checkArgCount(L, 1, methodName);

   S32 eventType = (S32)getInt(L, 0, methodName);
   if(eventType < 0 || eventType >= EventManager::EventTypes)
      return 0;

   eventManager.subscribe(L, (EventManager::EventType) eventType);
   subscriptions[eventType] = true;
   return 0;
}


S32 LuaRobot::unsubscribe(lua_State *L)
{
   // Get the event off the stack
   static const char *methodName = "Robot:unsubscribe()";
   checkArgCount(L, 1, methodName);

   S32 eventType = (S32)getInt(L, 0, methodName);
   if(eventType < 0 || eventType >= EventManager::EventTypes)
      return 0;

   eventManager.unsubscribe(L, (EventManager::EventType) eventType);
   subscriptions[eventType] = false;
   return 0;
}

S32 LuaRobot::engineerDeployObject(lua_State *L)
{
   static const char *methodName = "Robot:engineerDeployObject()";
   checkArgCount(L, 1, methodName);
   S32 type = (S32)getInt(L, 0, methodName);

   if(! thisRobot->getOwner())
      return returnBool(L, false);
   return returnBool(L, thisRobot->getOwner()->sEngineerDeployObject(type));
}

S32 LuaRobot::dropItem(lua_State *L)
{
   static const char *methodName = "Robot:dropItem()";

   S32 count = thisRobot->mMountedItems.size();
   for(S32 i = count - 1; i >= 0; i--)
      thisRobot->mMountedItems[i]->onItemDropped();

   return 0;
}

S32 LuaRobot::copyMoveFromObject(lua_State *L)
{
   static const char *methodName = "Robot:copyMoveFromObject()";

   checkArgCount(L, 2, methodName);
   U32 type = (U32)getInt(L, 1, methodName);
   LuaItem *luaobj = getItem(L, 2, type, methodName);
   GameObject *obj = luaobj->getGameObject();

   Move move = obj->getCurrentMove();
   move.time = thisRobot->getCurrentMove().time; // keep current move time
   thisRobot->setCurrentMove(move);

   return 0;
}


const char LuaRobot::className[] = "LuaRobot";     // This is the class name as it appears to the Lua scripts


////////////////////////////////////////
////////////////////////////////////////


bool EventManager::anyPending = false; 
Vector<lua_State *> EventManager::subscriptions[EventTypes];
Vector<lua_State *> EventManager::pendingSubscriptions[EventTypes];
Vector<lua_State *> EventManager::pendingUnsubscriptions[EventTypes];

void EventManager::subscribe(lua_State *L, EventType eventType)
{
   // First, see if we're already subscribed
   if(!isSubscribed(L, eventType) && !isPendingSubscribed(L, eventType))
   {
      removeFromPendingUnsubscribeList(L, eventType);
      pendingSubscriptions[eventType].push_back(L);
      anyPending = true;
   }
}


void EventManager::unsubscribe(lua_State *L, EventType eventType)
{
   if(isSubscribed(L, eventType) && !isPendingUnsubscribed(L, eventType))
   {
      removeFromPendingSubscribeList(L, eventType);
      pendingUnsubscriptions[eventType].push_back(L);
      anyPending = true;
   }
}


void EventManager::removeFromPendingSubscribeList(lua_State *subscriber, EventType eventType)
{
   for(S32 i = 0; i < pendingSubscriptions[eventType].size(); i++)
      if(pendingSubscriptions[eventType][i] == subscriber)
      {
         pendingSubscriptions[eventType].erase_fast(i);
         return;
      }
}


void EventManager::removeFromPendingUnsubscribeList(lua_State *unsubscriber, EventType eventType)
{
   for(S32 i = 0; i < pendingUnsubscriptions[eventType].size(); i++)
      if(pendingUnsubscriptions[eventType][i] == unsubscriber)
      {
         pendingUnsubscriptions[eventType].erase_fast(i);
         return;
      }
}


void EventManager::removeFromSubscribedList(lua_State *subscriber, EventType eventType)
{
   for(S32 i = 0; i < subscriptions[eventType].size(); i++)
      if(subscriptions[eventType][i] == subscriber)
      {
         subscriptions[eventType].erase_fast(i);
         return;
      }
}


// Unsubscribe an event bypassing the pending unsubscribe queue, when we know it will be OK
void EventManager::unsubscribeImmediate(lua_State *L, EventType eventType)
{
   removeFromSubscribedList(L, eventType);
   removeFromPendingSubscribeList(L, eventType);
   removeFromPendingUnsubscribeList(L, eventType);    // Probably not really necessary...
}


// Check if we're subscribed to an event
bool EventManager::isSubscribed(lua_State *L, EventType eventType)
{
   for(S32 i = 0; i < subscriptions[eventType].size(); i++)
      if(subscriptions[eventType][i] == L)
         return true;

   return false;
}


bool EventManager::isPendingSubscribed(lua_State *L, EventType eventType)
{
   for(S32 i = 0; i < pendingSubscriptions[eventType].size(); i++)
      if(pendingSubscriptions[eventType][i] == L)
         return true;

   return false;
}


bool EventManager::isPendingUnsubscribed(lua_State *L, EventType eventType)
{
   for(S32 i = 0; i < pendingUnsubscriptions[eventType].size(); i++)
      if(pendingUnsubscriptions[eventType][i] == L)
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


// This is a list of the function names to be called in the bot when a particular event is fired
static const char *eventFunctions[] = {
   "onShipSpawned",
   "onShipKilled",
   "onPlayerJoined",
   "onPlayerLeft",
   "onMsgReceived",
};


void EventManager::fireEvent(EventType eventType)
{
   for(S32 i = 0; i < subscriptions[eventType].size(); i++)
   {
      lua_State *L = subscriptions[eventType][i];
      try
      {
         lua_getglobal(L, "onMsgSent");

         if (lua_pcall(L, 0, 0, 0) != 0)
            throw LuaException(lua_tostring(L, -1));
      }
      catch(LuaException &e)
      {
         logprintf(LogConsumer::LogError, "Robot error firing event %d: %s.", eventType, e.what());
         OGLCONSOLE_Print("Robot error firing event %d: %s.", eventType, e.what());
         return;
      }
   }
}


void EventManager::fireEvent(EventType eventType, Ship *ship)
{
   for(S32 i = 0; i < subscriptions[eventType].size(); i++)
   {
      lua_State *L = subscriptions[eventType][i];
      try
      {
         lua_getglobal(L, eventFunctions[eventType]);
         ship->push(L);

         if (lua_pcall(L, 1, 0, 0) != 0)
            throw LuaException(lua_tostring(L, -1));
      }
      catch(LuaException &e)
      {
         logprintf(LogConsumer::LogError, "Robot error firing event %d: %s.", eventType, e.what());
         OGLCONSOLE_Print("Robot error firing event %d: %s.", eventType, e.what());
         return;
      }
   }
}


void EventManager::fireEvent(lua_State *caller_L, EventType eventType, const char *message, LuaPlayerInfo *player, bool global)
{
   for(S32 i = 0; i < subscriptions[eventType].size(); i++)
   {
      lua_State *L = subscriptions[eventType][i];

      if(L == caller_L)    // Don't alert bot about own message!
         continue;

      try
      {
         lua_getglobal(L, eventFunctions[eventType]);  
         lua_pushstring(L, message);
         player->push(L);
         lua_pushboolean(L, global);

         if (lua_pcall(L, 3, 0, 0) != 0)
            throw LuaException(lua_tostring(L, -1));
      }
      catch(LuaException &e)
      {
         logprintf(LogConsumer::LogError, "Robot error firing event %d: %s.", eventType, e.what());
         OGLCONSOLE_Print("Robot error firing event %d: %s.", eventType, e.what());
         return;
      }
   }
}


// PlayerJoined, PlayerLeft
void EventManager::fireEvent(lua_State *caller_L, EventType eventType, LuaPlayerInfo *player)
{
   for(S32 i = 0; i < subscriptions[eventType].size(); i++)
   {
      lua_State *L = subscriptions[eventType][i];
      
      if(L == caller_L)    // Don't alert bot about own joinage or leavage!
         continue;

      try   
      {
         lua_getglobal(L, eventFunctions[eventType]);  
         player->push(L);

         if (lua_pcall(L, 1, 0, 0) != 0)
            throw LuaException(lua_tostring(L, -1));
      }
      catch(LuaException &e)
      {
         logprintf(LogConsumer::LogError, "Robot error firing event %d: %s.", eventType, e.what());
         OGLCONSOLE_Print("Robot error firing event %d: %s.", eventType, e.what());
         return;
      }
   }
}


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(Robot);


Vector<Robot *> Robot::robots;

// Constructor, runs on client and server
Robot::Robot() : Ship("Robot", false, TEAM_NEUTRAL, Point(), 1, true), LuaScriptRunner()
{
   mHasSpawned = false;
   mObjectTypeNumber = RobotShipTypeNumber;

   mCurrentZone = U16_MAX;
   flightPlanTo = U16_MAX;

   mClientInfo = boost::shared_ptr<ClientInfo>(new LocalClientInfo(NULL, true));

   mPlayerInfo = new RobotPlayerInfo(this);
   mScore = 0;
   mTotalScore = 0;

   for(S32 i = 0; i < ModuleCount; i++)         // Here so valgrind won't complain if robot updates before initialize is run
   {
      mModulePrimaryActive[i] = false;
      mModuleSecondaryActive[i] = false;
   }
}


// Destructor, runs on client and server
Robot::~Robot()
{
   bool hasConn = mClientInfo->getConnection();
   TNLAssert(mHasSpawned == hasConn, "How are thsese not the same??");

   if(mHasSpawned)      // Can probably be replaced with mClientInfo->getConnection()
   {
      if(getGame()->getGameType())
         getGame()->getGameType()->serverRemoveClient(mClientInfo.get());

      setOwner(NULL);
   }

   // Close down our Lua interpreter
   cleanupAndTerminate();

   if(isGhost())
   {
      delete mPlayerInfo;     // If this is on the server, will be deleted below, after event is fired
      return;
   }

   // Server only from here on down

   // Remove this robot from the list of all robots
   for(S32 i = 0; i < robots.size(); i++)
      if(robots[i] == this)
      {
         robots.erase_fast(i);
         break;
      }

   mPlayerInfo->setDefunct();
   eventManager.fireEvent(L, EventManager::PlayerLeftEvent, getPlayerInfo());

   delete mPlayerInfo;

   logprintf(LogConsumer::LogLuaObjectLifecycle, "Robot terminated [%s] (%d)", mFilename.c_str(), robots.size());
}


// Reset everything on the robot back to the factory settings
// Only runs on server!
// return false if failed  -- should we also delete this at this point???
bool Robot::initialize(Point &pos)
{
   try
   {
      //respawnTimer.clear();
      flightPlan.clear();

      mCurrentZone = U16_MAX;   // Correct value will be calculated upon first request

      Parent::initialize(pos);

      enableCollision();

      // WarpPositionMask triggers the spinny spawning visual effect
      setMaskBits(RespawnMask | HealthMask | LoadoutMask | PositionMask | MoveMask | ModulePrimaryMask | ModuleSecondaryMask | WarpPositionMask);      // Send lots to the client

      TNLAssert(!isGhost(), "Didn't expect ghost here... this is supposed to only run on the server!");

      if(!runMain())               // Try to run, can fail on script error
         return false;

      eventManager.update();       // Ensure registrations made during bot initialization are ready to go

   }
   catch(LuaException &e)
   {
      logError("Robot error during spawn: %s.  Shutting robot down.", e.what());
      return false;
   }

   return true;
} 


// Loop through all our bots and start their interpreters
void Robot::startBots()
{   
   for(S32 i = 0; i < robots.size(); i++)
      if(!robots[i]->startLua())
      {
         robots.erase_fast(i);
         i--;
      }
}


void Robot::preHelperInit()
{
   // Push a pointer to this Robot to the Lua stack, then set the global name of this pointer.  
   // This is the name that we'll use to refer to this robot from our Lua code.  
   // Note that all globals need to be set before running lua_helper_functions, which makes it more difficult to set globals
   lua_pushlightuserdata(L, (void *)this);
   lua_setglobal(L, "Robot");
}


bool Robot::startLua()
{
   if(!LuaScriptRunner::startLua(mFilename.c_str(), mArgs, ROBOT))
      return false;

   string name = runGetName();

   mPlayerName = GameConnection::makeUnique(name).c_str();       // Make sure name is unique
   mIsAuthenticated = false;

   return true;
}



// Run bot's getName function, return default name if fn isn't defined
string Robot::runGetName()
{
   // Run the getName() function in the bot (will default to the one in robot_helper_functions if it's not overwritten by the bot)
   lua_getglobal(L, "getName");

   if (!lua_isfunction(L, -1) || lua_pcall(L, 0, 1, 0))     // Passing 0 params, getting one back
   {
      string name = "Nancy";
      logError("Robot error retrieving name (%s).  Using \"%s\".", lua_tostring(L, -1), name.c_str());
      return name;
   }
   else
   {
      string name = lua_tostring(L, -1);
      lua_pop(L, 1);
      return name;
   }
}


// Register our connector types with Lua
void Robot::registerClasses()
{
   Lunar<LuaUtil>::Register(L);

   Lunar<LuaGameInfo>::Register(L);
   Lunar<LuaTeamInfo>::Register(L);
   Lunar<LuaPlayerInfo>::Register(L);
   //Lunar<LuaTimer>::Register(L);

   Lunar<LuaWeaponInfo>::Register(L);
   Lunar<LuaModuleInfo>::Register(L);

   Lunar<LuaLoadout>::Register(L);
   Lunar<LuaPoint>::Register(L);

   Lunar<LuaRobot>::Register(L);
   Lunar<LuaShip>::Register(L);

   Lunar<RepairItem>::Register(L);
   Lunar<ResourceItem>::Register(L);
   Lunar<TestItem>::Register(L);
   Lunar<Asteroid>::Register(L);
   Lunar<Turret>::Register(L);
   Lunar<Teleporter>::Register(L);

   Lunar<ForceFieldProjector>::Register(L);
   Lunar<FlagItem>::Register(L);
   Lunar<SoccerBallItem>::Register(L);
   //Lunar<HuntersFlagItem>::Register(L);
   Lunar<ResourceItem>::Register(L);

   Lunar<LuaProjectile>::Register(L);
   Lunar<Mine>::Register(L);
   Lunar<SpyBug>::Register(L);

   Lunar<GoalZone>::Register(L);
   Lunar<LoadoutZone>::Register(L);
   Lunar<HuntersNexusObject>::Register(L);
}


// Don't forget to update the eventManager after running a robot's main function!
// return false if failed
bool Robot::runMain()
{
   try
   {
      lua_getglobal(L, "_main");       // _main calls main --> see robot_helper_functions.lua
      if(lua_pcall(L, 0, 0, 0) != 0)
         throw LuaException(lua_tostring(L, -1));
   }
   catch(LuaException &e)
   {
      logError("Robot error running main(): %s.  Shutting robot down.", e.what());
      return false;
   }

   return true;
}


EventManager Robot::getEventManager()
{
   return eventManager;
}


// This only runs the very first time the robot is added to the level
// Runs on client and server     --> is this ever actually called on the client????
void Robot::onAddedToGame(Game *game)
{
   Parent::onAddedToGame(game);
   
   if(isGhost())
      return;

   // Server only from here on out

   hasExploded = true;        // Becase we start off "dead", but will respawn real soon now...
   disableCollision();

   //setScopeAlways();        // Make them always visible on cmdr map --> del
   robots.push_back(this);    // Add this robot to the list of all robots (can't do this in constructor or else it gets run on client side too...)
   eventManager.fireEvent(L, EventManager::PlayerJoinedEvent, getPlayerInfo());
}


void Robot::kill()
{
   if(hasExploded) return;
   hasExploded = true;
   //respawnTimer.reset();
   setMaskBits(ExplosionMask);
   if(!isGhost() && getOwner())
      getLoadout(getOwner()->mOldLoadout);

   disableCollision();

   // Dump mounted items
   for(S32 i = mMountedItems.size() - 1; i >= 0; i--)
      if(mMountedItems[i].isValid())  // server quitting can make an object invalid.
         mMountedItems[i]->onMountDestroyed();
}


bool Robot::processArguments(S32 argc, const char **argv, Game *game)
{
   if(argc >= 1)
      mTeam = atoi(argv[0]);
   else
      mTeam = NO_TEAM;   
   
   if(argc >= 2)
      mFilename = argv[1];
   else
      mFilename = game->getSettings()->getIniSettings()->defaultRobotScript;


   string fullFilename = mFilename;  // for printing filename when not found

   FolderManager *folderManager = game->getSettings()->getFolderManager();

   if(mFilename != "")
      mFilename = folderManager->findBotFile(mFilename);

   if(mFilename == "")     // Bot script could not be located
   {
      logprintf("Could not find bot file %s", fullFilename.c_str());     // TODO: Better handling here
      OGLCONSOLE_Print("Could not find bot file %s", fullFilename.c_str());
      return false;
   }

   setScriptingDir(folderManager->luaDir);

   // Collect our arguments to be passed into the args table in the robot (starting with the robot name)
   // Need to make a copy or containerize argv[i] somehow, because otherwise new data will get written
   // to the string location subsequently, and our vals will change from under us.  That's bad!

   // We're using string here as a stupid way to get done what we need to do... perhaps there is a better way.

   for(S32 i = 2; i < argc; i++)        // Does nothing if we have no args.
      mArgs.push_back(string(argv[i]));

   return true;
}


void Robot::logError(const char *format, ...)
{
   va_list args;
   va_start(args, format);
   char buffer[2048];

   vsnprintf(buffer, sizeof(buffer), format, args);

   logError(buffer, mFilename.c_str());

   va_end(args);
}


void Robot::logError(const char *msg, const char *filename)
{
   logprintf(LogConsumer::LogError, "***ROBOT ERROR*** in %s ::: %s", filename, msg);
   OGLCONSOLE_Print("***ROBOT ERROR*** in %s ::: %s\n", filename, msg);
}


// Returns zone ID of current zone
S32 Robot::getCurrentZone(ServerGame *game)
{
   // We're in uncharted territory -- try to get the current zone
   mCurrentZone = BotNavMeshZone::findZoneContaining(game, getActualPos());

   return mCurrentZone;
}


// Setter method, not a robot function!
void Robot::setCurrentZone(S32 zone)
{
   mCurrentZone = zone;
}


F32 Robot::getAnglePt(Point point)
{
   return getActualPos().angleTo(point);
}


// Return coords of nearest ship... and experimental robot routine
bool Robot::findNearestShip(Point &loc)
{
   Vector<DatabaseObject *> foundObjects;
   Point closest;

   Point pos = getActualPos();
   Point extend(2000, 2000);
   Rect r(pos - extend, pos + extend);

   findObjects((TestFunc)isShipType, foundObjects, r);

   if(!foundObjects.size())
      return false;

   F32 dist = F32_MAX;
   bool found = false;

   for(S32 i = 0; i < foundObjects.size(); i++)
   {
      GameObject *foundObject = dynamic_cast<GameObject *>(foundObjects[i]);
      F32 d = foundObject->getActualPos().distanceTo(pos);
      if(d < dist && d > 0)      // d == 0 means we're comparing to ourselves
      {
         dist = d;
         loc = foundObject->getActualPos();     // use set here?
         found = true;
      }
   }
   return found;
}


bool Robot::canSeePoint(Point point)
{
   Point difference = point - getActualPos();

   Point crossVector(difference.y, -difference.x);  // Create a point whose vector from 0,0 is perpenticular to the original vector
   crossVector.normalize(mRadius);                  // reduce point so the vector has length of ship radius

   // edge points of ship
   Point shipEdge1 = getActualPos() + crossVector;
   Point shipEdge2 = getActualPos() - crossVector;

   // edge points of point
   Point pointEdge1 = point + crossVector;
   Point pointEdge2 = point - crossVector;

   Vector<Point> thisPoints;
   thisPoints.push_back(shipEdge1);
   thisPoints.push_back(shipEdge2);
   thisPoints.push_back(pointEdge2);
   thisPoints.push_back(pointEdge1);

   Vector<Point> otherPoints;
   Rect queryRect(thisPoints);
   fillVector.clear();
   findObjects((TestFunc)isCollideableType, fillVector, queryRect);

   for(S32 i=0; i < fillVector.size(); i++)
   {
      if(fillVector[i]->getCollisionPoly(otherPoints))
      {
         if(polygonsIntersect(thisPoints, otherPoints))
            return false;
      }
   }
   return true;
}


void Robot::render(S32 layerIndex)
{
#ifndef ZAP_DEDICATED
   if(isGhost())                                      // Client rendering client's objects
      Parent::render(layerIndex);

   else if(layerIndex == 1 && flightPlan.size() != 0)  // Client hosting is rendering server objects
   {
      glColor3f(1,1,0);       // yellow
      glBegin(GL_LINE_STRIP);
         glVertex(getActualPos());
         for(S32 i = flightPlan.size() - 1; i >= 0; i--)
            glVertex(flightPlan[i]);
         
      glEnd();
   }
#endif
}


void Robot::idle(GameObject::IdleCallPath path)
{
   U32 deltaT;

   if(path == GameObject::ServerIdleMainLoop)      // Running on server
   {
      deltaT = mCurrentMove.time;

      TNLAssert(deltaT != 0, "Robot::idle Time is zero")   // Time should never be zero anymore

      // Check to see if we need to respawn this robot
      if(hasExploded)
      {
            bool hasConn = mClientInfo->getConnection();
            TNLAssert(mHasSpawned == hasConn, "How are these not the same??");

         if(!mHasSpawned)  // Can probably be replaced with !mClientInfo->getConnection()
           spawn();

         return;
      }

      // Clear out current move.  It will get set just below with the lua call, but if that function
      // doesn't set the various move components, we want to make sure that they default to 0.
      mCurrentMove.fire = false;
      mCurrentMove.x = 0;
      mCurrentMove.y = 0;

      for(S32 i = 0; i < ShipModuleCount; i++)
      {
         mCurrentMove.modulePrimary[i] = false;
         mCurrentMove.moduleSecondary[i] = false;
      }

      if(!mIsPaused || mStepCount > 0)
      {
         if(mStepCount > 0)
            mStepCount--;
         
         try
         {
            lua_getglobal(L, "_onTick");   // _onTick calls onTick --> see robot_helper_functions.lua
            Lunar<LuaRobot>::push(L, this->mLuaRobot);

            lua_pushnumber(L, deltaT);    // Pass the time elapsed since we were last here

            if (lua_pcall(L, 2, 0, 0) != 0)
               throw LuaException(lua_tostring(L, -1));
         }
         catch(LuaException &e)
         {
            logError("Robot error running _onTick(): %s.  Shutting robot down.", e.what());

            // Safer than "delete this" -- adds bot to delete list, where it will be deleted later (or at least outside this construct)
            SafePtr<Robot> robotPtr = this;
            robotPtr->deleteObject();

            return;
         }
      }

      Parent::idle(GameObject::ServerIdleControlFromClient);   // Let's say the script is the client
      return;
   }

   TNLAssert(path != GameObject::ServerIdleControlFromClient, "Robot::idle, Should not have ServerIdleControlFromClient");

   if(!hasExploded)
      Parent::idle(path);     // All client paths can use this idle
}


void Robot::spawn()
{
   // Cannot be in onAddedToGame, as it will error while trying to add robots while level map is not ready

   GameConnection *gc = new GameConnection();
   gc->setClientInfo(mClientInfo);  // Fixes Robots scoring problems. Do not want a GameConnection to hold a different ClientInfo

   if(getName() == "")                          // Make sure bots have a name
      setName(GameConnection::makeUnique("Robot").c_str());

   gc->setControlObject(this);
   
   setOwner(gc);

   mClientInfo->setName(getName());
   mClientInfo->setConnection(gc);

   getGame()->getGameType()->serverAddClient(mClientInfo.get());    
   getGame()->addClientInfoToList(mClientInfo);
   
   mHasSpawned = true;
}



};

