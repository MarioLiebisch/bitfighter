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

#ifndef ZAP_DEDICATED
#  include "SDL/SDL_opengl.h"
#  include "UIMenuItems.h"
#  include "UIEditorMenus.h"
#  include "ClientGame.h"
#endif

#include "Spawn.h"
#include "game.h"

#include "stringUtils.h"         // For itos()
#include "gameObjectRender.h"    // For renderSquareItem(), renderFlag(), drawCircle()
#include "moveObject.h"          // For Circle, Asteroid class defs


namespace Zap
{

// Statics:
#ifndef ZAP_DEDICATED
   EditorAttributeMenuUI *AbstractSpawn::mAttributeMenuUI = NULL;
#endif

AbstractSpawn::AbstractSpawn(const Point &pos, S32 time)
{
   setVert(pos, 0);
   setRespawnTime(time);
};


AbstractSpawn::AbstractSpawn(const AbstractSpawn &copy) : EditorPointObject(copy), mTimer(copy.mTimer)
{
   mSpawnTime = copy.mSpawnTime;
}


AbstractSpawn::~AbstractSpawn()
{
   // Do nothing
}


void AbstractSpawn::setRespawnTime(S32 time)       // in seconds
{
   mSpawnTime = time;
   mTimer.setPeriod(time * 1000);
   mTimer.reset();
}


F32 AbstractSpawn::getEditorRadius(F32 currentScale)
{
   return 12;     // Constant size, regardless of zoom
}


bool AbstractSpawn::processArguments(S32 argc, const char **argv, Game *game)
{
   if(argc < 2)
      return false;

   Point pos;
   pos.read(argv);
   pos *= game->getGridSize();

   setVert(pos, 0);

   S32 time = (argc > 2) ? atoi(argv[2]) : getDefaultRespawnTime();

   setRespawnTime(time);

   return true;
}


string AbstractSpawn::toString(F32 gridSize) const
{
   // <<spawn class name>> <x> <y> <spawn timer>
   return string(getClassName()) + " " + geomToString(gridSize) + " " + itos(mSpawnTime);
}


#ifndef ZAP_DEDICATED

EditorAttributeMenuUI *AbstractSpawn::getAttributeMenu()
{
   if(getDefaultRespawnTime() == -1)  // No editing RespawnTimer for Ship Spawn
      return NULL;

   if(!mAttributeMenuUI)
   {
      ClientGame *clientGame = (ClientGame *)getGame();

      mAttributeMenuUI = new EditorAttributeMenuUI(clientGame);

      CounterMenuItem *menuItem = new CounterMenuItem(clientGame, "Spawn Timer:", 999, 1, 0, 1000, "secs", "Never spawns", 
                                                      "Time it takes for each item to be spawned");
      mAttributeMenuUI->menuItems.push_back(boost::shared_ptr<MenuItem>(menuItem));

      // Add our standard save and exit option to the menu
      mAttributeMenuUI->addSaveAndQuitMenuItem();
   }

   return mAttributeMenuUI;
}


void AbstractSpawn::startEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   attributeMenu->menuItems[0]->setIntValue(mSpawnTime);
}


void AbstractSpawn::doneEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   mSpawnTime = attributeMenu->menuItems[0]->getIntValue();
}


// Render some attributes when item is selected but not being edited
void AbstractSpawn::renderAttributeString(F32 currentScale)
{

   if(getDefaultRespawnTime() == -1)
      return;

   string txt;

   if(mSpawnTime == 0)
      txt = "Disabled";
   else
      txt = "Spawn time: " + itos(mSpawnTime) + " sec" + ( mSpawnTime != 1 ? "s" : "");

   renderItemText(txt.c_str(), 1, currentScale);
}

#endif

////////////////////////////////////////
////////////////////////////////////////

// Constructor
Spawn::Spawn(const Point &pos) : AbstractSpawn(pos)
{
   mObjectTypeNumber = ShipSpawnTypeNumber;
}


// Destructor
Spawn::~Spawn()
{
   // Do nothing
}


Spawn *Spawn::clone() const
{
   return new Spawn(*this);
}


bool Spawn::processArguments(S32 argc, const char **argv, Game *game)
{
   if(argc < 3)
      return false;

   S32 teamIndex = atoi(argv[0]);
   setTeam(teamIndex);

   Point pos;
   pos.read(argv + 1);
   pos *= game->getGridSize();

   setVert(pos, 0);

   return true;
}


string Spawn::toString(F32 gridSize) const
{
   // Spawn <team> <x> <y> 
   return string(getClassName()) + " " + itos(mTeam) + " " + geomToString(gridSize);
}


void Spawn::renderEditor(F32 currentScale)
{
#ifndef ZAP_DEDICATED
   Point pos = getVert(0);

   glPushMatrix();
      glTranslatef(pos.x, pos.y, 0);
      glScalef(1/currentScale, 1/currentScale, 1);    // Make item draw at constant size, regardless of zoom
      renderSquareItem(Point(0,0), getGame()->getTeamColor(mTeam), 1, &Colors::white, 'S');
   glPopMatrix();
#endif
}


void Spawn::renderDock()
{
   renderEditor(1);
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
ItemSpawn::ItemSpawn(const Point &pos, S32 time) : Parent(pos, time)
{
   // Do nothing
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
AsteroidSpawn::AsteroidSpawn(const Point &pos, S32 time) : Parent(pos, time)
{
   mObjectTypeNumber = AsteroidSpawnTypeNumber;
}


AsteroidSpawn::~AsteroidSpawn()
{
   // Do nothing
}


AsteroidSpawn *AsteroidSpawn::clone() const
{
   return new AsteroidSpawn(*this);
}


void AsteroidSpawn::spawn(Game *game, const Point &pos)
{
   Asteroid *asteroid = dynamic_cast<Asteroid *>(TNL::Object::create("Asteroid"));   // Create a new asteroid

   F32 ang = TNL::Random::readF() * Float2Pi;

   asteroid->setPosAng(pos, ang);

   asteroid->addToGame(game, game->getGameObjDatabase());              // And add it to the list of game objects
}


static void renderAsteroidSpawn(const Point &pos)
{
#ifndef ZAP_DEDICATED
   F32 scale = 0.8f;
   static const Point p(0,0);

   glPushMatrix();
      glTranslatef(pos.x, pos.y, 0);
      glScalef(scale, scale, 1);
      renderAsteroid(p, 2, .1f);

      glColor(Colors::white);
      drawCircle(p, 13);
   glPopMatrix();  
#endif
}


void AsteroidSpawn::renderEditor(F32 currentScale)
{
#ifndef ZAP_DEDICATED
   Point pos = getVert(0);

   glPushMatrix();
      glTranslate(pos);
      glScale(1/currentScale);    // Make item draw at constant size, regardless of zoom
      renderAsteroidSpawn(Point(0,0));
   glPopMatrix();   
#endif
}


void AsteroidSpawn::renderDock()
{
   renderAsteroidSpawn(getVert(0));
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
CircleSpawn::CircleSpawn(const Point &pos, S32 time) : Parent(pos, time)
{
   mObjectTypeNumber = CircleSpawnTypeNumber;
}


CircleSpawn *CircleSpawn::clone() const
{
   return new CircleSpawn(*this);
}


void CircleSpawn::spawn(Game *game, const Point &pos)
{
   for(S32 i = 0; i < 10; i++)
   {
      Circle *circle = dynamic_cast<Circle *>(TNL::Object::create("Circle"));   // Create a new Circle
      F32 ang = TNL::Random::readF() * Float2Pi;

      circle->setPosAng(pos, ang);

      circle->addToGame(game, game->getGameObjDatabase());              // And add it to the list of game objects
   }
}


static void renderCircleSpawn(const Point &pos)
{
#ifndef ZAP_DEDICATED
   F32 scale = 0.8f;
   static const Point p(0,0);

   glPushMatrix();
      glTranslatef(pos.x, pos.y, 0);
      glScalef(scale, scale, 1);
      drawCircle(p, 8);

      glColor(Colors::white);
      drawCircle(p, 13);
   glPopMatrix();  
#endif
}


void CircleSpawn::renderEditor(F32 currentScale)
{
#ifndef ZAP_DEDICATED
   Point pos = getVert(0);

   glPushMatrix();
      glTranslate(pos);
      glScale(1/currentScale);    // Make item draw at constant size, regardless of zoom
      renderCircleSpawn(Point(0,0));
   glPopMatrix();   
#endif
}


void CircleSpawn::renderDock()
{
   renderCircleSpawn(getVert(0));
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
FlagSpawn::FlagSpawn(const Point &pos, S32 time) : AbstractSpawn(pos, time)
{
   mObjectTypeNumber = FlagSpawnTypeNumber;
}


FlagSpawn::~FlagSpawn()
{
   // Do nothing
}


FlagSpawn *FlagSpawn::clone() const
{
   return new FlagSpawn(*this);
}


void FlagSpawn::renderEditor(F32 currentScale)
{
#ifndef ZAP_DEDICATED
   Point pos = getVert(0);

   glPushMatrix();
      glTranslatef(pos.x + 1, pos.y, 0);
      glScalef(0.4f/currentScale, 0.4f/currentScale, 1);
      Color color = getTeamColor(mTeam);  // To avoid taking address of temporary
      renderFlag(0, 0, &color);

      glColor(Colors::white);
      drawCircle(-4, 0, 26);
   glPopMatrix();
#endif
}


void FlagSpawn::renderDock()
{
   renderEditor(1);
}


bool FlagSpawn::processArguments(S32 argc, const char **argv, Game *game)
{
   if(argc < 1)
      return false;

   mTeam = atoi(argv[0]);                                            // Read team
   return Parent::processArguments(argc - 1, argv + 1, game);        // then read the rest of the args
}


string FlagSpawn::toString(F32 gridSize) const
{
   // FlagSpawn <team> <x> <y> <spawn timer for nexus> -- squeezing in team number from AbstractSpawn::toString
   string str1 = Parent::toString(gridSize);
   size_t firstarg = str1.find(' ');
   return str1.substr(0, firstarg) + " " + itos(mTeam) + str1.substr(firstarg);
}


};    // namespace