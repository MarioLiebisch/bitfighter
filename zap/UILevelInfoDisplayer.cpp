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

#include "UILevelInfoDisplayer.h"   // Header

#include "ScreenInfo.h"
#include "Colors.h"
#include "game.h"
#include "ClientGame.h"
#include "UI.h"                     // Only here for the margins
#include "GameTypesEnum.h"
#include "gameType.h"               // Can get rid of with a bunch of passthroughs
#include "FontManager.h"
#include "SymbolShape.h"

#include "stringUtils.h"
#include "OpenglUtils.h"               
#include "RenderUtils.h"


namespace Zap { 
   
extern ScreenInfo gScreenInfo;

namespace UI {

// Constructor
LevelInfoDisplayer::LevelInfoDisplayer()
{
   // Do nothing
}

// Destructor
LevelInfoDisplayer::~LevelInfoDisplayer()
{
   // Do nothing
}


void LevelInfoDisplayer::resetDisplayTimer()
{
   mDisplayTimer.reset(6 * 1000);
}


void LevelInfoDisplayer::idle(U32 timeDelta)
{
   Parent::idle(timeDelta);
   if(mDisplayTimer.update(timeDelta))
      onDeactivated();
}


void LevelInfoDisplayer::clearDisplayTimer()
{
   mDisplayTimer.clear();
}


void LevelInfoDisplayer::render(const GameType *gameType, S32 teamCount, bool isInDatabase) const
{
   FontManager::pushFontContext(LevelInfoContext);

   glPushMatrix();
   glTranslate(0, getInsideEdge(), 0);

    // Only render these when they are not empty
   bool showCredits = gameType->getLevelCredits()->isNotNull();    
   bool showDescr   = gameType->getLevelDescription()->isNotNull();

   const char *title   = gameType->getLevelName()->isNotNull() ? gameType->getLevelName()->getString() : "Unnamed Level";
   const S32 titleSize = 30;
   const S32 titleGap  = 10;

   Vector<SymbolShapePtr> symbols;
   symbols.push_back(SymbolString::getSymbolText(title, titleSize, LevelInfoContext));

   // Find the unicode in Character Map or similar utility,
   // then convert it here: http://www.ltg.ed.ac.uk/~richard/utf-8.html
   // Use Hex UTF-8 bytes, represent in string like this: \xE2\x99\xA6

   if(isInDatabase)
   {
      ClientGame::PersonalRating myRating = static_cast<ClientGame *>(gameType->getGame())->getPersonalLevelRating();
      S16 totalRating                     = static_cast<ClientGame *>(gameType->getGame())->getTotalLevelRating();

      symbols.push_back(SymbolString::getBlankSymbol(10));
      symbols.push_back(SymbolString::getSymbolText("\xEF\x80\x8B", 15, WebDingContext));  // Little database icon

      if(myRating != ClientGame::UnknownRating && totalRating != ClientGame::UnknownRating)
      {
         string myRatingStr    = ClientGame::getRatingString(myRating, true);
         string totalRatingStr = ClientGame::getRatingString(totalRating, false);

         symbols.push_back(SymbolString::getBlankSymbol(8));                                  // Padding
         SymbolString::symbolParse(NULL, myRatingStr + " / " + totalRatingStr, symbols, LevelInfoContext, 12, &Colors::red);
      }
   }

   SymbolString titleSymbolString(symbols);


   const char *descr           = gameType->getLevelDescription()->getString();
   const S32 descriptionSize   = 20;
   const S32 descriptionHeight = showDescr ? descriptionSize + 8 : 0;

   const char *designedBy      = "Designed By:";
   const char *credits         = gameType->getLevelCredits()->getString();
   const S32 creditsSize       = 20;
   const S32 creditsHeight     = showCredits ? creditsSize + 8 : 0;
   
   const S32 frameMargin       = UserInterface::vertMargin;

   const S32 totalHeight = frameMargin + titleSize + titleGap + descriptionHeight + creditsHeight + frameMargin;
   //const S32 totalWidth = max(getStringWidth(titleSize, title), 
   //                           max(getStringWidth(descriptionSize, descr), 
   //                              max(getStringPairWidth(creditsSize, designedBy, credits), 400))) +
   //                       frameMargin * 2;

   const S32 totalWidth = gScreenInfo.getGameCanvasWidth() - 60;
   S32 yPos = frameMargin + titleSize;

   // Draw top info box
   renderSlideoutWidgetFrame((gScreenInfo.getGameCanvasWidth() - totalWidth) / 2, 0, totalWidth, totalHeight, Colors::blue);

   glColor(Colors::white);
   titleSymbolString.render(gScreenInfo.getGameCanvasWidth() / 2, yPos, AlignmentCenter);

   yPos += titleGap;

   if(showDescr)
   {
      glColor(Colors::magenta);
      drawCenteredString(yPos, descriptionSize, descr);
      yPos += descriptionHeight;
   }

   if(showCredits)
   {
      drawCenteredStringPair(yPos, creditsSize, Colors::cyan, Colors::red, designedBy, credits);
      yPos += creditsHeight;
   }

   glPopMatrix();

   /////
   // Auxilliary side panel

   glPushMatrix();
   glTranslate(-getInsideEdge(), 0, 0);

   bool showTwoLinesOfInstructions = gameType->getInstructionString()[1];     // Show 'em if we got 'em

   const S32 sideBoxY = 275;     // Top edge of side box
   const S32 sideMargin = UserInterface::horizMargin;

   const S32 gameTypeTextSize  = 20;
   const S32 gameTypeMargin    =  8;
   const S32 gameTypeHeight    = gameTypeTextSize + gameTypeMargin;

   const S32 instructionSize   = 13;
   const S32 instructionMargin =  8;
   const S32 instructionHeight = instructionSize + instructionMargin;

   const S32 scoreToWinSize    = 20;
   const S32 scoreToWinMargin  =  8;
   const S32 scoreToWinHeight  = scoreToWinSize  + scoreToWinMargin;

   const S32 sideBoxTotalHeight = frameMargin + gameTypeHeight + instructionHeight * (showTwoLinesOfInstructions ? 2 : 1) + 
                                  scoreToWinHeight + frameMargin;

   const S32 instrWidth = max(getStringWidth(instructionSize, gameType->getInstructionString()[0]), 
                              showTwoLinesOfInstructions ? getStringWidth(instructionSize, gameType->getInstructionString()[1]) : 0); 
      
   // Prefix game type with "Team" if they are typically individual games, but are being played in team mode
   bool team = gameType->canBeIndividualGame() && gameType->getGameTypeId() != SoccerGame && teamCount > 1;
   string gt  = string(team ? "Team " : "") + gameType->getGameTypeName();
   string sgt = string("[") + gameType->getShortName() + "]";

   static const char *scoreToWinStr = "Score to Win:";
   const S32 scoreToWinWidth = getStringWidthf(scoreToWinSize, "%s%d", scoreToWinStr, gameType->getWinningScore()) + 5;
   const S32 sideBoxWidth    = max(instrWidth, max(getStringPairWidth(gameTypeTextSize, gt.c_str(), sgt.c_str()), scoreToWinWidth)) + 
                               sideMargin * 2;
   const S32 sideBoxCen      = gScreenInfo.getGameCanvasWidth() - sideBoxWidth / 2;
   
   renderSlideoutWidgetFrame(gScreenInfo.getGameCanvasWidth() - sideBoxWidth, sideBoxY, sideBoxWidth, sideBoxTotalHeight, Colors::blue);

   yPos = sideBoxY + frameMargin;

   drawCenteredStringPair(sideBoxCen, yPos, gameTypeTextSize, Colors::white, Colors::cyan, gt.c_str(), sgt.c_str());
   yPos += gameTypeHeight;

   glColor(Colors::yellow);
   drawCenteredString(sideBoxCen, yPos, instructionSize, gameType->getInstructionString()[0]);
   yPos += instructionHeight;

   // Add a second line of instructions if there is one...
   if(showTwoLinesOfInstructions)
   {
      drawCenteredString(sideBoxCen, yPos, instructionSize, gameType->getInstructionString()[1]);
      yPos += instructionHeight;
   }

   drawCenteredStringPair(sideBoxCen, yPos, scoreToWinSize, Colors::cyan, Colors::red, 
                  scoreToWinStr, itos(gameType->getWinningScore()).c_str());
   yPos += scoreToWinHeight;

   glPopMatrix();

   FontManager::popFontContext();
}


bool LevelInfoDisplayer::isActive() const
{
   return Parent::isActive() || mDisplayTimer.getCurrent() > 0;
}


bool LevelInfoDisplayer::isDisplayTimerActive() const
{
   return mDisplayTimer.getCurrent() > 0;
}



} } // Nested namespace
