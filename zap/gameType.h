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

#include "timer.h"
#include "gameObject.h"
#include "flagItem.h"
#include "teamInfo.h"
#include "UIMenuItems.h"   // For MenuItem defs needed in addGameSpecificParameterMenuItems() and overrides
#include "gameItems.h"     // For AsteroidSpawn
#include "robot.h"
#include "statistics.h"
#include <string>

namespace Zap
{

// Some forward declarations
class GoalZone;
class MenuItem;
class Item;
class SFXObject;
class VoiceDecoder;

class ClientRef : public Object
{
private:
   S32 mTeamId;
   S32 mScore;                      // Individual score for current game
   F32 mRating;                     // Skill rating from -1 to 1
   LuaPlayerInfo *mPlayerInfo;      // Lua access to this class

public:
   ClientRef();               // Constructor
   ~ClientRef();              // Destructor

   StringTableEntry name;     // Name of client - guaranteed to be unique of current clients

   S32 getTeam() { return mTeamId; }
   void setTeam(S32 teamId) { mTeamId = teamId; }

   S32 getScore() { return mScore; }
   void setScore(S32 score) { mScore = score; }
   void addScore(S32 score) { mScore += score; }

   F32 getRating() { return mRating; }
   void setRating(F32 rating) { mRating = rating; }

   Statistics mStatistics;        // Player statistics tracker

   LuaPlayerInfo *getPlayerInfo() { return mPlayerInfo; }

   bool isAdmin;
   bool isRobot;
   bool isLevelChanger;
   Timer respawnTimer;

   bool wantsScoreboardUpdates;  // Indicates whether the client has requested scoreboard streaming (e.g. pressing Tab key)
   bool readyForRegularGhosts;

   SafePtr<GameConnection> clientConnection;
   RefPtr<SFXObject> voiceSFX;
   RefPtr<VoiceDecoder> decoder;

   U32 ping;
};


////////////////////////////////////////
////////////////////////////////////////

class Robot;
class AsteroidSpawn;
class Team;

class GameType : public GameObject
{
private:
   Point getSpawnPoint(S32 team);         // Picks a spawn point for ship or robot
   virtual U32 getLowerRightCornerScoreboardOffsetFromBottom() { return 60; }      // Game-specific location for the bottom of the scoreboard on the lower-right corner
                                                                                   // (because games like hunters have more stuff down there we need to look out for)
   Vector<DatabaseObject *> mSpyBugs;    // List of all spybugs in the game
   bool mLevelHasLoadoutZone;
   bool mEngineerEnabled;

   void sendChatDisplayEvent(ClientRef *clientRef, bool global, const char *message, NetEvent *theEvent);      // In-game chat message

public:
   enum GameTypes
   {
      BitmatchGame,
      CTFGame,
      HTFGame,
      NexusGame,
      RabbitGame,
      RetrieveGame,
      SoccerGame,
      ZoneControlGame,
      GameTypesCount
   };

   // Potentially scoring events
   enum ScoringEvent {
      KillEnemy,              // all games
      KillSelf,               // all games
      KillTeammate,           // all games
      KillEnemyTurret,        // all games
      KillOwnTurret,          // all games

      KilledByAsteroid,       // all games
      KilledByTurret,         // all games

      CaptureFlag,
      CaptureZone,            // zone control -> gain zone
      UncaptureZone,          // zone control -> lose zone
      HoldFlagInZone,         // htf
      RemoveFlagFromEnemyZone,// htf
      RabbitHoldsFlag,        // rabbit, called every second
      RabbitKilled,           // rabbit
      RabbitKills,            // rabbit
      ReturnFlagsToNexus,     // hunters game
      ReturnFlagToZone,       // retrieve -> flag returned to zone
      LostFlag,               // retrieve -> enemy took flag
      ReturnTeamFlag,         // ctf -> holds enemy flag, touches own flag
      ScoreGoalEnemyTeam,     // soccer
      ScoreGoalHostileTeam,   // soccer
      ScoreGoalOwnTeam,       // soccer -> score on self
      ScoringEventsCount
   };

   static const S32 gMaxTeams = 9;                                   // Max teams allowed
   static const S32 gFirstTeamNumber = -2;                           // First team is "Hostile to All" with index -2
   static const U32 gMaxTeamCount = gMaxTeams - gFirstTeamNumber;    // Number of possible teams, including Neutral and Hostile to All
   static const char *validateGameType(const char *gtype);           // Returns a valid gameType, defaulting to gDefaultGameTypeIndex if needed

   virtual GameTypes getGameType() { return BitmatchGame; }
   virtual const char *getGameTypeString() { return "Bitmatch"; }                            // Will be overridden by other games
   virtual const char *getShortName() { return "BM"; }                                       //          -- ditto --
   virtual const char *getInstructionString() { return "Blast as many ships as you can!"; }  //          -- ditto --
   virtual bool isTeamGame() { return mTeams.size() > 1; }                                   // Team game if we have teams.  Otherwise it's every man for himself.
   virtual bool canBeTeamGame() { return true; }
   virtual bool canBeIndividualGame() { return true; }
   virtual bool getMountedObjectsMakesShipsVisible() { return true; }                        // True in all games except Nexus for now
   S32 getWinningScore() { return mWinningScore; }
   U32 getTotalGameTime() { return (mGameTimer.getPeriod() / 1000); }
   S32 getRemainingGameTime() { return (mGameTimer.getCurrent() / 1000); }
   S32 getLeadingScore() { return mLeadingTeamScore; }
   S32 getLeadingTeam() { return mLeadingTeam; }
   bool engineerIsEnabled() { return mEngineerEnabled; }

   void catalogSpybugs();     // Rebuild a list of spybugs in the game

   virtual bool isFlagGame() { return false; }              // Does game use flags?
   virtual bool isTeamFlagGame() { return false; }          // Does flag-team orientation matter?  Only true in CTF, really.
   virtual S32 getFlagCount() { return mFlags.size(); }     // Return the number of game-significant flags

   virtual bool isCarryingItems(Ship *ship) { return ship->mMountedItems.size() > 0; }     // Nexus game will override this

   // Some games may place restrictions on when players can fire or use modules
   virtual bool okToFire(Ship *ship) { return true; }
   virtual bool okToUseModules(Ship *ship) { return true; }

   virtual bool isSpawnWithLoadoutGame() { return false; }  // We do not spawn with our loadout, but instead need to pass through a loadout zone

   F32 getUpdatePriority(NetObject *scopeObject, U32 updateMask, S32 updateSkips);

   Vector<SafePtr<FlagItem> > mFlags;    // List of flags for those games that keep lists of flags (retireve, HTF, CTF)

   static void printRules();             // Dump game-rule info

   bool levelHasLoadoutZone() { return mLevelHasLoadoutZone; }        // Does the level have a loadout zone?

   enum
   {
      RespawnDelay = 1500,
      SwitchTeamsDelay = 60000,   // Time between team switches (ms) -->  60000 = 1 minute
      naScore = -99999,           // Score representing a nonsesical event
      NO_FLAG = -1,               // Constant used for ship not having a flag
   };

   struct BarrierRec
   {
      Vector<F32> verts;
      F32 width;
      bool solid;
   };

   Vector<BarrierRec> mBarriers;
   Vector<RefPtr<ClientRef> > mClientList;

   ClientRef *mLocalClient;

   virtual ClientRef *allocClientRef() { return new ClientRef; }

   Vector<Team> mTeams;                   // List of teams
   string mScriptName;                    // Name of script
   Vector<string> mScriptArgs;            // List of script params  
   Vector<FlagSpawn> mFlagSpawnPoints;    // List of non-team specific spawn points for flags
   Vector<AsteroidSpawn> mAsteroidSpawnPoints;    // List of spawn points for asteroids

   StringTableEntry mLevelName;
   StringTableEntry mLevelDescription;
   StringTableEntry mLevelCredits;
   Rect mViewBoundsWhileLoading;    // Show these view bounds while loading the map
   S32 mObjectsExpected;      // Count of objects we expect to get with this level (for display purposes only)
   S32 minRecPlayers;         // Recommended min players for this level
   S32 maxRecPlayers;         // Recommended max players for this level

   struct ItemOfInterest
   {
      SafePtr<Item> theItem;
      U32 teamVisMask;        // Bitmask, where 1 = object is visible to team in that position, 0 if not
   };

   Vector<ItemOfInterest> mItemsOfInterest;

   void addItemOfInterest(Item *theItem);

   Timer mScoreboardUpdateTimer;
   Timer mGameTimer;             // Track when current game will end
   Timer mGameTimeUpdateTimer;
   Timer mLevelInfoDisplayTimer;
   Timer mInputModeChangeAlertDisplayTimer;

   bool mCanSwitchTeams;         // Player can switch teams when this is true, not when it is false

   S32 mWinningScore;            // Game over when team (or player in individual games) gets this score
   S32 mLeadingTeam;             // Team with highest score
   S32 mLeadingTeamScore;        // Score of mLeadingTeam

   bool mBetweenLevels;          // We'll need to prohibit certain things (like team changes) when game is in an "intermediate" state
   bool mGameOver;               // Set to true when an end condition is met

   enum {
      MaxPing = 999,
      DefaultGameTime = 10 * 60 * 1000,
      DefaultWinningScore = 8,
      LevelInfoDisplayTime = 6000,
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

   enum ScoringGroup {
      IndividualScore,
      TeamScore,
   };

   virtual S32 getEventScore(ScoringGroup scoreGroup, ScoringEvent scoreEvent, S32 data);
   static string getScoringEventDescr(ScoringEvent event);

   // Static vectors used for constructing update RPCs
   static Vector<RangedU32<0, MaxPing> > mPingTimes;
   static Vector<SignedInt<24> > mScores;
   static Vector<RangedU32<0, 200> > mRatings;

   GameType();    // Constructor
   void countTeamPlayers();

   ClientRef *findClientRef(const StringTableEntry &name);

   bool processArguments(S32 argc, const char **argv);
   virtual void addGameSpecificParameterMenuItems(Vector<MenuItem *> &menuItems);

   void onAddedToGame(Game *theGame);

   void onLevelLoaded();      // Server-side function run once level is loaded from file

   void idle(GameObject::IdleCallPath path);

   void gameOverManGameOver();
   void saveGameStats();                     // Transmit statistics to the master server

   void checkForWinningScore(S32 score);     // Check if player or team has reachede the winning score
   virtual void onGameOver();

   virtual void serverAddClient(GameConnection *theClient);
   virtual void serverRemoveClient(GameConnection *theClient);

   virtual bool objectCanDamageObject(GameObject *damager, GameObject *victim);
   virtual void controlObjectForClientKilled(GameConnection *theClient, GameObject *clientObject, GameObject *killerObject);

   virtual void spawnShip(GameConnection *theClient);
   virtual void spawnRobot(Robot *robot);
   //Vector<Robot *> mRobotList;        // List of all robots in the game

   virtual void changeClientTeam(GameConnection *theClient, S32 team);     // Change player to team indicated, -1 = cycle teams

   virtual void renderInterfaceOverlay(bool scoreboardVisible);
   void renderObjectiveArrow(GameObject *target, Color c, F32 alphaMod = 1.0f);
   void renderObjectiveArrow(Point p, Color c, F32 alphaMod = 1.0f);

   void renderTimeLeft();
   void renderTalkingClients();

   void addTime(U32 time);    // Extend the game by time (in ms)

   virtual void clientRequestLoadout(GameConnection *client, const Vector<U32> &loadout);
   virtual void updateShipLoadout(GameObject *shipObject); // called from LoadoutZone when a Ship touches the zone
   void setClientShipLoadout(ClientRef *cl, const Vector<U32> &loadout);

   virtual Color getShipColor(Ship *s);         // Get the color of a ship
   virtual Color getTeamColor(S32 team);        // Get the color of a team, based on index
   Color getTeamColor(GameObject *theObject);   // Get the color of a team, based on object

   S32 getTeam(const char *playerName);         // Given a player, return their team

   const char *getTeamName(S32 team);           // Return the name of the team


   // gameType flag methods for CTF, Rabbit, Football
   virtual void addFlag(FlagItem *flag) {  /* do nothing */  }
   virtual void itemDropped(Ship *ship, Item *item) {  /* do nothing */  }
   virtual void shipTouchFlag(Ship *ship, FlagItem *flag) {  /* do nothing */  }

   virtual void addZone(GoalZone *zone) {  /* Do nothing */  }
   virtual void shipTouchZone(Ship *ship, GoalZone *zone) {  /* Do nothing */  }

   void queryItemsOfInterest();
   void performScopeQuery(GhostConnection *connection);
   virtual void performProxyScopeQuery(GameObject *scopeObject, GameConnection *connection);

   // Functions related to loading levels
   virtual bool processLevelItem(S32 argc, const char **argv);

   void onGhostAvailable(GhostConnection *theConnection);
   TNL_DECLARE_RPC(s2cSetLevelInfo, (StringTableEntry levelName, StringTableEntry levelDesc, S32 teamScoreLimit, StringTableEntry levelCreds, 
                                     S32 objectCount, F32 lx, F32 ly, F32 ux, F32 uy, bool levelHasLoadoutZone, bool engineerEnabled));
   TNL_DECLARE_RPC(s2cAddBarriers, (Vector<F32> barrier, F32 width, bool solid));
   TNL_DECLARE_RPC(s2cAddTeam, (StringTableEntry teamName, F32 r, F32 g, F32 b));
   TNL_DECLARE_RPC(s2cAddClient, (StringTableEntry clientName, bool isMyClient, bool isAdmin, bool isRobot, bool playAlert));
   TNL_DECLARE_RPC(s2cClientJoinedTeam, (StringTableEntry clientName, RangedU32<0, gMaxTeams> teamIndex));
   TNL_DECLARE_RPC(s2cClientBecameAdmin, (StringTableEntry clientName));
   TNL_DECLARE_RPC(s2cClientBecameLevelChanger, (StringTableEntry clientName));

   TNL_DECLARE_RPC(s2cSyncMessagesComplete, (U32 sequence));
   TNL_DECLARE_RPC(c2sSyncMessagesComplete, (U32 sequence));

   TNL_DECLARE_RPC(s2cSetGameOver, (bool gameOver));
   TNL_DECLARE_RPC(s2cSetTimeRemaining, (U32 timeLeft));
   TNL_DECLARE_RPC(s2cChangeScoreToWin, (U32 score, StringTableEntry changer));
   

   TNL_DECLARE_RPC(s2cCanSwitchTeams, (bool allowed));

   TNL_DECLARE_RPC(s2cRemoveClient, (StringTableEntry clientName));

   // Not all of these actually used?
   void updateScore(Ship *ship, ScoringEvent event, S32 data = 0);               // used
   void updateScore(ClientRef *client, S32 team, ScoringEvent event, S32 data = 0);
   void updateScore(ClientRef *client, ScoringEvent event, S32 data = 0);        // used
   void updateScore(S32 team, ScoringEvent event, S32 data = 0);

   void updateLeadingTeamAndScore();   // Sets mLeadingTeamScore and mLeadingTeam
   void updateRatings();               // Update everyone's game-normalized ratings at the end of the game


   TNL_DECLARE_RPC(s2cSetTeamScore, (RangedU32<0, gMaxTeams> teamIndex, U32 score));

   TNL_DECLARE_RPC(c2sRequestScoreboardUpdates, (bool updates));
   TNL_DECLARE_RPC(s2cScoreboardUpdate, (Vector<RangedU32<0, MaxPing> > pingTimes, Vector<SignedInt<24> > scores, Vector<RangedU32<0,200> > ratings));
   virtual void updateClientScoreboard(ClientRef *theClient);

   TNL_DECLARE_RPC(c2sAdvanceWeapon, ());
   TNL_DECLARE_RPC(c2sSelectWeapon, (RangedU32<0, ShipWeaponCount> index));
   TNL_DECLARE_RPC(c2sDropItem, ());
   TNL_DECLARE_RPC(c2sReaffirmMountItem, (U16 itemId));

   // Handle additional game-specific menu options for the client and the admin
   virtual void addClientGameMenuOptions(Vector<MenuItem *> &menuOptions);
   //virtual void processClientGameMenuOption(U32 index);                        // Param used only to hold team, at the moment

   virtual void addAdminGameMenuOptions(Vector<MenuItem *> &menuOptions);

   TNL_DECLARE_RPC(c2sAddTime, (U32 time));                                    // Admin is adding time to the game
   TNL_DECLARE_RPC(c2sChangeTeams, (S32 team));                                // Player wants to change teams

   TNL_DECLARE_RPC(c2sSendChat, (bool global, StringPtr message));             // In-game chat
   TNL_DECLARE_RPC(c2sSendChatSTE, (bool global, StringTableEntry ste));       // Quick-chat
   TNL_DECLARE_RPC(c2sSendCommand, (StringTableEntry cmd, Vector<StringPtr> args));

   TNL_DECLARE_RPC(s2cDisplayChatMessage, (bool global, StringTableEntry clientName, StringPtr message));
   TNL_DECLARE_RPC(s2cDisplayChatMessageSTE, (bool global, StringTableEntry clientName, StringTableEntry message));


   // killerName will be ignored if killer is supplied
   TNL_DECLARE_RPC(s2cKillMessage, (StringTableEntry victim, StringTableEntry killer, StringTableEntry killerName));

   TNL_DECLARE_RPC(c2sVoiceChat, (bool echo, ByteBufferPtr compressedVoice));
   TNL_DECLARE_RPC(s2cVoiceChat, (StringTableEntry client, ByteBufferPtr compressedVoice));

   TNL_DECLARE_CLASS(GameType);

   enum {
      mZoneGlowTime = 800,    // Time for visual effect, used by Nexus & GoalZone
   };
   Timer mZoneGlowTimer;
   S32 mGlowingZoneTeam;      // Which team's zones are glowing, -1 for all

   virtual void majorScoringEventOcurred(S32 team) { /* empty */ }    // Gets called when touchdown is scored...  currently only used by zone control & retrieve

   void processServerCommand(ClientRef *clientRef, const char *cmd, Vector<StringPtr> args);
};

#define GAMETYPE_RPC_S2C(className, methodName, args, argNames) \
   TNL_IMPLEMENT_NETOBJECT_RPC(className, methodName, args, argNames, NetClassGroupGameMask, RPCGuaranteedOrdered, RPCToGhost, 0)

#define GAMETYPE_RPC_C2S(className, methodName, args, argNames) \
   TNL_IMPLEMENT_NETOBJECT_RPC(className, methodName, args, argNames, NetClassGroupGameMask, RPCGuaranteedOrdered, RPCToGhostParent, 0)

};

#endif


