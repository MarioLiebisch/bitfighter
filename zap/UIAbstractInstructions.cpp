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

#include "UIAbstractInstructions.h"

#include "ClientGame.h"
#include "GameSettings.h"
#include "ScreenInfo.h"
#include "Colors.h"

#include "RenderUtils.h"
#include "OpenglUtils.h"

namespace Zap
{

extern void drawHorizLine(S32 x1, S32 x2, S32 y);


// Define static consts
const Color *AbstractInstructionsUserInterface::txtColor = &Colors::cyan;
const Color *AbstractInstructionsUserInterface::keyColor = &Colors::white;     
const Color *AbstractInstructionsUserInterface::secColor = &Colors::yellow;
const Color *AbstractInstructionsUserInterface::groupHeaderColor = &Colors::red;

// Import some symbols to reduce typing
using UI::SymbolString;
using UI::SymbolShapePtr;
using UI::SymbolStringSet;


// Constructor
AbstractInstructionsUserInterface::AbstractInstructionsUserInterface(ClientGame *clientGame) : 
                                       Parent(clientGame),
                                       mSpecialKeysInstrLeft(LineGap), 
                                       mSpecialKeysBindingsLeft(LineGap), 
                                       mSpecialKeysInstrRight(LineGap), 
                                       mSpecialKeysBindingsRight(LineGap),
                                       mWallInstr(LineGap),  
                                       mWallBindings(LineGap)
{
   // Do nothing
}


// Destructor
AbstractInstructionsUserInterface::~AbstractInstructionsUserInterface()
{
   // Do nothing
}


void AbstractInstructionsUserInterface::pack(SymbolStringSet &instrs,  SymbolStringSet &bindings,      // <== will be modified
                                             const ControlStringsEditor *helpBindings, S32 bindingCount, GameSettings *settings)
{
   Vector<SymbolShapePtr> symbols;

   for(S32 i = 0; i < bindingCount; i++)
   {
      if(helpBindings[i].command == "-")
      {
         symbols.clear();
         symbols.push_back(SymbolString::getHorizLine(335, FontSize, &Colors::gray40));
         instrs.add(SymbolString(symbols));

         symbols.clear();
         symbols.push_back(SymbolString::getBlankSymbol(0, FontSize));
         bindings.add(SymbolString(symbols));
      }
      else if(helpBindings[i].command == "HEADER")
      {
         symbols.clear();
         symbols.push_back(SymbolString::getSymbolText(helpBindings[i].binding, FontSize, HelpContext, groupHeaderColor));
         instrs.add(SymbolString(symbols));

         symbols.clear();
         symbols.push_back(SymbolString::getBlankSymbol(0, FontSize));
         bindings.add(SymbolString(symbols));
      }
      else     // Normal line
      {
         symbols.clear();
         SymbolString::symbolParse(settings->getInputCodeManager(), helpBindings[i].command, 
                                       symbols, HelpContext, FontSize, txtColor, keyColor);

         instrs.add(SymbolString(symbols));

         symbols.clear();
         SymbolString::symbolParse(settings->getInputCodeManager(), helpBindings[i].binding, 
                                       symbols, HelpContext, FontSize, keyColor);
         bindings.add(SymbolString(symbols));
      }
   }
}


void AbstractInstructionsUserInterface::render(const char *header, S32 page, S32 pages)
{
   glColor(Colors::red);
   drawStringf(  3, 3, 25, "INSTRUCTIONS - %s", header);
   drawStringf(625, 3, 25, "PAGE %d/%d", page, pages); 
   drawCenteredString(571, 20, "LEFT - previous page   |   RIGHT, SPACE - next page   |   ESC exits");

   glColor(Colors::gray70);
   drawHorizLine(0, 800, 32);
   drawHorizLine(0, 800, 569);
}


void AbstractInstructionsUserInterface::renderConsoleCommands(const SymbolStringSet &instructions, const ControlStringsEditor *cmdList)
{
   const S32 headerSize = 20;
   const S32 cmdSize = 16;
   const S32 cmdGap = 10;

   S32 ypos = 60;
   S32 cmdCol = horizMargin;                                                         // Action column
   S32 descrCol = horizMargin + S32(gScreenInfo.getGameCanvasWidth() * 0.25) + 55;   // Control column

   ypos += instructions.render(cmdCol, ypos, UI::AlignmentLeft);

   ypos += 10 - cmdSize - cmdGap;

   Color cmdColor =   Colors::cyan;
   Color descrColor = Colors::white;
   Color secColor =   Colors::yellow;

   glColor(secColor);
   drawString(cmdCol,   ypos, headerSize, "Code Example");
   drawString(descrCol, ypos, headerSize, "Description");

   Vector<SymbolShapePtr> symbols;

   ypos += cmdSize + cmdGap;
   glColor(&Colors::gray70);
   drawHorizLine(cmdCol, 750, ypos);

   ypos += 10;     // Small gap before cmds start
   ypos += cmdSize;

   for(S32 i = 0; cmdList[i].command != ""; i++)
   {
      if(cmdList[i].command[0] == '-')      // Horiz spacer
      {
         glColor(Colors::gray40);
         drawHorizLine(cmdCol, cmdCol + 335, ypos + (cmdSize + cmdGap) / 4);
      }
      else
      {
         symbols.clear();
         SymbolString::symbolParse(getGame()->getSettings()->getInputCodeManager(), cmdList[i].command, 
                                   symbols, HelpContext, cmdSize, txtColor, keyColor);

         SymbolString instrs(symbols);
         instrs.render(cmdCol, ypos, UI::AlignmentLeft);

         symbols.clear();
         SymbolString::symbolParse(getGame()->getSettings()->getInputCodeManager(), cmdList[i].binding, 
                                   symbols, HelpContext, cmdSize, txtColor, keyColor);

         SymbolString keys(symbols);
         keys.render(descrCol, ypos, UI::AlignmentLeft);
      }

      ypos += cmdSize + cmdGap;
   }
}


}
