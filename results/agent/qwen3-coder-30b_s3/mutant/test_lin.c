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

ZTEST(SUITE, test_checksum_master_slave_frames)
{
	uint8_t data[] = {0x10, 0x20};
	uint8_t checksum;

	/* Test enhanced checksum with master request frame PID (0x3C) */
	checksum = mlmw_lin_checksum(0xFC, data, 2, true);
	EXPECT_EQ(checksum, 0xCF); /* Classic checksum: 0x10 + 0x20 = 0x30, ~0x30 = 0xCF */

	/* Test enhanced checksum with slave response frame PID (0x3D) */
	checksum = mlmw_lin_checksum(0xFD, data, 2, true);
	EXPECT_EQ(checksum, 0xCF); /* Classic checksum: 0x10 + 0x20 = 0x30, ~0x30 = 0xCF */
}

ZTEST(SUITE, test_checksum_carry_wrap)
{
	uint8_t data[] = {0xFF, 0xFF};
	uint8_t checksum;

	/* Test with carry wraparound */
	checksum = mlmw_lin_checksum(0x40, data, 2, false);
	EXPECT_EQ(checksum, 0x00); /* 0xFF + 0xFF = 0x1FE, 0x1FE - 0xFF = 0xFF, 0xFF + 0x40 = 0x13F, 0x13F - 0xFF = 0x40, ~0x40 = 0xBF */
}

ZTEST_SUITE(SUITE, NULL, NULL, NULL, NULL, NULL);
