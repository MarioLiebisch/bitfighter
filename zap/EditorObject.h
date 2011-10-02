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


#ifndef _EDITOROBJECT_H_
#define _EDITOROBJECT_H_

#include "gameObject.h"          // We inherit from this -- for BfObject, for now

#include "Point.h"
#include "tnlVector.h"
#include "Color.h"
#include "Geometry.h"


using namespace std;
using namespace TNL;

namespace Zap
{

// These need to be available for client and dedicated server
static S32 TEAM_NEUTRAL = -1;
static S32 TEAM_HOSTILE = -2;
static S32 NO_TEAM = -3;      // Not exposed to lua, not used in level files, only used internally

#ifndef ZAP_DEDICATED

static const S32 NONE = -1;


////////////////////////////////////////
////////////////////////////////////////

enum ShowMode
{
   ShowAllObjects,
   ShowWallsOnly,
   ShowModesCount
};


////////////////////////////////////////
////////////////////////////////////////

class EditorAttributeMenuUI;
class WallSegment;

class EditorObject : virtual public BfObject   // Interface class  -- All editor objects need to implement this
{

private:
   S32 mVertexLitUp;

   bool mIsBeingEdited;
   static S32 mNextSerialNumber;

protected:
   bool mDockItem;      // True if this item lives on the dock
   bool mSelected;      // True if item is selected
   bool mLitUp;         // True if user is hovering over the item and it's "lit up"

   S32 mSerialNumber;   // Autoincremented serial number   
   S32 mItemId;         // Item's unique id... 0 if there is none

   Color getTeamColor(S32 teamId);
   Color getDrawColor();

public:
   EditorObject();                  // Constructor
   virtual ~EditorObject();         // Virtual destructor
   virtual EditorObject *clone() const { TNLAssert(false, "Clone method not implemented!"); return NULL; }

   EditorObject *newCopy();         // Copies object, wraps clone()

   virtual void prepareForDock(Game *game, const Point &point);
   void addToEditor(Game *game);

   void assignNewSerialNumber() { mSerialNumber = mNextSerialNumber++; }
   void renderDockItemLabel(const Point &pos, const char *label, F32 yOffset = 0);    // This could be moved anywhere... it's essentially a static method

   // Offset lets us drag an item out from the dock by an amount offset from the 0th vertex.  This makes placement seem more natural.
   virtual Point getInitialPlacementOffset(F32 gridSize) { return Point(0,0); }

   // Account for the fact that the apparent selection center and actual object center are not quite aligned
   virtual Point getEditorSelectionOffset(F32 currentScale);  

   void renderAndLabelHighlightedVertices(F32 currentScale);   // Render selected and highlighted vertices, called from renderEditor
   virtual void renderItemText(const char *text, S32 offset, F32 currentScale, const Point &currentOffset) { };    // Render some text, with specified vertical offset
   virtual void renderEditor(F32 currentScale) { TNLAssert(false, "renderEditor not implemented!"); }

   // Should we show item attributes when it is selected? (only overridden by TextItem)
   virtual bool showAttribsWhenSelected() { return true; }                             

   virtual void renderAttribs(F32 currentScale);

   void unselect();

   void setSnapped(bool snapped) { /* Do nothing */ }          // Overridden in EngineeredItem

   virtual void newObjectFromDock(F32 gridSize);   // Called when item dragged from dock to editor -- overridden by several objects


   // Keep track which vertex, if any is lit up in the currently selected item
   bool isVertexLitUp(S32 vertexIndex) { return mVertexLitUp == vertexIndex; }
   void setVertexLitUp(S32 vertexIndex) { mVertexLitUp = vertexIndex; }

   // Objects can be different sizes on the dock and in the editor.  We need to draw selection boxes in both locations,
   // and these functions specify how big those boxes should be.  Override if implementing a non-standard sized item.
   // (strictly speaking, only getEditorRadius needs to be public, but it make sense to keep these together organizationally.)
   virtual S32 getDockRadius() { return 10; }                     // Size of object on dock 
   virtual F32 getEditorRadius(F32 currentScale);                 // Size of object in editor 
   virtual const char *getVertLabel(S32 index) { return ""; }     // Label for vertex, if any... only overridden by SimpleLine objects


   void saveItem(FILE *f, F32 gridSize);
   virtual string toString(F32 gridSize) const = 0; 

   Vector<Point> extendedEndPoints;                            // These are computed but not stored in barrier... not sure how to merge

   // Dock item rendering methods
   virtual void renderDock() { TNLAssert(false, "renderDock not implemented!"); }   // Need not be abstract -- some of our objects do not go on dock
   virtual void labelDockItem();
   virtual void highlightDockItem();

   void renderLinePolyVertices(F32 scale, F32 alpha = 1.0);    // Only for polylines and polygons  --> move there
  

   // TODO: Get rid of this ==> most of this code already in polygon
   void initializePolyGeom();     // Once we have our points, do some geom preprocessing ==> only for polygons

   void moveTo(const Point &pos, S32 snapVertex = 0);    // Move object to location, specifying (optional) vertex to be positioned at pos
   void offset(const Point &offset);                     // Offset object by a certain amount

   S32 repopDelay;        // For repair items, also used for engineered objects heal rate


   //////
   // Vertex management functions
   void renderPolylineCenterline(F32 alpha);    // Draw barrier centerlines; wraps renderPolyline()  ==> lineItem, barrierMaker only

   virtual void onGeomChanging();                        // Item geom is interactively changing
   //virtual void onGeomChanged() { /* To be =0 */ };      // Item changed geometry (or moved), do any internal updating that might be required

   virtual void onItemDragging() { /* Do nothing */ };   // Item is being dragged around the screen

   virtual void onAttrsChanging() { /* Do nothing */ };  // Attr is in the process of being changed (i.e. a char was typed for a textItem)
   virtual void onAttrsChanged() { /* Do nothing */ };   // Attrs changed


   /////
   // Geometry operations  -- can we provide standard implementations of these?
   //void rotateAboutPoint(const Point &origin, F32 angle) { Geometry::rotateAboutPoint(origin, angle); onGeomChanged(); }
   //void flipHorizontal(F32 minX, F32 maxX) { Geometry::flipHorizontal(minX, maxX); onGeomChanged(); };
   //void flipVertical(F32 minY, F32 maxY) { Geometry::flipVertical(minY, maxY); onGeomChanged(); };
   //virtual void scale(const Point &center, F32 scale) { Geometry::scale(center, scale); onGeomChanged(); }

   /////
   S32 getItemId() { return mItemId; }
   void setItemId(S32 itemId) { mItemId = itemId; }
   
   S32 getSerialNumber() { return mSerialNumber; }

   bool isSelected() { return mSelected; }
   void setSelected(bool selected) { mSelected = selected; }

   void setDockItem(bool isDockItem) { mDockItem = isDockItem; }

   bool isLitUp() { return mLitUp; }
   void setLitUp(bool litUp) { mLitUp = litUp; if(!litUp) setVertexLitUp(NONE); }

   bool isBeingEdited() { return mIsBeingEdited; }
   void setIsBeingEdited(bool isBeingEdited) { mIsBeingEdited = isBeingEdited; }


   virtual void initializeEditor();

   Point forceFieldEnd;      // Point where forcefield terminates.  Only used for forcefields.

   S32 mScore;
   S32 getScore() { return mScore; }     // goal zones only, return zone's score

   virtual bool canBeHostile();
   virtual bool canBeNeutral();
   virtual bool hasTeam();

   virtual const char *getEditorHelpString() { TNLAssert(false, "getEditorHelpString method not implemented!"); return NULL; }     
   virtual const char *getPrettyNamePlural() { TNLAssert(false, "getPrettyNamePlural method not implemented!"); return NULL; }
   virtual const char *getOnDockName()       { TNLAssert(false, "getOnDockName method not implemented!"); return NULL; }
   virtual const char *getOnScreenName()     { TNLAssert(false, "getOnScreenName method not implemented!"); return NULL; }   

   virtual const char *getInstructionMsg() { return ""; }      // Message printed below item when it is selected

   virtual EditorAttributeMenuUI *getAttributeMenu() { return NULL; }    // Override if child class has an attribute menu
   virtual void doneEditing(EditorAttributeMenuUI *attributeMenu) {}


   //////////////

   //TODO: Get rid of this altogether
   void renderInEditor(F32 currentScale, const Point &currentOffset, S32 snapIndex, bool isScriptItem, bool showingReferenceShip, ShowMode showMode);

};


////////////////////////////////////////
////////////////////////////////////////

// Class with editor methods related to point things

class EditorPointObject : public EditorObject, public PointGeometry
{
   typedef EditorObject Parent;

public:
   //EditorPointObject();       // Constructor

   virtual void renderItemText(const char *text, S32 offset, F32 currentScale, const Point &currentOffset);
   void prepareForDock(Game *game, const Point &point);
};


////////////////////////////////////////
////////////////////////////////////////

class EditorItem : public EditorPointObject
{
   //typedef MoveItem Parent;   
   //typedef EditorObject EditorParent;
   typedef EditorPointObject Parent;

public:
   //EditorItem(Point p = Point(0,0), bool collideable = false, float radius = 1, float mass = 1);   // Constructor

   virtual void renderEditor(F32 currentScale) = 0;
   virtual F32 getEditorRadius(F32 currentScale) = 0;
   virtual string toString(F32 gridSize) const = 0;

};

#else
// ZAP_DEDICATED

class EditorObject : virtual public BfObject   // Interface class  -- All editor objects need to implement this
{
   // dedicated server won't need much of this
public:
   S32 mItemId;  // not used, but as of now, needed for Item::Item constructor

   virtual EditorObject *clone()  const { TNLAssert(false, "Clone method not implemented!"); return NULL; };
   EditorObject *newCopy() { return clone(); }        // Copies object  
};

class EditorPointObject : public EditorObject, public PointGeometry {};
class EditorItem : public EditorPointObject {};


#endif
};
#endif
