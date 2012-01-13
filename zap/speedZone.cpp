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

#include "speedZone.h"
#include "game.h"
#include "gameObject.h"
#include "gameType.h"
#include "gameNetInterface.h"
#include "gameObjectRender.h"
#include "ship.h"
#include "SoundSystem.h"
#include "stringUtils.h"
#include "gameConnection.h"
#include "Colors.h"

#ifndef ZAP_DEDICATED
#include "ClientGame.h"
#include "UIEditorMenus.h"    // For GoFastEditorAttributeMenuUI def
#include "UI.h"
#endif


namespace Zap
{

// Needed on OS X, but cause link errors in VC++
#ifndef WIN32
const U16 SpeedZone::minSpeed;
const U16 SpeedZone::maxSpeed;
const U16 SpeedZone::defaultSpeed;
#endif

TNL_IMPLEMENT_NETOBJECT(SpeedZone);

// Statics:
#ifndef ZAP_DEDICATED
   EditorAttributeMenuUI *SpeedZone::mAttributeMenuUI = NULL;
#endif


// Constructor
SpeedZone::SpeedZone()
{
   mNetFlags.set(Ghostable);
   mObjectTypeNumber = SpeedZoneTypeNumber;

   mSpeed = defaultSpeed;
   mSnapLocation = false;     // Don't snap unless specified
   mRotateSpeed = 0;
   mUnpackInit = 0;           // Some form of counter, to know that it is a rotating speed zone
}


// Destructor
SpeedZone::~SpeedZone()
{
   // Do nothing
}


U16 SpeedZone::getSpeed()
{
   return mSpeed;
}


void SpeedZone::setSpeed(U16 speed)
{
   mSpeed = speed;
}


bool SpeedZone::getSnapping()
{
   return mSnapLocation;
}


void SpeedZone::setSnapping(bool snapping)
{
   mSnapLocation = snapping;
}


SpeedZone *SpeedZone::clone() const
{
   return new SpeedZone(*this);

   //copyAttrs(clone.get());

   //return clone;
}


//void SpeedZone::copyAttrs(SpeedZone *target)
//{
//   SimpleLine::copyAttrs(target);
//
//   target->mSpeed = mSpeed;
//   target->mSnapLocation = mSnapLocation;
//   target->mRotateSpeed = mRotateSpeed;
//}


// Take our basic inputs, pos and dir, and expand them into a three element
// vector (the three points of our triangle graphic), and compute its extent
void SpeedZone::preparePoints()
{
   generatePoints(getVert(0), getVert(1), 1, mPolyBounds);

   computeExtent();
}


// static method
void SpeedZone::generatePoints(const Point &pos, const Point &dir, F32 gridSize, Vector<Point> &points)
{
   const S32 inset = 3;
   const F32 halfWidth = SpeedZone::halfWidth;
   const F32 height = SpeedZone::height;

   points.resize(12);

   Point parallel(dir - pos);
   parallel.normalize();

   const F32 chevronThickness = height / 3;
   const F32 chevronDepth = halfWidth - inset;

   Point tip = pos + parallel * height;
   Point perpendic(pos.y - tip.y, tip.x - pos.x);
   perpendic.normalize();

   
   S32 index = 0;

   for(S32 i = 0; i < 2; i++)
   {
      F32 offset = halfWidth * 2 * i - (i * 4);

      // Red chevron
      points[index++] = pos + parallel *  (chevronThickness + offset);                                          
      points[index++] = pos + perpendic * (halfWidth-2*inset) + parallel * (inset + offset);                             //  2   3
      points[index++] = pos + perpendic * (halfWidth-2*inset) + parallel * (chevronThickness + inset + offset);          //    1    4
      points[index++] = pos + parallel *  (chevronDepth + chevronThickness + inset + offset);                            //  6    5
      points[index++] = pos - perpendic * (halfWidth-2*inset) + parallel * (chevronThickness + inset + offset);
      points[index++] = pos - perpendic * (halfWidth-2*inset) + parallel * (inset + offset);
   }
}


void SpeedZone::render()
{
   renderSpeedZone(&mPolyBounds, getGame()->getCurrentTime());
}


Color SpeedZone::getEditorRenderColor()
{
   return Colors::red;
}


void SpeedZone::renderEditorItem()
{
   render();
}


void SpeedZone::onAttrsChanging()
{
   // Do nothing
}


void SpeedZone::onGeomChanging()
{
   onGeomChanged();
}


void SpeedZone::onItemDragging()
{
   onGeomChanged();
}


void SpeedZone::onGeomChanged()   
{  
   generatePoints(getVert(0), getVert(1), 1, mPolyBounds); 
   Parent::onGeomChanged();
}


// This object should be drawn above polygons
S32 SpeedZone::getRenderSortValue()
{
   return 0;
}


// Runs on server and client
void SpeedZone::onAddedToGame(Game *theGame)
{
   Parent::onAddedToGame(theGame);

   if(!isGhost())
      setScopeAlways();    // Runs on server
   //else
      //preparePoints();     // Runs on client,preparePoints runs in unpackUpdate
}


// Bounding box for quick collision-possibility elimination
void SpeedZone::computeExtent()
{
   setExtent(Rect(mPolyBounds));
}


// More precise boundary for more precise collision detection
bool SpeedZone::getCollisionPoly(Vector<Point> &polyPoints) const
{
   polyPoints.resize(mPolyBounds.size());

   for(S32 i = 0; i < mPolyBounds.size(); i++)
      polyPoints[i] = mPolyBounds[i];

   return true;
}


// Create objects from parameters stored in level file
bool SpeedZone::processArguments(S32 argc2, const char **argv2, Game *game)
{
   S32 argc = 0;
   const char *argv[8];                // 8 is ok, SpeedZone only supports 4 numbered args

   for(S32 i = 0; i < argc2; i++)      // The idea here is to allow optional R3.5 for rotate at speed of 3.5
   {
      char firstChar = argv2[i][0];    // First character of arg

      if((firstChar >= 'a' && firstChar <= 'z') || (firstChar >= 'A' && firstChar <= 'Z'))
      {
         if(firstChar == 'R') // 015a
            mRotateSpeed = (F32)atof(&argv2[i][1]);   // using second char to handle number, "R3.4" or "R-1.7"
         else if(!strnicmp(argv2[i], "Rotate=", 7))  // 016, same as 'R', better name
            mRotateSpeed = (F32)atof(&argv2[i][7]);   // "Rotate=3.4" or "Rotate=-1.7"
         else if(!stricmp(argv2[i], "SnapEnable"))
            mSnapLocation = true;
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

   // All "special" args have been processed, now we process the standard args

   if(argc < 4)      // Need two points at a minimum, with an optional speed item
      return false;

   Point pos, dir;

   pos.read(argv);
   pos *= game->getGridSize();

   dir.read(argv + 2);
   dir *= game->getGridSize();

   // Adjust the direction point so that it also represents the tip of the triangle
   Point offset(dir - pos);
   offset.normalize();
   dir = Point(pos + offset * height);

   // Save the points we read into our geometry
   setVert(pos, 0);
   setVert(dir, 1);


   if(argc >= 5)
      mSpeed = max(minSpeed, min(maxSpeed, (U16)(atoi(argv[4]))));

   preparePoints();

   return true;
}


// Editor
string SpeedZone::toString(F32 gridSize) const
{
   string out = string(getClassName()) + " " + geomToString(gridSize) + " " + itos(mSpeed);
   if(mSnapLocation)
      out += " SnapEnable";
   if(mRotateSpeed != 0)
      out += " Rotate=" + ftos(mRotateSpeed, 4);
   return out;
}


const char *SpeedZone::getVertLabel(S32 index)
{
   return index == 0 ? "Location" : "Direction";
}


const char *SpeedZone::getInstructionMsg()
{
   return "[Enter] to edit attributes";
}


#ifndef ZAP_DEDICATED

EditorAttributeMenuUI *SpeedZone::getAttributeMenu()
{
   // Lazily initialize this -- if we're in the game, we'll never need this to be instantiated
   if(!mAttributeMenuUI)
   {
      ClientGame *clientGame = static_cast<ClientGame *>(getGame());

      mAttributeMenuUI = new EditorAttributeMenuUI(clientGame);

      mAttributeMenuUI->addMenuItem(new CounterMenuItem("Speed:", 999, 100, minSpeed, maxSpeed, "", "Really slow", ""));
      mAttributeMenuUI->addMenuItem(new YesNoMenuItem("Snapping:", true, ""));

      // Add our standard save and exit option to the menu
      mAttributeMenuUI->addSaveAndQuitMenuItem();
   }

   return mAttributeMenuUI;
}


// Get the menu looking like what we want
void SpeedZone::startEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   attributeMenu->getMenuItem(0)->setIntValue(mSpeed);
   attributeMenu->getMenuItem(1)->setIntValue(mSnapLocation ? 1 : 0);
}


// Retrieve the values we need from the menu
void SpeedZone::doneEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   mSpeed        = attributeMenu->getMenuItem(0)->getIntValue();
   mSnapLocation = attributeMenu->getMenuItem(1)->getIntValue();    // Returns 0 or 1
}


// Render some attributes when item is selected but not being edited
string SpeedZone::getAttributeString()
{
   return "Speed: " + itos(mSpeed) + "; Snap: " + (mSnapLocation ? "Yes" : "No");      
}

#endif


static bool ignoreThisCollision = false;

// Checks collisions with a SpeedZone
bool SpeedZone::collide(GameObject *hitObject)
{
   if(ignoreThisCollision)
      return false;
   // This is run on both server and client side to reduce teleport lag effect.
   if(isShipType(hitObject->getObjectTypeNumber()))     // Only ships & robots collide
   {
      MoveObject *s = dynamic_cast<MoveObject *>(hitObject);
      if(!s)
         return false;

#ifndef ZAP_DEDICATED
      if(isGhost()) // on client, don't process speedZone on all moveObjects except the controlling one.
      {
         ClientGame *client = dynamic_cast<ClientGame *>(getGame());
         if(client)
         {
            GameConnection *gc = client->getConnectionToServer();
            if(gc && gc->getControlObject() != hitObject) 
               return false;
         }
      }
#endif
      return true;
   }
   return false;
}


// Handles collisions with a SpeedZone
void SpeedZone::collided(MoveObject *s, U32 stateIndex)
{
   MoveObject::MoveState *moveState = &s->mMoveState[stateIndex];

   Point pos = getVert(0);
   Point dir = getVert(1);

   Point impulse = (dir - pos);           // Gives us direction
   impulse.normalize(mSpeed);             // Gives us the magnitude of speed
   Point shipNormal = moveState->vel;
   shipNormal.normalize(mSpeed);
   F32 angleSpeed = mSpeed * 0.5f;

   // Using mUnpackInit, as client does not know that mRotateSpeed is not zero.
   if(mSnapLocation && mRotateSpeed == 0 && mUnpackInit < 3)
      angleSpeed *= 0.01f;
   if(shipNormal.distanceTo(impulse) < angleSpeed && moveState->vel.len() > mSpeed)
      return;

   // This following line will cause ships entering the speedzone to have their location set to the same point
   // within the zone so that their path out will be very predictable.
   if(mSnapLocation)
   {
      Point diffpos = moveState->pos - pos;
      Point thisAngle = dir - pos;
      thisAngle.normalize();
      Point newPos = thisAngle * diffpos.dot(thisAngle) + pos + impulse * 0.001f;

      Point oldVel = moveState->vel;
      Point oldPos = moveState->pos;

      ignoreThisCollision = true;  // Seem to need it to ignore collide to SpeedZone during a findFirstCollision
      moveState->vel = newPos - moveState->pos;

      F32 collisionTime = 1;
      Point collisionPoint;
      s->findFirstCollision(stateIndex, collisionTime, collisionPoint);
      s->mMoveState[stateIndex].pos += s->mMoveState[stateIndex].vel * collisionTime;

      ignoreThisCollision = false;

      if(collisionTime != 1)  // don't allow using speed zone when could not line up due to going into wall?
      {
         moveState->pos = oldPos;
         moveState->vel = oldVel;
         return;
      }
      moveState->vel = impulse * 1.5;     // Why multiply by 1.5?
   }
   else
   {
      if(shipNormal.distanceTo(impulse) < mSpeed && moveState->vel.len() > mSpeed * 0.8)
         return;

      moveState->vel += impulse * 1.5;    // Why multiply by 1.5?
   }

   if(!s->isGhost() && stateIndex == MoveObject::ActualState)  // Only server needs to send information
   {
      setMaskBits(HitMask);

      // Trigger a sound on the player's machine: They're going to be so far away they'll never hear the sound emitted by the gofast itself...
      if(s->getControllingClient() && s->getControllingClient().isValid())
         s->getControllingClient()->s2cDisplayMessage(0, SFXGoFastInside, "");
   }

}


void SpeedZone::idle(GameObject::IdleCallPath path)
{
   if(mRotateSpeed != 0)
   {
      Point dir = getVert(1);

      dir -= getVert(0);
      F32 angle = dir.ATAN2();
      angle += mRotateSpeed * mCurrentMove.time * 0.001f;
      dir.setPolar(1, angle);
      dir += getVert(0);
      setMaskBits(InitMask);

      setVert(dir, 1);

      preparePoints();     // Avoids "off center" problem
   }
}


U32 SpeedZone::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   if(stream->writeFlag(updateMask & InitMask))    
   {
      Point pos = getVert(0);
      Point dir = getVert(1);

      pos.write(stream);
      dir.write(stream);

      stream->writeInt(mSpeed, 16);
      stream->writeFlag(mSnapLocation);
   }      

   return 0;
}


void SpeedZone::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   if(stream->readFlag())     // InitMask
   {
      Point pos, dir;

      mUnpackInit++;

      pos.read(stream);
      dir.read(stream);

      setVert(pos, 0);
      setVert(dir, 1);

      mSpeed = stream->readInt(16);
      mSnapLocation = stream->readFlag();

      preparePoints();
   }

   else 
      SoundSystem::playSoundEffect(SFXGoFastOutside, getVert(0), getVert(0));
}



// Some properties about the item that will be needed in the editor
const char *SpeedZone::getEditorHelpString()
{
   return "Makes ships go fast in direction of arrow. [P]";
}


const char *SpeedZone::getPrettyNamePlural()
{
   return "GoFasts";
}


const char *SpeedZone::getOnDockName()
{
   return "GoFast";
}


const char *SpeedZone::getOnScreenName()
{
   return "GoFast";
}


bool SpeedZone::hasTeam()
{
   return false;
}


bool SpeedZone::canBeHostile()
{
   return false;
}


bool SpeedZone::canBeNeutral()
{
   return false;
}


};


