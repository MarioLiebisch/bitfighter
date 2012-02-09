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

#include "InputCode.h"

#include "../tnl/tnlJournal.h"
#include "../tnl/tnlLog.h"         // For logprintf

#include "zapjournal.h"            // For journaling support

#include <ctype.h>

#ifndef ZAP_DEDICATED
#include "Joystick.h"
#include "SDL/SDL.h"
#endif

#ifdef TNL_OS_WIN32
#include <windows.h>   // For ARRAYSIZE 
#endif


using namespace TNL;

namespace Zap
{

static bool inputCodeIsDown[MAX_INPUT_CODES];


// Constructor
InputCodeManager::InputCodeManager()
{
   keyHELP = KEY_F1;              // Display help
   keyOUTGAMECHAT = KEY_F5;       // Out of game chat
   keyFPS = KEY_F6;               // Show FPS display
   keyDIAG = KEY_F7;              // Show diagnostic overlay
   keyMISSION = KEY_F2;           // Show current mission info
}


// Initialize state of keys... assume none are depressed, or even sad
void InputCodeManager::resetStates()
{
   for(int i = 0; i < MAX_INPUT_CODES; i++)
      inputCodeIsDown[i] = false;
}

// Prints list of any input codes that are down, for debugging purposes
void InputCodeManager::dumpInputCodeStates()
{
  for(S32 i = 0; i < MAX_INPUT_CODES; i++)
     if(inputCodeIsDown[i])
        logprintf("Key %s down", inputCodeToString((InputCode) i));
}


// Set state of a input code as Up (false) or Down (true)
void InputCodeManager::setState(InputCode inputCode, bool state)
{  
   inputCodeIsDown[(S32)inputCode] = state;
}


// Returns true of input code is down, false if it is up
bool InputCodeManager::getState(InputCode inputCode)
{
//   logprintf("State: key %d is %s", inputCode, keyIsDown[(int) inputCode] ? "down" : "up");
   return inputCodeIsDown[(S32)inputCode];
}


static const InputCode modifiers[] = { KEY_CTRL, KEY_ALT, KEY_SHIFT, KEY_META, KEY_SUPER };

string InputCodeManager::makeInputString(InputCode inputCode)
{
   for(S32 i = 0; i < S32(ARRAYSIZE(modifiers)); i++)
      if(inputCode == modifiers[i])
         return "";

   string inputString = "";
   string joiner = "+";

   for(S32 i = 0; i < S32(ARRAYSIZE(modifiers)); i++)
      if(getState(modifiers[i]))
         inputString += inputCodeToString(modifiers[i]) + joiner;
   
   inputString += inputCodeToString(inputCode);
   return inputString;
}


// Can pass in one of the above, or KEY_NONE to check if no modifiers are pressed
bool InputCodeManager::checkModifier(InputCode mod1)    
{
   S32 foundCount = 0;

   for(S32 i = 0; i < S32(ARRAYSIZE(modifiers)); i++)
      if(getState(modifiers[i]))                      // Modifier is down
      {
         if(modifiers[i] == mod1)      
            foundCount++;
         else                                                  // Wrong modifier!               
            return false;        
      }

   return mod1 == KEY_NONE || foundCount == 1;
}


// Check if two modifiers are both pressed (i.e. Ctrl+Alt)
bool InputCodeManager::checkModifier(InputCode mod1, InputCode mod2)
{
   S32 foundCount = 0;

   for(S32 i = 0; i < S32(ARRAYSIZE(modifiers)); i++)
      if(getState(modifiers[i]))                      // Modifier is down
      {
         if(modifiers[i] == mod1 || modifiers[i] == mod2)      
            foundCount++;
         else                                                  // Wrong modifier!               
            return false;        
      }

   return foundCount == 2;
}


// Check to see if three modifers are all pressed (i.e. Ctrl+Alt+Shift)
bool InputCodeManager::checkModifier(InputCode mod1, InputCode mod2, InputCode mod3)
{
   S32 foundCount = 0;

   for(S32 i = 0; i < S32(ARRAYSIZE(modifiers)); i++)
      if(getState(modifiers[i]))                      // Modifier is down
      {
         if(modifiers[i] == mod1 || modifiers[i] == mod2 || modifiers[i] == mod3)      
            foundCount++;
         else                                                  // Wrong modifier!               
            return false;        
      }

   return foundCount == 3;
}


// Not static -- used externally
bool isPrintable(char c)
{
   return c >= 32 && c <= 126;
}


// If there is a printable ASCII code for the pressed key, return it
// Filter out some know spurious keystrokes
char InputCodeManager::keyToAscii(int unicode, InputCode inputCode)
{
   //if(inputCode == KEY_UP || inputCode == KEY_DOWN || inputCode == KEY_LEFT || inputCode == KEY_RIGHT)
   //   return 0;

   if((unicode & 0xFF80) != 0) 
      return 0;

   char ch = unicode & 0x7F;

   return isPrintable(ch) ? ch : 0;
}


InputCode InputCodeManager::getBinding(BindingName bindingName, InputMode inputMode)
{
   S32 mode = (S32)inputMode;

   switch(bindingName)
   {
      case BINDING_SELWEAP1:
	      return inputSELWEAP1[mode];
      case BINDING_SELWEAP2:
	      return inputSELWEAP2[mode];
      case BINDING_SELWEAP3:
	      return inputSELWEAP3[mode];
      case BINDING_ADVWEAP:
	      return inputADVWEAP[mode];
      case BINDING_CMDRMAP:
	      return inputCMDRMAP[mode];
      case BINDING_TEAMCHAT:
	      return inputTEAMCHAT[mode];
      case BINDING_GLOBCHAT:
	      return inputGLOBCHAT[mode];
      case BINDING_QUICKCHAT:
	      return inputQUICKCHAT[mode];
      case BINDING_CMDCHAT:
	      return inputCMDCHAT[mode];
      case BINDING_LOADOUT:
	      return inputLOADOUT[mode];
      case BINDING_MOD1:
	      return inputMOD1[mode];
      case BINDING_MOD2:
	      return inputMOD2[mode];
      case BINDING_FIRE:
	      return inputFIRE[mode];
      case BINDING_DROPITEM:
	      return inputDROPITEM[mode];
      case BINDING_TOGVOICE:
	      return inputTOGVOICE[mode];
      case BINDING_UP:
	      return inputUP[mode];
      case BINDING_DOWN:
	      return inputDOWN[mode];
      case BINDING_LEFT:
	      return inputLEFT[mode];
      case BINDING_RIGHT:
	      return inputRIGHT[mode];
      case BINDING_SCRBRD:
	      return inputSCRBRD[mode];
      case BINDING_HELP:
	      return keyHELP;
      case BINDING_OUTGAMECHAT:
	      return keyOUTGAMECHAT;
      case BINDING_MISSION:
	      return keyMISSION;
      case BINDING_FPS:
	      return keyFPS;
      case BINDING_DIAG:
	      return keyDIAG;
      case BINDING_DUMMY_MOVE_SHIP_KEYS_MOUSE:
         return MOUSE;
      case BINDING_DUMMY_MOVE_SHIP_KEYS_UD:
         return KEYS_UP_DOWN;
      case BINDING_DUMMY_MOVE_SHIP_KEYS_LR:
         return KEYS_LEFT_RIGHT;
      case BINDING_DUMMY_STICK_LEFT:
         return LEFT_JOYSTICK;
      case BINDING_DUMMY_STICK_RIGHT:
         return RIGHT_JOYSTICK;
      case BINDING_DUMMY_MSG_MODE:
         return KEY_CTRL_M;
      case BINDING_DUMMY_SS_MODE:
         return KEY_CTRL_Q;
         
   //         MOUSE, LEFT_JOYSTICK, RIGHT_JOYSTICK,     // Not exactly keys, but helpful to have in here!
   //KEYS_UP_DOWN, KEYS_LEFT_RIGHT,            // These are here because we need a dummy InputCode item in the instructions
   //KEY_CTRL_M, KEY_CTRL_Q, KEY_CTRL_S,
         
      default:
         TNLAssert(false, "Invalid key binding!");
         return KEY_NONE;
   }
}


void InputCodeManager::setBinding(BindingName bindingName, InputMode inputMode, InputCode key)
{
   S32 mode = (S32)inputMode;

   switch(bindingName)
   {
      case BINDING_SELWEAP1:
	      inputSELWEAP1[mode] = key;
         break;
      case BINDING_SELWEAP2:
	      inputSELWEAP2[mode] = key;
         break;
      case BINDING_SELWEAP3:
	      inputSELWEAP3[mode] = key;
         break;
      case BINDING_ADVWEAP:
	      inputADVWEAP[mode] = key;
         break;
      case BINDING_CMDRMAP:
	      inputCMDRMAP[mode] = key;
         break;
      case BINDING_TEAMCHAT:
	      inputTEAMCHAT[mode] = key;
         break;
      case BINDING_GLOBCHAT:
	      inputGLOBCHAT[mode] = key;
         break;
      case BINDING_QUICKCHAT:
	      inputQUICKCHAT[mode] = key;
         break;
      case BINDING_CMDCHAT:
	      inputCMDCHAT[mode] = key;
         break;
      case BINDING_LOADOUT:
	      inputLOADOUT[mode] = key;
         break;
      case BINDING_MOD1:
	      inputMOD1[mode] = key;
         break;
      case BINDING_MOD2:
	      inputMOD2[mode] = key;
         break;
      case BINDING_FIRE:
	      inputFIRE[mode] = key;
         break;
      case BINDING_DROPITEM:
	      inputDROPITEM[mode] = key;
         break;
      case BINDING_TOGVOICE:
	      inputTOGVOICE[mode] = key;
         break;
      case BINDING_UP:
	      inputUP[mode] = key;
         break;
      case BINDING_DOWN:
	      inputDOWN[mode] = key;
         break;
      case BINDING_LEFT:
	      inputLEFT[mode] = key;
         break;
      case BINDING_RIGHT:
	      inputRIGHT[mode] = key;
         break;
      case BINDING_SCRBRD:
	      inputSCRBRD[mode] = key;
         break;
      case BINDING_HELP:
	      keyHELP = key;
         break;
      case BINDING_OUTGAMECHAT:
	      keyOUTGAMECHAT = key;
         break;
      case BINDING_MISSION:
	      keyMISSION = key;
         break;
      case BINDING_FPS:
	      keyFPS = key;
         break;
      case BINDING_DIAG:
	      keyDIAG = key;
         break;
      default:
         TNLAssert(false, "Invalid key binding!");
   }
}



#ifndef ZAP_DEDICATED
// Translate SDL standard keys to our InputCodes
InputCode InputCodeManager::sdlKeyToInputCode(int key)
{
   switch(key)
   {
	   case SDLK_BACKSPACE:
		   return KEY_BACKSPACE;
	   case SDLK_TAB:
		   return KEY_TAB;
	   case SDLK_CLEAR:
		   return KEY_CLEAR;
	   case SDLK_RETURN:
		   return KEY_ENTER;
	   case SDLK_PAUSE:
		   return KEY_PAUSE;
	   case SDLK_ESCAPE:
		   return KEY_ESCAPE;
	   case SDLK_SPACE:
		   return KEY_SPACE;
	   case SDLK_EXCLAIM:
		   return KEY_EXCLAIM;
	   case SDLK_QUOTEDBL:
		   return KEY_DOUBLEQUOTE;
	   case SDLK_HASH:
		   return KEY_HASH;
	   case SDLK_DOLLAR:
		   return KEY_DOLLAR;
	   case SDLK_AMPERSAND:
		   return KEY_AMPERSAND;
	   case SDLK_QUOTE:
		   return KEY_QUOTE;
	   case SDLK_LEFTPAREN:
		   return KEY_OPENPAREN;
	   case SDLK_RIGHTPAREN:
		   return KEY_CLOSEPAREN;
	   case SDLK_ASTERISK:
		   return KEY_ASTERISK;
	   case SDLK_PLUS:
		   return KEY_PLUS;
	   case SDLK_COMMA:
		   return KEY_COMMA;
	   case SDLK_MINUS:
		   return KEY_MINUS;
	   case SDLK_PERIOD:
		   return KEY_PERIOD;
	   case SDLK_SLASH:
		   return KEY_SLASH;
	   case SDLK_0:
		   return KEY_0;
	   case SDLK_1:
		   return KEY_1;
	   case SDLK_2:
		   return KEY_2;
	   case SDLK_3:
		   return KEY_3;
	   case SDLK_4:
		   return KEY_4;
	   case SDLK_5:
		   return KEY_5;
	   case SDLK_6:
		   return KEY_6;
	   case SDLK_7:
		   return KEY_7;
	   case SDLK_8:
		   return KEY_8;
	   case SDLK_9:
		   return KEY_9;
	   case SDLK_COLON:
		   return KEY_COLON;
	   case SDLK_SEMICOLON:
		   return KEY_SEMICOLON;
	   case SDLK_LESS:
		   return KEY_LESS;
	   case SDLK_EQUALS:
		   return KEY_EQUALS;
	   case SDLK_GREATER:
		   return KEY_GREATER;
	   case SDLK_QUESTION:
		   return KEY_QUESTION;
	   case SDLK_AT:
		   return KEY_AT;
   	
	   case SDLK_LEFTBRACKET:
		   return KEY_OPENBRACKET;
	   case SDLK_BACKSLASH:
		   return KEY_BACKSLASH;
	   case SDLK_RIGHTBRACKET:
		   return KEY_CLOSEBRACKET;
	   case SDLK_CARET:
		   return KEY_CARET;
	   case SDLK_UNDERSCORE:
		   return KEY_UNDERSCORE;
	   case SDLK_BACKQUOTE:
		   return KEY_BACKQUOTE;
	   case SDLK_a:
		   return KEY_A;
	   case SDLK_b:
		   return KEY_B;
	   case SDLK_c:
		   return KEY_C;
	   case SDLK_d:
		   return KEY_D;
	   case SDLK_e:
		   return KEY_E;
	   case SDLK_f:
		   return KEY_F;
	   case SDLK_g:
		   return KEY_G;
	   case SDLK_h:
		   return KEY_H;
	   case SDLK_i:
		   return KEY_I;
	   case SDLK_j:
		   return KEY_J;
	   case SDLK_k:
		   return KEY_K;
	   case SDLK_l:
		   return KEY_L;
	   case SDLK_m:
		   return KEY_M;
	   case SDLK_n:
		   return KEY_N;
	   case SDLK_o:
		   return KEY_O;
	   case SDLK_p:
		   return KEY_P;
	   case SDLK_q:
		   return KEY_Q;
	   case SDLK_r:
		   return KEY_R;
	   case SDLK_s:
		   return KEY_S;
	   case SDLK_t:
		   return KEY_T;
	   case SDLK_u:
		   return KEY_U;
	   case SDLK_v:
		   return KEY_V;
	   case SDLK_w:
		   return KEY_W;
	   case SDLK_x:
		   return KEY_X;
	   case SDLK_y:
		   return KEY_Y;
	   case SDLK_z:
		   return KEY_Z;
	   case SDLK_DELETE:
		   return KEY_DELETE;

	   // International keyboard syms
	   case SDLK_WORLD_0:
		   return KEY_WORLD_0;
	   case SDLK_WORLD_1:
		   return KEY_WORLD_1;
	   case SDLK_WORLD_2:
		   return KEY_WORLD_2;
	   case SDLK_WORLD_3:
		   return KEY_WORLD_3;
	   case SDLK_WORLD_4:
		   return KEY_WORLD_4;
	   case SDLK_WORLD_5:
		   return KEY_WORLD_5;
	   case SDLK_WORLD_6:
		   return KEY_WORLD_6;
	   case SDLK_WORLD_7:
		   return KEY_WORLD_7;
	   case SDLK_WORLD_8:
		   return KEY_WORLD_8;
	   case SDLK_WORLD_9:
		   return KEY_WORLD_9;
	   case SDLK_WORLD_10:
		   return KEY_WORLD_10;
	   case SDLK_WORLD_11:
		   return KEY_WORLD_11;
	   case SDLK_WORLD_12:
		   return KEY_WORLD_12;
	   case SDLK_WORLD_13:
		   return KEY_WORLD_13;
	   case SDLK_WORLD_14:
		   return KEY_WORLD_14;
	   case SDLK_WORLD_15:
		   return KEY_WORLD_15;
	   case SDLK_WORLD_16:
		   return KEY_WORLD_16;
	   case SDLK_WORLD_17:
		   return KEY_WORLD_17;
	   case SDLK_WORLD_18:
		   return KEY_WORLD_18;
	   case SDLK_WORLD_19:
		   return KEY_WORLD_19;
	   case SDLK_WORLD_20:
		   return KEY_WORLD_20;
	   case SDLK_WORLD_21:
		   return KEY_WORLD_21;
	   case SDLK_WORLD_22:
		   return KEY_WORLD_22;
	   case SDLK_WORLD_23:
		   return KEY_WORLD_23;
	   case SDLK_WORLD_24:
		   return KEY_WORLD_24;
	   case SDLK_WORLD_25:
		   return KEY_WORLD_25;
	   case SDLK_WORLD_26:
		   return KEY_WORLD_26;
	   case SDLK_WORLD_27:
		   return KEY_WORLD_27;
	   case SDLK_WORLD_28:
		   return KEY_WORLD_28;
	   case SDLK_WORLD_29:
		   return KEY_WORLD_29;
	   case SDLK_WORLD_30:
		   return KEY_WORLD_30;
	   case SDLK_WORLD_31:
		   return KEY_WORLD_31;
	   case SDLK_WORLD_32:
		   return KEY_WORLD_32;
	   case SDLK_WORLD_33:
		   return KEY_WORLD_33;
	   case SDLK_WORLD_34:
		   return KEY_WORLD_34;
	   case SDLK_WORLD_35:
		   return KEY_WORLD_35;
	   case SDLK_WORLD_36:
		   return KEY_WORLD_36;
	   case SDLK_WORLD_37:
		   return KEY_WORLD_37;
	   case SDLK_WORLD_38:
		   return KEY_WORLD_38;
	   case SDLK_WORLD_39:
		   return KEY_WORLD_39;
	   case SDLK_WORLD_40:
		   return KEY_WORLD_40;
	   case SDLK_WORLD_41:
		   return KEY_WORLD_41;
	   case SDLK_WORLD_42:
		   return KEY_WORLD_42;
	   case SDLK_WORLD_43:
		   return KEY_WORLD_43;
	   case SDLK_WORLD_44:
		   return KEY_WORLD_44;
	   case SDLK_WORLD_45:
		   return KEY_WORLD_45;
	   case SDLK_WORLD_46:
		   return KEY_WORLD_46;
	   case SDLK_WORLD_47:
		   return KEY_WORLD_47;
	   case SDLK_WORLD_48:
		   return KEY_WORLD_48;
	   case SDLK_WORLD_49:
		   return KEY_WORLD_49;
	   case SDLK_WORLD_50:
		   return KEY_WORLD_50;
	   case SDLK_WORLD_51:
		   return KEY_WORLD_51;
	   case SDLK_WORLD_52:
		   return KEY_WORLD_52;
	   case SDLK_WORLD_53:
		   return KEY_WORLD_53;
	   case SDLK_WORLD_54:
		   return KEY_WORLD_54;
	   case SDLK_WORLD_55:
		   return KEY_WORLD_55;
	   case SDLK_WORLD_56:
		   return KEY_WORLD_56;
	   case SDLK_WORLD_57:
		   return KEY_WORLD_57;
	   case SDLK_WORLD_58:
		   return KEY_WORLD_58;
	   case SDLK_WORLD_59:
		   return KEY_WORLD_59;
	   case SDLK_WORLD_60:
		   return KEY_WORLD_60;
	   case SDLK_WORLD_61:
		   return KEY_WORLD_61;
	   case SDLK_WORLD_62:
		   return KEY_WORLD_62;
	   case SDLK_WORLD_63:
		   return KEY_WORLD_63;
	   case SDLK_WORLD_64:
		   return KEY_WORLD_64;
	   case SDLK_WORLD_65:
		   return KEY_WORLD_65;
	   case SDLK_WORLD_66:
		   return KEY_WORLD_66;
	   case SDLK_WORLD_67:
		   return KEY_WORLD_67;
	   case SDLK_WORLD_68:
		   return KEY_WORLD_68;
	   case SDLK_WORLD_69:
		   return KEY_WORLD_69;
	   case SDLK_WORLD_70:
		   return KEY_WORLD_70;
	   case SDLK_WORLD_71:
		   return KEY_WORLD_71;
	   case SDLK_WORLD_72:
		   return KEY_WORLD_72;
	   case SDLK_WORLD_73:
		   return KEY_WORLD_73;
	   case SDLK_WORLD_74:
		   return KEY_WORLD_74;
	   case SDLK_WORLD_75:
		   return KEY_WORLD_75;
	   case SDLK_WORLD_76:
		   return KEY_WORLD_76;
	   case SDLK_WORLD_77:
		   return KEY_WORLD_77;
	   case SDLK_WORLD_78:
		   return KEY_WORLD_78;
	   case SDLK_WORLD_79:
		   return KEY_WORLD_79;
	   case SDLK_WORLD_80:
		   return KEY_WORLD_80;
	   case SDLK_WORLD_81:
		   return KEY_WORLD_81;
	   case SDLK_WORLD_82:
		   return KEY_WORLD_82;
	   case SDLK_WORLD_83:
		   return KEY_WORLD_83;
	   case SDLK_WORLD_84:
		   return KEY_WORLD_84;
	   case SDLK_WORLD_85:
		   return KEY_WORLD_85;
	   case SDLK_WORLD_86:
		   return KEY_WORLD_86;
	   case SDLK_WORLD_87:
		   return KEY_WORLD_87;
	   case SDLK_WORLD_88:
		   return KEY_WORLD_88;
	   case SDLK_WORLD_89:
		   return KEY_WORLD_89;
	   case SDLK_WORLD_90:
		   return KEY_WORLD_90;
	   case SDLK_WORLD_91:
		   return KEY_WORLD_91;
	   case SDLK_WORLD_92:
		   return KEY_WORLD_92;
	   case SDLK_WORLD_93:
		   return KEY_WORLD_93;
	   case SDLK_WORLD_94:
		   return KEY_WORLD_94;
	   case SDLK_WORLD_95:
		   return KEY_WORLD_95;

	   // Numeric keypad
	   case SDLK_KP0:
		   return KEY_KEYPAD0;
	   case SDLK_KP1:
		   return KEY_KEYPAD1;
	   case SDLK_KP2:
		   return KEY_KEYPAD2;
	   case SDLK_KP3:
		   return KEY_KEYPAD3;
	   case SDLK_KP4:
		   return KEY_KEYPAD4;
	   case SDLK_KP5:
		   return KEY_KEYPAD5;
	   case SDLK_KP6:
		   return KEY_KEYPAD6;
	   case SDLK_KP7:
		   return KEY_KEYPAD7;
	   case SDLK_KP8:
		   return KEY_KEYPAD8;
	   case SDLK_KP9:
		   return KEY_KEYPAD9;
	   case SDLK_KP_PERIOD:
		   return KEY_KEYPAD_PERIOD;
	   case SDLK_KP_DIVIDE:
		   return KEY_KEYPAD_DIVIDE;
	   case SDLK_KP_MULTIPLY:
		   return KEY_KEYPAD_MULTIPLY;
	   case SDLK_KP_MINUS:
		   return KEY_KEYPAD_MINUS;
	   case SDLK_KP_PLUS:
		   return KEY_KEYPAD_PLUS;
	   case SDLK_KP_ENTER:
		   return KEY_KEYPAD_ENTER;
	   case SDLK_KP_EQUALS:
		   return KEY_KEYPAD_EQUALS;

	   // Arrows + Home/End pad
	   case SDLK_UP:
		   return KEY_UP;
	   case SDLK_DOWN:
		   return KEY_DOWN;
	   case SDLK_RIGHT:
		   return KEY_RIGHT;
	   case SDLK_LEFT:
		   return KEY_LEFT;
	   case SDLK_INSERT:
		   return KEY_INSERT;
	   case SDLK_HOME:
		   return KEY_HOME;
	   case SDLK_END:
		   return KEY_END;
	   case SDLK_PAGEUP:
		   return KEY_PAGEUP;
	   case SDLK_PAGEDOWN:
		   return KEY_PAGEDOWN;

	   // Function keys
	   case SDLK_F1:
		   return KEY_F1;
	   case SDLK_F2:
		   return KEY_F2;
	   case SDLK_F3:
		   return KEY_F3;
	   case SDLK_F4:
		   return KEY_F4;
	   case SDLK_F5:
		   return KEY_F5;
	   case SDLK_F6:
		   return KEY_F6;
	   case SDLK_F7:
		   return KEY_F7;
	   case SDLK_F8:
		   return KEY_F8;
	   case SDLK_F9:
		   return KEY_F9;
	   case SDLK_F10:
		   return KEY_F10;
	   case SDLK_F11:
		   return KEY_F11;
	   case SDLK_F12:
		   return KEY_F12;
	   case SDLK_F13:
		   return KEY_F13;
	   case SDLK_F14:
		   return KEY_F14;
	   case SDLK_F15:
		   return KEY_F15;

	   // Key state modifier keys
	   case SDLK_NUMLOCK:
		   return KEY_NUMLOCK;
	   case SDLK_CAPSLOCK:
		   return KEY_CAPSLOCK;
	   case SDLK_SCROLLOCK:
		   return KEY_SCROLLOCK;
	   case SDLK_RSHIFT:
	   case SDLK_LSHIFT:
		   return KEY_SHIFT;
	   case SDLK_RCTRL:
	   case SDLK_LCTRL:
		   return KEY_CTRL;
	   case SDLK_RALT:
	   case SDLK_LALT:
		   return KEY_ALT;
	   case SDLK_RMETA:
	   case SDLK_LMETA:
		   return KEY_META;
	   case SDLK_LSUPER:
	   case SDLK_RSUPER:
		   return KEY_SUPER;
	   case SDLK_MODE:
		   return KEY_MODE;
	   case SDLK_COMPOSE:
		   return KEY_COMPOSE;

	   // Miscellaneous function keys
	   case SDLK_HELP:
		   return KEY_HELP;
	   case SDLK_PRINT:
		   return KEY_PRINT;
	   case SDLK_SYSREQ:
		   return KEY_SYSREQ;
	   case SDLK_BREAK:
		   return KEY_BREAK;
	   case SDLK_MENU:
		   return KEY_MENU;
	   case SDLK_POWER:
		   return KEY_POWER;
	   case SDLK_EURO:
		   return KEY_EURO;
	   case SDLK_UNDO:
		   return KEY_UNDO;
      default:
         logprintf(LogConsumer::LogWarning, "Unknown key detected: %d", key);
         return KEY_UNKNOWN;
   }
}


S32 InputCodeManager::inputCodeToSDLKey(InputCode inputCode)
{
   switch(inputCode)
   {
	   case KEY_BACKSPACE:
		   return SDLK_BACKSPACE;
	   case KEY_TAB:
		   return SDLK_TAB;
	   case KEY_CLEAR:
		   return SDLK_CLEAR;
	   case KEY_ENTER:
		   return SDLK_RETURN;
	   case KEY_PAUSE:
		   return SDLK_PAUSE;
	   case KEY_ESCAPE:
		   return SDLK_ESCAPE;
	   case KEY_SPACE:
		   return SDLK_SPACE;
	   case KEY_EXCLAIM:
		   return SDLK_EXCLAIM;
	   case KEY_DOUBLEQUOTE:
		   return SDLK_QUOTEDBL;
	   case KEY_HASH:
		   return SDLK_HASH;
	   case KEY_DOLLAR:
		   return SDLK_DOLLAR;
	   case KEY_AMPERSAND:
		   return SDLK_AMPERSAND;
	   case KEY_QUOTE:
		   return SDLK_QUOTE;
	   case KEY_OPENPAREN:
		   return SDLK_LEFTPAREN;
	   case KEY_CLOSEPAREN:
		   return SDLK_RIGHTPAREN;
	   case KEY_ASTERISK:
		   return SDLK_ASTERISK;
	   case KEY_PLUS:
		   return SDLK_PLUS;
	   case KEY_COMMA:
		   return SDLK_COMMA;
	   case KEY_MINUS:
		   return SDLK_MINUS;
	   case KEY_PERIOD:
		   return SDLK_PERIOD;
	   case KEY_SLASH:
		   return SDLK_SLASH;
	   case KEY_0:
		   return SDLK_0;
	   case KEY_1:
		   return SDLK_1;
	   case KEY_2:
		   return SDLK_2;
	   case KEY_3:
		   return SDLK_3;
	   case KEY_4:
		   return SDLK_4;
	   case KEY_5:
		   return SDLK_5;
	   case KEY_6:
		   return SDLK_6;
	   case KEY_7:
		   return SDLK_7;
	   case KEY_8:
		   return SDLK_8;
	   case KEY_9:
		   return SDLK_9;
	   case KEY_COLON:
		   return SDLK_COLON;
	   case KEY_SEMICOLON:
		   return SDLK_SEMICOLON;
	   case KEY_LESS:
		   return SDLK_LESS;
	   case KEY_EQUALS:
		   return SDLK_EQUALS;
	   case KEY_GREATER:
		   return SDLK_GREATER;
	   case KEY_QUESTION:
		   return SDLK_QUESTION;
	   case KEY_AT:
		   return SDLK_AT;
		   
	   case KEY_OPENBRACKET:
		   return SDLK_LEFTBRACKET;
	   case KEY_BACKSLASH:
		   return SDLK_BACKSLASH;
	   case KEY_CLOSEBRACKET:
		   return SDLK_RIGHTBRACKET;
	   case KEY_CARET:
		   return SDLK_CARET;
	   case KEY_UNDERSCORE:
		   return SDLK_UNDERSCORE;
	   case KEY_BACKQUOTE:
		   return SDLK_BACKQUOTE;
	   case KEY_A:
		   return SDLK_a;
	   case KEY_B:
		   return SDLK_b;
	   case KEY_C:
		   return SDLK_c;
	   case KEY_D:
		   return SDLK_d;
	   case KEY_E:
		   return SDLK_e;
	   case KEY_F:
		   return SDLK_f;
	   case KEY_G:
		   return SDLK_g;
	   case KEY_H:
		   return SDLK_h;
	   case KEY_I:
		   return SDLK_i;
	   case KEY_J:
		   return SDLK_j;
	   case KEY_K:
		   return SDLK_k;
	   case KEY_L:
		   return SDLK_l;
	   case KEY_M:
		   return SDLK_m;
	   case KEY_N:
		   return SDLK_n;
	   case KEY_O:
		   return SDLK_o;
	   case KEY_P:
		   return SDLK_p;
	   case KEY_Q:
		   return SDLK_q;
	   case KEY_R:
		   return SDLK_r;
	   case KEY_S:
		   return SDLK_s;
	   case KEY_T:
		   return SDLK_t;
	   case KEY_U:
		   return SDLK_u;
	   case KEY_V:
		   return SDLK_v;
	   case KEY_W:
		   return SDLK_w;
	   case KEY_X:
		   return SDLK_x;
	   case KEY_Y:
		   return SDLK_y;
	   case KEY_Z:
		   return SDLK_z;
	   case KEY_DELETE:
		   return SDLK_DELETE;

	   // International keyboard syms
	   case KEY_WORLD_0:
		   return SDLK_WORLD_0;
	   case KEY_WORLD_1:
		   return SDLK_WORLD_1;
	   case KEY_WORLD_2:
		   return SDLK_WORLD_2;
	   case KEY_WORLD_3:
		   return SDLK_WORLD_3;
	   case KEY_WORLD_4:
		   return SDLK_WORLD_4;
	   case KEY_WORLD_5:
		   return SDLK_WORLD_5;
	   case KEY_WORLD_6:
		   return SDLK_WORLD_6;
	   case KEY_WORLD_7:
		   return SDLK_WORLD_7;
	   case KEY_WORLD_8:
		   return SDLK_WORLD_8;
	   case KEY_WORLD_9:
		   return SDLK_WORLD_9;
	   case KEY_WORLD_10:
		   return SDLK_WORLD_10;
	   case KEY_WORLD_11:
		   return SDLK_WORLD_11;
	   case KEY_WORLD_12:
		   return SDLK_WORLD_12;
	   case KEY_WORLD_13:
		   return SDLK_WORLD_13;
	   case KEY_WORLD_14:
		   return SDLK_WORLD_14;
	   case KEY_WORLD_15:
		   return SDLK_WORLD_15;
	   case KEY_WORLD_16:
		   return SDLK_WORLD_16;
	   case KEY_WORLD_17:
		   return SDLK_WORLD_17;
	   case KEY_WORLD_18:
		   return SDLK_WORLD_18;
	   case KEY_WORLD_19:
		   return SDLK_WORLD_19;
	   case KEY_WORLD_20:
		   return SDLK_WORLD_20;
	   case KEY_WORLD_21:
		   return SDLK_WORLD_21;
	   case KEY_WORLD_22:
		   return SDLK_WORLD_22;
	   case KEY_WORLD_23:
		   return SDLK_WORLD_23;
	   case KEY_WORLD_24:
		   return SDLK_WORLD_24;
	   case KEY_WORLD_25:
		   return SDLK_WORLD_25;
	   case KEY_WORLD_26:
		   return SDLK_WORLD_26;
	   case KEY_WORLD_27:
		   return SDLK_WORLD_27;
	   case KEY_WORLD_28:
		   return SDLK_WORLD_28;
	   case KEY_WORLD_29:
		   return SDLK_WORLD_29;
	   case KEY_WORLD_30:
		   return SDLK_WORLD_30;
	   case KEY_WORLD_31:
		   return SDLK_WORLD_31;
	   case KEY_WORLD_32:
		   return SDLK_WORLD_32;
	   case KEY_WORLD_33:
		   return SDLK_WORLD_33;
	   case KEY_WORLD_34:
		   return SDLK_WORLD_34;
	   case KEY_WORLD_35:
		   return SDLK_WORLD_35;
	   case KEY_WORLD_36:
		   return SDLK_WORLD_36;
	   case KEY_WORLD_37:
		   return SDLK_WORLD_37;
	   case KEY_WORLD_38:
		   return SDLK_WORLD_38;
	   case KEY_WORLD_39:
		   return SDLK_WORLD_39;
	   case KEY_WORLD_40:
		   return SDLK_WORLD_40;
	   case KEY_WORLD_41:
		   return SDLK_WORLD_41;
	   case KEY_WORLD_42:
		   return SDLK_WORLD_42;
	   case KEY_WORLD_43:
		   return SDLK_WORLD_43;
	   case KEY_WORLD_44:
		   return SDLK_WORLD_44;
	   case KEY_WORLD_45:
		   return SDLK_WORLD_45;
	   case KEY_WORLD_46:
		   return SDLK_WORLD_46;
	   case KEY_WORLD_47:
		   return SDLK_WORLD_47;
	   case KEY_WORLD_48:
		   return SDLK_WORLD_48;
	   case KEY_WORLD_49:
		   return SDLK_WORLD_49;
	   case KEY_WORLD_50:
		   return SDLK_WORLD_50;
	   case KEY_WORLD_51:
		   return SDLK_WORLD_51;
	   case KEY_WORLD_52:
		   return SDLK_WORLD_52;
	   case KEY_WORLD_53:
		   return SDLK_WORLD_53;
	   case KEY_WORLD_54:
		   return SDLK_WORLD_54;
	   case KEY_WORLD_55:
		   return SDLK_WORLD_55;
	   case KEY_WORLD_56:
		   return SDLK_WORLD_56;
	   case KEY_WORLD_57:
		   return SDLK_WORLD_57;
	   case KEY_WORLD_58:
		   return SDLK_WORLD_58;
	   case KEY_WORLD_59:
		   return SDLK_WORLD_59;
	   case KEY_WORLD_60:
		   return SDLK_WORLD_60;
	   case KEY_WORLD_61:
		   return SDLK_WORLD_61;
	   case KEY_WORLD_62:
		   return SDLK_WORLD_62;
	   case KEY_WORLD_63:
		   return SDLK_WORLD_63;
	   case KEY_WORLD_64:
		   return SDLK_WORLD_64;
	   case KEY_WORLD_65:
		   return SDLK_WORLD_65;
	   case KEY_WORLD_66:
		   return SDLK_WORLD_66;
	   case KEY_WORLD_67:
		   return SDLK_WORLD_67;
	   case KEY_WORLD_68:
		   return SDLK_WORLD_68;
	   case KEY_WORLD_69:
		   return SDLK_WORLD_69;
	   case KEY_WORLD_70:
		   return SDLK_WORLD_70;
	   case KEY_WORLD_71:
		   return SDLK_WORLD_71;
	   case KEY_WORLD_72:
		   return SDLK_WORLD_72;
	   case KEY_WORLD_73:
		   return SDLK_WORLD_73;
	   case KEY_WORLD_74:
		   return SDLK_WORLD_74;
	   case KEY_WORLD_75:
		   return SDLK_WORLD_75;
	   case KEY_WORLD_76:
		   return SDLK_WORLD_76;
	   case KEY_WORLD_77:
		   return SDLK_WORLD_77;
	   case KEY_WORLD_78:
		   return SDLK_WORLD_78;
	   case KEY_WORLD_79:
		   return SDLK_WORLD_79;
	   case KEY_WORLD_80:
		   return SDLK_WORLD_80;
	   case KEY_WORLD_81:
		   return SDLK_WORLD_81;
	   case KEY_WORLD_82:
		   return SDLK_WORLD_82;
	   case KEY_WORLD_83:
		   return SDLK_WORLD_83;
	   case KEY_WORLD_84:
		   return SDLK_WORLD_84;
	   case KEY_WORLD_85:
		   return SDLK_WORLD_85;
	   case KEY_WORLD_86:
		   return SDLK_WORLD_86;
	   case KEY_WORLD_87:
		   return SDLK_WORLD_87;
	   case KEY_WORLD_88:
		   return SDLK_WORLD_88;
	   case KEY_WORLD_89:
		   return SDLK_WORLD_89;
	   case KEY_WORLD_90:
		   return SDLK_WORLD_90;
	   case KEY_WORLD_91:
		   return SDLK_WORLD_91;
	   case KEY_WORLD_92:
		   return SDLK_WORLD_92;
	   case KEY_WORLD_93:
		   return SDLK_WORLD_93;
	   case KEY_WORLD_94:
		   return SDLK_WORLD_94;
	   case KEY_WORLD_95:
		   return SDLK_WORLD_95;

	   // Numeric keypad
	   case KEY_KEYPAD0:
		   return SDLK_KP0;
	   case KEY_KEYPAD1:
		   return SDLK_KP1;
	   case KEY_KEYPAD2:
		   return SDLK_KP2;
	   case KEY_KEYPAD3:
		   return SDLK_KP3;
	   case KEY_KEYPAD4:
		   return SDLK_KP4;
	   case KEY_KEYPAD5:
		   return SDLK_KP5;
	   case KEY_KEYPAD6:
		   return SDLK_KP6;
	   case KEY_KEYPAD7:
		   return SDLK_KP7;
	   case KEY_KEYPAD8:
		   return SDLK_KP8;
	   case KEY_KEYPAD9:
		   return SDLK_KP9;
	   case KEY_KEYPAD_PERIOD:
		   return SDLK_KP_PERIOD;
	   case KEY_KEYPAD_DIVIDE:
		   return SDLK_KP_DIVIDE;
	   case KEY_KEYPAD_MULTIPLY:
		   return SDLK_KP_MULTIPLY;
	   case KEY_KEYPAD_MINUS:
		   return SDLK_KP_MINUS;
	   case KEY_KEYPAD_PLUS:
		   return SDLK_KP_PLUS;
	   case KEY_KEYPAD_ENTER:
		   return SDLK_KP_ENTER;
	   case KEY_KEYPAD_EQUALS:
		   return SDLK_KP_EQUALS;

	   // Arrows + Home/End pad
	   case KEY_UP:
		   return SDLK_UP;
	   case KEY_DOWN:
		   return SDLK_DOWN;
	   case KEY_RIGHT:
		   return SDLK_RIGHT;
	   case KEY_LEFT:
		   return SDLK_LEFT;
	   case KEY_INSERT:
		   return SDLK_INSERT;
	   case KEY_HOME:
		   return SDLK_HOME;
	   case KEY_END:
		   return SDLK_END;
	   case KEY_PAGEUP:
		   return SDLK_PAGEUP;
	   case KEY_PAGEDOWN:
		   return SDLK_PAGEDOWN;

	   // Function keys
	   case KEY_F1:
		   return SDLK_F1;
	   case KEY_F2:
		   return SDLK_F2;
	   case KEY_F3:
		   return SDLK_F3;
	   case KEY_F4:
		   return SDLK_F4;
	   case KEY_F5:
		   return SDLK_F5;
	   case KEY_F6:
		   return SDLK_F6;
	   case KEY_F7:
		   return SDLK_F7;
	   case KEY_F8:
		   return SDLK_F8;
	   case KEY_F9:
		   return SDLK_F9;
	   case KEY_F10:
		   return SDLK_F10;
	   case KEY_F11:
		   return SDLK_F11;
	   case KEY_F12:
		   return SDLK_F12;
	   case KEY_F13:
		   return SDLK_F13;
	   case KEY_F14:
		   return SDLK_F14;
	   case KEY_F15:
		   return SDLK_F15;

	   // Key state modifier keys
	   case KEY_NUMLOCK:
		   return SDLK_NUMLOCK;
	   case KEY_CAPSLOCK:
		   return SDLK_CAPSLOCK;
	   case KEY_SCROLLOCK:
		   return SDLK_SCROLLOCK;
	   case KEY_MODE:
		   return SDLK_MODE;
	   case KEY_COMPOSE:
		   return SDLK_COMPOSE;

	   // Miscellaneous function keys
	   case KEY_HELP:
		   return SDLK_HELP;
	   case KEY_PRINT:
		   return SDLK_PRINT;
	   case KEY_SYSREQ:
		   return SDLK_SYSREQ;
	   case KEY_BREAK:
		   return SDLK_BREAK;
	   case KEY_MENU:
		   return SDLK_MENU;
	   case KEY_POWER:
		   return SDLK_POWER;
	   case KEY_EURO:
		   return SDLK_EURO;
	   case KEY_UNDO:
		   return SDLK_UNDO;
      default:
         logprintf(LogConsumer::LogWarning, "Unknown inputCode detected: %d", inputCode);
         return SDLK_UNKNOWN;
   }
}


InputCode InputCodeManager::joystickButtonToInputCode(Joystick::Button button)
{
   switch(button)
   {
      case Joystick::Button1:
         return BUTTON_1;
      case Joystick::Button2:
         return BUTTON_2;
      case Joystick::Button3:
         return BUTTON_3;
      case Joystick::Button4:
         return BUTTON_4;
      case Joystick::Button5:
         return BUTTON_5;
      case Joystick::Button6:
         return BUTTON_6;
      case Joystick::Button7:
         return BUTTON_7;
      case Joystick::Button8:
         return BUTTON_8;
      case Joystick::ButtonStart:
         return BUTTON_START;
      case Joystick::ButtonBack:
         return BUTTON_BACK;
      case Joystick::ButtonDPadUp:
         return BUTTON_DPAD_UP;
      case Joystick::ButtonDPadDown:
         return BUTTON_DPAD_DOWN;
      case Joystick::ButtonDPadLeft:
         return BUTTON_DPAD_LEFT;
      case Joystick::ButtonDPadRight:
         return BUTTON_DPAD_RIGHT;
      case Joystick::Button9:
         return BUTTON_9;
      case Joystick::Button10:
         return BUTTON_10;
      case Joystick::Button11:
         return BUTTON_11;
      case Joystick::Button12:
         return BUTTON_12;
      default:
         return BUTTON_UNKNOWN;
   }
}


InputCode InputCodeManager::joyHatToInputCode(int hatDirectionMask)
{
   switch(hatDirectionMask)
   {
      case 1:
         return BUTTON_DPAD_UP;
      case 2:
         return BUTTON_DPAD_RIGHT;
      case 4:
         return BUTTON_DPAD_DOWN;
      case 8:
         return BUTTON_DPAD_LEFT;
      default:
         return BUTTON_UNKNOWN;
   }
}


Joystick::Button inputCodeToJoystickButton(InputCode inputCode)
{
   switch(inputCode)
   {
      case BUTTON_1:
         return Joystick::Button1;
      case BUTTON_2:
         return Joystick::Button2;
      case BUTTON_3:
         return Joystick::Button3;
      case BUTTON_4:
         return Joystick::Button4;
      case BUTTON_5:
         return Joystick::Button5;
      case BUTTON_6:
         return Joystick::Button6;
      case BUTTON_7:
         return Joystick::Button7;
      case BUTTON_8:
         return Joystick::Button8;
      case BUTTON_9:
         return Joystick::Button9;
      case BUTTON_10:
         return Joystick::Button10;
      case BUTTON_11:
         return Joystick::Button11;
      case BUTTON_12:
         return Joystick::Button12;
      case BUTTON_START:
         return Joystick::ButtonStart;
      case BUTTON_BACK:
         return Joystick::ButtonBack;
      case BUTTON_DPAD_UP:
         return Joystick::ButtonDPadUp;
      case BUTTON_DPAD_DOWN:
         return Joystick::ButtonDPadDown;
      case BUTTON_DPAD_LEFT:
         return Joystick::ButtonDPadLeft;
      case BUTTON_DPAD_RIGHT:
         return Joystick::ButtonDPadRight;
      default:
         return Joystick::ButtonUnknown;
   }
}
#endif


// We'll also treat controller buttons like simulated keystrokes
bool InputCodeManager::isControllerButton(InputCode inputCode)
{
   return inputCode == BUTTON_1    || inputCode == BUTTON_2  || inputCode == BUTTON_3  ||
          inputCode == BUTTON_4    || inputCode == BUTTON_5  || inputCode == BUTTON_6  ||
          inputCode == BUTTON_7    || inputCode == BUTTON_8  || inputCode == BUTTON_9  ||
          inputCode == BUTTON_10   || inputCode == BUTTON_11 || inputCode == BUTTON_12 ||
          inputCode == BUTTON_BACK || inputCode == BUTTON_START;
}       


// Array tying InputCodes to string representations; used for translating one to the other 
static const char *keyNames[KEY_COUNT];

void InputCodeManager::initializeKeyNames()
{
   // Fill name list with default value
   for(S32 i = 0; i < KEY_COUNT; i++)
      keyNames[i] = "Unknown Key";

   // Now keys we know with our locally defined names
   keyNames[S32(KEY_BACKSPACE)] = "Backspace";
   keyNames[S32(KEY_DELETE)] = "Del";
   keyNames[S32(KEY_TAB)] = "Tab";
   keyNames[S32(KEY_ENTER)] = "Enter";
   keyNames[S32(KEY_ESCAPE)] = "Esc";
   keyNames[S32(KEY_SPACE)] = "Space";
   keyNames[S32(KEY_0)] = "0";
   keyNames[S32(KEY_1)] = "1";
   keyNames[S32(KEY_2)] = "2";
   keyNames[S32(KEY_3)] = "3";
   keyNames[S32(KEY_4)] = "4";
   keyNames[S32(KEY_5)] = "5";
   keyNames[S32(KEY_6)] = "6";
   keyNames[S32(KEY_7)] = "7";
   keyNames[S32(KEY_8)] = "8";
   keyNames[S32(KEY_9)] = "9";
   keyNames[S32(KEY_A)] = "A";
   keyNames[S32(KEY_B)] = "B";
   keyNames[S32(KEY_C)] = "C";
   keyNames[S32(KEY_D)] = "D";
   keyNames[S32(KEY_E)] = "E";
   keyNames[S32(KEY_F)] = "F";
   keyNames[S32(KEY_G)] = "G";
   keyNames[S32(KEY_H)] = "H";
   keyNames[S32(KEY_I)] = "I";
   keyNames[S32(KEY_J)] = "J";
   keyNames[S32(KEY_K)] = "K";
   keyNames[S32(KEY_L)] = "L";
   keyNames[S32(KEY_M)] = "M";
   keyNames[S32(KEY_N)] = "N";
   keyNames[S32(KEY_O)] = "O";
   keyNames[S32(KEY_P)] = "P";
   keyNames[S32(KEY_Q)] = "Q";
   keyNames[S32(KEY_R)] = "R";
   keyNames[S32(KEY_S)] = "S";
   keyNames[S32(KEY_T)] = "T";
   keyNames[S32(KEY_U)] = "U";
   keyNames[S32(KEY_V)] = "V";
   keyNames[S32(KEY_W)] = "W";
   keyNames[S32(KEY_X)] = "X";
   keyNames[S32(KEY_Y)] = "Y";
   keyNames[S32(KEY_Z)] = "Z";
   keyNames[S32(KEY_TILDE)] = "~";
   keyNames[S32(KEY_MINUS)] = "-";
   keyNames[S32(KEY_EQUALS)] = "=";
   keyNames[S32(KEY_OPENBRACKET)] = "[";
   keyNames[S32(KEY_CLOSEBRACKET)] = "]";
   keyNames[S32(KEY_BACKSLASH)] = "\\";
   keyNames[S32(KEY_SEMICOLON)] = ";";
   keyNames[S32(KEY_QUOTE)] = "'";
   keyNames[S32(KEY_COMMA)] = ",";
   keyNames[S32(KEY_PERIOD)] = ".";
   keyNames[S32(KEY_SLASH)] = "/";
   keyNames[S32(KEY_PAGEUP)] = "Page Up";
   keyNames[S32(KEY_PAGEDOWN)] = "Page Down";
   keyNames[S32(KEY_END)] = "End";
   keyNames[S32(KEY_HOME)] = "Home";
   keyNames[S32(KEY_LEFT)] = "Left Arrow";
   keyNames[S32(KEY_UP)] = "Up Arrow";
   keyNames[S32(KEY_RIGHT)] = "Right Arrow";
   keyNames[S32(KEY_DOWN)] = "Down Arrow";
   keyNames[S32(KEY_INSERT)] = "Insert";
   keyNames[S32(KEY_F1)] = "F1";
   keyNames[S32(KEY_F2)] = "F2";
   keyNames[S32(KEY_F3)] = "F3";
   keyNames[S32(KEY_F4)] = "F4";
   keyNames[S32(KEY_F5)] = "F5";
   keyNames[S32(KEY_F6)] = "F6";
   keyNames[S32(KEY_F7)] = "F7";
   keyNames[S32(KEY_F8)] = "F8";
   keyNames[S32(KEY_F9)] = "F9";
   keyNames[S32(KEY_F10)] = "F10";
   keyNames[S32(KEY_F11)] = "F11";
   keyNames[S32(KEY_F12)] = "F12";
   keyNames[S32(KEY_SHIFT)] = "Shift";
   keyNames[S32(KEY_ALT)] = "Alt";
   keyNames[S32(KEY_CTRL)] = "Ctrl";
   keyNames[S32(KEY_META)] = "Meta";
   keyNames[S32(KEY_SUPER)] = "Super";
   keyNames[S32(MOUSE_LEFT)] = "Left-mouse";
   keyNames[S32(MOUSE_MIDDLE)] = "Middle-mouse";
   keyNames[S32(MOUSE_RIGHT)] = "Right-mouse";
   keyNames[S32(MOUSE_WHEEL_UP)] = "Mouse wheel up";
   keyNames[S32(MOUSE_WHEEL_DOWN)] = "Mouse wheel down";
   keyNames[S32(BUTTON_1)] = "Button 1";
   keyNames[S32(BUTTON_2)] = "Button 2";
   keyNames[S32(BUTTON_3)] = "Button 3";
   keyNames[S32(BUTTON_4)] = "Button 4";
   keyNames[S32(BUTTON_5)] = "Button 5";
   keyNames[S32(BUTTON_6)] = "Button 6";
   keyNames[S32(BUTTON_7)] = "Button 7";
   keyNames[S32(BUTTON_8)] = "Button 8";
   keyNames[S32(BUTTON_9)] = "Button 9";
   keyNames[S32(BUTTON_10)] = "Button 10";
   keyNames[S32(BUTTON_11)] = "Button 11";
   keyNames[S32(BUTTON_12)] = "Button 12";
   keyNames[S32(BUTTON_BACK)] = "Back";
   keyNames[S32(BUTTON_START)] = "Start";
   keyNames[S32(BUTTON_DPAD_UP)] = "DPad Up";
   keyNames[S32(BUTTON_DPAD_DOWN)] = "DPad Down";
   keyNames[S32(BUTTON_DPAD_LEFT)] = "DPad Left";
   keyNames[S32(BUTTON_DPAD_RIGHT)] = "DPad Right";
   keyNames[S32(STICK_1_LEFT)] = "Stick 1 Left";
   keyNames[S32(STICK_1_RIGHT)] = "Stick 1 Right";
   keyNames[S32(STICK_1_UP)] = "Stick 1 Up";
   keyNames[S32(STICK_1_DOWN)] = "Stick 1 Down";
   keyNames[S32(STICK_2_LEFT)] = "Stick 2 Left";
   keyNames[S32(STICK_2_RIGHT)] = "Stick 2 Right";
   keyNames[S32(STICK_2_UP)] = "Stick 2 Up";
   keyNames[S32(STICK_2_DOWN)] = "Stick 2 Down";
   keyNames[S32(MOUSE)] = "Mouse";
   keyNames[S32(LEFT_JOYSTICK)] = "Left joystick";
   keyNames[S32(RIGHT_JOYSTICK)] = "Right joystick";
   keyNames[S32(KEY_CTRL_M)] = "Ctrl-M";
   keyNames[S32(KEY_CTRL_Q)] = "Ctrl-Q";
   keyNames[S32(KEY_CTRL_S)] = "Ctrl-S";
   keyNames[S32(KEY_BACKQUOTE)] = "`";
   keyNames[S32(KEY_MENU)] = "Menu";
   keyNames[S32(KEY_KEYPAD_DIVIDE)] = "Keypad /";
   keyNames[S32(KEY_KEYPAD_MULTIPLY)] = "Keypad *";
   keyNames[S32(KEY_KEYPAD_MINUS)] = "Keypad -";
   keyNames[S32(KEY_KEYPAD_PLUS)] = "Keypad +";
   keyNames[S32(KEY_PRINT)] = "PrntScrn";
   keyNames[S32(KEY_PAUSE)] = "Pause";
   keyNames[S32(KEY_SCROLLOCK)] = "ScrollLock";
   keyNames[S32(KEY_KEYPAD1)] = "Keypad 1";
   keyNames[S32(KEY_KEYPAD2)] = "Keypad 2";
   keyNames[S32(KEY_KEYPAD3)] = "Keypad 3";
   keyNames[S32(KEY_KEYPAD4)] = "Keypad 4";
   keyNames[S32(KEY_KEYPAD5)] = "Keypad 5";
   keyNames[S32(KEY_KEYPAD6)] = "Keypad 6";
   keyNames[S32(KEY_KEYPAD7)] = "Keypad 7";
   keyNames[S32(KEY_KEYPAD8)] = "Keypad 8";
   keyNames[S32(KEY_KEYPAD9)] = "Keypad 9";
   keyNames[S32(KEY_KEYPAD0)] = "Keypad 0";
   keyNames[S32(KEY_KEYPAD_PERIOD)] = "Keypad .";
   keyNames[S32(KEY_KEYPAD_ENTER)] = "Keypad Enter";
   keyNames[S32(KEY_LESS)] = "Less";
}


// Translate an InputCode into a string name, primarily used
// for displaying keys in help and during rebind mode, and
// also when storing key bindings in INI files
const char *InputCodeManager::inputCodeToString(InputCode inputCode)
{
   //TNLAssert(U32(inputCode) < U32(KEY_COUNT), "inputCode out of range");
   if(U32(inputCode) >= U32(KEY_COUNT))
      return "";

   return keyNames[S32(inputCode)];
}


// Translate from a string key name into a InputCode
// (primarily for loading key bindings from INI files)
InputCode InputCodeManager::stringToInputCode(const char *inputName)
{
   for(S32 i = 0; i < KEY_COUNT; i++)
      if(stricmp(inputName, keyNames[i]) == 0)
         return InputCode(i);

   return KEY_UNKNOWN;
}

};


#ifdef __cplusplus
extern "C" {
#endif
// Provide access to getState from C code.
// Has to be outside the namespace declaration because C doesn't use namespaces.
int getState_c(int inputCode) { return Zap::InputCodeManager::getState((Zap::InputCode)inputCode); }
#ifdef __cplusplus
}
#endif
