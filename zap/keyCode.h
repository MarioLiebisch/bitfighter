//-----------------------------------------------------------------------------------
//
// bitFighter - A multiplayer vector graphics space game
// Based on Zap demo released for Torque Network Library by GarageGames.com
//
// Derivative work copyright (C) 2008 Chris Eykamp
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

#ifndef _KEYCODE_H_
#define _KEYCODE_H_

namespace Zap
{

// These are all the keys that GLUT can detect.  If it's not on this
// list, the key doesn't exist for our purposes.  Sorry!
enum KeyCode {
   KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, // Keyboard input
   KEY_6, KEY_7, KEY_8, KEY_9, KEY_A, KEY_B,
   KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H,
   KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N,
   KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
   KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
   KEY_TILDE,  KEY_MINUS,  KEY_EQUALS,
   KEY_OPENBRACKET,  KEY_CLOSEBRACKET,
   KEY_BACKSLASH,    KEY_SEMICOLON,
   KEY_QUOTE,        KEY_COMMA,
   KEY_PERIOD,       KEY_SLASH,
   KEY_PAGEUP,       KEY_PAGEDOWN,
   KEY_HOME,         KEY_END,
   KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN,
   KEY_INSERT,     KEY_DELETE,
   KEY_F1, KEY_F2, KEY_F3, KEY_F4,  KEY_F5,  KEY_F6,
   KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
   // KEY_F13, KEY_F14, KEY_F15,    // Additional keys recognized by SDL
   KEY_BACKSPACE,  KEY_TAB, KEY_ENTER,
   KEY_ESCAPE,     KEY_SPACE,

   /* More SDL keys
   // Locks
   KEY_NUMLOCK, KEY_CAPSLOCK, KEY_SCROLLOCK,

   // Modifiers
   KEY_RSHIFT, KEY_LSHIFT, KEY_RCTRL, KEY_LCTRL,
   KEY_RALT, KEY_LALT, KEY_RMETA, KEY_LMETA,
   KEY_LSUPER, KEY_RSUPER,
   */
   
   // And these are psudeo modifers, that may represent multiple events
   // (e.g. KEY_SHIFT means KEY_LSHIFT or KEY_RSHIFT)
   KEY_SHIFT,   KEY_CTRL,   KEY_ALT,

   /* // Some keys in SDL that were not in GLUT
   KEY_CLEAR, KEY_PAUSE, KEY_EXCLAIM, KEY_DOUBLEQUOTE, KEY_HASH, KEY_DOLLAR,
   KEY_AMPERSAND, KEY_OPENPAREN, KEY_CLOSEPAREN, KEY_ASTERISK, KEY_PLUS,
   KEY_COLON, KEY_LESS, KEY_GREATER, KEY_QUESTION, KEY_AT, KEY_CARET,
   KEY_UNDERSCORE, KEY_BACKQUOTE,

   KEY_MODE, KEY_COMPOSE, KEY_HELP, KEY_PRINT, KEY_SYSREQ,
   KEY_BREAK, KEY_MENU, KEY_POWER, KEY_EURO, KEY_UNDO,

   // SDL "world keys"
   KEY_WORLD_0, KEY_WORLD_1, KEY_WORLD_2, KEY_WORLD_3, KEY_WORLD_4,
   KEY_WORLD_5, KEY_WORLD_6, KEY_WORLD_7, KEY_WORLD_8, KEY_WORLD_9,
   KEY_WORLD_10, KEY_WORLD_11, KEY_WORLD_12, KEY_WORLD_13, KEY_WORLD_14,
   KEY_WORLD_15, KEY_WORLD_16, KEY_WORLD_17, KEY_WORLD_18, KEY_WORLD_19,
   KEY_WORLD_20, KEY_WORLD_21, KEY_WORLD_22, KEY_WORLD_23, KEY_WORLD_24,
   KEY_WORLD_25, KEY_WORLD_26, KEY_WORLD_27, KEY_WORLD_28, KEY_WORLD_29,
   KEY_WORLD_30, KEY_WORLD_31, KEY_WORLD_32, KEY_WORLD_33, KEY_WORLD_34,
   KEY_WORLD_35, KEY_WORLD_36, KEY_WORLD_37, KEY_WORLD_38, KEY_WORLD_39,
   KEY_WORLD_40, KEY_WORLD_41, KEY_WORLD_42, KEY_WORLD_43, KEY_WORLD_44,
   KEY_WORLD_45, KEY_WORLD_46, KEY_WORLD_47, KEY_WORLD_48, KEY_WORLD_49,
   KEY_WORLD_50, KEY_WORLD_51, KEY_WORLD_52, KEY_WORLD_53, KEY_WORLD_54,
   KEY_WORLD_55, KEY_WORLD_56, KEY_WORLD_57, KEY_WORLD_58, KEY_WORLD_59,
   KEY_WORLD_60, KEY_WORLD_61, KEY_WORLD_62, KEY_WORLD_63, KEY_WORLD_64,
   KEY_WORLD_65, KEY_WORLD_66, KEY_WORLD_67, KEY_WORLD_68, KEY_WORLD_69,
   KEY_WORLD_70, KEY_WORLD_71, KEY_WORLD_72, KEY_WORLD_73, KEY_WORLD_74,
   KEY_WORLD_75, KEY_WORLD_76, KEY_WORLD_77, KEY_WORLD_78, KEY_WORLD_79,
   KEY_WORLD_80, KEY_WORLD_81, KEY_WORLD_82, KEY_WORLD_83, KEY_WORLD_84,
   KEY_WORLD_85, KEY_WORLD_86, KEY_WORLD_87, KEY_WORLD_88, KEY_WORLD_89,
   KEY_WORLD_90, KEY_WORLD_91, KEY_WORLD_92, KEY_WORLD_93, KEY_WORLD_94,
   KEY_WORLD_95,

   // Numeric keypad keys
   KEY_KEYPAD0, KEY_KEYPAD1, KEY_KEYPAD2, KEY_KEYPAD3, KEY_KEYPAD4,
   KEY_KEYPAD5, KEY_KEYPAD6, KEY_KEYPAD7, KEY_KEYPAD8, KEY_KEYPAD9,
   KEY_KEYPAD_PERIOD, KEY_KEYPAD_DIVIDE, KEY_KEYPAD_MULTIPLY,
   KEY_KEYPAD_MINUS, KEY_KEYPAD_PLUS, KEY_KEYPAD_ENTER, KEY_KEYPAD_EQUALS,
   */
   
   MOUSE_LEFT, MOUSE_MIDDLE, MOUSE_RIGHT,    // Mouse buttons

   BUTTON_1, BUTTON_2, BUTTON_3,             // Controller buttons
   BUTTON_4, BUTTON_5, BUTTON_6,
   BUTTON_7, BUTTON_8,                       // Often triggers 1 and 2
   BUTTON_BACK, BUTTON_START,                // Sometimes 9 & 10
   BUTTON_DPAD_UP, BUTTON_DPAD_DOWN,
   BUTTON_DPAD_LEFT, BUTTON_DPAD_RIGHT,

   STICK_1_LEFT, STICK_1_RIGHT,              // Joystick moves,
   STICK_1_UP, STICK_1_DOWN,                 // for menu navigation

   STICK_2_LEFT, STICK_2_RIGHT,
   STICK_2_UP, STICK_2_DOWN,

   MAX_KEYS,
   MOUSE, LEFT_JOYSTICK, RIGHT_JOYSTICK,     // Not exactly keys, but helpful to have in here!
   KEYS_UP_DOWN, KEYS_LEFT_RIGHT,            // These are here because we need a dummy KeyCode item in the instructions
   KEY_CTRL_M, KEY_CTRL_Q, KEY_CTRL_S,
   KEY_UNKNOWN
};

static bool keyIsDown[MAX_KEYS];
const char *keyCodeToString(KeyCode keyCode);
KeyCode stringToKeyCode(const char *keyname);

void setKeyState(KeyCode keyCode, bool state);  // Set key state (t=down, f=up)
bool getKeyState(KeyCode keyCode);              // Return current key state (t=down, f=up)
void resetKeyStates();                          // Initialize key states
void dumpKeyStates();                           // Log key states for testing
void checkModifierKeyState();                   // Handle Ctrl, Shift, Alt
//KeyCode standardSDLKeyToKeyCode(int key);       // Convert SDL keycodes to our KeyCode system

KeyCode standardGLUTKeyToKeyCode(int key);      // Convert standard keys to KeyCode
KeyCode specialGLUTKeyToKeyCode(int key);       // Convert special keys to KeyCode
char keyToAscii(int key, KeyCode keyCode);      // Return a printable ascii char, if possible
bool isControllerButton(KeyCode keyCode);       // Does keyCode represent a controller button?

extern KeyCode keySELWEAP1[2];
extern KeyCode keySELWEAP2[2];
extern KeyCode keySELWEAP3[2];
extern KeyCode keyADVWEAP[2];
extern KeyCode keyCMDRMAP[2];
extern KeyCode keyTEAMCHAT[2];
extern KeyCode keyGLOBCHAT[2];
extern KeyCode keyQUICKCHAT[2];
extern KeyCode keyCMDCHAT[2];
extern KeyCode keyLOADOUT[2];
extern KeyCode keyMOD1[2];
extern KeyCode keyMOD2[2];
extern KeyCode keyFIRE[2];
extern KeyCode keyTOGVOICE[2];
extern KeyCode keyUP[2];
extern KeyCode keyDOWN[2];
extern KeyCode keyLEFT[2];
extern KeyCode keyRIGHT[2];
extern KeyCode keySCRBRD[2];
extern KeyCode keyHELP;
extern KeyCode keyOUTGAMECHAT;
extern KeyCode keyMISSION;
extern KeyCode keyFPS;
extern KeyCode keyDIAG;


}

#endif
