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

#ifndef _GAMEOBJECT_H_
#define _GAMEOBJECT_H_

#include "gridDB.h"        // For DatabaseObject
#include "tnlNetObject.h"
#include "move.h"

#include "Geometry_Base.h"      

struct lua_State;  // or #include "lua.h"

#ifdef TNL_OS_WIN32 
#  pragma warning( disable : 4250)
#endif

namespace TNL{ class BitStream; }


namespace Zap
{

class GridDatabase;
class Game;
class ClientInfo;


// START GAME OBJECT TYPES
// Can have 256 types designated from 0-255

const U8 UnknownTypeNumber = 0;
const U8 PlayerShipTypeNumber = 1;
const U8 BarrierTypeNumber = 2;  // Barrier is different than PolyWall!!
// 3
const U8 LineTypeNumber = 4;
const U8 ResourceItemTypeNumber = 5;
const U8 TextItemTypeNumber = 6;
const U8 ForceFieldTypeNumber = 7;
const U8 LoadoutZoneTypeNumber = 8;
const U8 TestItemTypeNumber = 9;
const U8 FlagTypeNumber = 10;
const U8 SpeedZoneTypeNumber = 11;
const U8 SlipZoneTypeNumber = 12;
const U8 BulletTypeNumber = 13;
const U8 MineTypeNumber = 14;
const U8 SpyBugTypeNumber = 15;
const U8 NexusTypeNumber = 16;
// 17
const U8 RobotShipTypeNumber = 18;
const U8 TeleportTypeNumber = 19;
const U8 GoalZoneTypeNumber = 20;
const U8 AsteroidTypeNumber = 21;
const U8 RepairItemTypeNumber = 22;
const U8 EnergyItemTypeNumber = 23;
const U8 SoccerBallItemTypeNumber = 24;
const U8 WormTypeNumber = 25;
const U8 TurretTypeNumber = 26;
const U8 ForceFieldProjectorTypeNumber = 27;
const U8 PolyWallTypeNumber = 28;
const U8 CircleTypeNumber = 29;
const U8 CoreTypeNumber = 30;

const U8 BotNavMeshZoneTypeNumber = 64;  // separate database

// These are probably used only in editor...
const U8 WallItemTypeNumber = 66;
const U8 WallEdgeTypeNumber = 67;
const U8 WallSegmentTypeNumber = 68;
const U8 ShipSpawnTypeNumber = 69;
const U8 FlagSpawnTypeNumber = 70;
const U8 AsteroidSpawnTypeNumber = 71;
const U8 CircleSpawnTypeNumber = 72;

const U8 DeletedTypeNumber = 252;
// 255

// Derived Types are determined by function
bool isEngineeredType(U8 x);
bool isShipType(U8 x);
bool isProjectileType(U8 x);
bool isGrenadeType(U8 x);
bool isWithHealthType(U8 x);
bool isForceFieldDeactivatingType(U8 x);
bool isDamageableType(U8 x);
bool isMotionTriggerType(U8 x);
bool isTurretTargetType(U8 x);
bool isCollideableType(U8 x);                  // Move objects bounce off of these
bool isForceFieldCollideableType(U8 x);
bool isWallType(U8 x);
bool isWallItemType(U8 x);
bool isLineItemType(U8 x);
bool isWeaponCollideableType(U8 x);
bool isAsteroidCollideableType(U8 x);
bool isFlagCollideableType(U8 x);
bool isFlagOrShipCollideableType(U8 x);
bool isVisibleOnCmdrsMapType(U8 x);
bool isVisibleOnCmdrsMapWithSensorType(U8 x);

bool isAnyObjectType(U8 x);
// END GAME OBJECT TYPES

typedef bool (*TestFunc)(U8);

const S32 gSpyBugRange = 300;                // How far can a spy bug see?

class GameObject;
class Game;
class GameConnection;


////////////////////////////////////////
////////////////////////////////////////

enum DamageType
{
   DamageTypePoint,
   DamageTypeArea,
};

struct DamageInfo
{
   Point collisionPoint;
   Point impulseVector;
   F32 damageAmount;
   F32 damageSelfMultiplier;
   DamageType damageType;       // see enum above!
   GameObject *damagingObject;  // see class below!

   DamageInfo();  // Constructor
};


////////////////////////////////////////
////////////////////////////////////////

class GeometryContainer
{
private:
   Geometry *mGeometry;

public:
   GeometryContainer();                                     // Constructor
   GeometryContainer(const GeometryContainer &container);   // Copy constructor
   ~GeometryContainer();                                    // Destructor

   Geometry *getGeometry() const;
   const Geometry *getConstGeometry() const;
   void setGeometry(Geometry *geometry);

   const Vector<Point> *getOutline() const;
   const Vector<Point> *getFill() const;
   Point getVert(S32 index) const;
   string geomToString(F32 gridSize) const; 
};


////////////////////////////////////////
////////////////////////////////////////

// Interface class that feeds GameObject and EditorObject -- these things are common to in-game and editor instances of an object
class BfObject : public DatabaseObject
{
private:
   GeometryContainer mGeometry;

protected:
   Game *mGame;
   S32 mTeam;

public:
   BfObject();                // Constructor
   virtual ~BfObject();       // Provide virtual destructor

   S32 getTeam();
   void setTeam(S32 team);

   Game *getGame() const;
   void setNewGeometry(GeomType geomType);


   virtual void addToGame(Game *game, GridDatabase *database);
   virtual void removeFromGame();
   void clearGame();

   virtual bool processArguments(S32 argc, const char**argv, Game *game);

   virtual Point getPos() const;
   virtual void setPos(const Point &pos);

   void onPointsChanged();
   void updateExtentInDatabase();

   // Geometric operations
   void unselectVerts();
   bool vertSelected(S32 vertIndex);
   S32 getVertCount() const;

   GeomType getGeomType();

   virtual Point getVert(S32 index) const;               // Overridden by MoveObject
   virtual void setVert(const Point &pos, S32 index);    // Overridden by MoveObject

   bool anyVertsSelected();
   void selectVert(S32 vertIndex);
   void aselectVert(S32 vertIndex);   // Select another vertex (remember cmdline ArcInfo?)
   void unselectVert(S32 vertIndex);

   S32 getMinVertCount() const;       // Minimum number of vertices geometry needs to be viable
   void clearVerts();
   bool addVert(const Point &point, bool ignoreMaxPointsLimit = false);
   bool addVertFront(Point vert);


   const Vector<Point> *getOutline() const;
   const Vector<Point> *getFill()    const;
   bool deleteVert(S32 vertIndex);
   bool insertVert(Point vertex, S32 vertIndex);

   // These functions are declered in Geometry.cpp
   void rotateAboutPoint(const Point &center, F32 angle);
   void flip(F32 center, bool isHoriz);                   // Do a horizontal or vertical flip about line at center
   void scale(const Point &center, F32 scale);


   Point getCentroid();
   F32 getLabelAngle();
                                                    
   void packGeom(GhostConnection *connection, BitStream *stream);
   void unpackGeom(GhostConnection *connection, BitStream *stream);

   string geomToString(F32 gridSize) const;
   void readGeom(S32 argc, const char **argv, S32 firstCoord, F32 gridSize);

   //void newGeomCopy();

   virtual Rect calcExtents();

   void disableTriangulation();
};


////////////////////////////////////////
////////////////////////////////////////

class ClientInfo;

class GameObject : public virtual BfObject, public NetObject
{
   typedef NetObject Parent;

private:
   SafePtr<GameConnection> mControllingClient;     // Only has meaning on the server, will be null on the client
   SafePtr<ClientInfo> mOwner;
   U32 mDisableCollisionCount;                     // No collisions when > 0, use of counter allows "nested" collision disabling

   U32 mCreationTime;

protected:
   Move mLastMove;      // The move for the previous update
   Move mCurrentMove;   // The move for the current update
   StringTableEntry mKillString;     // Alternate descr of what shot projectile (e.g. "Red turret"), used when shooter is not a ship or robot

public:
   GameObject();                // Constructor
   virtual ~GameObject();       // Destructor

   virtual void addToGame(Game *game, GridDatabase *database);       // BotNavMeshZone has its own addToGame
   virtual void onAddedToGame(Game *game);

   void markAsGhost();

   virtual bool isMoveObject();
   virtual Point getVel();

   U32 getCreationTime();
   void setCreationTime(U32 creationTime);

   void deleteObject(U32 deleteTimeInterval = 0);
   
   StringTableEntry getKillString();

   F32 getRating();
   S32 getScore();

   enum MaskBits {
      //InitialMask = BIT(0),
      FirstFreeMask = BIT(0)
   };

   GameObject *findObjectLOS(U8 typeNumber, U32 stateIndex, Point rayStart, Point rayEnd, float &collisionTime, Point &collisionNormal);
   GameObject *findObjectLOS(TestFunc, U32 stateIndex, Point rayStart, Point rayEnd, float &collisionTime, Point &collisionNormal);

   bool isControlled();

   SafePtr<GameConnection> getControllingClient();
   void setControllingClient(GameConnection *c);         // This only gets run on the server

   void setOwner(ClientInfo *clientInfo);
   ClientInfo *getOwner();

   F32 getUpdatePriority(NetObject *scopeObject, U32 updateMask, S32 updateSkips);

   void findObjects(U8 typeNumber, Vector<DatabaseObject *> &fillVector, const Rect &extents);
   void findObjects(TestFunc, Vector<DatabaseObject *> &fillVector, const Rect &extents);

   virtual S32 getRenderSortValue();

   Rect getBounds(U32 stateIndex) const;

   const Move &getCurrentMove();
   const Move &getLastMove();
   void setCurrentMove(const Move &theMove);
   void setLastMove(const Move &theMove);

   // Render is called twice for every object that is in the
   // render list.  By default GameObject will call the render()
   // method one time (when layerIndex == 0).
   virtual void render(S32 layerIndex);
   virtual void render();

   enum IdleCallPath {
      ServerIdleMainLoop,              // Idle called from top-level idle loop on server
      ServerIdleControlFromClient,
      ClientIdleMainRemote,            // On client, when object is not our control object
      ClientIdleControlMain,           // On client, when object is our control object
      ClientIdleControlReplay,
   };

   virtual void idle(IdleCallPath path);

   virtual void writeControlState(BitStream *stream);
   virtual void readControlState(BitStream *stream);
   virtual F32 getHealth();
   virtual bool isDestroyed();

   virtual void controlMoveReplayComplete();

   void writeCompressedVelocity(Point &vel, U32 max, BitStream *stream);
   void readCompressedVelocity(Point &vel, U32 max, BitStream *stream);

   virtual bool collide(GameObject *hitObject);
   virtual Vector<Point> getReapirLocations();     // Gets location(s) where repair rays should be rendered while object is being repaired

   S32 radiusDamage(Point pos, S32 innerRad, S32 outerRad, TestFunc objectTypeTest, DamageInfo &info, F32 force = 2000);
   virtual void damageObject(DamageInfo *damageInfo);

   void onGhostAddBeforeUpdate(GhostConnection *theConnection);
   bool onGhostAdd(GhostConnection *theConnection);
   void disableCollision();
   void enableCollision();
   bool isCollisionEnabled();

   bool collisionPolyPointIntersect(Point point);
   bool collisionPolyPointIntersect(Vector<Point> points);
   bool collisionPolyPointIntersect(Point center, F32 radius);

   void setScopeAlways();

   S32 getTeamIndx(lua_State *L);      // Return item team to Lua
   virtual void push(lua_State *L);    // Lua-aware classes will implement this
   
   void readThisTeam(BitStream *stream);
   void writeThisTeam(BitStream *stream);
};


};

#endif

