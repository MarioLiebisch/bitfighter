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

#include "UINameEntry.h"
#include "UIMenus.h"
#include "UIGame.h"
#include "UIChat.h"
#include "UIEditor.h"
#include "ClientGame.h"
#include "gameConnection.h"
#include "config.h"
#include "md5wrapper.h"
#include "ScreenInfo.h"

#include "SDL/SDL_opengl.h"

#include <string>
#include <math.h>

namespace Zap
{
using namespace std;

// Constructor
TextEntryUserInterface::TextEntryUserInterface(ClientGame *game) : Parent(game)  
{
   setMenuID(TextEntryUI);
   title = "ENTER TEXT:";
   instr1 = "";
   instr2 = "Enter some text above";
   setSecret(false);
   cursorPos = 0;
   resetOnActivate = true;
   lineEditor = LineEditor(MAX_PLAYER_NAME_LENGTH);
}


void TextEntryUserInterface::onActivate()
{
   if(resetOnActivate)
      lineEditor.clear();
}


void TextEntryUserInterface::render()
{
   glColor3f(1,1,1);

   const S32 fontSize = 20;
   const S32 fontSizeBig = 30;
   const S32 canvasHeight = gScreenInfo.getGameCanvasHeight();

   S32 y = (canvasHeight / 2) - fontSize;

   drawCenteredString(y, fontSize, title);
   y += 45;
   glColor3f(0, 1, 0);
   drawCenteredString(canvasHeight - vertMargin - 2 * fontSize - 5, fontSize, instr1);
   drawCenteredString(canvasHeight - vertMargin - fontSize, fontSize, instr2);

   glColor3f(1,1,1);

   // this will have an effect of shrinking the text to fit on-screen when text get very long
   S32 w = getStringWidthf(fontSizeBig, lineEditor.getDisplayString().c_str());
   if(w > 750)
      w = 750 * fontSizeBig / w;
   else
      w = fontSizeBig;

   S32 x = drawCenteredString(y, w, lineEditor.getDisplayString().c_str());
   lineEditor.drawCursor(x, y, fontSizeBig);
}


void TextEntryUserInterface::idle(U32 timeDelta)
{
   LineEditor::updateCursorBlink(timeDelta);
}


void TextEntryUserInterface::onKeyDown(InputCode inputCode, char ascii)
{
   switch (inputCode)
   {
      case KEY_ENTER:
         onAccept(lineEditor.c_str());
         break;
      case KEY_BACKSPACE:
         lineEditor.backspacePressed();
         break;
      case KEY_DELETE:
         lineEditor.deletePressed();
         break;
      case KEY_ESCAPE:
         onEscape();
         break;
      default:
         lineEditor.addChar(ascii);
         break;
   }
}


void TextEntryUserInterface::setString(string str)
{
   lineEditor.setString(str);
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
LevelNameEntryUserInterface::LevelNameEntryUserInterface(ClientGame *game) : Parent(game)     
{
   setMenuID(LevelNameEntryUI);
   title = "ENTER LEVEL TO EDIT:";
   instr1 = "Enter an existing level, or create your own!";
   instr2 = "<- and -> keys retrieve existing level names";
   resetOnActivate = false;
   lineEditor.setFilter(LineEditor::fileNameFilter);
   lineEditor.mMaxLen = MAX_FILE_NAME_LEN;
}


void LevelNameEntryUserInterface::onEscape()
{
   playBoop();
   getUIManager()->reactivatePrevUI();      //gMainMenuUserInterface
}


void LevelNameEntryUserInterface::onActivate()
{
   Parent::onActivate();
   mLevelIndex = 0;

   mLevels = getGame()->getSettings()->getLevelList();

   // Remove the extension from the level file
   for(S32 i = 0; i < mLevels.size(); i++)
       mLevels[i] = stripExtension(mLevels[i]);

   // See if our current level is on the list -- if so, set mLevelIndex to that level
   for(S32 i = 0; i < mLevels.size(); i++)
      if(mLevels[i] == lineEditor.getString())
      {
         mLevelIndex = i;
         break;
      }
}


void LevelNameEntryUserInterface::onKeyDown(InputCode inputCode, char ascii)
{
   if(inputCode == KEY_RIGHT)
   {
      if(mLevels.size() == 0)
         return;

      mLevelIndex++;
      if(mLevelIndex >= mLevels.size())
         mLevelIndex = 0;

      lineEditor.setString(mLevels[mLevelIndex]);
   }
   else if(inputCode == KEY_LEFT)
   {
      if(mLevels.size() == 0)
         return;

      mLevelIndex--;
      if(mLevelIndex < 0)
         mLevelIndex = mLevels.size() - 1;

      lineEditor.setString(mLevels[mLevelIndex]);
   }
   else
      Parent::onKeyDown(inputCode, ascii);
}


extern CIniFile gINI;

void LevelNameEntryUserInterface::onAccept(const char *name)
{
   EditorUserInterface *ui = getUIManager()->getEditorUserInterface();
   ui->setLevelFileName(name);
   playBoop();
   ui->activate(false);
   
   // Get that baby into the INI file
   getGame()->getSettings()->getIniSettings()->lastEditorName = name;
   saveSettingsToINI(&gINI, getGame()->getSettings());             
   // Should be...
   //getGame()->getIniSettings()->saveSettingsToDisk();
}


////////////////////////////////////////
////////////////////////////////////////

void PasswordEntryUserInterface::render()
{
   const S32 canvasWidth = gScreenInfo.getGameCanvasWidth();
   const S32 canvasHeight = gScreenInfo.getGameCanvasHeight();

   if(getGame()->getConnectionToServer())
   {
      getUIManager()->getGameUserInterface()->render();

      glColor(Colors::black, 0.5);

      bool disableBlending = false;

      if(!glIsEnabled(GL_BLEND))
      {
         glEnable(GL_BLEND);
         disableBlending = true; 
      }

      glBegin(GL_POLYGON);
         glVertex2i(0,           0);
         glVertex2i(canvasWidth, 0);
         glVertex2i(canvasWidth, canvasHeight);
         glVertex2i(0,           canvasHeight);
      glEnd();

      if(disableBlending)
         glDisable(GL_BLEND);
   }

   Parent::render();
}


////////////////////////////////////////
////////////////////////////////////////

void PreGamePasswordEntryUserInterface::onAccept(const char *text)
{
   getGame()->joinGame(connectAddress, false, false);      // Not from master, not local
}


void PreGamePasswordEntryUserInterface::onEscape()
{
   getUIManager()->getMainMenuUserInterface()->activate();
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
ServerPasswordEntryUserInterface::ServerPasswordEntryUserInterface(ClientGame *game) : Parent(game)     
{
   setMenuID(PasswordEntryUI);
   title = "ENTER SERVER PASSWORD:";
   instr1 = "";
   instr2 = "Enter the password required for access to the server";
}


////////////////////////////////////////
////////////////////////////////////////

void InGamePasswordEntryUserInterface::onAccept(const char *text)
{
   GameConnection *gc = getGame()->getConnectionToServer();
   if(gc)
   {
      submitPassword(gc, text);

      getUIManager()->reactivatePrevUI();                                      // Reactivating clears subtitle message, so reactivate first...
      getUIManager()->getGameMenuUserInterface()->mMenuSubTitle = "** checking password **";     // ...then set the message
   }
   else
      getUIManager()->reactivatePrevUI();                                      // Otherwise, just reactivate the previous menu
}


void InGamePasswordEntryUserInterface::onEscape()
{
   getUIManager()->reactivatePrevUI();
}

////////////////////////////////////////
////////////////////////////////////////

// Constructor
AdminPasswordEntryUserInterface::AdminPasswordEntryUserInterface(ClientGame *game) : Parent(game)     
{
   setMenuID(AdminPasswordEntryUI);
   title = "ENTER ADMIN PASSWORD:";
   instr1 = "";
   instr2 = "Enter the admin password to perform admin tasks and change levels on this server";
}

AdminPasswordEntryUserInterface::~AdminPasswordEntryUserInterface()
{
}

void AdminPasswordEntryUserInterface::submitPassword(GameConnection *gameConnection, const char *text)
{
   gameConnection->submitAdminPassword(text);
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
LevelChangePasswordEntryUserInterface::LevelChangePasswordEntryUserInterface(ClientGame *game) : Parent(game)     
{
   setMenuID(LevelChangePasswordEntryUI);
   title = "ENTER LEVEL CHANGE PASSWORD:";
   instr1 = "";
   instr2 = "Enter the level change password to change levels on this server";
}

LevelChangePasswordEntryUserInterface::~LevelChangePasswordEntryUserInterface()
{
}


void LevelChangePasswordEntryUserInterface::submitPassword(GameConnection *gameConnection, const char *text)
{
   gameConnection->submitLevelChangePassword(text);
}


////////////////////////////////////////
////////////////////////////////////////
};


