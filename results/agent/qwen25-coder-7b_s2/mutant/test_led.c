#include "mlmw_test.h"
#include "mlmw/led.h"
#include <string.h>





ZTEST(agent_led, test_mlmw_derate_q15_below_start)
{
	uint16_t expected = MLMW_Q15_ONE;
	uint16_t actual = mlmw_derate_q15(0, 100, 200, 10000);

	// Check values with EXPECT_EQ(actual, expected)
	EXPECT_EQ(actual, expected);
}

ZTEST(agent_led, test_mlmw_derate_q15_above_max)
{
	uint16_t expected = 10000;
	uint16_t actual = mlmw_derate_q15(300, 100, 200, 10000);

	// Check values with EXPECT_EQ(actual, expected)
	EXPECT_EQ(actual, expected);
}



ZTEST(agent_led, test_mlmw_derate_q15_edge)
{
	uint16_t expected = MLMW_Q15_ONE;
	uint16_t actual = mlmw_derate_q15(100, 100, 200, 10000);

	// Check values with EXPECT_EQ(actual, expected)
	EXPECT_EQ(actual, expected);
}







ZTEST_SUITE(agent_led, NULL, NULL, NULL, NULL, NULL);
