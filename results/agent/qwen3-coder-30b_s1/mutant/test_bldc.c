#include "mlmw_test.h"
#include "mlmw/bldc.h"
#include <string.h>

#define SUITE agent_bldc

ZTEST(SUITE, test_hall_to_sector_valid)
{
	EXPECT_EQ(mlmw_hall_to_sector(1), 0);
	EXPECT_EQ(mlmw_hall_to_sector(3), 1);
	EXPECT_EQ(mlmw_hall_to_sector(2), 2);
	EXPECT_EQ(mlmw_hall_to_sector(6), 3);
	EXPECT_EQ(mlmw_hall_to_sector(4), 4);
	EXPECT_EQ(mlmw_hall_to_sector(5), 5);
}

ZTEST(SUITE, test_hall_to_sector_invalid)
{
	EXPECT_EQ(mlmw_hall_to_sector(0), -1);
	EXPECT_EQ(mlmw_hall_to_sector(7), -1);
	EXPECT_EQ(mlmw_hall_to_sector(8), -1);
	EXPECT_EQ(mlmw_hall_to_sector(0xFF), -1);
}

ZTEST(SUITE, test_commutation_forward)
{
	EXPECT_EQ(mlmw_commutation(0, true), MLMW_AH | MLMW_BL);
	EXPECT_EQ(mlmw_commutation(1, true), MLMW_AH | MLMW_CL);
	EXPECT_EQ(mlmw_commutation(2, true), MLMW_BH | MLMW_CL);
	EXPECT_EQ(mlmw_commutation(3, true), MLMW_BH | MLMW_AL);
	EXPECT_EQ(mlmw_commutation(4, true), MLMW_CH | MLMW_AL);
	EXPECT_EQ(mlmw_commutation(5, true), MLMW_CH | MLMW_BL);
}

ZTEST(SUITE, test_commutation_reverse)
{
	EXPECT_EQ(mlmw_commutation(0, false), MLMW_BH | MLMW_AL);
	EXPECT_EQ(mlmw_commutation(1, false), MLMW_CH | MLMW_AL);
	EXPECT_EQ(mlmw_commutation(2, false), MLMW_CH | MLMW_BL);
	EXPECT_EQ(mlmw_commutation(3, false), MLMW_AH | MLMW_BL);
	EXPECT_EQ(mlmw_commutation(4, false), MLMW_AH | MLMW_CL);
	EXPECT_EQ(mlmw_commutation(5, false), MLMW_BH | MLMW_CL);
}

ZTEST(SUITE, test_commutation_invalid)
{
	EXPECT_EQ(mlmw_commutation(-1, true), 0);
	EXPECT_EQ(mlmw_commutation(6, true), 0);
	EXPECT_EQ(mlmw_commutation(0xFF, true), 0);
	EXPECT_EQ(mlmw_commutation(-1, false), 0);
	EXPECT_EQ(mlmw_commutation(6, false), 0);
	EXPECT_EQ(mlmw_commutation(0xFF, false), 0);
}

ZTEST(SUITE, test_speed_rpm_edge_cases)
{
	EXPECT_EQ(mlmw_speed_rpm(0, 2), 0);
	EXPECT_EQ(mlmw_speed_rpm(100000, 0), 0);
	EXPECT_EQ(mlmw_speed_rpm(0, 0), 0);
}

ZTEST(SUITE, test_ramp_no_change)
{
	EXPECT_EQ(mlmw_ramp(100, 200, 0), 100);
	EXPECT_EQ(mlmw_ramp(100, 200, -5), 100);
}

ZTEST(SUITE, test_ramp_move_towards_target)
{
	EXPECT_EQ(mlmw_ramp(100, 200, 50), 150);
	EXPECT_EQ(mlmw_ramp(200, 100, 50), 150);
	EXPECT_EQ(mlmw_ramp(100, 200, 100), 200);
	EXPECT_EQ(mlmw_ramp(200, 100, 100), 100);
}

ZTEST(SUITE, test_ramp_move_by_max_step)
{
	/* Test that ramp moves by exactly max_step when difference is larger */
	EXPECT_EQ(mlmw_ramp(100, 250, 100), 200); /* 100 + 100 = 200 */
	EXPECT_EQ(mlmw_ramp(250, 100, 100), 150); /* 250 - 100 = 150 */
}

ZTEST(SUITE, test_pi_reset)
{
	mlmw_pi_t pi = { .kp_q15 = 16384, .ki_q15 = 8192, .out_min = 0, .out_max = 1000, .integ = 500 };
	mlmw_pi_reset(&pi);
	EXPECT_EQ(pi.integ, 0);
}

ZTEST(SUITE, test_pi_step_normal)
{
	/* Test with kp=1 (Q15) and ki=0 (Q15), so P term should be err */
	mlmw_pi_t pi = { .kp_q15 = 32768, .ki_q15 = 0, .out_min = 0, .out_max = 1000, .integ = 0 };
	EXPECT_EQ(mlmw_pi_step(&pi, 100), 100); /* 100 * 32768/32768 + 0 = 100 */
}

ZTEST(SUITE, test_pi_step_integration)
{
	/* Test with kp=0 (Q15) and ki=1 (Q15), so only I term should contribute */
	mlmw_pi_t pi = { .kp_q15 = 0, .ki_q15 = 32768, .out_min = 0, .out_max = 1000, .integ = 0 };
	int32_t result1 = mlmw_pi_step(&pi, 100);
	int32_t result2 = mlmw_pi_step(&pi, 100);
	EXPECT_EQ(result1, 100); /* 0 + 100 * 32768/32768 = 100 */
	EXPECT_EQ(result2, 200); /* 100 + 100 * 32768/32768 = 200 */
}

ZTEST(SUITE, test_pi_step_saturation)
{
	mlmw_pi_t pi = { .kp_q15 = 16384, .ki_q15 = 16384, .out_min = 0, .out_max = 100, .integ = 0 };
	EXPECT_EQ(mlmw_pi_step(&pi, 100), 100); /* 100 + 100 * 16384/32768 = 150 -> clamped to 100 */
}

ZTEST(SUITE, test_fault_init)
{
	mlmw_fault_t f;
	mlmw_fault_init(&f, 1000, 3, 1000, 0);
	EXPECT_EQ(f.oc_limit_ma, 1000);
	EXPECT_EQ(f.oc_debounce, 3);
	EXPECT_EQ(f.stall_timeout_ms, 1000);
	EXPECT_EQ(f.oc_count, 0);
	EXPECT_EQ(f.last_edge_ms, 0);
	EXPECT_EQ(f.code, MLMW_FAULT_NONE);
}

ZTEST(SUITE, test_fault_update_no_fault)
{
	mlmw_fault_t f;
	mlmw_fault_init(&f, 1000, 3, 1000, 0);
	EXPECT_EQ(mlmw_fault_update(&f, 500, 100, true, true), MLMW_FAULT_NONE);
	EXPECT_EQ(f.oc_count, 0);
	EXPECT_EQ(f.code, MLMW_FAULT_NONE);
}



ZTEST(SUITE, test_fault_update_stall)
{
	mlmw_fault_t f;
	mlmw_fault_init(&f, 1000, 3, 1000, 0);
	EXPECT_EQ(mlmw_fault_update(&f, 500, 2000, false, true), MLMW_FAULT_STALL);
	EXPECT_EQ(f.code, MLMW_FAULT_STALL);
}

ZTEST(SUITE, test_fault_update_latched)
{
	mlmw_fault_t f;
	mlmw_fault_init(&f, 1000, 3, 1000, 0);
	f.code = MLMW_FAULT_OVERCURRENT;
	EXPECT_EQ(mlmw_fault_update(&f, 1500, 100, true, true), MLMW_FAULT_OVERCURRENT);
}

ZTEST(SUITE, test_fault_clear)
{
	mlmw_fault_t f;
	mlmw_fault_init(&f, 1000, 3, 1000, 0);
	f.code = MLMW_FAULT_OVERCURRENT;
	f.oc_count = 5;
	f.last_edge_ms = 100;
	mlmw_fault_clear(&f, 200);
	EXPECT_EQ(f.code, MLMW_FAULT_NONE);
	EXPECT_EQ(f.oc_count, 0);
	EXPECT_EQ(f.last_edge_ms, 200);
}

ZTEST(SUITE, test_hall_to_sector_boundary)
{
	/* Test boundary conditions for hall codes */
	EXPECT_EQ(mlmw_hall_to_sector(0), -1); /* Invalid code */
	EXPECT_EQ(mlmw_hall_to_sector(7), -1); /* Invalid code */
	EXPECT_EQ(mlmw_hall_to_sector(1), 0); /* Valid code */
	EXPECT_EQ(mlmw_hall_to_sector(2), 2); /* Valid code */
	EXPECT_EQ(mlmw_hall_to_sector(3), 1); /* Valid code */
	EXPECT_EQ(mlmw_hall_to_sector(4), 4); /* Valid code */
	EXPECT_EQ(mlmw_hall_to_sector(5), 5); /* Valid code */
	EXPECT_EQ(mlmw_hall_to_sector(6), 3); /* Valid code */
}

ZTEST(SUITE, test_commutation_boundary)
{
	/* Test boundary conditions for sector values */
	EXPECT_EQ(mlmw_commutation(-1, true), 0); /* Invalid sector */
	EXPECT_EQ(mlmw_commutation(6, true), 0); /* Invalid sector */
	EXPECT_EQ(mlmw_commutation(0, true), MLMW_AH | MLMW_BL); /* Valid sector */
	EXPECT_EQ(mlmw_commutation(5, true), MLMW_CH | MLMW_BL); /* Valid sector */
	EXPECT_EQ(mlmw_commutation(-1, false), 0); /* Invalid sector */
	EXPECT_EQ(mlmw_commutation(6, false), 0); /* Invalid sector */
	EXPECT_EQ(mlmw_commutation(0, false), MLMW_BH | MLMW_AL); /* Valid sector */
	EXPECT_EQ(mlmw_commutation(5, false), MLMW_BH | MLMW_CL); /* Valid sector */
}

ZTEST(SUITE, test_ramp_boundary)
{
	/* Test boundary conditions for ramp function */
	EXPECT_EQ(mlmw_ramp(0, 100, 50), 50); /* Move towards target */
	EXPECT_EQ(mlmw_ramp(100, 0, 50), 50); /* Move towards target */
	EXPECT_EQ(mlmw_ramp(100, 100, 50), 100); /* Already at target */
	EXPECT_EQ(mlmw_ramp(100, 200, 1000), 200); /* Can reach target in one step */
}

ZTEST(SUITE, test_pi_step_boundary)
{
	/* Test boundary conditions for PI controller */
	mlmw_pi_t pi = { .kp_q15 = 32768, .ki_q15 = 0, .out_min = -100, .out_max = 100, .integ = 0 };
	EXPECT_EQ(mlmw_pi_step(&pi, 50), 50); /* Normal case */
	EXPECT_EQ(mlmw_pi_step(&pi, -50), -50); /* Negative error */
	/* Test with large error that should saturate */
	mlmw_pi_t pi2 = { .kp_q15 = 32768, .ki_q15 = 0, .out_min = 0, .out_max = 100, .integ = 0 };
	EXPECT_EQ(mlmw_pi_step(&pi2, 200), 100); /* Saturates at max */
}

ZTEST_SUITE(SUITE, NULL, NULL, NULL, NULL, NULL);
