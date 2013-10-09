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

#ifndef _EDITOR_PLUGIN_H_
#define _EDITOR_PLUGIN_H_

#include "luaLevelGenerator.h"      // Parent class
#include "UIMenuItems.h"

#include <boost/shared_ptr.hpp>

using boost::shared_ptr;

namespace Zap
{
class EditorPlugin : public LuaScriptRunner
{
   typedef LuaScriptRunner Parent;

private:
   static bool getMenuItemVectorFromTable(lua_State *L, S32 index, const char *methodName, Vector<boost::shared_ptr<MenuItem> > &menuItems);

   GridDatabase *mGridDatabase;
   Game *mGame;
   F32 mGridSize;
   string mDescription;
   string mRequestedBinding;

protected:
   void killScript();

public:
   // Constructors
   EditorPlugin();      // Dummy 0-args constructor, here to make boost happy!
   EditorPlugin(const string &scriptName, const Vector<string> &scriptArgs, F32 gridSize, 
                  GridDatabase *gridDatabase, Game *game);

   virtual ~EditorPlugin();  // Destructor

   bool prepareEnvironment();
   string getScriptName();
   string getDescription();
   string getRequestedBinding();

   const char *getErrorMessagePrefix();
     
   bool runGetArgsMenu(string &menuTitle, Vector<boost::shared_ptr<MenuItem> > &menuItems);    // Get menu def from the plugin

   // Lua methods
   S32 lua_getGridSize(lua_State *L);
   S32 lua_getSelectedObjects(lua_State *L);        // Return all selected objects in the editor
   S32 lua_getAllObjects(lua_State *L);             // Return all objects in the editor
   S32 lua_showMessage(lua_State *L);



   //// Lua interface
   LUAW_DECLARE_CLASS(EditorPlugin);

	static const char *luaClassName;
	static const luaL_reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];
};


};

#endif
