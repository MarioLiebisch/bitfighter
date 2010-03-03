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

#include "gameObject.h"
#include "gameType.h"
#include "gameNetInterface.h"
#include "UI.h"
#include "gameObjectRender.h"
#include "glutInclude.h"
#include "polygon.h"

namespace Zap
{

extern S32 gMaxPolygonPoints;

class LoadoutZone : public LuaPolygonalGameObject
{
private:
   typedef GameObject Parent;

public:
   // Constructor
   LoadoutZone()
   {
      mTeam = 0;
      mNetFlags.set(Ghostable);
      mObjectTypeMask = LoadoutZoneType | CommandMapVisType;
   }

   void render()
   {
      renderLoadoutZone(getGame()->getGameType()->getTeamColor(getTeam()), mPolyBounds, mPolyFill, mCentroid, mLabelAngle);
   }

   S32 getRenderSortValue()
   {
      return -1;
   }

   // Create objects from parameters stored in level file
   bool processArguments(S32 argc, const char **argv)
   {
      if(argc < 7)
         return false;

      mTeam = atoi(argv[0]);     // Team is first arg
      processPolyBounds(argc, argv, 1, getGame()->getGridSize());

      computeExtent();

      return true;
   }

   void onAddedToGame(Game *theGame)
   {
      if(!isGhost())
         setScopeAlways();
   }

   // Bounding box for quick collision-possibility elimination
   void computeExtent()
   {
      Rect extent(mPolyBounds[0], mPolyBounds[0]);
      for(S32 i = 1; i < mPolyBounds.size(); i++)
         extent.unionPoint(mPolyBounds[i]);
      setExtent(extent);
   }

   // More precise boundary for precise collision detection
   bool getCollisionPoly(Vector<Point> &polyPoints)
   {
      for(S32 i = 0; i < mPolyBounds.size(); i++)
         polyPoints.push_back(mPolyBounds[i]);
      return true;
   }

   // Only gets run on the server, never on client
   bool collide(GameObject *hitObject)
   {
      // Anyone can use neutral loadout zones (team == -1)
      if(!isGhost() && (hitObject->getTeam() == getTeam() || getTeam() == -1) && hitObject->getObjectTypeMask() & (ShipType | RobotType))
         getGame()->getGameType()->updateShipLoadout(hitObject);      

      return false;
   }


   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
   {
      stream->write(mTeam);

      Polygon::packUpdate(connection, stream);

      return 0;
   }


   void unpackUpdate(GhostConnection *connection, BitStream *stream)
   {
      stream->read(&mTeam);

      if(Polygon::unpackUpdate(connection, stream))
         computeExtent();
   }

   GameObject *getGameObject() { return this; }          // Return the underlying GameObject

   static const char className[];                        // Class name as it appears to Lua scripts
   static Lunar<LoadoutZone>::RegType methods[];

   TNL_DECLARE_CLASS(LoadoutZone);
};



TNL_IMPLEMENT_NETOBJECT(LoadoutZone);

const char LoadoutZone::className[] = "LoadoutZone";      // Class name as it appears to Lua scripts

// Define the methods we will expose to Lua
Lunar<LoadoutZone>::RegType LoadoutZone::methods[] =
{
   // Standard gameItem methods
   method(LoadoutZone, getClassID),
   method(LoadoutZone, getLoc),
   method(LoadoutZone, getRad),
   method(LoadoutZone, getVel),
   method(LoadoutZone, getTeamIndx),

   {0,0}    // End method list
};


};


