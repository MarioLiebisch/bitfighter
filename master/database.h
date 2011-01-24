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
#include "tnlTypes.h"
#include "tnlVector.h"

#include <string>

// Forward declaration
namespace mysqlpp
{
	class TCPConnection;
};

using namespace TNL;
using namespace std;

struct WeaponStats 
{
   WeaponType weaponType;
   S32 shots;
   S32 hits;
};


struct PlayerStats
{
   string name;
   bool isAuthenticated;
   bool isRobot;
   string gameResult;
   S32 points;
   S32 kills;
   S32 deaths;
   S32 suicides;
   bool switchedTeams;     // do we currently track this?  is it meaningful?
   Vector<WeaponStats> weaponStats;
};


struct TeamStats 
{
   string color;
   string name;
   S32 playerCount;
   S32 botCount;
   string gameResult;     // 'W', 'L', 'T'
   Vector<PlayerStats> playerStats;    // Info about all players on this team
};

struct GameStats
{
   string gameType;
   string levelName;
   bool isOfficial;
   S32 playerCount;
   S32 duration;     // game length in seconds
   bool isTeamGame;
   S32 teamCount;
   bool isTied;
   Vector<TeamStats> teamStats;     // for team games
};


class DatabaseWriter 
{
private:
   bool mIsValid;
   const char *mServer;
   const char *mDb;
   const char *mUser;
   const char *mPassword;

   void initialize(const char *server, const char *db, const char *user, const char *password);

public:
   DatabaseWriter();
   DatabaseWriter(const char *server, const char *db, const char *user, const char *password);     // Constructor
   DatabaseWriter(const char *db, const char *user, const char *password);                         // Constructor

   void insertStats(const string &serverName, const string &serverIP, S32 serverVersion, const GameStats &gameStats);
};

/*
Create connection, lifetime is lifetime of app
Create query object
Run insert query
   --> will return SimpleResult object
   */






#endif


