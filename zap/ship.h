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

#ifndef _SHIP_H_
#define _SHIP_H_

#include "item.h"    // for Item and LuaItem
#include "gameObject.h"
#include "moveObject.h"
#include "luaObject.h"
#include "SoundEffect.h"
#include "Timer.h"
#include "shipItems.h"
#include "gameWeapons.h"

#ifndef ZAP_DEDICATED
#include "sparkManager.h"
#endif

namespace Zap
{
class SpeedZone;

static const S32 ShipModuleCount = 2;                // Modules a ship can carry
static const S32 ShipWeaponCount = 3;                // Weapons a ship can carry
static const U32 DefaultLoadout[] = { ModuleBoost, ModuleShield, WeaponPhaser, WeaponMine, WeaponBurst };


//////////////////////////////////////////////

class LuaShip : public LuaItem
{

private:
   SafePtr<Ship> thisShip;    // Reference to actual C++ ship object

public:

   LuaShip(Ship *ship);   // C++ constructor
   LuaShip();             // C++ default constructor ==> not used.  Constructor with Ship (above) used instead
   LuaShip(lua_State *L); // Lua constructor ==> not used.  Class only instantiated from C++.

   virtual ~LuaShip();    // Destructor

   static S32 id;
   S32 mId;

   static const char className[];

   static Lunar<LuaShip>::RegType methods[];

   virtual S32 getClassID(lua_State *L);    // Robot will override this

   S32 isAlive(lua_State *L);
   S32 getAngle(lua_State *L);
   S32 getLoc(lua_State *L);
   S32 getRad(lua_State *L);
   S32 getVel(lua_State *L);
   S32 hasFlag(lua_State *L);

   S32 getEnergy(lua_State *L);  // Return ship's energy as a fraction between 0 and 1
   S32 getHealth(lua_State *L);  // Return ship's health as a fraction between 0 and 1

   S32 getFlagCount(lua_State *L);

   S32 getTeamIndx(lua_State *L);
   S32 getPlayerInfo(lua_State *L);
   S32 isModActive(lua_State *L);
   S32 getMountedItems(lua_State *L);
   S32 getCurrLoadout(lua_State *L);
   S32 getReqLoadout(lua_State *L);

   GameObject *getGameObject();
   const char *getClassName() const;

   void push(lua_State *L);      // Push item onto stack

   S32 getActiveWeapon(lua_State *L);                // Get WeaponIndex for current weapon

   virtual Ship *getObj();       // Access to underlying object, robot will override
};

////////////////////////////////////////
////////////////////////////////////////

class Statistics;
class ClientInfo;

// class derived_class_name: public base_class_name
class Ship : public MoveObject
{
   typedef MoveObject Parent;

private:
   bool isBusy;
   bool mIsRobot;

   U32 mRespawnTime;

   // Find objects of specified type that may be under the ship, and put them in fillVector
   void findObjectsUnderShip(U8 typeNumber);

protected:
   ClientInfo *mClientInfo;

   bool mModulePrimaryActive[ModuleCount];       // Is the primary component of the module active at this moment?
   bool mModuleSecondaryActive[ModuleCount];     // Is the secondary component of the module active?

   ShipModule mModule[ShipModuleCount];   // Modules ship is carrying
   WeaponType mWeapon[ShipWeaponCount];

   void initialize(Point &pos);           // Some initialization code needed by both bots and ships
   Point mSpawnPoint;                     // Where ship or robot spawned.  Will only be valid on server, client doesn't currently get this.

public:
   static const S32 CollisionRadius = 24;
   static const S32 RepairRadius = 65;
   static const U32 SpawnShieldTime = 5000;        // Time spawn shields are active
   static const U32 SpawnShieldFlashTime = 1500;   // Time at which shields start to flash

   enum {
      MaxVelocity = 450,        // points per second
      Acceleration = 2500,      // points per second per second
      BoostMaxVelocity = 700,   // points per second
      BoostAcceleration = 5000, // points per second per second

      RepairDisplayRadius = 18,
      VisibilityRadius = 30,
      KillDeleteDelay = 1500,
      ExplosionFadeTime = 300,
      MaxControlObjectInterpDistance = 200,
      TrailCount = 2,
      EnergyMax = 100000,
      EnergyRechargeRate = 6000,          // How much energy/second
      EnergyRechargeRateWhenIdle = 8000, // How much energy/second if the ship isn't moving
      EnergyRechargeRateInLoadoutZone = 12000,  // How much energy/second if the ship is in a loadout zone
      EnergyShieldHitDrain = 20000,       // Energy loss when shields stop a projectile (currently disabled)
      EnergyCooldownThreshold = 15000,
      WeaponFireDecloakTime = 350,
      SensorZoomTime = 300,
      SensorInitialEnergyUsage = 5000,
      CloakFadeTime = 300,
      CloakCheckRadius = 200,
      RepairHundredthsPerSecond = 16,
      MaxEngineerDistance = 100,
      WarpFadeInTime = 500,
   };

   enum MaskBits {
      InitialMask = BIT(0),         // Initial ship position
      PositionMask = BIT(1),        // Ship position to be sent
      MoveMask = BIT(2),            // New user input
      WarpPositionMask = BIT(3),    // When ship makes a big jump in position
      ExplosionMask = BIT(4),
      HealthMask = BIT(5),
      ModulePrimaryMask = BIT(6),   // Is module primary component active
      ModuleSecondaryMask = BIT(7), // Is module secondary component active
      LoadoutMask = BIT(8),
      RespawnMask = BIT(9),         // For when robots respawn
      TeleportMask = BIT(10),        // Ship has just teleported
      ChangeTeamMask = BIT(11),     // Used for when robots change teams
      SpawnShieldMask = BIT(12),    // Used for when robots change teams
   };

   enum SensorStatus {
      SensorStatusPassive,
      SensorStatusActive,
      SensorStatusOff
   };

   S32 mFireTimer;
   Timer mWarpInTimer;
   F32 mHealth;
   S32 mEnergy;
   bool mCooldownNeeded;
   U32 mSensorStartTime;
   Point mImpulseVector;

   SensorStatus mSensorStatus;
   SensorStatus mPrevSensorStatus;

   F32 getSlipzoneSpeedMoficationFactor();
   void updateSensorStatus(SensorStatus sensorStatus, SensorStatus prevSensorStatus);

   SFXHandle mModuleSound[ModuleCount];

   U32 mActiveWeaponIndx;                 // Index of selected weapon on ship

   void selectWeapon();                   // Select next weapon
   void selectWeapon(U32 weaponIndex);    // Select weapon by index
   WeaponType getWeapon(U32 indx);        // Returns weapon in slot indx
   ShipModule getModule(U32 indx);        // Returns module in slot indx


   Timer mSensorZoomTimer;
   Timer mWeaponFireDecloakTimer;
   Timer mCloakTimer;
   Timer mSpawnShield;
   Timer mModuleSecondaryCooldownTimer[ModuleCount];

#ifndef ZAP_DEDICATED
   U32 mSparkElapsed;
   S32 mLastTrailPoint[TrailCount];  // TrailCount = 2
   FXTrail mTrail[TrailCount];
#endif

   F32 mass;            // Mass of ship, not used
   bool hasExploded;

   Vector<SafePtr<MoveItem> > mMountedItems;
   Vector<SafePtr<GameObject> > mRepairTargets;

   virtual void render(S32 layerIndex);
   void calcThrustComponents(F32 *thrust);

   // Constructor
   Ship(ClientInfo *clientInfo = NULL, S32 team = -1, Point p = Point(0,0), F32 m = 1.0, bool isRobot = false);      
   
   ~Ship();           // Destructor

   F32 getHealth();
   S32 getEnergy();
   F32 getEnergyFraction();     // Only used by bots
   S32 getMaxEnergy();
   void changeEnergy(S32 deltaEnergy);

   void onGhostRemove();

   bool isModulePrimaryActive(ShipModule mod);
   bool isModuleSecondaryActive(ShipModule mod);

   void engineerBuildObject();

   bool hasModule(ShipModule mod);

   bool isDestroyed();
   bool isItemMounted();    // <== unused
   bool isVisible();

   S32 carryingFlag();     // Returns index of first flag, or NO_FLAG if ship has no flags
   S32 getFlagCount();     // Returns the number of flags ship is carrying

   bool isCarryingItem(U8 objectType);
   MoveItem *unmountItem(U8 objectType);

   F32 getSensorZoomFraction();
   Point getAimVector();

   void getLoadout(Vector<U32> &loadout);
   void setLoadout(const Vector<U32> &loadout, bool silent = false);
   bool isLoadoutSameAsCurrent(const Vector<U32> &loadout);
   void setDefaultLoadout();           // Set the ship's loadout to the default values

   ClientInfo *getClientInfo();
   static string loadoutToString(const Vector<U32> &loadout);
   static bool stringToLoadout(string loadoutStr, Vector<U32> &loadout);


   virtual void idle(IdleCallPath path);

   virtual void processMove(U32 stateIndex);

   WeaponType getSelectedWeapon();   // Return currently selected weapon
   U32 getSelectedWeaponIndex();     // Return index of currently selected weapon (0, 1, 2)

   void processWeaponFire();
   void processModules();
   void updateModuleSounds();
   void emitMovementSparks();
   bool findRepairTargets();
   void repairTargets();

   void controlMoveReplayComplete();
   void onAddedToGame(Game *game);

   void emitShipExplosion(Point pos);
   //void setActualPos(Point p);
   void setActualPos(Point p, bool warp);
   void activateModulePrimary(U32 indx);    // Activate the specified module primary component for the current move
   void activateModuleSecondary(U32 indx);  // Activate the specified module secondary component for the current move

   SensorStatus getSensorStatus();
   SensorStatus getPreviousSensorStatus();

   virtual void kill(DamageInfo *theInfo);
   virtual void kill();

   virtual void damageObject(DamageInfo *theInfo);

   static void computeMaxFireDelay();

   void writeControlState(BitStream *stream);
   void readControlState(BitStream *stream);

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);
   F32 getUpdatePriority(NetObject *scopeObject, U32 updateMask, S32 updateSkips);

   virtual bool processArguments(S32 argc, const char **argv, Game *game);

   LuaShip luaProxy;                                  // Our Lua proxy object
   bool isRobot();
   void push(lua_State *L);                           // Push a LuaShip proxy object onto the stack

   GameObject *isInZone(U8 zoneType);     // Return whether the ship is currently in a zone of the specified type, and which one
   //GameObject *isInZone(GameObject *zone);
   DatabaseObject *isOnObject(U8 objectType); // Returns the object in question if this ship is on an object of type objectType

   bool isOnObject(GameObject *object);               // Return whether or not ship is sitting on a particular item

   TNL_DECLARE_CLASS(Ship);
};


};

#endif

