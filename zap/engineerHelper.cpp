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

#include "engineerHelper.h"
#include "EngineeredItem.h"      // For EngineerModuleDeployer
#include "UIGame.h"
#include "Color.h"                  // For Color def
#include "Colors.h"
#include "ClientGame.h"
#include "JoystickRender.h"
#include "config.h"
#include "gameObjectRender.h"       // For drawSquare

#include "ship.h"

#include "SDL_opengl.h"

namespace Zap
{


EngineerConstructionItemInfo engineerItemInfo[] = {
      { EngineeredTurret,           KEY_1, BUTTON_1, true,  "Turret",        "" },
      { EngineeredForceField,       KEY_2, BUTTON_2, true,  "Force Field",   "" },
      { EngineeredTeleportEntrance, KEY_3, BUTTON_3, true,  "Teleport",      "" },
      { EngineeredTeleportExit,     KEY_4, BUTTON_4, false, "Teleport Exit", "" },
};


////////////////////////////////////////
////////////////////////////////////////

// Constructor
EngineerHelper::EngineerHelper(ClientGame *clientGame) : Parent(clientGame)
{
   mEngineerCostructionItemInfos = Vector<EngineerConstructionItemInfo>(engineerItemInfo, ARRAYSIZE(engineerItemInfo));
}


// Destructor
EngineerHelper::~EngineerHelper()
{
   // Do nothing
}


void EngineerHelper::setSelectedEngineeredObject(U32 objectType)
{
   for(S32 i = 0; i < mEngineerCostructionItemInfos.size(); i++)
      if(mEngineerCostructionItemInfos[i].mObjectType == objectType)
         mSelectedItem = i;
}


void EngineerHelper::onMenuShow()
{
   mSelectedItem = -1;
}


bool EngineerHelper::isEngineerHelper()
{
   return true;
}


static Point deployPosition, deployNormal;

void EngineerHelper::render()
{
   S32 yPos = MENU_TOP;
   const S32 fontSize = 15;
   const Color engineerMenuHeaderColor (Colors::red);

   if(mSelectedItem == -1)    // Haven't selected an item yet
   {
      const S32 xPos = UserInterface::horizMargin + 50;

      drawMenuBorderLine(yPos, engineerMenuHeaderColor);

      glColor(engineerMenuHeaderColor);
      UserInterface::drawString(UserInterface::horizMargin, yPos, fontSize, "What do you want to Engineer?");
      yPos += fontSize + 10;

      GameSettings *settings = getGame()->getSettings();
      InputMode inputMode = settings->getInputCodeManager()->getInputMode();

      bool showKeys = settings->getIniSettings()->showKeyboardKeys || inputMode == InputModeKeyboard;

      for(S32 i = 0; i < mEngineerCostructionItemInfos.size(); i++)
      {
         // Don't show an option that shouldn't be shown!
         if(!mEngineerCostructionItemInfos[i].showOnMenu)
            continue;

         // Draw key controls for selecting the object to be created
         U32 joystickIndex = Joystick::SelectedPresetIndex;

         if(inputMode == InputModeJoystick)     // Only draw joystick buttons when in joystick mode
            JoystickRender::renderControllerButton(F32(UserInterface::horizMargin + (showKeys ? 0 : 20)), (F32)yPos, 
                                                   joystickIndex, mEngineerCostructionItemInfos[i].mButton, false);

         if(showKeys)
         {
            glColor(Colors::white);     // Render key in white
            JoystickRender::renderControllerButton((F32)UserInterface::horizMargin + 20, (F32)yPos, 
                                                   joystickIndex, mEngineerCostructionItemInfos[i].mKey, false);
         }

         glColor(0.1, 1.0, 0.1);     
         S32 x = UserInterface::drawStringAndGetWidth(xPos, yPos, fontSize, mEngineerCostructionItemInfos[i].mName); 

         glColor(.2, .8, .8);    
         UserInterface::drawString(xPos + x, yPos, fontSize, mEngineerCostructionItemInfos[i].mHelp);      // The help string, if there is one

         yPos += fontSize + 7;
      }

      yPos += 2;

      drawMenuBorderLine(yPos - fontSize - 2, engineerMenuHeaderColor);
      yPos += 8;

      drawMenuCancelText(yPos, engineerMenuHeaderColor, fontSize);
   }
   else     // Have selected a module, need to indicate where to deploy
   {
      S32 xPos = UserInterface::horizMargin;
      glColor(Colors::green);
      UserInterface::drawStringf(xPos, yPos, fontSize, "Placing %s.", mEngineerCostructionItemInfos[mSelectedItem].mName);
      yPos += fontSize + 7;
      UserInterface::drawString(xPos, yPos, fontSize, "Aim at a spot on the wall, and activate the module again.");
   }
}


// Return true if key did something, false if key had no effect
// Runs on client
bool EngineerHelper::processInputCode(InputCode inputCode)
{
   if(Parent::processInputCode(inputCode))    // Check for cancel keys
      return true;

   GameConnection *gc = getGame()->getConnectionToServer();
   InputCodeManager *inputCodeManager = getGame()->getSettings()->getInputCodeManager();

   if(mSelectedItem == -1)    // Haven't selected an item yet
   {
      for(S32 i = 0; i < mEngineerCostructionItemInfos.size(); i++)
         if(inputCode == mEngineerCostructionItemInfos[i].mKey || inputCode == mEngineerCostructionItemInfos[i].mButton)
         {
            mSelectedItem = i;
            return true;
         }

      Ship *ship = dynamic_cast<Ship *>(gc->getControlObject());
      if(!ship || (inputCode == inputCodeManager->getBinding(InputCodeManager::BINDING_MOD1) && ship->getModule(0) == ModuleEngineer) ||
                  (inputCode == inputCodeManager->getBinding(InputCodeManager::BINDING_MOD2) && ship->getModule(1) == ModuleEngineer))
      {
         exitHelper();
         return true;
      }
   }
   else                       // Placing item
   {
      Ship *ship = dynamic_cast<Ship *>(gc->getControlObject());
      if(ship && ((inputCode == inputCodeManager->getBinding(InputCodeManager::BINDING_MOD1) && ship->getModule(0) == ModuleEngineer) ||
                  (inputCode == inputCodeManager->getBinding(InputCodeManager::BINDING_MOD2) && ship->getModule(1) == ModuleEngineer)))
      {
         EngineerModuleDeployer deployer;

         // Check deployment status on client; will be checked again on server,
         // but server will only handle likely valid placements
         if(deployer.canCreateObjectAtLocation(getGame()->getGameObjDatabase(), ship, mEngineerCostructionItemInfos[mSelectedItem].mObjectType))
         {
            // Send command to server to deploy
            if(gc)
               gc->c2sEngineerDeployObject(mEngineerCostructionItemInfos[mSelectedItem].mObjectType);
         }
         // If location is bad, show error message
         else
            getGame()->displayErrorMessage(deployer.getErrorMessage().c_str());

         // Normally we'd exit the helper menu here, but we don't since teleport has two parts.
         // We therefore let a server command dictate what we do (see GameConnection::s2cEngineerResponseEvent)

         return true;
      }
   }

   return false;
}


// Basically draws a red box where the ship is pointing
void EngineerHelper::renderDeploymentMarker(Ship *ship)
{
   // Only render wall mounted items (not teleport)
   if(mSelectedItem != -1 &&
         EngineerModuleDeployer::findDeployPoint(ship, mEngineerCostructionItemInfos[mSelectedItem].mObjectType, deployPosition, deployNormal))
   {
      EngineerModuleDeployer deployer;
      bool canDeploy = deployer.canCreateObjectAtLocation(getGame()->getGameObjDatabase(), ship, mEngineerCostructionItemInfos[mSelectedItem].mObjectType);

      switch(mEngineerCostructionItemInfos[mSelectedItem].mObjectType)
      {
         case EngineeredTurret:
         case EngineeredForceField:
            if(canDeploy)
               glColor(Colors::green);
            else
               glColor(Colors::red);

            drawSquare(deployPosition, 5);
            break;

         case EngineeredTeleportEntrance:
         case EngineeredTeleportExit:
            if(canDeploy)
               renderTeleporterOutline(deployPosition, 75.f, Colors::green);
            else
               renderTeleporterOutline(deployPosition, 75.f, Colors::red);
            break;

         default:
            break;
      }
   }
}


const char *EngineerHelper::getCancelMessage()
{
   return "Engineered item not deployed";
}


};

