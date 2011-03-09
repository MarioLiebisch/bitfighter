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

#include "gridDB.h"
#include "gameObject.h"
#include "moveObject.h"    // For def of ActualState

namespace Zap
{

// Constructor
GridDatabase::GridDatabase(bool usingGameCoords)
{
   mUsingGameCoords = usingGameCoords;

   mQueryId = 0;
   BucketWidth = usingGameCoords ? 255 : 1;

   for(U32 i = 0; i < BucketRowCount; i++)
      for(U32 j = 0; j < BucketRowCount; j++)
         mBuckets[i][j] = 0;
}


void GridDatabase::addToDatabase(DatabaseObject *theObject, const Rect &extents)
{
   S32 minx, miny, maxx, maxy;
   F32 widthDiv = 1 / F32(BucketWidth);

   minx = S32(extents.min.x * widthDiv);
   miny = S32(extents.min.y * widthDiv);
   maxx = S32(extents.max.x * widthDiv);
   maxy = S32(extents.max.y * widthDiv);

   if(maxx > minx + BucketRowCount)
      maxx = minx + BucketRowCount;
   if(maxy > miny + BucketRowCount)
      maxy = miny + BucketRowCount;

   for(S32 x = minx; x <= maxx; x++)
      for(S32 y = miny; y <= maxy; y++)
      {
         BucketEntry *be = mChunker.alloc();
         be->theObject = theObject;
         be->nextInBucket = mBuckets[x & BucketMask][y & BucketMask];
         mBuckets[x & BucketMask][y & BucketMask] = be;
      }
}


void GridDatabase::removeFromDatabase(DatabaseObject *theObject, const Rect &extents)
{
   S32 minx, miny, maxx, maxy;
   F32 widthDiv = 1 / F32(BucketWidth);

   minx = S32(extents.min.x * widthDiv);
   miny = S32(extents.min.y * widthDiv);
   maxx = S32(extents.max.x * widthDiv);
   maxy = S32(extents.max.y * widthDiv);

   if(maxx > minx + BucketRowCount)
      maxx = minx + BucketRowCount;
   if(maxy > miny + BucketRowCount)
      maxy = miny + BucketRowCount;


   for(S32 x = minx; x <= maxx; x++)
   {
      for(S32 y = miny; y <= maxy; y++)
      {
         for(BucketEntry **walk = &mBuckets[x & BucketMask][y & BucketMask]; *walk; walk = &((*walk)->nextInBucket))
         {
            if((*walk)->theObject == theObject)
            {
               BucketEntry *rem = *walk;
               *walk = rem->nextInBucket;
               mChunker.free(rem);
               break;
            }
         }
      }
   }
}

// Find all objects in &extents that are of type typeMask
void GridDatabase::findObjects(U32 typeMask, Vector<DatabaseObject *> &fillVector, const Rect &extents)
{
   S32 minx, miny, maxx, maxy;

   F32 widthDiv = 1 / F32(BucketWidth);
   minx = S32(extents.min.x * widthDiv);
   miny = S32(extents.min.y * widthDiv);
   maxx = S32(extents.max.x * widthDiv);
   maxy = S32(extents.max.y * widthDiv);

   if(maxx >= minx + BucketRowCount)
      maxx = minx + BucketRowCount - 1;
   if(maxy >= miny + BucketRowCount)
      maxy = miny + BucketRowCount - 1;

   mQueryId++;    // Used to prevent the same item from being found in multiple buckets

   for(S32 x = minx; x <= maxx; x++)
   {
      for(S32 y = miny; y <= maxy; y++)
      {
         for(BucketEntry *walk = mBuckets[x & BucketMask][y & BucketMask]; walk; walk = walk->nextInBucket)
         {
            DatabaseObject *theObject = walk->theObject;

            if(theObject->mLastQueryId != mQueryId &&             // Object hasn't been queried; and
               (theObject->getObjectTypeMask() & typeMask) &&     // is of the right type; and
               theObject->extent.intersects(extents) )            // overlaps our extents
            {
               walk->theObject->mLastQueryId = mQueryId;    // Flag the object so we know we've already visited it
               fillVector.push_back(walk->theObject);       // And save it as a found item
            }
         }
      }
   }
}


// Assumes a polygon in format A-B-C-D if format is true, A-B, C-D, E-F if format is false
bool PolygonLineIntersect(Point *poly, U32 vertexCount, bool format, Point p1, Point p2, 
                          float &collisionTime, Point &normal)
{
   Point v1 = poly[vertexCount - 1];
   Point v2, dv;
   Point dp = p2 - p1;

   S32 inc = format ? 1 : 2;

   F32 currentCollisionTime = 100;

   for(U32 i = 0; i < vertexCount - (inc - 1); i += inc)    // Count by 1s when format is true, 2 when false
   {
      if(format)     // A-B-C-D format ==> examine every contiguous pair of vertices
         v2.set(poly[i]);
      else           // A-B C-D format ==> don't examine segment B-C
      {
         v1.set(poly[i]);
         v2.set(poly[i + 1]);
      }

      // edge from v1 -> v2
      // ray from p1 -> p2

      dv.set(v2 - v1);

      // p1.x + s * dp.x = v1.x + t * dv.x
      // p1.y + s * dp.y = v1.y + t * dv.y

      // s = (v1.x - p1.x + t * dv.x) / dp.x
      // p1.y + dp.y * (v1.x - p1.x + t * dv.x) / dp.x = v1.y + t * dv.y
      // p1.y * dp.x + dp.y * (v1.x - p1.x) + t * dp.y * dv.x =
      //                         v1.y * dp.x + t * dp.x * dv.y
      // t * (dp.y * dv.x - dp.x * dv.y) =
      //                -p1.y * dp.x - dp.y * v1.x + dp.y * p1.x + v1.y * dp.x

      // t = ((p1.x - v1.x) * dp.y + (v1.y - p1.y) * dp.x) / (dp.y * dv.x - dp.x * dv.y)

      // t = (p1.x + s * dp.x - v1.x) / dv.x
      // ( p1.y + s * dp.y - v1.y ) / dv.y = (p1.x + s * dp.x - v1.x) / dv.x
      // s * (dp.y / dv.y) + (p1.y - v1.y) / dv.y =
      // s * (dp.x / dv.x) + (p1.x - v1.x) / dv.x
      // s * dp.y * dv.x + (p1.y - v1.y) * dv.x =
      // s * dp.x * dv.y + (p1.x - v1.x) * dv.y
      // s * ( dp.y * dv.x - dp.x * dv.y ) = (p1.x - v1.x) * dv.y - (p1.y - v1.y) * dv.x

      // s = ( (p1.x - v1.x) * dv.y + (v1.y - p1.y) * dv.x) / ( dp.y * dv.x - dp.x * dv.y)

      F32 denom = dp.y * dv.x - dp.x * dv.y;
      if(denom != 0) // otherwise, the lines are parallel
      {
         F32 s = ( (p1.x - v1.x) * dv.y + (v1.y - p1.y) * dv.x ) / denom;
         F32 t = ( (p1.x - v1.x) * dp.y + (v1.y - p1.y) * dp.x ) / denom;

         if(s >= 0 && s <= 1 && t >= 0 && t <= 1)
         {
            if(s < currentCollisionTime)
            {
               normal.set(dv.y, -dv.x);
               currentCollisionTime = s;
            }
         }
      } 
      v1.set(v2);    // No real effect if format == false
   }
   if(currentCollisionTime <= 1)
   {
      collisionTime = currentCollisionTime;
      return true;
   }
   return false;
}


extern bool FindLowestRootInInterval(F32 inA, F32 inB, F32 inC, F32 inUpperBound, F32 &outX);

bool CircleLineIntersect(Point center, float radius, Point rayStart, Point rayEnd, float &collisionTime)
{
   // if the point is in the circle, it's a collision at the start
   Point d = center - rayStart;
   Point v = rayEnd - rayStart;

   if(d.len() <= radius)
   {
      collisionTime = 0;
      return true;
   }

   // otherwise, solve the following equation for t
   // (d - vt)^2 = radius^2

   float a = v.dot(v);
   float b = -2 * d.dot(v);
   float c = d.dot(d) - radius * radius;

   return FindLowestRootInInterval(a, b, c, 100, collisionTime);
}


// Find objects along a ray, returning first discovered object, along with time of
// that collision and a Point representing the normal angle at intersection point
//             (at least I think that's what's going on here - CE)
DatabaseObject *GridDatabase::findObjectLOS(U32 typeMask, U32 stateIndex, 
                                            const Point &rayStart, const Point &rayEnd, 
                                            float &collisionTime, Point &surfaceNormal)
{
   return findObjectLOS(typeMask, stateIndex, true, rayStart, rayEnd, collisionTime, surfaceNormal);
}


// Format is a passthrough to PolygonLineIntersect().  Will be true for most items, false for walls in editor.
DatabaseObject *GridDatabase::findObjectLOS(U32 typeMask, U32 stateIndex, bool format,
                                            const Point &rayStart, const Point &rayEnd, 
                                            float &collisionTime, Point &surfaceNormal)
{
   Rect queryRect(rayStart, rayEnd);

   static Vector<DatabaseObject *> fillVector;

   fillVector.clear();
   findObjects(typeMask, fillVector, queryRect);

   Point collisionPoint;

   collisionTime = 100;
   DatabaseObject *retObject = NULL;

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      if(!fillVector[i]->isCollisionEnabled())     // Skip collision-disabled objects
         continue;

      static Vector<Point> poly;
      poly.clear();
      Point center;
      F32 radius;
      float ct;

      if(fillVector[i]->getCollisionPoly(poly))
      {
         if(poly.size() == 0)    // This can happen in the editor when a wall segment is completely hidden by another
            continue;

         Point normal;
         if(PolygonLineIntersect(&poly[0], poly.size(), format, rayStart, rayEnd, ct, normal))
         {
            if(ct < collisionTime)
            {
               collisionTime = ct;
               retObject = fillVector[i];
               surfaceNormal = normal;
            }
         }
      }
      else if(fillVector[i]->getCollisionCircle(stateIndex, center, radius))
      {
         if(CircleLineIntersect(center, radius, rayStart, rayEnd, ct))
         {
            if(ct < collisionTime)
            {
               collisionTime = ct;
               surfaceNormal = (rayStart + (rayEnd - rayStart) * ct) - center;
               retObject = fillVector[i];
            }
         }
      }
   }

   if(retObject)
      surfaceNormal.normalize();

   return retObject;
}


bool GridDatabase::pointCanSeePoint(const Point &point1, const Point &point2)
{
   F32 time;
   Point coll;

   return( findObjectLOS(BarrierType, MoveObject::ActualState, true, point1, point2, time, coll) == NULL );
}


////////////////////////////////////////
////////////////////////////////////////

void DatabaseObject::addToDatabase()
{
   if(!mInDatabase)
   {
      mInDatabase = true;
      getGridDatabase()->addToDatabase(this, extent);
   }
}


void DatabaseObject::removeFromDatabase()
{
   if(mInDatabase)
   {
      mInDatabase = false;
      getGridDatabase()->removeFromDatabase(this, extent);
   }
}


// Update object's extents in the database -- will not add object to database if it's not already in it
void DatabaseObject::setExtent(const Rect &extents)
{
   GridDatabase *gridDB = getGridDatabase();

   if(mInDatabase && gridDB != NULL)
   {
      // Remove from the extents database for current extents...
      gridDB->removeFromDatabase(this, extent);    // old extent
      // ...and re-add for the new extent
      gridDB->addToDatabase(this, extents);
   }

   extent.set(extents);
}


};


