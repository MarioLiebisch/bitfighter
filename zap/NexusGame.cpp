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

#include "NexusGame.h"
#include "robot.h"            // For EventManager

#include "stringUtils.h"      // For ftos et al
#include "masterConnection.h" // For master connection details

#ifndef ZAP_DEDICATED
#   include "gameObjectRender.h"
#   include "ScreenInfo.h"
#   include "ClientGame.h"
#   include "UIGame.h"
#   include "UIMenuItems.h"
#   include "SDL/SDL_opengl.h"
#endif


#include <math.h>

namespace Zap
{

TNL_IMPLEMENT_NETOBJECT(NexusGameType);


TNL_IMPLEMENT_NETOBJECT_RPC(NexusGameType, s2cSetNexusTimer, (U32 nexusTime, bool isOpen), (nexusTime, isOpen), NetClassGroupGameMask, RPCGuaranteedOrdered, RPCToGhost, 0)
{
   mNexusTimer.reset(nexusTime);
   mNexusIsOpen = isOpen;
}

GAMETYPE_RPC_S2C(NexusGameType, s2cAddYardSaleWaypoint, (F32 x, F32 y), (x, y))
{
   YardSaleWaypoint w;
   w.timeLeft.reset(YardSaleWaypointTime);
   w.pos.set(x,y);
   mYardSaleWaypoints.push_back(w);
}


TNL_IMPLEMENT_NETOBJECT_RPC(NexusGameType, s2cNexusMessage,
   (U32 msgIndex, StringTableEntry clientName, U32 flagCount, U32 score), (msgIndex, clientName, flagCount, score),
   NetClassGroupGameMask, RPCGuaranteedOrdered, RPCToGhost, 0)
{
#ifndef ZAP_DEDICATED

   ClientGame *clientGame = static_cast<ClientGame *>(getGame());

   if(msgIndex == NexusMsgScore)
   {
      SoundSystem::playSoundEffect(SFXFlagCapture);
      clientGame->displayMessage(Color(0.6f, 1.0f, 0.8f),"%s returned %d flag%s to the Nexus for %d points!", 
                                 clientName.getString(), flagCount, flagCount > 1 ? "s" : "", score);
   }
   else if(msgIndex == NexusMsgYardSale)
   {
      clientGame->displayMessage(Color(0.6f, 1.0f, 0.8f),
                                 "%s is having a YARD SALE!",
                                 clientName.getString());
      SoundSystem::playSoundEffect(SFXFlagSnatch);
   }
   else if(msgIndex == NexusMsgGameOverWin)
   {
      clientGame->displayMessage(Color(0.6f, 1.0f, 0.8f),
                                 "Player %s wins the game!",
                                 clientName.getString());
      SoundSystem::playSoundEffect(SFXFlagCapture);
   }
   else if(msgIndex == NexusMsgGameOverTie)
   {
      clientGame->displayMessage(Color(0.6f, 1.0f, 0.8f), "The game ended in a tie.");
      SoundSystem::playSoundEffect(SFXFlagDrop);
   }
#endif
}


// Constructor
NexusGameType::NexusGameType() : GameType(100)
{
   mNexusClosedTime = 60;
   mNexusOpenTime = 15;
   mNexusTimer.reset(mNexusClosedTime * 1000);
   mNexusIsOpen = false;
}


bool NexusGameType::processArguments(S32 argc, const char **argv, Game *game)
{
   if(argc > 0)
   {
      setGameTime(F32(atof(argv[0]) * 60.0));                 // Game time, stored in minutes in level file
      if(argc > 1)
      {
         mNexusClosedTime = S32(atof(argv[1]) * 60.f + 0.5);  // Time until nexus opens, specified in minutes (0.5 converts truncation into rounding)
         if(argc > 2)
         {
            mNexusOpenTime = S32(atof(argv[2]));              // Time nexus remains open, specified in seconds
            if(argc > 3)
               setWinningScore(atoi(argv[3]));                // Winning score
         }
      }
   }
   mNexusTimer.reset(mNexusClosedTime * 1000);

   return true;
}


string NexusGameType::toString() const
{
   return string(getClassName()) + " " + ftos(F32(getTotalGameTime()) / 60 , 3) + " " + ftos(F32(mNexusClosedTime) / 60, 3) + " " + 
                                         ftos(F32(mNexusOpenTime), 3) + " " + itos(getWinningScore());
}


S32 NexusGameType::getNexusTimeLeft()
{
   return mNexusTimer.getCurrent();
}


bool NexusGameType::isFlagGame()
{
   return true;  // Well, technically not, but we'll morph flags to our own uses as we load the level
}


bool NexusGameType::isTeamFlagGame()
{
   return false;  // Ditto... team info will be ignored... no need to show warning in editor
}


bool NexusGameType::isSpawnWithLoadoutGame()
{
   return true;
}


void NexusGameType::addNexus(NexusObject *nexus)
{
   mNexus.push_back(nexus);
}


bool NexusGameType::isCarryingItems(Ship *ship)
{
   if(ship->mMountedItems.size() > 1)     // Currently impossible, but in future may be possible
      return true;
   if(ship->mMountedItems.size() == 0)    // Should never happen
      return false;

   MoveItem *item = ship->mMountedItems[0];   // Currently, ship always has a NexusFlagItem... this is it
   if(!item)                                  // Null when a player drop flag and get destroyed at the same time
      return false;  

   return ( ((NexusFlagItem *) item)->getFlagCount() > 0 );    
}


// Cycle through mounted items and find the first one (last one, actually) that's a NexusFlagItem.
// Returns NULL if it can't find one.
static NexusFlagItem *findFirstNexusFlag(Ship *ship)
{
   for(S32 i = ship->mMountedItems.size() - 1; i >= 0; i--)
   {
      MoveItem *item = ship->mMountedItems[i];
      NexusFlagItem *flag = dynamic_cast<NexusFlagItem *>(item);

      if(flag)
         return flag;
   }

   return NULL;
}


// The flag will come from ship->mount.  *item is used as it is posssible to carry and drop multiple items
void NexusGameType::itemDropped(Ship *ship, MoveItem *item)
{
   NexusFlagItem *flag = dynamic_cast<NexusFlagItem *>(item);
   if(!flag)
      return;

   U32 flagCount = flag->getFlagCount();

   if(flagCount == 0)  // This is needed if you drop your flags, then pick up a different item type (like resource item), and drop it
      return;

   if(!ship->getClientInfo())
      return;

   Vector<StringTableEntry> e;

   e.push_back(ship->getClientInfo()->getName());
   if(flagCount > 1)
      e.push_back(itos(flagCount).c_str());


   static StringTableEntry dropOneString( "%e0 dropped a flag!");
   static StringTableEntry dropManyString( "%e0 dropped %e1 flags!");

   StringTableEntry *ste = (flagCount > 1) ? &dropManyString : &dropOneString;

   broadcastMessage(GameConnection::ColorNuclearGreen, SFXFlagDrop, *ste, e);
}


#ifndef ZAP_DEDICATED
// Any unique items defined here must be handled in both getMenuItem() and saveMenuItem() below!
Vector<string> NexusGameType::getGameParameterMenuKeys()
{
   Vector<string> items = Parent::getGameParameterMenuKeys();
   
   // Remove Win Score, replace it with some Nexus specific items
   for(S32 i = 0; i < items.size(); i++)
      if(items[i] == "Win Score")
      {
         items.erase(i);      // Delete "Win Score"

         // Create slots for 3 new items, and fill them with our Nexus specific items
         items.insert(i, "Nexus Time to Open");
         items.insert(i + 1, "Nexus Time Remain Open");
         items.insert(i + 2, "Nexus Win Score");

         break;
      }

   return items;
}


// Definitions for those items
boost::shared_ptr<MenuItem> NexusGameType::getMenuItem(const string &key)
{
   if(key == "Nexus Time to Open")
      return boost::shared_ptr<MenuItem>(new TimeCounterMenuItem("Time for Nexus to Open:", mNexusClosedTime, 99*60, "Never", 
                                                                 "Time it takes for the Nexus to open"));
   else if(key == "Nexus Time Remain Open")
      return boost::shared_ptr<MenuItem>(new TimeCounterMenuItemSeconds("Time Nexus Remains Open:", mNexusOpenTime, 99*60, "Always", 
                                                                        "Time that the Nexus will remain open"));
   else if(key == "Nexus Win Score")
      return boost::shared_ptr<MenuItem>(new CounterMenuItem("Score to Win:", getWinningScore(), 100, 100, 20000, "points", "", 
                                                             "Game ends when one player or team gets this score"));
   else return Parent::getMenuItem(key);
}


bool NexusGameType::saveMenuItem(const MenuItem *menuItem, const string &key)
{
   if(key == "Nexus Time to Open")
      mNexusClosedTime = menuItem->getIntValue();
   else if(key == "Nexus Time Remain Open")
      mNexusOpenTime = menuItem->getIntValue();
   else if(key == "Nexus Win Score")
      setWinningScore(menuItem->getIntValue());
   else 
      return Parent::saveMenuItem(menuItem, key);

   return true;
}
#endif


TNL_IMPLEMENT_NETOBJECT(NexusObject);


TNL_IMPLEMENT_NETOBJECT_RPC(NexusObject, s2cFlagsReturned, (), (), NetClassGroupGameMask, RPCGuaranteedOrdered, RPCToGhost, 0)
{
   getGame()->getGameType()->mZoneGlowTimer.reset();
}


// The nexus is open.  A ship has entered it.  Now what?
// Runs on server only
void NexusGameType::shipTouchNexus(Ship *theShip, NexusObject *theNexus)
{
   NexusFlagItem *theFlag = findFirstNexusFlag(theShip);

   if(!theFlag)      // Just in case!
      return;

   updateScore(theShip, ReturnFlagsToNexus, theFlag->getFlagCount());

   S32 flagsReturned = theFlag->getFlagCount();
   ClientInfo *scorer = theShip->getClientInfo();

   if(flagsReturned > 0 && scorer)
   {
      s2cNexusMessage(NexusMsgScore, scorer->getName().getString(), theFlag->getFlagCount(), 
                      getEventScore(TeamScore, ReturnFlagsToNexus, theFlag->getFlagCount()) );
      theNexus->s2cFlagsReturned();    // Alert the Nexus that someone has returned flags to it
   }
   theFlag->changeFlagCount(0);

   // See if this event qualifies for an achievement
   if(flagsReturned >= 25 &&                              // Return 25+ flags
      scorer && scorer->isAuthenticated() &&              // Player must be authenticated
      getGame()->getPlayerCount() >= 4 &&                 // Game must have 4+ human players
      getGame()->getAuthenticatedPlayerCount() >= 2 &&    // Two of whom must be authenticated
      !hasFlagSpawns() && !hasPredeployedFlags())         // Level can have no flag spawns, nor any predeployed flags
   {
      MasterServerConnection *masterConn = getGame()->getConnectionToMaster();
      if(masterConn && masterConn->isEstablished())
      {
         Vector<StringTableEntry> e;
         e.push_back(scorer->getName());

         masterConn->s2mAcheivementAchieved(BADGE_TWENTY_FIVE_FLAGS, scorer->getName());
         StringTableEntry msg("%0 has earned the TWENTY FIVE FLAGS badge!");
         broadcastMessage(GameConnection::ColorYellow, SFXFlagCapture, msg, e);
      }
   }
}


// Runs on the server
void NexusGameType::onGhostAvailable(GhostConnection *theConnection)
{
   Parent::onGhostAvailable(theConnection);

   NetObject::setRPCDestConnection(theConnection);
   s2cSetNexusTimer(mNexusTimer.getCurrent(), mNexusIsOpen);
   NetObject::setRPCDestConnection(NULL);
}


// Runs on the server
// If a flag is released from a ship, it will have underlying startVel, to which a random vector will be added
void releaseFlag(Game *game, Point pos, Point startVel)
{
   F32 th = TNL::Random::readF() * Float2Pi;
   F32 f = (TNL::Random::readF() * 2 - 1) * 100;
   Point vel(cos(th) * f, sin(th) * f);
   vel += startVel;

   FlagItem *newFlag = new FlagItem(pos, vel, true);
   newFlag->addToGame(game, game->getGameObjDatabase());
}


// Runs on client and server
void NexusGameType::idle(GameObject::IdleCallPath path, U32 deltaT)
{
   Parent::idle(path, deltaT);

   if(isGhost()) 
      idle_client(deltaT);
   else
      idle_server(deltaT);
}


void NexusGameType::idle_client(U32 deltaT)
 {
   mNexusTimer.update(deltaT);
   for(S32 i = 0; i < mYardSaleWaypoints.size();)
   {
      if(mYardSaleWaypoints[i].timeLeft.update(deltaT))
         mYardSaleWaypoints.erase_fast(i);
      else
         i++;
   }
}


void NexusGameType::idle_server(U32 deltaT)
{
   if(!mNexusIsOpen && mNexusTimer.update(deltaT))         // Nexus has just opened
   {
      mNexusTimer.reset(mNexusOpenTime * 1000);
      mNexusIsOpen = true;
      s2cSetNexusTimer(mNexusTimer.getCurrent(), mNexusIsOpen);
      static StringTableEntry msg("The Nexus is now OPEN!");

      // Broadcast a message
      broadcastMessage(GameConnection::ColorNuclearGreen, SFXFlagSnatch, msg);

      // Check if anyone is already in the Nexus, examining each client's ship in turn...
      for(S32 i = 0; i < getGame()->getClientCount(); i++)
      {
         Ship *client_ship = getGame()->getClientInfo(i)->getShip();

         if(!client_ship)
            continue;

         NexusObject *nexus = dynamic_cast<NexusObject *>(client_ship->isInZone(NexusTypeNumber));

         if(nexus)
            shipTouchNexus(client_ship, nexus);
      }

      // Fire an event
      EventManager::get()->fireEvent(EventManager::NexusOpenedEvent);
   }
   else if(mNexusIsOpen && mNexusTimer.update(deltaT))       // Nexus has just closed
   {
      mNexusTimer.reset(mNexusClosedTime * 1000);
      mNexusIsOpen = false;
      s2cSetNexusTimer(mNexusTimer.getCurrent(), mNexusIsOpen);

      static StringTableEntry msg("The Nexus is now CLOSED.");
      broadcastMessage(GameConnection::ColorNuclearGreen, SFXFlagDrop, msg);

      // Fire an event
      EventManager::get()->fireEvent(EventManager::NexusClosedEvent);
   }

   // Advance all flagSpawn timers and see if it's time for a new flag
   for(S32 i = 0; i < getFlagSpawnCount(); i++)
   {
      FlagSpawn *flagSpawn = const_cast<FlagSpawn *>(getFlagSpawn(i));    // We need to get a modifiable pointer so we can update the timer

      if(flagSpawn->updateTimer(deltaT))
      {
         releaseFlag(getGame(), getFlagSpawn(i)->getPos(), Point(0,0));   // Release a flag
         flagSpawn->resetTimer();                                         // Reset the timer
      }
   }
}


// What does a particular scoring event score?
S32 NexusGameType::getEventScore(ScoringGroup scoreGroup, ScoringEvent scoreEvent, S32 flags)
{
   S32 score = 0;
   for(S32 count = 1; count <= flags; count++)
      score += (count * 10);

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
         case ReturnFlagsToNexus:
            return score;
         default:
            return naScore;
      }
   }
   else  // scoreGroup == IndividualScore
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
         case ReturnFlagsToNexus:
            return score;
         default:
            return naScore;
      }
   }
}


GameTypeId NexusGameType::getGameTypeId() const
{
   return NexusGame;
}


const char *NexusGameType::getShortName() const
{
   return "N";
}


const char *NexusGameType::getInstructionString() const
{
   return "Collect flags from opposing players and bring them to the Nexus!";
}


bool NexusGameType::canBeTeamGame() const
{
   return true;
}


bool NexusGameType::canBeIndividualGame() const
{
   return true;
}


U32 NexusGameType::getLowerRightCornerScoreboardOffsetFromBottom() const
{
   return 88;
}


//////////  Client only code:

extern Color gNexusOpenColor;
extern Color gNexusClosedColor;

#define NEXUS_STR mNexusIsOpen ?  "Nexus closes: " : "Nexus opens: "
#define NEXUS_NEVER_STR mNexusIsOpen ? "Nexus never closes" : "Nexus never opens"

#ifndef ZAP_DEDICATED
void NexusGameType::renderInterfaceOverlay(bool scoreboardVisible)
{
   Parent::renderInterfaceOverlay(scoreboardVisible);

   glColor(mNexusIsOpen ? gNexusOpenColor : gNexusClosedColor);      // Display timer in appropriate color

   U32 timeLeft = mNexusTimer.getCurrent();
   U32 minsRemaining = timeLeft / (60 * 1000);
   U32 secsRemaining = (timeLeft - (minsRemaining * 60 * 1000)) / 1000;

   const S32 x = gScreenInfo.getGameCanvasWidth() - UserInterface::horizMargin;
   const S32 y = gScreenInfo.getGameCanvasHeight() - UserInterface::vertMargin - 25;
   const S32 size = 20;

   if(mNexusTimer.getPeriod() == 0)
      UserInterface::drawStringfr(x, y, size, NEXUS_NEVER_STR);
   else
      UserInterface::drawStringfr(x, y, size, "%s%02d:%02d", NEXUS_STR, minsRemaining, secsRemaining);

   for(S32 i = 0; i < mYardSaleWaypoints.size(); i++)
      renderObjectiveArrow(&mYardSaleWaypoints[i].pos, &Colors::white);

   for(S32 i = 0; i < mNexus.size(); i++)
      renderObjectiveArrow(dynamic_cast<GameObject *>(mNexus[i].getPointer()), mNexusIsOpen ? &gNexusOpenColor : &gNexusClosedColor);
}
#endif

#undef NEXUS_STR


//////////  END Client only code



void NexusGameType::controlObjectForClientKilled(ClientInfo *theClient, GameObject *clientObject, GameObject *killerObject)
{
   Parent::controlObjectForClientKilled(theClient, clientObject, killerObject);

   Ship *theShip = dynamic_cast<Ship *>(clientObject);
   if(!theShip)
      return;

   // Check for yard sale  (is this when the flags a player is carrying go drifting about??)
   for(S32 i = theShip->mMountedItems.size() - 1; i >= 0; i--)
   {
      MoveItem *item = theShip->mMountedItems[i];
      NexusFlagItem *flag = dynamic_cast<NexusFlagItem *>(item);

      if(flag)
      {
         if(flag->getFlagCount() >= YardSaleCount)
         {
            Point pos = flag->getActualPos();
            s2cAddYardSaleWaypoint(pos.x, pos.y);
            s2cNexusMessage(NexusMsgYardSale, theShip->getClientInfo()->getName().getString(), 0, 0);
         }

         return;
      }
   }
}


void NexusGameType::shipTouchFlag(Ship *theShip, FlagItem *theFlag)
{
   // Don't mount to ship, instead increase current mounted NexusFlag
   //    flagCount, and remove collided flag from game
   for(S32 i = theShip->mMountedItems.size() - 1; i >= 0; i--)
   {
      NexusFlagItem *theFlag = dynamic_cast<NexusFlagItem *>(theShip->mMountedItems[i].getPointer());
      if(theFlag)
      {
         theFlag->changeFlagCount(theFlag->getFlagCount() + 1);
         break;
      }
   }

   theFlag->setCollideable(false);
   theFlag->removeFromDatabase();
   theFlag->deleteObject();
}


// Special spawn function for Nexus games (runs only on server)
bool NexusGameType::spawnShip(ClientInfo *clientInfo)
{
   if(!Parent::spawnShip(clientInfo))
      return false;

   Ship *ship = clientInfo->getShip();

   NexusFlagItem *newFlag = new NexusFlagItem(ship->getActualPos());
   newFlag->addToGame(getGame(), getGame()->getGameObjDatabase());
   newFlag->mountToShip(ship);    // mountToShip() can handle NULL
   newFlag->changeFlagCount(0);

   return true;
}

////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(NexusFlagItem);

// C++ constructor
NexusFlagItem::NexusFlagItem(Point pos, Point vel, bool useDropDelay) : FlagItem(pos, true, (F32)Ship::CollisionRadius, 4)  // radius was 30, which had problem with sticking to wall when drop too close to walls
{
   mFlagCount = 0;

   setActualVel(vel);
   if(useDropDelay)
      mDroppedTimer.reset(DROP_DELAY);
}


//////////  Client only code:

void NexusFlagItem::renderItem(const Point &pos)
{
#ifndef ZAP_DEDICATED
   // Don't render flags on cloaked ships
   if(mMount.isValid() && mMount->isModulePrimaryActive(ModuleCloak))
      return;

   Parent::renderItem(pos);

   if(mIsMounted && mFlagCount > 0)
   {
      if(mFlagCount >= 40)
         glColor(Colors::paleRed);
      else if(mFlagCount >= 20)
         glColor(Colors::yellow);
      else if(mFlagCount >= 10)
         glColor(Colors::green);
      else
         glColor(Colors::white);

      UserInterface::drawStringf(pos.x + 10, pos.y - 46, 12, "%d", mFlagCount);
   }
#endif
}

//////////  END Client only code



// Private helper function
void NexusFlagItem::dropFlags(U32 flags)
{
   if(!mMount.isValid())
      return;

   if(isGhost())  //avoid problem with adding flag to client, when it doesn't really exist on server.
      return;

   for(U32 i = 0; i < flags; i++)
      releaseFlag(getGame(), mMount->getActualPos(), mMount->getActualVel());

   changeFlagCount(0);
}


void NexusFlagItem::onMountDestroyed()
{
   if(mMount && mMount->getClientInfo())
      mMount->getClientInfo()->getStatistics()->mFlagDrop += mFlagCount + 1;

   dropFlags(mFlagCount + 1);    // Drop at least one flag plus as many as the ship carries

   // Now delete the flag itself
   dismount();
   removeFromDatabase();
   deleteObject();
}


void NexusFlagItem::onItemDropped()
{
   if(!isGhost())
   {
      GameType *gameType = getGame()->getGameType();
      if(!gameType)                 // Crashed here once, don't know why, so I added the check
         return;

      gameType->itemDropped(mMount, this);
   }
   dropFlags(mFlagCount);          // Only dropping the flags we're carrying, not the "extra" one that comes when we die
}


U32 NexusFlagItem::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   if(stream->writeFlag(updateMask & FlagCountMask))
      stream->write(mFlagCount);

   return Parent::packUpdate(connection, updateMask, stream);
}


void NexusFlagItem::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   if(stream->readFlag())
      stream->read(&mFlagCount);

   Parent::unpackUpdate(connection, stream);
}


bool NexusFlagItem::isItemThatMakesYouVisibleWhileCloaked()
{
   return false;
}


void NexusFlagItem::changeFlagCount(U32 change)
{
   mFlagCount = change;
   setMaskBits(FlagCountMask);
}


U32 NexusFlagItem::getFlagCount()
{
   return mFlagCount;
}


bool NexusFlagItem::isAtHome()
{
   return false;
}


void NexusFlagItem::sendHome()
{
   // Do nothing
}


////////////////////////////////////////
////////////////////////////////////////


// Constructor
NexusObject::NexusObject()
{
   mObjectTypeNumber = NexusTypeNumber;
   mNetFlags.set(Ghostable);
}


NexusObject *NexusObject::clone() const
{
   return new NexusObject(*this);
}


// The nexus object itself
// If there are 2 or 4 params, this is an Zap! rectangular format object
// If there are more, this is a Bitfighter polygonal format object
// Note parallel code in EditorUserInterface::processLevelLoadLine
bool NexusObject::processArguments(S32 argc2, const char **argv2, Game *game)
{
   // Need to handle or ignore arguments that starts with letters,
   // so a possible future version can add parameters without compatibility problem.
   S32 argc = 0;
   const char *argv[64]; // 32 * 2 = 64
   for(S32 i = 0; i < argc2; i++)  // the idea here is to allow optional R3.5 for rotate at speed of 3.5
   {
      char c = argv2[i][0];
      //switch(c)
      //{
      //case 'A': Something = atof(&argv2[i][1]); break;  // using second char to handle number
      //}
      if((c < 'a' || c > 'z') && (c < 'A' || c > 'Z'))
      {
         if(argc < 65)
         {  argv[argc] = argv2[i];
            argc++;
         }
      }
   }

   if(argc < 2)
      return false;

   if(argc <= 4)     // Zap! format
   {
      Point pos;
      pos.read(argv);
      pos *= game->getGridSize();

      Point ext(50, 50);
      if(argc == 4)
         ext.set(atoi(argv[2]), atoi(argv[3]));

      addVert(Point(pos.x - ext.x, pos.y - ext.y));   // UL corner
      addVert(Point(pos.x + ext.x, pos.y - ext.y));   // UR corner
      addVert(Point(pos.x + ext.x, pos.y + ext.y));   // LR corner
      addVert(Point(pos.x - ext.x, pos.y + ext.y));   // LL corner
   }
   else              // Bitfighter format
      readGeom(argc, argv, 0, game->getGridSize());

   updateExtentInDatabase();   

   return true;
}


const char *NexusObject::getEditorHelpString()
{
   return "Area to bring flags in Hunter game.  Cannot be used in other games.";
}


const char *NexusObject::getPrettyNamePlural()
{
   return "Nexii";
}


const char *NexusObject::getOnDockName()
{
   return "Nexus";
}


const char *NexusObject::getOnScreenName()
{
   return "Nexus";
}


string NexusObject::toString(F32 gridSize) const
{
   return string(getClassName()) + " " + geomToString(gridSize);
}


void NexusObject::onAddedToGame(Game *theGame)
{
   Parent::onAddedToGame(theGame);

   if(!isGhost())
      setScopeAlways();    // Always visible!

   NexusGameType *gt = dynamic_cast<NexusGameType *>( getGame()->getGameType() );
   if(gt) gt->addNexus(this);
}

void NexusObject::idle(GameObject::IdleCallPath path)
{
   //U32 deltaT = mCurrentMove.time;
}


void NexusObject::render()
{
#ifndef ZAP_DEDICATED
   GameType *gt = getGame()->getGameType();
   NexusGameType *theGameType = dynamic_cast<NexusGameType *>(gt);
   renderNexus(getOutline(), getFill(), getCentroid(), getLabelAngle(), 
              (theGameType && theGameType->mNexusIsOpen), gt ? gt->mZoneGlowTimer.getFraction() : 0);
#endif
}


void NexusObject::renderDock()
{
#ifndef ZAP_DEDICATED
  renderNexus(getOutline(), getFill(), false, 0);
#endif
}


S32 NexusObject::getRenderSortValue()
{
   return -1;
}


void NexusObject::renderEditor(F32 currentScale)
{
   render();
   EditorPolygon::renderEditor(currentScale);
}


bool NexusObject::getCollisionPoly(Vector<Point> &polyPoints) const
{
   polyPoints = *getOutline();
   return true;
}


bool NexusObject::collide(GameObject *hitObject)
{
   if(isGhost())
      return false;

   // From here on out, runs on server only

   if( ! (isShipType(hitObject->getObjectTypeNumber())) )
      return false;

   Ship *theShip = dynamic_cast<Ship *>(hitObject);
   if(!theShip)
      return false;

   if(theShip->hasExploded)                              // Ignore collisions with exploded ships
      return false;

   NexusGameType *theGameType = dynamic_cast<NexusGameType *>(getGame()->getGameType());
   if(theGameType && theGameType->mNexusIsOpen)          // Is the nexus open?
      theGameType->shipTouchNexus(theShip, this);

   return false;
}


U32 NexusObject::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   packGeom(connection, stream);

   return 0;
}


void NexusObject::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   unpackGeom(connection, stream);      
}


const char NexusObject::className[] = "NexusObject";      // Class name as it appears to Lua scripts

// Define the methods we will expose to Lua
Lunar<NexusObject>::RegType NexusObject::methods[] =
{
   // Standard gameItem methods
   method(NexusObject, getClassID),
   method(NexusObject, getLoc),
   method(NexusObject, getRad),
   method(NexusObject, getVel),
   method(NexusObject, getTeamIndx),

   {0,0}    // End method list
};


//  Lua constructor
NexusObject::NexusObject(lua_State *L)
{
   // Do nothing
}


GameObject *NexusObject::getGameObject()
{
   return this;
}


S32 NexusObject::getClassID(lua_State *L)
{
   return returnInt(L, NexusTypeNumber);
}


void NexusObject::push(lua_State *L)
{
   Lunar<NexusObject>::push(L, this);
}


};

