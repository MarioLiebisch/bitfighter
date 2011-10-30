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

#include "lineEditor.h"
#include <math.h>

#ifndef ZAP_DEDICATED
#include "UI.h"
#endif

namespace Zap
{

// Constructor
LineEditor::LineEditor(U32 maxLength, string value)
{
   mMaxLen = maxLength;
   mLine = value;
   mFilter = allAsciiFilter;
   mPrompt = "";
   mSecret = false;
   matchIndex = -1;
}


char LineEditor::at(U32 pos)
{
   if(pos >= mLine.length())
      return 0;

   return mLine.at(pos);
} 


// Given a list of potential match candidates, and a partially typed string, find all candidates that are potential matches
void LineEditor::buildMatchList(Vector<string> *candidates, const char *partial)
{
   // Search for matching candidates
   if(matchIndex == -1)    // -1 --> Need to build a new match list (gets set to -1 when we change mLineEditor by typing)
   {
      matchList.clear();

      S32 len = (S32)strlen(partial);

      for(S32 i = 0; i < candidates->size(); i++)
      {
         // If partial is empty, then everything matches -- we want all candidates in our list
         if(!strcmp(partial, "") || !stricmp((*candidates)[i].substr(0, len).c_str(), partial))
            matchList.push_back((*candidates)[i]);
      }
   }
}


// Draw our cursor, assuming string is drawn at x,y  (vert spacing works differently than on the angle version
void LineEditor::drawCursor(S32 x, S32 y, S32 fontSize)
{
   drawCursorAngle(x, y + fontSize, fontSize, UserInterface::getStringWidth(fontSize, mLine.c_str()), 0);
}


// Draw our cursor, assuming string is drawn at x,y with starting width
void LineEditor::drawCursor(S32 x, S32 y, S32 fontSize, S32 startingWidth)
{
   drawCursorAngle(x, y + fontSize, fontSize, startingWidth, 0);
}


// Draw our cursor, assuming string is drawn at x,y at specified angle 
void LineEditor::drawCursorAngle(F32 x, F32 y, F32 fontSize, F32 angle)
{
   S32 width = UserInterface::getStringWidth(fontSize, mLine.c_str());
   drawCursorAngle(x, y, fontSize, width, angle);
}


// static
void LineEditor::drawCursorAngle(S32 x, S32 y, F32 fontSize, S32 width, F32 angle)
{
#ifndef ZAP_DEDICATED
   if((Platform::getRealMilliseconds() / 100) % 2)
   {
      const F32 gap = fontSize / 6;
      F32 xpos = x + (((F32)width + gap) * cos(angle)); 
      F32 ypos = y + (((F32)width + gap) * sin(angle)); 

      UserInterface::drawAngleString_fixed(xpos, ypos, (F32)fontSize, angle, "_");
   }
#endif
}


void LineEditor::addChar(const char c) 
{ 
   if(c == 0)
      return;

   if((mFilter == digitsOnlyFilter) && (c < '0' || c > '9'))
      return;
      
   else if((mFilter == numericFilter) && (c != '-' && c != '.' && (c < '0' || c > '9')))
      return;

   else if((mFilter == noQuoteFilter) && (c == '"' ))
      return;
   
   else if((mFilter == fileNameFilter) && ! ( (c >= '0' && c <= '9') ||
                                              (c == '_')             ||
                                              (c >= 'A' && c <= 'Z') ||
                                              (c >= 'a' && c <= 'z') )  )
      return;
   
   if(length() < mMaxLen) mLine.append(string(1,c)); 
   matchIndex = -1;
}


// inputCode will have either backspace or delete in it -- basically a convenience function
void LineEditor::handleBackspace(InputCode inputCode)
{
   if(inputCode == KEY_BACKSPACE)
      backspacePressed();
   else       // KEY_DELETE
      deletePressed();
}


};

