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

#ifndef _LUAGAME_H_
#define _LUAGAME_H_


#include "luaObject.h"
#include "gameWeapons.h"
#include "shipItems.h"     // For module defs
#include "move.h"          // For ShipModuleCount + ShipWeaponCount

namespace Zap
{

class LuaGameInfo : public LuaObject
{
public:
   LuaGameInfo(lua_State *L);      // Constructor
   virtual ~LuaGameInfo();                 // Destructor

   static const char className[];

   static Lunar<LuaGameInfo>::RegType methods[];

   S32 getGameType(lua_State *L);
   S32 getGameTypeName(lua_State *L);
   S32 getFlagCount(lua_State *L);
   S32 getWinningScore(lua_State *L);
   S32 getGameTimeTotal(lua_State *L);
   S32 getGameTimeRemaining(lua_State *L);
   S32 getLeadingScore(lua_State *L);
   S32 getLeadingTeam(lua_State *L);
   S32 getTeamCount(lua_State *L);

   S32 getLevelName(lua_State *L);
   S32 getGridSize(lua_State *L);
   S32 isTeamGame(lua_State *L);

   S32 getEventScore(lua_State *L);

   S32 getPlayers(lua_State *L);       // Return table of all players/bots in game
   S32 isNexusOpen(lua_State *L);
   S32 getNexusTimeLeft(lua_State *L);
};


////////////////////////////////////////
////////////////////////////////////////

class LuaWeaponInfo : public LuaObject
{

private:
   U32 mWeaponIndex;

public:
   // Initialize the pointer
   LuaWeaponInfo(lua_State *L);            // Lua constructor
   LuaWeaponInfo(WeaponType weapon);       // C++ constructor

   virtual ~LuaWeaponInfo();                       // Destructor

   static const char className[];

   static Lunar<LuaWeaponInfo>::RegType methods[];

   S32 getName(lua_State *L);
   S32 getID(lua_State *L);

   S32 getRange(lua_State *L);
   S32 getFireDelay(lua_State *L);
   S32 getMinEnergy(lua_State *L);
   S32 getEnergyDrain(lua_State *L);
   S32 getProjVel(lua_State *L);
   S32 getProjLife(lua_State *L);
   S32 getDamage(lua_State *L);
   S32 getDamageSelf(lua_State *L);
   S32 getCanDamageTeammate(lua_State *L);
};


////////////////////////////////////////
////////////////////////////////////////

class LuaModuleInfo : public LuaObject
{
private:
   U32 mModuleIndex;

public:
   LuaModuleInfo(lua_State *L);      // Constructor
   virtual ~LuaModuleInfo();                 // Destructor

   static const char className[];

   static Lunar<LuaModuleInfo>::RegType methods[];

   S32 getName(lua_State *L);
   S32 getID(lua_State *L);

};


////////////////////////////////////////
////////////////////////////////////////

class LuaLoadout : public LuaObject
{
private:
   U8 mLoadout[ShipModuleCount + ShipWeaponCount];

public:
   //LuaLoadout(lua_State *L);                              // Lua constructor
   LuaLoadout(const U8 loadoutItems[] = DefaultLoadout);    // C++ constructor

   virtual ~LuaLoadout();                                   // Destructor

   LUAW_DECLARE_CLASS(LuaLoadout);

   static const luaL_reg luaMethods[];
   static const char *luaClassName;

   S32 setWeapon(lua_State *L);     // setWeapon(i, mod) ==> Set weapon at index i
   S32 setModule(lua_State *L);     // setModule(i, mod) ==> Set module at index i
   S32 isValid(lua_State *L);       // isValid() ==> Is loadout config valid?
   S32 equals(lua_State *L);        // equals(Loadout) ==> is loadout the same as Loadout?
   S32 getWeapon(lua_State *L);     // getWeapon(i) ==> return weapon at index i
   S32 getModule(lua_State *L);     // getModule(i) ==> return module at index i

   U8 getLoadoutItem(S32 indx);     // Helper function, not accessible from Lua
};


};

#endif

