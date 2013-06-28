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

#ifndef _UI_H_
#define _UI_H_

#ifdef ZAP_DEDICATED
#  error "UI.h shouldn't be included in dedicated build"
#endif

//#include "UIManager.h"
#include "lineEditor.h"
#include "InputCode.h"

#include "Timer.h"
#include "tnl.h"
#include "tnlLog.h"

#include <string>

using namespace TNL;
using namespace std;

namespace Zap
{

extern F32 gLineWidth1;
extern F32 gDefaultLineWidth;
extern F32 gLineWidth3;
extern F32 gLineWidth4;


////////////////////////////////////////
////////////////////////////////////////

class Game;
class ClientGame;
class GameSettings;
class UIManager;
class Color;

class UserInterface
{
   friend class UIManager;    // Give UIManager access to private and protected members

private:
   //UIID mInternalMenuID;                     // Unique interface ID

   static void doDrawAngleString(F32 x, F32 y, F32 size, F32 angle, const char *string, bool autoLineWidth = true);
   static void doDrawAngleString(S32 x, S32 y, F32 size, F32 angle, const char *string, bool autoLineWidth = true);

   ClientGame *mClientGame;

   U32 mTimeSinceLastInput;

   // Activate menus via the UIManager, please!
   void activate();
   void reactivate();

protected:
   bool mDisableShipKeyboardInput;           // Disable ship movement while user is in menus

public:
   explicit UserInterface(ClientGame *game);       // Constructor
   virtual ~UserInterface();                       // Destructor

   static const S32 MOUSE_SCROLL_INTERVAL = 100;
   static const S32 MAX_PASSWORD_LENGTH = 32;      // Arbitrary, doesn't matter, but needs to be _something_

   static const S32 MaxServerNameLen = 40;
   static const S32 MaxServerDescrLen = 254;

   ClientGame *getGame() const;

   UIManager *getUIManager() const;

#ifdef TNL_OS_XBOX
   static const S32 horizMargin = 50;
   static const S32 vertMargin = 38;
#else
   static const S32 horizMargin = 15;
   static const S32 vertMargin = 15;
#endif

   static S32 messageMargin;

   U32 getTimeSinceLastInput();

   virtual void render();
   virtual void idle(U32 timeDelta);
   virtual void onActivate();
   virtual void onDeactivate(bool nextUIUsesEditorScreenMode);
   virtual void onReactivate();
   virtual void onDisplayModeChange();

   virtual bool usesEditorScreenMode();   // Returns true if the UI attempts to use entire screen like editor, false otherwise

   void renderConsole()const ;            // Render game console
   virtual void renderMasterStatus();     // Render master server connection status

   // Helpers to simplify dealing with key bindings
   static InputCode getInputCode(GameSettings *settings, InputCodeManager::BindingName binding);
   void setInputCode(GameSettings *settings, InputCodeManager::BindingName binding, InputCode inputCode);
   bool checkInputCode(GameSettings *settings, InputCodeManager::BindingName, InputCode inputCode);
   const char *getInputCodeString(GameSettings *settings, InputCodeManager::BindingName binding);


   // Input event handlers
   virtual bool onKeyDown(InputCode inputCode);
   virtual void onKeyUp(InputCode inputCode);
   virtual void onTextInput(char ascii);
   virtual void onMouseMoved();
   virtual void onMouseDragged();

   void renderMessageBox(const char *title, const char *instr, string message[], S32 msgLines, S32 vertOffset = 0, S32 style = 1) const;
   void renderUnboxedMessageBox(const char *title, const char *instr, string message[], S32 msgLines, S32 vertOffset = 0) const;

   static void renderFancyBox(S32 xRight, S32 yTop, S32 xLeft, S32 yBottom, S32 cornerInset, const Color &fillColor, F32 fillAlpha, const Color &borderColor);
   static void renderCenteredFancyBox(S32 boxTop, S32 boxHeight, S32 inset, S32 cornerInset, const Color &fillColor, F32 fillAlpha, const Color &borderColor);

   static void dimUnderlyingUI(F32 amount = 0.75f);

   static void renderDiagnosticKeysOverlay();

   static void drawMenuItemHighlight(S32 x1, S32 y1, S32 x2, S32 y2, bool disabled = false);
   static void playBoop();    // Make some noise!
};



// Used only for multiple mClientGame in one instance
struct UserInterfaceData
{
   S32 vertMargin, horizMargin;
   S32 chatMargin;

   //void get();
   //void set();
};

};

#endif


