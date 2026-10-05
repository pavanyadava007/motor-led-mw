#include "mlmw_test.h"
#include "mlmw/lin.h"
#include <string.h>

#define SUITE agent_lin











ZTEST(SUITE, test_checksum_classic)
{
	uint8_t data[] = {0x01, 0x02, 0x03};
	uint8_t checksum;

	/* Test classic checksum with PID 0x40 (id = 1) */
	checksum = mlmw_lin_checksum(0x40, data, 3, false);
	EXPECT_EQ(checksum, 0xF9); /* 0x01 + 0x02 + 0x03 = 0x06, ~0x06 = 0xF9 */

	/* Test classic checksum with PID 0x80 (id = 0) */
	checksum = mlmw_lin_checksum(0x80, data, 3, false);
	EXPECT_EQ(checksum, 0xF9); /* Same result as above */
}













ZTEST_SUITE(SUITE, NULL, NULL, NULL, NULL, NULL);
