/*
 * mpo_numstr.cpp
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

#include <mpolib2/mpo_numstr.h>

#ifndef WIN32
#include <ctype.h>	// for toupper
#endif

const char *DIGITS = "0123456789ABCDEF";

// NOTE : this function doesn't do full safety checking in the interest of simplicity and speed
int numstr::ToInt32(const char *str)
{
	const int BASE = 10;	// for now we always assume base is 10 because I never seem to use anything else
	int result = 0;
	bool found_first_digit = false;	// whether we have found the first digit or not
	int sign_mult = 1;	// 1 if the number is positive, -1 if it's negative

	for (unsigned int i = 0; i < my_strlen(str); i++)
	{
		if (!found_first_digit)
		{
			if (is_digit(str[i], BASE))
			{
				found_first_digit = true;
			}

			// if it a negative number?
			else if (str[i] == '-')
			{
				sign_mult = -1;
			}

			// else it's an unknown character, so we ignore it until we get to the first digit
		}

		// note: we do not want this to be an "else if" because the above 'if' needs to flow into this
		if (found_first_digit)
		{
			// make sure we aren't dealing with any non-integers
			if ((str[i] >= '0') && (str[i] <= '9'))
			{
				result *= BASE;
				result += str[i] - '0';
			}
			// else we've hit unknown characters so we're done
			else
			{
				break;
			}
		}
	}

	return result * sign_mult;
}

unsigned int numstr::ToUint32(const char *str, int base)
{
	unsigned int u = 0;
	ToUint(str,u,base);
	return u;
}

MPO_UINT64 numstr::ToUint64(const char *str, int base)
{
	MPO_UINT64 u = 0;
	ToUint(str,u,base);
	return u;
}

double numstr::ToDouble(const char *str)
{
	const double BASE = 10.0;	// this makes it easier for now ...
	const double BASE_DIVIDE = 0.1;	// because multiplaying by 0.1 is faster than dividing by 10.0
	bool found_period = false;	// whether we've encountered the period in the double yet
	bool found_first_digit = false;
	double result = 0.0;
	double divide_by = 1.0;	// after we pass the decimal point, we need to divide subsequent numbers to put them in their proper sphere
	double sign_mult = 1.0;	// final result is multiplied by this to set the sign

	for (unsigned int i = 0; i < my_strlen(str); i++)
	{
		if (!found_first_digit)
		{
			if (is_digit(str[i], (int) BASE))
			{
				found_first_digit = true;
			}

			// if it a negative number?
			else if (str[i] == '-')
			{
				sign_mult = -1.0;
			}

			else if (str[i] == '.')
			{
				found_period = true;
			}

			// else it's an unknown character, so we ignore it until we get to the first digit
		}

		// note: we do not want this to be an "else if" because the above 'if' needs to flow into this
		if (found_first_digit)
		{
			// make sure we aren't dealing with any non-integers
			if ((str[i] >= '0') && (str[i] <= '9'))
			{
				// if we haven't encountered the decimal place yet
				if (found_period == false)
				{
					result *= BASE;
					result += str[i] - '0';
				}
				// if we are passed the decimal place
				else
				{
					divide_by *= BASE_DIVIDE;	// each new digit we find needs to be divided by 10x the previous divide_by value
					result += ((str[i] - '0') * divide_by);
				}
			}
			// else if we've hit the decimal point
			else if (str[i] == '.')
			{
				found_period = true;
			}
			// else we've hit unknown characters so we're done
			else
			{
				break;
			}
		}
	}

	return result * sign_mult;
}

template <class T, class UT> void IToStr(string *pStrDst, T num, int base = 10, unsigned int min_digits = 0)
{
	const char *DIGITS = "0123456789ABCDEF";
	char res[21];	// max 64-bit number is 19 digits, plus 1 char for negative sign and 1 char for null terminator
	int idx = sizeof(res)-1;	// start at the end of the array because we have to work backward
	bool bNeg = false;	// whether the number is negative
	int min_idx = idx - min_digits;

	res[sizeof(res)-1] = 0;

	// we consciously don't support a base above 16
	assert(base <= 16);

	// we won't support padding beyond the size of our array
	assert(min_digits <= (sizeof(res)-2));

	UT u;

	if (num >= 0)
	{
		u = num;
	}
	else
	{
		u = -num;
		bNeg = true;
	}

	do
	{
		idx--;
		res[idx] = DIGITS[(u % base)];
		u = u / base;
	} while (u != 0);

	// pad the front of the number with 0's to satisfy the min_digits requirement
	while (idx > min_idx)
	{
		idx--;
		res[idx] = '0';
	}

	if (bNeg)
	{
		idx--;
		res[idx] = '-';
	}

	pStrDst->assign(&res[idx], (sizeof(res)-1)-idx);
}

string numstr::ToStr(int num, int base, unsigned int min_digits)
{
	string strRes;
	IToStr<int,unsigned int>(&strRes, num, base, min_digits);
	return strRes;
}

void numstr::ToStr(string *pStr, int i, int base, unsigned int min_digits)
{
	IToStr<int,unsigned int>(pStr, i, base, min_digits);
}

string numstr::ToStr(long int i, int base, unsigned int min_digits)
{
	string strRes;
	IToStr<long int,unsigned long int>(&strRes, i, base, min_digits);
	return strRes;
}

void numstr::ToStr(string *pStr, long int i, int base, unsigned int min_digits)
{
	IToStr<long int,unsigned long int>(pStr, i, base, min_digits);
}

string numstr::ToStr(MPO_INT64 num, int base, unsigned int min_digits)
{
	string strRes;
	IToStr<MPO_INT64,MPO_UINT64>(&strRes, num, base, min_digits);
	return strRes;
}

void numstr::ToStr(string *pStr, MPO_INT64 i, int base, unsigned int min_digits)
{
	IToStr<MPO_INT64,MPO_UINT64>(pStr, i, base, min_digits);
}

template <class T> static void UToStr(string *pStrDst, T u, int base = 10, unsigned int min_digits = 0)
{
	const char *DIGITS = "0123456789ABCDEF";
	char res[20];	// max 64-bit number is 19 digits, plus 1 char for null terminator
	int idx = sizeof(res)-1;	// start at the end of the array because we have to work backward
	int min_idx = idx - min_digits;

	res[sizeof(res)-1] = 0;

	// we consciously don't support a base above 16
	assert(base <= 16);

	// we won't support padding beyond the size of our array
	assert(min_digits <= (sizeof(res)-1));

	do
	{
		idx--;
		res[idx] = DIGITS[(u % base)];
		u = u / base;
	} while (u != 0);

	// pad the front of the number with 0's to satisfy the min_digits requirement
	while (idx > min_idx)
	{
		idx--;
		res[idx] = '0';
	}

	pStrDst->assign(&res[idx], (sizeof(res)-1)-idx);
}

string numstr::ToStr(unsigned char c, int base, unsigned int min_digits)
{
	string strRes;
	UToStr(&strRes, c, base, min_digits);
	return strRes;
}

string numstr::ToStr(unsigned int u, int base, unsigned int min_digits)
{
	string strRes;
	UToStr(&strRes, u, base, min_digits);
	return strRes;
}

string numstr::ToStr(long unsigned int u, int base, unsigned int min_digits)
{
	string strRes;
	UToStr(&strRes, u, base, min_digits);
	return strRes;
}

string numstr::ToStr(MPO_UINT64 u, int base, unsigned int min_digits)
{
	string strRes;
	UToStr(&strRes, u, base, min_digits);
	return strRes;
}

string numstr::ToStr(double d, unsigned int min_digits_before, unsigned int min_digits_after, unsigned int max_digits_after)
{
	string result = "(overflow)";
	const double BASE = 10.0;	// we will only support base 10 with doubles to simplify things ...
	unsigned int decimal_length = 0;

	// bounds check: make sure the double is within our limits (2^62 was the highest I could get it to work without introducing overflow errors)
	if ((d <= 4611686018427387904.0) && (d >= -4611686018427387904.0))
	{
		// round properly according to max digits
		double dRounder = 0.5;
		for (unsigned int i = 0; i < max_digits_after; i++)
		{
			dRounder *= 0.1;
		}
		
		// we follow the same convention used by the round() function.  Namely, 0.5 goes to 1 and -0.5 goes to -1 when rounding to the nearest whole number.
		if (d >= 0)
		{
			d += dRounder;
		}
		else
		{
			d -= dRounder;
		}

		MPO_INT64 int64_portion = (MPO_INT64) d;	// strip off floating point part
		d = d - int64_portion;	// isolate just the decimal portion

		result = ToStr(int64_portion, 10, min_digits_before);	// use our other function to calculate the int portion

		result = result + ".";	// add decimal place, it will always be displayed even if there is no fractional value to this number

		if (d < 0) d *= -1.0;	// force d to be positive

		// NOTE : d will now always be positive
		do
		{
			d *= BASE;	// move decimal point one notch to the right
			int int_portion = (int) d;	// grab the number that is above the decimal point
			result = result + DIGITS[int_portion];
			d = d - int_portion;
			decimal_length++;	// gotta keep track of this for 'min_digits_after' calculation
		} while ((d != 0.0) && (max_digits_after > decimal_length));

		while (decimal_length < min_digits_after)
		{
			result = result + "0";	// pad trailing zeroes
			decimal_length++;
		}
	}
	// else return default result to indicate error

	return result;
}

string numstr::ToUnitStr(MPO_UINT64 u)
{
	string result;
	double d;

	// if less than 1 k
	if (u < 1024)
	{
		result = ToStr(u) + " B";
	}

	// less than 1 meg
	else if (u < 1048576)
	{
		d = u * 0.0009765625;	// same as dividing by 1024
		result = ToStr(d, 0, 1, 2) + " KiB";
	}

	// less than 1 gig
	else if (u < 1073741824)
	{
		d = u * (1.0/1048576.0);	// convert to megs
		result = ToStr(d, 0, 1, 2) + " MiB";
	}

	// else leave it as gigs, we won't go any higher for now
	else
	{
		d = u * (1.0/1073741824.0);	// convert to gigs
		result = ToStr(d, 0, 1, 2) + " GiB";
	}

	return result;
}

unsigned int numstr::my_strlen(const char *s)
{
	unsigned int i = 0;
	while (s[i] != 0) i++;
	return i;
}

////////////////////////////////
// private funcs

bool numstr::is_digit(char ch, int base)
{
	if ((base == 10) && (ch >= '0') && (ch <= '9')) return(true);
	else if ((base == 16) && (
		((ch >= '0') && (ch <= '9')) || 
		((toupper(ch) >= 'A') && (toupper(ch) <= 'F'))
		)) return(true);
	// else no other base is supported

	return false;
}
