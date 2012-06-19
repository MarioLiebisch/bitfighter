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

#include "EngineeredItem.h"
#include "projectile.h"
#include "gameType.h"
#include "gameWeapons.h"
#include "SoundSystem.h"
#include "gameObjectRender.h"
#include "GeomUtils.h"
#include "BotNavMeshZone.h"
#include "gameConnection.h"
#include "WallSegmentManager.h"
#include "ClientInfo.h"
#include "teleporter.h"


#ifndef ZAP_DEDICATED
#  include "UIEditorMenus.h"       // For EditorAttributeMenuUI def
#  include "ClientGame.h"          // for accessing client's spark manager
#endif


#include "Colors.h"
#include "stringUtils.h"

#include <math.h>

namespace Zap
{

// Statics:
#ifndef ZAP_DEDICATED
   EditorAttributeMenuUI *EngineeredItem::mAttributeMenuUI = NULL;
#endif

static bool forceFieldEdgesIntersectPoints(const Vector<Point> &points, const Vector<Point> forceField)
{
   return polygonIntersectsSegment(points, forceField[0], forceField[1]) || polygonIntersectsSegment(points, forceField[2], forceField[3]);
}


// Constructor
Engineerable::Engineerable()
{
   mEngineered = false;
}

// Destructor
Engineerable::~Engineerable()
{
   // Do nothing
}


void Engineerable::setEngineered(bool isEngineered)
{
   mEngineered = isEngineered;
}


bool Engineerable::isEngineered()
{
   // If the engineered item has a resource attached, then it was engineered by a player
   return mEngineered;
}


void Engineerable::setResource(MoveItem *resource)
{
   TNLAssert(resource->isMounted() == false, "Doh!");
   mResource = resource;
   mResource->removeFromDatabase();
}


////////////////////////////////////////
////////////////////////////////////////

// Returns true if deploy point is valid, false otherwise.  deployPosition and deployNormal are populated if successful.
bool EngineerModuleDeployer::findDeployPoint(Ship *ship, U32 objectType, Point &deployPosition, Point &deployNormal)
{
   if(objectType == EngineeredTurret || objectType == EngineeredForceField)
   {
         // Ship must be within Ship::MaxEngineerDistance of a wall, pointing at where the object should be placed
         Point startPoint = ship->getActualPos();
         Point endPoint = startPoint + ship->getAimVector() * Ship::MaxEngineerDistance;

         F32 collisionTime;

         BfObject *hitObject = ship->findObjectLOS((TestFunc)isWallType, ActualState, startPoint, endPoint,
                                                     collisionTime, deployNormal);

         if(!hitObject)    // No appropriate walls found, can't deploy, sorry!
            return false;


         if(deployNormal.dot(ship->getAimVector()) > 0)
            deployNormal = -deployNormal;      // This is to fix deploy at wrong side of barrier.

         // Set deploy point, and move one unit away from the wall (this is a tiny amount, keeps linework from overlapping with wall)
         deployPosition.set(startPoint + (endPoint - startPoint) * collisionTime + deployNormal);
   }
   else if(objectType == EngineeredTeleportEntrance || objectType == EngineeredTeleportExit)
      deployPosition.set(ship->getActualPos() + (ship->getAimVector() * (Ship::CollisionRadius + Teleporter::TELEPORTER_RADIUS)));

   return true;
}


// Check for sufficient energy and resources; return empty string if everything is ok
string EngineerModuleDeployer::checkResourcesAndEnergy(Ship *ship)
{
   if(!ship->isCarryingItem(ResourceItemTypeNumber))
      return "!!! Need resource item to use Engineer module";

   if(ship->getEnergy() < ship->getGame()->getModuleInfo(ModuleEngineer)->getPrimaryPerUseCost())
      return "!!! Not enough energy to engineer an object";

   return "";
}


// Returns "" if location is OK, otherwise returns an error message
// Runs on client and server
bool EngineerModuleDeployer::canCreateObjectAtLocation(GridDatabase *gameObjectDatabase, Ship *ship, U32 objectType)
{
   string msg;

   // Everything needs energy and a resource, except the teleport exit
   if(objectType != EngineeredTeleportExit)
      mErrorMessage = checkResourcesAndEnergy(ship);

   if(mErrorMessage != "")
      return false;

   if(!findDeployPoint(ship, objectType, mDeployPosition, mDeployNormal))
   {
      mErrorMessage = "!!! Could not find a suitable wall for mounting the item";
      return false;
   }

   Vector<Point> bounds;
   bool goodDeploymentPosition = false;

   // Seems inefficient to construct these just for the purpose of bounds checking...
   switch(objectType)
   {
      case EngineeredTurret:
         Turret::getTurretGeometry(mDeployPosition, mDeployNormal, bounds);   
         goodDeploymentPosition = EngineeredItem::checkDeploymentPosition(bounds, gameObjectDatabase);
         break;
      case EngineeredForceField:
         ForceFieldProjector::getForceFieldProjectorGeometry(mDeployPosition, mDeployNormal, bounds);
         goodDeploymentPosition = EngineeredItem::checkDeploymentPosition(bounds, gameObjectDatabase);
         break;
      case EngineeredTeleportEntrance:
      case EngineeredTeleportExit:
         goodDeploymentPosition = Teleporter::checkDeploymentPosition(mDeployPosition, gameObjectDatabase);
         break;
      default:    // will never happen
         TNLAssert(false, "Bad objectType");
         return false;
   }

   if(!goodDeploymentPosition)
   {
      mErrorMessage = "!!! Cannot deploy item at this location";
      return false;
   }

   // If this is anything but a forcefield, then we're good to go!
   if(objectType != EngineeredForceField)
      return true;


   // Forcefields only from here on down; we've got miles to go before we sleep

   /// Part ONE
   // We need to ensure forcefield doesn't cross another; doing so can create an impossible situation
   // Forcefield starts at the end of the projector.  Need to know where that is.
   Point forceFieldStart = ForceFieldProjector::getForceFieldStartPoint(mDeployPosition, mDeployNormal, 0);

   // Now we can find the point where the forcefield would end if this were a valid position
   Point forceFieldEnd;
   DatabaseObject *terminatingWallObject;
   ForceField::findForceFieldEnd(gameObjectDatabase, forceFieldStart, mDeployNormal, forceFieldEnd, &terminatingWallObject);

   bool collision = false;

   // Check for collisions with existing projectors
   Rect queryRect(forceFieldStart, forceFieldEnd);
   queryRect.expand(Point(5,5));    // Just a touch bigger than the bare minimum

   Vector<Point> candidateForceFieldGeom;
   ForceField::getGeom(forceFieldStart, forceFieldEnd, candidateForceFieldGeom);

   fillVector.clear();
   gameObjectDatabase->findObjects(ForceFieldProjectorTypeNumber, fillVector, queryRect);

   Vector<Point> ffpGeom;     // Geom of any projectors we find

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      ForceFieldProjector *ffp = dynamic_cast<ForceFieldProjector *>(fillVector[i]);
      if(ffp)
      {
         ffpGeom.clear();
         ffp->getCollisionPoly(ffpGeom);
         if(forceFieldEdgesIntersectPoints(ffpGeom, candidateForceFieldGeom))
         {
            collision = true;
            break;
         }
      }
   }
   
   if(!collision)
   {
      // Check for collision with forcefields that could be projected from those projectors.
      // Projectors up to two forcefield lengths away must be considered because the end of 
      // one could intersect the end of the other.
      fillVector.clear();
      queryRect.expand(Point(ForceField::MAX_FORCEFIELD_LENGTH, ForceField::MAX_FORCEFIELD_LENGTH));
      gameObjectDatabase->findObjects(ForceFieldProjectorTypeNumber, fillVector, queryRect);

      // Reusable containers for holding geom of any forcefields we might need to check for intersection with our candidate
      Point start, end;
      Vector<Point> ffGeom;

      for(S32 i = 0; i < fillVector.size(); i++)
      {
         ForceFieldProjector *proj = dynamic_cast<ForceFieldProjector *>(fillVector[i]);
         if(proj)
         {
            proj->getForceFieldStartAndEndPoints(start, end);

            ffGeom.clear();
            ForceField::getGeom(start, end, ffGeom);

            if(forceFieldEdgesIntersectPoints(candidateForceFieldGeom, ffGeom))
            {
               collision = true;
               break;
            }
         }
      }
   }

   if(collision)
   {
      mErrorMessage = "!!! Cannot deply forcefield where it could cross another.";
      return false;
   }


   /// Part TWO - preventative abuse measures

   // First thing first, is abusive engineer allowed?  If so, let's get out of here
   if(ship->getGame()->getGameType()->isEngineerUnrestrictedEnabled())
      return true;

   // Continuing on..  let's check to make sure that forcefield doesn't come within a ship's
   // width of a wall; this should really squelch the forcefield abuse
   bool wallTooClose = false;
   fillVector.clear();

   // Build collision poly from forcefield and ship's width
   // Similar to expanding a barrier spine
   Vector<Point> collisionPoly;
   Point dir = forceFieldEnd - forceFieldStart;

   Point crossVec(dir.y, -dir.x);
   crossVec.normalize(2 * Ship::CollisionRadius + ForceField::ForceFieldHalfWidth);

   collisionPoly.push_back(forceFieldStart + crossVec);
   collisionPoly.push_back(forceFieldEnd + crossVec);
   collisionPoly.push_back(forceFieldEnd - crossVec);
   collisionPoly.push_back(forceFieldStart - crossVec);

   // Reset query rect
   queryRect = Rect(collisionPoly);

   // Search for wall segments within query
   gameObjectDatabase->findObjects(isWallType, fillVector, queryRect);

   Vector<Point> currentPoly;
   for(S32 i = 0; i < fillVector.size(); i++)
   {
      // Exclude the end segment from our search
      if(terminatingWallObject && terminatingWallObject == fillVector[i])
         continue;

      currentPoly.clear();
      fillVector[i]->getCollisionPoly(currentPoly);

      if(polygonsIntersect(currentPoly, collisionPoly))
      {
         wallTooClose = true;
         break;
      }
   }

   if(wallTooClose)
   {
      mErrorMessage = "!!! Cannot deploy forcefield where it will pass too close to a wall";
      return false;
   }


   /// Part THREE
   // Now we should check for any turrets that may be in the way using the same geometry as in
   // part two.  We can excluded engineered turrets because they can be destroyed
   bool turretInTheWay = false;
   gameObjectDatabase->findObjects(TurretTypeNumber, fillVector, queryRect);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      Turret *turret = dynamic_cast<Turret *>(fillVector[i]);

      if(!turret)
         continue;

      // We don't care about engineered turrets because they can be destroyed
      if(turret->isEngineered())
         continue;

      currentPoly.clear();
      turret->getCollisionPoly(currentPoly);

      if(polygonsIntersect(currentPoly, collisionPoly))
      {
         turretInTheWay = true;
         break;
      }
   }

   if(turretInTheWay)
   {
      mErrorMessage = "!!! Cannot deploy forcefield over a non-destructible turret";
      return false;
   }

   return true;     // We've run the gammut -- this location is OK
}


// Runs on server
// Only run after canCreateObjectAtLocation, which checks for errors and sets mDeployPosition
bool EngineerModuleDeployer::deployEngineeredItem(ClientInfo *clientInfo, U32 objectType)
{
   // Do some basic crash-proofing sanity checks first
   Ship *ship = dynamic_cast<Ship *>(clientInfo->getShip());
   if(!ship)
      return false;

   BfObject *deployedObject = NULL;

   switch(objectType)
   {
      case EngineeredTurret:
         deployedObject = new Turret(ship->getTeam(), mDeployPosition, mDeployNormal);    // Deploy pos/norm calc'ed in canCreateObjectAtLocation()
         break;

      case EngineeredForceField:
         deployedObject = new ForceFieldProjector(ship->getTeam(), mDeployPosition, mDeployNormal);
         break;

      case EngineeredTeleportEntrance:
         deployedObject = new Teleporter(mDeployPosition, mDeployPosition);
         ship->setEngineeredTeleport(static_cast<Teleporter*>(deployedObject));
         ship->disableWeaponsAndModules(true);
         break;

      case EngineeredTeleportExit:
         if(ship->getEngineeredTeleport() && ship->getEngineeredTeleport()->needsEndpoint())
         {
            ship->getEngineeredTeleport()->setEndpoint(mDeployPosition);
            ship->setEngineeredTeleport(NULL);   // Clear out the attached teleporter
            ship->disableWeaponsAndModules(false);
         }
         else   // Something went wrong
            return false;

         return true;
         break;

      default:
         return false;
   }


   Engineerable *engineerable = dynamic_cast<Engineerable *>(deployedObject);

   if((!deployedObject || !engineerable) && !clientInfo->isRobot())              // Something went wrong
   {
      clientInfo->getConnection()->s2cDisplayErrorMessage("Error deploying object.");
      delete deployedObject;
      return false;
   }

   ship->engineerBuildObject();     // Deducts energy

   deployedObject->setOwner(clientInfo);

   engineerable->computeExtent();   // Recomputes extents

   deployedObject->addToGame(ship->getGame(), ship->getGame()->getGameObjDatabase());

   engineerable->onConstructed();

   MoveItem *resource = ship->unmountItem(ResourceItemTypeNumber);

   engineerable->setResource(resource);
   engineerable->setEngineered(true);

   return true;
}


string EngineerModuleDeployer::getErrorMessage()
{
   return mErrorMessage;
}


////////////////////////////////////////
////////////////////////////////////////


// Constructor
EngineeredItem::EngineeredItem(S32 team, Point anchorPoint, Point anchorNormal) : Engineerable(), mAnchorNormal(anchorNormal)
{
   setPos(anchorPoint);
   mHealth = 1.0f;
   setTeam(team);
   mOriginalTeam = team;
   mIsDestroyed = false;
   mHealRate = 0;
   mMountSeg = NULL;
   mSnapped = false;

   mRadius = 7;

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
EngineeredItem::~EngineeredItem()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


bool EngineeredItem::processArguments(S32 argc, const char **argv, Game *game)
{
   if(argc < 3)
      return false;

   setTeam(atoi(argv[0]));
   mOriginalTeam = getTeam();
   if(mOriginalTeam == TEAM_NEUTRAL)      // Neutral object starts with no health and can be repaired and claimed by anyone
      mHealth = 0;
   
   Point pos;
   pos.read(argv + 1);
   pos *= game->getGridSize();

   if(argc >= 4)
   {
      mHealRate = atoi(argv[3]);
      mHealTimer.setPeriod(mHealRate * 1000);
   }

   // Find the mount point:
   Point normal, anchor;

   // Anchor objects to the correct point
   if(!findAnchorPointAndNormal(game->getGameObjDatabase(), pos, (F32)MAX_SNAP_DISTANCE, true, anchor, normal))
   {
      setPos(pos);               // Found no mount point, but for editor, needs to set the position
      mAnchorNormal.set(1,0);
   }
   else
   {
      setPos(anchor + normal);
      mAnchorNormal.set(normal);
   }
   
   computeObjectGeometry();                                    // Fills mCollisionPolyPoints 
   computeExtent();                                            // Uses mCollisionPolyPoints
   computeBufferForBotZone(mBufferedObjectPointsForBotZone);   // Fill mBufferedObjectPointsForBotZone

   return true;
}


void EngineeredItem::computeObjectGeometry()
{
   getObjectGeometry(getPos(), mAnchorNormal, mCollisionPolyPoints);
}


F32 EngineeredItem::getSelectionOffsetMagnitude()
{
   TNLAssert(false, "Not implemented");
   return 0;
}


// Server only
void EngineeredItem::computeBufferForBotZone(Vector<Point> &zonePoints)
{
   Vector<Point> inputPoints;

   getCollisionPoly(inputPoints);

   if(isWoundClockwise(inputPoints))
      inputPoints.reverse();

   // Fill zonePoints
   offsetPolygon(&inputPoints, zonePoints, (F32)BotNavMeshZone::BufferRadius);
}



void EngineeredItem::onAddedToGame(Game *game)
{
   Parent::onAddedToGame(game);

   if(mHealth != 0)
      onEnabled();
}


string EngineeredItem::toString(F32 gridSize) const
{
   return string(Object::getClassName()) + " " + itos(getTeam()) + " " + geomToString(gridSize) + " " + itos(mHealRate);
}


#ifndef ZAP_DEDICATED

EditorAttributeMenuUI *EngineeredItem::getAttributeMenu()
{
   // Lazily initialize this -- if we're in the game, we'll never need this to be instantiated
   if(!mAttributeMenuUI)
   {
      ClientGame *clientGame = static_cast<ClientGame *>(getGame());
      mAttributeMenuUI = new EditorAttributeMenuUI(clientGame);

      // Value doesn't matter (set to 99 here), as it will be clobbered when startEditingAttrs() is called
      CounterMenuItem *menuItem = new CounterMenuItem("10% Heal:", 99, 1, 0, 100, "secs", "Disabled", 
                                                      "Time for this item to heal itself 10%");
      mAttributeMenuUI->addMenuItem(menuItem);

      // Add our standard save and exit option to the menu
      mAttributeMenuUI->addSaveAndQuitMenuItem();
   }

   return mAttributeMenuUI;
}


// Get the menu looking like what we want
void EngineeredItem::startEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   attributeMenu->getMenuItem(0)->setIntValue(mHealRate);
}


// Retrieve the values we need from the menu
void EngineeredItem::doneEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   mHealRate = attributeMenu->getMenuItem(0)->getIntValue();
}
#endif


void EngineeredItem::onGeomChanged()
{
   getObjectGeometry(getPos(), mAnchorNormal, mCollisionPolyPoints);     // Recompute collision poly
   Parent::onGeomChanged();
}

#ifndef ZAP_DEDICATED
Point EngineeredItem::getEditorSelectionOffset(F32 currentScale)
{
   if(!mSnapped)
      return Parent::getEditorSelectionOffset(currentScale);

   F32 m = getSelectionOffsetMagnitude();

   Point cross(mAnchorNormal.y, -mAnchorNormal.x);
   F32 ang = cross.ATAN2();

   F32 x = -m * sin(ang);
   F32 y = m * cos(ang);

   return Point(x,y);
}


// Render some attributes when item is selected but not being edited
string EngineeredItem::getAttributeString()
{
   return "10% Heal: " + (mHealRate == 0 ? "Disabled" : itos(mHealRate) + " sec" + ( mHealRate != 1 ? "s" : ""));      
}

#endif


// This is used for both positioning items in-game and for snapping them to walls in the editor --> static method
// Polulates anchor and normal
DatabaseObject *EngineeredItem::findAnchorPointAndNormal(GridDatabase *wallEdgeDatabase, const Point &pos, F32 snapDist, 
                                                           bool format, Point &anchor, Point &normal)
{
   return findAnchorPointAndNormal(wallEdgeDatabase, pos, snapDist, format, (TestFunc)isWallType, anchor, normal);
}


DatabaseObject *EngineeredItem::findAnchorPointAndNormal(GridDatabase *wallEdgeDatabase, const Point &pos, F32 snapDist, 
                                                           bool format, TestFunc testFunc, Point &anchor, Point &normal)
{
   F32 minDist = F32_MAX;
   DatabaseObject *closestWall = NULL;

   Point n;    // Reused in loop below
   F32 t;

   // Start with a sweep of the area
   for(F32 theta = 0; theta < Float2Pi; theta += FloatPi * 0.125f)   // Reducing to 0.0125 seems to have no effect
   {
      Point dir(cos(theta), sin(theta));
      dir *= snapDist;
      Point mountPos = pos - dir * 0.001f;  // Offsetting slightly prevents spazzy behavior in editor
      
      // Look for walls
      DatabaseObject *wall = wallEdgeDatabase->findObjectLOS(testFunc, ActualState, format, mountPos, mountPos + dir, t, n);

      if(wall != NULL)     // Found one!
      {
         if(t < minDist)
         {
            anchor.set(mountPos + dir * t);
            normal.set(n);
            minDist = t;
            closestWall = wall;
         }
      }
   }

   return closestWall;
}


void EngineeredItem::setAnchorNormal(const Point &nrml)
{
   mAnchorNormal = nrml;
}


WallSegment *EngineeredItem::getMountSegment()
{
   return mMountSeg;
}


void EngineeredItem::setMountSegment(WallSegment *mountSeg)
{
   mMountSeg = mountSeg;
}


WallSegment *EngineeredItem::getEndSegment()
{
   return NULL;
}


void EngineeredItem::setEndSegment(WallSegment *endSegment)
{
   // Do nothing
}


void EngineeredItem::setSnapped(bool snapped)
{
   mSnapped = snapped;
}


static const F32 disabledLevel = 0.25;

bool EngineeredItem::isEnabled()
{
   return mHealth >= disabledLevel;
}


void EngineeredItem::damageObject(DamageInfo *di)
{
   F32 prevHealth = mHealth;

   if(di->damageAmount > 0)
      mHealth -= di->damageAmount * .25f; // ???
   else
      mHealth -= di->damageAmount;

   if(mHealth < 0)
      mHealth = 0;
   else if(mHealth > 1)
      mHealth = 1;

   mHealTimer.reset();     // Restart healing timer...

   // No additional damage, nothing more to do (i.e. was already at 0)
   if(prevHealth == mHealth)
      return;

   setMaskBits(HealthMask);

   // Check if turret just died
   if(prevHealth >= disabledLevel && mHealth < disabledLevel)        // Turret just died
   {
      // Revert team to neutral if this was a repaired turret
      if(getTeam() != mOriginalTeam)
      {
         setTeam(mOriginalTeam);
         setMaskBits(TeamMask);
      }
      onDisabled();

      // Handle scoring
      if(isTurret() && di->damagingObject && di->damagingObject->getOwner())
      {
         ClientInfo *player = di->damagingObject->getOwner();
         GameType *gt = getGame()->getGameType();

         if(gt->isTeamGame() && player->getTeamIndex() == getTeam())
            gt->updateScore(player, GameType::KillOwnTurret);
         else
            gt->updateScore(player, GameType::KillEnemyTurret);
      }
   }
   else if(prevHealth < disabledLevel && mHealth >= disabledLevel)   // Turret was just repaired or healed
   {
      if(getTeam() == TEAM_NEUTRAL)                   // Neutral objects...
      {
         if(di->damagingObject)
         {
            setTeam(di->damagingObject->getTeam());   // ...join the team of their repairer
            setMaskBits(TeamMask);                    // Broadcast new team status
         }
      }
      onEnabled();
   }

   if(mHealth == 0 && mResource.isValid())
   {
      mIsDestroyed = true;
      onDestroyed();

      mResource->addToDatabase(getGame()->getGameObjDatabase());
      mResource->setPos(getPos() + mAnchorNormal * mResource->getRadius());

      deleteObject(500);
   }
}


bool EngineeredItem::collide(BfObject *hitObject)
{
   return true;
}


F32 EngineeredItem::getHealth()
{
   return mHealth;
}


void EngineeredItem::computeExtent()
{
   Vector<Point> v;
   getCollisionPoly(v);

   setExtent(Rect(v));
}


void EngineeredItem::onConstructed()
{
   onEnabled();
}


void EngineeredItem::onDestroyed()
{
   // Do nothing
}


void EngineeredItem::onDisabled()
{
   // Do nothing
}


void EngineeredItem::onEnabled()
{
   // Do nothing
}


bool EngineeredItem::isTurret()
{
   return false;
}


void EngineeredItem::getObjectGeometry(const Point &anchor, const Point &normal, Vector<Point> &geom) const
{
   TNLAssert(false, "function not implemented!");
}


void EngineeredItem::setPos(Point p)
{
   Parent::setPos(p);
   computeExtent();           // Sets extent based on actual geometry of object
}


void EngineeredItem::explode()
{
#ifndef ZAP_DEDICATED
   const S32 EXPLOSION_COLOR_COUNT = 12;

   static Color ExplosionColors[EXPLOSION_COLOR_COUNT] = {
      Colors::red,
      Color(0.9, 0.5, 0),
      Colors::white,
      Colors::yellow,
      Colors::red,
      Color(0.8, 1.0, 0),
      Colors::orange50,
      Colors::white,
      Colors::red,
      Color(0.9, 0.5, 0),
      Colors::white,
      Colors::yellow,
   };

   SoundSystem::playSoundEffect(SFXShipExplode, getPos());

   F32 a = TNL::Random::readF() * 0.4f + 0.5f;
   F32 b = TNL::Random::readF() * 0.2f + 0.9f;
   F32 c = TNL::Random::readF() * 0.15f + 0.125f;
   F32 d = TNL::Random::readF() * 0.2f + 0.9f;

   TNLAssert(dynamic_cast<ClientGame *>(getGame()) != NULL, "Not a ClientGame");

   ClientGame *game = static_cast<ClientGame *>(getGame());

   Point pos = getPos();

   game->emitExplosion(pos, 0.65f, ExplosionColors, EXPLOSION_COLOR_COUNT);
   game->emitBurst(pos, Point(a,c) * 0.6f, Color(1, 1, 0.25), Colors::red);
   game->emitBurst(pos, Point(b,d) * 0.6f, Colors::yellow, Colors::yellow);

   disableCollision();
#endif
}


bool EngineeredItem::isDestroyed()
{
   return mIsDestroyed;
}


// Make sure position looks good when player deploys item with Engineer module -- make sure we're not deploying on top of
// a wall or another engineered item
// static method
bool EngineeredItem::checkDeploymentPosition(const Vector<Point> &thisBounds, GridDatabase *gb)
{
   Vector<DatabaseObject *> foundObjects;
   Rect queryRect(thisBounds);
   gb->findObjects((TestFunc) isForceFieldCollideableType, foundObjects, queryRect);

   for(S32 i = 0; i < foundObjects.size(); i++)
   {
      Vector<Point> foundObjectBounds;
      dynamic_cast<BfObject *>(foundObjects[i])->getCollisionPoly(foundObjectBounds);

      if(polygonsIntersect(thisBounds, foundObjectBounds))     // Do they intersect?
         return false;     // Bad location
   }
   return true;            // Good location
}


U32 EngineeredItem::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   if(stream->writeFlag(updateMask & InitialMask))
   {
      Point pos = getPos();

      stream->write(pos.x);
      stream->write(pos.y);
      stream->write(mAnchorNormal.x);
      stream->write(mAnchorNormal.y);
      stream->writeFlag(mEngineered);
   }

   if(stream->writeFlag(updateMask & TeamMask))
      writeThisTeam(stream);

   if(stream->writeFlag(updateMask & HealthMask))
   {
      if(stream->writeFlag(isEnabled()))
         stream->writeFloat((mHealth - disabledLevel) / (1 - disabledLevel), 5);
      else
         stream->writeFloat(mHealth / disabledLevel, 5);

      stream->writeFlag(mIsDestroyed);
   }
   return 0;
}


void EngineeredItem::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   bool initial = false;

   if(stream->readFlag())
   {
      Point pos;
      initial = true;
      stream->read(&pos.x);
      stream->read(&pos.y);
      stream->read(&mAnchorNormal.x);
      stream->read(&mAnchorNormal.y);
      mEngineered = stream->readFlag();
      setPos(pos);
   }


   if(stream->readFlag())
      readThisTeam(stream);

   if(stream->readFlag())
   {
      if(stream->readFlag())
         mHealth = stream->readFloat(5) * (1 - disabledLevel) + disabledLevel; // enabled
      else
         mHealth = stream->readFloat(5) * (disabledLevel * 0.99f); // disabled, make sure (mHealth < disabledLevel)


      bool wasDestroyed = mIsDestroyed;
      mIsDestroyed = stream->readFlag();

      if(mIsDestroyed && !wasDestroyed && !initial)
         explode();
   }

   if(initial)
   {
      computeObjectGeometry();
      computeExtent();
   }
}


void EngineeredItem::healObject(S32 time)
{
   if(mHealRate == 0 || getTeam() == TEAM_NEUTRAL)      // Neutral items don't heal!
      return;

   F32 prevHealth = mHealth;

   if(mHealTimer.update(time))
   {
      mHealth += .1f;
      setMaskBits(HealthMask);

      if(mHealth >= 1)
         mHealth = 1;
      else
         mHealTimer.reset();

      if(prevHealth < disabledLevel && mHealth >= disabledLevel)
         onEnabled();
   }
}


// Server only
const Vector<Point> *EngineeredItem::getBufferForBotZone()
{
   return &mBufferedObjectPointsForBotZone;
}


// Find mount point or turret or forcefield closest to pos
Point EngineeredItem::mountToWall(const Point &pos, WallSegmentManager *wallSegmentManager)
{  
   Point anchor, nrml;
   DatabaseObject *mountEdge = NULL, *mountSeg = NULL;

   // First we snap to a wall edge -- this will ensure we don't end up attaching to an interior wall segment in the case of a wall intersection.
   // That will determine our location, but we also need to figure out which segment we're attaching to so that if that segment were to move,
   // we could update or item accordingly.  Unfortunately, there is no direct way to associate a WallEdge with a WallSegment, but we can do
   // it indirectly by snapping again, this time to a segment in our WallSegment database.  By using the snap point we found initially, that will
   // ensure the segment we find is associated with the edge found in the first pass.
   mountEdge = findAnchorPointAndNormal(wallSegmentManager->getWallEdgeDatabase(), pos, 
                               (F32)EngineeredItem::MAX_SNAP_DISTANCE, false, (TestFunc)isWallType, anchor, nrml);

   if(mountEdge)
   {
      Point p;
      p.interp(.1f, pos, anchor);    // Backing off just a bit makes things much less spazzy.  10% seems to work well.

      mountSeg = findAnchorPointAndNormal(wallSegmentManager->getWallSegmentDatabase(), p,   
                        (F32)EngineeredItem::MAX_SNAP_DISTANCE, false, (TestFunc)isWallType, anchor, nrml);
   }

   // Can find an edge but not a segment while a wall is being dragged -- the edge remains in it's original location while the
   // segment is some distance away
   if(mountSeg)   // Found a segment we can mount to
   {
      setPos(anchor);
      setAnchorNormal(nrml);
      setMountSegment(dynamic_cast<WallSegment *>(mountSeg));

      mSnapped = true;
      onGeomChanged();

      return anchor;
   }
   else           // No suitable segments found
   {
      mSnapped = false;
      onGeomChanged();

      return pos;
   }
}


///// Lua interface
REGISTER_LUA_SUBCLASS(EngineeredItem, Item);

const char *EngineeredItem::luaClassName = "EngineeredItem";


const luaL_reg EngineeredItem::luaMethods[] =
{
   { "isActive",  luaW_doMethod<EngineeredItem, &EngineeredItem::isActive>  },
   { "getAngle",  luaW_doMethod<EngineeredItem, &EngineeredItem::getAngle>  },
   { "getHealth", luaW_doMethod<EngineeredItem, &EngineeredItem::getHealth> },
   { NULL, NULL }
};


S32 EngineeredItem::getHealth(lua_State *L)
{
   return returnFloat(L, mHealth);
}


S32 EngineeredItem::isActive(lua_State *L)
{
   return returnInt(L, isEnabled());
}


S32 EngineeredItem::getAngle(lua_State *L)
{
   return returnFloat(L, mAnchorNormal.ATAN2());
}


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(ForceFieldProjector);

// Constructor
ForceFieldProjector::ForceFieldProjector(S32 team, Point anchorPoint, Point anchorNormal) : Parent(team, anchorPoint, anchorNormal)
{
   mNetFlags.set(Ghostable);
   mObjectTypeNumber = ForceFieldProjectorTypeNumber;
   onGeomChanged(); // can't be placed on parent, as parent construtor must initalized first

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
ForceFieldProjector::~ForceFieldProjector()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


ForceFieldProjector *ForceFieldProjector::clone() const
{
   return new ForceFieldProjector(*this);
}


void ForceFieldProjector::onDisabled()
{
   if(mField.isValid())
      mField->deleteObject(0);
}


void ForceFieldProjector::idle(BfObject::IdleCallPath path)
{
   if(path != ServerIdleMainLoop)
      return;

   healObject(mCurrentMove.time);
}


static const S32 PROJECTOR_OFFSET = 15;      // Distance from wall to projector tip; thickness, if you will

F32 ForceFieldProjector::getSelectionOffsetMagnitude()
{
   return PROJECTOR_OFFSET / 3;     // Centroid of a triangle is at 1/3 its height
}


void ForceFieldProjector::getObjectGeometry(const Point &anchor, const Point &normal, Vector<Point> &geom) const
{
   geom.clear();
   getForceFieldProjectorGeometry(anchor, normal, geom);
}


// static method
void ForceFieldProjector::getForceFieldProjectorGeometry(const Point &anchor, const Point &normal, Vector<Point> &geom)
{
   static const S32 PROJECTOR_HALF_WIDTH = 12;  // Half the width of base of the projector, along the wall

   Point cross(normal.y, -normal.x);
   cross.normalize((F32)PROJECTOR_HALF_WIDTH);

   geom.push_back(anchor + cross);
   geom.push_back(getForceFieldStartPoint(anchor, normal));
   geom.push_back(anchor - cross);
}


// Get the point where the forcefield actually starts, as it leaves the projector; i.e. the tip of the projector.  Static method.
Point ForceFieldProjector::getForceFieldStartPoint(const Point &anchor, const Point &normal, F32 scaleFact)
{
   return Point(anchor.x + normal.x * PROJECTOR_OFFSET * scaleFact, 
                anchor.y + normal.y * PROJECTOR_OFFSET * scaleFact);
}


void ForceFieldProjector::getForceFieldStartAndEndPoints(Point &start, Point &end)
{
   Point pos = getPos();

   start = getForceFieldStartPoint(pos, mAnchorNormal);

   DatabaseObject *collObj;
   ForceField::findForceFieldEnd(getDatabase(), getForceFieldStartPoint(pos, mAnchorNormal), mAnchorNormal, end, &collObj);
}


WallSegment *ForceFieldProjector::getEndSegment()
{
   return mForceFieldEndSegment;
}


void ForceFieldProjector::setEndSegment(WallSegment *endSegment)
{
   mForceFieldEndSegment = endSegment;
}


// Forcefield projector has been turned on some how; either at the beginning of a level, or via repairing, or deploying. 
// Runs on both client and server
void ForceFieldProjector::onEnabled()
{
   // Database can be NULL here if adding a forcefield from the editor:  The editor will
   // add a new game object *without* adding it to a grid database in order to optimize
   // adding large groups of objects with copy/paste/undo/redo
   if(!getDatabase())
      return;

   if(!isGhost() && mField.isNull())  // server only, add mField only when we don't have any
   {
      Point start = getForceFieldStartPoint(getPos(), mAnchorNormal);
      Point end;
      DatabaseObject *collObj;

      ForceField::findForceFieldEnd(getDatabase(), start, mAnchorNormal, end, &collObj);

      mField = new ForceField(getTeam(), start, end);
      mField->addToGame(getGame(), getGame()->getGameObjDatabase());
   }
}


bool ForceFieldProjector::getCollisionPoly(Vector<Point> &polyPoints) const
{
   TNLAssert(mCollisionPolyPoints.size() != 0, "mCollisionPolyPoints.size() shouldn't be zero");
   polyPoints = mCollisionPolyPoints;
   return true;
}


void ForceFieldProjector::onAddedToGame(Game *theGame)
{
   Parent::onAddedToGame(theGame);
}


void ForceFieldProjector::render()
{
   renderForceFieldProjector(&mCollisionPolyPoints, getColor(), isEnabled());
}


void ForceFieldProjector::renderDock()
{
   renderSquareItem(getPos(), getColor(), 1, &Colors::white, '>');
}


void ForceFieldProjector::renderEditor(F32 currentScale, bool snappingToWallCornersEnabled)
{
#ifndef ZAP_DEDICATED
   F32 scaleFact = 1;
   const Color *color = getColor();

   if(mSnapped)
   {
      Point forceFieldStart = getForceFieldStartPoint(getPos(), mAnchorNormal, scaleFact);

      renderForceFieldProjector(&mCollisionPolyPoints, color, true);
      renderForceField(forceFieldStart, forceFieldEnd, color, true, scaleFact);
   }
   else
      renderDock();
#endif
}


const char *ForceFieldProjector::getOnScreenName()     { return "ForceFld"; }
const char *ForceFieldProjector::getOnDockName()       { return "ForceFld"; }
const char *ForceFieldProjector::getPrettyNamePlural() { return "Force Field Projectors"; }
const char *ForceFieldProjector::getEditorHelpString() { return "Creates a force field that lets only team members pass. [F]"; }


bool ForceFieldProjector::hasTeam() { return true; }
bool ForceFieldProjector::canBeHostile() { return true; }
bool ForceFieldProjector::canBeNeutral() { return true; }


void ForceFieldProjector::onItemDragging()
{
   onGeomChanged();
}


// Determine on which segment forcefield lands -- only used in the editor, wraps ForceField::findForceFieldEnd()
void ForceFieldProjector::findForceFieldEnd()
{
   // Load the corner points of a maximum-length forcefield into geom
   DatabaseObject *collObj;

   F32 scale = 1;
   
   Point start = getForceFieldStartPoint(getPos(), mAnchorNormal);

   // Pass in database containing WallSegments, returns object in collObj
   if(ForceField::findForceFieldEnd(getDatabase()->getWallSegmentManager()->getWallEdgeDatabase(), 
                                    start, mAnchorNormal, forceFieldEnd, &collObj))
   {
      setEndSegment(dynamic_cast<WallSegment *>(collObj));
   }
   else
      setEndSegment(NULL);

   Vector<Point> geom;
   ForceField::getGeom(start, forceFieldEnd, geom, scale);    
   setExtent(Rect(geom));
}


void ForceFieldProjector::onGeomChanged()
{
   if(mSnapped)
      findForceFieldEnd();

   Parent::onGeomChanged();
}


///// Lua interface
REGISTER_LUA_SUBCLASS(ForceFieldProjector, EngineeredItem);

const char *ForceFieldProjector::luaClassName = "ForceFieldProjector";


// No custom ForceFieldProjector methods
const luaL_reg ForceFieldProjector::luaMethods[] =
{
   { NULL, NULL }
};


// LuaItem methods
S32 ForceFieldProjector::getLoc(lua_State *L)
{
   return LuaObject::returnPoint(L, getPos() + mAnchorNormal * getRadius() );
}


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(ForceField);

ForceField::ForceField(S32 team, Point start, Point end)
{
   setTeam(team);
   mStart = start;
   mEnd = end;

   Rect extent(mStart, mEnd);
   extent.expand(Point(5,5));
   setExtent(extent);

   mFieldUp = true;
   mObjectTypeNumber = ForceFieldTypeNumber;
   mNetFlags.set(Ghostable);
}


bool ForceField::collide(BfObject *hitObject)
{
   if(!mFieldUp)
      return false;

   // If it's a ship that collides with this forcefield, check team to allow it through
   if(isShipType(hitObject->getObjectTypeNumber()))
   {
      if(hitObject->getTeam() == getTeam())     // Ship and force field are same team
      {
         if(!isGhost())
         {
            mFieldUp = false;
            mDownTimer.reset(FieldDownTime);
            setMaskBits(StatusMask);
         }
         return false;
      }
   }
   // If it's a flag that collides with this forcefield and we're hostile, let it through
   else if(hitObject->getObjectTypeNumber() == FlagTypeNumber)
   {
      if(getTeam() == TEAM_HOSTILE)
         return false;
      else
         return true;
   }

   return true;
}


// Returns true if two forcefields intersect
bool ForceField::intersects(ForceField *forceField)
{
   Vector<Point> thisGeom, thatGeom;

   getGeom(thisGeom);
   forceField->getGeom(thatGeom);

   return polygonsIntersect(thisGeom, thatGeom);
}


void ForceField::idle(BfObject::IdleCallPath path)
{
   if(path != ServerIdleMainLoop)
      return;

   if(mDownTimer.update(mCurrentMove.time))
   {
      // do an LOS test to see if anything is in the field:
      F32 t;
      Point n;
      if(!findObjectLOS((TestFunc)isForceFieldDeactivatingType, ActualState, mStart, mEnd, t, n))
      {
         mFieldUp = true;
         setMaskBits(StatusMask);
      }
      else
         mDownTimer.reset(10);
   }
}


U32 ForceField::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   if(stream->writeFlag(updateMask & InitialMask))
   {
      stream->write(mStart.x);
      stream->write(mStart.y);
      stream->write(mEnd.x);
      stream->write(mEnd.y);
      writeThisTeam(stream);
   }
   stream->writeFlag(mFieldUp);
   return 0;
}


void ForceField::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   bool initial = false;
   if(stream->readFlag())
   {
      initial = true;
      stream->read(&mStart.x);
      stream->read(&mStart.y);
      stream->read(&mEnd.x);
      stream->read(&mEnd.y);
      readThisTeam(stream);

      Rect extent(mStart, mEnd);
      extent.expand(Point(5,5));
      setExtent(extent);
   }
   bool wasUp = mFieldUp;
   mFieldUp = stream->readFlag();

   if(initial || (wasUp != mFieldUp))
      SoundSystem::playSoundEffect(mFieldUp ? SFXForceFieldUp : SFXForceFieldDown, mStart);
}


const F32 ForceField::ForceFieldHalfWidth = 2.5;

// static
void ForceField::getGeom(const Point &start, const Point &end, Vector<Point> &geom, F32 scaleFact)
{

   Point normal(end.y - start.y, start.x - end.x);
   normal.normalize(ForceFieldHalfWidth * scaleFact);

   geom.push_back(start + normal);
   geom.push_back(end + normal);
   geom.push_back(end - normal);
   geom.push_back(start - normal);
}


// Non-static version
void ForceField::getGeom(Vector<Point> &geom) const
{
   getGeom(mStart, mEnd, geom);
}


// Pass in a database containing walls or wallsegments
bool ForceField::findForceFieldEnd(GridDatabase *db, const Point &start, const Point &normal,  
                                   Point &end, DatabaseObject **collObj)
{
   F32 time;
   Point n;

   end.set(start.x + normal.x * MAX_FORCEFIELD_LENGTH, start.y + normal.y * MAX_FORCEFIELD_LENGTH);

   *collObj = db->findObjectLOS((TestFunc)isWallType, ActualState, start, end, time, n);

   if(*collObj)
   {
      end.set(start + (end - start) * time); 
      return true;
   }

   return false;
}


bool ForceField::getCollisionPoly(Vector<Point> &points) const
{
   getGeom(points);
   return true;
}


void ForceField::render()
{
   renderForceField(mStart, mEnd, getColor(), mFieldUp);
}


S32 ForceField::getRenderSortValue()
{
   return 0;
}


void ForceField::getForceFieldStartAndEndPoints(Point &start, Point &end)
{
   start = mStart;
   end = mEnd;
}


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(Turret);

// Constructor
Turret::Turret(S32 team, Point anchorPoint, Point anchorNormal) : EngineeredItem(team, anchorPoint, anchorNormal)
{
   mObjectTypeNumber = TurretTypeNumber;

   mWeaponFireType = WeaponTurret;
   mNetFlags.set(Ghostable);

   onGeomChanged();

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
Turret::~Turret()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


Turret *Turret::clone() const
{
   return new Turret(*this);
}


bool Turret::processArguments(S32 argc2, const char **argv2, Game *game)
{
   S32 argc1 = 0;
   const char *argv1[32];
   for(S32 i = 0; i < argc2; i++)
   {
      char firstChar = argv2[i][0];
      if((firstChar >= 'a' && firstChar <= 'z') || (firstChar >= 'A' && firstChar <= 'Z'))  // starts with a letter
      {
         if(!strncmp(argv2[i], "W=", 2))  // W= is in 015a
         {
            S32 w = 0;
            while(w < WeaponCount && stricmp(GameWeapon::weaponInfo[w].name.getString(), &argv2[i][2]))
               w++;
            if(w < WeaponCount)
               mWeaponFireType = w;
            break;
         }
      }
      else
      {
         if(argc1 < 32)
         {
            argv1[argc1] = argv2[i];
            argc1++;
         }
      }
      
   }

   bool returnBool = EngineeredItem::processArguments(argc1, argv1, game);
   mCurrentAngle = mAnchorNormal.ATAN2();
   return returnBool;
}


string Turret::toString(F32 gridSize) const
{
   string out = EngineeredItem::toString(gridSize);
   if(mWeaponFireType != WeaponTurret)
      out = out + " W=" + GameWeapon::weaponInfo[mWeaponFireType].name.getString();
   return out;
}


void Turret::getObjectGeometry(const Point &anchor, const Point &normal, Vector<Point> &geom) const
{
   geom.clear();
   getTurretGeometry(anchor, normal, geom);
}


// static method
void Turret::getTurretGeometry(const Point &anchor, const Point &normal, Vector<Point> &polyPoints)
{
   Point cross(normal.y, -normal.x);
   polyPoints.push_back(anchor + cross * 25);
   polyPoints.push_back(anchor + cross * 10 + Point(normal) * 45);
   polyPoints.push_back(anchor - cross * 10 + Point(normal) * 45);
   polyPoints.push_back(anchor - cross * 25);
}


bool Turret::getCollisionPoly(Vector<Point> &polyPoints) const
{
   polyPoints = mCollisionPolyPoints;
   return true;
}


F32 Turret::getEditorRadius(F32 currentScale)
{
   if(mSnapped)
      return 25 * currentScale;
   else 
      return Parent::getEditorRadius(currentScale);
}


F32 Turret::getSelectionOffsetMagnitude()
{
   return 20;
}


void Turret::onAddedToGame(Game *theGame)
{
   Parent::onAddedToGame(theGame);
   mCurrentAngle = mAnchorNormal.ATAN2();
}


bool Turret::isTurret()
{
   return true;
}


void Turret::render()
{
   renderTurret(getColor(), getPos(), mAnchorNormal, isEnabled(), mHealth, mCurrentAngle);
}


void Turret::renderDock()
{
   renderSquareItem(getPos(), getColor(), 1, &Colors::white, 'T');
}


void Turret::renderEditor(F32 currentScale, bool snappingToWallCornersEnabled)
{
   if(mSnapped)
      render();
   else
      renderDock();
}


U32 Turret::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 ret = Parent::packUpdate(connection, updateMask, stream);
   if(stream->writeFlag(updateMask & AimMask))
      stream->write(mCurrentAngle);

   return ret;
}


void Turret::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   Parent::unpackUpdate(connection, stream);

   if(stream->readFlag())
      stream->read(&mCurrentAngle);
}


// Choose target, aim, and, if possible, fire
void Turret::idle(IdleCallPath path)
{
   if(path != ServerIdleMainLoop)
      return;

   // Server only!

   healObject(mCurrentMove.time);

   if(!isEnabled())
      return;

   mFireTimer.update(mCurrentMove.time);

   // Choose best target:
   Point aimPos = getPos() + mAnchorNormal * TURRET_OFFSET;
   Point cross(mAnchorNormal.y, -mAnchorNormal.x);

   Rect queryRect(aimPos, aimPos);
   queryRect.unionPoint(aimPos + cross * TurretPerceptionDistance);
   queryRect.unionPoint(aimPos - cross * TurretPerceptionDistance);
   queryRect.unionPoint(aimPos + mAnchorNormal * TurretPerceptionDistance);
   fillVector.clear();
   findObjects((TestFunc)isTurretTargetType, fillVector, queryRect);    // Get all potential targets

   BfObject *bestTarget = NULL;
   F32 bestRange = F32_MAX;
   Point bestDelta;

   Point delta;

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      if(isShipType(fillVector[i]->getObjectTypeNumber()))
      {
         Ship *potential = dynamic_cast<Ship *>(fillVector[i]);

         // Is it dead or cloaked?  Carrying objects makes ship visible, except in nexus game
         if(!potential->isVisible() || potential->hasExploded)
            continue;
      }

      // Don't target mounted items (like resourceItems and flagItems)
      MoveItem *item = dynamic_cast<MoveItem *>(fillVector[i]);
      if(item && item->isMounted())
         continue;

      BfObject *potential = dynamic_cast<BfObject *>(fillVector[i]);
      if(potential->getTeam() == getTeam())     // Is target on our team?
         continue;                              // ...if so, skip it!

      // Calculate where we have to shoot to hit this...
      Point Vs = potential->getVel();
      F32 S = (F32)GameWeapon::weaponInfo[mWeaponFireType].projVelocity;
      Point d = potential->getPos() - aimPos;

// This could possibly be combined with Robot's getFiringSolution, as it's essentially the same thing
      F32 t;      // t is set in next statement
      if(!FindLowestRootInInterval(Vs.dot(Vs) - S * S, 2 * Vs.dot(d), d.dot(d), GameWeapon::weaponInfo[mWeaponFireType].projLiveTime * 0.001f, t))
         continue;

      Point leadPos = potential->getPos() + Vs * t;

      // Calculate distance
      delta = (leadPos - aimPos);

      Point angleCheck = delta;
      angleCheck.normalize();

      // Check that we're facing it...
      if(angleCheck.dot(mAnchorNormal) <= -0.1f)
         continue;

      // See if we can see it...
      Point n;
      if(findObjectLOS((TestFunc)isWallType, ActualState, aimPos, potential->getPos(), t, n))
         continue;

      // See if we're gonna clobber our own stuff...
      disableCollision();
      Point delta2 = delta;
      delta2.normalize(GameWeapon::weaponInfo[mWeaponFireType].projLiveTime * (F32)GameWeapon::weaponInfo[mWeaponFireType].projVelocity / 1000.f);
      BfObject *hitObject = findObjectLOS((TestFunc) isWithHealthType, 0, aimPos, aimPos + delta2, t, n);
      enableCollision();

      // Skip this target if there's a friendly object in the way
      if(hitObject && hitObject->getTeam() == getTeam() &&
        (hitObject->getPos() - aimPos).len() < delta.len())         
         continue;

      F32 dist = delta.len();

      if(dist < bestRange)
      {
         bestDelta  = delta;
         bestRange  = dist;
         bestTarget = potential;
      }
   }

   if(!bestTarget)      // No target, nothing to do
      return;
 
   // Aim towards the best target.  Note that if the turret is at one extreme of its range, and the target is at the other,
   // then the turret will rotate the wrong-way around to aim at the target.  If we were to detect that condition here, and
   // constrain our turret to turning the correct direction, that would be great!!
   F32 destAngle = bestDelta.ATAN2();

   F32 angleDelta = destAngle - mCurrentAngle;

   if(angleDelta > FloatPi)
      angleDelta -= Float2Pi;
   else if(angleDelta < -FloatPi)
      angleDelta += Float2Pi;

   F32 maxTurn = TurretTurnRate * mCurrentMove.time * 0.001f;

   if(angleDelta != 0)
      setMaskBits(AimMask);

   if(angleDelta > maxTurn)
      mCurrentAngle += maxTurn;
   else if(angleDelta < -maxTurn)
      mCurrentAngle -= maxTurn;
   else
   {
      mCurrentAngle = destAngle;

      if(mFireTimer.getCurrent() == 0)
      {
         bestDelta.normalize();
         Point velocity;

         // String handling in C++ is such a mess!!!
         string killer = string("got blasted by ") + getGame()->getTeamName(getTeam()).getString() + " turret";
         mKillString = killer.c_str();

         GameWeapon::createWeaponProjectiles(WeaponType(mWeaponFireType), bestDelta, aimPos, velocity, 0, mWeaponFireType == WeaponBurst ? 45.f : 35.f, this);
         mFireTimer.reset(GameWeapon::weaponInfo[mWeaponFireType].fireDelay);
      }
   }
}


const char *Turret::getOnScreenName()     { return "Turret";  }
const char *Turret::getOnDockName()       { return "Turret";  }
const char *Turret::getPrettyNamePlural() { return "Turrets"; }
const char *Turret::getEditorHelpString() { return "Creates shooting turret.  Can be on a team, neutral, or \"hostile to all\". [Y]"; }


bool Turret::hasTeam()      { return true; }
bool Turret::canBeHostile() { return true; }
bool Turret::canBeNeutral() { return true; }


void Turret::onItemDragging()
{
   onGeomChanged();
}


void Turret::onGeomChanged() 
{ 
   mCurrentAngle = mAnchorNormal.ATAN2();       // Keep turret pointed away from the wall... looks better like that!
   Parent::onGeomChanged();
}


///// Lua interface
REGISTER_LUA_SUBCLASS(Turret, EngineeredItem);

const char *Turret::luaClassName = "Turret";

// Standard methods available to all Items
const luaL_reg Turret::luaMethods[] =
{
   { "getAngleAim", luaW_doMethod<Turret, &Turret::getAngleAim> },
   { NULL, NULL }
};


S32 Turret::getAngleAim(lua_State *L)
{
   return returnFloat(L, mCurrentAngle);
}


// Override some methods
S32 Turret::getRad(lua_State *L)
{
   return returnInt(L, TURRET_OFFSET);
}


S32 Turret::getLoc(lua_State *L)
{
   return LuaObject::returnPoint(L, getPos() + mAnchorNormal * TURRET_OFFSET);
}



};

