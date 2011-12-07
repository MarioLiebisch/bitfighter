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

#ifndef _UINAMEENTRY_H_
#define _UINAMEENTRY_H_

#include "UI.h"
#include "Timer.h"
#include "config.h"
#include "md5wrapper.h"    // For hashing

namespace Zap
{

extern md5wrapper md5;

class TextEntryUserInterface : public UserInterface
{
   typedef UserInterface Parent;
private:
   U32 cursorPos;

protected:
   const char *title;
   const char *instr1;
   const char *instr2;
   
   bool resetOnActivate;
   LineEditor lineEditor;

   virtual void onAccept(const char *text) = 0;
   virtual void onEscape() = 0;

public:
   TextEntryUserInterface(ClientGame *game);  // Constructor

   virtual void onActivate();
   void render();
   void idle(U32 t);

   void setSecret(bool secret) { lineEditor.setSecret(secret); }

   virtual void onKeyDown(InputCode inputCode, char ascii);

   const char *getText() { return lineEditor.c_str(); }
   string getSaltedHashText() { return md5.getSaltedHashFromString(lineEditor.getString()); }
   void setString(string str);
};


////////////////////////////////////////
////////////////////////////////////////

//class NameEntryUserInterface : public MenuUserInterface
//{
//public:
//   NameEntryUserInterface();      // Constructor
//
//   virtual void onAccept(const char *text);
//   virtual void onEscape();
//};


////////////////////////////////////////
////////////////////////////////////////

class LevelNameEntryUserInterface : public TextEntryUserInterface
{

typedef TextEntryUserInterface Parent;

private:
   bool mFoundLevel;
   S32 mLevelIndex;
   Vector<string> mLevels;

   bool setLevelIndex();
   void completePartial();

public:
   LevelNameEntryUserInterface(ClientGame *game);      // Constructor

   virtual void onKeyDown(InputCode inputCode, char ascii);
   virtual void onAccept(const char *text);
   virtual void onEscape();

   virtual void onActivate();
};


////////////////////////////////////////
////////////////////////////////////////

class PasswordEntryUserInterface :  public TextEntryUserInterface
{
   typedef TextEntryUserInterface Parent;

public:
   PasswordEntryUserInterface(ClientGame *game) : Parent(game)    // Constructor
   {
      setSecret(true);
   }

   virtual void render();
   virtual void onAccept(const char *text) = 0;
   virtual void onEscape() = 0;
};


////////////////////////////////////////
////////////////////////////////////////

class GameConnection;

class PreGamePasswordEntryUserInterface :  public PasswordEntryUserInterface
{
   typedef PasswordEntryUserInterface Parent;

private:
   Address connectAddress;
public:
   PreGamePasswordEntryUserInterface(ClientGame *game) : Parent(game) { /* Do nothing */ }    // Constructor
   virtual ~PreGamePasswordEntryUserInterface() {}
   void onAccept(const char *text);
   void onEscape();
   void setConnectServer(const Address &addr) { connectAddress = addr; }
};


////////////////////////////////////////
////////////////////////////////////////

class InGamePasswordEntryUserInterface :  public PasswordEntryUserInterface
{
   typedef PasswordEntryUserInterface Parent;

public:
   InGamePasswordEntryUserInterface(ClientGame *game) : Parent(game) { /* Do nothing */ }  // Constructor
   void onAccept(const char *text);
   void onEscape();
   virtual void submitPassword(GameConnection *gameConnection, const char *text) = 0;
};


////////////////////////////////////////
////////////////////////////////////////

class ServerPasswordEntryUserInterface : public PreGamePasswordEntryUserInterface
{
   typedef PreGamePasswordEntryUserInterface Parent;

public:
   ServerPasswordEntryUserInterface(ClientGame *game);                // Constructor
};


////////////////////////////////////////
////////////////////////////////////////

class AdminPasswordEntryUserInterface : public InGamePasswordEntryUserInterface
{
   typedef InGamePasswordEntryUserInterface Parent;

public:
   AdminPasswordEntryUserInterface(ClientGame *game);           // Constructor
   virtual ~AdminPasswordEntryUserInterface();

   void submitPassword(GameConnection *gameConnection, const char *text);
};


////////////////////////////////////////
////////////////////////////////////////

class LevelChangePasswordEntryUserInterface : public InGamePasswordEntryUserInterface
{
   typedef InGamePasswordEntryUserInterface Parent;

public:
   LevelChangePasswordEntryUserInterface(ClientGame *game);     // Constructor
   virtual ~LevelChangePasswordEntryUserInterface();

   void submitPassword(GameConnection *gameConnection, const char *text);
};


////////////////////////////////////////
////////////////////////////////////////

};

#endif

