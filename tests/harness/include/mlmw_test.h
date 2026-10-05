/* Assertion helpers for the on-target suites: on failure they print the expression,
 * the actual value and the expected value, so a failing run says what the code did. */
#ifndef MLMW_TEST_H
#define MLMW_TEST_H

#include <zephyr/ztest.h>

#define EXPECT_EQ(actual, expected)                                                        \
	zassert_equal((long)(actual), (long)(expected), "%s: actual %ld, expected %ld",     \
		      #actual, (long)(actual), (long)(expected))

#define EXPECT_TRUE(cond) zassert_true((cond), "%s is false", #cond)
#define EXPECT_FALSE(cond) zassert_false((cond), "%s is true", #cond)

#endif
