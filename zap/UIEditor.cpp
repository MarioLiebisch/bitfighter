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

#include "UINameEntry.h"
#include "UIEditorInstructions.h"
#include "UIChat.h"
#include "UIDiagnostics.h"
#include "UITeamDefMenu.h"
#include "UIGameParameters.h"
#include "UIErrorMessage.h"
#include "UIYesNo.h"
#include "gameObjectRender.h"
#include "game.h"                // For EditorGame def
#include "gameType.h"
#include "soccerGame.h"          // For Soccer ball radius
#include "engineeredObjects.h"   // For Turret properties
#include "barrier.h"             // For BarrierWidth
#include "gameItems.h"           // For Asteroid defs
#include "teleporter.h"          // For Teleporter def
#include "speedZone.h"           // For Speedzone def
#include "loadoutZone.h"         // For LoadoutZone def
#include "huntersGame.h"         // For HuntersNexusObject def
#include "config.h"

#include "gameLoader.h"          // For LevelLoadException def

#include "GeomUtils.h"
#include "textItem.h"            // For MAX_TEXTITEM_LEN and MAX_TEXT_SIZE
#include "luaLevelGenerator.h"
#include "stringUtils.h"
#include "../glut/glutInclude.h"

#include "oglconsole.h"          // Our console object

#include <ctype.h>
#include <exception>
#include <algorithm>             // For sort
#include <math.h>

using namespace boost;

namespace Zap
{

EditorUserInterface gEditorUserInterface;

const S32 DOCK_WIDTH = 50;
const F32 MIN_SCALE = .05;        // Most zoomed-in scale
const F32 MAX_SCALE = 2.5;        // Most zoomed-out scale
const F32 STARTING_SCALE = 0.5;   

extern Color gNexusOpenColor;
extern Color EDITOR_WALL_FILL_COLOR;

static const Color inactiveSpecialAttributeColor = Color(.6, .6, .6);


static const S32 TEAM_NEUTRAL = Item::TEAM_NEUTRAL;
static const S32 TEAM_HOSTILE = Item::TEAM_HOSTILE;

static Vector<boost::shared_ptr<EditorObject> > *mLoadTarget;

enum EntryMode {
   EntryID,          // Entering an objectID
   EntryAngle,       // Entering an angle
   EntryScale,       // Entering a scale
   EntryNone         // Not in a special entry mode
};


static EntryMode entryMode;
static Vector<ZoneBorder> zoneBorders;

void saveLevelCallback()
{
   if(gEditorUserInterface.saveLevel(true, true))
      UserInterface::reactivateMenu(gMainMenuUserInterface);
   else
      gEditorUserInterface.reactivate();
}


void backToMainMenuCallback()
{
   UserInterface::reactivateMenu(gMainMenuUserInterface);
}


extern EditorGame *gEditorGame;

// Constructor
EditorUserInterface::EditorUserInterface() : mGridDatabase(GridDatabase(false))     // false --> not using game coords
{
   setMenuID(EditorUI);

   // Create some items for the dock...  One of each, please!
   mShowMode = ShowAllObjects; 
   mWasTesting = false;

   mSnapVertex_i = NULL;
   mSnapVertex_j = NONE;
   mItemHit = NULL;
   mEdgeHit = NONE;

   mLastUndoStateWasBarrierWidthChange = false;

   mUndoItems.resize(UNDO_STATES);

   // Pass the gridDatabase on to these other objects, so they can have local access
   WallSegment::setGridDatabase(&mGridDatabase);      // Still needed?  Can do this via editorGame?
   WallSegmentManager::setGridDatabase(&mGridDatabase);
}


static const S32 DOCK_POLY_HEIGHT = 20;
static const S32 DOCK_POLY_WIDTH = DOCK_WIDTH - 10;

void EditorUserInterface::addToDock(EditorObject* object)
{
   mDockItems.push_back(boost::shared_ptr<EditorObject>(object));
}


void EditorUserInterface::addToEditor(EditorObject* object)
{
   mItems.push_back(boost::shared_ptr<EditorObject>(object));
}


void EditorUserInterface::addDockObject(EditorObject *object, F32 xPos, F32 yPos)
{
   object->addToDock(gEditorGame, Point(xPos, yPos));       
   object->setTeam(mCurrentTeam);
}


void EditorUserInterface::populateDock()
{
   mDockItems.clear();

   if(mShowMode == ShowAllObjects)
   {
      S32 xPos = gScreenInfo.getGameCanvasWidth() - horizMargin - DOCK_WIDTH / 2;
      S32 yPos = 35;
      const S32 spacer = 35;

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

      if(gEditorGame->getGameType()->getGameType() == GameType::SoccerGame)
         addDockObject(new SoccerBallItem(), xPos, yPos);
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

      // These two will share a line
      addDockObject(new TestItem(), xPos - 10, yPos);
      addDockObject(new ResourceItem(), xPos + 10, yPos);
      yPos += 25;

      
      addDockObject(new LoadoutZone(), xPos, yPos);
      yPos += 25;

      if(gEditorGame->getGameType()->getGameType() == GameType::NexusGame)
      {
         addDockObject(new HuntersNexusObject(), xPos, yPos);
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
}


// Destructor -- unwind things in an orderly fashion
EditorUserInterface::~EditorUserInterface()
{
   mItems.clear();
   mDockItems.clear();
   mLevelGenItems.clear();
   mClipboard.deleteAndClear();
   delete mNewItem;

   //for(S32 i = 0; i < UNDO_STATES; i++)
   //   mUndoItems[i].deleteAndClear();
}


static const S32 NO_NUMBER = -1;

// Draw a vertex of a selected editor item  -- still used for snapping vertex
void renderVertex(VertexRenderStyles style, const Point &v, S32 number, F32 alpha, S32 size)
{
   bool hollow = style == HighlightedVertex || style == SelectedVertex || style == SelectedItemVertex || style == SnappingVertex;

   // Fill the box with a dark gray to make the number easier to read
   if(hollow && number != NO_NUMBER)
   {
      glColor3f(.25, .25, .25);
      drawFilledSquare(v, size);
   }
      
   if(style == HighlightedVertex)
      glColor(HIGHLIGHT_COLOR, alpha);
   else if(style == SelectedVertex)
      glColor(SELECT_COLOR, alpha);
   else if(style == SnappingVertex)
      glColor(magenta, alpha);
   else
      glColor(red, alpha);

   drawSquare(v, size, !hollow);

   if(number != NO_NUMBER)     // Draw vertex numbers
   {
      glColor(white, alpha);
      UserInterface::drawStringf(v.x - UserInterface::getStringWidthf(6, "%d", number) / 2, v.y - 3, 6, "%d", number);
   }
}


void renderVertex(VertexRenderStyles style, const Point &v, S32 number, F32 alpha)
{
   renderVertex(style, v, number, alpha, 5);
}


void renderVertex(VertexRenderStyles style, const Point &v, S32 number)
{
   renderVertex(style, v, number, 1);
}


inline F32 getGridSize()
{
   return gEditorGame->getGridSize();
}


// Replaces the need to do a convertLevelToCanvasCoord on every point before rendering
static void setLevelToCanvasCoordConversion()
{
   F32 scale =  gEditorUserInterface.getCurrentScale();
   Point offset = gEditorUserInterface.getCurrentOffset();

   glTranslatef(offset.x, offset.y, 0);
   glScalef(scale, scale, 1);
} 


// Draws a line connecting points in mVerts
void EditorUserInterface::renderPolyline(const Vector<Point> *verts)
{
   glPushMatrix();
      setLevelToCanvasCoordConversion();
      renderPointVector(verts, GL_LINE_STRIP);
   glPopMatrix();
}


extern Color gNeutralTeamColor;
extern Color gHostileTeamColor;


inline F32 getCurrentScale()
{
   return gEditorUserInterface.getCurrentScale();
}


inline Point convertLevelToCanvasCoord(const Point &point, bool convert = true) 
{ 
   return gEditorUserInterface.convertLevelToCanvasCoord(point, convert); 
}


////////////////////////////////////
////////////////////////////////////

// Objects created with this method MUST be deleted!
// Returns NULL if className is invalid
static EditorObject *newEditorObject(const char *className)
{
   Object *theObject = Object::create(className);        // Create an object of the specified type
   TNLAssert(dynamic_cast<EditorObject *>(theObject), "This is not an EditorObject!");

   return dynamic_cast<EditorObject *>(theObject);       // Force our new object to be an EditorObject
}


// Removes most recent undo state from stack --> won't actually delete items on stack until we need the slot, or we quit
void EditorUserInterface::deleteUndoState()
{
   mLastUndoIndex--;
   mLastRedoIndex--; 
}


// Experimental save to string method
static void copyItems(Vector<boost::shared_ptr<EditorObject> > &from, Vector<string> &to)
{
   to.resize(from.size());      // Preallocation makes things go faster

   for(S32 i = 0; i < from.size(); i++)
      to[i] = from[i]->toString();
}


// TODO: Make this an UIEditor method, and get rid of the global
static void restoreItems(const Vector<string> &from, Vector<boost::shared_ptr<EditorObject> > &to)
{
   to.clear();
   to.reserve(from.size());      // Preallocation makes things go faster

   Vector<string> args;

   for(S32 i = 0; i < from.size(); i++)
   {
      args = parseString(from[i]);
      EditorObject *newObject = newEditorObject(args[0].c_str());

      S32 args_count = 0;
      const char *args_char[LevelLoader::MAX_LEVEL_LINE_ARGS];  // Convert to a format processArgs will allow
         
      // Skip the first arg because we've already handled that one above
      for(S32 j = 1; j < args.size() && j < LevelLoader::MAX_LEVEL_LINE_ARGS; j++)
      {
         args_char[j-1] = args[j].c_str();
         args_count++;
      }

      newObject->addToEditor(gEditorGame);
      newObject->processArguments(args_count, args_char);
   }
}


static void copyItems(const Vector<EditorObject *> &from, Vector<EditorObject *> &to)
{
   to.deleteAndClear();
   
   to.resize(from.size());      // Preallocation makes things go faster

   for(S32 i = 0; i < from.size(); i++)
      to[i] = from[i]->newCopy();
}


// Save the current state of the editor objects for later undoing
void EditorUserInterface::saveUndoState()
{
   // Use case: We do 5 actions, save, undo 2, redo 1, then do some new action.  
   // Our "no need to save" undo point is lost forever.
   if(mAllUndoneUndoLevel > mLastRedoIndex)     
      mAllUndoneUndoLevel = NONE;

   copyItems(mItems, mUndoItems[mLastUndoIndex % UNDO_STATES]);

   mLastUndoIndex++;
   mLastRedoIndex++; 


   if(mLastUndoIndex % UNDO_STATES == mFirstUndoIndex % UNDO_STATES)           // Undo buffer now full...
   {
      mFirstUndoIndex++;
      mAllUndoneUndoLevel -= 1;     // If this falls below 0, then we can't undo our way out of needing to save
   }
   
   mNeedToSave = (mAllUndoneUndoLevel != mLastUndoIndex);
   mRedoingAnUndo = false;
   mLastUndoStateWasBarrierWidthChange = false;
}


void EditorUserInterface::autoSave()
{
   saveLevel(false, false, true);
}


void EditorUserInterface::undo(bool addToRedoStack)
{
   if(!undoAvailable())
      return;

   mSnapVertex_i = NULL;
   mSnapVertex_j = NONE;

   if(mLastUndoIndex == mLastRedoIndex && !mRedoingAnUndo)
   {
      saveUndoState();
      mLastUndoIndex--;
      mLastRedoIndex--;
      mRedoingAnUndo = true;
   }

   mLastUndoIndex--;
   mItems.clear();
   restoreItems(mUndoItems[mLastUndoIndex % UNDO_STATES], mItems);

   rebuildEverything();

   mLastUndoStateWasBarrierWidthChange = false;
   validateLevel();
}
   

void EditorUserInterface::redo()
{
   if(mLastRedoIndex != mLastUndoIndex)      // If there's a redo state available...
   {
      mSnapVertex_i = NULL;
      mSnapVertex_j = NONE;

      mLastUndoIndex++;
      mItems.clear();
      restoreItems(mUndoItems[mLastUndoIndex % UNDO_STATES], mItems);   

      rebuildEverything();
      validateLevel();
   }
}


void EditorUserInterface::rebuildEverything()
{
   mWallSegmentManager.recomputeAllWallGeometry();
   resnapAllEngineeredItems();

   mNeedToSave = (mAllUndoneUndoLevel != mLastUndoIndex);
   mItemToLightUp = NULL;
   autoSave();
}


static Vector<DatabaseObject *> fillVector;     // Reusable container

void EditorUserInterface::resnapAllEngineeredItems()
{
   fillVector.clear();

   gEditorGame->getGridDatabase()->findObjects(EngineeredType, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      EngineeredObject *engrObj = dynamic_cast<EngineeredObject *>(fillVector[i]);

      //mountToWall(engrObj, engrObj->getVert(0));
      engrObj->mountToWall(engrObj->getVert(0));
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
      if(name.find('.') == std::string::npos)      // Append extension, if one is needed
         mEditFileName = name + ".level";
      // else... what?
}


void EditorUserInterface::setLevelGenScriptName(string line)
{
   mScriptLine = trim(line);
}


void EditorUserInterface::makeSureThereIsAtLeastOneTeam()
{
   if(mTeams.size() == 0)
   {
      TeamEditor t;
      t.setName(gTeamPresets[0].name);
      t.color.set(gTeamPresets[0].r, gTeamPresets[0].g, gTeamPresets[0].b);
      mTeams.push_back(t);
   }
}


// This sort will put points on top of lines on top of polygons...  as they should be
// NavMeshZones are now drawn on top, to make them easier to see.  Disable with Ctrl-A!
// We'll also put walls on the bottom, as this seems to work best in practice
S32 QSORT_CALLBACK geometricSort(boost::shared_ptr<EditorObject> a, boost::shared_ptr<EditorObject> b)
{
   if((a)->getObjectTypeMask() & BarrierType)
      return -1;
   if((b)->getObjectTypeMask() & BarrierType)
      return 1;

   return( (a)->getGeomType() - (b)->getGeomType() );
}


static void geomSort(Vector<boost::shared_ptr<EditorObject> >& objects)
{
   if(objects.size() >= 2)  // nothing to sort when there is one or zero objects
      // Cannot use Vector.sort() here because I couldn't figure out how to cast shared_ptr as pointer (*)
      sort(objects.getStlVector().begin(), objects.getStlVector().end(), geometricSort);
}


extern const char *gGameTypeNames[];
extern S32 gDefaultGameTypeIndex;
extern S32 gMaxPolygonPoints;
extern ConfigDirectories gConfigDirs;

// Loads a level
void EditorUserInterface::loadLevel()
{
   // Initialize
   mItems.clear();
   mTeams.clear();
   mSnapVertex_i = NULL;
   mSnapVertex_j = NONE;
   mAddingVertex = false;
   clearLevelGenItems();
   mLoadTarget = &mItems;
   mGameTypeArgs.clear();
   gGameParamUserInterface.gameParams.clear();
   gGameParamUserInterface.savedMenuItems.clear();          // clear() because this is not a pointer vector
   gGameParamUserInterface.menuItems.deleteAndClear();      // Keeps interface from using our menuItems to rebuild savedMenuItems
   gEditorGame->setGridSize(Game::DefaultGridSize);         // Used in editor for scaling walls and text items appropriately

   gEditorGame->setGameType(new GameType());

   char fileBuffer[1024];
   dSprintf(fileBuffer, sizeof(fileBuffer), "%s/%s", gConfigDirs.levelDir.c_str(), mEditFileName.c_str());

   if(gEditorGame->loadLevelFromFile(fileBuffer))   // Process level file --> returns true if file found and loaded, false if not (assume it's a new level)
   {
      // Loaded a level!
      makeSureThereIsAtLeastOneTeam(); // Make sure we at least have one team
      validateTeams();                 // Make sure every item has a valid team
      validateLevel();                 // Check level for errors (like too few spawns)
      //mItems.sort(geometricSort);
      geomSort(mItems);
      gGameParamUserInterface.ignoreGameParams = false;
   }
   else     
   {
      // New level!
      makeSureThereIsAtLeastOneTeam();                               // Make sure we at least have one team, like the man said.
      gGameParamUserInterface.gameParams.push_back("GameType 10 8"); // A nice, generic game type that we can default to

      if(gIniSettings.name != gIniSettings.defaultName)
         gGameParamUserInterface.gameParams.push_back("LevelCredits " + gIniSettings.name);  // Prepoluate level credits

      gGameParamUserInterface.ignoreGameParams = true;               // Don't rely on the above for populating GameParameters menus... only to make sure something is there if we save
   }
   clearUndoHistory();                 // Clean out undo/redo buffers
   clearSelection();                   // Nothing starts selected
   mShowMode = ShowAllObjects;         // Turn everything on
   mNeedToSave = false;                // Why save when we just loaded?
   mAllUndoneUndoLevel = mLastUndoIndex;
   populateDock();                     // Add game-specific items to the dock

   // Bulk-process new items, walls first
   for(S32 i = 0; i < mItems.size(); i++)
      mItems[i]->processEndPoints();

   mWallSegmentManager.recomputeAllWallGeometry();
   
   // Snap all engineered items to the closest wall, if one is found
   resnapAllEngineeredItems();

   // Run onGeomChanged for all non-wall items (engineered items already had onGeomChanged run during resnap operation)
   for(S32 i = 0; i < mItems.size(); i++)
      if(mItems[i]->getObjectTypeMask() & ~(BarrierType | EngineeredType))
         mItems[i]->onGeomChanged();
}


extern OGLCONSOLE_Console gConsole;

void EditorUserInterface::clearLevelGenItems()
{
   mLevelGenItems.clear();
}


extern void removeCollinearPoints(Vector<Point> &points, bool isPolygon);

void EditorUserInterface::copyScriptItemsToEditor()
{
   if(mLevelGenItems.size() == 0)
      return;     // Print error message?

   saveUndoState();

   for(S32 i = 0; i < mLevelGenItems.size(); i++)
      mItems.push_back(mLevelGenItems[i]);

   mLevelGenItems.clear();    // Don't want to delete these objects... we just handed them off to mItems!

   rebuildEverything();

   mLastUndoStateWasBarrierWidthChange = false;
}


void EditorUserInterface::runLevelGenScript()
{
   // Parse mScriptLine 
   if(mScriptLine == "")      // No script included!!
      return;

   OGLCONSOLE_Output(gConsole, "Running script %s\n", mScriptLine.c_str());

   Vector<string> scriptArgs = parseString(mScriptLine);
   
   string scriptName = scriptArgs[0];
   scriptArgs.erase(0);

   clearLevelGenItems();      // Clear out any items from the last run

   // Set the load target to the levelgen list, as that's where we want our items stored
   mLoadTarget = &mLevelGenItems;

   runScript(scriptName, scriptArgs);

   // Reset the target
   mLoadTarget = &mItems;
}


// Runs an arbitrary lua script.  Command is first item in cmdAndArgs, subsequent items are the args, if any
void EditorUserInterface::runScript(const string &scriptName, const Vector<string> &args)
{
   string name = ConfigDirectories::findLevelGenScript(scriptName);  // Find full name of levelgen script

   if(name == "")
   {
      logprintf(LogConsumer::LogWarning, "Warning: Could not find script \"%s\"",  scriptName.c_str());
      // TODO: Show an error to the user
      return;
   }

   // TODO: Uncomment the following, make it work again (commented out during refactor of editor load process)
   // Load the items
   //LuaLevelGenerator(name, args, gEditorGame->getGridSize(), getGridDatabase(), this, gConsole);
   
   // Process new items
   // Not sure about all this... may need to test
   // Bulk-process new items, walls first
   for(S32 i = 0; i < mLoadTarget->size(); i++)
      if((*mLoadTarget)[i]->getObjectTypeMask() & WallType)
      {
         if((*mLoadTarget)[i]->getVertCount() < 2)      // Invalid item; delete
         {
            (*mLoadTarget).erase(i);
            i--;
         }

         (*mLoadTarget)[i]->processEndPoints();
      }
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
   foundSpawn.resize(mTeams.size());
   for(S32 i = 0; i < mTeams.size(); i++)      // Initialize vector
      foundSpawn[i] = false;
      
   fillVector.clear();
   gEditorGame->getGridDatabase()->findObjects(ShipSpawnType, fillVector);
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
   gEditorGame->getGridDatabase()->findObjects(SoccerBallItemType, fillVector);
   if(fillVector.size() > 0)
      foundSoccerBall = true;

   fillVector.clear();
   gEditorGame->getGridDatabase()->findObjects(NexusType, fillVector);
   if(fillVector.size() > 0)
      foundNexus = true;

   fillVector.clear();
   gEditorGame->getGridDatabase()->findObjects(FlagType, fillVector);
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
   gEditorGame->getGridDatabase()->findObjects(FlagSpawnType, fillVector);
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

   // Check for soccer ball in a a game other than SoccerGameType. Doesn't crash no more.
   if(foundSoccerBall && gEditorGame->getGameType()->getGameType() != GameType::SoccerGame)
      mLevelWarnings.push_back("WARNING: Soccer ball can only be used in soccer game.");

   // Check for the nexus object in a non-hunter game. Does not affect gameplay in non-hunter game.
   if(foundNexus && gEditorGame->getGameType()->getGameType() != GameType::NexusGame)
      mLevelWarnings.push_back("WARNING: Nexus object can only be used in Hunters game.");

   // Check for missing nexus object in a hunter game.  This cause mucho dolor!
   if(!foundNexus && gEditorGame->getGameType()->getGameType() == GameType::NexusGame)
      mLevelErrorMsgs.push_back("ERROR: Nexus game must have a Nexus.");

   if(foundFlags && !gEditorGame->getGameType()->isFlagGame())
      mLevelWarnings.push_back("WARNING: This game type does not use flags.");
   else if(foundTeamFlags && !gEditorGame->getGameType()->isTeamFlagGame())
      mLevelWarnings.push_back("WARNING: This game type does not use team flags.");

   // Check for team flag spawns on games with no team flags
   if(foundTeamFlagSpawns && !foundTeamFlags)
      mLevelWarnings.push_back("WARNING: Found team flag spawns but no team flags.");

   // Errors that may be corrected by levelgen -- script could add spawns
   // Neutral spawns work for all; if there's one, then that will satisfy our need for spawns for all teams
   if(mScriptLine == "" && !foundNeutralSpawn)
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


// Check that each item has a valid team  (fixes any problems it finds)
void EditorUserInterface::validateTeams()
{
   S32 teams = mTeams.size();

   for(S32 i = 0; i < mItems.size(); i++)
   {
      S32 team = mItems[i]->getTeam();

      if(mItems[i]->hasTeam() && ((team >= 0 && team < teams) || team == TEAM_NEUTRAL || team == TEAM_HOSTILE))  
         continue;      // This one's OK

      if(team == TEAM_NEUTRAL && mItems[i]->canBeNeutral())
         continue;      // This one too

      if(team == TEAM_HOSTILE && mItems[i]->canBeHostile())
         continue;      // This one too

      if(mItems[i]->hasTeam())
         mItems[i]->setTeam(0);               // We know there's at least one team, so there will always be a team 0
      else if(mItems[i]->canBeHostile() && !mItems[i]->canBeNeutral())
         mItems[i]->setTeam(TEAM_HOSTILE); 
      else
         mItems[i]->setTeam(TEAM_NEUTRAL);    // We won't consider the case where hasTeam == canBeNeutral == canBeHostile == false
   }
}


// Search through editor objects, to make sure everything still has a valid team.  If not, we'll assign it a default one.
// Note that neutral/hostile items are on team -1/-2, and will be unaffected by this loop or by the number of teams we have.
void EditorUserInterface::teamsHaveChanged()
{
   bool teamsChanged = false;

   if(mTeams.size() != mOldTeams.size())
      teamsChanged = true;
   else
      for(S32 i = 0; i < mTeams.size(); i++)
         if(mTeams[i].color != mOldTeams[i].color || mTeams[i].getName() != mOldTeams[i].getName())
         {
            teamsChanged = true;
            break;
         }

   if(!teamsChanged)       // Nothing changed, we're done here
      return;

   for (S32 i = 0; i < mItems.size(); i++)
      if(mItems[i]->getTeam() >= mTeams.size())       // Team no longer valid?
         mItems[i]->setTeam(0);                    // Then set it to first team

   // And the dock items too...
   for (S32 i = 0; i < mDockItems.size(); i++)
      if(mDockItems[i]->getTeam() >= mTeams.size())
         mDockItems[i]->setTeam(0);

   validateLevel();          // Revalidate level -- if teams have changed, requirements for spawns have too
   mNeedToSave = true;
   autoSave();
   mAllUndoneUndoLevel = -1; // This change can't be undone
}


string EditorUserInterface::getLevelFileName()
{
   return mEditFileName;
}


// Handle console input
// Valid commands: help, run, clear, quit, exit
void processEditorConsoleCommand(OGLCONSOLE_Console console, char *cmdline)
{
   Vector<string> words = parseString(cmdline);
   if(words.size() == 0)
      return;

   string cmd = lcase(words[0]);

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
         gEditorUserInterface.saveUndoState();
         words.erase(0);         // Get rid of "run", leaving script name and args

         string name = words[0];
         words.erase(0);

         gEditorUserInterface.onBeforeRunScriptFromConsole();
         gEditorUserInterface.runScript(name, words);
         gEditorUserInterface.onAfterRunScriptFromConsole();
      }
   }   

   else if(cmd == "clear")
      gEditorUserInterface.clearLevelGenItems();

   else
      OGLCONSOLE_Output(console, "Unknown command: %s\n", cmd.c_str());
}


void EditorUserInterface::onBeforeRunScriptFromConsole()
{
   // Use selection as a marker -- will have to change in future
   for(S32 i = 0; i < mItems.size(); i++)
      mItems[i]->setSelected(true);
}


void EditorUserInterface::onAfterRunScriptFromConsole()
{
   for(S32 i = 0; i < mItems.size(); i++)
      mItems[i]->setSelected(!mItems[i]->isSelected());

   rebuildEverything();
}


extern void actualizeScreenMode(bool, bool = false);

void EditorUserInterface::onActivate()
{
   if(gConfigDirs.levelDir == "")      // Never did resolve a leveldir... no editing for you!
   {
      gEditorUserInterface.reactivatePrevUI();     // Must come before the error msg, so it will become the previous UI when that one exits

      gErrorMsgUserInterface.reset();
      gErrorMsgUserInterface.setTitle("HOUSTON, WE HAVE A PROBLEM");
      gErrorMsgUserInterface.setMessage(1, "No valid level folder was found..."); 
      gErrorMsgUserInterface.setMessage(2, "cannot start the level editor");
      gErrorMsgUserInterface.setMessage(4, "Check the LevelDir parameter in your INI file,");
      gErrorMsgUserInterface.setMessage(5, "or your command-line parameters to make sure");
      gErrorMsgUserInterface.setMessage(6, "you have correctly specified a valid folder.");
      gErrorMsgUserInterface.activate();

      return;
   }

   // Check if we have a level name:
   if(getLevelFileName() == "")         // We need to take a detour to get a level name
   {
      // Don't save this menu (false, below).  That way, if the user escapes out, and is returned to the "previous"
      // UI, they will get back to where they were before (prob. the main menu system), not back to here.
      gLevelNameEntryUserInterface.activate(false);

      return;
   }

   mLevelErrorMsgs.clear();
   mLevelWarnings.clear();

   mSaveMsgTimer.clear();

   mGameTypeArgs.clear();

   mHasBotNavZones = false;

   loadLevel();
   setCurrentTeam(0);

   mSnapDisabled = false;      // Hold [space] to temporarily disable snapping

   // Reset display parameters...
   centerView();
   mDragSelecting = false;
   mUp = mDown = mLeft = mRight = mIn = mOut = false;
   mCreatingPoly = false;
   mCreatingPolyline = false;
   mDraggingObjects = false;
   mDraggingDockItem = NONE;
   mCurrentTeam = 0;
   mShowingReferenceShip = false;
   entryMode = EntryNone;

   mItemToLightUp = NULL;    

   mSaveMsgTimer = 0;

   OGLCONSOLE_EnterKey(processEditorConsoleCommand);     // Setup callback for processing console commands


   actualizeScreenMode(false); 
}


void EditorUserInterface::onDeactivate()
{
   mDockItems.clear();     // Free some memory -- dock will be rebuilt when editor restarts
   actualizeScreenMode(true);
}


void EditorUserInterface::onReactivate()
{
   mDraggingObjects = false;  

   if(mWasTesting)
   {
      mWasTesting = false;
      mSaveMsgTimer.clear();
   }

   remove("editor.tmp");      // Delete temp file

   if(mCurrentTeam >= mTeams.size())
      mCurrentTeam = 0;

   OGLCONSOLE_EnterKey(processEditorConsoleCommand);     // Restore callback for processing console commands

   actualizeScreenMode(true);
}


static Point sCenter;

// Called when we shift between windowed and fullscreen mode, before change is made
void EditorUserInterface::onPreDisplayModeChange()
{
   sCenter.set(mCurrentOffset.x - gScreenInfo.getGameCanvasWidth() / 2, mCurrentOffset.y - gScreenInfo.getGameCanvasHeight() / 2);
}

// Called when we shift between windowed and fullscreen mode, after change is made
void EditorUserInterface::onDisplayModeChange()
{
   // Recenter canvas -- note that canvasWidth may change during displayMode change
   mCurrentOffset.set(sCenter.x + gScreenInfo.getGameCanvasWidth() / 2, sCenter.y + gScreenInfo.getGameCanvasHeight() / 2);

   populateDock();               // If game type has changed, items on dock will change
}


Point EditorUserInterface::snapPointToLevelGrid(Point const &p)
{
   if(mSnapDisabled)
      return p;

   // First, find a snap point based on our grid
   F32 factor = (showMinorGridLines() ? 0.1 : 0.5) * getGridSize();     // Tenths or halves -- major gridlines are gridsize pixels apart

   return Point(floor(p.x / factor + 0.5) * factor, floor(p.y / factor + 0.5) * factor);
}


Point EditorUserInterface::snapPoint(Point const &p, bool snapWhileOnDock)
{
   if(mouseOnDock() && !snapWhileOnDock) 
      return p;      // No snapping!

   Point snapPoint(p);

   if(mDraggingObjects)
   {
      // Mark all items being dragged as no longer being snapped -- only our primary "focus" item will be snapped
      for(S32 i = 0; i < mItems.size(); i++)
         if(mItems[i]->isSelected())
            mItems[i]->setSnapped(false);
   }
   
   // Turrets & forcefields: Snap to a wall edge as first (and only) choice
   if(mDraggingObjects && (mSnapVertex_i->getObjectTypeMask() & EngineeredType))
   {
      EngineeredObject *engrObj = dynamic_cast<EngineeredObject *>(mSnapVertex_i);
      return engrObj->mountToWall(snapPointToLevelGrid(p));
   }

   F32 maxSnapDist = 2 / (mCurrentScale * mCurrentScale);
   F32 minDist = maxSnapDist;

   // Where will we be snapping things?
   bool snapToWallCorners = !mSnapDisabled && mDraggingObjects && !(mSnapVertex_i->getObjectTypeMask() & BarrierType) && mSnapVertex_i->getGeomType() != geomPoly;
   bool snapToLevelGrid = !mSnapDisabled;


   if(snapToLevelGrid)     // Lowest priority
   {
      snapPoint = snapPointToLevelGrid(p);
      minDist = snapPoint.distSquared(p);
   }


   // Now look for other things we might want to snap to
   for(S32 i = 0; i < mItems.size(); i++)
   {
      // Don't snap to selected items or items with selected verts
      if(mItems[i]->isSelected() || mItems[i]->anyVertsSelected())    
         continue;

      for(S32 j = 0; j < mItems[i]->getVertCount(); j++)
      {
         F32 dist = mItems[i]->getVert(j).distSquared(p);
         if(dist < minDist)
         {
            minDist = dist;
            snapPoint.set(mItems[i]->getVert(j));
         }
      }
   }

   // Search for a corner to snap to - by using wall edges, we'll also look for intersections between segments
   if(snapToWallCorners)
      checkCornersForSnap(p, WallSegmentManager::mWallEdges, minDist, snapPoint);

   return snapPoint;
}


extern bool findNormalPoint(const Point &p, const Point &s1, const Point &s2, Point &closest);

static Point closest;      // Reusable container

static bool checkEdge(const Point &clickPoint, const Point &start, const Point &end, F32 &minDist, Point &snapPoint)
{
   if(findNormalPoint(clickPoint, start, end, closest))    // closest is point on line where clickPoint normal intersects
   {
      F32 dist = closest.distSquared(clickPoint);
      if(dist < minDist)
      {
         minDist = dist;
         snapPoint.set(closest);  
         return true;
      }
   }

   return false;
}


// Checks for snapping against a series of edges defined by verts in A-B-C-D format if abcFormat is true, or A-B B-C C-D if false
// Sets snapPoint and minDist.  Returns index of closest segment found if closer than minDist.
S32 EditorUserInterface::checkEdgesForSnap(const Point &clickPoint, const Vector<Point> &verts, bool abcFormat,
                                           F32 &minDist, Point &snapPoint )
{
   S32 inc = abcFormat ? 1 : 2;   
   S32 segFound = NONE;

   for(S32 i = 0; i < verts.size() - 1; i += inc)
      if(checkEdge(clickPoint, verts[i], verts[i+1], minDist, snapPoint))
         segFound = i;

   return segFound;
}


S32 EditorUserInterface::checkEdgesForSnap(const Point &clickPoint, const Vector<WallEdge *> &edges, bool abcFormat,
                                           F32 &minDist, Point &snapPoint )
{
   S32 inc = abcFormat ? 1 : 2;   
   S32 segFound = NONE;

   for(S32 i = 0; i < edges.size(); i++)
   {
      if(checkEdge(clickPoint, *edges[i]->getStart(), *edges[i]->getEnd(), minDist, snapPoint))
         segFound = i;
   }

   return segFound;
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

static const Color grayedOutColorBright = Color(.5, .5, .5);
static const Color grayedOutColorDim = Color(.25, .25, .25);
static bool fillRendered = false;


bool EditorUserInterface::showMinorGridLines()
{
   return mCurrentScale >= .5;
}


// Render background snap grid
void EditorUserInterface::renderGrid()
{
   if(mShowingReferenceShip)
      return;   

   F32 colorFact = mSnapDisabled ? .5 : 1;
   
   // Minor grid lines
   for(S32 i = 1; i >= 0; i--)
   {
      if(i && showMinorGridLines() || !i)      // Minor then major gridlines
      {
         F32 gridScale = mCurrentScale * getGridSize() * (i ? 0.1 : 1);    // Major gridlines are gridSize() pixels apart   
         F32 color = (i ? .2 : .4) * colorFact;

         F32 xStart = fmod(mCurrentOffset.x, gridScale);
         F32 yStart = fmod(mCurrentOffset.y, gridScale);

         glColor3f(color, color, color);
         glBegin(GL_LINES);
            while(yStart < gScreenInfo.getGameCanvasHeight())
            {
               glVertex2f(0, yStart);
               glVertex2f(gScreenInfo.getGameCanvasWidth(), yStart);
               yStart += gridScale;
            }
            while(xStart < gScreenInfo.getGameCanvasWidth())
            {
               glVertex2f(xStart, 0);
               glVertex2f(xStart, gScreenInfo.getGameCanvasHeight());
               xStart += gridScale;
            }
         glEnd();
      }
   }

   // Draw axes
   glColor3f(0.7 * colorFact, 0.7 * colorFact, 0.7 * colorFact);
   glLineWidth(gLineWidth3);

   Point origin = convertLevelToCanvasCoord(Point(0,0));

   glBegin(GL_LINES);
      glVertex2f(0, origin.y);
      glVertex2f(gScreenInfo.getGameCanvasWidth(), origin.y);
      glVertex2f(origin.x, 0);
      glVertex2f(origin.x, gScreenInfo.getGameCanvasHeight());
   glEnd();

   glLineWidth(gDefaultLineWidth);
}


S32 getDockHeight(ShowMode mode)
{
   if(mode == ShowWallsOnly)
      return 62;
   else  // mShowMode == ShowAllObjects || mShowMode == ShowAllButNavZones
      return gScreenInfo.getGameCanvasHeight() - 2 * EditorUserInterface::vertMargin;
}


void EditorUserInterface::renderDock(F32 width)    // width is current wall width, used for displaying info on dock
{
   // Render item dock down RHS of screen
   const S32 canvasWidth = gScreenInfo.getGameCanvasWidth();
   const S32 canvasHeight = gScreenInfo.getGameCanvasHeight();

   S32 dockHeight = getDockHeight(mShowMode);

   for(S32 i = 1; i >= 0; i--)
   {
      glColor(i ? black : (mouseOnDock() ? yellow : white));       

      glBegin(i ? GL_POLYGON : GL_LINE_LOOP);
         glVertex2f(canvasWidth - DOCK_WIDTH - horizMargin, canvasHeight - vertMargin);
         glVertex2f(canvasWidth - horizMargin,              canvasHeight - vertMargin);
         glVertex2f(canvasWidth - horizMargin,              canvasHeight - vertMargin - dockHeight);
         glVertex2f(canvasWidth - DOCK_WIDTH - horizMargin, canvasHeight - vertMargin - dockHeight);
      glEnd();
   }

   // Draw coordinates on dock -- if we're moving an item, show the coords of the snap vertex, otherwise show the coords of the
   // snapped mouse position
   Point pos;
   if(mSnapVertex_i)
      pos = mSnapVertex_i->getVert(mSnapVertex_j);
   else
      pos = snapPoint(convertCanvasToLevelCoord(mMousePos));

   F32 xpos = gScreenInfo.getGameCanvasWidth() - horizMargin - DOCK_WIDTH / 2;

   char text[50];
   glColor(white);
   dSprintf(text, sizeof(text), "%2.2f|%2.2f", pos.x, pos.y);
   drawStringc(xpos, gScreenInfo.getGameCanvasHeight() - vertMargin - 15, 8, text);

   // And scale
   dSprintf(text, sizeof(text), "%2.2f", mCurrentScale);
   drawStringc(xpos, gScreenInfo.getGameCanvasHeight() - vertMargin - 25, 8, text);

   // Show number of teams
   dSprintf(text, sizeof(text), "Teams: %d",  mTeams.size());
   drawStringc(xpos, gScreenInfo.getGameCanvasHeight() - vertMargin - 35, 8, text);

   glColor(mNeedToSave ? red : green);     // Color level name by whether it needs to be saved or not
   dSprintf(text, sizeof(text), "%s%s", mNeedToSave ? "*" : "", mEditFileName.substr(0, mEditFileName.find_last_of('.')).c_str());    // Chop off extension
   drawStringc(xpos, gScreenInfo.getGameCanvasHeight() - vertMargin - 45, 8, text);

   // And wall width as needed
   if(width != NONE)
   {
      glColor(white);
      dSprintf(text, sizeof(text), "Width: %2.0f", width);
      drawStringc(xpos, gScreenInfo.getGameCanvasHeight() - vertMargin - 55, 8, text);
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
      static const Color errorColor(1, 0, 0);

      bool errorFound = false;

      // Check for duplicate IDs if we're in ID entry mode
      if(entryMode == EntryID)
      {
         U32 id = atoi(mEntryBox.c_str());      // mEntryBox has digits only filter applied; ids can only be positive ints

         if(id != 0)    // Check for duplicates
         {
            for(S32 i = 0; i < mItems.size(); i++)
               if(mItems[i]->getItemId() == id && !mItems[i]->isSelected())
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
      glEnableBlend;
      S32 xpos = (gScreenInfo.getGameCanvasWidth()  - boxwidth) / 2;
      S32 ypos = (gScreenInfo.getGameCanvasHeight() - boxheight) / 2;

      for(S32 i = 1; i >= 0; i--)
      {
         glColor(Color(.3,.6,.3), i ? .85 : 1);

         glBegin(i ? GL_POLYGON : GL_LINE_LOOP);
            glVertex2f(xpos,            ypos);
            glVertex2f(xpos + boxwidth, ypos);
            glVertex2f(xpos + boxwidth, ypos + boxheight);
            glVertex2f(xpos,            ypos + boxheight);
         glEnd();
      }
      glDisableBlend;

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
      glTranslatef(mMousePos.x, mMousePos.y, 0);
      glScalef(mCurrentScale / getGridSize(), mCurrentScale / getGridSize(), 1);
      glRotatef(90, 0, 0, 1);
      renderShip(red, 1, thrusts, 1, 5, 0, false, false, false, false);
      glRotatef(-90, 0, 0, 1);

      // And show how far it can see
      F32 horizDist = Game::PLAYER_VISUAL_DISTANCE_HORIZONTAL;
      F32 vertDist = Game::PLAYER_VISUAL_DISTANCE_VERTICAL;

      glEnableBlend;     // Enable transparency
      glColor4f(.5, .5, 1, .35);
      glBegin(GL_POLYGON);
         glVertex2f(-horizDist, -vertDist);
         glVertex2f(horizDist, -vertDist);
         glVertex2f(horizDist, vertDist);
         glVertex2f(-horizDist, vertDist);
      glEnd();
      glDisableBlend;

   glPopMatrix();
}


static F32 getRenderingAlpha(bool isScriptItem)
{
   return isScriptItem ? .6 : 1;     // Script items will appear somewhat translucent
}


const char *getModeMessage(ShowMode mode)
{
   if(mode == ShowWallsOnly)
      return "Wall editing mode.  Hit Ctrl-A to change.";
   else if(mode == ShowAllButNavZones)
      return "NavMesh zones hidden.  Hit Ctrl-A to change.";
   else if(mode == NavZoneMode)
      return "NavMesh editing mode.  Hit Ctrl-A to change.";
   else     // Normal mode
      return "";
}


void EditorUserInterface::render()
{
   mouseIgnore = false; // Needed to avoid freezing effect from too many mouseMoved events without a render in between (sam)

   renderGrid();        // Render grid first, so it's at the bottom

   // Render any items generated by the levelgen script... these will be rendered below normal items. 
   glPushMatrix();
      setLevelToCanvasCoordConversion();

      glColor(Color(0,.25,0));
      for(S32 i = 0; i < mLevelGenItems.size(); i++)
         if(mLevelGenItems[i]->getObjectTypeMask() & BarrierType)
            for(S32 j = 0; j < mLevelGenItems[i]->extendedEndPoints.size(); j+=2)
               renderTwoPointPolygon(mLevelGenItems[i]->extendedEndPoints[j], mLevelGenItems[i]->extendedEndPoints[j+1], 
                                     mLevelGenItems[i]->getWidth() / getGridSize() / 2, GL_POLYGON);
   glPopMatrix();

   for(S32 i = 0; i < mLevelGenItems.size(); i++)
      mLevelGenItems[i]->render(true, mShowingReferenceShip, mShowMode);
   
   // Render polyWall item fill just before rendering regular walls.  This will create the effect of all walls merging together.  
   // PolyWall outlines are already part of the wallSegmentManager, so will be rendered along with those of regular walls.

   fillVector.clear();
   gEditorGame->getGridDatabase()->findObjects(fillVector);

   glPushMatrix();  
      setLevelToCanvasCoordConversion();
      for(S32 i = 0; i < fillVector.size(); i++)
         if(fillVector[i]->getObjectTypeMask() & PolyWallType)
         {
            PolyWall *wall = dynamic_cast<PolyWall *>(fillVector[i]);
            wall->renderFill();
         }
   
      mWallSegmentManager.renderWalls(true, mDraggingObjects, mShowingReferenceShip, getRenderingAlpha(false/*isScriptItem*/));
   glPopMatrix();

   // == Normal items ==
   // Draw map items (teleporters, etc.) that are not being dragged, and won't have any text labels  (below the dock)
   for(S32 i = 0; i < fillVector.size(); i++)
   {
      EditorObject *obj = dynamic_cast<EditorObject *>(fillVector[i]);
      if(!(mDraggingObjects && obj->isSelected()))
         obj->render(false, mShowingReferenceShip, mShowMode);
   }

   // == Selected items ==
   // Draw map items (teleporters, etc.) that are are selected and/or lit up, so label is readable (still below the dock)
   // Do this as a separate operation to ensure that these are drawn on top of those drawn above.
   for(S32 i = 0; i < fillVector.size(); i++)
   {
      EditorObject *obj = dynamic_cast<EditorObject *>(fillVector[i]);
      if(obj->isSelected() || obj->isLitUp())
         obj->render(false, mShowingReferenceShip, mShowMode);
   }


   fillRendered = false;
   F32 width = NONE;

   if(mCreatingPoly || mCreatingPolyline)    // Draw geomLine features under construction
   {
      mNewItem->addVert(snapPoint(convertCanvasToLevelCoord(mMousePos)));
      glLineWidth(gLineWidth3);

      if(mCreatingPoly) // Wall
         glColor(SELECT_COLOR);
      else              // LineItem
         glColor(getTeamColor(mNewItem->getTeam()));

      renderPolyline(mNewItem->getOutline());

      glLineWidth(gDefaultLineWidth);

      for(S32 j = mNewItem->getVertCount() - 1; j >= 0; j--)      // Go in reverse order so that placed vertices are drawn atop unplaced ones
      {
         Point v = convertLevelToCanvasCoord(mNewItem->getVert(j));

         // Draw vertices
         if(j == mNewItem->getVertCount() - 1)           // This is our most current vertex
            renderVertex(HighlightedVertex, v, NO_NUMBER);
         else
            renderVertex(SelectedItemVertex, v, j);
      }
      mNewItem->deleteVert(mNewItem->getVertCount() - 1);
   }

   // Since we're not constructing a barrier, if there are any barriers or lineItems selected, 
   // get the width for display at bottom of dock
   else  
   {
      fillVector.clear();
      gEditorGame->getGridDatabase()->findObjects(BarrierType | LineType, fillVector);

      for(S32 i = 0; i < fillVector.size(); i++)
      {
         EditorObject *obj = dynamic_cast<EditorObject *>(fillVector[i]);
         if((obj->isSelected() || (obj->isLitUp() && obj->isVertexLitUp(NONE))) )
         {
            width = obj->getWidth();
            break;
         }
      }
   }


   // Render our snap vertex
   if(!mShowingReferenceShip && mSnapVertex_i && mSnapVertex_i->isSelected() && mSnapVertex_j != NONE)      
      renderVertex(SnappingVertex, mSnapVertex_i->getVert(mSnapVertex_j) * mCurrentScale + mCurrentOffset, NO_NUMBER/*, alpha*/);  // Hollow magenta box

   if(mShowingReferenceShip)
      renderReferenceShip();
   else
      renderDock(width);

   // Draw map items (teleporters, etc.) that are being dragged  (above the dock).  But don't draw walls here, or
   // we'll lose our wall centernlines.
   if(mDraggingObjects)
   {
      fillVector.clear();
      gEditorGame->getGridDatabase()->findObjects(~BarrierType, fillVector);

      for(S32 i = 0; i < fillVector.size(); i++)
      {
         EditorObject *obj = dynamic_cast<EditorObject *>(fillVector[i]);
         if(obj->isSelected())
            obj->render(false, mShowingReferenceShip, mShowMode);
      }
   }

   if(mDragSelecting)      // Draw box for selecting items
   {
      glColor(white);
      Point downPos = convertLevelToCanvasCoord(mMouseDownPos);
      glBegin(GL_LINE_LOOP);
         glVertex2f(downPos.x,   downPos.y);
         glVertex2f(mMousePos.x, downPos.y);
         glVertex2f(mMousePos.x, mMousePos.y);
         glVertex2f(downPos.x,   mMousePos.y);
      glEnd();
   }

   // Render messages at bottom of screen
   if(mouseOnDock())    // On the dock?  If so, render help string if hovering over item
   {
      S32 hoverItem = findHitItemOnDock(mMousePos);

      if(hoverItem != NONE)
      {
         mDockItems[hoverItem]->setLitUp(true);    // Will trigger a selection highlight to appear around dock item

         const char *helpString = mDockItems[hoverItem]->getEditorHelpString();

         glColor3f(.1, 1, .1);

         // Center string between left side of screen and edge of dock
         S32 x = (S32)(gScreenInfo.getGameCanvasWidth() - horizMargin - DOCK_WIDTH - getStringWidth(15, helpString)) / 2;
         drawString(x, gScreenInfo.getGameCanvasHeight() - vertMargin - 15, 15, helpString);
      }
   }

   // Render dock items
   if(!mShowingReferenceShip)
      for(S32 i = 0; i < mDockItems.size(); i++)
      {
         mDockItems[i]->render(false, false, mShowMode);
         mDockItems[i]->setLitUp(false);
      }

   if(mSaveMsgTimer.getCurrent())
   {
      F32 alpha = 1.0;
      if(mSaveMsgTimer.getCurrent() < 1000)
         alpha = (F32) mSaveMsgTimer.getCurrent() / 1000;

      glEnableBlend;
         glColor(mSaveMsgColor, alpha);
         drawCenteredString(gScreenInfo.getGameCanvasHeight() - vertMargin - 65, 25, mSaveMsg.c_str());
      glDisableBlend;
   }

   if(mWarnMsgTimer.getCurrent())
   {
      F32 alpha = 1.0;
      if (mWarnMsgTimer.getCurrent() < 1000)
         alpha = (F32) mWarnMsgTimer.getCurrent() / 1000;

      glEnableBlend;
         glColor(mWarnMsgColor, alpha);
         drawCenteredString(gScreenInfo.getGameCanvasHeight() / 4, 25, mWarnMsg1.c_str());
         drawCenteredString(gScreenInfo.getGameCanvasHeight() / 4 + 30, 25, mWarnMsg2.c_str());
      glDisableBlend;
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

      glColor(yellow);

      for(S32 i = 0; i < mLevelWarnings.size(); i++)
      {
         drawCenteredString(ypos, 20, mLevelWarnings[i].c_str());
         ypos += 25;
      }
   }

   glColor(cyan);
   drawCenteredString(vertMargin, 14, getModeMessage(mShowMode));

   renderTextEntryOverlay();

   renderConsole();  // Rendered last, so it's always on top
}


// TODO: Merge with nearly identical version in gameType
Color EditorUserInterface::getTeamColor(S32 team)
{
   if(team == Item::TEAM_NEUTRAL || team >= mTeams.size() || team < Item::TEAM_HOSTILE)
      return gNeutralTeamColor;
   else if(team == Item::TEAM_HOSTILE)
      return gHostileTeamColor;
   else
      return mTeams[team].color;
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


// Will set the correct translation and scale to render items at correct location and scale as if it were a real level.
// Unclear enough??
//void EditorUserInterface::setTranslationAndScale(const Point &pos)
//{
//   F32 scale = gEditorUserInterface.getCurrentScale();
//
//   glScalef(scale, scale, 1);
//   glTranslatef(pos.x / scale - pos.x, pos.y / scale - pos.y, 0);
//}


void EditorUserInterface::clearSelection()
{
   for(S32 i = 0; i < mItems.size(); i++)
      mItems[i]->unselect();
}


static S32 getNextItemId()
{
   static S32 nextItemId = 0;
   return nextItemId++;
}


// Copy selection to the clipboard
void EditorUserInterface::copySelection()
{
   if(!anyItemsSelected())
      return;

   bool alreadyCleared = false;

   S32 itemCount = mItems.size();
   for(S32 i = 0; i < itemCount; i++)
   {
      if(mItems[i]->isSelected())
      {
         EditorObject *newItem =  mItems[i]->newCopy();      
         newItem->setSelected(false);

         //newItem->offset(Point(0.5, 0.5));

         if(!alreadyCleared)  // Make sure we only purge the existing clipboard if we'll be putting someting new there
         {
            mClipboard.deleteAndClear();
            alreadyCleared = true;
         }

         mClipboard.push_back(newItem);
      }
   }
}


// Paste items on the clipboard
void EditorUserInterface::pasteSelection()
{
   if(mDraggingObjects)      // Pasting while dragging can cause crashes!!
      return;

   S32 itemCount = mClipboard.size();

    if(itemCount == 0)       // Nothing on clipboard, nothing to do
      return;

   saveUndoState();      // So we can undo the paste

   clearSelection();     // Only the pasted items should be selected

   Point pos = snapPoint(convertCanvasToLevelCoord(mMousePos));

   Point firstPoint = mClipboard[0]->getVert(0);
   Point offset;

   for(S32 i = 0; i < itemCount; i++)
   {
      offset = firstPoint - mClipboard[i]->getVert(0);

      EditorObject *newObj = mClipboard[i]->newCopy();

      newObj->addToEditor(gEditorGame);

      newObj->setSerialNumber(getNextItemId());
      newObj->setSelected(true);
      newObj->moveTo(pos + offset);
      newObj->onGeomChanged();
   }
   //mItems.sort(geometricSort);
   geomSort(mItems);
   validateLevel();
   mNeedToSave = true;
   autoSave();
}


// Expand or contract selection by scale
void EditorUserInterface::scaleSelection(F32 scale)
{
   if(!anyItemsSelected() || scale < .01 || scale == 1)    // Apply some sanity checks
      return;

   saveUndoState();

   // Find center of selection
   Point min, max;                        
   computeSelectionMinMax(min, max);
   Point ctr = (min + max) * 0.5;

   if(scale > 1 && min.distanceTo(max) * scale > 50)    // If walls get too big, they'll bog down the db
      return;

   for(S32 i = 0; i < mItems.size(); i++)
      if(mItems[i]->isSelected())
         mItems[i]->scale(ctr, scale);

   mNeedToSave = true;
   autoSave();
}


// Rotate selected objects around their center point by angle
void EditorUserInterface::rotateSelection(F32 angle)
{
   if(!anyItemsSelected())
      return;

   saveUndoState();

   for(S32 i = 0; i < mItems.size(); i++)
   {
      if(mItems[i]->isSelected())
         mItems[i]->rotateAboutPoint(Point(0,0), angle);
   }
   mNeedToSave = true;
   autoSave();
}


// Find all objects in bounds 
// TODO: This should be a database function!
void EditorUserInterface::computeSelectionMinMax(Point &min, Point &max)
{
   min.set(F32_MAX, F32_MAX);
   max.set(F32_MIN, F32_MIN);

   for(S32 i = 0; i < mItems.size(); i++)
   {
      if(mItems[i]->isSelected())
      {
         EditorObject *item = mItems[i].get();
         for(S32 j = 0; j < item->getVertCount(); j++)
         {
            Point v = item->getVert(j);

            if(v.x < min.x)   min.x = v.x;
            if(v.x > max.x)   max.x = v.x;
            if(v.y < min.y)   min.y = v.y;
            if(v.y > max.y)   max.y = v.y;
         }
      }
   }
}


// Set the team affiliation of any selected items
void EditorUserInterface::setCurrentTeam(S32 currentTeam)
{
   mCurrentTeam = currentTeam;
   bool anyChanged = false;

   saveUndoState();

   if(currentTeam >= mTeams.size())
   {
      char msg[255];
      if(mTeams.size() == 1)
         dSprintf(msg, sizeof(msg), "Only 1 team has been configured.");
      else
         dSprintf(msg, sizeof(msg), "Only %d teams have been configured.", mTeams.size());
      gEditorUserInterface.setWarnMessage(msg, "Hit [F2] to configure teams.");
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

   for(S32 i = 0; i < mItems.size(); i++)
   {
      if(mItems[i]->isSelected())
      {
         if(!mItems[i]->hasTeam())
            continue;

         if(currentTeam == TEAM_NEUTRAL && !mItems[i]->canBeNeutral())
            continue;

         if(currentTeam == TEAM_HOSTILE && !mItems[i]->canBeHostile())
            continue;

         if(!anyChanged)
            saveUndoState();

         mItems[i]->setTeam(currentTeam);
         anyChanged = true;
      }
   }

   // Overwrite any warnings set above.  If we have a group of items selected, it makes no sense to show a
   // warning if one of those items has the team set improperly.  The warnings are more appropriate if only
   // one item is selected, or none of the items are given a valid team setting.

   if(anyChanged)
   {
      gEditorUserInterface.setWarnMessage("", "");
      validateLevel();
      mNeedToSave = true;
      autoSave();
   }
}


void EditorUserInterface::flipSelectionHorizontal()
{
   if(!anyItemsSelected())
      return;

   saveUndoState();

   Point min, max;
   computeSelectionMinMax(min, max);

   for(S32 i = 0; i < mItems.size(); i++)
      if(mItems[i]->isSelected())
         mItems[i]->flipHorizontal(min, max);

   mNeedToSave = true;
   autoSave();
}


void EditorUserInterface::flipSelectionVertical()
{
   if(!anyItemsSelected())
      return;

   saveUndoState();

   Point min, max;
   computeSelectionMinMax(min, max);

   for(S32 i = 0; i < mItems.size(); i++)
      if(mItems[i]->isSelected())
         mItems[i]->flipVertical(min, max);

   mNeedToSave = true;
   autoSave();
}


//// Identify a vert that our mouse is over; hitObject contains the vert, hitVertex is the index of the vert we hit
//void EditorUserInterface::findHitVertex(EditorObject *&hitObject, S32 &hitVertex)
//{
//   hitObject = NULL;
//   hitVertex = NONE;
//
//   for(S32 x = 1; x >= 0; x--)    // Two passes... first for selected item, second for all items
//   {
//      for(S32 i = mItems.size() - 1; i >= 0; i--)     // Reverse order so we get items "from the top down"
//      { 
//         if(x && !mItems[i]->isSelected() && !mItems[i]->anyVertsSelected())
//            continue;
//
//         U32 type = mItems[i]->getObjectTypeMask();
//         if(mShowMode == ShowWallsOnly && !(type & BarrierType) && !(type & PolyWallType))        // Only select walls in CTRL-A mode
//            continue;
//
//         if(mItems[i]->getGeomType() < geomPoint)        // Was <=... why?
//            continue;
//
//         S32 radius = mItems[i]->getEditorRadius(mCurrentScale) * mItems[i]->getEditorRenderScaleFactor(mCurrentScale);
//
//         for(S32 j = mItems[i]->getVertCount() - 1; j >= 0; j--)
//         {
//            Point p = mMousePos - mCurrentOffset - mItems[i]->getVert(j) * mCurrentScale;    // Pixels from mouse to mItems[i]->getVert(j), at any zoom
//            if(fabs(p.x) < radius && fabs(p.y) < radius)
//            {
//               hitObject = mItems[i];
//               hitVertex = j;
//               return;
//            }
//         }
//      }
//   }
//}


static const S32 POINT_HIT_RADIUS = 9;
static const S32 EDGE_HIT_RADIUS = 6;

void EditorUserInterface::findHitItemAndEdge()
{
   mItemHit = NULL;
   mEdgeHit = NONE;
   mVertexHit = NONE;

   // Do this in two passes -- the first we only consider selected items, the second pass will consider all targets.
   // This will give priority to moving vertices of selected items
   for(S32 firstPass = 1; firstPass >= 0; firstPass--)   // firstPass will be true the first time through, false the second time
   {
      for(S32 i = mItems.size() - 1; i >= 0; i--)        // Go in reverse order to prioritize items drawn on top
      {
         if(firstPass && !mItems[i]->isSelected() && !mItems[i]->anyVertsSelected())     // First pass is for selected items only
            continue;
         
         // Only select walls in CTRL-A mode...
         U32 type = mItems[i]->getObjectTypeMask();
         if(mShowMode == ShowWallsOnly && !(type & BarrierType) && !(type & PolyWallType))        // Only select walls in CTRL-A mode
            continue;                                                              // ...so if it's not a wall, proceed to next item

         S32 radius = mItems[i]->getEditorRadius(mCurrentScale);

         for(S32 j = mItems[i]->getVertCount() - 1; j >= 0; j--)
         {
            // p represents pixels from mouse to mItems[i]->getVert(j), at any zoom
            Point p = mMousePos - mCurrentOffset - mItems[i]->getVert(j) * mCurrentScale;    

            if(fabs(p.x) < radius && fabs(p.y) < radius)
            {
               mItemHit = mItems[i].get();
               mVertexHit = j;
               return;
            }
         }

         // This is all we can check on point items -- it makes no sense to check edges or other higher order geometry
         if(mItems[i]->getGeomType() == geomPoint)
            continue;

         /////
         // Didn't find a vertex hit... now we look for an edge

         // Make a copy of the items vertices that we can add to in the case of a loop
         Vector<Point> verts = *mItems[i]->getOutline();    

         if(mItems[i]->getGeomType() == geomPoly)   // Add first point to the end to create last side on poly
            verts.push_back(verts.first());

         Point p1 = convertLevelToCanvasCoord(mItems[i]->getVert(0));
         Point closest;
         
         for(S32 j = 0; j < mItems[i]->getVertCount() - 1; j++)
         {
            Point p2 = convertLevelToCanvasCoord(mItems[i]->getVert(j+1));
            
            if(findNormalPoint(mMousePos, p1, p2, closest))
            {
               F32 distance = (mMousePos - closest).len();
               if(distance < EDGE_HIT_RADIUS)
               {
                  mItemHit = mItems[i].get();
                  mEdgeHit = j;

                  return;
               }
            }
            p1.set(p2);
         }
      }
   }

   if(mShowMode == ShowWallsOnly) 
      return;

   /////
   // If we're still here, it means we didn't find anything yet.  Make one more pass, and see if we're in any polys.
   // This time we'll loop forward, though I don't think it really matters.
   for(S32 i = 0; i < mItems.size(); i++)
   {
      if(mItems[i]->getGeomType() == geomPoly)
      {
         Vector<Point> verts;
         for(S32 j = 0; j < mItems[i]->getVertCount(); j++)
            verts.push_back(convertLevelToCanvasCoord(mItems[i]->getVert(j)));

         if(PolygonContains2(verts.address(), verts.size(), mMousePos))
         {
            mItemHit = mItems[i].get();
            return;
         }
      }
   }
}


S32 EditorUserInterface::findHitItemOnDock(Point canvasPos)
{
   if(mShowMode == ShowWallsOnly)     // Only add dock items when objects are visible
      return NONE;

   for(S32 i = mDockItems.size() - 1; i >= 0; i--)     // Go in reverse order because the code we copied did ;-)
   {
      Point pos = mDockItems[i]->getVert(0);

      if(fabs(canvasPos.x - pos.x) < POINT_HIT_RADIUS && fabs(canvasPos.y - pos.y) < POINT_HIT_RADIUS)
         return i;
   }

   // Now check for polygon interior hits
   for(S32 i = 0; i < mDockItems.size(); i++)
      if(mDockItems[i]->getGeomType() == geomPoly)
      {
         Vector<Point> verts;
         for(S32 j = 0; j < mDockItems[i]->getVertCount(); j++)
            verts.push_back(mDockItems[i]->getVert(j));

         if(PolygonContains2(verts.address(),verts.size(), canvasPos))
            return i;
      }

   return NONE;
}


// Incoming calls from GLUT come here...
void EditorUserInterface::onMouseMoved(S32 x, S32 y)
{
   onMouseMoved();      //... and go here
}


void EditorUserInterface::onMouseMoved()
{
   if(mouseIgnore)  // Needed to avoid freezing effect from too many mouseMoved events without a render in between
      return;

   mouseIgnore = true;

   mMousePos.set(gScreenInfo.getMousePos());

   if(mCreatingPoly || mCreatingPolyline)
      return;

   //findHitVertex(mMousePos, vertexHitObject, vertexHit);      // Sets vertexHitObject and vertexHit
   findHitItemAndEdge();                                      //  Sets mItemHit, mVertexHit, and mEdgeHit

   // Unhighlight the currently lit up object, if any
   if(mItemToLightUp)
      mItemToLightUp->setLitUp(false);

   S32 vertexToLightUp = NONE;
   mItemToLightUp = NULL;

   // We hit a vertex that wasn't already selected
   if(mVertexHit != NONE && !mItemHit->vertSelected(mVertexHit))   
   {
      mItemToLightUp = mItemHit;
      mItemToLightUp->setVertexLitUp(mVertexHit);
   }

   // We hit an item that wasn't already selected
   else if(mItemHit && !mItemHit->isSelected())                   
      mItemToLightUp = mItemHit;

   // Check again, and take a point object in preference to a vertex
   if(mItemHit && !mItemHit->isSelected() && mItemHit->getGeomType() == geomPoint)  
   {
      mItemToLightUp = mItemHit;
      vertexToLightUp = NONE;
   }

   if(mItemToLightUp)
      mItemToLightUp->setLitUp(true);

   bool showMoveCursor = (mItemHit || mVertexHit != NONE || mItemHit || mEdgeHit != NONE || 
                         (mouseOnDock() && findHitItemOnDock(mMousePos) != NONE));


   findSnapVertex();

   glutSetCursor((showMoveCursor && !mShowingReferenceShip) ? GLUT_CURSOR_SPRAY : GLUT_CURSOR_RIGHT_ARROW);
}


void EditorUserInterface::onMouseDragged(S32 x, S32 y)
{
   if(mouseIgnore)  // Needed to avoid freezing effect from too many mouseMoved events without a render in between (sam)
      return;

   mouseIgnore = true;

   mMousePos.set(gScreenInfo.getMousePos());

   if(mCreatingPoly || mCreatingPolyline || mDragSelecting)
      return;

   if(mDraggingDockItem != NONE)      // We just started dragging an item off the dock
       startDraggingDockItem();  

   findSnapVertex();

   if(!mSnapVertex_i || mSnapVertex_j == NONE)
      return;
   
   
   if(!mDraggingObjects)      // Just started dragging
   {
      mMoveOrigin = mSnapVertex_i->getVert(mSnapVertex_j);
      mOriginalVertLocations.clear();

      for(S32 i = 0; i < mItems.size(); i++)
         if(mItems[i]->isSelected() || mItems[i]->anyVertsSelected())
            for(S32 j = 0; j < mItems[i]->getVertCount(); j++)
               if(mItems[i]->isSelected() || mItems[i]->vertSelected(j))
                  mOriginalVertLocations.push_back(mItems[i]->getVert(j));

      saveUndoState();
   }

   mDraggingObjects = true;



   Point delta;

   // The thinking here is that for large items -- walls, polygons, etc., we may grab an item far from its snap vertex, and we
   // want to factor that offset into our calculations.  For point items (and vertices), we don't really care about any slop
   // in the selection, and we just want the damn thing where we put it.
   if(mSnapVertex_i->getGeomType() == geomPoint || (mItemHit && mItemHit->anyVertsSelected()))
      delta = snapPoint(convertCanvasToLevelCoord(mMousePos)) - mMoveOrigin;
   else
      delta = (snapPoint(convertCanvasToLevelCoord(mMousePos) + mMoveOrigin - mMouseDownPos) - mMoveOrigin );


   // Update coordinates of dragged item
   S32 count = 0;
   for(S32 i = 0; i < mItems.size(); i++)
      if(mItems[i]->isSelected() || mItems[i]->anyVertsSelected())
      {
         for(S32 j = 0; j < mItems[i]->getVertCount(); j++)
            if(mItems[i]->isSelected() || mItems[i]->vertSelected(j))
               mItems[i]->setVert(mOriginalVertLocations[count++] + delta, j);

         // If we are dragging a vertex, and not the entire item, we are changing the geom, so notify the item
         if(mItems[i]->anyVertsSelected())
            mItems[i]->onGeomChanging();  

         if(mItems[i]->isSelected())     
            mItems[i]->onItemDragging();      // Make sure this gets run after we've updated the item's location
      }
}


// User just dragged an item off the dock
void EditorUserInterface::startDraggingDockItem()
{
   // Instantiate object so we are in essence dragging a non-dock item
   EditorObject *item = mDockItems[mDraggingDockItem]->newCopy();
   item->newObjectFromDock(getGridSize());

   //item->initializeEditor(getGridSize());    // Override this to define some initial geometry for your object... 
   item->setDockItem(false);

   // Offset lets us drag an item out from the dock by an amount offset from the 0th vertex.  This makes placement seem more natural.
   Point pos = snapPoint(convertCanvasToLevelCoord(mMousePos), true) - item->getInitialPlacementOffset(getGridSize());
   item->moveTo(pos);
      
   item->setWidth((mDockItems[mDraggingDockItem]->getGeomType() == geomPoly) ? .7 : 1);      // TODO: Still need this?

   item->addToEditor(gEditorGame);

   // A little hack here to keep the polywall fill from appearing to be left behind behind the dock
   //if(item->getObjectTypeMask() & PolyWallType)
   //   item->clearPolyFillPoints();

   clearSelection();            // No items are selected...
   item->setSelected(true);     // ...except for the new one
   geomSort(mItems);            // So things will render in the proper order
   mDraggingDockItem = NONE;    // Because now we're dragging a real item
   validateLevel();             // Check level for errors


   // Because we sometimes have trouble finding an item when we drag it off the dock, after it's been sorted,
   // we'll manually set mItemHit based on the selected item, which will always be the one we just added.
   mEdgeHit = NONE;
   for(S32 i = 0; i < mItems.size(); i++)
      if(mItems[i]->isSelected())
      {
         mItemHit = mItems[i].get();
         break;
      }
}


// Sets mSnapVertex_i and mSnapVertex_j based on the vertex closest to the cursor that is part of the selected set
// What we really want is the closest vertex in the closest feature
void EditorUserInterface::findSnapVertex()
{
   F32 closestDist = F32_MAX;

   if(mDraggingObjects)    // Don't change snap vertex once we're dragging
      return;

   mSnapVertex_i = NULL;
   mSnapVertex_j = NONE;

   Point mouseLevelCoord = convertCanvasToLevelCoord(mMousePos);

   // If we have a hit item, and it's selected, find the closest vertex in the item
   if(mItemHit && mItemHit->isSelected())   
   {
      // If we've hit an edge, restrict our search to the two verts that make up that edge
      if(mEdgeHit != NONE)
      {
         mSnapVertex_i = mItemHit;     // Regardless of vertex, this is our hit item
         S32 v1 = mEdgeHit;
         S32 v2 = mEdgeHit + 1;

         // Handle special case of looping item
         if(mEdgeHit == mItemHit->getVertCount() - 1)
            v2 = 0;

         // Find closer vertex: v1 or v2
         mSnapVertex_j = (mItemHit->getVert(v1).distSquared(mouseLevelCoord) < 
                          mItemHit->getVert(v2).distSquared(mouseLevelCoord)) ? v1 : v2;

         return;
      }

      // Didn't hit an edge... find the closest vertex anywhere in the item
      for(S32 j = 0; j < mItemHit->getVertCount(); j++)
      {
         F32 dist = mItemHit->getVert(j).distSquared(mouseLevelCoord);

         if(dist < closestDist)
         {
            closestDist = dist;
            mSnapVertex_i = mItemHit;
            mSnapVertex_j = j;
         }
      }
      return;
   } 

   // Otherwise, we don't have a selected hitItem -- look for a selected vertex
   for(S32 i = 0; i < mItems.size(); i++)
      for(S32 j = 0; j < mItems[i]->getVertCount(); j++)
      {
         // If we find a selected vertex, there will be only one, and this is our snap point
         if(mItems[i]->vertSelected(j))
         {
            mSnapVertex_i = mItems[i].get();
            mSnapVertex_j = j;
            return;     
         }
      }
}


void EditorUserInterface::deleteSelection(bool objectsOnly)
{
   if(mDraggingObjects)     // No deleting while we're dragging, please...
      return;

   if(!anythingSelected())  // Nothing to delete
      return;

   bool deleted = false;

   for(S32 i = mItems.size()-1; i >= 0; i--)  // Reverse to avoid having to have i-- in middle of loop
   {
      if(mItems[i]->isSelected())
      {  
         // Since indices change as items are deleted, this will keep incorrect items from being deleted
         if(mItems[i]->isLitUp())
            mItemToLightUp = NULL;

         if(!deleted)
            saveUndoState();

         deleteItem(i);
         deleted = true;
      }
      else if(!objectsOnly)      // Deleted any selected vertices
      {
         bool geomChanged = false;

         for(S32 j = 0; j < mItems[i]->getVertCount(); j++) 
         {
            if(mItems[i]->vertSelected(j))
            {
               
               if(!deleted)
                  saveUndoState();
              
               mItems[i]->deleteVert(j);
               deleted = true;

               geomChanged = true;
               mSnapVertex_i = NULL;
               mSnapVertex_j = NONE;
            }
         }

         // Deleted last vertex, or item can't lose a vertex... it must go!
         if(mItems[i]->getVertCount() == 0 || (mItems[i]->getGeomType() == geomSimpleLine && mItems[i]->getVertCount() < 2)
                                       || (mItems[i]->getGeomType() == geomLine && mItems[i]->getVertCount() < 2)
                                       || (mItems[i]->getGeomType() == geomPoly && mItems[i]->getVertCount() < 2))
         {
            deleteItem(i);
            deleted = true;
         }
         else if(geomChanged)
            mItems[i]->onGeomChanged();

      }  // else if(!objectsOnly) 
   }  // for

   if(deleted)
   {
      mNeedToSave = true;
      autoSave();

      mItemToLightUp = NULL;     // In case we just deleted a lit item; not sure if really needed, as we do this above
      //vertexToLightUp = NONE;
   }
}

// Increase selected wall thickness by amt
void EditorUserInterface::incBarrierWidth(S32 amt)
{
   if(!mLastUndoStateWasBarrierWidthChange)
      saveUndoState(); 

   for(S32 i = 0; i < mItems.size(); i++)
      if(mItems[i]->isSelected())
         mItems[i]->increaseWidth(amt);

   mLastUndoStateWasBarrierWidthChange = true;
}


// Decrease selected wall thickness by amt
void EditorUserInterface::decBarrierWidth(S32 amt)
{
   if(!mLastUndoStateWasBarrierWidthChange)
      saveUndoState(); 

   for(S32 i = 0; i < mItems.size(); i++)
      if(mItems[i]->isSelected())
         mItems[i]->decreaseWidth(amt);

   mLastUndoStateWasBarrierWidthChange = true;
}


// Split wall/barrier on currently selected vertex/vertices
void EditorUserInterface::splitBarrier()
{
   bool split = false;

   for(S32 i = 0; i < mItems.size(); i++)
      if(mItems[i]->getGeomType() == geomLine)
          for(S32 j = 1; j < mItems[i]->getVertCount() - 1; j++)     // Can't split on end vertices!
            if(mItems[i]->vertSelected(j))
            {
               if(!split)
                  saveUndoState();
               split = true;

               // Create a poor man's copy
               EditorObject *newItem = mItems[i]->newCopy();
               newItem->setTeam(-1);
               newItem->setWidth(mItems[i]->getWidth());
               newItem->clearVerts();

               for(S32 k = j; k < mItems[i]->getVertCount(); k++) 
               {
                  newItem->addVert(mItems[i]->getVert(k));
                  if (k > j)
                  {
                     mItems[i]->deleteVert(k);     // Don't delete j == k vertex -- it needs to remain as the final vertex of the old wall
                     k--;
                  }
               }


               // Tell the new segments that they have new geometry
               mItems[i]->onGeomChanged();
               newItem->onGeomChanged();
               mItems.push_back(boost::shared_ptr<EditorObject>(newItem));

               // And get them in the right order
               //mItems.sort(geometricSort);  
               geomSort(mItems);
               goto done2;                         // Yes, gotos are naughty, but they just work so well sometimes...
            }
done2:
   if(split)
   {
      clearSelection();
      mNeedToSave = true;
      autoSave();
   }
}


// Join two or more sections of wall that have coincident end points.  Will ignore invalid join attempts.
void EditorUserInterface::joinBarrier()
{
   S32 joinedItem = NONE;

   for(S32 i = 0; i < mItems.size()-1; i++)
      if(mItems[i]->getGeomType() == geomLine && (mItems[i]->isSelected()))
      {
         for(S32 j = i + 1; j < mItems.size(); j++)
         {
            if(mItems[j]->getObjectTypeMask() & mItems[i]->getObjectTypeMask() && (mItems[j]->isSelected()))
            {
               if(mItems[i]->getVert(0).distanceTo(mItems[j]->getVert(0)) < .01)    // First vertices are the same  1 2 3 | 1 4 5
               {
                  if(joinedItem == NONE)
                     saveUndoState();
                  joinedItem = i;

                  for(S32 a = 1; a < mItems[j]->getVertCount(); a++)             // Skip first vertex, because it would be a dupe
                     mItems[i]->addVertFront(mItems[j]->getVert(a));

                  deleteItem(j);
                  i--;  j--;
               }
               // First vertex conincides with final vertex 3 2 1 | 5 4 3
               else if(mItems[i]->getVert(0).distanceTo(mItems[j]->getVert(mItems[j]->getVertCount()-1)) < .01)     
               {
                  if(joinedItem == NONE)
                     saveUndoState();

                  joinedItem = i;
                  for(S32 a = mItems[j]->getVertCount()-2; a >= 0; a--)
                     mItems[i]->addVertFront(mItems[j]->getVert(a));

                  deleteItem(j);
                  i--;  j--;

               }
               // Last vertex conincides with first 1 2 3 | 3 4 5
               else if(mItems[i]->getVert(mItems[i]->getVertCount()-1).distanceTo(mItems[j]->getVert(0)) < .01)     
               {
                  if(joinedItem == NONE)
                     saveUndoState();

                  joinedItem = i;

                  for(S32 a = 1; a < mItems[j]->getVertCount(); a++)  // Skip first vertex, because it would be a dupe         
                     mItems[i]->addVert(mItems[j]->getVert(a));

                  deleteItem(j);
                  i--;  j--;
               }
               else if(mItems[i]->getVert(mItems[i]->getVertCount()-1).distanceTo(mItems[j]->getVert(mItems[j]->getVertCount()-1)) < .01)     // Last vertices coincide  1 2 3 | 5 4 3
               {
                  if(joinedItem == NONE)
                     saveUndoState();

                  joinedItem = i;

                  for(S32 a = mItems[j]->getVertCount()-2; a >= 0; a--)
                     mItems[i]->addVert(mItems[j]->getVert(a));

                  deleteItem(j);
                  i--;  j--;
               }
            }
         }
      }

   if(joinedItem != NONE)
   {
      clearSelection();
      mNeedToSave = true;
      autoSave();
      mItems[joinedItem]->onGeomChanged();
   }
}


void EditorUserInterface::deleteItem(S32 itemIndex)
{
   S32 mask = mItems[itemIndex]->getObjectTypeMask();
   if(mask & BarrierType || mask & PolyWallType)
   {
      // Need to recompute boundaries of any intersecting walls
      mWallSegmentManager.invalidateIntersectingSegments(mItems[itemIndex].get());    // Mark intersecting segments invalid
      mWallSegmentManager.deleteSegments(mItems[itemIndex]->getItemId());       // Delete the segments associated with the wall

      mItems.erase(itemIndex);

      mWallSegmentManager.recomputeAllWallGeometry();                           // Recompute wall edges
      resnapAllEngineeredItems();         // Really only need to resnap items that were attached to deleted wall... but we
                                          // don't yet have a method to do that, and I'm feeling lazy at the moment
   }
   else
      mItems.erase(itemIndex);


   // Reset a bunch of things
   mSnapVertex_i = NULL;
   mSnapVertex_j = NONE;
   mItemToLightUp = NULL;

   validateLevel();

   onMouseMoved();   // Reset cursor  
}

// Process a level line as the level is being loaded
//void EditorUserInterface::processLevelLoadLine(U32 argc, U32 id, const char **argv) 
//{
//   EditorObject *newObject = newEditorObject(argv[0]);
//
//   newObject->addToEditor(gEditorGame);
//   newObject->processArguments(argc, argv);
//}


// Process a line read from level file
void EditorUserInterface::processLevelLoadLine(U32 argc, U32 id, const char **argv)
{
   Game *game = gEditorGame;
   S32 strlenCmd = (S32) strlen(argv[0]);

   // This is a legacy from the old Zap! days... we do bots differently in Bitfighter, so we'll just ignore this line if we find it.
   if(!stricmp(argv[0], "BotsPerTeam"))
      return;

   else if(!stricmp(argv[0], "GridSize"))      // GridSize requires a single parameter (an int specifiying how many pixels in a grid cell)
   {                                           
      if(argc < 2)
         throw LevelLoadException("Improperly formed GridSize parameter");

      game->setGridSize(atof(argv[1]));
   }

   // Parse GameType line... All game types are of form XXXXGameType
   else if(strlenCmd >= 8 && !strcmp(argv[0] + strlenCmd - 8, "GameType"))
   {
      if(gEditorGame->getGameType())
         throw LevelLoadException("Duplicate GameType parameter");

      // validateGameType() will return a valid GameType string -- either what's passed in, or the default if something bogus was specified
      TNL::Object *theObject = TNL::Object::create(GameType::validateGameType(argv[0]));      
      GameType *gt = dynamic_cast<GameType *>(theObject);  // Force our new object to be a GameObject

      bool validArgs = gt->processArguments(argc - 1, argv + 1);

      gEditorGame->setGameType(gt);

      if(!validArgs || strcmp(gt->getClassName(), argv[0]))
         throw LevelLoadException("Improperly formed GameType parameter");
      
      // Save the args (which we already have split out) for easier handling in the Game Parameter Editor
      //for(U32 i = 1; i < argc; i++)
      //   gEditorUserInterface.mGameTypeArgs.push_back(argv[i]);
   }
   else if(gEditorGame->getGameType() && gEditorGame->getGameType()->processLevelParam(argc, argv)) 
   {
      // Do nothing here
   }

   // here on up is same as in server game... TODO: unify!

   else if(!strcmp(argv[0], "Script"))
   {
      gEditorUserInterface.mScriptLine = "";
      // Munge everything into a string.  We'll have to parse after editing in GameParamsMenu anyway.
      for(U32 i = 1; i < argc; i++)
      {
         if(i > 1)
            gEditorUserInterface.mScriptLine += " ";

         gEditorUserInterface.mScriptLine += argv[i];
      }
   }

   // Parse Team definition line
   else if(!strcmp(argv[0], "Team"))
   {
      if(mTeams.size() >= GameType::gMaxTeams)     // Ignore teams with numbers higher than 9
         return;

      TeamEditor team;
      team.readTeamFromLevelLine(argc, argv);

      // If team was read and processed properly, numPlayers will be 0
      if(team.numPlayers != -1)
         mTeams.push_back(team);
   }

   else
   {
      string objectType = argv[0];

      if(objectType == "BarrierMakerS")
      {
         objectType = "PolyWall";
      }

      TNL::Object *theObject = TNL::Object::create(objectType.c_str());   // Create an object of the type specified on the line
      EditorObject *object = dynamic_cast<EditorObject *>(theObject);     // Coerce our new object to be a GameObject

      if(!object)    // Well... that was a bad idea!
      {
         delete theObject;
      }
      else  // object was valid
      {
         object->addToEditor(gEditorGame);
         object->processArguments(argc, argv);

         //mLoadTarget->push_back(newItem);           // Don't add to editor if not valid...

         return;
      }
   }

   // What remains are various game parameters...  Note that we will hit this block even if we already looked at gridSize and such...
   // Before copying, we'll make a dumb copy, which will be overwritten if the user goes into the GameParameters menu
   // This will cover us if the user comes in, edits the level, saves, and exits without visiting the GameParameters menu
   // by simply echoing all the parameters back out to the level file without further processing or review.
   string temp;
   for(U32 i = 0; i < argc; i++)
   {
      temp += argv[i];
      if(i < argc - 1)
         temp += " ";
   }

   gGameParamUserInterface.gameParams.push_back(temp);
} 


void EditorUserInterface::insertNewItem(GameObjectType itemType)
{
   if(mShowMode == ShowWallsOnly || mDraggingObjects)     // No inserting when items are hidden or being dragged!
      return;

   clearSelection();
   saveUndoState();

   S32 team = -1;

   EditorObject *newObject = NULL;

   // Find a dockItem to copy
   for(S32 i = 0; i < mDockItems.size(); i++)
      if(mDockItems[i]->getObjectTypeMask() & itemType)
      {
         newObject = mDockItems[i]->newCopy();
         newObject->initializeEditor(getGridSize());
         newObject->onGeomChanged();

         break;
      }
   TNLAssert(newObject, "Couldn't create object in insertNewItem()");

   newObject->moveTo(snapPoint(convertCanvasToLevelCoord(mMousePos)));
   newObject->addToEditor(gEditorGame);     // Adds newItem to mItems list

   //mItems.sort(geometricSort);
   geomSort(mItems);
   validateLevel();
   mNeedToSave = true;
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
   if(mItems.size() || mLevelGenItems.size())
   {
      F32 minx =  F32_MAX,   miny =  F32_MAX;
      F32 maxx = -F32_MAX,   maxy = -F32_MAX;

      for(S32 i = 0; i < mItems.size(); i++)
         for(S32 j = 0; j < mItems[i]->getVertCount(); j++)
         {
            if(mItems[i]->getVert(j).x < minx)
               minx = mItems[i]->getVert(j).x;
            if(mItems[i]->getVert(j).x > maxx)
               maxx = mItems[i]->getVert(j).x;
            if(mItems[i]->getVert(j).y < miny)
               miny = mItems[i]->getVert(j).y;
            if(mItems[i]->getVert(j).y > maxy)
               maxy = mItems[i]->getVert(j).y;
         }

      for(S32 i = 0; i < mLevelGenItems.size(); i++)
         for(S32 j = 0; j < mLevelGenItems[i]->getVertCount(); j++)
         {
            if(mLevelGenItems[i]->getVert(j).x < minx)
               minx = mLevelGenItems[i]->getVert(j).x;
            if(mLevelGenItems[i]->getVert(j).x > maxx)
               maxx = mLevelGenItems[i]->getVert(j).x;
            if(mLevelGenItems[i]->getVert(j).y < miny)
               miny = mLevelGenItems[i]->getVert(j).y;
            if(mLevelGenItems[i]->getVert(j).y > maxy)
               maxy = mLevelGenItems[i]->getVert(j).y;
         }

      // If we have only one point object in our level, the following will correct
      // for any display weirdness.
      if(minx == maxx && miny == maxy)    // i.e. a single point item
      {
         mCurrentScale = MIN_SCALE;
         mCurrentOffset.set(gScreenInfo.getGameCanvasWidth() / 2  - mCurrentScale * minx, 
                            gScreenInfo.getGameCanvasHeight() / 2 - mCurrentScale * miny);
      }
      else
      {
         F32 midx = (minx + maxx) / 2;
         F32 midy = (miny + maxy) / 2;

         mCurrentScale = min(gScreenInfo.getGameCanvasWidth() / (maxx - minx), gScreenInfo.getGameCanvasHeight() / (maxy - miny));
         mCurrentScale /= 1.3;      // Zoom out a bit
         mCurrentOffset.set(gScreenInfo.getGameCanvasWidth() / 2  - mCurrentScale * midx, 
                            gScreenInfo.getGameCanvasHeight() / 2 - mCurrentScale * midy);
      }
   }
   else     // Put (0,0) at center of screen
   {
      mCurrentScale = STARTING_SCALE;
      mCurrentOffset.set(gScreenInfo.getGameCanvasWidth() / 2, gScreenInfo.getGameCanvasHeight() / 2);
   }
}


// Gets run when user exits special-item editing mode, called from attribute editors
void EditorUserInterface::doneEditingAttributes(EditorAttributeMenuUI *editor, EditorObject *object)
{
   object->onAttrsChanged();

   // Find any other selected items of the same type of the item we just edited, and update their values too
   for(S32 i = 0; i < mItems.size(); i++)
   {
      if(mItems[i].get() != object && mItems[i]->isSelected() && mItems[i]->getObjectTypeMask() == object->getObjectTypeMask())
      {
         editor->doneEditing(mItems[i].get());  // Transfer attributes from editor to object
         mItems[i]->onAttrsChanged();     // And notify the object that its attributes have changed
      }
   }
}


extern string itos(S32);
extern string itos(U32);
extern string itos(U64);

extern string ftos(F32, S32);

// Handle key presses
void EditorUserInterface::onKeyDown(KeyCode keyCode, char ascii)
{
   if(OGLCONSOLE_ProcessBitfighterKeyEvent(keyCode, ascii))      // Pass the key on to the console for processing
      return;

   // TODO: Make this stuff work like the attribute entry stuff; use a real menu and not this ad-hoc code
   // This is where we handle entering things like rotation angle and other data that requires a special entry box.
   // NOT for editing an item's attributes.  Still used, but untested in refactor.
   if(entryMode != EntryNone)
      textEntryKeyHandler(keyCode, ascii);

   else if(keyCode == KEY_ENTER)       // Enter - Edit props
      startAttributeEditor();

   // Regular key handling from here on down
   else if(getKeyState(KEY_SHIFT) && keyCode == KEY_0)  // Shift-0 -> Set team to hostile
      setCurrentTeam(-2);

   else if(ascii == '#' || ascii == '!')
   {
      S32 selected = NONE;

      // Find first selected item, and just work with that.  Unselect the rest.
      for(S32 i = 0; i < mItems.size(); i++)
      {
         if(mItems[i]->isSelected())
         {
            if(selected == NONE)
            {
               selected = i;
               continue;
            }
            else
               mItems[i]->setSelected(false);
         }
      }

      if(selected == NONE)      // Nothing selected, nothing to do!
         return;

      mEntryBox = getNewEntryBox(mItems[selected]->getItemId() <= 0 ? "" : itos(mItems[selected]->getItemId()), 
                                 "Item ID:", 10, LineEditor::digitsOnlyFilter);
      entryMode = EntryID;
   }

   else if(ascii >= '0' && ascii <= '9')           // Change team affiliation of selection with 0-9 keys
   {
      setCurrentTeam(ascii - '1');
      return;
   }

   // Ctrl-left click is same as right click for Mac users
   else if(keyCode == MOUSE_RIGHT || (keyCode == MOUSE_LEFT && getKeyState(KEY_CTRL)))
   {
      if(getKeyState(MOUSE_LEFT) && !getKeyState(KEY_CTRL))    // Prevent weirdness
         return;  

      mMousePos.set(gScreenInfo.getMousePos());

      if(mCreatingPoly || mCreatingPolyline)
      {
         if(mNewItem->getVertCount() < gMaxPolygonPoints)          // Limit number of points in a polygon/polyline
         {
            mNewItem->addVert(snapPoint(convertCanvasToLevelCoord(mMousePos)));
            mNewItem->onGeomChanging();
         }
         
         return;
      }

      saveUndoState();     // Save undo state before we clear the selection
      clearSelection();    // Unselect anything currently selected

      // Can only add new vertices by clicking on item's edge, not it's interior (for polygons, that is)
      if(mEdgeHit != NONE && mItemHit && (mItemHit->getGeomType() == geomLine || mItemHit->getGeomType() >= geomPoly))
      {
         if(mItemHit->getVertCount() >= gMaxPolygonPoints)     // Polygon full -- can't add more
            return;

         Point newVertex = snapPoint(convertCanvasToLevelCoord(mMousePos));      // adding vertex w/ right-mouse

         mAddingVertex = true;

         // Insert an extra vertex at the mouse clicked point, and then select it.
         mItemHit->insertVert(newVertex, mEdgeHit + 1);
         mItemHit->selectVert(mEdgeHit + 1);

         // Alert the item that its geometry is changing
         mItemHit->onGeomChanging();

         mMouseDownPos = newVertex;
         
      }
      else     // Start creating a new poly or new polyline (tilda key + right-click ==> start polyline)
      {
         S32 width;

         if(getKeyState(KEY_TILDE))
         {
            mCreatingPolyline = true;
            mNewItem = new LineItem();
            width = 2;
         }
         else
         {
            mCreatingPoly = true;
            width = Barrier::BarrierWidth;
            mNewItem = new WallItem();
         }

        
         mNewItem->initializeEditor(getGridSize());
         mNewItem->setTeam(mCurrentTeam);
         mNewItem->addVert(snapPoint(convertCanvasToLevelCoord(mMousePos)));
         mNewItem->setDockItem(false);
      }
   }
   else if(keyCode == MOUSE_LEFT)
   {
      if(getKeyState(MOUSE_RIGHT))              // Prevent weirdness
         return;

      mDraggingDockItem = NONE;
      mMousePos.set(gScreenInfo.getMousePos());

      if(mCreatingPoly || mCreatingPolyline)    // Save any polygon/polyline we might be creating
      {
         saveUndoState();                       // Save state prior to addition of new polygon

         if(mNewItem->getVertCount() <= 1)
            delete mNewItem;
         else
         {
            mNewItem->addToEditor(gEditorGame);
            mNewItem->onGeomChanged();          // Walls need to be added to editor BEFORE onGeomChanged() is run!
            geomSort(mItems);
         }

         mNewItem = NULL;

         mCreatingPoly = false;
         mCreatingPolyline = false;
      }

      mMouseDownPos = convertCanvasToLevelCoord(mMousePos);

      if(mouseOnDock())    // On the dock?  Did we hit something to start dragging off the dock?
      {
         clearSelection();
         mDraggingDockItem = findHitItemOnDock(mMousePos);
      }
      else                 // Mouse is not on dock
      {
         mDraggingDockItem = NONE;

         // rules for mouse down:
         // if the click has no shift- modifier, then
         //   if the click was on something that was selected
         //     do nothing
         //   else
         //     clear the selection
         //     add what was clicked to the selection
         //  else
         //    toggle the selection of what was clicked

        /* S32 vertexHit;
         EditorObject *vertexHitPoly;
*/
         //findHitVertex(mMousePos, vertexHitPoly, vertexHit);
         //findHitItemAndEdge();      //  Sets mItemHit, mVertexHit, and mEdgeHit


         if(!getKeyState(KEY_SHIFT))      // Shift key is not down
         {
            // If we hit a vertex of an already selected item --> now we can move that vertex w/o losing our selection.
            // Note that in the case of a point item, we want to skip this step, as we don't select individual vertices.
            if(mVertexHit != NONE && mItemHit->isSelected() && mItemHit->getGeomType() != geomPoint)    
            {
               clearSelection();
               mItemHit->selectVert(mVertexHit);
            }
            if(mItemHit && mItemHit->isSelected())   // Hit an already selected item
            {
               // Do nothing
            }
            else if(mItemHit && mItemHit->getGeomType() == geomPoint)  // Hit a point item
            {
               clearSelection();
               mItemHit->setSelected(true);
            }
            else if(mVertexHit != NONE && (!mItemHit || !mItemHit->isSelected()))      // Hit a vertex of an unselected item
            {        // (braces required)
               if(!mItemHit->vertSelected(mVertexHit))
               {
                  clearSelection();
                  mItemHit->selectVert(mVertexHit);
               }
            }
            else if(mItemHit)                                                        // Hit a non-point item, but not a vertex
            {
               clearSelection();
               mItemHit->setSelected(true);
            }
            else     // Clicked off in space.  Starting to draw a bounding rectangle?
            {
               mDragSelecting = true;
               clearSelection();
            }
         }
         else     // Shift key is down
         {
            if(mVertexHit != NONE)
            {
               if(mItemHit->vertSelected(mVertexHit))
                  mItemHit->unselectVert(mVertexHit);
               else
                  mItemHit->aselectVert(mVertexHit);
            }
            else if(mItemHit)
               mItemHit->setSelected(!mItemHit->isSelected());    // Toggle selection of hit item
            else
               mDragSelecting = true;
         }
     }     // end mouse not on dock block, doc

     findSnapVertex();     // Update snap vertex in the event an item was selected

   }     // end if keyCode == MOUSE_LEFT

   // Neither mouse button, let's try some keys
   else if(keyCode == KEY_D)              // D - Pan right
      mRight = true;
   else if(keyCode == KEY_RIGHT)          // Right - Pan right
      mRight = true;
   else if(keyCode == KEY_H)              // H - Flip horizontal
      flipSelectionHorizontal();
   else if(keyCode == KEY_V && getKeyState(KEY_CTRL))    // Ctrl-V - Paste selection
      pasteSelection();
   else if(keyCode == KEY_V)              // V - Flip vertical
      flipSelectionVertical();
   else if(keyCode == KEY_SLASH)
      OGLCONSOLE_ShowConsole();

   else if(keyCode == KEY_L && getKeyState(KEY_CTRL) && getKeyState(KEY_SHIFT))
   {
      loadLevel();                        // Ctrl-Shift-L - Reload level
      gEditorUserInterface.setSaveMessage("Reloaded " + getLevelFileName(), true);
   }
   else if(keyCode == KEY_Z)
   {
      if(getKeyState(KEY_CTRL) && getKeyState(KEY_SHIFT))   // Ctrl-Shift-Z - Redo
         redo();
      else if(getKeyState(KEY_CTRL))    // Ctrl-Z - Undo
         undo(true);
      else                              // Z - Reset veiw
        centerView();
   }
   else if(keyCode == KEY_R)
      if(getKeyState(KEY_CTRL) && getKeyState(KEY_SHIFT))      // Ctrl-Shift-R - Rotate by arbitrary amount
      {
         if(!anyItemsSelected())
            return;

         mEntryBox = getNewEntryBox("", "Rotation angle:", 10, LineEditor::numericFilter);
         entryMode = EntryAngle;
      }
      else if(getKeyState(KEY_CTRL))        // Ctrl-R - Run levelgen script, or clear last results
      {
         if(mLevelGenItems.size() == 0)
            runLevelGenScript();
         else
            clearLevelGenItems();
      }
      else
         rotateSelection(getKeyState(KEY_SHIFT) ? 15 : -15); // Shift-R - Rotate CW, R - Rotate CCW
   else if((keyCode == KEY_I) && getKeyState(KEY_CTRL))  // Ctrl-I - Insert items generated with script into editor
   {
      copyScriptItemsToEditor();
   }

   else if((keyCode == KEY_UP) && !getKeyState(KEY_CTRL) || keyCode == KEY_W)  // W or Up - Pan up
      mUp = true;
   else if(keyCode == KEY_UP && getKeyState(KEY_CTRL))      // Ctrl-Up - Zoom in
      mIn = true;
   else if(keyCode == KEY_DOWN)
   { /* braces required */
      if(getKeyState(KEY_CTRL))           // Ctrl-Down - Zoom out
         mOut = true;
      else                                // Down - Pan down
         mDown = true;
   }
   else if(keyCode == KEY_S)
   {
      if(getKeyState(KEY_CTRL))           // Ctrl-S - Save
         gEditorUserInterface.saveLevel(true, true);
      else                                // S - Pan down
         mDown = true;
   }
   else if(keyCode == KEY_A && getKeyState(KEY_CTRL))            // Ctrl-A - toggle see all objects
   {
      mShowMode = (ShowMode) ((U32)mShowMode + 1);
      //if(mShowMode == ShowAllButNavZones && !mHasBotNavZones)    // Skip hiding NavZones if we don't have any
      //   mShowMode = (ShowMode) ((U32)mShowMode + 1);

      if(mShowMode == ShowModesCount)
         mShowMode = (ShowMode) 0;     // First mode

      if(mShowMode == ShowWallsOnly && !mDraggingObjects)
         glutSetCursor(GLUT_CURSOR_RIGHT_ARROW);

      populateDock();   // Different modes have different items

      onMouseMoved();   // Reset mouse to spray if appropriate
   }
   else if(keyCode == KEY_LEFT || keyCode == KEY_A)   // Left or A - Pan left
      mLeft = true;
   else if(keyCode == KEY_EQUALS)         // Plus (+) - Increase barrier width
   {
      if(getKeyState(KEY_SHIFT))          // SHIFT --> by 1
         incBarrierWidth(1);
      else                                // unshifted --> by 5
         incBarrierWidth(5);
   }
   else if(keyCode == KEY_MINUS)          // Minus (-)  - Decrease barrier width
   {
      if(getKeyState(KEY_SHIFT))          // SHIFT --> by 1
         decBarrierWidth(1);
      else                                // unshifted --> by 5
         decBarrierWidth(5);
   }

   else if(keyCode == KEY_E)              // E - Zoom In
         mIn = true;
   else if(keyCode == KEY_BACKSLASH)      // \ - Split barrier on selected vertex
      splitBarrier();
   else if(keyCode == KEY_J)
      joinBarrier();
   else if(keyCode == KEY_X && getKeyState(KEY_CTRL) && getKeyState(KEY_SHIFT)) // Ctrl-Shift-X - Resize selection
   {
      if(!anyItemsSelected())
         return;

      mEntryBox = getNewEntryBox("", "Resize factor:", 10, LineEditor::numericFilter);
      entryMode = EntryScale;
   }
   else if(keyCode == KEY_X && getKeyState(KEY_CTRL))     // Ctrl-X - Cut selection
   {
      copySelection();
      deleteSelection(true);
   }
   else if(keyCode == KEY_C && getKeyState(KEY_CTRL))    // Ctrl-C - Copy selection to clipboard
      copySelection();
   else if(keyCode == KEY_C )             // C - Zoom out
      mOut = true;
   else if(keyCode == KEY_F3)             // F3 - Level Parameter Editor
   {
      UserInterface::playBoop();
      gGameParamUserInterface.activate();
   }
   else if(keyCode == KEY_F2)             // F2 - Team Editor Menu
   {
      gTeamDefUserInterface.activate();
      UserInterface::playBoop();
   }
   else if(keyCode == KEY_T)              // T - Teleporter
      insertNewItem(TeleportType);
   else if(keyCode == KEY_P)              // P - Speed Zone
      insertNewItem(SpeedZoneType);
   else if(keyCode == KEY_G)              // G - Spawn
      insertNewItem(ShipSpawnType);
   else if(keyCode == KEY_B && getKeyState(KEY_CTRL)) // Ctrl-B - Spy Bug
      insertNewItem(SpyBugType);
   else if(keyCode == KEY_B)              // B - Repair
      insertNewItem(RepairItemType);
   else if(keyCode == KEY_Y)              // Y - Turret
      insertNewItem(TurretType);
   else if(keyCode == KEY_M)              // M - Mine
      insertNewItem(MineType);
   else if(keyCode == KEY_F)              // F - Force Field
      insertNewItem(ForceFieldProjectorType);
   else if(keyCode == KEY_BACKSPACE || keyCode == KEY_DELETE)
         deleteSelection(false);
   else if(keyCode == keyHELP)            // Turn on help screen
   {
      gEditorInstructionsUserInterface.activate();
      UserInterface::playBoop();
   }
   else if(keyCode == keyOUTGAMECHAT)     // Turn on Global Chat overlay
      gChatInterface.activate();
//   else if(keyCode == keyDIAG)            // Turn on diagnostic overlay -- why not here??
//      gDiagnosticInterface.activate();
   else if(keyCode == KEY_ESCAPE)           // Activate the menu
   {
      UserInterface::playBoop();
      gEditorMenuUserInterface.activate();
   }
   else if(keyCode == KEY_SPACE)
      mSnapDisabled = true;
   else if(keyCode == KEY_TAB)
      mShowingReferenceShip = true;
}


// Handle keyboard activity when we're editing an item's attributes
void EditorUserInterface::textEntryKeyHandler(KeyCode keyCode, char ascii)
{
   if(keyCode == KEY_ENTER)
   {
      if(entryMode == EntryID)
      {
         for(S32 i = 0; i < mItems.size(); i++)
            if(mItems[i]->isSelected())             // Should only be one
            {
               U32 id = atoi(mEntryBox.c_str());
               if(mItems[i]->getItemId() != (S32)id)     // Did the id actually change?
               {
                  mItems[i]->setItemId(id);
                  mAllUndoneUndoLevel = -1;        // If so, it can't be undone
               }
               break;
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
   else if(keyCode == KEY_ESCAPE)
   {
      entryMode = EntryNone;
   }
   else if(keyCode == KEY_BACKSPACE || keyCode == KEY_DELETE)
      mEntryBox.handleBackspace(keyCode);

   else
      mEntryBox.addChar(ascii);

   // else ignore keystroke
}


static const S32 MAX_REPOP_DELAY = 600;      // That's 10 whole minutes!

void EditorUserInterface::startAttributeEditor()
{
   for(S32 i = 0; i < mItems.size(); i++)
   {
      if(mItems[i]->isSelected())
      {
         // Force item i to be the one and only selected item type.  This will clear up some problems that
         // might otherwise occur.  If you have multiple items selected, all will end up with the same values.
         for(S32 j = 0; j < mItems.size(); j++)
            if(mItems[j]->isSelected() && mItems[j]->getObjectTypeMask() != mItems[i]->getObjectTypeMask())
               mItems[j]->unselect();


         // Activate the attribute editor if there is one
         EditorAttributeMenuUI *menu = mItems[i]->getAttributeMenu();
         if(menu)
         {
            mItems[i]->setIsBeingEdited(true);
            menu->startEditing(mItems[i].get());
            menu->activate();

            saveUndoState();
         }

         break;
      }
   }
}


void EditorUserInterface::onKeyUp(KeyCode keyCode)
{
   switch(keyCode)
   {
      case KEY_UP:
         mIn = false;
         // fall-through OK
      case KEY_W:
         mUp = false;
         break;
      case KEY_DOWN:
         mOut = false;
         // fall-through OK
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
         mSnapDisabled = false;
         break;
      case KEY_TAB:
         mShowingReferenceShip = false;
         break;
      case MOUSE_LEFT:
      case MOUSE_RIGHT:  
         mMousePos.set(gScreenInfo.getMousePos());

         if(mDragSelecting)      // We were drawing a rubberband selection box
         {
            Rect r(convertCanvasToLevelCoord(mMousePos), mMouseDownPos);
            S32 j;

            for(S32 i = 0; i < mItems.size(); i++)
            {
               // Skip hidden items
               if(mShowMode == ShowWallsOnly)
               {
                  if(mItems[i]->getObjectTypeMask() & ~BarrierType && mItems[i]->getObjectTypeMask() & ~PolyWallType)
                     continue;
               }
               else if(mShowMode == ShowAllButNavZones)
               {
                  if(mItems[i]->getObjectTypeNumber() == BotNavMeshZoneTypeNumber)
                     continue;
               }

               // Make sure that all vertices of an item are inside the selection box; basically means that the entire 
               // item needs to be surrounded to be included in the selection
               for(j = 0; j < mItems[i]->getVertCount(); j++)
                  if(!r.contains(mItems[i]->getVert(j)))
                     break;
               if(j == mItems[i]->getVertCount())
                  mItems[i]->setSelected(true);
            }
            mDragSelecting = false;
         }
         else if(mDraggingObjects)     // We were dragging and dropping.  Could have been a move or a delete (by dragging to dock).
         {
            if(mAddingVertex)
            {
               deleteUndoState();
               mAddingVertex = false;
            }

            onFinishedDragging();
         }

         break;
   }     // case
}


// Called when user has been dragging an object and then releases it
void EditorUserInterface::onFinishedDragging()
{
   mDraggingObjects = false;

   if(mouseOnDock())                      // Mouse is over the dock -- either dragging to or from dock
   {
      if(mDraggingDockItem == NONE)       // This was really a delete (item dragged to dock)
      {
         for(S32 i = 0; i < mItems.size(); i++)    //  Delete all selected items
            if(mItems[i]->isSelected())
            {
               deleteItem(i);
               i--;
            }
      }
      else        // Dragged item off the dock, then back on  ==> nothing changed; restore to unmoved state, which was stored on undo stack
         undo(false);
   }
   else    // Mouse not on dock... we were either dragging from the dock or moving something, 
   {       // need to save an undo state if anything changed
      if(mDraggingDockItem == NONE)    // Not dragging from dock - user is moving object around screen
      {
         // If our snap vertex has moved then all selected items have moved
         bool itemsMoved = mSnapVertex_i->getVert(mSnapVertex_j) != mMoveOrigin;

         if(itemsMoved)    // Move consumated... update any moved items, and save our autosave
         {
            for(S32 i = 0; i < mItems.size(); i++)
               if(mItems[i]->isSelected() || mItems[i]->anyVertsSelected())
                  mItems[i]->onGeomChanged();

            mNeedToSave = true;
            autoSave();
            geomSort(mItems);  

            return;
         }
         else     // We started our move, then didn't end up moving anything... remove associated undo state
            deleteUndoState();
      }
   }
}


bool EditorUserInterface::mouseOnDock()
{
   return (mMousePos.x >= gScreenInfo.getGameCanvasWidth() - DOCK_WIDTH - horizMargin &&
           mMousePos.x <= gScreenInfo.getGameCanvasWidth() - horizMargin &&
           mMousePos.y >= gScreenInfo.getGameCanvasHeight() - vertMargin - getDockHeight(mShowMode) &&
           mMousePos.y <= gScreenInfo.getGameCanvasHeight() - vertMargin);
}


bool EditorUserInterface::anyItemsSelected()
{
   for(S32 i = 0; i < mItems.size(); i++)
      if(mItems[i]->isSelected())
         return true;

   return false;
}


bool EditorUserInterface::anythingSelected()
{
   for(S32 i = 0; i < mItems.size(); i++)
   {
      if(mItems[i]->isSelected() || mItems[i]->anyVertsSelected() )
         return true;
   }

   return false;
}


void EditorUserInterface::idle(U32 timeDelta)
{
   F32 pixelsToScroll = timeDelta * (getKeyState(KEY_SHIFT) ? 1.0f : 0.5f);    // Double speed when shift held down

   if(mLeft && !mRight)
      mCurrentOffset.x += pixelsToScroll;
   else if(mRight && !mLeft)
      mCurrentOffset.x -= pixelsToScroll;
   if(mUp && !mDown)
      mCurrentOffset.y += pixelsToScroll;
   else if(mDown && !mUp)
      mCurrentOffset.y -= pixelsToScroll;

   Point mouseLevelPoint = convertCanvasToLevelCoord(mMousePos);

   if(mIn && !mOut)
      mCurrentScale *= 1 + timeDelta * 0.002;
   else if(mOut && !mIn)
      mCurrentScale *= 1 - timeDelta * 0.002;

   if(mCurrentScale < MIN_SCALE)
     mCurrentScale = MIN_SCALE;
   else if(mCurrentScale > MAX_SCALE)
      mCurrentScale = MAX_SCALE;

   Point newMousePoint = convertLevelToCanvasCoord(mouseLevelPoint);
   mCurrentOffset += mMousePos - newMousePoint;

   mSaveMsgTimer.update(timeDelta);
   mWarnMsgTimer.update(timeDelta);

   LineEditor::updateCursorBlink(timeDelta);
}

// Unused??
static void escapeString(const char *src, char dest[1024])
{
   S32 i;
   for(i = 0; src[i]; i++)
      if(src[i] == '\"' || src[i] == ' ' || src[i] == '\n' || src[i] == '\t')
         break;

   if(!src[i])
   {
      strcpy(dest, src);
      return;
   }
   char *dptr = dest;
   *dptr++ = '\"';
   char c;
   while((c = *src++) != 0)
   {
      switch(c)
      {
         case '\"':
            *dptr++ = '\\';
            *dptr++ = '\"';
            break;
         case '\n':
            *dptr++ = '\\';
            *dptr++ = 'n';
            break;
         case '\t':
            *dptr++ = '\\';
            *dptr++ = 't';
            break;
         default:
            *dptr++ = c;
      }
   }
   *dptr++ = '\"';
   *dptr++ = 0;
}

void EditorUserInterface::setSaveMessage(string msg, bool savedOK)
{
   mSaveMsg = msg;
   mSaveMsgTimer = saveMsgDisplayTime;
   mSaveMsgColor = (savedOK ? Color(0, 1, 0) : red);
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
         gErrorMsgUserInterface.reset();
         gErrorMsgUserInterface.setTitle("INVALID FILE NAME");
         gErrorMsgUserInterface.setMessage(1, "The level file name is invalid or empty.  The level cannot be saved.");
         gErrorMsgUserInterface.setMessage(2, "To correct the problem, please change the file name using the");
         gErrorMsgUserInterface.setMessage(3, "Game Parameters menu, which you can access by pressing [F3].");

         gErrorMsgUserInterface.activate();

         return false;
      }

      char fileNameBuffer[256];
      dSprintf(fileNameBuffer, sizeof(fileNameBuffer), "%s/%s", gConfigDirs.levelDir.c_str(), saveName.c_str());
      FILE *f = fopen(fileNameBuffer, "w");
      if(!f)
         throw(SaveException("Could not open file for writing"));

      // Write out our game parameters --> first one will be the gameType, along with all required parameters
      for(S32 i = 0; i < gGameParamUserInterface.gameParams.size(); i++)
         if(gGameParamUserInterface.gameParams[i].substr(0, 5) != "Team ")  // Don't write out teams here... do it below!
            s_fprintf(f, "%s\n", gGameParamUserInterface.gameParams[i].c_str());

      for(S32 i = 0; i < mTeams.size(); i++)
         s_fprintf(f, "Team %s %g %g %g\n", mTeams[i].getName().getString(),
            mTeams[i].color.r, mTeams[i].color.g, mTeams[i].color.b);

      // Write out all level items (do two passes; walls first, non-walls next, so turrets & forcefields have something to grab onto)
      for(S32 j = 0; j < 2; j++)
         for(S32 i = 0; i < mItems.size(); i++)
         {
            EditorObject *p = mItems[i].get();

            // Make sure we are writing wall items on first pass, non-wall items next
            if((p->getObjectTypeMask() & BarrierType || p->getObjectTypeMask() & PolyWallType) != (j == 0))
               continue;

            p->saveItem(f);

         }
      fclose(f);
   }
   catch (SaveException &e)
   {
      if(showFailMessages)
         gEditorUserInterface.setSaveMessage("Error Saving: " + string(e.what()), false);
      return false;
   }

   if(!autosave)     // Doesn't count as a save!
   {
      mNeedToSave = false;
      mAllUndoneUndoLevel = mLastUndoIndex;     // If we undo to this point, we won't need to save
   }

   if(showSuccessMessages)
      gEditorUserInterface.setSaveMessage("Saved " + getLevelFileName(), true);

   return true;
}


// We need some local hook into the testLevelStart() below.  Ugly but apparently necessary.
void testLevelStart_local()
{
   gEditorUserInterface.testLevelStart();
}


extern void initHostGame(Address bindAddress, Vector<string> &levelList, bool testMode);
extern CmdLineSettings gCmdLineSettings;

void EditorUserInterface::testLevel()
{
   bool gameTypeError = false;
   if(!gEditorGame->getGameType())     // Not sure this could really happen anymore...  TODO: Make sure we always have a valid gametype
      gameTypeError = true;

   // With all the map loading error fixes, game should never crash!
   validateLevel();
   if(mLevelErrorMsgs.size() || mLevelWarnings.size() || gameTypeError)
   {
      gYesNoUserInterface.reset();
      gYesNoUserInterface.setTitle("LEVEL HAS PROBLEMS");

      S32 line = 1;
      for(S32 i = 0; i < mLevelErrorMsgs.size(); i++)
         gYesNoUserInterface.setMessage(line++, mLevelErrorMsgs[i].c_str());

      for(S32 i = 0; i < mLevelWarnings.size(); i++)
         gYesNoUserInterface.setMessage(line++, mLevelWarnings[i].c_str());

      if(gameTypeError)
      {
         gYesNoUserInterface.setMessage(line++, "ERROR: GameType is invalid.");
         gYesNoUserInterface.setMessage(line++, "(Fix in Level Parameters screen [F3])");
      }

      gYesNoUserInterface.setInstr("Press [Y] to start, [ESC] to cancel");
      gYesNoUserInterface.registerYesFunction(testLevelStart_local);   // testLevelStart_local() just calls testLevelStart() below
      gYesNoUserInterface.activate();

      return;
   }

   testLevelStart();
}


void EditorUserInterface::testLevelStart()
{
   string tmpFileName = mEditFileName;
   mEditFileName = "editor.tmp";

   glutSetCursor(GLUT_CURSOR_NONE);    // Turn off cursor
   bool nts = mNeedToSave;             // Save these parameters because they are normally reset when a level is saved.
   S32 auul = mAllUndoneUndoLevel;     // Since we're only saving a temp copy, we really shouldn't reset them...

   if(saveLevel(true, false))
   {
      mEditFileName = tmpFileName;     // Restore the level name

      mWasTesting = true;

      Vector<string> levelList;
      levelList.push_back("editor.tmp");
      initHostGame(Address(IPProtocol, Address::Any, 28000), levelList, true);
   }

   mNeedToSave = nts;                  // Restore saved parameters
   mAllUndoneUndoLevel = auul;
}


////////////////////////////////////////
////////////////////////////////////////

EditorMenuUserInterface gEditorMenuUserInterface;

// Constructor
EditorMenuUserInterface::EditorMenuUserInterface()
{
   setMenuID(EditorMenuUI);
   mMenuTitle = "EDITOR MENU";
}


void EditorMenuUserInterface::onActivate()
{
   Parent::onActivate();
   setupMenus();
}


extern IniSettings gIniSettings;
extern MenuItem *getWindowModeMenuItem();

//////////
// Editor menu callbacks
//////////

void reactivatePrevUICallback(U32 unused)
{
   gEditorUserInterface.reactivatePrevUI();
}

static void testLevelCallback(U32 unused)
{
   gEditorUserInterface.testLevel();
}


void returnToEditorCallback(U32 unused)
{
   gEditorUserInterface.saveLevel(true, true);  // Save level
   gEditorUserInterface.setSaveMessage("Saved " + gEditorUserInterface.getLevelFileName(), true);
   gEditorUserInterface.reactivatePrevUI();        // Return to editor
}

static void activateHelpCallback(U32 unused)
{
   gEditorInstructionsUserInterface.activate();
}

static void activateLevelParamsCallback(U32 unused)
{
   gGameParamUserInterface.activate();
}

static void activateTeamDefCallback(U32 unused)
{
   gTeamDefUserInterface.activate();
}

void quitEditorCallback(U32 unused)
{
   if(gEditorUserInterface.mNeedToSave)
   {
      gYesNoUserInterface.reset();
      gYesNoUserInterface.setInstr("Press [Y] to save, [N] to quit [ESC] to cancel");
      gYesNoUserInterface.setTitle("SAVE YOUR EDITS?");
      gYesNoUserInterface.setMessage(1, "You have not saved your edits to the level.");
      gYesNoUserInterface.setMessage(3, "Do you want to?");
      gYesNoUserInterface.registerYesFunction(saveLevelCallback);
      gYesNoUserInterface.registerNoFunction(backToMainMenuCallback);
      gYesNoUserInterface.activate();
   }
   else
      gEditorUserInterface.reactivateMenu(gMainMenuUserInterface);

   gEditorUserInterface.clearUndoHistory();        // Clear up a little memory
}

//////////

void EditorMenuUserInterface::setupMenus()
{
   menuItems.deleteAndClear();
   menuItems.push_back(new MenuItem(0, "RETURN TO EDITOR", reactivatePrevUICallback,    "", KEY_R));
   menuItems.push_back(getWindowModeMenuItem());
   menuItems.push_back(new MenuItem(0, "TEST LEVEL",       testLevelCallback,           "", KEY_T));
   menuItems.push_back(new MenuItem(0, "SAVE LEVEL",       returnToEditorCallback,      "", KEY_S));
   menuItems.push_back(new MenuItem(0, "INSTRUCTIONS",     activateHelpCallback,        "", KEY_I, keyHELP));
   menuItems.push_back(new MenuItem(0, "LEVEL PARAMETERS", activateLevelParamsCallback, "", KEY_L, KEY_F3));
   menuItems.push_back(new MenuItem(0, "MANAGE TEAMS",     activateTeamDefCallback,     "", KEY_M, KEY_F2));
   menuItems.push_back(new MenuItem(0, "QUIT",             quitEditorCallback,          "", KEY_Q, KEY_UNKNOWN));
}


void EditorMenuUserInterface::onEscape()
{
   glutSetCursor(GLUT_CURSOR_NONE);
   reactivatePrevUI();
}


////////////////////////////////////////
////////////////////////////////////////
// Stores the selection state of a particular EditorObject.  Does not store the item itself
// Primary constructor
SelectionItem::SelectionItem(EditorObject *item)
{
   mSelected = item->isSelected();

   for(S32 i = 0; i < item->getVertCount(); i++)
      mVertSelected.push_back(item->vertSelected(i));
}


void SelectionItem::restore(EditorObject *item)
{
   item->setSelected(mSelected);
   item->unselectVerts();

   for(S32 i = 0; i < item->getVertCount(); i++)
      item->aselectVert(mVertSelected[i]);
}


////////////////////////////////////////
////////////////////////////////////////
// Selection stores the selection state of group of EditorObjects
// Constructor
Selection::Selection(Vector<EditorObject *> &items)
{
   for(S32 i = 0; i < items.size(); i++)
      mSelection.push_back(SelectionItem(items[i]));
}


void Selection::restore(Vector<EditorObject *> &items)
{
   for(S32 i = 0; i < items.size(); i++)
      mSelection[i].restore(items[i]);
}


};

