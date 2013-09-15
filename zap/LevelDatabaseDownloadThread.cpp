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

#include "LevelDatabaseDownloadThread.h"
#include "HttpRequest.h"
#include "ClientGame.h"
#include "ServerGame.h"
#include "LevelSource.h"

#include "stringUtils.h"

#include "tnlThread.h"

#include <stdio.h>
#include <iostream>

namespace Zap
{

string LevelDatabaseDownloadThread::LevelRequest = "bitfighter.org/pleiades/levels/raw/%s";
string LevelDatabaseDownloadThread::LevelgenRequest = "bitfighter.org/pleiades/levels/raw/%s/levelgen";

// Constructor
LevelDatabaseDownloadThread::LevelDatabaseDownloadThread(string levelId, ClientGame *game)
   : mLevelId(levelId), 
     mGame(game)
{
   // Do nothing
}


// Destructor
LevelDatabaseDownloadThread::~LevelDatabaseDownloadThread()    
{
   // Do nothing
}


U32 LevelDatabaseDownloadThread::run()
{
   char url[UrlLength];

   string levelFileName = "db_" + mLevelId + ".level";

   FolderManager *fm = mGame->getSettings()->getFolderManager();
   string filePath = joindir(fm->levelDir, levelFileName);

   if(fileExists(filePath))
   {
      mGame->displayErrorMessage("!!! Already have a file called %s on the server.  Download aborted.", filePath.c_str());
      return 0;
   }


   mGame->displaySuccessMessage("Downloading %s", mLevelId.c_str());
   dSprintf(url, UrlLength, LevelRequest.c_str(), mLevelId.c_str());
   HttpRequest req(url);
   
   if(!req.send())
   {
      mGame->displayErrorMessage("!!! Error connecting to server");
      delete this;
      return 0;
   }

   if(req.getResponseCode() != HttpRequest::OK)
   {
      mGame->displayErrorMessage("!!! Server returned an error: %d", req.getResponseCode());
      delete this;
      return 0;
   }

   string levelCode = req.getResponseBody();

   if(writeFile(filePath, levelCode))
   {
      mGame->displaySuccessMessage("Saved to %s", levelFileName.c_str());
      if(gServerGame)
      {
    	  LevelInfo levelInfo;
    	  levelInfo.filename = levelFileName;
        levelInfo.folder = fm->levelDir;

    	  if(gServerGame->populateLevelInfoFromSource(filePath, levelInfo))
        {
    	     gServerGame->addLevel(levelInfo);
           gServerGame->sendLevelListToLevelChangers(string("Level ") + levelInfo.mLevelName.getString() + " added to server");
        }
      }
   }
   else  // File writing went bad
   {
      mGame->displayErrorMessage("!!! Could not write to %s", levelFileName.c_str());
      delete this;
      return 0;
   }

   dSprintf(url, UrlLength, LevelgenRequest.c_str(), mLevelId.c_str());
   req = HttpRequest(url);
   if(!req.send())
   {
      mGame->displayErrorMessage("!!! Error connecting to server");
      delete this;
      return 0;
   }

   if(req.getResponseCode() != HttpRequest::OK)
   {
      mGame->displayErrorMessage("!!! Server returned an error: %d", req.getResponseCode());
      delete this;
      return 0;
   }

   string levelgenCode = req.getResponseBody();

   // no data is sent if the level has no levelgen
   if(levelgenCode.length() > 0)
   {
      // the leveldb prepends a lua comment with the target filename, and here we parse it
      int startIndex = 3; // the length of "-- "
      int breakIndex = levelgenCode.find_first_of("\r\n");
      string levelgenFileName = levelgenCode.substr(startIndex, breakIndex - startIndex);
      // trim the filename line before writing
      levelgenCode = levelgenCode.substr(breakIndex + 2, levelgenCode.length());

      FolderManager *fm = mGame->getSettings()->getFolderManager();
      filePath = joindir(fm->levelDir, levelgenFileName);
      if(writeFile(filePath, levelgenCode))
      {
         mGame->displaySuccessMessage("Saved to %s", levelgenFileName.c_str());
      }
      else
      {
         mGame->displayErrorMessage("!!! Could not write to %s", levelgenFileName.c_str());
      }
   }
   delete this;
   return 0;
}


}

