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

#include "OpenglUtils.h"

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
   Parent::idle(timeDelta);
}


void TextEntryUserInterface::setSecret(bool secret)
{
   lineEditor.setSecret(secret);
}


const char *TextEntryUserInterface::getText()
{
   return lineEditor.c_str();
}


string TextEntryUserInterface::getSaltedHashText()
{
   return md5.getSaltedHashFromString(lineEditor.getString());
}


bool TextEntryUserInterface::onKeyDown(InputCode inputCode)
{
   if(Parent::onKeyDown(inputCode))
      return true;

   switch (inputCode)
   {
      case KEY_ENTER:
         onAccept(lineEditor.c_str());
         return true;
      case KEY_BACKSPACE:
         lineEditor.backspacePressed();
         return true;
      case KEY_DELETE:
         lineEditor.deletePressed();
         return true;
      case KEY_ESCAPE:
         onEscape();
         return true;
      default:
         return false;
   }
}


void TextEntryUserInterface::onTextInput(char ascii)
{
   lineEditor.addChar(ascii);
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
   instr2 = "Arrows / wheel cycle existing levels | Tab completes partial name";
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

   mFoundLevel = setLevelIndex();
}


// See if the current level is on the list -- if so, set mLevelIndex to that level and return true
bool LevelNameEntryUserInterface::setLevelIndex()
{
   for(S32 i = 0; i < mLevels.size(); i++)
   {
      // Exact match
      if(mLevels[i] == lineEditor.getString())
      {
         mLevelIndex = i;
         return true;
      }
      
      // No exact match, but we just passed the item and have selected the closest one alphabetically following
      if(mLevels[i] > lineEditor.getString())
      {
         mLevelIndex = i;
         return false;
      }
   }

   mLevelIndex = 0;     // First item
   return false;
}


bool LevelNameEntryUserInterface::onKeyDown(InputCode inputCode)
{
   if(Parent::onKeyDown(inputCode))
   { 
      // Do nothing -- key handled
   }
   else if(inputCode == KEY_RIGHT || inputCode == KEY_DOWN || inputCode == MOUSE_WHEEL_DOWN)
   {
      if(mLevels.size() == 0)
         return true;

      if(!mFoundLevel)           // If we have a partially entered name, just simulate hitting tab 
      {
         completePartial();      // Resets mFoundLevel
         if(!mFoundLevel)
            mLevelIndex--;       // Counteract increment below
      }

      mLevelIndex++;
      if(mLevelIndex >= mLevels.size())
         mLevelIndex = 0;

      lineEditor.setString(mLevels[mLevelIndex]);
   }

   else if(inputCode == KEY_LEFT || inputCode == KEY_UP || inputCode == MOUSE_WHEEL_UP)
   {
      if(mLevels.size() == 0)
         return true;

      if(!mFoundLevel)
         completePartial();

      mLevelIndex--;
      if(mLevelIndex < 0)
         mLevelIndex = mLevels.size() - 1;

      lineEditor.setString(mLevels[mLevelIndex]);
   }

   else if(inputCode == KEY_TAB)       // Tab will try to complete a name from whatever the user has already typed
      completePartial();

   else                                // Normal typed key - not handled
   {
      mFoundLevel = setLevelIndex();   // Update levelIndex to reflect current level
      return false;
   }

   // Something was handled!
   return true;
}


void LevelNameEntryUserInterface::completePartial()
{
   mFoundLevel = setLevelIndex();
   lineEditor.completePartial(&mLevels, lineEditor.getString(), 0, ""); 
   setLevelIndex();   // Update levelIndex to reflect current level
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

// Constructor
PasswordEntryUserInterface::PasswordEntryUserInterface(ClientGame *game) : Parent(game)
{
   setSecret(true);
}


void PasswordEntryUserInterface::render()
{
   const S32 canvasWidth = gScreenInfo.getGameCanvasWidth();
   const S32 canvasHeight = gScreenInfo.getGameCanvasHeight();

   if(getGame()->getConnectionToServer())
   {
      getUIManager()->getGameUserInterface()->render();

      glColor(Colors::black, 0.5);

      TNLAssert(glIsEnabled(GL_BLEND), "Why is blending off here?");

      glBegin(GL_POLYGON);
         glVertex2i(0,           0);
         glVertex2i(canvasWidth, 0);
         glVertex2i(canvasWidth, canvasHeight);
         glVertex2i(0,           canvasHeight);
      glEnd();
   }

   Parent::render();
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
PreGamePasswordEntryUserInterface::PreGamePasswordEntryUserInterface(ClientGame *game) :
      Parent(game)
{
   /* Do nothing */
}


PreGamePasswordEntryUserInterface::~PreGamePasswordEntryUserInterface()
{
   // Do nothing
}


void PreGamePasswordEntryUserInterface::onAccept(const char *text)
{
   getGame()->joinGame(connectAddress, false, false); // Not from master, not local
}


void PreGamePasswordEntryUserInterface::onEscape()
{
   getUIManager()->getMainMenuUserInterface()->activate();
}


void PreGamePasswordEntryUserInterface::setConnectServer(const Address &addr)
{
   connectAddress = addr;
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

// Constructor
InGamePasswordEntryUserInterface::InGamePasswordEntryUserInterface(ClientGame *game) :
      Parent(game)
{
   /* Do nothing */
}


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
LevelChangeOrAdminPasswordEntryUserInterface::LevelChangeOrAdminPasswordEntryUserInterface(ClientGame *game) : Parent(game)     
{
   setMenuID(LevelChangePasswordEntryUI);
   title = "ENTER PASSWORD:";
   instr1 = "";
   instr2 = "Enter the level change or admin password to change levels on this server";
}

LevelChangeOrAdminPasswordEntryUserInterface::~LevelChangeOrAdminPasswordEntryUserInterface()
{
}


void LevelChangeOrAdminPasswordEntryUserInterface::submitPassword(GameConnection *gameConnection, const char *text)
{
   gameConnection->submitPassword(text);
}


////////////////////////////////////////
////////////////////////////////////////
};


