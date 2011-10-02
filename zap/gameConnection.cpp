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

#include "gameConnection.h"
#include "game.h"
#include "soccerGame.h"          // For checking if pick up soccer is allowed
#include "IniFile.h"             // For CIniFile def
#include "playerInfo.h"
#include "shipItems.h"           // For EngineerBuildObjects enum
#include "masterConnection.h"    // For MasterServerConnection def
#include "EngineeredItem.h"      // For EngineerModuleDeployer
#include "BanList.h"

#ifndef ZAP_DEDICATED
#include "ClientGame.h"
#endif

#include "UIMenus.h"             // For enum in PlayerMenuUserInterface

#include "md5wrapper.h"

#include "Colors.h"
#include "stringUtils.h"         // For strictjoindir()
  

namespace Zap
{

TNL_IMPLEMENT_NETCONNECTION(GameConnection, NetClassGroupGame, true);

// Constructor -- used on Server by TNL, not called directly, used when a new client connects to the server
GameConnection::GameConnection()
{
   TNLAssert(gServerGame, "Client should not be using this constructor!");

   mSettings = gServerGame->getSettings();      // HACK!!  Should not refer to gServerGame!!

   mClientInfo = boost::shared_ptr<ClientInfo>(new LocalClientInfo(this, false));

   mVote = 0;
   mVoteTime = 0;
   mChatMute = false;
#ifndef ZAP_DEDICATED
   mClientGame = NULL;
#endif

	initialize();
   mPlayerInfo = new PlayerInfo(mClientInfo.get());   // Deleted in destructor
}


#ifndef ZAP_DEDICATED
// Constructor on client side
GameConnection::GameConnection(ClientGame *clientGame)
{
   initialize();

   mSettings = clientGame->getSettings();
   mClientInfo = clientGame->getClientInfo_shared_ptr();

   TNLAssert(mClientInfo->getName() != "", "Client has invalid name!");
   if(mClientInfo->getName() == "")
      mClientInfo->setName("Chump");


   setSimulatedNetParams(mSettings->getSimulatedLoss(), mSettings->getSimulatedLag());

   // These are not used on client
   mPlayerInfo = NULL;     
}
#endif


void GameConnection::initialize()
{
   setTranslatesStrings();
   mInCommanderMap = false;
   mIsBusy = false;
   mGotPermissionsReply = false;
   mWaitingForPermissionsReply = false;
   mSwitchTimer.reset(0);

   mGamesPlayed = 0;                      // Overall
   mCumulativeScore = 0;                  // Total points scored my this connection
   mTotalCumulativeScore = 0;             // Total points scored by anyone while this connection is alive

   mAcheivedConnection = false;
   mLastEnteredLevelChangePassword = "";
   mLastEnteredAdminPassword = "";

   // Things related to verification
   mClientClaimsToBeVerified = false;     // Does client report that they are verified
   mClientNeedsToBeVerified = false;      // If so, do we still need to verify that claim?
   mIsVerified = false;                   // Final conclusion... client is or is not verified
   mAuthenticationCounter = 0;            // Counts number of retries

   switchedTeamCount = 0;
   mSendableFlags = 0;
   mDataBuffer = NULL;

   reset();
}

// Destructor
GameConnection::~GameConnection()
{
   // Log the disconnect...
   if(!mClientInfo->isRobot())        // Avoid cluttering log with useless messages like "IP:any:0 - client quickbot disconnected."
      logprintf(LogConsumer::LogConnection, "%s - client \"%s\" disconnected.", getNetAddressString(), mClientInfo->getName().getString());

   if(isConnectionToClient())    // Only true if we're the server
   {
      if(gServerGame->getSuspendor() == this)     
         gServerGame->suspenderLeftGame();

      if(mAcheivedConnection)         
      {
        // Compute time we were connected
        time_t quitTime;
        time(&quitTime);

        double elapsed = difftime (quitTime, joinTime);

        logprintf(LogConsumer::ServerFilter, "%s [%s] quit [%s] (%.2lf secs)", mClientInfo->getName().getString(), 
                                                  isLocalConnection() ? "Local Connection" : getNetAddressString(), 
                                                  getTimeStamp().c_str(), elapsed);
      }
   }

   delete mDataBuffer;
   delete mPlayerInfo;
}


// Clears/initializes some things between levels
void GameConnection::reset()
{  
   //mScore = 0;
   //mRating = 0;

   mReadyForRegularGhosts = false;
   mWantsScoreboardUpdates = false;
}


// Definitive, final declaration of whether this player is (or is not) verified on this server
// Runs on both client (tracking other players) and server (tracking all players)
//void GameConnection::setAuthenticated(bool isAuthenticated)
//{ 
//   mIsVerified = isAuthenticated; 
//   mClientNeedsToBeVerified = false; 
//
//   if(isConnectionToClient())    // Only run this bit if we are a server
//   {
//      // If we are verified, we need to alert any connected clients, so they can render ships properly.  This is done via the ship object.
//      Ship *ship = dynamic_cast<Ship *>(getControlObject());
//      if(ship)
//         ship->setIsAuthenticated(isAuthenticated, mClientInfo->getName());
//   }
//}



// Old server side /getmap command, now unused, may be removed
// 1. client send /getmap command
// 2. server send map if allowed
// 3. When client get all the level map data parts, it create file and save the map

// This new client side /getmap command
// 1. client create file to write
// 2. client requent current level
// 3. server send data and client writes to file, What if sendmap not allowed?
// 4. server send CommandComplete
TNL_IMPLEMENT_RPC(GameConnection, c2sRequestCurrentLevel, (), (), NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirClientToServer, 0)
{
   if(!mSettings->getIniSettings()->allowGetMap)
   {
      s2rCommandComplete(COMMAND_NOT_ALLOWED);  
      return;
   }

   const char *filename = gServerGame->getCurrentLevelFileName().getString();
   
   // Initialize on the server to start sending requested file -- will return OK if everything is set up right
   FolderManager *folderManager = mSettings->getFolderManager();
   SenderStatus stat = gServerGame->dataSender.initialize(this, folderManager, filename, LEVEL_TYPE);

   if(stat != STATUS_OK)
   {
      const char *msg = DataConnection::getErrorMessage(stat, filename).c_str();

      logprintf(LogConsumer::LogError, "%s", msg);
      s2rCommandComplete(COULD_NOT_OPEN_FILE);
      return;
   }
}

const U32 maxDataBufferSize = 1024*256;

// << DataSendable >>
// Send a chunk of the file -- this gets run on the receiving end       
TNL_IMPLEMENT_RPC(GameConnection, s2rSendLine, (StringPtr line), (line), 
                  NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirAny, 0)
//void s2rSendLine2(StringPtr line)
{
   if(!isInitiator()) // make it client only.
      return;
   //// server might need mOutputFile, if the server were to receive files. Currently, server don't receive files in-game.
   //TNLAssert(mClientGame != NULL, "trying to get mOutputFile, mClientGame is NULL");

   //if(mClientGame && mClientGame->getUserInterface()->mOutputFile)
   //   fwrite(line.getString(), 1, strlen(line.getString()), mClientGame->getUserInterface()->mOutputFile);
      //mOutputFile.write(line.getString(), strlen(line.getString()));
   // else... what?
   if(mDataBuffer)
   {
      // Limit memory consumption:
      if(mDataBuffer->getBufferSize() < maxDataBufferSize)                                   
         mDataBuffer->appendBuffer((U8 *)line.getString(), (U32)strlen(line.getString()));
   }
   else
   {
      mDataBuffer = new ByteBuffer((U8 *)line.getString(), (U32)strlen(line.getString()));
      mDataBuffer->takeOwnership();
   }
}


// << DataSendable >>
// When sender is finished, it sends a commandComplete message
TNL_IMPLEMENT_RPC(GameConnection, s2rCommandComplete, (RangedU32<0,SENDER_STATUS_COUNT> status), (status), 
                  NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirAny, 0)
{
   if(!isInitiator()) // Make it client only
      return;

#ifndef ZAP_DEDICATED
   // Server might need mOutputFile, if the server were to receive files.  Currently, server doesn't receive files in-game.
   TNLAssert(mClientGame != NULL, "We need a clientGame to proceed...");

   if(mClientGame)
   {
      string levelDir = mSettings->getFolderManager()->levelDir;
      const char *outputFilename = strictjoindir(levelDir, mClientGame->getRemoteLevelDownloadFilename()).c_str();

      if(strcmp(outputFilename, ""))
      {
         if(status.value == STATUS_OK && mDataBuffer)
         {
            FILE *outputFile = fopen(outputFilename, "wb");

            if(!outputFile)
            {
               logprintf("Problem opening file %s for writing", outputFilename);
               mClientGame->displayErrorMessage("!!! Problem opening file %s for writing", outputFilename);
            }
            else
            {
               fwrite((char *)mDataBuffer->getBuffer(), 1, mDataBuffer->getBufferSize(), outputFile);
               fclose(outputFile);

               mClientGame->displaySuccessMessage("Level downloaded to %s", mClientGame->getRemoteLevelDownloadFilename().c_str());
            }
         }
         else if(status.value == COMMAND_NOT_ALLOWED)
            mClientGame->displayErrorMessage("!!! Getmap command is disabled on this server");
         else
            mClientGame->displayErrorMessage("Error downloading level");

         // mClientGame->setOutputFilename("");   <=== what is this for??  
      }
   }
#endif
   if(mDataBuffer)
   {
      delete mDataBuffer;
      mDataBuffer = NULL;
   }
}


extern md5wrapper md5;

void GameConnection::submitAdminPassword(const char *password)
{
   string encrypted = md5.getSaltedHashFromString(password);
   c2sAdminPassword(encrypted.c_str());

   mLastEnteredAdminPassword = password;

   setGotPermissionsReply(false);
   setWaitingForPermissionsReply(true);      // Means we'll show a reply from the server
}


void GameConnection::submitLevelChangePassword(string password)    // password here has not yet been encrypted
{
   string encrypted = md5.getSaltedHashFromString(password);
   c2sLevelChangePassword(encrypted.c_str());

   mLastEnteredLevelChangePassword = password;     // Remember the password

   setGotPermissionsReply(false);                  // No reply yet
   setWaitingForPermissionsReply(true);            // True means that we'll show a reply from the server
}


void GameConnection::suspendGame()
{
   c2sSuspendGame(true);
}


void GameConnection::unsuspendGame()
{
   c2sSuspendGame(false);
}

// Client requests that the game be suspended while he waits for other players.  This runs on the server.
TNL_IMPLEMENT_RPC(GameConnection, c2sSuspendGame, (bool suspend), (suspend), NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirClientToServer, 0)
{
   if(suspend)
      gServerGame->suspendGame(this);
   else
      gServerGame->unsuspendGame(true);
}

  
// Here, the server has sent a message to a suspended client to wake up, action's coming in hot!
// We'll also play the playerJoined sfx to alert local client that the game is on again.
TNL_IMPLEMENT_RPC(GameConnection, s2cUnsuspend, (), (), NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirServerToClient, 0)
{
#ifndef ZAP_DEDICATED
   mClientGame->unsuspendGame();       
   SoundSystem::playSoundEffect(SFXPlayerJoined, 1);
#endif
}


void GameConnection::changeParam(const char *param, ParamType type)
{
   c2sSetParam(param, type);
}


TNL_IMPLEMENT_RPC(GameConnection, c2sEngineerDeployObject, (RangedU32<0,EngineeredItemCount> type), (type), 
                  NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirClientToServer, 0)
{
   sEngineerDeployObject(type);
}


// Server only, robots can run this, bypassing the net interface. Return true if successfuly deployed.  Why is this in gameConnection???
bool GameConnection::sEngineerDeployObject(U32 type)
{
   Ship *ship = dynamic_cast<Ship *>(getControlObject());
   if(!ship)                                          // Not a good sign...
      return false;                                   // ...bail

   GameType *gameType = ship->getGame()->getGameType();

   if(!gameType->isEngineerEnabled())          // Something fishy going on here...
      return false;                                   // ...bail

   EngineerModuleDeployer deployer;

   if(!deployer.canCreateObjectAtLocation(ship->getGame()->getGameObjDatabase(), ship, type))     
      s2cDisplayErrorMessage(deployer.getErrorMessage().c_str());

   else if(deployer.deployEngineeredItem(this, type))
   {
      // Announce the build
      StringTableEntry msg( "%e0 has engineered a %e1." );

      Vector<StringTableEntry> e;
      e.push_back(mClientInfo->getName());
      e.push_back(type == EngineeredTurret ? "turret" : "force field");
   

      gameType->broadcastMessage(ColorAqua, SFXNone, msg, e);

      return true;
   }

   // else... fail silently?
   return false;
}


// Client tells the server that they claim to be authenticated.  Of course, we need to verify this ourselves.
TNL_IMPLEMENT_RPC(GameConnection, c2sSetAuthenticated, (), (), 
                  NetClassGroupGameMask, RPCGuaranteed, RPCDirClientToServer, 0)
{
   mIsVerified = false; 
   mClientNeedsToBeVerified = true; 
   mClientClaimsToBeVerified = true;

   requestAuthenticationVerificationFromMaster();
}


TNL_IMPLEMENT_RPC(GameConnection, c2sAdminPassword, (StringPtr pass), (pass), 
                  NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirClientToServer, 0)
{
   string adminPW = mSettings->getAdminPassword();

   // If admin password is blank, no one can get admin permissions except the local host, if there is one...
   if(adminPW == "" || strcmp(md5.getSaltedHashFromString(adminPW).c_str(), pass))
   {
      s2cSetIsAdmin(false);      // Tell client they have NOT been granted access
      return;
   }

   mClientInfo->setIsAdmin(true);               // Enter admin PW and...

   if(!mClientInfo->isLevelChanger())
   {
      mClientInfo->setIsLevelChanger(true);     // ...get these permissions too!
      sendLevelList();
   }
      
   s2cSetIsAdmin(true);                         // Tell client they have been granted access

   if(mSettings->getIniSettings()->allowAdminMapUpload)
      s2rSendableFlags(1);                      // Enable level uploads

   GameType *gameType = gServerGame->getGameType();

   if(gameType)
      gameType->s2cClientBecameAdmin(mClientInfo->getName());  // Announce change to world
}


// pass is our hashed password
TNL_IMPLEMENT_RPC(GameConnection, c2sLevelChangePassword, (StringPtr pass), (pass), NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirClientToServer, 0)
{
   string levChangePW = mSettings->getLevelChangePassword();

   // If password is blank, permissions always granted
   if(levChangePW != "" && strcmp(md5.getSaltedHashFromString(levChangePW).c_str(), pass))
   {
      s2cSetIsLevelChanger(false, true);  // Tell client they have NOT been granted access
      return;
   }

   mClientInfo->setIsLevelChanger(true);

   s2cSetIsLevelChanger(true, true);      // Tell client they have been granted access
   sendLevelList();                       // Send client the level list

   // Announce change to world
   gServerGame->getGameType()->s2cClientBecameLevelChanger(mClientInfo->getName());  
}


extern ServerGame *gServerGame;

// Allow admins to change the passwords on their systems
TNL_IMPLEMENT_RPC(GameConnection, c2sSetParam, (StringPtr param, RangedU32<0, GameConnection::ParamTypeCount> type), (param, type),
                  NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirClientToServer, 0)
{
   if(!mClientInfo->isAdmin())   // Do nothing --> non-admins have no pull here.  Note that this should never happen; client should filter out
      return;                    // non-admins before we get here, but we'll check anyway in case the client has been hacked.

   // Check for forbidden blank parameters -- the following commands require a value to be passed in param
   if( (type == (U32)AdminPassword || type == (U32)ServerName || type == (U32)ServerDescr || type == (U32)LevelDir) &&
                          !strcmp(param.getString(), ""))
      return;

   // Add a message to the server log
   if(type == (U32)DeleteLevel)
      logprintf(LogConsumer::ServerFilter, "User [%s] added level [%s] to server skip list", mClientInfo->getName().getString(), 
                                                gServerGame->getCurrentLevelFileName().getString());
   else
   {
      const char *types[] = { "level change password", "admin password", "server password", "server name", "server description", "leveldir param" };
      logprintf(LogConsumer::ServerFilter, "User [%s] %s to [%s]", mClientInfo->getName().getString(), 
                                                                   strcmp(param.getString(), "") ? "changed" : "cleared", types[type]);
   }

   // Update our in-memory copies of the param, but do not save the new values to the INI
   if(type == (U32)LevelChangePassword)
      mSettings->setLevelChangePassword(param.getString(), false);
   
   else if(type == (U32)AdminPassword)
      mSettings->setAdminPassword(param.getString(), false);
   
   else if(type == (U32)ServerPassword)
      mSettings->setServerPassword(param.getString(), false);
   
   else if(type == (U32)ServerName)
   {
      mSettings->setHostName(param.getString(), false);
      if(gServerGame->getConnectionToMaster())
         gServerGame->getConnectionToMaster()->s2mChangeName(StringTableEntry(param.getString()));   
   }
   else if(type == (U32)ServerDescr)
   {
      mSettings->setHostDescr(param.getString(), false);

      if(gServerGame->getConnectionToMaster())
         gServerGame->getConnectionToMaster()->s2mServerDescription(StringTableEntry(param.getString()));
   }

   else if(type == (U32)LevelDir)
   {
      FolderManager *folderManager = mSettings->getFolderManager();
      string candidate = folderManager->resolveLevelDir(param.getString());

      if(folderManager->levelDir == candidate)
      {
         s2cDisplayErrorMessage("!!! Specified folder is already the current level folder");
         return;
      }

      // Make sure the specified dir exists; hopefully it contains levels
      if(candidate == "" || !fileExists(candidate))
      {
         s2cDisplayErrorMessage("!!! Could not find specified folder");
         return;
      }

      Vector<string> newLevels = mSettings->getLevelList(candidate);

      if(newLevels.size() == 0)
      {
         s2cDisplayErrorMessage("!!! Specified folder contains no levels");
         return;
      }

      gServerGame->buildBasicLevelInfoList(newLevels);      // Populates mLevelInfos on gServerGame with nearly empty LevelInfos 

      bool anyLoaded = false;

      for(S32 i = 0; i < newLevels.size(); i++)
      {
         string levelFile = folderManager->findLevelFile(candidate, newLevels[i]);

         LevelInfo levelInfo(newLevels[i]);
         if(gServerGame->getLevelInfo(levelFile, levelInfo))
         {
            if(!anyLoaded)    // i.e. we just found the first valid level; safe to clear out old list
               gServerGame->clearLevelInfos();

            anyLoaded = true;

            gServerGame->addLevelInfo(levelInfo);
         }
      }

      if(!anyLoaded)
      {
         s2cDisplayErrorMessage("!!! Specified folder contains no valid levels.  See server log for details.");
         return;
      }

      folderManager->levelDir = candidate;

      // Send the new list of levels to all levelchangers
      for(S32 i = 0; i < gServerGame->getClientCount(); i++)
      {
         ClientInfo *clientInfo = gServerGame->getClientInfo(i);
         GameConnection *conn = clientInfo->getConnection();

         if(clientInfo->isLevelChanger())
            conn->sendLevelList();
      }

      s2cDisplayMessage(ColorAqua, SFXNone, "Level folder changed");

   }  // end change leveldir

   else if(type == (U32)DeleteLevel)
   {
      // Avoid duplicates on skip list
      bool found = false;
      Vector<string> *skipList = mSettings->getLevelSkipList();

      for(S32 i = 0; i < skipList->size(); i++)
         if(skipList->get(i) == gServerGame->getCurrentLevelFileName().getString())    // Already on our list!
         {
            found = true;
            break;
         }

      if(!found)
      {
         // Add level to our skip list.  Deleting it from the active list of levels is more of a challenge...
         skipList->push_back(gServerGame->getCurrentLevelFileName().getString());
         writeSkipList(&gINI, skipList);           // Write skipped levels to INI
         gINI.WriteFile();                         // Save new INI settings to disk
      }
   }

   if(type != (U32)DeleteLevel && type != (U32)LevelDir)
   {
      const char *keys[] = { "LevelChangePassword", "AdminPassword", "ServerPassword", "ServerName", "ServerDescription" };

      // Update the INI file
      gINI.SetValue("Host", keys[type], param.getString(), true);
      gINI.WriteFile();    // Save new INI settings to disk
   }

   // Some messages we might show the user... should these just be inserted directly below?
   static StringTableEntry levelPassChanged   = "Level change password changed";
   static StringTableEntry levelPassCleared   = "Level change password cleared -- anyone can change levels";
   static StringTableEntry adminPassChanged   = "Admin password changed";
   static StringTableEntry serverPassChanged  = "Server password changed -- only players with the password can connect";
   static StringTableEntry serverPassCleared  = "Server password cleared -- anyone can connect";
   static StringTableEntry serverNameChanged  = "Server name changed";
   static StringTableEntry serverDescrChanged = "Server description changed";
   static StringTableEntry serverLevelDeleted = "Level added to skip list; level will stay in rotation until server restarted";

   // Pick out just the right message
   StringTableEntry msg;

   if(type == (U32)LevelChangePassword)
   {
      msg = strcmp(param.getString(), "") ? levelPassChanged : levelPassCleared;

      // If we're clearning the level change password, quietly grant access to anyone who doesn't already have it
      if(!strcmp(param.getString(), ""))
      {
         for(S32 i = 0; i < gServerGame->getClientCount(); i++)
         {
            ClientInfo *clientInfo = gServerGame->getClientInfo(i);
            GameConnection *conn = clientInfo->getConnection();

            if(!clientInfo->isLevelChanger())
            {
               clientInfo->setIsLevelChanger(true);
               conn->sendLevelList();
               conn->s2cSetIsLevelChanger(true, false);     // Silently
            }
         }
      }
      else  // If setting a password, remove everyone's permissions (except admins)
      { 
         for(S32 i = 0; i < gServerGame->getClientCount(); i++)
         {
            ClientInfo *clientInfo = gServerGame->getClientInfo(i);
            GameConnection *conn = clientInfo->getConnection();

            if(clientInfo->isLevelChanger() && (!clientInfo->isAdmin()))
            {
               clientInfo->setIsLevelChanger(false);
               conn->s2cSetIsLevelChanger(false, false);
            }
         }
      }
   }
   else if(type == (U32)AdminPassword)
      msg = adminPassChanged;
   else if(type == (U32)ServerPassword)
      msg = strcmp(param.getString(), "") ? serverPassChanged : serverPassCleared;
   else if(type == (U32)ServerName)
   {
      msg = serverNameChanged;

      // If we've changed the server name, notify all the clients
      for(S32 i = 0; i < gServerGame->getClientCount(); i++)
         gServerGame->getClientInfo(i)->getConnection()->s2cSetServerName(mSettings->getHostName());
   }
   else if(type == (U32)ServerDescr)
      msg = serverDescrChanged;
   else if(type == (U32)DeleteLevel)
      msg = serverLevelDeleted;

   s2cDisplayMessage(ColorRed, SFXNone, msg);      // Notify user their bidding has been done
}


// Kick player or change his team
TNL_IMPLEMENT_RPC(GameConnection, c2sAdminPlayerAction,
                     (StringTableEntry playerName, U32 actionIndex, S32 team), (playerName, actionIndex, team),
                     NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirClientToServer, 0)
{
   if(!mClientInfo->isAdmin())
      return;              // Do nothing --> non-admins have no pull here

   // else...

   GameType *gt = gServerGame->getGameType();

   // Find connection for player with name playerName
   ClientInfo *clientInfo = gt->getGame()->findClientInfo(playerName);
   GameConnection *theClient = clientInfo->getConnection();

   if(!clientInfo)    // Hmmm... couldn't find the dude.  Maybe he disconnected?
      return;

   static StringTableEntry kickMessage("%e0 was kicked from the game by %e1.");
   static StringTableEntry changeTeamMessage("%e0 had their team changed by %e1.");

   StringTableEntry msg;
   Vector<StringTableEntry> e;
   e.push_back(clientInfo->getName());       // --> Name of player being administered
   e.push_back(getClientInfo()->getName());  // --> Name of player doing the adminstering

   switch(actionIndex)
   {
      case PlayerMenuUserInterface::ChangeTeam:
         msg = changeTeamMessage;
         gt->changeClientTeam(clientInfo, team);
         break;

      case PlayerMenuUserInterface::Kick:
         msg = kickMessage;
         if(mClientInfo->isAdmin())
         {
            static StringTableEntry nokick("Can't kick an administrator!");
            s2cDisplayMessage(ColorAqua, SFXNone, nokick);
            return;
         }

         if(theClient->isEstablished())     // Robots don't have established connections
         {
            ConnectionParameters &p = theClient->getConnectionParameters();

            if(p.mIsArranged)
               gServerGame->getSettings()->getBanList()->kickHost(p.mPossibleAddresses[0]);      // Banned for 30 seconds

            gServerGame->getSettings()->getBanList()->kickHost(theClient->getNetAddress());      // Banned for 30 seconds
            theClient->disconnect(ReasonKickedByAdmin, "");
         }

         for(S32 i = 0; i < Robot::robots.size(); i++)
            if(Robot::robots[i]->getName() == clientInfo->getName())
               delete Robot::robots[i];

         break;

      default:
         return;
   }

   gt->broadcastMessage(ColorAqua, SFXIncomingMessage, msg, e);
}


//// Announce a new player has become an admin
//TNL_IMPLEMENT_RPC(GameConnection, s2cClientBecameAdmin, (StringTableEntry name), (name), NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirServerToClient, 1)
//{
//   ClientRef *cl = findClientConnection(name);
//   cl->clientConnection->isAdmin = true;
//   getUserInterface().displayMessage(Color(0,1,1), "%s has been granted administrator access.", name.getString());
//}


// This gets called under two circumstances; when it's a new game, or when the server's name is changed by an admin
TNL_IMPLEMENT_RPC(GameConnection, s2cSetServerName, (StringTableEntry name), (name),
   NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirServerToClient, 0)
{
   setServerName(name);

   // If we know the level change password, apply for permissions if we don't already have them
   if(!mClientInfo->isLevelChanger())
   {
      string levelChangePassword = gINI.GetValue("SavedLevelChangePasswords", getServerName());
      if(levelChangePassword != "")
      {
         c2sLevelChangePassword(md5.getSaltedHashFromString(levelChangePassword).c_str());
         setWaitingForPermissionsReply(false);     // Want to return silently
      }
   }

   // If we know the admin password, apply for permissions if we don't already have them
   if(!mClientInfo->isAdmin())
   {
      string adminPassword = gINI.GetValue("SavedAdminPasswords", getServerName());
      if(adminPassword != "")
      {
         c2sAdminPassword(md5.getSaltedHashFromString(adminPassword).c_str());
         setWaitingForPermissionsReply(false);     // Want to return silently
      }
   }
}


extern Color gCmdChatColor;

TNL_IMPLEMENT_RPC(GameConnection, s2cSetIsAdmin, (bool granted), (granted),
   NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirServerToClient, 0)
{
#ifndef ZAP_DEDICATED

   ClientInfo *clientInfo = mClientGame->getClientInfo();

   if(granted)
      logprintf(LogConsumer::ServerFilter, "User [%s] granted admin permissions", clientInfo->getName().getString());
   else
      logprintf(LogConsumer::ServerFilter, "User [%s] denied admin permissions", clientInfo->getName().getString());

   clientInfo->setIsAdmin(granted);

   // Admin permissions automatically give level change permission
   if(granted)                      // Don't rescind level change permissions for entering a bad admin PW
   {
      if(!clientInfo->isLevelChanger())
         sendLevelList();

      clientInfo->setIsLevelChanger(true);
   }


   // If we entered a password, and it worked, let's save it for next time
   if(granted && mLastEnteredAdminPassword != "")
   {
      gINI.SetValue("SavedAdminPasswords", getServerName(), mLastEnteredAdminPassword, true);
      mLastEnteredAdminPassword = "";
   }

   // We have the wrong password, let's make sure it's not saved
   if(!granted)
      gINI.deleteKey("SavedAdminPasswords", getServerName());

   setGotPermissionsReply(true);

   // If we're not waiting, don't do anything.  Supresses superflous messages on startup.
   if(waitingForPermissionsReply())
      mClientGame->gotAdminPermissionsReply(granted);
#endif
}


TNL_IMPLEMENT_RPC(GameConnection, s2cSetIsLevelChanger, (bool granted, bool notify), (granted, notify),
   NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirServerToClient, 0)
{
#ifndef ZAP_DEDICATED
   logprintf(LogConsumer::ServerFilter, "User [%s] %s level change permissions", granted ? "granted" : "denied", mClientInfo->getName().getString());

   // If we entered a password, and it worked, let's save it for next time
   if(granted && mLastEnteredLevelChangePassword != "")
   {
      gINI.SetValue("SavedLevelChangePasswords", getServerName(), mLastEnteredLevelChangePassword, true);
      mLastEnteredLevelChangePassword = "";
   }

   // We have the wrong password, let's make sure it's not saved
   if(!granted)
      gINI.deleteKey("SavedLevelChangePasswords", getServerName());

   ClientInfo *clientInfo = mClientGame->getClientInfo();

   // Check for permissions being rescinded by server, will happen if admin changes level change pw
   if(clientInfo->isLevelChanger() && !granted)
      mClientGame->displayMessage(gCmdChatColor, "An admin has changed the level change password; you must enter the new password to change levels.");

   clientInfo->setIsLevelChanger(granted);

   setGotPermissionsReply(true);

   // If we're not waiting, don't show us a message.  Supresses superflous messages on startup.
   if(waitingForPermissionsReply() && notify)
      mClientGame->gotLevelChangePermissionsReply(granted);
#endif
}


TNL_IMPLEMENT_RPC(GameConnection, c2sRequestCommanderMap, (), (),
   NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirClientToServer, 0)
{
   mInCommanderMap = true;
}

TNL_IMPLEMENT_RPC(GameConnection, c2sReleaseCommanderMap, (), (),
   NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirClientToServer, 0)
{
   mInCommanderMap = false;
}

// Client has changed his loadout configuration.  This gets run on the server as soon as the loadout is entered.
TNL_IMPLEMENT_RPC(GameConnection, c2sRequestLoadout, (Vector<U32> loadout), (loadout), NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirClientToServer, 0)
{
   sRequestLoadout(loadout);
}
void GameConnection::sRequestLoadout(Vector<U32> &loadout)
{
   mLoadout = loadout;
   GameType *gt = gServerGame->getGameType();
   if(gt)
      gt->SRV_clientRequestLoadout(this, mLoadout);    // This will set loadout if ship is in loadout zone

   // Check if ship is in a loadout zone, in which case we'll make the loadout take effect immediately
   //Ship *ship = dynamic_cast<Ship *>(this->getControlObject());

   //if(ship && ship->isInZone(LoadoutZoneType))
      //ship->setLoadout(loadout);
}

Color colors[] =
{
   Colors::white,          // ColorWhite
   Colors::red,            // ColorRed    ==> also used for chat commands
   Colors::green,          // ColorGreen
   Colors::blue,           // ColorBlue
   Colors::cyan,           // ColorAqua
   Colors::yellow,         // ColorYellow
   Color(0.6f, 1, 0.8f),   // ColorNuclearGreen
};


Color gCmdChatColor = colors[GameConnection::ColorRed];

void GameConnection::displayMessage(U32 colorIndex, U32 sfxEnum, const char *message)
{
#ifndef ZAP_DEDICATED
   mClientGame->displayMessage(colors[colorIndex], "%s", message);
   if(sfxEnum != SFXNone)
      SoundSystem::playSoundEffect(sfxEnum);
#endif
}


// I believe this is not used -CE
TNL_IMPLEMENT_RPC(GameConnection, s2cDisplayMessageESI,
                  (RangedU32<0, GameConnection::ColorCount> color, RangedU32<0, NumSFXBuffers> sfx, StringTableEntry formatString,
                  Vector<StringTableEntry> e, Vector<StringPtr> s, Vector<S32> i),
                  (color, sfx, formatString, e, s, i),
                  NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirServerToClient, 0)
{
   char outputBuffer[256];
   S32 pos = 0;
   const char *src = formatString.getString();
   while(*src)
   {
      if(src[0] == '%' && (src[1] == 'e' || src[1] == 's' || src[1] == 'i') && (src[2] >= '0' && src[2] <= '9'))
      {
         S32 index = src[2] - '0';
         switch(src[1])
         {
            case 'e':
               if(index < e.size())
                  pos += dSprintf(outputBuffer + pos, 256 - pos, "%s", e[index].getString());
               break;
            case 's':
               if(index < s.size())
                  pos += dSprintf(outputBuffer + pos, 256 - pos, "%s", s[index].getString());
               break;
            case 'i':
               if(index < i.size())
                  pos += dSprintf(outputBuffer + pos, 256 - pos, "%d", i[index]);
               break;
         }
         src += 3;
      }
      else
         outputBuffer[pos++] = *src++;

      if(pos >= 255)
         break;
   }
   outputBuffer[pos] = 0;
   displayMessage(color, sfx, outputBuffer);
}


TNL_IMPLEMENT_RPC(GameConnection, s2cDisplayMessageE,
                  (RangedU32<0, GameConnection::ColorCount> color, RangedU32<0, NumSFXBuffers> sfx, StringTableEntry formatString,
                  Vector<StringTableEntry> e), (color, sfx, formatString, e),
                  NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirServerToClient, 0)
{
   displayMessageE(color, sfx, formatString, e);
}


TNL_IMPLEMENT_RPC(GameConnection, s2cTouchdownScored,
                  (U32 sfx, S32 team, StringTableEntry formatString, Vector<StringTableEntry> e),
                  (sfx, team, formatString, e),
                  NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirServerToClient, 0)
{
#ifndef ZAP_DEDICATED
   displayMessageE(GameConnection::ColorNuclearGreen, sfx, formatString, e);
   mClientGame->getGameType()->majorScoringEventOcurred(team);
#endif
}


void GameConnection::displayMessageE(U32 color, U32 sfx, StringTableEntry formatString, Vector<StringTableEntry> e)
{
   char outputBuffer[256];
   S32 pos = 0;
   const char *src = formatString.getString();
   while(*src)
   {
      if(src[0] == '%' && (src[1] == 'e') && (src[2] >= '0' && src[2] <= '9'))
      {
         S32 index = src[2] - '0';
         switch(src[1])
         {
            case 'e':
               if(index < e.size())
                  pos += dSprintf(outputBuffer + pos, 256 - pos, "%s", e[index].getString());
               break;
         }
         src += 3;
      }
      else
         outputBuffer[pos++] = *src++;

      if(pos >= 255)
         break;
   }
   outputBuffer[pos] = 0;
   displayMessage(color, sfx, outputBuffer);
}


void GameConnection::sendLevelList()
{
   // Send blank entry to clear the remote list
   s2cAddLevel("", "");    

   // Build list remotely by sending level names one-by-one
   for(S32 i = 0; i < gServerGame->getLevelCount(); i++)
   {
      LevelInfo levelInfo = gServerGame->getLevelInfo(i);
      s2cAddLevel(levelInfo.levelName, levelInfo.levelType);
   }
}


//class RPC_GameConnection_s2cDisplayMessage : public TNL::RPCEvent { \
//public: \
//   TNL::FunctorDecl<void (GameConnection::*) args > mFunctorDecl;\
//   RPC_GameConnection_s2cDisplayMessage() : TNL::RPCEvent(guaranteeType, eventDirection), mFunctorDecl(&GameConnection::s2cDisplayMessage_remote) { mFunctor = &mFunctorDecl; } \
//   TNL_DECLARE_CLASS( RPC_GameConnection_s2cDisplayMessage ); \
//   bool checkClassType(TNL::Object *theObject) { return dynamic_cast<GameConnection *>(theObject) != NULL; } }; \
//   TNL_IMPLEMENT_NETEVENT( RPC_GameConnection_s2cDisplayMessage, groupMask, rpcVersion ); \
//   void GameConnection::name args { if(!canPostNetEvent()) return; RPC_GameConnection_s2cDisplayMessage *theEvent = new RPC_GameConnection_s2cDisplayMessage; theEvent->mFunctorDecl.set argNames ; postNetEvent(theEvent); } \
//   TNL::NetEvent * GameConnection::s2cDisplayMessage_construct args { RPC_GameConnection_s2cDisplayMessage *theEvent = new RPC_GameConnection_s2cDisplayMessage; theEvent->mFunctorDecl.set argNames ; return theEvent; } \
//   void GameConnection::s2cDisplayMessage_test args { RPC_GameConnection_s2cDisplayMessage *theEvent = new RPC_GameConnection_s2cDisplayMessage; theEvent->mFunctorDecl.set argNames ; TNL::PacketStream ps; theEvent->pack(this, &ps); ps.setBytePosition(0); theEvent->unpack(this, &ps); theEvent->process(this); } \
//   void GameConnection::s2cDisplayMessage_remote args


TNL_IMPLEMENT_RPC(GameConnection, s2cDisplayMessage,
                  (RangedU32<0, GameConnection::ColorCount> color, RangedU32<0, NumSFXBuffers> sfx, StringTableEntry formatString),
                  (color, sfx, formatString),
                  NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirServerToClient, 0)
{
   displayMessage(color, sfx, formatString.getString());
}


TNL_IMPLEMENT_RPC(GameConnection, s2cDisplayErrorMessage,
                  (StringTableEntry formatString),
                  (formatString),
                  NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirServerToClient, 0)
{
#ifndef ZAP_DEDICATED
   mClientGame->displayErrorMessage(formatString.getString());
#endif
}


TNL_IMPLEMENT_RPC(GameConnection, s2cDisplayMessageBox, (StringTableEntry title, StringTableEntry instr, Vector<StringTableEntry> message),
                  (title, instr, message), NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirServerToClient, 0)
{
#ifndef ZAP_DEDICATED
   mClientGame->displayMessageBox(title, instr, message);
#endif
}


// Server sends the name and type of a level to the client (gets run repeatedly when client connects to the server). 
// Sending a blank name and type will clear the list.
TNL_IMPLEMENT_RPC(GameConnection, s2cAddLevel, (StringTableEntry name, StringTableEntry type), (name, type),
                  NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirServerToClient, 0)
{
   // Sending a blank name and type will clear the list.  Type should never be blank except in this use case, so check it first.
   if(type == "" && name == "")
      mLevelInfos.clear();
   else
      mLevelInfos.push_back(LevelInfo(name, type));
}


// Server sends the level that got removed, or removes all levels from list when index is -1
// Unused??
TNL_IMPLEMENT_RPC(GameConnection, s2cRemoveLevel, (S32 index), (index),
                  NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirServerToClient, 0)
{
   if(index < 0)
      mLevelInfos.clear();
   else if(index < mLevelInfos.size())
      mLevelInfos.erase(index);
}


TNL_IMPLEMENT_RPC(GameConnection, c2sRequestLevelChange, (S32 newLevelIndex, bool isRelative), (newLevelIndex, isRelative), 
                              NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirClientToServer, 0)
{
   c2sRequestLevelChange2(newLevelIndex, isRelative);
}


void GameConnection::c2sRequestLevelChange2(S32 newLevelIndex, bool isRelative)
{
   if(!mClientInfo->isLevelChanger())
      return;

   // Use voting when there is no level change password and there is more then 1 player (unless changer is an admin)
   if(!mClientInfo->isAdmin() && mSettings->getLevelChangePassword().length() == 0 && 
         gServerGame->getPlayerCount() > 1 && gServerGame->voteStart(mClientInfo.get(), 0, newLevelIndex))
      return;

   bool restart = false;

   if(isRelative)
      newLevelIndex = (gServerGame->getCurrentLevelIndex() + newLevelIndex ) % gServerGame->getLevelCount();
   else if(newLevelIndex == ServerGame::REPLAY_LEVEL)
      restart = true;

   StringTableEntry msg( restart ? "%e0 restarted the current level." : "%e0 changed the level to %e1." );
   Vector<StringTableEntry> e;
   e.push_back(mClientInfo->getName());
   
   if(!restart)
      e.push_back(gServerGame->getLevelNameFromIndex(newLevelIndex));

   gServerGame->cycleLevel(newLevelIndex);

   gServerGame->getGameType()->broadcastMessage(ColorYellow, SFXNone, msg, e);
}


TNL_IMPLEMENT_RPC(GameConnection, c2sRequestShutdown, (U16 time, StringPtr reason), (time, reason), 
                  NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirClientToServer, 0)
{
   if(!mClientInfo->isAdmin())
      return;

   logprintf(LogConsumer::ServerFilter, "User [%s] requested shutdown in %d seconds [%s]", 
         mClientInfo->getName().getString(), time, reason.getString());

   gServerGame->setShuttingDown(true, time, this, reason.getString());

   // Broadcast the message
   for(S32 i = 0; i < gServerGame->getClientCount(); i++)
   {
      GameConnection *conn = gServerGame->getClientInfo(i)->getConnection();
      conn->s2cInitiateShutdown(time, mClientInfo->getName(), reason, conn == this);
   }
}


TNL_IMPLEMENT_RPC(GameConnection, s2cInitiateShutdown, (U16 time, StringTableEntry name, StringPtr reason, bool originator),
                  (time, name, reason, originator), NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirServerToClient, 0)
{
#ifndef ZAP_DEDICATED
   mClientGame->shutdownInitiated(time, name, reason, originator);
#endif
}


TNL_IMPLEMENT_RPC(GameConnection, c2sRequestCancelShutdown, (), (), NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirClientToServer, 0)
{
   if(!mClientInfo->isAdmin())
      return;

   logprintf(LogConsumer::ServerFilter, "User %s canceled shutdown", mClientInfo->getName().getString());

   // Broadcast the message
   for(S32 i = 0; i < gServerGame->getClientCount(); i++)
   {
      GameConnection *conn = gServerGame->getClientInfo(i)->getConnection();

      if(conn != this)     // Don't send message to cancellor!
         conn->s2cCancelShutdown();
   }

   gServerGame->setShuttingDown(false, 0, NULL, "");
}


TNL_IMPLEMENT_RPC(GameConnection, s2cCancelShutdown, (), (), NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirServerToClient, 0)
{
#ifndef ZAP_DEDICATED
   mClientGame->cancelShutdown();
#endif
}


// Client tells server that they are busy chatting or futzing with menus or configuring ship... or not
TNL_IMPLEMENT_RPC(GameConnection, c2sSetIsBusy, (bool busy), (busy), NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirClientToServer, 0)
{
   setIsBusy(busy);
}


TNL_IMPLEMENT_RPC(GameConnection, c2sSetServerAlertVolume, (S8 vol), (vol), NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirClientToServer, 0)
{
   //setServerAlertVolume(vol);
}


// Tell all clients name has changed, and update server side name
// Server only
void updateClientChangedName(ClientInfo *clientInfo, StringTableEntry newName)
{
   GameType *gameType = gServerGame->getGameType();

   logprintf(LogConsumer::LogConnection, "Name changed from %s to %s", clientInfo->getName().getString(), newName.getString());

   if(gameType)
      gameType->s2cRenameClient(clientInfo->getName(), newName);

   clientInfo->setName(newName);

   Ship *ship = dynamic_cast<Ship *>(clientInfo->getConnection()->getControlObject());

   if(ship)
   {
      ship->setName(newName);
      ship->setMaskBits(Ship::AuthenticationMask);    // Will trigger sending new ship name on next update
   }
}


// Client connects to master after joining a game, authentication fails,
// then client has changed name to non-reserved, or entered password.
TNL_IMPLEMENT_RPC(GameConnection, c2sRenameClient, (StringTableEntry newName), (newName), NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirClientToServer, 0)
{
   StringTableEntry oldName = mClientInfo->getName();
   mClientInfo->setName("");     

   StringTableEntry uniqueName = GameConnection::makeUnique(newName.getString()).c_str();    // Make sure new name is unique
   mClientInfo->setName(oldName);         // Restore name to properly get it updated to clients
   setClientNameNonUnique(newName);       // For correct authentication
   
   mIsVerified = false;                   // Renamed names are never verified
   mClientInfo->setAuthenticated(false);  // Prevents name from being underlined
   mClientNeedsToBeVerified = false;      // Prevent attempts to authenticate the new name
   mClientClaimsToBeVerified = false;

   if(oldName != uniqueName)              // Did the name actually change?
      updateClientChangedName(mClientInfo.get(), uniqueName);
}


TNL_IMPLEMENT_RPC(GameConnection, s2rSendableFlags, (U8 flags), (flags), NetClassGroupGameMask, RPCGuaranteed, RPCDirAny, 0)
{
   mSendableFlags = flags;
}


extern LevelInfo getLevelInfoFromFileChunk(char *chunk, S32 size, LevelInfo &levelInfo);

TNL_IMPLEMENT_RPC(GameConnection, s2rSendDataParts, (U8 type, ByteBufferPtr data), (type, data), NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirAny, 0)
{
   if(!mSettings->getIniSettings()->allowMapUpload && !mClientInfo->isAdmin())  // Don't need it when not enabled, saves some memory. May remove this, it is checked again leter.
      return;

   if(mDataBuffer)
   {
      if(mDataBuffer->getBufferSize() < maxDataBufferSize)  // Limit memory consumption
         mDataBuffer->appendBuffer(*data.getPointer());
   }
   else
   {
      mDataBuffer = new ByteBuffer(*data.getPointer());
      mDataBuffer->takeOwnership();
   }

   if(type == 1 &&      // TODO: 1 Should be an enum, means we're transmitting a level file.  Probably already have an enum that would work.
         (mSettings->getIniSettings()->allowMapUpload || (mSettings->getIniSettings()->allowAdminMapUpload && mClientInfo->isAdmin())) &&
         !isInitiator() && mDataBuffer->getBufferSize() != 0)
   {
      // Only server runs this part of code
      FolderManager *folderManager = mSettings->getFolderManager();

      LevelInfo levelInfo("Transmitted Level");
      getLevelInfoFromFileChunk((char *)mDataBuffer->getBuffer(), mDataBuffer->getBufferSize(), levelInfo);

      //BitStream s(mDataBuffer.getBuffer(), mDataBuffer.getBufferSize());
      char filename[128];

      string titleName = makeFilenameFromString(levelInfo.levelName.getString());
      dSprintf(filename, sizeof(filename), "upload_%s.level", titleName.c_str());

      string fullFilename = strictjoindir(folderManager->levelDir, filename);

      FILE *f = fopen(fullFilename.c_str(), "wb");
      if(f)
      {
         fwrite(mDataBuffer->getBuffer(), 1, mDataBuffer->getBufferSize(), f);
         fclose(f);
         logprintf(LogConsumer::ServerFilter, "%s %s Uploaded %s", getNetAddressString(), mClientInfo->getName().getString(), filename);
         S32 id = gServerGame->addUploadedLevelInfo(filename, levelInfo);
         c2sRequestLevelChange2(id, false);
      }
      else
         s2cDisplayErrorMessage("!!! Upload failed -- server can't write file");
   }

   if(type != 0)
   {
      delete mDataBuffer;
      mDataBuffer = NULL;
   }
}


bool GameConnection::s2rUploadFile(const char *filename, U8 type)
{
   BitStream s;
   const U32 partsSize = 512;   // max 1023, limited by ByteBufferSizeBitSize value of 10

   FILE *f = fopen(filename, "rb");

   if(f)
   {
      U32 size = partsSize;
      while(size == partsSize)
      {
         ByteBuffer *bytebuffer = new ByteBuffer(512);

         size = (U32)fread(bytebuffer->getBuffer(), 1, bytebuffer->getBufferSize(), f);

         if(size != partsSize)
            bytebuffer->resize(size);

         s2rSendDataParts(size == partsSize ? 0 : type, ByteBufferPtr(bytebuffer));
      }
      fclose(f);
      return true;
   }

   return false;
}


// Send password, client's name, and version info to game server
void GameConnection::writeConnectRequest(BitStream *stream)
{
#ifndef ZAP_DEDICATED
   Parent::writeConnectRequest(stream);

   bool isLocal = gServerGame;      // Only way to have gServerGame defined is if we're also hosting... ergo, we must be local

   string serverPW;
   string lastServerName = mClientGame->getRequestedServerName();

   // If we're local, just use the password we already know because, you know, we're the server
   if(isLocal)
      serverPW = md5.getSaltedHashFromString(mSettings->getServerPassword());

   // If we have a saved password for this server, use that
   else if(gINI.GetValue("SavedServerPasswords", lastServerName) != "")
      serverPW = md5.getSaltedHashFromString(gINI.GetValue("SavedServerPasswords", lastServerName)); 

   // Otherwise, use whatever's in the interface entry box
   else 
      serverPW = mClientGame->getHashedServerPassword();

   // Write some info about the client... name, id, and verification status
   stream->writeString(serverPW.c_str());
   stream->writeString(mClientInfo->getName().getString());

    mClientInfo->getId()->write(stream);

   stream->writeFlag(mIsVerified);    // Tell server whether we (the client) claim to be authenticated
#endif
}


// On the server side of things, read the connection request, and return if everything looks legit.  If not, provide an error string
// to help diagnose the problem, or prompt further data from client (such as a password).
// Note that we'll always go through this, even if the client is running on in the same process as the server.
// Note also that mSettings will be NULL here.
bool GameConnection::readConnectRequest(BitStream *stream, NetConnection::TerminationReason &reason)
{
   if(!Parent::readConnectRequest(stream, reason))
      return false;

   char buf[256];

   stream->readString(buf);
   string serverPassword = gServerGame->getSettings()->getServerPassword();

   if(serverPassword != "" && stricmp(buf, md5.getSaltedHashFromString(serverPassword).c_str()))
   {
      reason = ReasonNeedServerPassword;
      return false;
   }

   // Now read the player name, id, and verification status
   stream->readString(buf);
   size_t len = strlen(buf);

   if(len > MAX_PLAYER_NAME_LENGTH)      // Make sure it isn't too long
      len = MAX_PLAYER_NAME_LENGTH;

   // Clean up name, render it safe

   // Strip leading and trailing spaces...
   char *name = buf;
   while(len && *name == ' ')
   {
      name++;
      len--;
   }
   while(len && name[len-1] == ' ')
      len--;

   // Remove invisible chars and quotes
   for(size_t i = 0; i < len; i++)
      if(name[i] < ' ' || name[i] > '~' || name[i] == '"')
         name[i] = 'X';

   name[len] = 0;    // Terminate string properly

   // Now that we have the name, check if the client is banned
   if(gServerGame->getSettings()->getBanList()->isBanned(getNetAddress().toString(), string(name)))
   {
      reason = ReasonBanned;
      return false;
   }

   // Was the client kicked temporarily?
   if(gServerGame->getSettings()->getBanList()->isAddressKicked(getNetAddress()))
   {
      reason = ReasonKickedByAdmin;
      return false;
   }

   // Is the server Full?
   if(gServerGame->isFull())
   {
      reason = ReasonServerFull;
      return false;
   }

   // Change name to be unique - i.e. if we have multiples of 'ChumpChange'
   mClientInfo->setName(makeUnique(name));   // Unique name
   mClientNameNonUnique = name;              // For authentication non-unique name

   mClientInfo->getId()->read(stream);
   mIsVerified = false;
   mClientNeedsToBeVerified = mClientClaimsToBeVerified = stream->readFlag();

   requestAuthenticationVerificationFromMaster();

   return true;
}


void GameConnection::updateAuthenticationTimer(U32 timeDelta)
{
   if(mAuthenticationTimer.update(timeDelta))
      requestAuthenticationVerificationFromMaster();
}


void GameConnection::requestAuthenticationVerificationFromMaster()
{
   MasterServerConnection *masterConn = gServerGame->getConnectionToMaster();

   // Ask master if client name/id match and the client is authenticated
   if(masterConn && masterConn->isEstablished() && mClientClaimsToBeVerified)
      masterConn->requestAuthentication(mClientNameNonUnique, *mClientInfo->getId());   
}


// Make sure name is unique.  If it's not, make it so.  The problem is that then the client doesn't know their official name.
// This makes the assumption that we'll find a unique name before numstr runs out of space (allowing us to try 999,999,999 or so combinations)
string GameConnection::makeUnique(string name)
{
   U32 index = 0;
   string proposedName = name;

   bool unique = false;

   while(!unique)
   {
      unique = true;

      for(S32 i = 0; i < gServerGame->getClientCount(); i++)
      {
         // TODO:  How to combine these blocks?
         if(proposedName == gServerGame->getClientInfo(i)->getName().getString())     // Collision detected!
         {
            unique = false;

            char numstr[10];
            sprintf(numstr, ".%d", index);

            // Max length name can be such that when number is appended, it's still less than MAX_PLAYER_NAME_LENGTH
            S32 maxNamePos = MAX_PLAYER_NAME_LENGTH - (S32)strlen(numstr); 
            name = name.substr(0, maxNamePos);     // Make sure name won't grow too long
            proposedName = name + numstr;

            index++;
            break;
         }
      }

      for(S32 i = 0; i < Robot::robots.size(); i++)
      {
         if(proposedName == Robot::robots[i]->getName().getString())
         {
            unique = false;

            char numstr[10];
            sprintf(numstr, ".%d", index);

            // Max length name can be such that when number is appended, it's still less than MAX_PLAYER_NAME_LENGTH
            S32 maxNamePos = MAX_PLAYER_NAME_LENGTH - (S32)strlen(numstr);   
            name = name.substr(0, maxNamePos);                      // Make sure name won't grow too long
            proposedName = name + numstr;

            index++;
            break;
         }
      }
   }

   return proposedName;
}


// Runs on client and server?
void GameConnection::onConnectionEstablished()
{
   U32 minPacketSendPeriod = 40; //50;   <== original zap settings
   U32 minPacketRecvPeriod = 40; //50;
   U32 maxSendBandwidth = 65535; //2000;
   U32 maxRecvBandwidth = 65535; //2000;

   Address addr = this->getNetAddress();
   if(this->isLocalConnection())    // Local connections don't use network, maximum bandwidth
   {
      minPacketSendPeriod = 15;
      minPacketRecvPeriod = 15;
      maxSendBandwidth = 65535;     // Error when higher than 65535
      maxRecvBandwidth = 65535;
   }
   

   Parent::onConnectionEstablished();

   if(isInitiator())    // Runs on client
   {
#ifndef ZAP_DEDICATED
      mClientGame->setInCommanderMap(false);       // Start game in regular mode
      mClientGame->clearZoomDelta();               // No in zoom effect
      setGhostFrom(false);
      setGhostTo(true);
      logprintf(LogConsumer::LogConnection, "%s - connected to server.", getNetAddressString());

      setFixedRateParameters(minPacketSendPeriod, minPacketRecvPeriod, maxSendBandwidth, maxRecvBandwidth);       

      // If we entered a password, and it worked, let's save it for next time.  If we arrive here and the saved password is empty
      // it means that the user entered a good password.  So we save.
      bool isLocal = gServerGame;
      
      string lastServerName = mClientGame->getRequestedServerName();

      if(!isLocal && gINI.GetValue("SavedServerPasswords", lastServerName) == "")
         gINI.SetValue("SavedServerPasswords", lastServerName, mClientGame->getServerPassword(), true);


      if(!isLocalConnection())    // Might use /connect, want to add to list after successfully connected. Does nothing while connected to master.
      {         
         string addr = getNetAddressString();
         bool found = false;

         for(S32 i = 0; i < mSettings->getIniSettings()->prevServerListFromMaster.size(); i++)
            if(mSettings->getIniSettings()->prevServerListFromMaster[i].compare(addr) == 0) 
            {
               found = true;
               break;
            }

         if(!found) 
            mSettings->getIniSettings()->prevServerListFromMaster.push_back(addr);
      }
#endif
   }
   else                 // Runs on server
   {
      gServerGame->addClient(mClientInfo);
      setGhostFrom(true);
      setGhostTo(false);
      activateGhosting();
      setFixedRateParameters(minPacketSendPeriod, minPacketRecvPeriod, maxSendBandwidth, maxRecvBandwidth);        

      // Ideally, the server name would be part of the connection handshake, but this will work as well
      s2cSetServerName(gServerGame->getSettings()->getHostName());   // Note: mSettings is NULL here

      time(&joinTime);
      mAcheivedConnection = true;
      
      // Notify the bots that a new player has joined
      Robot::getEventManager().fireEvent(NULL, EventManager::PlayerJoinedEvent, getPlayerInfo());

      if(gServerGame->getSettings()->getLevelChangePassword() == "")   // Grant level change permissions if level change PW is blank
      {
         mClientInfo->setIsLevelChanger(true);
         s2cSetIsLevelChanger(true, false);          // Tell client, but don't display notification
         sendLevelList();
      }

      const char *name =  mClientInfo->getName().getString();

      logprintf(LogConsumer::LogConnection, "%s - client \"%s\" connected.", getNetAddressString(), name);
      logprintf(LogConsumer::ServerFilter,  "%s [%s] joined [%s]", name, 
                                             isLocalConnection() ? "Local Connection" : getNetAddressString(), getTimeStamp().c_str());

      GameType *gt = gServerGame->getGameType();

      if(gServerGame->getSettings()->getIniSettings()->allowMapUpload)
         s2rSendableFlags(1);
   }
}


// Established connection is terminated.  Compare to onConnectTerminate() below.
void GameConnection::onConnectionTerminated(NetConnection::TerminationReason reason, const char *reasonStr)
{
   if(isInitiator())    // i.e. this is a client that connected to the server
   {
#ifndef ZAP_DEDICATED
      TNLAssert(mClientGame, "onConnectionTerminated: mClientGame is NULL");

      if(mClientGame)
         mClientGame->onConnectionTerminated(getNetAddress(), reason, reasonStr);
#endif
   }
   else     // Server
   {
      getPlayerInfo()->setDefunct();
      Robot::getEventManager().fireEvent(NULL, EventManager::PlayerLeftEvent, getPlayerInfo());

      gServerGame->removeClient(mClientInfo.get());
   }
}


// This function only gets called while the player is trying to connect to a server.  Connection has not yet been established.
// Compare to onConnectIONTerminated()
void GameConnection::onConnectTerminated(TerminationReason reason, const char *notUsed)
{
   if(isInitiator())
   {
#ifndef ZAP_DEDICATED
      TNLAssert(mClientGame, "onConnectTerminated: mClientGame is NULL");
      if(!mClientGame)
         return;

      mClientGame->onConnectTerminated(getNetAddress(), reason);
#endif

   }
}


// Return a measure of a player's strength.  Right now this is rather bogus -- any improvements welcomed!!!
// Should return a number between -1 and 1
// Better: https://secure.wikimedia.org/wikipedia/en/wiki/Elo_rating_system
F32 GameConnection::getCumulativeRating()
{
   // Initial case: no one has scored
   if(mTotalCumulativeScore == 0)      
      return .5;

   // Standard case: 
   else   
      return F32(mCumulativeScore) / F32(mTotalCumulativeScore);
}


void GameConnection::endOfGameScoringHandler()
{
   mGamesPlayed++;
   //mScore = 0; 
   //mRating = 0; 
   //mTotalScore = 0;
   mStatistics.resetStatistics();
}


// Gets run when game is really and truly over, after post-game scoreboard is displayed.  Over.
void GameConnection::onEndGhosting()
{
   TNLAssert(isConnectionToServer() && mClientGame, "when else is this called?");
   
   Parent::onEndGhosting();
   mClientGame->clearClientList();
}

};


