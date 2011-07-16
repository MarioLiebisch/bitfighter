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

#include "../tnl/tnlTypes.h"

#include "UI.h"
#include "input.h"
#include "timer.h"

#include <string>

namespace Zap
{

using namespace std;

struct TeamPreset
{
   const char *name;    // Team's name, as seen in save file
   F32 r;
   F32 g;
   F32 b;
};

class TeamDefUserInterface : public UserInterface
{
private:
   Timer errorMsgTimer;
   string errorMsg;
   
   S32 selectedIndex;          // Highlighted menu item
   S32 changingItem;           // Index of key we're changing (in keyDef mode), -1 otherwise

   bool mEditing;              // true if editing selectedIndex, false if not

public:
   TeamDefUserInterface();     // Constructor
   const char *mMenuTitle;
   const char *mMenuSubTitle;
   Color mMenuSubTitleColor;
   const char *mMenuFooter;

   void render();              // Draw the menu
   void idle(U32 timeDelta);
   void onKeyDown(KeyCode keyCode, char ascii);
   void onMouseMoved(S32 x, S32 y);

   void onActivate();
   void onEscape();
      
};

extern TeamDefUserInterface gTeamDefUserInterface;

};

#endif

