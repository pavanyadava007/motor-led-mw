/* One test source for two runners: on the emulated target (Zephyr ztest) and on the
 * host (tests/host/runner.c). Each test carries a "verifies:" comment with the
 * requirement ids it covers; bms/tools/trace.py reads those tags. */
#ifndef BMS_TEST_H
#define BMS_TEST_H

#ifdef __ZEPHYR__
#include <zephyr/ztest.h>
#define BMS_TEST(suite, name) ZTEST(suite, name)
#define BMS_SUITE(suite) ZTEST_SUITE(suite, NULL, NULL, NULL, NULL, NULL)
/* arguments are evaluated exactly once (z_zassert is a function, so the message
 * arguments would otherwise run side effects a second time) */
#define CHECK(c)                                                                             \
	do {                                                                                 \
		bool c_ = (c);                                                               \
		zassert_true(c_, "%s", #c);                                                  \
	} while (0)
#define CHECK_EQ(a, b)                                                                       \
	do {                                                                                 \
		long a_ = (long)(a);                                                         \
		long b_ = (long)(b);                                                         \
		zassert_equal(a_, b_, "%s: actual %ld, expected %ld", #a, a_, b_);           \
	} while (0)
#define CHECK_NEAR(a, b, tol)                                                                \
	do {                                                                                 \
		double d_ = (double)(a) - (double)(b);                                       \
		zassert_true((d_ <= (double)(tol)) && (-d_ <= (double)(tol)),                \
			     "%s not within tolerance of %s", #a, #b);                       \
	} while (0)
#else
#include <stdio.h>
typedef void (*bms_test_fn)(void);
void bms_test_register(const char *suite, const char *name, bms_test_fn fn);
void bms_test_fail(const char *file, int line, const char *expr, long a, long b);
#define BMS_TEST(suite, name)                                                                \
	static void suite##_##name(void);                                                    \
	__attribute__((constructor)) static void reg_##suite##_##name(void)                  \
	{                                                                                    \
		bms_test_register(#suite, #name, suite##_##name);                            \
	}                                                                                    \
	static void suite##_##name(void)
#define BMS_SUITE(suite) extern int bms_suite_##suite##_unused
#define CHECK(c)                                                                             \
	do {                                                                                 \
		if (!(c)) {                                                                  \
			bms_test_fail(__FILE__, __LINE__, #c, 0, 0);                         \
			return;                                                              \
		}                                                                            \
	} while (0)
#define CHECK_EQ(a, b)                                                                       \
	do {                                                                                 \
		if ((long)(a) != (long)(b)) {                                                \
			bms_test_fail(__FILE__, __LINE__, #a " == " #b, (long)(a), (long)(b)); \
			return;                                                              \
		}                                                                            \
	} while (0)
#define CHECK_NEAR(a, b, tol)                                                                \
	do {                                                                                 \
		double d_ = (double)(a) - (double)(b);                                       \
		if (d_ > (double)(tol) || -d_ > (double)(tol)) {                             \
			bms_test_fail(__FILE__, __LINE__, #a " ~= " #b, 0, 0);               \
			return;                                                              \
		}                                                                            \
	} while (0)
#endif

#endif
