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

#include "quickChatHelper.h"     

#include "UI.h"      // For playBoop()

#include "ClientGame.h"
#include "Colors.h"

#include "RenderUtils.h"
#include "OpenglUtils.h"

#include <ctype.h>

namespace Zap
{

static const char  *quickChatLegendText[]   = { "Team Message",         "Global Message"         };
static const Color *quickChatLegendColors[] = { &Colors::teamChatColor, &Colors::globalChatColor };


////////////////////////////////////////
////////////////////////////////////////

QuickChatNode::QuickChatNode() : caption(""), msg("")
{
   depth = 0;        // This is a beginning or ending node
   inputCode  = KEY_UNKNOWN;
   buttonCode = KEY_UNKNOWN;
   teamOnly    = false;
   commandOnly = false;
   isMsgItem   = false;
}

// Destructor
QuickChatNode::~QuickChatNode()
{
   // Do nothing
}


////////////////////////////////////////
////////////////////////////////////////


Vector<QuickChatNode> QuickChatHelper::nodeTree;      // Holds our tree of QuickChat groups and messages, as defined in the INI file

QuickChatHelper::QuickChatHelper()
{
   mCurNode = 0;
   mItemWidth = -1;
   mMenuItems1IsCurrent = true;
}

// Destructor
QuickChatHelper::~QuickChatHelper()
{
   // Do nothing
}


HelperMenu::HelperMenuType QuickChatHelper::getType() { return QuickChatHelperType; }


// Returns true if there was something to render, false if our current chat tree position has nothing to render.  This can happen
// when a chat tree has a bunch of keyboard only items and we're in joystick mode... if no items are drawn, there's no point
// in remaining in QuickChat mode, is there?
void QuickChatHelper::render()
{
   S32 yPos = MENU_TOP + MENU_PADDING;

   if(!nodeTree.size())
   {
      glColor(Colors::ErrorMessageTextColor);
      drawCenteredString(yPos, MENU_FONT_SIZE, "Quick Chat messages improperly configured.  Please see bitfighter.ini.");
      return;
   }

   Vector<OverlayMenuItem> *menuItems    = getMenuItems(mMenuItems1IsCurrent);
   Vector<OverlayMenuItem> *oldMenuItems = getMenuItems(!mMenuItems1IsCurrent);

   if(menuItems->size() == 0)    // Nothing to render, let's go home
   {
      TNLAssert(menuItems->size() > 0, "Should have some items here!");

      glColor(Colors::red); 
      drawString(0, yPos, MENU_FONT_SIZE, "No messages here (misconfiguration?)");
      yPos += MENU_FONT_SIZE + MENU_FONT_SPACING;
   }
   else
   {
      // Protect against an empty oldMenuItems list, as will happen when this is called at the top level
      const OverlayMenuItem *oldItem = oldMenuItems->size() > 0 ? &oldMenuItems->get(0) : NULL;
      drawItemMenu("QuickChat menu", &menuItems->get(0), menuItems->size(), oldItem, oldMenuItems->size(),
                   quickChatLegendText, quickChatLegendColors, ARRAYSIZE(quickChatLegendText));
   }
}


void QuickChatHelper::onActivated()
{
   Parent::onActivated();

   mMenuItems1.clear();
   mMenuItems2.clear();

   updateChatMenuItems(0);

  
   if(mItemWidth == -1)
   {
      for(S32 i = 0; i < nodeTree.size(); i++)
      {
         S32 width = getStringWidth(MENU_FONT_SIZE, nodeTree[i].caption.c_str());
         if(width > mItemWidth)
            mItemWidth = width;
      }
   }
}


Vector<OverlayMenuItem> *QuickChatHelper::getMenuItems(bool one)
{
   return one ? &mMenuItems1 : &mMenuItems2;
}


void QuickChatHelper::updateChatMenuItems(S32 curNode)
{
   mCurNode = curNode;

   S32 walk = mCurNode;
   U32 matchLevel = nodeTree[walk].depth + 1;
   walk++;

   mMenuItems1IsCurrent = !mMenuItems1IsCurrent;

   Vector<OverlayMenuItem> *menuItems = getMenuItems(mMenuItems1IsCurrent);
   menuItems->clear();

   GameSettings *settings = getGame()->getSettings();

   bool showKeys = settings->getInputMode() == InputModeKeyboard ||
                   settings->getIniSettings()->mSettings.getVal<YesNo>("ShowKeyboardKeysInStickMode");

   // First get to the end...
   while(nodeTree[walk].depth >= matchLevel)
      walk++;

   // Then draw bottom up...
   while(walk != mCurNode)
   {  
      // When we're using a controller, don't present options with no defined controller key
      if(nodeTree[walk].depth == matchLevel && (showKeys || nodeTree[walk].buttonCode != KEY_UNKNOWN) )
      {
         OverlayMenuItem item;
         item.button = nodeTree[walk].buttonCode;
         item.key    = nodeTree[walk].inputCode;
         item.showOnMenu = true;
         item.name = nodeTree[walk].caption.c_str();
         item.help = "";
         item.itemColor = nodeTree[walk].teamOnly ? &Colors::teamChatColor : &Colors::globalChatColor;

         menuItems->push_back(item);
      }
      walk--;
   }
}


// Returns true if key was used, false if not
bool QuickChatHelper::processInputCode(InputCode inputCode)
{
   if(Parent::processInputCode(inputCode))
      return true;

   if(!nodeTree.size())       // We'll crash if we go any further!
      return false;

   // Try to find a match if we can...

   S32 oldNode = mCurNode;

   // Set up walk...
   S32 walk = mCurNode;
   U32 matchLevel = nodeTree[walk].depth + 1;
   walk++;

   // Iterate over anything at our desired depth or lower
   while(nodeTree[walk].depth >= matchLevel)
   {
      // If it has the same key...
      bool match = (inputCode == nodeTree[walk].inputCode) || (inputCode == nodeTree[walk].buttonCode);

      if(match && nodeTree[walk].depth == matchLevel)
      {
         // ...then select it
         updateChatMenuItems(walk);
         resetScrollTimer();

         UserInterface::playBoop();

         // If we're at a leaf (ie, next child down is higher or equal to us), then issue the chat and call it good
         walk++;
         if(nodeTree[mCurNode].depth >= nodeTree[walk].depth)
         {
            exitHelper();

            if(nodeTree[mCurNode].commandOnly)
               getGame()->runCommand(nodeTree[mCurNode].msg.c_str());

            else
               getGame()->sendChatSTE(!nodeTree[mCurNode].teamOnly, nodeTree[mCurNode].msg.c_str());

            // Because we've run off the end of the menu tree, which is how we know we hit a terminal node and not the parent
            // of yet more items, we need to restore the menus for the menu closing transition animation.  Finally,
            // we will clear the transition timer to suppress the transition animation.
            mMenuItems1IsCurrent = !mMenuItems1IsCurrent;
            updateChatMenuItems(oldNode);
            clearScrollTimer();
         }
         return true;
      }
      walk++;
   }

   return false;
}


bool QuickChatHelper::isMovementDisabled() const
{
   return false;
}


};

