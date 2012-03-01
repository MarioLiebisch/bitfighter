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

#ifndef COREGAME_H_
#define COREGAME_H_

#include "gameType.h"
#include "item.h"


namespace Zap {

// Forward Declarations
class CoreItem;
class ClientInfo;

class CoreGameType : public GameType
{
   typedef GameType Parent;

private:
   Vector<SafePtr<CoreItem> > mCores;

public:
   static const S32 DestroyedCoreScore = 1;

   CoreGameType();
   virtual ~CoreGameType();

   bool processArguments(S32 argc, const char **argv, Game *game);
   string toString() const;

   S32 getTeamCoreCount(S32 teamIndex);
   bool isTeamCoreBeingAttacked(S32 teamIndex);

   // Runs on client
   void renderInterfaceOverlay(bool scoreboardVisible);

   void addCore(CoreItem *core, S32 team);

   // What does a particular scoring event score?
   S32 getEventScore(ScoringGroup scoreGroup, ScoringEvent scoreEvent, S32 data);
   void score(ClientInfo *destroyer, S32 coreOwningTeam, S32 score);

#ifndef ZAP_DEDICATED
   Vector<string> getGameParameterMenuKeys();
#endif

   GameTypeId getGameTypeId() const;
   const char *getShortName() const;
   const char *getInstructionString() const;
   bool canBeTeamGame() const;
   bool canBeIndividualGame() const;


   TNL_DECLARE_CLASS(CoreGameType);
};


////////////////////////////////////////
////////////////////////////////////////

class EditorAttributeMenuUI;

class CoreItem : public Item
{

typedef Item Parent;

public:
   static const S32 CORE_PANELS = 10;     // Note that changing this will require update of all clients, and a new CS_PROTOCOL_VERSION

private:
   static const U32 CoreStartWidth = 200;
   static const U32 CoreMinWidth = 20;
   static const U32 CoreDefaultStartingHealth = 10;  // 1 health is the equivalent damage a normal ship can take
   static const U32 CoreHeartbeatStartInterval = 2000;  // Milliseconds
   static const U32 CoreHeartbeatMinInterval = 500;
   static const U32 CoreAttackedWarningDuration = 600;
   static const U32 ExplosionInterval = 600;
   static const U32 ExplosionCount = 3;

   static U32 mCurrentExplosionNumber;

   static const F32 DamageReductionRatio;

   bool mHasExploded;
   bool mBeingAttacked;
   F32 mStartingHealth;          // Health stored in the level file, will be divided amongst panels
   F32 mStartingPanelHealth;     // Health divided up amongst panels

   F32 mPanelHealth[CORE_PANELS];
   Timer mHeartbeatTimer;        // Client-side timer
   Timer mExplosionTimer;        // Client-side timer
   Timer mAttackedWarningTimer;  // Server-side timer

#ifndef ZAP_DEDICATED
   static EditorAttributeMenuUI *mAttributeMenuUI;      // Menu for attribute editing; since it's static, don't bother with smart pointer
#endif
protected:
      enum MaskBits {
      PanelDamagedMask = Parent::FirstFreeMask << 0,  // each bit mask have own panel updates (PanelDamagedMask << n)
      PanelDamagedAllMask = ((1 << CORE_PANELS) - 1) * PanelDamagedMask,  // all bits of PanelDamagedMask
      FirstFreeMask   = Parent::FirstFreeMask << CORE_PANELS
   };

public:

   CoreItem();     // Constructor
   CoreItem *clone() const;

   void renderItem(const Point &pos);
   bool getCollisionPoly(Vector<Point> &polyPoints) const;
   bool getCollisionCircle(U32 state, Point &center, F32 &radius) const;
   bool collide(GameObject *otherObject);

   F32 calcCoreWidth() const;
   bool isBeingAttacked();

   void setStartingHealth(F32 health);
   F32 getTotalHealth();                  // Returns total health of all panels
   F32 getHealth();                       // Returns overall health of item as a ratio between 0 and 1

   void onAddedToGame(Game *theGame);

   void damageObject(DamageInfo *theInfo);
   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);
   void onItemExploded(Point pos);
   void doExplosion(const Point &pos);
   void doPanelDebris(S32 panelIndex);

   void idle(GameObject::IdleCallPath path);

   bool processArguments(S32 argc, const char **argv, Game *game);
   string toString(F32 gridSize) const;

   TNL_DECLARE_CLASS(CoreItem);

#ifndef ZAP_DEDICATED
   // These four methods are all that's needed to add an editable attribute to a class...
   EditorAttributeMenuUI *getAttributeMenu();
   void startEditingAttrs(EditorAttributeMenuUI *attributeMenu);    // Called when we start editing to get menus populated
   void doneEditingAttrs(EditorAttributeMenuUI *attributeMenu);     // Called when we're done to retrieve values set by the menu
   string getAttributeString();
#endif

   ///// Editor methods
   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();

   F32 getEditorRadius(F32 currentScale);
   void renderEditor(F32 currentScale);      // Render item in the editor
   void renderDock();

   ///// Lua interface
public:
   CoreItem(lua_State *L);    // Constructor

   static const char className[];

   static Lunar<CoreItem>::RegType methods[];

   S32 getClassID(lua_State *L);

   S32 getHealth(lua_State *L);   
   void push(lua_State *L);
};



} /* namespace Zap */
#endif /* COREGAME_H_ */
