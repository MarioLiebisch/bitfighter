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

#include "CTFGame.h"
#include "ship.h"
#include "UIGame.h"
#include "flagItem.h"
#include "game.h"

#ifndef ZAP_DEDICATED
#include "ClientGame.h"
#endif

#include <stdio.h>

namespace Zap
{

TNL_IMPLEMENT_NETOBJECT(CTFGameType);


void CTFGameType::addFlag(FlagItem *flag)
{
   //S32 i;
   //for(i = 0; i < mFlags.size(); i++)     // What is this for???
   //{
   //   if(mFlags[i] == NULL)
   //   {
   //      mFlags[i] = theFlag;
   //      break;
   //   }
   //}
   //if(i == mFlags.size())
   //   mFlags.push_back(theFlag); 

   Parent::addFlag(flag);

   if(!isGhost())
      addItemOfInterest(flag);      // Server only
}


void CTFGameType::shipTouchFlag(Ship *theShip, FlagItem *theFlag)
{
   if(theShip->getTeam() == theFlag->getTeam())      // Touch own flag
   {
      if(!theFlag->isAtHome())
      {
         static StringTableEntry returnString("%e0 returned the %e1 flag.");

         Vector<StringTableEntry> e;
         e.push_back(theShip->getName());
         e.push_back(getGame()->getTeamName(theFlag->getTeam()));

         for(S32 i = 0; i < getClientCount(); i++)
            getClient(i)->clientConnection->s2cDisplayMessageE(GameConnection::ColorNuclearGreen, SFXFlagReturn, returnString, e);

         theFlag->sendHome();

         updateScore(theShip, ReturnTeamFlag);

         if(theShip->getOwner())
            theShip->getOwner()->mStatistics.mFlagReturn++;
      }
      else     // Flag is at home
      {
         // Check if this client has an enemy flag mounted
         for(S32 i = 0; i < theShip->mMountedItems.size(); i++)
         {
            FlagItem *mountedFlag = dynamic_cast<FlagItem *>(theShip->mMountedItems[i].getPointer());
            if(mountedFlag)
            {
               static StringTableEntry capString("%e0 captured the %e1 flag!");
               Vector<StringTableEntry> e;
               e.push_back(theShip->getName());
               e.push_back(getGame()->getTeamName(mountedFlag->getTeam()));

               for(S32 i = 0; i < getClientCount(); i++)
                  getClient(i)->clientConnection->s2cDisplayMessageE(GameConnection::ColorNuclearGreen, SFXFlagCapture, capString, e);

               mountedFlag->dismount();
               mountedFlag->sendHome();

               updateScore(theShip, CaptureFlag);

               if(theShip->getOwner())
                  theShip->getOwner()->mStatistics.mFlagScore++;

            }
         }
      }
   }
   else     // Touched enemy flag
   {
      // Make sure we don't already have a flag mounted... this will never happen in an ordinary game
      // But you can only carry one flag in CTF
      if(theShip->carryingFlag() == NO_FLAG)
      {
         // Alert the clients
         static StringTableEntry takeString("%e0 took the %e1 flag!");
         Vector<StringTableEntry> e;
         e.push_back(theShip->getName());
         e.push_back(getGame()->getTeamName(theFlag->getTeam()));

         for(S32 i = 0; i < getClientCount(); i++)
            getClient(i)->clientConnection->s2cDisplayMessageE(GameConnection::ColorNuclearGreen, SFXFlagSnatch, takeString, e);

         theFlag->mountToShip(theShip);

         if(theShip->getOwner())
            theShip->getOwner()->mStatistics.mFlagPickup++;
      }
   }
}


class FlagItem;

void CTFGameType::itemDropped(Ship *ship, MoveItem *item)
{
   FlagItem *flag = dynamic_cast<FlagItem *>(item);

   if(flag)
   {
      static StringTableEntry dropString("%e0 dropped the %e1 flag!");

      Vector<StringTableEntry> e;
      e.push_back(ship->getName());
      e.push_back(getGame()->getTeamName(flag->getTeam()));

      for(S32 i = 0; i < getClientCount(); i++)
         getClient(i)->clientConnection->s2cDisplayMessageE(GameConnection::ColorNuclearGreen, SFXFlagDrop, dropString, e);
   }
}


// Identical to HTF, Retrieve
void CTFGameType::performProxyScopeQuery(GameObject *scopeObject, GameConnection *connection)
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


void CTFGameType::renderInterfaceOverlay(bool scoreboardVisible)
{
#ifndef ZAP_DEDICATED
   Parent::renderInterfaceOverlay(scoreboardVisible);

   // Rendering objective arrows makes no sense if there is no ship at the moment...
   Ship *ship = dynamic_cast<Ship *>(dynamic_cast<ClientGame *>(getGame())->getConnectionToServer()->getControlObject());
   if(!ship)
      return;

   for(S32 i = 0; i < mFlags.size(); i++)
   {
      if(!mFlags[i].isValid())
         continue;

      if(mFlags[i]->isMounted())
      {
         Ship *mount = mFlags[i]->getMount();
         if(mount)
            renderObjectiveArrow(mount, getTeamColor(mount->getTeam()));
      }
      else
         renderObjectiveArrow(mFlags[i], getTeamColor(mFlags[i]->getTeam()));
   }
#endif
}


bool CTFGameType::teamHasFlag(S32 teamId) const
{
   for(S32 i = 0; i < mFlags.size(); i++)
      if(mFlags[i] && mFlags[i]->isMounted() && mFlags[i]->getMount() && mFlags[i]->getMount()->getTeam() == (S32)teamId)
         return true;

   return false;
}


// What does a particular scoring event score?
S32 CTFGameType::getEventScore(ScoringGroup scoreGroup, ScoringEvent scoreEvent, S32 data)
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
         case ReturnTeamFlag:
            return 0;
         case CaptureFlag:
            return 1;
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
         case ReturnTeamFlag:
            return 1;
         case CaptureFlag:
            return 5;
         default:
            return naScore;
      }
   }
}


};


