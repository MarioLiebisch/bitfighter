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

#include "../zap/SharedConstants.h"

#include "master.h"
#include "masterInterface.h"
#include "authenticator.h"    // For authenticating users against the PHPBB3 database
#include "database.h"         // For writing to the database

#include "tnlNetInterface.h"
#include "tnlVector.h"
#include "tnlAsymmetricKey.h"
#include "tnlThread.h"

#include <stdio.h>
#include <string>
#include <stdarg.h>     // For va_args
#include <time.h>
#include <map>

#include "../zap/stringUtils.h"     // For itos, replaceString

#include "../zap/version.h"  // for MASTER_PROTOCOL_VERSION - in case we ever forget to update master...

#include "../zap/IniFile.h"  // For INI reading/writing

using namespace TNL;
using namespace std;
using namespace Zap;

NetInterface *gNetInterface = NULL;

map <U32, string> gMOTDClientMap;

U32 gLatestReleasedCSProtocol = 0; // Will be updated with value from cfg file
U32 gLatestReleasedBuildVersion = 0;

string gMasterName;                // Name of the master server
string gJasonOutFile;              // File where JSON data gets dumped
bool gNeedToWriteStatus = true;    // Tracks whether we need to update our status file, for possible display on a website
U32 gNeedToWriteStatusDelayed = 0; // Delayed update JSON status file

// Variables for managing access to MySQL
string gMySqlAddress;
string gDbUsername;
string gDbPassword;

U32 gServerStartTime;

// Variables for verifying usernames/passwords in PHPBB3
string gPhpbb3Database;
string gPhpbb3TablePrefix;

// Variables for writing stats
string gStatsDatabaseAddress;
string gStatsDatabaseName;
string gStatsDatabaseUsername;
string gStatsDatabasePassword;

Vector<string> master_admins;

Vector<Address> gListAddressHide;

bool gWriteStatsToMySql;

CIniFile gMasterINI("dummy");

HighScores MasterServerConnection::highScores;


static bool isControlCharacter(char ch)
{
   return ch > 0 && ch <= 0x1F;
}


static bool containsControlCharacter( const char* str )
{
   while ( *str )
      if ( isControlCharacter( *(str++) ) )
         return true;

   return false;
}


// Sanitize strings before inclusion into JSON
static const char *sanitizeForJson(const char *value)
{
   unsigned maxsize = strlen(value)*2 + 3; // allescaped+quotes+NULL
   std::string result;
   result.reserve(maxsize);  // memory management

   // Return if no escaping needed
   if(strpbrk(value, "\"\\\b\f\n\r\t<>&") == NULL && !containsControlCharacter(value))
      return value;

   // If any of the above exist then do some escaping
   for(const char* c=value; *c != 0; ++c)
   {
      switch(*c)
      {
         // For JSON
         case '\"':
            result += "\\\"";
            break;
         case '\\':
            result += "\\\\";
            break;
         case '\b':
            result += "\\b";
            break;
         case '\f':
            result += "\\f";
            break;
         case '\n':
            result += "\\n";
            break;
         case '\r':
            result += "\\r";
            break;
         case '\t':
            result += "\\t";
            break;

         // For html markup entities
         case '&':
            result += "&amp;";
            break;
         case '<':
            result += "&lt;";
            break;
         case '>':
            result += "&gt;";
            break;
         default:
            if( isControlCharacter( *c ) )
            {
               // Do nothing for the moment -- there shouldn't be any control chars here, and if there are we don't really care.
               // However, some day we might want to support this, so we'll leave the code in place.
               //std::ostringstream oss;
               //oss << "\\u" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << static_cast<int>(*c);
               //result += oss.str();
            }
            else
               result += *c;

            break;
         }
   }

   return result.c_str();
}


// TODO: Should we be reusing these?
DatabaseWriter getDatabaseWriter()
{
   if(gWriteStatsToMySql)
      return DatabaseWriter(gStatsDatabaseAddress.c_str(),  gStatsDatabaseName.c_str(), 
                            gStatsDatabaseUsername.c_str(), gStatsDatabasePassword.c_str());
   else
      return DatabaseWriter("stats.db");
}



class DatabaseAccessThread : public TNL::Thread
{
public:
   class BasicEntry : public RefPtrData
   {
   public:
      virtual void run() = 0;  // runs on seperate thread
      virtual void finish() {}; // finishes the entry on primary thread after "run()" is done to avoid 2 threads crashing in to the same network TNL and others.
   };
private:
   U32 mEntryStart;
   U32 mEntryThread;
   U32 mEntryEnd;
   static const U32 mEntrySize = 32;

   RefPtr<BasicEntry> mEntry[mEntrySize];

public:
   DatabaseAccessThread() // Constructor
   {
      mEntryStart = 0;
      mEntryThread = 0;
      mEntryEnd = 0;
   }

   void addEntry(BasicEntry *entry)
   {
      U32 entryEnd = mEntryEnd + 1;
      if(entryEnd >= mEntrySize)
         entryEnd = 0;
      if(entryEnd == mEntryStart) // too many entries
      {
         logprintf(LogConsumer::LogError, "Database thread overloaded - database access too slow?");
         return;
      }
      mEntry[mEntryEnd] = entry;
      mEntryEnd = entryEnd;
   }

   U32 run()
   {
      while(true)
      {
         Platform::sleep(50);
         while(mEntryThread != mEntryEnd)
         {
            mEntry[mEntryThread]->run();
            mEntryThread++;
            if(mEntryThread >= mEntrySize)
               mEntryThread = 0;
         }
      }
      return 0;
   }
   void runAtMainThread()
   {
      while(mEntryStart != mEntryThread)
      {
         mEntry[mEntryStart]->finish();
         mEntry[mEntryStart].set(NULL);  // RefPtr, we can just set to NULL and it will delete itself.
         mEntryStart++;
         if(mEntryStart >= mEntrySize)
            mEntryStart = 0;
      }
   }
} gDatabaseAccessThread;





   /// Constructor initializes the linked list info with "safe" values
   /// so we don't explode if we destruct right away.
   MasterServerConnection::MasterServerConnection()
   {
      mStrikeCount = 0; 
      mLastActivityTime = 0;
      mNext = this;
      mPrev = this;
      setIsConnectionToClient();
      setIsAdaptive();
      isInGlobalChat = false;
      mAuthenticated = false;
      mIsDebugClient = false;
      mIsIgnoredFromList = false;
      mIsMasterAdmin = false;
      mLoggingStatus = "Not_Connected";
      mConnectionType = MasterConnectionTypeNone;
   }

   /// Destructor removes the connection from the doubly linked list of server connections
   MasterServerConnection::~MasterServerConnection()
   {
      // Unlink it if it's in the list
      mPrev->mNext = mNext;
      mNext->mPrev = mPrev;

      if(mLoggingStatus != "")
      {
         logprintf(LogConsumer::LogConnection, "CONNECT_FAILED\t%s\t%s\t%s", getTimeStamp().c_str(), getNetAddress().toString(), mLoggingStatus.c_str());
      }
      else if(mConnectionType == MasterConnectionTypeServer)
      {
         // SERVER_DISCONNECT | timestamp | player/server name
         logprintf(LogConsumer::LogConnection, "SERVER_DISCONNECT\t%s\t%s", getTimeStamp().c_str(), mPlayerOrServerName.getString());
      }
      else if(mConnectionType == MasterConnectionTypeClient)
      {
         // CLIENT_DISCONNECT | timestamp | player name
         logprintf(LogConsumer::LogConnection, "CLIENT_DISCONNECT\t%s\t%s", getTimeStamp().c_str(), mPlayerOrServerName.getString());
      }

      if(isInGlobalChat)
         for(MasterServerConnection *walk = gClientList.mNext; walk != &gClientList; walk = walk->mNext)
            if(walk->isInGlobalChat)
               walk->m2cPlayerLeftGlobalChat(mPlayerOrServerName);

      gNeedToWriteStatus = true;
   }

   /// Adds this connection to the doubly linked list of servers
   void MasterServerConnection::linkToServerList()
   {
      mNext = gServerList.mNext;
      mPrev = gServerList.mNext->mPrev;
      mNext->mPrev = this;
      mPrev->mNext = this;

      gNeedToWriteStatus = true;

      // SERVER_CONNECT | timestamp | server name | server description
      logprintf(LogConsumer::LogConnection, "SERVER_CONNECT\t%s\t%s\t%s", getTimeStamp().c_str(), mPlayerOrServerName.getString(), mServerDescr.getString());
   }


   void MasterServerConnection::linkToClientList()
   {
      mNext = gClientList.mNext;
      mPrev = gClientList.mNext->mPrev;
      mNext->mPrev = this;
      mPrev->mNext = this;

      gNeedToWriteStatus = true;

      // CLIENT_CONNECT | timestamp | player name
      logprintf(LogConsumer::LogConnection, "CLIENT_CONNECT\t%s\t%s", getTimeStamp().c_str(), mPlayerOrServerName.getString());
   }


   // Check username & password against database
   MasterServerConnection::PHPBB3AuthenticationStatus MasterServerConnection::verifyCredentials(string &username, string password)

#ifdef VERIFY_PHPBB3    // Defined in Linux Makefile, not in VC++ project
   {
      Authenticator authenticator;

      // Security levels: 0 = no security (no checking for sql-injection attempts, not recommended unless you add your own security)
      //          1 = basic security (prevents the use of any of these characters in the username: "(\"*^';&></) " including the space)
      //        1 = very basic security (prevents the use of double quote character)
      //          2 = alphanumeric (only allows alphanumeric characters in the username)
      //
      // We'll use level 1 for now, so users can put special characters in their username
      authenticator.initialize(gMySqlAddress, gDbUsername, gDbPassword, gPhpbb3Database, gPhpbb3TablePrefix, 1);

      S32 errorcode;
      if(authenticator.authenticate(username, password, errorcode))   // returns true if the username was found and the password is correct
         return Authenticated;
      else
      {
         switch(errorcode)
         {
            case 0:  return CantConnect;
            case 1:  return UnknownUser;
            case 2:  return WrongPassword;
            case 3:  return InvalidUsername;
            default: return UnknownStatus;
         }
      }
   }
#else // verifyCredentials
   {
      return Unsupported;
   }
#endif


   struct Auth_Stats : public DatabaseAccessThread::BasicEntry
   {
      SafePtr<MasterServerConnection> client;
      Int<BADGE_COUNT> badges;
      U16 gamesPlayed;
      MasterServerConnection::PHPBB3AuthenticationStatus stat;
      string playerName;
      char password[256];

      void run()
      {
         stat = MasterServerConnection::verifyCredentials(playerName, password);
         if(stat == MasterServerConnection::Authenticated)
         {
            DatabaseWriter databaseWriter = getDatabaseWriter();
            badges = databaseWriter.getAchievements(playerName.c_str());
            gamesPlayed = databaseWriter.getGamesPlayed(playerName.c_str());
         }
         else
         {
            badges = NO_BADGES;
            gamesPlayed = 0;
         }
      }
      void finish()
      {
         StringTableEntry playerNameSTE(playerName.c_str());
         if(client) // Check for NULL, Sometimes, a client disconnects very fast
            client->processAutentication(playerNameSTE, stat, badges, gamesPlayed);
      }
   };
   MasterServerConnection::PHPBB3AuthenticationStatus MasterServerConnection::checkAuthentication(const char *password, bool doNotDelay)
   {
      // Don't let username start with spaces or be zero length.
      if(mPlayerOrServerName.getString()[0] == ' ' || mPlayerOrServerName.getString()[0] == 0)
         return InvalidUsername;

      RefPtr<Auth_Stats> auth = new Auth_Stats();
      auth->client = this;
      auth->playerName = mPlayerOrServerName.getString();
      strncpy(auth->password, password, sizeof(auth->password));
      auth->stat = UnknownStatus;
      gDatabaseAccessThread.addEntry(auth);
      if(doNotDelay)  // Wait up to 1000 milliseconds so we can return some value, for clients version 017 and older
      {
         U32 timer = Platform::getRealMilliseconds();
         while(Platform::getRealMilliseconds() - timer < 1000 && auth->stat == UnknownStatus)
         {
            Platform::sleep(5);
         }
         return auth->stat;
      }
      return UnknownStatus;
   }


   void MasterServerConnection::processAutentication(StringTableEntry newName, PHPBB3AuthenticationStatus stat, 
                                                     TNL::Int<32> badges, U16 gamesPlayed)
   {
      mBadges = NO_BADGES;
      if(stat == WrongPassword)
      {
         logprintf(LogConsumer::LogConnection, "User %s provided the wrong password", mPlayerOrServerName.getString());
         disconnect(ReasonBadLogin, "");
      }
      else if(stat == InvalidUsername)
      {
         logprintf(LogConsumer::LogConnection, "User name %s contains illegal characters", mPlayerOrServerName.getString());
         // Send message back to client to request new username/password
         disconnect(ReasonInvalidUsername, "");
      }
      else if(stat == Authenticated)
      {
         mIsIgnoredFromList = false;  // just for authenticating..
         logprintf(LogConsumer::LogConnection, "Authenticated user %s", mPlayerOrServerName.getString());
         mAuthenticated = true;
         gNeedToWriteStatus = true;  // Make sure JSON show as Authenticated  = true

         if(mPlayerOrServerName != newName)
         {
            if(isInGlobalChat)  // Need to tell clients new name, in case of delayed authentication
               for(MasterServerConnection *walk = gClientList.mNext; walk != &gClientList; walk = walk->mNext)
                  if (walk != this && walk->isInGlobalChat)
                  {
                     walk->m2cPlayerLeftGlobalChat(mPlayerOrServerName);
                     walk->m2cPlayerJoinedGlobalChat(newName);
                  }
            mPlayerOrServerName = newName;
         }

         mBadges = badges;
         if(mCMProtocolVersion <= 6)      // 018a ==> 6, 019 ==> 7
            m2cSetAuthenticated((U32)AuthenticationStatusAuthenticatedName, badges, newName.getString());
         else
            m2cSetAuthenticated_019((U32)AuthenticationStatusAuthenticatedName, badges, gamesPlayed, newName.getString());

         for(S32 i=0; i < master_admins.size(); i++)  // check for master admin
            if(newName == master_admins[i])
            {
               mIsMasterAdmin = true;
               break;
            }
      }
      else if(stat == UnknownUser || stat == Unsupported)
      {
         if(mCMProtocolVersion <= 6)
            m2cSetAuthenticated(AuthenticationStatusUnauthenticatedName, NO_BADGES, "");
         else
            m2cSetAuthenticated_019(AuthenticationStatusUnauthenticatedName, NO_BADGES, 0, "");
      }

      else  // stat == CantConnect || stat == UnknownStatus
      {
         if(mCMProtocolVersion <= 6)
            m2cSetAuthenticated(AuthenticationStatusFailed, NO_BADGES, "");
         else
            m2cSetAuthenticated_019(AuthenticationStatusFailed, NO_BADGES, 0, "");
      }
   }



   // Client has contacted us and requested a list of active servers
   // that match their criteria.
   //
   // The query server method builds a piecewise list of servers
   // that match the client's particular filter criteria and
   // sends it to the client, followed by a QueryServersDone RPC.
   TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, c2mQueryServers, (U32 queryId))
   {
      Vector<IPAddress> theVector(IP_MESSAGE_ADDRESS_COUNT);
      theVector.reserve(IP_MESSAGE_ADDRESS_COUNT);

      for(MasterServerConnection *walk = gServerList.mNext; walk != &gServerList; walk = walk->mNext)
      {
         if(walk->mIsIgnoredFromList)  // hide hidden servers...
            continue;

         // First check the version -- we only want to match potential players that agree on which protocol to use
         if(walk->mCSProtocolVersion != mCSProtocolVersion)     // Incomptible protocols
            continue;

         //// Ok, so the protocol version is correct, however...
         //if(walk->mPlayerCount > maxPlayers || walk->mPlayerCount < minPlayers)   // ...too few or too many players
         //   continue;
         //if(infoFlags & ~walk->mInfoFlags)           // ...wrong info flags
         //   continue;
         //if(maxBots < walk->mNumBots)                // ...too many bots
         //   continue;
         //if(gameType.isNotNull() && (gameType != walk->mLevelName))          // ...wrong level name
         //   continue;
         //if(missionType.isNotNull() && (missionType != walk->mLevelType))    // ...wrong level type
         //   continue;

         // Somehow, despite it all, we matched.  Add us to the results list.
         theVector.push_back(walk->getNetAddress().toIPAddress());

         // If we get a packet's worth, send it to the client and empty our buffer...
         if(theVector.size() == IP_MESSAGE_ADDRESS_COUNT)
         {
            m2cQueryServersResponse(queryId, theVector);
            theVector.clear();
         }
      }

      // Send the final packet
      m2cQueryServersResponse(queryId, theVector);
      // If we sent any with the previous message, send another list with no servers.
      if(theVector.size())
      {
         theVector.clear();
         m2cQueryServersResponse(queryId, theVector);
      }
   }

   /// checkActivityTime validates that this particular connection is
   /// not issuing too many requests at once in an attempt to DOS
   /// by flooding either the master server or any other server
   /// connected to it.  A client whose last activity time falls
   /// within the specified delta gets a strike... 3 strikes and
   /// you're out!  Strikes go away after being good for a while.
   bool MasterServerConnection::checkActivityTime(U32 timeDeltaMinimum)
   {
      U32 currentTime = Platform::getRealMilliseconds();
      if(currentTime - mLastActivityTime < timeDeltaMinimum)
      {
         mStrikeCount++;
         if(mStrikeCount == 3)
         {
            logprintf(LogConsumer::LogConnection, "User %s Disconnect due to flood control set at %i milliseconds", 
                                                  mPlayerOrServerName.getString(), timeDeltaMinimum);
            disconnect(ReasonFloodControl, "");
            return false;
         }
      }
      else if(mStrikeCount > 0)
         mStrikeCount--;

      mLastActivityTime = currentTime;
      return true;
   }

   void MasterServerConnection::removeConnectRequest(GameConnectRequest *gcr)
   {
      for(S32 j = 0; j < mConnectList.size(); j++)
      {
         if(gcr == mConnectList[j])
         {
            mConnectList.erase_fast(j);
            break;
         }
      }
   }

   GameConnectRequest *MasterServerConnection::findAndRemoveRequest(U32 requestId)
   {
      GameConnectRequest *req = NULL;
      for(S32 j = 0; j < mConnectList.size(); j++)
      {
         if(mConnectList[j]->hostQueryId == requestId)
         {
            req = mConnectList[j];
            mConnectList.erase_fast(j);
            break;
         }
      }
      if(!req)
         return NULL;

      if(req->initiator.isValid())
         req->initiator->removeConnectRequest(req);
      for(S32 j = 0; j < gConnectList.size(); j++)
      {
         if(gConnectList[j] == req)
         {
            gConnectList.erase_fast(j);
            break;
         }
      }
      return req;
   }


   MasterServerConnection *MasterServerConnection::findClient(Nonce &clientId)   // Should be const, but that won't compile for reasons not yet determined!!
   {
      if(!clientId.isValid())
         return NULL;

      for(MasterServerConnection *walk = gClientList.mNext; walk != &gClientList; walk = walk->mNext)
         if(walk->mPlayerId == clientId)
            return walk;

      return NULL;
   }


   static bool listClient(MasterServerConnection *client)
   {
      if(client->mIsIgnoredFromList)     // Skip clients with ignored flag set
         return false;

      if(client->mIsDebugClient)         // Skip clients running in debug mode
         return false;

      return true;
   }


   // Write a current count of clients/servers for display on a website, using JSON format
   // This gets updated whenever we gain or lose a server, at most every REWRITE_TIME ms
   void MasterServerConnection::writeClientServerList_JSON()
   {
      if(gJasonOutFile == "")
         return;

      bool first = true;
      S32 playerCount = 0;
      S32 serverCount = 0;

      FILE *f = fopen(gJasonOutFile.c_str(), "w");
      if(f)
      {
         // First the servers
         fprintf(f, "{\n\t\"servers\": [");

            for(MasterServerConnection *walk = gServerList.mNext; walk != &gServerList; walk = walk->mNext)
            {
               if(walk->mIsIgnoredFromList)
                  continue;

               fprintf(f, "%s\n\t\t{\n\t\t\t\"serverName\": \"%s\",\n\t\t\t\"protocolVersion\": %d,\n\t\t\t\"currentLevelName\": \"%s\",\n\t\t\t\"currentLevelType\": \"%s\",\n\t\t\t\"playerCount\": %d\n\t\t}",
                          first ? "" : ", ", sanitizeForJson(walk->mPlayerOrServerName.getString()), 
                          walk->mCSProtocolVersion, walk->mLevelName.getString(), walk->mLevelType.getString(), walk->mPlayerCount);
               playerCount +=  walk->mPlayerCount;
               serverCount++;
               first = false;
            }

         // Next the player names      // "players": [ "chris", "colin", "fred", "george", "Peter99" ],
         fprintf(f, "\n\t],\n\t\"players\": [");
         first = true;
         for(MasterServerConnection *walk = gClientList.mNext; walk != &gClientList; walk = walk->mNext)
         {
            if(listClient(walk))
            {
               fprintf(f, "%s\"%s\"", first ? "":", ", sanitizeForJson(walk->mPlayerOrServerName.getString()));
               first = false;
            }
         }

         // Authentication status      // "authenticated": [ true, false, false, true, true ],
         fprintf(f, "],\n\t\"authenticated\": [");
         first = true;
         for(MasterServerConnection *walk = gClientList.mNext; walk != &gClientList; walk = walk->mNext)
         {
            if(listClient(walk))
            {
               fprintf(f, "%s%s", first ? "":", ", walk->mAuthenticated ? "true" : "false");
               first = false;
            }
         }

         // Finally, the player and server counts
         fprintf(f, "],\n\t\"serverCount\": %d,\n\t\"playerCount\": %d\n}\n", serverCount, playerCount);

         fflush(f);
         fclose(f);
      }
      else
         logprintf(LogConsumer::LogError, "Could not write to JSON file \"%s\"", gJasonOutFile.c_str());
   }

   /*  Resulting JSON data should look like this:
   {
      "servers": [
         {
            "serverName": "Spang",
            "protocolVersion": 23,
            "currentLevelName": "Triple Threat",
            "currentLevelType": "CTF",
            "playerCount": 2
         },
         {
            "serverName": "Ramst",
            "protocolVersion": 23,
            "currentLevelName": "Kaptor",
            "currentLevelType": "Hunters",
            "playerCount": 3
         }
      ],
      "players": [ "chris", "colin", "fred", "george", "Peter99" ],
      "authenticated": [ true, false, false, true, true ],
      "serverCount": 2,
      "playerCount": 5
   }
   */

   // This is called when a client wishes to arrange a connection with a server
   TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, c2mRequestArrangedConnection, (U32 requestId, IPAddress remoteAddress, IPAddress internalAddress,
                                                           ByteBufferPtr connectionParameters) )
   {
      // First, make sure that we're connected with the server that they're requesting a connection with
      MasterServerConnection *conn = (MasterServerConnection *) gNetInterface->findConnection(remoteAddress);
      if(!conn)
      {
         ByteBufferPtr ptr = new ByteBuffer((U8 *) MasterNoSuchHost, (U32) strlen(MasterNoSuchHost) + 1);
         m2cArrangedConnectionRejected(requestId, ptr);
         return;
      }

      // Record the request...
      GameConnectRequest *req = new GameConnectRequest;
      req->initiator = this;
      req->host = conn;
      req->initiatorQueryId = requestId;
      req->hostQueryId = mLastQueryId++;
      req->requestTime = Platform::getRealMilliseconds();

 
      logprintf(LogConsumer::LogConnectionManager, "Client: %s requested connection to %s", 
                                                        getNetAddress().toString(), conn->getNetAddress().toString());

      // Add the request to the relevant lists (the global list, this connection's list,
      // and the other connection's list).
      mConnectList.push_back(req);
      conn->mConnectList.push_back(req);
      gConnectList.push_back(req);

      // Do some DOS checking...
      checkActivityTime(2000);      // 2 secs

      // Get our address...
      Address theAddress = getNetAddress();

      // Record some different addresses to try...
      Vector<IPAddress> possibleAddresses;

      // The address, but port+1
      theAddress.port++;
      possibleAddresses.push_back(theAddress.toIPAddress());

      // The address, with the original port
      theAddress.port--;
      possibleAddresses.push_back(theAddress.toIPAddress());

      // Or the address the port thinks it's talking to.
      // (That is, if it's not the any address.)
      Address theInternalAddress(internalAddress);
      Address anyAddress;
      if(!theInternalAddress.isEqualAddress(anyAddress) && theInternalAddress != theAddress)
         possibleAddresses.push_back(internalAddress);

      conn->m2sClientRequestedArrangedConnection(req->hostQueryId, possibleAddresses, connectionParameters);
   }


   /// s2mAcceptArrangedConnection is sent by a server to notify the master that it will accept the connection
   /// request from a client.  The requestId parameter sent by the MasterServer in m2sClientRequestedArrangedConnection
   /// should be sent back as the requestId field.  The internalAddress is the server's self-determined IP address.

   // Called to indicate a connect request is being accepted.
   TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, s2mAcceptArrangedConnection, (U32 requestId, IPAddress internalAddress, ByteBufferPtr connectionData))
   {
      GameConnectRequest *req = findAndRemoveRequest(requestId);
      if(!req)
         return;

      Address theAddress = getNetAddress();
      Vector<IPAddress> possibleAddresses;

      theAddress.port++;
      possibleAddresses.push_back(theAddress.toIPAddress());

      theAddress.port--;
      possibleAddresses.push_back(theAddress.toIPAddress());

      Address theInternalAddress(internalAddress);
      Address anyAddress;
      if(!theInternalAddress.isEqualAddress(anyAddress) && theInternalAddress != theAddress)
         possibleAddresses.push_back(internalAddress);


      char buffer[256];
      strcpy(buffer, getNetAddress().toString());

      logprintf(LogConsumer::LogConnectionManager, "[%s] Server: %s accepted connection request from %s", getTimeStamp().c_str(), buffer, 
                                                        req->initiator.isValid() ? req->initiator->getNetAddress().toString() :        
                                                                                   "Unknown");

      // If we still know about the requestor, tell him his connection was accepted...
      if(req->initiator.isValid())
         req->initiator->m2cArrangedConnectionAccepted(req->initiatorQueryId, possibleAddresses, connectionData);

      delete req;
   }



   // s2mRejectArrangedConnection notifies the Master Server that the server is rejecting the arranged connection
   // request specified by the requestId.  The rejectData will be passed along to the requesting client.
   // Called to indicate a connect request is being rejected.
   TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, s2mRejectArrangedConnection, (U32 requestId, ByteBufferPtr rejectData))
   {
      GameConnectRequest *req = findAndRemoveRequest(requestId);
      if(!req)
         return;

      logprintf(LogConsumer::LogConnectionManager, "[%s] Server: %s reject connection request from %s",
                                                        getTimeStamp().c_str(), getNetAddress().toString(),
                                                        req->initiator.isValid() ? req->initiator->getNetAddress().toString() : "Unknown");

      if(req->initiator.isValid())
         req->initiator->m2cArrangedConnectionRejected(req->initiatorQueryId, rejectData);

      delete req;
   }


   // s2mUpdateServerStatus updates the status of a server to the Master Server, specifying the current game
   // and mission types, any player counts and the current info flags.
   // Updates the master with the current status of a game server.
   TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, s2mUpdateServerStatus, (StringTableEntry levelName, StringTableEntry levelType,
                                                    U32 botCount, U32 playerCount, U32 maxPlayers, U32 infoFlags))
   {
      // Only accept updates from game servers
      if(mConnectionType != MasterConnectionTypeServer)
         return;

      // Only update if anything has changed
      if(mLevelName   != levelName   || mLevelType  != levelType  || mNumBots   != botCount   || 
         mPlayerCount != playerCount || mMaxPlayers != maxPlayers || mInfoFlags != infoFlags )
      {
         mLevelName = levelName;
         mLevelType = levelType;

         mNumBots     = botCount;
         mPlayerCount = playerCount;
         mMaxPlayers  = maxPlayers;
         mInfoFlags   = infoFlags;

         // Check to ensure we're not getting flooded with these requests
         checkActivityTime(4000);      // 4 secs     version 014 send status every 5 seconds

         gNeedToWriteStatus = true;
      }
   }


   ////////////////////////////////////////
   ////////////////////////////////////////

   Int<BADGE_COUNT> MasterServerConnection::getBadges()
   {
      //// This is, obviously, temporary; can be removed when we have a real master-side badge system in place
      //if(stricmp(name.getString(), "watusimoto") == 0 || stricmp(name.getString(), "raptor") == 0 ||
      //   stricmp(name.getString(), "sam686") == 0 )
      //{
      //   return DEVELOPER_BADGE;
      //}

      if(mCSProtocolVersion <= 35) // bitfighter 017 limited to display only some badges
         return mBadges & (BIT(DEVELOPER_BADGE) | BIT(FIRST_VICTORY) | BIT(BADGE_TWENTY_FIVE_FLAGS));

      return mBadges;
   }


   U16 MasterServerConnection::getGamesPlayed()
   {
      return mGamesPlayed;
   }


   void MasterServerConnection::processIsAuthenticated(GameStats *gameStats)
   {
      for(S32 i = 0; i < gameStats->teamStats.size(); i++)
      {
         Vector<PlayerStats> *playerStats = &gameStats->teamStats[i].playerStats;

         for(S32 j = 0; j < playerStats->size(); j++)
         {
            Nonce playerId = playerStats->get(j).nonce;
            MasterServerConnection *client = findClient(playerId);
            playerStats->get(j).isAuthenticated = (client && client->isAuthenticated());
         }
      }
   }



   struct AddGameReport : public DatabaseAccessThread::BasicEntry
   {
      GameStats mStats;
      void run()
      {
         DatabaseWriter databaseWriter = getDatabaseWriter();
         // Will fail if compiled without database support and gWriteStatsToDatabase is true
         databaseWriter.insertStats(mStats);
      }
   };
   void MasterServerConnection::writeStatisticsToDb(VersionedGameStats &stats)
   {
      if(!checkActivityTime(6000))     // 6 seconds
         return;

      if(!stats.valid)
      {
         logprintf(LogConsumer::LogWarning, "Invalid stats: version %d, %s %s", stats.version, getNetAddressString(), mPlayerOrServerName.getString());
         return;
      }

      GameStats *gameStats = &stats.gameStats;

      gameStats->serverIP = getNetAddressString();
      gameStats->serverName = mPlayerOrServerName.getString();
      gameStats->cs_protocol_version = mCSProtocolVersion;

      processIsAuthenticated(gameStats);
      processStatsResults(gameStats);

      RefPtr<AddGameReport> gameReport = new AddGameReport();
      gameReport->mStats = *gameStats;  // copy so we keep data during a thread
      gDatabaseAccessThread.addEntry(gameReport);
   }

   
   struct AchievementWriter : public DatabaseAccessThread::BasicEntry
   {
      U8 achievementId;
      StringTableEntry playerNick;
      StringTableEntry mPlayerOrServerName;
      string addressString;
      void run()
      {
         DatabaseWriter databaseWriter = getDatabaseWriter();
         // Will fail if compiled without database support and gWriteStatsToDatabase is true
         databaseWriter.insertAchievement(achievementId, playerNick.getString(), mPlayerOrServerName.getString(), addressString);
      }
   };
   void MasterServerConnection::writeAchievementToDb(U8 achievementId, const StringTableEntry &playerNick)
   {
      if(!checkActivityTime(6000))  // 6 seconds
         return;

      // Basic sanity check
      if(playerNick == "")
         return;

      RefPtr<AchievementWriter> a_writer = new AchievementWriter();
      a_writer->achievementId = achievementId;
      a_writer->playerNick = playerNick;
      a_writer->mPlayerOrServerName = mPlayerOrServerName;
      a_writer->addressString = getNetAddressString();
      gDatabaseAccessThread.addEntry(a_writer);
   }


   struct LevelInfoWriter : public DatabaseAccessThread::BasicEntry
   {
      string hash;
      string levelName;
      string creator;
      StringTableEntry gameType;
      bool hasLevelGen;
      U8 teamCount;
      S32 winningScore;
      S32 gameDurationInSeconds;
      void run()
      {
         DatabaseWriter databaseWriter = getDatabaseWriter();
         // Will fail if compiled without database support and gWriteStatsToDatabase is true
         databaseWriter.insertLevelInfo(hash, levelName, creator, gameType.getString(), hasLevelGen, teamCount, winningScore, gameDurationInSeconds);
      }
   };
   void MasterServerConnection::writeLevelInfoToDb(const string &hash, const string &levelName, const string &creator, 
                                                   const StringTableEntry &gameType, bool hasLevelGen, U8 teamCount, S32 winningScore, 
                                                   S32 gameDurationInSeconds)
   {
      if(!checkActivityTime(6000))  // 6 seconds
         return;

      // Basic sanity check
      if(hash == "" || gameType == "")
         return;

      RefPtr<LevelInfoWriter> l_writer = new LevelInfoWriter();
      l_writer->hash = hash;
      l_writer->levelName = levelName;
      l_writer->creator = creator;
      l_writer->gameType = gameType;
      l_writer->hasLevelGen = hasLevelGen;
      l_writer->teamCount = teamCount;
      l_writer->winningScore = winningScore;
      l_writer->gameDurationInSeconds = gameDurationInSeconds;
      gDatabaseAccessThread.addEntry(l_writer);
   }


   struct HighScoresReader : public DatabaseAccessThread::BasicEntry
   {
      S32 scoresPerGroup;
      void run()
      {
         DatabaseWriter databaseWriter = getDatabaseWriter();

         // Client will display these in two columns, row by row

         MasterServerConnection::highScores.groupNames.clear();
         MasterServerConnection::highScores.names.clear();
         MasterServerConnection::highScores.scores.clear();

         MasterServerConnection::highScores.groupNames.push_back("Official Wins Last Week");
         databaseWriter.getTopPlayers("v_last_week_top_player_official_wins",    "win_count",  scoresPerGroup, MasterServerConnection::highScores.names, MasterServerConnection::highScores.scores);

         MasterServerConnection::highScores.groupNames.push_back("Official Wins This Week, So Far");
         databaseWriter.getTopPlayers("v_current_week_top_player_official_wins", "win_count",  scoresPerGroup, MasterServerConnection::highScores.names, MasterServerConnection::highScores.scores);

         MasterServerConnection::highScores.groupNames.push_back("Games Played Last Week");
         databaseWriter.getTopPlayers("v_last_week_top_player_games",            "game_count", scoresPerGroup, MasterServerConnection::highScores.names, MasterServerConnection::highScores.scores);

         MasterServerConnection::highScores.groupNames.push_back("Games Played This Week, So Far");
         databaseWriter.getTopPlayers("v_current_week_top_player_games",         "game_count", scoresPerGroup, MasterServerConnection::highScores.names, MasterServerConnection::highScores.scores);

         MasterServerConnection::highScores.groupNames.push_back("Latest BBB Winners");
         databaseWriter.getTopPlayers("v_latest_bbb_winners",         "rank", scoresPerGroup, MasterServerConnection::highScores.names, MasterServerConnection::highScores.scores);

         MasterServerConnection::highScores.scoresPerGroup = scoresPerGroup;
      }
      void finish()
      {
         MasterServerConnection::highScores.isBuzy = false;
         for(S32 i=0; i < MasterServerConnection::highScores.waitingClients.size(); i++)
            if(MasterServerConnection::highScores.waitingClients[i])
               MasterServerConnection::highScores.waitingClients[i]->m2cSendHighScores(MasterServerConnection::highScores.groupNames, MasterServerConnection::highScores.names, MasterServerConnection::highScores.scores);
         MasterServerConnection::highScores.waitingClients.clear();

      }
   };
   HighScores *MasterServerConnection::getHighScores(S32 scoresPerGroup)
   {
      // Check if we have the scores cached and not outdated
      U32 currentTime = Platform::getRealMilliseconds();
      if(currentTime - highScores.lastClock > 2 * 60 * 1000  // 2 hours
          || !highScores.isValid || scoresPerGroup != highScores.scoresPerGroup)     // Remember... highScores is static!
         if(!highScores.isBuzy)
      {
         highScores.isBuzy = true;
         highScores.isValid = true;
         highScores.lastClock = currentTime;
         RefPtr<HighScoresReader> highScoreReader = new HighScoresReader();
         highScoreReader->scoresPerGroup = scoresPerGroup;
         gDatabaseAccessThread.addEntry(highScoreReader);
      }
      
      return &highScores;
   }


   //////////

   TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, s2mSendStatistics, (VersionedGameStats stats))
   {
      writeStatisticsToDb(stats);
      highScores.isValid = false;
   }


   TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, s2mAcheivementAchieved, (U8 achievementId, StringTableEntry playerNick))
   {
      if(achievementId > BADGE_COUNT)  // Check for out of range badges
         return;

      writeAchievementToDb(achievementId, playerNick);

      for(MasterServerConnection *walk = gClientList.mNext; walk != &gClientList; walk = walk->mNext)
         if(walk->mPlayerOrServerName == playerNick)
         {
            walk->mBadges = mBadges | BIT(achievementId); // Add to local variable without needing to reload from database
            break;
         }
   }


   TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, s2mSendLevelInfo, 
                              (string hash, string levelName, string creator, 
                               StringTableEntry gameType, bool hasLevelGen, U8 teamCount, S32 winningScore, S32 gameDurationInSeconds))
   {
      writeLevelInfoToDb(hash, levelName, creator, gameType, hasLevelGen, teamCount, winningScore, gameDurationInSeconds);
   }


   TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, c2mRequestHighScores, ())
   {
      Vector<StringTableEntry> groupNames;
      Vector<string> names;
      Vector<string> scores;

      HighScores *hightScores = getHighScores(3);     

      if(highScores.isBuzy)
      {
         bool exists = false;
            for(S32 i = 0; i < highScores.waitingClients.size(); i++)
               if(highScores.waitingClients[i] == this)
                  exists = true;
         if(!exists)
            highScores.waitingClients.push_back(this);
      }
      else
         m2cSendHighScores(hightScores->groupNames, hightScores->names, hightScores->scores);
   }


   TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, c2mRequestMOTD, ())
   {
      sendMotd();
   }


   // Game server wants to know if user name has been verified
   TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, s2mRequestAuthentication, (Vector<U8> id, StringTableEntry name))
   {
      Nonce clientId(id);     // Reconstitute our id

      for(MasterServerConnection *walk = gClientList.mNext; walk != &gClientList; walk = walk->mNext)
         if(walk->mPlayerId == clientId)
         {
            AuthenticationStatus status;

            // Need case insensitive comparison here
            if(!stricmp(name.getString(), walk->mPlayerOrServerName.getString()) && walk->isAuthenticated())      
               status = AuthenticationStatusAuthenticatedName;

            // If server just restarted, clients will need to reauthenticate, and that may take some time.
            // We'll give them 90 seconds.
            else if(Platform::getRealMilliseconds() - gServerStartTime < 90 * 1000)      
               status = AuthenticationStatusTryAgainLater;             
            else
               status = AuthenticationStatusUnauthenticatedName;

            if(mCMProtocolVersion <= 6)      // 018a ==> 6, 019 ==> 7
               m2sSetAuthenticated(id, walk->mPlayerOrServerName, status, walk->getBadges());
            else
               m2sSetAuthenticated_019(id, walk->mPlayerOrServerName, status, walk->getBadges(), walk->getGamesPlayed());
            break;
         }
   }


   string MasterServerConnection::cleanName(string name)
   {
      trim(name);
      if(name == "")
         return "ChumpChange";

      return name;
   }


   void MasterServerConnection::sendMotd()
   {
      // Figure out which MOTD to send to client, based on game version (stored in mVersionString)
      string motdString = "Welcome to Bitfighter!";  // Default msg

      if(gMOTDClientMap[mClientBuild] != "")
         motdString = gMOTDClientMap[mClientBuild];

      m2cSetMOTD(gMasterName.c_str(), motdString.c_str());     // Even level 0 clients can handle this
   }


   // Must match MasterServerConnection::writeConnectRequest()!!
   bool MasterServerConnection::readConnectRequest(BitStream *bstream, NetConnection::TerminationReason &reason)
   {
      mLoggingStatus = "Something failed in readConnectRequest";
      if(!Parent::readConnectRequest(bstream, reason))
      {
         mLoggingStatus = "Parent::readConnectRequest failed";
         return false;
      }

      // Note that if player is hosting a game interactively (i.e. with host option on main menu), they
      // will create two connections here, one for the host, and one for the client instance.
      char readstr[256]; // to hold the string being read

      bstream->read(&mCMProtocolVersion);    // Version of protocol we'll use with the client
      if(mCMProtocolVersion < 4 || mCMProtocolVersion > MASTER_PROTOCOL_VERSION) // check for unsupported version
      {
         mLoggingStatus = "Bad version";
         return false;
      }

      bstream->read(&mCSProtocolVersion);    // Protocol this client uses for C-S communication
      bstream->read(&mClientBuild);          // Client's build number

      // In protocol 6 we've added the concept of a connection type
      if(mCMProtocolVersion >= 6)
         mConnectionType = (MasterConnectionType) bstream->readEnum(MasterConnectionTypeCount);

      // Protocol 5 and earlier only sent 1 bit to determine if it was a game server
      else
      {
         if(bstream->readFlag())
            mConnectionType = MasterConnectionTypeServer;
         else
            mConnectionType = MasterConnectionTypeClient;
      }

      // Now read the bitstream and do other logic according to the connection type
      switch(mConnectionType)
      {
         // If it's a game server, read status info...
         case MasterConnectionTypeServer:
         {
            bstream->read(&mNumBots);
            bstream->read(&mPlayerCount);
            bstream->read(&mMaxPlayers);
            bstream->read(&mInfoFlags);

            bstream->readString(readstr);    // Current level name
            mLevelName = readstr;

            bstream->readString(readstr);    // Current level type (hunters, ctf, soccer, etc.)
            mLevelType = readstr;

            bstream->readString(readstr);
            mPlayerOrServerName = cleanName(readstr).c_str();

            bstream->readString(readstr);
            mServerDescr = readstr;

            linkToServerList();
         }
         break;

         case MasterConnectionTypeClient:
         {
            bstream->readString(readstr);          // Client's joystick autodetect string
            mAutoDetectStr = readstr;

            bstream->readString(readstr);
            mPlayerOrServerName = cleanName(readstr).c_str();

            bstream->readString(readstr); // the last "readstr" for "password" in startAuthentication

            // Read client flags -- only the first is currently used
            if(mCMProtocolVersion >= 6)
            {
               U8 flags = bstream->readInt(8);
               mIsDebugClient = flags & ClientDebugModeFlag;
            }

            mPlayerId.read(bstream);

            // Probably redundant, but let's cycle through all our clients and make sure the playerId is unique.  With 2^64
            // possibilities, it most likely will be.
            for(MasterServerConnection *walk = gClientList.mNext; walk != &gClientList; walk = walk->mNext)
               if(walk != this && walk->mPlayerId == mPlayerId)
               {
                  logprintf(LogConsumer::LogConnection, "User %s provided duplicate id to %s", mPlayerOrServerName.getString(),
                                                                                                walk->mPlayerOrServerName.getString());
                  disconnect(ReasonDuplicateId, "");
                  reason = ReasonDuplicateId;
                  return false;
               }

            // Start the authentication by reading database on seperate thread
            // On clients 017 and older, they completely ignore any disconnect reason once fully connected,
            // so we pause waiting for database instead of fully connecting yet.

            for(S32 i = 0; i < gListAddressHide.size(); i++)
               if(getNetAddress().isEqualAddress(gListAddressHide[i]))
                  mIsIgnoredFromList = true;

            switch(checkAuthentication(readstr, mCSProtocolVersion <= 35)) // readstr is password
            {
               case WrongPassword:   reason = ReasonBadLogin;        return false;
               case InvalidUsername: reason = ReasonInvalidUsername; return false;
               case UnknownStatus: // make JSON delay write, to reduce chances of see the newly joined player as unauthenticated
               {
                  bool NeedToWriteStatusPrev = gNeedToWriteStatus;
                  linkToClientList();
                  gNeedToWriteStatus = NeedToWriteStatusPrev; // Don't let linkToClientList set it to true
                  gNeedToWriteStatusDelayed = Platform::getRealMilliseconds();
                  break;
               }
               default: linkToClientList();
            }

            // If client needs to upgrade, tell them
            m2cSendUpdgradeStatus(gLatestReleasedCSProtocol > mCSProtocolVersion || gLatestReleasedBuildVersion > mClientBuild);

            // Send message of the day
            sendMotd();
         }
         break;

         case MasterConnectionTypeAnonymous:
         {
            // Do nothing!
         }
         break;

         default:
            break;
      }


      // Log our connection if from client or server
      if(mConnectionType == MasterConnectionTypeServer || mConnectionType == MasterConnectionTypeClient)
      {
         // CLIENT/SERVER_INFO | timestamp | protocol version | build number | address | controller
         logprintf(LogConsumer::LogConnection, "%s\t%s\t%d\t%d\t%s\t%s",
               mConnectionType == MasterConnectionTypeServer ? "SERVER_INFO" : "CLIENT_INFO",
               getTimeStamp().c_str(), mCMProtocolVersion, mClientBuild, getNetAddress().toString(),
               strcmp(mAutoDetectStr.getString(), "") ? mAutoDetectStr.getString():"<None>");
      }

      mLoggingStatus = "";  // No issues!

      return true;
   }

   // Appears unused!
   //static void checkConnectTimeouts()
   //{
   //   const S32 ConnectRequestTimeout = 30000;      // 30 secs
   //   U32 currentTime = Platform::getRealMilliseconds();

   //   // Expire any connect requests that have grown old...
   //   for(S32 i = 0; i < gConnectList.size(); )    // third param deliberately blank
   //   {
   //      GameConnectRequest *gcr = gConnectList[i];
   //      if(currentTime - gcr->requestTime > ConnectRequestTimeout)
   //      {
   //         // It's old, so remove it from the initiator's list...
   //         if(gcr->initiator.isValid())
   //         {
   //            gcr->initiator->removeConnectRequest(gcr);
   //            ByteBufferPtr reqTimeoutBuffer = new ByteBuffer((U8 *) MasterRequestTimedOut, (U32) strlen(MasterRequestTimedOut) + 1);
   //            
   //            // For older clients (009 and below)
   //            if(mCMProtocolVersion <= 1)
   //               gcr->initiator->s2mRejectArrangedConnection(gcr->initiatorQueryId, reqTimeoutBuffer);
   //            // For clients 010 and above
   //            else
   //               gcr->initiator->s2mRejectArrangedConnection(gcr->initiatorQueryId, reqTimeoutBuffer, ConnectionAttemptTimedOut);
   //         }

   //         // And the host's lists..
   //         if(gcr->host.isValid())
   //            gcr->host->removeConnectRequest(gcr);

   //         // Delete it...
   //         delete gcr;

   //         // And remove it from our list, too.
   //         gConnectList.erase_fast(i);
   //         continue;
   //      }
   //      i++;
   //   }
   //}


   TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, c2mJoinGlobalChat, ())
   {
      Vector<StringTableEntry> names;

      for(MasterServerConnection *walk = gClientList.mNext; walk != &gClientList; walk = walk->mNext)
         if (walk != this && walk->isInGlobalChat)
            names.push_back(walk->mPlayerOrServerName);

      if(names.size() > 0)
         m2cPlayersInGlobalChat(names); // Send to this client, to avoid blank name list of quickly leave/join.

      if(mIsIgnoredFromList) // don't list name in lobby, too.
         return;

      mLeaveGlobalChatTimer = 0; // don't continue with delayed chat leave.
      if(isInGlobalChat)  // Already in Global Chat
         return;

      isInGlobalChat = true;
      
      for(MasterServerConnection *walk = gClientList.mNext; walk != &gClientList; walk = walk->mNext)
         if (walk != this && walk->isInGlobalChat)
            walk->m2cPlayerJoinedGlobalChat(mPlayerOrServerName);

   }


   TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, c2mLeaveGlobalChat, ())
   {
      if(!isInGlobalChat)  // was not in Global Chat
         return;

      // using delayed leave, to avoid quickly join / leave problem.
      mLeaveGlobalChatTimer = Platform::getRealMilliseconds() - 20; // "-20" make up for inaccurate getRealMilliseconds going backwards by 1 or 2 milliseconds.
      gLeaveChatTimerList.push_back(this);

      //isInGlobalChat = false;
      //for(MasterServerConnection *walk = gClientList.mNext; walk != &gClientList; walk = walk->mNext)
      //   if (walk != this && walk->isInGlobalChat)
      //      walk->m2cPlayerLeftGlobalChat(mPlayerOrServerName);
   }


   // Got out-of-game chat message from client, need to relay it to others
   TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, c2mSendChat, (StringPtr message))
   {
      // Variables needed for private message handling
      bool isPrivate = false;
      string pmRecipient;
      const char *strippedMessage = NULL;

      bool badCommand = false;

      // Incoming command!
      if(message.getString()[0] == '/')
      {
         Vector<string> words = parseStringAndStripLeadingSlash(message.getString());

         string command = lcase(words[0]);

         // Master Admin-only commands
         if(mIsMasterAdmin)
         {
            if(command == "dropserver")
            {
               bool droppedServer = false;
               Address addr(words[1].c_str());

               for(MasterServerConnection *walk = gServerList.mNext; walk != &gServerList; walk = walk->mNext)
                  if(walk->getNetAddress().isEqualAddress(addr) && (addr.port == 0 || addr.port == walk->getNetAddress().port))
                  {
                     walk->mIsIgnoredFromList = true;
                     m2cSendChat(walk->mPlayerOrServerName, true, "dropped");
                     droppedServer = true;
                  }

               if(!droppedServer)
                  m2cSendChat(mPlayerOrServerName, true, "dropserver: address not found");

               return;
            }
            else if(command == "restoreservers")
            {
               bool broughtBackServer = false;
               for(MasterServerConnection *walk = gServerList.mNext; walk != &gServerList; walk = walk->mNext)
                  if(walk->mIsIgnoredFromList)
                  {
                     broughtBackServer = true;
                     walk->mIsIgnoredFromList = false;
                     m2cSendChat(walk->mPlayerOrServerName, true, "servers restored");
                  }
               if(!broughtBackServer)
                  m2cSendChat(mPlayerOrServerName, true, "No server was hidden");
               return;
            }
            else if(command == "hideplayer")
            {
               bool found = false;
               for(MasterServerConnection *walk = gClientList.mNext; walk != &gClientList; walk = walk->mNext)
                  if(strcmp(words[1].c_str(), walk->mPlayerOrServerName.getString()) == 0)
                  {
                     walk->mIsIgnoredFromList = !walk->mIsIgnoredFromList;
                     m2cSendChat(walk->mPlayerOrServerName, true, walk->mIsIgnoredFromList ? "player hidden" : "player not hidden anymore");
                     found = true;
                  }
               if(!found)
                  m2cSendChat(mPlayerOrServerName, true, "player not found");
            }
            else if(command == "hideip")
            {
               Address addr(words[1].c_str());
               bool found = false;
               for(MasterServerConnection *walk = gClientList.mNext; walk != &gClientList; walk = walk->mNext)
                  if(addr.isEqualAddress(walk->getNetAddress()))
                  {
                     walk->mIsIgnoredFromList = true;
                     m2cSendChat(walk->mPlayerOrServerName, true, "player now hidden");
                     c2mLeaveGlobalChat_remote();  // Also mute and delist the player
                     found = true;
                  }
               gListAddressHide.push_back(addr);
               if(found)
                  m2cSendChat(mPlayerOrServerName, true, "player found, and is in IP hidden list");
               if(!found)
                  m2cSendChat(mPlayerOrServerName, true, "player not found, but is in IP hidden list");
            }
            else if(command ==  "unhideips")
            {
               gListAddressHide.clear();
               m2cSendChat(mPlayerOrServerName, true, "cleared IP hidden list");
            }
            else
               badCommand = true;  // Don't relay bad commands as chat messages
         }
         // Not admin command
         else
         {
            if(command == "pm")
            {
               if(words.size() < 3)  // No message?
               {
                  logprintf(LogConsumer::LogError, "Malformed private message: %s", message.getString());
                  m2cSendChat(mPlayerOrServerName, true, "Malformed private message");
                  return;
               }

               isPrivate = true;

               pmRecipient = words[1];

               S32 argCount = 2 + countCharInString(words[1], ' ');  // Set pointer after 2 args + number of spaces in player name
               strippedMessage = findPointerOfArg(message, argCount);

               // Now relay the message and only send to client with the specified nick
               for(MasterServerConnection *walk = gClientList.mNext; walk != &gClientList; walk = walk->mNext)
               {
                  if(stricmp(walk->mPlayerOrServerName.getString(), pmRecipient.c_str()) == 0)
                  {
                     walk->m2cSendChat(mPlayerOrServerName, isPrivate, strippedMessage);
                     break;
                  }
               }
            }
            else
               badCommand = true;  // Don't relay bad commands as chat messages
         }
      }


      // If player is being ignored or chatting too fast
      if(mIsIgnoredFromList || !checkMessage(message, isPrivate ? 2 : 0))
      {
         if(!mChatTooFast)
            logprintf(LogConsumer::LogChat, mIsIgnoredFromList ? "Tried to chat but muted and hidden: %s" : "This player may be chatting to fast: %s ", mPlayerOrServerName.getString());

         mChatTooFast = true;
         static const StringTableEntry msg("< You are chatting too fast, your message didn't make it through.");
         m2cSendChat(msg, false, StringPtr(" "));

         return;  // Bail
      }
      mChatTooFast = false;


      // Now relay the chat to all connected clients
      if(!badCommand && !isPrivate)
      {
         for(MasterServerConnection *walk = gClientList.mNext; walk != &gClientList; walk = walk->mNext)
         {
            if(walk != this)    // ...except self!
               walk->m2cSendChat(mPlayerOrServerName, isPrivate, message);
         }
      }


      // Log F5 chat messages
      if(isPrivate)
         logprintf(LogConsumer::LogChat, "Relayed private msg from %s to %s: %s", mPlayerOrServerName.getString(), pmRecipient.c_str(), strippedMessage);
      else
         logprintf(LogConsumer::LogChat, "Relayed chat msg from %s: %s", mPlayerOrServerName.getString(), message.getString());
   }

   TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, s2mChangeName, (StringTableEntry name))
   {
      if(mConnectionType == MasterConnectionTypeServer)  // server only, don't want clients to rename yet (client names need to authenticate)
      {
         mPlayerOrServerName = name;
         gNeedToWriteStatus = true;  // update server name in ".json"
      }
   }
   TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, s2mServerDescription, (StringTableEntry descr))
   {
      mServerDescr = descr;
   }

TNL_IMPLEMENT_NETCONNECTION(MasterServerConnection, NetClassGroupMaster, true);

Vector< GameConnectRequest* > MasterServerConnection::gConnectList;
Vector<SafePtr<MasterServerConnection> > MasterServerConnection::gLeaveChatTimerList;

MasterServerConnection MasterServerConnection::gServerList;
MasterServerConnection MasterServerConnection::gClientList;


void seedRandomNumberGenerator()
{
   U32 time = Platform::getRealMilliseconds();

   U8 buf[16];

   buf[0] = U8(time);
   buf[1] = U8(time >> 8);
   buf[2] = U8(time >> 16);
   buf[3] = U8(time >> 24);

   // Need at least 16 bytes to make anything happen.  We'll provide 4 sort of good ones, and 12 bytes of uninitialized crap.
   Random::addEntropy(buf, 16);
}


//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
////////////////////////////////////////
////////////////////////////////////////

#include <stdio.h>

////////////////////////////////////////
////////////////////////////////////////

FileLogConsumer gFileLogConsumer;
FileLogConsumer gStatisticsLogConsumer;
StdoutLogConsumer gStdoutLogConsumer;

////////////////////////////////////////
////////////////////////////////////////

// Create a test database and write some records to it.  Return exit code.
S32 testDb(const char *dbName)
{
   DatabaseWriter databaseWriter(dbName);
   databaseWriter.setDumpSql(true);

   databaseWriter.insertAchievement(1, "ChumpChange", "Achievement Server", "99.99.99.99:9999");
   databaseWriter.insertLevelInfo("9aa6e5f2256c17d2d430b100032b997c", "Clown Car", "Jenkins!", "Core", false, 2, 20, 600);

   GameStats gameStats;
   gameStats.build_version = 100;
   gameStats.cs_protocol_version = 101;
   gameStats.duration = 999;
   gameStats.gameType = "Frogger";
   gameStats.isOfficial = false;
   gameStats.isTeamGame = true;
   gameStats.levelName = "LouLou";
   gameStats.playerCount = 2;
   gameStats.serverIP = "999.999.999.999";
   gameStats.serverName = "Crazy IP Server";

   TeamStats teamStats;
   teamStats.gameResult = 'W';
   teamStats.hexColor = "#FF0000";
   teamStats.name = "Red Dudes";
   teamStats.score = 2;

   PlayerStats playerStats;
   playerStats.changedLoadout = 1;
   playerStats.crashedIntoAsteroid = 2;      
   playerStats.deaths = 3;
   playerStats.flagDrop = 4;  
   playerStats.flagPickup = 5;
   playerStats.flagReturn = 6;
   playerStats.flagScore = 7;
   playerStats.fratricides = 8;
   playerStats.gameResult = 'W';
   playerStats.isAdmin = false;
   playerStats.isAuthenticated = true;
   playerStats.isHosting = true;
   playerStats.isLevelChanger = true;
   playerStats.isRobot = false;
   playerStats.kills = 9;
   playerStats.name = "Player 1";
   playerStats.playTime = 99;
   playerStats.points = 10;
   playerStats.suicides = 11;
   playerStats.switchedTeamCount = 12;  
   playerStats.teleport = 13; 
   teamStats.playerStats.push_back(playerStats);

   playerStats.changedLoadout = 101;
   playerStats.crashedIntoAsteroid = 102;
   playerStats.deaths = 103;
   playerStats.flagDrop = 104;
   playerStats.flagPickup = 105;
   playerStats.flagReturn = 106;
   playerStats.flagScore = 107;
   playerStats.fratricides = 108;
   playerStats.gameResult = 'W';
   playerStats.isAdmin = false;
   playerStats.isAuthenticated = true;
   playerStats.isHosting = true;
   playerStats.isLevelChanger = true;
   playerStats.isRobot = false;
   playerStats.kills = 109;
   playerStats.name = "Player 1";
   playerStats.playTime = 1099;
   playerStats.points = 1010;
   playerStats.suicides = 1011;
   playerStats.switchedTeamCount = 1012;
   playerStats.teleport = 1013;
   teamStats.playerStats.push_back(playerStats);

   gameStats.teamStats.push_back(teamStats);

   databaseWriter.insertStats(gameStats);

   printf("Created database %s", dbName);

   return 0;
}


////////////////////////////////////////
////////////////////////////////////////

U32 gMasterPort = 25955;      // <== Default, can be overwritten in cfg file

extern void readConfigFile(CIniFile *ini);


int main(int argc, const char **argv)
{
   if(argc == 2 && strcmp(argv[1], "-testdb") == 0)
      exit(testDb("test_db"));

   gServerStartTime = Platform::getRealMilliseconds();

   gMasterName = "Bitfighter Master Server";    // Default, can be overridden in cfg file

   seedRandomNumberGenerator();

   // Configure logging
   S32 events = LogConsumer::AllErrorTypes | LogConsumer::LogConnection | LogConsumer::LogConnectionManager | LogConsumer::LogChat;

   gFileLogConsumer.init("bitfighter_master.log", "a");
   gFileLogConsumer.setMsgTypes(events);                               // Primary logfile
   gFileLogConsumer.logprintf("------ Bitfighter Master Server Log File ------");

   gStatisticsLogConsumer.init("bitfighter_player_stats.log", "a");
   gStatisticsLogConsumer.setMsgTypes(LogConsumer::StatisticsFilter);  // Statistics file

   gStdoutLogConsumer.setMsgTypes(events);                             // stdout


   // Set INI location
   gMasterINI.SetPath("master.ini");

   // Parse command line parameters...
   readConfigFile(&gMasterINI);


   // Initialize our net interface so we can accept connections...
   gNetInterface = new NetInterface(Address(IPProtocol, Address::Any, gMasterPort));

   //for the master server alone, we don't need a key exchange - that would be a waste
   //gNetInterface->setRequiresKeyExchange(true);
   //gNetInterface->setPrivateKey(new AsymmetricKey(20));

   // Log a welcome message in the main log and to the console
   gFileLogConsumer.logprintf("[%s] Master Server %s started - listening on port %d", getTimeStamp().c_str(), gMasterName.c_str(), gMasterPort);
   gStdoutLogConsumer.logprintf("Master Server %s started - listening on port %d", gMasterName.c_str(), gMasterPort);

   const U32 REWRITE_TIME = 5000;        // Rewrite status file at most this often (in ms)
   const U32 REREAD_TIME  = 5000;        // How often to we re-read our config file? (in ms)

   U32 lastConfigReadTime = Platform::getRealMilliseconds();
   U32 lastWroteStatusTime = lastConfigReadTime - REWRITE_TIME;    // So we can do a write right off the bat

   gDatabaseAccessThread.start();  // start a thread that handles some of slow task with database.

    // And until infinity, process whatever comes our way.
   while(true)     // To infinity and beyond!!
   {
      gNetInterface->checkIncomingPackets();
      gNetInterface->processConnections();
      U32 currentTime = Platform::getRealMilliseconds();

      if(currentTime - lastConfigReadTime > REREAD_TIME)     // Reread the config file every 5000ms
      {
         lastConfigReadTime = currentTime;
         readConfigFile(&gMasterINI);
      }

      if(gNeedToWriteStatusDelayed != 0)
         if(currentTime - gNeedToWriteStatusDelayed > REWRITE_TIME)
            gNeedToWriteStatus = true;

      // Write status file as need, at most every REWRITE_TIME ms
      if(gNeedToWriteStatus && currentTime - lastWroteStatusTime > REWRITE_TIME)  
      {
         lastWroteStatusTime = currentTime;
         MasterServerConnection::writeClientServerList_JSON();
         gNeedToWriteStatus = false;
         gNeedToWriteStatusDelayed = 0;
      }

      // Cycle through all our connections, checking if any have timed out
      for(S32 i = MasterServerConnection::gConnectList.size() - 1; i >= 0; i--)
      {
         GameConnectRequest *request = MasterServerConnection::gConnectList[i];

         if(currentTime - request->requestTime > 5000)   // 5 seconds
         {
            if(request->initiator.isValid())
            {
               ByteBufferPtr ptr = new ByteBuffer((U8 *) MasterRequestTimedOut, (U32) strlen(MasterRequestTimedOut) + 1);

               request->initiator->m2cArrangedConnectionRejected(request->initiatorQueryId, ptr);   // 0 = ReasonTimedOut
               request->initiator->removeConnectRequest(request);
            }

            if(request->host.isValid())
               request->host->removeConnectRequest(request);

            MasterServerConnection::gConnectList.erase_fast(i);
            delete request;
         }
      }

      // Process any delayed disconnects; we use this to avoid repeating and flooding join / leave messages
      for(S32 i = MasterServerConnection::gLeaveChatTimerList.size() - 1; i >= 0; i--)
      {
         MasterServerConnection *c = MasterServerConnection::gLeaveChatTimerList[i];

         if(!c || c->mLeaveGlobalChatTimer == 0)
            MasterServerConnection::gLeaveChatTimerList.erase(i);
         else
         {
            if(currentTime - c->mLeaveGlobalChatTimer > 1000)
            {
               c->isInGlobalChat = false;

               MasterServerConnection *head = &MasterServerConnection::gClientList;

               for(MasterServerConnection *walk = head->mNext; walk != head; walk = walk->mNext)
                  if (walk != c && walk->isInGlobalChat)
                     walk->m2cPlayerLeftGlobalChat(c->mPlayerOrServerName);

               MasterServerConnection::gLeaveChatTimerList.erase(i);
            }
         }
      }

      gDatabaseAccessThread.runAtMainThread();

      Platform::sleep(5);
   }

   return 0;
}
