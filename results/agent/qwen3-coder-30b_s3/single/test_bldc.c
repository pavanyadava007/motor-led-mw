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



ZTEST(SUITE, test_pi_reset)
{
	mlmw_pi_t pi = { .kp_q15 = 16384, .ki_q15 = 8192, .out_min = 0, .out_max = 100, .integ = 50 };
	mlmw_pi_reset(&pi);
	EXPECT_EQ(pi.integ, 0);
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

ZTEST_SUITE(SUITE, NULL, NULL, NULL, NULL, NULL);
