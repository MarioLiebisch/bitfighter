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

#include "UIMenus.h"
#include "UINameEntry.h"
#include "UIGame.h"
#include "UIQueryServers.h"
#include "UICredits.h"
#include "UIChat.h"
#include "UIEditor.h"
#include "UIInstructions.h"
#include "UIDiagnostics.h"
#include "UIKeyDefMenu.h"
#include "UIErrorMessage.h"
#include "gameObjectRender.h"    // For renderBitfighterLogo, glColor
#include "ClientGame.h"
#include "gameType.h"
#include "input.h"
#include "IniFile.h"
#include "config.h"
#include "Colors.h"
#include "ScreenInfo.h"
#include "Joystick.h"
#include "JoystickRender.h"

#include "SDL/SDL.h"
#include "SDL/SDL_opengl.h"

#include <algorithm>
#include <string>
#include <math.h>

namespace Zap
{

// Sorts alphanumerically by menuItem's prompt  ==> used for getting levels in the right order and such
S32 QSORT_CALLBACK menuItemValueSort(boost::shared_ptr<MenuItem> *a, boost::shared_ptr<MenuItem> *b)
{
   return stricmp((*a)->getPrompt().c_str(), (*b)->getPrompt().c_str());
}


extern void actualizeScreenMode(bool);
extern void shutdownBitfighter();

////////////////////////////////////
////////////////////////////////////

// Max number of menu items we show on screen before we go into scrolling mode
#define MAX_MENU_SIZE S32((gScreenInfo.getGameCanvasHeight() - 150) / (getTextSize() + getGap()))   

// Constructor
MenuUserInterface::MenuUserInterface(ClientGame *game) : UserInterface(game)
{
   initialize();
}


MenuUserInterface::MenuUserInterface(ClientGame *game, const string &title) : UserInterface(game)
{
   initialize();

   mMenuTitle = title;
}


void MenuUserInterface::initialize()
{
   setMenuID(GenericUI);
   mMenuTitle = "Menu:";
   mMenuSubTitle = "";

   selectedIndex = 0;
   itemSelectedWithMouse = false;
   mFirstVisibleItem = 0;
   mRenderInstructions = true;
}


// Gets run when menu is activated.  This is also called by almost all other menus/subclasses.
void MenuUserInterface::onActivate()
{
   mDisableShipKeyboardInput = true;       // Keep keystrokes from getting to game
   selectedIndex = 0;
   mFirstVisibleItem = 0;
}


void MenuUserInterface::onReactivate()
{
   mDisableShipKeyboardInput = true;       // Keep keystrokes from getting to game
}


void MenuUserInterface::clearMenuItems()
{
   mMenuItems.clear();
}


void MenuUserInterface::sortMenuItems()
{
   mMenuItems.sort(menuItemValueSort);
}


void MenuUserInterface::addMenuItem(MenuItem *menuItem)
{
   menuItem->setMenu(this);
   mMenuItems.push_back(boost::shared_ptr<MenuItem>(menuItem));
}


// For those times when you really need to add a pre-packaged menu item... normally, you won't need to do this
void MenuUserInterface::addWrappedMenuItem(boost::shared_ptr<MenuItem> menuItem)
{
   menuItem->setMenu(this);
   mMenuItems.push_back(menuItem);
}


S32 MenuUserInterface::getMenuItemCount()
{
   return mMenuItems.size();
}


MenuItem *MenuUserInterface::getLastMenuItem()
{
   return mMenuItems.last().get();
}


MenuItem *MenuUserInterface::getMenuItem(S32 index)
{
   return mMenuItems[index].get();
}


void MenuUserInterface::idle(U32 timeDelta)
{
   // Controls rate of scrolling long menus with mouse
   mScrollTimer.update(timeDelta);

   // Call mouse handler so users can scroll scrolling menus just by holding mouse in position
   // (i.e. we don't want to limit scrolling action only to times when user moves mouse)
   if(itemSelectedWithMouse)
      processMouse();
}


// Return index offset to account for scrolling menus
S32 MenuUserInterface::getOffset()
{
   S32 offset = 0;
   S32 count = mMenuItems.size();

   if(count > MAX_MENU_SIZE)     // Do some sort of scrolling
   {
      // itemSelectedWithMouse basically lets users highlight the top and bottom items in a scrolling list,
      // which can't be done when using the keyboard
      if(selectedIndex - mFirstVisibleItem < (itemSelectedWithMouse ? 0 : 1))
         offset = selectedIndex - (itemSelectedWithMouse ? 0 : 1);
      else if( selectedIndex - mFirstVisibleItem > (MAX_MENU_SIZE - (itemSelectedWithMouse ? 1 : 2)) )
         offset = selectedIndex - (MAX_MENU_SIZE - (itemSelectedWithMouse ? 1 : 2));
      else offset = mFirstVisibleItem;

      if(offset < 0)
         offset = 0;
      else if(offset + MAX_MENU_SIZE >= mMenuItems.size())
         offset = mMenuItems.size() - MAX_MENU_SIZE;
   }

   mFirstVisibleItem = offset;

   return offset;
}


// Get vert pos of first menu item
S32 MenuUserInterface::getYStart()
{
   S32 vertOff = (getMenuID() == MainUI) ? 40 : 0;    // Make room for the logo on the main menu

   if(getMenuID() == GameParamsUI)  // If we're on the GameParams menu, start at a constant position
      return 70;
   else                             // Otherwise, attpempt to center the menu vertically
      return (gScreenInfo.getGameCanvasHeight() - min(mMenuItems.size(), MAX_MENU_SIZE) * (getTextSize() + getGap())) / 2 + vertOff;
}


static void renderMenuInstructions(GameSettings *settings)
{
   S32 canvasWidth = gScreenInfo.getGameCanvasWidth();
   S32 canvasHeight = gScreenInfo.getGameCanvasHeight();

   S32 y = canvasHeight - UserInterface::vertMargin - 20;
   const S32 size = 18;

   glColor(Colors::white);

   U32 joystickType = settings->getIniSettings()->joystickType;

   if(settings->getIniSettings()->inputMode == InputModeKeyboard)
     UserInterface::drawCenteredString(y, size, "UP, DOWN to choose | ENTER to select | ESC exits menu");
   else
   {
     S32 totalWidth = JoystickRender::getControllerButtonRenderedSize(joystickType, BUTTON_DPAD_UP) +
                      JoystickRender::getControllerButtonRenderedSize(joystickType, BUTTON_DPAD_DOWN) +
                      JoystickRender::getControllerButtonRenderedSize(joystickType, BUTTON_START) +
                      JoystickRender::getControllerButtonRenderedSize(joystickType, BUTTON_BACK) +
                      UserInterface::getStringWidth(size, "to choose |  to select |  exits menu");

     S32 x = canvasWidth / 2 - UserInterface::horizMargin - totalWidth/2;

     JoystickRender::renderControllerButton((F32)x, (F32)y, joystickType, BUTTON_DPAD_UP, false);
     x += JoystickRender::getControllerButtonRenderedSize(joystickType, BUTTON_DPAD_UP) + UserInterface::getStringWidth(size, " ");

     JoystickRender::renderControllerButton((F32)x, (F32)y, joystickType, BUTTON_DPAD_DOWN, false);
     x += JoystickRender::getControllerButtonRenderedSize(joystickType, BUTTON_DPAD_DOWN) + UserInterface::getStringWidth(size, " ");

     glColor(Colors::white);
     static const char *msg1 = "to choose | ";

     UserInterface::drawString(x, y, size, msg1);
     x += UserInterface::getStringWidth(size, msg1);

     JoystickRender::renderControllerButton((F32)x, F32(y + 4), joystickType, BUTTON_START, false);
     x += JoystickRender::getControllerButtonRenderedSize(joystickType, BUTTON_START);

     glColor(Colors::white);
     static const char *msg2 = "to select | ";
     UserInterface::drawString(x, y, size, msg2);
     x += UserInterface::getStringWidth(size, msg2);

     JoystickRender::renderControllerButton(F32(x + 4), F32(y + 4), joystickType, BUTTON_BACK, false);
     x += JoystickRender::getControllerButtonRenderedSize(joystickType, BUTTON_BACK) + 4;

     glColor(Colors::white);
     UserInterface::drawString(x, y, size, "exits menu");
   }
}


static const S32 ARROW_WIDTH = 100;
static const S32 ARROW_HEIGHT = 20;
static const S32 ARROW_MARGIN = 5;

static void renderArrowAbove(S32 pos, S32 height)
{
   S32 canvasWidth = gScreenInfo.getGameCanvasWidth();

   for(S32 i = 1; i >= 0; i--)
   {
      // First create a black poly to blot out what's behind, then the arrow itself
      glColor(i ? Colors::black : Colors::blue);
      glBegin(i ? GL_POLYGON : GL_LINE_LOOP);
         glVertex2i( (canvasWidth - ARROW_WIDTH) / 2, pos - ARROW_MARGIN - 7);
         glVertex2i( (canvasWidth + ARROW_WIDTH) / 2, pos - ARROW_MARGIN - 7);
         glVertex2i(canvasWidth / 2, pos - (height + ARROW_MARGIN ) - 7);
      glEnd();
   }
}


static void renderArrowBelow(S32 pos, S32 height)
{
   S32 canvasWidth = gScreenInfo.getGameCanvasWidth();
   for(S32 i = 1; i >= 0; i--)
   {
      // First create a black poly to blot out what's behind, then the arrow itself
      glColor(i ? Colors::black : Colors::blue);
      glBegin(i ? GL_POLYGON : GL_LINE_LOOP);
         glVertex2i( (canvasWidth - ARROW_WIDTH) / 2, pos + ARROW_MARGIN - 7);
         glVertex2i( (canvasWidth + ARROW_WIDTH) / 2, pos + ARROW_MARGIN - 7);
         glVertex2i(canvasWidth / 2, pos + (height + ARROW_MARGIN) - 7);
      glEnd();
   }
}


// Basic menu rendering
void MenuUserInterface::render()
{
   S32 canvasWidth = gScreenInfo.getGameCanvasWidth();
   S32 canvasHeight = gScreenInfo.getGameCanvasHeight();

   // Draw the game screen, then dim it out so you can still see it under our overlay
   if(getGame()->getConnectionToServer())
   {
      if(getUIManager()->getGameUserInterface())
         getUIManager()->getGameUserInterface()->render();

      glColor(Colors::black, 0.6f);

      bool disableBlending = false;

      if(!glIsEnabled(GL_BLEND))
      {
         glEnable(GL_BLEND);
         disableBlending = true; 
      }

      glBegin(GL_POLYGON);
         glVertex2i(0, 0);
         glVertex2i(canvasWidth, 0);
         glVertex2i(canvasWidth, canvasHeight);
         glVertex2i(0, canvasHeight);
      glEnd();

      if(disableBlending)
         glDisable(GL_BLEND);
   }

   glColor(Colors::white);    
   drawCenteredString(vertMargin, 30, mMenuTitle.c_str());

   glColor(mMenuSubTitleColor);
   drawCenteredString(vertMargin + 35, 18, mMenuSubTitle.c_str());

   if(mRenderInstructions)
      renderMenuInstructions(getGame()->getSettings());

   S32 count = mMenuItems.size();

   if(count > MAX_MENU_SIZE)     // Need some sort of scrolling?
      count = MAX_MENU_SIZE;

   S32 yStart = getYStart();
   S32 offset = getOffset();

   // Just because it looks good (was 2, which looks crappy on gameParams menu.  0 seems to look ok everywhere.
   S32 adjfact = 0;    
   S32 shrinkfact = 1;

   for(S32 i = 0; i < count; i++)
   {
      S32 y = yStart + i * (getTextSize() + getGap());

      if(selectedIndex == i + offset)  // Highlight selected item
         drawMenuItemHighlight(0,           y - getGap() / 2 + adjfact + shrinkfact, 
                               canvasWidth, y + getTextSize() + getGap() / 2 + adjfact - shrinkfact);

      mMenuItems[i+offset]->render(y, getTextSize(), selectedIndex == i+offset);
   }

   // Render an indicator that there are scrollable items above and/or below
   if(mMenuItems.size() > MAX_MENU_SIZE)
   {
      glColor(Colors::blue);

      if(offset > 0)                                     // There are items above
         renderArrowAbove(yStart, ARROW_HEIGHT);

      if(offset < mMenuItems.size() - MAX_MENU_SIZE)     // There are items below
         renderArrowBelow(yStart + (getTextSize() + getGap()) * MAX_MENU_SIZE, ARROW_HEIGHT);
   }

   // Render a help string at the bottom of the menu
   const S32 helpFontSize = 15;
   glColor(Colors::green);
   S32 ypos = canvasHeight - vertMargin - 50;

   // Render a special instruction line (should this be a method of CounterMenuItemType?
   UserInterface::drawCenteredString(ypos, helpFontSize, mMenuItems[selectedIndex]->getSpecialEditingInstructions() );

   ypos -= helpFontSize + 5;
   drawCenteredString(ypos, helpFontSize, mMenuItems[selectedIndex]->getHelp());

   renderExtras();  // Draw something unique on a menu
}


// Fill responses with values from each menu item in turn
void MenuUserInterface::getMenuResponses(Vector<string> &responses)
{
   for(S32 i = 0; i < mMenuItems.size(); i++)
      responses.push_back(mMenuItems[i]->getValue());
}


// Handle mouse input, figure out which menu item we're over, and highlight it
void MenuUserInterface::onMouseMoved()
{
   // Really only matters when starting to host game... don't want to be able to change menu items while the levels are loading.
   // This is purely an aesthetic issue, a minor irritant.
   if(gServerGame && gServerGame->hostingModePhase == ServerGame::LoadingLevels)
      return;

   itemSelectedWithMouse = true;
   SDL_ShowCursor(SDL_ENABLE);  // Show cursor when user moves mouse

   selectedIndex = getSelectedMenuItem();

   processMouse();
}


S32 MenuUserInterface::getSelectedMenuItem()
{
   return S32( floor(( gScreenInfo.getMousePos()->y - getYStart() + 10 ) / (getTextSize() + getGap())) ) + mFirstVisibleItem;
}


void MenuUserInterface::processMouse()
{
   if(mMenuItems.size() > MAX_MENU_SIZE)   // We have a scrolling situation here...
   {
      if(selectedIndex < mFirstVisibleItem)      // Scroll up
      {
         if(!mScrollTimer.getCurrent() && mFirstVisibleItem > 0)
         {
            mFirstVisibleItem--;
            mScrollTimer.reset(100);
         }
         selectedIndex = mFirstVisibleItem;
      }
      else if(selectedIndex > mFirstVisibleItem + MAX_MENU_SIZE - 1)   // Scroll down
      {
         if(!mScrollTimer.getCurrent() && selectedIndex > mFirstVisibleItem + MAX_MENU_SIZE - 2)
         {
            mFirstVisibleItem++;
            mScrollTimer.reset(MOUSE_SCROLL_INTERVAL);
         }
         selectedIndex = mFirstVisibleItem + MAX_MENU_SIZE - 1;
      }
      else
         mScrollTimer.clear();
   }

   if(selectedIndex < 0)                              // Scrolled off top of list
   {
      selectedIndex = 0;
      mFirstVisibleItem = 0;
   }
   else if(selectedIndex >= mMenuItems.size())         // Scrolled off bottom of list
   {
      selectedIndex = mMenuItems.size() - 1;
      mFirstVisibleItem = max(mMenuItems.size() - MAX_MENU_SIZE, 0);
   }
}


void MenuUserInterface::onKeyDown(InputCode inputCode, char ascii)
{
   if(inputCode == KEY_UNKNOWN)
      return;

   // Check for in autorepeat mode
   mRepeatMode = mKeyDown;
   mKeyDown = true;

   // Handle special case of keystrokes during hosting preparation phases
   if(gServerGame && (gServerGame->hostingModePhase == ServerGame::LoadingLevels || 
                      gServerGame->hostingModePhase == ServerGame::DoneLoadingLevels))
   {
      if(inputCode == KEY_ESCAPE)     // Can only get here when hosting
      {
         gServerGame->hostingModePhase = ServerGame::NotHosting;
         getUIManager()->getHostMenuUserInterface()->clearLevelLoadDisplay();
         getGame()->closeConnectionToGameServer();

         delete gServerGame;
         gServerGame = NULL;

      }
      return;
   }

   MainMenuUserInterface *ui = getUIManager()->getMainMenuUserInterface();

   if(!ui->mFirstTime)
      ui->showAnimation = false;    // Stop animations if a key is pressed

   mMenuItems[selectedIndex]->handleKey(inputCode, ascii) || processMenuSpecificKeys(inputCode, ascii) || processKeys(inputCode, ascii);

   // Finally, since the user has indicated they want to use keyboard/controller input, hide the cursor
   if(inputCode != MOUSE_LEFT && inputCode != MOUSE_MIDDLE && inputCode != MOUSE_RIGHT && inputCode != KEY_ESCAPE)
      SDL_ShowCursor(SDL_DISABLE);
}


void MenuUserInterface::onKeyUp(InputCode inputCode)
{
   mKeyDown = false;
   mRepeatMode = false;
}


// Generic handler looks for keystrokes and translates them into menu actions
bool MenuUserInterface::processMenuSpecificKeys(InputCode inputCode, char ascii)
{
   // First check for some shortcut keys

   for(S32 i = 0; i < mMenuItems.size(); i++)
   {
      if(inputCode == mMenuItems[i]->key1 || inputCode == mMenuItems[i]->key2)
      {
         selectedIndex = i;

         mMenuItems[i]->activatedWithShortcutKey();
         return true;
      }
   }

   return false;
}


// Process the keys that work on all menus
bool MenuUserInterface::processKeys(InputCode inputCode, char ascii)
{
   inputCode = convertJoystickToKeyboard(inputCode);

   if(inputCode == KEY_LEFT || inputCode == KEY_RIGHT || inputCode == MOUSE_LEFT || inputCode == MOUSE_RIGHT)
   {
      mMenuItems[selectedIndex]->handleKey(inputCode, ascii);
      playBoop();
   }

   else if(inputCode == KEY_SPACE || inputCode == KEY_RIGHT || inputCode == KEY_ENTER || inputCode == MOUSE_LEFT)
   {
      playBoop();
      if(inputCode != MOUSE_LEFT)
         itemSelectedWithMouse = false;

      else // it was MOUSE_LEFT after all
      {
         // Make sure we're actually pointing at a menu item before we process it
         S32 yStart = getYStart();
         const Point *mousePos = gScreenInfo.getMousePos();

         if(mousePos->y < yStart || mousePos->y > yStart + (mMenuItems.size() + 1) * (getTextSize() + getGap()))
            return true;
      }

      mMenuItems[selectedIndex]->handleKey(inputCode, ascii);

      if(mMenuItems[selectedIndex]->enterAdvancesItem())
         advanceItem();
   }

   else if(inputCode == KEY_ESCAPE)
   {
      playBoop();
      onEscape();
   }
   else if(inputCode == KEY_UP || (inputCode == KEY_TAB && checkModifier(KEY_SHIFT)))   // Prev item
   {
      selectedIndex--;
      itemSelectedWithMouse = false;

      if(selectedIndex < 0)                        // Scrolling off the top
      {
         if((mMenuItems.size() > MAX_MENU_SIZE) && mRepeatMode)        // Allow wrapping on long menus only when not in repeat mode
         {
            selectedIndex = 0;               // No wrap --> (first item)
            return true;                     // (leave before playBoop)
         }
         else                                      // Always wrap on shorter menus
            selectedIndex = mMenuItems.size() - 1;  // Wrap --> (select last item)
      }
      playBoop();
   }

   else if(inputCode == KEY_DOWN || inputCode == KEY_TAB)    // Next item
      advanceItem();

   else if(inputCode == keyOUTGAMECHAT)     // Turn on Global Chat overlay
   {
      getUIManager()->getChatUserInterface()->activate();
      playBoop();
   }
   else if(inputCode == keyDIAG)            // Turn on diagnostic overlay
   {
      getUIManager()->getDiagnosticUserInterface()->activate();
      playBoop();
   }

   return true;      // Probably wrong, but doesn't really matter at this point
}


void MenuUserInterface::advanceItem()
{
   selectedIndex++;
   itemSelectedWithMouse = false;

   if(selectedIndex >= mMenuItems.size())     // Scrolling off the bottom
   {
      if((mMenuItems.size() > MAX_MENU_SIZE) && mRepeatMode)     // Allow wrapping on long menus only when not in repeat mode
      {
         selectedIndex = getMenuItemCount() - 1;                 // No wrap --> (last item)
         return;                                                 // (leave before playBoop)
      }
      else                     // Always wrap on shorter menus
         selectedIndex = 0;    // Wrap --> (first item)
   }
   playBoop();
}


void MenuUserInterface::onEscape()
{
   // Do nothing
}


////////////////////////////////////////
////////////////////////////////////////


//////////
// MainMenuUserInterface callbacks
//////////

static void joinSelectedCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->getQueryServersUserInterface()->activate();
}

static void hostSelectedCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->getHostMenuUserInterface()->activate();
}

static void helpSelectedCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->getInstructionsUserInterface()->activate();
}

static void optionsSelectedCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->getOptionsMenuUserInterface()->activate();
}

static void editorSelectedCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->getEditorUserInterface()->setLevelFileName("");      // Reset this so we get the level entry screen
   game->getUIManager()->getEditorUserInterface()->activate();
}


static void creditsSelectedCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->getCreditsUserInterface()->activate();
}


static void quitSelectedCallback(ClientGame *game, U32 unused)
{
   shutdownBitfighter();
}

//////////

// Constructor
MainMenuUserInterface::MainMenuUserInterface(ClientGame *game) : Parent(game)
{
   showAnimation = true;
   mFirstTime = true;
   setMenuID(MainUI);
   mMenuTitle = "";
   mMOTD[0] = 0;
   mMenuSubTitle = "";
   mRenderInstructions = false;

   mNeedToUpgrade = false;           // Assume we're up-to-date until we hear from the master
   mShowedUpgradeAlert = false;      // So we don't show the upgrade message more than once

   addMenuItem(new MenuItem("JOIN LAN/INTERNET GAME", joinSelectedCallback,    "", KEY_J));
   addMenuItem(new MenuItem("HOST GAME",              hostSelectedCallback,    "", KEY_H));
   addMenuItem(new MenuItem("INSTRUCTIONS",           helpSelectedCallback,    "", KEY_I, keyHELP));
   addMenuItem(new MenuItem("OPTIONS",                optionsSelectedCallback, "", KEY_O));
   addMenuItem(new MenuItem("LEVEL EDITOR",           editorSelectedCallback,  "", KEY_L, KEY_E));
   addMenuItem(new MenuItem("CREDITS",                creditsSelectedCallback, "", KEY_C));
   addMenuItem(new MenuItem("QUIT",                   quitSelectedCallback,    "", KEY_Q));
}


void MainMenuUserInterface::onActivate()
{
   // Time for a clean start.  No matter how we got here, there's no going back.
   // Needed mainly because the editor makes things confusing.  Now that that's been reworked,
   // it's probably not needed at all.
   getUIManager()->clearPrevUIs();

   mFadeInTimer.reset(FadeInTime);
   mColorTimer.reset(ColorTime);
   mColorTimer2.reset(ColorTime2);
   mTransDir = true;

   mFirstTime = false;

   if(showAnimation)
      getUIManager()->getSplashUserInterface()->activate();   // Show splash screen the first time through
}


// Set the MOTD we recieved from the master
void MainMenuUserInterface::setMOTD(const char *motd)
{
   strncpy(mMOTD, motd, MOTD_LEN);     

   motdArriveTime = getGame()->getCurrentTime();    // Used for scrolling the message
}


// Set needToUpgrade flag that tells us the client is out-of-date
void MainMenuUserInterface::setNeedToUpgrade(bool needToUpgrade)
{
   mNeedToUpgrade = needToUpgrade;

   if(mNeedToUpgrade && !mShowedUpgradeAlert)
      showUpgradeAlert();
}

static const S32 MOTD_POS = 540;

void MainMenuUserInterface::render()
{
   Parent::render();

   S32 canvasWidth = gScreenInfo.getGameCanvasWidth();
   S32 canvasHeight = gScreenInfo.getGameCanvasHeight();

   if(mMOTD != "")
   {
      U32 width = getStringWidth(20, mMOTD);
      glColor(Colors::white);
      U32 totalWidth = width + canvasWidth;
      U32 pixelsPerSec = 100;
      U32 delta = getGame()->getCurrentTime() - motdArriveTime;
      delta = U32(delta * pixelsPerSec * 0.001) % totalWidth;

      drawString(canvasWidth - delta, MOTD_POS, 20, mMOTD);
   }

   // Fade in the menu here if we are showing it the first time...  this will tie in
   // nicely with the splash screen, and make the transition less jarring and sudden
   if(showAnimation)
   {
      bool disableBlending = false;

      if(!glIsEnabled(GL_BLEND))
      {
         glEnable(GL_BLEND);
         disableBlending = true; 
      }
       
      glBegin(GL_POLYGON);
         glColor(Colors::black, (F32) mFadeInTimer.getCurrent() / (F32) FadeInTime);
         glVertex2i(0, 0);
         glVertex2i(canvasWidth, 0);
         glVertex2i(canvasWidth, canvasHeight);
         glVertex2i(0, canvasHeight);
      glEnd();
   
      if(disableBlending)
         glDisable(GL_BLEND);
   }

   // Render logo at top, never faded
   renderStaticBitfighterLogo();
}


void MainMenuUserInterface::idle(U32 timeDelta)
{
   mFadeInTimer.update(timeDelta);
   if(mColorTimer.update(timeDelta))
   {
      mColorTimer.reset(ColorTime);
      mTransDir = !mTransDir;
   }

   if(mColorTimer2.update(timeDelta))
   {
      mColorTimer2.reset(ColorTime2);
      mTransDir2 = !mTransDir2;
   }
}


bool MainMenuUserInterface::getNeedToUpgrade()
{
   return mNeedToUpgrade;
}


void MainMenuUserInterface::renderExtras()
{
   glColor(Colors::white);
   const S32 size = 20;
   drawCenteredString(gScreenInfo.getGameCanvasHeight() - vertMargin - size, size, "join us @ www.bitfighter.org");
}


void MainMenuUserInterface::showUpgradeAlert()
{
   ErrorMessageUserInterface *ui = getUIManager()->getErrorMsgUserInterface();

   ui->reset();
   ui->setTitle("OUTDATED VERSION");
   ui->setMessage(1, "You are running an older version of Bitfighter.");
   ui->setMessage(2, "You will only be able to play with players who still");
   ui->setMessage(3, "have the same outdated version.");
   ui->setMessage(4, "");
   ui->setMessage(5, "To get the latest, visit bitfighter.org");

   ui->activate();

   mShowedUpgradeAlert = true;            // Only show this alert once per session -- we don't need to beat them over the head with it!
}


// Take action based on menu selection
void MainMenuUserInterface::processSelection(U32 index)
{
   if(!mFirstTime)
      showAnimation = false;
}


void MainMenuUserInterface::onEscape()
{
   shutdownBitfighter();    // Quit!
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
OptionsMenuUserInterface::OptionsMenuUserInterface(ClientGame *game) : Parent(game)
{
   setMenuID(OptionsUI);
   mMenuTitle = "OPTIONS MENU:";
}


void OptionsMenuUserInterface::onActivate()
{
   Parent::onActivate();
   setupMenus();
}


static string getVolMsg(F32 volume)
{
   S32 vol = U32((volume + 0.05) * 10.0);

   string msg = itos(vol);

   if(vol == 0)
      msg += " [MUTE]";

   return msg;
}


//////////
// Callbacks for Options menu
static void setSFXVolumeCallback(ClientGame *game, U32 vol)
{
   game->getSettings()->getIniSettings()->sfxVolLevel = F32(vol) / 10;
}

static void setMusicVolumeCallback(ClientGame *game, U32 vol)
{
   game->getSettings()->getIniSettings()->musicVolLevel = F32(vol) / 10;
}

static void setVoiceVolumeCallback(ClientGame *game, U32 vol)
{
   game->getSettings()->getIniSettings()->voiceChatVolLevel = F32(vol) / 10;
}


static void setControlsCallback(ClientGame *game, U32 val)
{
   game->getSettings()->getIniSettings()->controlsRelative = (val == 1);
}


static void setFullscreenCallback(ClientGame *game, U32 mode)
{
   // Save existing setting
   game->getSettings()->getIniSettings()->oldDisplayMode = game->getSettings()->getIniSettings()->displayMode;     

   game->getSettings()->getIniSettings()->displayMode = (DisplayMode)mode;
   actualizeScreenMode(false);
}


static void defineKeysCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->getKeyDefMenuUserInterface()->activate();
}

static void setControllerCallback(ClientGame *game, U32 jsType)
{
   game->getSettings()->getIniSettings()->joystickType = jsType;
}


static void addStickOptions(Vector<string> *opts)
{
   opts->clear();
   opts->push_back("KEYBOARD");
   
   for(S32 i = 0; i < Joystick::DetectedJoystickNameList.size(); i++)
      opts->push_back(string("JOYSTICK ") + itos(i+1));
}


static S32 INPUT_MODE_MENU_ITEM_INDEX = 0;

static void setInputModeCallback(ClientGame *game, U32 val)
{
   S32 sticks = Joystick::DetectedJoystickNameList.size();

   if(sticks != Joystick::DetectedJoystickNameList.size())
   {
      ToggleMenuItem *menuItem = dynamic_cast<ToggleMenuItem *>(game->getUIManager()->getOptionsMenuUserInterface()->
                                                                getMenuItem(INPUT_MODE_MENU_ITEM_INDEX));

      if(menuItem)
         addStickOptions(&menuItem->mOptions);

      if(val > (U32)Joystick::DetectedJoystickNameList.size())
      {
         val = 0;
         menuItem->setValueIndex(0);
      }

      // Special case handler for common situation
      if(sticks == 0 && Joystick::DetectedJoystickNameList.size() == 1)      // User just plugged a stick in
         menuItem->setValueIndex(1);
   }

   game->getSettings()->getIniSettings()->inputMode = (val == 0) ? InputModeKeyboard : InputModeJoystick;
   if(val >= 1) 
      Joystick::UseJoystickNumber = val - 1;

   Joystick::initJoystick();      // Will allow people to plug in joystick while in this menu...
}


static void setVoiceEchoCallback(ClientGame *game, U32 val)
{
   game->getSettings()->getIniSettings()->echoVoice = (val == 1);
}

//////////

MenuItem *getWindowModeMenuItem(U32 displayMode)
{
   Vector<string> opts;   
   opts.push_back("WINDOWED");
   opts.push_back("FULLSCREEN STRETCHED");
   opts.push_back("FULLSCREEN");

   return new ToggleMenuItem("DISPLAY MODE:", opts, displayMode, true, 
                             setFullscreenCallback, "Set the game mode to windowed or fullscreen", KEY_G);
}


void OptionsMenuUserInterface::setupMenus()
{
   clearMenuItems();
   
   Vector<string> opts;
   opts.push_back("ABSOLUTE");
   opts.push_back("RELATIVE");

   GameSettings *settings = getGame()->getSettings();

   bool relative = settings->getIniSettings()->controlsRelative;

   addMenuItem(new ToggleMenuItem("CONTROLS:", opts, relative ? 1 : 0, true, 
                                  setControlsCallback, "Set controls to absolute or relative mode", KEY_C));

   addMenuItem(getWindowModeMenuItem((U32)settings->getIniSettings()->displayMode));

   Joystick::initJoystick();   // Refresh joystick list

   addStickOptions(&opts);

   InputMode inputMode = settings->getIniSettings()->inputMode;

   addMenuItem(new ToggleMenuItem("PRIMARY INPUT:", 
                                  opts, 
                                  (U32)inputMode,
                                  true, 
                                  setInputModeCallback, 
                                  "Specify whether you want to play with your keyboard or joystick", 
                                  KEY_P, KEY_I));

   INPUT_MODE_MENU_ITEM_INDEX = getMenuItemCount() - 1;

   opts.clear();
   for(S32 i = 0; i < ControllerTypeCount; i++)
      opts.push_back(Joystick::joystickTypeToPrettyString(i));

   U32 joystickType = settings->getIniSettings()->joystickType;

   // Simple bounds check -- could be GenericController, UnknownController, or NoController
   U32 selectedOption = joystickType < ControllerTypeCount ? joystickType : 0;

   addMenuItem(new ToggleMenuItem("JOYSTICK:", opts, selectedOption, true, 
                                  setControllerCallback, "Choose which joystick to use in joystick mode", KEY_J));

   addMenuItem(new MenuItem(getMenuItemCount(), "DEFINE KEYS / BUTTONS", defineKeysCallback, 
                            "Remap keyboard or joystick controls", KEY_D, KEY_K));

   opts.clear();
   for(S32 i = 0; i <= 10; i++)
      opts.push_back(getVolMsg( F32(i) / 10 ));

   addMenuItem(new ToggleMenuItem("SFX VOLUME:",        opts, U32((settings->getIniSettings()->sfxVolLevel + 0.05) * 10.0), false, 
                                  setSFXVolumeCallback,   "Set sound effects volume", KEY_S));

   addMenuItem(new ToggleMenuItem("MUSIC VOLUME:",      opts, U32((settings->getIniSettings()->musicVolLevel + 0.05) * 10.0), false,
                                  setMusicVolumeCallback, "Set music volume", KEY_M));

   addMenuItem(new ToggleMenuItem("VOICE CHAT VOLUME:", opts, U32((settings->getIniSettings()->voiceChatVolLevel + 0.05) * 10.0), false, 
                                  setVoiceVolumeCallback, "Set voice chat volume",    KEY_V));

   // No music yet, so keep this out to keep menus from getting too long.  Uncomment when we have music.
   //menuItems.push_back(new MenuItem("MUSIC VOLUME:", getVolMsg(settings->getIniSettings()->musicVolLevel), 6, KEY_M, KEY_UNKNOWN));

   opts.clear();
   opts.push_back("DISABLED");
   opts.push_back("ENABLED");
   addMenuItem(new ToggleMenuItem("VOICE ECHO:", opts, settings->getIniSettings()->echoVoice ? 1 : 0, true, 
                                  setVoiceEchoCallback, "Toggle whether you hear your voice on voice chat",  KEY_E));
}


static bool isFullScreen(DisplayMode displayMode)
{
   return displayMode == DISPLAY_MODE_FULL_SCREEN_STRETCHED || displayMode == DISPLAY_MODE_FULL_SCREEN_UNSTRETCHED;
}


void OptionsMenuUserInterface::toggleDisplayMode()
{
   GameSettings *settings = getGame()->getSettings();

   DisplayMode oldMode = settings->getIniSettings()->oldDisplayMode;

   settings->getIniSettings()->oldDisplayMode = settings->getIniSettings()->displayMode;     // Save current setting
   // When we're in the editor, and we toggle views, we'll skip one of the fullscreen modes, as they essentially do the same thing in that UI
   if(UserInterface::current->getMenuID() == EditorUI)
   {
      if(isFullScreen(settings->getIniSettings()->displayMode))
         settings->getIniSettings()->displayMode = DISPLAY_MODE_WINDOWED;

      // If we know what the previous fullscreen mode was, use that
      else if(isFullScreen(oldMode))
         settings->getIniSettings()->displayMode = oldMode;

      // Otherwise, pick some sort of full-screen mode...
      else
         settings->getIniSettings()->displayMode = DISPLAY_MODE_FULL_SCREEN_STRETCHED;
   }
   else  // Not in the editor, just advance to the next mode
   {
      DisplayMode mode = DisplayMode((U32)settings->getIniSettings()->displayMode + 1);
      settings->getIniSettings()->displayMode = (mode == DISPLAY_MODE_UNKNOWN) ? (DisplayMode) 0 : mode;    // Bounds check 
   }

   actualizeScreenMode(false);
}


// Save options to INI file, and return to our regularly scheduled program
void OptionsMenuUserInterface::onEscape()
{
   saveSettingsToINI(&gINI, getGame()->getSettings());
   getUIManager()->reactivatePrevUI();      //mGameUserInterface
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
NameEntryUserInterface::NameEntryUserInterface(ClientGame *game) : MenuUserInterface(game)
{
   setMenuID(OptionsUI);
   mMenuTitle = "ENTER YOUR NICKNAME:";
   mReason = NetConnection::ReasonNone;
}


void NameEntryUserInterface::setReactivationReason(NetConnection::TerminationReason r) 
{ 
   mReason = r; 
   mMenuTitle = ""; 
}


void NameEntryUserInterface::onActivate()
{
   Parent::onActivate();
   setupMenu();
   getGame()->setReadyToConnectToMaster(false);
}


extern void seedRandomNumberGenerator(string name);

// User is ready to move on... deal with it
static void nameAndPasswordAcceptCallback(ClientGame *clientGame, U32 unused)
{
   UIManager *uiManager = clientGame->getUIManager();
   NameEntryUserInterface *ui = uiManager->getNameEntryUserInterface();

   if(uiManager->hasPrevUI())
      uiManager->reactivatePrevUI();
   else
      uiManager->getMainMenuUserInterface()->activate();

   clientGame->resetMasterConnectTimer();
   
   clientGame->updatePlayerNameAndPassword(ui->getMenuItem(1)->getValueForWritingToLevelFile(), 
                                           ui->getMenuItem(2)->getValueForWritingToLevelFile());

   clientGame->setReadyToConnectToMaster(true);
   seedRandomNumberGenerator(clientGame->getClientInfo()->getName().getString());

   if(clientGame->getConnectionToServer())                 // Rename while in game server, if connected
      clientGame->getConnectionToServer()->c2sRenameClient(clientGame->getClientInfo()->getName());
}


void NameEntryUserInterface::setupMenu()
{
   clearMenuItems();

   addMenuItem(new MenuItem("OK", nameAndPasswordAcceptCallback, ""));
   addMenuItem(new TextEntryMenuItem("NICKNAME:", getGame()->getSettings()->getIniSettings()->lastName, 
                                    getGame()->getSettings()->getDefaultName(), "", MAX_PLAYER_NAME_LENGTH));
   addMenuItem(new TextEntryMenuItem("PASSWORD:", getGame()->getLoginPassword(), "", "", MAX_PLAYER_PASSWORD_LENGTH));
   
   getMenuItem(1)->setFilter(LineEditor::noQuoteFilter);      // quotes are incompatible with PHPBB3 logins
   getMenuItem(2)->setSecret(true);
}


void NameEntryUserInterface::renderExtras()
{
   const S32 size = 15;
   const S32 gap = 5;
   const S32 canvasHeight = gScreenInfo.getGameCanvasHeight();

   const S32 rows = 3;
   S32 row = 0;

   glColor3f(0,1,0);

   drawCenteredString(canvasHeight - vertMargin - 30 - (rows - row) * size - (rows - row) * gap, size, 
            "You can skip this screen by editing the [Settings] section of Bitfighter.ini");
   row++;

   drawCenteredString(canvasHeight - vertMargin - 30 - (rows - row) * size - (rows - row) * gap, size, 
            "A password is only needed if you are using a reserved name.  You can reserve your");
   row++;

   drawCenteredString(canvasHeight - vertMargin - 30 - (rows - row) * size - (rows - row) * gap, size, 
            "nickname by registering for the bitfighter.org forums.  Registration is free.");


   if(mReason == NetConnection::ReasonBadLogin || mReason == NetConnection::ReasonInvalidUsername)
   {
      const char *message[] = { "If you have reserved this name by registering for",
                                "the forums, enter your forum password below. Otherwise,",
                                "this user name may be reserved. Please choose another."
                              };

      renderMessageBox("Invalid Name or Password", "", message, 3, -190);
   }
}

// Save options to INI file, and return to our regularly scheduled program
void NameEntryUserInterface::onEscape()
{
   shutdownBitfighter();
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
HostMenuUserInterface::HostMenuUserInterface(ClientGame *game) : MenuUserInterface(game)
{
   setMenuID(HostingUI);
   mMenuTitle ="HOST A GAME:";

   levelLoadDisplayFadeTimer.setPeriod(1000);
   levelLoadDisplayDisplay = true;
   mEditingIndex = -1;     // Not editing at the start
}


void HostMenuUserInterface::onActivate()
{
   Parent::onActivate();
   setupMenus();

   mLevelLoadDisplayTotal = 0;
   clearLevelLoadDisplay();
}


extern void initHostGame(GameSettings *settings, const Vector<string> &levelList, bool testMode, bool dedicatedServer);

static void startHostingCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->getHostMenuUserInterface()->saveSettings();

   GameSettings *settings = game->getSettings();

   initHostGame(settings, settings->getLevelList(), false, false);
}


void HostMenuUserInterface::setupMenus()
{
   clearMenuItems();

   GameSettings *settings = getGame()->getSettings();

   addMenuItem(new MenuItem("START HOSTING", startHostingCallback, "", KEY_H));

   addMenuItem(new TextEntryMenuItem("SERVER NAME:", settings->getHostName(), 
                                     "<Bitfighter Host>", "", QueryServersUserInterface::MaxServerNameLen,  KEY_N));

   addMenuItem(new TextEntryMenuItem("DESCRIPTION:", settings->getHostDescr(),                    
                                     "<Empty>", "", QueryServersUserInterface::MaxServerDescrLen, KEY_D));

   addMenuItem(new TextEntryMenuItem("LEVEL CHANGE PASSWORD:", settings->getLevelChangePassword(), 
                                     "<Anyone can change levels>", "", MAX_PASSWORD_LENGTH, KEY_L));

   addMenuItem(new TextEntryMenuItem("ADMIN PASSWORD:", settings->getAdminPassword(),       
                                     "<No remote admin access>", "", MAX_PASSWORD_LENGTH, KEY_A));

   addMenuItem(new TextEntryMenuItem("CONNECTION PASSWORD:", settings->getServerPassword(), "<Anyone can connect>", "",      
                                     MAX_PASSWORD_LENGTH, KEY_C));

   addMenuItem(new YesNoMenuItem("ALLOW MAP DOWNLOADS:", settings->getIniSettings()->allowGetMap, "", KEY_M));

   //addMenuItem(new CounterMenuItem("MAXIMUM PLAYERS:",   settings->getIniSettings()->maxplayers, 1, 2, MAX_PLAYERS, "", "", "", KEY_P));
   //addMenuItem(new TextEntryMenuItem("PORT:",             itos(DEFAULT_GAME_PORT),  "Use default of " + itos(DEFAULT_GAME_PORT), 
   //                                         "", 10, KEY_P));
}


// Save options to INI file, and return to our regularly scheduled program
// This only gets called when escape not already handled by preprocessKeys(), i.e. when we're not editing
void HostMenuUserInterface::onEscape()
{
   saveSettings();
   getUIManager()->reactivatePrevUI();     
}


// Save parameters and get them into the INI file
void HostMenuUserInterface::saveSettings()
{
   GameSettings *settings = getGame()->getSettings();

   settings->setHostName (getMenuItem(OPT_NAME)->getValue(), true);
   settings->setHostDescr(getMenuItem(OPT_DESCR)->getValue(), true);

   settings->setAdminPassword(getMenuItem(OPT_ADMIN_PASS)->getValue(), true);
   settings->setLevelChangePassword(getMenuItem(OPT_LVL_PASS)->getValue(), true);
   settings->setServerPassword(getMenuItem(OPT_PASS)->getValue(), true);

   settings->getIniSettings()->allowGetMap = (getMenuItem(OPT_GETMAP)->getValue() == "yes");
   //settings->getIniSettings()->maxplayers = getMenuItem(OPT_MAX_PLAYERS)->getIntValue();

   saveSettingsToINI(&gINI, getGame()->getSettings());
}


void HostMenuUserInterface::render()
{
   Parent::render();

   // If we're in LoadingLevels mode, show the progress panel...
   renderProgressListItems();
   if(gServerGame && (gServerGame->hostingModePhase == ServerGame::LoadingLevels || 
                      gServerGame->hostingModePhase == ServerGame::DoneLoadingLevels))
   {
      // There will be exactly one new entry every time we get here!
      addProgressListItem("Loaded level " + gServerGame->getLastLevelLoadName() + "...");
   }
}


// Add bit of text to progress item, and manage the list
void HostMenuUserInterface::addProgressListItem(string item)
{
   mLevelLoadDisplayNames.push_back(item);

   mLevelLoadDisplayTotal++;

   // Keep the list from growing too long:
   if(mLevelLoadDisplayNames.size() > 15)
      mLevelLoadDisplayNames.erase(0);
}


void HostMenuUserInterface::clearLevelLoadDisplay()
{
   mLevelLoadDisplayNames.clear();
   mLevelLoadDisplayTotal = 0;
}


void HostMenuUserInterface::renderProgressListItems()
{
   if(levelLoadDisplayDisplay || levelLoadDisplayFadeTimer.getCurrent() > 0)
   {
      bool disableBlending = false;

      if(!glIsEnabled(GL_BLEND))
      {
         glEnable(GL_BLEND);
         disableBlending = true; 
      }

      for(S32 i = 0; i < mLevelLoadDisplayNames.size(); i++)
      {
         glColor(Colors::white, (1.4f - ((F32) (mLevelLoadDisplayNames.size() - i) / 10.f)) * 
                                        (levelLoadDisplayDisplay ? 1 : levelLoadDisplayFadeTimer.getFraction()) );
         drawStringf(100, gScreenInfo.getGameCanvasHeight() - vertMargin - (mLevelLoadDisplayNames.size() - i) * 20, 
                     15, "%s", mLevelLoadDisplayNames[i].c_str());
      }

      if(disableBlending)
         glDisable(GL_BLEND);
   }
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
GameMenuUserInterface::GameMenuUserInterface(ClientGame *game) : MenuUserInterface(game)
{
   setMenuID(GameMenuUI);
   mMenuTitle = "GAME MENU:";
}


void GameMenuUserInterface::idle(U32 timeDelta)
{
   GameConnection *gc = getGame()->getConnectionToServer();

   if(gc && gc->waitingForPermissionsReply() && gc->gotPermissionsReply())      // We're waiting for a reply, and it has arrived
   {
      gc->setWaitingForPermissionsReply(false);
      buildMenu();                                                   // Update menu to reflect newly available options
   }
}


void GameMenuUserInterface::onActivate()
{
   Parent::onActivate();
   buildMenu();
   mMenuSubTitle = "";
   mMenuSubTitleColor = Colors::cyan;
}


void GameMenuUserInterface::onReactivate()
{
   mMenuSubTitle = "";
}


static void endGameCallback(ClientGame *game, U32 unused)
{
   game->closeConnectionToGameServer();

   if(gServerGame)
   {
      delete gServerGame;
      gServerGame = NULL;
   }
}


static void addTwoMinsCallback(ClientGame *game, U32 unused)
{
   if(game->getGameType())
      game->getGameType()->addTime(2 * 60 * 1000);

   game->getUIManager()->reactivatePrevUI();     // And back to our regularly scheduled programming!
}


static void chooseNewLevelCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->getLevelMenuUserInterface()->activate();
}


static void restartGameCallback(ClientGame *game, U32 unused)
{
   game->getConnectionToServer()->c2sRequestLevelChange(-2, false);
   game->getUIManager()->reactivatePrevUI();     // And back to our regularly scheduled programming! 
}


static void levelChangePWCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->getLevelChangePasswordEntryUserInterface()->activate();
}


static void adminPWCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->getAdminPasswordEntryUserInterface()->activate();
}


static void kickPlayerCallback(ClientGame *game, U32 unused)
{
   PlayerMenuUserInterface *ui = game->getUIManager()->getPlayerMenuUserInterface();

   ui->action = PlayerMenuUserInterface::Kick;
   ui->activate();
}


void GameMenuUserInterface::buildMenu()
{
   clearMenuItems();

   lastInputMode = getGame()->getSettings()->getIniSettings()->inputMode;      // Save here so we can see if we need to display alert msg if input mode changes

   addMenuItem(new MenuItem("OPTIONS",      optionsSelectedCallback, "", KEY_O));
   addMenuItem(new MenuItem("INSTRUCTIONS", helpSelectedCallback,    "", KEY_I, keyHELP));

   GameType *gameType = getGame()->getGameType();

   // Add any game-specific menu items
   if(gameType)
   {
      mGameType = gameType;
      gameType->addClientGameMenuOptions(getGame(), this);
   }

   GameConnection *gc = (getGame())->getConnectionToServer();

   if(gc)
   {
      if(gc->getClientInfo()->isLevelChanger())
      {
         addMenuItem(new MenuItem("PLAY DIFFERENT LEVEL", chooseNewLevelCallback, "", KEY_L, KEY_P));
         addMenuItem(new MenuItem("ADD TIME (2 MINS)",    addTwoMinsCallback,     "", KEY_T, KEY_2));
         addMenuItem(new MenuItem("RESTART LEVEL",        restartGameCallback,    "", KEY_R));
      }
      else        // Not level changer
         addMenuItem(new MenuItem("ENTER LEVEL CHANGE PASSWORD", levelChangePWCallback, "", KEY_L, KEY_P));

      if(gc->getClientInfo()->isAdmin())
      {
         // Add any game-specific menu items
         if(gameType)
         {
            mGameType = gameType;
            gameType->addAdminGameMenuOptions(this);
         }

         addMenuItem(new MenuItem("KICK A PLAYER", kickPlayerCallback, "", KEY_K));
      }
      else     // Not admin
         addMenuItem(new MenuItem("ENTER ADMIN PASSWORD", adminPWCallback, "", KEY_A, KEY_E));
   }

   if(getUIManager()->cameFrom(EditorUI))    // Came from editor
      addMenuItem(new MenuItem("RETURN TO EDITOR", endGameCallback, "", KEY_Q, KEY_R));
   else
      addMenuItem(new MenuItem("QUIT GAME",        endGameCallback, "", KEY_Q));
}


void GameMenuUserInterface::onEscape()
{
   getUIManager()->reactivatePrevUI();      //mGameUserInterface

   // Show alert about input mode changing, if needed
   bool inputModesChanged = (lastInputMode == getGame()->getSettings()->getIniSettings()->inputMode);
   getUIManager()->getGameUserInterface()->resetInputModeChangeAlertDisplayTimer(inputModesChanged ? 0 : 2800);
}

////////////////////////////////////////
////////////////////////////////////////

// Constructor
LevelMenuUserInterface::LevelMenuUserInterface(ClientGame *game) : MenuUserInterface(game)
{
   setMenuID(LevelTypeUI);
}


static const char *UPLOAD_LEVELS = "UPLOAD LEVELS";
static const char *ALL_LEVELS = "All Levels";
static const U32 ALL_LEVELS_MENUID = 0x80000001;
static const U32 UPLOAD_LEVELS_MENUID = 0x80000002;


static void selectLevelTypeCallback(ClientGame *game, U32 level)
{
   LevelMenuSelectUserInterface *ui = game->getUIManager()->getLevelMenuSelectUserInterface();

   // First entry will be "All Levels", subsequent entries will be level types populated from mLevelInfos
   if(level == ALL_LEVELS_MENUID)
      ui->category = ALL_LEVELS;
   else if(level == UPLOAD_LEVELS_MENUID)
      ui->category = UPLOAD_LEVELS;

   else
   {
      // Replace the following with a getLevelCount() function on game??
      GameConnection *gc = game->getConnectionToServer();
      if(!gc || gc->mLevelInfos.size() < (S32(level) - 1))
         return;

      ui->category = gc->mLevelInfos[level - 1].levelType.getString();
   }

  ui->activate();
}


void LevelMenuUserInterface::onActivate()
{
   Parent::onActivate();
   mMenuTitle = "CHOOSE LEVEL TYPE:";

   // replace with getLevelCount() method on game?
   GameConnection *gc = getGame()->getConnectionToServer();
   if(!gc || !gc->mLevelInfos.size())
      return;

   clearMenuItems();

   char c[] = "A";   // Shortcut key
   addMenuItem(new MenuItem(ALL_LEVELS_MENUID, ALL_LEVELS, selectLevelTypeCallback, "", stringToInputCode(c)));

   // Cycle through all levels, looking for unique type strings
   for(S32 i = 0; i < gc->mLevelInfos.size(); i++)
   {
      S32 j;
      for(j = 0; j < getMenuItemCount(); j++)
         if(gc->mLevelInfos[i].levelName == "" || !stricmp(gc->mLevelInfos[i].levelType.getString(), getMenuItem(j)->getPrompt().c_str()) )     
         {
            break;                  // Skip over levels with blank names or duplicate entries
         }
      if(j == getMenuItemCount())     // Must be a new type
      {
         strncpy(c, gc->mLevelInfos[i].levelType.getString(), 1);
         addMenuItem(new MenuItem(i + 1, gc->mLevelInfos[i].levelType.getString(), selectLevelTypeCallback, "", stringToInputCode(c)));
      }
   }

   sortMenuItems();

   if((gc->mSendableFlags & 1) && !gc->isLocalConnection())   // local connection is useless, already have all maps..
      addMenuItem(new MenuItem(UPLOAD_LEVELS_MENUID, UPLOAD_LEVELS, selectLevelTypeCallback, "", stringToInputCode(c)));
}


void LevelMenuUserInterface::onEscape()
{
   getUIManager()->reactivatePrevUI();    // to mGameUserInterface
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
LevelMenuSelectUserInterface::LevelMenuSelectUserInterface(ClientGame *game) : Parent(game)
{
   setMenuID(LevelUI);
}


static void processLevelSelectionCallback(ClientGame *game, U32 index)             
{
   game->getUIManager()->getLevelMenuSelectUserInterface()->processSelection(index);
}


const U32 UPLOAD_LEVELS_BIT = 0x80000000;

void LevelMenuSelectUserInterface::processSelection(U32 index)     
{
   Parent::onActivate();
   GameConnection *gc = getGame()->getConnectionToServer();

   if((index & UPLOAD_LEVELS_BIT) && (index & (~UPLOAD_LEVELS_BIT)) < U32(mLevels.size()))
   {
      FolderManager *folderManager = getGame()->getSettings()->getFolderManager();
      string filename = strictjoindir(folderManager->levelDir, mLevels[index & (~UPLOAD_LEVELS_BIT)]);

      if(!gc->s2rUploadFile(filename.c_str(), 1))     // TODO: 1 Should be an enum
         getGame()->displayErrorMessage("!!! Can't upload level: unable to read file");
   }
   else
      gc->c2sRequestLevelChange(index, false);        // The selection index is the level to load

   getUIManager()->reactivateMenu(getUIManager()->getGameUserInterface());    // Jump back to the game menu
}


void LevelMenuSelectUserInterface::onActivate()
{
   Parent::onActivate();
   mMenuTitle = "CHOOSE LEVEL: [" + category + "]";

   // Replace with a getLevelCount() method on Game?
   ClientGame *game = getGame();
   GameConnection *gc = game->getConnectionToServer();

   if(!gc || !gc->mLevelInfos.size())
      return;

   clearMenuItems();

   mLevels.clear();

   char c[2];
   c[1] = 0;   // null termination

   if(!strcmp(category.c_str(), UPLOAD_LEVELS))
   {
      // Get all the playable levels in levelDir
      mLevels = getGame()->getSettings()->getLevelList();     

      for(S32 i = 0; i < mLevels.size(); i++)
      {
         c[0] = mLevels[i].c_str()[0];
         addMenuItem(new MenuItem(i | UPLOAD_LEVELS_BIT, mLevels[i].c_str(), processLevelSelectionCallback, "", stringToInputCode(c)));
      }
   }
 
   for(S32 i = 0; i < gc->mLevelInfos.size(); i++)
   {
      if(gc->mLevelInfos[i].levelName == "")   // Skip levels with blank names --> but all should have names now!
         continue;
      if(!strcmp( gc->mLevelInfos[i].levelType.getString(), category.c_str() ) || 
         !strcmp(category.c_str(), ALL_LEVELS) )
      {
         c[0] = gc->mLevelInfos[i].levelName.getString()[0];
         addMenuItem(new MenuItem(i, gc->mLevelInfos[i].levelName.getString(), processLevelSelectionCallback, "", stringToInputCode(c)));
      }
   }

   sortMenuItems();
   mFirstVisibleItem = 0;

   if(itemSelectedWithMouse)
      onMouseMoved();
   else
      selectedIndex = 0;
}

#undef ALL_LEVELS


// Override parent, and make keys simply go to first level with that letter, rather than selecting it automatically
bool LevelMenuSelectUserInterface::processMenuSpecificKeys(InputCode inputCode, char ascii)
{
   // First check for some shortcut keys
   for(S32 i = 0; i < getMenuItemCount(); i++)
   {
      // Lets us advance to next level with same starting letter  
      S32 indx = selectedIndex + i + 1;
      if(indx >= getMenuItemCount())
         indx -= getMenuItemCount();

      if(inputCode == getMenuItem(indx)->key1 || inputCode == getMenuItem(indx)->key2)
      {
         selectedIndex = indx;
         playBoop();

         return true;
      }
   }

   return false;
}


void LevelMenuSelectUserInterface::onEscape()
{
   getUIManager()->reactivatePrevUI();    // to LevelMenuUserInterface
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
PlayerMenuUserInterface::PlayerMenuUserInterface(ClientGame *game) : Parent(game)
{
   setMenuID(PlayerUI);
}


static void playerSelectedCallback(ClientGame *game, U32 index) 
{
   game->getUIManager()->getPlayerMenuUserInterface()->playerSelected(index);
}


void PlayerMenuUserInterface::playerSelected(U32 index)
{
   // When we created the menu, names were not sorted, and item indices were assigned in "natural order".  Then
   // the menu items were sorted by name, and now the indices are now jumbled.  This bit here tries to get the
   // new, actual list index of an item given its original index.
   for(S32 i = 0; i < getMenuItemCount(); i++)
      if(getMenuItem(i)->getIndex() == (S32)index)
      {
         index = i;
         break;
      }

   GameType *gt = getGame()->getGameType();

   if(action == ChangeTeam)
   {
      TeamMenuUserInterface *ui = getUIManager()->getTeamMenuUserInterface();

      ui->activate();     // Show menu to let player select a new team
      ui->nameToChange = getMenuItem(index)->getPrompt();
   }
   else if(gt)    // action == Kick
      gt->c2sKickPlayer(getMenuItem(index)->getPrompt());


   if(action != ChangeTeam)                              // Unless we need to move on to the change team screen...
      getUIManager()->reactivateMenu(getUIManager()->getGameUserInterface());     // ...it's back to the game!
}


// By putting the menu building code in render, menus can be dynamically updated
void PlayerMenuUserInterface::render()
{
   clearMenuItems();

   GameConnection *conn = getGame()->getConnectionToServer();
   if(!conn)
      return;

   char c[] = "A";      // Dummy shortcut key
   for(S32 i = 0; i < getGame()->getClientCount(); i++)
   {
      ClientInfo *clientInfo = ((Game *)getGame())->getClientInfo(i);      // Lame!

      strncpy(c, clientInfo->getName().getString(), 1);        // Grab first char of name for a shortcut key

      // Will be used to show admin/player/robot prefix on menu
      PlayerType pt = clientInfo->isRobot() ? PlayerTypeRobot : (clientInfo->isAdmin() ? PlayerTypeAdmin : PlayerTypePlayer);    

      PlayerMenuItem *newItem = new PlayerMenuItem(i, clientInfo->getName().getString(), playerSelectedCallback, 
                                                   stringToInputCode(c), pt);
      newItem->setUnselectedColor(getGame()->getTeamColor(clientInfo->getTeamIndex()));

      addMenuItem(newItem);

   }

   sortMenuItems();

   if(action == Kick)
      mMenuTitle = "CHOOSE PLAYER TO KICK:";
   else if(action == ChangeTeam)
      mMenuTitle = "CHOOSE WHOSE TEAM TO CHANGE:";
   Parent::render();
}


void PlayerMenuUserInterface::onEscape()
{
   getUIManager()->reactivatePrevUI();   //mGameUserInterface
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
TeamMenuUserInterface::TeamMenuUserInterface(ClientGame *game) : Parent(game)
{
   setMenuID(TeamUI);
}


static void processTeamSelectionCallback(ClientGame *game, U32 index)        
{
   game->getUIManager()->getTeamMenuUserInterface()->processSelection(index);
}


void TeamMenuUserInterface::processSelection(U32 index)        
{
   GameType *gt = getGame()->getGameType();

   if(!gt)
      return;

   // Make sure user isn't just changing to the team they're already on...
   if(index != (U32)getGame()->getTeamIndex(nameToChange.c_str()))
   {
      if(getPrevMenuID() == PlayerUI)     // Initiated by an admin (PlayerUI is the kick/change team player-pick admin menu)
      {
         StringTableEntry e(nameToChange.c_str());
         gt->c2sTriggerTeamChange(e, index);   // Index will be the team index
      }
      else                                // Came from player changing own team
         gt->c2sChangeTeams(index);
   }

   getUIManager()->reactivateMenu(getUIManager()->getGameUserInterface());    // Back to the game!
}


// By reconstructing our menu at render time, changes to teams caused by others will be reflected immediately
void TeamMenuUserInterface::render()
{
   clearMenuItems();

   getGame()->countTeamPlayers();    // Make sure numPlayers is correctly populated

   char c[] = "A";      // Dummy shortcut key, will change below
   for(S32 i = 0; i < getGame()->getTeamCount(); i++)
   {
      AbstractTeam *team = getGame()->getTeam(i);
      strncpy(c, team->getName().getString(), 1);     // Grab first char of name for a shortcut key

      bool isCurrent = (i == getGame()->getTeamIndex(nameToChange.c_str()));
      
      addMenuItem(new TeamMenuItem(i, team, processTeamSelectionCallback, stringToInputCode(c), isCurrent));
   }

   string name = "";
   if(getGame()->getConnectionToServer() && getGame()->getConnectionToServer()->getControlObject())
   {
      Ship *ship = dynamic_cast<Ship *>(getGame()->getConnectionToServer()->getControlObject());
      if(ship)
         name = ship->getName().getString();
   }

   if(name != nameToChange)    // i.e. names differ, this isn't the local player
   {
      name = nameToChange;
      name += " ";
   }
   else
      name = "";

   // Finally, set menu title
   mMenuTitle = (string("TEAM TO SWITCH ") + name + "TO:").c_str();       // No space before the TO!

   Parent::render();
}


void TeamMenuUserInterface::onEscape()
{
   getUIManager()->reactivatePrevUI();
}

#undef MAX_MENU_SIZE

};

