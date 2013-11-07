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

#ifndef _FONT_MANAGER_H_
#define _FONT_MANAGER_H_

#include "tnlTypes.h"

#include "FontContextEnum.h"
#include "freeglut_stroke.h"     // Our stroke font handler -- include here to resolve namespace grief

extern "C" { 
#  include "../fontstash/fontstash.h" 
}

#include <string>

using namespace TNL;
using namespace std;

struct sth_stash;

namespace Zap
{

class BfFont;
class GameSettings;

class FontManager 
{

private:
   static sth_stash *mStash;

   static BfFont *getFont(FontId currentFontId);

   static S32 getStrokeFontStringLength(const SFG_StrokeFont *font, const char* string);
   static S32 getTtfFontStringLength(BfFont *font, const char* string);

public:
   static void initialize(GameSettings *settings, bool useExternalFonts = true);
   static void cleanup();

   static sth_stash *getStash();

   static void drawTTFString(BfFont *font, const char *string, F32 size);
   static void drawStrokeCharacter(const SFG_StrokeFont *font, S32 character);

   static S32 getStringLength(const char* string);

   static void renderString(F32 size, const char *string);

   static void setFont(FontId fontId);
   static void setFontContext(FontContext fontContext);

   static void pushFontContext(FontContext fontContext);
   static void popFontContext();
};


////////////////////////////////////////
////////////////////////////////////////

class BfFont 
{
private:
   bool mIsStrokeFont;
   bool mOk;

   S32 mStashFontId;

   const SFG_StrokeFont *mStrokeFont;     // Will be NULL for TTF fonts

public:
   BfFont(const ::SFG_StrokeFont *strokeFont);              // Stroke font constructor
   BfFont(const string &fontFile, GameSettings *settings);  // TTF font constructor
   virtual ~BfFont();                                       // Destructor

   const SFG_StrokeFont *getStrokeFont();
   bool isStrokeFont();
   S32 getStashFontId();

};


};

#endif

