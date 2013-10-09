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

#include "loadoutHelper.h"

#include "UIGame.h"
#include "UIManager.h"
#include "UIInstructions.h"
#include "Colors.h"
#include "LoadoutTracker.h"
#include "gameConnection.h"
#include "ClientGame.h"


namespace Zap
{

// For clarity
#define UNSEL_COLOR &Colors::overlayMenuUnselectedItemColor
#define HELP_COLOR  &Colors::overlayMenuHelpColor

static const OverlayMenuItem loadoutModuleMenuItems[] = {
   { KEY_1, BUTTON_1, true, ModuleBoost,    "Turbo Boost",           UNSEL_COLOR, "",               NULL       },
   { KEY_2, BUTTON_2, true, ModuleShield,   "Shield Generator",      UNSEL_COLOR, "",               NULL       },
   { KEY_3, BUTTON_3, true, ModuleRepair,   "Repair Module",         UNSEL_COLOR, "",               NULL       },
   { KEY_4, BUTTON_4, true, ModuleSensor,   "Enhanced Sensor",       UNSEL_COLOR, "",               NULL       },
   { KEY_5, BUTTON_5, true, ModuleCloak,    "Cloak Field Modulator", UNSEL_COLOR, "",               NULL       },
   { KEY_6, BUTTON_6, true, ModuleArmor,    "Armor",                 UNSEL_COLOR, "(adds mass)",    HELP_COLOR },
   { KEY_7, BUTTON_7, true, ModuleEngineer, "Engineer",              UNSEL_COLOR, "",               NULL       },
};

static const S32 moduleEngineerIndex = 6;

static const OverlayMenuItem loadoutWeaponMenuItems[] = {
   { KEY_1, BUTTON_1, true, WeaponPhaser, "Phaser",     UNSEL_COLOR, "", NULL },
   { KEY_2, BUTTON_2, true, WeaponBounce, "Bouncer",    UNSEL_COLOR, "", NULL },
   { KEY_3, BUTTON_3, true, WeaponTriple, "Triple",     UNSEL_COLOR, "", NULL },
   { KEY_4, BUTTON_4, true, WeaponBurst,  "Burster",    UNSEL_COLOR, "", NULL },
   { KEY_5, BUTTON_5, true, WeaponMine,   "Mine Layer", UNSEL_COLOR, "", NULL },
   { KEY_6, BUTTON_6, true, WeaponSeeker, "Seeker",     UNSEL_COLOR, "", NULL },
};                                                  

#undef UNSEL_COLOR
#undef HELP_COLOR

////////////////////////////////////////
////////////////////////////////////////

// Constructor
LoadoutHelper::LoadoutHelper()
{
   mCurrentIndex = 0;
   mItemWidth = max(getMaxItemWidth(loadoutModuleMenuItems, ARRAYSIZE(loadoutModuleMenuItems)),
                    getMaxItemWidth(loadoutWeaponMenuItems, ARRAYSIZE(loadoutWeaponMenuItems)));
}

// Destructor
LoadoutHelper::~LoadoutHelper()
{
   // Do nothing
}


HelperMenu::HelperMenuType LoadoutHelper::getType() { return LoadoutHelperType; }


// Gets called at the beginning of every game; available options may change based on level
void LoadoutHelper::pregameSetup(bool engineerEnabled)
{
   mEngineerEnabled = engineerEnabled;
}


void LoadoutHelper::onActivated()
{
   Parent::onActivated();

   // Player has proven they know how to change loadouts, so no need to show a help message on how to do it
   getGame()->getUIManager()->removeInlineHelpItem(ChangeConfigItem, true);

   mCurrentIndex = 0;

   mModuleMenuItems = Vector<OverlayMenuItem>(loadoutModuleMenuItems, ARRAYSIZE(loadoutModuleMenuItems));
   mWeaponMenuItems = Vector<OverlayMenuItem>(loadoutWeaponMenuItems, ARRAYSIZE(loadoutWeaponMenuItems));

   mModuleMenuItems[moduleEngineerIndex].showOnMenu = mEngineerEnabled;    // Can't delete this or other arrays will become unaligned

   mLoadoutChanged = false;
}


void LoadoutHelper::render()
{
   char title[100];

   bool showingModules = mCurrentIndex < ShipModuleCount;

   if(showingModules)
      dSprintf(title, sizeof(title), "Pick %d modules:", ShipModuleCount);
   else
      dSprintf(title, sizeof(title), "Pick %d weapons:", ShipWeaponCount);


   if(showingModules)
      drawItemMenu(title, &mModuleMenuItems[0], mModuleMenuItems.size(), NULL, 0);
   else
      drawItemMenu(title, &mWeaponMenuItems[0], mWeaponMenuItems.size(), &mModuleMenuItems[0], mModuleMenuItems.size());
}


// Return true if key did something, false if key had no effect
// Runs on client
bool LoadoutHelper::processInputCode(InputCode inputCode)
{
   if(Parent::processInputCode(inputCode))    // Check for cancel keys
      return true;
   
   S32 index;

   Vector<OverlayMenuItem> *menuItems = (mCurrentIndex < ShipModuleCount) ? &mModuleMenuItems : &mWeaponMenuItems;

   for(index = 0; index < menuItems->size(); index++)
      if(inputCode == menuItems->get(index).key || inputCode == menuItems->get(index).button)
         break;

   if(index == menuItems->size() || !menuItems->get(index).showOnMenu)
      return false;

   // Make sure user doesn't select the same loadout item twice
   bool alreadyUsed = false;
	
   if(mCurrentIndex < ShipModuleCount)    // We're working with modules
   {                                      // (note... braces required here!)
      for(S32 i = 0; i < mCurrentIndex && !alreadyUsed; i++)
         if(mModule[i] == index)
            alreadyUsed = true;
   }
   else                                   // We're working with weapons
      for(S32 i = ShipModuleCount; i < mCurrentIndex && !alreadyUsed; i++)
         if(mWeapon[i - ShipModuleCount] == index)
            alreadyUsed = true;

   if(!alreadyUsed)
   {
      menuItems->get(index).itemColor = &Colors::overlayMenuSelectedItemColor;
      menuItems->get(index).helpColor = &Colors::overlayMenuSelectedItemColor;

      mModule[mCurrentIndex] = ShipModule(index);
      mCurrentIndex++;

      // Check if we need to switch over to weapons
      if(mCurrentIndex == ShipModuleCount)
         resetScrollTimer();
   }

   if(mCurrentIndex == ShipModuleCount + ShipWeaponCount)     // All loadout options selected, process complete
   {
      LoadoutTracker loadout;

      for(S32 i = 0; i < ShipModuleCount; i++)
         loadout.setModule(i, ShipModule(loadoutModuleMenuItems[mModule[i]].itemIndex));

      for(S32 i = 0; i < ShipWeaponCount; i++)
         loadout.setWeapon(i, WeaponType(loadoutWeaponMenuItems[mWeapon[i]].itemIndex));

      mLoadoutChanged = !getGame()->getLocalPlayerShip()->isLoadoutSameAsCurrent(loadout);

      GameConnection *conn = getGame()->getConnectionToServer();

      if(conn)
      {
         if(getGame()->getSettings()->getIniSettings()->mSettings.getVal<YesNo>("VerboseHelpMessages"))
            getGame()->displayShipDesignChangedMessage(loadout, "Modifications canceled -- new ship design same as the current");     

         // Request loadout even if it was the same -- if I have loadout A, with on-deck loadout B, and I enter a new loadout
         // that matches A, it would be better to have loadout remain unchanged if I entered a loadout zone.
         // Tell server loadout has changed.  Server will activate it when we enter a loadout zone.
         conn->c2sRequestLoadout(loadout.toU8Vector());     
      }
      exitHelper();     
   }

   return true;
}


InputCode LoadoutHelper::getActivationKey() 
{ 
   GameSettings *settings = getGame()->getSettings();
   return settings->getInputCodeManager()->getBinding(InputCodeManager::BINDING_LOADOUT); 
}


const char *LoadoutHelper::getCancelMessage() const
{
   return "Modifications canceled -- ship design unchanged.";
}


void LoadoutHelper::activateHelp(UIManager *uiManager)
{
   // Go to module help page
   if(mCurrentIndex < ShipModuleCount)
      uiManager->getUI<InstructionsUserInterface>()->activatePage(InstructionsUserInterface::InstructionModules);
   // Go to weapons help page
   else
      uiManager->getUI<InstructionsUserInterface>()->activatePage(InstructionsUserInterface::InstructionWeaponProjectiles);
}


void LoadoutHelper::onWidgetClosed()
{
   // We only want to display this help item if the loadout actually changed
   if(mLoadoutChanged)
   {
      bool hasLoadout = getGame()->levelHasLoadoutZone();
      getGame()->getUIManager()->addInlineHelpItem(hasLoadout ?  LoadoutChangedZoneItem : LoadoutChangedNoZoneItem);
   }
}


};

