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

#ifndef _POINT_H_
#define _POINT_H_

namespace TNL{ class BitStream; } // or #include "tnlBitStream.h"
#include <string>

// forward declarations
namespace TNL {
   typedef float F32;
};


namespace Zap
{

class Point
{
public:
   TNL::F32 x;
   TNL::F32 y;

   // Constructors...
   Point();
   Point(const Point& pt);

   // Thanks, Ben & Mike!
   // Templates required to be in headers
   template<class T, class U>
   Point(T in_x, U in_y) { x = static_cast<TNL::F32>(in_x); y = static_cast<TNL::F32>(in_y); }

   template<class T, class U>
   void set(T ix, U iy) { x = (TNL::F32)ix; y = (TNL::F32)iy; }

   void set(const Point &pt);
   void set(const Point *pt);

   TNL::F32 len() const;		// Distance from (0,0)
   TNL::F32 lenSquared() const;
   void normalize();
   void normalize(float newLen);
   TNL::F32 ATAN2() const;
   TNL::F32 distanceTo(const Point &pt) const;
   TNL::F32 distSquared(const Point &pt) const;

   TNL::F32 angleTo(const Point &p) const;

   Point rotate(TNL::F32 ang);

   void setAngle(const TNL::F32 ang);
   void setPolar(const TNL::F32 l, const TNL::F32 ang);

   TNL::F32 determinant(const Point &p);

   void scaleFloorDiv(float scaleFactor, float divFactor);

   TNL::F32 dot(const Point &p) const;
   void read(const char **argv);
   void read(TNL::BitStream *stream);
   void write(TNL::BitStream *stream);

   std::string toString();
   
   // inlines  need to be in header, too
   inline Point operator+(const Point &pt) const
   {
      return Point (x + pt.x, y + pt.y);
   }

   inline Point operator-(const Point &pt) const
   {
      return Point (x - pt.x, y - pt.y);
   }

   inline Point operator-() const
   {
      return Point(-x, -y);
   }

   inline Point& operator+=(const Point &pt)
   {
      x += pt.x;
      y += pt.y;
      return *this;
   }

   inline Point& operator-=(const Point &pt)
   {
      x -= pt.x;
      y -= pt.y;
      return *this;
   }

   inline Point operator*(const TNL::F32 f)
   {
      return Point (x * f, y * f);
   }

   inline Point operator/(const TNL::F32 f)
   {
      return Point (x / f, y / f);
   }

   inline Point& operator*=(const TNL::F32 f)
   {
      x *= f;
      y *= f;
      return *this;
   }

   inline Point& operator/=(const TNL::F32 f)
   {
      x /= f;
      y /= f;
      return *this;
   }

   inline Point operator*(const Point &pt)
   {
      return Point(x * pt.x, y * pt.y);
   }

   inline Point operator/(const Point &pt)
   {
      return Point(x / pt.x, y / pt.y);
   }

   // Performance equivalent to set
   inline Point& operator=(const Point &pt)
   {
      x = pt.x;
      y = pt.y;
      return *this;
   }

   inline bool operator==(const Point &pt) const
   {
      return x == pt.x && y == pt.y;
   }

   inline bool operator!=(const Point &pt) const
   {
      return x != pt.x || y != pt.y;
   }
}; // class


};	// namespace

#endif

