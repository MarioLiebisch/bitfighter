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

#ifndef _UIEDITOR_H_
#define _UIEDITOR_H_

#include "UI.h"                  // Parent
#include "UIMenus.h"             // Parent

#include "EditorPlugin.h"        // For plugin support
#include "teamInfo.h"            // For TeamManager def

#include "VertexStylesEnum.h"
#include "BfObject.h"            // For BfObject definition
#include "Timer.h"
#include "Point.h"
#include "Color.h"
#include "EditorAttributeMenuItemBuilder.h"

#include "tnlNetStringTable.h"

#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>

using namespace std;

namespace Zap
{

class DatabaseObject;
class EditorAttributeMenuUI;
class EditorTeam;
class GameType;
class LuaLevelGenerator;
class PluginMenuUI;
class SimpleTextEntryMenuUI;
struct FolderManager;

class EditorUserInterface : public UserInterface
{
   typedef UserInterface Parent;

public:
   struct PluginInfo
   {
      PluginInfo(string prettyName, string fileName, string description, string requestedBinding)
         : prettyName(prettyName), fileName(fileName), description(description), requestedBinding(requestedBinding) { }
      string prettyName;
      string fileName;
      string binding;
      string description;
      string requestedBinding;
   };

   // Some items have special attributes.  These are the ones we can edit in the editor.
   enum SpecialAttribute {  
      Text,
      RepopDelay,
      GoFastSpeed,
      GoFastSnap,
      NoAttribute    // Must be last
   };

private:
   string mInfoMsg;
   string mSaveMsg;
   Color mSaveMsgColor;

   string mWarnMsg1;
   string mWarnMsg2;
   Color mWarnMsgColor;

   S32 mCurrentTeam;

   enum SnapContext {
      FULL_SNAPPING,
      NO_GRID_SNAPPING,
      NO_SNAPPING
   };

   enum RenderModes {
      RENDER_UNSELECTED_NONWALLS,
      RENDER_SELECTED_NONWALLS,
      RENDER_UNSELECTED_WALLS,
      RENDER_SELECTED_WALLS
   };

   enum DockMode {
      DOCKMODE_ITEMS,
      DOCKMODE_PLUGINS
   };

   enum SimpleTextEntryType {
      SimpleTextEntryID,              // Entering an objectID
      SimpleTextEntryRotateOrigin,    // Entering an angle for rotating about the origin
      SimpleTextEntryRotateCentroid,  // Entering an angle for spinning
      SimpleTextEntryScale,           // Entering a scale
   };

   DockMode mDockMode;

   SnapContext mSnapContext;

   Timer mSaveMsgTimer;
   Timer mWarnMsgTimer;

   Vector<boost::shared_ptr<GridDatabase> > mUndoItems;  // Undo/redo history 
   Point mMoveOrigin;                           // Point representing where items were moved "from" for figuring out how far they moved
   Point mSnapDelta;                            // For tracking how far from the snap point our cursor is
   Vector<Point> mMoveOrigins;

   boost::shared_ptr<GridDatabase> mEditorDatabase;

   void setDatabase(boost::shared_ptr<GridDatabase> database);

   Vector<boost::shared_ptr<BfObject> > mDockItems;    // Items sitting in the dock

   U32 mFirstUndoIndex;
   U32 mLastUndoIndex;
   U32 mLastRedoIndex;

   bool mDragCopying;
   bool mJustInsertedVertex;
   bool mRedoingAnUndo;

   Vector<string> mRobotLines;         // A list of robot lines read from a level file when loading from the editor

   void clearSnapEnvironment();

   static const U32 UNDO_STATES = 128;
   void deleteUndoState();             // Removes most recent undo state from stack
   bool undoAvailable();               // Is an undo state available?
   void undo(bool addToRedoStack);     // Restore mItems to latest undo state
   void redo();                        // Redo latest undo

   Vector<boost::shared_ptr<BfObject> > mClipboard;    // Items on clipboard

   bool mLastUndoStateWasBarrierWidthChange;

   string mEditFileName;            // Manipulate with get/setLevelFileName

   TeamManager mTeamManager;

   F32 mCurrentScale;
   Point mCurrentOffset;            // Coords of UR corner of screen

   Point mMousePos;                 // Where the mouse is at the moment
   Point mMouseDownPos;             // Where the mouse was pressed for a drag operation

   bool mAutoScrollWithMouse;       // Make use of scrolling using middle mouse position
   bool mAutoScrollWithMouseReady;
   Point mScrollWithMouseLocation;
   bool showMinorGridLines();

   // Helper drawing methods
   void renderTurretAndSpyBugRanges(GridDatabase *editorDb);   // Draw translucent turret & spybug ranges
   void renderObjectsUnderConstruction();                      // Render partially constructed walls and other items that aren't yet in a db
   void renderDock();
   void renderInfoPanel();
   void renderPanelInfoLine(S32 line, const char *format, ...);

   void renderItemInfoPanel();

   void renderReferenceShip();
   void renderDragSelectBox();      // Render box when selecting a group of items
   void renderDockItems();          // Render all items on the dock
   void renderDockPlugins();
   void renderSaveMessage();
   void renderWarnings();

   EditorAttributeMenuItemBuilder mEditorAttributeMenuItemBuilder;

   bool mCreatingPoly;
   bool mCreatingPolyline;
   bool mDragSelecting;
   bool mAddingVertex;
   bool mPreviewMode;
   bool mNormalizedScreenshotMode;

   boost::shared_ptr<EditorPlugin> mPluginRunner;

   Vector<string> mLevelErrorMsgs, mLevelWarnings;
   Vector<PluginInfo> mPluginInfos;

   bool mUp, mDown, mLeft, mRight, mIn, mOut;

   void selectAll(GridDatabase *database);          // Mark all objects and vertices in specified db as selected
   void clearSelection(GridDatabase *database);     // Mark all objects and vertices in specified db as unselected

   void centerView();            // Center display on all objects
   void splitBarrier();          // Split wall on selected vertex/vertices
   void doSplit(BfObject *object, S32 vertex);
   void joinBarrier();           // Join barrier bits together into one (if ends are coincident)

   BfObject *doMergeLines   (BfObject *firstItem, S32 firstItemIndex);   
   BfObject *doMergePolygons(BfObject *firstItem, S32 firstItemIndex);

   bool anyItemsSelected(GridDatabase *database);  // Are any items selected?
   bool anythingSelected();                        // Are any items/vertices selected?
public:
   S32 getItemSelectedCount();                     // How many are objects are selected?

private:
   // Sets mHitItem and mEdgeHit -- findHitItemAndEdge calls one or more of the associated helper functions below
   void findHitItemAndEdge();                         
   bool checkForVertexHit(BfObject *object);
   bool checkForEdgeHit(const Point &point, BfObject *object);        
   bool checkForWallHit(const Point &point, DatabaseObject *wallSegment);
   bool checkForPolygonHit(const Point &point, BfObject *object);    

   void findHitItemOnDock();     // Sets mDockItemHit
   S32 findHitPlugin();

   void findSnapVertex();
   S32 mSnapVertexIndex;

   S32 mEdgeHit;
   S32 mHitVertex;


   SafePtr<BfObject> mNewItem;
   SafePtr<BfObject> mSnapObject;
   SafePtr<BfObject> mHitItem;

   SafePtr<BfObject> mDraggingDockItem;
   SafePtr<BfObject> mDockItemHit;

   U32 mDockWidth;
   bool mouseOnDock();                // Return whether mouse is currently over the dock
   bool mNeedToSave;                  // Have we modified the level such that we need to save?

   void insertNewItem(U8 itemTypeNumber);    // Insert a new object into the specified database

   bool mWasTesting;
   GameType *mEditorGameType;    // Used to store our GameType while we're testing

   void onFinishedDragging();    // Called when we're done dragging an object
   void onSelectionChanged();    // Called when current selection has changed

   void onMouseClicked_left();
   void onMouseClicked_right();

   Point convertCanvasToLevelCoord(Point p);
   Point convertLevelToCanvasCoord(Point p, bool convert = true);

   void resnapAllEngineeredItems(GridDatabase *database);

   boost::scoped_ptr<SimpleTextEntryMenuUI> mSimpleTextEntryMenu;
   boost::scoped_ptr<PluginMenuUI> mPluginMenu;      
   map<string, Vector<string> > mPluginMenuValues;

   void showCouldNotFindScriptMessage(const string &scriptName);

   GridDatabase mLevelGenDatabase;     // Database for inserting objects when running a levelgen script in the editor

   void translateSelectedItems(const Point &offset);
   void snapSelectedEngineeredItems(const Point &cumulativeOffset);

   void render();
   void renderObjects(GridDatabase *database, RenderModes renderMode, bool isLevelgenOverlay);
   void renderWallsAndPolywalls(GridDatabase *database, const Point &offset, bool selected, bool isLevelGenDatabase);

   void autoSave();                    // Hope for the best, prepare for the worst
   bool doSaveLevel(const string &saveName, bool showFailMessages);

protected:
   void onActivate();
   void onReactivate();

   void renderMasterStatus();

   bool usesEditorScreenMode();

public:
   explicit EditorUserInterface(ClientGame *game);    // Constructor
   virtual ~EditorUserInterface();           // Destructor

   GridDatabase *getDatabase();      // Need external access to this in one static function

   void setLevelFileName(string name);
   void setLevelGenScriptName(string name);

   string getLevelFileName();
   void cleanUp();
   void loadLevel();
   U32 mAllUndoneUndoLevel;   // What undo level reflects everything back just the

   void saveUndoState(bool forceSelection = false);     // Save the current state of the editor objects for later undoing
   void removeUndoState();    // Remove and discard the most recently saved undo state 

   Vector<string> mGameTypeArgs;

   static const string UnnamedFile;

   bool saveLevel(bool showFailMessages, bool showSuccessMessages);   // Public because called from callbacks
   string getLevelText();

   F32 getCurrentScale();
   Point getCurrentOffset();

   void clearUndoHistory();        // Wipe undo/redo history

   Vector<TeamInfo> mOldTeams;     // Team list from before we run team editor, so we can see what changed

   void rebuildEverything(GridDatabase *database);   // Does lots of things in undo, redo, and add items from script

   void onQuitted();       // Releases some memory when quitting the editor

   S32 getTeamCount();
   EditorTeam *getTeam(S32 teamId);

   void addTeam(EditorTeam *team);
   void addTeam(EditorTeam *team, S32 index);
   void removeTeam(S32 teamId);
   void clearTeams();

   bool getNeedToSave() const;
   void setNeedToSave(bool needToSave);

   void clearRobotLines();
   void addRobotLine(const string &robotLine);

   bool mDraggingObjects;     // Should be private

   // Handle input
   bool onKeyDown(InputCode inputCode);                         // Handle all keyboard inputs, mouse clicks, and button presses
   void onTextInput(char ascii);                                // Handle all text input characters
   bool checkPluginKeyBindings(string inputString);             // Handle keys bound to plugins
   void specialAttributeKeyHandler(InputCode inputCode, char ascii);
   void startAttributeEditor();
   void doneEditingAttributes(EditorAttributeMenuUI *editor, BfObject *object);   // Gets run when user exits attribute editor

   void startSimpleTextEntryMenu(SimpleTextEntryType entryType);
   void doneWithSimpleTextEntryMenu(SimpleTextEntryMenuUI *menu, S32 entryType);

   void zoom(F32 zoomAmount);

   void onKeyUp(InputCode inputCode);
   void onMouseMoved();
   void onMouseDragged();
   void onMouseDragged_StartDragging(const bool needToSaveUndoState);
   void onMouseDragged_CtrlPlusDrag(const Vector<DatabaseObject *> *objList);
   void startDraggingDockItem();
   BfObject *copyDockItem(BfObject *source);
   bool mouseIgnore;

   void populateDock();                         // Load up dock with game-specific items to drag and drop
   void addDockObject(BfObject *object, F32 xPos, F32 yPos);

   string mScriptLine;                           // Script and args, if any

   void idle(U32 timeDelta);
   void deleteSelection(bool objectsOnly);       // Delete selected items (true = items only, false = items & vertices)
   void copySelection();                         // Copy selection to clipboard
   void pasteSelection();                        // Paste selection from clipboard
   void setCurrentTeam(S32 currentTeam);         // Set current team for selected items, also sets team for all dock items

   void flipSelectionHorizontal();               // Flip selection along horizontal axis
   void flipSelectionVertical();                 // Flip selection along vertical axis
   void flipSelection(F32 center, bool isHoriz); // Do the actual flipping for the above

   void scaleSelection(F32 scale);               // Scale selection by scale
   void rotateSelection(F32 angle, bool useOrigin); // Rotate selecton by angle
   void setSelectionId(S32 id);

   void validateLevel();               // Check level for things that will make the game crash!
   void validateTeams();               // Check that each item has a valid team (and fix any errors found)
   void validateTeams(const Vector<DatabaseObject *> *dbObjects);

   void teamsHaveChanged();            // Another team validation routine, used when all items have valid teams, but the teams themselves change
   void makeSureThereIsAtLeastOneTeam();

   void changeBarrierWidth(S32 amt);   // Increase selected wall thickness by amt

   void testLevel();
   void testLevelStart();
   void setSaveMessage(string msg, bool savedOK);
   void setWarnMessage(string msg1, string msg2);

   void onDisplayModeChange();      // Called when we shift between windowed and fullscreen mode, after change is made

   // Snapping related functions:
   Point snapPoint(GridDatabase *database, Point const &p, bool snapWhileOnDock = false);
   Point snapPointToLevelGrid(Point const &p);

   bool getSnapToWallCorners();     // Returns true if wall corners are active snap targets

   void onBeforeRunScriptFromConsole();
   void onAfterRunScriptFromConsole();

   S32 checkCornersForSnap(const Point &clickPoint,  const Vector<DatabaseObject *> *edges, F32 &minDist, Point &snapPoint);

   void deleteItem(S32 itemIndex, bool batchMode = false);

   // Helpers for doing batch deletes
   void doneDeleteingWalls(); 
   void doneDeleting();

   // Run a script, and put resulting objects in database
   void runScript(GridDatabase *database, const FolderManager *folderManager, const string &scriptName, const Vector<string> &args);
   void runPlugin(const FolderManager *folderManager, const string &scriptName, const Vector<string> &args);  

   string getPluginSignature();                 // Try to create some sort of uniqeish signature for the plugin
   void onPluginExecuted(const Vector<string> &args);
   void runLevelGenScript();              // Run associated levelgen script
   void copyScriptItemsToEditor();        // Insert these items into the editor as first class items that can be manipulated or saved
   void clearLevelGenItems();             // Clear any previously created levelgen items

   void addToEditor(BfObject *obj);

   void createNormalizedScreenshot(ClientGame* game);

   void findPlugins();
   U32 findPluginDockWidth();
};


////////////////////////////////////////
////////////////////////////////////////

class EditorMenuUserInterface : public MenuUserInterface
{
   typedef MenuUserInterface Parent;

private:
   void setupMenus();
   void processSelection(U32 index);
   void processShiftSelection(U32 index);
   void onEscape();

protected:
   void onActivate();

public:
   explicit EditorMenuUserInterface(ClientGame *game);    // Constructor
   virtual ~EditorMenuUserInterface();
};


};

#endif
