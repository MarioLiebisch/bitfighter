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

#include "PickupItem.h"
#include "gameType.h"
#include "game.h"
#include "gameConnection.h"


#ifndef ZAP_DEDICATED

#include "UIEditorMenus.h"       // For EditorAttributeMenuUI def

#endif


#include "gameObjectRender.h"
#include "stringUtils.h"      // for itos()



namespace Zap
{

// Statics:
#ifndef ZAP_DEDICATED
   EditorAttributeMenuUI *PickupItem::mAttributeMenuUI = NULL;
#endif


PickupItem::PickupItem(Point p, float radius, S32 repopDelay) : Parent(p, radius)
{
   mRepopDelay = repopDelay;
   mIsVisible = true;
   mIsMomentarilyVisible = false;

   mNetFlags.set(Ghostable);
}


void PickupItem::idle(GameObject::IdleCallPath path)
{
   if(!mIsVisible && path == GameObject::ServerIdleMainLoop)
   {
      if(mRepopTimer.update(mCurrentMove.time))
      {
         setMaskBits(PickupMask);
         mIsVisible = true;

         // Check if there is a ship sitting on this item... it so, ship gets the repair!
         for(S32 i = 0; i < getGame()->getClientCount(); i++)
         {
            GameConnection *connection = getGame()->getClientInfo(i)->getConnection();

            TNLAssert(connection, "Defunct client connection in item.cpp!");
            if(!connection)    
               continue;

            Ship *ship = dynamic_cast<Ship *>(connection->getControlObject());

            if(ship && ship->isOnObject(this))
            {
               collide(ship);
               mIsMomentarilyVisible = true;
            }
         }
      }
   }

   //updateExtent();    ==> Taking this out... why do we need it for a non-moving object?  CE 8/23/11
}


bool PickupItem::processArguments(S32 argc, const char **argv, Game *game)
{
   if(argc < 2)
      return false;
   else if(!Parent::processArguments(argc, argv, game))
      return false;

   if(argc == 3)
   {
      S32 repopDelay = atoi(argv[2]);    // 3rd param is time for this to regenerate in seconds
      if(repopDelay > 0)
         mRepopDelay = repopDelay;
      else
         mRepopDelay = 0;
   }

   return true;
}


string PickupItem::toString(F32 gridSize) const
{
   return Parent::toString(gridSize) + " " + (mRepopDelay > 0 ? itos(mRepopDelay) : "");
}


U32 PickupItem::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 retMask = Parent::packUpdate(connection, updateMask, stream);       // Writes id and pos

   stream->writeFlag(updateMask & InitialMask);
   stream->writeFlag(mIsVisible || mIsMomentarilyVisible);

   if(mIsMomentarilyVisible)
   {
      mIsMomentarilyVisible = false;
      setMaskBits(PickupMask);
   }

   return retMask;
}


void PickupItem::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   Parent::unpackUpdate(connection, stream);    // Get id and pos

   bool isInitial = stream->readFlag();
   bool visible = stream->readFlag();

   if(!isInitial && !visible && mIsVisible)
      onClientPickup();

   mIsVisible = visible;
}


// Runs on both client and server, but does nothing on client
bool PickupItem::collide(GameObject *otherObject)
{
   if(mIsVisible && !isGhost() && isShipType(otherObject->getObjectTypeNumber()))
   {
      if(pickup(dynamic_cast<Ship *>(otherObject)))
      {
         setMaskBits(PickupMask);
         mRepopTimer.reset(mRepopDelay * 1000);
         mIsVisible = false;
      }
   }
   return false;
}


#ifndef ZAP_DEDICATED

EditorAttributeMenuUI *PickupItem::getAttributeMenu()
{
   // Lazily initialize this -- if we're in the game, we'll never need this to be instantiated
   if(!mAttributeMenuUI)
   {
      ClientGame *clientGame = (ClientGame *)getGame();
      mAttributeMenuUI = new EditorAttributeMenuUI(clientGame);

      // Value doesn't matter (set to 99 here), as it will be clobbered when startEditingAttrs() is called
      CounterMenuItem *menuItem = new CounterMenuItem(clientGame, "Regen Time:", 99, 1, 0, 100, "secs", "No regen", 
                                                      "Time for this item to reappear after it has been picked up");

      mAttributeMenuUI->addMenuItem(menuItem);

      // Add our standard save and exit option to the menu
      mAttributeMenuUI->addSaveAndQuitMenuItem();
   }

   return mAttributeMenuUI;
}


// Get the menu looking like what we want
void PickupItem::startEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   attributeMenu->getMenuItem(0)->setIntValue(mRepopDelay);
}


// Retrieve the values we need from the menu
void PickupItem::doneEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   mRepopDelay = attributeMenu->getMenuItem(0)->getIntValue();
}


// Render some attributes when item is selected but not being edited
void PickupItem::renderAttributeString(F32 currentScale)
{
   string txt;

   if(mRepopDelay == 0)
      txt = "Regen: None";
   else
      txt = "Regen: " + itos(mRepopDelay) + " sec" + ( mRepopDelay != 1 ? "s" : "");

   renderItemText(txt.c_str(), 1, currentScale);
}

#endif


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(RepairItem);

// Constructor
RepairItem::RepairItem(Point pos) : PickupItem(pos, (F32)REPAIR_ITEM_RADIUS, DEFAULT_RESPAWN_TIME) 
{ 
   mObjectTypeNumber = RepairItemTypeNumber;
}


RepairItem *RepairItem::clone() const
{
   return new RepairItem(*this);
}


// Runs on server, returns true if we're doing the pickup, false otherwise
bool RepairItem::pickup(Ship *theShip)
{
   if(theShip->getHealth() >= 1)
      return false;

   DamageInfo di;
   di.damageAmount = -0.5f;      // Negative damage = repair!
   di.damageType = DamageTypePoint;
   di.damagingObject = this;

   theShip->damageObject(&di);
   return true;
}


// Runs on client when item's unpack method signifies the item has been picked up
void RepairItem::onClientPickup()
{
   SoundSystem::playSoundEffect(SFXShipHeal, getRenderPos(), getRenderVel());
}


void RepairItem::renderItem(const Point &pos)
{
   if(!isVisible())
      return;

   renderRepairItem(pos);
}


void RepairItem::renderDock()
{
   renderRepairItem(getVert(0), true, 0, 1);
}


F32 RepairItem::getEditorRadius(F32 currentScale)
{
   return 22 * currentScale;
}


const char RepairItem::className[] = "RepairItem";      // Class name as it appears to Lua scripts

// Lua constructor
RepairItem::RepairItem(lua_State *L)
{
   mObjectTypeNumber = RepairItemTypeNumber;
}


// Define the methods we will expose to Lua
Lunar<RepairItem>::RegType RepairItem::methods[] =
{
   // Standard gameItem methods
   method(RepairItem, getClassID),
   method(RepairItem, getLoc),
   method(RepairItem, getRad),
   method(RepairItem, getVel),
   method(RepairItem, getTeamIndx),

   // Class specific methods
   method(RepairItem, isVis),
   {0,0}    // End method list
};

S32 RepairItem::isVis(lua_State *L) { return returnBool(L, isVisible()); }        // Is RepairItem visible? (returns boolean)


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(EnergyItem);

// Constructor
EnergyItem::EnergyItem(Point p) : PickupItem(p, 20, DEFAULT_RESPAWN_TIME) 
{ 
   mObjectTypeNumber = EnergyItemTypeNumber;
};   


EnergyItem *EnergyItem::clone() const
{
   return new EnergyItem(*this);
}


// Runs on server, returns true if we're doing the pickup, false otherwise
bool EnergyItem::pickup(Ship *theShip)
{
   S32 energy = theShip->getEnergy();
   S32 maxEnergy = theShip->getMaxEnergy();

   if(energy >= maxEnergy)      // Don't need no stinkin' energy!!
      return false;

   theShip->changeEnergy(maxEnergy / 2);     // Bump up energy by 50%, changeEnergy() sets energy delta

   return true;
}


// Runs on client when item's unpack method signifies the item has been picked up
void EnergyItem::onClientPickup()
{
   SoundSystem::playSoundEffect(SFXShipHeal, getRenderPos(), getRenderVel());
}


void EnergyItem::renderItem(const Point &pos)
{
   if(!isVisible())
      return;

   renderEnergyItem(pos);
}


const char EnergyItem::className[] = "EnergyItem";      // Class name as it appears to Lua scripts

// Lua constructor
EnergyItem::EnergyItem(lua_State *L)
{
   mObjectTypeNumber = EnergyItemTypeNumber;
}


// Define the methods we will expose to Lua
Lunar<EnergyItem>::RegType EnergyItem::methods[] =
{
   // Standard gameItem methods
   method(EnergyItem, getClassID),
   method(EnergyItem, getLoc),
   method(EnergyItem, getRad),
   method(EnergyItem, getVel),
   method(EnergyItem, getTeamIndx),

   // Class specific methods
   method(EnergyItem, isVis),
   {0,0}    // End method list
};

S32 EnergyItem::isVis(lua_State *L) { return returnBool(L, isVisible()); }        // Is EnergyItem visible? (returns boolean)



};
