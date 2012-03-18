//-----------------------------------------------------------------------------------
//
// Bitfighter - A multiplayer vector graphics space game
// Based on Zap demo relased for Torque Network Library by GarageGames.com
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

#include "Timer.h"

#include <cmath>

namespace Zap
{


Timer::Timer(U32 period)
{
   mCurrentCounter = mPeriod = period;
}


Timer::~Timer()
{
   // Do nothing
}


bool Timer::update(U32 timeDelta)
{
   if(mCurrentCounter == 0)
      return false;

   if (timeDelta >= mCurrentCounter)
   {
      mCurrentCounter = 0;
      return true;
   }
   mCurrentCounter -= timeDelta;
   return false;
}


U32 Timer::getCurrent() const
{
   return mCurrentCounter;
}


F32 Timer::getFraction() const
{
   if (!mPeriod)
      return 0;
   return mCurrentCounter / F32(mPeriod);
}


void Timer::setPeriod(U32 period)
{
   mPeriod = period;
}


U32 Timer::getPeriod() const
{
   return mPeriod;
}


U32 Timer::getElapsed() const
{
   return mPeriod - mCurrentCounter;
}


void Timer::reset()
{
   mCurrentCounter = mPeriod;
}


void Timer::extend(S32 time)
{
   U32 U32time = U32(abs(time));

   if(time > 0)
   {
      mPeriod += U32time;
      mCurrentCounter += U32time;
   }
   else if(time < 0)
   {
      mPeriod = mPeriod > U32time ? mPeriod - U32time : 0;
      mCurrentCounter = mCurrentCounter > U32time ? mCurrentCounter - U32time : 0;
   }
}


void Timer::reset(U32 newCounter, U32 newPeriod)
{
   if(newPeriod == 0)
      newPeriod = newCounter;

   mCurrentCounter = newCounter;
   mPeriod = newPeriod;
}


void Timer::clear()
{
   mCurrentCounter = 0;
}


} /* namespace Zap */
