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

#ifndef _LUA_SCRIPT_RUNNER_H
#define _LUA_SCRIPT_RUNNER_H

#include "LuaBase.h"          // Parent class
#include "EventManager.h"
#include "LuaWrapper.h"

#include "tnl.h"
#include "tnlVector.h"

#include <deque>
#include <string>

using namespace std;
using namespace TNL;


#define method(class, name)  { #name, &class::name }
#define ARRAYDEF(...) __VA_ARGS__                  // Wrap inline array definitions so they don't confuse the preprocessor   


namespace Zap
{

class BfObject;
class DatabaseObject;
class GridDatabase;
class LuaPlayerInfo;
class Rect;
class Ship;
class MenuItem;

//////////////////////////////////////////
////////////////////////////////////////

typedef struct { LuaBase *objectPtr; } UserData;

////////////////////////////////////////
////////////////////////////////////////


#define LUA_HELPER_FUNCTIONS_KEY      "lua_helper_functions"
#define ROBOT_HELPER_FUNCTIONS_KEY    "robot_helper_functions"
#define LEVELGEN_HELPER_FUNCTIONS_KEY "levelgen_helper_functions"

class LuaScriptRunner : public LuaBase
{

typedef LuaBase Parent;

private:
   static deque<string> mCachedScripts;

   static string mScriptingDir;
   static bool mScriptingDirSet;

   void setLuaArgs(const Vector<string> &args);
   static void setModulePath();

   static void configureNewLuaInstance();              // Prepare a new Lua environment for use

   static void loadCompileSaveHelper(const string &scriptName, const char *registryKey);
   static void loadCompileSaveScript(const char *filename, const char *registryKey);
   static void loadCompileScript(const char *filename);

   void pushStackTracer();      // Put error handler function onto the stack


   static void setEnums(lua_State *L);                       // Set a whole slew of enum values that we want the scripts to have access to
   static void setGlobalObjectArrays(lua_State *L);          // And some objects
   static void logErrorHandler(const char *msg, const char *prefix);

protected:
   static lua_State *L;          // Main Lua state variable
   string mScriptName;           // Fully qualified script name, with path and everything
   Vector<string> mScriptArgs;   // List of arguments passed to the script

   string mScriptId;             // Unique id for this script

   bool mSubscriptions[EventManager::EventTypes];  // Keep track of which events we're subscribed to for rapid unsubscription upon death or destruction

   // This method should be abstract, but luaW requires us to be able to instantiate this class
   virtual bool prepareEnvironment();
   void setSelf(lua_State *L, LuaScriptRunner *self, const char *name);

   static int luaPanicked(lua_State *L);  // Handle a total freakout by Lua
   static void registerClasses();
   void setEnvironment();                 // Sets the environment for the function on the top of the stack to that associated with name

   static void deleteScript(const char *name);  // Remove saved script from the Lua registry

   static void registerLooseFunctions(lua_State *L);   // Register some functions not associated with a particular class

   S32 findObjectById(lua_State *L, const Vector<DatabaseObject *> *objects);
   S32 findObjects(lua_State *L, GridDatabase *database, Rect *scope = NULL, Ship *caller = NULL);


// Sets a var in the script's environment to give access to the caller's "this" obj, with the var name "name".
// Basically sets the "bot", "levelgen", and "plugin" vars.
template <class T>
void setSelf(lua_State *L, T *self, const char *name)
{
   lua_getfield(L, LUA_REGISTRYINDEX, self->getScriptId()); // Put script's env table onto the stack  -- env_table
                                                         
   lua_pushstring(L, name);                                 //                                        -- env_table, "plugin"
   luaW_push(L, self);                                      //                                        -- env_table, "plugin", *this
   lua_rawset(L, -3);                                       // env_table["plugin"] = *this            -- env_table
                                                                                                    
   lua_pop(L, -1);                                          // Cleanup                                -- <<empty stack>>

   TNLAssert(lua_gettop(L) == 0 || LuaBase::dumpStack(L), "Stack not cleared!");
}


protected:
   virtual void killScript() = 0;


public:
   LuaScriptRunner();               // Constructor
   virtual ~LuaScriptRunner();      // Destructor

   static void clearScriptCache();

   static void setScriptingDir(const string &scriptingDir);

   virtual const char *getErrorMessagePrefix();

   static lua_State *getL();
   static bool startLua();          // Create L
   static void shutdown();          // Delete L

   bool runMain();                                    // Run a script's main() function
   bool runMain(const Vector<string> &args);          // Run a script's main() function, putting args into Lua's arg table

   bool loadScript(bool cacheScript);  // Loads script from file into a Lua chunk, then runs it
   bool runScript(bool cacheScript);   // Load the script, execute the chunk to get it in memory, then run its main() function

   bool runCmd(const char *function, S32 returnValues);

   const char *getScriptId();
   static bool loadFunction(lua_State *L, const char *scriptId, const char *functionName);
   bool loadAndRunGlobalFunction(lua_State *L, const char *key, ScriptContext context);

   void logError(const char *format, ...);

   S32 doSubscribe(lua_State *L, ScriptContext context);
   S32 doUnsubscribe(lua_State *L);

   // Consolidate code from bots and levelgens -- this tickTimer works for both!
   template <class T>
   void tickTimer(U32 deltaT)          
   {
      TNLAssert(lua_gettop(L) == 0 || LuaBase::dumpStack(L), "Stack dirty!");
      clearStack(L);

      luaW_push<T>(L, static_cast<T *>(this));           // -- this
      lua_pushnumber(L, deltaT);                         // -- this, deltaT

      // Note that we don't care if this generates an error... if it does the error handler will
      // print a nice message, then call killScript().
      runCmd("_tickTimer", 0);
   }



   //// Lua interface
   static const char *luaClassName;
   static const LuaFunctionProfile functionArgs[];

   static S32 lua_logprint(lua_State *L);
   static S32 lua_print(lua_State *L);
   static S32 lua_getRandomNumber(lua_State *L);
   static S32 lua_getMachineTime(lua_State *L);
   static S32 lua_findFile(lua_State *L);
   static S32 lua_readFromFile(lua_State *L);
   static S32 lua_writeToFile(lua_State *L);
};


////////////////////////////////////////
////////////////////////////////////////
//
// Some ugly macro defs that will make our Lua classes sleek and beautiful
//
////////////////////////////////////////
////////////////////////////////////////
//
// See discussion of this code here:
// http://stackoverflow.com/questions/11413663/reducing-code-repetition-in-c
//
// Starting with a definition like the following:
/*
 #define LUA_METHODS(CLASS, METHOD) \
    METHOD(CLASS, addDest,    ARRAYDEF({{ PT,  END }}), 1 ) \
    METHOD(CLASS, delDest,    ARRAYDEF({{ INT, END }}), 1 ) \
    METHOD(CLASS, clearDests, ARRAYDEF({{      END }}), 1 ) \
*/

#define LUA_METHOD_ITEM(class_, name, b, c) \
{ #name, luaW_doMethod<class_, &class_::lua_## name > },


#define GENERATE_LUA_METHODS_TABLE(class_, table_) \
const luaL_reg class_::luaMethods[] =              \
{                                                  \
   table_(class_, LUA_METHOD_ITEM)                 \
   { NULL, NULL }                                  \
}

// Generates something like the following:
// const luaL_reg Teleporter::luaMethods[] =
// {
//       { "addDest",    luaW_doMethod<Teleporter, &Teleporter::lua_addDest >    }
//       { "delDest",    luaW_doMethod<Teleporter, &Teleporter::lua_delDest >    }
//       { "clearDests", luaW_doMethod<Teleporter, &Teleporter::lua_clearDests > }
//       { NULL, NULL }
// };


////////////////////////////////////////

 #define LUA_FUNARGS_ITEM(class_, name, profiles, profileCount) \
{ #name, {profiles, profileCount } },
 

#define GENERATE_LUA_FUNARGS_TABLE(class_, table_)  \
const LuaFunctionProfile class_::functionArgs[] =   \
{                                                   \
   table_(class_, LUA_FUNARGS_ITEM)                 \
   { NULL, {{{ }}, 0 } }                            \
}

// Generates something like the following (without the comment block, of course!):
// const LuaFunctionProfile Teleporter::functionArgs[] =
//    |---------------- LuaFunctionProfile ------------------|     
//    |- Function name -|-------- LuaFunctionArgList --------|
//    |                 |-argList -|- # elements in argList -|  
// {
//    { "addDest",    {{{ PT,  END }},         1             } },
//    { "delDest",    {{{ INT, END }},         1             } },
//    { "clearDests", {{{      END }},         1             } },
//    { NULL, {{{ }}, 0 } }
// };

////////////////////////////////////////
////////////////////////////////////////


};

#endif
