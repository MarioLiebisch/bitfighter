
//-----------------------------------------------------------------------------------
//
// Bitfighter - A multiplayer vector graphics space game
// Based on Zap demo relased for Torque Network Library by GarageGames.com
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

#include "moveObject.h"
#include "gameType.h"
#include "goalZone.h"
#include "GeomUtils.h"
#include "ship.h"
#include "SoundSystem.h"
#include "speedZone.h"
#include "game.h"
#include "gameConnection.h"
#include "stringUtils.h"

#include "luaObject.h"

#ifndef ZAP_DEDICATED
#  include "ClientGame.h"
#  include "sparkManager.h"
#  include "UI.h" // for extern void glColor
#  include "UIEditorMenus.h"
#endif

#include "LuaWrapper.h"

#include <math.h>

namespace Zap
{

MoveObject::MoveObject(const Point &pos, F32 radius, F32 mass) : Parent(pos, radius)    // Constructor
{
   for(U32 i = 0; i < MoveStateCount; i++)
   {
      mMoveState[i].pos = pos;
      mMoveState[i].angle = 0;
   }

   mMass = mass;
   mInterpolating = false;
   mHitLimit = 16;
}


void MoveObject::idle(BfObject::IdleCallPath path)
{
   mHitLimit = 16;      // Reset hit limit
}


void MoveObject::onAddedToGame(Game *game)
{
   Parent::onAddedToGame(game);

#ifndef ZAP_DEDICATED
   if(isGhost())     // Client only
      this->setControllingClient(static_cast<ClientGame *>(game)->getConnectionToServer());
#endif
}

static const float MoveObjectCollisionElasticity = 1.7f;


// Update object's extents in the database
void MoveObject::updateExtentInDatabase()
{
   Rect r(mMoveState[ActualState].pos, mMoveState[RenderState].pos);
   r.expand(Point(mRadius + 10, mRadius + 10));
   setExtent(r);
}


bool MoveObject::isMoveObject()
{
   return true;
}


Point MoveObject::getVert(S32 index) const
{
   return getActualPos();
}


Point MoveObject::getPos() const
{
   return getActualPos();
}


Point MoveObject::getVel()
{
   return getActualVel();
}


Point MoveObject::getRenderPos() const
{
   return mMoveState[RenderState].pos;
}


Point MoveObject::getActualPos() const
{
   return mMoveState[ActualState].pos;
}


Point MoveObject::getRenderVel() const
{
   return mMoveState[RenderState].vel;
}


Point MoveObject::getActualVel() const
{
   return mMoveState[ActualState].vel;
}


void MoveObject::setActualPos(const Point &pos)
{
   mMoveState[ActualState].pos = pos;
}


void MoveObject::setActualVel(const Point &vel)
{
   mMoveState[ActualState].vel = vel;
}


void MoveObject::setVert(const Point &pos, S32 index)
{
   Parent::setVert(pos, index);
   setPos(pos);
}


// For Geometry, should set both actual and render position
void MoveObject::setPos(const Point &pos)
{
   mMoveState[ActualState].pos = pos;
   mMoveState[RenderState].pos = pos;
   Parent::setVert(pos, 0);      // Kind of hacky... need to get this point into the geom object, need to avoid stack overflow
   updateExtentInDatabase();
}


F32 MoveObject::getMass()
{
   return mMass;
}


void MoveObject::setMass(F32 mass)
{
   mMass = mass;
}


// Ship movement system
// Identify the several cases in which a ship may be moving:
// if this is a client:
//   Ship controlled by this client.  Pos may have been set to something else by server, leaving renderPos elsewhere
//     all movement updates affect pos

// collision process for ships:
//

//
// ship *theShip;
// F32 time;
// while(time > 0)
// {
//    ObjHit = findFirstCollision(theShip);
//    advanceToCollision();
//    if(velocitiesColliding)
//    {
//       doCollisionResponse();
//    }
//    else
//    {
//       computeMinimumSeperationTime(ObjHit);
//       displaceObject(ObjHit, seperationTime);
//    }
// }
//
// displaceObject(Object, time)
// {
//    while(time > 0)
//    {
//       ObjHit = findFirstCollision();
//       advanceToCollision();
//       if(velocitiesColliding)
//       {
//          doCollisionResponse();
//          return;
//       }
//       else
//       {
//          computeMinimumSeperationTime(ObjHit);
//          displaceObject(ObjHit, seperationTime);
//       }
//    }
// }


// See http://flipcode.com/archives/Theory_Practice-Issue_01_Collision_Detection.shtml --> Example 1  May or may not be relevant
F32 MoveObject::computeMinSeperationTime(U32 stateIndex, MoveObject *contactShip, Point intendedPos)
{
   F32 myRadius;
   F32 contactShipRadius;

   Point myPos;
   Point contactShipPos;

   getCollisionCircle(stateIndex, myPos, myRadius);   // getCollisionCircle sets myPos and myRadius

   contactShip->getCollisionCircle(stateIndex, contactShipPos, contactShipRadius);

   // Find out if either of the colliding objects uses collisionPolys or not
   //Vector<Point> dummy;
   //F32 fixfact = (getCollisionPoly(dummy) || contactShip->getCollisionPoly(dummy)) ? 0 : 1;

   Point v = contactShip->mMoveState[stateIndex].vel;
   Point posDelta = contactShipPos - intendedPos;

   F32 R = myRadius + contactShipRadius;

   F32 a = v.dot(v);
   F32 b = 2 * v.dot(posDelta);
   F32 c = posDelta.dot(posDelta) - R * R;

   F32 t;

   bool result = FindLowestRootInInterval(a, b, c, 100000, t);

   return result ? t : -1;
}

const F32 moveTimeEpsilon = 0.000001f;
const F32 velocityEpsilon = 0.00001f;

// Apply mMoveState info to an object to compute it's new position.  Used for ships et. al.
// isBeingDisplaced is true when the object is being pushed by something else, which will only happen in a collision
// Remember: stateIndex will be one of 0-ActualState, 1-RenderState, or 2-LastProcessState
void MoveObject::move(F32 moveTime, U32 stateIndex, bool isBeingDisplaced, Vector<SafePtr<MoveObject> > displacerList)
{
   TNLAssert(this, "'THIS' is NULL");

   U32 tryCount = 0;
   const U32 TRY_COUNT_MAX = 8;
   Vector<SafePtr<BfObject> > disabledList;
   F32 moveTimeStart = moveTime;

   while(moveTime > moveTimeEpsilon && tryCount < TRY_COUNT_MAX)     // moveTimeEpsilon is a very short, but non-zero, bit of time
   {
      tryCount++;

      // Ignore tiny movements unless we're processing a collision
      if(!isBeingDisplaced && mMoveState[stateIndex].vel.len() < velocityEpsilon)
         break;

      F32 collisionTime = moveTime;
      Point collisionPoint;

      BfObject *objectHit = findFirstCollision(stateIndex, collisionTime, collisionPoint);
      if(!objectHit)    // No collision (or if isBeingDisplaced is true, we haven't been pushed into another object)
      {
         mMoveState[stateIndex].pos += mMoveState[stateIndex].vel * moveTime;    // Move to desired destination
         break;
      }

      // Collision!  Advance to the point of collision
      mMoveState[stateIndex].pos += mMoveState[stateIndex].vel * collisionTime;

      if(objectHit->isMoveObject())     // Collided with a MoveObject
      {
         MoveObject *moveObjectThatWasHit = static_cast<MoveObject *>(objectHit);  

         Point velDelta = moveObjectThatWasHit->mMoveState[stateIndex].vel - mMoveState[stateIndex].vel;
         Point posDelta = moveObjectThatWasHit->mMoveState[stateIndex].pos - mMoveState[stateIndex].pos;

         // Prevent infinite loops with a series of objects trying to displace each other forever
         if(isBeingDisplaced)
         {
            bool hit = false;
            for(S32 i = 0; i < displacerList.size(); i++)
               if(moveObjectThatWasHit == displacerList[i])
                 hit = true;
            if(hit) break;
         }
 
         if(posDelta.dot(velDelta) < 0)   // moveObjectThatWasHit is closing faster than we are ???
         {
            computeCollisionResponseMoveObject(stateIndex, moveObjectThatWasHit);
            if(isBeingDisplaced)
               break;
         }
         else                            // We're moving faster than the object we hit (I think)
         {
            Point intendedPos = mMoveState[stateIndex].pos + mMoveState[stateIndex].vel * moveTime;

            F32 displaceEpsilon = 0.002f;
            F32 t = computeMinSeperationTime(stateIndex, moveObjectThatWasHit, intendedPos);
            if(t <= 0)
               break;   // Some kind of math error, couldn't find result: stop simulating this ship

            // Note that we could end up with an infinite feedback loop here, if, for some reason, two objects keep trying to displace
            // one another, as this will just recurse deeper and deeper.

            displacerList.push_back(this);

            // Only try a limited number of times to avoid dragging the game under the dark waves of infinity
            if(mHitLimit > 0) 
            {
               // Move the displaced object a tiny bit, true -> isBeingDisplaced
               moveObjectThatWasHit->move(t + displaceEpsilon, stateIndex, true, displacerList); 
               mHitLimit--;
            }
         }
      }
      else if(isCollideableType(objectHit->getObjectTypeNumber()))
      {
         computeCollisionResponseBarrier(stateIndex, collisionPoint);
      }
      else if(objectHit->getObjectTypeNumber() == SpeedZoneTypeNumber)
      {
         TNLAssert(dynamic_cast<SpeedZone *>(objectHit), "Not a SpeedZone error, but Object Type is SpeedZoneTypeNumber should always be SpeedZone");
         SpeedZone *speedZone = (SpeedZone *)objectHit;
         speedZone->collided(this, stateIndex);
         disabledList.push_back(objectHit);
         objectHit->disableCollision();
         tryCount--;   // SpeedZone don't count as tryCount
      }
      moveTime -= collisionTime;
   }
   for(S32 i = 0; i < disabledList.size(); i++)   // enable any disabled collision
      if(disabledList[i].isValid())
         disabledList[i]->enableCollision();

   if(tryCount == TRY_COUNT_MAX && moveTime > moveTimeStart * 0.98f)
      mMoveState[stateIndex].vel.set(0,0); // prevents some overload by not trying to move anymore
}


bool MoveObject::collide(BfObject *otherObject)
{
   return true;
}


TestFunc MoveObject::collideTypes()
{
   return (TestFunc) isAnyObjectType;
}


static S32 QSORT_CALLBACK sortBarriersFirst(DatabaseObject **a, DatabaseObject **b)
{
   return ((*b)->getObjectTypeNumber() == BarrierTypeNumber ? 1 : 0) - ((*a)->getObjectTypeNumber() == BarrierTypeNumber ? 1 : 0);
}


BfObject *MoveObject::findFirstCollision(U32 stateIndex, F32 &collisionTime, Point &collisionPoint)
{
   // Check for collisions against other objects
   Point delta = mMoveState[stateIndex].vel * collisionTime;

   Rect queryRect(mMoveState[stateIndex].pos, mMoveState[stateIndex].pos + delta);
   queryRect.expand(Point(mRadius, mRadius));

   fillVector.clear();

   findObjects(collideTypes(), fillVector, queryRect);   // Free CPU for finding only the ones we care about

   fillVector.sort(sortBarriersFirst);  // Sort to do Barriers::Collide first, to prevent picking up flag (FlagItem::Collide) through Barriers, especially when client does /maxfps 10

   F32 collisionFraction;

   BfObject *collisionObject = NULL;
   Vector<Point> poly;

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      BfObject *foundObject = dynamic_cast<BfObject *>(fillVector[i]);

      if(!foundObject->isCollisionEnabled())
         continue;

      poly.clear();

      if(foundObject->getCollisionPoly(poly))
      {
         Point cp;

         if(PolygonSweptCircleIntersect(&poly[0], poly.size(), mMoveState[stateIndex].pos,
                                        delta, mRadius, cp, collisionFraction))
         {
            if(cp != mMoveState[stateIndex].pos)   // avoid getting stuck inside polygon wall
            {
               bool collide1 = collide(foundObject);
               bool collide2 = foundObject->collide(this);

               if(!(collide1 && collide2))
                  continue;

               collisionPoint = cp;
               delta *= collisionFraction;
               collisionTime *= collisionFraction;
               collisionObject = foundObject;

               if(!collisionTime)
                  break;
            }
         }
      }
      else
      {
         F32 myRadius;
         F32 otherRadius;
         Point myPos;
         Point shipPos;

         getCollisionCircle(stateIndex, myPos, myRadius);
         if(foundObject->getCollisionCircle(stateIndex, shipPos, otherRadius))
         {

            Point v = mMoveState[stateIndex].vel;
            Point p = myPos - shipPos;

            if(v.dot(p) < 0)
            {
               F32 R = myRadius + otherRadius;
               if(p.len() <= R)
               {
                  bool collide1 = collide(foundObject);
                  bool collide2 = foundObject->collide(this);

                  if(!(collide1 && collide2))
                     continue;

                  collisionTime = 0;
                  collisionObject = foundObject;
                  delta.set(0,0);

                  p.normalize(myRadius);  // we need this calculation, just to properly show bounce sparks at right position
                  collisionPoint = myPos - p;
               }
               else
               {
                  F32 a = v.dot(v);
                  F32 b = 2 * p.dot(v);
                  F32 c = p.dot(p) - R * R;
                  F32 t;
                  if(FindLowestRootInInterval(a, b, c, collisionTime, t))
                  {
                     bool collide1 = collide(foundObject);
                     bool collide2 = foundObject->collide(this);

                     if(!(collide1 && collide2))
                        continue;

                     collisionTime = t;
                     collisionObject = foundObject;
                     delta = mMoveState[stateIndex].vel * collisionTime;

                     p.normalize(otherRadius);  // we need this calculation, just to properly show bounce sparks at right position
                     collisionPoint = shipPos + p;
                  }
               }
            }
         }
      }
   }
   return collisionObject;
}


// Collided with BarrierType, EngineeredType, or ForceFieldType.  What's the response?
void MoveObject::computeCollisionResponseBarrier(U32 stateIndex, Point &collisionPoint)
{
   // Reflect the velocity along the collision point
   Point normal = mMoveState[stateIndex].pos - collisionPoint;
   normal.normalize();

   mMoveState[stateIndex].vel -= normal * MoveObjectCollisionElasticity * normal.dot(mMoveState[stateIndex].vel);

#ifndef ZAP_DEDICATED
   // Emit some bump particles on client
   if(isGhost())     // i.e. on client side
   {
      F32 scale = normal.dot(mMoveState[stateIndex].vel) * 0.01f;
      if(scale > 0.5f)
      {
         // Make a noise...
         SoundSystem::playSoundEffect(SFXBounceWall, collisionPoint, Point(), getMin(1.0f, scale - 0.25f));

         Color bumpC(scale/3, scale/3, scale);

         for(S32 i = 0; i < 4 * pow((F32)scale, 0.5f); i++)
         {
            Point chaos(TNL::Random::readF(), TNL::Random::readF());
            chaos *= scale + 1;

            TNLAssert(dynamic_cast<ClientGame *>(getGame()) != NULL, "Not a ClientGame");

            if(TNL::Random::readF() > 0.5)
               static_cast<ClientGame *>(getGame())->emitSpark(collisionPoint, 
                                                               normal * chaos.len() + Point(normal.y, -normal.x) * scale * 5  + chaos + 
                                                                         mMoveState[stateIndex].vel*0.05f, bumpC);

            if(TNL::Random::readF() > 0.5)
               static_cast<ClientGame *>(getGame())->emitSpark(collisionPoint, 
                                                               normal * chaos.len() + Point(normal.y, -normal.x) * scale * -5 + chaos + 
                                                                         mMoveState[stateIndex].vel*0.05f, bumpC);
         }
      }
   }
#endif
}


// Runs on both client and server side...
void MoveObject::computeCollisionResponseMoveObject(U32 stateIndex, MoveObject *moveObjectThatWasHit)
{
   // collisionVector is simply a line from the center of moveObjectThatWasHit to the center of this
   Point collisionVector = moveObjectThatWasHit->mMoveState[stateIndex].pos -mMoveState[stateIndex].pos;

   collisionVector.normalize();

   bool moveObjectThatWasHitWasMovingTooSlow = (moveObjectThatWasHit->mMoveState[stateIndex].vel.lenSquared() < 0.001f);

   // Initial velocities projected onto collisionVector
   F32 v1i = mMoveState[stateIndex].vel.dot(collisionVector);
   F32 v2i = moveObjectThatWasHit->mMoveState[stateIndex].vel.dot(collisionVector);

   F32 v1f, v2f;     // Final velocities

   F32 e = 0.9f;     // Elasticity, I think

   v1f = (e * moveObjectThatWasHit->mMass * (v2i - v1i) + mMass * v1i + moveObjectThatWasHit->mMass * v2i) / (mMass + moveObjectThatWasHit->mMass);
   v2f = (e *                       mMass * (v1i - v2i) + mMass * v1i + moveObjectThatWasHit->mMass * v2i) / (mMass + moveObjectThatWasHit->mMass);

   moveObjectThatWasHit->mMoveState[stateIndex].vel += collisionVector * (v2f - v2i);
   mMoveState[stateIndex].vel += collisionVector * (v1f - v1i);

   if(!isGhost())    // Server only
   {
      // Check for asteroids hitting a ship
      Ship *ship = dynamic_cast<Ship *>(moveObjectThatWasHit);
      Asteroid *asteroid = dynamic_cast<Asteroid *>(this);
 
      if(!ship)
      {
         // Since asteroids and ships are both MoveObjects, we'll also check to see if ship hit an asteroid
         ship = dynamic_cast<Ship *>(this);
         asteroid = dynamic_cast<Asteroid *>(moveObjectThatWasHit);
      }

      if(ship && asteroid)      // Collided!  Do some damage!  Bring it on!
      {
         DamageInfo theInfo;
         theInfo.collisionPoint = mMoveState[ActualState].pos;
         theInfo.damageAmount = 1.0f;     // Kill ship
         theInfo.damageType = DamageTypePoint;
         theInfo.damagingObject = asteroid;
         theInfo.impulseVector = mMoveState[ActualState].vel;

         ship->damageObject(&theInfo);
      }
   }
#ifndef ZAP_DEDICATED
   else     // Client only
   {
      playCollisionSound(stateIndex, moveObjectThatWasHit, v1i);

      MoveItem *item = dynamic_cast<MoveItem *>(moveObjectThatWasHit);
      GameType *gameType = getGame()->getGameType();

      if(item && gameType && moveObjectThatWasHitWasMovingTooSlow)  // only if not moving fast, to prevent some overload
         gameType->c2sResendItemStatus(item->getItemId());
   }
#endif
}


void MoveObject::playCollisionSound(U32 stateIndex, MoveObject *moveObjectThatWasHit, F32 velocity)
{
   if(velocity > 0.25)    // Make sound if the objects are moving fast enough
      SoundSystem::playSoundEffect(SFXBounceObject, moveObjectThatWasHit->mMoveState[stateIndex].pos);
}


void MoveObject::updateInterpolation()
{
   U32 deltaT = mCurrentMove.time;
   {
      mMoveState[RenderState].angle = mMoveState[ActualState].angle;

      if(mInterpolating)
      {
         // first step is to constrain the render velocity to
         // the vector of difference between the current position and
         // the actual position.
         // we can also clamp to zero, the actual velocity, or the
         // render velocity, depending on which one is best.

         Point deltaP = mMoveState[ActualState].pos - mMoveState[RenderState].pos;
         F32 distance = deltaP.len();

         if(!distance)
            goto interpDone;

         deltaP.normalize();
         F32 rvel = deltaP.dot(mMoveState[RenderState].vel);
         F32 avel = deltaP.dot(mMoveState[ActualState].vel);

         if(rvel < avel)
            rvel = avel;
         if(rvel < 0)
            rvel = 0;

         bool hit = true;
         float time = deltaT * 0.001f;
         if(rvel * time > distance)
            goto interpDone;

         float requestVel = distance / time;
         float interpMaxVel = InterpMaxVelocity;
         float currentActualVelocity = mMoveState[ActualState].vel.len();
         if(interpMaxVel < currentActualVelocity)
            interpMaxVel = currentActualVelocity;
         if(requestVel > interpMaxVel)
         {
            hit = false;
            requestVel = interpMaxVel;
         }
         F32 a = (requestVel - rvel) / time;
         if(a > InterpAcceleration)
         {
            a = InterpAcceleration;
            hit = false;
         }

         if(hit)
            goto interpDone;

         rvel += a * time;
         mMoveState[RenderState].vel = deltaP * rvel;
         mMoveState[RenderState].pos += mMoveState[RenderState].vel * time;
      }
      else
      {
   interpDone:
         mInterpolating = false;
         mMoveState[RenderState] = mMoveState[ActualState];
      }
   }
}

bool MoveObject::getCollisionCircle(U32 stateIndex, Point &point, F32 &radius) const
{
   point = mMoveState[stateIndex].pos;
   radius = mRadius;
   return true;
}


void MoveObject::onGeomChanged()
{
   // This is here, to make sure pressing TAB in editor will show correct location for MoveItems
   //mMoveState[ActualState].pos = getActualPos();
   //mMoveState[RenderState].pos = getRenderPos();
   
   Parent::onGeomChanged();
}


void MoveObject::computeImpulseDirection(DamageInfo *theInfo)
{
   // Compute impulse direction
   Point dv = theInfo->impulseVector - mMoveState[ActualState].vel;
   Point iv = mMoveState[ActualState].pos - theInfo->collisionPoint;
   iv.normalize();
   mMoveState[ActualState].vel += iv * dv.dot(iv) * 0.3f / mMass;
}

////
// LuaItem interface

S32 MoveObject::getVel(lua_State *L)
{
   return LuaObject::returnPoint(L, getActualVel());
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
MoveItem::MoveItem(Point p, bool collideable, float radius, float mass) : MoveObject(p, radius, mass)
{
   mIsMounted = false;
   mIsCollideable = collideable;
   mInitial = false;

   updateTimer = 0;

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
MoveItem::~MoveItem()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


bool MoveItem::processArguments(S32 argc, const char **argv, Game *game)
{
   if(argc < 2)
      return false;
   else if(!Parent::processArguments(argc, argv, game))
      return false;

   Point pos = getActualPos();

   for(U32 i = 0; i < MoveStateCount; i++)
      mMoveState[i].pos = pos;

   updateExtentInDatabase();

   return true;
}


// Server only
string MoveItem::toString(F32 gridSize) const
{
   return string(getClassName()) + " " + geomToString(gridSize);
}


// Client only, in-game
void MoveItem::render()
{
   // If the item is mounted, renderItem will be called from the ship it is mounted to
   if(mIsMounted)
      return;

   renderItem(mMoveState[RenderState].pos);
}


// Runs on both client and server, comes from collision() on the server and the colliding client, and from
// unpackUpdate() in the case of all clients
//
// theShip could be NULL here, and this could still be legit (e.g. flag is in scope, and ship is out of scope)
void MoveItem::mountToShip(Ship *theShip)     
{
   TNLAssert(isGhost() || isInDatabase(), "Error, mount item not in database.");

   if(mMount.isValid() && mMount == theShip)    // Already mounted on ship!  Nothing to do!
      return;

   if(mMount.isValid())                         // Mounted on something else; dismount!
      dismount();

   mMount = theShip;
   if(theShip)
      theShip->mMountedItems.push_back(this);

   mIsMounted = true;
   setMaskBits(MountMask);
}


void MoveItem::setMountedMask()
{
   setMaskBits(MountMask);
}


void MoveItem::setPositionMask()
{
   setMaskBits(PositionMask);
}


bool MoveItem::isMounted()
{
   return mIsMounted;
}


bool MoveItem::isItemThatMakesYouVisibleWhileCloaked()
{
   return true;
}


void MoveItem::setCollideable(bool isCollideable)
{
   mIsCollideable = isCollideable;
}


void MoveItem::renderItem(const Point &pos)
{
   TNLAssert(false, "Unimplemented function!");
}


void MoveItem::onMountDestroyed()
{
   dismount();
}


// Runs on client & server, via different code paths
void MoveItem::onItemDropped()
{
   if(!getGame())    // Can happen on game startup
      return;

   GameType *gt = getGame()->getGameType();
   if(!gt || !mMount.isValid())
      return;

   if(!isGhost())    // Server only; on client calls onItemDropped from dismount
   {
      gt->itemDropped(mMount, this);
      dismount();
   }

   mDroppedTimer.reset(DROP_DELAY);
}


// Client & server, called via different paths
void MoveItem::dismount()
{
   if(mMount.isValid())      // Mount could be null if mount is out of scope, but is dropping an always-in-scope item
   {
      for(S32 i = 0; i < mMount->mMountedItems.size(); i++)
         if(mMount->mMountedItems[i].getPointer() == this)
         {
            mMount->mMountedItems.erase(i);     // Remove mounted item from our mount's list of mounted things
            break;
         }
   }

   if(isGhost())     // Client only; on server, we came from onItemDropped()
      onItemDropped();

   mMount = NULL;
   mIsMounted = false;
   setMaskBits(MountMask | PositionMask);    // Sending position fixes the super annoying "flag that can't be picked up" bug
}


 // if wanting to use setActualPos(const Point &p), will have to change all class that have setActualPos, to allow virtual inheritance to work right.
void MoveItem::setActualPos(const Point &pos)
{
   mMoveState[ActualState].pos = pos;
   setMaskBits(WarpPositionMask | PositionMask);
}


void MoveItem::setActualVel(const Point &vel)
{
   mMoveState[ActualState].vel = vel;
   setMaskBits(PositionMask);
}


Ship *MoveItem::getMount()
{
   return mMount;
}


void MoveItem::idle(BfObject::IdleCallPath path)
{
   if(!isInDatabase())
      return;

   Parent::idle(path);

   if(mIsMounted)    // Item is mounted on something else
   {
      if(mMount.isNull() || mMount->hasExploded)
      {
         if(!isGhost())    // Server only
            dismount();
      }
      else
      {
         mMoveState[RenderState].pos = mMount->getRenderPos();
         mMoveState[ActualState].pos = mMount->getActualPos();
      }
   }
   else              // Not mounted
   {
      float time = mCurrentMove.time * 0.001f;
      move(time, ActualState, false);
      if(path == BfObject::ServerIdleMainLoop)
      {
         // Only update if it's actually moving...
         if(mMoveState[ActualState].vel.lenSquared() != 0)
         {
            // Update less often on slow moving item, more often on fast moving item, and update when we change velocity.
            // Update at most every 5 seconds.
            updateTimer -= (mMoveState[ActualState].vel.len() + 20) * time;
            if(updateTimer < 0 || mMoveState[ActualState].vel.distSquared(prevMoveVelocity) > 100)
            {
               setMaskBits(PositionMask);
               updateTimer = 100;
               prevMoveVelocity = mMoveState[ActualState].vel;
            }
         }
         else if(prevMoveVelocity.lenSquared() != 0)
         {
            setMaskBits(PositionMask);  // update to client that this item is no longer moving.
            prevMoveVelocity.set(0,0);
         }

         mMoveState[RenderState] = mMoveState[ActualState];

      }
      else
         updateInterpolation();
   }
   updateExtentInDatabase();

   // Server only...
   U32 deltaT = mCurrentMove.time;
   mDroppedTimer.update(deltaT);
}


static const S32 VEL_POINT_SEND_BITS = 511;     // 511 = 2^9 - 1, the biggest int we can pack into 9 bits.

U32 MoveItem::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 retMask = 0;
   if(stream->writeFlag(updateMask & InitialMask))
   {
      // Send id in inital packet
      stream->writeRangedU32(getItemId(), 0, U16_MAX);
   }
   if(stream->writeFlag(updateMask & PositionMask))
   {
      ((GameConnection *) connection)->writeCompressedPoint(mMoveState[ActualState].pos, stream);
      writeCompressedVelocity(mMoveState[ActualState].vel, VEL_POINT_SEND_BITS, stream);      
      stream->writeFlag(updateMask & WarpPositionMask);
   }
   if(stream->writeFlag(updateMask & MountMask) && stream->writeFlag(mIsMounted))      // mIsMounted gets written iff MountMask is set  
   {
      S32 index = connection->getGhostIndex(mMount);     // Index of ship with item mounted

      if(stream->writeFlag(index != -1))                 // True if some ship has item, false if nothing is mounted
         stream->writeInt(index, GhostConnection::GhostIdBitSize);
      else
         retMask |= MountMask;
   }
   return retMask;
}


void MoveItem::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   bool interpolate = false;
   bool positionChanged = false;

   mInitial = stream->readFlag();

   if(mInitial)     // InitialMask
      setItemId(stream->readRangedU32(0, U16_MAX));

   if(stream->readFlag())     // PositionMask
   {
      ((GameConnection *) connection)->readCompressedPoint(mMoveState[ActualState].pos, stream);
      readCompressedVelocity(mMoveState[ActualState].vel, VEL_POINT_SEND_BITS, stream);   
      positionChanged = true;
      interpolate = !stream->readFlag();
   }

   if(stream->readFlag())     // MountMask
   {
      bool isMounted = stream->readFlag();
      if(isMounted)
      {
         Ship *theShip = NULL;
         
         if(stream->readFlag())
            theShip = dynamic_cast<Ship *>(connection->resolveGhost(stream->readInt(GhostConnection::GhostIdBitSize)));

         mountToShip(theShip);
      }
      else
         dismount();
   }

   if(positionChanged)
   {
      if(interpolate)
      {
         mInterpolating = true;
         move(connection->getOneWayTime() * 0.001f, ActualState, false);
      }
      else
      {
         mInterpolating = false;
         mMoveState[RenderState] = mMoveState[ActualState];
      }
   }
}


bool MoveItem::collide(BfObject *otherObject)
{
   return mIsCollideable && !mIsMounted;
}


S32 MoveItem::isOnShip(lua_State *L)
{
   return returnBool(L, mIsMounted);
}


S32 MoveItem::getShip(lua_State *L) 
{ 
   if(mMount.isValid()) 
   {
      mMount->push(L); 
      return 1;
   } 
   else 
      return returnNil(L); 
}


// Standard methods available to all MoveItems
const luaL_reg MoveItem::luaMethods[] =
{
   { "isOnShip",        luaW_doMethod<MoveItem, &MoveItem::isOnShip> },
   { "getShip",         luaW_doMethod<MoveItem, &MoveItem::getShip>  },
   { NULL, NULL }
};

const char *MoveItem::luaClassName = "MoveItem";

REGISTER_LUA_SUBCLASS(MoveItem, Item);


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(Asteroid);
class LuaAsteroid;

static F32 asteroidVel = 250;

static const S32 ASTEROID_INITIAL_SIZELEFT = 3;

static const F32 ASTEROID_MASS_LAST_SIZE = 1;
static const F32 ASTEROID_RADIUS_MULTIPLYER_LAST_SIZE = 89 * 0.2f;

F32 Asteroid::getAsteroidRadius(S32 size_left)
{
   return ASTEROID_RADIUS_MULTIPLYER_LAST_SIZE / 2 * F32(1 << size_left);  // doubles for each size left
}

F32 Asteroid::getAsteroidMass(S32 size_left)
{
   return ASTEROID_MASS_LAST_SIZE / 2 * F32(1 << size_left);  // doubles for each size left
}

// Constructor
Asteroid::Asteroid() : Parent(Point(0,0), true, getAsteroidRadius(ASTEROID_INITIAL_SIZELEFT), getAsteroidMass(ASTEROID_INITIAL_SIZELEFT))
{
   mSizeLeft = ASTEROID_INITIAL_SIZELEFT;  // higher = bigger

   mNetFlags.set(Ghostable);
   mObjectTypeNumber = AsteroidTypeNumber;
   hasExploded = false;
   mDesign = TNL::Random::readI(0, ASTEROID_DESIGNS - 1);

   // Give the asteroids some intial motion in a random direction
   F32 ang = TNL::Random::readF() * Float2Pi;
   F32 vel = asteroidVel;

   for(U32 i = 0; i < MoveStateCount; i++)
   {
      mMoveState[i].vel.x = vel * cos(ang);
      mMoveState[i].vel.y = vel * sin(ang);
   }

   mKillString = "crashed into an asteroid";

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
Asteroid::~Asteroid()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


Asteroid *Asteroid::clone() const
{
   return new Asteroid(*this);
}


U32 Asteroid::getDesignCount()
{
   return ASTEROID_DESIGNS;
}


void Asteroid::renderItem(const Point &pos)
{
   if(!hasExploded)
      renderAsteroid(pos, mDesign, mRadius / 89.f);
}


void Asteroid::renderDock()
{
   renderAsteroid(getActualPos(), 2, .1f);
}


const char *Asteroid::getOnScreenName()     { return "Asteroid";  }
const char *Asteroid::getPrettyNamePlural() { return "Asteroids"; }
const char *Asteroid::getOnDockName()       { return "Ast.";      }
const char *Asteroid::getEditorHelpString() { return "Shootable asteroid object.  Just like the arcade game."; }


F32 Asteroid::getEditorRadius(F32 currentScale)
{
   return mRadius * currentScale;
}


bool Asteroid::getCollisionPoly(Vector<Point> &polyPoints) const
{
   //for(S32 i = 0; i < ASTEROID_POINTS; i++)
   //{
   //   Point p = Point(mMoveState[MoveObject::ActualState].pos.x + (F32) AsteroidCoords[mDesign][i][0] * asteroidRenderSize[mSizeIndex],
   //                   mMoveState[MoveObject::ActualState].pos.y + (F32) AsteroidCoords[mDesign][i][1] * asteroidRenderSize[mSizeIndex] );

   //   polyPoints.push_back(p);
   //}

   return false;  // No Collision Poly, that may help reduce lag with client and server
}


#define ABS(x) (((x) > 0) ? (x) : -(x))


void Asteroid::damageObject(DamageInfo *theInfo)
{
   if(hasExploded)   // Avoid index out of range error
      return; 

   // Compute impulse direction
   mSizeLeft--;
   
   if(mSizeLeft <= 0)    // Kill small items
   {
      hasExploded = true;
      deleteObject(500);
      setMaskBits(ExplodedMask);    // Fix asteroids delay destroy after hit again...
      return;
   }

   setMaskBits(ItemChangedMask);    // So our clients will get new size
   setRadius(getAsteroidRadius(mSizeLeft));
   setMass(getAsteroidMass(mSizeLeft));

   F32 ang = TNL::Random::readF() * Float2Pi;      // Sync
   //F32 vel = asteroidVel;

   setPosAng(getActualPos(), ang);

   Asteroid *newItem = new Asteroid();
   newItem->mSizeLeft = mSizeLeft;
   newItem->setRadius(getAsteroidRadius(mSizeLeft));
   newItem->setMass(getAsteroidMass(mSizeLeft));

   F32 ang2;
   do
      ang2 = TNL::Random::readF() * Float2Pi;      // Sync
   while(ABS(ang2 - ang) < .0436 );    // That's 20 degrees in radians, folks!

   newItem->setPosAng(getActualPos(), ang2);

   newItem->addToGame(getGame(), getGame()->getGameObjDatabase());    // And add it to the list of game objects
}


void Asteroid::setPosAng(Point pos, F32 ang)
{
   for(U32 i = 0; i < MoveStateCount; i++)
   {
      mMoveState[i].pos = pos;
      mMoveState[i].angle = ang;
      mMoveState[i].vel.x = asteroidVel * cos(ang);
      mMoveState[i].vel.y = asteroidVel * sin(ang);
   }
}


static U8 ASTEROID_SIZELEFT_BIT_COUNT = 3;
static S32 ASTEROID_SIZELEFT_MAX = 5;   // For editor attribute. real limit based on bit count is (1 << ASTEROID_SIZELEFT_BIT_COUNT) - 1; // = 7

U32 Asteroid::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 retMask = Parent::packUpdate(connection, updateMask, stream);

   if(stream->writeFlag(updateMask & ItemChangedMask))
   {
      stream->writeInt(mSizeLeft, ASTEROID_SIZELEFT_BIT_COUNT);
      stream->writeEnum(mDesign, ASTEROID_DESIGNS);
   }

   stream->writeFlag(hasExploded);

   return retMask;
}


void Asteroid::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   Parent::unpackUpdate(connection, stream);

   if(stream->readFlag())
   {
      mSizeLeft = stream->readInt(ASTEROID_SIZELEFT_BIT_COUNT);
      setRadius(getAsteroidRadius(mSizeLeft));
      setMass(getAsteroidMass(mSizeLeft));
      mDesign = stream->readEnum(ASTEROID_DESIGNS);

      if(!mInitial)
         SoundSystem::playSoundEffect(SFXAsteroidExplode, mMoveState[RenderState].pos);
   }

   bool explode = (stream->readFlag());     // Exploding!  Take cover!!

   if(explode && !hasExploded)
   {
      hasExploded = true;
      disableCollision();
      onItemExploded(mMoveState[RenderState].pos);
   }
}


bool Asteroid::collide(BfObject *otherObject)
{
   if(hasExploded)
      return false;

   if(isGhost())   //client only, to try to prevent asteroids desync...
   {
      Ship *ship = dynamic_cast<Ship *>(otherObject);
      if(ship)
      {
         // Client does not know if we actually get destroyed from asteroids
         // prevents bouncing off asteroids, then LAG puts back to position.
         if(! ship->isModulePrimaryActive(ModuleShield)) return false;
      }
   }

   // Asteroids don't collide with one another!
   return dynamic_cast<Asteroid *>(otherObject) ? false : true;
}


TestFunc Asteroid::collideTypes()
{
   return (TestFunc) isAsteroidCollideableType;
}


// Client only
void Asteroid::onItemExploded(Point pos)
{
   SoundSystem::playSoundEffect(SFXAsteroidExplode, pos);
   // FXManager::emitBurst(pos, Point(.1, .1), Colors::white, Colors::white, 10);
}


bool Asteroid::processArguments(S32 argc2, const char **argv2, Game *game)
{
   S32 argc = 0;
   const char *argv[8];                // 8 is ok for now..

   for(S32 i = 0; i < argc2; i++)      // The idea here is to allow optional R3.5 for rotate at speed of 3.5
   {
      char firstChar = argv2[i][0];    // First character of arg

      if((firstChar >= 'a' && firstChar <= 'z') || (firstChar >= 'A' && firstChar <= 'Z'))
      {
         if(!strnicmp(argv2[i], "Size=", 5))
            mSizeLeft = atoi(&argv2[i][5]);
      }
      else
      {
         if(argc < 8)
         {  
            argv[argc] = argv2[i];
            argc++;
         }
      }
   }
   setRadius(getAsteroidRadius(mSizeLeft));
   setMass(getAsteroidMass(mSizeLeft));

   return Parent::processArguments(argc, argv, game);
}

string Asteroid::toString(F32 gridSize) const
{
   if(mSizeLeft != ASTEROID_INITIAL_SIZELEFT)
      return Parent::toString(gridSize) + " Size=" + itos(mSizeLeft);
   else
      return Parent::toString(gridSize);
}


#ifndef ZAP_DEDICATED

EditorAttributeMenuUI *Asteroid::mAttributeMenuUI = NULL;

EditorAttributeMenuUI *Asteroid::getAttributeMenu()
{
   // Lazily initialize this -- if we're in the game, we'll never need this to be instantiated
   if(!mAttributeMenuUI)
   {
      ClientGame *clientGame = static_cast<ClientGame *>(getGame());

      mAttributeMenuUI = new EditorAttributeMenuUI(clientGame);

      mAttributeMenuUI->addMenuItem(new CounterMenuItem("Size:", mSizeLeft, 1, 1, ASTEROID_SIZELEFT_MAX, "", "", ""));

      // Add our standard save and exit option to the menu
      mAttributeMenuUI->addSaveAndQuitMenuItem();
   }

   return mAttributeMenuUI;
}


// Get the menu looking like what we want
void Asteroid::startEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   attributeMenu->getMenuItem(0)->setIntValue(mSizeLeft);
}


// Retrieve the values we need from the menu
void Asteroid::doneEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   mSizeLeft = attributeMenu->getMenuItem(0)->getIntValue();
   setRadius(getAsteroidRadius(mSizeLeft));
   setMass(getAsteroidMass(mSizeLeft));
}


// Render some attributes when item is selected but not being edited
string Asteroid::getAttributeString()
{
   return "Size: " + itos(mSizeLeft);      
}

#endif


///// Lua interface
S32 Asteroid::getSize(lua_State *L)
{
   return returnInt(L, ASTEROID_INITIAL_SIZELEFT - mSizeLeft);
}


S32 Asteroid::getSizeCount(lua_State *L)
{
   return returnInt(L, ASTEROID_INITIAL_SIZELEFT);
}


// Define the methods we will expose to Lua
const luaL_reg Asteroid::luaMethods[] =
{
   // Class specific methods
   { "getSize",       luaW_doMethod<Asteroid, &Asteroid::getSize> },
   { "getSizeCount",  luaW_doMethod<Asteroid, &Asteroid::getSizeCount> },    // <=== could be static

   {0,0}    // End method list
};

const char *Asteroid::luaClassName = "Asteroid";

REGISTER_LUA_SUBCLASS(Asteroid, MoveItem);

////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(Circle);
class LuaCircle;

static F32 CIRCLE_VEL = 250;

static const F32 CIRCLE_MASS = 4;

// Constructor
Circle::Circle() : Parent(Point(0,0), true, (F32)CIRCLE_RADIUS, CIRCLE_MASS)
{
   mNetFlags.set(Ghostable);
   mObjectTypeNumber = CircleTypeNumber;
   hasExploded = false;

   // Give the asteroids some intial motion in a random direction
   F32 ang = TNL::Random::readF() * Float2Pi;
   F32 vel = CIRCLE_VEL;

   for(U32 i = 0; i < MoveStateCount; i++)
   {
      mMoveState[i].vel.x = vel * cos(ang);
      mMoveState[i].vel.y = vel * sin(ang);
   }

   mKillString = "crashed into an circle";
}


// Destructor
Circle::~Circle()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


Circle *Circle::clone() const
{
   return new Circle(*this);
}


void Circle::idle(BfObject::IdleCallPath path)
{
   //if(path == BfObject::ServerIdleMainLoop)
   {
      // Find nearest ship
      fillVector.clear();
      findObjects((TestFunc)isShipType, fillVector, Rect(getActualPos(), 1200));

      F32 dist = F32_MAX;
      Ship *closest = NULL;

      for(S32 i = 0; i < fillVector.size(); i++)
      {
         Ship *ship = dynamic_cast<Ship *>(fillVector[i]);
         F32 d = getActualPos().distSquared(ship->getActualPos());
         if(d < dist)
         {
            closest = ship;
            dist = d;
         }
      }

      if(!closest)
         return;

      Point v = getActualVel();
      v += closest->getActualPos() - getActualPos();

      v.normalize(CIRCLE_VEL);

      setActualVel(v);
   }

   Parent::idle(path);
}


void Circle::renderItem(const Point &pos)
{
#ifndef ZAP_DEDICATED
   if(!hasExploded)
   {
      glColor(Colors::red);
      drawCircle(pos, (F32)CIRCLE_RADIUS);
   }
#endif
}


void Circle::renderDock()
{
   drawCircle(getActualPos(), 2);
}


const char *Circle::getOnScreenName()     { return "Circle";  }
const char *Circle::getPrettyNamePlural() { return "Circles"; }
const char *Circle::getOnDockName()       { return "Circ.";   }
const char *Circle::getEditorHelpString() { return "Shootable circle object.  Scary."; }


F32 Circle::getEditorRadius(F32 currentScale)
{
   return CIRCLE_RADIUS * currentScale;
}


bool Circle::getCollisionPoly(Vector<Point> &polyPoints) const
{
   return false;
}


void Circle::damageObject(DamageInfo *theInfo)
{
   // Compute impulse direction
   hasExploded = true;
   deleteObject(500);
   setMaskBits(ExplodedMask);    // Fix asteroids delay destroy after hit again...
   return;
}


void Circle::setPosAng(Point pos, F32 ang)
{
   for(U32 i = 0; i < MoveStateCount; i++)
   {
      mMoveState[i].pos = pos;
      mMoveState[i].angle = ang;
      mMoveState[i].vel.x = asteroidVel * cos(ang);
      mMoveState[i].vel.y = asteroidVel * sin(ang);
   }
}


U32 Circle::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 retMask = Parent::packUpdate(connection, updateMask, stream);

   stream->writeFlag(hasExploded);

   return retMask;
}


void Circle::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   Parent::unpackUpdate(connection, stream);

   bool explode = (stream->readFlag());     // Exploding!  Take cover!!

   if(explode && !hasExploded)
   {
      hasExploded = true;
      disableCollision();
      onItemExploded(mMoveState[RenderState].pos);
   }
}


bool Circle::collide(BfObject *otherObject)
{
   return true;
}


// Client only
void Circle::onItemExploded(Point pos)
{
   SoundSystem::playSoundEffect(SFXAsteroidExplode, pos);
}


void Circle::playCollisionSound(U32 stateIndex, MoveObject *moveObjectThatWasHit, F32 velocity)
{
   // Do nothing
}


U32 Circle::getDesignCount()
{
   return ASTEROID_DESIGNS;
}


// Inherits all MoveItem methods, has no custom methods
const luaL_reg Circle::luaMethods[] =
{
   {NULL, NULL}   
};

const char *Circle::luaClassName = "Circle";

REGISTER_LUA_SUBCLASS(Circle, MoveItem);


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(Worm);

Worm::Worm()
{
   mNetFlags.set(Ghostable);
   mObjectTypeNumber = WormTypeNumber;
   hasExploded = false;

   mDirTimer.reset(100);

   mKillString = "killed by a worm";

   mHeadIndex = 0;
   mTailLength = 0;
}


bool Worm::processArguments(S32 argc, const char **argv, Game *game)
{
   if(argc < 2)
      return false;

   Point pos;
   pos.read(argv);
   pos *= game->getGridSize();

   setPos(pos);

   mHeadIndex = 0;
   mTailLength = 0;

   setExtent(Rect(pos, 1));

   return true;
}


const char *Worm::getOnScreenName()
{
   return "Worm";
}

string Worm::toString(F32 gridSize) const
{
   return string(getClassName()) + " " + geomToString(gridSize);
}

void Worm::render()
{
#ifndef ZAP_DEDICATED
   if(!hasExploded)
   {
      if(mTailLength <= 1)
      {
         renderWorm(mPoints[mHeadIndex]);
         return;
      }
      Vector<Point> p;
      S32 i = mHeadIndex;
      for(S32 count = 0; count <= mTailLength; count++)
      {
         p.push_back(mPoints[i]);
         i--;
         if(i < 0)
            i = maxTailLength - 1;
      }
      glColor(Colors::white);
      renderLine(&p);
   }
#endif
}


Worm *Worm::clone() const
{
   return new Worm(*this);
}


void Worm::renderEditor(F32 currentScale, bool snappingToWallCornersEnabled)
{
   render();
}


void Worm::renderDock()
{
   render();
}


F32 Worm::getEditorRadius(F32 currentScale)
{
   return 10;     // Or something... who knows??
}


bool Worm::getCollisionCircle(U32 state, Point &center, F32 &radius) const
{
   //center = mMoveState[state].pos;
   //radius = F32(WORM_RADIUS);
   return false;
}


bool Worm::getCollisionPoly(Vector<Point> &polyPoints) const
{
   S32 i = mHeadIndex;
   for(S32 count = 0; count < mTailLength; count++)
   {
      polyPoints.push_back(mPoints[i]);
      i--;
      if(i < 0)
         i = maxTailLength - 1;
   }
   for(S32 count = 0; count < mTailLength; count++)
   {
      polyPoints.push_back(mPoints[i]);
      i++;
      if(i >= maxTailLength)
         i = 0;
   }
   return polyPoints.size() != 0;  // false with zero points
}

bool Worm::collide(BfObject *otherObject)
{
   // Worms don't collide with one another!
   return /*dynamic_cast<Worm *>(otherObject) ? false : */true;
}


void Worm::setPosAng(Point pos, F32 ang)
{
   if(mTailLength < maxTailLength - 1)
      mTailLength++;

   mHeadIndex++;
   if(mHeadIndex >= maxTailLength)
      mHeadIndex = 0;

   mPoints[mHeadIndex] = pos;
   setMaskBits(TailPointParts << mHeadIndex);

   Vector<Point> p;
   getCollisionPoly(p);
   setExtent(p);
}


void Worm::setNextAng(F32 nextAng)
{
   //mNextAng = nextAng;
}


void Worm::damageObject(DamageInfo *theInfo)
{
   hasExploded = true;
   deleteObject(500);
}


void Worm::idle(BfObject::IdleCallPath path)
{
   if(!isInDatabase())
      return;

   if(path == ServerIdleMainLoop && mDirTimer.update(mCurrentMove.time))
   {
      Point p;
      mAngle = (mAngle + (TNL::Random::readF() - 0.5f) * 173); // * FloatPi * 4.f;
      if(mAngle < -FloatPi * 4.f)
         mAngle += FloatPi * 8.f;
      if(mAngle > FloatPi * 4.f)
         mAngle -= FloatPi * 8.f;

      p.setPolar(20, mAngle);
      setPosAng(mPoints[mHeadIndex] + p, mAngle);      // TODO:  don't go through walls
      mDirTimer.reset(200);
   }

   Parent::idle(path);
}


U32 Worm::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 retMask = Parent::packUpdate(connection, updateMask, stream);

   for(S32 i = 0; i < maxTailLength; i++)
   {
      if(stream->writeFlag(TailPointParts << i))
      {
         mPoints[i].write(stream);
      }
   }

   stream->writeInt(mHeadIndex, 5);
   stream->writeInt(mTailLength, 5);

   stream->writeFlag(hasExploded);

   return retMask;
}


void Worm::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   Parent::unpackUpdate(connection, stream);

   for(S32 i=0; i < maxTailLength; i++)
   {
      if(stream->readFlag())
      {
         mPoints[i].read(stream);
      }
   }

   mHeadIndex = stream->readInt(5);
   mTailLength = stream->readInt(5);

   Vector<Point> p;
   getCollisionPoly(p);
   setExtent(p);


   bool explode = (stream->readFlag());     // Exploding!  Take cover!!

   if(explode && !hasExploded)
   {
      hasExploded = true;
      disableCollision();
      //onItemExploded(mMoveState[RenderState].pos);
   }
}


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(TestItem);

static const F32 TEST_ITEM_MASS = 4;

// Constructor
TestItem::TestItem() : Parent(Point(0,0), true, (F32)TEST_ITEM_RADIUS, TEST_ITEM_MASS)
{
   mNetFlags.set(Ghostable);
   mObjectTypeNumber = TestItemTypeNumber;

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
TestItem::~TestItem() 
{
   LUAW_DESTRUCTOR_CLEANUP;
}


TestItem *TestItem::clone() const
{
   return new TestItem(*this);
}


void TestItem::idle(BfObject::IdleCallPath path)
{
   //if(path == ServerIdleMainLoop && (abs(getPos().x) > 1000 || abs(getPos().y > 1000)))
   //   deleteObject(100);

   Parent::idle(path);
}


void TestItem::renderItem(const Point &pos)
{
   renderTestItem(pos);
}


void TestItem::renderDock()
{
   renderTestItem(getActualPos(), 8);
}


const char *TestItem::getOnScreenName()      {  return "TestItem";   }
const char *TestItem::getPrettyNamePlural()  {  return "TestItems";  }
const char *TestItem::getOnDockName()        {  return "Test";       }
const char *TestItem::getEditorHelpString()  {  return "Bouncy object that floats around and gets in the way."; }


F32 TestItem::getEditorRadius(F32 currentScale)
{
   return getRadius() * currentScale;
}


// Appears to be server only??
void TestItem::damageObject(DamageInfo *theInfo)
{
   computeImpulseDirection(theInfo);
}


bool TestItem::getCollisionPoly(Vector<Point> &polyPoints) const
{
   //for(S32 i = 0; i < 8; i++)    // 8 so that first point gets repeated!  Needed?  Maybe not
   //{
   //   Point p = Point(60 * cos(i * Float2Pi / 7 + FloatHalfPi) + mMoveState[ActualState].pos.x, 60 * sin(i * Float2Pi / 7 + FloatHalfPi) + mMoveState[ActualState].pos.y);
   //   polyPoints.push_back(p);
   //}

   // Override parent so getCollisionCircle is used instead
   return false;
}


// Inherits all MoveItem methods, has no custom methods
const luaL_reg TestItem::luaMethods[] =
{
   { NULL, NULL }
};

const char *TestItem::luaClassName = "TestItem";

REGISTER_LUA_SUBCLASS(TestItem, MoveItem);


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(ResourceItem);

static const F32 RESOURCE_ITEM_MASS = 1;

   // Constructor
ResourceItem::ResourceItem() : Parent(Point(0,0), true, (F32)RESOURCE_ITEM_RADIUS, RESOURCE_ITEM_MASS)
{
   mNetFlags.set(Ghostable);
   mObjectTypeNumber = ResourceItemTypeNumber;

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
ResourceItem::~ResourceItem() 
{
   LUAW_DESTRUCTOR_CLEANUP;
}


ResourceItem *ResourceItem::clone() const
{
   return new ResourceItem(*this);
}


void ResourceItem::renderItem(const Point &pos)
{
   renderResourceItem(pos);
}


void ResourceItem::renderDock()
{
   renderResourceItem(getActualPos(), .4f, 0, 1);
}


const char *ResourceItem::getOnScreenName()     { return "ResourceItem"; }
const char *ResourceItem::getPrettyNamePlural() { return "Resource Items"; }
const char *ResourceItem::getOnDockName()       { return "Res."; }
const char *ResourceItem::getEditorHelpString() { return "Small bouncy object; capture one to activate Engineer module"; }


bool ResourceItem::collide(BfObject *hitObject)
{
   if(mIsMounted)
      return false;

   if( ! (isShipType(hitObject->getObjectTypeNumber())) )
      return true;

   // Ignore collisions that occur to recently dropped items.  Make sure item is ready to be picked up! 
   if(mDroppedTimer.getCurrent())    
      return false;

   Ship *ship = dynamic_cast<Ship *>(hitObject);
   if(!ship || ship->hasExploded)
      return false;

   if(ship->hasModule(ModuleEngineer) && !ship->isCarryingItem(ResourceItemTypeNumber))
   {
      if(!isGhost())
         mountToShip(ship);
      return false;
   }
   return true;
}


void ResourceItem::damageObject(DamageInfo *theInfo)
{
   computeImpulseDirection(theInfo);
}


void ResourceItem::onItemDropped()
{
   if(mMount.isValid() && !isGhost())   //Server only, to prevent desync
   {
      this->setActualPos(mMount->getActualPos()); 
      this->setActualVel(mMount->getActualVel() * 1.5);
   }   
   
   Parent::onItemDropped();
}


///// Lua Interface

// Inherits all MoveItem methods, and has a few of its own
const luaL_reg ResourceItem::luaMethods[] =
{
   { NULL, NULL }
};

const char *ResourceItem::luaClassName = "ResourceItem";

REGISTER_LUA_SUBCLASS(ResourceItem, MoveItem);

};

