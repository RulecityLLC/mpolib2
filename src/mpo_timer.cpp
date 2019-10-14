/*
 * mpo_timer.cpp
 *
 * Copyright (C) 2019 Matthew P. Ownby
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

#include <mpolib2/mpo_timer.h>
#include <mpolib2/mpo_numstr.h>
#include <stdexcept>
#include "mpo_timer_internal.h"

#ifdef WIN32
static double g_d641000DivPerfFreq;	// for QueryPerformanceFrequency
static bool g_bPerfFreqInitialized = false;

#else

#include <stdlib.h>	// for srand
#include <sys/time.h>
#include <unistd.h>
#endif


unsigned int MpoTimerUtil::RefreshTimer()
{
	unsigned int result = 0;

#ifndef WIN32
	struct timeval tv;

	if (gettimeofday(&tv, NULL) == 0)
	{
		tv.tv_sec &= 0x003FFFFF;        // to prevent overflow from *1000, we only care about relative time anyway
		result = (unsigned int) ((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
	}
	else
	{
		throw runtime_error("gettimeofday failed");
	}
#endif

#ifdef WIN32
	//result = timeGetTime();

	// this should only happen the first time this function is called
	if (!g_bPerfFreqInitialized)
	{
		LARGE_INTEGER u64PerfFreq;	// for QueryPerformanceFrequency
		QueryPerformanceFrequency(&u64PerfFreq);
		g_d641000DivPerfFreq = 1000.0 / u64PerfFreq.QuadPart;	// re-arrange so that we only need to multiply by a double later
		g_bPerfFreqInitialized = true;
	}

	LARGE_INTEGER u64PerfCount;
	if (QueryPerformanceCounter(&u64PerfCount))
	{
		result = (unsigned int) (u64PerfCount.QuadPart * g_d641000DivPerfFreq);
	}
	else
	{
		throw runtime_error("QueryPerformanceCounter failed");
	}
#endif

	return result;
}

// returns the difference between the current time and the old time
unsigned int MpoTimerUtil::GetElapsedMs(unsigned int old_time)
{
	return(RefreshTimer() - old_time);
}

// sleeps for a certain period of time
void MpoTimerUtil::MakeDelay(unsigned int ms)
{
#ifdef WIN32
	Sleep(ms);
#else
	usleep(ms * 1000);
#endif
}

MPO_UINT64 MpoTimerUtil::GetRandNum64()
{
	MPO_UINT64 result = 0;

#ifndef WIN32
	// preferred method: grabbing stuff from /dev/urandom (linux)
	FILE *F = fopen("/dev/urandom", "rb");
	if (F)
	{
		if (fread(&result, 1, sizeof(result), F) != sizeof(result))
		{
			// get rid of stupid g++ warnings
		}
		fclose(F);
	}
	// if /dev/urandom method fails, here is a more universal method
	else
	{
		struct timeval tv;
		gettimeofday(&tv, NULL);	// get a random number (time)
		srand(tv.tv_sec);
		result = rand() ^ tv.tv_sec;
		result <<= 32;
		result |= rand() ^ tv.tv_usec;
	}
#else
	SYSTEMTIME cur_time;
	FILETIME file_time;
	GetSystemTime(&cur_time);
	SystemTimeToFileTime(&cur_time, &file_time);
	srand(file_time.dwLowDateTime);
	result = rand() ^ file_time.dwHighDateTime;
	result <<= 32;
	result |= rand() ^ file_time.dwLowDateTime;
#endif

	return result;
}

unsigned int MpoTimerUtil::GetRandNum32()
{
	unsigned int result = 0;

#ifndef WIN32
	// preferred method: grabbing stuff from /dev/urandom (linux)
	FILE *F = fopen("/dev/urandom", "rb");
	if (F)
	{
		if (fread(&result, 1, sizeof(result), F) != sizeof(result))
		{
			// get rid of stupid g++ warnings
		}
		fclose(F);
	}
	// if /dev/urandom method fails, here is a more universal method
	else
	{
		struct timeval tv;
		gettimeofday(&tv, NULL);	// get a random number (time)
		srand(tv.tv_sec ^ tv.tv_usec);
		result = rand();
	}
#else
	SYSTEMTIME cur_time;
	FILETIME file_time;
	GetSystemTime(&cur_time);
	SystemTimeToFileTime(&cur_time, &file_time);
	srand(file_time.dwLowDateTime ^ file_time.dwHighDateTime);
	result = rand();
#endif

	return result;
}

string MpoTimerUtil::SToStr(unsigned int uS)
{
	const unsigned int S_PER_MIN = 60;
	const unsigned int MIN_PER_HOUR = 60;
	const unsigned int HOUR_PER_DAY = 24;

	const unsigned int S_PER_HOUR = S_PER_MIN * MIN_PER_HOUR;
	const unsigned int S_PER_DAY = S_PER_HOUR * HOUR_PER_DAY;

	unsigned int uDays = uS / S_PER_DAY;	// # of days
	uS = uS % S_PER_DAY;	// get remaining ms
	unsigned int uHours = uS / S_PER_HOUR;
	uS = uS % S_PER_HOUR;
	unsigned int uMins = uS / S_PER_MIN;
	unsigned int uSecs = uS % S_PER_MIN;

	string result = "";

	// we can be vague since it is a long time away
	if (uDays > 0)
	{
		result = numstr::ToStr(uDays) + " day";
		if (uDays > 1) result += "s";	// if there is more than 1 day, pluralize it

		// if we have some hours to report
		if (uHours > 0)
		{
			result += ", " + numstr::ToStr(uHours) + " hours";
		}
	}
	// less than 0 days, so we can do HH:MM:SS format
	else
	{
		result = numstr::ToStr(uHours, 10, 2) + ":" + numstr::ToStr(uMins, 10, 2) + ":" + numstr::ToStr(uSecs, 10, 2);
	}

	return result;
}

//////////

unsigned int MpoTimer::GetCurValMs()
{
	return MpoTimerUtil::RefreshTimer();
}

unsigned int MpoTimer::GetElapsedMs(unsigned int uOldTime)
{
	return MpoTimerUtil::GetElapsedMs(uOldTime);
}
