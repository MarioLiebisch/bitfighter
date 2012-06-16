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
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//------------------------------------------------------------------------------------

#include "ship.h"
#include "ClientInfo.h"
#include "gameConnection.h"
#include "playerInfo.h"
#include "EditorObject.h"        // For NO_TEAM def
#include "EngineeredItem.h"      // For EngineerModuleDeployer def
#include "gameConnection.h"

#include "voiceCodec.h"


class Game;

namespace Zap
{

// Constructor
ClientInfo::ClientInfo()
{
   mPlayerInfo = NULL;

   mScore = 0;
   mTotalScore = 0;
   mTeamIndex = (NO_TEAM + 0);
   mPing = 0;
   mIsAdmin = false;
   mIsLevelChanger = false;
   mIsRobot = false;
   mIsAuthenticated = false;
   mBadges = NO_BADGES;
   mNeedToCheckAuthenticationWithMaster = false;     // Does client report that they are verified
   mSpawnDelayed = false;
   mIsBusy = false;
}


// Destructor
ClientInfo::~ClientInfo()
{
   delete mPlayerInfo;
}


void ClientInfo::setAuthenticated(bool isAuthenticated, Int<BADGE_COUNT> badges)
{
   mNeedToCheckAuthenticationWithMaster = false;     // Once we get here, we'll treat the ruling as definitive
   mIsAuthenticated = isAuthenticated; 
   mBadges = badges;
}


Int<BADGE_COUNT> ClientInfo::getBadges()
{
   return mBadges;
}


bool ClientInfo::hasBadge(MeritBadges badge)
{
   return mBadges & BIT(badge);
}


const StringTableEntry ClientInfo::getName()
{
   return mName;
}


void ClientInfo::setName(const StringTableEntry &name)
{
   mName = name;
}


S32 ClientInfo::getScore()
{
   return mScore;
}


void ClientInfo::setScore(S32 score)
{
   mScore = score;
}


void ClientInfo::addScore(S32 score)
{
   mScore += score;
}


void ClientInfo::setShip(Ship *ship)
{
   mShip = ship;
}


Ship *ClientInfo::getShip()
{
   return mShip;
}


void ClientInfo::setNeedToCheckAuthenticationWithMaster(bool needToCheck)
{
   mNeedToCheckAuthenticationWithMaster = needToCheck;
}


bool ClientInfo::getNeedToCheckAuthenticationWithMaster()
{
   return mNeedToCheckAuthenticationWithMaster;
}


// Check if player is "on hold" due to inactivity; bots are never on hold.  Server only!
bool ClientInfo::shouldDelaySpawn()
{
   return mIsRobot ? false : getConnection()->getTimeSinceLastMove() > 20000;    // 20 secs -- includes time between games
}


// Returns true if spawn has actually been delayed 
bool ClientInfo::isSpawnDelayed()
{
   return mSpawnDelayed;
}


// Returns true if spawn has actually been delayed 
bool ClientInfo::isBusy()
{
   return mIsBusy;
}


void ClientInfo::setIsBusy(bool isBusy)
{
   mIsBusy = isBusy;
}


void ClientInfo::resetLoadout(bool levelHasLoadoutZone)
{
   mOldLoadout.clear();

   // Save current loadout to put on-deck
   Vector<U8> loadout = getLoadout();

   resetLoadout();

   // If the current level has a loadout zone, put last level's load-out on-deck
   if(levelHasLoadoutZone)
      sRequestLoadout(loadout);
}


void ClientInfo::resetLoadout()
{
   mLoadout.clear();

   for(S32 i = 0; i < ShipModuleCount + ShipWeaponCount; i++)
      mLoadout.push_back(DefaultLoadout[i]);
}


const Vector<U8> &ClientInfo::getLoadout()
{
   return mLoadout;
}


S32 ClientInfo::getPing()
{
   return mPing;
}


void ClientInfo::setPing(S32 ping)
{
   mPing = ping;
}


S32 ClientInfo::getTeamIndex()
{
   return mTeamIndex;
}


void ClientInfo::setTeamIndex(S32 teamIndex)
{
   mTeamIndex = teamIndex;
}


bool ClientInfo::isAuthenticated()
{
   return mIsAuthenticated;
}


bool ClientInfo::isLevelChanger()
{
   return mIsLevelChanger;
}


void ClientInfo::setIsLevelChanger(bool isLevelChanger)
{
   mIsLevelChanger = isLevelChanger;
}


bool ClientInfo::isAdmin()
{
   return mIsAdmin;
}


void ClientInfo::setIsAdmin(bool isAdmin)
{
   mIsAdmin = isAdmin;
}


bool ClientInfo::isRobot()
{
   return mIsRobot;
}


F32 ClientInfo::getCalculatedRating()
{
   return mStatistics.getCalculatedRating();
}


// Resets stats and the like
void ClientInfo::endOfGameScoringHandler()
{
   mStatistics.addGamePlayed();
   mStatistics.resetStatistics();
}


LuaPlayerInfo *ClientInfo::getPlayerInfo()
{
   // Lazily initialize
   if(!mPlayerInfo)
      mPlayerInfo = new PlayerInfo(this);   // Deleted in destructor

   return mPlayerInfo;
}


// Server only, robots can run this, bypassing the net interface. Return true if successfuly deployed.
// TODO: Move this elsewhere   <--  where?...  anybody?
bool ClientInfo::sEngineerDeployObject(U32 objectType)
{
   Ship *ship = getShip();
   if(!ship)                                          // Not a good sign...
      return false;                                   // ...bail

   GameType *gameType = ship->getGame()->getGameType();

   if(!gameType->isEngineerEnabled())          // Something fishy going on here...
      return false;                            // ...bail

   EngineerModuleDeployer deployer;

   // Check if we can create the engineer object; if not, return false
   if(!deployer.canCreateObjectAtLocation(ship->getGame()->getGameObjDatabase(), ship, objectType))
   {
      if(!isRobot())
         getConnection()->s2cDisplayErrorMessage(deployer.getErrorMessage().c_str());

      return false;
   }

   // Now deploy the object
   if(deployer.deployEngineeredItem(ship->getClientInfo(), objectType))
   {
      // Announce the build
      StringTableEntry msg( "%e0 has engineered a %e1." );

      Vector<StringTableEntry> e;
      e.push_back(getName());
      switch(objectType)
      {
         case EngineeredTurret: e.push_back("turret"); break;
         case EngineeredForceField: e.push_back("force field"); break;
         case EngineeredTeleportEntrance: e.push_back("teleport entrance"); break;
         case EngineeredTeleportExit: e.push_back("teleport exit"); break;
         default: e.push_back(""); break;   // Shouldn't occur
      }

      gameType->broadcastMessage(GameConnection::ColorAqua, SFXNone, msg, e);

      return true;
   }

   // Else... fail silently?
   return false;
}


void ClientInfo::sRequestLoadout(Vector<U8> &loadout)
{
   mLoadout = loadout;

   GameType *gt = mGame->getGameType();

   if(gt)
      gt->SRV_clientRequestLoadout(this, mLoadout);    // This will set loadout if ship is in loadout zone
}


// Return pointer to statistics tracker 
Statistics *ClientInfo::getStatistics()
{
   return &mStatistics;
}


Nonce *ClientInfo::getId()
{
   return &mId;
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
FullClientInfo::FullClientInfo(Game *game, GameConnection *gameConnection, bool isRobot) : ClientInfo()
{
   mClientConnection = gameConnection;
   mIsRobot = isRobot;
   mGame = game;
}


// Destructor
FullClientInfo::~FullClientInfo()
{
   // Do nothing
}


void FullClientInfo::setAuthenticated(bool isAuthenticated, Int<BADGE_COUNT> badges)
{
   TNLAssert(isAuthenticated || badges == NO_BADGES, "Unauthenticated players should never have badges!");
   Parent::setAuthenticated(isAuthenticated, badges);

   // Broadcast new connection status to all clients, except the client that is authenticated.  Presumably they already know.  
   //  ===> This may now be wrong <=== It does seem a little roundabout to use the game server to communicate
   // between the FullClientInfo and the RemoteClientInfo on each client; we could contact the RemoteClientInfo directly and
   // then not send a message to the client being authenticated below.  But I think this is cleaner architecturally, and this
   // message is not sent often.
   if(mClientConnection && mClientConnection->isConnectionToClient())      
      for(S32 i = 0; i < gServerGame->getClientCount(); i++)
         if(gServerGame->getClientInfo(i)->getName() != mName && gServerGame->getClientInfo(i)->getConnection())
            gServerGame->getClientInfo(i)->getConnection()->s2cSetAuthenticated(mName, isAuthenticated, badges);
}


F32 FullClientInfo::getRating()
{
   // Initial case: no one has scored
   if(mTotalScore == 0)      
      return .5;

   // Standard case: 
   else   
      return F32(mScore) / F32(mTotalScore);
}


// Server only -- RemoteClientInfo has a client-side override
void FullClientInfo::setSpawnDelayed(bool spawnDelayed)
{
   if(spawnDelayed == mSpawnDelayed)
      return;

   if(spawnDelayed && !mSpawnDelayed)
      getConnection()->s2cPlayerSpawnDelayed();    // Tell client their spawn has been delayed

   mGame->getGameType()->s2cSetIsSpawnDelayed(mName, spawnDelayed);
   // Clients that joins mid game will get this SpawnDelayed set in GameType::s2cAddClient

   mSpawnDelayed = spawnDelayed;
}


GameConnection *FullClientInfo::getConnection()
{
   return mClientConnection;
}


void FullClientInfo::setConnection(GameConnection *conn)
{
   mClientConnection = conn;
}


void FullClientInfo::setRating(F32 rating)
{
   TNLAssert(false, "Ratings can't be set for this class!");
}


SoundEffect *FullClientInfo::getVoiceSFX()
{
   TNLAssert(false, "Can't access VoiceSFX from this class!");
   return NULL;
}


VoiceDecoder *FullClientInfo::getVoiceDecoder()
{
   TNLAssert(false, "Can't access VoiceDecoder from this class!");
   return NULL;
}


////////////////////////////////////////
////////////////////////////////////////


#ifndef ZAP_DEDICATED
// Constructor
RemoteClientInfo::RemoteClientInfo(Game *game, const StringTableEntry &name, bool isAuthenticated, Int<BADGE_COUNT> badges, 
                                   bool isRobot, bool isAdmin, bool isLevelChanger, bool isSpawnDelayed, bool isBusy) : ClientInfo()
{
   mGame = game;
   mName = name;
   mIsAuthenticated = isAuthenticated;
   mIsRobot = isRobot;
   mIsAdmin = isAdmin;
   mIsLevelChanger = isLevelChanger;
   mTeamIndex = NO_TEAM;
   mRating = 0;
   mBadges = badges;
   mSpawnDelayed = isSpawnDelayed;
   mIsBusy = isBusy;

   // Initialize speech stuff
   mDecoder = new SpeexVoiceDecoder();                                  // Deleted in destructor
   mVoiceSFX = new SoundEffect(SFXVoice, NULL, 1, Point(), Point());    // RefPtr, will self-delete
}


// Destructor
RemoteClientInfo::~RemoteClientInfo()
{
   delete mDecoder;
}


GameConnection *RemoteClientInfo::getConnection()
{
   TNLAssert(false, "Can't get a GameConnection from a RemoteClientInfo!");
   return NULL;
}


void RemoteClientInfo::setConnection(GameConnection *conn)
{
   TNLAssert(false, "Can't set a GameConnection on a RemoteClientInfo!");
}


// game is only needed for signature compatibility
void RemoteClientInfo::setSpawnDelayed(bool spawnDelayed)
{
   mSpawnDelayed = spawnDelayed;
}


F32 RemoteClientInfo::getRating()
{
   return mRating;
}


void RemoteClientInfo::setRating(F32 rating)
{
   mRating = rating;
}


// Voice chat stuff -- these will be invalid on the server side
SoundEffect *RemoteClientInfo::getVoiceSFX()
{
   return mVoiceSFX;
}


VoiceDecoder *RemoteClientInfo::getVoiceDecoder()
{
   return mDecoder;
}

#endif


};
