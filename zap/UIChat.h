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

#ifndef _UICHAT_H_
#define _UICHAT_H_

#include "tnlNetBase.h"

#include "game.h"
#include "UI.h"
#include "point.h"
#include "lineEditor.h"

#include <map>

namespace Zap
{

class ChatMessage
{
public:
   ChatMessage() { /* do nothing */ }                             // Quickie constructor

   ChatMessage(string frm, string msg, Color col, bool isPriv, bool isSys)    // "Real" constructor
   {
      color = col;
      message = msg;
      from = frm;
      time = getShortTimeStamp();   // Record time message arrived
      isPrivate = isPriv;
      isSystem = isSys;
   }

   Color color;      // Chat message colors
   string message;   // Hold chat messages
   string from;      // Hold corresponding nicks
   string time;      // Time message arrived
   bool isPrivate;   // Holds public/private status of message
   bool isSystem;    // Message from system?
};


///////////////////////////////////////
///////////////////////////////////////

// For sorting our color-nick map, which we'll never do, so this is essentially a dummy
struct strCmp {
   bool operator()( string s1, string s2 ) const {
      return s1 < s2;
   }
};

// All our chat classes will inherit from this
class AbstractChat
{
private:
   static std::map<string, Color, strCmp> mFromColors;       // Map nicknames to colors
   static U32 mColorPtr;
   Color getNextColor();                                     // Get next available color for a new nick
   static const S32 MESSAGES_TO_RETAIN = 80;                 // Plenty for now... far too many, really

   static U32 mMessageCount;

   Color getColor(string name);

protected:
   // Message data
   static ChatMessage mMessages[MESSAGES_TO_RETAIN];
   LineEditor mLineEditor;

   ChatMessage getMessage(U32 index);
   U32 mChatCursorPos;                     // Where is cursor?

   U32 getMessageCount() { return mMessageCount; }

   bool composingMessage() { return mLineEditor.length() > 0; }

public:
   AbstractChat();      // Constructor
   void newMessage(string from, string message, bool isPrivate, bool isSystem);   // Handle incoming msg

   void addCharToMessage(char ascii);     // Append char to message being composed
   void clearChat();                      // Clear message being composed
   void issueChat();                      // Send chat message

   void leaveGlobalChat();                // Send msg to master telling them we're leaving chat

   void renderMessages(U32 yPos, U32 numberToDisplay);
   void renderMessageComposition(S32 ypos);   // Render outgoing chat message composition line

   void renderChatters(S32 xpos, S32 ypos);   // Render list of other people in chat room
   void deliverPrivateMessage(const char *sender, const char *message);

   // Handle players joining and leaving the chat session
   void playerJoinedGlobalChat(const StringTableEntry &playerNick);
   void playerLeftGlobalChat(const StringTableEntry &playerNick);


   // Sizes and other things to help with positioning
   static const U32 CHAT_FONT_SIZE = 14;      // Font size to display those messages
   static const U32 CHAT_TIME_FONT_SIZE = 8;  // Size of the timestamp
   static const U32 CHAT_FONT_MARGIN = 3;     // Vertical margin
   static const U32 CHAT_NAMELIST_SIZE = 11;  // Size of names of people in chatroom


   static Vector<StringTableEntry> mPlayersInGlobalChat;
};


///////////////////////////////////////
///////////////////////////////////////

class ChatUserInterface : public UserInterface, public AbstractChat
{
private:
   Color mMenuSubTitleColor;

   virtual void renderHeader();
   //virtual void renderFooter();
   virtual void onOutGameChat();    // What to do if user presses [F5]
   bool mRenderUnderlyingUI;

public:
   ChatUserInterface();             // Constructor

   // UI related
   void render();
   void onKeyDown(KeyCode keyCode, char ascii);

   void onActivate();
   void onActivateLobbyMode();

   void onEscape();

   void idle(U32 timeDelta);

   void setRenderUnderlyingUI(bool render) { mRenderUnderlyingUI = render; }
};

extern ChatUserInterface gChatInterface;

///////////////////////////////////////
///////////////////////////////////////

class SuspendedUserInterface : public ChatUserInterface
{
private:
   void renderHeader();
   void onOutGameChat();      // What to do if user presses [F5]

public:
   SuspendedUserInterface();          // Constructor
};

extern SuspendedUserInterface gSuspendedInterface;

};

#endif

