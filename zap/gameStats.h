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

#ifndef _GAMESTATS_H_
#define _GAMESTATS_H_

#include "gameWeapons.h"

#include "tnlTypes.h"
#include "tnlVector.h"
#include "tnlBitStream.h"
#include "tnlNetStringTable.h"
#include "tnlNonce.h"
//#include "tnlString.h"

using namespace std;
using namespace TNL;

namespace Zap {

struct WeaponStats 
{
   WeaponType weaponType;
   U16 shots;
   U16 hits;
};


struct PlayerStats
{
   string name;
   bool isAuthenticated;
   Nonce nonce;  // used for authentication
   bool isRobot;
   string gameResult;
   S32 points;
   S32 kills;
   S32 deaths;
   S32 suicides;
   bool switchedTeams;
   S32 switchedTeamCount;
   Vector<WeaponStats> weaponStats;

   bool isAdmin;
   bool isLevelChanger; // might not be needed...
};


struct TeamStats 
{
   U32 color_bin; // To send as number, not string
   string color;
   string name;
   S32 score;
   string gameResult;     // 'W', 'L', 'T'
   Vector<PlayerStats> playerStats;    // Info about all players on this team
};


struct GameStats
{
   string serverName;
   string serverIP;
	S32 cs_protocol_version;

	S32 build_version;

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


struct VersionedGameStats
{
   U8 version;
   bool valid;
   GameStats gameStats;
};


};    // end namespace Zap


namespace Types
{
   extern void read(TNL::BitStream &s, Zap::VersionedGameStats *val);
   extern void write(TNL::BitStream &s, Zap::VersionedGameStats &val);
};




#endif