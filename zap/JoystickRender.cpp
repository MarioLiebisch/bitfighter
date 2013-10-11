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

#include "JoystickRender.h"
#include "Joystick.h"
#include "InputCode.h"
#include "SymbolShape.h"
#include "FontManager.h"
#include "Colors.h"
#include "gameObjectRender.h"
#include "SymbolShape.h"

#include "RenderUtils.h"
#include "OpenglUtils.h"

namespace Zap
{

using namespace UI;


JoystickRender::JoystickRender()
{
   // Do nothing
}


JoystickRender::~JoystickRender()
{
   // Do nothing
}


inline const Color *JoystickRender::getButtonColor(bool activated)
{
   if(activated)
      return &Colors::red;

   return &Colors::white;
}


// Render dpad graphic
void JoystickRender::renderDPad(Point center, F32 radius, bool upActivated, bool downActivated, bool leftActivated,
                bool rightActivated, const char *msg1, const char *msg2)
{
   radius = radius * 0.143f;   // = 1/7  Correct for the fact that when radius = 1, graphic has 7 px radius

   static Point points[7];

   // Up arrow
   points[0] = (center + Point(-1, -2) * radius);
   points[1] = (center + Point(-1, -4) * radius);
   points[2] = (center + Point(-3, -4) * radius);
   points[3] = (center + Point( 0, -7) * radius);
   points[4] = (center + Point( 3, -4) * radius);
   points[5] = (center + Point( 1, -4) * radius);
   points[6] = (center + Point( 1, -2) * radius);

   glColor(getButtonColor(upActivated));
   renderVertexArray((F32 *)points, ARRAYSIZE(points), GL_LINE_LOOP);

   // Down arrow
   points[0] = (center + Point(-1, 2) * radius);
   points[1] = (center + Point(-1, 4) * radius);
   points[2] = (center + Point(-3, 4) * radius);
   points[3] = (center + Point( 0, 7) * radius);
   points[4] = (center + Point( 3, 4) * radius);
   points[5] = (center + Point( 1, 4) * radius);
   points[6] = (center + Point( 1, 2) * radius);

   glColor(getButtonColor(downActivated));
   renderVertexArray((F32 *)points, ARRAYSIZE(points), GL_LINE_LOOP);

   // Left arrow
   points[0] = (center + Point(-2, -1) * radius);
   points[1] = (center + Point(-4, -1) * radius);
   points[2] = (center + Point(-4, -3) * radius);
   points[3] = (center + Point(-7,  0) * radius);
   points[4] = (center + Point(-4,  3) * radius);
   points[5] = (center + Point(-4,  1) * radius);
   points[6] = (center + Point(-2,  1) * radius);

   glColor(getButtonColor(leftActivated));
   renderVertexArray((F32 *)points, ARRAYSIZE(points), GL_LINE_LOOP);

   // Right arrow
   points[0] = (center + Point(2, -1) * radius);
   points[1] = (center + Point(4, -1) * radius);
   points[2] = (center + Point(4, -3) * radius);
   points[3] = (center + Point(7,  0) * radius);
   points[4] = (center + Point(4,  3) * radius);
   points[5] = (center + Point(4,  1) * radius);
   points[6] = (center + Point(2,  1) * radius);

   glColor(getButtonColor(rightActivated));
   renderVertexArray((F32 *)points, ARRAYSIZE(points), GL_LINE_LOOP);

   // Label the graphic
   glColor(Colors::white);
   if(strcmp(msg1, "") == 0)    // That is, != "".  Remember, kids, strcmp returns 0 when strings are identical!
   {
      S32 size = 12;
      S32 width = getStringWidth(size, msg1);
      drawString(center.x - width / 2, center.y + 27, size, msg1);
   }

   if(strcmp(msg2, "") == 0)
   {
      S32 size = 10;
      S32 width = getStringWidth(size, msg2);
      drawString(center.x - width / 2, center.y + 42, size, msg2);
   }
}


S32 JoystickRender::getControllerButtonRenderedSize(S32 joystickIndex, InputCode inputCode)
{
   // Return keyboard key size, just in case
   if(!InputCodeManager::isControllerButton(inputCode))
      return SymbolKey(InputCodeManager::inputCodeToString(inputCode)).getWidth();

   // Get joystick button size
   JoystickButton button = InputCodeManager::inputCodeToJoystickButton(inputCode);

   Joystick::ButtonShape buttonShape =
         Joystick::JoystickPresetList[joystickIndex].buttonMappings[button].buttonShape;

   switch(buttonShape)
   {
      case Joystick::ButtonShapeRound:             return buttonHalfHeight * 2;
      case Joystick::ButtonShapeRect:              return rectButtonWidth;
      case Joystick::ButtonShapeSmallRect:         return smallRectButtonWidth;
      case Joystick::ButtonShapeRoundedRect:       return rectButtonWidth;
      case Joystick::ButtonShapeSmallRoundedRect:  return smallRectButtonWidth;
      case Joystick::ButtonShapeHorizEllipse:      return horizEllipseButtonRadiusX * 2;
      case Joystick::ButtonShapeRightTriangle:     return rectButtonWidth;
      default:                                     return rectButtonWidth;
   }

   return -1;     // Kill a useless warning
}

// Thinking...
//class SymbolShape 
//{
//   void render(S32 x, S32 y);
//};
//
//
//class Symbol 
//{
//   SymbolShape shape;
//   string label;
//   Color color;
//
//   void render(S32 x, S32 y);
//};


// Renders something resembling a controller button or keyboard key
// Note:  buttons are with the given x coordinate as their center
void JoystickRender::renderControllerButton(F32 x, F32 y, U32 joystickIndex, InputCode inputCode, bool activated)
{
   // Render keyboard keys, just in case
   if(!InputCodeManager::isControllerButton(inputCode))
   {
      SymbolKey(InputCodeManager::inputCodeToString(inputCode)).render(Point(x, y + 17));
      return;
   }

   JoystickButton button = InputCodeManager::inputCodeToJoystickButton(inputCode);

   // Don't render if button doesn't exist
   if(!Joystick::isButtonDefined(joystickIndex, button))
      return;

   Joystick::ButtonShape buttonShape =
         Joystick::JoystickPresetList[joystickIndex].buttonMappings[button].buttonShape;

   const char *label = Joystick::JoystickPresetList[joystickIndex].buttonMappings[button].label.c_str();
   Color *buttonColor = &Joystick::JoystickPresetList[joystickIndex].buttonMappings[button].color;


   // Note:  the x coordinate is already at the center
   Point location(x,y);
   Point center = location + Point(0, buttonHalfHeight);

   const Color *color = getButtonColor(activated);

   // Render joystick button shape
   switch(buttonShape)
   {
      case Joystick::ButtonShapeRect:
         //shapeRect.render(center);
         drawRoundedRect(center, rectButtonWidth, rectButtonHeight, 3);
         break;
      case Joystick::ButtonShapeSmallRect:
         //shapeSmallRect.render(center);
         drawRoundedRect(center, smallRectButtonWidth, smallRectButtonHeight, 3);
         break;
      case Joystick::ButtonShapeRoundedRect:
         //shapeRoundedRect.render(center);
         drawRoundedRect(center, rectButtonWidth, rectButtonHeight, 5);
         break;
      case Joystick::ButtonShapeSmallRoundedRect:
         //shapeSmallRoundedRect.render(center);
         drawRoundedRect(center, smallRectButtonWidth, smallRectButtonHeight, 5);
         break;
      case Joystick::ButtonShapeHorizEllipse:
         glColor(buttonColor);
         //shapeHorizEllipse.render(center);
         drawFilledEllipse(center, horizEllipseButtonRadiusX, horizEllipseButtonRadiusY, 0);
         glColor(Colors::white);
         drawEllipse(center, horizEllipseButtonRadiusX, horizEllipseButtonRadiusY, 0);
         break;
      case Joystick::ButtonShapeRightTriangle:
         //shapeRightTriangle.render(center);
         location = location + Point(-rightTriangleWidth / 4.0f, 0);  // Need to off-center the label slightly for this button
         drawButtonRightTriangle(center);
         break;
      case Joystick::ButtonShapeRound:
      default:
         //shapeCircle.render(center);
         drawCircle(center, buttonHalfHeight);
         break;
   }

   // Now render joystick label or symbol
   Joystick::ButtonSymbol buttonSymbol =
         Joystick::JoystickPresetList[joystickIndex].buttonMappings[button].buttonSymbol;

   // Change color of label to the preset (default white)
   glColor(buttonColor);

   switch(buttonSymbol)
   {
      case Joystick::ButtonSymbolPsCircle:
         drawPlaystationCircle(center);
         break;
      case Joystick::ButtonSymbolPsCross:
         drawPlaystationCross(center);
         break;
      case Joystick::ButtonSymbolPsSquare:
         drawPlaystationSquare(center);
         break;
      case Joystick::ButtonSymbolPsTriangle:
         drawPlaystationTriangle(center);
         break;
      case Joystick::ButtonSymbolSmallLeftTriangle:
         drawSmallLeftTriangle(center);
         break;
      case Joystick::ButtonSymbolSmallRightTriangle:
         drawSmallRightTriangle(center);
         break;
      case Joystick::ButtonSymbolNone:
      default:
         drawString(location.x - getStringWidth(labelSize, label) / 2, location.y + 2, labelSize, label);
         break;
   }
}


void JoystickRender::drawPlaystationCross(const Point &center)
{
   glColor(Colors::paleBlue);
   Point p1(center + Point(-5, -5));
   Point p2(center + Point(5, 5));
   Point p3(center + Point(-5, 5));
   Point p4(center + Point(5, -5));

   F32 vertices[] = {
         p1.x, p1.y,
         p2.x, p2.y,
         p3.x, p3.y,
         p4.x, p4.y
   };
   renderVertexArray(vertices, ARRAYSIZE(vertices) / 2, GL_LINES);
}


void JoystickRender::drawPlaystationCircle(const Point &center)
{
   glColor(Colors::paleRed);
   drawCircle(center, 6);
}


void JoystickRender::drawPlaystationSquare(const Point &center)
{
   glColor(Colors::palePurple);
   Point p1(center + Point(-5, -5));
   Point p2(center + Point(-5, 5));
   Point p3(center + Point(5, 5));
   Point p4(center + Point(5, -5));

   F32 vertices[] = {
         p1.x, p1.y,
         p2.x, p2.y,
         p3.x, p3.y,
         p4.x, p4.y
   };
   renderVertexArray(vertices, ARRAYSIZE(vertices) / 2, GL_LINE_LOOP);
}


void JoystickRender::drawPlaystationTriangle(const Point &center)
{
   Point p1(center + Point(0, -7));
   Point p2(center + Point(6, 5));
   Point p3(center + Point(-6, 5));

   F32 vertices[] = {
         p1.x, p1.y,
         p2.x, p2.y,
         p3.x, p3.y
   };
   renderVertexArray(vertices, ARRAYSIZE(vertices) / 2, GL_LINE_LOOP);
}


void JoystickRender::drawSmallLeftTriangle(const Point & center)
{
   Point p1(center + Point(4, 0));
   Point p2(center + Point(-3, 5));
   Point p3(center + Point(-3, -5));

   F32 vertices[] = {
         p1.x, p1.y,
         p2.x, p2.y,
         p3.x, p3.y
   };
   renderVertexArray(vertices, ARRAYSIZE(vertices) / 2, GL_LINE_LOOP);
}


void JoystickRender::drawSmallRightTriangle(const Point & center)
{
   Point p1(center + Point(-4, 0));
   Point p2(center + Point(3, 5));
   Point p3(center + Point(3, -5));

   F32 vertices[] = {
         p1.x, p1.y,
         p2.x, p2.y,
         p3.x, p3.y
   };
   renderVertexArray(vertices, ARRAYSIZE(vertices) / 2, GL_LINE_LOOP);
}


void JoystickRender::drawButtonRightTriangle(const Point &center)
{
   Point p1(center + Point(-15, -9));
   Point p2(center + Point(-15, 10));
   Point p3(center + Point(12, 0));

   F32 vertices[] = {
         p1.x, p1.y,
         p2.x, p2.y,
         p3.x, p3.y
   };
   renderVertexArray(vertices, ARRAYSIZE(vertices) / 2, GL_LINE_LOOP);
}

////////// End rendering functions

} /* namespace Zap */
