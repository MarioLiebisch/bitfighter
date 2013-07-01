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

#ifndef _GAMETYPE_H_
#define _GAMETYPE_H_


#include "flagItem.h"
#include "gameStats.h"           // For VersionedGameStats
#include "barrier.h"             // For WallRec def

#include "game.h"                // For MaxTeams
#include "gameConnection.h"      // For MessageColors enum
#include "GameTypesEnum.h"
#include "DismountModesEnum.h"

#include "Timer.h"

#include <string>
#include <boost/shared_ptr.hpp>

#include <map>


namespace Zap
{

// Some forward declarations
class GoalZone;
class CoreItem;
class MenuItem;
class MoveItem;
class ClientGame;
class Robot;
class AsteroidSpawn;
class Team;
class SpyBug;
class MenuUserInterface;
class Zone;


////////////////////////////////////////
////////////////////////////////////////

class GameTimer
{
private:
   Timer mTimer;
   bool mIsUnlimited;
   bool mGameOver;
   S32 mRenderingOffset;

public:
   void reset(U32 timeInMs);
   void sync(U32 deltaT);
   void extend(S32 deltaT);

   void setIsUnlimited(bool isUnlimited);
   void setRenderingOffset(S32 offset);
   void setGameIsOver();
   void setTimeRemaining(U32 timeLeft, bool isUnlimited);

   bool update(U32 deltaT);
   bool isUnlimited() const;

   U32 getCurrent() const; 
   U32 getTotalGameTime() const;
   S32 getRenderingOffset() const;


   string toString_minutes() const;      // Creates string representation of timer for level saving purposes
};


////////////////////////////////////////
////////////////////////////////////////


class GameType : public NetObject
{
   typedef NetObject Parent;

   static const S32 TeamNotSpecified = -99999;

private:
   Game *mGame;

   Point getSpawnPoint(S32 team);        // Pick a spawn point for ship or robot

   bool mLevelHasLoadoutZone;
   bool mLevelHasPredeployedFlags;
   bool mLevelHasFlagSpawns;

   bool mShowAllBots;
   U32 mTotalGamePlay;

   Vector<WallRec> mWalls;

   // In-game chat message:
   void sendChat(const StringTableEntry &senderName, ClientInfo *senderClientInfo, const StringPtr &message, bool global);

   S32 mWinningScore;               // Game over when team (or player in individual games) gets this score
   S32 mLeadingTeam;                // Team with highest score
   S32 mLeadingTeamScore;           // Score of mLeadingTeam
   S32 mLeadingPlayer;              // Player index of mClientInfos with highest score
   S32 mLeadingPlayerScore;         // Score of mLeadingPlayer
   S32 mSecondLeadingPlayer;        // Player index of mClientInfos with highest score
   S32 mSecondLeadingPlayerScore;   // Score of mLeadingPlayer

   bool mCanSwitchTeams;            // Player can switch teams when this is true, not when it is false
   bool mBetweenLevels;             // We'll need to prohibit certain things (like team changes) when game is in an "intermediate" state
   bool mGameOver;                  // Set to true when an end condition is met

   bool mEngineerEnabled;
   bool mEngineerUnrestrictedEnabled;
   bool mBotsAllowed;
   bool mBotBalancingDisabled;

   static const U32 mBotBalanceTimerPeriod;

   // Info about current level
   StringTableEntry mLevelName;
   StringTableEntry mLevelDescription;
   StringTableEntry mLevelCredits;

   string mScriptName;                 // Name of levelgen script, if any
   Vector<string> mScriptArgs;         // List of script params  

   S32 mMinRecPlayers;         // Recommended min players for this level
   S32 mMaxRecPlayers;         // Recommended max players for this level

   Vector<SafePtr<MoveItem> > mCacheResendItem;  // Speed up c2sResendItemStatus

   void idle_client(U32 deltaT);
   void idle_server(U32 deltaT);

   S32 findLargestTeamWithBots() const;

protected:
   Timer mScoreboardUpdateTimer;

   GameTimer mGameTimer;               // Track when current game will end
   Timer mGameTimeUpdateTimer;         // Timer for when to send clients a game clock update
   Timer mBotBalanceAnalysisTimer;     // Analyze if we need to add/remove bots to balance team
                       
   virtual void syncTimeRemaining(U32 timeLeft);
   virtual void setTimeRemaining(U32 timeLeft, bool isUnlimited);                         // Runs on server
   virtual void setTimeRemaining(U32 timeLeft, bool isUnlimited, S32 renderingOffset);    // Runs on client

   void notifyClientsWhoHasTheFlag();           // Notify the clients when flag status changes... only called by some game types (server only)
   bool doTeamHasFlag(S32 teamIndex) const;     // Do the actual work of figuring out if the specified team has the flag  (server only)
   void updateWhichTeamsHaveFlags();         


public:
   static const S32 MAX_GAME_TIME = S32_MAX;

   static const S32 gFirstTeamNumber = -2;                              // First team is "Hostile to All" with index -2
   static const U32 gMaxTeamCount = Game::MAX_TEAMS - gFirstTeamNumber; // Number of possible teams, including Neutral and Hostile to All
   static const char *validateGameType(const char *gtype);              // Returns a valid gameType, defaulting to base class if needed

   Game *getGame() const;
   bool onGhostAdd(GhostConnection *theConnection);
   void onGhostRemove();

   void broadcastTimeSyncSignal();                     // Send remaining time to all clients
   void broadcastNewRemainingTime();                   // Send remaining time to all clients after time has been updated

   const char *getGameTypeName() const;   

   virtual GameTypeId getGameTypeId() const;
   virtual const char *getShortName() const;          // Will be overridden by other games
   virtual const char **getInstructionString() const; //          -- ditto --
   virtual HelpItem getGameStartInlineHelpItem() const;
   virtual bool isTeamGame() const;                   // Team game if we have teams.  Otherwise it's every man for himself.
   virtual bool canBeTeamGame() const;
   virtual bool canBeIndividualGame() const;
   virtual bool teamHasFlag(S32 teamIndex) const;

   virtual void onFlagMounted(S32 teamIndex);         // A flag was picked up by a ship on the specified team

   S32 getWinningScore() const;
   void setWinningScore(S32 score);

   Vector<AbstractSpawn *> getSpawnPoints(TypeNumber typeNumber, S32 teamIndex = TeamNotSpecified);


   // Info about the level itself
   bool hasFlagSpawns() const;      
   bool hasPredeployedFlags() const;

   void setGameTime(F32 timeInSeconds);

   U32 getTotalGameTime() const;            // In seconds
   S32 getRemainingGameTime() const;        // In seconds
   S32 getRemainingGameTimeInMs() const;    // In ms
   S32 getRenderingOffset() const;
   bool isTimeUnlimited() const;
   void extendGameTime(S32 timeInMs);

   S32 getLeadingScore() const;
   S32 getLeadingTeam() const;
   S32 getLeadingPlayerScore() const;
   S32 getLeadingPlayer() const;
   S32 getSecondLeadingPlayerScore() const;
   S32 getSecondLeadingPlayer() const;

   void addWall(const WallRec &barrier, Game *game);

   virtual bool isFlagGame() const; // Does game use flags?
   virtual S32 getFlagCount();      // Return the number of game-significant flags

   virtual bool isCarryingItems(Ship *ship); // Nexus game will override this

   virtual bool isSpawnWithLoadoutGame();    // We do not spawn with our loadout, but instead need to pass through a loadout zone

   F32 getUpdatePriority(NetObject *scopeObject, U32 updateMask, S32 updateSkips);

   static void printRules();             // Dump game-rule info

   bool levelHasLoadoutZone();           // Does the level have a loadout zone?

   bool advanceGameClock(U32 deltaT);

   enum
   {
      RespawnDelay = 1500,
      SwitchTeamsDelay = 60000,   // Time between team switches (ms) -->  60000 = 1 minute
      naScore = -99999,           // Score representing a nonsesical event
      NO_FLAG = -1,               // Constant used for ship not having a flag
   };


   const Vector<WallRec> *getBarrierList();

   S32 mObjectsExpected;            // Count of objects we expect to get with this level (for display purposes only)

   struct ItemOfInterest
   {
      SafePtr<MoveItem> theItem;
      U32 teamVisMask;        // Bitmask, where 1 = object is visible to team in that position, 0 if not
   };

   Vector<ItemOfInterest> mItemsOfInterest;

   void addItemOfInterest(MoveItem *theItem);

   void broadcastMessage(GameConnection::MessageColors color, SFXProfiles sfx, const StringTableEntry &formatString);

   void broadcastMessage(GameConnection::MessageColors color, SFXProfiles sfx, 
                         const StringTableEntry &formatString, const Vector<StringTableEntry> &e);

   bool isGameOver() const;

   static const char *getGameTypeName(GameTypeId gameType);       // Return string like "Capture The Flag"
   static const char *getGameTypeClassName(GameTypeId gameType);  // Return string like "CTFGameType"

   static Vector<string> getGameTypeNames();

   bool mHaveSoccer;                // Does level have soccer balls? used to determine weather or not to send s2cSoccerCollide

   bool mBotZoneCreationFailed;

   enum
   {
      MaxPing = 999,
      DefaultGameTime = 10 * 60 * 1000,
      DefaultWinningScore = 8,
   };

   // Some games have extra game parameters.  We need to create a structure to communicate those parameters to the editor so
   // it can make an intelligent decision about how to handle them.  Note that, for now, all such parameters are assumed to be S32.
   struct ParameterDescription
   {
      const char *name;
      const char *units;
      const char *help;
      S32 value;     // Default value for this parameter
      S32 minval;    // Min value for this param
      S32 maxval;    // Max value for this param
   };

   enum ScoringGroup
   {
      IndividualScore,
      TeamScore,
   };

   virtual S32 getEventScore(ScoringGroup scoreGroup, ScoringEvent scoreEvent, S32 data);
   static string getScoringEventDescr(ScoringEvent event);

   // Static vectors used for constructing update RPCs
   static Vector<RangedU32<0, MaxPing> > mPingTimes;
   static Vector<SignedInt<24> > mScores;
   static Vector<SignedFloat<8> > mRatings;

   explicit GameType(S32 winningScore = DefaultWinningScore);    // Constructor
   virtual ~GameType();                                 // Destructor

   virtual void addToGame(Game *game, GridDatabase *database);

   virtual bool processArguments(S32 argc, const char **argv, Game *game);
   virtual string toLevelCode() const;

#ifndef ZAP_DEDICATED
   virtual Vector<string> getGameParameterMenuKeys();
   virtual boost::shared_ptr<MenuItem> getMenuItem(const string &key);
   virtual bool saveMenuItem(const MenuItem *menuItem, const string &key);
#endif

   virtual bool processSpecialsParam(const char *param);
   virtual string getSpecialsLine();


   const StringTableEntry *getLevelName() const;
   void setLevelName(const StringTableEntry &levelName);

   const StringTableEntry *getLevelDescription() const;
   void setLevelDescription(const StringTableEntry &levelDescription);

   const StringTableEntry *getLevelCredits() const;
   void setLevelCredits(const StringTableEntry &levelCredits);

   S32 getMinRecPlayers();
   void setMinRecPlayers(S32 minPlayers);

   S32 getMaxRecPlayers();
   void setMaxRecPlayers(S32 maxPlayers);

   bool isEngineerEnabled();
   void setEngineerEnabled(bool enabled);
   bool isEngineerUnrestrictedEnabled();
   void setEngineerUnrestrictedEnabled(bool enabled);

   bool areBotsAllowed();
   void setBotsAllowed(bool allowed);
   
   string getScriptLine() const;
   void setScript(const Vector<string> &args);

   string getScriptName() const;
   const Vector<string> *getScriptArgs();

   void onAddedToGame(Game *theGame);

   void onLevelLoaded();      // Server-side function run once level is loaded from file

   virtual void idle(BfObject::IdleCallPath path, U32 deltaT);

   void gameOverManGameOver();
   VersionedGameStats getGameStats();
   void getSortedPlayerScores(S32 teamIndex, Vector<ClientInfo *> &playerScores) const;
   void saveGameStats();                     // Transmit statistics to the master server

   void achievementAchieved(U8 achievement, const StringTableEntry &playerName);

   virtual void onGameOver();

   void serverAddClient(ClientInfo *clientInfo);         

   void serverRemoveClient(ClientInfo *clientInfo);   // Remove a client from the game

   virtual bool objectCanDamageObject(BfObject *damager, BfObject *victim);
   virtual void controlObjectForClientKilled(ClientInfo *theClient, BfObject *clientObject, BfObject *killerObject);

   virtual bool spawnShip(ClientInfo *clientInfo);
   virtual void spawnRobot(Robot *robot);

   virtual void changeClientTeam(ClientInfo *client, S32 team);     // Change player to team indicated, -1 = cycle teams

#ifndef ZAP_DEDICATED
   virtual void renderInterfaceOverlay(S32 canvasWidth, S32 canvasHeight) const;
   virtual void renderScoreboardOrnament(S32 teamIndex, S32 xpos, S32 ypos) const;
   virtual S32 renderTimeLeftSpecial(S32 right, S32 bottom, bool render) const;

   void renderObjectiveArrow(const BfObject *target, S32 canvasWidth, S32 canvasHeigh) const;
   void renderObjectiveArrow(const BfObject *target, const Color *c, S32 canvasWidth, S32 canvasHeight, F32 alphaMod = 1.0f) const;
   void renderObjectiveArrow(const Point &p, const Color *c, S32 canvasWidth, S32 canvasHeight, F32 alphaMod = 1.0f) const;
#endif


   void addTime(U32 time);          // Extend the game by time (in ms)

   void clientRequestLoadout(ClientInfo *clientInfo, const LoadoutTracker &loadout);
   void updateShipLoadout(BfObject *shipObject); // called from LoadoutZone when a Ship touches the zone

   bool makeSureTeamCountIsNotZero();                 // Zero teams can cause crashiness

   virtual const Color *getTeamColor(const BfObject *object) const; // Get the color of a team, based on object
           const Color *getTeamColor(S32 team)               const; // Get the color of a team, based on index


   //S32 getTeam(const StringTableEntry &playerName);   // Given a player's name, return their team

   virtual bool isDatabasable();                      // Makes no sense to insert a GameType in our spatial database!

   // gameType flag methods for CTF, Rabbit, Football
   virtual void addFlag(FlagItem *flag);
   virtual void itemDropped(Ship *ship, MoveItem *item, DismountMode dismountMode);    // TODO: Make this a mountableItem instead of MoveItem
   virtual void shipTouchFlag(Ship *ship, FlagItem *flag);

   virtual void releaseFlag(const Point &pos, const Point &vel = Point(0,0), S32 count = 1);

   virtual void shipTouchZone(Ship *ship, GoalZone *zone);

   void queryItemsOfInterest();
   void performScopeQuery(GhostConnection *connection);
   virtual void performProxyScopeQuery(BfObject *scopeObject, ClientInfo *clientInfo);

   virtual void onGhostAvailable(GhostConnection *theConnection);
   TNL_DECLARE_RPC(s2cSetLevelInfo, (StringTableEntry levelName, StringTableEntry levelDesc, S32 teamScoreLimit, StringTableEntry levelCreds, 
                                     S32 objectCount, F32 lx, F32 ly, F32 ux, F32 uy, bool levelHasLoadoutZone, bool engineerEnabled, bool engineerAbuseEnabled, U32 levelDatabaseId));
   TNL_DECLARE_RPC(s2cAddWalls, (Vector<F32> barrier, F32 width, bool solid));
   TNL_DECLARE_RPC(s2cAddTeam, (StringTableEntry teamName, F32 r, F32 g, F32 b, U32 score, bool firstTeam));
   TNL_DECLARE_RPC(s2cAddClient, (StringTableEntry clientName, bool isAuthenticated, Int<BADGE_COUNT> badges, 
                                  bool isMyClient, RangedU32<0, ClientInfo::MaxRoles> role, bool isRobot, bool isSpawnDelayed, bool isBusy, bool playAlert, bool showMessage));
   TNL_DECLARE_RPC(s2cClientJoinedTeam, (StringTableEntry clientName, RangedU32<0, Game::MAX_TEAMS> teamIndex, bool showMessage));

   TNL_DECLARE_RPC(s2cClientChangedRoles, (StringTableEntry clientName, RangedU32<0, ClientInfo::MaxRoles> role));

   TNL_DECLARE_RPC(s2cSyncMessagesComplete, (U32 sequence));
   TNL_DECLARE_RPC(c2sSyncMessagesComplete, (U32 sequence));

   TNL_DECLARE_RPC(s2cSetGameOver, (bool gameOver));
   TNL_DECLARE_RPC(s2cSyncTimeRemaining, (U32 timeLeftInMs));
   TNL_DECLARE_RPC(s2cSetNewTimeRemaining, (U32 timeLeftInMs, bool isUnlimited, S32 renderingOffset));
   TNL_DECLARE_RPC(s2cChangeScoreToWin, (U32 score, StringTableEntry changer));

   TNL_DECLARE_RPC(s2cSendFlagPossessionStatus, (U16 packedBits));

   TNL_DECLARE_RPC(s2cCanSwitchTeams, (bool allowed));

   TNL_DECLARE_RPC(s2cRenameClient, (StringTableEntry oldName,StringTableEntry newName));
   void updateClientChangedName(ClientInfo *clientInfo, StringTableEntry newName);

   TNL_DECLARE_RPC(s2cRemoveClient, (StringTableEntry clientName));

   TNL_DECLARE_RPC(s2cAchievementMessage, (U32 achievement, StringTableEntry clientName));

   // Not all of these actually used?
   void updateScore(Ship *ship, ScoringEvent event, S32 data = 0);              
   void updateScore(ClientInfo *clientInfo, ScoringEvent scoringEvent, S32 data = 0); 
   void updateScore(S32 team, ScoringEvent event, S32 data = 0);
   virtual void updateScore(ClientInfo *player, S32 team, ScoringEvent event, S32 data = 0); // Core uses their own updateScore

   void updateLeadingTeamAndScore();   // Sets mLeadingTeamScore and mLeadingTeam
   void updateLeadingPlayerAndScore(); // Sets mLeadingTeamScore and mLeadingTeam
   void updateRatings();               // Update everyone's game-normalized ratings at the end of the game


   TNL_DECLARE_RPC(s2cSetTeamScore, (RangedU32<0, Game::MAX_TEAMS> teamIndex, U32 score));
   TNL_DECLARE_RPC(s2cSetPlayerScore, (U16 index, S32 score));

   TNL_DECLARE_RPC(c2sRequestScoreboardUpdates, (bool updates));
   TNL_DECLARE_RPC(s2cScoreboardUpdate, (Vector<RangedU32<0, MaxPing> > pingTimes, Vector<SignedFloat<8> > ratings));

   void updateClientScoreboard(ClientInfo *clientInfo);

   TNL_DECLARE_RPC(c2sChooseNextWeapon, ());
   TNL_DECLARE_RPC(c2sChoosePrevWeapon, ());
   TNL_DECLARE_RPC(c2sSelectWeapon, (RangedU32<0, ShipWeaponCount> index));
   TNL_DECLARE_RPC(c2sDropItem, ());

   // These are used when the client sees something happen and wants a confirmation from the server
   TNL_DECLARE_RPC(c2sResendItemStatus, (U16 itemId));

#ifndef ZAP_DEDICATED
   // Handle additional game-specific menu options for the client and the admin
   virtual void addClientGameMenuOptions(ClientGame *game, MenuUserInterface *menu);
   //virtual void processClientGameMenuOption(U32 index);                        // Param used only to hold team, at the moment

   virtual void addAdminGameMenuOptions(MenuUserInterface *menu);
#endif

   void sendChatFromRobot(bool global, const StringPtr &message, ClientInfo *botClientInfo);
   void sendGlobalChatFromController(const StringPtr &message);
   void sendTeamChatFromController(const StringPtr &message, S32 teamIndex);
   void sendPrivateChatFromController(const StringPtr &message, const StringPtr &playerName);
   void sendAnnouncementFromController(const StringPtr &message);

   TNL_DECLARE_RPC(c2sAddTime, (U32 time));                                    // Admin is adding time to the game
   TNL_DECLARE_RPC(c2sChangeTeams, (S32 team));                                // Player wants to change teams
   void processClientRequestForChangingGameTime(S32 time, bool isUnlimited, bool changeTimeIfAlreadyUnlimited, bool addTime);

   TNL_DECLARE_RPC(c2sSendAnnouncement,(string message));
   TNL_DECLARE_RPC(s2cDisplayAnnouncement,(string message));

   TNL_DECLARE_RPC(c2sSendChatPM, (StringTableEntry toName, StringPtr message));                        // using /pm command
   TNL_DECLARE_RPC(c2sSendChat, (bool global, StringPtr message));             // In-game chat
   TNL_DECLARE_RPC(c2sSendChatSTE, (bool global, StringTableEntry ste));       // Quick-chat
   TNL_DECLARE_RPC(c2sSendCommand, (StringTableEntry cmd, Vector<StringPtr> args));

   TNL_DECLARE_RPC(s2cDisplayChatPM, (StringTableEntry clientName, StringTableEntry toName, StringPtr message));
   TNL_DECLARE_RPC(s2cDisplayChatMessage, (bool global, StringTableEntry clientName, StringPtr message));

   // killerName will be ignored if killer is supplied
   TNL_DECLARE_RPC(s2cKillMessage, (StringTableEntry victim, StringTableEntry killer, StringTableEntry killerName));

   TNL_DECLARE_RPC(c2sVoiceChat, (bool echo, ByteBufferPtr compressedVoice));
   TNL_DECLARE_RPC(s2cVoiceChat, (StringTableEntry client, ByteBufferPtr compressedVoice));

   TNL_DECLARE_RPC(c2sSetTime, (U32 time));
   TNL_DECLARE_RPC(c2sSetWinningScore, (U32 score));
   TNL_DECLARE_RPC(c2sResetScore, ());
   TNL_DECLARE_RPC(c2sAddBot, (Vector<StringTableEntry> args));
   TNL_DECLARE_RPC(c2sAddBots, (U32 count, Vector<StringTableEntry> args));
   TNL_DECLARE_RPC(c2sKickBot, ());
   TNL_DECLARE_RPC(c2sKickBots, ());
   TNL_DECLARE_RPC(c2sShowBots, ());
   TNL_DECLARE_RPC(c2sSetMaxBots, (S32 count));
   TNL_DECLARE_RPC(c2sBanPlayer, (StringTableEntry playerName, U32 duration));
   TNL_DECLARE_RPC(c2sBanIp, (StringTableEntry ipAddressString, U32 duration));
   TNL_DECLARE_RPC(c2sRenamePlayer, (StringTableEntry playerName, StringTableEntry newName));
   TNL_DECLARE_RPC(c2sGlobalMutePlayer, (StringTableEntry playerName));
   TNL_DECLARE_RPC(c2sClearScriptCache, ());
   TNL_DECLARE_RPC(c2sTriggerTeamChange, (StringTableEntry playerName, S32 teamIndex));
   TNL_DECLARE_RPC(c2sKickPlayer, (StringTableEntry playerName));

   TNL_DECLARE_RPC(s2cSetIsSpawnDelayed, (StringTableEntry name, bool idle));
   TNL_DECLARE_RPC(s2cSetPlayerEngineeringTeleporter, (StringTableEntry name, bool isEngineeringTeleporter));

   TNL_DECLARE_CLASS(GameType);

   enum
   {
      mZoneGlowTime = 800,    // Time for visual effect, used by Nexus & GoalZone
   };

   Timer mZoneGlowTimer;
   S32 mGlowingZoneTeam;      // Which team's zones are glowing, -1 for all

   virtual void majorScoringEventOcurred(S32 team);    // Gets called when touchdown is scored...  currently only used by zone control & retrieve

   void processServerCommand(ClientInfo *clientInfo, const char *cmd, Vector<StringPtr> args);
   void addBotFromClient(Vector<StringTableEntry> args);

   string addBot(Vector<StringTableEntry> args);
   S32 getMaxPlayersPerBalancedTeam(S32 minPlayersNeeded, S32 teamCount);
   void balanceTeams();

   map <pair<U16,U16>, Vector<Point> > cachedBotFlightPlans;  // cache of zone-to-zone flight plans, shared for all bots

   GameTimer *getTimer();
};

#define GAMETYPE_RPC_S2C(className, methodName, args, argNames) \
   TNL_IMPLEMENT_NETOBJECT_RPC(className, methodName, args, argNames, NetClassGroupGameMask, RPCGuaranteedOrdered, RPCToGhost, 0)

#define GAMETYPE_RPC_C2S(className, methodName, args, argNames) \
   TNL_IMPLEMENT_NETOBJECT_RPC(className, methodName, args, argNames, NetClassGroupGameMask, RPCGuaranteedOrdered, RPCToGhostParent, 0)

};

#endif
