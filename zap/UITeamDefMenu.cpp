////-----------------------------------------------------------------------------------
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

#include "UIMenus.h"
#include "UITeamDefMenu.h"
#include "UIChat.h"
#include "UIDiagnostics.h"
#include "UIEditor.h"
#include "input.h"
#include "InputCode.h"
#include "IniFile.h"
#include "config.h"
#include "gameType.h"      // For MAX_TEAMS
#include "ScreenInfo.h"
#include "ClientGame.h"
#include "Cursor.h"

#include "SDL/SDL.h"
#include "SDL/SDL_opengl.h"

#include <string>

namespace Zap
{

// Note: Do not make any of the following team names longer than MAX_TEAM_NAME_LENGTH, which is currently 32
// Note: Make sure we have at least 9 presets below...  (instructions are wired for keys 1-9)
TeamPreset gTeamPresets[] = {
   { "Blue",        0,     0,    1 },
   { "Red",         1,     0,    0 },
   { "Yellow",      1,     1,    0 },
   { "Green",       0,     1,    0 },
   { "Pink",        1, .45f, .875f },
   { "Orange",      1,  .67f,    0 },
   { "Lilac",     .79f,   .5, .96f },
   { "LightBlue", .45f, .875f,   1 },
   { "Ruby",      .67f,    0,    0 },
};

// Other ideas
//Team Blue 0 0 1
//Team Red 1 0 0
//Team Green 0 1 0
//Team Yellow 1 1 0
//Team Turquoise 0 1 1
//Team Pink 1 0 1
//Team Orange 1 0.5 0
//Team Black 0 0 0
//Team White 1 1 1
//Team Sapphire 0 0 0.7
//Team Ruby 0.7 0 0
//Team Emerald 0 0.7 0
//Team Lime 0.8 1 0
//Team DarkAngel 0 0.7 0.7
//Team Purple 0.7 0 0.7
//Team Peach 1 0.7 0


// Constructor
TeamDefUserInterface::TeamDefUserInterface(ClientGame *game) : Parent(game)
{
   setMenuID(TeamDefUI);
   mMenuTitle = "Configure Teams";
   mMenuSubTitle = "For quick configuration, press [ALT]1-9 to specify number of teams";
   mMenuSubTitleColor = Colors::white;
}

static const U32 errorMsgDisplayTime = 4000; // 4 seconds
static const S32 fontsize = 19;
static const S32 fontgap = 12;
static const U32 yStart = UserInterface::vertMargin + 90;
static const U32 itemHeight = fontsize + 5;

void TeamDefUserInterface::onActivate()
{
   selectedIndex = 0;                                 // First item selected when we begin
   mEditing = false;                                  // Not editing anything by default

   EditorUserInterface *ui = getUIManager()->getEditorUserInterface();
   S32 teamCount = ui->getTeamCount();

   ui->mOldTeams.resize(teamCount);  // Avoid unnecessary reallocations

   for(S32 i = 0; i < teamCount; i++)
   {
      EditorTeam *team = ui->getTeam(i);

      ui->mOldTeams[i].color = team->getColor();
      ui->mOldTeams[i].name = team->getName().getString();
   }

   // Display an intitial message to users
   errorMsgTimer.reset(errorMsgDisplayTime);
   errorMsg = "";
   SDL_SetCursor(Cursor::getTransparent());
}


void TeamDefUserInterface::idle(U32 timeDelta)
{
   if(errorMsgTimer.update(timeDelta))
      errorMsg = "";
}


extern Color gNeutralTeamColor;
extern Color gHostileTeamColor;

// TODO: Clean this up a bit...  this menu was two-cols before, and some of that garbage is still here...
void TeamDefUserInterface::render()
{
   const S32 canvasWidth = gScreenInfo.getGameCanvasWidth();
   const S32 canvasHeight = gScreenInfo.getGameCanvasHeight();

   glColor(Colors::white);
   drawCenteredString(vertMargin, 30, mMenuTitle);
   drawCenteredString(vertMargin + 35, 18, mMenuSubTitle);

   glColor(Colors::menuHelpColor);
   drawCenteredString(canvasHeight - vertMargin - 115, 16, "[1] - [9] selects a team preset for current slot");
   drawCenteredString(canvasHeight - vertMargin - 92,  16, "[Enter] edits team name");
   drawCenteredString(canvasHeight - vertMargin - 69,  16, "[R] [G] [B] to change preset color (with or without [Shift])");
   drawCenteredString(canvasHeight - vertMargin - 46,  16, "[Ins] or [+] to insert team | [Del] or [-] to remove selected team");

   glColor(Colors::white);
   drawCenteredString(canvasHeight - vertMargin - 20, 18, "Arrow Keys to choose | ESC to exit");

   EditorUserInterface *ui = getUIManager()->getEditorUserInterface();

   S32 size = ui->getTeamCount();

   if(selectedIndex >= size)
      selectedIndex = 0;


   // Draw the fixed teams
   glColor(gNeutralTeamColor);
   drawCenteredStringf(yStart, fontsize, "Neutral Team (can't change)");
   glColor(gHostileTeamColor);
   drawCenteredStringf(yStart + fontsize + fontgap, fontsize, "Hostile Team (can't change)");

   for(S32 j = 0; j < size; j++)
   {
      S32 i = j + 2;    // Take account of the two fixed teams (neutral & hostile)

      U32 y = yStart + i * (fontsize + fontgap);

      if(selectedIndex == j)       // Highlight selected item
         drawMenuItemHighlight(0, y - 2, canvasWidth, y + itemHeight + 2);

      if(j < ui->getTeamCount())
      {
         char numstr[10];
         dSprintf(numstr, sizeof(numstr), "Team %d: ", j+1);

         char namestr[MAX_NAME_LEN + 20];    // Added a little extra, just to cover any contingency...
         dSprintf(namestr, sizeof(namestr), "%s%s", numstr, ui->getTeam(j)->getName().getString());

         char colorstr[16];                  // Need enough room for "(100, 100, 100)" + 1 for null
         const Color *color = ui->getTeamColor(j);
         dSprintf(colorstr, sizeof(colorstr), "(%d, %d, %d)", S32(color->r * 100 + 0.5), S32(color->g * 100 + 0.5), S32(color->b * 100 + 0.5));
         
         static const char *nameColorStr = "%s  %s";

         // Draw item text
         glColor(color);
         drawCenteredStringf(y, fontsize, nameColorStr, namestr, colorstr);

         // Draw cursor if we're editing
         if(mEditing && j == selectedIndex)
         {
            S32 x = getCenteredStringStartingPosf(fontsize, nameColorStr, namestr, colorstr) + getStringWidth(fontsize, numstr);
            ui->getTeam(j)->getLineEditor()->drawCursor(x, y, fontsize);
         }
      }
   }

   // Draw the help string
   glColor(Colors::menuHelpColor);

   if(errorMsgTimer.getCurrent())
   {
      F32 alpha = 1.0;
      if (errorMsgTimer.getCurrent() < 1000)
         alpha = (F32) errorMsgTimer.getCurrent() / 1000;

      TNLAssert(glIsEnabled(GL_BLEND), "Why is blending off here?");

      glColor(Colors::red, alpha);
      drawCenteredString(canvasHeight - vertMargin - 141, fontsize, errorMsg.c_str());
   }
}

// Run this as we're exiting the menu
void TeamDefUserInterface::onEscape()
{
   // Make sure there is at least one team left...
   EditorUserInterface *ui = getUIManager()->getEditorUserInterface();

   ui->makeSureThereIsAtLeastOneTeam();
   ui->teamsHaveChanged();

   getUIManager()->reactivatePrevUI();
}


class Team;
string origName;
extern bool isPrintable(char c);

void TeamDefUserInterface::onKeyDown(InputCode inputCode, char ascii)
{
   EditorUserInterface *ui = getUIManager()->getEditorUserInterface();

   if(inputCode == KEY_ENTER)
   {
      mEditing = !mEditing;
      if(mEditing)
         origName = ui->getTeam(selectedIndex)->getName().getString();
   }
   else if(mEditing)                // Editing, send keystroke to editor
   {
      if(inputCode == KEY_ESCAPE)     // Stop editing, and restore the original value
      {
         ui->getTeam(selectedIndex)->setName(origName.c_str());
         mEditing = false;
      }
      else if(inputCode == KEY_BACKSPACE || inputCode == KEY_DELETE)
      {
         ui->getTeam(selectedIndex)->getLineEditor()->handleBackspace(inputCode);
      }
      else if(isPrintable(ascii))
      {
         ui->getTeam(selectedIndex)->getLineEditor()->addChar(ascii);
      }
   }
   else if(ascii >= '1' && ascii <= '9')        // Keys 1-9 --> use preset
   {
      if(checkModifier(KEY_ALT))                // Replace all teams with # of teams based on presets
      {
         U32 count = (ascii - '0');
         ui->clearTeams();
         for(U32 i = 0; i < count; i++)
         {
            EditorTeam *team = new EditorTeam;
            team->setName(gTeamPresets[i].name);
            team->setColor(gTeamPresets[i].r, gTeamPresets[i].g, gTeamPresets[i].b);
            ui->addTeam(team);
         }
      }
      else                          // Replace selection with preset of number pressed
      {
         U32 indx = (ascii - '1');
         ui->getTeam(selectedIndex)->setName(gTeamPresets[indx].name);
         ui->getTeam(selectedIndex)->setColor(gTeamPresets[indx].r, gTeamPresets[indx].g, gTeamPresets[indx].b);
      }
   }

   else if(inputCode == KEY_DELETE || inputCode == KEY_MINUS)            // Del or Minus - Delete current team
   {
      if(ui->getTeamCount() == 1) 
      {
         errorMsgTimer.reset(errorMsgDisplayTime);
         errorMsg = "There must be at least one team";
         return;
      }

      ui->removeTeam(selectedIndex);
      if(selectedIndex >= ui->getTeamCount())
         selectedIndex = ui->getTeamCount() - 1;
   }
  
   else if(inputCode == KEY_INSERT || inputCode == KEY_EQUALS)           // Ins or Plus (equals) - Add new item
   {
      S32 teamCount = ui->getTeamCount();

      if(teamCount >= GameType::MAX_TEAMS)
      {
         errorMsgTimer.reset(errorMsgDisplayTime);
         errorMsg = "Too many teams for this interface";
         return;
      }

      S32 presetIndex = teamCount % GameType::MAX_TEAMS;

      EditorTeam *team = new EditorTeam;
      team->setName(gTeamPresets[presetIndex].name);
      team->setColor(gTeamPresets[presetIndex].r, gTeamPresets[presetIndex].g, gTeamPresets[presetIndex].b);
      ui->addTeam(team, teamCount);

      selectedIndex++;

      if(selectedIndex < 0)      // It can happen with too many deletes
         selectedIndex = 0;
   }

   else if(inputCode == KEY_R)
     ui->getTeam(selectedIndex)->alterRed(checkModifier(KEY_SHIFT) ? -.01f : .01f);

   else if(inputCode == KEY_G)
      ui->getTeam(selectedIndex)->alterGreen(checkModifier(KEY_SHIFT) ? -.01f : .01f);

   else if(inputCode == KEY_B)
      ui->getTeam(selectedIndex)->alterBlue(checkModifier(KEY_SHIFT) ? -.01f : .01f);

   else if(inputCode == KEY_ESCAPE || inputCode == BUTTON_BACK)       // Quit
   {
      playBoop();
      onEscape();
   }
   else if(inputCode == KEY_UP || inputCode == BUTTON_DPAD_UP)        // Prev item
   {
      selectedIndex--;
      if(selectedIndex < 0)
         selectedIndex = ui->getTeamCount() - 1;
      playBoop();
      SDL_SetCursor(Cursor::getTransparent());

   }
   else if(inputCode == KEY_DOWN || inputCode == BUTTON_DPAD_DOWN)    // Next item
   {
      selectedIndex++;
      if(selectedIndex >= ui->getTeamCount())
         selectedIndex = 0;
      playBoop();
      SDL_SetCursor(Cursor::getTransparent());
   }
   else if(inputCode == keyDIAG)     // Turn on diagnostic overlay
   {
      getUIManager()->getDiagnosticUserInterface()->activate();
      playBoop();
   }
   else if(inputCode == keyOUTGAMECHAT)     // Turn on Global Chat overlay
   {
      getUIManager()->getChatUserInterface()->activate();
      playBoop();
   }
}


void TeamDefUserInterface::onMouseMoved(S32 x, S32 y)
{
   SDL_SetCursor(Cursor::getDefault());

   EditorUserInterface *ui = getUIManager()->getEditorUserInterface();

   S32 teams = ui->getTeamCount();

   selectedIndex = (S32)((gScreenInfo.getMousePos()->y - yStart + 6) / (fontsize + fontgap)) - 2; 

   if(selectedIndex >= teams)
      selectedIndex = teams - 1;

   if(selectedIndex < 0)
      selectedIndex = 0;
}


};

