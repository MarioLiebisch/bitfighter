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

#ifndef _RETRIEVEGAMETYPE_H_
#define _RETRIEVEGAMETYPE_H_

#include "gameType.h"


namespace Zap
{

class RetrieveGameType : public GameType
{
private:
   typedef GameType Parent;

protected:
   Vector<GoalZone *> mZones;

public:
   // Server only
   void addFlag(FlagItem *flag);


   void addZone(GoalZone *zone);


   bool isFlagGame() { return true; }


   // Note -- neutral or enemy-to-all robots can't pick up the flag!!!
   void shipTouchFlag(Ship *theShip, FlagItem *theFlag);


   void itemDropped(Ship *ship, MoveItem *item);


   // The ship has entered a drop zone, either friend or foe
   void shipTouchZone(Ship *s, GoalZone *z);


   // A major scoring event has ocurred -- in this case, it's all flags being collected by one team
   void majorScoringEventOcurred(S32 team);

   // Same code as in HTF, CTF
   void performProxyScopeQuery(GameObject *scopeObject, ClientInfo *clientInfo);


   // Runs on client
   void renderInterfaceOverlay(bool scoreboardVisible);


   // What does a particular scoring event score?
   S32 getEventScore(ScoringGroup scoreGroup, ScoringEvent scoreEvent, S32 data);

   bool isTeamFlagGame() { return true; }

   GameTypes getGameType() { return RetrieveGame; }
   const char *getGameTypeString() const { return "Retrieve"; }
   const char *getShortName() const { return "Ret"; }
   const char *getInstructionString() { return "Find all the flags, and bring them to your capture zones!"; }
   bool isTeamGame() { return true; }
   bool canBeTeamGame() { return true; }
   bool canBeIndividualGame() { return false; }


   TNL_DECLARE_CLASS(RetrieveGameType);
};


};

#endif