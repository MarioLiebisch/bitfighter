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

#ifndef _ENGINEEREDITEM_H_
#define _ENGINEEREDITEM_H_

#include "gameObject.h"
#include "item.h"
#include "moveObject.h"    // For MoveItem
#include "barrier.h"

namespace Zap
{

class EngineeredItem : public Item 
{
private:
   typedef Item Parent;

   void computeBufferForBotZone(Vector<Point> &zonePoints);    // Server only
   Vector<Point> mBufferedObjectPointsForBotZone;              // Only populated on the server

#ifndef ZAP_DEDICATED
   static EditorAttributeMenuUI *mAttributeMenuUI;      // Menu for text editing; since it's static, don't bother with smart pointer
#endif

   virtual F32 getSelectionOffsetMagnitude() = 0;       // Provides base magnitude for getEditorSelectionOffset()

protected:
   F32 mHealth;
   SafePtr<MoveItem> mResource;
   Point mAnchorNormal;
   bool mIsDestroyed;
   S32 mOriginalTeam;
   bool mEngineered;

   bool mSnapped;             // Item is snapped to a wall

   S32 mHealRate;             // Rate at which items will heal themselves, defaults to 0
   Timer mHealTimer;          // Timer for tracking mHealRate

   Vector<Point> mCollisionPolyPoints;    // Used on server, also used for rendering on client
   void computeObjectGeometry();          // Populates mCollisionPolyPoints


   WallSegment *mMountSeg;    // Segment we're mounted to in the editor (don't care in the game)

   enum MaskBits
   {
      InitialMask = Parent::FirstFreeMask << 0,
      HealthMask = Parent::FirstFreeMask << 1,
      TeamMask = Parent::FirstFreeMask << 2,
      FirstFreeMask = Parent::FirstFreeMask << 3,
   };

public:
   EngineeredItem(S32 team = -1, Point anchorPoint = Point(), Point anchorNormal = Point());
   virtual bool processArguments(S32 argc, const char **argv, Game *game);

   virtual void onAddedToGame(Game *theGame);

   static const S32 MAX_SNAP_DISTANCE = 100;    // Max distance to look for a mount point

   void setResource(MoveItem *resource);
   static bool checkDeploymentPosition(const Vector<Point> &thisBounds, GridDatabase *gb);
   void computeExtent();

   virtual void onDestroyed();
   virtual void onDisabled();
   virtual void onEnabled();
   virtual bool isTurret();

   virtual void getObjectGeometry(const Point &anchor, const Point &normal, Vector<Point> &geom) const;

   void setPos(Point p);


#ifndef ZAP_DEDICATED
   Point getEditorSelectionOffset(F32 currentScale);
#endif

   bool isEnabled();    // True if still active, false otherwise

   void explode();
   bool isDestroyed();
   void setEngineered(bool isEngineered);
   bool isEngineered(); // Was this engineered py a player?

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   void damageObject(DamageInfo *damageInfo);
   bool collide(GameObject *hitObject);
   F32 getHealth();
   void healObject(S32 time);
   Point mountToWall(const Point &pos, WallSegmentManager *wallSegmentManager);

   void onGeomChanged();

   const Vector<Point> *getBufferForBotZone();

   // Figure out where to put our turrets and forcefield projectors.  Will return NULL if no mount points found.
   static DatabaseObject *findAnchorPointAndNormal(GridDatabase *db, const Point &pos, F32 snapDist, bool format, 
                                                   Point &anchor, Point &normal);

   static DatabaseObject *findAnchorPointAndNormal(GridDatabase *db, const Point &pos, F32 snapDist, 
                                                   bool format, TestFunc testFunc, Point &anchor, Point &normal);

   void setAnchorNormal(const Point &nrml);
   WallSegment *getMountSegment();
   void setMountSegment(WallSegment *mountSeg);

   // These methods are overriden in ForceFieldProjector
   virtual WallSegment *getEndSegment();
   virtual void setEndSegment(WallSegment *endSegment);

   //// Is item sufficiently snapped?  
   void setSnapped(bool snapped);


   /////
   // Editor stuff
   virtual string toString(F32 gridSize) const;

#ifndef ZAP_DEDICATED
   // These four methods are all that's needed to add an editable attribute to a class...
   EditorAttributeMenuUI *getAttributeMenu();
   void startEditingAttrs(EditorAttributeMenuUI *attributeMenu);    // Called when we start editing to get menus populated
   void doneEditingAttrs(EditorAttributeMenuUI *attributeMenu);     // Called when we're done to retrieve values set by the menu

   virtual string getAttributeString();
#endif

   /////
   // LuaItem interface
   // S32 getLoc(lua_State *L);   ==> Will be implemented by derived objects
   // S32 getRad(lua_State *L);   ==> Will be implemented by derived objects
   S32 getVel(lua_State *L);

   // More Lua methods that are inherited by turrets and forcefield projectors
   S32 getTeamIndx(lua_State *L);
   S32 getHealth(lua_State *L);
   S32 isActive(lua_State *L);
   S32 getAngle(lua_State *L);
};


class ForceField : public GameObject
{
   typedef GameObject Parent;

private:
   Point mStart, mEnd;
   Timer mDownTimer;
   bool mFieldUp;

protected:
   enum MaskBits {
      InitialMask   = Parent::FirstFreeMask << 0,
      StatusMask    = Parent::FirstFreeMask << 1,
      FirstFreeMask = Parent::FirstFreeMask << 2
   };

public:
   static const S32 FieldDownTime = 250;
   static const S32 MAX_FORCEFIELD_LENGTH = 2500;

   static const F32 ForceFieldHalfWidth;

   ForceField(S32 team = -1, Point start = Point(), Point end = Point());
   bool collide(GameObject *hitObject);
   bool intersects(ForceField *forceField);     // Return true if forcefields intersect
   void idle(GameObject::IdleCallPath path);


   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   bool getCollisionPoly(Vector<Point> &polyPoints) const;

   void getGeom(Vector<Point> &geom) const;
   static void getGeom(const Point &start, const Point &end, Vector<Point> &points, F32 scaleFact = 1);
   static bool findForceFieldEnd(GridDatabase *db, const Point &start, const Point &normal, 
                                 Point &end, DatabaseObject **collObj);

   void render();
   S32 getRenderSortValue();

   void getForceFieldStartAndEndPoints(Point &start, Point &end);

   TNL_DECLARE_CLASS(ForceField);
};


class ForceFieldProjector : public EngineeredItem
{
private:
   typedef EngineeredItem Parent;
   SafePtr<ForceField> mField;
   WallSegment *mForceFieldEndSegment;
   Point forceFieldEnd;

   void getObjectGeometry(const Point &anchor, const Point &normal, Vector<Point> &geom) const;

   F32 getSelectionOffsetMagnitude();

public:
   static const S32 defaultRespawnTime = 0;

   ForceFieldProjector(S32 team = -1, Point anchorPoint = Point(), Point anchorNormal = Point());  // Constructor
   ForceFieldProjector *clone() const;
   
   bool getCollisionPoly(Vector<Point> &polyPoints) const;
   
   static void getForceFieldProjectorGeometry(const Point &anchor, const Point &normal, Vector<Point> &geom);
   static Point getForceFieldStartPoint(const Point &anchor, const Point &normal, F32 scaleFact = 1);

   // Get info about the forcfield that might be projected from this projector
   void getForceFieldStartAndEndPoints(Point &start, Point &end);

   WallSegment *getEndSegment();
   void setEndSegment(WallSegment *endSegment);

   void onAddedToGame(Game *theGame);
   void idle(GameObject::IdleCallPath path);

   void render();
   void onEnabled();
   void onDisabled();

   TNL_DECLARE_CLASS(ForceFieldProjector);

   // Some properties about the item that will be needed in the editor
   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();
   bool hasTeam();
   bool canBeHostile();
   bool canBeNeutral();

   void renderDock();
   void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled);

   void onItemDragging();   // Item is being actively dragged

   void onGeomChanged();
   void findForceFieldEnd();                      // Find end of forcefield in editor

   ///// Lua Interface

   ForceFieldProjector(lua_State *L);             //  Lua constructor

   static const char className[];

   static Lunar<ForceFieldProjector>::RegType methods[];

   S32 getClassID(lua_State *L);
   void push(lua_State *L);

   // LuaItem methods
   //S32 getRad(lua_State *L) { return returnInt(L, radius); }
   S32 getLoc(lua_State *L);
};


////////////////////////////////////////
////////////////////////////////////////

class Turret : public EngineeredItem
{
   typedef EngineeredItem Parent;

private:
   Timer mFireTimer;
   F32 mCurrentAngle;

   F32 getSelectionOffsetMagnitude();

public:
   Turret(S32 team = -1, Point anchorPoint = Point(), Point anchorNormal = Point(1, 0));     // Constructor
   Turret *clone() const;

   S32 mWeaponFireType;
   bool processArguments(S32 argc, const char **argv, Game *game);
   string toString(F32 gridSize) const;

   static const S32 defaultRespawnTime = 0;

   static const S32 TURRET_OFFSET = 15;               // Distance of the turret's render location from it's attachment location
                                                      // Also serves as radius of circle of turret's body, where the turret starts
   static const S32 TurretTurnRate = 4;               // How fast can turrets turn to aim?
   static const S32 TurretPerceptionDistance = 800;   // Area to search for potential targets...

   static const S32 AimMask = Parent::FirstFreeMask;


   void getObjectGeometry(const Point &anchor, const Point &normal, Vector<Point> &polyPoints) const;
   static void getTurretGeometry(const Point &anchor, const Point &normal, Vector<Point> &polyPoints);
   
   bool getCollisionPoly(Vector<Point> &polyPoints) const;

   F32 getEditorRadius(F32 currentScale);

   void render();
   void idle(IdleCallPath path);
   void onAddedToGame(Game *theGame);
   bool isTurret();

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   TNL_DECLARE_CLASS(Turret);

   /////
   // Some properties about the item that will be needed in the editor
   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();
   bool hasTeam();
   bool canBeHostile();
   bool canBeNeutral();

   void onGeomChanged();
   void onItemDragging();

   void renderDock();
   void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled);

   ///// 
   //Lua Interface

   Turret(lua_State *L);             //  Lua constructor
   static const char className[];
   static Lunar<Turret>::RegType methods[];

   S32 getClassID(lua_State *L);
   void push(lua_State *L);

   // LuaItem methods
   S32 getRad(lua_State *L);
   S32 getLoc(lua_State *L);
   S32 getAngleAim(lua_State *L);

};


////////////////////////////////////////
////////////////////////////////////////


class EngineerModuleDeployer
{
private:
   Point mDeployPosition, mDeployNormal;
   string mErrorMessage;

public:
   // Check potential deployment position
   bool canCreateObjectAtLocation(GridDatabase *database, Ship *ship, U32 objectType);

   bool deployEngineeredItem(ClientInfo *clientInfo, U32 objectType);  // Deploy!
   string getErrorMessage();

   static bool findDeployPoint(Ship *ship, Point &deployPosition, Point &deployNormal);
   static string checkResourcesAndEnergy(Ship *ship);
};


};
#endif

