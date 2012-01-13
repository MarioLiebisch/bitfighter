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

#include "projectile.h"
#include "gameWeapons.h"
#include "ship.h"
#include "SoundSystem.h"
#include "gameObject.h"
#include "gameObjectRender.h"
#include "game.h"
#include "gameConnection.h"
#include "stringUtils.h"

#ifndef ZAP_DEDICATED
#include "ClientGame.h"
#include "sparkManager.h"
#include "SDL/SDL_opengl.h"
#include "UI.h"
#endif


namespace Zap
{

const char LuaProjectile::className[] = "ProjectileItem";      // Class name as it appears to Lua scripts

// Define the methods we will expose to Lua
Lunar<LuaProjectile>::RegType LuaProjectile::methods[] =
{
   // Standard gameItem methods
   method(LuaProjectile, getClassID),
   method(LuaProjectile, getLoc),
   method(LuaProjectile, getRad),
   method(LuaProjectile, getVel),
   method(LuaProjectile, getTeamIndx),

   method(LuaProjectile, getWeapon),

   {0,0}    // End method list
};


//  C++ constructor
LuaProjectile::LuaProjectile()
{
   // Do not use
}

//  Lua constructor
LuaProjectile::LuaProjectile(lua_State *L)
{
   // Do not use
}


const char *LuaProjectile::getClassName() const
{
   return "LuaProjectile";
}


S32 LuaProjectile::getWeapon(lua_State *L)
{
   TNLAssert(false, "Unimplemented method!");
   return 0;
}

//============================
// LuaItem interface

S32 LuaProjectile::getClassID(lua_State *L)
{
   return returnInt(L, BulletTypeNumber);
}


S32 LuaProjectile::getLoc(lua_State *L)
{
   TNLAssert(false, "Unimplemented method!");
   return 0;
}


S32 LuaProjectile::getRad(lua_State *L)
{
   TNLAssert(false, "Unimplemented method!");
   return 0;
}


S32 LuaProjectile::getVel(lua_State *L)
{
   TNLAssert(false, "Unimplemented method!");
   return 0;
}


S32 LuaProjectile::getTeamIndx(lua_State *L)
{
   TNLAssert(false, "Unimplemented method!");
   return 0;
}


GameObject *LuaProjectile::getGameObject()
{
   TNLAssert(false, "Unimplemented method!");
   return NULL;
}


void LuaProjectile::push(lua_State *L)
{
   TNLAssert(false, "Unimplemented method!");
}


/////////////////////////////////////
/////////////////////////////////////


//===========================================

TNL_IMPLEMENT_NETOBJECT(Projectile);

// Constructor
Projectile::Projectile(WeaponType type, Point pos, Point vel, GameObject *shooter)
{
   mObjectTypeNumber = BulletTypeNumber;

   mNetFlags.set(Ghostable);
   mPos = pos;
   mVelocity = vel;

   mTimeRemaining = GameWeapon::weaponInfo[type].projLiveTime;
   collided = false;
   hitShip = false;
   alive = true;
   hasBounced = false;
   mShooter = shooter;

   // Copy some attributes from the shooter
   if(shooter)
   {
      setOwner(shooter->getOwner());
      mTeam = shooter->getTeam();
      mKillString = shooter->getKillString();
   }
   else
      setOwner(NULL);

   mType = GameWeapon::weaponInfo[type].projectileType;
   mWeaponType = type;
}


U32 Projectile::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   if(stream->writeFlag(updateMask & PositionMask))
   {
      ((GameConnection *) connection)->writeCompressedPoint(mPos, stream);
      writeCompressedVelocity(mVelocity, COMPRESSED_VELOCITY_MAX, stream);
   }

   if(stream->writeFlag(updateMask & InitialMask))
   {
      stream->writeEnum(mType, ProjectileTypeCount);

      S32 index = -1;
      if(mShooter.isValid())
         index = connection->getGhostIndex(mShooter);
      if(stream->writeFlag(index != -1))
         stream->writeInt(index, GhostConnection::GhostIdBitSize);
   }

   stream->writeFlag(collided);
   if(collided)
      stream->writeFlag(hitShip);
   stream->writeFlag(alive);

   return 0;
}


void Projectile::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   bool initial = false;
   if(stream->readFlag())  // Read position, for correcting bouncers, needs to be before inital for SoundSystem::playSoundEffect
   {
      ((GameConnection *) connection)->readCompressedPoint(mPos, stream);
      readCompressedVelocity(mVelocity, COMPRESSED_VELOCITY_MAX, stream);
   }

   if(stream->readFlag())         // Initial chunk of data, sent once for this object
   {

      mType = (ProjectileType) stream->readEnum(ProjectileTypeCount);

      TNLAssert(connection, "Defunct connection to server in projectile.cpp!");

      if(stream->readFlag())
         mShooter = dynamic_cast<Ship *>(connection->resolveGhost(stream->readInt(GhostConnection::GhostIdBitSize)));

      Rect newExtent(mPos, mPos);
      setExtent(newExtent);
      initial = true;
      SoundSystem::playSoundEffect(GameWeapon::projectileInfo[mType].projectileSound, mPos, mVelocity);
   }
   bool preCollided = collided;
   collided = stream->readFlag();
   
   if(collided)
      hitShip = stream->readFlag();

   alive = stream->readFlag();

   if(!preCollided && collided)     // Projectile has "become" collided
      explode(NULL, mPos);

   if(!collided && initial)
   {
      mCurrentMove.time = U32(connection->getOneWayTime());
     //idle(GameObject::ClientIdleMainRemote);   // not CE
   }
}


void Projectile::handleCollision(GameObject *hitObject, Point collisionPoint)
{
   collided = true;
   Ship *s = dynamic_cast<Ship *>(hitObject);
   hitShip = (s != NULL);

   if(!isGhost())    // If we're on the server, that is
   {
      DamageInfo theInfo;

      theInfo.collisionPoint = collisionPoint;
      theInfo.damageAmount = GameWeapon::weaponInfo[mWeaponType].damageAmount;
      theInfo.damageType = DamageTypePoint;
      theInfo.damagingObject = this;
      theInfo.impulseVector = mVelocity;
      theInfo.damageSelfMultiplier = GameWeapon::weaponInfo[mWeaponType].damageSelfMultiplier;

      hitObject->damageObject(&theInfo);

      Ship *shooter = dynamic_cast<Ship *>(mShooter.getPointer());

      if(hitShip && shooter)
         shooter->getClientInfo()->getStatistics()->countHit(mWeaponType);
   }

   mTimeRemaining = 0;
   explode(hitObject, collisionPoint);
}


void Projectile::idle(GameObject::IdleCallPath path)
{
   U32 deltaT = mCurrentMove.time;

   if(!collided && alive)
   {
    U32 aliveTime = getGame()->getCurrentTime() - getCreationTime();  // Age of object, in ms
    F32 timeLeft = (F32)deltaT;
    S32 loopcount1 = 32;
    while(timeLeft > 0.01f && loopcount1 != 0)    // This loop is to prevent slow bounce on low frame rate / high time left.
    {
      loopcount1--;
      // Calculate where projectile will be at the end of the current interval
      Point endPos = mPos + mVelocity * timeLeft * 0.001f;

      // Check for collision along projected route of movement
      static Vector<GameObject *> disabledList;

      Rect queryRect(mPos, endPos);     // Bounding box of our travels

      disabledList.clear();


      // Don't collide with shooter during first 500ms of life
      if(mShooter.isValid() && aliveTime < 500 && !hasBounced)
      {
         disabledList.push_back(mShooter);
         mShooter->disableCollision();
      }

      GameObject *hitObject;

      F32 collisionTime;
      Point surfNormal;

      // Do the search
      while(true)  
      {
         hitObject = findObjectLOS((TestFunc)isWeaponCollideableType,
                                   MoveObject::RenderState, mPos, endPos, collisionTime, surfNormal);

         if((!hitObject || hitObject->collide(this)))
            break;

         // Disable collisions with things that don't want to be
         // collided with (i.e. whose collide methods return false)
         disabledList.push_back(hitObject);
         hitObject->disableCollision();
      }

      // Re-enable collison flag for ship and items in our path that don't want to be collided with
      // Note that if we hit an object that does want to be collided with, it won't be in disabledList
      // and thus collisions will not have been disabled, and thus don't need to be re-enabled.
      // Our collision detection is done, and hitObject contains the first thing that the projectile hit.

      for(S32 i = 0; i < disabledList.size(); i++)
         disabledList[i]->enableCollision();

      if(hitObject)  // Hit something...  should we bounce?
      {
         bool bounce = false;

         if(mType == ProjectileBounce && isWallType(hitObject->getObjectTypeNumber()))
            bounce = true;
         else if(isShipType(hitObject->getObjectTypeNumber()))
         {
            Ship *s = dynamic_cast<Ship *>(hitObject);
            if(s->isModulePrimaryActive(ModuleShield))
               bounce = true;
         }

         if(bounce)
         {
            hasBounced = true;
            // We hit something that we should bounce from, so bounce!
            F32 float1 = surfNormal.dot(mVelocity) * 2;
            mVelocity -= surfNormal * float1;
            if(float1 > 0)
               surfNormal = -surfNormal;      // This is to fix going through polygon barriers

            Point collisionPoint = mPos + (endPos - mPos) * collisionTime;
            mPos = collisionPoint + surfNormal;
            timeLeft = timeLeft * (1 - collisionTime);

            MoveObject *obj = dynamic_cast<MoveObject *>(hitObject);
            if(obj)
            {
               setMaskBits(PositionMask);  // Bouncing off a moving objects can easily get desync.
               float1 = mPos.distanceTo(obj->getRenderPos());
               if(float1 < obj->getRadius())
               {
                  float1 = obj->getRadius() * 1.01f / float1;
                  mPos = mPos * float1 + obj->getRenderPos() * (1 - float1);  // to fix bouncy stuck inside shielded ship
               }
            }

            if(isGhost())
               SoundSystem::playSoundEffect(SFXBounceShield, collisionPoint, surfNormal * surfNormal.dot(mVelocity) * 2);
         }
         else
         {
            // Not bouncing, so advance to location of collision
            Point collisionPoint = mPos + (endPos - mPos) * collisionTime;
            handleCollision(hitObject, collisionPoint);     // What we hit, where we hit it
            timeLeft = 0;
         }

      }
      else        // Hit nothing, advance projectile to endPos
      {
         timeLeft = 0;
         //// Steer towards a nearby testitem
         //static Vector<DatabaseObject *> targetItems;
         //targetItems.clear();
         //Rect searchArea(pos, 2000);
         //S32 HeatSeekerTargetType = TestItemType | ResourceItemType;
         //findObjects(HeatSeekerTargetType, targetItems, searchArea);

         //F32 maxPull = 0;
         //Point dist;
         //Point maxDist;

         //for(S32 i = 0; i < targetItems.size(); i++)
         //{
         //   GameObject *target = dynamic_cast<GameObject *>(targetItems[i]);
         //   dist.set(pos - target->getActualPos());

         //   F32 pull = min(100.0f / dist.len(), 1.0);      // Pull == strength of attraction
         //   
         //   pull *= (target->getObjectTypeMask() & ResourceItemType) ? .5 : 1;

         //   if(pull > maxPull)
         //   {
         //      maxPull = pull;
         //      maxDist.set(dist);
         //   }
         //   
         //}

         //if(maxPull > 0)
         //{
         //   F32 speed = velocity.len();
         //   velocity += (velocity - maxDist) * maxPull;
         //   velocity.normalize(speed);
         //   endPos.set(pos + velocity * (F32)deltaT * 0.001);  // Apply the adjusted velocity right now!
         //}

         mPos.set(endPos);     // Keep this line
      }

      Rect newExtent(mPos, mPos);
      setExtent(newExtent);
    }
   }


   // Kill old projectiles
   if(alive && path == GameObject::ServerIdleMainLoop)
   {
      if(mTimeRemaining <= deltaT)
      {
         deleteObject(500);
         mTimeRemaining = 0;
         alive = false;
         setMaskBits(ExplodedMask);
      }
      else
         mTimeRemaining -= deltaT;     // Decrement time left to live
   }
}


// Gets run when projectile suffers damage, like from a burst going off
void Projectile::damageObject(DamageInfo *info)
{
   mTimeRemaining = 0;     // This will kill projectile --> remove this to have projectiles unaffected
}


void Projectile::explode(GameObject *hitObject, Point pos)
{
#ifndef ZAP_DEDICATED
   // Do some particle spew...
   if(isGhost())
   {
      TNLAssert(dynamic_cast<ClientGame *>(getGame()) != NULL, "Not a ClientGame");
      static_cast<ClientGame *>(getGame())->emitExplosion(pos, 0.3f, GameWeapon::projectileInfo[mType].sparkColors, NumSparkColors);

      Ship *s = dynamic_cast<Ship *>(hitObject);

      SFXProfiles sound; 
      if(s && s->isModulePrimaryActive(ModuleShield))  // We hit a ship with shields up
         sound = SFXBounceShield;
      else if((hitShip || s))                          // We hit a ship with shields down
         sound = SFXShipHit;
      else                                             // We hit something else
         sound = GameWeapon::projectileInfo[mType].impactSound;

      SoundSystem::playSoundEffect(sound, pos, mVelocity);       // Play the sound
   }
#endif
}


Point Projectile::getRenderVel() const
{
   return mVelocity;
}


Point Projectile::getActualVel() const
{
   return mVelocity;
}


Point Projectile::getRenderPos() const
{
   return mPos;
}


Point Projectile::getActualPos() const
{
   return mPos;
}


// TODO: Get rid of this! (currently won't render without it)
void Projectile::render()
{
   renderItem(getActualPos());
}


void Projectile::renderItem(const Point &pos)
{
   if(collided || !alive)
      return;

   renderProjectile(pos, mType, getGame()->getCurrentTime() - getCreationTime());
}


//// Lua methods

S32 Projectile::getLoc(lua_State *L)
{
   return returnPoint(L, getActualPos());
}


S32 Projectile::getRad(lua_State *L)
{
   // TODO: Wrong!!  Radius of item (returns number)
   return returnInt(L, 10);
}


S32 Projectile::getVel(lua_State *L)
{
   return returnPoint(L, getActualVel());
}


S32 Projectile::getTeamIndx(lua_State *L)
{
   return returnInt(L, mShooter->getTeam());
}


GameObject *Projectile::getGameObject()
{
   return this;
}


S32 Projectile::getWeapon(lua_State *L)
{
   return returnInt(L, mWeaponType);
}


void Projectile::push(lua_State *L)
{
   Lunar<LuaProjectile>::push(L, this);
}


//-----------------------------------------------------------------------------
TNL_IMPLEMENT_NETOBJECT(GrenadeProjectile);

GrenadeProjectile::GrenadeProjectile(Point pos, Point vel, GameObject *shooter): MoveItem(pos, true, mRadius, mMass)
{
   mObjectTypeNumber = BulletTypeNumber;

   mNetFlags.set(Ghostable);

   mMoveState[ActualState].pos = pos;
   mMoveState[ActualState].vel = vel;
   setMaskBits(PositionMask);
   mWeaponType = WeaponBurst;

   updateExtentInDatabase();

   mTimeRemaining = GameWeapon::weaponInfo[WeaponBurst].projLiveTime;
   exploded = false;

   if(shooter)
   {
      setOwner(shooter->getOwner());
      mTeam = shooter->getTeam();
   }
   else
      setOwner(NULL);

   mRadius = 7;
   mMass = 1;
}


// Runs on client and server
void GrenadeProjectile::idle(IdleCallPath path)
{
   bool collisionDisabled = false;
   GameConnection *gc = NULL;

#ifndef ZAP_DEDICATED
   if(isGhost())   // Fix effect of ship getting ahead of burst on laggy client  
   {
      U32 aliveTime = getGame()->getCurrentTime() - getCreationTime();  // Age of object, in ms

      ClientGame *clientGame = dynamic_cast<ClientGame *>(getGame());
      gc = clientGame->getConnectionToServer();

      collisionDisabled = aliveTime < 250 && gc && gc->getControlObject();

      if(collisionDisabled) 
         gc->getControlObject()->disableCollision();
   }
#endif

   Parent::idle(path);

   if(collisionDisabled) 
      gc->getControlObject()->enableCollision();

   // Do some drag...  no, not that kind of drag!
   mMoveState[ActualState].vel -= mMoveState[ActualState].vel * (((F32)mCurrentMove.time) / 1000.f);

   if(isGhost())       // Here on down is server only
      return;

   if(!exploded)
      if(getActualVel().len() < 4.0)
         explode(getActualPos(), WeaponBurst);

   // Update TTL
   S32 deltaT = mCurrentMove.time;
   if(path == GameObject::ClientIdleMainRemote)
      mTimeRemaining += deltaT;
   else if(!exploded)
   {
      if(mTimeRemaining <= deltaT)
        explode(getActualPos(), WeaponBurst);
      else
         mTimeRemaining -= deltaT;
   }
}


U32 GrenadeProjectile::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 ret = Parent::packUpdate(connection, updateMask, stream);

   stream->writeFlag(exploded);
   stream->writeFlag((updateMask & InitialMask) && (getGame()->getCurrentTime() - getCreationTime() < 500));
   return ret;
}


void GrenadeProjectile::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   Parent::unpackUpdate(connection, stream);

   TNLAssert(connection, "Invalid connection to server in GrenadeProjectile//projectile.cpp");

   if(stream->readFlag())
      explode(getActualPos(), WeaponBurst);

   if(stream->readFlag())
      SoundSystem::playSoundEffect(SFXGrenadeProjectile, getActualPos(), getActualVel());
}


void GrenadeProjectile::damageObject(DamageInfo *theInfo)
{
   // If we're being damaged by another grenade, explode...
   if(theInfo->damageType == DamageTypeArea)
   {
      explode(getActualPos(), WeaponBurst);
      return;
   }

   computeImpulseDirection(theInfo);

   setMaskBits(PositionMask);
}


// Also used for mines and spybugs  --> not sure if we really need to pass weaponType
void GrenadeProjectile::explode(Point pos, WeaponType weaponType)
{
   if(exploded) return;

#ifndef ZAP_DEDICATED
   if(isGhost())
   {
      // Make us go boom!
      Color b(1,1,1);

      TNLAssert(dynamic_cast<ClientGame *>(getGame()) != NULL, "Not a ClientGame");
      //static_cast<ClientGame *>(getGame())->emitExplosion(getRenderPos(), 0.5, GameWeapon::projectileInfo[ProjectilePhaser].sparkColors, NumSparkColors);      // Original, nancy explosion
      static_cast<ClientGame *>(getGame())->emitBlast(pos, OuterBlastRadius);          // New, manly explosion

      SoundSystem::playSoundEffect(SFXMineExplode, getActualPos(), Point());
   }
#endif

   disableCollision();

   if(!isGhost())
   {
      setMaskBits(PositionMask);
      deleteObject(100);

      DamageInfo info;
      info.collisionPoint       = pos;
      info.damagingObject       = this;
      info.damageAmount         = GameWeapon::weaponInfo[weaponType].damageAmount;
      info.damageType           = DamageTypeArea;
      info.damageSelfMultiplier = GameWeapon::weaponInfo[weaponType].damageSelfMultiplier;

      S32 hits = radiusDamage(pos, InnerBlastRadius, OuterBlastRadius, (TestFunc)isDamageableType, info);

      if(getOwner())
         for(S32 i = 0; i < hits; i++)
            getOwner()->getClientInfo()->getStatistics()->countHit(mWeaponType);
   }
   exploded = true;
}


bool GrenadeProjectile::collide(GameObject *otherObj)
{
   return true;
}


void GrenadeProjectile::renderItem(const Point &pos)
{
   if(exploded)
      return;

   // Add some sparks... this needs work, as it is rather dooky  Looks too much like a comet!
   //S32 num = Random::readI(1, 10);
   //for(S32 i = 0; i < num; i++)
   //{
   //   Point sparkVel = mMoveState[RenderState].vel * Point(Random::readF() * -.5 + .55, Random::readF() * -.5 + .55);
   //   //sparkVel.normalize(Random::readF());
   //   FXManager::emitSpark(pos, sparkVel, Color(Random::readF() *.5 +.5, Random::readF() *.5, 0), Random::readF() * 2, FXManager::SparkTypePoint);
   //}

   WeaponInfo *wi = GameWeapon::weaponInfo + WeaponBurst;
   F32 initTTL = (F32) wi->projLiveTime;
   renderGrenade( pos, (initTTL - (F32) (getGame()->getCurrentTime() - getCreationTime())) / initTTL, getGame()->getSettings()->getIniSettings()->burstGraphicsMode );
}


///// Lua interface

//  Lua constructor
GrenadeProjectile::GrenadeProjectile(lua_State *L)
{
   // Do not use
}


S32 GrenadeProjectile::getLoc(lua_State *L)
{
   return Parent::getLoc(L);
}


S32 GrenadeProjectile::getRad(lua_State *L)
{
   return Parent::getRad(L);
}


S32 GrenadeProjectile::getVel(lua_State *L)
{
   return Parent::getVel(L);
}


S32 GrenadeProjectile::getTeamIndx(lua_State *L)
{
   return returnInt(L, mTeam + 1);
}


GameObject *GrenadeProjectile::getGameObject()
{
   return this;
}


S32 GrenadeProjectile::getWeapon(lua_State *L)
{
   return returnInt(L, WeaponBurst);
}


void GrenadeProjectile::push(lua_State *L)
{
   Lunar<LuaProjectile>::push(L, this);
}


////////////////////////////////////////
////////////////////////////////////////

static void drawLetter(char letter, const Point &pos, const Color &color, F32 alpha)
{
#ifndef ZAP_DEDICATED
   // Mark the item with a letter, unless we're showing the reference ship
   F32 vertOffset = 8;
   if (letter >= 'a' && letter <= 'z')    // Better position lowercase letters
      vertOffset = 10;

   glColor(color, alpha);
   F32 xpos = pos.x - UserInterface::getStringWidthf(15, "%c", letter) / 2;

   UserInterface::drawStringf(xpos, pos.y - vertOffset, 15, "%c", letter);
#endif
}


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(Mine);

// Constructor (planter defaults to null)
Mine::Mine(Point pos, Ship *planter) : GrenadeProjectile(pos, Point())
{
   mObjectTypeNumber = MineTypeNumber;
   mWeaponType = WeaponMine;

   if(planter)
   {
      setOwner(planter->getOwner());
      mTeam = planter->getTeam();
   }
   else
   {
      mTeam = -2;    // Hostile to all, as mines generally are!
      setOwner(NULL);
   }

   mArmed = false;
   mKillString = "mine";      // Triggers special message when player killed

   setExtent(Rect(pos, pos));
}


Mine *Mine::clone() const
{
   return new Mine(*this);
}


// ProcessArguments() used is the one in item
string Mine::toString(F32 gridSize) const
{
   return string(Object::getClassName()) + " " + geomToString(gridSize);
}


void Mine::idle(IdleCallPath path)
{
   // Skip the grenade timing goofiness...
   MoveItem::idle(path);

   if(exploded || path != GameObject::ServerIdleMainLoop)
      return;

   // And check for enemies in the area...
   Point pos = getActualPos();
   Rect queryRect(pos, pos);
   queryRect.expand(Point(SensorRadius, SensorRadius));

   fillVector.clear();
   findObjects((TestFunc)isMotionTriggerType, fillVector, queryRect);

   // Found something!
   bool foundItem = false;
   for(S32 i = 0; i < fillVector.size(); i++)
   {
      GameObject *foundObject = dynamic_cast<GameObject *>(fillVector[i]);

      F32 radius;
      Point ipos;
      if(foundObject->getCollisionCircle(MoveObject::RenderState, ipos, radius))
      {
         if((ipos - pos).len() < (radius + SensorRadius))
         {
            bool isMine = foundObject->getObjectTypeNumber() == MineTypeNumber;
            if(!isMine)
            {
               foundItem = true;
               break;
            }
            else if(mArmed && foundObject != this)
            {
               foundItem = true;
               break;
            }
         }
      }
   }
   if(foundItem)
   {     // braces needed
      if(mArmed)
         explode(getActualPos(), WeaponMine);
   }
   else
   {
      if(!mArmed)
      {
         setMaskBits(ArmedMask);
         mArmed = true;
      }
   }
}


bool Mine::collide(GameObject *otherObj)
{
   if(isProjectileType(otherObj->getObjectTypeNumber()))
      explode(getActualPos(), WeaponMine);
   return false;
}


void Mine::damageObject(DamageInfo *info)
{
   if(info->damageAmount > 0.f && !exploded)
      explode(getActualPos(), WeaponMine);
}


// Only runs on server side
U32 Mine::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 ret = Parent::packUpdate(connection, updateMask, stream);

   if(updateMask & InitialMask)
   {
      writeThisTeam(stream);
      stream->writeStringTableEntry(getOwner() ? getOwner()->getClientInfo()->getName() : "");
   }

   stream->writeFlag(mArmed);

   return ret;
}


void Mine::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   bool initial = false;
   Parent::unpackUpdate(connection, stream);

   if(mInitial)     // Initial data
   {
      initial = true;
      readThisTeam(stream);
      stream->readStringTableEntry(&mSetBy);
   }
   bool wasArmed = mArmed;
   mArmed = stream->readFlag();

   if(initial && !mArmed)
      SoundSystem::playSoundEffect(SFXMineDeploy, getActualPos(), Point());
   else if(!initial && !wasArmed && mArmed)
      SoundSystem::playSoundEffect(SFXMineArm, getActualPos(), Point());
}


void Mine::renderItem(const Point &pos)
{
#ifndef ZAP_DEDICATED
   if(exploded)
      return;

   bool visible, armed;

   Game *game = getGame();
   ClientGame *clientGame = dynamic_cast<ClientGame *>(game);

   if(clientGame && clientGame->getConnectionToServer())
   {
      Ship *ship = dynamic_cast<Ship *>(clientGame->getConnectionToServer()->getControlObject());

      if(!ship)
         return;

      armed = mArmed;

      GameType *gameType = clientGame->getGameType();

      GameConnection *localClient = clientGame->getConnectionToServer();

      // Can see mine if laid by teammate in team game || sensor is active ||
      // you laid it yourself
      visible = ( (ship->getTeam() == getTeam()) && gameType->isTeamGame() ) || ship->isModulePrimaryActive(ModuleSensor) ||
                  (localClient && localClient->getClientInfo()->getName() == mSetBy);
   }
   else     // Must be in editor?
   {
      armed = true;
      visible = true;
   }

   renderMine(pos, armed, visible);
#endif
}


void Mine::renderEditor(F32 currentScale)
{
   renderMine(getVert(0), true, true);
}


void Mine::renderDock()
{
#ifndef ZAP_DEDICATED
   glColor3f(.7f, .7f, .7f);
   drawCircle(getVert(0), 9);
   drawLetter('M', getVert(0), Color(.7), 1);
#endif
}


const char *Mine::getEditorHelpString()
{
   return "Mines can be prepositioned, and are are \"hostile to all\". [M]";
}


const char *Mine::getPrettyNamePlural()
{
   return "Mines";
}


const char *Mine::getOnDockName()
{
   return "Mine";
}


const char *Mine::getOnScreenName()
{
   return "Mine";
}


bool Mine::hasTeam()
{
   return false;
}


bool Mine::canBeHostile()
{
   return false;
}


bool Mine::canBeNeutral()
{
   return false;
}


// Lua methods
const char Mine::className[] = "MineItem";      // Class name as it appears to Lua scripts

// Define the methods we will expose to Lua
Lunar<Mine>::RegType Mine::methods[] =
{
   // Standard gameItem methods
   method(Mine, getClassID),
   method(Mine, getLoc),
   method(Mine, getRad),
   method(Mine, getVel),
   method(Mine, getTeamIndx),

   method(Mine, getWeapon),

   {0,0}    // End method list
};

//  Lua constructor
Mine::Mine(lua_State *L)
{
   // Do not use
}

S32 Mine::getClassID(lua_State *L)
{
   return returnInt(L, MineTypeNumber);
}


S32 Mine::getLoc(lua_State *L)
{
   return Parent::getLoc(L);
}


S32 Mine::getRad(lua_State *L)
{
   return Parent::getRad(L);
}


S32 Mine::getVel(lua_State *L)
{
   return Parent::getVel(L);
}


GameObject *Mine::getGameObject()
{
   return this;
}


S32 Mine::getWeapon(lua_State *L)
{
   return returnInt(L, WeaponMine);
}


void Mine::push(lua_State *L)
{
   Lunar<LuaProjectile>::push(L, this);
}

//////////////////////////////////
//////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(SpyBug);

// Constructor
SpyBug::SpyBug(Point pos, Ship *planter) : GrenadeProjectile(pos, Point())
{
   mObjectTypeNumber = SpyBugTypeNumber;

   mWeaponType = WeaponSpyBug;
   mNetFlags.set(Ghostable);

   if(planter)
   {
      setOwner(planter->getOwner());
      mTeam = planter->getTeam();
   }
   else
   {
      mTeam = -1;
      setOwner(NULL);
   }

   setExtent(Rect(pos, pos));
}


// Destructor
SpyBug::~SpyBug()
{
   if(gServerGame && gServerGame->getGameType())
      gServerGame->getGameType()->catalogSpybugs();
}


SpyBug *SpyBug::clone() const
{
   return new SpyBug(*this);
}


bool SpyBug::processArguments(S32 argc, const char **argv, Game *game)
{
   if(argc < 3)
      return false;

   mTeam = atoi(argv[0]);                        // Team first!

   // Strips off first arg from argv, so the parent gets the straight coordinate pair it's expecting
   if(!Parent::processArguments(2, &argv[1], game))    
      return false;

   return true;
}


// ProcessArguments() used is the one in item
string SpyBug::toString(F32 gridSize) const
{
   return string(Object::getClassName()) + " " + itos(mTeam) + " " + geomToString(gridSize);
}


// Spy bugs are always in scope.  This only really matters on pre-positioned spy bugs...
void SpyBug::onAddedToGame(Game *theGame)
{
   Parent::onAddedToGame(theGame);

   if(!isGhost())
      setScopeAlways();

   GameType *gt = getGame()->getGameType();
   if(gt && !isGhost())  // non-Ghost / ServerGame only, currently useless for client
      gt->addSpyBug(this);
}


void SpyBug::idle(IdleCallPath path)
{
   // Skip the grenade timing goofiness...
   MoveItem::idle(path);

   if(exploded || path != GameObject::ServerIdleMainLoop)
      return;
}


bool SpyBug::collide(GameObject *otherObj)
{
   if(isProjectileType(otherObj->getObjectTypeNumber()))
      explode(getActualPos(), WeaponSpyBug);
   return false;
}


void SpyBug::damageObject(DamageInfo *info)
{
   if(info->damageAmount > 0.f && !exploded)    // Any damage will kill the SpyBug
      explode(getActualPos(), WeaponSpyBug);
}


U32 SpyBug::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 ret = Parent::packUpdate(connection, updateMask, stream);
   if(stream->writeFlag(updateMask & InitialMask))
   {
      writeThisTeam(stream);
      //RDW I want to kill the compiler that allows binding NULL to a reference.
      //stream->writeStringTableEntry(getOwner() ? getOwner()->getClientName() : NULL);
      // Just don't kill the coder who keeps doing it! -CE
      // And remember, pack and unpack must match, so if'ing this out won't work unless we do the same on unpack.
      StringTableEntryRef noOwner = StringTableEntryRef("");
      stream->writeStringTableEntry(getOwner() ? getOwner()->getClientInfo()->getName() : noOwner);
   }
   return ret;
}

void SpyBug::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   bool initial = false;
   Parent::unpackUpdate(connection, stream);

   if(stream->readFlag())
   {
      initial = true;
      readThisTeam(stream);
      stream->readStringTableEntry(&mSetBy);
   }
   if(initial)
      SoundSystem::playSoundEffect(SFXSpyBugDeploy, getActualPos(), Point());
}


void SpyBug::renderItem(const Point &pos)
{
#ifndef ZAP_DEDICATED
   if(exploded)
      return;

   bool visible;

   Game *game = getGame();
   ClientGame *clientGame = dynamic_cast<ClientGame *>(game);

   if(clientGame && clientGame->getConnectionToServer())
   {
      GameConnection *conn = clientGame->getConnectionToServer();   
      Ship *ship = dynamic_cast<Ship *>(conn->getControlObject());

      if(!ship)
         return;

      GameType *gameType = clientGame->getGameType();

      // Can see bug if laid by teammate in team game || sensor is active ||
      //       you laid it yourself                   || spyBug is neutral
      visible = ( ((ship->getTeam() == getTeam()) && gameType->isTeamGame())   || ship->isModulePrimaryActive(ModuleSensor) ||
                  (conn && conn->getClientInfo()->getName() == mSetBy) || getTeam() == TEAM_NEUTRAL);
   }
   else    
      visible = true;      // We get here in editor when in preview mode

   renderSpyBug(pos, getGame()->getTeamColor(mTeam), visible, true);
#endif
}


void SpyBug::renderEditor(F32 currentScale)
{
   renderSpyBug(getVert(0), getTeamColor(getTeam()), true, true);
}


void SpyBug::renderDock()
{
#ifndef ZAP_DEDICATED
   const F32 radius = 9;

   glColor(getTeamColor(mTeam));
   drawFilledCircle(getVert(0), radius);

   glColor(.7f);
   drawCircle(getVert(0), radius);
   drawLetter('S', getVert(0), Color(mTeam < 0 ? .5 : .7), 1);    // Use darker gray for neutral spybugs so S will show up clearer
#endif
}


const char *SpyBug::getEditorHelpString()
{
   return "Remote monitoring device that shows enemy ships on the commander's map. [Ctrl-B]";
}


const char *SpyBug::getPrettyNamePlural()
{
   return "Spy Bugs";
}


const char *SpyBug::getOnDockName()
{
   return "Bug";
}


const char *SpyBug::getOnScreenName()
{
   return "Spy Bug";
}


bool SpyBug::hasTeam()
{
   return true;
}


bool SpyBug::canBeHostile()
{
   return false;
}


bool SpyBug::canBeNeutral()
{
   return true;
}


// Can the player see the spybug?
bool SpyBug::isVisibleToPlayer(S32 playerTeam, StringTableEntry playerName, bool isTeamGame)
{
   // On our team (in a team game) || was set by us (in any game) || is neutral (in any game)
   return ((getTeam() == playerTeam) && isTeamGame) || playerName == mSetBy || mTeam == -1;
}



// Lua methods
const char SpyBug::className[] = "SpyBugItem";      // Class name as it appears to Lua scripts

// Define the methods we will expose to Lua
Lunar<SpyBug>::RegType SpyBug::methods[] =
{
   // Standard gameItem methods
   method(SpyBug, getClassID),
   method(SpyBug, getLoc),
   method(SpyBug, getRad),
   method(SpyBug, getVel),
   method(SpyBug, getTeamIndx),

   method(SpyBug, getWeapon),

   {0,0}    // End method list
};


//  Lua constructor
SpyBug::SpyBug(lua_State *L)
{
   // Do not use
}

S32 SpyBug::getClassID(lua_State *L)
{
   return returnInt(L, SpyBugTypeNumber);
}


S32 SpyBug::getLoc(lua_State *L)
{
   return Parent::getLoc(L);
}


S32 SpyBug::getRad(lua_State *L)
{
   return Parent::getRad(L);
}


S32 SpyBug::getVel(lua_State *L)
{
   return Parent::getVel(L);
}


GameObject *SpyBug::getGameObject()
{
   return this;
}


S32 SpyBug::getWeapon(lua_State *L)
{
   return returnInt(L, WeaponSpyBug);
}


void SpyBug::push(lua_State *L)
{
   Lunar<LuaProjectile>::push(L, this);
}


};

