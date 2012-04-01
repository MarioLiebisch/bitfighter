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

#include "barrier.h"
#include "config.h"
#include "EngineeredItem.h"      // For EngineerModuleDeployer
#include "game.h"
#include "gameLoader.h"
#include "gameNetInterface.h"
#include "gameObject.h"
#include "gameObjectRender.h"
#include "gameType.h"
#include "masterConnection.h"
#include "move.h"
#include "moveObject.h"
#include "projectile.h"          // For SpyBug class
#include "SoundSystem.h"
#include "SharedConstants.h"     // For ServerInfoFlags enum
#include "ship.h"
#include "GeomUtils.h"
#include "luaLevelGenerator.h"
#include "robot.h"
#include "shipItems.h"           // For moduleInfos
#include "stringUtils.h"
#include "NexusGame.h"           // For creating new NexusFlagItem
#include "teamInfo.h"            // For TeamManager def
#include "playerInfo.h"

#include "ClientInfo.h"

#include "IniFile.h"             // For CIniFile def
#include "BanList.h"             // For banList kick duration

#include "BotNavMeshZone.h"      // For zone clearing code

#include "WallSegmentManager.h"

#include "tnl.h"
#include "tnlRandom.h"
#include "tnlGhostConnection.h"
#include "tnlNetInterface.h"

#include "md5wrapper.h"

#include <boost/shared_ptr.hpp>
#include <sys/stat.h>
#include <cmath>


#include "soccerGame.h"


#ifdef PRINT_SOMETHING
#include "ClientGame.h"  // only used to print some variables in ClientGame...
#endif


using namespace TNL;

namespace Zap
{

////////////////////////////////////////
////////////////////////////////////////

// Constructor
NameToAddressThread::NameToAddressThread(const char *address_string) : mAddress_string(address_string)
{
   mDone = false;
}

// Destructor
NameToAddressThread::~NameToAddressThread()
{
   // Do nothing
}


U32 NameToAddressThread::run()
{
   // This can take a lot of time converting name (such as "bitfighter.org:25955") into IP address.
   mAddress.set(mAddress_string);
   mDone = true;
   return 0;
}


////////////////////////////////////////
////////////////////////////////////////

#ifndef ZAP_DEDICATED
class ClientGame;
extern ClientGame *gClientGame; // only used to see if we have a ClientGame, for buildBotMeshZones
#endif

// Global Game objects
ServerGame *gServerGame = NULL;

static Vector<DatabaseObject *> fillVector2;


//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------

// Constructor
Game::Game(const Address &theBindAddress, GameSettings *settings) : mGameObjDatabase(new GridDatabase())      //? was without new
{
   mSettings = settings;

   mNextMasterTryTime = 0;
   mReadyToConnectToMaster = false;

   mCurrentTime = 0;
   mGameSuspended = false;

   mRobotCount = 0;
   mPlayerCount = 0;

   mTimeUnconnectedToMaster = 0;

   mNetInterface = new GameNetInterface(theBindAddress, this);
   mHaveTriedToConnectToMaster = false;

   mNameToAddressThread = NULL;

   mTeamManager = new TeamManager;                    // gets deleted in destructor 
   mActiveTeamManager = mTeamManager;
}


// Destructor
Game::~Game()
{
   delete mTeamManager;

   if(mNameToAddressThread)
      delete mNameToAddressThread;
}


F32 Game::getGridSize() const
{
   return mGridSize;
}


void Game::setGridSize(F32 gridSize) 
{ 
   mGridSize = max(min(gridSize, F32(MAX_GRID_SIZE)), F32(MIN_GRID_SIZE));
}


U32 Game::getCurrentTime()
{
   return mCurrentTime;
}


const Vector<SafePtr<GameObject> > &Game::getScopeAlwaysList()
{
   return mScopeAlwaysList;
}


void Game::setScopeAlwaysObject(GameObject *theObject)
{
   mScopeAlwaysList.push_back(theObject);
}


GameSettings *Game::getSettings()
{
   return mSettings;
}


bool Game::isSuspended()
{
   return mGameSuspended;
}


void Game::resetMasterConnectTimer()
{
   mNextMasterTryTime = 0;
}


void Game::setReadyToConnectToMaster(bool ready)
{
   mReadyToConnectToMaster = ready;
}


Point Game::getScopeRange(S32 sensorStatus)
{
   switch(sensorStatus)
   {
      case Ship::SensorStatusPassive:
         return Point(PLAYER_SENSOR_PASSIVE_VISUAL_DISTANCE_HORIZONTAL + PLAYER_SCOPE_MARGIN,
               PLAYER_SENSOR_PASSIVE_VISUAL_DISTANCE_VERTICAL + PLAYER_SCOPE_MARGIN);

      case Ship::SensorStatusActive:
         return Point(PLAYER_SENSOR_ACTIVE_VISUAL_DISTANCE_HORIZONTAL + PLAYER_SCOPE_MARGIN,
               PLAYER_SENSOR_ACTIVE_VISUAL_DISTANCE_VERTICAL + PLAYER_SCOPE_MARGIN);

      case Ship::SensorStatusOff:
      default:
         return Point(PLAYER_VISUAL_DISTANCE_HORIZONTAL + PLAYER_SCOPE_MARGIN,
               PLAYER_VISUAL_DISTANCE_VERTICAL + PLAYER_SCOPE_MARGIN);
   }
}


S32 Game::getClientCount() const
{
   return mClientInfos.size();
}


S32 Game::getPlayerCount() const
{
   return mPlayerCount;
}


S32 Game::getAuthenticatedPlayerCount() const
{
   S32 count = 0;
   for(S32 i = 0; i < mClientInfos.size(); i++)
      if(!mClientInfos[i]->isRobot() && mClientInfos[i]->isAuthenticated())
         count++;

   return count;
}


S32 Game::getRobotCount() const
{
   return mRobotCount;
}


ClientInfo *Game::getClientInfo(S32 index) const
{ 
   return mClientInfos[index]; 
}


const Vector<ClientInfo *> *Game::getClientInfos()
{
   return &mClientInfos;
}


// ClientInfo will be a RemoteClientInfo in ClientGame and a FullClientInfo in ServerGame
void Game::addToClientList(ClientInfo *clientInfo) 
{ 
   mClientInfos.push_back(clientInfo);

   if(clientInfo->isRobot())
      mRobotCount++;
   else
      mPlayerCount++;
}     


// Helper function for other find functions
S32 Game::findClientIndex(const StringTableEntry &name)
{
   for(S32 i = 0; i < mClientInfos.size(); i++)
      if(mClientInfos[i]->getName() == name) 
         return i;

   return -1;     // Not found
}


void Game::removeFromClientList(const StringTableEntry &name)
{
   S32 index = findClientIndex(name);

   if(index >= 0)
   {
      if(mClientInfos[index]->isRobot())
         mRobotCount--;
      else
         mPlayerCount--;

      mClientInfos.erase_fast(index);
   }
}


void Game::removeFromClientList(ClientInfo *clientInfo)
{
   for(S32 i = 0; i < mClientInfos.size(); i++)
      if(mClientInfos[i] == clientInfo)
      {
         if(mClientInfos[i]->isRobot())
            mRobotCount--;
         else
            mPlayerCount--;

         mClientInfos.erase_fast(i);
         return;
      }
}


void Game::clearClientList() 
{ 
   mClientInfos.clear(); 

   mRobotCount = 0;
   mPlayerCount = 0;
}


// Find clientInfo given a player name
ClientInfo *Game::findClientInfo(const StringTableEntry &name)
{
   S32 index = findClientIndex(name);

   return index >= 0 ? mClientInfos[index] : NULL;
}


GameNetInterface *Game::getNetInterface()
{
   return mNetInterface;
}


GridDatabase *Game::getGameObjDatabase()
{
   return mGameObjDatabase.get();
}


MasterServerConnection *Game::getConnectionToMaster()
{
   return mConnectionToMaster;
}


GameType *Game::getGameType() const
{
   return mGameType;    // This is a safePtr, so it can be NULL, but will never point off into space
}


S32 Game::getTeamCount() const
{
   return mActiveTeamManager->getTeamCount();
}


AbstractTeam *Game::getTeam(S32 team) const
{
   return mActiveTeamManager->getTeam(team);
}


// Sorts teams by score, high to low
S32 QSORT_CALLBACK teamScoreSort(Team **a, Team **b)
{
   return (*b)->getScore() - (*a)->getScore();  
}


Vector<Team *> *Game::getSortedTeamList_score() const
{
   static Vector<Team *> teams;     // Static, since we return a pointer, and the object pointed to needs to outlive this function scope

   S32 teamCount = getTeamCount();

   teams.resize(teamCount);

   for(S32 i = 0; i < teamCount; i++)
   {
      teams[i] = (Team *)getTeam(i);
      teams[i]->mId = i;
   }

   teams.sort(teamScoreSort);

   return &teams;
}


// There is a bigger need to use StringTableEntry and not const char *
//    mainly to prevent errors on CTF neutral flag and out of range team number.
StringTableEntry Game::getTeamName(S32 teamIndex) const
{
   if(teamIndex >= 0 && teamIndex < getTeamCount())
      return getTeam(teamIndex)->getName();
   else if(teamIndex == TEAM_HOSTILE)
      return StringTableEntry("Hostile");
   else if(teamIndex == TEAM_NEUTRAL)
      return StringTableEntry("Neutral");
   else
      return StringTableEntry("UNKNOWN");
}


// Given a player's name, return his team
S32 Game::getTeamIndex(const StringTableEntry &playerName)
{
   ClientInfo *clientInfo = findClientInfo(playerName);              // Returns NULL if player can't be found
   
   return clientInfo ? clientInfo->getTeamIndex() : TEAM_NEUTRAL;    // If we can't find the team, let's call it neutral
}


// The following just delegate their work to the TeamManager
void Game::removeTeam(S32 teamIndex) { mActiveTeamManager->removeTeam(teamIndex); }

void Game::addTeam(AbstractTeam *team) { mActiveTeamManager->addTeam(team); }

void Game::addTeam(AbstractTeam *team, S32 index) { mActiveTeamManager->addTeam(team, index); }

void Game::replaceTeam(AbstractTeam *team, S32 index) { mActiveTeamManager->replaceTeam(team, index); }

void Game::clearTeams() { mActiveTeamManager->clearTeams(); }


// Makes sure that the mTeams[] structure has the proper player counts
// Needs to be called manually before accessing the structure
// Rating may only work on server... not tested on client
void Game::countTeamPlayers()
{
   for(S32 i = 0; i < getTeamCount(); i++)
   {
      TNLAssert(dynamic_cast<Team *>(getTeam(i)), "Invalid team");      // Assert for safety
      static_cast<Team *>(getTeam(i))->clearStats();                    // static_cast for speed
   }

   for(S32 i = 0; i < getClientCount(); i++)
   {
      ClientInfo *clientInfo =  getClientInfo(i);

      S32 teamIndex = clientInfo->getTeamIndex();

      if(teamIndex >= 0 && teamIndex < getTeamCount())
      { 
         // Robot could be neutral or hostile, skip out of range team numbers
         TNLAssert(dynamic_cast<Team *>(getTeam(teamIndex)), "Invalid team");
         Team *team = static_cast<Team *>(getTeam(teamIndex));            

         if(clientInfo->isRobot())
            team->incrementBotCount();
         else
            team->incrementPlayerCount();

         // The following bit won't work on the client... 
         if(isServer())
         {
            const F32 BASE_RATING = .1f;
            team->addRating(max(clientInfo->getCalculatedRating(), BASE_RATING));    
         }
      }
   }
}


void Game::setGameType(GameType *theGameType)      // TODO==> Need to store gameType as a shared_ptr or auto_ptr
{
   //delete mGameType;          // Cleanup, if need be
   mGameType = theGameType;
}


U32 Game::getTimeUnconnectedToMaster()
{
   return mTimeUnconnectedToMaster;
}


// Note: lots of stuff for this method in child classes!
void Game::onConnectedToMaster()
{
   getSettings()->saveMasterAddressListInIniUnlessItCameFromCmdLine();
}


void Game::resetLevelInfo()
{
   setGridSize((F32)DefaultGridSize);
}


// Process a single line of a level file, loaded in gameLoader.cpp
// argc is the number of parameters on the line, argv is the params themselves
// Used by ServerGame and the editor
void Game::processLevelLoadLine(U32 argc, U32 id, const char **argv, GridDatabase *database, bool inEditor, const string &levelFileName)
{
   S32 strlenCmd = (S32) strlen(argv[0]);

   // This is a legacy from the old Zap! days... we do bots differently in Bitfighter, so we'll just ignore this line if we find it.
   if(!stricmp(argv[0], "BotsPerTeam"))
      return;

   else if(!stricmp(argv[0], "GridSize"))      // GridSize requires a single parameter (an int specifiying how many pixels in a grid cell)
   {                                           
      if(argc < 2)
         logprintf(LogConsumer::LogLevelError, "Improperly formed GridSize parameter");
      else
         setGridSize((F32)atof(argv[1]));

      return;
   }

   // Parse GameType line... All game types are of form XXXXGameType
   else if(strlenCmd >= 8 && !strcmp(argv[0] + strlenCmd - 8, "GameType"))
   {
      // validateGameType() will return a valid GameType string -- either what's passed in, or the default if something bogus was specified
      TNL::Object *theObject;
      if(!strcmp(argv[0], "HuntersGameType"))
         theObject = new NexusGameType();
      else
         theObject = TNL::Object::create(GameType::validateGameType(argv[0]));

      GameType *gt = dynamic_cast<GameType *>(theObject);  // Force our new object to be a GameObject

      if(gt)
      {
         bool validArgs = gt->processArguments(argc - 1, argv + 1, NULL);
         if(!validArgs)
            logprintf(LogConsumer::LogLevelError, "GameType have incorrect parameters");

         gt->addToGame(this, database);    
      }
      else
         logprintf(LogConsumer::LogLevelError, "Could not create a GameType");
      
      return;
   }

   if(getGameType() && processLevelParam(argc, argv)) 
   {
      // Do nothing here
   }
   else if(getGameType() && processPseudoItem(argc, argv, levelFileName, database))
   {
      // Do nothing here
   }
   
   else
   {
      char obj[LevelLoader::MaxArgLen + 1];

      // Convert any NexusFlagItem into FlagItem, only NexusFlagItem will show up on ship
      if(!stricmp(argv[0], "HuntersFlagItem") || !stricmp(argv[0], "NexusFlagItem"))
         strcpy(obj, "FlagItem");

      // Convert legacy Hunters* objects
      else if(!stricmp(argv[0], "HuntersNexusObject"))
         strcpy(obj, "NexusObject");

      else
         strncpy(obj, argv[0], LevelLoader::MaxArgLen);

      obj[LevelLoader::MaxArgLen] = '\0';
      TNL::Object *theObject = TNL::Object::create(obj);          // Create an object of the type specified on the line

      SafePtr<GameObject> object  = dynamic_cast<GameObject *>(theObject);  // Force our new object to be a GameObject
      EditorObject *eObject = dynamic_cast<EditorObject *>(theObject);


      if(!object && !eObject)    // Well... that was a bad idea!
      {
         logprintf(LogConsumer::LogLevelError, "Unknown object type \"%s\" in level \"%s\"", obj, levelFileName.c_str());
         delete theObject;
      }
      else  // object was valid
      {
         computeWorldObjectExtents();    // Make sure this is current if we process a robot that needs this for intro code

         bool validArgs = object->processArguments(argc - 1, argv + 1, this);

         // Mark the item as being a ghost (client copy of a server object) so that the object will not trigger server-side tests
         // The only time this code is run on the client is when loading into the editor.
         if(inEditor)
            object->markAsGhost();

         if(validArgs)
         {
            if(object.isValid())  // processArguments might delete this object (teleporter)
               object->addToGame(this, database);
         }
         else
         {
            logprintf(LogConsumer::LogLevelError, "Invalid arguments in object \"%s\" in level \"%s\"", obj, levelFileName.c_str());
            object->destroySelf();
         }
      }
   }
}


// Returns true if we've handled the line (even if it handling it means that the line was bogus); returns false if
// caller needs to create an object based on the line
bool Game::processLevelParam(S32 argc, const char **argv)
{
   if(!stricmp(argv[0], "Team"))
      onReadTeamParam(argc, argv);

   // TODO: Create better way to change team details from level scripts: https://code.google.com/p/bitfighter/issues/detail?id=106
   else if(!stricmp(argv[0], "TeamChange"))   // For level script. Could be removed when there is a better way to change team names and colors.
      onReadTeamChangeParam(argc, argv);

   else if(!stricmp(argv[0], "Specials"))
      onReadSpecialsParam(argc, argv);

   else if(!strcmp(argv[0], "Script"))
      onReadScriptParam(argc, argv);

   else if(!stricmp(argv[0], "LevelName"))
      onReadLevelNameParam(argc, argv);
   
   else if(!stricmp(argv[0], "LevelDescription"))
      onReadLevelDescriptionParam(argc, argv);

   else if(!stricmp(argv[0], "LevelCredits"))
      onReadLevelCreditsParam(argc, argv);
   else if(!stricmp(argv[0], "MinPlayers"))     // Recommend a min number of players for this map
   {
      if(argc > 1)
         getGameType()->setMinRecPlayers(atoi(argv[1]));
   }
   else if(!stricmp(argv[0], "MaxPlayers"))     // Recommend a max number of players for this map
   {
      if(argc > 1)
         getGameType()->setMaxRecPlayers(atoi(argv[1]));
   }
   else
      return false;     // Line not processed; perhaps the caller can handle it?

   return true;         // Line processed; caller can ignore it
}


// Write out the game processed above; returns multiline string
string Game::toString()
{
   string str;

   GameType *gameType = getGameType();

   str = gameType->toString() + "\n";

   str += string("LevelName ") + writeLevelString(gameType->getLevelName()->getString()) + "\n";
   str += string("LevelDescription ") + writeLevelString(gameType->getLevelDescription()->getString()) + "\n";
   str += string("LevelCredits ") + writeLevelString(gameType->getLevelCredits()->getString()) + "\n";

   str += string("GridSize ") + ftos(mGridSize) + "\n";

   for(S32 i = 0; i < mActiveTeamManager->getTeamCount(); i++)
      str += mActiveTeamManager->getTeam(i)->toString() + "\n";

   str += gameType->getSpecialsLine() + "\n";

   if(gameType->getScriptName() != "")
      str += "Script " + gameType->getScriptLine() + "\n";

   str += string("MinPlayers") + (gameType->getMinRecPlayers() > 0 ? " " + itos(gameType->getMinRecPlayers()) : "") + "\n";
   str += string("MaxPlayers") + (gameType->getMaxRecPlayers() > 0 ? " " + itos(gameType->getMaxRecPlayers()) : "") + "\n";

   return str;
}


// Only occurs in scripts; could be in editor or on server
void Game::onReadTeamChangeParam(S32 argc, const char **argv)
{
   if(argc >= 2)   // Enough arguments?
   {
      S32 teamNumber = atoi(argv[1]);   // Team number to change

      if(teamNumber >= 0 && teamNumber < getTeamCount())
      {
         AbstractTeam *team = getNewTeam();
         team->processArguments(argc-1, argv+1);          // skip one arg
         replaceTeam(team, teamNumber);
      }
   }
}


void Game::onReadSpecialsParam(S32 argc, const char **argv)
{         
   for(S32 i = 1; i < argc; i++)
      if(!getGameType()->processSpecialsParam(argv[i]))
         logprintf(LogConsumer::LogLevelError, "Invalid specials parameter: %s", argv[i]);
}


void Game::onReadScriptParam(S32 argc, const char **argv)
{
   Vector<string> args;

   // argv[0] is always "Script"
   for(S32 i = 1; i < argc; i++)
      args.push_back(argv[i]);

   getGameType()->setScript(args);
}


static string getString(S32 argc, const char **argv)
{
   string s;
   for(S32 i = 1; i < argc; i++)
   {
      s += argv[i];
      if(i < argc - 1)
         s += " ";
   }

   return s;
}


void Game::onReadLevelNameParam(S32 argc, const char **argv)
{
   string s = getString(argc, argv);
   getGameType()->setLevelName(s.substr(0, MAX_GAME_NAME_LEN).c_str());
}


void Game::onReadLevelDescriptionParam(S32 argc, const char **argv)
{
   string s = getString(argc, argv);
   getGameType()->setLevelDescription(s.substr(0, MAX_GAME_DESCR_LEN).c_str());
}


void Game::onReadLevelCreditsParam(S32 argc, const char **argv)
{
   string s = getString(argc, argv);
   getGameType()->setLevelCredits(s.substr(0, MAX_GAME_DESCR_LEN).c_str());
}


// Only used during level load process...  actually, used at all?  If so, should be combined with similar code in gameType
// Not used during normal game load... perhaps by lua loader?
void Game::setGameTime(F32 time)
{
   GameType *gt = getGameType();

   TNLAssert(gt, "Null gametype!");

   if(gt)
      gt->setGameTime(time * 60);
}


// Runs on server (levelgen) and client (Editor)
bool Game::runLevelGenScript(const FolderManager *folderManager, const string &scriptName, const Vector<string> &scriptArgs, 
                                   GridDatabase *targetDatabase)
{
   string fullname = folderManager->findLevelGenScript(scriptName);  // Find full name of levelgen script

   if(fullname == "")
   {
      Vector<StringTableEntry> e;
      e.push_back(scriptName.c_str());
      getGameType()->broadcastMessage(GameConnection::ColorRed, SFXNone, "!!! Error running levelgen %e0: Could not find file", e);
      logprintf(LogConsumer::LogWarning, "Warning: Could not find script \"%s\"", scriptName.c_str());
      return false;
   }

   // The script file will be the first argument, subsequent args will be passed on to the script
   LuaLevelGenerator levelgen = LuaLevelGenerator(fullname, folderManager->luaDir, scriptArgs, getGridSize(), 
                                                  targetDatabase, this);
   if(!levelgen.runScript())
   {
      Vector<StringTableEntry> e;
      e.push_back(scriptName.c_str());

      getGameType()->broadcastMessage(GameConnection::ColorRed, SFXNone, "!!! Error running levelgen %e0: See server log for details", e);
      return false;
   }

   return true;
}


// If there is no valid connection to master server, perodically try to create one.
// If user is playing a game they're hosting, they should get one master connection
// for the client and one for the server.
// Called from both clientGame and serverGame idle fuctions, so think of this as a kind of idle
void Game::checkConnectionToMaster(U32 timeDelta)
{
   if(mConnectionToMaster.isValid() && mConnectionToMaster->isEstablished())
      mTimeUnconnectedToMaster = 0;
   else if(mReadyToConnectToMaster)
      mTimeUnconnectedToMaster += timeDelta;

   if(!mConnectionToMaster.isValid())      // It's valid if it isn't null, so could be disconnected and would still be valid
   {
      Vector<string> *masterServerList = mSettings->getMasterServerList();

      if(masterServerList->size() == 0)
         return;

      if(mNextMasterTryTime < timeDelta && mReadyToConnectToMaster)
      {
         if(!mNameToAddressThread)
         {
            if(mHaveTriedToConnectToMaster && masterServerList->size() >= 2)
            {  
               // Rotate the list so as to try each one until we find one that works...
               masterServerList->push_back(string(masterServerList->get(0)));  // don't remove string(...), or else this line is a mystery why push_back an empty string.
               masterServerList->erase(0);
            }

            const char *addr = masterServerList->get(0).c_str();

            mHaveTriedToConnectToMaster = true;
            logprintf(LogConsumer::LogConnection, "%s connecting to master [%s]", isServer() ? "Server" : "Client", addr);

            mNameToAddressThread = new NameToAddressThread(addr);
            mNameToAddressThread->start();
         }
         else
         {
            if(mNameToAddressThread->mDone)
            {
               if(mNameToAddressThread->mAddress.isValid())
               {
                  TNLAssert(!mConnectionToMaster.isValid(), "Already have connection to master!");
                  mConnectionToMaster = new MasterServerConnection(this);
                  mConnectionToMaster->connect(mNetInterface, mNameToAddressThread->mAddress);
               }
   
               mNextMasterTryTime = GameConnection::MASTER_SERVER_FAILURE_RETRY_TIME;     // 10 secs, just in case this attempt fails
               delete mNameToAddressThread;
               mNameToAddressThread = NULL;
            }
         }
      }
      else if(!mReadyToConnectToMaster)
         mNextMasterTryTime = 0;
      else
         mNextMasterTryTime -= timeDelta;
   }
}

Game::DeleteRef::DeleteRef(GameObject *o, U32 d)
{
   theObject = o;
   delay = d;
}


void Game::addToDeleteList(GameObject *theObject, U32 delay)
{
   mPendingDeleteObjects.push_back(DeleteRef(theObject, delay));
}


// Cycle through our pending delete list, and either delete an object or update its timer
void Game::processDeleteList(U32 timeDelta)
{
   for(S32 i = 0; i < mPendingDeleteObjects.size(); )    // no i++
   {     // braces required
      if(timeDelta > mPendingDeleteObjects[i].delay)
      {
         GameObject *g = mPendingDeleteObjects[i].theObject;
         delete g;
         mPendingDeleteObjects.erase_fast(i);
      }
      else
      {
         mPendingDeleteObjects[i].delay -= timeDelta;
         i++;
      }
   }
}


// Delete all objects of specified type  --> currently only used to remove all walls from the game
void Game::deleteObjects(U8 typeNumber)
{
   fillVector.clear();
   mGameObjDatabase->findObjects(typeNumber, fillVector);
   for(S32 i = 0; i < fillVector.size(); i++)
   {
      GameObject *obj = dynamic_cast<GameObject *>(fillVector[i]);
      obj->deleteObject(0);
   }
}


// Not currently used
void Game::deleteObjects(TestFunc testFunc)
{
   fillVector.clear();
   mGameObjDatabase->findObjects(testFunc, fillVector);
   for(S32 i = 0; i < fillVector.size(); i++)
   {
      GameObject *obj = dynamic_cast<GameObject *>(fillVector[i]);
      obj->deleteObject(0);
   }
}


const ModuleInfo *Game::getModuleInfo(ShipModule module)
{
   TNLAssert(U32(module) < U32(ModuleCount), "out of range module");
   return &gModuleInfo[(U32) module];
}


void Game::computeWorldObjectExtents()
{
   mWorldExtents = mGameObjDatabase->getExtents();
}


Rect Game::computeBarrierExtents()
{
   Rect theRect;

   fillVector.clear();
   mGameObjDatabase->findObjects((TestFunc)isWallType, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
      theRect.unionRect(fillVector[i]->getExtent());

   return theRect;
}


Point Game::computePlayerVisArea(Ship *ship) const
{
   F32 activeFraction = ship->getSensorActiveZoomFraction();
   F32 equipFraction = ship->getSensorEquipZoomFraction();

   F32 fraction = 0;

   Point regVis(PLAYER_VISUAL_DISTANCE_HORIZONTAL, PLAYER_VISUAL_DISTANCE_VERTICAL);
   Point sensPassiveVis(PLAYER_SENSOR_PASSIVE_VISUAL_DISTANCE_HORIZONTAL, PLAYER_SENSOR_PASSIVE_VISUAL_DISTANCE_VERTICAL);
   Point sensActiveVis(PLAYER_SENSOR_ACTIVE_VISUAL_DISTANCE_HORIZONTAL, PLAYER_SENSOR_ACTIVE_VISUAL_DISTANCE_VERTICAL);

   Point *comingFrom;
   Point *goingTo;

   // This trainwreck is how to determine our visibility based on the tri-state sensor module
   // It depends on the last sensor status, which is determined if one of the two timers is still
   // running
   switch(ship->getSensorStatus())
   {
      case Ship::SensorStatusPassive:
         if (equipFraction < 1)
         {
            comingFrom = &regVis;
            goingTo = &sensPassiveVis;
            fraction = equipFraction;
         }
         else if(activeFraction < 1)
         {
            goingTo = &sensPassiveVis;
            comingFrom = &sensActiveVis;
            fraction = activeFraction;
         }
         else
         {
            goingTo = &sensPassiveVis;
            comingFrom = &sensPassiveVis;
            fraction = 1;
         }
         break;

      case Ship::SensorStatusActive:
         goingTo = &sensActiveVis;
         comingFrom = &sensPassiveVis;
         fraction = activeFraction;
         break;

      case Ship::SensorStatusOff:
      default:
         if (activeFraction < 1)
            comingFrom = &sensActiveVis;
         else
            comingFrom = &sensPassiveVis;
         goingTo = &regVis;
         fraction = equipFraction;
         break;
   }

   // Ugly
   return *comingFrom + (*goingTo - *comingFrom) * fraction;
}


// Make sure name is unique.  If it's not, make it so.  The problem is that then the client doesn't know their official name.
// This makes the assumption that we'll find a unique name before numstr runs out of space (allowing us to try 999,999,999 or so combinations)
string Game::makeUnique(const char *name)
{
   U32 index = 0;
   string proposedName = name;

   bool unique = false;

   while(!unique)
   {
      unique = true;

      for(S32 i = 0; i < getClientCount(); i++)
      {
         if(proposedName == getClientInfo(i)->getName().getString())     // Collision detected!
         {
            unique = false;

            char numstr[10];
            sprintf(numstr, ".%d", index);

            // Max length name can be such that when number is appended, it's still less than MAX_PLAYER_NAME_LENGTH
            S32 maxNamePos = MAX_PLAYER_NAME_LENGTH - (S32)strlen(numstr); 
            proposedName = string(name).substr(0, maxNamePos) + numstr;     // Make sure name won't grow too long

            index++;
            break;
         }
      }
   }

   return proposedName;
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
LevelInfo::LevelInfo()      
{
   initialize();
}


// Constructor, only used on client
LevelInfo::LevelInfo(const StringTableEntry &name, GameTypeId type)
{
   initialize();

   levelName = name;  
   levelType = type; 
}


// Constructor
LevelInfo::LevelInfo(const string &levelFile)
{
   initialize();

   levelFileName = levelFile.c_str();
}


void LevelInfo::initialize()
{
   levelName = "";
   levelType = BitmatchGame;
   levelFileName = "";
   minRecPlayers = 0;
   maxRecPlayers = 0;
}


// Called when ClientGame and ServerGame are destructed, and new levels are loaded on the server
void Game::cleanUp()
{
   // Delete any objects on the delete list
   processDeleteList(0xFFFFFFFF);

   // Delete any game objects that may exist  --> not sure this will be needed when we're using shared_ptr
   // sam: should be deleted to properly get removed from server's database and to remove client's net objects.
   fillVector.clear();
   mGameObjDatabase->findObjects(fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      mGameObjDatabase->removeFromDatabase(fillVector[i], fillVector[i]->getExtent());
      delete dynamic_cast<Object *>(fillVector[i]); // dynamic_cast might be needed to avoid errors
   }

   mActiveTeamManager->clearTeams();

   if(mGameType.isValid() && !mGameType->isGhost())
      delete mGameType.getPointer();
}


Rect Game::getWorldExtents()
{
   return mWorldExtents;
}


bool Game::isTestServer()
{
   return false;
}


const Color *Game::getTeamColor(S32 teamId) const
{
   return mActiveTeamManager->getTeamColor(teamId);
}


void Game::onReadTeamParam(S32 argc, const char **argv)
{
   if(getTeamCount() < GameType::MAX_TEAMS)     // Too many teams?
   {
      AbstractTeam *team = getNewTeam();
      if(team->processArguments(argc, argv))
         addTeam(team);
   }
}


void Game::setActiveTeamManager(TeamManager *teamManager)
{
   mActiveTeamManager = teamManager;
}

};


