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

#ifndef _LUAOBJECT_H_
#define _LUAOBJECT_H_


#include "GameTypesEnum.h"
#include "EventManager.h"

#include "lua.h"
#include "../lua/include/lunar.h"

#include "LuaWrapper.h"

#include "Point.h"

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
class LuaPlayerInfo;
class Ship;
class MenuItem;


class LuaObject
{
public:
   enum LuaArgType {
      BOOL,    // Boolean
      INT,     // Integer argument
      FLT,     // Float argument
      STR,     // String argument
      PT,      // Point argument (only used by Lua scripts)
      END      // End of list sentinel value
   };

   static const S32 MAX_ARGS = 6;
   static const S32 MAX_PROFILES = 4;

   struct LuaFunctionProfile {
      const char *functionName;
      LuaArgType argList[MAX_PROFILES][MAX_ARGS];
      const S32 profileCount;
   };

protected:
   static MenuItem *pushMenuItem (lua_State *L, MenuItem *menuItem);

   // This doesn't really need to be virtual, but something here does, to allow dynamic_casting to occur... I picked
   // this one pretty much arbitrarily...  it won't be overridden.
   virtual void getStringVectorFromTable(lua_State *L, S32 index, const char *methodName, Vector<string> &strings);
   static bool getMenuItemVectorFromTable(lua_State *L, S32 index, const char *methodName, Vector<MenuItem *> &menuItems);

   static Point getPointOrXY(lua_State *L, S32 index);
   static Point getCheckedVec(lua_State *L, S32 index, const char *methodName);


   static void setfield (lua_State *L, const char *key, F32 value);

public:
   LuaObject();            // Constructor
   virtual ~LuaObject();   // Destructor

   // All of these return<T> functions work in the same way.  Include at the end of a child class method.
   // Usage: return returnInt(L, int);

   template<class T> S32 returnVal(lua_State *L, T value, bool letLuaDelete = true);

   // The basics:
   static S32 returnInt(lua_State *L, S32 num);
   static S32 returnFloat(lua_State *L, F32 num);
   static S32 returnString(lua_State *L, const char *str);
   static S32 returnBool(lua_State *L, bool boolean);
   static S32 returnNil(lua_State *L);

   static void checkArgCount(lua_State *L, S32 argsWanted, const char *methodName);
   static S32 checkArgList(lua_State *L, const LuaFunctionProfile *functionInfos, const char *className, const char *functionName);
   static string LuaObject::prettyPrintParamList(const LuaFunctionProfile *functionInfo);


   // More complex objects:
   static S32 returnPoint(lua_State *L, const Point &point);
   static S32 returnMenuItem(lua_State *L, MenuItem *menuItem);
   static S32 returnShip(lua_State *L, Ship *ship);                // Handles null references properly

   static S32 returnPlayerInfo(lua_State *L, Ship *ship);
   static S32 returnPlayerInfo(lua_State *L, LuaPlayerInfo *playerInfo);


   static void clearStack(lua_State *L);
   static F32 getFloat(lua_State *L, S32 index);
   static F32 getCheckedFloat(lua_State *L, S32 index, const char *methodName);

   static bool getBool(lua_State *L, S32 index, const char *methodName);
   static bool getBool(lua_State *L, S32 index, const char *methodName, bool defaultVal);

   static lua_Integer getInt(lua_State *L, S32 index);
   static lua_Integer getInt(lua_State *L, S32 index, S32 defaultVal);
   static lua_Integer getInt(lua_State *L, S32 index, const char *methodName, S32 minVal, S32 maxVal);

   static lua_Integer getCheckedInt(lua_State *L, S32 index, const char *methodName);

   static const char *getString(lua_State *L, S32 index, const char *methodName);
   static const char *getString(lua_State *L, S32 index, const char *methodName, const char *defaultVal);

   // Some debugging helpers
   static void dumpTable(lua_State *L, S32 tableIndex, const char *msg = "");
   static bool dumpStack(lua_State* L, const char *msg = "");

   static bool shouldLuaGarbageCollectThisObject();     
};


////////////////////////////////////////
////////////////////////////////////////

typedef struct { LuaObject *objectPtr; } UserData;

////////////////////////////////////////
////////////////////////////////////////


#define LUA_HELPER_FUNCTIONS_KEY      "lua_helper_functions"
#define ROBOT_HELPER_FUNCTIONS_KEY    "robot_helper_functions"
#define LEVELGEN_HELPER_FUNCTIONS_KEY "levelgen_helper_functions"

class LuaScriptRunner
{
public:
   enum ScriptType {
      ROBOT,
      LEVELGEN,
      PLUGIN
   };

private:
   static deque<string> mCachedScripts;

   static string mScriptingDir;
   static bool mScriptingDirSet;

   void setLuaArgs(const Vector<string> &args);
   void setModulePath();

   bool configureNewLuaInstance();           // Prepare a new Lua environment for use

   bool loadCompileSaveHelper(const string &scriptName, const char *registryKey);
   bool loadCompileSaveScript(const char *filename, const char *registryKey);
   bool loadCompileScript(const char *filename);

   void registerLooseFunctions(lua_State *L);   // Register some functions not associated with a particular class
   void setEnums(lua_State *L);                 // Set a whole slew of enum values that we want the scripts to have access to

   string mScriptId;             // Unique id for this script

protected:
   static lua_State *L;          // Main Lua state variable
   string mScriptName;           // Fully qualified script name, with path and everything
   Vector<string> mScriptArgs;   // List of arguments passed to the script

   virtual bool loadScript();
   bool startLua(ScriptType scriptType);
   bool mSubscriptions[EventManager::EventTypes];  // Keep track of which events we're subscribed to for rapid unsubscription upon death or destruction

   // These two methods should be abstract, but luaW requires us to be able to instantiate this class
   virtual void logError(const char *format, ...);
   virtual bool prepareEnvironment();

   static void printStackTrace(lua_State *L);


   static int luaPanicked(lua_State *L);

   virtual void registerClasses();
   
   void setEnvironment();

   static void deleteScript(const char *name);      // Remove saved script from the Lua registry

   virtual void tickTimer(U32 deltaT);       // Advance script timers

public:
   LuaScriptRunner();               // Constructor
   virtual ~LuaScriptRunner();      // Destructor

   static void clearScriptCache();

   void setScriptingDir(const string &scriptingDir);

   static lua_State *getL();
   static void shutdown();

   bool runMain();                                    // Run a script's main() function
   bool runMain(const Vector<string> &args);          // Run a script's main() function, putting args into Lua's arg table

   bool retrieveFunction(const char *functionName);   // Put specified function on top of the stack, if it's defined

   // Event subscriptions
   int subscribe(lua_State *L);
   int unsubscribe(lua_State *L);

   const char *getScriptId();
   static void loadFunction(lua_State *L, const char *scriptId, const char *functionName);
   bool loadAndRunGlobalFunction(lua_State *L, const char *key);

   // Lua interface
   //LUAW_DECLARE_CLASS(LuaScriptRunner);
   //static const luaL_reg luaMethods[];
   //static const char *luaClassName;
};


};

#endif

