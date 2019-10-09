#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <string>
#include <list>

// Our home grown test framework was getting stale and crusty;
// Now we are just a wrapper for Google Test
#include <gtest/gtest.h>

using namespace std;

#define TEST_CHECK(a)   EXPECT_TRUE(a)

#define TEST_REQUIRE(a) ASSERT_TRUE(a)

#define TEST_CHECK_EQUAL(a,b)   EXPECT_EQ(a,b)

#define TEST_REQUIRE_EQUAL(a,b) ASSERT_EQ(a,b)

#define TEST_CHECK_NOT_EQUAL(a,b)   EXPECT_NE(a,b)

#define TEST_REQUIRE_NOT_EQUAL(a,b) ASSERT_NE(a,b)

#define TEST_CHECK_THROW(a) { bool bCaught = false; try { a; } catch (...) { bCaught = true; } EXPECT_TRUE(bCaught); }

// normal test case (all tests will run)
// TEST is from google test
#define TEST_CASE(name) TEST(MpoLibTest, name)

// exclusive test flag ignored for now
#define TEST_CASE_EX(name) TEST_CASE(name)

#endif // TEST_FRAMEWORK_H
