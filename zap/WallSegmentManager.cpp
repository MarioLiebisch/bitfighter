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

#include "WallSegmentManager.h"
#include "gameObjectRender.h"
#include "EngineeredItem.h"         // For forcefieldprojector def
#include "game.h"

#ifndef ZAP_DEDICATED 
#   include "UI.h"  // for glColor(Color)
#   include "SDL/SDL_opengl.h"
#endif


using namespace TNL;

namespace Zap
{
   

// Constructor
WallSegmentManager::WallSegmentManager()
{
   // These deleted in the destructor
   mWallSegmentDatabase = new GridDatabase(false);      
   mWallEdgeDatabase = new GridDatabase(false);
}


// Destructor
WallSegmentManager::~WallSegmentManager()
{
   delete mWallEdgeDatabase;
   delete mWallSegmentDatabase;

   deleteSegments();
   deleteEdges();
}


GridDatabase *WallSegmentManager::getWallSegmentDatabase()
{
   return mWallSegmentDatabase;
}


GridDatabase *WallSegmentManager::getWallEdgeDatabase()
{
   return mWallEdgeDatabase;
}


// This variant only resnaps engineered items that were attached to a segment that moved
void WallSegmentManager::finishedChangingWalls(EditorObjectDatabase *editorObjectDatabase, S32 changedWallSerialNumber)
{
   rebuildEdges();         // Rebuild all edges for all walls

   // This block is a modified version of updateAllMountedItems that homes in on a particular segment
   // First, find any items directly mounted on our wall, and update their location.  Because we don't know where the wall _was_, we 
   // will need to search through all the engineered items, and query each to find which ones where attached to the wall that moved.
   fillVector.clear();
   editorObjectDatabase->findObjects((TestFunc)isEngineeredType, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      EngineeredItem *engrItem = dynamic_cast<EngineeredItem *>(fillVector[i]);     // static_cast doesn't seem to work

      // Remount any engr items that were either not attached to any wall, or were attached to any segments on the modified wall
      if(engrItem->getMountSegment() == NULL || engrItem->getMountSegment()->getOwner() == changedWallSerialNumber)
         engrItem->mountToWall(engrItem->getVert(0), editorObjectDatabase->getWallSegmentManager());

      // Calculate where all ffs land -- no telling if the segment we moved is or was interfering in its path
      if(!engrItem->isTurret())
      {
         ForceFieldProjector *ffp = static_cast<ForceFieldProjector *>(engrItem);
         ffp->findForceFieldEnd();
      }
   }

   rebuildSelectedOutline();
}


void WallSegmentManager::finishedChangingWalls(EditorObjectDatabase *editorDatabase)
{
   rebuildEdges();         // Rebuild all edges for all walls
   updateAllMountedItems(editorDatabase);
   rebuildSelectedOutline();
}


// This function clears the WallSegment database, and refills it with the output of clipper
void WallSegmentManager::recomputeAllWallGeometry(GridDatabase *gameDatabase)
{
   buildAllWallSegmentEdgesAndPoints(gameDatabase);
   rebuildEdges();
}


// Take geometry from all wall segments, and run them through clipper to generate new edge geometry.  Then use the results to create
// a bunch of WallEdge objects, which will be stored in mWallEdgeDatabase for future reference.  The two key things to understand here
// are that 1) it's all-or-nothing: all edges need to be recomputed at once; there is no way to do a partial rebuild.  And 2) the edges
// cannot be associated with their source segment, so we'll need to rely on other tricks to find an associated wall when needed.
void WallSegmentManager::rebuildEdges()
{
   mWallEdgePoints.clear();

   // Run clipper
   clipAllWallEdges(mWallSegments, mWallEdgePoints);    

   deleteEdges();
   mWallEdges.resize(mWallEdgePoints.size() / 2);

   // Add clipped wallEdges to the spatial database
   for(S32 i = 0; i < mWallEdgePoints.size(); i+=2)
   {
      WallEdge *newEdge = new WallEdge(mWallEdgePoints[i], mWallEdgePoints[i+1], mWallEdgeDatabase);    // Create the edge object
      mWallEdges[i/2] = newEdge;
   }
}


// Delete all segments, then find all walls and build a new set of segments
void WallSegmentManager::buildAllWallSegmentEdgesAndPoints(GridDatabase *database)
{
   deleteSegments();

   fillVector.clear();
   database->findObjects((TestFunc)isWallType, fillVector);

   Vector<DatabaseObject *> engrObjects;
   database->findObjects((TestFunc)isEngineeredType, engrObjects);   // All engineered objects

   // Iterate over all our wall objects
   for(S32 i = 0; i < fillVector.size(); i++)
      buildWallSegmentEdgesAndPoints(database, fillVector[i], engrObjects);
}


// Given a wall, build all the segments and related geometry; also manage any affected mounted items
// Operates only on passed wall segment -- does not alter others
void WallSegmentManager::buildWallSegmentEdgesAndPoints(GridDatabase *database, DatabaseObject *wallDbObject, 
                                                        const Vector<DatabaseObject *> &engrObjects)
{
#ifndef ZAP_DEDICATED
   // Find any engineered objects that terminate on this wall, and mark them for resnapping later

   Vector<EngineeredItem *> toBeRemounted;    // A list of engr objects terminating on the wall segment that we'll be deleting

   EditorObject *wall = dynamic_cast<EditorObject *>(wallDbObject);     // Wall we're deleting and rebuilding
   TNLAssert(wall, "Bad cast -- expected an EditorObject!");
   

   S32 count = mWallSegments.size(); 

   // Loop through all the walls, and, for each, see if any of the engineered objects we were given are mounted to it
   for(S32 i = 0; i < count; i++)
      if(mWallSegments[i]->getOwner() == wall->getSerialNumber())       // Segment belongs to wall
         for(S32 j = 0; j < engrObjects.size(); j++)                    // Loop through all engineered objects checking the mount seg
         {
            EngineeredItem *engrObj = dynamic_cast<EngineeredItem *>(engrObjects[j]);

            // Does FF start or end on this segment?
            if(engrObj->getMountSegment() == mWallSegments[i] || engrObj->getEndSegment() == mWallSegments[i])
               toBeRemounted.push_back(engrObj);
         }

   // Get rid of any existing segments that correspond to our wall; we'll be building new ones
   deleteSegments(wall->getSerialNumber());

   Rect allSegExtent;

   // Polywalls will have one segment; it will have the same geometry as the polywall itself
   if(wall->getObjectTypeNumber() == PolyWallTypeNumber)
   {
      WallSegment *newSegment = new WallSegment(mWallSegmentDatabase, *wall->getOutline(), wall->getSerialNumber());
      mWallSegments.push_back(newSegment);
   }

   // Traditional walls will be represented by a series of rectangles, each representing a "puffed out" pair of sequential vertices
   else     
   {
      TNLAssert(dynamic_cast<WallItem *>(wall), "Expected an WallItem!");
      WallItem *wallItem = static_cast<WallItem *>(wall);

      // Create a WallSegment for each sequential pair of vertices
      for(S32 i = 0; i < wallItem->extendedEndPoints.size(); i += 2)
      {
         WallSegment *newSegment = new WallSegment(mWallSegmentDatabase, wallItem->extendedEndPoints[i], wallItem->extendedEndPoints[i+1], 
                                                   (F32)wallItem->getWidth(), wallItem->getSerialNumber());    // Create the segment

         if(i == 0)
            allSegExtent.set(newSegment->getExtent());
         else
            allSegExtent.unionRect(newSegment->getExtent());

         mWallSegments.push_back(newSegment);          // And add it to our master segment list
      }
   }

   wall->setExtent(allSegExtent);      // A wall's extent is the union of the extents of all its segments.  Makes sense, right?

   // Remount all turrets & forcefields mounted on or terminating on any of the wall segments we deleted and potentially recreated
   for(S32 j = 0; j < toBeRemounted.size(); j++)  
      toBeRemounted[j]->mountToWall(toBeRemounted[j]->getVert(0), database->getWallSegmentManager());

#endif
}


// Used above and from instructions
void WallSegmentManager::clipAllWallEdges(const Vector<WallSegment *> &wallSegments, Vector<Point> &wallEdges)
{
   Vector<const Vector<Point> *> inputPolygons;
   Vector<Vector<Point> > solution;

   for(S32 i = 0; i < wallSegments.size(); i++)
      inputPolygons.push_back(wallSegments[i]->getCorners());

   mergePolys(inputPolygons, solution);      // Merged wall segments are placed in solution

   unpackPolygons(solution, wallEdges);
}


// Called by WallItems and PolyWalls when their geom changes
void WallSegmentManager::updateAllMountedItems(EditorObjectDatabase *database)
{
   // First, find any items directly mounted on our wall, and update their location.  Because we don't know where the wall _was_, we 
   // will need to search through all the engineered items, and query each to find which ones where attached to the wall that moved.
   fillVector.clear();
   database->findObjects((TestFunc)isEngineeredType, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      EngineeredItem *engrItem = dynamic_cast<EngineeredItem *>(fillVector[i]);     // static_cast doesn't seem to work
      engrItem->mountToWall(engrItem->getVert(0), database->getWallSegmentManager());
   }
}


// Called when a wall segment has somehow changed.  All current and previously intersecting segments 
// need to be recomputed.  This only operates on the specified item.  rebuildEdges() will need to be run separately.
void WallSegmentManager::computeWallSegmentIntersections(GridDatabase *gameObjDatabase, EditorObject *item)
{
   Vector<DatabaseObject *> engrObjects;
   gameObjDatabase->findObjects((TestFunc)isEngineeredType, engrObjects);   // All engineered objects

   buildWallSegmentEdgesAndPoints(gameObjDatabase, item, engrObjects);
}


void WallSegmentManager::clear()
{
   mWallEdgeDatabase->removeEverythingFromDatabase();
   mWallSegmentDatabase->removeEverythingFromDatabase();
   deleteSegments();
   deleteEdges();

   mWallEdgePoints.clear();
}


void WallSegmentManager::clearSelected()
{
   for(S32 i = 0; i < mWallSegments.size(); i++)
      mWallSegments[i]->setSelected(false);
}


void WallSegmentManager::setSelected(S32 owner, bool selected)
{
   for(S32 i = 0; i < mWallSegments.size(); i++)
      if(mWallSegments[i]->getOwner() == owner)
         mWallSegments[i]->setSelected(selected);
}


void WallSegmentManager::rebuildSelectedOutline()
{
   Vector<WallSegment *> selectedSegments;

   for(S32 i = 0; i < mWallSegments.size(); i++)
      if(mWallSegments[i]->isSelected())
         selectedSegments.push_back(mWallSegments[i]);

   clipAllWallEdges(selectedSegments, mSelectedWallEdgePoints);    // Populate edgePoints from segments
}


void WallSegmentManager::deleteSegments()
{
   mWallSegments.deleteAndClear();
}


void WallSegmentManager::deleteEdges()
{
   mWallEdges.deleteAndClear();
}


// Delete all wall segments owned by specified owner
void WallSegmentManager::deleteSegments(S32 owner)
{
   S32 count = mWallSegments.size();

   for(S32 i = 0; i < count; i++)
      if(mWallSegments[i]->getOwner() == owner)
      {
         delete mWallSegments[i];      // Destructor will remove segment from database
         mWallSegments.erase_fast(i);  // Order of segments isn't important
         i--;
         count--;
      }
}


extern Color EDITOR_WALL_FILL_COLOR;

// Only called from the editor -- renders both walls and polywalls.  Does not render centerlines
void WallSegmentManager::renderWalls(GameSettings *settings, F32 currentScale, bool dragMode, const Point &selectedItemOffset, 
                                     bool previewMode, bool showSnapVertices, F32 alpha)
{
#ifndef ZAP_DEDICATED
   // We'll use the editor color most of the time; only in preview mode in the editor do we use the game color
   bool useGameColor = UserInterface::current && UserInterface::current->getMenuID() == EditorUI && previewMode;
   Color fillColor = useGameColor ? settings->getWallFillColor() : EDITOR_WALL_FILL_COLOR;

   bool moved = (selectedItemOffset.x != 0 || selectedItemOffset.y != 0);

   // Render walls that have been moved first (i.e. render their shadows)
   glColor(.1);
   if(moved)
      for(S32 i = 0; i < mWallSegments.size(); i++)
         if(mWallSegments[i]->isSelected())     
            mWallSegments[i]->renderFill(Point(0,0));


   // Then normal, unselected walls
   glColor(fillColor);
   for(S32 i = 0; i < mWallSegments.size(); i++)
      if(!moved || !mWallSegments[i]->isSelected())         
         mWallSegments[i]->renderFill(selectedItemOffset);      // renderFill ignores offset for unselected walls

   renderWallEdges(&mWallEdgePoints, settings->getWallOutlineColor());     // Render wall outlines

   // Render selected/moving walls last so they appear on top
   glColor(fillColor);
   if(moved)
   {
      for(S32 i = 0; i < mWallSegments.size(); i++)
         if(mWallSegments[i]->isSelected())  
            mWallSegments[i]->renderFill(selectedItemOffset);

      // Render wall outlines for selected walls only
      renderWallEdges(&mSelectedWallEdgePoints, selectedItemOffset, settings->getWallOutlineColor());      
   }

   if(showSnapVertices)
   {
      glLineWidth(gLineWidth1);

      //glColor(Colors::magenta);
      for(S32 i = 0; i < mWallEdgePoints.size(); i++)
         renderSmallSolidVertex(currentScale, mWallEdgePoints[i], dragMode);

      glLineWidth(gDefaultLineWidth);
   }
#endif
}


};