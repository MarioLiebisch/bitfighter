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


#ifndef _SPAWN_H_
#define _SPAWN_H_

#include "EditorObject.h"     // For PointObject def
#include "Timer.h"


namespace Zap
{

class EditorAttributeMenuUI;     // Needed in case class def hasn't been included in dedicated build

// Parent class for spawns that generate items
class AbstractSpawn : public PointObject
{
   typedef EditorObject Parent;

private:
   static EditorAttributeMenuUI *mAttributeMenuUI;

protected:
   S32 mSpawnTime;
   Timer mTimer;

   void setRespawnTime(S32 time);

public:
   AbstractSpawn(const Point &pos = Point(), S32 time = 0); // Constructor
   AbstractSpawn(const AbstractSpawn &copy);                // Copy constructor
   virtual ~AbstractSpawn();                                // Destructor
   
   virtual bool processArguments(S32 argc, const char **argv, Game *game);

   ///// Editor methods
   virtual const char *getEditorHelpString() = 0;
   virtual const char *getPrettyNamePlural() = 0;
   virtual const char *getOnDockName() = 0;
   virtual const char *getOnScreenName() = 0;


#ifndef ZAP_DEDICATED
   // These four methods are all that's needed to add an editable attribute to a class...
   EditorAttributeMenuUI *getAttributeMenu();
   void startEditingAttrs(EditorAttributeMenuUI *attributeMenu);    // Called when we start editing to get menus populated
   void doneEditingAttrs(EditorAttributeMenuUI *attributeMenu);     // Called when we're done to retrieve values set by the menu

   virtual string getAttributeString();
#endif

   virtual const char *getClassName() const = 0;

   virtual S32 getDefaultRespawnTime() = 0;

   virtual string toString(F32 gridSize) const;

   F32 getEditorRadius(F32 currentScale);

   bool updateTimer(U32 deltaT);
   void resetTimer();
   U32 getPeriod();     // temp debugging

   virtual void renderEditorPreview(F32 currentScale);
   virtual void renderEditor(F32 currentScale) = 0;
   virtual void renderDock() = 0;
};


class Spawn : public AbstractSpawn
{
   typedef AbstractSpawn Parent;

public:
   Spawn(const Point &pos = Point());  // C++ constructor (no lua constructor)
   virtual ~Spawn();
   Spawn *clone() const;

   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();

   const char *getClassName() const;

   bool processArguments(S32 argc, const char **argv, Game *game);
   string toString(F32 gridSize) const;

   S32 getDefaultRespawnTime();    // Somewhat meaningless in this context

   void renderEditor(F32 currentScale);
   void renderDock();
};


////////////////////////////////////////
////////////////////////////////////////

class ItemSpawn : public AbstractSpawn
{
   typedef AbstractSpawn Parent;

public:
   ItemSpawn(const Point &pos, S32 time);
   virtual void spawn(Game *game, const Point &pos) = 0;
};

////////////////////////////////////////
////////////////////////////////////////

class AsteroidSpawn : public ItemSpawn    
{
   typedef ItemSpawn Parent;

public:
   static const S32 DEFAULT_RESPAWN_TIME = 30;    // in seconds

   AsteroidSpawn(const Point &pos = Point(), S32 time = DEFAULT_RESPAWN_TIME);  // C++ constructor (no lua constructor)
   virtual ~AsteroidSpawn();
   AsteroidSpawn *clone() const;

   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();

   const char *getClassName() const;

   S32 getDefaultRespawnTime();

   void spawn(Game *game, const Point &pos);
   void renderEditor(F32 currentScale);
   void renderDock();
};


////////////////////////////////////////
////////////////////////////////////////

class CircleSpawn : public ItemSpawn    
{
   typedef ItemSpawn Parent;

public:
   static const S32 DEFAULT_RESPAWN_TIME = 20;    // in seconds

   CircleSpawn(const Point &pos = Point(), S32 time = DEFAULT_RESPAWN_TIME);  // C++ constructor (no lua constructor)
   CircleSpawn *clone() const;

   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();

   const char *getClassName() const;

   S32 getDefaultRespawnTime();

   void spawn(Game *game, const Point &pos);
   void renderEditor(F32 currentScale);
   void renderDock();
};

////////////////////////////////////////
////////////////////////////////////////

class FlagSpawn : public AbstractSpawn
{
   typedef AbstractSpawn Parent;

public:
   static const S32 DEFAULT_RESPAWN_TIME = 30;    // in seconds

   FlagSpawn(const Point &pos = Point(), S32 time = DEFAULT_RESPAWN_TIME);  // C++ constructor (no lua constructor)
   virtual ~FlagSpawn();
   FlagSpawn *clone() const;

   bool updateTimer(S32 deltaT);
   void resetTimer();

   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();

   const char *getClassName() const;

   S32 getDefaultRespawnTime();

   void spawn(Game *game, const Point &pos);
   void renderEditor(F32 currentScale);
   void renderDock();

   bool processArguments(S32 argc, const char **argv, Game *game);
   string toString(F32 gridSize) const;
};


};    // namespace


#endif
