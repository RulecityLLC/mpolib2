/*
 * mpo_timer.h
 *
 * Copyright (C) 2005 Matthew P. Ownby
 *
 * This file is part of MPOLIB, a multi-purpose library
 *
 * MPOLIB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPOLIB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// MPO's NOTE:
//  I may wish to use MPOLIB in a proprietary product some day.  Therefore,
//   the only way I can accept other people's changes to my code is if they
//   give me full ownership of those changes.

#ifndef TIMER_H
#define TIMER_H

#include "mpo_dll.h"

#include <mpolib2/mpo_fileio.h>	// for UINT64 type
#include <string>

using namespace std;

// returns a time value in ms that can be compared to subsequent time
// values to compute elapsed time.  The time value is only useful when
// compared against other time values.
EXPORT_ME unsigned int refresh_timer();

// returns subtracts the old time from the current time and returns
// the result (in ms)
EXPORT_ME unsigned int get_elapsed_ms(unsigned int old_time);

// Returns true if the elapsed time is equal or greater to the interval
EXPORT_ME bool time_elapsed(unsigned int interval, unsigned int old_time);

// sleeps for a certain amount of time
EXPORT_ME void make_delay(unsigned int ms);

// gets a 64-bit random number, influenced by current system time
EXPORT_ME MPO_UINT64 get_rand_num64();

// gets a 32-bit random number
EXPORT_ME unsigned int get_rand_num32();

// converts a remaining 'seconds' value to days, hours, minute, and seconds
EXPORT_ME string SToStr(unsigned int uS);

////////

// the purpose of this interface/class is so we can mock it out with unit tests
class IMpoTimer
{
public:
	virtual unsigned int GetCurValMs() = 0;

	virtual unsigned int GetElapsedMs(unsigned int uOldTime) = 0;
};

class MpoTimer : public IMpoTimer
{
public:
	MpoTimer();
	unsigned int GetCurValMs();
	unsigned int GetElapsedMs(unsigned int uOldTime);
};

#endif
