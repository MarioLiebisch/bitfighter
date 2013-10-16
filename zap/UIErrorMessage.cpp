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

#include "UIErrorMessage.h"

#include "UIManager.h"
#include "ClientGame.h"
//#include "Colors.h"


namespace Zap
{

// Constructor
AbstractMessageUserInterface::AbstractMessageUserInterface(ClientGame *game) : Parent(game)
{
   reset();
}


AbstractMessageUserInterface::~AbstractMessageUserInterface()
{
   // Do nothing
}


void AbstractMessageUserInterface::onActivate()
{
   // Do nothing -- block parent's onActivate
}


// First line is 1
void AbstractMessageUserInterface::setMessage(S32 id, string message)
{
   TNLAssert(id >= 1 && id <= MAX_LINES, "Invalid line id!");

   Vector<UI::SymbolShapePtr> symbols;
   SymbolString::symbolParse(getGame()->getSettings()->getInputCodeManager(), message, 
                             symbols, HelpContext, 18);

   mMessage[id - 1] = SymbolShapePtr(new SymbolString(symbols));
}


void AbstractMessageUserInterface::setTitle(const char *message)
{
   mTitle = message;
}


void AbstractMessageUserInterface::setInstr(const char *message)
{
   mInstr = message;
}


void AbstractMessageUserInterface::quit()
{
   getUIManager()->reactivatePrevUI();
}


void AbstractMessageUserInterface::setPresentation(S32 presentationId)
{
   mPresentationId = presentationId;
}


void AbstractMessageUserInterface::reset()
{
   mPresentationId = 0;

   for(S32 i = 0; i < MAX_LINES; i++)
      mMessage[i] =  SymbolShapePtr(new SymbolBlank());
}


bool AbstractMessageUserInterface::onKeyDown(InputCode inputCode)
{
   return Parent::onKeyDown(inputCode);
}


void AbstractMessageUserInterface::render()
{
   if(getUIManager()->getPrevUI() != this)
      getUIManager()->renderPrevUI(this);

   if(mPresentationId == 0)      // Standard presentation
      renderMessageBox(mTitle, mInstr, mMessage, MAX_LINES);
   else if(mPresentationId == 1)
       renderUnboxedMessageBox(mTitle, mInstr, mMessage, MAX_LINES);
   else
      TNLAssert(false, "Unknown value of mPresentationId!");
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
ErrorMessageUserInterface::ErrorMessageUserInterface(ClientGame *game) : Parent(game)
{
   // Do nothing
}


// Destructor
ErrorMessageUserInterface::~ErrorMessageUserInterface()
{
   // Do nothing
}


void ErrorMessageUserInterface::reset()
{
   Parent::reset();

   mTitle = "WE HAVE A PROBLEM";          // Default title
   mInstr = "Hit any key to continue";
}


bool ErrorMessageUserInterface::onKeyDown(InputCode inputCode)
{
   if(Parent::onKeyDown(inputCode))
      return true;
   else
      quit();     // Quit the interface when any key is pressed...  any key at all.  Mostly.

   return false;
}


};


