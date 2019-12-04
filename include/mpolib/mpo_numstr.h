/*
 * mpo_numstr.h
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

// numstr.h
// by Matt Ownby

#ifndef NUMSTR_H
#define NUMSTR_H

#include <assert.h>
#include "mpo_dll.h"
#include "mpo_types.h"

/* Make sure the types really have the right sizes */
// NOTE : this idea inspired by SDL
#define MPO_COMPILE_TIME_ASSERT(name, x)               \
       typedef int MPO_dummy_ ## name[(x) * 2 - 1]

MPO_COMPILE_TIME_ASSERT(uint64, sizeof(MPO_UINT64) == 8);
MPO_COMPILE_TIME_ASSERT(sint64, sizeof(MPO_INT64) == 8);

#include <string>

using namespace std;

class EXPORT_ME numstr
{
public:
	static int ToInt32(const char *str);
	static unsigned int ToUint32(const char *str, int base = 10);
	static MPO_UINT64 ToUint64(const char *str, int base = 10);
	static double ToDouble(const char *s);
	static string ToStr(int i, int base = 10, unsigned int min_digits = 0);
	static void ToStr(string *pStr, int i, int base = 10, unsigned int min_digits = 0);
	static string ToStr(long int i, int base = 10, unsigned int min_digits = 0);
	static void ToStr(string *pStr, long int i, int base = 10, unsigned int min_digits = 0);
	static string ToStr(MPO_INT64 num, int base = 10, unsigned int min_digits = 0);
	static void ToStr(string *pStr, MPO_INT64 i, int base = 10, unsigned int min_digits = 0);
	static string ToStr(unsigned int u, int base = 10, unsigned int min_digits = 0);
	static string ToStr(long unsigned int u, int base = 10, unsigned int min_digits = 0);
	static string ToStr(unsigned char u, int base = 10, unsigned int min_digits = 0);
	static string ToStr(MPO_UINT64 u, int base = 10, unsigned int min_digits = 0);
	
	// NOTE : double cannot be > 2^63 (size of signed 64-bit int) or this conversion will fail
	static string ToStr(double d, unsigned int min_digits_before = 0, unsigned int min_digits_after = 0, unsigned int max_digits_after = 5);

	// converts raw bytes to KiB, MiB, or GiB to make it more readable
	static string ToUnitStr(MPO_UINT64 u);
	
	static unsigned int my_strlen(const char *s);
private:
	static bool is_digit(char ch, int base);
	
	// NOTE : this is put in the .h because VC++ 6.0 can't handle it any other way
	// convert a string to an unsigned number
	template <class T> static void ToUint(const char *str, T &result, int base)
	{
		bool found_first_digit = false;	// whether we have found the first digit or not
		
		result = 0;
		
		// go through each digit
		for (unsigned int i = 0; i < my_strlen(str); i++)
		{
			if (!found_first_digit)
			{
				if (is_digit(str[i], base))
				{
					found_first_digit = true;
				}
				
				// else it's an unknown character, so we ignore it until we get to the first digit
			}
			
			// note: we do not want this to be an "else if" because the above 'if' needs to flow into this
			if (found_first_digit)
			{
				// converting from base10 ASCII
				if ((base == 10) && (str[i] >= '0') && (str[i] <= '9'))
				{
					result *= 10;
					result += str[i] - '0';
				}
				// converting from HEX ASCII
				else if (base == 16)
				{
					// if the number is between '0' and '9'
					if ((str[i] >= '0') && (str[i] <= '9'))
					{
						result *= 16;
						result += str[i] - '0';
					}
					// if the number is between 'A' and 'F'
					else if ((toupper(str[i]) >= 'A') && (toupper(str[i]) <= 'F'))
					{
						result *= 16;
						result += (toupper(str[i]) - 'A') + 10;	// A is the same as 10 decimal
					}
				}
				// else other bases are unsupported
				else
				{
					break;
				}
			}
		}
	}

};

#endif	// NUMSTR_H
