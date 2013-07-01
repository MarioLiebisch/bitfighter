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

#include "soccerGame.h"

#include "gameNetInterface.h"
#include "projectile.h"
#include "goalZone.h"
#include "Spawn.h"      // For AbstractSpawn def

#include "Colors.h"

#include "gameObjectRender.h"


namespace Zap
{

// Constructor
SoccerGameType::SoccerGameType()
{
   // Do nothing
}

// Destructor
SoccerGameType::~SoccerGameType()
{
   // Do nothing
}


TNL_IMPLEMENT_NETOBJECT(SoccerGameType);

TNL_IMPLEMENT_NETOBJECT_RPC(SoccerGameType, s2cSoccerScoreMessage,
   (U32 msgIndex, StringTableEntry clientName, RangedU32<0, GameType::gMaxTeamCount> teamIndex, Point scorePos), (msgIndex, clientName, teamIndex, scorePos),
   NetClassGroupGameMask, RPCGuaranteedOrdered, RPCToGhost, 0)
{
   // Before calling this RPC, we subtracted gFirstTeamNumber, so we need to add it back here...
   S32 teamIndexAdjusted = (S32) teamIndex + GameType::gFirstTeamNumber;      
   string msg;
   getGame()->playSoundEffect(SFXFlagCapture);
   getGame()->emitTextEffect("GOAL!", Colors::red80, scorePos);

   // Compose the message

   if(clientName.isNull())    // Unknown player scored
   {
      if(teamIndexAdjusted >= 0)
         msg = "A goal was scored on team " + string(getGame()->getTeamName(teamIndexAdjusted).getString());
      else if(teamIndexAdjusted == -1)
         msg = "A goal was scored on a neutral goal!";
      else if(teamIndexAdjusted == -2)
         msg = "A goal was scored on a hostile goal!";
      else
         msg = "A goal was scored on an unknown goal!";
   }
   else if(msgIndex == SoccerMsgScoreGoal)
   {
      if(isTeamGame())
      {
         if(teamIndexAdjusted >= 0)
            msg = string(clientName.getString()) + " scored a goal on team " + string(getGame()->getTeamName(teamIndexAdjusted).getString());
         else if(teamIndexAdjusted == -1)
            msg = string(clientName.getString()) + " scored a goal on a neutral goal!";
         else if(teamIndexAdjusted == -2)
            msg = string(clientName.getString()) + " scored a goal on a hostile goal (for negative points!)";
         else
            msg = string(clientName.getString()) + " scored a goal on an unknown goal!";
      }
      else  // every man for himself
      {
         if(teamIndexAdjusted >= -1)      // including neutral goals
            msg = string(clientName.getString()) + " scored a goal!";
         else if(teamIndexAdjusted == -2)
            msg = string(clientName.getString()) + " scored a goal on a hostile goal (for negative points!)";
      }
   }
   else if(msgIndex == SoccerMsgScoreOwnGoal)
   {
      msg = string(clientName.getString()) + " scored an own-goal, giving the other team" + 
                  (getGame()->getTeamCount() == 2 ? "" : "s") + " a point!";
   }

   // Print the message
   getGame()->displayMessage(Color(0.6f, 1.0f, 0.8f), msg.c_str());
}


void SoccerGameType::setBall(SoccerBallItem *theBall)
{
   mBall = theBall;
}


// Helper function to make sure the two-arg version of updateScore doesn't get a null ship
void SoccerGameType::updateSoccerScore(Ship *ship, S32 scoringTeam, ScoringEvent scoringEvent, S32 score)
{
   if(ship)
      updateScore(ship, scoringEvent, score);
   else
      updateScore(NULL, scoringTeam, scoringEvent, score);
}


void SoccerGameType::scoreGoal(Ship *ship, const StringTableEntry &scorerName, S32 scoringTeam, const Point &scorePos, S32 goalTeamIndex, S32 score)
{
   if(scoringTeam == NO_TEAM)
   {
      s2cSoccerScoreMessage(SoccerMsgScoreGoal, scorerName, (U32) (goalTeamIndex - gFirstTeamNumber), scorePos);
      return;
   }

   if(isTeamGame() && (scoringTeam == TEAM_NEUTRAL || scoringTeam == goalTeamIndex))    // Own-goal
   {
      updateSoccerScore(ship, scoringTeam, ScoreGoalOwnTeam, score);

      // Subtract gFirstTeamNumber to fit goalTeamIndex into a neat RangedU32 container
      s2cSoccerScoreMessage(SoccerMsgScoreOwnGoal, scorerName, (U32) (goalTeamIndex - gFirstTeamNumber), scorePos);
   }
   else     // Goal on someone else's goal
   {
      if(goalTeamIndex == TEAM_HOSTILE)
         updateSoccerScore(ship, scoringTeam, ScoreGoalHostileTeam, score);
      else
         updateSoccerScore(ship, scoringTeam, ScoreGoalEnemyTeam, score);

      s2cSoccerScoreMessage(SoccerMsgScoreGoal, scorerName, (U32) (goalTeamIndex - gFirstTeamNumber), scorePos);      // See comment above
   }
}


// Runs on client
void SoccerGameType::renderInterfaceOverlay(S32 canvasWidth, S32 canvasHeight) const
{
#ifndef ZAP_DEDICATED

   Parent::renderInterfaceOverlay(canvasWidth, canvasHeight);

   Ship *ship = getGame()->getLocalPlayerShip();

   if(!ship)
      return;

   S32 team = ship->getTeam();

   const Vector<DatabaseObject *> *zones = getGame()->getGameObjDatabase()->findObjects_fast(GoalZoneTypeNumber);

   for(S32 i = 0; i < zones->size(); i++)
   {
      GoalZone *zone = static_cast<GoalZone *>(zones->get(i));

      if(zone->getTeam() != team)
         renderObjectiveArrow(zone, canvasWidth, canvasHeight);
   }

   if(mBall.isValid())
      renderObjectiveArrow(mBall, canvasWidth, canvasHeight);
#endif
}


GameTypeId SoccerGameType::getGameTypeId() const { return SoccerGame; }
const char *SoccerGameType::getShortName() const { return "S"; }

static const char *instructions[] = { "Push the ball into the",  "opposing team's goal!" };
const char **SoccerGameType::getInstructionString() const { return instructions; } 

HelpItem SoccerGameType::getGameStartInlineHelpItem() const { return SGameStartItem; }

bool SoccerGameType::canBeTeamGame()       const { return true;  }
bool SoccerGameType::canBeIndividualGame() const { return true;  }


// What does a particular scoring event score?
S32 SoccerGameType::getEventScore(ScoringGroup scoreGroup, ScoringEvent scoreEvent, S32 data)
{
   if(scoreGroup == TeamScore)
   {
      switch(scoreEvent)
      {
         case KillEnemy:
            return 0;
         case KilledByAsteroid:  // Fall through OK
         case KilledByTurret:    // Fall through OK
         case KillSelf:
            return 0;
         case KillTeammate:
            return 0;
         case KillEnemyTurret:
            return 0;
         case KillOwnTurret:
            return 0;
         case ScoreGoalEnemyTeam:
            return data;
         case ScoreGoalOwnTeam:
            return -data;
         case ScoreGoalHostileTeam:
            return -data;
         default:
            return naScore;
      }
   }
   else  // scoreGroup == IndividualScore
   {
      switch(scoreEvent)
      {
         case KillEnemy:
            return 1;
         case KilledByAsteroid:  // Fall through OK
         case KilledByTurret:    // Fall through OK
         case KillSelf:
            return -1;
         case KillTeammate:
            return 0;
         case KillEnemyTurret:
            return 1;
         case KillOwnTurret:
            return -1;
         case ScoreGoalEnemyTeam:
            return 5 * data;
         case ScoreGoalOwnTeam:
            return -5 * data;
         case ScoreGoalHostileTeam:
            return -5 * data;
         default:
            return naScore;
      }
   }
}

////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(SoccerBallItem);

static const F32 SOCCER_BALL_ITEM_MASS = 4;

/**
  *  @luaconst SoccerBallItem::SoccerBallItem()
  *  @luaconst SoccerBallItem::SoccerBallItem(point)
  */
// Combined Lua / C++ default constructor
SoccerBallItem::SoccerBallItem(lua_State *L) : Parent(Point(0,0), true, (F32)SoccerBallItem::SOCCER_BALL_RADIUS, SOCCER_BALL_ITEM_MASS)
{
   mObjectTypeNumber = SoccerBallItemTypeNumber;
   mNetFlags.set(Ghostable);
   mLastPlayerTouch = NULL;
   mLastPlayerTouchTeam = NO_TEAM;
   mLastPlayerTouchName = StringTableEntry(NULL);

   mSendHomeTimer.setPeriod(1500);     // Ball will linger in goal for 1500 ms before being sent home

   mDragFactor = 0.0;      // No drag
   mLuaBall = false;

   if(L)
   {
      static LuaFunctionArgList constructorArgList = { {{ END }, { PT, END }}, 2 };

      S32 profile = checkArgList(L, constructorArgList, "SoccerBallItem", "constructor");

      if(profile == 1)
         setPos(L, 1);

      mLuaBall = true;
   }

   mInitialPos = getPos();

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


SoccerBallItem::~SoccerBallItem()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


SoccerBallItem *SoccerBallItem::clone() const
{
   return new SoccerBallItem(*this);
}


bool SoccerBallItem::processArguments(S32 argc2, const char **argv2, Game *game)
{
   S32 argc = 0;
   const char *argv[16];

   for(S32 i = 0; i < argc2; i++)      // The idea here is to allow optional R3.5 for rotate at speed of 3.5
   {
      char firstChar = argv2[i][0];    // First character of arg

      if((firstChar < 'a' || firstChar > 'z') && (firstChar < 'A' || firstChar > 'Z'))    // firstChar is not a letter
      {
         if(argc < 16)
         {  
            argv[argc] = argv2[i];
            argc++;
         }
      }
   }

   if(!Parent::processArguments(argc, argv, game))
      return false;

   mInitialPos = getActualPos();

   // Add a spawn point at the ball's starting location
   FlagSpawn *spawn = new FlagSpawn(mInitialPos, 0);
   spawn->addToGame(game, game->getGameObjDatabase());

   return true;
}


// Yes, this method is superfluous, but makes it clear that it wasn't forgotten... always include toLevelCode() alongside processArguments()!
string SoccerBallItem::toLevelCode(F32 gridSize) const
{
   return Parent::toLevelCode(gridSize);
}


void SoccerBallItem::onAddedToGame(Game *game)
{
   Parent::onAddedToGame(game);

   // Make soccer ball always visible
   if(!isGhost())
      setScopeAlways();

   // Make soccer ball only visible when in scope
   //if(!isGhost())
   //   theGame->getGameType()->addItemOfInterest(this);

   //((SoccerGameType *) theGame->getGameType())->setBall(this);
   GameType *gt = game->getGameType();
   if(gt)
   {
      if(gt->getGameTypeId() == SoccerGame)
         static_cast<SoccerGameType *>(gt)->setBall(this);
   }

   // If this ball was added by Lua, make sure there is a spawn point at its
   // starting position
   if(mLuaBall)
   {
      FlagSpawn *spawn = new FlagSpawn(mInitialPos, 0);
      spawn->addToGame(mGame, mGame->getGameObjDatabase());
   }
}


void SoccerBallItem::renderItem(const Point &pos)
{
   renderSoccerBall(pos);
}


const char *SoccerBallItem::getOnScreenName()     { return "Soccer Ball";  }
const char *SoccerBallItem::getOnDockName()       { return "Ball";         }
const char *SoccerBallItem::getPrettyNamePlural() { return "Soccer Balls"; }
const char *SoccerBallItem::getEditorHelpString() { return "Soccer ball, can only be used in Soccer games."; }


bool SoccerBallItem::hasTeam()      { return false; }
bool SoccerBallItem::canBeHostile() { return false; }
bool SoccerBallItem::canBeNeutral() { return false; }


const Color *SoccerBallItem::getColor() const
{ 
   return getGame()->getTeamColor(TEAM_NEUTRAL);
}


void SoccerBallItem::renderDock()
{
   renderSoccerBall(getRenderPos(), 7);
}


void SoccerBallItem::renderEditor(F32 currentScale, bool snappingToWallCornersEnabled)
{
   renderItem(getRenderPos());
}


void SoccerBallItem::idle(BfObject::IdleCallPath path)
{
   if(mSendHomeTimer.update(mCurrentMove.time))
   {
      if(!isGhost())
         sendHome();
   }
   else if(mSendHomeTimer.getCurrent())      // Goal has been scored, waiting for ball to reset
   {
      F32 accelFraction = 1 - (0.95f * mCurrentMove.time * 0.001f);

      setActualVel(getActualVel() * accelFraction);
      setRenderVel(getActualVel() * accelFraction);
   }
   
   else if(getActualVel().lenSquared() > 0)  // Add some friction to the soccer ball
   {
      F32 accelFraction = 1 - (mDragFactor * mCurrentMove.time * 0.001f);
   
      setActualVel(getActualVel() * accelFraction);
      setRenderVel(getActualVel() * accelFraction);
   }

   Parent::idle(path);
}


void SoccerBallItem::damageObject(DamageInfo *theInfo)
{
   computeImpulseDirection(theInfo);

   if(theInfo->damagingObject)
   {
      U8 typeNumber = theInfo->damagingObject->getObjectTypeNumber();

      if(isShipType(typeNumber))
      {
         mLastPlayerTouch = static_cast<Ship *>(theInfo->damagingObject);
         mLastPlayerTouchTeam = mLastPlayerTouch->getTeam();
         if(mLastPlayerTouch->getClientInfo())
            mLastPlayerTouchName = mLastPlayerTouch->getClientInfo()->getName();
         else
            mLastPlayerTouchName = NULL;
      }

      else if(isProjectileType(typeNumber))
      {
         BfObject *shooter;

         if(typeNumber == BulletTypeNumber)
            shooter = static_cast<Projectile *>(theInfo->damagingObject)->mShooter;
         else if(typeNumber == BurstTypeNumber || typeNumber == MineTypeNumber || typeNumber == SpyBugTypeNumber)
            shooter = static_cast<Burst *>(theInfo->damagingObject)->mShooter;
         else if(typeNumber == SeekerTypeNumber)
            shooter = static_cast<Seeker *>(theInfo->damagingObject)->mShooter;
         else
         {
            TNLAssert(false, "Undefined projectile type?");
            shooter = NULL;
         }

         if(shooter && isShipType(shooter->getObjectTypeNumber()))
         {
            Ship *ship = static_cast<Ship *>(shooter);
            mLastPlayerTouch = ship;             // If shooter was a turret, say, we'd expect s to be NULL.
            mLastPlayerTouchTeam = theInfo->damagingObject->getTeam(); // Projectile always have a team from what fired it, can be used to credit a team.
            if(ship->getClientInfo())
               mLastPlayerTouchName = ship->getClientInfo()->getName();
            else
               mLastPlayerTouchName = NULL;
         }
      }
      else
         resetPlayerTouch();
   }
}


void SoccerBallItem::resetPlayerTouch()
{
   mLastPlayerTouch = NULL;
   mLastPlayerTouchTeam = NO_TEAM;
   mLastPlayerTouchName = StringTableEntry(NULL);
}


void SoccerBallItem::sendHome()
{
   TNLAssert(!isGhost(), "Should only run on server!");

   // In soccer game, we use flagSpawn points to designate where the soccer ball should spawn.
   // We'll simply redefine "initial pos" as a random selection of the flag spawn points

   Vector<AbstractSpawn *> spawnPoints = getGame()->getGameType()->getSpawnPoints(FlagSpawnTypeNumber);

   S32 spawnIndex = TNL::Random::readI() % spawnPoints.size();
   mInitialPos = spawnPoints[spawnIndex]->getPos();

   setPosVelAng(mInitialPos, Point(0,0), 0);

   setMaskBits(WarpPositionMask | PositionMask);      // By warping, we eliminate the "drifting" effect we got when we used PositionMask

   updateExtentInDatabase();

   resetPlayerTouch();
}


bool SoccerBallItem::collide(BfObject *hitObject)
{
   if(mSendHomeTimer.getCurrent())     // If we've already scored, and we're waiting for the ball to reset, there's nothing to do
      return true;

   if(isShipType(hitObject->getObjectTypeNumber()))
   {
      if(!isGhost())    //Server side
      {
         Ship *ship = static_cast<Ship *>(hitObject);
         mLastPlayerTouch = ship;
         mLastPlayerTouchTeam = mLastPlayerTouch->getTeam();                  // Used to credit team if ship quits game before goal is scored
         if(mLastPlayerTouch->getClientInfo())
            mLastPlayerTouchName = mLastPlayerTouch->getClientInfo()->getName(); // Used for making nicer looking messages in same situation
         else
            mLastPlayerTouchName = NULL;
      }
   }
   else if(hitObject->getObjectTypeNumber() == GoalZoneTypeNumber)      // SCORE!!!!
   {
      GoalZone *goal = static_cast<GoalZone *>(hitObject);

      if(!isGhost())
      {
         GameType *gameType = getGame()->getGameType();
         if(gameType && gameType->getGameTypeId() == SoccerGame)
            static_cast<SoccerGameType *>(gameType)->scoreGoal(mLastPlayerTouch, mLastPlayerTouchName, mLastPlayerTouchTeam, getActualPos(), goal->getTeam(), goal->getScore());

         mSendHomeTimer.reset();
      }
      return false;
   }
   return true;
}


U32 SoccerBallItem::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   return Parent::packUpdate(connection, updateMask, stream);
}


void SoccerBallItem::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   Parent::unpackUpdate(connection, stream);
}


/////
// Lua interface

/**
 *  @luaclass SoccerBallItem
 *  @brief    Target object used in Soccer games
 */
// No soccerball specific methods!
//                Fn name                  Param profiles            Profile count                           
#define LUA_METHODS(CLASS, METHOD) \

GENERATE_LUA_FUNARGS_TABLE(SoccerBallItem, LUA_METHODS);
GENERATE_LUA_METHODS_TABLE(SoccerBallItem, LUA_METHODS);

#undef LUA_METHODS



const char *SoccerBallItem::luaClassName = "SoccerBallItem";
REGISTER_LUA_SUBCLASS(SoccerBallItem, MoveObject);


};

