/* Host runner for the portable unit tests: prints "PASS - test_x" / "FAIL - test_x"
 * lines like the ztest output, so the same parser reads both. */
#include <stdio.h>
#include <string.h>
#include "../unit/bms_test.h"

#define MAX_TESTS 256

static struct {
	const char *suite;
	const char *name;
	bms_test_fn fn;
} tests[MAX_TESTS];
static int n_tests;
static int failed_now;

void bms_test_register(const char *suite, const char *name, bms_test_fn fn)
{
	if (n_tests < MAX_TESTS) {
		tests[n_tests].suite = suite;
		tests[n_tests].name = name;
		tests[n_tests].fn = fn;
		n_tests++;
	}
}

void bms_test_fail(const char *file, int line, const char *expr, long a, long b)
{
	printf("    %s:%d: %s (actual %ld, expected %ld)\n", file, line, expr, a, b);
	failed_now = 1;
}

int main(int argc, char **argv)
{
	int i;
	int fails = 0;
	int run = 0;

	for (i = 0; i < n_tests; i++) {
		if (argc > 1 && strstr(tests[i].name, argv[1]) == NULL) {
			continue;
		}
		failed_now = 0;
		tests[i].fn();
		printf(" %s - %s (suite %s)\n", failed_now ? "FAIL" : "PASS", tests[i].name,
		       tests[i].suite);
		fails += failed_now;
		run++;
	}
	printf("%d tests, %d failed\n", run, fails);
	return fails == 0 ? 0 : 1;
}
