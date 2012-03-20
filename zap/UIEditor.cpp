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

#include "UIEditor.h"
#include "UIEditorMenus.h"    // For access to menu methods such as setObject
#include "EditorObject.h"

#include "Cursor.h"          // For various editor cursor

#include "UINameEntry.h"
#include "UIEditorInstructions.h"
#include "UIChat.h"
#include "UIDiagnostics.h"
#include "UITeamDefMenu.h"
#include "UIGameParameters.h"
#include "UIErrorMessage.h"
#include "UIYesNo.h"
#include "gameObjectRender.h"
#include "ClientGame.h"  
#include "gameType.h"
#include "soccerGame.h"          // For Soccer ball radius
#include "NexusGame.h"           // For NexusObject def
#include "CoreGame.h"            // For CoreItem def
#include "EngineeredItem.h"      // For Turret properties
#include "PickupItem.h"          // For RepairItem
#include "barrier.h"             // For DEFAULT_BARRIER_WIDTH
#include "item.h"                // For Asteroid defs
#include "teleporter.h"          // For Teleporter def
#include "speedZone.h"           // For Speedzone def
#include "loadoutZone.h"         // For LoadoutZone def
#include "config.h"
#include "goalZone.h"

#include "gameLoader.h"          // For LevelLoadException def

#include "Colors.h"
#include "GeomUtils.h"
#include "textItem.h"            // For MAX_TEXTITEM_LEN and MAX_TEXT_SIZE
#include "luaLevelGenerator.h"
#include "stringUtils.h"

#include "oglconsole.h"          // Our console object
#include "ScreenInfo.h"
#include "VideoSystem.h"

#include "SDL.h"
#include "SDL_opengl.h"

#include <boost/shared_ptr.hpp>

#include <ctype.h>
#include <exception>
#include <algorithm>             // For sort
#include <math.h>

using namespace boost;

namespace Zap
{

const S32 DOCK_WIDTH = 50;          // Width of dock, in pixels
const F32 MIN_SCALE = .05f;         // Most zoomed-in scale
const F32 MAX_SCALE = 2.5;          // Most zoomed-out scale
const F32 STARTING_SCALE = 0.5;

static EditorObjectDatabase *mLoadTarget;

// statics
Vector<string> EditorUserInterface::robots;        // List of robot lines in the level file

enum EntryMode {
   EntryID,          // Entering an objectID
   EntryAngle,       // Entering an angle
   EntryScale,       // Entering a scale
   EntryNone         // Not in a special entry mode
};


static EntryMode entryMode;
static Vector<ZoneBorder> zoneBorders;

static void saveLevelCallback(ClientGame *game)
{
   UIManager *uiManager = game->getUIManager();

   if(uiManager->getEditorUserInterface()->saveLevel(true, true))
      uiManager->reactivateMenu(uiManager->getMainMenuUserInterface());   
   else
      uiManager->getEditorUserInterface()->reactivate();
}


void backToMainMenuCallback(ClientGame *game)
{
   game->getUIManager()->getEditorUserInterface()->onQuitted();
   game->getUIManager()->reactivateMenu(game->getUIManager()->getMainMenuUserInterface());    
}


// Constructor
EditorUserInterface::EditorUserInterface(ClientGame *game) : Parent(game)
{
   setMenuID(EditorUI);

   mWasTesting = false;

   mSnapObject = NULL;
   mSnapVertexIndex = NONE;
   mHitItem = NULL;
   mHitVertex = NONE;
   mDockItemHit = NULL;
   mEdgeHit = NONE;

   mEditorDatabase = boost::shared_ptr<EditorObjectDatabase>(new EditorObjectDatabase());

   setNeedToSave(false);

   mNewItem = NULL;

   mTeamManager = new TeamManager;

   mLastUndoStateWasBarrierWidthChange = false;

   mUndoItems.resize(UNDO_STATES);     // Create slots for all our undos... also creates a ton of empty dbs.  Maybe we should be using pointers?
   mAutoScrollWithMouse = false;
   mAutoScrollWithMouseReady = false;
}


EditorObjectDatabase *EditorUserInterface::getDatabase()  
{ 
   return mEditorDatabase.get();
}  


void EditorUserInterface::setDatabase(boost::shared_ptr<EditorObjectDatabase> database)
{
   TNLAssert(database.get(), "Database should not be NULL!");
   mEditorDatabase = boost::dynamic_pointer_cast<EditorObjectDatabase>(database);
}



// Really quitting... no going back!
void EditorUserInterface::onQuitted()
{
   clearUndoHistory();     // Clear up a little memory
   mDockItems.clear();     // Free a little more -- dock will be rebuilt when editor restarts
}


void EditorUserInterface::addDockObject(EditorObject *object, F32 xPos, F32 yPos)
{
   object->prepareForDock(getGame(), Point(xPos, yPos), mCurrentTeam);     // Prepare object   
   mDockItems.push_back(boost::shared_ptr<EditorObject>(object));          // Add item to our list of dock objects
}


void EditorUserInterface::populateDock()
{
   mDockItems.clear();

   F32 xPos = (F32)gScreenInfo.getGameCanvasWidth() - horizMargin - DOCK_WIDTH / 2;
   F32 yPos = 35;
   const F32 spacer = 35;

   addDockObject(new RepairItem(), xPos, yPos);
   //addDockObject(new ItemEnergy(), xPos + 10, yPos);
   yPos += spacer;

   addDockObject(new Spawn(), xPos, yPos);
   yPos += spacer;

   addDockObject(new ForceFieldProjector(), xPos, yPos);
   yPos += spacer;

   addDockObject(new Turret(), xPos, yPos);
   yPos += spacer;

   addDockObject(new Teleporter(), xPos, yPos);
   yPos += spacer;

   addDockObject(new SpeedZone(), xPos, yPos);
   yPos += spacer;

   addDockObject(new TextItem(), xPos, yPos);
   yPos += spacer;

   if(getGame()->getGameType()->getGameTypeId() == SoccerGame)
      addDockObject(new SoccerBallItem(), xPos, yPos);
   else if(getGame()->getGameType()->getGameTypeId() == CoreGame)
      addDockObject(new CoreItem(), xPos, yPos);
   else
      addDockObject(new FlagItem(), xPos, yPos);
   yPos += spacer;

   addDockObject(new FlagSpawn(), xPos, yPos);
   yPos += spacer;

   addDockObject(new Mine(), xPos - 10, yPos);
   addDockObject(new SpyBug(), xPos + 10, yPos);
   yPos += spacer;

   // These two will share a line
   addDockObject(new Asteroid(), xPos - 10, yPos);
   addDockObject(new AsteroidSpawn(), xPos + 10, yPos);
   yPos += spacer;

   //addDockObject(new CircleSpawn(), xPos - 10, yPos);
//   addDockObject(new Core(), xPos /*+ 10*/, yPos);
//   yPos += spacer;


   // These two will share a line
   addDockObject(new TestItem(), xPos - 10, yPos);
   addDockObject(new ResourceItem(), xPos + 10, yPos);
   yPos += 25;

      
   addDockObject(new LoadoutZone(), xPos, yPos);
   yPos += 25;

   if(getGame()->getGameType()->getGameTypeId() == NexusGame)
   {
      addDockObject(new NexusObject(), xPos, yPos);
      yPos += 25;
   }
   else
   {
      addDockObject(new GoalZone(), xPos, yPos);
      yPos += 25;
   }

   addDockObject(new PolyWall(), xPos, yPos);
   yPos += spacer;
}


// Destructor -- unwind things in an orderly fashion
EditorUserInterface::~EditorUserInterface()
{
   clearDatabase(getDatabase());

   mDockItems.clear();
   mLevelGenDatabase.removeEverythingFromDatabase();
   mClipboard.clear();

   delete mNewItem;
   delete mTeamManager;
}


// Removes most recent undo state from stack --> won't actually delete items on stack until we need the slot, or we quit
void EditorUserInterface::deleteUndoState()
{
   mLastUndoIndex--;
   mLastRedoIndex--; 
}


// Save the current state of the editor objects for later undoing
void EditorUserInterface::saveUndoState(bool forceSelectionOfTargetObject)
{
   // Use case: We do 5 actions, save, undo 2, redo 1, then do some new action.  
   // Our "no need to save" undo point is lost forever.
   if(mAllUndoneUndoLevel > mLastRedoIndex)     
      mAllUndoneUndoLevel = NONE;


   // Select item so when we undo, it will be selected, which looks better
   bool unselHitItem = false;
   if(forceSelectionOfTargetObject && mHitItem && !mHitItem->isSelected())
   {
      mHitItem->setSelected(true);
      unselHitItem = true;
   }


   EditorObjectDatabase *eod = getDatabase();
   TNLAssert(eod, "I expected to have a valid EditorObjectDatabase here!");

   EditorObjectDatabase *newDB = eod;
   //eod->dumpObjects();     

   mUndoItems[mLastUndoIndex % UNDO_STATES] = boost::shared_ptr<EditorObjectDatabase>(new EditorObjectDatabase(*newDB));  // Make a copy

   //mUndoItems[mLastUndoIndex % UNDO_STATES]->dumpObjects();

   mLastUndoIndex++;
   mLastRedoIndex = mLastUndoIndex;

   if(mLastUndoIndex % UNDO_STATES == mFirstUndoIndex % UNDO_STATES)           // Undo buffer now full...
   {
      mFirstUndoIndex++;
      mAllUndoneUndoLevel -= 1;     // If this falls below 0, then we can't undo our way out of needing to save
   }
   
   setNeedToSave(mAllUndoneUndoLevel != mLastUndoIndex);
   mRedoingAnUndo = false;
   mLastUndoStateWasBarrierWidthChange = false;

   if(unselHitItem)
      mHitItem->setSelected(false);
}


// Remove and discard the most recently saved undo state 
void EditorUserInterface::removeUndoState()
{
   mLastUndoIndex--;
   mLastRedoIndex = mLastUndoIndex;

   if(mLastUndoIndex % UNDO_STATES == mFirstUndoIndex % UNDO_STATES)           // Undo buffer now full...
   {
      mFirstUndoIndex++;
      mAllUndoneUndoLevel -= 1;     // If this falls below 0, then we can't undo our way out of needing to save
   }
   
   setNeedToSave(mAllUndoneUndoLevel != mLastUndoIndex);
}


void EditorUserInterface::autoSave()
{
   saveLevel(false, false, true);
}


void EditorUserInterface::undo(bool addToRedoStack)
{
   if(!undoAvailable())
      return;

   mSnapObject = NULL;
   mSnapVertexIndex = NONE;

   if(mLastUndoIndex == mLastRedoIndex && !mRedoingAnUndo)
   {
      saveUndoState();
      mLastUndoIndex--;
      mLastRedoIndex--;
      mRedoingAnUndo = true;
   }

   mLastUndoIndex--;

   setDatabase(mUndoItems[mLastUndoIndex % UNDO_STATES]);
   EditorObjectDatabase *database = getDatabase();
   mLoadTarget = database;

   rebuildEverything(database);    // Well, rebuild segments from walls at least

   onSelectionChanged();

   mLastUndoStateWasBarrierWidthChange = false;
   validateLevel();
}
   

void EditorUserInterface::redo()
{
   if(mLastRedoIndex != mLastUndoIndex)      // If there's a redo state available...
   {
      mSnapObject = NULL;
      mSnapVertexIndex = NONE;

      mLastUndoIndex++;

      // Perform a little trick here... if we're redoing, and it's our final step of the redo tree, and there's only one item selected,
      // we'll make sure that same item is selected when we're finished.  That will help keep the focus on the item that's being modified
      // during the redo step, and make the redo feel more natural.
      // Act I:
      S32 selectedItem = NONE;

      const Vector<EditorObject *> *objList = getDatabase()->getObjectList();
      if(mLastRedoIndex == mLastUndoIndex && getItemSelectedCount() == 1)
         for(S32 i = 0; i < objList->size(); i++)
            if(objList->get(i)->isSelected())
            {
               selectedItem = objList->get(i)->getSerialNumber();
               break;
            }

      setDatabase(mUndoItems[mLastUndoIndex % UNDO_STATES]);
      EditorObjectDatabase *database = mUndoItems[mLastUndoIndex % UNDO_STATES].get();
      mLoadTarget = database;

      // Act II:
      if(selectedItem != NONE)
      {
         const Vector<EditorObject *> *objList = getDatabase()->getObjectList();
         clearSelection(getDatabase());
         for(S32 i = 0; i < objList->size(); i++)
            if(objList->get(i)->getSerialNumber() == selectedItem)
            {
               objList->get(i)->setSelected(true);
               break;
            }
      }


      TNLAssert(mUndoItems[mLastUndoIndex % UNDO_STATES], "null!");

      rebuildEverything(database);   // Needed?  Yes, for now, but theoretically no, because we should be restoring everything fully reconstituted...
      onSelectionChanged();
      validateLevel();

      onMouseMoved();               // If anything gets undeleted under the mouse, make sure it's highlighted
   }
}


void EditorUserInterface::rebuildEverything(EditorObjectDatabase *database)
{
   database->getWallSegmentManager()->recomputeAllWallGeometry(database);
   resnapAllEngineeredItems(database);

   // If we're rebuilding items in our levelgen database, no need to save anything!
   if(database != &mLevelGenDatabase)
   {
      setNeedToSave(mAllUndoneUndoLevel != mLastUndoIndex);
      autoSave();
   }
}


// Resnaps all engineered items in database
void EditorUserInterface::resnapAllEngineeredItems(EditorObjectDatabase *database)
{
   fillVector.clear();
   database->findObjects((TestFunc)isEngineeredType, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      EngineeredItem *engrObj = dynamic_cast<EngineeredItem *>(fillVector[i]);
      engrObj->mountToWall(engrObj->getPos(), database->getWallSegmentManager());
   }
}


bool EditorUserInterface::undoAvailable()
{
   return mLastUndoIndex - mFirstUndoIndex != 1;
}


// Wipe undo/redo history
void EditorUserInterface::clearUndoHistory()
{
   mFirstUndoIndex = 0;
   mLastUndoIndex = 1;
   mLastRedoIndex = 1;
   mRedoingAnUndo = false;
}


extern TeamPreset gTeamPresets[];

void EditorUserInterface::setLevelFileName(string name)
{
   if(name == "")
      mEditFileName = "";
   else
      if(name.find('.') == string::npos)      // Append extension, if one is needed
         mEditFileName = name + ".level";
      // else... what?
}


void EditorUserInterface::makeSureThereIsAtLeastOneTeam()
{
   if(getTeamCount() == 0)
   {
      EditorTeam *team = new EditorTeam;
      team->setName(gTeamPresets[0].name);
      team->setColor(gTeamPresets[0].r, gTeamPresets[0].g, gTeamPresets[0].b);

      getGame()->addTeam(team);
   }
}


extern S32 gMaxPolygonPoints;

// Loads a level
void EditorUserInterface::loadLevel()
{
   ClientGame *game = getGame();

   // Initialize
   mLoadTarget = getDatabase();
   clearDatabase(mLoadTarget);
   game->clearTeams();
   mSnapObject = NULL;
   mSnapVertexIndex = NONE;
   mAddingVertex = false;
   clearLevelGenItems();
   mGameTypeArgs.clear();
   robots.clear();

   game->resetLevelInfo();

   GameType *gameType = new GameType;
   gameType->addToGame(game, mLoadTarget);

   FolderManager *folderManager = game->getSettings()->getFolderManager();
   string fileName = joindir(folderManager->levelDir, mEditFileName).c_str();


   // Process level file --> returns true if file found and loaded, false if not (assume it's a new level)
   if(game->loadLevelFromFile(fileName, true, mLoadTarget))   
   {
      // Loaded a level!
      makeSureThereIsAtLeastOneTeam(); // Make sure we at least have one team
      validateTeams();                 // Make sure every item has a valid team
      validateLevel();                 // Check level for errors (like too few spawns)
   }
   else     
   {
      // New level!
      makeSureThereIsAtLeastOneTeam();                               // Make sure we at least have one team, like the man said.
   }

   clearUndoHistory();                 // Clean out undo/redo buffers
   clearSelection(mLoadTarget);        // Nothing starts selected
   setNeedToSave(false);               // Why save when we just loaded?
   mAllUndoneUndoLevel = mLastUndoIndex;
   populateDock();                     // Add game-specific items to the dock

   // Bulk-process new items, walls first
   mLoadTarget->getWallSegmentManager()->recomputeAllWallGeometry(mLoadTarget);
   
   // Snap all engineered items to the closest wall, if one is found
   resnapAllEngineeredItems(mLoadTarget);
}


void EditorUserInterface::clearLevelGenItems()
{
   mLevelGenDatabase.removeEverythingFromDatabase();
}


void EditorUserInterface::copyScriptItemsToEditor()
{
   // Duplicate EditorObject pointer list to avoid unsynchronized loop removal
   Vector<EditorObject*> tempList(*mLevelGenDatabase.getObjectList());

   if(mLevelGenDatabase.getObjectList()->size() == 0)
      return;     // Print error message?

   saveUndoState();

   // We can't call addToEditor immediately because it calls addToGame which will trigger
   // an assert since the levelGen items are already added to the game.  We must therefor
   // remove them from the game first
   for(S32 i = 0; i < tempList.size(); i++)
   {
      EditorObject* obj = tempList[i];
      obj->removeFromGame();
      obj->addToEditor(getGame(), getDatabase());
   }
      
   mLevelGenDatabase.removeEverythingFromDatabase();    // Don't want to delete these objects... we just handed them off to the database!

   rebuildEverything(getDatabase());

   mLastUndoStateWasBarrierWidthChange = false;
}


extern OGLCONSOLE_Console gConsole;

// User has pressed Ctrl+R -- run the levelgen script and insert any resulting items into the editor in a separate database
void EditorUserInterface::runLevelGenScript()
{
   string scriptName = getGame()->getGameType()->getScriptName();

   if(scriptName == "")      // No script included!!
      return;

   OGLCONSOLE_Output(gConsole, "Running script %s\n", getGame()->getGameType()->getScriptLine().c_str());

   const Vector<string> *scriptArgs = getGame()->getGameType()->getScriptArgs();

   clearLevelGenItems();      // Clear out any items from the last run

   FolderManager *folderManager = getGame()->getSettings()->getFolderManager();
   runScript(&mLevelGenDatabase, folderManager, scriptName, *scriptArgs);
}


// Runs an arbitrary lua script.  Command is first item in cmdAndArgs, subsequent items are the args, if any
void EditorUserInterface::runScript(EditorObjectDatabase *database, const FolderManager *folderManager, const string &scriptName, const Vector<string> &args)
{
   string name = folderManager->findLevelGenScript(scriptName);  // Find full name of levelgen script

   if(name == "")
   {
      OGLCONSOLE_Output(gConsole, "Could not find script %s; looked in folders: %s\n", 
                                   scriptName.c_str(), concatenate(folderManager->getScriptFolderList()).c_str());
      return;
   }
   
   // Load the items
   LuaLevelGenerator levelGen(name, folderManager->luaDir, args, getGame()->getGridSize(), database, getGame(), true);

   if(!levelGen.runScript())     // Error reporting handled within
      return;

   // Process new items that need it
   // Walls need processing so that they can render properly
   fillVector.clear();
   database->findObjects((TestFunc)isWallType, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      EditorObject *obj = dynamic_cast<EditorObject *>(fillVector[i]);

      if(obj->getVertCount() < 2)      // Invalid item; delete  --> aren't 1 point walls already excluded, making this check redundant?
         database->removeFromDatabase(obj, obj->getExtent());

      if(obj->getObjectTypeNumber() != PolyWallTypeNumber)
         dynamic_cast<WallItem *>(obj)->processEndPoints();
   }

   rebuildEverything(database);

   // When I came through here in early june, there was nothing else here... shouldn't there be some handling of non-wall objects?  -CE
   // June of what year?  -bbr
   // June 2011 -- obviously this is unfinished business
   // OK OK, We should probably handle other items that have geometry to render, like turrets/etc...
}


static void showPluginError(const ClientGame *game)
{
   Vector<StringTableEntry> messages;
   messages.push_back("This plugin encountered an error configuring its options menu.");
   messages.push_back("It is likely that it has been misconfigured.");
   messages.push_back("");
   messages.push_back("See the Bitfighter logfile or console for details.");

   game->displayMessageBox("Problem With Plugin", "Press any key to return to the editor", messages);
}


// Try to create some sort of uniqeish signature for the plugin
string EditorUserInterface::getPluginSignature()
{
   string key = mPluginRunner->getScriptName();

   if(mPluginMenu)
      for(S32 i = 0; i < mPluginMenu->getMenuItemCount(); i++)
         key += itos(mPluginMenu->getMenuItem(i)->getItemType()) + "-";

   return key;
}


void EditorUserInterface::runPlugin(const FolderManager *folderManager, const string &scriptName, const Vector<string> &args)
{
   string fullName = folderManager->findPlugin(scriptName);     // Find full name of plugin script

   if(fullName == "")
   {
      showCouldNotFindScriptMessage(scriptName);
      return;
   }

   // For the moment, we can treat plugins as levelgens that have a getArgsMenu() function
   LuaLevelGenerator *levelGen = new LuaLevelGenerator(fullName, folderManager->luaDir, args, getGame()->getGridSize(), 
                                                       mLoadTarget, getGame());

   mPluginRunner = boost::shared_ptr<LuaLevelGenerator>(levelGen);

   if(!mPluginRunner->loadScript())       // Loads the script and runs it to get everything loaded into memory.  Does not run main().
   {
      showPluginError(getGame());
      return;
   }

   string title;
   Vector<MenuItem *> menuItems;
   bool error;

   if(!levelGen->runGetArgsMenu(title, menuItems, error))     // Fills menuItems, sets error
   {
      onPluginMenuClosed(Vector<string>());        // No menu items?  Let's run the script directly!
      return;     
   }

   if(error)
   {
      showPluginError(getGame());
      return;
   }

   if(menuItems.size() == 0)                       // No menu items?  Let's run the script directly!
   {
      onPluginMenuClosed(Vector<string>());        // We'll use whatever args we already have
      return;
   }

   // Build a menu from the menuItems returned by the plugin
   mPluginMenu.reset(new PluginMenuUI(getGame(), title));      // Using a smart pointer here, for auto deletion

   for(S32 i = 0; i < menuItems.size(); i++)
      mPluginMenu->addMenuItem(menuItems[i]);

   mPluginMenu->addSaveAndQuitMenuItem("Run plugin", "Saves values and runs plugin");

   mPluginMenu->setMenuCenterPoint(Point(gScreenInfo.getGameCanvasWidth() / 2, gScreenInfo.getGameCanvasHeight() / 2));  

   // Restore previous values, if available
   string key = getPluginSignature();

   if(mPluginMenuValues.count(key) == 1)    // i.e. the key exists; use count to avoid creating new entry if it does not exist
      for(S32 i = 0; i < mPluginMenuValues[key].size(); i++)
         mPluginMenu->getMenuItem(i)->setValue(mPluginMenuValues[key].get(i));

   mPluginMenu->activate();
}


void EditorUserInterface::onPluginMenuClosed(const Vector<string> &args)
{
   TNLAssert(mPluginRunner, "NULL PluginRunner!");
   
   saveUndoState();

   // Save menu values for next time -- using a key that includes both the script name and the type of menu items
   // provides some protection against the script being changed while Bitfighter is running.  Probably not realy
   // necessary, but we can afford it here.
   string key = getPluginSignature();

   mPluginMenuValues[key] = args;

   mPluginRunner->runMain(args);
   rebuildEverything(getDatabase());
}


void EditorUserInterface::showCouldNotFindScriptMessage(const string &scriptName)
{
   string pluginDir = getGame()->getSettings()->getFolderManager()->pluginDir;

   Vector<StringTableEntry> messages;
   messages.push_back("");
   messages.push_back("Could not find the plugin called " + scriptName);
   messages.push_back("I looked in the " + pluginDir + " folder.");
   messages.push_back("");
   messages.push_back("You likely have a typo in the [EditorPlugins]");
   messages.push_back("section of your INI file.");

   getGame()->displayMessageBox("Plugin not Found", "Press any key to return to the editor", messages);
}


void EditorUserInterface::validateLevel()
{
   bool hasError = false;
   mLevelErrorMsgs.clear();
   mLevelWarnings.clear();

   bool foundSoccerBall = false;
   bool foundNexus = false;
   bool foundFlags = false;
   bool foundTeamFlags = false;
   bool foundTeamFlagSpawns = false;
   bool foundNeutralSpawn = false;

   vector<bool> foundSpawn;
   char buf[32];

   string teamList, teams;

   // First, catalog items in level
   S32 teamCount = getTeamCount();
   foundSpawn.resize(teamCount);

   for(S32 i = 0; i < teamCount; i++)      // Initialize vector
      foundSpawn[i] = false;

   GridDatabase *gridDatabase = getDatabase();
      
   fillVector.clear();
   gridDatabase->findObjects(ShipSpawnTypeNumber, fillVector);
   for(S32 i = 0; i < fillVector.size(); i++)
   {
      Spawn *spawn = dynamic_cast<Spawn *>(fillVector[i]);
      S32 team = spawn->getTeam();

      if(team == TEAM_NEUTRAL)
         foundNeutralSpawn = true;
      else if(team >= 0)
         foundSpawn[team] = true;
   }

   fillVector.clear();
   gridDatabase->findObjects(SoccerBallItemTypeNumber, fillVector);
   if(fillVector.size() > 0)
      foundSoccerBall = true;

   fillVector.clear();
   gridDatabase->findObjects(NexusTypeNumber, fillVector);
   if(fillVector.size() > 0)
      foundNexus = true;

   fillVector.clear();
   gridDatabase->findObjects(FlagTypeNumber, fillVector);
   for (S32 i = 0; i < fillVector.size(); i++)
   {
      foundFlags = true;
      FlagItem *flag = dynamic_cast<FlagItem *>(fillVector[i]);
      if(flag->getTeam() >= 0)
      {
         foundTeamFlags = true;
         break;
      }
   }

   fillVector.clear();
   gridDatabase->findObjects(FlagSpawnTypeNumber, fillVector);
   for(S32 i = 0; i < fillVector.size(); i++)
   {
      FlagSpawn *flagSpawn = dynamic_cast<FlagSpawn *>(fillVector[i]);
      if(flagSpawn->getTeam() >= 0)
      {
         foundTeamFlagSpawns = true;
         break;
      }
   }

   // "Unversal errors" -- levelgens can't (yet) change gametype

   GameType *gameType = getGame()->getGameType();

   // Check for soccer ball in a a game other than SoccerGameType. Doesn't crash no more.
   if(foundSoccerBall && gameType->getGameTypeId() != SoccerGame)
      mLevelWarnings.push_back("WARNING: Soccer ball can only be used in soccer game.");

   // Check for the nexus object in a non-hunter game. Does not affect gameplay in non-hunter game.
   if(foundNexus && gameType->getGameTypeId() != NexusGame)
      mLevelWarnings.push_back("WARNING: Nexus object can only be used in Nexus game.");

   // Check for missing nexus object in a hunter game.  This cause mucho dolor!
   if(!foundNexus && gameType->getGameTypeId() == NexusGame)
      mLevelErrorMsgs.push_back("ERROR: Nexus game must have a Nexus.");

   if(foundFlags && !gameType->isFlagGame())
      mLevelWarnings.push_back("WARNING: This game type does not use flags.");
   else if(foundTeamFlags && !gameType->isTeamFlagGame())
      mLevelWarnings.push_back("WARNING: This game type does not use team flags.");

   // Check for team flag spawns on games with no team flags
   if(foundTeamFlagSpawns && !foundTeamFlags)
      mLevelWarnings.push_back("WARNING: Found team flag spawns but no team flags.");

   // Errors that may be corrected by levelgen -- script could add spawns
   // Neutral spawns work for all; if there's one, then that will satisfy our need for spawns for all teams
   if(getGame()->getGameType()->getScriptName() == "" && !foundNeutralSpawn)
   {
      // Make sure each team has a spawn point
      for(S32 i = 0; i < (S32)foundSpawn.size(); i++)
         if(!foundSpawn[i])
         {
            dSprintf(buf, sizeof(buf), "%d", i+1);

            if(!hasError)     // This is our first error
            {
               teams = "team ";
               teamList = buf;
            }
            else
            {
               teams = "teams ";
               teamList += ", ";
               teamList += buf;
            }
            hasError = true;
         }
   }

   if(hasError)     // Compose error message
      mLevelErrorMsgs.push_back("ERROR: Need spawn point for " + teams + teamList);
}


void EditorUserInterface::validateTeams()
{
   validateTeams(getDatabase()->findObjects_fast());
}


// Check that each item has a valid team  (fixes any problems it finds)
void EditorUserInterface::validateTeams(const Vector<DatabaseObject *> *dbObjects)
{
   S32 teams = getTeamCount();

   for(S32 i = 0; i < dbObjects->size(); i++)
   {
      EditorObject *obj = dynamic_cast<EditorObject *>(dbObjects->get(i));
      S32 team = obj->getTeam();

      if(obj->hasTeam() && ((team >= 0 && team < teams) || team == TEAM_NEUTRAL || team == TEAM_HOSTILE))  
         continue;      // This one's OK

      if(team == TEAM_NEUTRAL && obj->canBeNeutral())
         continue;      // This one too

      if(team == TEAM_HOSTILE && obj->canBeHostile())
         continue;      // This one too

      if(obj->hasTeam())
         obj->setTeam(0);               // We know there's at least one team, so there will always be a team 0
      else if(obj->canBeHostile() && !obj->canBeNeutral())
         obj->setTeam(TEAM_HOSTILE); 
      else
         obj->setTeam(TEAM_NEUTRAL);    // We won't consider the case where hasTeam == canBeNeutral == canBeHostile == false
   }
}


// Search through editor objects, to make sure everything still has a valid team.  If not, we'll assign it a default one.
// Note that neutral/hostile items are on team -1/-2, and will be unaffected by this loop or by the number of teams we have.
void EditorUserInterface::teamsHaveChanged()
{
   bool teamsChanged = false;

   if(getTeamCount() != mOldTeams.size())     // Number of teams has changed
      teamsChanged = true;
   else
      for(S32 i = 0; i < getTeamCount(); i++)
      {
         EditorTeam *team = getTeam(i);

         if(mOldTeams[i].color != team->getColor() || mOldTeams[i].name != team->getName().getString()) // Color(s) or names(s) have changed
         {
            teamsChanged = true;
            break;
         }
      }

   if(!teamsChanged)       // Nothing changed, we're done here
      return;

   validateTeams();

   // TODO: I hope we can get rid of this in future... perhaps replace with mDockItems being stored in a database, and pass the database?
   Vector<DatabaseObject *> hackyjunk;
   hackyjunk.resize(mDockItems.size());
   for(S32 i = 0; i < mDockItems.size(); i++)
      hackyjunk[i] = mDockItems[i].get();

   validateTeams(&hackyjunk);

   validateLevel();          // Revalidate level -- if teams have changed, requirements for spawns have too
   setNeedToSave(true);
   autoSave();
   mAllUndoneUndoLevel = -1; // This change can't be undone
}


string EditorUserInterface::getLevelFileName()
{
   return mEditFileName;
}


void EditorUserInterface::onSelectionChanged()
{
   EditorObjectDatabase *database = getDatabase();
   WallSegmentManager *wallSegmentManager = database->getWallSegmentManager();

   wallSegmentManager->clearSelected();
   
   // Update wall segment manager with what's currently selected
   fillVector.clear();
   database->findObjects((TestFunc)isWallType, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      TNLAssert(dynamic_cast<EditorObject *>(fillVector[i]), "Bad cast!");
      EditorObject *editorObject = dynamic_cast<EditorObject *>(fillVector[i]);

      if(editorObject->isSelected())
         wallSegmentManager->setSelected(editorObject->getSerialNumber(), true);
   }

   wallSegmentManager->rebuildSelectedOutline();
}


// Handle console input
// Valid commands: help, run, clear, quit, exit
void processEditorConsoleCommand(OGLCONSOLE_Console console, char *cmdline)
{
   Vector<string> words = parseString(cmdline);
   if(words.size() == 0)
      return;

   string cmd = lcase(words[0]);
   EditorUserInterface *ui = gClientGame->getUIManager()->getEditorUserInterface();

   if(cmd == "quit" || cmd == "exit") 
      OGLCONSOLE_HideConsole();

   else if(cmd == "help" || cmd == "?") 
      OGLCONSOLE_Output(console, "Commands: help; run; clear; quit\n");

   else if(cmd == "run")
   {
      if(words.size() == 1)      // Too few args
         OGLCONSOLE_Output(console, "Usage: run <script_name> {args}\n");
      else
      {
         ui->saveUndoState();
         words.erase(0);         // Get rid of "run", leaving script name and args

         string name = words[0];
         words.erase(0);

         ui->onBeforeRunScriptFromConsole();
         ui->runScript(ui->getDatabase(), gClientGame->getSettings()->getFolderManager(), name, words);
         ui->onAfterRunScriptFromConsole();
      }
   }   

   else if(cmd == "clear")
      ui->clearLevelGenItems();

   else
      OGLCONSOLE_Output(console, "Unknown command: %s\n", cmd.c_str());
}


void EditorUserInterface::onBeforeRunScriptFromConsole()
{
   const Vector<EditorObject *> *objList = getDatabase()->getObjectList();

   // Use selection as a marker -- will have to change in future
   for(S32 i = 0; i < objList->size(); i++)
      objList->get(i)->setSelected(true);
}


void EditorUserInterface::onAfterRunScriptFromConsole()
{
   const Vector<EditorObject *> *objList = getDatabase()->getObjectList();

   // Since all our original objects were marked as selected before the script was run, and since the objects generated by
   // the script are not selected, if we invert the selection, our script items will now be selected.
   for(S32 i = 0; i < objList->size(); i++)
      objList->get(i)->setSelected(!objList->get(i)->isSelected());

   rebuildEverything(getDatabase());
   onSelectionChanged();
}


void EditorUserInterface::onActivate()
{
   FolderManager *folderManager = getGame()->getSettings()->getFolderManager();

   if(folderManager->levelDir == "")      // Never did resolve a leveldir... no editing for you!
   {
      getUIManager()->reactivatePrevUI();     // Must come before the error msg, so it will become the previous UI when that one exits

      ErrorMessageUserInterface *ui = getUIManager()->getErrorMsgUserInterface();
      ui->reset();
      ui->setTitle("HOUSTON, WE HAVE A PROBLEM");
      ui->setMessage(1, "No valid level folder was found..."); 
      ui->setMessage(2, "cannot start the level editor");
      ui->setMessage(4, "Check the LevelDir parameter in your INI file,");
      ui->setMessage(5, "or your command-line parameters to make sure");
      ui->setMessage(6, "you have correctly specified a valid folder.");
      ui->activate();

      return;
   }

   // Check if we have a level name:
   if(getLevelFileName() == "")         // We need to take a detour to get a level name
   {
      // Don't save this menu (false, below).  That way, if the user escapes out, and is returned to the "previous"
      // UI, they will get back to where they were before (prob. the main menu system), not back to here.
      getUIManager()->getLevelNameEntryUserInterface()->activate(false);

      return;
   }

   mLevelErrorMsgs.clear();
   mLevelWarnings.clear();

   mSaveMsgTimer.clear();

   mGameTypeArgs.clear();

   getGame()->setActiveTeamManager(mTeamManager);

   loadLevel();
   setCurrentTeam(0);

   mSnapContext = FULL_SNAPPING;      // Hold [space/shift+space] to temporarily disable snapping

   // Reset display parameters...
   mDragSelecting = false;
   mUp = mDown = mLeft = mRight = mIn = mOut = false;
   mCreatingPoly = false;
   mCreatingPolyline = false;
   mDraggingObjects = false;
   mDraggingDockItem = NULL;
   mCurrentTeam = 0;
   mPreviewMode = false;
   mDragCopying = false;
   mJustInsertedVertex = false;
   entryMode = EntryNone;

   SDL_SetCursor(Cursor::getDefault());

   mSaveMsgTimer = 0;

   OGLCONSOLE_EnterKey(processEditorConsoleCommand);     // Setup callback for processing console commands

   VideoSystem::actualizeScreenMode(true);

   centerView();
}


void EditorUserInterface::renderMasterStatus()
{
   /* Do nothing, don't render this in editor */
}


bool EditorUserInterface::usesEditorScreenMode()
{
   return true;
}


void EditorUserInterface::onReactivate()     // Run when user re-enters the editor after testing, among other things
{
   mDraggingObjects = false;  
   SDL_SetCursor(Cursor::getDefault());

   if(mWasTesting)
   {
      mWasTesting = false;
      mSaveMsgTimer.clear();

      getGame()->setGameType(mEditorGameType); 

      remove("editor.tmp");      // Delete temp file
   }


   getGame()->setActiveTeamManager(mTeamManager);

   if(mCurrentTeam >= getTeamCount())
      mCurrentTeam = 0;

   OGLCONSOLE_EnterKey(processEditorConsoleCommand);     // Restore callback for processing console commands

   if(UserInterface::comingFrom->usesEditorScreenMode() != usesEditorScreenMode())
      VideoSystem::actualizeScreenMode(true);

   mDockItemHit = NULL;
}


S32 EditorUserInterface::getTeamCount()
{
   return getGame()->getTeamCount();
}


EditorTeam *EditorUserInterface::getTeam(S32 teamId)
{
   TNLAssert(dynamic_cast<EditorTeam *>(getGame()->getTeam(teamId)), "Expected a EditorTeam");
   return static_cast<EditorTeam *>(getGame()->getTeam(teamId));
}


void EditorUserInterface::clearTeams()
{
   getGame()->clearTeams();
}


bool EditorUserInterface::getNeedToSave() const
{
   return mNeedToSave;
}


void EditorUserInterface::setNeedToSave(bool needToSave)
{
   mNeedToSave = needToSave;
}


void EditorUserInterface::addTeam(EditorTeam *team)
{
   getGame()->addTeam(team);
}


void EditorUserInterface::addTeam(EditorTeam *team, S32 teamIndex)
{
   getGame()->addTeam(team, teamIndex);
}


void EditorUserInterface::removeTeam(S32 teamIndex)
{
   getGame()->removeTeam(teamIndex);
}


Point EditorUserInterface::convertCanvasToLevelCoord(Point p)
{
   return (p - mCurrentOffset) * (1 / mCurrentScale);
}


Point EditorUserInterface::convertLevelToCanvasCoord(Point p, bool convert)
{
   return convert ? p * mCurrentScale + mCurrentOffset : p;
}


// Called when we shift between windowed and fullscreen mode, after change is made
void EditorUserInterface::onDisplayModeChange()
{
   static S32 previousXSize = -1;
   static S32 previousYSize = -1;

   if(previousXSize != gScreenInfo.getGameCanvasWidth() || previousYSize != gScreenInfo.getGameCanvasHeight())
   {
      // Recenter canvas -- note that canvasWidth may change during displayMode change
      mCurrentOffset.set(mCurrentOffset.x - previousXSize / 2 + gScreenInfo.getGameCanvasWidth() / 2, 
                         mCurrentOffset.y - previousYSize / 2 + gScreenInfo.getGameCanvasHeight() / 2);
   }

   // Need to populate the dock here because dock items are tied to a particular screen x,y; 
   // maybe it would be better to give them a dock x,y instead?
   if(getGame()->getGameType())
      populateDock();               // If game type has changed, items on dock will change

   previousXSize = gScreenInfo.getGameCanvasWidth();
   previousYSize = gScreenInfo.getGameCanvasHeight();
}


Point EditorUserInterface::snapPointToLevelGrid(Point const &p)
{
   if(mSnapContext != FULL_SNAPPING)
      return p;

   // First, find a snap point based on our grid
   F32 factor = (showMinorGridLines() ? 0.1f : 0.5f) * getGame()->getGridSize();     // Tenths or halves -- major gridlines are gridsize pixels apart

   return Point(floor(p.x / factor + 0.5) * factor, floor(p.y / factor + 0.5) * factor);
}


Point EditorUserInterface::snapPoint(EditorObjectDatabase *database, Point const &p, bool snapWhileOnDock)
{
   if(mouseOnDock() && !snapWhileOnDock) 
      return p;      // No snapping!

   const Vector<EditorObject *> *objList = database->getObjectList();

   Point snapPoint(p);

   WallSegmentManager *wallSegmentManager = database->getWallSegmentManager();

   if(mDraggingObjects)
   {
      // Mark all items being dragged as no longer being snapped -- only our primary "focus" item will be snapped
      for(S32 i = 0; i < objList->size(); i++)
         if(objList->get(i)->isSelected())
            objList->get(i)->setSnapped(false);
   
      // Turrets & forcefields: Snap to a wall edge as first (and only) choice, regardless of whether snapping is on or off
      if(isEngineeredType(mSnapObject->getObjectTypeNumber()))
      {
         EngineeredItem *engrObj = dynamic_cast<EngineeredItem *>(mSnapObject);
         return engrObj->mountToWall(snapPointToLevelGrid(p), wallSegmentManager);
      }
   }

   F32 minDist = 255 / mCurrentScale;    // 255 just seems to work well, not related to gridsize; only has an impact when grid is off

   if(mSnapContext == FULL_SNAPPING)     // Only snap to grid when full snapping is enabled; lowest priority snaps go first
   {
      snapPoint = snapPointToLevelGrid(p);
      minDist = snapPoint.distSquared(p);
   }

   if(mSnapContext != NO_SNAPPING)
   {
      // Where will we be snapping things?
      bool snapToWallCorners = getSnapToWallCorners();

      // Now look for other things we might want to snap to
      for(S32 i = 0; i < objList->size(); i++)
      {
         EditorObject *obj = objList->get(i);
         // Don't snap to selected items or items with selected verts (keeps us from snapping to ourselves, which is usually trouble)
         if(obj->isSelected() || obj->anyVertsSelected())    
            continue;

         for(S32 j = 0; j < obj->getVertCount(); j++)
         {
            F32 dist = obj->getVert(j).distSquared(p);
            if(dist < minDist)
            {
               minDist = dist;
               snapPoint.set(obj->getVert(j));
            }
         }
      }

      // Search for a corner to snap to - by using wall edges, we'll also look for intersections between segments
      if(snapToWallCorners)   
         checkCornersForSnap(p, wallSegmentManager->mWallEdges, minDist, snapPoint);
   }

   return snapPoint;
}


bool EditorUserInterface::getSnapToWallCorners()
{
   return mSnapContext != NO_SNAPPING && mDraggingObjects && !(isWallType(mSnapObject->getObjectTypeNumber()));
}


static bool checkPoint(const Point &clickPoint, const Point &point, F32 &minDist, Point &snapPoint)
{
   F32 dist = point.distSquared(clickPoint);
   if(dist < minDist)
   {
      minDist = dist;
      snapPoint = point;
      return true;
   }

   return false;
}


S32 EditorUserInterface::checkCornersForSnap(const Point &clickPoint, const Vector<WallEdge *> &edges, F32 &minDist, Point &snapPoint)
{
   S32 vertFound = NONE;
   const Point *vert;

   for(S32 i = 0; i < edges.size(); i++)
      for(S32 j = 0; j < 1; j++)
      {
         vert = (j == 0) ? edges[i]->getStart() : edges[i]->getEnd();
         if(checkPoint(clickPoint, *vert, minDist, snapPoint))
            vertFound = i;
      }

   return vertFound;
}


////////////////////////////////////
////////////////////////////////////
// Rendering routines

extern Color gErrorMessageTextColor;


bool EditorUserInterface::showMinorGridLines()
{
   return mCurrentScale >= .5;
}


// Render background snap grid
void EditorUserInterface::renderGrid()
{
   F32 snapFadeFact = (mSnapContext == FULL_SNAPPING) ? 1 : 0.5f;

   // Gridlines
   for(S32 i = 1; i >= 0; i--)
   {
      if((i && showMinorGridLines()) || !i)      // First minor then major
      {
         F32 gridScale = mCurrentScale * getGame()->getGridSize() * (i ? 0.1f : 1);    // Major gridlines are gridSize() pixels apart   
         
         F32 xStart = fmod(mCurrentOffset.x, gridScale);
         F32 yStart = fmod(mCurrentOffset.y, gridScale);

         F32 grayVal = ((i ? .2f : .4f) * snapFadeFact);
         glColor(grayVal);

         while(yStart < gScreenInfo.getGameCanvasHeight())
         {
            drawHorizLine(0, (F32)gScreenInfo.getGameCanvasWidth(), yStart);
            yStart += gridScale;
         }
         while(xStart < gScreenInfo.getGameCanvasWidth())
         {
            drawVertLine(xStart, 0, (F32)gScreenInfo.getGameCanvasHeight());
            xStart += gridScale;
         }
      }
   }

   // Draw axes
   glColor(0.7f * snapFadeFact);
   glLineWidth(gLineWidth3);

   Point origin = convertLevelToCanvasCoord(Point(0,0));

   drawHorizLine(0, (F32)gScreenInfo.getGameCanvasWidth(), origin.y);
   drawVertLine(origin.x, 0, (F32)gScreenInfo.getGameCanvasHeight());

   glLineWidth(gDefaultLineWidth);
}


static S32 QSORT_CALLBACK sortByTeam(DatabaseObject **a, DatabaseObject **b)
{
   TNLAssert(dynamic_cast<BfObject *>(*a), "Not a BfObject");
   TNLAssert(dynamic_cast<BfObject *>(*b), "Not a BfObject");
   return ((BfObject *)(*b))->getTeam() - ((BfObject *)(*a))->getTeam();
}


void EditorUserInterface::renderTurretRanges(EditorObjectDatabase *editorDb)
{
   fillVector.clear();
      
   editorDb->findObjects(SpyBugTypeNumber, fillVector);

   if(fillVector.size() != 0)
   {
      // Use Z Buffer to make use of not drawing overlap visible area of same team SpyBug, but does overlap different team
      fillVector.sort(sortByTeam);
      glClear(GL_DEPTH_BUFFER_BIT);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_DEPTH_WRITEMASK);
      glDepthFunc(GL_LESS);
      glPushMatrix();
      glTranslatef(0, 0, -0.95f);

      // This blending works like this, source(SRC) * GL_ONE_MINUS_DST_COLOR + destination(DST) * GL_ONE
      glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE);  

      S32 prevTeam = -10;

      // Draw spybug visibility ranges first, underneath everything else
      for(S32 i = 0; i < fillVector.size(); i++)
      {
         EditorObject *editorObj = dynamic_cast<EditorObject *>(fillVector[i]);

         if(i != 0 && editorObj->getTeam() != prevTeam)
            glTranslatef(0, 0, 0.05f);
         prevTeam = editorObj->getTeam();

         Point pos = editorObj->getPos();
         pos *= mCurrentScale;
         pos += mCurrentOffset;
         renderSpyBugVisibleRange(pos, getTeamColor(editorObj->getTeam()), mCurrentScale);
      }

      setDefaultBlendFunction();

      glPopMatrix();
      glDisable(GL_DEPTH_WRITEMASK);
      glDisable(GL_DEPTH_TEST);
   }

   // Next draw turret firing ranges for selected or highlighted turrets only
   fillVector.clear();
      
   editorDb->findObjects(TurretTypeNumber, fillVector);
   for(S32 i = 0; i < fillVector.size(); i++)
   {
      EditorObject *editorObj = dynamic_cast<EditorObject *>(fillVector[i]);
      if(editorObj->isSelected() || editorObj->isLitUp())
      {
         Point pos = editorObj->getPos();
         pos *= mCurrentScale;
         pos += mCurrentOffset;
         renderTurretFiringRange(pos, getTeamColor(editorObj->getTeam()), mCurrentScale);
      }
   }
}


S32 getDockHeight()
{
   return gScreenInfo.getGameCanvasHeight() - 2 * EditorUserInterface::vertMargin;
}


void EditorUserInterface::renderDock()    
{
   // Render item dock down RHS of screen
   const S32 canvasWidth = gScreenInfo.getGameCanvasWidth();
   const S32 canvasHeight = gScreenInfo.getGameCanvasHeight();

   S32 dockHeight = getDockHeight();

   drawFilledRect(canvasWidth - DOCK_WIDTH - horizMargin, canvasHeight - vertMargin, 
                  canvasWidth - horizMargin,              canvasHeight - vertMargin - dockHeight, 
                  Colors::black, (mouseOnDock() ? Colors::yellow : Colors::white));
}


const S32 PANEL_TEXT_SIZE = 10;
const S32 PANEL_SPACING = S32(PANEL_TEXT_SIZE * 1.3);

void EditorUserInterface::renderInfoPanel() 
{
   const S32 canvasHeight = gScreenInfo.getGameCanvasHeight();

   const S32 panelWidth = 180;
   const S32 panelHeight = 4 * PANEL_SPACING + 9;

   TNLAssert(glIsEnabled(GL_BLEND), "Why is blending off here?");

   drawFilledRect(horizMargin, canvasHeight - vertMargin, 
                  horizMargin + panelWidth, canvasHeight - vertMargin - panelHeight, 
                  Colors::richGreen, .7f, Colors::white);


   // Draw coordinates on panel -- if we're moving an item, show the coords of the snap vertex, otherwise show the coords of the
   // snapped mouse position
   Point pos;

   if(mSnapObject)
      pos = mSnapObject->getVert(mSnapVertexIndex);
   else
      pos = snapPoint(getDatabase(), convertCanvasToLevelCoord(mMousePos));


   glColor(Colors::white);
   renderPanelInfoLine(1, "Cursor X,Y: %2.1f,%2.1f", pos.x, pos.y);

   // And scale
   renderPanelInfoLine(2, "Zoom Scale: %2.2f", mCurrentScale);

   // Show number of teams
   renderPanelInfoLine(3, "Team Count: %d", getTeamCount());

   glColor(mNeedToSave ? Colors::red : Colors::green);     // Color level name by whether it needs to be saved or not

   // Filename without extension
   renderPanelInfoLine(4, "Filename: %s%s", mNeedToSave ? "*" : "", mEditFileName.substr(0, mEditFileName.find_last_of('.')).c_str());
}


void EditorUserInterface::renderPanelInfoLine(S32 line, const char *format, ...)
{
   const S32 XPOS = horizMargin + 4;

   va_list args;
   static char text[512];  // reusable buffer

   va_start(args, format);
   vsnprintf(text, sizeof(text), format, args); 
   va_end(args);

   drawString(XPOS, gScreenInfo.getGameCanvasHeight() - vertMargin - PANEL_TEXT_SIZE - line * PANEL_SPACING + 6, PANEL_TEXT_SIZE, text);
}


// Shows selected item attributes, or, if we're hovering over dock item, shows dock item info string
// This method is a total mess!  TODO: Rewrite
void EditorUserInterface::renderItemInfoPanel()
{
   string text = "";
   string hoverText = "";
   string item = "";
   S32 hitCount = 0;
   string attribs = "";
   bool multipleKindsOfObjectsSelected = false;

   bool dockHit;
   const char *instructs = "";

   if(mDockItemHit)
   {
      dockHit = true;

      item = mDockItemHit->getOnScreenName();
      text = mDockItemHit->getEditorHelpString();
   }
   else
   {
      dockHit = false;

      const Vector<EditorObject *> *objList = getDatabase()->getObjectList();

      for(S32 i = 0; i < objList->size(); i++)
      {
         if(objList->get(i)->isSelected())
         {
            hitCount++;

            if(text != "")
            {
               if(item != objList->get(i)->getOnScreenName())
               {
                  item = "Multiple objects selected";
                  multipleKindsOfObjectsSelected = true;
               }

               text = " ";
               continue;
            }
            else
            {
               item = objList->get(i)->getOnScreenName();
               attribs = objList->get(i)->getAttributeString();
            }

            if(attribs != "")
            {
               text = "Attributes -> " + attribs;
               instructs = objList->get(i)->getInstructionMsg();
            }
            else
               text = " ";
         }

         else if(objList->get(i)->isLitUp())
            hoverText = string("Hover: ") + objList->get(i)->getOnScreenName();
      }
   }


   Color textColor = (dockHit ? Colors::green : Colors::yellow);

   S32 xpos = horizMargin + 4 + 180 + 5;
   S32 upperLineTextSize = 14;
   S32 ypos = gScreenInfo.getGameCanvasHeight() - vertMargin - PANEL_TEXT_SIZE - PANEL_SPACING + 6;

   if(text != "" && text != " ")
   {
      glColor(textColor);
      drawString(xpos, ypos,                 PANEL_TEXT_SIZE, instructs);
      drawString(xpos, ypos - PANEL_SPACING, PANEL_TEXT_SIZE, text.c_str());
   }

   ypos -= PANEL_SPACING + S32(upperLineTextSize * 1.3);
   if(item != "")
   {
      if(!multipleKindsOfObjectsSelected)
         item = (mDraggingObjects ? "Dragging " : "Selected ") + item;

      if(hitCount > 1)
         item += " (" + itos(hitCount) + ")";

      glColor(textColor);
      drawString(xpos, ypos, upperLineTextSize, item.c_str());
   }

   ypos -= S32(upperLineTextSize * 1.3);
   if(hoverText != "" && !dockHit)
   {
      glColor(Colors::white);
      drawString(xpos, ypos, upperLineTextSize, hoverText.c_str());
   }
}


void EditorUserInterface::renderTextEntryOverlay()
{
   // Render id-editing overlay
   if(entryMode != EntryNone)
   {
      static const S32 fontsize = 16;
      static const S32 inset = 9;
      static const S32 boxheight = fontsize + 2 * inset;
      static const Color color(0.9, 0.9, 0.9);
      static const Color errorColor(Colors::red);

      bool errorFound = false;

      // Check for duplicate IDs if we're in ID entry mode
      if(entryMode == EntryID)
      {
         S32 id = atoi(mEntryBox.c_str());      // mEntryBox has digits only filter applied; ids can only be positive ints

         if(id != 0)    // Check for duplicates
         {
            const Vector<EditorObject *> *objList = getDatabase()->getObjectList();

            for(S32 i = 0; i < objList->size(); i++)
               if(objList->get(i)->getItemId() == id && !objList->get(i)->isSelected())
               {
                  errorFound = true;
                  break;
               }
         }
      }

      // Calculate box width
      S32 boxwidth = 2 * inset + getStringWidth(fontsize, mEntryBox.getPrompt().c_str()) + 
          mEntryBox.getMaxLen() * getStringWidth(fontsize, "-") + 25;

      // Render entry box
      TNLAssert(glIsEnabled(GL_BLEND), "Why is blending off here?");

      S32 xpos = (gScreenInfo.getGameCanvasWidth()  - boxwidth) / 2;
      S32 ypos = (gScreenInfo.getGameCanvasHeight() - boxheight) / 2;

      Color boxColor = Color(.3f,.6f,.3f);
      drawFilledRect(xpos, ypos, xpos + boxwidth, ypos + boxheight, boxColor, .85f, boxColor);

      xpos += inset;
      ypos += inset;

      glColor(errorFound ? errorColor : color);
      xpos += drawStringAndGetWidthf(xpos, ypos, fontsize, "%s ", mEntryBox.getPrompt().c_str());
      drawString(xpos, ypos, fontsize, mEntryBox.c_str());

      mEntryBox.drawCursor(xpos, ypos, fontsize);
   }
}


void EditorUserInterface::renderReferenceShip()
{
   // Render ship at cursor to show scale
   static F32 thrusts[4] =  { 1, 0, 0, 0 };

   glPushMatrix();
      glTranslate(mMousePos);
      glScale(mCurrentScale);
      glRotatef(90, 0, 0, 1);
      renderShip(&Colors::red, 1, thrusts, 1, 5, 0, false, false, false, false, false);
      glRotatef(-90, 0, 0, 1);

      // And show how far it can see
      const S32 horizDist = Game::PLAYER_VISUAL_DISTANCE_HORIZONTAL;
      const S32 vertDist = Game::PLAYER_VISUAL_DISTANCE_VERTICAL;

      TNLAssert(glIsEnabled(GL_BLEND), "Why is blending off here?");

      glColor4f(.5, .5, 1, .35f);
      drawFilledRect(-horizDist, -vertDist, horizDist, vertDist);

   glPopMatrix();
}


static F32 getRenderingAlpha(bool isScriptItem)
{
   return isScriptItem ? .4f : 1;     // Script items will appear somewhat translucent
}


void EditorUserInterface::render()
{
   EditorObjectDatabase *editorDb = getDatabase();

   mouseIgnore = false;                // Avoid freezing effect from too many mouseMoved events without a render in between (sam)

   TNLAssert(glIsEnabled(GL_BLEND), "Blending should be on here!");

   // Render bottom-most layer of our display
   if(mPreviewMode)
      renderTurretRanges(editorDb);    // Render range of all turrets in editorDb
   else
      renderGrid();                    


   glPushMatrix();
      glTranslate(getCurrentOffset());
      glScale(getCurrentScale());

      // mSnapDelta only gets recalculated during a dragging event -- if an item is no longer being dragged, we
      // don't want to use the now stale value in mSnapDelta, but rather (0,0) to reflect the rahter obvoius fact
      // that walls that are not being dragged should be rendered in place.
      static Point delta;
      delta = mDraggingObjects ? mSnapDelta : Point(0,0);

      // == Render walls and polyWalls ==
      renderWalls(&mLevelGenDatabase, delta, false, true );
      renderWalls(editorDb, delta, false, false);

      // == Normal, unselected items ==
      // Draw map items (teleporters, etc.) that are not being dragged, and won't have any text labels  (below the dock)
      renderObjects(editorDb, RENDER_UNSELECTED_NONWALLS, false);             // Render our normal objects
      renderObjects(&mLevelGenDatabase, RENDER_UNSELECTED_NONWALLS, true);    // Render any levelgen objects being overlaid

      // == Selected items ==
      // Draw map items (teleporters, etc.) that are are selected and/or lit up, so label is readable (still below the dock)
      // Do this as a separate operation to ensure that these are drawn on top of those drawn above.
      // We do render polywalls here because this is what draws the highlighted outline when the polywall is selected.
      renderObjects(editorDb, RENDER_SELECTED_NONWALLS, false);               // Render selected objects 

      renderWalls(editorDb, delta, true, false);   

      // == Draw geomPolyLine features under construction ==
      if(mCreatingPoly || mCreatingPolyline)    
         renderObjectsUnderConstruction();

      // Since we're not constructing a barrier, if there are any barriers or lineItems selected, 
      // get the width for display at bottom of dock
      else  
      {
         fillVector.clear();
         editorDb->findObjects((TestFunc)isLineItemType, fillVector);

         for(S32 i = 0; i < fillVector.size(); i++)
         {
            LineItem *obj = dynamic_cast<LineItem *>(fillVector[i]);   // Walls are a subclass of LineItem, so this will work for both

            if(obj && (obj->isSelected() || (obj->isLitUp() && obj->isVertexLitUp(NONE))))
               break;
         }
      }

      // Draw map items (teleporters, etc.) that are being dragged  (above the dock).  But don't draw walls here, or
      // we'll lose our wall centernlines.
      //if(mDraggingObjects)
      //   for(S32 i = 0; i < objList->size(); i++)
      //   {
      //      EditorObject *obj = objList->get(i);
      //      if(obj->isSelected() && !isWallType(obj->getObjectTypeNumber()))    // Object is selected and is not a wall
      //         obj->renderInEditor(mCurrentScale, mSnapVertexIndex, false, mPreviewMode);
      //   }

      // Render our snap vertex as a hollow magenta box...
      if(!mPreviewMode && mSnapObject && mSnapObject->isSelected() && mSnapVertexIndex != NONE &&        // ...but not in preview mode...
         mSnapObject->getGeomType() != geomPoint &&                                                      // ...and not on point objects...
         !mSnapObject->isVertexLitUp(mSnapVertexIndex) && !mSnapObject->vertSelected(mSnapVertexIndex))  // ...or selected vertices
      {
         renderVertex(SnappingVertex, mSnapObject->getVert(mSnapVertexIndex), NO_NUMBER, mCurrentScale/*, alpha*/);  
      }

    glPopMatrix(); 


   if(mPreviewMode)
      renderReferenceShip();
   else
   {
      // The following items are hidden in preview mode:
      renderDock();
      renderDockItems();
      renderInfoPanel();
      renderItemInfoPanel();
   }

   renderDragSelectBox();

   if(mAutoScrollWithMouse)
   {
      glColor(Colors::white);
      drawFourArrows(mScrollWithMouseLocation);
   }

   renderHelpMessage();    // Also highlights dock item we're hovering over
   renderSaveMessage();
   renderWarnings();

   renderTextEntryOverlay();

   renderConsole();        // Rendered last, so it's always on top
}


// Render objects in the specified database
void EditorUserInterface::renderObjects(EditorObjectDatabase *database, RenderModes renderMode, bool isLevelgenOverlay)
{
   const Vector<EditorObject *> *objList = database->getObjectList();

   for(S32 i = 0; i < objList->size(); i++)
   {
      EditorObject *obj = objList->get(i);

      bool isSelected = obj->isSelected() || obj->isLitUp();
      bool isWall = isWallType(obj->getObjectTypeNumber());

      bool wantSelected = (renderMode == RENDER_SELECTED_NONWALLS || renderMode == RENDER_SELECTED_WALLS);
      bool wantWalls = ( renderMode == RENDER_UNSELECTED_WALLS || renderMode == RENDER_SELECTED_WALLS);

      if(isSelected == wantSelected && isWall == wantWalls)     
         obj->renderInEditor(mCurrentScale, mSnapVertexIndex, isLevelgenOverlay, mPreviewMode); // <== wall centerlines rendered in here
   }
}


// Render walls and centerlines
void EditorUserInterface::renderWalls(EditorObjectDatabase *database, const Point &offset, bool selected, bool isLevelGenDatabase)
{
   // Render wall outlines and fill
   database->getWallSegmentManager()->renderWalls(getGame()->getSettings(), mCurrentScale, mDraggingObjects, selected,
                                                  offset, mPreviewMode, getSnapToWallCorners(), getRenderingAlpha(isLevelGenDatabase));

   // Render walls as ordinary objects; this will draw wall centerlines
   if(!isLevelGenDatabase)
      renderObjects(database, selected ? RENDER_SELECTED_WALLS : RENDER_UNSELECTED_WALLS, false);  
}


void EditorUserInterface::renderObjectsUnderConstruction()
{
   // Add a vert (and deleted it later) to help show what this item would look like if the user placed the vert in the current location
   mNewItem->addVert(snapPoint(getDatabase(), convertCanvasToLevelCoord(mMousePos)));
   glLineWidth(gLineWidth3);

   if(mCreatingPoly) // Wall
      glColor(*SELECT_COLOR);
   else              // LineItem
      glColor(getTeamColor(mNewItem->getTeam()));

   renderLine(mNewItem->getOutline());

   glLineWidth(gDefaultLineWidth);

   for(S32 j = mNewItem->getVertCount() - 1; j >= 0; j--)      // Go in reverse order so that placed vertices are drawn atop unplaced ones
   {
      Point v = mNewItem->getVert(j);
            
      // Draw vertices
      if(j == mNewItem->getVertCount() - 1)                    // This is our most current vertex
         renderVertex(HighlightedVertex, v, NO_NUMBER, mCurrentScale);
      else
         renderVertex(SelectedItemVertex, v, j, mCurrentScale);
   }
   mNewItem->deleteVert(mNewItem->getVertCount() - 1); 
}


// Draw box for selecting items
void EditorUserInterface::renderDragSelectBox()
{
   if(!mDragSelecting)   
      return;
   
   glColor(Colors::white);
   Point downPos = convertLevelToCanvasCoord(mMouseDownPos);
   drawHollowRect(downPos.x, downPos.y, mMousePos.x, mMousePos.y);
}


void EditorUserInterface::renderDockItems()
{
   for(S32 i = 0; i < mDockItems.size(); i++)
   {
      mDockItems[i]->renderInEditor(mCurrentScale, mSnapVertexIndex, false, false);
      mDockItems[i]->setLitUp(false);
   }
}


// Render help messages at bottom of screen
void EditorUserInterface::renderHelpMessage()
{
   if(!mouseOnDock() || mPreviewMode)  // Help messages only shown when hovering over dock item, and only when dock is visible
      return;

   if(!mDockItemHit)
      return;

   mDockItemHit->setLitUp(true);       // Will trigger a selection highlight to appear around dock item

   //glColor(Colors::green);

   //// Center string between left side of screen and edge of dock
   //const char *helpString = mDockItemHit->getEditorHelpString();
   //S32 x = (gScreenInfo.getGameCanvasWidth() - horizMargin - DOCK_WIDTH - getStringWidth(15, helpString)) / 2;
   //drawString(x, gScreenInfo.getGameCanvasHeight() - vertMargin - 15, 15, helpString);
}


void EditorUserInterface::renderSaveMessage()
{
   if(mSaveMsgTimer.getCurrent())
   {
      F32 alpha = 1.0;
      if(mSaveMsgTimer.getCurrent() < 1000)
         alpha = (F32) mSaveMsgTimer.getCurrent() / 1000;

      TNLAssert(glIsEnabled(GL_BLEND), "Why is blending off here?");

      glColor(mSaveMsgColor, alpha);
      drawCenteredString(gScreenInfo.getGameCanvasHeight() - vertMargin - 65, 25, mSaveMsg.c_str());
   }
}


void EditorUserInterface::renderWarnings()
{
   if(mWarnMsgTimer.getCurrent())
   {
      F32 alpha = 1.0;
      if (mWarnMsgTimer.getCurrent() < 1000)
         alpha = (F32) mWarnMsgTimer.getCurrent() / 1000;

      TNLAssert(glIsEnabled(GL_BLEND), "Why is blending off here?");

      glColor(mWarnMsgColor, alpha);
      drawCenteredString(gScreenInfo.getGameCanvasHeight() / 4, 25, mWarnMsg1.c_str());
      drawCenteredString(gScreenInfo.getGameCanvasHeight() / 4 + 30, 25, mWarnMsg2.c_str());
   }

   if(mLevelErrorMsgs.size() || mLevelWarnings.size())
   {
      S32 ypos = vertMargin + 50;

      glColor(gErrorMessageTextColor);

      for(S32 i = 0; i < mLevelErrorMsgs.size(); i++)
      {
         drawCenteredString(ypos, 20, mLevelErrorMsgs[i].c_str());
         ypos += 25;
      }

      glColor(Colors::yellow);

      for(S32 i = 0; i < mLevelWarnings.size(); i++)
      {
         drawCenteredString(ypos, 20, mLevelWarnings[i].c_str());
         ypos += 25;
      }
   }
}


const Color *EditorUserInterface::getTeamColor(S32 team)
{
   return getGame()->getTeamColor(team);
}


////////////////////////////////////////
////////////////////////////////////////
/*
1. User creates wall by drawing line
2. Each line segment is converted to a series of endpoints, who's location is adjusted to improve rendering (extended)
3. Those endpoints are used to generate a series of WallSegment objects, each with 4 corners
4. Those corners are used to generate a series of edges on each WallSegment.  Initially, each segment has 4 corners
   and 4 edges.
5. Segments are intsersected with one another, punching "holes", and creating a series of shorter edges that represent
   the dark blue outlines seen in the game and in the editor.

If wall shape or location is changed steps 1-5 need to be repeated
If intersecting wall is changed, only steps 4 and 5 need to be repeated
If wall thickness is changed, steps 3-5 need to be repeated
*/


// TODO: Move this method to EditorObjectDatabase!!
// Mark all objects in database as unselected
void EditorUserInterface::clearSelection(EditorObjectDatabase *database)
{
   const Vector<EditorObject *> *objList = database->getObjectList();

   for(S32 i = 0; i < objList->size(); i++)
      objList->get(i)->unselect();
}


// TODO: Move this method to EditorObjectDatabase!!
// Mark everything as selected
void EditorUserInterface::selectAll(EditorObjectDatabase *database)
{
   const Vector<EditorObject *> *objList = database->getObjectList();

   for(S32 i = 0; i < objList->size(); i++)
      objList->get(i)->setSelected(true);
}



// TODO: Move this method to EditorObjectDatabase!!
bool EditorUserInterface::anyItemsSelected(EditorObjectDatabase *database)
{
   const Vector<EditorObject *> *objList = database->getObjectList();

   for(S32 i = 0; i < objList->size(); i++)
      if(objList->get(i)->isSelected())
         return true;

   return false;
}


// TODO: Move this method to GridDatabase!!
// Find all objects in bounds, populate points passed in min & max
void EditorUserInterface::computeSelectionMinMax(EditorObjectDatabase *database, Point &min, Point &max)
{
   min.set(F32_MAX, F32_MAX);
   max.set(-F32_MAX, -F32_MAX);

   const Vector<EditorObject *> *objList = database->getObjectList();

   for(S32 i = 0; i < objList->size(); i++)
   {
      EditorObject *obj = objList->get(i);

      if(obj->isSelected())
      {
         for(S32 j = 0; j < obj->getVertCount(); j++)
         {
            Point v = obj->getVert(j);

            if(v.x < min.x)   min.x = v.x;
            if(v.x > max.x)   max.x = v.x;
            if(v.y < min.y)   min.y = v.y;
            if(v.y > max.y)   max.y = v.y;
         }
      }
   }
}


// TODO: Move to GridDatabase
void EditorUserInterface::clearDatabase(GridDatabase *database)
{
   fillVector.clear();
   database->findObjects(fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
      database->removeFromDatabase(fillVector[i], fillVector[i]->getExtent());
}


// Copy selection to the clipboard
void EditorUserInterface::copySelection()
{
   EditorObjectDatabase *database = getDatabase();

   if(!anyItemsSelected(database))
      return;

   mClipboard.clear();     

   const Vector<EditorObject *> *objList = database->getObjectList();

   for(S32 i = 0; i < objList->size(); i++)
      if(objList->get(i)->isSelected())
         mClipboard.push_back(boost::shared_ptr<EditorObject>(objList->get(i)->copy()));
}


// Paste items on the clipboard
void EditorUserInterface::pasteSelection()
{
   EditorObjectDatabase *database = getDatabase();

   if(mDraggingObjects)    // Pasting while dragging can cause crashes!!
      return;

   S32 objCount = mClipboard.size();

    if(objCount == 0)         // Nothing on clipboard, nothing to do
      return;

   saveUndoState();           // So we can undo the paste

   clearSelection(database);  // Only the pasted items should be selected when we're done

   Point pastePos = snapPoint(database, convertCanvasToLevelCoord(mMousePos));

   Point firstPoint = mClipboard[0]->getVert(0);
   Point offsetFromFirstPoint;

   Vector<EditorObject *> copiedObjects;

   for(S32 i = 0; i < objCount; i++)
   {
      offsetFromFirstPoint = firstPoint - mClipboard[i]->getVert(0);

      EditorObject *newObject = mClipboard[i]->newCopy();
      newObject->setSelected(true);
      newObject->moveTo(pastePos - offsetFromFirstPoint);

      // addToGame is first so setSelected and onGeomChanged have mGame (at least barriers need it)
      newObject->addToGame(getGame(), NULL);    // Passing NULL keeps item out of any databases... will add in bulk below

      copiedObjects.push_back(newObject);
   }

   getDatabase()->addToDatabase(copiedObjects);
      
   for(S32 i = 0; i < copiedObjects.size(); i++)   
      copiedObjects[i]->onGeomChanged();
   

   onSelectionChanged();

   validateLevel();
   setNeedToSave(true);
   autoSave();
}


// Expand or contract selection by scale
void EditorUserInterface::scaleSelection(F32 scale)
{
   EditorObjectDatabase *database = getDatabase();

   if(!anyItemsSelected(database) || scale < .01 || scale == 1)    // Apply some sanity checks
      return;

   saveUndoState();

   // Find center of selection
   Point min, max;                        
   computeSelectionMinMax(database, min, max);
   Point ctr = (min + max) * 0.5;

   if(scale > 1 && min.distanceTo(max) * scale  > 50 * getGame()->getGridSize())    // If walls get too big, they'll bog down the db
      return;

   const Vector<EditorObject *> *objList = database->getObjectList();

   for(S32 i = 0; i < objList->size(); i++)
      if(objList->get(i)->isSelected())
         objList->get(i)->scale(ctr, scale);

   setNeedToSave(true);
   autoSave();
}


// Rotate selected objects around their center point by angle
void EditorUserInterface::rotateSelection(F32 angle)
{
   EditorObjectDatabase *database = getDatabase();

   if(!anyItemsSelected(database))
      return;

   saveUndoState();

   const Vector<EditorObject *> *objList = database->getObjectList();

   for(S32 i = 0; i < objList->size(); i++)
      if(objList->get(i)->isSelected())
      {
         objList->get(i)->rotateAboutPoint(Point(0,0), angle);
         objList->get(i)->onGeomChanged();
      }

   setNeedToSave(true);
   autoSave();
}


// Set the team affiliation of any selected items
void EditorUserInterface::setCurrentTeam(S32 currentTeam)
{
   mCurrentTeam = currentTeam;
   bool anyChanged = false;

   if(anythingSelected())
      saveUndoState();

   if(currentTeam >= getTeamCount())
   {
      char msg[255];

      if(getTeamCount() == 1)
         dSprintf(msg, sizeof(msg), "Only 1 team has been configured.");
      else
         dSprintf(msg, sizeof(msg), "Only %d teams have been configured.", getTeamCount());

      setWarnMessage(msg, "Hit [F2] to configure teams.");

      return;
   }

   // Update all dock items to reflect new current team
   for(S32 i = 0; i < mDockItems.size(); i++)
   {
      if(!mDockItems[i]->hasTeam())
         continue;

      if(currentTeam == TEAM_NEUTRAL && !mDockItems[i]->canBeNeutral())
         continue;

      if(currentTeam == TEAM_HOSTILE && !mDockItems[i]->canBeHostile())
         continue;

      mDockItems[i]->setTeam(currentTeam);
   }


   const Vector<EditorObject *> *objList = getDatabase()->getObjectList();

   for(S32 i = 0; i < objList->size(); i++)
   {
      EditorObject *obj = objList->get(i);
      if(obj->isSelected())
      {
         if(!obj->hasTeam())
            continue;

         if(currentTeam == TEAM_NEUTRAL && !obj->canBeNeutral())
            continue;

         if(currentTeam == TEAM_HOSTILE && !obj->canBeHostile())
            continue;

         if(!anyChanged)
            saveUndoState();

         obj->setTeam(currentTeam);
         anyChanged = true;
      }
   }

   // Overwrite any warnings set above.  If we have a group of items selected, it makes no sense to show a
   // warning if one of those items has the team set improperly.  The warnings are more appropriate if only
   // one item is selected, or none of the items are given a valid team setting.

   if(anyChanged)
   {
      setWarnMessage("", "");
      validateLevel();
      setNeedToSave(true);
      autoSave();
   }
}


void EditorUserInterface::flipSelectionHorizontal()
{
   Point min, max;
   computeSelectionMinMax(getDatabase(), min, max);
   F32 centerX = (min.x + max.x) / 2;

   flipSelection(centerX, true);
}


void EditorUserInterface::flipSelectionVertical()
{
   Point min, max;
   computeSelectionMinMax(getDatabase(), min, max);
   F32 centerY = (min.y + max.y) / 2;

   flipSelection(centerY, false);
}


void EditorUserInterface::flipSelection(F32 center, bool isHoriz)
{
   EditorObjectDatabase *database = getDatabase();

   if(!anyItemsSelected(database))
      return;

   saveUndoState();

   Point min, max;
   computeSelectionMinMax(database, min, max);
//   F32 centerX = (min.x + max.x) / 2;

   const Vector<EditorObject *> *objList = database->getObjectList();

   bool modifiedWalls = false;

   EditorObject::beginBatchGeomUpdate();

   for(S32 i = 0; i < objList->size(); i++)
   {
      EditorObject *obj = objList->get(i);

      if(obj->isSelected())
      {
         obj->flip(center, isHoriz);
         obj->onGeomChanged();

         if(isWallType(obj->getObjectTypeNumber()))
            modifiedWalls = true;
      }
   }

   EditorObject::endBatchGeomUpdate(database, modifiedWalls);

   setNeedToSave(true);
   autoSave();
}


static const S32 POINT_HIT_RADIUS = 9;
static const S32 EDGE_HIT_RADIUS = 6;

void EditorUserInterface::findHitItemAndEdge()
{
   mHitItem = NULL;
   mEdgeHit = NONE;
   mHitVertex = NONE;

   // Make hit rectangle larger than 1x1 -- when we consider point items, we need to make sure that we grab the item even when we're not right
   // on top of it, as the point item's hit target is much larger than the item itself.  100 is a guess that seems to work well.
   // Note that this is only used for requesting a candidate list from the database, actual hit detection is more precise.
   const Rect cursorRect((mMousePos - mCurrentOffset) / mCurrentScale, 100); 

   fillVector.clear();
   EditorObjectDatabase *editorDb = getDatabase();
   editorDb->findObjects((TestFunc)isAnyObjectType, fillVector, cursorRect);

   Point mouse = convertCanvasToLevelCoord(mMousePos);      // Figure out where the mouse is in level coords

   // Do this in two passes -- the first we only consider selected items, the second pass will consider all targets.
   // This will give priority to hitting vertices of selected items.
   for(S32 firstPass = 1; firstPass >= 0; firstPass--)     // firstPass will be true the first time through, false the second time
      for(S32 i = fillVector.size() - 1; i >= 0; i--)      // Go in reverse order to prioritize items drawn on top
      {
         EditorObject *obj = dynamic_cast<EditorObject *>(fillVector[i]);

         TNLAssert(obj, "Expected an EditorObject!");

         if(firstPass == (!obj->isSelected() && !obj->anyVertsSelected()))  // First pass is for selected items only
            continue;                                                       // Second pass only for unselected items
         
         if(checkForVertexHit(obj) || checkForEdgeHit(mouse, obj)) 
            return;                 
      }

   // We've already checked for wall vertices; now we'll check for hits in the interior of walls
   GridDatabase *wallDb = editorDb->getWallSegmentManager()->getWallSegmentDatabase();
   fillVector2.clear();

   wallDb->findObjects((TestFunc)isAnyObjectType, fillVector2, cursorRect);

   for(S32 i = 0; i < fillVector2.size(); i++)
      if(checkForWallHit(mouse, fillVector2[i]))
         return;

   // If we're still here, it means we didn't find anything yet.  Make one more pass, and see if we're in any polys.
   // This time we'll loop forward, though I don't think it really matters.
   for(S32 i = 0; i < fillVector.size(); i++)
     if(checkForPolygonHit(mouse, dynamic_cast<EditorObject *>(fillVector[i])))
        return;
}


// Vertex is weird because we don't always do thing in level coordinates -- some of our hit computation is based on
// absolute screen coordinates; some things, like wall vertices, are the same size at every zoom scale.  
bool EditorUserInterface::checkForVertexHit(EditorObject *object)
{
   F32 radius = object->getEditorRadius(mCurrentScale);

   for(S32 i = object->getVertCount() - 1; i >= 0; i--)
   {
      // p represents pixels from mouse to obj->getVert(j), at any zoom
      Point p = mMousePos - mCurrentOffset - (object->getVert(i) + object->getEditorSelectionOffset(mCurrentScale)) * mCurrentScale;    

      if(fabs(p.x) < radius && fabs(p.y) < radius)
      {
         mHitItem = object;
         mHitVertex = i;
         return true;
      }
   }

   return false;
}


bool EditorUserInterface::checkForEdgeHit(const Point &point, EditorObject *object)
{
   // Points have no edges, and walls are checked via another mechanism
   if(object->getGeomType() == geomPoint) 
      return false;

   const Vector<Point> &verts = *object->getOutline(); 
   TNLAssert(verts.size(), "Empty vertex problem");

   bool loop = (object->getGeomType() == geomPolygon);

   Point closest;

   S32 j_prev  = loop ? (verts.size() - 1) : 0;
         
   for(S32 j = loop ? 0 : 1; j < verts.size(); j++)
   {
      if(findNormalPoint(point, verts[j_prev], verts[j], closest))
      {
         F32 distance = (point - closest).len();
         if(distance < EDGE_HIT_RADIUS / mCurrentScale) 
         {
            mHitItem = object;
            mEdgeHit = j_prev;

            return true;
         }
      }
      j_prev = j;
   }

   return false;
}


bool EditorUserInterface::checkForWallHit(const Point &point, DatabaseObject *object)
{
   WallSegment *wallSegment = dynamic_cast<WallSegment *>(object);
   TNLAssert(wallSegment, "Expected a WallSegment!");

   if(triangulatedFillContains(wallSegment->getTriangulatedFillPoints(), point))
   {
      // Now that we've found a segment that our mouse is over, we need to find the wall object that it belongs to.  Chances are good
      // that it will be one of the objects sitting in fillVector.
      for(S32 i = 0; i < fillVector.size(); i++)
      {
         if(isWallType(fillVector[i]->getObjectTypeNumber()))
         {
            EditorObject *eobj = dynamic_cast<EditorObject *>(fillVector[i]);

            if(eobj->getSerialNumber() == wallSegment->getOwner())
            {
               mHitItem = eobj;
               return true;
            }
         }
      }

      // Note, if we get to here, we have a problem.

      //logprintf("Found seg: %s", wallSegment->getExtent().toString().c_str());

      //TNLAssert(false, "Should have found a wall.  Either the extents are wrong again, or the walls and their segments are out of sync.");

      // This code does a less efficient but more thorough job finding a wall that matches the segment we hit... if the above assert
      // keeps going off, and we can't fix it, this code here should take care of the problem.  But using it is an admission of failure.

      EditorObjectDatabase *editorDb = getDatabase();
      const Vector<EditorObject *> *objList = editorDb->getObjectList();

      for(S32 i = 0; i < objList->size(); i++)
      {
         if(isWallType(objList->get(i)->getObjectTypeNumber()))
         {
            EditorObject *eobj = objList->get(i);

            if(eobj->getSerialNumber() == wallSegment->getOwner())
            {
               //editorDb->dumpObjects();
               //logprintf("Found wall: %s", eobj->getExtent().toString().c_str());
               mHitItem = eobj;
               return true;
            }
         }
      }
   }

   return false;
}


bool EditorUserInterface::checkForPolygonHit(const Point &point, EditorObject *object)
{
   if(object->getGeomType() == geomPolygon && triangulatedFillContains(object->getFill(), point))
   {
      mHitItem = object;
      return true;
   }

   return false;
}


// Sets mDockItemHit
void EditorUserInterface::findHitItemOnDock()
{
   mDockItemHit = NULL;

   for(S32 i = mDockItems.size() - 1; i >= 0; i--)     // Go in reverse order because the code we copied did ;-)
   {
      Point pos = mDockItems[i]->getPos();

      if(fabs(mMousePos.x - pos.x) < POINT_HIT_RADIUS && fabs(mMousePos.y - pos.y) < POINT_HIT_RADIUS)
      {
         mDockItemHit = mDockItems[i].get();
         return;
      }
   }

   // Now check for polygon interior hits
   for(S32 i = 0; i < mDockItems.size(); i++)
      if(mDockItems[i]->getGeomType() == geomPolygon)
      {
         Vector<Point> verts;
         for(S32 j = 0; j < mDockItems[i]->getVertCount(); j++)
            verts.push_back(mDockItems[i]->getVert(j));

         if(PolygonContains2(verts.address(),verts.size(), mMousePos))
         {
            mDockItemHit = mDockItems[i].get();
            return;
         }
      }

   return;
}


void EditorUserInterface::onMouseMoved()
{
   Parent::onMouseMoved();

   if(mouseIgnore)  // Needed to avoid freezing effect from too many mouseMoved events without a render in between
      return;

   mouseIgnore = true;

   mMousePos.set(gScreenInfo.getMousePos());

   // Doing this with MOUSE_RIGHT allows you to drag a vertex you just placed by holding the right-mouse button
   if(InputCodeManager::getState(MOUSE_LEFT) || InputCodeManager::getState(MOUSE_RIGHT) || InputCodeManager::getState(MOUSE_MIDDLE))
   {
      onMouseDragged();
      return;
   }

   if(mCreatingPoly || mCreatingPolyline)
      return;

   // Turn off highlight on selected item -- will be turned back on for this object or another below
   if(mHitItem)
      mHitItem->setLitUp(false);

   findHitItemAndEdge();      //  Sets mHitItem, mHitVertex, and mEdgeHit
   findHitItemOnDock();

   // We hit a vertex that wasn't already selected
   if(mHitItem && mHitVertex != NONE && !mHitItem->vertSelected(mHitVertex))   
      mHitItem->setVertexLitUp(mHitVertex);

   // Highlight currently selected item
   if(mHitItem)
      mHitItem->setLitUp(true);

   findSnapVertex();
   SDL_SetCursor(Cursor::getDefault());
}


void EditorUserInterface::onMouseDragged()
{
   if(InputCodeManager::getState(MOUSE_MIDDLE) && mMousePos != mScrollWithMouseLocation)
   {
      mCurrentOffset += mMousePos - mScrollWithMouseLocation;
      mScrollWithMouseLocation = mMousePos;
      mAutoScrollWithMouseReady = false;

      return;
   }

   if(mCreatingPoly || mCreatingPolyline || mDragSelecting)
      return;


   bool needToSaveUndoState = true;

   // I assert we never need to save an input state here if we are right-mouse dragging
   if(InputCodeManager::getState(MOUSE_RIGHT))
      needToSaveUndoState = false;

   if(mDraggingDockItem != NULL)    // We just started dragging an item off the dock
   {
       startDraggingDockItem();  
       needToSaveUndoState = false;
   }

   findSnapVertex();                               // Sets mSnapObject and mSnapVertexIndex
   if(!mSnapObject || mSnapVertexIndex == NONE)    // If we've just started dragging a dock item, this will be it
      return;


   if(!mDraggingObjects)            // Just started dragging
   {
      if(needToSaveUndoState)
         saveUndoState(true);                 // Save undo state before we clear the selection

      mMoveOrigin = mSnapObject->getVert(mSnapVertexIndex);

      const Vector<EditorObject *> *objList = getDatabase()->getObjectList();

#ifdef TNL_OS_MAC_OSX 
      bool ctrlDown = InputCodeManager::getState(KEY_META);
#else
      bool ctrlDown = InputCodeManager::getState(KEY_CTRL);
#endif

      if(ctrlDown)     // Ctrl+Drag ==> copy and drag (except for Mac)
      {
         Vector<EditorObject *> copiedObjects;

         for(S32 i = 0; i < objList->size(); i++)
         {
            EditorObject *obj = objList->get(i);

            if(obj->isSelected())
            {
               EditorObject *newObject =  obj->newCopy();   
               newObject->setSelected(true);
               newObject->addToGame(getGame(), NULL);    // NULL keeps object out of database... will be added in bulk below
               
               copiedObjects.push_back(newObject);

               // Make mHitItem be the new copy of the old mHitItem
               if(mHitItem == obj)
                  mHitItem = newObject;

               if(mSnapObject == obj)
                  mSnapObject = newObject;
            }
         }

         mDragCopying = true;

         // Now mark source objects as unselected
         for(S32 i = 0; i < objList->size(); i++)
         {
            objList->get(i)->setSelected(false);
            objList->get(i)->setLitUp(false);
         }

         // Now add copied objects to our database; these were marked as selected when they were created
         getDatabase()->addToDatabase(copiedObjects);

         // Running onGeomChanged causes any copied walls to have a full body while we're dragging them 
         for(S32 i = 0; i < copiedObjects.size(); i++)   
            copiedObjects[i]->onGeomChanged(); 
      }

      onSelectionChanged();
      mDraggingObjects = true; 
      mSnapDelta.set(0,0);
   }


   SDL_SetCursor(Cursor::getSpray());

   Point lastSnapDelta = mSnapDelta;
   // The thinking here is that for large items -- walls, polygons, etc., we may grab an item far from its snap vertex, and we
   // want to factor that offset into our calculations.  For point items (and vertices), we don't really care about any slop
   // in the selection, and we just want the damn thing where we put it.
   if(mSnapObject->getGeomType() == geomPoint || (mHitItem && mHitItem->anyVertsSelected()))
      mSnapDelta = snapPoint(getDatabase(), convertCanvasToLevelCoord(mMousePos)) - mMoveOrigin;
   else  // larger items
      mSnapDelta = snapPoint(getDatabase(), convertCanvasToLevelCoord(mMousePos) + mMoveOrigin - mMouseDownPos) - mMoveOrigin;

   // Update coordinates of dragged item
   translateSelectedItems(getDatabase(), mSnapDelta - lastSnapDelta);
}


void EditorUserInterface::translateSelectedItems(EditorObjectDatabase *database, const Point &offset)
{
   const Vector<EditorObject *> *objList = database->getObjectList();

   S32 count = 0;

   for(S32 i = 0; i < objList->size(); i++)
   {
      EditorObject *obj = objList->get(i);

      if(obj->isSelected() || obj->anyVertsSelected())
      {
         Point newVert;    // Reusable container

         for(S32 j = 0; j < obj->getVertCount(); j++)
            if(obj->isSelected() || obj->vertSelected(j))
            {
               newVert = obj->getVert(j) + offset;
               obj->setVert(newVert, j);
               count++;
            }

         // If we are dragging a vertex, and not the entire item, we are changing the geom, so notify the item
         if(obj->anyVertsSelected())
            obj->onGeomChanging();  

         if(obj->isSelected())     
            obj->onItemDragging();      // Make sure this gets run after we've updated the item's location
      }
   }
}


EditorObject *EditorUserInterface::copyDockItem(EditorObject *source)
{
   // Instantiate object so we are essentially dragging a non-dock item
   EditorObject *newObject = source->newCopy();
   newObject->newObjectFromDock(getGame()->getGridSize());     // Do things particular to creating an object that came from dock

   return newObject;
}


// User just dragged an item off the dock
void EditorUserInterface::startDraggingDockItem()
{
   saveUndoState();     // Save our undo state before we create a new item

   EditorObject *item = copyDockItem(mDraggingDockItem);

   // Offset lets us drag an item out from the dock by an amount offset from the 0th vertex.  This makes placement seem more natural.
   Point pos = convertCanvasToLevelCoord(mMousePos) - item->getInitialPlacementOffset(getGame()->getGridSize());
   item->moveTo(pos);
      
   EditorObjectDatabase *database = getDatabase();

   item->addToEditor(getGame(), database); 
   database->dumpObjects();
   
   clearSelection(database);    // No items are selected...
   item->setSelected(true);     // ...except for the new one
   onSelectionChanged();
   mDraggingDockItem = NULL;    // Because now we're dragging a real item
   validateLevel();             // Check level for errors


   // Because we sometimes have trouble finding an item when we drag it off the dock, after it's been sorted,
   // we'll manually set mHitItem based on the selected item, which will always be the one we just added.
   // TODO: Still needed?

   const Vector<EditorObject *> *objList = database->getObjectList();

   mEdgeHit = NONE;
   for(S32 i = 0; i < objList->size(); i++)
      if(objList->get(i)->isSelected())
      {
         mHitItem = objList->get(i);
         break;
      }
}


// Sets mSnapObject and mSnapVertexIndex based on the vertex closest to the cursor that is part of the selected set
// What we really want is the closest vertex in the closest feature
void EditorUserInterface::findSnapVertex()
{
   F32 closestDist = F32_MAX;

   if(mDraggingObjects)    // Don't change snap vertex once we're dragging
      return;

   mSnapObject = NULL;
   mSnapVertexIndex = NONE;

   Point mouseLevelCoord = convertCanvasToLevelCoord(mMousePos);

   // If we have a hit item, and it's selected, find the closest vertex in the item
   if(mHitItem && mHitItem->isSelected())   
   {
      // If we've hit an edge, restrict our search to the two verts that make up that edge
      if(mEdgeHit != NONE)
      {
         mSnapObject = mHitItem;     // Regardless of vertex, this is our hit item
         S32 v1 = mEdgeHit;
         S32 v2 = mEdgeHit + 1;

         // Handle special case of looping item
         if(mEdgeHit == mHitItem->getVertCount() - 1)
            v2 = 0;

         // Find closer vertex: v1 or v2
         mSnapVertexIndex = (mHitItem->getVert(v1).distSquared(mouseLevelCoord) < 
                          mHitItem->getVert(v2).distSquared(mouseLevelCoord)) ? v1 : v2;

         return;
      }

      // Didn't hit an edge... find the closest vertex anywhere in the item
      for(S32 j = 0; j < mHitItem->getVertCount(); j++)
      {
         F32 dist = mHitItem->getVert(j).distSquared(mouseLevelCoord);

         if(dist < closestDist)
         {
            closestDist = dist;
            mSnapObject = mHitItem;
            mSnapVertexIndex = j;
         }
      }
      return;
   } 

   const Vector<EditorObject *> *objList = getDatabase()->getObjectList();

   // Otherwise, we don't have a selected hitItem -- look for a selected vertex
   for(S32 i = 0; i < objList->size(); i++)
   {
      EditorObject *obj = objList->get(i);

      for(S32 j = 0; j < obj->getVertCount(); j++)
      {
         // If we find a selected vertex, there will be only one, and this is our snap point
         if(obj->vertSelected(j))
         {
            mSnapObject = obj;
            mSnapVertexIndex = j;
            return;     
         }
      }
   }
}


// Delete selected items (true = items only, false = items & vertices)
void EditorUserInterface::deleteSelection(bool objectsOnly)
{
   if(mDraggingObjects)     // No deleting while we're dragging, please...
      return;

   if(!anythingSelected())  // Nothing to delete
      return;

   bool deleted = false, deletedWall = false;

   const Vector<EditorObject *> *objList = getDatabase()->getObjectList();

   for(S32 i = objList->size() - 1; i >= 0; i--)  // Reverse to avoid having to have i-- in middle of loop
   {
      EditorObject *obj = objList->get(i);

      if(obj->isSelected())
      {  
         // Since indices change as items are deleted, this will keep incorrect items from being deleted
         if(obj->isLitUp())
            mHitItem = NULL;

         if(!deleted)
            saveUndoState();

         if(isWallType(obj->getObjectTypeNumber()))
            deletedWall = true;

         deleteItem(i, true);
         deleted = true;
      }
      else if(!objectsOnly)      // Deleted only selected vertices
      {
         bool geomChanged = false;

         for(S32 j = 0; j < obj->getVertCount(); j++) 
         {
            if(obj->vertSelected(j))
            {
               if(!deleted)
                  saveUndoState();
              
               obj->deleteVert(j);
               deleted = true;

               geomChanged = true;
               mSnapObject = NULL;
               mSnapVertexIndex = NONE;
            }
         }

         // Check if item has too few vertices left to be viable
         if(obj->getVertCount() < obj->getMinVertCount())
         {
            if(isWallType(obj->getObjectTypeNumber()))
               deletedWall = true;

            deleteItem(i, true);
            deleted = true;
         }
         else if(geomChanged)
            obj->onGeomChanged();

      }  // else if(!objectsOnly) 
   }  // for


   if(deletedWall)
      doneDeleteingWalls();

   if(deleted)
   {
      setNeedToSave(true);
      autoSave();

      mHitItem = NULL;     // In case we just deleted a lit item; not sure if really needed, as we do this above
      mHitVertex = NONE;

      doneDeleteing();
   }
}


// Increase selected wall thickness by amt
void EditorUserInterface::changeBarrierWidth(S32 amt)
{
   if(!mLastUndoStateWasBarrierWidthChange)
      saveUndoState(); 

   fillVector2.clear();    // fillVector gets modified in some child function, so use our secondary reusable container
   getDatabase()->findObjects((TestFunc)isLineItemType, fillVector2);

   for(S32 i = 0; i < fillVector2.size(); i++)
   {
      LineItem *obj = dynamic_cast<LineItem *>(fillVector2[i]);   // Walls are a subclass of LineItem, so this will work for both
      if(obj->isSelected())
         obj->changeWidth(amt);     
   }

   mLastUndoStateWasBarrierWidthChange = true;
}


// Split wall/barrier on currently selected vertex/vertices
// Or, if entire wall is selected, split on snapping vertex -- this seems a natural way to do it
void EditorUserInterface::splitBarrier()
{
   bool split = false;

   EditorObjectDatabase *database = getDatabase();

   const Vector<EditorObject *> *objList = database->getObjectList();

   for(S32 i = 0; i < objList->size(); i++)
   {
      EditorObject *obj = objList->get(i);

      if(obj->getGeomType() == geomPolyLine)
          for(S32 j = 1; j < obj->getVertCount() - 1; j++)     // Can't split on end vertices!
            if(obj->vertSelected(j))
            {
               saveUndoState();
               
               doSplit(obj, j);

               split = true;
               goto done2;                         // Yes, gotos are naughty, but they just work so well sometimes...
            }
   }

   // If we didn't find a suitable selected vertex to split on, look for a selected line with a magenta vertex
   if(!split && mSnapObject && mSnapObject->getGeomType() == geomPolyLine && mSnapObject->isSelected() && 
         mSnapVertexIndex != NONE && mSnapVertexIndex != 0 && mSnapVertexIndex != mSnapObject->getVertCount() - 1)
   {         
      saveUndoState();

      doSplit(mSnapObject, mSnapVertexIndex);

      split = true;
   }

done2:
   if(split)
   {
      clearSelection(database);
      setNeedToSave(true);
      autoSave();
   }
}


// Split wall or line -- will probably crash on other geom types
void EditorUserInterface::doSplit(EditorObject *object, S32 vertex)
{
   EditorObject *newObj = object->newCopy();    // Copy the attributes
   newObj->clearVerts();                        // Wipe out the geometry

   // Note that it would be more efficient to start at the end and work backwards, but the direction of our numbering would be
   // reversed in the new object compared to what it was.  This isn't important at the moment, but it just seems wrong from a
   // geographic POV.  Which I have.
   for(S32 i = vertex; i < object->getVertCount(); i++) 
   {
      newObj->addVert(object->getVert(i), true);      // true: If wall already has more than max number of points, let children have more as well
      if(i != vertex)               // i.e. if this isn't the first iteration
      {
         object->deleteVert(i);     // Don't delete first vertex -- we need it to remain as final vertex of old feature
         i--;
      }
   }

   newObj->addToEditor(getGame(), object->getEditorObjectDatabase());     // Needs to happen before onGeomChanged, so mGame will not be NULL

   // Tell the new segments that they have new geometry
   object->onGeomChanged();
   newObj->onGeomChanged();            
}


// Join two or more sections of wall that have coincident end points.  Will ignore invalid join attempts.
void EditorUserInterface::joinBarrier()
{
   EditorObject *joinedObj = NULL;

   EditorObjectDatabase *database = getDatabase();

   const Vector<EditorObject *> *objList = database->getObjectList();

   for(S32 i = 0; i < objList->size()-1; i++)
   {
      EditorObject *obj_i = objList->get(i);

      if(obj_i->getGeomType() == geomPolyLine && obj_i->isSelected())      // Will work for both lines and walls, or any future polylines
      {
         for(S32 j = i + 1; j < objList->size(); j++)                      // Compare against remaining objects
         {
            EditorObject *obj_j = objList->get(j);

            if(obj_j->getObjectTypeNumber() == obj_i->getObjectTypeNumber() && obj_j->isSelected())
            {
               // Don't join if resulting object would be too big!
               if(obj_i->getVertCount() + obj_j->getVertCount() > gMaxPolygonPoints)
                  continue;

               if(obj_i->getVert(0).distanceTo(obj_j->getVert(0)) < .01)   // First vertices are the same  1 2 3 | 1 4 5
               {
                  if(!joinedObj)          // This is first join candidate found; something's going to merge, so save an undo state
                     saveUndoState();
               
                  joinedObj = obj_i;

                  for(S32 a = 1; a < obj_j->getVertCount(); a++)           // Skip first vertex, because it would be a dupe
                     obj_i->addVertFront(obj_j->getVert(a));

                  deleteItem(j);
                  i--;  j--;
               }

               // First vertex conincides with final vertex 3 2 1 | 5 4 3
               else if(obj_i->getVert(0).distanceTo(obj_j->getVert(obj_j->getVertCount()-1)) < .01)     
               {
                  if(!joinedObj)
                     saveUndoState();

                  joinedObj = obj_i;
                  
                  for(S32 a = obj_j->getVertCount()-2; a >= 0; a--)
                     obj_i->addVertFront(obj_j->getVert(a));

                  deleteItem(j);    // j has been merged into i; don't need j anymore!
                  i--;  j--;
               }

               // Last vertex conincides with first 1 2 3 | 3 4 5
               else if(obj_i->getVert(obj_i->getVertCount()-1).distanceTo(obj_j->getVert(0)) < .01)     
               {
                  if(!joinedObj)
                     saveUndoState();

                  joinedObj = obj_i;

                  for(S32 a = 1; a < obj_j->getVertCount(); a++)  // Skip first vertex, because it would be a dupe         
                     obj_i->addVert(obj_j->getVert(a));

                  deleteItem(j);
                  i--;  j--;
               }

               // Last vertices coincide  1 2 3 | 5 4 3
               else if(obj_i->getVert(obj_i->getVertCount()-1).distanceTo(obj_j->getVert(obj_j->getVertCount()-1)) < .01)     
               {
                  if(!joinedObj)
                     saveUndoState();

                  joinedObj = obj_i;

                  for(S32 a = obj_j->getVertCount()-2; a >= 0; a--)
                     obj_i->addVert(obj_j->getVert(a));

                  deleteItem(j);
                  i--;  j--;
               }
            }
         }
      }
   }

   if(joinedObj)     // We had a successful merger
   {
      clearSelection(database);
      setNeedToSave(true);
      autoSave();
      joinedObj->onGeomChanged();

      joinedObj->setSelected(true);

      onSelectionChanged();
   }
}


void EditorUserInterface::deleteItem(S32 itemIndex, bool batchMode)
{
   EditorObjectDatabase *database = getDatabase();
   WallSegmentManager *wallSegmentManager = database->getWallSegmentManager();

   const Vector<EditorObject *> *objList = database->getObjectList();

   EditorObject *obj = objList->get(itemIndex);


   if(isWallType(obj->getObjectTypeNumber()))
   {
      // Need to recompute boundaries of any intersecting walls
      wallSegmentManager->deleteSegments(obj->getItemId());          // Delete the segments associated with the wall
      database->removeFromDatabase(obj, obj->getExtent());

      if(!batchMode)
         doneDeleteingWalls();
   }
   else
      database->removeFromDatabase(obj, obj->getExtent());

   if(!batchMode)
      doneDeleteing();
}


// After deleting a bunch of items, clean up
void EditorUserInterface::doneDeleteingWalls()
{
   WallSegmentManager *wallSegmentManager = mLoadTarget->getWallSegmentManager();

   wallSegmentManager->recomputeAllWallGeometry(mLoadTarget);   // Recompute wall edges
   resnapAllEngineeredItems(mLoadTarget);         
}


void EditorUserInterface::doneDeleteing()
{
   // Reset a bunch of things
   mSnapObject = NULL;
   mSnapVertexIndex = NONE;

   validateLevel();

   onMouseMoved();   // Reset cursor  
}


// Only called when user presses a hotkey to insert an item -- may crash if item to be inserted is not currently on the dock!
void EditorUserInterface::insertNewItem(U8 itemTypeNumber)
{
   if(mDraggingObjects)     // No inserting when items are being dragged!
      return;

   EditorObjectDatabase *database = getDatabase();

   clearSelection(database);
   saveUndoState();

   EditorObject *newObject = NULL;

   // Find a dockItem to copy
   for(S32 i = 0; i < mDockItems.size(); i++)
      if(mDockItems[i]->getObjectTypeNumber() == itemTypeNumber)
      {
         newObject = copyDockItem(mDockItems[i].get());
         break;
      }

   // May occur if requested item is not currently on the dock
   TNLAssert(newObject, "Couldn't create object in insertNewItem()");
   if(!newObject)
      return;


   newObject->moveTo(snapPoint(database, convertCanvasToLevelCoord(mMousePos)));
   newObject->addToEditor(getGame(), database);    
   newObject->onGeomChanged();

   validateLevel();
   setNeedToSave(true);
   autoSave();
}


static LineEditor getNewEntryBox(string value, string prompt, S32 length, LineEditor::LineEditorFilter filter)
{
   LineEditor entryBox(length);
   entryBox.setPrompt(prompt);
   entryBox.setString(value);
   entryBox.setFilter(filter);

   return entryBox;
}


void EditorUserInterface::centerView()
{
//   const Vector<EditorObject *> *objList = getDatabase()->getObjectList();
//   const Vector<EditorObject *> *levelGenObjList = mLevelGenDatabase.getObjectList();

   Rect extents = getDatabase()->getExtents();
   extents.unionRect(mLevelGenDatabase.getExtents());


   //if(objList->size() || levelGenObjList->size())
   //{
   //   F32 minx =  F32_MAX,   miny =  F32_MAX;
   //   F32 maxx = -F32_MAX,   maxy = -F32_MAX;

   //   for(S32 i = 0; i < objList->size(); i++)
   //   {
   //      EditorObject *obj = objList->get(i);

   //      for(S32 j = 0; j < obj->getVertCount(); j++)
   //      {
   //         if(obj->getVert(j).x < minx)
   //            minx = obj->getVert(j).x;
   //         if(obj->getVert(j).x > maxx)
   //            maxx = obj->getVert(j).x;
   //         if(obj->getVert(j).y < miny)
   //            miny = obj->getVert(j).y;
   //         if(obj->getVert(j).y > maxy)
   //            maxy = obj->getVert(j).y;
   //      }
   //   }

   //   for(S32 i = 0; i < levelGenObjList->size(); i++)
   //   {
   //      EditorObject *obj = levelGenObjList->get(i);

   //      for(S32 j = 0; j < obj->getVertCount(); j++)
   //      {
   //         if(obj->getVert(j).x < minx)
   //            minx = obj->getVert(j).x;
   //         if(obj->getVert(j).x > maxx)
   //            maxx = obj->getVert(j).x;
   //         if(obj->getVert(j).y < miny)
   //            miny = obj->getVert(j).y;
   //         if(obj->getVert(j).y > maxy)
   //            maxy = obj->getVert(j).y;
   //      }
   //   }


   F32 x = extents.getCenter().x;
   F32 y = extents.getCenter().y;

   // If we have nothing, or maybe only one point object in our level
   if(extents.getWidth() < 1 && extents.getHeight() < 1)    // e.g. a single point item
   {
      mCurrentScale = STARTING_SCALE;
      mCurrentOffset.set(gScreenInfo.getGameCanvasWidth() / 2  - mCurrentScale * x, 
                           gScreenInfo.getGameCanvasHeight() / 2 - mCurrentScale * y);
   }
   else
   {
      mCurrentScale = min(gScreenInfo.getGameCanvasWidth() / extents.getWidth(), 
                           gScreenInfo.getGameCanvasHeight() / extents.getHeight());

      mCurrentScale /= 1.3f;      // Zoom out a bit

      mCurrentOffset.set(gScreenInfo.getGameCanvasWidth() / 2  - mCurrentScale * x, 
                           gScreenInfo.getGameCanvasHeight() / 2 - mCurrentScale * y);
   }
   //}
   //else     // Put (0,0) at center of screen
   //{
   //   mCurrentScale = STARTING_SCALE;
   //   mCurrentOffset.set(gScreenInfo.getGameCanvasWidth() / 2, gScreenInfo.getGameCanvasHeight() / 2);
   //}
}


F32 EditorUserInterface::getCurrentScale()
{
   return mCurrentScale;
}


Point EditorUserInterface::getCurrentOffset()
{
   return mCurrentOffset;
}


// Gets run when user exits special-item editing mode, called from attribute editors
void EditorUserInterface::doneEditingAttributes(EditorAttributeMenuUI *editor, EditorObject *object)
{
   object->onAttrsChanged();

   const Vector<EditorObject *> *objList = getDatabase()->getObjectList();

   // Find any other selected items of the same type of the item we just edited, and update their attributes too
   for(S32 i = 0; i < objList->size(); i++)
   {
      EditorObject *obj = objList->get(i);

      if(obj != object && obj->isSelected() && obj->getObjectTypeNumber() == object->getObjectTypeNumber())
      {
         editor->doneEditingAttrs(obj);  // Transfer attributes from editor to object
         obj->onAttrsChanged();          // And notify the object that its attributes have changed
      }
   }
}


void EditorUserInterface::zoom(F32 zoomAmount)
{
   Point mouseLevelPoint = convertCanvasToLevelCoord(mMousePos);

   mCurrentScale *= 1 + zoomAmount;

   if(mCurrentScale < MIN_SCALE)
      mCurrentScale = MIN_SCALE;
   else if(mCurrentScale > MAX_SCALE)
      mCurrentScale = MAX_SCALE;
   
   Point newMousePoint = convertLevelToCanvasCoord(mouseLevelPoint);

   mCurrentOffset += mMousePos - newMousePoint;
}


// Handle key presses
bool EditorUserInterface::onKeyDown(InputCode inputCode, char ascii)
{
   if(Parent::onKeyDown(inputCode, ascii))
      return true;

   if(OGLCONSOLE_ProcessBitfighterKeyEvent(inputCode, ascii))      // Pass the key on to the console for processing
      return true;

   string inputString = InputCodeManager::makeInputString(inputCode);

   // TODO: Make this stuff work like the attribute entry stuff; use a real menu and not this ad-hoc code
   // This is where we handle entering things like rotation angle and other data that requires a special entry box.
   // NOT for editing an item's attributes.  Still used, but untested in refactor.
   if(entryMode != EntryNone)
      textEntryKeyHandler(inputCode, ascii);

   else if(inputCode == KEY_ENTER || inputCode == KEY_KEYPAD_ENTER)       // Enter - Edit props
      startAttributeEditor();

   // Mouse wheel zooms in and out

   else if(inputCode == MOUSE_WHEEL_UP)
      zoom(0.2f);
   else if(inputCode == MOUSE_WHEEL_DOWN)
      zoom(-0.2f);
   else if(inputCode == MOUSE_MIDDLE)     // Click wheel to drag
   {
      mScrollWithMouseLocation = mMousePos;
      mAutoScrollWithMouseReady = !mAutoScrollWithMouse; // Ready to scroll when button is released
      mAutoScrollWithMouse = false;  // turn off in case we were already auto scrolling.
   }

   // Regular key handling from here on down
   else if(InputCodeManager::checkModifier(KEY_SHIFT) && inputCode == KEY_0)  // Shift-0 -> Set team to hostile
      setCurrentTeam(-2);

   else if(ascii == '#' || ascii == '!')
   {
      S32 selected = NONE;

      const Vector<EditorObject *> *objList = getDatabase()->getObjectList();

      // Find first selected item, and just work with that.  Unselect the rest.
      for(S32 i = 0; i < objList->size(); i++)
      {
         if(objList->get(i)->isSelected())
         {
            if(selected == NONE)
            {
               selected = i;
               continue;
            }
            else
               objList->get(i)->setSelected(false);
         }
      }

      onSelectionChanged();

      if(selected == NONE)      // Nothing selected, nothing to do!
         return true;

      mEntryBox = getNewEntryBox(objList->get(selected)->getItemId() <= 0 ? "" : itos(objList->get(selected)->getItemId()), 
                                 "Item ID:", 10, LineEditor::digitsOnlyFilter);
      entryMode = EntryID;
   }

   else if(ascii >= '0' && ascii <= '9' && InputCodeManager::checkModifier(KEY_NONE))  // Change team affiliation of selection with 0-9 keys
   {
      setCurrentTeam(ascii - '1');
      return true;
   }

#ifdef TNL_OS_MAC_OSX 
      // Ctrl-left click is same as right click for Mac users
      else if(inputCode == MOUSE_RIGHT || (inputCode == MOUSE_LEFT && InputCodeManager::checkModifier(KEY_CTRL)))
#else
      else if(inputCode == MOUSE_RIGHT)
#endif
      onMouseClicked_right();

   else if(inputCode == MOUSE_LEFT)
      onMouseClicked_left();

   // Neither mouse button, let's try some keys
   else if(inputString == "D"|| inputString == "Shift+D")            // Pan right
      mRight = true;
   else if(inputString == "Right Arrow")  // Pan right
      mRight = true;
   else if(inputString == "H")            // Flip horizontal
      flipSelectionHorizontal();
   else if(inputString == "Ctrl+V")       // Paste selection
      pasteSelection();
   else if(inputString == "V")            // Flip vertical
      flipSelectionVertical();
   else if(inputString == "/" || inputString == "Keypad /")
      OGLCONSOLE_ShowConsole();

   else if(inputString == "Ctrl+Shift+L") // Reload level
   {
      loadLevel();                        
      setSaveMessage("Reloaded " + getLevelFileName(), true);
   }
   else if(inputString == "Ctrl+Shift+Z") // Redo
   {
      if(!mCreatingPolyline && !mCreatingPoly && !mDraggingObjects && !mDraggingDockItem)
         redo();
   }
   else if(inputString == "Ctrl+Z")       // Undo
   {
      if(!mCreatingPolyline && !mCreatingPoly && !mDraggingObjects && !mDraggingDockItem)
         undo(true);
   }
   else if(inputString == "Z")            // Reset veiw
      centerView();
   else if(inputString == "Ctrl+Shift+R") // Rotate by arbitrary amount
   {
      if(!anyItemsSelected(getDatabase()))
         return true;

      mEntryBox = getNewEntryBox("", "Rotation angle:", 10, LineEditor::numericFilter);
      entryMode = EntryAngle;
   }
   else if(inputString == "Ctrl+R")       // Run levelgen script, or clear last results
   {
      if(mLevelGenDatabase.getObjectList()->size() == 0)
         runLevelGenScript();
      else
         clearLevelGenItems();
   }
   else if(inputString == "R")            // Rotate CCW
      rotateSelection(-15.f); 
   else if(inputString == "Shift+R")      // Rotate CW
      rotateSelection(15.f); 

   else if(inputString == "Ctrl+I")       // Insert items generated with script into editor
      copyScriptItemsToEditor();

   else if(inputString == "Up Arrow" || inputString == "W" || inputString == "Shift+W")  // W or Up - Pan up
      mUp = true;
   else if(inputString == "Ctrl+Up Arrow")      // Zoom in
      mIn = true;
   else if(inputString == "Ctrl+Down Arrow")    // Zoom out
      mOut = true;
   else if(inputString == "Down Arrow")         // Pan down
      mDown = true;
   else if(inputString == "Ctrl+S")             // Save
      saveLevel(true, true);
   else if(inputString == "S"|| inputString == "Shift+S")                  // Pan down
      mDown = true;
   else if(inputString == "Left Arrow" || inputString == "A"|| inputString == "Shift+A")   // Left or A - Pan left
      mLeft = true;
   else if(inputString == "Shift+=" || inputString == "Shift+Keypad +")    // Shifted - Increase barrier width by 1
      changeBarrierWidth(1);
   else if(inputString == "=" || inputString == "Keypad +")                // Unshifted + --> by 5
      changeBarrierWidth(5);
   else if(inputString == "Shift+-" || inputString == "Shift+Keypad -")    // Shifted - Decrease barrier width by 1
      changeBarrierWidth(-1);
   else if(inputString == "-" || inputString == "Keypad -")                // Unshifted --> by 5
      changeBarrierWidth(-5);
   else if(inputString == "E")            // Zoom In
      mIn = true;
   else if(inputString == "\\")           // Split barrier on selected vertex
      splitBarrier();
   else if(inputString == "J")            // Join selected barrier segments
      joinBarrier();
   else if(inputString == "Ctrl+A")       // Select everything
      selectAll(getDatabase());
   else if(inputString == "Ctrl+Shift+X") // Resize selection
   {
      if(anyItemsSelected(getDatabase()))
      {
         mEntryBox = getNewEntryBox("", "Resize factor:", 10, LineEditor::numericFilter);
         entryMode = EntryScale;
      }
   }
   else if(inputString == "Ctrl+X")     // Cut selection
   {
      copySelection();
      deleteSelection(true);
   }
   else if(inputString == "Ctrl+C")    // Copy selection to clipboard
      copySelection();
   else if(inputString == "C")         // Zoom out
      mOut = true;
   else if(inputString == "F3")        // Level Parameter Editor
   {
      playBoop();
      getUIManager()->getGameParamUserInterface()->activate();
   }
   else if(inputString == "F2")               // Team Editor Menu
   {
      getUIManager()->getTeamDefUserInterface()->activate();
      playBoop();
   }
   else if(inputString == "T")                // Teleporter
      insertNewItem(TeleportTypeNumber);
   else if(inputString == "P")                // Speed Zone
      insertNewItem(SpeedZoneTypeNumber);
   else if(inputString == "G")                // Spawn
      insertNewItem(ShipSpawnTypeNumber);
   else if(inputString == "Ctrl+B")           // Spy Bug
      insertNewItem(SpyBugTypeNumber);
   else if(inputString == "B")                // Repair
      insertNewItem(RepairItemTypeNumber);
   else if(inputString == "Y")                // Turret
      insertNewItem(TurretTypeNumber);
   else if(inputString == "M")                // Mine
      insertNewItem(MineTypeNumber);
   else if(inputString == "F")                // Force Field
      insertNewItem(ForceFieldProjectorTypeNumber);
   else if(inputString == "Backspace" || inputString == "Del" || inputString == "Keypad .")     // Keypad . is the keypad's del key
      deleteSelection(false);
   else if(checkInputCode(getGame()->getSettings(), InputCodeManager::BINDING_HELP, inputCode)) // Turn on help screen
   {
      getGame()->getUIManager()->getEditorInstructionsUserInterface()->activate();
      playBoop();
   }
   else if(inputCode == KEY_ESCAPE)          // Activate the menu
   {
      playBoop();
      getGame()->getUIManager()->getEditorMenuUserInterface()->activate();
   }
   else if(inputString == "Space")           // No snapping to grid, but still to other things
      mSnapContext = NO_GRID_SNAPPING;
   else if(inputString == "Shift+Space")     // Completely disable snapping
      mSnapContext = NO_SNAPPING;
   else if(inputString == "Tab")             // Turn on preview mode
      mPreviewMode = true;
   else if(checkPluginKeyBindings(inputString))
   {
      // Do nothing
   }

   return true;
}


void EditorUserInterface::onMouseClicked_left()
{
   if(InputCodeManager::getState(MOUSE_RIGHT))  // Prevent weirdness
      return;

   mDraggingDockItem = NULL;
   mMousePos.set(gScreenInfo.getMousePos());
   mJustInsertedVertex = false;

   if(mCreatingPoly || mCreatingPolyline)       // Save any polygon/polyline we might be creating
   {
      if(mNewItem->getVertCount() < 2)
      {
         delete mNewItem;
         removeUndoState();
      }
      else
      {
         mNewItem->addToEditor(getGame(), getDatabase());
         mNewItem->onGeomChanged();             // Add walls BEFORE onGeomChanged() is run!
      }

      mNewItem = NULL;

      mCreatingPoly = false;
      mCreatingPolyline = false;
   }

   mMouseDownPos = convertCanvasToLevelCoord(mMousePos);

   if(mouseOnDock())    // On the dock?  Did we hit something to start dragging off the dock?
   {
      clearSelection(getDatabase());
      mDraggingDockItem = mDockItemHit;
      SDL_SetCursor(Cursor::getSpray());
   }
   else                 // Mouse is not on dock
   {
      mDraggingDockItem = NULL;
      SDL_SetCursor(Cursor::getDefault());

      // rules for mouse down:
      // if the click has no shift- modifier, then
      //   if the click was on something that was selected
      //     do nothing
      //   else
      //     clear the selection
      //     add what was clicked to the selection
      //  else
      //    toggle the selection of what was clicked

      if(InputCodeManager::checkModifier(KEY_SHIFT))  // ==> Shift key is down
      {
         if(!mHitItem && mHitVertex != NONE)                   // If mHitItem is not NULL, we may have hit a point object
         {
            if(mHitItem->vertSelected(mHitVertex))
               mHitItem->unselectVert(mHitVertex);
            else
               mHitItem->aselectVert(mHitVertex);
         }
         else if(mHitItem)
         {
            mHitItem->setSelected(!mHitItem->isSelected());    // Toggle selection of hit item
            onSelectionChanged();
         }
         else
            mDragSelecting = true;
      }
      else                                            // ==> Shift key is NOT down
      {
         // If we hit a vertex of an already selected item --> now we can move that vertex w/o losing our selection.
         // Note that in the case of a point item, we want to skip this step, as we don't select individual vertices.
         if(mHitVertex != NONE && mHitItem && mHitItem->isSelected() && mHitItem->getGeomType() != geomPoint)
         {
            clearSelection(getDatabase());
            mHitItem->selectVert(mHitVertex);
            onSelectionChanged();
         }

         if(mHitItem && mHitItem->isSelected())    // Hit an already selected item
         {
            // Do nothing so user can drag a group of items that's already been selected
         }
         else if(mHitItem && mHitItem->getGeomType() == geomPoint)  // Hit a point item
         {
            clearSelection(getDatabase());
            mHitItem->setSelected(true);
            onSelectionChanged();
         }
         else if(mHitVertex != NONE && (mHitItem && !mHitItem->isSelected()))      // Hit a vertex of an unselected item
         {        // (braces required)
            if(!(mHitItem->vertSelected(mHitVertex)))
            {
               clearSelection(getDatabase());
               mHitItem->selectVert(mHitVertex);
               onSelectionChanged();
            }
         }
         else if(mHitItem)                                                          // Hit a non-point item, but not a vertex
         {
            clearSelection(getDatabase());
            mHitItem->setSelected(true);
            onSelectionChanged();
         }
         else     // Clicked off in space.  Starting to draw a bounding rectangle?
         {
            mDragSelecting = true;
            clearSelection(getDatabase());
            onSelectionChanged();
         }
      }
   }     // end mouse not on dock block, doc

   findSnapVertex();     // Update snap vertex in the event an item was selected
}


void EditorUserInterface::onMouseClicked_right()
{
   if(InputCodeManager::getState(MOUSE_LEFT) && !InputCodeManager::checkModifier(KEY_CTRL))  // Prevent weirdness
      return;  

   mMousePos.set(gScreenInfo.getMousePos());

   if(mCreatingPoly || mCreatingPolyline)
   {
      if(mNewItem->getVertCount() < gMaxPolygonPoints)            // Limit number of points in a polygon/polyline
      {
         mNewItem->addVert(snapPoint(getDatabase(), convertCanvasToLevelCoord(mMousePos)));
         mNewItem->onGeomChanging();
      }
         
      return;
   }

   saveUndoState(true);             // Save undo state before we clear the selection

   clearSelection(getDatabase());   // Unselect anything currently selected
   onSelectionChanged();

   // Can only add new vertices by clicking on item's edge, not it's interior (for polygons, that is)
   if(mEdgeHit != NONE && mHitItem && (mHitItem->getGeomType() == geomPolyLine || mHitItem->getGeomType() >= geomPolygon))
   {
      if(mHitItem->getVertCount() >= gMaxPolygonPoints)     // Polygon full -- can't add more
         return;

      Point newVertex = snapPoint(getDatabase(), convertCanvasToLevelCoord(mMousePos));   // adding vertex w/ right-mouse

      mAddingVertex = true;

      // Insert an extra vertex at the mouse clicked point, and then select it
      mHitItem->insertVert(newVertex, mEdgeHit + 1);
      mHitItem->selectVert(mEdgeHit + 1);
      mJustInsertedVertex = true;

      // Alert the item that its geometry is changing -- needed by polygons so they can recompute their fill
      mHitItem->onGeomChanging();

      // The user might just insert a vertex and be done; in that case we'll need to rebuild the wall outlines to account
      // for the new vertex.  If the user continues to drag the vertex to a new location, this will be wasted effort...
      mHitItem->onGeomChanged();


      mMouseDownPos = newVertex;
   }
   else     // Start creating a new poly or new polyline (tilde key + right-click ==> start polyline)
   { 
      if(InputCodeManager::getState(KEY_BACKQUOTE))   // Tilde  
      {
         mCreatingPolyline = true;
         mNewItem = new LineItem();
      }
      else
      {
         mCreatingPoly = true;
         mNewItem = new WallItem();
      }

      mNewItem->initializeEditor();
      mNewItem->setTeam(mCurrentTeam);
      mNewItem->addVert(snapPoint(getDatabase(), convertCanvasToLevelCoord(mMousePos)));
   }
}


// Returns true if key was handled, false if not
bool EditorUserInterface::checkPluginKeyBindings(string inputString)
{
   GameSettings *settings = getGame()->getSettings();
   const Vector<PluginBinding> *bindings = settings->getPluginBindings(); 

   for(S32 i = 0; i < bindings->size(); i++)
      if(inputString == bindings->get(i).key)
      {
         runPlugin(settings->getFolderManager(), bindings->get(i).script, Vector<string>());
         return true;
      }

   return false;
}


// Handle keyboard activity when we're editing an item's attributes
void EditorUserInterface::textEntryKeyHandler(InputCode inputCode, char ascii)
{
   if(inputCode == KEY_ENTER || inputCode == KEY_KEYPAD_ENTER)
   {
      if(entryMode == EntryID)
      {
         const Vector<EditorObject *> *objList = getDatabase()->getObjectList();

         for(S32 i = 0; i < objList->size(); i++)
         {
            EditorObject *obj = objList->get(i);

            if(obj->isSelected())             // Should only be one
            {
               U32 id = atoi(mEntryBox.c_str());
               if(obj->getItemId() != (S32)id)     // Did the id actually change?
               {
                  obj->setItemId(id);
                  mAllUndoneUndoLevel = -1;        // If so, it can't be undone
               }
               break;
            }
         }
      }
      else if(entryMode == EntryAngle)
      {
         F32 angle = (F32) atof(mEntryBox.c_str());
         rotateSelection(-angle);      // Positive angle should rotate CW, negative makes that happen
      }
      else if(entryMode == EntryScale)
      {
         F32 scale = (F32) atof(mEntryBox.c_str());
         scaleSelection(scale);
      }

      entryMode = EntryNone;
   }
   else if(inputCode == KEY_ESCAPE)
   {
      entryMode = EntryNone;
   }
   else if(inputCode == KEY_BACKSPACE || inputCode == KEY_DELETE)
      mEntryBox.handleBackspace(inputCode);

   else
      mEntryBox.addChar(ascii);

   // else ignore keystroke
}


void EditorUserInterface::startAttributeEditor()
{
   const Vector<EditorObject *> *objList = getDatabase()->getObjectList();

   for(S32 i = 0; i < objList->size(); i++)
   {
      EditorObject *obj_i = objList->get(i);
      if(obj_i->isSelected())
      {
         // Force item i to be the one and only selected item type.  This will clear up some problems that might otherwise
         // occur if you had different item types selected while you were editing attributes.   If you have multiple
         // items selected, all will end up with the same values, which only make sense if they are the same kind
         // of object.  So after this runs, there may be multiple items selected, but they'll all  be the same type.
         for(S32 j = 0; j < objList->size(); j++)
         {
            EditorObject *obj_j = objList->get(j);

            if(obj_j->isSelected() && obj_j->getObjectTypeNumber() != obj_i->getObjectTypeNumber())
               obj_j->unselect();
         }

         // Activate the attribute editor if there is one
         EditorAttributeMenuUI *menu = obj_i->getAttributeMenu();
         if(menu)
         {
            obj_i->setIsBeingEdited(true);
            menu->startEditingAttrs(obj_i);
            menu->activate();

            saveUndoState();
         }

         break;
      }
   }
}


void EditorUserInterface::onKeyUp(InputCode inputCode)
{
   switch(inputCode)
   {
      case KEY_UP:
         mIn = false;
         mUp = false;
         break;
      case KEY_W:
         mUp = false;
         break;
      case KEY_DOWN:
         mOut = false;
         mDown = false;
         break;
      case KEY_S:
         mDown = false;
         break;
      case KEY_LEFT:
      case KEY_A:
         mLeft = false;
         break;
      case KEY_RIGHT:
      case KEY_D:
         mRight = false;
         break;
      case KEY_E:
         mIn = false;
         break;
      case KEY_C:
         mOut = false;
         break;
      case KEY_SPACE:
         mSnapContext = FULL_SNAPPING;
         break;
      case KEY_TAB:
         mPreviewMode = false;
         break;
      case MOUSE_MIDDLE:
         mAutoScrollWithMouse = mAutoScrollWithMouseReady;
         break;
      case MOUSE_LEFT:
      case MOUSE_RIGHT:  
         mMousePos.set(gScreenInfo.getMousePos());

         if(mDragSelecting)      // We were drawing a rubberband selection box
         {
            Rect r(convertCanvasToLevelCoord(mMousePos), mMouseDownPos);

            fillVector.clear();

            getDatabase()->findObjects(fillVector);


            for(S32 i = 0; i < fillVector.size(); i++)
            {
               EditorObject *obj = dynamic_cast<EditorObject *>(fillVector[i]);

               // Make sure that all vertices of an item are inside the selection box; basically means that the entire 
               // item needs to be surrounded to be included in the selection
               S32 j;

               for(j = 0; j < obj->getVertCount(); j++)
                  if(!r.contains(obj->getVert(j)))
                     break;
               if(j == obj->getVertCount())
                  obj->setSelected(true);
            }
            mDragSelecting = false;
            onSelectionChanged();
         }

         else if(mDraggingObjects || mAddingVertex)     // We were dragging and dropping.  Could have been a move or a delete (by dragging to dock).
         {
            if(mAddingVertex)
            {
               //deleteUndoState();
               mAddingVertex = false;
            }
            
            onFinishedDragging();
         }

         break;

      default:
         break;
   }     // case
}


// Called when user has been dragging an object and then releases it
void EditorUserInterface::onFinishedDragging()
{
   mDraggingObjects = false;
   SDL_SetCursor(Cursor::getDefault());

   // Dragged item off the dock, then back on  ==> nothing changed; restore to unmoved state, which was stored on undo stack
   if(mouseOnDock() && mDraggingDockItem != NULL)
   {
      undo(false);
      return;
   }

   // Mouse is over the dock and we dragged something to the dock (probably a delete)
   if(mouseOnDock() && !mDraggingDockItem)
   {
      const Vector<EditorObject *> *objList = getDatabase()->getObjectList();
      bool deletedSomething = false, deletedWall = false;

      for(S32 i = 0; i < objList->size(); i++)    //  Delete all selected items
         if(objList->get(i)->isSelected())
         {
            if(isWallType(objList->get(i)->getObjectTypeNumber()))
               deletedWall = true;

            deleteItem(i, true);
            i--;
            deletedSomething = true;
         }

      // We deleted something, do some clean up and our job is done
      if(deletedSomething)
      {
         if(deletedWall)
            doneDeleteingWalls();

         doneDeleteing();

         return;
      }
   }

   // Mouse not on dock, we are either:
   // 1. dragging from the dock,
   // 2. moving something,
   // 3. or we moved something to the dock and nothing was deleted, e.g. when dragging a vertex
   // need to save an undo state if anything changed
   if(mDraggingDockItem == NULL)    // Not dragging from dock - user is moving object around screen, or dragging vertex to dock
   {
      // If our snap vertex has moved then all selected items have moved
      bool itemsMoved = mDragCopying || (mSnapObject && mSnapObject->getVert(mSnapVertexIndex) != mMoveOrigin);

      if(itemsMoved)    // Move consumated... update any moved items, and save our autosave
      {
         const Vector<EditorObject *> *objList = getDatabase()->getObjectList();

         for(S32 i = 0; i < objList->size(); i++)
            if(objList->get(i)->isSelected() || objList->get(i)->anyVertsSelected())
               objList->get(i)->onGeomChanged();

         setNeedToSave(true);
         autoSave();

         mDragCopying = false;

         return;
      }
      else if(!mJustInsertedVertex)    // We started our move, then didn't end up moving anything... remove associated undo state
         deleteUndoState();
      else
         mJustInsertedVertex = false;
   }
}


bool EditorUserInterface::mouseOnDock()
{
   return (mMousePos.x >= gScreenInfo.getGameCanvasWidth() - DOCK_WIDTH - horizMargin &&
           mMousePos.x <= gScreenInfo.getGameCanvasWidth() - horizMargin &&
           mMousePos.y >= gScreenInfo.getGameCanvasHeight() - vertMargin - getDockHeight() &&
           mMousePos.y <= gScreenInfo.getGameCanvasHeight() - vertMargin);
}


S32 EditorUserInterface::getItemSelectedCount()
{
   const Vector<EditorObject *> *objList = getDatabase()->getObjectList();

   S32 count = 0;

   for(S32 i = 0; i < objList->size(); i++)
      if(objList->get(i)->isSelected())
         count++;

   return count;
}


bool EditorUserInterface::anythingSelected()
{
   const Vector<EditorObject *> *objList = getDatabase()->getObjectList();

   for(S32 i = 0; i < objList->size(); i++)
      if(objList->get(i)->isSelected() || objList->get(i)->anyVertsSelected() )
         return true;

   return false;
}


void EditorUserInterface::idle(U32 timeDelta)
{
   Parent::idle(timeDelta);

   F32 pixelsToScroll = timeDelta * (InputCodeManager::getState(KEY_SHIFT) ? 1.0f : 0.5f);    // Double speed when shift held down

   if(mLeft && !mRight)
      mCurrentOffset.x += pixelsToScroll;
   else if(mRight && !mLeft)
      mCurrentOffset.x -= pixelsToScroll;
   if(mUp && !mDown)
      mCurrentOffset.y += pixelsToScroll;
   else if(mDown && !mUp)
      mCurrentOffset.y -= pixelsToScroll;

   if(mAutoScrollWithMouse)
   {
      mCurrentOffset += (mScrollWithMouseLocation - mMousePos) * pixelsToScroll / 128.f;
      onMouseMoved();  // Prevents skippy problem while dragging something
   }

//   Point mouseLevelPoint = convertCanvasToLevelCoord(mMousePos);

   if(mIn && !mOut)
      zoom(timeDelta * 0.002f);
   else if(mOut && !mIn)
      zoom(timeDelta * -0.002f);

   mSaveMsgTimer.update(timeDelta);
   mWarnMsgTimer.update(timeDelta);
}


void EditorUserInterface::setSaveMessage(string msg, bool savedOK)
{
   mSaveMsg = msg;
   mSaveMsgTimer = saveMsgDisplayTime;
   mSaveMsgColor = (savedOK ? Colors::green : Colors::red);
}


void EditorUserInterface::setWarnMessage(string msg1, string msg2)
{
   mWarnMsg1 = msg1;
   mWarnMsg2 = msg2;
   mWarnMsgTimer = warnMsgDisplayTime;
   mWarnMsgColor = gErrorMessageTextColor;
}


bool EditorUserInterface::saveLevel(bool showFailMessages, bool showSuccessMessages, bool autosave)
{
   string saveName = autosave ? "auto.save" : mEditFileName;

   try
   {
      // Check if we have a valid (i.e. non-null) filename
      if(saveName == "")
      {
         ErrorMessageUserInterface *ui = getUIManager()->getErrorMsgUserInterface();

         ui->reset();
         ui->setTitle("INVALID FILE NAME");
         ui->setMessage(1, "The level file name is invalid or empty.  The level cannot be saved.");
         ui->setMessage(2, "To correct the problem, please change the file name using the");
         ui->setMessage(3, "Game Parameters menu, which you can access by pressing [F3].");

         ui->activate();

         return false;
      }

      FolderManager *folderManager = getGame()->getSettings()->getFolderManager();

      string fileName = joindir(folderManager->levelDir, saveName).c_str();

      FILE *f = fopen(fileName.c_str(), "w");
      if(!f)
         throw(SaveException("Could not open file for writing"));

      // Write out basic game parameters, including gameType info
      s_fprintf(f, "%s", getGame()->toString().c_str());    // Note that this toString appends a newline char; most don't

      // Next come the robots
      for(S32 i = 0; i < robots.size(); i++)
         s_fprintf(f, "%s\n", robots[i].c_str());

      // Write out all level items (do two passes; walls first, non-walls next, so turrets & forcefields have something to grab onto)
      const Vector<EditorObject *> *objList = getDatabase()->getObjectList();

      for(S32 j = 0; j < 2; j++)
         for(S32 i = 0; i < objList->size(); i++)
         {
            EditorObject *p = objList->get(i);

            // Writing wall items on first pass, non-wall items next -- that will make sure mountable items have something to grab onto
            if((j == 0 && isWallType(p->getObjectTypeNumber())) || (j == 1 && ! isWallType(p->getObjectTypeNumber())) )
               p->saveItem(f, getGame()->getGridSize());
         }
      fclose(f);
   }
   catch (SaveException &e)
   {
      if(showFailMessages)
         setSaveMessage("Error Saving: " + string(e.what()), false);
      return false;
   }

   if(!autosave)     // Doesn't count as a save!
   {
      mNeedToSave = false;
      mAllUndoneUndoLevel = mLastUndoIndex;     // If we undo to this point, we won't need to save
   }

   if(showSuccessMessages)
      setSaveMessage("Saved " + getLevelFileName(), true);

   return true;
}


// We need some local hook into the testLevelStart() below.  Ugly but apparently necessary.
void testLevelStart_local(ClientGame *game)
{
   game->getUIManager()->getEditorUserInterface()->testLevelStart();
}


extern void initHostGame(GameSettings *settings, const Vector<string> &levelList, bool testMode, bool dedicatedServer);

void EditorUserInterface::testLevel()
{
   bool gameTypeError = false;
   if(!getGame()->getGameType())     // Not sure this could really happen anymore...  TODO: Make sure we always have a valid gametype
      gameTypeError = true;

   // With all the map loading error fixes, game should never crash!
   validateLevel();
   if(mLevelErrorMsgs.size() || mLevelWarnings.size() || gameTypeError)
   {
      YesNoUserInterface *ui = getUIManager()->getYesNoUserInterface();

      ui->reset();
      ui->setTitle("LEVEL HAS PROBLEMS");

      S32 line = 1;
      for(S32 i = 0; i < mLevelErrorMsgs.size(); i++)
         ui->setMessage(line++, mLevelErrorMsgs[i].c_str());

      for(S32 i = 0; i < mLevelWarnings.size(); i++)
         ui->setMessage(line++, mLevelWarnings[i].c_str());

      if(gameTypeError)
      {
         ui->setMessage(line++, "ERROR: GameType is invalid.");
         ui->setMessage(line++, "(Fix in Level Parameters screen [F3])");
      }

      ui->setInstr("Press [Y] to start, [ESC] to cancel");
      ui->registerYesFunction(testLevelStart_local);   // testLevelStart_local() just calls testLevelStart() below
      ui->activate();

      return;
   }

   testLevelStart();
}


void EditorUserInterface::testLevelStart()
{
   string tmpFileName = mEditFileName;
   mEditFileName = "editor.tmp";

   SDL_SetCursor(Cursor::getTransparent());  // Turn off cursor
   bool nts = mNeedToSave;             // Save these parameters because they are normally reset when a level is saved.
   S32 auul = mAllUndoneUndoLevel;     // Since we're only saving a temp copy, we really shouldn't reset them...

   mEditorGameType = getGame()->getGameType();     // Sock our current gametype away, will use it when we reenter the editor

   if(saveLevel(true, false))
   {
      mEditFileName = tmpFileName;     // Restore the level name

      mWasTesting = true;

      Vector<string> levelList;
      levelList.push_back("editor.tmp");
      initHostGame(getGame()->getSettings(), levelList, true, false);
   }

   mNeedToSave = nts;                  // Restore saved parameters
   mAllUndoneUndoLevel = auul;
}


////////////////////////////////////////
////////////////////////////////////////


// Constructor
EditorMenuUserInterface::EditorMenuUserInterface(ClientGame *game) : Parent(game)
{
   setMenuID(EditorMenuUI);
   mMenuTitle = "EDITOR MENU";
}


void EditorMenuUserInterface::onActivate()
{
   Parent::onActivate();
   setupMenus();
}


extern MenuItem *getWindowModeMenuItem(U32 displayMode);

//////////
// Editor menu callbacks
//////////

void reactivatePrevUICallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->reactivatePrevUI();
}


static void testLevelCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->getEditorUserInterface()->testLevel();
}


void returnToEditorCallback(ClientGame *game, U32 unused)
{
   EditorUserInterface *ui = game->getUIManager()->getEditorUserInterface();

   ui->saveLevel(true, true);                                     // Save level
   ui->setSaveMessage("Saved " + ui->getLevelFileName(), true);   // Setup a message for the user
   game->getUIManager()->reactivatePrevUI();                      // Return to editor
}


static void activateHelpCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->getEditorInstructionsUserInterface()->activate();
}


static void activateLevelParamsCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->getGameParamUserInterface()->activate();
}


static void activateTeamDefCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->getTeamDefUserInterface()->activate();
}


void quitEditorCallback(ClientGame *game, U32 unused)
{
   EditorUserInterface *editorUI = game->getUIManager()->getEditorUserInterface();

   if(editorUI->getNeedToSave())
   {
      YesNoUserInterface *ui = game->getUIManager()->getYesNoUserInterface();

      ui->reset();
      ui->setInstr("Press [Y] to save, [N] to quit [ESC] to cancel");
      ui->setTitle("SAVE YOUR EDITS?");
      ui->setMessage(1, "You have not saved your edits to the level.");
      ui->setMessage(3, "Do you want to?");
      ui->registerYesFunction(saveLevelCallback);
      ui->registerNoFunction(backToMainMenuCallback);
      ui->activate();
   }
   else
     backToMainMenuCallback(game);
}

//////////

void EditorMenuUserInterface::setupMenus()
{
   GameSettings *settings = getGame()->getSettings();
   clearMenuItems();
   addMenuItem(new MenuItem("RETURN TO EDITOR", reactivatePrevUICallback,    "", KEY_R));
   addMenuItem(getWindowModeMenuItem((U32)settings->getIniSettings()->displayMode));
   addMenuItem(new MenuItem("TEST LEVEL",       testLevelCallback,           "", KEY_T));
   addMenuItem(new MenuItem("SAVE LEVEL",       returnToEditorCallback,      "", KEY_S));
   addMenuItem(new MenuItem("INSTRUCTIONS",     activateHelpCallback,        "", KEY_I, getInputCode(settings, InputCodeManager::BINDING_HELP)));
   addMenuItem(new MenuItem("LEVEL PARAMETERS", activateLevelParamsCallback, "", KEY_L, KEY_F3));
   addMenuItem(new MenuItem("MANAGE TEAMS",     activateTeamDefCallback,     "", KEY_M, KEY_F2));
   addMenuItem(new MenuItem("QUIT",             quitEditorCallback,          "", KEY_Q, KEY_UNKNOWN));
}


void EditorMenuUserInterface::onEscape()
{
   SDL_SetCursor(Cursor::getTransparent());
   getUIManager()->reactivatePrevUI();
}


};

