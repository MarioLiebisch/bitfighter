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


#include "LoadoutTracker.h"
#include "stringUtils.h"      // For parseString

#include "tnlLog.h"


namespace Zap
{

// Constructor
LoadoutTracker::LoadoutTracker()
{
   resetLoadout();

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Constructor
LoadoutTracker::LoadoutTracker(const string &loadoutStr)
{
   resetLoadout();
   setLoadout(loadoutStr);

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Constructor
LoadoutTracker::LoadoutTracker(const Vector<U8> &loadout)
{
   resetLoadout();
   setLoadout(loadout);

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}

// Destructor
LoadoutTracker::~LoadoutTracker()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


// Reset this loadout to its factory settings
void LoadoutTracker::resetLoadout()
{
   for(S32 i = 0; i < ShipModuleCount; i++)
      mModules[i] = ModuleNone;

   for(S32 i = 0; i < ShipWeaponCount; i++)
      mWeapons[i] = WeaponNone;

   deactivateAllModules();
   mActiveWeapon = 0;
}


// Returns true if anything changed
bool LoadoutTracker::update(const LoadoutTracker &loadout)
{
   bool loadoutChanged = false;

   for(S32 i = 0; i < ShipModuleCount; i++)
      if(mModules[i] != loadout.mModules[i])
      {
         mModules[i] = loadout.mModules[i];
         loadoutChanged = true;
      }

   for(S32 i = 0; i < ModuleCount; i++)
   {
      mModulePrimaryActive[i]   = loadout.mModulePrimaryActive[i];  
      mModuleSecondaryActive[i] = loadout.mModuleSecondaryActive[i];
   }

   for(S32 i = 0; i < ShipWeaponCount; i++)
      if(mWeapons[i] != loadout.mWeapons[i])
      {
         mWeapons[i] = loadout.mWeapons[i];
         loadoutChanged = true;
      }

   return loadoutChanged;
}


// Takes a vector of U8s repesenting loadout... M,M,W,W,W
void LoadoutTracker::setLoadout(const Vector<U8> &items)
{
   resetLoadout();

   // Check for the proper number of items
   if(items.size() != ShipModuleCount + ShipWeaponCount)
      return;

   // Do some range checking
   for(S32 i = 0; i < ShipModuleCount; i++)
      if(items[i] >= ModuleCount)
         return;

   for(S32 i = 0; i < ShipWeaponCount; i++)
      if(items[i + ShipModuleCount] >= WeaponCount)
         return;

   // If everything checks out, we can fill the loadout
   for(S32 i = 0; i < ShipModuleCount; i++)
      mModules[i] = (ShipModule) items[i];

   for(S32 i = 0; i < ShipWeaponCount; i++)
      mWeapons[i] = (WeaponType) items[i + ShipModuleCount];
}


void LoadoutTracker::setLoadout(const string &loadoutStr)
{
   // If we have a loadout string, try to get something useful out of it.
   // Note that even if we are able to parse the loadout successfully, it might still be invalid for a 
   // particular server or gameType... engineer, for example, is not allowed everywhere.
   if(loadoutStr == "")
      return;

   Vector<string> words;
   parseString(loadoutStr, words, ',');

   if(words.size() != ShipModuleCount + ShipWeaponCount)      // Invalid loadout string
   {
      logprintf(LogConsumer::ConfigurationError, "Misconfigured loadout preset found in INI");
      return;
   }

   bool found;

   for(S32 i = 0; i < ShipModuleCount; i++)
   {
      found = false;
      const char *word = words[i].c_str();

      for(S32 j = 0; j < ModuleCount; j++)
         if(stricmp(word, ModuleInfo::getModuleInfo((ShipModule) j)->getName()) == 0)     // Case insensitive
         {
            mModules[i] = ShipModule(j);
            found = true;
            break;
         }

      if(!found)
      {
         logprintf(LogConsumer::ConfigurationError, "Unknown module found in loadout preset in INI file: %s", word);
         resetLoadout();
         return;
      }
   }

   for(S32 i = 0; i < ShipWeaponCount; i++)
   {
      found = false;
      const char *word = words[i + ShipModuleCount].c_str();

      for(S32 j = 0; j < WeaponCount; j++)
         if(stricmp(word, WeaponInfo::getWeaponInfo(WeaponType(j)).name.getString()) == 0)
         {
            mWeapons[i] = WeaponType(j);
            found = true;
            break;
         }

      if(!found)
      {
         logprintf(LogConsumer::ConfigurationError, "Unknown weapon found in loadout preset in INI file: %s", word);
         resetLoadout();
         return;
      }
   }
}


void LoadoutTracker::setModule(U32 moduleIndex, ShipModule module)
{
   mModules[moduleIndex] = module;
}


void LoadoutTracker::setWeapon(U32 weaponIndex, WeaponType weapon)
{
   mWeapons[weaponIndex] = weapon;
}


void LoadoutTracker::setActiveWeapon(U32 weaponIndex)
{
   mActiveWeapon = weaponIndex % ShipWeaponCount;
}


void LoadoutTracker::setModulePrimary(ShipModule module, bool isActive)
{
   mModulePrimaryActive[module] = isActive;
}


void LoadoutTracker::setModuleIndexPrimary(U32 moduleIndex, bool isActive)
{
   mModulePrimaryActive[mModules[moduleIndex]] = isActive;
}


void LoadoutTracker::setModuleSecondary(ShipModule module, bool isActive)
{
    mModuleSecondaryActive[module] = isActive;
}


void LoadoutTracker::setModuleIndexSecondary(U32 moduleIndex, bool isActive)
{
   mModuleSecondaryActive[mModules[moduleIndex]] = isActive;
}


void LoadoutTracker::deactivateAllModules()
{
   for(S32 i = 0; i < ModuleCount; i++)
   {
      mModulePrimaryActive[i] = false;
      mModuleSecondaryActive[i] = false;
   }
}


bool LoadoutTracker::hasModule(ShipModule mod) const
{
   for(S32 i = 0; i < ShipModuleCount; i++)
      if(mModules[i] == mod)
         return true;

   return false;
}


bool LoadoutTracker::hasWeapon(WeaponType weapon) const
{
   for(S32 i = 0; i < ShipWeaponCount; i++)
      if(mWeapons[i] == weapon)
         return true;

   return false;
}


bool LoadoutTracker::isValid() const
{
   return mModules[0] != ModuleNone;
}


bool LoadoutTracker::isWeaponActive(U32 weaponIndex) const
{
   return weaponIndex == mActiveWeapon;
}


WeaponType LoadoutTracker::getWeapon(U32 weaponIndex) const
{
   return mWeapons[weaponIndex];
}


WeaponType LoadoutTracker::getActiveWeapon() const
{
   return mWeapons[mActiveWeapon];
}


U32 LoadoutTracker::getActiveWeaponIndex() const
{
   return mActiveWeapon;
}


ShipModule LoadoutTracker::getModule(U32 modIndex) const
{
   return mModules[modIndex];
}


bool LoadoutTracker::isModulePrimaryActive(ShipModule module) const
{
      return mModulePrimaryActive[module];
}


bool LoadoutTracker::isModuleSecondaryActive(ShipModule module) const
{
   return mModuleSecondaryActive[module];
}


Vector<U8> LoadoutTracker::toU8Vector() const
{
   Vector<U8> loadout(ShipModuleCount + ShipWeaponCount);

   for(S32 i = 0; i < ShipModuleCount; i++)
      loadout.push_back(U8(mModules[i]));

   for(S32 i = 0; i < ShipWeaponCount; i++)
      loadout.push_back(U8(mWeapons[i]));

   return loadout;
}


bool LoadoutTracker::operator == (const LoadoutTracker &other) const 
{
   for(S32 i = 0; i < ShipModuleCount; i++)
      if(getModule(i) != other.getModule(i))
         return false;

   for(S32 i = 0; i < ShipWeaponCount; i++)
      if(getWeapon(i) != other.getWeapon(i))
         return false;

   return true;
}


bool LoadoutTracker::operator != (const LoadoutTracker &other) const 
{
   return !(*this == other);
}


string LoadoutTracker::toString() const
{
   if(!isValid())
      return "";

   Vector<string> loadoutStrings(ShipModuleCount + ShipWeaponCount);    // Reserve space for efficiency

   // First modules
   for(S32 i = 0; i < ShipModuleCount; i++)
      loadoutStrings.push_back(ModuleInfo::getModuleInfo((ShipModule) mModules[i])->getName());

   // Then weapons
   for(S32 i = 0; i < ShipWeaponCount; i++)
      loadoutStrings.push_back(WeaponInfo::getWeaponInfo(mWeapons[i]).name.getString());

   return listToString(loadoutStrings, ',');
}



/**
 *  @luaclass Loadout
 *  @brief    Get and set ship Loadout properties
 *  @descr    Use the %Loadout object to modify a ship or robots current loadout
 *
 *  You only need get this object once, then you can use it as often as you like. It will
 *  always reflect the latest data.
 */
//                Fn name                  Param profiles            Profile count
#define LUA_METHODS(CLASS, METHOD) \
   METHOD(CLASS, setWeapon,      ARRAYDEF({{ WEAP_SLOT, WEAP_ENUM, END }}), 1 )  \
   METHOD(CLASS, setModule,      ARRAYDEF({{ MOD_SLOT, MOD_ENUM, END }}), 1 )    \
   METHOD(CLASS, isValid,        ARRAYDEF({{ END }}), 1 )                        \
   METHOD(CLASS, equals,         ARRAYDEF({{ LOADOUT, END }}), 1 )               \
   METHOD(CLASS, getWeapon,      ARRAYDEF({{ WEAP_SLOT, END }}), 1 )             \
   METHOD(CLASS, getModule,      ARRAYDEF({{ MOD_SLOT, END }}), 1 )              \

GENERATE_LUA_FUNARGS_TABLE(LoadoutTracker, LUA_METHODS);
GENERATE_LUA_METHODS_TABLE(LoadoutTracker, LUA_METHODS);

#undef LUA_METHODS

const char *LoadoutTracker::luaClassName = "Loadout";     // Class name as it appears to Lua scripts
REGISTER_LUA_CLASS(LoadoutTracker);


S32 LoadoutTracker::lua_setWeapon(lua_State *L)     // setWeapon(i, wep) ==> Set weapon at index i
{
   checkArgList(L, functionArgs, "Loadout", "setWeapon");

   U32 index = (U32) getInt(L, 1);
   WeaponType weapon = (WeaponType) getWeaponType(L, 2);

   mWeapons[index - 1] = weapon;

   return 0;
}


S32 LoadoutTracker::lua_setModule(lua_State *L)     // setModule(i, mod) ==> Set module at index i
{
   checkArgList(L, functionArgs, "Loadout", "setModule");

   U32 index = (U32) getInt(L, 1);
   ShipModule module  = getShipModule(L, 2);

   mModules[index - 1] = module;

   return 0;
}


S32 LoadoutTracker::lua_isValid(lua_State *L)       // isValid() ==> Is loadout config valid?
{
   bool hasSensor = false;

   U32 mod[ShipModuleCount];
   for(S32 i = 0; i < ShipModuleCount; i++)
   {
      for(S32 j = 0; j < i; j++)
         if(mod[j] == mModules[i])     // Duplicate entry!
            return returnBool(L, false);

      mod[i] = mModules[i];

      if(mModules[i] == ModuleSensor)
         hasSensor = true;
   }

   bool hasSpyBug = false;

   U32 weap[ShipWeaponCount];
   for(S32 i = 0; i < ShipWeaponCount; i++)
   {
      for(S32 j = 0; j < i; j++)
         if(weap[j] == mWeapons[i])     // Duplicate entry!
            return returnBool(L, false);
      weap[i] = mWeapons[i];
      if(mWeapons[i] == WeaponSpyBug)
         hasSpyBug = true;
   }

   // Make sure we don't have any invalid combos
   if(hasSpyBug && !hasSensor)
      return returnBool(L, false);

   return returnBool(L, true);
}


S32 LoadoutTracker::lua_equals(lua_State *L)        // equals(Loadout) ==> is loadout the same as Loadout?
{
   checkArgList(L, functionArgs, "Loadout", "equals");

   LoadoutTracker *loadout = luaW_check<LoadoutTracker>(L, 1);

   if(*this == *loadout)
      return returnBool(L, true);

   return returnBool(L, false);
}


S32 LoadoutTracker::lua_getWeapon(lua_State *L)     // getWeapon(i) ==> return weapon at index i (1, 2, 3)
{
   checkArgList(L, functionArgs, "Loadout", "getWeapon");

   WeaponType weapon = getWeaponType(L, 1);

   return returnInt(L, mWeapons[weapon - 1]);
}


S32 LoadoutTracker::lua_getModule(lua_State *L)     // getModule(i) ==> return module at index i (1, 2)
{
   checkArgList(L, functionArgs, "Loadout", "getModule");

   ShipModule module = (ShipModule) getInt(L, 1);

   return returnInt(L, mModules[module - 1]);
}



}
