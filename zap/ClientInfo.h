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

#ifndef _CLIENT_INFO_H_
#define _CLIENT_INFO_H_

#include "statistics.h"          // For Statistics def
#include "SharedConstants.h"
#include "ship.h"

#include "Timer.h"

#include "tnlNetBase.h"
#include "tnlNonce.h"


class Game;
class GameConnection;
class SoundEffect;

class LuaPlayerInfo;


#ifndef ZAP_DEDICATED
#  include "voiceCodec.h"
#endif


namespace Zap
{

// This object only concerns itself with things that one client tracks about another.  We use it for other purposes, of course, 
// as a convenient strucure for holding certain settings about the local client, or about remote clients when we are running on the server.
// But the general scope of what we track should be limited; other items should be stored directly on the GameConnection object itself.
class ClientInfo : public SafePtrData
{
private:
   LuaPlayerInfo *mPlayerInfo;   // Lua access to this class
   Statistics mStatistics;       // Statistics tracker
   SafePtr<Ship> mShip;          // SafePtr will return NULL if ship object is deleted
   Vector<U8> mLoadout;
   bool mNeedToCheckAuthenticationWithMaster;

protected:
   StringTableEntry mName;
   S32 mScore;
   S32 mTotalScore;
   Nonce mId;
   S32 mTeamIndex;
   S32 mPing;
   bool mIsLevelChanger;
   bool mIsAdmin;
   bool mIsRobot;
   bool mIsAuthenticated;
   bool mSpawnDelayed;
   bool mIsBusy;
   Int<BADGE_COUNT> mBadges;
   Game *mGame;

public:
   ClientInfo();           // Constructor
   virtual ~ClientInfo();  // Destructor

   virtual GameConnection *getConnection() = 0;
   virtual void setConnection(GameConnection *conn) = 0;

   const StringTableEntry getName();
   void setName(const StringTableEntry &name);

   S32 getScore();
   void setScore(S32 score);
   void addScore(S32 score);

   const Vector<U8> &getLoadout();
   Timer respawnTimer;

   Vector<U8> mOldLoadout;            // Server: to respawn with old loadout  Client: to check if using same loadout configuration
   void resetLoadout(bool levelHasLoadoutZone);
   void resetLoadout();
   void sRequestLoadout(Vector<U8> &loadout);

   void setNeedToCheckAuthenticationWithMaster(bool needToCheck);
   bool getNeedToCheckAuthenticationWithMaster();

   bool shouldDelaySpawn();
   bool isSpawnDelayed();              // Returns true if spawn has actually been delayed   
   virtual void setSpawnDelayed(bool spawnDelayed) = 0;

   bool isBusy();
   void setIsBusy(bool isBusy);

   Ship *getShip();                    // NULL on client
   void setShip(Ship *ship);

   virtual void setRating(F32 rating) = 0;
   virtual F32 getRating() = 0;
   F32 getCalculatedRating();
   void endOfGameScoringHandler();     // Resets stats and the like


   S32 getPing();
   void setPing(S32 ping);

   S32 getTeamIndex();
   void setTeamIndex(S32 teamIndex);

   virtual void setAuthenticated(bool isAuthenticated, Int<BADGE_COUNT> badges);
   bool isAuthenticated();

   Int<BADGE_COUNT> getBadges();
   bool hasBadge(MeritBadges badge);

   bool isLevelChanger();
   void setIsLevelChanger(bool isLevelChanger);

   bool isAdmin();
   void setIsAdmin(bool isAdmin);

   bool isRobot();

   Statistics *getStatistics();      // Return pointer to statistics tracker 

   LuaPlayerInfo *getPlayerInfo();

   bool sEngineerDeployObject(U32 type);      // Player using engineer module, robots use this, bypassing the net interface. True if successful.


   Nonce *getId();

   virtual SoundEffect *getVoiceSFX() = 0;
   virtual VoiceDecoder *getVoiceDecoder() = 0;
};

////////////////////////////////////////
////////////////////////////////////////

class GameConnection;

class FullClientInfo : public ClientInfo
{
   typedef ClientInfo Parent;

private:
   GameConnection *mClientConnection;
   
public:
   FullClientInfo(Game *game, GameConnection *clientConnection, bool isRobot);     // Constructor
   virtual ~FullClientInfo();                                          // Destructor

   // WARNING!! mClientConnection can be NULL on client and server's robots
   GameConnection *getConnection();
   void setConnection(GameConnection *conn);

   void setAuthenticated(bool isAuthenticated, Int<BADGE_COUNT> badges);

   void setSpawnDelayed(bool spawnDelayed);

   void setRating(F32 rating);
   F32 getRating();

   SoundEffect *getVoiceSFX();
   VoiceDecoder *getVoiceDecoder();
};


////////////////////////////////////////
////////////////////////////////////////
// RemoteClientInfo is used on the client side to track information about other players; these other players do not have a connection
// to us -- all the information we know about them is located on this RemoteClientInfo object.
#ifndef ZAP_DEDICATED
class RemoteClientInfo : public ClientInfo
{
   typedef ClientInfo Parent;

private:
   F32 mRating;      // Ratings are provided by the server and stored here

   // For voice chat
   RefPtr<SoundEffect> mVoiceSFX;
   VoiceDecoder *mDecoder;

public:
   RemoteClientInfo(Game *game, const StringTableEntry &name, bool isAuthenticated, Int<BADGE_COUNT> badges,      // Constructor
                    bool isRobot, bool isAdmin, bool isSpawnDelayed, bool isBusy);  
   virtual ~RemoteClientInfo();      
   // Destructor
   void initialize();

   GameConnection *getConnection();
   void setConnection(GameConnection *conn);

   F32 getRating();
   void setRating(F32 rating);

   void setSpawnDelayed(bool spawnDelayed);

   // Voice chat stuff -- these will be invalid on the server side
   SoundEffect *getVoiceSFX();
   VoiceDecoder *getVoiceDecoder();
};
#endif


};


#endif

