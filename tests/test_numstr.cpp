#include "test_headers.h"

// returns true if desired value is 'close enough' to actual value (within the PRECIS constant)
bool is_close_enough(double desired, double actual)
{
	double diff = desired - actual;
	const double PRECIS = 0.00001;
	return ((diff > -PRECIS) && (diff < PRECIS));
}

// regular pow() was being retarded, so I wrote my own
double my_pow(double d, unsigned int val)
{
	double result = 1;

	for (unsigned int u = 0; u < val; u++)
	{
		result *= d;
	}

	return result;
}

TEST_CASE(numstr)
{
	string s;

	numstr::ToStr(&s, -1, 10, 0);
	TEST_CHECK_EQUAL(s, "-1");

	// integers to strings
	TEST_CHECK((numstr::ToStr(57) == "57"));
    TEST_CHECK((numstr::ToStr(253, 10, 5) == "00253"));
    TEST_CHECK((numstr::ToStr(4567, 10, 2) == "4567"));
    TEST_CHECK((numstr::ToStr(-44) == "-44"));
    TEST_CHECK((numstr::ToStr((int) 0x7FFFFFFF) == "2147483647"));
    TEST_CHECK((numstr::ToStr((int) 0x80000000) == "-2147483648"));
    TEST_CHECK((numstr::ToStr((int) 0) == "0"));
    TEST_CHECK((numstr::ToStr((MPO_INT64) 0x7FFFFFFFFFFFFFFF) == "9223372036854775807"));
    TEST_CHECK((numstr::ToStr((MPO_INT64) 0x8000000000000000) == "-9223372036854775808"));
    TEST_CHECK((numstr::ToStr((MPO_INT64) 0x7FFFFFFFFFFFFFFF, 16) == "7FFFFFFFFFFFFFFF"));
    TEST_CHECK((numstr::ToStr((MPO_INT64) 0xFFFFFFFFFFFFFFFF, 16) == "-1"));
    TEST_CHECK((numstr::ToStr((MPO_UINT64) 0x8000000000000000, 16) == "8000000000000000"));

	// strings to ints
    TEST_CHECK((numstr::ToUint32("235") == 235));
    TEST_CHECK((numstr::ToUint32("blah...4756**") == 4756));
    TEST_CHECK((numstr::ToUint32("no num here") == 0));
    TEST_CHECK((numstr::ToUint32("  4.5.  ") == 4));

	// doubles to strings
	// (have to specify max digits because of double precision, or lack thereof)
    TEST_CHECK((numstr::ToStr(3.14, 0, 0, 2) == "3.14"));
    TEST_CHECK((numstr::ToStr(1234.5678, 0, 0, 4) == "1234.5678"));
    TEST_CHECK((numstr::ToStr(85899345920.345346, 0, 0, 3) == "85899345920.345"));
    TEST_CHECK((numstr::ToStr(-85899345920.345346, 0, 0, 3) == "-85899345920.345"));
    TEST_CHECK((numstr::ToStr(my_pow(2, 62), 0, 0, 1) == "4611686018427387904.0"));
    TEST_CHECK((numstr::ToStr(my_pow(2, 64), 0, 0, 1) == "(overflow)"));
    TEST_CHECK((numstr::ToStr(my_pow(2, 62) * -1.0, 0, 0, 1) == "-4611686018427387904.0"));
    TEST_CHECK((numstr::ToStr(0.99590913484638233, 0, 0, 2) == "1.00"));
    TEST_CHECK((numstr::ToStr(1.05, 0, 0, 1) == "1.1"));
    TEST_CHECK((numstr::ToStr(-1.05, 0, 0, 1) == "-1.1"));

	// this one tests proper rounding (without rounding the result would be 101.09)
    TEST_CHECK((numstr::ToStr(101.1, 0, 2, 2) == "101.10"));

	// TODO : strip off trailing digits if they are all 0's

	// str to double
    TEST_CHECK(is_close_enough(5678.1234, numstr::ToDouble("5678.1234")));
    TEST_CHECK(is_close_enough(0.56, numstr::ToDouble(".56")));
    TEST_CHECK(is_close_enough(0.78, numstr::ToDouble("0.78")));
    TEST_CHECK(is_close_enough(5.0, numstr::ToDouble("5")));
    TEST_CHECK(is_close_enough(-57.8, numstr::ToDouble("-57.8")));
    TEST_CHECK(is_close_enough(-0.3, numstr::ToDouble("-.3")));
    TEST_CHECK((numstr::ToDouble("-0") == 0));
}
