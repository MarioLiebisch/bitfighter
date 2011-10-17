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

#ifndef _UIMENUS_H_
#define _UIMENUS_H_

#ifndef ZAP_DEDICATED

#include "UI.h"
#include "input.h"         // For InputMode def
#include "UIMenuItems.h"
#include "tnlNetConnection.h" // for TerminationReason


namespace Zap
{

using namespace std;


// This class is the template for most all of our menus...
class MenuUserInterface : public UserInterface
{
   typedef UserInterface Parent;

private:
   Vector<boost::shared_ptr<MenuItem> > mMenuItems;

   S32 getYStart();     // Get vert pos of first menu item
   S32 getOffset(); 
   Timer mScrollTimer;

   // For detecting keys being held down
   bool mRepeatMode;
   bool mKeyDown;

   virtual S32 getTextSize() { return 23; }              // Let menus set their own text size
   virtual S32 getGap() { return 18; }

   virtual void renderExtras() { /* Do nothing */ }      // For drawing something extra on a menu
   void advanceItem();                                   // What happens when we move on to the next menu item?

   virtual void initialize();
   virtual S32 getSelectedMenuItem();

protected:
   
   S32 mFirstVisibleItem;  // Some menus have items than will fit on the screen; this is the index of the first visible item

   bool mRenderInstructions;

   // Handle keyboard input while a menu is displayed
   virtual bool processMenuSpecificKeys(InputCode inputCode, char ascii);
   virtual bool processKeys(InputCode inputCode, char ascii);

   void sortMenuItems();
   void clearMenuItems();
   S32 getMenuItemCount();
   MenuItem *getLastMenuItem();

public:
   // Constructor
   MenuUserInterface(ClientGame *game);      
   MenuUserInterface(ClientGame *game, const string &title);

   void addMenuItem(MenuItem *menuItem);
   void addWrappedMenuItem(boost::shared_ptr<MenuItem> menuItem);
   MenuItem *getMenuItem(S32 index);

   bool itemSelectedWithMouse;

   static const S32 MOUSE_SCROLL_INTERVAL = 100;

   string mMenuTitle;
   string mMenuSubTitle;

   Color mMenuSubTitleColor;
   bool mMenuFooterContainsInstructions;

   void idle(U32 timeDelta); 

   S32 selectedIndex;

   void getMenuResponses(Vector<string> &responses);    // Fill responses with values from menu

   void render();    // Draw the basic menu
   void onKeyDown(InputCode inputCode, char ascii);
   void onKeyUp(InputCode inputCode);
   void onMouseMoved(S32 x, S32 y) { onMouseMoved(); }      // Redirect to argless version
   void onMouseMoved();
   void processMouse();

   void onActivate();
   void onReactivate();

   virtual void onEscape();
};


////////////////////////////////////////
////////////////////////////////////////

// <--- DO NOT SUBCLASS MainMenuUserInterface!! (unless you override onActivate) ---> //
class MainMenuUserInterface : public MenuUserInterface
{
   typedef MenuUserInterface Parent;

private:
   char mMOTD[MOTD_LEN];
   U32 motdArriveTime;
   Timer mFadeInTimer;        // Track the brief fade in interval the first time menu is shown
   Timer mColorTimer;
   Timer mColorTimer2;
   enum {
      FadeInTime = 400,       // Time that fade in lasts (ms) 
      ColorTime = 1000,
      ColorTime2 = 1700,
   };
   bool mTransDir;
   bool mTransDir2;
   bool mNeedToUpgrade;       // True if client is out of date and needs to upgrade, false if we're on the latest version
   bool mShowedUpgradeAlert;  // So we don't show the upgrade message more than once

   void renderExtras();

public:
   MainMenuUserInterface(ClientGame *game);           // Constructor
   void processSelection(U32 index);
   void onEscape();
   void render();
   void idle(U32 timeDelta); 
   void setMOTD(const char *motd);              // Message of the day, from Master
   void onActivate();
   void setNeedToUpgrade(bool needToUpgrade);   // Is client in need of an upgrade?

   bool showAnimation;                          // Is this the first time the menu is shown?
   bool mFirstTime;
   void showUpgradeAlert();                     // Display message to the user that it is time to upgrade
   bool getNeedToUpgrade();
};


////////////////////////////////////////
////////////////////////////////////////

class OptionsMenuUserInterface : public MenuUserInterface
{
   typedef MenuUserInterface Parent;

public:
   OptionsMenuUserInterface(ClientGame *game);        // Constructor
   void processSelection(U32 index) { }         // Process selected menu item when right arrow is pressed
   void processShiftSelection(U32 index) { }    // And when the left arrow is pressed
   void onEscape();
   void setupMenus();
   void onActivate();
   void toggleDisplayMode();
};


////////////////////////////////////////
////////////////////////////////////////

class HostMenuUserInterface : public MenuUserInterface
{
   enum MenuItems {
      OPT_HOST,
      OPT_NAME,
      OPT_DESCR,
      OPT_LVL_PASS,
      OPT_ADMIN_PASS,
      OPT_PASS,
      OPT_GETMAP,
      OPT_MAX_PLAYERS,
      OPT_PORT,
      OPT_COUNT
   };

   static const S32 FIRST_EDITABLE_ITEM = OPT_NAME;

   typedef MenuUserInterface Parent;

private:
   Vector<string> mLevelLoadDisplayNames;    // For displaying levels as they're loaded in host mode
   S32 mLevelLoadDisplayTotal;
   S32 mEditingIndex;                        // Index of item we're editing, -1 if none

public:
   HostMenuUserInterface(ClientGame *game);        // Constructor
   void onEscape();
   void setupMenus();
   void onActivate();
   void render();
   void saveSettings();       // Save parameters in INI file

   // For displaying the list of levels as we load them:
   Timer levelLoadDisplayFadeTimer;
   bool levelLoadDisplayDisplay;
   void addProgressListItem(string item);
   void renderProgressListItems();
   void clearLevelLoadDisplay();
};

////////////////////////////////////////
////////////////////////////////////////

class NameEntryUserInterface : public MenuUserInterface
{
private:
   typedef MenuUserInterface Parent;
   void renderExtras();
   NetConnection::TerminationReason mReason;

public:
   NameEntryUserInterface(ClientGame *game);    // Constructor
   void processSelection(U32 index) { }         // Process selected menu item when right arrow is pressed
   void processShiftSelection(U32 index) { }    // And when the left arrow is pressed
   void onEscape();
   void setupMenu();
   void onActivate();
   void setReactivationReason(NetConnection::TerminationReason r);
};


////////////////////////////////////////
////////////////////////////////////////

class GameType;

class GameMenuUserInterface : public MenuUserInterface
{
   enum {
      OPT_OPTIONS,
      OPT_HELP,
      OPT_ADMIN,
      OPT_KICK,
      OPT_LEVEL_CHANGE_PW,
      OPT_CHOOSELEVEL,
      OPT_ADD2MINS,
      OPT_RESTART,
      OPT_QUIT
   };

private:
   typedef MenuUserInterface Parent;
   SafePtr<GameType> mGameType;
   InputMode lastInputMode;
   void buildMenu();

public:
   GameMenuUserInterface(ClientGame *game);            // Constructor

   void idle(U32 timeDelta);
   void onActivate();
   void onReactivate();
   void onEscape();
};


////////////////////////////////////////
////////////////////////////////////////

class LevelMenuUserInterface : public MenuUserInterface
{
private:
   typedef MenuUserInterface Parent;

public:
   LevelMenuUserInterface(ClientGame *game);        // Constructor
   void onActivate();
   void onEscape();
};


////////////////////////////////////////
////////////////////////////////////////

class LevelMenuSelectUserInterface : public MenuUserInterface
{
   typedef MenuUserInterface Parent;

private:
   Vector<string> mLevels;

public:
   LevelMenuSelectUserInterface(ClientGame *game);        // Constructor
   string category;
   void onActivate();
   bool processMenuSpecificKeys(InputCode inputCode, char ascii);  // Custom key handling for level selection menus

   void processSelection(U32 index);
   void onEscape();
};


////////////////////////////////////////
////////////////////////////////////////

class AdminMenuUserInterface : public MenuUserInterface
{
   typedef MenuUserInterface Parent;

private:
   SafePtr<GameType> mGameType;

public:
   AdminMenuUserInterface();      // Constructor
   void onActivate();
   void processSelection(U32 index);
   void processShiftSelection(U32 index);
   void onEscape();
};


////////////////////////////////////////
////////////////////////////////////////

class PlayerMenuUserInterface : public MenuUserInterface
#else
class PlayerMenuUserInterface
#endif
{
#ifndef ZAP_DEDICATED
   typedef MenuUserInterface Parent;

public:
   PlayerMenuUserInterface(ClientGame *game);        // Constructor

   void render();
   void playerSelected(U32 index);
   void onEscape();
#endif

public:
   enum Action { // This is needed in ZAP_DEDICATED for use in GameConnection::c2sAdminPlayerAction
      Kick,
      ChangeTeam,
      ActionCount,
   } action;
};

#ifndef ZAP_DEDICATED

////////////////////////////////////////
////////////////////////////////////////

class TeamMenuUserInterface : public MenuUserInterface
{
   typedef MenuUserInterface Parent;

public:
   TeamMenuUserInterface(ClientGame *game);        // Constructor
   void render();
   void onEscape();
   string nameToChange;

   void processSelection(U32 index);
};

};

#endif      // #ifndef ZAP_DEDICATED

#endif      // #ifndef _UIMENUS_H_

