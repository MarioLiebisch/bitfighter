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

#ifndef _BARRIER_H_
#define _BARRIER_H_

#include "gameObject.h"
#include "textItem.h"      // for LineItem def  TODO: get rid of this

#include "Point.h"
#include "tnlVector.h"
#include "tnlNetObject.h"

namespace Zap
{

/// The Barrier class represents rectangular barriers that player controlled
/// ships cannot pass through... i.e. walls  Barrier objects, once created, never
/// change state, simplifying the pack/unpack update methods.  Barriers are
/// constructed as an expanded line segment.
class Barrier : public GameObject
{
   typedef GameObject Parent;

private:
   Vector<Point> mBufferedObjectPointsForBotZone;

   // Takes a segment and "puffs it out" to a polygon for bot zone generation.
   // This polygon is the width of the barrier plus the ship's collision radius added to the outside
   void computeBufferForBotZone(Vector<Point> &zonePoints);

public:
   Vector<Point> mPoints; ///< The points of the barrier --> if only two, first will be start, second end of an old-school segment

   bool mSolid;
   // By precomputing and storing, we should ease the rendering cost
   Vector<Point> mRenderFillGeometry; ///< Actual geometry used for rendering fill.
   Vector<Point> mRenderOutlineGeometry; ///< Actual geometry used for rendering outline.

   F32 mWidth;

   static const S32 MIN_BARRIER_WIDTH = 1;         // Clipper doesn't much like 0 width walls
   static const S32 MAX_BARRIER_WIDTH = 2500;      // Geowar has walls at least 350 units wide, so going lower will break at least one level

   static const S32 DEFAULT_BARRIER_WIDTH = 50; ///< The default width of the barrier in game units

   static Vector<Point> mRenderLineSegments;    ///< The clipped line segments representing this barrier.
   Vector<Point> mBotZoneBufferLineSegments;    ///< The line segments representing a buffered barrier.

   /// Barrier constructor
   Barrier(const Vector<Point> &points = Vector<Point>(), F32 width = DEFAULT_BARRIER_WIDTH, bool solid = false);

   /// Adds the server object to the net interface's scope always list
   void onAddedToGame(Game *theGame);

   /// Renders barrier fill
   void render(S32 layerIndex);                                           // Renders barrier fill barrier-by-barrier
   static void renderEdges(S32 layerIndex, const Color &outlineColor);    // Renders all edges in one pass

   /// Returns a sorting key for the object.  Barriers should be drawn first so as to appear behind other objects.
   S32 getRenderSortValue();

   /// returns the collision polygon of this barrier, which is the boundary extruded from the start,end line segment.
   bool getCollisionPoly(Vector<Point> &polyPoints) const;
   const Vector<Point> *getCollisionPolyPtr() const;

   /// collide always returns true for Barrier objects.
   bool collide(GameObject *otherObject);

   // Takes a list of vertices and converts them into a list of lines representing the edges of an object
   static void resetEdges(const Vector<Point> &corners, Vector<Point> &edges);

   // Simply takes a segment and "puffs it out" to a rectangle of a specified width, filling cornerPoints.  Does not modify endpoints.
   static void expandCenterlineToOutline(const Point &start, const Point &end, F32 width, Vector<Point> &cornerPoints);

   const Vector<Point> *getBufferForBotZone();

   // Combines multiple barriers into a single complex polygon
   static bool unionBarriers(const Vector<DatabaseObject *> &barriers, Vector<Vector<Point> > &solution);

   /// Clips the current set of render lines against the polygon passed as polyPoints, modifies lineSegmentPoints.
   static void clipRenderLinesToPoly(const Vector<DatabaseObject *> &barrierList, Vector<Point> &lineSegmentPoints);

   static void constructBarrierEndPoints(const Vector<Point> *vec, F32 width, Vector<Point> &barrierEnds);

   // Clean up edge geometry and get barriers ready for proper rendering
   static void prepareRenderingGeometry(Game *game);


   TNL_DECLARE_CLASS(Barrier);
};


////////////////////////////////////////
////////////////////////////////////////

// TODO: Don't need this to be a GameObject  -- perhaps merge with barrier above?
class WallItem : public LineItem
{
   typedef LineItem Parent;

public:
   WallItem();    // Constructor
   WallItem *clone() const;

   Vector<Point> extendedEndPoints;
   virtual Rect calcExtents();
   virtual void onGeomChanged();
   void processEndPoints();  

   void render();

   // Some properties about the item that will be needed in the editor
   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();          // Vertices should not be labeled
   string getAttributeString();
   const char *getInstructionMsg();

   bool hasTeam();
   bool canBeHostile();
   bool canBeNeutral();
   F32 getEditorRadius(F32 currentScale);  // Basically, the size of our hit target for vertices

   const Color *getEditorRenderColor();    // Unselected wall spine color

   void scale(const Point &center, F32 scale);

   string toString(F32 gridSize) const;

   bool processArguments(S32 argc, const char **argv, Game *game);
   void setWidth(S32 width);

   void setSelected(bool selected);

   static const S32 VERTEX_SIZE = 5;
};


////////////////////////////////////////
////////////////////////////////////////

class PolyWall : public EditorPolygon     // Don't need GameObject component of this...
{
   typedef EditorPolygon Parent;

private:
   //void computeExtent();

public:
   PolyWall();      // Constructor
   PolyWall *clone() const;

   bool processArguments(S32 argc, const char **argv, Game *game);

   void renderDock();

   S32 getRenderSortValue();

   void setSelected(bool selected);

   virtual void onGeomChanged();
   virtual void onItemDragging();

   /////
   // Editor methods
   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();
   string toString(F32 gridSize) const;

   F32 getEditorRadius(F32 currentScale);

   /////
   // Lua interface  ==>  don't need these!!

   PolyWall(lua_State *L);         //  Lua constructor
   GameObject *getGameObject();    // Return the underlying GameObject

   static const char className[];  // Class name as it appears to Lua scripts
   static Lunar<PolyWall>::RegType methods[];

   S32 getClassID(lua_State *L);
   TNL_DECLARE_CLASS(PolyWall);

private:
   void push(lua_State *L);
};


////////////////////////////////////////
////////////////////////////////////////

class WallSegment : public DatabaseObject
{
private:
   S32 mOwner;
   bool mSelected;

  void init(GridDatabase *database, S32 owner);
  bool invalid;              // A flag for marking segments in need of processing

   Vector<Point> mEdges;    
   Vector<Point> mCorners;
   Vector<Point> mTriangulatedFillPoints;

public:
   WallSegment(GridDatabase *gridDatabase, const Point &start, const Point &end, F32 width, S32 owner = -1);    // Normal wall segment
   WallSegment(GridDatabase *gridDatabase, const Vector<Point> &points, S32 owner = -1);                        // PolyWall 
   virtual ~WallSegment();

   S32 getOwner();
   void invalidate();

   bool isSelected();
   void setSelected(bool selected);

   void resetEdges();         // Compute basic edges from corner points
   void computeBoundingBox(); // Computes bounding box based on the corners, updates database
   
   void renderFill(const Point &offset);

   const Vector<Point> *getCorners();
   const Vector<Point> *getEdges();
   const Vector<Point> *getTriangulatedFillPoints();

   ////////////////////
   // DatabaseObject methods

   // Note that the poly returned here is different than what you might expect -- it is composed of the edges,
   // not the corners, and is thus in A-B, C-D, E-F format rather than the more typical A-B-C-D format returned
   // by getCollisionPoly() elsewhere in the game.  Therefore, it needs to be handled differently.
   bool getCollisionPoly(Vector<Point> &polyPoints) const;
   bool getCollisionCircle(U32 stateIndex, Point &point, float &radius) const;
};


////////////////////////////////////////
////////////////////////////////////////

// Width of line representing centerline of barriers
#define WALL_SPINE_WIDTH gLineWidth3


class WallEdge : public DatabaseObject
{
private:
   Point mStart, mEnd;

public:
   WallEdge(const Point &start, const Point &end, GridDatabase *database);
   virtual ~WallEdge();

   Point *getStart();
   Point *getEnd();

   // Note that the poly returned here is different than what you might expect -- it is composed of the edges,
   // not the corners, and is thus in A-B, C-D, E-F format rather than the more typical A-B-C-D format returned
   // by getCollisionPoly() elsewhere in the game.  Therefore, it needs to be handled differently.
   bool getCollisionPoly(Vector<Point> &polyPoints) const;
   bool getCollisionCircle(U32 stateIndex, Point &point, float &radius) const;
};


};

#endif

