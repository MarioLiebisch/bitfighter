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

#ifndef _CTFGAME_H_
#define _CTFGAME_H_

#include "gameType.h"
#include "item.h"

namespace Zap
{

class Ship;
class FlagItem;

class CTFGameType : public GameType
{
private:
   typedef GameType Parent;

public:
   void addFlag(FlagItem *flag);
   void shipTouchFlag(Ship *ship, FlagItem *flag);
   void itemDropped(Ship *ship, MoveItem *item);
   void performProxyScopeQuery(GameObject *scopeObject, ClientInfo *clientInfo);
   void renderInterfaceOverlay(bool scoreboardVisible);
   bool teamHasFlag(S32 teamId) const;

   GameTypes getGameType() const { return CTFGame; }
   const char *getGameTypeString() const { return "Capture the Flag"; }
   const char *getShortName() const { return "CTF"; }
   const char *getInstructionString() { return "Take the opposing team's flag and touch it to your flag!"; }
   
   bool isFlagGame() { return true; }
   bool isTeamGame() { return true; }
   bool canBeTeamGame()  const { return true; }
   bool canBeIndividualGame() const { return false; }
   bool isTeamFlagGame() { return true; }    // Teams matter with our flags in this game

   S32 getEventScore(ScoringGroup scoreGroup, ScoringEvent scoreEvent, S32 data);

   TNL_DECLARE_CLASS(CTFGameType);
};

};


#endif

