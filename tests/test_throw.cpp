#include "test_headers.h"

void thrower()
{
	throw "I'm throwing an exception!";
}

TEST_CASE(test_throw)
{
	TEST_CHECK_THROW(thrower());
}