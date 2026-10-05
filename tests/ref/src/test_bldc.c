#include <zephyr/ztest.h>
#include "mlmw/bldc.h"

ZTEST(ref_bldc, test_hall_sequence)
{
	const uint8_t order[6] = {1, 3, 2, 6, 4, 5};

	for (int s = 0; s < 6; s++) {
		zassert_equal(mlmw_hall_to_sector(order[s]), s);
	}
	zassert_equal(mlmw_hall_to_sector(0), -1);
	zassert_equal(mlmw_hall_to_sector(7), -1);
}

ZTEST(ref_bldc, test_commutation_forward)
{
	zassert_equal(mlmw_commutation(0, true), MLMW_AH | MLMW_BL);
	zassert_equal(mlmw_commutation(3, true), MLMW_BH | MLMW_AL);
	zassert_equal(mlmw_commutation(-1, true), 0);
	zassert_equal(mlmw_commutation(6, true), 0);
}

ZTEST(ref_bldc, test_commutation_one_high_one_low)
{
	for (int s = 0; s < 6; s++) {
		for (int dir = 0; dir < 2; dir++) {
			uint8_t m = mlmw_commutation((int8_t)s, dir);
			uint8_t hi = m & (MLMW_AH | MLMW_BH | MLMW_CH);
			uint8_t lo = m & (MLMW_AL | MLMW_BL | MLMW_CL);

			zassert_true(hi && !(hi & (hi - 1)), "one high side");
			zassert_true(lo && !(lo & (lo - 1)), "one low side");
			zassert_equal(hi & (lo << 1), 0, "no shoot-through");
		}
	}
}

ZTEST(ref_bldc, test_speed)
{
	/* 1000 us per edge, 2 pole pairs: 60e6 / 12000 = 5000 rpm */
	zassert_equal(mlmw_speed_rpm(1000, 2), 5000);
	zassert_equal(mlmw_speed_rpm(0, 2), 0);
	zassert_equal(mlmw_speed_rpm(1000, 0), 0);
}

ZTEST(ref_bldc, test_ramp)
{
	zassert_equal(mlmw_ramp(0, 100, 10), 10);
	zassert_equal(mlmw_ramp(0, -100, 10), -10);
	zassert_equal(mlmw_ramp(95, 100, 10), 100);
	zassert_equal(mlmw_ramp(5, 5, 10), 5);
	zassert_equal(mlmw_ramp(0, 100, 0), 0);
}

ZTEST(ref_bldc, test_pi_proportional)
{
	mlmw_pi_t pi = {.kp_q15 = 16384, .ki_q15 = 0, .out_min = -1000, .out_max = 1000};

	mlmw_pi_reset(&pi);
	zassert_equal(mlmw_pi_step(&pi, 100), 50);
	zassert_equal(mlmw_pi_step(&pi, 10000), 1000);
	zassert_equal(mlmw_pi_step(&pi, -10000), -1000);
}

ZTEST(ref_bldc, test_pi_integrates)
{
	mlmw_pi_t pi = {.kp_q15 = 0, .ki_q15 = 32767, .out_min = -1000, .out_max = 1000};

	mlmw_pi_reset(&pi);
	mlmw_pi_step(&pi, 100);
	int32_t u = mlmw_pi_step(&pi, 100);

	zassert_true(u >= 198 && u <= 200, "u=%d", u);
}

ZTEST(ref_bldc, test_pi_antiwindup)
{
	mlmw_pi_t pi = {.kp_q15 = 0, .ki_q15 = 32767, .out_min = -100, .out_max = 100};

	mlmw_pi_reset(&pi);
	for (int k = 0; k < 50; k++) {
		mlmw_pi_step(&pi, 1000);
	}
	zassert_true(pi.integ <= 100);
	/* recovers immediately once the error changes sign */
	zassert_true(mlmw_pi_step(&pi, -50) < 100);
}

ZTEST(ref_bldc, test_fault_overcurrent_debounce)
{
	mlmw_fault_t f;

	mlmw_fault_init(&f, 1000, 3, 100, 0);
	zassert_equal(mlmw_fault_update(&f, 1500, 1, true, true), MLMW_FAULT_NONE);
	zassert_equal(mlmw_fault_update(&f, 1500, 2, true, true), MLMW_FAULT_NONE);
	zassert_equal(mlmw_fault_update(&f, 1500, 3, true, true), MLMW_FAULT_OVERCURRENT);
	/* latched */
	zassert_equal(mlmw_fault_update(&f, 0, 4, true, true), MLMW_FAULT_OVERCURRENT);
	mlmw_fault_clear(&f, 5);
	zassert_equal(mlmw_fault_update(&f, 0, 6, true, true), MLMW_FAULT_NONE);
}

ZTEST(ref_bldc, test_fault_stall)
{
	mlmw_fault_t f;

	mlmw_fault_init(&f, 1000, 3, 100, 0);
	zassert_equal(mlmw_fault_update(&f, 0, 50, false, true), MLMW_FAULT_NONE);
	zassert_equal(mlmw_fault_update(&f, 0, 150, false, true), MLMW_FAULT_STALL);
}

ZTEST(ref_bldc, test_no_stall_when_stopped)
{
	mlmw_fault_t f;

	mlmw_fault_init(&f, 1000, 3, 100, 0);
	zassert_equal(mlmw_fault_update(&f, 0, 500, false, false), MLMW_FAULT_NONE);
}

ZTEST_SUITE(ref_bldc, NULL, NULL, NULL, NULL, NULL);
