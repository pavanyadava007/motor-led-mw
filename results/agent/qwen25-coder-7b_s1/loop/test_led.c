#include "mlmw_test.h"
#include "mlmw/led.h"
#include <string.h>



ZTEST(agent_led, test_mlmw_gamma_edge)
{
	EXPECT_EQ(mlmw_gamma(0), 0);
	EXPECT_EQ(mlmw_gamma(255), 65535);
}

ZTEST(agent_led, test_mlmw_derate_q15_below_start)
{
	EXPECT_EQ(mlmw_derate_q15(-100, 0, 100, 10000), MLMW_Q15_ONE);
}

ZTEST(agent_led, test_mlmw_derate_q15_above_max)
{
	EXPECT_EQ(mlmw_derate_q15(200, 0, 100, 10000), 10000);
}











ZTEST_SUITE(agent_led, NULL, NULL, NULL, NULL, NULL);
