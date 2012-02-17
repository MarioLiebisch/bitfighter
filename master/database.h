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

#ifndef DATABASE_H
#define DATABASE_H

#include "../zap/gameWeapons.h"     // For WeaponType enum
#include "../zap/gameStats.h"
#include "tnlTypes.h"
#include "tnlVector.h"
#include "tnlNonce.h"
#include "../sqlite/sqlite3.h"

#ifdef BF_WRITE_TO_MYSQL
#include "mysql++.h"
#endif

#include <string>

// Forward declaration
namespace mysqlpp
{
   class TCPConnection;
};

using namespace TNL;
using namespace Zap;
using namespace mysqlpp;
using namespace std;


struct ServerInfo
{
   U64 id;
   string name;
   string ip;

   // Quickie constructor
   ServerInfo(U64 id, const string name, const string ip) 
   { 
      this->id = id; 
      this->name = name; 
      this->ip = ip;
   }
};


////////////////////////////////////////
////////////////////////////////////////

class Query;

class DatabaseWriter 
{
private:
   bool mMySql;        // Can this connection use mysql?

   char mServer[64];   // was const char *, but problems when data in pointer dies.
   char mDb[64];
   char mUser[64];
   char mPassword[64];
   Vector<ServerInfo> cachedServers;

   S32 lastGameID;

   void initialize(const char *server, const char *db, const char *user, const char *password);
   void createStatsDatabase();
   string getSqliteSchema();

public:
   DatabaseWriter();

   // MySQL constructors
   DatabaseWriter(const char *server, const char *db, const char *user, const char *password);     // Constructor
   DatabaseWriter(const char *db, const char *user, const char *password);                         // Constructor

   // SQLite constructor
   DatabaseWriter(const char *db);

   void insertStats(const GameStats &gameStats);
   void insertAchievement(U8 achievementId, const StringTableEntry &playerNick, const string &serverName, const string &serverIP);

   U64 getServerID(Query *query, sqlite3 *sqliteDb, const string &serverName, const string &serverIP);

   void addToServerCache(U64 id, const string &serverName, const string &serverIPAddr);   // Add database ID to our cache
   U64 getServerIDFromCache(const string &serverName, const string &serverIPAddr);        // And get it back out again
};


#endif
