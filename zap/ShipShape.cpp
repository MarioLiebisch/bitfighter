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

#include "ShipShape.h"
#include "Colors.h"

namespace Zap
{


ShipShapeInfo ShipShape::shipShapeInfos[ShapeTypeCount] =
{
   // Normal
   {
      // Outer hull
      // uses GL_LINE_LOOP
      3,
      { -20, -15,   0, 25,   20, -15 },

      // Inner hull
      // uses GL_LINE_STRIP because it can come in pieces
      1,
      {
         {
            4,
            { -12, -13,   0, 22,   12, -13,   -12, -13 },
         },
      },

      // Corners - movement trails show from these
      3,
      { -20, -15,   0,  25,   20, -15 },

      // Flame ports
      // Uses GL_LINES - each pair is drawn as a separate line
      8,
      { -12.5, 0,   -12.5, 10,    -12.5, 10,   -7.5, 10,    7.5, 10,   12.5, 10,   12.5, 10,   12.5, 0 },

      // Flames
      // Uses GL_LINE_STRIP
      // Forward
      1,
      {
         {
            3,
            {
               {
                  Colors::red,
                  { -8, -15,   0, -15,   8, -15 },
                  -20,
               },
               {
                  Colors::orange50,
                  { -6, -15,   0, -15,   6, -15 },
                  -15,
               },
               {
                  Colors::yellow,
                  { -4, -15,   0, -15,   4, -15 },
                  -8,
               },
            },
         },
      },

      // Reverse
      2,
      {
         {
            2,
            {
               {
                  Colors::orange50,
                  { 7.5, 10,   10, 10,   12.5, 10 },
                  15,
               },
               {
                  Colors::yellow,
                  { 9, 10,   10, 10,   11, 10 },
                  10,
               },
            },
         },
         {
            2,
            {
               {
                  Colors::orange50,
                  { -7.5, 10,   -10, 10,   -12.5, 10 },
                  15,
               },
               {
                  Colors::yellow,
                  { -9, 10,   -10, 10,   -11, 10 },
                  10,
               },
            },
         },
      },

      // Port
      1,
      {
         {
            3,
            {
               {
                  Colors::red,
                  { 12.5, 10,   12.5, 5,   12.5, 0 },
                  18.75,
               },
               {
                  Colors::orange50,
                  { 12.5, 8,   12.5, 5,   12.5, 2 },
                  12.5,
               },
               {
                  Colors::yellow,
                  { 12.5, 6,   12.5, 5,   12.5, 4 },
                  6.25,
               },
            },
         },
      },

      // Starboard
      1,
      {
         {
            3,
            {
               {
                  Colors::red,
                  { -12.5, 10,   -12.5, 5,   -12.5, 0 },
                  -18.75,
               },
               {
                  Colors::orange50,
                  { -12.5, 8,   -12.5, 5,   -12.5, 2 },
                  -12.5,
               },
               {
                  Colors::yellow,
                  { -12.5, 6,   -12.5, 5,   -12.5, 4 },
                  -6.25,
               },
            },
         },
      },

   },


   // BirdOfPrey
   {
      // Outer hull
      20,
      {
            15, -6.25,
            6.25, -6.25,
            5, 15,
            6.25, 15,
            7.5, 18.75,
            2.5, 25,
            -2.5, 25,
            -7.5, 18.75,
            -6.25, 15,
            -5, 15,
            -6.25, -6.25,
            -15, -6.25,
            -17.5, 10,
            -20, 10,
            -22.5, -10,
            -10, -22.5,
            10, -22.5,
            22.5, -10,
            20, 10,
            17.5, 10,
            15, -6.25,
      },

      // Inner hull
      2,
      {
         {
            10,
            {
                  -7.5, -8.75,
                  -16.25, -8.75,
                  -18.75, 7.5,
                  -20, -8.75,
                  -10, -18.75,
                  10, -18.75,
                  20, -8.75,
                  18.75, 7.5,
                  16.25, -8.75,
                  7.5, -8.75,
            },
         },
         {
            6,
            {
                  -3.75, 16.25,
                  -5, 18.75,
                  -1.25, 22.5,
                  1.25, 22.5,
                  5, 18.75,
                  3.75, 16.25,
            },
         },
      },

      // Corners
      7,
      {
            0, 25,
            -20, 10,
            -22.5, -10,
            -10, -22.5,
            10, -22.5,
            22.5, -10,
            20, 10,
      },

      // Flame ports
      0,
      { 0, 0 },
   },
};

} /* namespace Zap */
