#include "TestUtils.h"

#include "../zap/gameType.h"
#include "../zap/ServerGame.h"
#include "../zap/ClientGame.h"
#include "../zap/ChatCommands.h"

#include "gtest/gtest.h"

namespace Zap
{

using namespace std;
using namespace TNL;


static void killShip(Ship *ship)
{
   DamageInfo killerDamage;
   killerDamage.damageAmount = 1;
   ship->damageObject(&killerDamage);
}


// Scenario 1: Player is idle and gets suspended -- no other players in game
static void doScenario1(GamePair &gamePair)
{
   ClientGame *clientGame = gamePair.client;
   ServerGame *serverGame = gamePair.server;

   // Idle for a while -- ship should become spawn delayed.  GameConnection::SPAWN_DELAY_TIME is measured in ms.
   gamePair.idle(10, GameConnection::SPAWN_DELAY_TIME / 10);

   ASSERT_TRUE(serverGame->getClientInfo(0)->isPlayerInactive());    // No input from player, should be flagged as inactive
   // Note that spawn delay does not get set until the delayed ship tries to spawn, even if player is marked as inactive

   // Kill the ship again -- should be delayed when it tries to respawn because client has been inactive
   fillVector.clear();
   serverGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   EXPECT_EQ(1, fillVector.size());    // Should only be one ship

   Ship *ship = static_cast<Ship *>(fillVector[0]);      // Server's copy of the ship

   killShip(ship);

   gamePair.idle(10, GameType::RespawnDelay / 10 + 5);
   // Since server has received no input from client for GameConnection::SPAWN_DELAY_TIME ms, and ship has attempted to respawn, should be spawn-delayed
   ASSERT_TRUE(serverGame->getClientInfo(0)->isSpawnDelayed());
   ASSERT_TRUE(clientGame->isSpawnDelayed());      // Status should have propagated to client by now

   // At this point, client and server are both aware that the spawn is delayed due to player inactivity

   gamePair.idle(10, 10);              // Idle 10x
   // If spawn were not delayed, ship would have respawned.  Check for it on the server.
   // (Dead ship may linger on client while exploding, so we won't check there.)
   fillVector.clear();
   serverGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   ASSERT_EQ(0, fillVector.size());                   // Ship is spawn delayed and won't spawn... hence no ships
   ASSERT_EQ(0, serverGame->getClientInfo(0)->getReturnToGameTime());      // No returnToGamePenalty in this scenario
   ASSERT_EQ(0, clientGame->getReturnToGameDelay());
   ASSERT_FALSE(static_cast<FullClientInfo *>(serverGame->getClientInfo(0))->hasReturnToGamePenalty());   // No penalty in the works

   // Undelay spawn
   clientGame->undelaySpawn();         // This is what gets run when player presses a key
   gamePair.idle(10, 5);               // Idle 5x; give things time to propagate

   ASSERT_FALSE(serverGame->getClientInfo(0)->isSpawnDelayed());     // Player should no longer be spawn delayed
   ASSERT_FALSE(clientGame->inReturnToGameCountdown());
   fillVector.clear();
   serverGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   ASSERT_EQ(1, fillVector.size());    // Ship should have spawned and be available on client and server
   fillVector.clear();
   clientGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   ASSERT_EQ(1, fillVector.size());    // Ship should have spawned and be available on client and server
}


// Scenario 2: Player enters idle command, other players, so server does not suspend itself.  Since
// player used idle command, a 5 second penalty will be levied against them.
static void doScenario2(GamePair &gamePair)
{
   ClientGame *clientGame = gamePair.client;
   ServerGame *serverGame = gamePair.server;

      // Add a second player so server does not suspend itself
   ClientGame *clientGame2 = newClientGame();
   GameSettingsPtr settings = clientGame2->getSettingsPtr();
   clientGame2->userEnteredLoginCredentials("TestUser2", "password", false);    // Simulates entry from NameEntryUserInterface
   clientGame2->joinLocalGame(serverGame->getNetInterface());
   clientGame2->getConnectionToServer()->useZeroLatencyForTesting();

   for(S32 i = 0; i < serverGame->getClientCount(); i++)
      serverGame->getClientInfo(i)->getConnection()->useZeroLatencyForTesting();

   // Should now be 2 ships in the game -- one belonging to clientGame and another belonging to clientGame2
   gamePair.idle(10, 5);               // Idle 5x; give things time to propagate
   fillVector.clear();
   serverGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   ASSERT_EQ(2, fillVector.size());                   // Ship should have been killed off -- only 2nd player ship should be left
   fillVector.clear();
   clientGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   ASSERT_EQ(2, fillVector.size());

   Vector<string> words;
   ChatCommands::idleHandler(clientGame, words);
   gamePair.idle(Ship::KillDeleteDelay / 15, 20);     // Idle; give things time to propagate, timers to time out, etc.
   //for(S32 i = 0; i < 5; i++) clientGame2->idle(10);
   ASSERT_TRUE(serverGame->getClientInfo(0)->isSpawnDelayed());
   ASSERT_TRUE(clientGame->isSpawnDelayed());         // Status should have propagated to client by now
   fillVector.clear();
   serverGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   ASSERT_EQ(1, fillVector.size());                   // Ship should have been killed off -- only 2nd player ship should be left
   fillVector.clear();
   clientGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   ASSERT_EQ(1, fillVector.size());
   //ASSERT_FALSE(clientGame2->findClientInfo("TestPlayerOne")->isSpawnDelayed());    // Check that other player knows our status

   // ReturnToGame penalty has been set, but won't start to count down until ship attempts to spawn
   ASSERT_FALSE(clientGame->inReturnToGameCountdown());
   ASSERT_TRUE(static_cast<FullClientInfo *>(serverGame->getClientInfo(0))->hasReturnToGamePenalty()); // Penalty has been primed
   ASSERT_EQ(0, serverGame->getClientInfo(0)->getReturnToGameTime());

   // Player presses a key to rejoin the game; there should be a SPAWN_UNDELAY_TIMER_DELAY ms penalty incurred for using /idle
   ASSERT_FALSE(serverGame->isSuspended()) << "Game is suspended -- subsequent tests will fail";
   clientGame->undelaySpawn();                                             // Simulate effects of key press
   gamePair.idle(10, 10);                                                  // Idle; give things time to propagate
   //for(S32 i = 0; i < 5; i++) clientGame2->idle(10);
   ASSERT_TRUE(serverGame->getClientInfo(0)->getReturnToGameTime() > 0);   // Timers should be set and counting down
   ASSERT_TRUE(clientGame->inReturnToGameCountdown());

   // Check to ensure ship didn't spawn -- spawn should be delayed until penalty period has expired
   fillVector.clear();
   serverGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   ASSERT_EQ(1, fillVector.size());   // (one ship for clientGame2) 
   fillVector.clear();
   clientGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   ASSERT_EQ(1, fillVector.size());
   //ASSERT_TRUE(clientGame2->findClientInfo("TestPlayerOne")->isSpawnDelayed());    // Check that other player knows our status

   // After some time has passed -- no longer in returnToGameCountdown period, ship should have appeared on server and client
   gamePair.idle(ClientInfo::SPAWN_UNDELAY_TIMER_DELAY / 100, 105);
   ASSERT_FALSE(clientGame->inReturnToGameCountdown());
   fillVector.clear();
   serverGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   ASSERT_EQ(2, fillVector.size());    
   fillVector.clear();
   clientGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   ASSERT_EQ(2, fillVector.size());

   // Cleanup -- remove second player from game
   clientGame2->getConnectionToServer()->disconnect(NetConnection::ReasonSelfDisconnect, "");
}


// Scenario 3, 4 -- Player enters /idle command, no other players, so server suspends itself
// In this case, no returnToGame penalty should be levied
// In this scenario 3, player un-idles during the suspend game timer countdown (there is a 2 second delay after all players are idle)
// In scenario 4, player enters full suspend mode before unidling
static void doScenario34(GamePair &gamePair, bool letGameSlipIntoFullSuspendMode)
{
   ClientGame *clientGame = gamePair.client;
   ServerGame *serverGame = gamePair.server;

   // Make sure we start off in a "normal" state
   ASSERT_FALSE(serverGame->isOrIsAboutToBeSuspended());
   ASSERT_FALSE(clientGame->isSpawnDelayed());         

   gamePair.idle(Ship::KillDeleteDelay / 15, 20);     // Idle; give things time to propagate
   fillVector.clear();
   serverGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   ASSERT_EQ(1, fillVector.size());                   // Now that player 2 has left, should only be one ship
   fillVector.clear();
   clientGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   ASSERT_EQ(1, fillVector.size());

   Vector<string> words;
   ChatCommands::idleHandler(clientGame, words);
   gamePair.idle(10, 10);     // Idle; give things time to propagate, timers to time out, etc.
   ASSERT_TRUE(serverGame->getClientInfo(0)->isSpawnDelayed());
   ASSERT_TRUE(serverGame->isOrIsAboutToBeSuspended());
   EXPECT_FALSE(serverGame->isSuspended());
   ASSERT_TRUE(clientGame->isSpawnDelayed());         // Status should have propagated to client by now
   ASSERT_TRUE(clientGame->isOrIsAboutToBeSuspended());
   EXPECT_FALSE(clientGame->isSuspended());

   fillVector.clear();
   serverGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   ASSERT_EQ(0, fillVector.size());                   // No ships remaining in game -- don't check client as it may have exploding ship there
   fillVector.clear();

   // ReturnToGame penalty has been set, but won't start to count down until ship attempts to spawn
   ASSERT_FALSE(clientGame->inReturnToGameCountdown());
   ASSERT_TRUE(static_cast<FullClientInfo *>(serverGame->getClientInfo(0))->hasReturnToGamePenalty()); // Penalty has been primed
   ASSERT_EQ(0, serverGame->getClientInfo(0)->getReturnToGameTime());

   if(letGameSlipIntoFullSuspendMode)
      gamePair.idle(Game::PreSuspendSettlingPeriod / 20, 25);

   // Player presses a key to rejoin the game; since game was suspended, player can resume without penalty
   ASSERT_TRUE(serverGame->isOrIsAboutToBeSuspended()) << "Game should be suspended";

   if(letGameSlipIntoFullSuspendMode)
      EXPECT_TRUE(serverGame->isSuspended());
   else
      EXPECT_FALSE(serverGame->isSuspended());

   clientGame->undelaySpawn();                                             // Simulate effects of key press
   gamePair.idle(10, 5);                                                   // Idle; give things time to propagate
   ASSERT_FALSE(serverGame->isOrIsAboutToBeSuspended());

   ASSERT_EQ(0, serverGame->getClientInfo(0)->getReturnToGameTime());      // No returnToGame penalty
   ASSERT_FALSE(clientGame->inReturnToGameCountdown());

   gamePair.idle(Ship::KillDeleteDelay / 15, 20);     // Idle; give dead ships time to be cleaned up

   // Check to ensure ship spawned
   fillVector.clear();
   serverGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   ASSERT_EQ(1, fillVector.size());   
   fillVector.clear();
   clientGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   ASSERT_EQ(1, fillVector.size());

   ASSERT_FALSE(serverGame->isOrIsAboutToBeSuspended());
   ASSERT_FALSE(clientGame->isSpawnDelayed());         
}


// Scenario 5 -- Player enters /idle when in punishment delay period for pervious /idle command
static void doScenario5(GamePair &gamePair)
{
   ClientGame *clientGame = gamePair.client;
   ServerGame *serverGame = gamePair.server;
}


// Scenario 6 -- Player is shown a new levelup screen
static void doScenario6(GamePair &gamePair)
{
   ClientGame *clientGame = gamePair.client;
   ServerGame *serverGame = gamePair.server;
}


// Scenario 7 -- Player is shown a levelup screen they have already seen
static void doScenario7(GamePair &gamePair)
{
   ClientGame *clientGame = gamePair.client;
   ServerGame *serverGame = gamePair.server;
}


// The spawnDelay mechansim is complex and interacts with other weird things like levelUp messages and server suspension
TEST(SpawnDelayTest, SpawnDelayTests)
{
   GamePair gamePair("");     // An empty level should work fine here
   ClientGame *clientGame = gamePair.client;
   ServerGame *serverGame = gamePair.server;


   // Idle for a while, let things settle
   gamePair.idle(10, 5);

   Vector<DatabaseObject *> fillVector;

   // Ship should have spawned by now... check for it on the client and server
   fillVector.clear();
   serverGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   ASSERT_EQ(1, fillVector.size());    // Ship should have spawned by now

   Ship *ship = static_cast<Ship *>(fillVector[0]);      // Server's copy of the ship

   fillVector.clear();
   clientGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   ASSERT_EQ(1, fillVector.size());    // And it should be on the client, too

   ASSERT_FALSE(clientGame->isSpawnDelayed());     // Should not be spawn-delayed at this point

   killShip(ship);

   gamePair.idle(10, 5);      // 5 cycles

   // Ship should have spawned by now... check for it on the client and server
   fillVector.clear();
   serverGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   ASSERT_EQ(1, fillVector.size());    //  Ship should have respawned and be available on the server...
   fillVector.clear();
   clientGame->getGameObjDatabase()->findObjects(PlayerShipTypeNumber, fillVector);
   ASSERT_EQ(1, fillVector.size());    // ...and also on the client

   // Scenario 1: Player is idle and gets suspended -- no other players in game
   doScenario1(gamePair);

   // Scenario 2: Player enters /idle command, other players, so server does not suspend itself.  Since
   // player used /idle command, a 5 second penalty will be levied against them.
   doScenario2(gamePair);

   // Scenarios 3 & 4 -- Player enters /idle command, no other players, so server suspends itself partially (3) or fully (4)
   // See https://code.google.com/p/googletest/wiki/AdvancedGuide#Using_Assertions_in_Sub-routines for details on this technique
   {
      SCOPED_TRACE("Scenario 3, letGameSlipIntoFullSuspendMode is false");
      doScenario34(gamePair, false);
   }
   {
      SCOPED_TRACE("Scenario 4, letGameSlipIntoFullSuspendMode is true");
      doScenario34(gamePair, true);
   }

   // Scenario 5 -- Player enters /idle when in punishment delay period for pervious /idle command
   doScenario5(gamePair);

   // Scenario 6 -- Player is shown a new levelup screen
   doScenario6(gamePair);

   // Scenario 7 -- Player is shown a levelup screen they have already seen
   doScenario7(gamePair);
}



}