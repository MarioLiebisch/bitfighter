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

#ifndef _CLIENTGAME_H_
#define _CLIENTGAME_H_

#ifdef ZAP_DEDICATED
#error "ClientGame.h shouldn't be included in dedicated build"
#endif

#include "game.h"

#include "gameConnection.h"
#include "dataConnection.h"      // For DataSender

#ifdef TNL_OS_WIN32
#include <windows.h>             // For screensaver... windows only feature, I'm afraid!
#endif

#include "sparkManager.h"

using namespace std;

namespace Zap
{


class ClientGame : public Game, public FXManager
{
   typedef Game Parent;

private:
   enum {
      NumStars = 256,               // 256 stars should be enough for anybody!   -- Bill Gates
      CommanderMapZoomTime = 350,   // Transition time between regular map and commander's map; in ms, higher = slower
   };

   Point mStars[NumStars];

   SafePtr<GameConnection> mConnectionToServer; // If this is a client game, this is the connection to the server
   bool mInCommanderMap;

   U32 mCommanderZoomDelta;

   Timer mScreenSaverTimer;
   void supressScreensaver();

   UIManager *mUIManager;

   string mRemoteLevelDownloadFilename;

   bool mDebugShowShipCoords;       // Show coords on ship?
   bool mDebugShowMeshZones;        // Show bot nav mesh zones?

   Vector<string> mMuteList;        // List of players we aren't listening to anymore because they've annoyed us!
   Vector<string> mVoiceMuteList;   // List of players we mute because they are abusing voice chat

   string mLoginPassword;

   // ClientGame has two ClientInfos for the local player; mClientInfo is a FullClientInfo, which contains a rich array of information
   // about the local player.  When a player connects to a server, all players (including the connecting player) are sent a much briefer
   // set of info about the player, which is stored in a RemoteClientInfo.  So when a player connects, they get several RemoteClientInfos,
   // one of which describes themselves.  It is occasionally useful to have a handle on that, which is what mLocalRemoteClientInfo provides.
   ClientInfo *mClientInfo;               // ClientInfo object for local client
   ClientInfo *mLocalRemoteClientInfo;    // ClientInfo pointer to the remote ClientInfo received from the server

   S32 findClientIndex(const StringTableEntry &name);

   AbstractTeam *getNewTeam();

   bool mSpawnDelayed;

public:
   ClientGame(const Address &bindAddress, GameSettings *settings);
   virtual ~ClientGame();

   void joinGame(Address remoteAddress, bool isFromMaster, bool local);
   void closeConnectionToGameServer();

   UserInterfaceData *mUserInterfaceData;

   bool hasValidControlObject();
   bool isConnectedToServer();

   GameConnection *getConnectionToServer();
   void setConnectionToServer(GameConnection *connection);

   ClientInfo *getClientInfo();

   ClientInfo *getLocalRemoteClientInfo();

   string getLoginPassword() const;
   void setLoginPassword(const string &loginPassword);

   void correctPlayerName(const string &name);                                      // When server corrects capitalization of name or similar
   void updatePlayerNameAndPassword(const string &name, const string &password);    // When user enters new name and password on NameEntryUI

   void displayShipDesignChangedMessage(const Vector<U32> &loadout, const char *msgToShowIfLoadoutsAreTheSame);

   UIManager *getUIManager() const;

   UIMode getUIMode();

   bool getInCommanderMap();
   void setInCommanderMap(bool inCommanderMap);
   F32 getCommanderZoomFraction() const;

   Point worldToScreenPoint(const Point *p) const;
   void drawStars(F32 alphaFrac, Point cameraPos, Point visibleExtent);

   void render();             // Delegates to renderNormal, renderCommander, or renderSuspended, as appropriate
   void renderNormal();       // Render game in normal play mode
   void renderCommander();    // Render game in commander's map mode
   void renderSuspended();    // Render suspended game

   void renderOverlayMap();   // Render the overlay map in normal play mode
   void resetZoomDelta();
   void clearZoomDelta();
   bool isServer();
   void idle(U32 timeDelta);
   void zoomCommanderMap();

   bool isShowingDebugShipCoords() const;     // Show coords on ship?
   void toggleShowingShipCoords();

   bool isShowingDebugMeshZones()  const;     // Show bot nav mesh zones?
   void toggleShowingMeshZones();

   void gotServerListFromMaster(const Vector<IPAddress> &serverList);
   void gotChatMessage(const char *playerNick, const char *message, bool isPrivate, bool isSystem);
   void setPlayersInGlobalChat(const Vector<StringTableEntry> &playerNicks);
   void playerJoinedGlobalChat(const StringTableEntry &playerNick);
   void playerLeftGlobalChat(const StringTableEntry &playerNick);

   void onPlayerJoined(ClientInfo *clientInfo, bool isLocalClient, bool playAlert);
   void onPlayerQuit(const StringTableEntry &name);

   void setSpawnDelayed(bool spawnDelayed);
   bool isSpawnDelayed();
   void undelaySpawn();

   // Check for permissions
   bool hasAdmin(const char *failureMessage);
   bool hasLevelChange(const char *failureMessage);

   void enterMode(UIMode mode);


   void addToMuteList(const string &name);
   void removeFromMuteList(const string &name);
   bool isOnMuteList(const string &name);

   void addToVoiceMuteList(const string &name);
   void removeFromVoiceMuteList(const string &name);
   bool isOnVoiceMuteList(const string &name);


   void connectionToServerRejected(const char *reason);
   void setMOTD(const char *motd);
   void setNeedToUpgrade(bool needToUpgrade);

   string getRemoteLevelDownloadFilename() const;
   void setRemoteLevelDownloadFilename(const string &filename);

   void changePassword(GameConnection::ParamType type, const Vector<string> &words, bool required);
   void changeServerParam(GameConnection::ParamType type, const Vector<string> &words);
   bool checkName(const string &name);    // Make sure name is valid, and correct case of name if otherwise correct


   // Alert users when they get a reply to their request for elevated permissions
   void gotAdminPermissionsReply(bool granted);
   void gotLevelChangePermissionsReply(bool granted);
   void gotWrongPassword();

   void gotPingResponse(const Address &address, const Nonce &nonce, U32 clientIdentityToken);
   void gotQueryResponse(const Address &address, const Nonce &nonce, const char *serverName, const char *serverDescr, 
                         U32 playerCount, U32 maxPlayers, U32 botCount, bool dedicated, bool test, bool passwordRequired);

   void shutdownInitiated(U16 time, const StringTableEntry &name, const StringPtr &reason, bool originator);
   void cancelShutdown();

   void displayMessageBox(const StringTableEntry &title, const StringTableEntry &instr, const Vector<StringTableEntry> &message) const;
   void displayMessage(const Color &msgColor, const char *format, ...);

   void onConnectionTerminated(const Address &serverAddress, NetConnection::TerminationReason reason, const char *reasonStr);
   void onConnectionToMasterTerminated(NetConnection::TerminationReason reason, const char *reasonStr);

   void onConnectTerminated(const Address &serverAddress, NetConnection::TerminationReason reason, const char *reasonStr);
   void runCommand(const char *command);
   void setVolume(VolumeType volType, const Vector<string> &words);

   string getRequestedServerName();
   string getServerPassword();
   string getHashedServerPassword();

   void displayErrorMessage(const char *format, ...);
   void displaySuccessMessage(const char *format, ...);

   void suspendGame();
   void unsuspendGame();

   bool processPseudoItem(S32 argc, const char **argv, const string &levelFileName, GridDatabase *database);        // For loading levels in editor
};

////////////////////////////////////////
////////////////////////////////////////


extern ClientGame *gClientGame;

};


#endif

