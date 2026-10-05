#include "mlmw_test.h"
#include "mlmw/lin.h"
#include <string.h>

#define SUITE agent_lin













ZTEST(SUITE, test_checksum_classic_no_data)
{
	/* Test classic checksum with no data */
	uint8_t checksum = mlmw_lin_checksum(0x00, NULL, 0, false);
	EXPECT_EQ(checksum, 0xFF);

	/* Test classic checksum with id 1 and no data */
	checksum = mlmw_lin_checksum(0x40, NULL, 0, false);
	EXPECT_EQ(checksum, 0xFF);
}

ZTEST(SUITE, test_checksum_classic_with_data)
{
	uint8_t data[] = {0x01, 0x02, 0x03};
	uint8_t checksum = mlmw_lin_checksum(0x00, data, 3, false);
	/* Sum = 0x01 + 0x02 + 0x03 = 0x06 */
	/* Inverted = ~0x06 = 0xF9 */
	EXPECT_EQ(checksum, 0xF9);

	data[0] = 0xFF;
	data[1] = 0x01;
	data[2] = 0x00;
	checksum = mlmw_lin_checksum(0x40, data, 3, false);
	/* Sum = 0xFF + 0x01 + 0x00 = 0x100 */
	/* Carry wrap: 0x100 - 0xFF = 0x01 */
	/* Inverted = ~0x01 = 0xFE */
	EXPECT_EQ(checksum, 0xFE);
}

ZTEST(SUITE, test_checksum_enhanced_master_req)
{
	/* Test enhanced checksum with master request id (should use classic) */
	uint8_t data[] = {0x01, 0x02};
	uint8_t checksum = mlmw_lin_checksum(MLMW_LIN_ID_MASTER_REQ, data, 2, true);
	/* Sum = 0x01 + 0x02 = 0x03 */
	/* Inverted = ~0x03 = 0xFC */
	EXPECT_EQ(checksum, 0xFC);
}

ZTEST(SUITE, test_checksum_enhanced_slave_resp)
{
	/* Test enhanced checksum with slave response id (should use classic) */
	uint8_t data[] = {0x01, 0x02};
	uint8_t checksum = mlmw_lin_checksum(MLMW_LIN_ID_SLAVE_RESP, data, 2, true);
	/* Sum = 0x01 + 0x02 = 0x03 */
	/* Inverted = ~0x03 = 0xFC */
	EXPECT_EQ(checksum, 0xFC);
}

ZTEST(SUITE, test_checksum_enhanced_normal_frame)
{
	/* Test enhanced checksum with normal frame id */
	uint8_t data[] = {0x10, 0x20};
	uint8_t checksum = mlmw_lin_checksum(0x50, data, 2, true);
	/* Sum = 0x50 + 0x10 + 0x20 = 0x80 */
	/* Inverted = ~0x80 = 0x7F */
	EXPECT_EQ(checksum, 0x7F);

	/* Test with carry wrap */
	data[0] = 0xFF;
	data[1] = 0x01;
	checksum = mlmw_lin_checksum(0x50, data, 2, true);
	/* Sum = 0x50 + 0xFF + 0x01 = 0x150 */
	/* Carry wrap: 0x150 - 0xFF = 0x51 */
	/* Inverted = ~0x51 = 0xAE */
	EXPECT_EQ(checksum, 0xAE);
}

ZTEST(SUITE, test_checksum_enhanced_with_pid_inclusion)
{
	/* Test enhanced checksum with PID inclusion */
	uint8_t data[] = {0x01, 0x02};
	uint8_t pid = mlmw_lin_pid(0x15); /* id = 0x15, P0 = 1^0^1^0 = 0, P1 = !(0^1^0^1) = !0 = 1 */
	/* pid = 0x15 | (0 << 6) | (1 << 7) = 0x15 | 0x80 = 0x95 */
	uint8_t checksum = mlmw_lin_checksum(0x95, data, 2, true);
	/* Sum = 0x95 + 0x01 + 0x02 = 0x98 */
	/* Inverted = ~0x98 = 0x67 */
	EXPECT_EQ(checksum, 0x67);
}

ZTEST(SUITE, test_checksum_zero_data)
{
	uint8_t data[] = {0x00, 0x00, 0x00};
	uint8_t checksum = mlmw_lin_checksum(0x00, data, 3, false);
	/* Sum = 0x00 + 0x00 + 0x00 = 0x00 */
	/* Inverted = ~0x00 = 0xFF */
	EXPECT_EQ(checksum, 0xFF);
}

ZTEST(SUITE, test_checksum_with_large_data)
{
	uint8_t data[255];
	for (int i = 0; i < 255; i++) {
		data[i] = i;
	}
	uint8_t checksum = mlmw_lin_checksum(0x00, data, 255, false);
	/* Sum = 0 + 1 + 2 + ... + 254 = 254*255/2 = 32070 */
	/* 32070 = 0x7D4E */
	/* Carry wrap happens multiple times */
	/* Let's compute manually step by step in the function */
	uint16_t sum = 0;
	for (int i = 0; i < 255; i++) {
		sum += i;
		if (sum > 0xFF) {
			sum = sum - 0xFF;
		}
	}
	sum = ~sum & 0xFF;
	EXPECT_EQ(checksum, sum);
}

ZTEST_SUITE(SUITE, NULL, NULL, NULL, NULL, NULL);
