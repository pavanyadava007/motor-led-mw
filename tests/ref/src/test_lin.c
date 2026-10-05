#include <zephyr/ztest.h>
#include "mlmw/lin.h"

ZTEST(ref_lin, test_pid_known_values)
{
	/* values from the LIN 2.2A specification table */
	zassert_equal(mlmw_lin_pid(0x00), 0x80);
	zassert_equal(mlmw_lin_pid(0x01), 0xC1);
	zassert_equal(mlmw_lin_pid(0x3C), 0x3C);
	zassert_equal(mlmw_lin_pid(0x3D), 0x7D);
	zassert_equal(mlmw_lin_pid(0x10), 0x50);
}

ZTEST(ref_lin, test_pid_ignores_upper_bits)
{
	zassert_equal(mlmw_lin_pid(0xC1), mlmw_lin_pid(0x01));
}

ZTEST(ref_lin, test_pid_valid)
{
	for (uint8_t id = 0; id < 64; id++) {
		uint8_t pid = mlmw_lin_pid(id);

		zassert_true(mlmw_lin_pid_valid(pid));
		zassert_false(mlmw_lin_pid_valid(pid ^ 0x40));
		zassert_false(mlmw_lin_pid_valid(pid ^ 0x80));
	}
}

ZTEST(ref_lin, test_checksum_classic)
{
	uint8_t d[] = {0x4A, 0x55, 0x93, 0xE5};

	/* 0x4A+0x55=0x9F, +0x93=0x132->0x33, +0xE5=0x118->0x19, ~ = 0xE6 */
	zassert_equal(mlmw_lin_checksum(0x50, d, sizeof(d), false), 0xE6);
}

ZTEST(ref_lin, test_checksum_enhanced_includes_pid)
{
	uint8_t d[] = {0x01, 0x02};

	zassert_equal(mlmw_lin_checksum(0x50, d, 2, true), (uint8_t)~(0x50 + 0x03));
}

ZTEST(ref_lin, test_checksum_diag_frames_classic)
{
	uint8_t d[] = {0x10, 0x20};
	uint8_t pid = mlmw_lin_pid(0x3C);

	zassert_equal(mlmw_lin_checksum(pid, d, 2, true), mlmw_lin_checksum(pid, d, 2, false));
}

ZTEST_SUITE(ref_lin, NULL, NULL, NULL, NULL, NULL);
