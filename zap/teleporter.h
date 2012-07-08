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

#ifndef _TELEPORTER_H_
#define _TELEPORTER_H_

#include "SimpleLine.h"    // For SimpleLine def
#include "gameConnection.h"
#include "gameObject.h"
#include "projectile.h"    // For LuaItem
#include "Point.h"
#include "Colors.h"
#include "Engineerable.h"

#include "tnlNetObject.h"

class ship;

namespace Zap
{

class Teleporter;

// Manage destinations for a teleporter
struct DestManager 
{
   private:
      Vector<Point> mDests;
      Teleporter *mOwner;

   public:
      void setOwner(Teleporter *owner);

      S32 getDestCount() const;
      Point getDest(S32 index) const;
      S32 getRandomDest() const;

      void addDest(const Point &dest);
      void delDest(S32 index);

      void resize(S32 count);
      void read(S32 index, BitStream *stream);     // Read a single dest
      void read(BitStream *stream);                // Read a whole list of dests

      void clear();
      /*const*/ Vector<Point> *getDestList() /*const*/;
};


////////////////////////////////////////
////////////////////////////////////////

class Teleporter : public SimpleLine, public Engineerable
{
   typedef SimpleLine Parent;

public:
   enum {
      InitMask      = BIT(0),
      TeleportMask  = BIT(1),
      HealthMask    = BIT(2),
      DestroyedMask = BIT(3),

      TeleporterTriggerRadius = 50,
      TeleporterDelay = 1500,                // Time teleporter remains idle after it has been used
      TeleporterExpandTime = 1350,
      TeleportInExpandTime = 750,
      TeleportInRadius = 120,

      TeleporterExplosionTime = 1000,
   };

   static const S32 TELEPORTER_RADIUS = 75;  // Overall size of the teleporter

private:
   S32 mLastDest;    // Destination of last ship through

   bool mHasExploded;
   F32 mStartingHealth;

   void computeExtent();

   Timer mExplosionTimer;
   bool mFinalExplosionTriggered;

   DestManager mDestManager;

   SafePtr<Ship> mEngineeringShip;

public:
   Teleporter(Point pos = Point(), Point dest = Point(), Ship *engineeringShip = NULL);  // Constructor
   virtual ~Teleporter();                                                                // Destructor

   Teleporter *clone() const;

   U32 timeout;
   U32 mTime;

   U32 mTeleporterDelay;

   S32 getDestCount();
   Point getDest(S32 index);
   void addDest(const Point &dest);

   static bool checkDeploymentPosition(const Point &position, GridDatabase *gb, Ship *ship);

   virtual bool processArguments(S32 argc, const char **argv, Game *game);
   string toString(F32 gridSize) const;
   Rect calcExtents();

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   void damageObject(DamageInfo *theInfo);
   void onDestroyed();
   bool collide(BfObject *otherObject);

   bool getCollisionCircle(U32 state, Point &center, F32 &radius) const;
   bool getCollisionPoly(Vector<Point> &polyPoints) const;

   void idle(BfObject::IdleCallPath path);
   void render();

#ifndef ZAP_DEDICATED
   void doExplosion();
#endif

   void onAddedToGame(Game *theGame);

   TNL_DECLARE_CLASS(Teleporter);

   TNL_DECLARE_RPC(s2cAddDestination, (Point));   
   TNL_DECLARE_RPC(s2cClearDestinations, ());   


   ///// Editor Methods
   Color getEditorRenderColor();

   void renderEditorItem();

   void onAttrsChanging();
   void onGeomChanging();

   void onConstructed();

   bool hasAnyDests();
   void setEndpoint(const Point &point);

   // Some properties about the item that will be needed in the editor
   const char *getOnScreenName();
   const char *getOnDockName();
   const char *getPrettyNamePlural();
   const char *getEditorHelpString();

   bool hasTeam();
   bool canBeHostile();
   bool canBeNeutral();


   ///// Lua Interface

   //                            Fn name     Valid param profiles  Profile count                           
#  define TELEPORTER_LUA_METHOD_TABLE \
      TELEPORTER_LUA_METHOD_ITEM(addDest,    ARRAYDEF({{ PT,  END }}), 1 ) \
      TELEPORTER_LUA_METHOD_ITEM(delDest,    ARRAYDEF({{ INT, END }}), 1 ) \
      TELEPORTER_LUA_METHOD_ITEM(clearDests, ARRAYDEF({{          }}), 0 ) \

   LUAW_DECLARE_CLASS(Teleporter);

   static const char *luaClassName;
   static const luaL_reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   S32 addDest(lua_State *L);
   S32 delDest(lua_State *L);
   S32 clearDests(lua_State *L);
};


};

#endif
