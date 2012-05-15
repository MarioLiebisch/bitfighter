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

#ifndef _PROJECTILE_H_
#define _PROJECTILE_H_

#include "gameType.h"
#include "gameObject.h"
#include "item.h"
#include "gameWeapons.h"
#include "SoundSystem.h" // for enum SFXProfiles

namespace Zap
{

class Ship;

class LuaProjectile : public LuaObject
{
public:
   LuaProjectile();                      //  C++ constructor

   LuaProjectile(lua_State *L);          //  Lua constructor

   static const char className[];
   static Lunar<LuaProjectile>::RegType methods[];
   const char *getClassName() const;

   virtual S32 getWeapon(lua_State *L);  // Return info about the weapon/projectile

   //============================
   // LuaItem interface

   virtual S32 getClassID(lua_State *L); // Object's class

   S32 getLoc(lua_State *L);     // Center of item (returns point)
   S32 getRad(lua_State *L);             // Radius of item (returns number)
   S32 getVel(lua_State *L);             // Speed of item (returns point)

   void push(lua_State *L);
};


////////////////////////////////////
////////////////////////////////////

// Basic bullet object
class Projectile : public BfObject, public LuaProjectile
{
   typedef BfObject Parent;

private:
   static const S32 COMPRESSED_VELOCITY_MAX = 2047;

protected:
   enum MaskBits {
      InitialMask   = Parent::FirstFreeMask << 0,
      ExplodedMask  = Parent::FirstFreeMask << 1,
      PositionMask  = Parent::FirstFreeMask << 2,
      FirstFreeMask = Parent::FirstFreeMask << 3
   };

   Point mVelocity;

public:
   U32 mTimeRemaining;
   ProjectileType mType;
   WeaponType mWeaponType;
   bool collided;
   bool hitShip;
   bool alive;
   bool hasBounced;
   SafePtr<BfObject> mShooter;

   Projectile(WeaponType type = WeaponPhaser, Point pos = Point(), Point vel = Point(), BfObject *shooter = NULL);    // Constructor
   ~Projectile();                                                                                                     // Destructor

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   void handleCollision(BfObject *theObject, Point collisionPoint);

   void idle(BfObject::IdleCallPath path);
   void damageObject(DamageInfo *info);
   void explode(BfObject *hitObject, Point p);

   virtual Point getRenderVel() const;
   virtual Point getActualVel() const;

   void render();
   void renderItem(const Point &pos);

   TNL_DECLARE_CLASS(Projectile);

   // Lua interface
   S32 getRad(lua_State *L);     // Radius of item (returns number)
   S32 getVel(lua_State *L);     // Speed of item (returns point)
   S32 getTeamIndx(lua_State *L);   // Return team of shooter     

   S32 getWeapon(lua_State *L);  // Return which type of weapon this is

   void push(lua_State *L);
};


// Basic burst object, and the base clase used for both mines and spybugs
class BurstProjectile : public MoveItem, public LuaProjectile
{
private:
   typedef MoveItem Parent;

public:
   BurstProjectile(Point pos = Point(), Point vel = Point(), BfObject *shooter = NULL);     // Constructor
   ~BurstProjectile();                                                                        // Destructor

   enum Constants
   {
      ExplodeMask = MoveItem::FirstFreeMask,
      FirstFreeMask = ExplodeMask << 1,
   };

   static const S32 InnerBlastRadius = 100;
   static const S32 OuterBlastRadius = 250;

   S32 mTimeRemaining;
   bool exploded;
   bool collide(BfObject *otherObj);   // Things (like bullets) can collide with grenades

   WeaponType mWeaponType;
   void renderItem(const Point &pos);
   void idle(IdleCallPath path);
   void damageObject(DamageInfo *damageInfo);
   void explode(Point p, WeaponType weaponType);
   StringTableEntry mSetBy;      // Who laid the mine/spy bug?

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   TNL_DECLARE_CLASS(BurstProjectile);

   // Lua interface
   BurstProjectile(lua_State *L);            //  Lua constructor

   // Need to provide these methods to deal with the complex web of inheritence... these come from both
   // Item and LuaProjectile, and by providing these, everything works.
   S32 getRad(lua_State *L);       // Radius of item (returns number)
   S32 getVel(lua_State *L);       // Speed of item (returns point)

   S32 getWeapon(lua_State *L);    // Return which type of weapon this is

   void push(lua_State *L);
};


////////////////////////////////////////
////////////////////////////////////////

class Mine : public BurstProjectile
{
   typedef BurstProjectile Parent;

public:
   enum Constants
   {
      ArmedMask = BurstProjectile::FirstFreeMask,
      SensorRadius     = 50,
   };

   static const S32 InnerBlastRadius = 100;
   static const S32 OuterBlastRadius = 250;

   Mine(Point pos = Point(), Ship *owner=NULL);    // Constructor
   ~Mine();                                        // Destructor
   Mine *clone() const;

   bool mArmed;
   SafePtr<GameConnection> mOwnerConnection;
   bool collide(BfObject *otherObj);
   void idle(IdleCallPath path);

   void damageObject(DamageInfo *damageInfo);
   void renderItem(const Point &pos);

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   TNL_DECLARE_CLASS(Mine);

   /////
   // Editor methods
   void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled);
   void renderDock();

   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();
   bool hasTeam();
   bool canBeHostile();
   bool canBeNeutral();

   string toString(F32 gridSize) const;

   /////
   // Lua interface
   Mine(lua_State *L);            //  Lua constructor

   static const char className[];
   static Lunar<Mine>::RegType methods[];

   virtual S32 getClassID(lua_State *L); // Object's class

   S32 getRad(lua_State *L);     // Radius of item (returns number)
   S32 getVel(lua_State *L);     // Speed of item (returns point)

   S32 getWeapon(lua_State *L);  // Return which type of weapon this is

   void push(lua_State *L);
};


////////////////////////////////////////
////////////////////////////////////////

class SpyBug : public BurstProjectile
{
   typedef BurstProjectile Parent;

public:
   SpyBug(Point pos = Point(), Ship *owner = NULL);      // Constructor
   ~SpyBug();                                            // Destructor
   SpyBug *clone() const;

   bool processArguments(S32 argc, const char **argv, Game *game);
   void onAddedToGame(Game *theGame);


   SafePtr<GameConnection> mOwnerConnection;
   bool collide(BfObject *otherObj);
   void idle(IdleCallPath path);

   void damageObject(DamageInfo *damageInfo);
   void renderItem(const Point &pos);

   bool isVisibleToPlayer(S32 playerTeam, StringTableEntry playerName, bool isTeamGame);

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   TNL_DECLARE_CLASS(SpyBug);

   /////
   // Editor methods
   void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled);
   void renderDock();

   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();
   bool hasTeam();
   bool canBeHostile();
   bool canBeNeutral();

   string toString(F32 gridSize) const;

   /////
   // Lua interface
   SpyBug(lua_State *L);            //  Lua constructor

   static const char className[];
   static Lunar<SpyBug>::RegType methods[];

   virtual S32 getClassID(lua_State *L); // Object's class

   S32 getRad(lua_State *L);     // Radius of item (returns number)
   S32 getVel(lua_State *L);     // Speed of item (returns point)

   S32 getWeapon(lua_State *L);  // Return which type of weapon this is

   void push(lua_State *L);
};



};
#endif

