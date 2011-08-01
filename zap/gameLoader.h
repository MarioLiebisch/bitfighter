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

#ifndef _GAMELOADER_H_
#define _GAMELOADER_H_

#include <string>
#include "tnlVector.h"
#include "tnlTypes.h"


using namespace TNL;
using namespace std;

namespace Zap
{

class GridDatabase;

class LevelLoader
{
public:
   // Put these in here so we can access them from luaLevelGenerator
   static const S32 MAX_LEVEL_LINE_ARGS = 128;     // Most args on a single line,
   static const S32 MaxArgLen = 100;               // Each at most MaxArgLen bytes long  (enforced in addCharToArg)
   static const S32 MaxIdLen = 11;                 // Max 32-bit int is 10 digits, plus room for a null

   static const S32 MAX_LEVEL_LINE_LENGTH = 4096;  // Max total level line length we'll tolerate

   bool loadLevelFromFile(const char *file, bool inEditor, GridDatabase *database);
   void parseLevelLine(const char *line, GridDatabase *database, bool inEditor, const string &levelFileName);

   // Implementers of this class need to provide the following implementations:
   virtual void processLevelLoadLine(TNL::U32 argc, TNL::U32 id, const char **argv, GridDatabase *database, bool inEditor, const string &levelFileName) = 0;
   virtual void setGameTime(TNL::F32 time) = 0;
};


////////////////////////////////////////
////////////////////////////////////////

// Provide a class to help organize loading of levels from disk
class LevelListLoader
{
public:
   static TNL::Vector<std::string> buildLevelList();
   static void removeSkippedLevels(TNL::Vector<std::string> &levelList);
};


////////////////////////////////////////
////////////////////////////////////////

// Thrown when the loader encounters a line that seems broken for some reason
struct LevelLoadException : public std::exception
{
   std::string msg;

   LevelLoadException (std::string str) : msg(str) { /* do nothing */ }    // Constructor
   ~LevelLoadException () throw() { /* do nothing */ }                // Destructor, required by gcc to avoid "looser throw" error
   const char* what() const throw() { return msg.c_str(); }
};

};

#endif


