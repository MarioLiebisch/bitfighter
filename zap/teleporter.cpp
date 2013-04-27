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

#include "teleporter.h"

using namespace TNL;

#include "loadoutZone.h"          // For when ship teleports onto a loadout zone
#include "gameLoader.h"
#include "gameObjectRender.h"
#include "ClientInfo.h"

#include "Colors.h"
#include "SoundSystem.h"

#include "stringUtils.h"
#include "tnlMethodDispatch.h"      // For writing vectors

#include "ship.h"

#ifndef ZAP_DEDICATED
#   include "ClientGame.h"
#endif

#include <math.h>

#ifndef sq
#  define sq(a) ((a) * (a))
#endif


namespace Zap
{

S32 DestManager::getDestCount() const
{
   return mDests.size();
}


Point DestManager::getDest(S32 index) const
{
   // If we have no desitnations, return the orgin for a kind of bizarre loopback effect
   if(mDests.size() == 0)
      return mOwner->getPos();

   return mDests[index];
}


S32 DestManager::getRandomDest() const
{
   if(mDests.size() == 0)
      return 0;
   else
      return (S32)TNL::Random::readI(0, mDests.size() - 1);
}


void DestManager::addDest(const Point &dest)
{
   TNLAssert(mOwner, "We need an owner here!");
   mDests.push_back(dest);
   if(mDests.size() == 1)      // Just added the first dest
   {
      mOwner->setVert(dest, 1);
      mOwner->updateExtentInDatabase();
   }
   // If we're the server, update the clients 
   if(mOwner->getGame() && mOwner->getGame()->isServer())
      mOwner->s2cAddDestination(dest);
}


void DestManager::setDest(S32 index, const Point &dest)
{
   TNLAssert(index < mDests.size(), "Invalid index!");
   mDests[index] = dest;
}


void DestManager::delDest(S32 index)
{
   if(mDests.size() == 1)
      clear();
   else
      mDests.erase(index);

   // If we're the server, update the clients -- this will likely be used rarely; not worth a new RPC, I think
   if(mOwner->getGame() && mOwner->getGame()->isServer())
   {
      mOwner->s2cClearDestinations();
      for(S32 i = 0; i < mDests.size(); i++)
         mOwner->s2cAddDestination(mDests[i]);
   }
}


void DestManager::clear()
{
   mDests.clear();
   mOwner->setVert(mOwner->getPos(), 1);       // Set destination to same as origin
   mOwner->updateExtentInDatabase();

   // If we're the server, update the clients 
   if(mOwner->getGame() && mOwner->getGame()->isServer())
      mOwner->s2cClearDestinations();
}


void DestManager::resize(S32 count)
{
   mDests.resize(count);
}


// Read a single dest
void DestManager::read(S32 index, BitStream *stream)
{
   mDests[index].read(stream);
}


// Read a whole list of dests
void DestManager::read(BitStream *stream)
{
   Types::read(*stream, &mDests);
}


/*const*/ Vector<Point> *DestManager::getDestList() /*const*/
{
   return &mDests;
}


// Only done at creation time
void DestManager::setOwner(Teleporter *owner)
{
   mOwner = owner;
}

////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(Teleporter);

static Vector<DatabaseObject *> foundObjects;      // Reusable container


// Combined default C++/Lua constructor
Teleporter::Teleporter(lua_State *L)
{
   initialize(Point(0,0), Point(0,0), NULL);
   
   if(L)
   {
      static LuaFunctionArgList constructorArgList = { {{ END }, { PT, END }, { LINE, END } }, 3 };
      
      S32 profile = checkArgList(L, constructorArgList, "Teleporter", "constructor");

      if(profile > 0)
         doSetGeom(L);
   }
}


// Constructor used by engineer
Teleporter::Teleporter(const Point &pos, const Point &dest, Ship *engineeringShip) : Engineerable()
{
   initialize(pos, dest, engineeringShip);
}


// Destructor
Teleporter::~Teleporter()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


// Note that the teleporter may already have destinations when we get here... just something to keep in mind
void Teleporter::initialize(const Point &pos, const Point &dest, Ship *engineeringShip)
{
   mObjectTypeNumber = TeleporterTypeNumber;
   mNetFlags.set(Ghostable);

   mTime = 0;
   mTeleporterCooldown = TeleporterCooldown;    // Teleporters can have non-standard cooldown periods, but start with default
   setTeam(TEAM_NEUTRAL);

   setVert(pos, 0);
   setVert(dest, 1);

   mHasExploded = false;
   mFinalExplosionTriggered = false;
   mStartingHealth = 1.0f;

   mLastDest = 0;
   mDestManager.setOwner(this);

   mEngineeringShip = engineeringShip;

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


Teleporter *Teleporter::clone() const
{
   return new Teleporter(*this);
}


void Teleporter::onAddedToGame(Game *theGame)
{
   Parent::onAddedToGame(theGame);
   linkToIdleList(&theGame->idlingObjects);

   if(!isGhost())
      setScopeAlways();    // Always in scope!
}


bool Teleporter::processArguments(S32 argc2, const char **argv2, Game *game)
{
   S32 argc = 0;
   const char *argv[8];

   for(S32 i = 0; i < argc2; i++)      // The idea here is to allow optional R3.5 for rotate at speed of 3.5
   {
      char firstChar = argv2[i][0];    // First character of arg

      if((firstChar >= 'a' && firstChar <= 'z') || (firstChar >= 'A' && firstChar <= 'Z'))  // starts with a letter
      {
         if(!strnicmp(argv2[i], "Delay=", 6))
            mTeleporterCooldown = MAX(100, U32(atof(&argv2[i][6]) * 1000));
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

   if(argc != 4)
      return false;

   Point pos, dest;

   pos.read(argv);
   dest.read(argv + 2);

   pos  *= game->getGridSize();
   dest *= game->getGridSize();

   setVert(pos,  0);
   setVert(dest, 1);

   // See if we already have any teleports with this pos... if so, this is a "multi-dest" teleporter.
   // Note that editor handles multi-dest teleporters as separate single dest items, so this only runs on server!
   if(game->isServer())    
   {
      foundObjects.clear();
      game->getGameObjDatabase()->findObjects(TeleporterTypeNumber, foundObjects, Rect(pos, 1));

      for(S32 i = 0; i < foundObjects.size(); i++)
      {
         Teleporter *tel = static_cast<Teleporter *>(foundObjects[i]);
         if(tel->getVert(0).distSquared(pos) < 1)     // i.e These are really close!  Must be the same!
         {
            tel->addDest(dest);

            // See http://www.parashift.com/c++-faq-lite/delete-this.html for thoughts on delete this here
            delete this;    // Since this is really part of a different teleporter, delete this one 
            return true;    // There will only be one!
         }
      }

      // New teleporter origin
      addDest(dest);
      computeExtent(); // for ServerGame extent
   }
#ifndef ZAP_DEDICATED
   else     // Is client
   {
      addDest(dest);
      setExtent(calcExtents()); // for editor
   }
#endif

   return true;
}


TNL_IMPLEMENT_NETOBJECT_RPC(Teleporter, s2cAddDestination, (Point dest), (dest),
   NetClassGroupGameMask, RPCGuaranteed, RPCToGhost, 0)
{
   mDestManager.addDest(dest);
}


TNL_IMPLEMENT_NETOBJECT_RPC(Teleporter, s2cClearDestinations, (), (),
   NetClassGroupGameMask, RPCGuaranteed, RPCToGhost, 0)
{
   mDestManager.clear();
}


string Teleporter::toLevelCode(F32 gridSize) const
{
   string out = string(appendId(getClassName())) + " " + geomToLevelCode(gridSize);

   if(mTeleporterCooldown != TeleporterCooldown)
      out += " Delay=" + ftos(mTeleporterCooldown / 1000.f, 3);
   return out;
}


Rect Teleporter::calcExtents()
{
   Rect rect(getVert(0), getVert(1));
   rect.expand(Point(Teleporter::TELEPORTER_RADIUS, Teleporter::TELEPORTER_RADIUS));
   return rect;
}


bool Teleporter::checkDeploymentPosition(const Point &position, GridDatabase *gb, Ship *ship)
{
   Rect queryRect(position, TELEPORTER_RADIUS * 2);
   Point outPoint;  // only used as a return value in polygonCircleIntersect

   foundObjects.clear();
   gb->findObjects((TestFunc) isCollideableType, foundObjects, queryRect);

   Point foundObjectCenter;
   F32 foundObjectRadius;

   for(S32 i = 0; i < foundObjects.size(); i++)
   {
      BfObject *bfObject = static_cast<BfObject *>(foundObjects[i]);

      // Skip if found objects are same team as the ship that is deploying the teleporter
      if(bfObject->getTeam() == ship->getTeam())
         continue;

      // Now calculate bounds
      const Vector<Point> *foundObjectBounds = bfObject->getCollisionPoly();
      if(foundObjectBounds)
      {
         // If they intersect, then bad deployment position
         if(foundObjectBounds->size() != 0)
            if(polygonCircleIntersect(foundObjectBounds->address(), foundObjectBounds->size(), position, sq(TELEPORTER_RADIUS), outPoint))
               return false;
      }
      // Try the collision circle if no poly bounds were found
      else
      {
         if(bfObject->getCollisionCircle(RenderState, foundObjectCenter, foundObjectRadius))
            if(circleCircleIntersect(foundObjectCenter, foundObjectRadius, position, TELEPORTER_RADIUS))
               return false;
      }

   }

   return true;
}


U32 Teleporter::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   if(stream->writeFlag(updateMask & InitMask))
   {
      getVert(0).write(stream);

      stream->writeFlag(mEngineered);

      S32 dests = mDestManager.getDestCount();

      stream->writeInt(dests, 16);

      for(S32 i = 0; i < dests; i++)
         getDest(i).write(stream);

      if(stream->writeFlag(mTeleporterCooldown != TeleporterCooldown))  // Most teleporter will have default timing
         stream->writeInt(mTeleporterCooldown, 32);

      if(mTeleporterCooldown != 0 && stream->writeFlag(mTeleportCooldown.getCurrent() != 0))
         stream->writeInt(mTeleportCooldown.getCurrent(), 32);    // A player might join while this teleporter is in the middle of cooldown  // TODO: Make this a rangedInt
   }
   else if(stream->writeFlag(updateMask & TeleportMask))          // Basically, this gets triggered if a ship passes through
   {
      TNLAssert(U32(mLastDest) < U32(mDestManager.getDestCount()), "packUpdate out of range teleporter number");
      stream->write(mLastDest);                                   // Where ship is going
   }

   // If we're not destroyed and health has changed
   if(!stream->writeFlag(mHasExploded))
   {
      if(stream->writeFlag(updateMask & HealthMask))
         stream->writeFloat(mStartingHealth, 6);
   }

   return 0;
}


void Teleporter::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   if(stream->readFlag())                 // InitMask
   {
      U32 count;
      Point pos;
      pos.read(stream);
      setVert(pos, 0);
      setVert(pos, 1);                    // Simulate a point geometry -- will be changed later when we add our first dest

      mEngineered = stream->readFlag();

      count = stream->readInt(16);
      mDestManager.resize(count);         // Prepare the list for multiple additions

      for(U32 i = 0; i < count; i++)
         mDestManager.read(i, stream);
      
      computeExtent();

      if(stream->readFlag())
         mTeleporterCooldown = stream->readInt(32);

      if(mTeleporterCooldown != 0 && stream->readFlag())
         mTeleportCooldown.reset(stream->readInt(32));
   }
   else if(stream->readFlag())         // TeleportMask
   {
      S32 dest;
      stream->read(&dest);

#ifndef ZAP_DEDICATED
      TNLAssert(U32(dest) < U32(mDestManager.getDestCount()), "unpackUpdate out of range teleporter number");
      if(U32(dest) < U32(mDestManager.getDestCount()))
      {
         TNLAssert(dynamic_cast<ClientGame *>(getGame()) != NULL, "Not a ClientGame");
         static_cast<ClientGame *>(getGame())->emitTeleportInEffect(mDestManager.getDest(dest), 0);

         SoundSystem::playSoundEffect(SFXTeleportIn, mDestManager.getDest(dest));
      }

      SoundSystem::playSoundEffect(SFXTeleportOut, getVert(0));
#endif
      mTeleportCooldown.reset(mTeleporterCooldown);
   }

   if(stream->readFlag())     // mHasExploded
   {
      if(!mHasExploded)
      {
         mHasExploded = true;
         disableCollision();
         mExplosionTimer.reset(TeleporterExplosionTime);
         SoundSystem::playSoundEffect(SFXTeleportExploding, getVert(0));
         mFinalExplosionTriggered = false;
      }
   }

   // HealthMask
   else if(stream->readFlag())
      mStartingHealth = stream->readFloat(6);
}


void Teleporter::damageObject(DamageInfo *theInfo)
{
   // Only engineered teleports can be damaged
   if(!mEngineered)
      return;

   if(mHasExploded)
      return;

   // Reduce damage to 1/4 to match other engineered items
   if(theInfo->damageAmount > 0)
      mStartingHealth -= theInfo->damageAmount * .25f;

   setMaskBits(HealthMask);

   // Destroyed!
   if(mStartingHealth <= 0)
      onDestroyed();
}


void Teleporter::onDestroyed()
{
   mHasExploded = true;

   releaseResource(getVert(0), getGame()->getGameObjDatabase());

   deleteObject(TeleporterExplosionTime + 500);  // Guarantee our explosion effect will complete
   setMaskBits(DestroyedMask);

   if(mEngineeringShip && mEngineeringShip->getEngineeredTeleporter() == this && mEngineeringShip->getClientInfo())
   {
      mEngineeringShip->getClientInfo()->sTeleporterCleanup();

      if(!mEngineeringShip->getClientInfo()->isRobot())     // If they're not a bot, tell client to hide engineer menu
      {
         static const StringTableEntry Your_Teleporter_Got_Destroyed("Your teleporter got destroyed");
         mEngineeringShip->getClientInfo()->getConnection()->s2cDisplayErrorMessage(Your_Teleporter_Got_Destroyed);
         mEngineeringShip->getClientInfo()->getConnection()->s2cEngineerResponseEvent(EngineeredTeleporterExit);
      }
   }
}

void Teleporter::doTeleport()
{
   static const F32 TRIGGER_RADIUS  = F32(TELEPORTER_RADIUS - Ship::CollisionRadius);
   static const F32 TELEPORT_RADIUS = F32(TELEPORTER_RADIUS + Ship::CollisionRadius);

   if(mDestManager.getDestCount() == 0)      // Ignore 0-dest teleporters -- where would you go??
      return;

   Rect queryRect(getVert(0), TRIGGER_RADIUS * 2);     

   foundObjects.clear();
   findObjects((TestFunc)isShipType, foundObjects, queryRect);

   S32 dest = mDestManager.getRandomDest();

   Point teleportCenter = getVert(0);

   bool isTriggered = false; // First, check if triggered, can come from idle timer.
   for(S32 i = 0; i < foundObjects.size(); i++)
   {
      if((teleportCenter - static_cast<Ship *>(foundObjects[i])->getRenderPos()).lenSquared() < sq(TRIGGER_RADIUS))
      {
         isTriggered = true;
         break;
      }
   }

   if(!isTriggered)
      return;

   setMaskBits(TeleportMask);
   mTeleportCooldown.reset(mTeleporterCooldown);      // Teleport needs to wait a bit before being usable again

   // We've triggered the teleporter.  Relocate any ships within range.  Any ship touching the teleport will be warped.
   for(S32 i = 0; i < foundObjects.size(); i++)
   {
      Ship *ship = static_cast<Ship *>(foundObjects[i]);
      if((teleportCenter - ship->getRenderPos()).lenSquared() < sq(TELEPORT_RADIUS))
      {
         mLastDest = dest;    // Save the destination

         Point newPos = ship->getActualPos() - teleportCenter + mDestManager.getDest(dest);
         ship->setActualPos(newPos, true);

         if(ship->getClientInfo() && ship->getClientInfo()->getStatistics())
            ship->getClientInfo()->getStatistics()->mTeleport++;

         // See if we've teleported onto a zone of some sort
         BfObject *zone = ship->isInAnyZone();
         if(zone)
            zone->collide(ship);
      }
   }

   Vector<DatabaseObject *> foundTeleporters; // Must be kept local, non static, because of possible recursive.
   queryRect.set(getVert(0), TRIGGER_RADIUS * 2);
   findObjects(TeleporterTypeNumber, foundTeleporters, queryRect);
   for(S32 i = 0; i < foundTeleporters.size(); i++)
      if(static_cast<Teleporter *>(foundTeleporters[i])->mTeleportCooldown.getCurrent() == 0)
         static_cast<Teleporter *>(foundTeleporters[i])->doTeleport();
}


bool Teleporter::collide(BfObject *otherObject)
{
   U8 otherObjectType = otherObject->getObjectTypeNumber();

   if(isShipType(otherObjectType))     // i.e. ship or robot
   {
      static const F32 TRIGGER_RADIUS  = F32(TELEPORTER_RADIUS - Ship::CollisionRadius);
      static const F32 TELEPORT_RADIUS = F32(TELEPORTER_RADIUS + Ship::CollisionRadius);

      if(isGhost())
         return false; // Server only

      if(mTeleportCooldown.getCurrent() > 0)    // Ignore teleports in cooldown mode
         return false;

      if(mDestManager.getDestCount() == 0)      // Ignore 0-dest teleporters -- where would you go??
         return false;

      if(mHasExploded)                          // Destroyed teleports don't work so well anymore...
         return false;

      // First see if we've triggered the teleport...
      Ship *ship = static_cast<Ship *>(otherObject);

      // Check if the center of the ship is closer than TRIGGER_RADIUS -- this is equivalent to testing if
      // the ship is entirely within the outer radius of the teleporter.  Therefore, ships can almost entirely 
      // overlap the teleporter before triggering the teleport.
      if((getVert(0) - ship->getActualPos()).lenSquared() > sq(TRIGGER_RADIUS))  
         return false;     // Too far -- teleport not activated!

      // Check for players within a square box around the teleporter.  Not all these ships will teleport; the actual determination is made
      // via a circle, and will be checked below.

      doTeleport();

      return true;
   }

   // Only engineered teleports have collision with projectiles
   if(isProjectileType(otherObjectType))
      return mEngineered;        

   return false;
}


bool Teleporter::getCollisionCircle(U32 state, Point &center, F32 &radius) const
{
   center = getVert(0);
   radius = TELEPORTER_RADIUS;
   return true;
}


const Vector<Point> *Teleporter::getCollisionPoly() const
{
   return NULL;
}


void Teleporter::computeExtent()
{
   setExtent(Rect(getVert(0), (F32)TELEPORTER_RADIUS * 2));    // This Rect constructor takes a diameter, not a radius
}


S32 Teleporter::getDestCount()
{
   return mDestManager.getDestCount();
}


Point Teleporter::getDest(S32 index)
{
   return mDestManager.getDest(index);
}


void Teleporter::addDest(const Point &dest)
{
   mDestManager.addDest(dest);
}


void Teleporter::delDest(S32 index)
{
   mDestManager.delDest(index);
}


void Teleporter::onConstructed()
{
   // Do nothing
}


bool Teleporter::hasAnyDests()
{
   return mDestManager.getDestCount() > 0;
}


// Server only, also called from editor
void Teleporter::setEndpoint(const Point &point)
{
   mDestManager.addDest(point);
   setVert(point, 1);
}


void Teleporter::idle(BfObject::IdleCallPath path)
{
   U32 deltaT = mCurrentMove.time;
   mTime += deltaT;

   // Client only
   if(path == BfObject::ClientIdleMainRemote)
   {
      // Update Explosion Timer
      if(mHasExploded)
         mExplosionTimer.update(deltaT);
   }

   if(mTeleportCooldown.update(deltaT) && path == BfObject::ServerIdleMainLoop)
      doTeleport();
}


void Teleporter::render()
{
#ifndef ZAP_DEDICATED
   // Render at a different radius depending on if a ship has just gone into the teleport
   // and we are waiting for the teleport timeout to expire
   F32 radiusFraction;
   if(!mHasExploded)
   {
      U32 cooldown = mTeleportCooldown.getCurrent();

      if(cooldown == 0)
         radiusFraction = 1;

      else if(cooldown > TeleporterExpandTime - TeleporterCooldown + mTeleporterCooldown)
         radiusFraction = F32(cooldown - TeleporterExpandTime + TeleporterCooldown - mTeleporterCooldown) / 
                          F32(TeleporterCooldown - TeleporterExpandTime);

      else if(mTeleporterCooldown < TeleporterExpandTime)
         radiusFraction = F32(mTeleporterCooldown - cooldown + TeleporterExpandTime - TeleporterCooldown) / 
                          F32(mTeleporterCooldown + TeleporterExpandTime - TeleporterCooldown);

      else if(cooldown < TeleporterExpandTime)
         radiusFraction = F32(TeleporterExpandTime - cooldown) / 
                          F32(TeleporterExpandTime);
      else
         radiusFraction = 0;
   }
   else
   {
      // If the teleport has been destroyed, adjust the radius larger/smaller for a neat effect
      U32 halfPeriod = mExplosionTimer.getPeriod() / 2;

      if(mExplosionTimer.getCurrent() > halfPeriod)
         radiusFraction = 2.f - F32(mExplosionTimer.getCurrent() - halfPeriod) / 
                                F32(halfPeriod);

      else
         radiusFraction = 2 * F32(mExplosionTimer.getCurrent()) / 
                              F32(halfPeriod);

      // Add ending explosion
      if(mExplosionTimer.getCurrent() == 0 && !mFinalExplosionTriggered)
         doExplosion();
   }

   if(radiusFraction != 0)
   {
      U32 trackerCount = 100;    // Trackers are the swirling bits in a teleporter.  100 gives the "classic" appearance.
      if(mStartingHealth < 1.f)
         trackerCount = U32(mStartingHealth * 75.f) + 25;

      F32 zoomFraction = static_cast<ClientGame *>(getGame())->getCommanderZoomFraction();
      U32 renderStyle = mEngineered ? 2 : 0;
      renderTeleporter(getVert(0), renderStyle, true, mTime, zoomFraction, radiusFraction, 
                       (F32)TELEPORTER_RADIUS, 1.0, mDestManager.getDestList(), trackerCount);
   }

   if(mEngineered)
   {
      // Render the exit of engineered teleports with an outline.  If teleporter has exploded, implode the exit.
      // The implosion calculations were an attempt to avoid using another timer, but perhaps that would be clearer than this mess
      const F32 IMPLOSION_FACTOR = .2;     // Smaller numbers = faster implosion

      F32 implosionOffset  = mExplosionTimer.getPeriod() * (1 - IMPLOSION_FACTOR);
      F32 implosionTime    = mExplosionTimer.getPeriod() * IMPLOSION_FACTOR;

      F32 sizeFraction = mHasExploded ? F32(mExplosionTimer.getCurrent() - implosionOffset) / implosionTime : 1;

      if(sizeFraction > 0)
         for(S32 i = mDestManager.getDestCount() - 1; i >= 0; i--)
            renderTeleporterOutline(mDestManager.getDest(i), (F32)TELEPORTER_RADIUS * sizeFraction, Colors::richGreen);
   }
#endif
}


#ifndef ZAP_DEDICATED
void Teleporter::doExplosion()
{
   mFinalExplosionTriggered = true;
   const S32 EXPLOSION_COLOR_COUNT = 12;

   static Color ExplosionColors[EXPLOSION_COLOR_COUNT] = {
         Colors::green,
         Color(0, 1, 0.5),
         Colors::white,
         Colors::yellow,
         Colors::green,
         Color(0, 0.8, 1.0),
         Color(0, 1, 0.5),
         Colors::white,
         Colors::green,
         Color(0, 1, 0.5),
         Colors::white,
         Colors::yellow,
   };

   SoundSystem::playSoundEffect(SFXShipExplode, getPos());

   F32 a = TNL::Random::readF() * 0.4f  + 0.5f;
   F32 b = TNL::Random::readF() * 0.2f  + 0.9f;
   F32 c = TNL::Random::readF() * 0.15f + 0.125f;
   F32 d = TNL::Random::readF() * 0.2f  + 0.9f;

   ClientGame *game = static_cast<ClientGame *>(getGame());

   Point pos = getPos();

   game->emitExplosion(pos, 0.65f, ExplosionColors, EXPLOSION_COLOR_COUNT);
   game->emitBurst(pos, Point(a,c) * 0.6f, Colors::yellow, Colors::green);
   game->emitBurst(pos, Point(b,d) * 0.6f, Colors::yellow, Colors::green);
}
#endif


void Teleporter::renderEditorItem()
{
#ifndef ZAP_DEDICATED
   renderTeleporterEditorObject(getVert(0), TELEPORTER_RADIUS, getEditorRenderColor());
#endif
}


Color Teleporter::getEditorRenderColor()
{
   return Colors::green;
}


void Teleporter::newObjectFromDock(F32 gridSize)
{
   addDest(Point(0,0));      // We need to have one for rendering preview... actual location will be updated in onGeomChanged()
   Parent::newObjectFromDock(gridSize);
}


void Teleporter::onAttrsChanging()
{
   /* Do nothing */
}


void Teleporter::onGeomChanging()
{
   onGeomChanged();
}


void Teleporter::onGeomChanged()
{
   Parent::onGeomChanged();

   // Update the dest manager.  We need this for rendering in preview mode.
   mDestManager.setDest(0, getVert(1));
}   


const char *Teleporter::getOnScreenName()     { return "Teleporter";    }
const char *Teleporter::getPrettyNamePlural() { return "Teleporters"; }
const char *Teleporter::getOnDockName()       { return "Teleporter";    }
const char *Teleporter::getEditorHelpString() { return "Teleports ships from one place to another. [T]"; }


bool Teleporter::hasTeam()      { return false; }
bool Teleporter::canBeHostile() { return false; }
bool Teleporter::canBeNeutral() { return false; }


//// Lua methods

/**
  *  @luaconst Teleporter::Teleporter()
  *  @luaconst Teleporter::Teleporter(geom)
  *  @luaclass Teleporter
  *  @brief Instantly transports ships from here to there.
  *  @descr A %Teleporter represents the basic teleporter object.  Every teleporter has an intake location
  *         and one or more destinations.  When a ship enters the teleporter, a destination will be chosen 
  *         randomly if there is more than one.   
  */
//               Fn name                       Param profiles         Profile count                           
#define LUA_METHODS(CLASS, METHOD) \
   METHOD(CLASS, addDest,      ARRAYDEF({ { PT,  END }                }), 1 ) \
   METHOD(CLASS, delDest,      ARRAYDEF({ { INT, END }                }), 1 ) \
   METHOD(CLASS, clearDests,   ARRAYDEF({ {      END }                }), 1 ) \
   METHOD(CLASS, getDest,      ARRAYDEF({ { INT, END }                }), 1 ) \
   METHOD(CLASS, getDestCount, ARRAYDEF({ {      END }                }), 1 ) \
   METHOD(CLASS, setGeom,      ARRAYDEF({ { PT,  END }, { LINE, END } }), 2 ) \

GENERATE_LUA_METHODS_TABLE(Teleporter, LUA_METHODS);
GENERATE_LUA_FUNARGS_TABLE(Teleporter, LUA_METHODS);

#undef LUA_METHODS


const char *Teleporter::luaClassName = "Teleporter";
REGISTER_LUA_SUBCLASS(Teleporter, BfObject);

/** 
 *  @luafunc Teleporter::addDest(dest)
 *  @brief Adds a destination to the teleporter.
 *  @param dest - A point or coordinate pair representing the location of the destination.
 *
 *  Example:
 *  @code 
 *    t = Teleporter.new()
 *    t:addDest(100,150)
 *    levelgen:addItem(t)  -- or plugin:addItem(t) in a plugin
 *  @endcode
 */
S32 Teleporter::lua_addDest(lua_State *L)
{
   checkArgList(L, functionArgs, "Teleporter", "addDest");

   Point point = getPointOrXY(L, 1);
   addDest(point);

   return 0;
}


/**
  *  @luafunc Teleporter::delDest(index)
  *  @brief Removes a destination from the teleporter.
  *  @param index - The index of the destination to delete. If you specify
  *         an invalid index, will generate an error.
  *  @note  Remember that in Lua, indices start with 1!
  */
S32 Teleporter::lua_delDest(lua_State *L)
{
   checkArgList(L, functionArgs, "Teleporter", "delDest");

   S32 index = getInt(L, 1, "Teleporter:delDest()", 1, mDestManager.getDestCount());

   index--;    // Adjust for Lua's 1-based index

   mDestManager.delDest(index);

   return 0;
}


/**
  *  @luafunc Teleporter::clearDests()
  *  @brief Removes all destinations from the teleporter.
  */
S32 Teleporter::lua_clearDests(lua_State *L)
{
   checkArgList(L, functionArgs, "Teleporter", "clearDests");

   mDestManager.clear();
   return 0;
}


/**
  *  @luafunc point Teleporter::getDest(index)
  *  @brief   Returns the specified destination.
  *  @param   index - Index of the dest to return.  Will generate an error if index is invalid.
  *  @return  A point object representing the requested destination.
  */
S32 Teleporter::lua_getDest(lua_State *L)
{
   checkArgList(L, functionArgs, "Teleporter", "getDest");
   S32 index = getInt(L, 1) - 1;    // - 1 corrects for Lua indices starting at 1

   if(index < 0 || index >= mDestManager.getDestCount())
      throw LuaException("Index out of range (requested " + itos(index) + ")");

   return returnPoint(L, mDestManager.getDest(index));
}


/**
  *  @luafunc int Teleporter::getDestCount()
  *  @brief   Returns the number of destinations this teleporter has.
  *  @return  The number of destinations this teleporter has.
  */
S32 Teleporter::lua_getDestCount(lua_State *L)
{
   return returnInt(L, mDestManager.getDestCount());
}


// Overrides

/**
  *  @luafunc Teleporter::setGeom(geometry)
  *  @brief   Sets teleporter geometry; differs from standard conventions.
  *  @descr   In this case, geometry represents both %Teleporter's location and those of all destinations.
  *           The first point specified will be used to set the location.  All existing destinations will be
  *           deleted, and each subsequent point will be used to define a new destination.
  *
  *  Note that in the editor, teleporters can only have a single destination.  Since scripts can add or modify editor items,
  *  when the script has finished running, all affected teleporters will be converted into a series of single destination 
  *  items, all having the same location but with different destinations.
  *
  *  If the teleporter has no destinations, it will not be added to the editor.
  *  @param   \e Geom geometry: New geometry for %Teleporter.
  */
S32 Teleporter::lua_setGeom(lua_State *L)
{
   checkArgList(L, functionArgs, "Teleporter", "setGeom");

   doSetGeom(L);

   //clearStack(L);    // Why do we need this?
   return 0;
}


// Helper for Lua constructor and setGeom(L) methods
void Teleporter::doSetGeom(lua_State *L)
{
   mDestManager.clear();      // Any existing destinations are toast

   Vector<Point> points = getPointsOrXYs(L, 1);

   setVert(points[0], 0);     // Origin

   if(points.size() >= 2)     
   {
      setVert(points[1], 1);  // Dest

      // Notify destination manager about the new destinations
      for(S32 i = 1; i < points.size(); i++)
         addDest(points[i]);
   }
   else
      setVert(Point(0,0), 1);  // Set default dest -- maybe not important to set this

   computeExtent();
}


/**
  *  @luafunc Geom Teleporter::getGeom()
  *  @brief   Gets teleporter geometry; differs from standard conventions.
  *  @descr   In this case, geometry represents both %Teleporter's location and those of all destinations.
  *           The first point in the Geom will be the %teleporter's intake location.  Each destination will be represented by
  *           an additional point.
  *
  *  In the editor, all teleporters are simple lines, and will return geometries with two points -- an origin and a destination.
  *  @param   \e Geom geometry: New geometry for %Teleporter.
  */
S32 Teleporter::lua_getGeom(lua_State *L)
{
   Vector<Point> points;

   points.push_back(getPos());

   for(S32 i = 0; i < mDestManager.getDestCount(); i++)
      points.push_back(mDestManager.getDest(i));

   return returnPoints(L, &points);
}

   
};

