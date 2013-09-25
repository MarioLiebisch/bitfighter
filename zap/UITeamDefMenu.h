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

#ifndef _UITEAMDEFMENU_H_
#define _UITEAMDEFMENU_H_

#include "UI.h"
#include "InputModeEnum.h"
#include "Color.h"
#include "Timer.h"
#include "SymbolShape.h"

namespace Zap
{

using namespace std;


class TeamDefUserInterface : public UserInterface
{
   typedef UserInterface Parent;

private:
   Timer errorMsgTimer;
   string errorMsg;

   UI::SymbolStringSet mMenuSubTitle;
   UI::SymbolStringSet mBottomInstructions;
   
   S32 selectedIndex;          // Highlighted menu item
   S32 changingItem;           // Index of key we're changing (in keyDef mode), -1 otherwise

   bool mEditing;              // true if editing selectedIndex, false if not

public:
   explicit TeamDefUserInterface(ClientGame *game);     // Constructor
   virtual ~TeamDefUserInterface();

   const char *mMenuTitle;
   const char *mMenuFooter;

   void render();              // Draw the menu
   void idle(U32 timeDelta);
   bool onKeyDown(InputCode inputCode);
   void onTextInput(char ascii);
   void onMouseMoved();

   void onActivate();
   void onEscape();
};

};

#endif

