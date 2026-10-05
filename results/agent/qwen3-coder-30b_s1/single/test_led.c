#include "mlmw_test.h"
#include "mlmw/led.h"
#include <string.h>

#define SUITE agent_led

ZTEST(SUITE, test_gamma_min)
{
	uint16_t result = mlmw_gamma(0);
	EXPECT_EQ(result, 0);
}

ZTEST(SUITE, test_gamma_max)
{
	uint16_t result = mlmw_gamma(255);
	EXPECT_EQ(result, 65535);
}







ZTEST(SUITE, test_derate_q15_at_start)
{
	uint16_t result = mlmw_derate_q15(0, 0, 50, 16384);
	EXPECT_EQ(result, 32767);
}

ZTEST(SUITE, test_derate_q15_above_max)
{
	uint16_t result = mlmw_derate_q15(60, 0, 50, 16384);
	EXPECT_EQ(result, 16384);
}

ZTEST(SUITE, test_derate_q15_below_start)
{
	uint16_t result = mlmw_derate_q15(-10, 0, 50, 16384);
	EXPECT_EQ(result, 32767);
}

ZTEST(SUITE, test_derate_q15_linear)
{
	uint16_t result = mlmw_derate_q15(25, 0, 50, 16384);
	/* (32767 - 16384) * 25 / 50 + 16384 = 16384 + 16384/2 = 24576 */
	EXPECT_EQ(result, 24576);
}



ZTEST(SUITE, test_derate_q15_min_greater_than_one)
{
	uint16_t result = mlmw_derate_q15(20, 0, 50, 40000);
	EXPECT_EQ(result, 32767);
}



ZTEST(SUITE, test_led_mix_zero_brightness)
{
	mlmw_led_cal_t cal = {
		.gain_q15 = {32767, 32767, 32767},
		.t_start = 0,
		.t_max = 50,
		.min_q15 = 16384
	};
	uint8_t rgb[3] = {255, 255, 255};
	uint8_t brightness = 0;
	int16_t temp_c = 0;
	uint16_t duty[3];

	mlmw_led_mix(&cal, rgb, brightness, temp_c, duty);
	EXPECT_EQ(duty[0], 0);
	EXPECT_EQ(duty[1], 0);
	EXPECT_EQ(duty[2], 0);
}









ZTEST_SUITE(SUITE, NULL, NULL, NULL, NULL, NULL);
