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

#ifndef _UIQUICKCHAT_H_
#define _UIQUICKCHAT_H_


#include "helperMenu.h"

#include "../tnl/tnlNetBase.h"
#include "../tnl/tnlNetStringTable.h"



#include "UI.h"
#include "timer.h"
#include "keyCode.h"

namespace Zap
{

struct QuickChatNode
{
   U32 depth;
   KeyCode keyCode;
   KeyCode buttonCode;
   bool teamOnly;
   string caption;
   string msg;
   bool isMsgItem;         // False for groups, true for messages
};


////////////////////////////////////////
////////////////////////////////////////

class QuickChatHelper : public HelperMenu
{
private:
   S32 mCurNode;

public:
   QuickChatHelper();      // Constructor

   virtual void render();                
   virtual void show(bool fromController);  
   virtual bool processKeyCode(KeyCode keyCode);    
};

extern Vector<QuickChatNode> gQuickChatTree;      // Holds our tree of QuickChat groups and messages, as defined in the INI file

};

#endif

