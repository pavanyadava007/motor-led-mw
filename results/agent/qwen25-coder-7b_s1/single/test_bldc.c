#include "mlmw_test.h"
#include "mlmw/bldc.h"
#include <string.h>

ZTEST(agent_bldc, test_hall_to_sector)
{
    EXPECT_EQ(mlmw_hall_to_sector(0), -1);
    EXPECT_EQ(mlmw_hall_to_sector(1), 0);
    EXPECT_EQ(mlmw_hall_to_sector(2), 2);
    EXPECT_EQ(mlmw_hall_to_sector(3), 1);
    EXPECT_EQ(mlmw_hall_to_sector(4), 4);
    EXPECT_EQ(mlmw_hall_to_sector(5), 5);
    EXPECT_EQ(mlmw_hall_to_sector(6), 3);
    EXPECT_EQ(mlmw_hall_to_sector(7), -1);
}

ZTEST(agent_bldc, test_commutation_forward)
{
    EXPECT_EQ(mlmw_commutation(0, true), MLMW_AH | MLMW_BL);
    EXPECT_EQ(mlmw_commutation(1, true), MLMW_AH | MLMW_CL);
    EXPECT_EQ(mlmw_commutation(2, true), MLMW_BH | MLMW_CL);
    EXPECT_EQ(mlmw_commutation(3, true), MLMW_BH | MLMW_AL);
    EXPECT_EQ(mlmw_commutation(4, true), MLMW_CH | MLMW_AL);
    EXPECT_EQ(mlmw_commutation(5, true), MLMW_CH | MLMW_BL);
    EXPECT_EQ(mlmw_commutation(6, true), 0);
    EXPECT_EQ(mlmw_commutation(7, true), 0);
}

ZTEST(agent_bldc, test_commutation_reverse)
{
    EXPECT_EQ(mlmw_commutation(0, false), MLMW_BH | MLMW_AL);
    EXPECT_EQ(mlmw_commutation(1, false), MLMW_CH | MLMW_AL);
    EXPECT_EQ(mlmw_commutation(2, false), MLMW_CH | MLMW_BL);
    EXPECT_EQ(mlmw_commutation(3, false), MLMW_AH | MLMW_BL);
    EXPECT_EQ(mlmw_commutation(4, false), MLMW_AH | MLMW_CL);
    EXPECT_EQ(mlmw_commutation(5, false), MLMW_BH | MLMW_CL);
    EXPECT_EQ(mlmw_commutation(6, false), 0);
    EXPECT_EQ(mlmw_commutation(7, false), 0);
}



ZTEST(agent_bldc, test_ramp)
{
    EXPECT_EQ(mlmw_ramp(0, 10, 5), 5);
    EXPECT_EQ(mlmw_ramp(0, 10, 0), 0);
    EXPECT_EQ(mlmw_ramp(10, 0, 5), 5);
    EXPECT_EQ(mlmw_ramp(5, 10, 5), 10);
    EXPECT_EQ(mlmw_ramp(10, 5, 5), 5);
}

ZTEST(agent_bldc, test_pi_reset)
{
    mlmw_pi_t pi = {0};
    mlmw_pi_reset(&pi);
    EXPECT_EQ(pi.integ, 0);
}



ZTEST(agent_bldc, test_fault_init)
{
    mlmw_fault_t f = {0};
    mlmw_fault_init(&f, 1000, 3, 1000, 0);
    EXPECT_EQ(f.oc_limit_ma, 1000);
    EXPECT_EQ(f.oc_debounce, 3);
    EXPECT_EQ(f.stall_timeout_ms, 1000);
    EXPECT_EQ(f.oc_count, 0);
    EXPECT_EQ(f.last_edge_ms, 0);
    EXPECT_EQ(f.code, MLMW_FAULT_NONE);
}



ZTEST_SUITE(agent_bldc, NULL, NULL, NULL, NULL, NULL);
