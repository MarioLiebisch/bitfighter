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

#include "retrieveGame.h"
#include "goalZone.h"
#include "ship.h"
#include "flagItem.h"
#include "gameObjectRender.h"
#include "game.h"
#include "gameConnection.h"

#ifndef ZAP_DEDICATED
#include "ClientGame.h"
#endif

namespace Zap
{

   // Server only
   void RetrieveGameType::addFlag(FlagItem *flag)
   {
      //S32 i;
      //for(i = 0; i < mFlags.size(); i++)  // What is this?
      //{
      //   if(mFlags[i] == NULL)
      //   {
      //      mFlags[i] = theFlag;
      //      break;
      //   }
      //}
      //if(i == mFlags.size())
      //   mFlags.push_back(theFlag);    // Parent::addFlag(flag);

      Parent::addFlag(flag);

      if(!isGhost())
         addItemOfInterest(flag);      // Server only  
   }


   void RetrieveGameType::addZone(GoalZone *zone)
   {
      mZones.push_back(zone);
   }


   // Note -- neutral or enemy-to-all robots can't pick up the flag!!!
   void RetrieveGameType::shipTouchFlag(Ship *theShip, FlagItem *theFlag)
   {
      // See if the ship is already carrying a flag - can only carry one at a time
      if(theShip->carryingFlag() != NO_FLAG)
         return;

      // Can only pick up flags on your team or neutral
      if(theFlag->getTeam() != -1 && theShip->getTeam() != theFlag->getTeam())
         return;

      S32 flagIndex;

      for(flagIndex = 0; flagIndex < mFlags.size(); flagIndex++)
         if(mFlags[flagIndex] == theFlag)
            break;

      // See if this flag is already in a flag zone owned by the ship's team
      if(theFlag->getZone() != NULL && theFlag->getZone()->getTeam() == theShip->getTeam())
         return;

      static StringTableEntry stealString("%e0 stole a flag from team %e1!");
      static StringTableEntry takeString("%e0 of team %e1 took a flag!");
      static StringTableEntry oneFlagTakeString("%e0 of team %e1 took the flag!");

      StringTableEntry r = takeString;

      if(mFlags.size() == 1)
         r = oneFlagTakeString;

      S32 team;
      if(theFlag->getZone() == NULL)      // Picked up flag just sitting around
         team = theShip->getTeam();
      else                                // Grabbed flag from enemy zone
      {
         r = stealString;
         team = theFlag->getZone()->getTeam();
         updateScore(team, LostFlag);
         if(theShip->getOwner())
            theShip->getOwner()->mStatistics.mFlagReturn++;  // used as flag steal
      }
      if(theShip->getOwner())
         theShip->getOwner()->mStatistics.mFlagPickup++;

      Vector<StringTableEntry> e;
      e.push_back(theShip->getName());
      e.push_back(getGame()->getTeamName(team));

      broadcastMessage(GameConnection::ColorNuclearGreen, SFXFlagSnatch, r, e);

      theFlag->mountToShip(theShip);
      updateScore(theShip, RemoveFlagFromEnemyZone);
      theFlag->setZone(NULL);
   }


   void RetrieveGameType::itemDropped(Ship *ship, MoveItem *item)
   {
      FlagItem *flag = dynamic_cast<FlagItem *>(item);

      if(flag)
      {
         static StringTableEntry dropString("%e0 dropped a flag!");
         Vector<StringTableEntry> e;
         e.push_back(ship->getName());

         broadcastMessage(GameConnection::ColorNuclearGreen, SFXFlagDrop, dropString, e);
      }
   }


   // The ship has entered a drop zone, either friend or foe
   void RetrieveGameType::shipTouchZone(Ship *s, GoalZone *z)
   {
      // See if this is an opposing team's zone.  If so, do nothing.
      if(s->getTeam() != z->getTeam())
         return;

      // See if this zone already has a flag in it.  If so, do nothing.
      for(S32 i = 0; i < mFlags.size(); i++)
         if(mFlags[i]->getZone() == z)
            return;

      // Ok, it's an empty zone on our team: See if this ship is carrying a flag...
      S32 flagIndex = s->carryingFlag();

      if(flagIndex == NO_FLAG)
         return;

      // Ok, the ship has a flag and it's on the ship and we're in an empty zone
      MoveItem *item = s->mMountedItems[flagIndex];
      FlagItem *mountedFlag = dynamic_cast<FlagItem *>(item);
      if(mountedFlag)
      {
         static StringTableEntry capString("%e0 retrieved a flag!");
         static StringTableEntry oneFlagCapString("%e0 retrieved the flag!");

         Vector<StringTableEntry> e;
         e.push_back(s->getName());
         broadcastMessage(GameConnection::ColorNuclearGreen, SFXFlagCapture, (mFlags.size() == 1) ? oneFlagCapString : capString, e);

         // Drop the flag into the zone
         mountedFlag->dismount();

         S32 flagIndex;
         for(flagIndex = 0; flagIndex < mFlags.size(); flagIndex++)
            if(mFlags[flagIndex] == mountedFlag)
               break;

         mFlags[flagIndex]->setZone(z);
         mountedFlag->setActualPos(z->getExtent().getCenter());

         // Score the flag...
         updateScore(s, ReturnFlagToZone);

         if(s->getOwner())
            s->getOwner()->mStatistics.mFlagScore++;

         // See if all the flags are owned by one team...
         for(S32 i = 0; i < mFlags.size(); i++)
         {
            bool ourFlag = (mFlags[i]->getTeam() == s->getTeam()) || (mFlags[i]->getTeam() == -1);    // Team flag || neutral flag
            if(ourFlag && (!mFlags[i]->getZone() || mFlags[i]->getZone()->getTeam() != s->getTeam()))
               return;     // ...if not, we're done
         }

         // One team has all the flags
         if(mFlags.size() != 1)
         {
            static StringTableEntry capAllString("Team %e0 retrieved all the flags!");
            e[0] = getGame()->getTeamName(s->getTeam());

            for(S32 i = 0; i < getGame()->getClientCount(); i++)
              getGame()->getClient(i)->getConnection()->s2cTouchdownScored(SFXFlagCapture, s->getTeam(), capAllString, e);
         }

         // Return all the flags to their starting locations if need be
         for(S32 i = 0; i < mFlags.size(); i++)
         {
            if(mFlags[i]->getTeam() == s->getTeam() || mFlags[i]->getTeam() == -1) // team and neutral flags
            {
               mFlags[i]->setZone(NULL);
               if(!mFlags[i]->isAtHome())
                  mFlags[i]->sendHome();
            }
         }
      }
   }


   // A major scoring event has ocurred -- in this case, it's all flags being collected by one team
   void RetrieveGameType::majorScoringEventOcurred(S32 team)
   {
      mZoneGlowTimer.reset();
      mGlowingZoneTeam = team;
   }

   // Same code as in HTF, CTF
   void RetrieveGameType::performProxyScopeQuery(GameObject *scopeObject, GameConnection *connection)
   {
      Parent::performProxyScopeQuery(scopeObject, connection);
      S32 uTeam = scopeObject->getTeam();

      for(S32 i = 0; i < mFlags.size(); i++)
      {
         if(mFlags[i]->isAtHome() || mFlags[i]->getZone())
            connection->objectInScope(mFlags[i]);
         else
         {
            Ship *mount = mFlags[i]->getMount();
            if(mount && mount->getTeam() == uTeam)
            {
               connection->objectInScope(mount);
               connection->objectInScope(mFlags[i]);
            }
         }
      }
   }


   // Runs on client
   void RetrieveGameType::renderInterfaceOverlay(bool scoreboardVisible)
   {
#ifndef ZAP_DEDICATED

      Parent::renderInterfaceOverlay(scoreboardVisible);
      Ship *ship = dynamic_cast<Ship *>(dynamic_cast<ClientGame *>(getGame())->getConnectionToServer()->getControlObject());
      if(!ship)
         return;
      bool uFlag = false;
      S32 team = ship->getTeam();

      for(S32 i = 0; i < mFlags.size(); i++)
      {
         if(mFlags[i].isValid() && mFlags[i]->getMount() == ship)
         {
            for(S32 j = 0; j < mZones.size(); j++)
            {
               // See if this is one of our zones and that it doesn't have a flag in it.
               if(mZones[j]->getTeam() != team)
                  continue;
               S32 k;
               for(k = 0; k < mFlags.size(); k++)
               {
                  if(!mFlags[k].isValid())
                     continue;
                  if(mFlags[k]->getZone() == mZones[j])
                     break;
               }
               if(k == mFlags.size())
                  renderObjectiveArrow(mZones[j], getTeamColor(team));
            }
            uFlag = true;
            break;
         }
      }

      for(S32 i = 0; i < mFlags.size(); i++)
      {
         if(!mFlags[i].isValid())
            continue;

         if(!mFlags[i]->isMounted() && !uFlag)
         {
            GoalZone *gz = mFlags[i]->getZone();
            if(gz && gz->getTeam() != team)
               renderObjectiveArrow(mFlags[i], getTeamColor(gz->getTeam()));
            else if(!gz)
               renderObjectiveArrow(mFlags[i], getTeamColor(-1));
         }
         else
         {
            Ship *mount = mFlags[i]->getMount();
            if(mount && mount != ship)
               renderObjectiveArrow(mount, getTeamColor(mount->getTeam()));
         }
      }
#endif
   }


   // What does a particular scoring event score?
   S32 RetrieveGameType::getEventScore(ScoringGroup scoreGroup, ScoringEvent scoreEvent, S32 data)
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
            case ReturnFlagToZone:
               return 1;
            case RemoveFlagFromEnemyZone:
               return 0;
         case LostFlag:    // Not really an individual scoring event!
               return -1;
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
            case ReturnFlagToZone:
               return 2;
            case RemoveFlagFromEnemyZone:
               return 1;
            // case LostFlag:    // Not really an individual scoring event!
            //    return 0;
            default:
               return naScore;
         }
      }
   }


TNL_IMPLEMENT_NETOBJECT(RetrieveGameType);


};


