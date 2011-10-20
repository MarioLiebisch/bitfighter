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

#include "barrier.h"
#include "BotNavMeshZone.h"
#include "gameObjectRender.h"
#include "GeomUtils.h"              // For polygon triangulation
#include "EngineeredItem.h"      // For forcefieldprojector def
#include "gameType.h"               // For BarrierRec struct
#include "game.h"
#include "config.h"
#include "stringUtils.h"

#ifndef ZAP_DEDICATED 
#include "UI.h"  // for glColor(Color)
#include "SDL/SDL_opengl.h"
#endif

#include <cmath>                    // C++ version of this headers includes float overloads

using namespace TNL;

namespace Zap
{

TNL_IMPLEMENT_NETOBJECT(Barrier);

Vector<Point> Barrier::mRenderLineSegments;

bool loadBarrierPoints(const WallRec *barrier, Vector<Point> &points)
{
   // Convert the list of floats into a list of points
   for(S32 i = 1; i < barrier->verts.size(); i += 2)
      points.push_back( Point(barrier->verts[i-1], barrier->verts[i]) );

   removeCollinearPoints(points, false);   // Remove collinear points to make rendering nicer and datasets smaller

   return (points.size() >= 2);
}

////////////////////////////////////////
////////////////////////////////////////

// Runs on server or on client, never in editor
void WallRec::constructWalls(Game *theGame)
{
   Vector<Point> vec;

   if(!loadBarrierPoints(this, vec))
      return;

   if(solid)   // This is a solid polygon
   {
      if(vec.first() == vec.last())      // Does our barrier form a closed loop?
         vec.erase(vec.size() - 1);      // If so, remove last vertex

      Barrier *b = new Barrier(vec, width, true);
      b->addToGame(theGame, theGame->getGameObjDatabase());
   }
   else        // This is a standard series of segments
   {
      // First, fill a vector with barrier segments
      Vector<Point> barrierEnds;
      Barrier::constructBarrierEndPoints(&vec, width, barrierEnds);

      Vector<Point> pts;
      // Then add individual segments to the game
      for(S32 i = 0; i < barrierEnds.size(); i += 2)
      {
         pts.clear();
         pts.push_back(barrierEnds[i]);
         pts.push_back(barrierEnds[i+1]);

         Barrier *b = new Barrier(pts, width, false);    // false = not solid
         b->addToGame(theGame, theGame->getGameObjDatabase());
      }
   }
}




////////////////////////////////////////
////////////////////////////////////////

// Constructor --> gets called from constructBarriers above
Barrier::Barrier(const Vector<Point> &points, F32 width, bool solid)
{
   mObjectTypeNumber = BarrierTypeNumber;
   mPoints = points;

   if(points.size() < 2)      // Invalid barrier!
   {
      //delete this;    // Sam: cannot "delete this" in constructor, as "new" still returns non-NULL address
      logprintf(LogConsumer::LogWarning, "Invalid barrier detected (has only one point).  Disregarding...");
      return;
   }

   Rect extent(points);

   if(width < 0)             // Force positive width
      width = -width;

   mWidth = width;           // must be positive to avoid problem with bufferBarrierForBotZone
   width = width * 0.5f + 1;  // divide by 2 to avoid double size extents, add 1 to avoid rounding errors.
   if(points.size() == 2)    // It's a regular segment, need to make a little larger to accomodate width
      extent.expand(Point(width, width));
   // use mWidth, not width, for anything below this.

   setExtent(extent);

    mSolid = solid;

   if(mSolid)
   {
      if (isWoundClockwise(mPoints))  // all walls must be CCW to clip correctly
         mPoints.reverse();

      Triangulate::Process(mPoints, mRenderFillGeometry);

      if(mRenderFillGeometry.size() == 0)      // Geometry is bogus; perhaps duplicated points, or other badness
      {
         //delete this;    // Sam: cannot "delete this" in constructor, as "new" still returns non-NULL address
         logprintf(LogConsumer::LogWarning, "Invalid barrier detected (polywall with invalid geometry).  Disregarding...");
         return;
      }
   }
   else
   {
      if (mPoints.size() == 2 && mPoints[0] == mPoints[1])   // Test for zero-length barriers
         mPoints[1] += Point(0,0.5);                         // Add vertical vector of half a point so we can see outline geo in-game

      if(mPoints.size() == 2 && mWidth != 0)   // It's a regular segment, so apply width
         expandCenterlineToOutline(mPoints[0], mPoints[1], mWidth, mRenderFillGeometry);     // Fills with 4 points
   }

   getCollisionPoly(mRenderOutlineGeometry);    // Outline is the same for both barrier geometries
}


void Barrier::onAddedToGame(Game *game)
{
   Parent::onAddedToGame(game);
}


// Combines multiple barriers into a single complex polygon... fills solution
bool Barrier::unionBarriers(const Vector<DatabaseObject *> &barriers, Vector<Vector<Point> > &solution)
{
   Vector<Vector<Point> > inputPolygons;
   Vector<Point> inputPoly;

   // Reusable container
   Vector<Point> points;

   for(S32 i = 0; i < barriers.size(); i++)
   {
      Barrier *barrier = dynamic_cast<Barrier *>(barriers[i]);

      if(!barrier)
         continue;

      inputPoly.clear();

      barrier->getCollisionPoly(points);        // Puts object bounds into points
      inputPoly = points;

#ifdef DUMP_DATA
      for(S32 j = 0; j < barrier->mBotZoneBufferGeometry.size(); j++)
         logprintf("Before Clipper Point: %f %f", barrier->mBotZoneBufferGeometry[j].x, barrier->mBotZoneBufferGeometry[j].y);
#endif

      inputPolygons.push_back(inputPoly);
   }

   return mergePolys(inputPolygons, solution);
}


// Processes mPoints and fills polyPoints 
bool Barrier::getCollisionPoly(Vector<Point> &polyPoints) const
{
   if (mSolid)
      polyPoints = mPoints;
   else
      polyPoints = mRenderFillGeometry;

   return true;
}


// Takes a list of vertices and converts them into a list of lines representing the edges of an object -- static method
void Barrier::resetEdges(const Vector<Point> &corners, Vector<Point> &edges)
{
   edges.clear();

   S32 last = corners.size() - 1;             
   for(S32 i = 0; i < corners.size(); i++)
   {
      edges.push_back(corners[last]);
      edges.push_back(corners[i]);
      last = i;
   }
}


// Given the points in vec, figure out where the ends of the walls should be (they'll need to be extended slighly in some cases
// for better rendering).  Set extendAmt to 0 to see why it's needed.
// Populates barrierEnds with the results.
void Barrier::constructBarrierEndPoints(const Vector<Point> *vec, F32 width, Vector<Point> &barrierEnds)    // static
{
   barrierEnds.clear();    // local static vector

   if(vec->size() <= 1)     // Protect against bad data
      return;

   bool loop = (vec->first() == vec->last());      // Does our barrier form a closed loop?

   Vector<Point> edgeVector;
   for(S32 i = 0; i < vec->size() - 1; i++)
   {
      Point e = vec->get(i+1) - vec->get(i);
      e.normalize();
      edgeVector.push_back(e);
   }

   Point lastEdge = edgeVector[edgeVector.size() - 1];
   Vector<F32> extend;

   for(S32 i = 0; i < edgeVector.size(); i++)
   {
      Point curEdge = edgeVector[i];
      double cosTheta = curEdge.dot(lastEdge);

      // Do some bounds checking.  Crazy, I know, but trust me, it's worth it!
      if (cosTheta > 1.0)
         cosTheta = 1.0;
      else if(cosTheta < -1.0)  
         cosTheta = -1.0;

      cosTheta = abs(cosTheta);     // Seems to reduce "end gap" on acute junction angles
      
      F32 extendAmt = width * 0.5f * F32(tan( acos(cosTheta) / 2 ));
      if(extendAmt > 0.01f)
         extendAmt -= 0.01f;
      extend.push_back(extendAmt);
   
      lastEdge = curEdge;
   }

   F32 first = extend[0];
   extend.push_back(first);

   for(S32 i = 0; i < edgeVector.size(); i++)
   {
      F32 extendBack = extend[i];
      F32 extendForward = extend[i+1];
      if(i == 0 && !loop)
         extendBack = 0;
      if(i == edgeVector.size() - 1 && !loop)
         extendForward = 0;

      Point start = vec->get(i) - edgeVector[i] * extendBack;
      Point end = vec->get(i+1) + edgeVector[i] * extendForward;

      barrierEnds.push_back(start);
      barrierEnds.push_back(end);
   }
}


// Simply takes a segment and "puffs it out" to a rectangle of a specified width, filling cornerPoints.  Does not modify endpoints.
void Barrier::expandCenterlineToOutline(const Point &start, const Point &end, F32 width, Vector<Point> &cornerPoints)
{
   cornerPoints.clear();

   Point dir = end - start;
   Point crossVec(dir.y, -dir.x);
   crossVec.normalize(width * 0.5f);

   cornerPoints.push_back(start + crossVec);
   cornerPoints.push_back(end + crossVec);
   cornerPoints.push_back(end - crossVec);
   cornerPoints.push_back(start - crossVec);
}


Vector<Point> Barrier::getBufferForBotZone()
{
   Vector<Point> bufferedPoints;

   // Use a clipper library to buffer polywalls; should be counter-clockwise by here
   if(mSolid)
   {
      offsetPolygon(mPoints, bufferedPoints, (F32)BotNavMeshZone::BufferRadius);
   }

   // If a barrier, do our own buffer
   // Puffs out segment to the specified width with a further buffer for bot zones, has an inset tangent corner cut
   else
   {
      Point& start = mPoints[0];
      Point& end = mPoints[1];
      Point difference = end - start;

      Point crossVector(difference.y, -difference.x);  // Create a point whose vector from 0,0 is perpenticular to the original vector
      crossVector.normalize((mWidth * 0.5f) + BotNavMeshZone::BufferRadius);  // reduce point so the vector has length of barrier width + ship radius

      Point parallelVector(difference.x, difference.y); // Create a vector parallel to original segment
      parallelVector.normalize((F32)BotNavMeshZone::BufferRadius);  // Reduce point so vector has length of ship radius

      // For octagonal zones
      //   create extra vectors that are offset full offset to create 'cut' corners
      //   (FloatSqrtHalf * BotNavMeshZone::BufferRadius)  creates a tangent to the radius of the buffer
      //   we then subtract a little from the tangent cut to shorten the buffer on the corners and allow zones to be created when barriers are close
      Point crossPartial = crossVector;
      crossPartial.normalize((FloatSqrtHalf * BotNavMeshZone::BufferRadius) + (mWidth * 0.5f) - (0.3f * BotNavMeshZone::BufferRadius));

      Point parallelPartial = parallelVector;
      parallelPartial.normalize((FloatSqrtHalf * BotNavMeshZone::BufferRadius) - (0.3f * BotNavMeshZone::BufferRadius));

      // Now add/subtract perpendicular and parallel vectors to buffer the segments
      bufferedPoints.push_back((start - parallelVector)  + crossPartial);
      bufferedPoints.push_back((start - parallelPartial)  + crossVector);
      bufferedPoints.push_back(end   + parallelPartial + crossVector);
      bufferedPoints.push_back(end   + parallelVector  + crossPartial);
      bufferedPoints.push_back(end   + parallelVector  - crossPartial);
      bufferedPoints.push_back(end   + parallelPartial - crossVector);
      bufferedPoints.push_back((start - parallelPartial) - crossVector);
      bufferedPoints.push_back((start - parallelVector)  - crossPartial);
   }

   return bufferedPoints;
}


// Clears out overlapping barrier lines for better rendering appearance, modifies lineSegmentPoints.
// This is effectively called on every pair of potentially intersecting barriers, and lineSegmentPoints gets 
// refined as each additional intersecting barrier gets processed.
// static method
void Barrier::clipRenderLinesToPoly(const Vector<DatabaseObject *> &barrierList, Vector<Point> &lineSegmentPoints)
{
   Vector<Vector<Point> > solution;

   unionBarriers(barrierList, solution);

   unpackPolygons(solution, lineSegmentPoints);
}


// Merges wall outlines together, client only
// This is used for barriers and polywalls
void Barrier::prepareRenderingGeometry(Game *game)
{
   mRenderLineSegments.clear();

   Vector<DatabaseObject *> barrierList;

   game->getGameObjDatabase()->findObjects((TestFunc)isWallType, barrierList);

   clipRenderLinesToPoly(barrierList, mRenderLineSegments);
}


// Render wall fill only for this wall; all edges rendered in a single pass later
void Barrier::render(S32 layerIndex)
{
   if(layerIndex == 0)           // First pass: draw the fill
      renderWallFill(&mRenderFillGeometry, mSolid, getGame()->getSettings()->getWallFillColor());
}


// static 
void Barrier::renderEdges(S32 layerIndex, const Color &outlineColor)
{
   if(layerIndex == 1)
      renderWallEdges(&mRenderLineSegments, outlineColor);
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
WallItem::WallItem()
{
   mObjectTypeNumber = WallItemTypeNumber;
   setWidth(Barrier::DEFAULT_BARRIER_WIDTH);
}


WallItem *WallItem::clone() const
{
   return new WallItem(*this);
}


// Only called from editor... use of getEditorDatabase() below is a bit hacky...
void WallItem::onGeomChanged()
{
   // Fill extendedEndPoints from the vertices of our wall's centerline, or from PolyWall edges
   processEndPoints();

   //if(getObjectTypeMask() & ItemPolyWall)     // Prepare interior fill triangulation
   //   initializePolyGeom();          // Triangulate, find centroid, calc extents

   Game *game = getGame();
   game->getWallSegmentManager()->computeWallSegmentIntersections(game->getEditorDatabase(), this);

   // Find any forcefields that might intersect our new wall segment and recalc their endpoints
   Rect aoi = getExtent();    
    // A FF could extend into our area of interest from quite a distance, so expand search region accordingly
   aoi.expand(Point(ForceField::MAX_FORCEFIELD_LENGTH, ForceField::MAX_FORCEFIELD_LENGTH));  

   fillVector.clear();
   getGame()->getEditorDatabase()->findObjects(ForceFieldProjectorTypeNumber, fillVector, aoi);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      ForceFieldProjector *ffp = dynamic_cast<ForceFieldProjector *>(fillVector[i]);
      ffp->findForceFieldEnd();
   }
}


void WallItem::processEndPoints()
{
#ifndef ZAP_DEDICATED
   Barrier::constructBarrierEndPoints(getOutline(), (F32)getWidth(), extendedEndPoints);
#endif
}


// Size of object in editor 
F32 WallItem::getEditorRadius(F32 currentScale)
{
   return VERTEX_SIZE;   // Keep vertex hit targets the same regardless of scale
}


string WallItem::toString(F32 gridSize) const
{
   return "BarrierMaker " + itos(getWidth()) + " " + geomToString(gridSize);
}


void WallItem::scale(const Point &center, F32 scale)
{
   EditorObject::scale(center, scale);

   // Adjust the wall thickness
   // Scale might not be accurate due to conversion to S32
   setWidth(S32(getWidth() * scale));
}


void WallItem::setWidth(S32 width) 
{         
   LineItem::setWidth(width, Barrier::MIN_BARRIER_WIDTH, Barrier::MAX_BARRIER_WIDTH);     // Why do we need LineItem:: prefix here???
}



////////////////////////////////////////
////////////////////////////////////////

extern Color EDITOR_WALL_FILL_COLOR;

TNL_IMPLEMENT_NETOBJECT(PolyWall);

const char PolyWall::className[] = "PolyWall";      // Class name as it appears to Lua scripts


PolyWall::PolyWall()
{
   mObjectTypeNumber = PolyWallTypeNumber;
}


PolyWall *PolyWall::clone() const
{
   return new PolyWall(*this);
}


void PolyWall::processEndPoints()
{
#ifndef ZAP_DEDICATED
   extendedEndPoints.clear();
   for(S32 i = 1; i < getVertCount(); i++)
   {
      extendedEndPoints.push_back(getVert(i-1));
      extendedEndPoints.push_back(getVert(i));
   }

   // Close the loop
   extendedEndPoints.push_back(getVert(getVertCount()));
   extendedEndPoints.push_back(getVert(0));
#endif
}


static const Color HIGHLIGHT_COLOR = Colors::white;

void PolyWall::render()
{
#ifndef ZAP_DEDICATED
   glColor(HIGHLIGHT_COLOR);
   renderPolygonOutline(getOutline());
#endif
}


void PolyWall::renderFill()
{
   renderPolygonFill(getFill(), &EDITOR_WALL_FILL_COLOR, 1);
}


void PolyWall::renderEditor(F32 currentScale)
{
#ifndef ZAP_DEDICATED
   glColor(HIGHLIGHT_COLOR);
   renderPolygonOutline(getOutline());
   EditorPolygon::renderEditor(currentScale);
#endif
}


void PolyWall::renderDock()
{
   renderPolygonFill(getFill(), &EDITOR_WALL_FILL_COLOR);
   renderPolygonOutline(getOutline(), &Colors::blue);
}


bool PolyWall::processArguments(S32 argc, const char **argv, Game *game)
{
   if(argc < 6)
      return false;

   readGeom(argc, argv, 0, game->getGridSize());
   //computeExtent();

   return true;
}


string PolyWall::toString(F32 gridSize) const
{
   return string(getClassName()) + " " + geomToString(gridSize);
}


// Only called from editor???
void PolyWall::onGeomChanged()
{
   //if(getObjectTypeMask() & ItemPolyWall)     // Prepare interior fill triangulation
   //   initializePolyGeom();          // Triangulate, find centroid, calc extents

   Parent::onGeomChanged();

   getGame()->getWallSegmentManager()->computeWallSegmentIntersections(getGame()->getEditorDatabase(), this);

   // Find any forcefields that might intersect our new wall segment and recalc their endpoints
   Rect aoi = getExtent();    
    // A FF could extend into our area of interest from quite a distance, so expand search region accordingly
   aoi.expand(Point(ForceField::MAX_FORCEFIELD_LENGTH, ForceField::MAX_FORCEFIELD_LENGTH));  

   fillVector.clear();
   getGame()->getEditorDatabase()->findObjects(ForceFieldProjectorTypeNumber, fillVector, aoi);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      ForceFieldProjector *ffp = dynamic_cast<ForceFieldProjector *>(fillVector[i]);
      ffp->findForceFieldEnd();
   }
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
WallEdge::WallEdge(const Point &start, const Point &end, GridDatabase *database) 
{ 
   mStart = start; 
   mEnd = end; 

   addToDatabase(database);


   setExtent(Rect(start, end)); 

   // Set some things required by DatabaseObject
   mObjectTypeNumber = WallEdgeTypeNumber;
}


// Destructor
WallEdge::~WallEdge()
{
    // Make sure object is out of the database
   removeFromDatabase(); 
}

////////////////////////////////////////
////////////////////////////////////////


// Constructor
WallSegmentManager::WallSegmentManager()
{
   mWallSegmentDatabase = new GridDatabase();
   mWallEdgeDatabase = new GridDatabase();
}


// Destructor
WallSegmentManager::~WallSegmentManager()
{
   delete mWallSegmentDatabase;
   delete mWallEdgeDatabase;
}


void WallSegmentManager::recomputeAllWallGeometry(GridDatabase *gameDatabase)
{
   buildAllWallSegmentEdgesAndPoints(gameDatabase);

   mWallEdgePoints.clear();

   // Clip mWallSegments to create wallEdgePoints, which will be used below to create a new set of WallEdge objects
   clipAllWallEdges(mWallSegments, mWallEdgePoints);    

   mWallEdges.deleteAndClear();
   mWallEdges.resize(mWallEdgePoints.size() / 2);

   // Add clipped wallEdges to the spatial database
   for(S32 i = 0; i < mWallEdgePoints.size(); i+=2)
   {
      WallEdge *newEdge = new WallEdge(mWallEdgePoints[i], mWallEdgePoints[i+1], mWallEdgeDatabase);    // Create the edge object
      mWallEdges[i/2] = newEdge;
   }
}


// Delete all segments, then find all walls and build a new set of segments
void WallSegmentManager::buildAllWallSegmentEdgesAndPoints(GridDatabase *gameDatabase)
{
   deleteAllSegments();

   fillVector.clear();

   gameDatabase->findObjects((TestFunc)isWallType, fillVector);

   Vector<DatabaseObject *> engrObjects;
   gameDatabase->findObjects((TestFunc)isEngineeredType, engrObjects);   // All engineered objects

   // Iterate over all our wall objects
   for(S32 i = 0; i < fillVector.size(); i++)
      buildWallSegmentEdgesAndPoints(gameDatabase, fillVector[i], engrObjects);
}


void WallSegmentManager::buildWallSegmentEdgesAndPoints(GridDatabase *gameDatabase, DatabaseObject *dbObject)
{
   fillVector.clear();
   gameDatabase->findObjects((TestFunc)isEngineeredType, fillVector);    // All engineered objects

   buildWallSegmentEdgesAndPoints(gameDatabase, dbObject, fillVector);
}


void WallSegmentManager::buildWallSegmentEdgesAndPoints(GridDatabase *gameDatabase, DatabaseObject *wallDbObject, const Vector<DatabaseObject *> &engrObjects)
{
#ifndef ZAP_DEDICATED
   // Find any engineered objects that terminate on this wall, and mark them for resnapping later

   Vector<EngineeredItem *> eosOnDeletedSegs;    // A list of engr objects terminating on the wall segment that we'll be deleting

   EditorObject *wall = dynamic_cast<EditorObject *>(wallDbObject);   // Wall we're deleting and rebuilding

   TNLAssert(wall, "Bad cast -- expected an EditorObject!");

   S32 count = mWallSegments.size(); 

   // Loop through all the walls, and, for each, see if any of the engineered objects we were given are mounted to it
   for(S32 i = 0; i < count; i++)
      if(mWallSegments[i]->getOwner() == wall->getSerialNumber())      // Segment belongs to item
         for(S32 j = 0; j < engrObjects.size(); j++)
         {
            EngineeredItem *engrObj = dynamic_cast<EngineeredItem *>(engrObjects[j]);

            // Does FF start or end on this segment?
            if(engrObj->getMountSegment() == mWallSegments[i] || engrObj->getEndSegment() == mWallSegments[i])
               eosOnDeletedSegs.push_back(engrObj);
         }

   // Get rid of any existing segments that correspond to our item; we'll be building new ones
   deleteSegments(wall->getSerialNumber());

   Rect allSegExtent;

   if(wall->getObjectTypeNumber() == PolyWallTypeNumber)
   {
      WallSegment *newSegment = new WallSegment(mWallSegmentDatabase, *wall->getOutline(), wall->getSerialNumber());
      mWallSegments.push_back(newSegment);
   }
   else     // Tranditional wall -- need to generate a series of 2-point segments that represent each section of our wall
   {
      WallItem *wallItem = dynamic_cast<WallItem *>(wall);        
      TNLAssert(wallItem, "Bad cast -- expected an WallItem!");

      // Create a series of WallSegments, each representing a sequential pair of vertices on our wall
      for(S32 i = 0; i < wall->extendedEndPoints.size(); i += 2)
      {
         WallSegment *newSegment = new WallSegment(mWallSegmentDatabase, wallItem->extendedEndPoints[i], wallItem->extendedEndPoints[i+1], 
                                                   (F32)wallItem->getWidth(), wallItem->getSerialNumber());    // Create the segment
         mWallSegments.push_back(newSegment);          // And add it to our master segment list
      }
   }

   for(S32 i = 0; i < mWallSegments.size(); i++)
   {
      mWallSegments[i]->addToDatabase(mWallSegmentDatabase);  // Add it to our segment database

      Rect segExtent(mWallSegments[i]->corners);              // Calculate a bounding box around the segment
      if(i == 0)
         allSegExtent.set(segExtent);
      else
         allSegExtent.unionRect(segExtent);
   }

   wall->setExtent(allSegExtent);      // A wall's extent is the union of the extents of all its segments.  Makes sense, right?

   // Alert all forcefields terminating on any of the wall segments we deleted and potentially recreated
   for(S32 j = 0; j < eosOnDeletedSegs.size(); j++)  
      eosOnDeletedSegs[j]->mountToWall(eosOnDeletedSegs[j]->getVert(0), mWallEdgeDatabase, mWallSegmentDatabase);
#endif
}


// Used above and from instructions
void WallSegmentManager::clipAllWallEdges(const Vector<WallSegment *> &wallSegments, Vector<Point> &wallEdges)
{
   Vector<Vector<Point> > inputPolygons, solution;

   for(S32 i = 0; i < wallSegments.size(); i++)
      inputPolygons.push_back(wallSegments[i]->corners);

   mergePolys(inputPolygons, solution);      // Merged wall segments are placed in solution

   unpackPolygons(solution, wallEdges);
}


// Takes a wall, finds all intersecting segments, and marks them invalid
void WallSegmentManager::invalidateIntersectingSegments(GridDatabase *gameDatabase, EditorObject *item)
{
#ifndef ZAP_DEDICATED
   fillVector.clear();

   // Before we update our edges, we need to mark all intersecting segments using the invalid flag.
   // These will need new walls after we've moved our segment.  We'll look for those intersecting segments in our edge database.
   for(S32 i = 0; i < mWallSegments.size(); i++)
      if(mWallSegments[i]->getOwner() == item->getSerialNumber())      // Segment belongs to our item; look it up in the database
         gameDatabase->findObjects(WallSegmentTypeNumber, fillVector, mWallSegments[i]->getExtent());

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      WallSegment *intersectingSegment = dynamic_cast<WallSegment *>(fillVector[i]);
      TNLAssert(intersectingSegment, "NULL segment!");

      // Reset the edges of all invalidated segments to their factory settings
      intersectingSegment->resetEdges();   
      intersectingSegment->invalidate();
   }

   buildWallSegmentEdgesAndPoints(gameDatabase, item);

   // Is any of this needed?  CE 7/26
   fillVector.clear();

   //// Invalidate all segments that potentially intersect the changed segment in its new location
   //for(S32 i = 0; i < mWallSegments.size(); i++)
   //   if(mWallSegments[i]->getOwner() == item->getSerialNumber())      // Segment belongs to our item, compare to all others
   //      mWallSegmentDatabase->findObjects(0, fillVector, mWallSegments[i]->getExtent(), WallSegmentTypeNumber);

   //for(S32 i = 0; i < fillVector.size(); i++)
   //{
   //   logprintf("typeMask: %d ==> expecting %d", fillVector[i]->getObjectTypeMask(), WallSegmentType);
   //   WallSegment *wallSegment = dynamic_cast<WallSegment *>(fillVector[i]);
   //   TNLAssert(wallSegment, "Uh oh!");
   //   wallSegment->invalidate();
   //}


   for(S32 i = 0; i < mWallSegmentDatabase->getObjectCount(); i++)
   {
      if(mWallSegmentDatabase->getObjectByIndex(i)->getObjectTypeNumber() == WallSegmentTypeNumber)
      {
         WallSegment *wallSegment = dynamic_cast<WallSegment *>(mWallSegmentDatabase->getObjectByIndex(i));
         TNLAssert(wallSegment, "Uh oh!");
         wallSegment->invalidate();
      }
   }
#endif
}


// Called when a wall segment has somehow changed.  All current and previously intersecting segments 
// need to be recomputed.
void WallSegmentManager::computeWallSegmentIntersections(GridDatabase *gameObjDatabase, EditorObject *item)
{
   invalidateIntersectingSegments(gameObjDatabase, item);  // TODO: Is this step still needed?  similar things, both run buildWallSegmentEdgesAndPoints()
   recomputeAllWallGeometry(gameObjDatabase);
}


void WallSegmentManager::deleteAllSegments()
{
   mWallSegments.deleteAndClear();
}


// Delete all wall segments owned by specified owner
void WallSegmentManager::deleteSegments(S32 owner)
{
   S32 count = mWallSegments.size();

   for(S32 i = 0; i < count; i++)
      if(mWallSegments[i]->getOwner() == owner)
      {
         delete mWallSegments[i];    // Destructor will remove segment from database
         mWallSegments.erase_fast(i);
         i--;
         count--;
      }
}


// Only called from the editor
void WallSegmentManager::renderWalls(GameSettings *settings, bool draggingObjects, bool showingReferenceShip, bool showSnapVertices, F32 alpha)
{
#ifndef ZAP_DEDICATED
   fillVector.clear();
   mWallSegmentDatabase->findObjects((TestFunc)isWallType, fillVector);

   for(S32 i = 0; i < mWallSegments.size(); i++)
   {  
      bool isBeingDragged = false;

     /* if(draggingObjects)
      {
         for(S32 j = 0; j < fillVector.size(); j++)
         {
            WallSegment *obj = dynamic_cast<WallSegment *>(fillVector[j]);
            if(obj->isSelected() && obj->getItemId() == mWallSegments[i]->getOwner())
            {
               isBeingDragged = true;
               break;
            }
         }
      }*/

      // We'll use the editor color most of the time; only in preview mode in the editor do we use the game color
      bool useGameColor = UserInterface::current && UserInterface::current->getMenuID() == EditorUI && showingReferenceShip;
      //TNLAssert(UserInterface::current->getMenuID() == EditorUI, "How did we get here, then???"); // (came from editing attributes of SpeedZone)

      Color fillColor = useGameColor ? settings->getWallFillColor() : EDITOR_WALL_FILL_COLOR;
      mWallSegments[i]->renderFill(fillColor, isBeingDragged);
   }

   renderWallEdges(&mWallEdgePoints, settings->getWallOutlineColor());      // Render wall outlines

   if(showSnapVertices)
   {
      glLineWidth(gLineWidth1);

      glColor(Colors::magenta);
      for(S32 i = 0; i < mWallEdgePoints.size(); i++)
         drawCircle(mWallEdgePoints[i], 5);

      glLineWidth(gDefaultLineWidth);
   }

   //// Render snap targets at each wall edge vertex
   //if(showSnapVertices)
   //   for(S32 i = 0; i < mWallEdgePoints.size(); i++)
   //      EditorUserInterface::renderSnapTarget(mWallEdgePoints[i]);
#endif
}


////////////////////////////////////////
////////////////////////////////////////

// Regular constructor
WallSegment::WallSegment(GridDatabase *gridDatabase, const Point &start, const Point &end, F32 width, S32 owner) 
{ 
   mDatabase = gridDatabase;
   // Calculate segment corners by expanding the extended end points into a rectangle
   Barrier::expandCenterlineToOutline(start, end, width, corners);   
   init(owner);
}


// PolyWall constructor
WallSegment::WallSegment(GridDatabase *gridDatabase, const Vector<Point> &points, S32 owner)
{
   mDatabase = gridDatabase;

   corners = points;

   if(isWoundClockwise(points))
      corners.reverse();

   init(owner);
}


// Intialize, only called from constructors above
void WallSegment::init(S32 owner)
{
   // Recompute the edges based on our new corner points
   resetEdges();                                            

   Rect extent(corners);
   setExtent(extent); 

   // Drawing walls filled requires that points be triangluated
   Triangulate::Process(corners, triangulatedFillPoints);

   mOwner = owner; 
   invalid = false; 

   /////
   // Set some things required by DatabaseObject
   mObjectTypeNumber = WallSegmentTypeNumber;
}


// Destructor
WallSegment::~WallSegment()
{ 
   // Make sure object is out of the database
   mDatabase->removeFromDatabase(this, getExtent()); 

   // Find any forcefields that were using this as an end point and let them know the segment is gone.  Since 
   // segment is no longer in database, when we recalculate the forcefield, our endSegmentPointer will be reset.
   // This is a last-ditch effort to ensure that the pointers point at something real.

   //fillVector.clear();
   //mGame->getGridDatabase()->findObjects(ForceFieldProjectorType, fillVector);

   //for(S32 i = 0; i < fillVector.size(); i++)
   //{
   //   ForceFieldProjector *proj = dynamic_cast<ForceFieldProjector *>(fillVector[i]);
   //   TNLAssert(proj, "bad cast!");

   //   if(proj->forceFieldEndSegment == this || proj->getMountSegment() == this) 
   //      proj->onGeomChanged();            // Will force recalculation of mount and endpoint
   //}
}
 
// Resets edges of a wall segment to their factory settings; i.e. 4 simple walls representing a simple outline
void WallSegment::resetEdges()
{
   Barrier::resetEdges(corners, edges);
}


void WallSegment::renderFill(const Color &fillColor, bool beingDragged)
{
#ifndef ZAP_DEDICATED

   Color color = fillColor;

   bool enableLineSmoothing = false;
   
   if(glIsEnabled(GL_BLEND)) 
   {
      glDisable(GL_BLEND);
      enableLineSmoothing = true;
   }
   
   color *= beingDragged ? 0.5f : 1;

   renderWallFill(&triangulatedFillPoints, true, color);       // Use true because all segment fills are triangulated

   if(enableLineSmoothing) 
      glEnable(GL_BLEND);
#endif
}

};
