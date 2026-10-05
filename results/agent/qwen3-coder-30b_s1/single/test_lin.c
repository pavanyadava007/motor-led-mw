#include "mlmw_test.h"
#include "mlmw/lin.h"
#include <string.h>

#define SUITE agent_lin







ZTEST(SUITE, test_pid_valid_basic)
{
	/* Valid PID for ID 0 */
	uint8_t pid = mlmw_lin_pid(0);
	EXPECT_TRUE(mlmw_lin_pid_valid(pid));

	/* Valid PID for ID 1 */
	pid = mlmw_lin_pid(1);
	EXPECT_TRUE(mlmw_lin_pid_valid(pid));

	/* Invalid PID - wrong parity */
	EXPECT_FALSE(mlmw_lin_pid_valid(0x01)); /* Should be 0x00 for ID 0 */
	EXPECT_FALSE(mlmw_lin_pid_valid(0x41)); /* Should be 0x40 for ID 1 */
}



ZTEST(SUITE, test_checksum_classic)
{
	uint8_t data[] = {0x01, 0x02, 0x03};
	uint8_t checksum;

	/* Test classic checksum with ID 0 */
	checksum = mlmw_lin_checksum(0x00, data, 3, false);
	EXPECT_EQ(checksum, 0xF9); /* (0x01 + 0x02 + 0x03) = 0x06, ~0x06 = 0xF9 */

	/* Test classic checksum with ID 1 */
	checksum = mlmw_lin_checksum(0x40, data, 3, false);
	EXPECT_EQ(checksum, 0xF9); /* Same result as above */
}









ZTEST(SUITE, test_checksum_edge_cases)
{
	uint8_t data[] = {0x00};
	uint8_t checksum;

	/* Test with single zero byte */
	checksum = mlmw_lin_checksum(0x00, data, 1, false);
	EXPECT_EQ(checksum, 0xFF); /* ~0x00 = 0xFF */

	/* Test with single non-zero byte */
	checksum = mlmw_lin_checksum(0x00, data, 1, true);
	EXPECT_EQ(checksum, 0xFF); /* Same result */
}

ZTEST_SUITE(SUITE, NULL, NULL, NULL, NULL, NULL);
