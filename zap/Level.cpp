//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "Level.h"

#include "Colors.h"
#include "EngineeredItem.h"
#include "game.h"
#include "gameType.h"
#include "LevelDatabase.h"
#include "LevelLoadException.h"
#include "robot.h"
#include "Spawn.h"
#include "WallItem.h"
#include "WallSegmentManager.h"

#include "Md5Utils.h"
#include "stringUtils.h"

#include "tnlLog.h"

#include <fstream>
#include <sstream>


namespace Zap
{


// Constructor
Level::Level() 
{ 
   initialize();
}


// Constructor, with passed level code, mainly used for testing
Level::Level(const string &levelCode)
{
   initialize();
   loadLevelFromString(levelCode);
}


// Destructor
Level::~Level()
{
   // Clean up our GameType -- it's a RefPtr, so will be deleted when all refs are removed
   //if(mGameType.isValid() && !mGameType->isGhost())
   //   mGameType.set(NULL);
}


void Level::initialize()
{
   mVersion = 0; 
   mLegacyGridSize = 1; 
   mDatabaseId = 0; 
   mGame = NULL;
   mTeamInfos.reset(new Vector<TeamInfo>);      // mTeamInfos is a shared_ptr, which will handle cleanup
   mLevelDatabaseId = LevelDatabase::NOT_IN_DATABASE;
}


Level *Level::clone()
{
   Level *newLevel = new Level;

   newLevel->copyObjects(this);
   newLevel->setGameType(getGameType()->clone());
   newLevel->setTeamInfosPtr(getTeamInfosClone());

   return newLevel;
}


string Level::getHash() const
{
   return mLevelHash;
}


// Gets run when this level is associated with a game.  From this point forward, the Level object
// will be tainted and polluted.  Don't reuse -- discard when done.
// Runs on client and server
void Level::onAddedToGame(Game *game)
{
   mGame = game;

   // On client, might not have a GameType yet... need to wait for one to be sent from server
   if(mGameType)
      mGameType->addToGame(game);

   for(S32 i = 0; i < mTeamInfos->size(); i++)
   {
      AbstractTeam *team = game->getNewTeam();  // Will be deleted by TeamManager
      team->set(mTeamInfos->get(i));
      mTeamManager.addTeam(team);
   }
}


GridDatabase &Level::getBotZoneDatabase()
{
   return mBotZoneDatabase;
}


Vector<BotNavMeshZone *> &Level::getBotZoneList()
{
   return mAllZones;
}


// Server only!
void Level::addBots(Game *game)
{
   for(S32 i = 0; i < mRobotLines.size(); i++)
   {
      Robot *robot = new Robot();
      if(robot->processArguments(mRobotLines[i], game))
         robot->addToGame(game, this);
      else
         delete robot;
   }
}


// This is the core loader for levels in-game and in-editor.  This will always return a valid level, even
// if contents is empty or somehow invalid.
void Level::loadLevelFromString(const string &contents, const string &filename)
{
   istringstream iss(contents);
   string line;

   Md5::IncrementalHasher md5;

   while(std::getline(iss, line))
   {
      parseLevelLine(line, filename);
      md5.add(line);
   }

	mLevelHash = md5.getHash();

   // Build wall edge geometry
   buildWallEdgeGeometry();

   // Snap enigneered items to those edges
   snapAllEngineeredItems(false);

   validateLevel();
}


void Level::buildWallEdgeGeometry()
{
   const Vector<DatabaseObject *> *polyWalls = findObjects_fast(PolyWallTypeNumber);
   const Vector<DatabaseObject *> *wallItems = findObjects_fast(WallItemTypeNumber);

   fillVector.clear();
   fillVector.reserve(wallItems->size() + polyWalls->size());

   for(S32 i = 0; i < wallItems->size(); i++)
      fillVector.push_back(wallItems->get(i));

   for(S32 i = 0; i < polyWalls->size(); i++)
      fillVector.push_back(polyWalls->get(i));

   getWallSegmentManager()->recomputeAllWallGeometry(this, fillVector);
}


// Snaps all engineered items in database
void Level::snapAllEngineeredItems(bool onlyUnsnapped)
{
   Vector<Zap::DatabaseObject *> fillVector;

   findObjects((TestFunc)isEngineeredType, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      EngineeredItem *engrObj = static_cast<EngineeredItem *>(fillVector[i]);

      // Skip already snapped items if only processing unsnapped ones
      if(onlyUnsnapped && engrObj->isSnapped())
         continue;

      engrObj->mountToWall(engrObj->getPos(), getWallSegmentManager(), NULL);
   }
}


// Load level stored in filename into database; returns true if file exists, false if not.  In either case,
// the Level object will be left in a usable state.
bool Level::loadLevelFromFile(const string &filename)
{
   string contents;
   bool fileExists = readFile(filename, contents);

   loadLevelFromString(contents, filename);

#ifdef SAM_ONLY
   // In case the level crash the game trying to load, want to know which file is the problem. 
   logprintf("Loading %s", filename.c_str());
#endif

   return fileExists;
}


// Ensure level has minimum standards of usability... Called after Level has loaded, but before it is added to a game.
void Level::validateLevel()
{
   if(!mGameType)
      mGameType.set(new GameType(this));    // Cleaned up... where, exactly?

   if(mTeamInfos->size() == 0)
   {
      TeamInfo teamInfo;
      mTeamInfos->push_back(teamInfo);
   }
}


// Each line of the file is handled separately by processLevelLoadLine in game.cpp or UIEditor.cpp
void Level::parseLevelLine(const string &line, const string &levelFileName)
{
   Vector<string> args = parseString(line);
   U32 argc = args.size();
   S32 id = 0;

   string errorMsg;     // Reusable object

   const char** argv = new const char* [argc];     // Deleted below

   if(argc >= 1)
   {
      std::size_t pos = args[0].find("!");
      if(pos != string::npos)
      {
         id = atoi(args[0].substr(pos + 1, args[0].size() - pos - 1).c_str());
         args[0] = args[0].substr(0, pos);
      }
   }

   for(U32 i = 0; i < argc; i++)
      argv[i] = args[i].c_str();

   try
   {
      errorMsg.clear();
      bool ok = processLevelLoadLine(argc, id, (const char **)argv, errorMsg);
      if(!ok)
         logprintf(LogConsumer::LogLevelError, "Level Error: Non-fatal found in level %s: %s", 
                   levelFileName.c_str(), errorMsg.c_str());
   }
   catch(LevelLoadException &e)
   {
      logprintf(LogConsumer::LogLevelError, "Level Error: Fatal error with level %s, line %s: %s", 
                levelFileName.c_str(), line.c_str(), e.what());  // TODO: fix "line" variable having hundreds of level lines
   }

   delete[] argv;
}


static string getString(S32 argc, const char **argv)
{
   string str;

   for(S32 i = 1; i < argc; i++)
   {
      str.append(argv[i]);

      if(i < argc - 1)
         str += " ";
   }

   return trim(str);    // TODO: Is trim really necessary?  Write a test and find out!
}


// Return a copy of the teamInfos for this level
boost::shared_ptr<Vector<TeamInfo> > Level::getTeamInfosClone() const
{
   boost::shared_ptr<Vector<TeamInfo> > teamInfos;
   teamInfos.reset(new Vector<TeamInfo>);
   teamInfos->reserve(mTeamInfos->size());

   for(S32 i = 0; i < mTeamInfos->size(); i++)
      teamInfos->push_back(mTeamInfos->get(i));

   return teamInfos;
}


// Write out the game processed above; returns multiline string
string Level::toLevelCode() const
{
   string str;

   GameType *gameType = getGameType();

   str = "LevelFormat " + itos(Level::CurrentLevelFormat) + "\n";

   str += string(gameType->toLevelCode() + "\n");

   str += string("LevelName ")        + writeLevelString(gameType->getLevelName().c_str()) + "\n";
   str += string("LevelDescription ") + writeLevelString(gameType->getLevelDescription()) + "\n";
   str += string("LevelCredits ")     + writeLevelString(gameType->getLevelCredits().c_str()) + "\n";

   if(getLevelDatabaseId())
      str += string("LevelDatabaseId ") + itos(getLevelDatabaseId()) + "\n";

   for(S32 i = 0; i < getTeamCount(); i++)
      str += getTeamLevelCode(i) + "\n";

   str += gameType->getSpecialsLine() + "\n";

   if(gameType->getScriptName() != "")
      str += "Script " + gameType->getScriptLine() + "\n";

   str += string("MinPlayers") + (gameType->getMinRecPlayers() > 0 ? " " + itos(gameType->getMinRecPlayers()) : "") + "\n";
   str += string("MaxPlayers") + (gameType->getMaxRecPlayers() > 0 ? " " + itos(gameType->getMaxRecPlayers()) : "") + "\n";

   return str;
}


// Overridden on client
void Level::setLevelDatabaseId(U32 id)
{
   mLevelDatabaseId = id;
}


U32 Level::getLevelDatabaseId() const
{
   return mLevelDatabaseId;
}


GameType *Level::getGameType() const
{
   return mGameType;
}


// Used by editor and on client when GameType object arrives from server, and on server when loading a level.  
// Note that gameType could be NULL when testing from the editor.
void Level::setGameType(GameType *gameType)
{
   mGameType = gameType;      // mGameType is a referenced counted pointer, so cleanup will happen automatically

   if(mGameType)
      mGameType->setLevel(this);
}


void Level::deleteObject(S32 serialNumber)
{
   DatabaseObject *obj = findObjBySerialNumber(serialNumber);
   TNLAssert(obj, "Expect to find obj here!");

   removeFromDatabase(obj, true);
}


// Swap the specified object with the other specified object
void Level::swapObject(S32 serialNumber, const BfObject *withThisObject)
{
   DatabaseObject *obj = findObjBySerialNumber(serialNumber);
   TNLAssert(obj, "Could not find object!");

   Parent::removeFromDatabase(obj, true);
   addToDatabase(withThisObject->clone());
}


// Find specified object in specified database
BfObject *Level::findObjBySerialNumber(S32 serialNumber) const
{
   const Vector<DatabaseObject *> *objList = findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->getSerialNumber() == serialNumber)
         return obj;
   }

   return NULL;
}


string Level::getLevelName() const
{
   return mGameType->getLevelName();
}


string Level::getLevelCredits() const
{
   return mGameType->getLevelCredits();
}


S32 Level::getWinningScore() const
{
   return mGameType->getWinningScore();
}


F32 Level::getLegacyGridSize() const
{
   return mLegacyGridSize;
}


bool Level::getAddedToGame() const
{
   return (mGame != NULL);
}


S32 Level::getTeamCount() const
{
   if(mGame)
      return mTeamManager.getTeamCount();
   
   return mTeamInfos->size();
}


// Zero teams will crash, returns true if we had to add a team.  Server only.
bool Level::makeSureTeamCountIsNotZero()
{
   if(getTeamCount() == 0) 
   {
      addTeam(new Team("Missing Team", Colors::blue));     // Will be deleted by TeamManager
      return true;
   }

   return false;
}


const Color &Level::getTeamColor(S32 index) const
{
   // Note this logic duplicated inside mTeamManager... if that is only called from here, we can remove it there

   if(index == TEAM_NEUTRAL)
      return Colors::NeutralTeamColor;

   if(index == TEAM_HOSTILE)
      return Colors::HostileTeamColor;

   if(mGame)
      return mTeamManager.getTeamColor(index);
   else
      return mTeamInfos->get(index).getColor();
}


void Level::setTeamColor(S32 index, const Color &color)
{
   TNLAssert(mGame, "Expected game!");

   mTeamManager.setTeamColor(index, color);
}



void Level::setTeamName(S32 index, const string &name)
{
   TNLAssert(mGame, "Expected game!");

   mTeamManager.setTeamName(index, name.c_str());
}


StringTableEntry Level::getTeamName(S32 index) const
{
   if(mGame)
      return mTeamManager.getTeamName(index);
   else
      return mTeamInfos->get(index).getName();
}


void Level::removeTeam(S32 teamIndex)
{ 
   if(mGame)
      mTeamManager.removeTeam(teamIndex);
   else
      mTeamInfos->erase(teamIndex);
}


void Level::addTeam(AbstractTeam *team)
{ 
   TNLAssert(mGame, "Expected to have been added to a game by now!");
   mTeamManager.addTeam(team);
}


void Level::addTeam(const TeamInfo &teamInfo)
{ 
   mTeamInfos->push_back(teamInfo);
}


void Level::addTeam(AbstractTeam *team, S32 index)
{ 
   TNLAssert(mGame, "Expected to have been added to a game by now!");
   mTeamManager.addTeam(team, index);
}


void Level::addTeam(const TeamInfo &teamInfo, S32 index)
{ 
   mTeamInfos->insert(index, teamInfo);
}


AbstractTeam *Level::getTeam(S32 teamIndex) const 
{ 
   return mTeamManager.getTeam(teamIndex);        
}


void Level::setTeamInfosPtr(const boost::shared_ptr<Vector<TeamInfo> > &teamInfos)
{
   mTeamInfos = teamInfos;
}


boost::shared_ptr<Vector<TeamInfo> > Level::getTeamInfosPtr() const
{
   return mTeamInfos;
}


void Level::setTeamHasFlag(S32 teamIndex, bool hasFlag)
{
   TNLAssert(mGame, "Expected to have been added to a game by now!");
   mTeamManager.setTeamHasFlag(teamIndex, hasFlag);
}


bool Level::getTeamHasFlag(S32 teamIndex) const 
{ 
   TNLAssert(mGame, "Expected to have been added to a game by now!");
   return mTeamManager.getTeamHasFlag(teamIndex); 
}


// Perhaps unused
void Level::replaceTeam(AbstractTeam *team, S32 index)
{ 
   TNLAssert(mGame, "Expected to have been added to a game by now!");
   mTeamManager.replaceTeam(team, index);
}


void Level::clearTeams()
{ 
   if(mGame)
      return mTeamManager.clearTeams();
  else
      mTeamInfos->clear();
}


const TeamInfo &Level::getTeamInfo(S32 index) const
{
   return mTeamInfos->get(index);
}


string Level::getTeamLevelCode(S32 index) const
{
   if(mGame)
      return mTeamManager.getTeam(index)->toLevelCode();
   else
      return mTeamInfos->get(index).toLevelCode();
}


// Process a single line of a level file, loaded in gameLoader.cpp
// argc is the number of parameters on the line, argv is the params themselves
// Returns true if everything went ok, sets errorMsg and returns false if there was a problem.
bool Level::processLevelLoadLine(U32 argc, S32 id, const char **argv, string &errorMsg)
{
   if(argc == 0 || !strcmp(argv[0], "#"))    // Blank line or a comment
      return true;

   S32 strlenCmd = (S32) strlen(argv[0]);

   // This is a legacy from the old Zap! days... we do bots differently in Bitfighter, so we'll just ignore this line if we find it.
   if(stricmp(argv[0], "BotsPerTeam") == 0)
      return true;

   // LevelFormat was introduced in 019 to handle significant file format changes, like
   // with GridSize removal and the saving of real spacial coordinates.
   //
   // If it is present, this should be the first line of the file.
   else if(stricmp(argv[0], "LevelFormat") == 0)
   {
      S32 ver;

      if(argc < 2)
      {
         errorMsg = "Invalid LevelFormat parameter -- must specify version!";
         return false;
      }
      else if(mVersion > 0)
      {
         errorMsg = "Duplicate LevelFormat parameter -- ignoring!";
         return false;
      }
      
      ver = atoi(argv[1]);

      if(ver < 1)
      {
         errorMsg = "Invalid LevelFormat parameter -- must be greater than 0!";
         return false;
      }

      mVersion = (U32)ver;

      return true;
   }

   // Legacy Gridsize handling - levels used to have a 'GridSize' line that could be used to
   // multiply all points found in the level file.  Since version 019 this is no longer used
   // and all points are saved as the real spacial coordinates.
   //
   // If a level file contains this setting, we will use it to multiply all points found in
   // the level file.  However, once it is loaded and resaved in the editor, this setting will
   // disappear and all points will reflect their true, absolute nature.
   else if(!stricmp(argv[0], "GridSize"))
   {
      // We should have properly detected the level format by the time GridSize is found
      if(mVersion == 1)
      {
         if(argc < 2)
         {
            errorMsg = "Improperly formed GridSize parameter";
            return false;
         }
         else
         {
            mLegacyGridSize = (F32)atof(argv[1]);
            return true;
         }
      }
      else
         errorMsg = "Warning: GridSize should no longer be used in level files";
         return true;
   }

   else if(stricmp(argv[0], "LevelDatabaseId") == 0)
   {
      U32 id = 0;

      if(argc >= 2)
         id = atoi(argv[1]);

      if(id == 0)
      {
         errorMsg = "Invalid LevelDatabaseId specified";
         return false;
      }
      
      mDatabaseId = id;

      return true;
   }

   // Parse GameType line... All game types are of form XXXXGameType
   else if(strlenCmd >= 8 && strcmp(argv[0] + strlenCmd - 8, "GameType") == 0)
   {
      // First check to see if we have a LevelFormat line, which should have been detected
      // by now since it's the first line of the file.  If it didn't find it, we are at
      // version 1 and we have to set the old GridSize to 255 as default
      //
      // This check is performed here because every file should have a GameType...  right??
      if(mVersion == 0)
      {
         mVersion = 1;
         mLegacyGridSize = 255;     // Default -- will probably be overritten by level file
      }

      if(mGameType.isValid())
      {
         errorMsg = "Duplicate GameType is not allowed";
         return false;
      }

      // validateGameType() will return a valid GameType string -- either what's passed in, or the default if something bogus was specified
      TNL::Object *theObject = TNL::Object::create(GameType::validateGameType(argv[0]).c_str());

      GameType *gt = dynamic_cast<GameType *>(theObject);
      if(gt)
      {
         bool ok = gt->processArguments(argc - 1, argv + 1, NULL);
         if(!ok)
         {
            errorMsg = "GameType has invalid parameters";
            return false;
         }

         setGameType(gt);
         return true;
      }

      errorMsg = "Could not create a GameType";
      return false;
   }

   // Handle various game parameters and team configurations
   if(mGameType && processLevelParam(argc, argv)) 
   {
      // Do nothing here -- all the action is in the if statement
   }

   // We will not add Walls directly to the database, because they are handled differently on the client, server, and editor.
   // This special handling will occur when the level is loaded into the ServerGame/Editor.
   //
   // On client and server, we will be adding Barriers in lieu of WallItems to the database.  Barriers are simple polygon items,
   // either consisting of a single wall segment that has been "puffed out" and rectangularized, or a single PolyWall polygon.
   // Additionally, on the server, we need a list of full-on walls that we can send to the client; the client will manufacture their
   // own Barriers.  WallItems are more efficient to send because we only need worry about vertex list and width, whereas Barriers
   // require 4 points per segment.
   //
   // In the editor, we want WallItems directly in the database so they can be more easily manipulated by the user.
   else if(stricmp(argv[0], "BarrierMaker") == 0)
   {
      WallItem *wallItem = new WallItem();  

      if(!wallItem->processArguments(argc, argv, this))
      {
         errorMsg = "Invalid BarierMaker definition!";
         delete wallItem;
         return false;
      }

      wallItem->setUserAssignedId(id, false);
      addWallItem(wallItem);
   }

   else if(stricmp(argv[0], "Robot") == 0)
   {
      // Save these for later -- we'll use them to build a robot army when the game starts
      mRobotLines.push_back(getString(argc, argv));      // getString strips off "Robot" prefix
   }
   
   // Handle regular game objects
   else
   {
      string objName;

      // Convert any NexusFlagItem into FlagItem, only NexusFlagItem will show up on ship
      if(stricmp(argv[0], "HuntersFlagItem") == 0 || stricmp(argv[0], "NexusFlagItem") == 0)
         objName = "FlagItem";

      // Convert legacy Hunters* objects
      else if(stricmp(argv[0], "HuntersNexusObject") == 0 || stricmp(argv[0], "NexusObject") == 0)
         objName = "NexusZone";

      else if(stricmp(argv[0], "BarrierMakerS") == 0)
         objName = "PolyWall";

      else
         objName = argv[0];


      // Must have a GameType at this point, if not, we will add one to prevent problems loading a level with missing GameType
      if(!mGameType)   
         mGameType.set(new GameType(this));    // Cleaned up... where, exactly?

      TNL::Object *obj = TNL::Object::create(objName.c_str());    // Create an object of the type specified on the line

      SafePtr<BfObject> object = dynamic_cast<BfObject *>(obj);   // Force our new object to be a BfObject

      if(object.isNull())    // Well... that was a bad idea!
      {
         errorMsg = "Unknown object type '" + objName + "'!";
         delete obj;
         return false;
      }

      // Object was valid... carry on!
      bool validArgs = object->processArguments(argc - 1, argv + 1, this);

      // processArguments() might delete this object (this happens with multi-dest teleporters), so isNull() could be true
      // even when the object is entirely legit
      if(object.isNull())
         return false;

      if(!validArgs)
      {
         errorMsg = "Invalid arguments for object '" + objName + "'!";
         delete obj;
         return false;
      }

      object->setUserAssignedId(id, false);
      object->setExtent(object->calcExtents());    // This looks ugly
      addToDatabase(object);

      if(stricmp(argv[0], "Polywall") == 0)
         object->onGeomChanged();
   }

   return true;
}


void Level::addWallItem(WallItem *wallItem, Game *game)
{
   if(!game)
      game = mGame;

   wallItem->addToDatabase(this);
   wallItem->onGeomChanged(); 


   // Normally we won't yet be in a game; but if we are, then we have a little more work to do
   if(game)
   {
      game->addWallItem(wallItem, this);
   }
}


// Returns true if we've handled the line (even if it handling it means that the line was bogus); 
// returns false if the line was not recognized.
bool Level::processLevelParam(S32 argc, const char **argv)
{
   if(stricmp(argv[0], "Team") == 0)
   {
      if(mTeamInfos->size() >= Game::MAX_TEAMS)     
         logprintf(LogConsumer::LogLevelError, "Level has too many teams: you can only have %d!", Game::MAX_TEAMS);
      else
      {
         TeamInfo teamInfo;
         if(teamInfo.processArguments(argc, argv))
            addTeam(teamInfo);
         else
            logprintf(LogConsumer::LogLevelError, "Invalid Team delcaration in level file: Team %s", getString(argc, argv).c_str());
      }
   }

   // TODO: Create better way to change team details from level scripts: https://code.google.com/p/bitfighter/issues/detail?id=106
   // For level script. Could be removed when there is a better way to change team names and colors.
   // Is this even used anywhere?!?
   else if(stricmp(argv[0], "TeamChange") == 0)   
   {
      if(argc >= 2)   // Enough arguments?
         mTeamChangeLines.push_back(getString(argc, argv)); // getString strips off "TeamChange" prefix
      else
         logprintf(LogConsumer::LogLevelError, "TeamChange parameter did not have enough arguments!");
   }

   else if(stricmp(argv[0], "Specials") == 0)
   {         
      for(S32 i = 1; i < argc; i++)
         if(!mGameType->processSpecialsParam(argv[i]))
            logprintf(LogConsumer::LogLevelError, "Invalid specials parameter: %s", argv[i]);
   }

   else if(stricmp(argv[0], "Script") == 0)
   {
      Vector<string> args;

      // argv[0] is always "Script"
      for(S32 i = 1; i < argc; i++)
         args.push_back(argv[i]);

      mGameType->setScript(args);
   }

   else if(stricmp(argv[0], "LevelName") == 0)
   {
      string s = getString(argc, argv);
      mGameType->setLevelName(s.substr(0, MAX_GAME_NAME_LEN).c_str());
   }
   
   else if(stricmp(argv[0], "LevelDescription") == 0)
   {
      string s = getString(argc, argv);
      mGameType->setLevelDescription(s.substr(0, MAX_GAME_DESCR_LEN));
   }

   else if(stricmp(argv[0], "LevelCredits") == 0)
   {
      string s = getString(argc, argv);
      mGameType->setLevelCredits(s.substr(0, MAX_GAME_DESCR_LEN).c_str());
   }

   else if(stricmp(argv[0], "MinPlayers") == 0)     // Recommend a min number of players for this map
   {
      if(argc > 1)
         mGameType->setMinRecPlayers(atoi(argv[1]));
   }

   else if(stricmp(argv[0], "MaxPlayers") == 0)     // Recommend a max number of players for this map
   {
      if(argc > 1)
         mGameType->setMaxRecPlayers(atoi(argv[1]));
   }

   else
      return false;     // Line not processed; perhaps the caller can handle it?

   return true;         // Line processed; caller can ignore it
}


S32 Level::getBotCount() const
{
   TNLAssert(mGame, "Expected to have been added to a game by now!");
   return mTeamManager.getBotCount();
}



}
