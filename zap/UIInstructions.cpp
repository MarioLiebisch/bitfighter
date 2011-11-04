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

#include "UIInstructions.h"
#include "UIMenus.h"
#include "UIGame.h"
#include "UIChat.h"
#include "UIDiagnostics.h"
#include "ClientGame.h"
#include "gameObjectRender.h"
#include "ship.h"
#include "teleporter.h"
#include "EngineeredItem.h"
#include "input.h"
#include "speedZone.h"           // For SpeedZone::height
#include "GeomUtils.h"      // For polygon triangulation
#include "config.h"
#include "Colors.h"
#include "ScreenInfo.h"
#include "JoystickRender.h"

#include "SDL/SDL_opengl.h"

namespace Zap
{

// Constructor
InstructionsUserInterface::InstructionsUserInterface(ClientGame *game) : Parent(game)
{
   setMenuID(InstructionsUI);
}


void InstructionsUserInterface::onActivate()
{
   mCurPage = 1;
}


static const U32 NUM_PAGES = 12;

static const char *pageHeaders[] = {
   "CONTROLS",
   "LOADOUT SELECTION",
   "WEAPON PROJECTILES",
   "SPY BUGS",
   "GAME OBJECTS",
   "MORE GAME OBJECTS",
   "MORE GAME OBJECTS",
   "ADVANCED COMMANDS",
   "LEVEL COMMANDS",
   "ADMIN COMMANDS",
   "DEBUG COMMANDS",
   "SCRIPTING CONSOLE"
};


static const S32 FIRST_COMMAND_PAGE = 8;


static ControlStringsEditor consoleCommands1[] = {
   { "add <a> <b>", "Print a + b -- test command" },
   { "exit, quit", "Close the console" },
   { NULL, NULL },      // End of list
};


void InstructionsUserInterface::render()
{
   glColor3f(1,0,0);
   drawStringf(3, 3, 25, "INSTRUCTIONS - %s", pageHeaders[mCurPage - 1]);
   drawStringf(625, 3, 25, "PAGE %d/%d", mCurPage, NUM_PAGES);
   drawCenteredString(571, 20, "LEFT - previous page  RIGHT, SPACE - next page  ESC exits");
   glColor3f(0.7f, 0.7f, 0.7f);
   glBegin(GL_LINES);
      glVertex2f(0, 31);
      glVertex2f(800, 31);
      glVertex2f(0, 569);
      glVertex2f(800, 569);
   glEnd();

   switch(mCurPage)
   {
      case 1:
         renderPage1();
         break;
      case 2:
         renderPage2();
         break;
      case 3:
         renderPageObjectDesc(0);
         break;
      case 4:
         renderPageObjectDesc(1);
         break;
      case 5:
         renderPageObjectDesc(2);
         break;
      case 6:
         renderPageObjectDesc(3);
         break;
      case 7:
         renderPageObjectDesc(4);
         break;
      case 8:
         renderPageCommands(0);
         break;
      case 9:
         renderPageCommands(1, "Level change permissions are required to use these commands");     // Level control commands
         break;
      case 10:
         renderPageCommands(2, "Admin permissions are required to use these commands");            // Admin commands
         break;
      case 11:
         renderPageCommands(3);     // Debug commands
         break;
      case 12:
         renderConsoleCommands("Open the console by pressing [Shift]-[/] in game", consoleCommands1);   // Scripting console
         break;

      // When adding page, be sure to increase NUM_PAGES, and add item to pageHeaders array
   }
}


void InstructionsUserInterface::activateInCommandMode()
{
   this->activate();
   mCurPage = FIRST_COMMAND_PAGE;
}


struct ControlString
{
   const char *controlString;
   InputCode *primaryControlIndex;
};

InputCode dummyMouse = MOUSE;
InputCode dummyStickLeft = LEFT_JOYSTICK;
InputCode dummyStickRight = RIGHT_JOYSTICK;
InputCode dummyMoveShipKeysUD = KEYS_UP_DOWN;
InputCode dummyMoveShipKeysLR = KEYS_LEFT_RIGHT;
InputCode dummyMsgMode = KEY_CTRL_M;
InputCode dummySSMode = KEY_CTRL_Q;


static ControlString controlsKeyboard[] = {
         { "Move ship", &dummyMoveShipKeysUD},
         { " ", &dummyMoveShipKeysLR},
         { "Aim ship", &dummyMouse },
         { "Fire weapon", &inputFIRE[InputModeKeyboard] },
         { "Activate module 1", &inputMOD1[InputModeKeyboard] },
         { "Activate module 2", &inputMOD2[InputModeKeyboard] },
         { "-", NULL },
         { "Open ship config menu", &inputLOADOUT[InputModeKeyboard] },
         { "Toggle map view", &inputCMDRMAP[InputModeKeyboard] },
         { "Drop flag", &inputDROPITEM[InputModeKeyboard] },
         { "Show scoreboard", &inputSCRBRD[InputModeKeyboard] },
         { NULL, NULL },      // End col 1
         { "Cycle current weapon", &inputADVWEAP[InputModeKeyboard] },
         { "Select weapon 1", &inputSELWEAP1[InputModeKeyboard] },
         { "Select weapon 2", &inputSELWEAP2[InputModeKeyboard] },
         { "Select weapon 3", &inputSELWEAP3[InputModeKeyboard] },
         { "-", NULL },
         { "Open QuickChat menu", &inputQUICKCHAT[InputModeKeyboard] },
         { "Chat to team", &inputTEAMCHAT[InputModeKeyboard] },
         { "Chat to everyone", &inputGLOBCHAT[InputModeKeyboard] },
         { "Record voice chat", &inputTOGVOICE[InputModeKeyboard] },
         { "Message display mode", &dummyMsgMode },
         { "Save screenshot", &dummySSMode },
         { NULL, NULL },   // End col 2
      };

static ControlString controlsGamepad[] = {
         { "Move Ship", &dummyStickLeft },
         { "Aim Ship/Fire Weapon", &dummyStickRight },
         { "Activate module 1", &inputMOD1[InputModeJoystick] },
         { "Activate module 2", &inputMOD2[InputModeJoystick] },
         { "-", NULL },
         { "Open ship config menu", &inputLOADOUT[InputModeJoystick] },
         { "Toggle map view", &inputCMDRMAP[InputModeJoystick] },
         { "Drop flag", &inputDROPITEM[InputModeJoystick] },
         { "Show scoreboard", &inputSCRBRD[InputModeJoystick] },
         { NULL, NULL },
         { "Cycle current weapon", &inputADVWEAP[InputModeJoystick] },
         { "Select weapon 1", &inputSELWEAP1[InputModeJoystick] },
         { "Select weapon 2", &inputSELWEAP2[InputModeJoystick] },
         { "Select weapon 3", &inputSELWEAP3[InputModeJoystick] },
         { "-", NULL },
         { "Open QuickChat menu", &inputQUICKCHAT[InputModeJoystick] },
         { "Chat to team", &inputTEAMCHAT[InputModeJoystick] },
         { "Chat to everyone", &inputGLOBCHAT[InputModeJoystick] },
         { "Record voice chat", &inputTOGVOICE[InputModeJoystick] },
         { NULL, NULL },
      };



// This has become rather ugly and inelegant.  Sorry!
void InstructionsUserInterface::renderPage1()
{
   S32 canvasWidth = gScreenInfo.getGameCanvasWidth();
   S32 starty = 75;
   S32 y;
   S32 col1 = horizMargin;
   S32 col2 = horizMargin + canvasWidth / 4 + 45;     // +45 to make a little more room for Action col
   S32 col3 = horizMargin + canvasWidth / 2;
   S32 col4 = horizMargin + (canvasWidth * 3) / 4 + 45;
   S32 actCol = col1;      // Action column
   S32 contCol = col2;     // Control column
   bool firstCol = true;
   bool done = false;

   glBegin(GL_LINES);
      glVertex2i(col1, starty + 26);
      glVertex2i(750, starty + 26);
   glEnd();

   static const Color txtColor = Colors::cyan;
   static const Color keyColor = Colors::white;      // white
   static const Color secColor = Colors::yellow;

   ControlString *controls = (getGame()->getSettings()->getIniSettings()->inputMode == InputModeKeyboard) ? controlsKeyboard : controlsGamepad;

   glColor(secColor);
   drawString(col1, starty, 20, "Action");
   drawString(col2, starty, 20, "Control");
   drawString(col3, starty, 20, "Action");
   drawString(col4, starty, 20, "Control");

   y = starty + 28;
   for(S32 i = 0; !done; i++)              // Iterate over ControlStrings above
   {
      if(!controls[i].controlString)
      {
         if(!firstCol)
            done = true;
         else
         {  // Start second column
            firstCol = false;
            y = starty + 2;
            actCol = col3;
            contCol = col4;

            glColor(secColor);
         }
      }
      else if(!strcmp(controls[i].controlString, "-"))      // Horiz spacer
      {
         glColor3f(0.4f, 0.4f, 0.4f);
         glBegin(GL_LINES);
            glVertex2i(actCol, y + 13);
            glVertex2i(actCol + 335, y + 13);
         glEnd();
      }
      else
      {
         glColor(txtColor);
         drawString(actCol, y, 18, controls[i].controlString);      // Textual description of function (1st arg in lists above)

         // Some special cases
         //if((OptionsMenuUserInterface::joystickType == LogitechDualAction || OptionsMenuUserInterface::joystickType == PS2DualShock) && (i == 2))     // TODO: Test this...
         //   JoystickRender::renderControllerButton(col2, y + 4, BUTTON_7, false);

         //if((OptionsMenuUserInterface::joystickType == LogitechDualAction || OptionsMenuUserInterface::joystickType == PS2DualShock) && (i == 3))
         //   JoystickRender::renderControllerButton(col2, y + 4, BUTTON_8, false);

         glColor(keyColor);

         // We'll also handle special case of arrow keys...
         if (*controls[i].primaryControlIndex == KEYS_UP_DOWN)
         {     // (braces needed)
            if (inputLEFT[InputModeKeyboard] == KEY_LEFT && inputRIGHT[InputModeKeyboard] == KEY_RIGHT && inputUP[InputModeKeyboard] == KEY_UP && inputDOWN[InputModeKeyboard] == KEY_DOWN)
               drawString(contCol, y, 18, "Arrow Keys");
            else     // Center Up key above Down key
               drawStringf(contCol + getStringWidthf(15, "[%s] ", inputCodeToString(inputLEFT[InputModeKeyboard])), y + 4, 15, "[%s]", inputCodeToString(inputUP[InputModeKeyboard]));
         }
         else if (*controls[i].primaryControlIndex == KEYS_LEFT_RIGHT)
         {     // (braces needed)
            if (inputLEFT[InputModeKeyboard] == KEY_LEFT && inputRIGHT[InputModeKeyboard] == KEY_RIGHT && inputUP[InputModeKeyboard] == KEY_UP && inputDOWN[InputModeKeyboard] == KEY_DOWN)
               y -= 26;    // Hide this line
            else
               drawStringf(col2, y + 4, 15, "[%s] [%s] [%s]", inputCodeToString(inputLEFT[InputModeKeyboard]), inputCodeToString(inputDOWN[InputModeKeyboard]), inputCodeToString(inputRIGHT[InputModeKeyboard]));
         }
         else
            JoystickRender::renderControllerButton((F32)contCol, F32(y + 4), getGame()->getSettings()->getIniSettings()->joystickType, 
                                                   *controls[i].primaryControlIndex, false, 10);
      }

      y += 26;
   }

   y+=10;
   glColor(secColor);
   drawCenteredString(y, 20, "These special keys are also usually active:");
   y+=40;
   glColor(txtColor);
   drawString(col1, y, 18, "Help");
   glColor(keyColor);
   drawStringf(col2, y, 18, "[%s]", inputCodeToString(keyHELP));
   glColor(txtColor);
   drawString(col3, y, 18, "Universal Chat");
   glColor(keyColor);
   drawStringf(col4, y, 18, "[%s]", inputCodeToString(keyOUTGAMECHAT));
   y+=26;
   glColor(txtColor);
   drawString(col1, y, 18, "Mission");
   glColor(keyColor);
   drawStringf(col2, y, 18, "[%s]", inputCodeToString(keyMISSION));
   glColor(txtColor);
   drawString(col3, y, 18, "Display FPS / Lag");
   glColor(keyColor);
   drawStringf(col4, y, 18, "[%s]", inputCodeToString(keyFPS));
   y+=26;
   glColor(txtColor);
   drawString(col3, y, 18, "Diagnostics");
   glColor(keyColor);
   drawStringf(col4, y, 18, "[%s]", inputCodeToString(keyDIAG));

}

static const char *loadoutInstructions[] = {
   "Players can outfit their ships with 3 weapons and 2 modules.",      // TODO: Replace 3 & 2 w/constants
   "Pressing the ship configuration menu key brings up a menu that",
   "allows the player to choose the next loadout for his or her ship.",
   "This loadout will not be active on the ship until the player",
   "flies over a Loadout Zone area.  Modules include:",
};

static const char *moduleDescriptions[] = {
   "Boost - Boosts movement speed",
   "Shield - Reflects incoming projectiles",
   "Armor - Reduces damage, but makes ship harder to control",
   "Repair - Repairs self and nearby damaged objects",
   "Sensor - Increases visible distance and reveals hidden objects",
   "Cloak - Turns the ship invisible",
   "Engineer - Collect resources to build turrets and forcefields",
};

void InstructionsUserInterface::renderPage2()
{
   S32 y = 75;
   glColor3f(1,1,1);
   for(U32 i = 0; i < ARRAYSIZE(loadoutInstructions); i++)
   {
      drawCenteredString(y, 20, loadoutInstructions[i]);
      y += 26;
   }

   y += 30;
   for(S32 i = 0; i < 7; i++)
   {
      glColor(Colors::white);
      drawString(105, y, 20, moduleDescriptions[i]);
      glPushMatrix();
      glTranslatef(60, F32(y + 10), 0);
      glScale(0.7f);
      glRotatef(-90, 0, 0, 1);

      static F32 thrusts[4] =  { 1, 0, 0, 0 };
      static F32 thrustsBoost[4] =  { 1.3f, 0, 0, 0 };

      switch(i)
      {
         case 0:     // Boost
            renderShip(&Colors::blue, 1, thrustsBoost, 1, (F32)Ship::CollisionRadius, 0, false, false, false, false);
            glBegin(GL_LINES);
               glColor3f(1,1,0);
               glVertex2f(-20, -17);
               glColor(Colors::black);
               glVertex2f(-20, -50);
               glColor(Colors::yellow);
               glVertex2f(20, -17);
               glColor(Colors::black);
               glVertex2f(20, -50);
            glEnd();
            break;

         case 1:     // Shield
            renderShip(&Colors::blue, 1, thrusts, 1, (F32)Ship::CollisionRadius, 0, false, true, false, false);
            break;

         case 2:     // Armor
            renderShip(&Colors::blue, 1, thrusts, 1, (F32)Ship::CollisionRadius, 0, false, false, false, true);
            break;

         case 3:     // Repair
            {
               F32 health = (getGame()->getCurrentTime() & 0x7FF) * 0.0005f;

               renderShip(&Colors::blue, 1, thrusts, health, (F32)Ship::CollisionRadius, 0, false, false, false, false);
               glLineWidth(gLineWidth3);
               glColor(Colors::red);
               drawCircle(0, 0, Ship::RepairDisplayRadius);
               glLineWidth(gDefaultLineWidth);
            }
            break;

         case 4:     // Sensor
            renderShip(&Colors::blue, 1, thrusts, 1, (F32)Ship::CollisionRadius, getGame()->getCurrentTime(), false, false, true, false);
            break;

         case 5:     // Cloak
            {
               U32 time = getGame()->getCurrentTime();
               F32 frac = F32(time & 0x3FF);
               F32 alpha;
               if((time & 0x400) != 0)
                  alpha = frac * 0.001f;
               else
                  alpha = 1 - (frac * 0.001f);
               renderShip(&Colors::blue, alpha, thrusts, 1, (F32)Ship::CollisionRadius, 0, false, false, false, false);
            }
            break;

         case 6:     // Engineer
            {
               renderShip(&Colors::blue, 1, thrusts, 1, (F32)Ship::CollisionRadius, 0, false, false, false, false);
               renderResourceItem(Point(0,0));
            }
            break;
      }
      glPopMatrix();
      y += 50;
   }
}

const char *gGameObjectInfo[] = {
   /* 00 */   "Phaser", "The default weapon",
   /* 01 */   "Bouncer", "Bounces off walls",
   /* 03 */   "Triple", "Fires three diverging shots",
   /* 03 */   "Burst", "Explosive projectile",
   /* 04 */   "Friendly Mine", "Team's mines show trigger radius",
   /* 05 */   "Enemy Mine", "These are much harder to see",

   /* 06 */   "Friendly Spy Bug", "Lets you surveil the area",
   /* 07 */   "Enemy Spy Bug", "Destroy these when you find them",
   /* 08 */   "", "",
   /* 09 */   "", "",
   /* 10 */   "", "",
   /* 11 */   "", "",

   /* 12 */   "Repair Item", "Repairs damage to ship",
   /* 13 */   "Energy Item", "Restores ship's energy",
   /* 14 */   "Neutral Turret", "Repair to take team ownership",
   /* 15 */   "Active Turret", "Fires at enemy team",
   /* 16 */   "Neutral Emitter", "Repair to take team ownership",
   /* 17 */   "Force Field Emitter", "Allows only one team to pass",

   /* 18 */   "Teleporter", "Warps ship to another location",
   /* 19 */   "Flag", "Objective item in some game types",
   /* 20 */   "Loadout Zone", "Updates ship configuration",
   /* 21 */   "Nexus", "Bring flags here in Nexus game",
   /* 22 */   "Asteroid", "Silent but deadly",
   /* 23 */   "GoFast", "Makes ship go fast",

   /* 24 */   "Test Item", "Bouncy ball",
   /* 25 */   "Resource Item", "Use with engineer module",
   /* 26 */   "Soccer Ball", "Push into enemy goal in Soccer game",
};
static U32 GameObjectCount = 27;      // <=== If you add something above, increment this!


void InstructionsUserInterface::renderPageObjectDesc(U32 index)
{
   U32 objectsPerPage = 6;
   U32 startIndex = index * objectsPerPage;
   U32 endIndex = startIndex + objectsPerPage;
   if(endIndex > GameObjectCount)
      endIndex = GameObjectCount;

   for(U32 i = startIndex; i < endIndex; i++)
   {
      const char *text = gGameObjectInfo[i * 2];
      const char *desc = gGameObjectInfo[i * 2 + 1];
      S32 index = i - startIndex;

      Point objStart((index & 1) * 400, (index >> 1) * 165);
      objStart += Point(200, 90);
      Point start = objStart + Point(0, 55);

      glColor3f(1,1,0);
      renderCenteredString(start, (S32)20, text);

      glColor3f(1,1,1);
      renderCenteredString(start + Point(0, 25), (S32)17, desc);

      glPushMatrix();
      glTranslate(objStart);
      glScale(0.7f);


      // TODO: Do this once, elsewhere
      Vector<Point> speedZoneRenderPoints;
      SpeedZone::generatePoints(Point(-SpeedZone::height / 2, 0), Point(1, 0), 1, speedZoneRenderPoints);

      switch(i)
      {
         case 0:
            renderProjectile(Point(0, 0), 0, getGame()->getCurrentTime());
            break;
         case 1:
            renderProjectile(Point(0, 0), 1, getGame()->getCurrentTime());
            break;
         case 2:
            renderProjectile(Point(0, 0), 2, getGame()->getCurrentTime());
            break;
         case 3:
            renderGrenade(Point(0, 0), 1, getGame()->getSettings()->getIniSettings()->burstGraphicsMode);
            break;
         case 4:
            renderMine(Point(0, 0), true, true);
            break;
         case 5:
            renderMine(Point(0, 0), true, false);
            break;
         case 6:
            renderSpyBug(Point(0, 0), Colors::blue, true, true);
            break;
         case 7:
            renderSpyBug(Point(0, 0), Colors::blue, false, true);
            break;
         case 8:     // Blank
         case 9:
         case 10:
         case 11:
            break;
         case 12:
            renderRepairItem(Point(0, 0));
            break;
         case 13:
            renderEnergyItem(Point(0, 0));
            break;
         case 14:
            renderTurret(Colors::white, Point(0, 15), Point(0, -1), false, 0, 0);
            break;
         case 15:
            renderTurret(Colors::blue, Point(0, 15), Point(0, -1), true, 1, 0);
            break;

         case 16:
            renderForceFieldProjector(Point(-7.5, 0), Point(1, 0), &Colors::white, false);
            break;
         case 17:
            renderForceFieldProjector(Point(-50, 0), Point(1, 0), &Colors::red, true);
            renderForceField(Point(-35, 0), Point(50, 0), Colors::red, true);
            break;
         case 18:
            renderTeleporter(Point(0, 0), 0, true, getGame()->getCurrentTime(), 1, 1, (F32)Teleporter::TELEPORTER_RADIUS, 1, Vector<Point>(), false);
            break;
         case 19:
            renderFlag(0, 0, &Colors::red);
            break;
         case 20:    // Loadout zone
            {              // braces needed: see C2360
               Vector<Point> o;     // outline
               o.push_back(Point(-150, -30));
               o.push_back(Point(150, -30));
               o.push_back(Point(150, 30));
               o.push_back(Point(-150, 30));

               Vector<Point> f;     // fill
               Triangulate::Process(o, f);

               renderLoadoutZone(&Colors::blue, &o, &f, findCentroid(o), angleOfLongestSide(o));
            }

            break;

         case 21:    // Nexus
            {
               Vector<Point> o;     // outline
               o.push_back(Point(-150, -30));
               o.push_back(Point(150, -30));
               o.push_back(Point(150, 30));
               o.push_back(Point(-150, 30));

               Vector<Point> f;     // fill
               Triangulate::Process(o, f);

               renderNexus(&o, &f, findCentroid(o), angleOfLongestSide(o), 
                                       getGame()->getCurrentTime() % 5000 > 2500, 0);
            }
            break;

         case 22:    // Asteroid... using goofball factor to keep out of sync with Nexus graphic
            renderAsteroid(Point(0,-10), 
                     (S32)(getGame()->getCurrentTime() / 2891) % Asteroid::getDesignCount(), .7f);    
            break;

         case 23:    // SpeedZone
            renderSpeedZone(&speedZoneRenderPoints, getGame()->getCurrentTime());
            break;

         case 24:    // TestItem
            renderTestItem(Point(0,0));
            break;

         case 25:    // ResourceItem
            renderResourceItem(Point(0,0));
            break;

         case 26:    // SoccerBall
            renderSoccerBall(Point(0,0));
            break;
      }
      glPopMatrix();
      objStart.y += 75;
      start.y += 75;
   }
}


extern CommandInfo chatCmds[];
extern S32 chatCmdSize;

void InstructionsUserInterface::renderPageCommands(U32 page, const char *msg)
{
   TNLAssert(page < COMMAND_CATEGORIES, "Page too high!");

   S32 ypos = 50;

   S32 cmdCol = horizMargin;                                                         // Action column
   S32 descrCol = horizMargin + S32(gScreenInfo.getGameCanvasWidth() * 0.25) + 55;   // Control column

   const S32 instrSize = 18;

   glColor3f(0,1,0);
   drawStringf(cmdCol, ypos, instrSize, "Enter a cmd by pressing [%s], or by typing one at the chat prompt", 
                                        inputCodeToString(inputCMDCHAT[getGame()->getSettings()->getIniSettings()->inputMode]));
   ypos += 28;
   drawString(cmdCol, ypos, instrSize, "Use [TAB] to expand a partially typed command");
   ypos += 28;

   if(strcmp(msg, ""))
   {
      drawString(cmdCol, ypos, instrSize, msg);
      ypos += 28;
   }

   Color cmdColor =   Colors::cyan;
   Color descrColor = Colors::white;
   Color secColor =   Colors::yellow;

   const S32 headerSize = 20;
   const S32 cmdSize = 16;
   const S32 cmdGap = 10;

   glColor(secColor);
   drawString(cmdCol, ypos, headerSize, "Command");
   drawString(descrCol, ypos, headerSize, "Description");

   //glColor3f(0,1,0);
   ypos += cmdSize + cmdGap;
   glBegin(GL_LINES);
      glVertex2i(cmdCol, ypos);
      glVertex2i(750, ypos);
   glEnd();

   ypos += 5;     // Small gap before cmds start


   bool first = true;
   S32 section = -1;

   for(S32 i = 0; i < chatCmdSize && U32(chatCmds[i].helpCategory) <= page; i++)
   {
      if(U32(chatCmds[i].helpCategory) < page)
         continue;

      if(first)
      {
         section = chatCmds[i].helpGroup;
         first = false;
      }

      // Check if we've just changed sections... if so, draw a horizontal line ----------
      if(chatCmds[i].helpGroup > section)      
      {
         glColor3f(0.4f, 0.4f, 0.4f);
         glBegin(GL_LINES);
            glVertex2i(cmdCol, ypos + (cmdSize + cmdGap) / 3);
            glVertex2i(cmdCol + 335, ypos + (cmdSize + cmdGap) / 3);
         glEnd();

         section = chatCmds[i].helpGroup;

         ypos += cmdSize + cmdGap;
      }

      glColor(cmdColor);
      
      // Assemble command & args from data in the chatCmds struct
      string cmdString = "/" + chatCmds[i].cmdName;

      for(S32 j = 0; j < chatCmds[i].cmdArgCount; j++)
         cmdString += " " + chatCmds[i].helpArgString[j];

      drawString(cmdCol, ypos, cmdSize, cmdString.c_str());      
      glColor(descrColor);
      drawString(descrCol, ypos, cmdSize, chatCmds[i].helpTextString.c_str());

      ypos += cmdSize + cmdGap;
   }
}


void InstructionsUserInterface::nextPage()
{
   mCurPage++;
   if(mCurPage > NUM_PAGES)
      mCurPage = 1;
}


void InstructionsUserInterface::prevPage()
{
   if(mCurPage > 1)
      mCurPage--;
   else
      mCurPage = NUM_PAGES;
}


void InstructionsUserInterface::exitInstructions()
{
   playBoop();
    getUIManager()->reactivatePrevUI();      //mGameUserInterface
}


void InstructionsUserInterface::onKeyDown(InputCode inputCode, char ascii)
{
   if(inputCode == KEY_LEFT || inputCode == BUTTON_DPAD_LEFT || inputCode == BUTTON_DPAD_UP || inputCode == KEY_UP)
   {
      playBoop();
      prevPage();
   }
   else if(inputCode == KEY_RIGHT || inputCode == KEY_SPACE || inputCode == BUTTON_DPAD_RIGHT ||
           inputCode == BUTTON_DPAD_DOWN || inputCode == KEY_ENTER || inputCode == KEY_DOWN)
   {
      playBoop();
      nextPage();
   }
   else if(inputCode == keyOUTGAMECHAT)     // Turn on Global Chat overlay
      getUIManager()->getChatUserInterface()->activate();
   else if(inputCode == keyDIAG)            // Turn on diagnostic overlay
      getUIManager()->getDiagnosticUserInterface()->activate();
   else if(inputCode == keyHELP || inputCode == KEY_ESCAPE  || inputCode == BUTTON_BACK)
      exitInstructions();
}

};
