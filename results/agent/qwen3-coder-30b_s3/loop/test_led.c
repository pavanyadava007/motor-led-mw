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

ZTEST(SUITE, test_derate_q15_normal)
{
	uint16_t result = mlmw_derate_q15(20, 0, 50, 16384);
	EXPECT_EQ(result, 26214);
}

ZTEST(SUITE, test_led_mix_full_brightness)
{
	mlmw_led_cal_t cal = {
		.gain_q15 = {32767, 32767, 32767},
		.t_start = 0,
		.t_max = 50,
		.min_q15 = 16384
	};
	uint8_t rgb[3] = {255, 255, 255};
	uint8_t brightness = 255;
	int16_t temp_c = 0;
	uint16_t duty[3];

	mlmw_led_mix(&cal, rgb, brightness, temp_c, duty);
	EXPECT_EQ(duty[0], 65531);
	EXPECT_EQ(duty[1], 65531);
	EXPECT_EQ(duty[2], 65531);
}

ZTEST(SUITE, test_led_mix_max_rgb)
{
	mlmw_led_cal_t cal = {
		.gain_q15 = {32767, 32767, 32767},
		.t_start = 0,
		.t_max = 50,
		.min_q15 = 32767
	};
	uint8_t rgb[3] = {255, 255, 255};
	uint8_t brightness = 255;
	int16_t temp_c = 0;
	uint16_t duty[3];

	mlmw_led_mix(&cal, rgb, brightness, temp_c, duty);
	EXPECT_EQ(duty[0], 65531);
	EXPECT_EQ(duty[1], 65531);
	EXPECT_EQ(duty[2], 65531);
}

ZTEST(SUITE, test_led_mix_with_derating)
{
	mlmw_led_cal_t cal = {
		.gain_q15 = {32767, 32767, 32767},
		.t_start = 0,
		.t_max = 50,
		.min_q15 = 16384
	};
	uint8_t rgb[3] = {255, 255, 255};
	uint8_t brightness = 255;
	int16_t temp_c = 25;
	uint16_t duty[3];

	mlmw_led_mix(&cal, rgb, brightness, temp_c, duty);
	EXPECT_EQ(duty[0], 49149);
	EXPECT_EQ(duty[1], 49149);
	EXPECT_EQ(duty[2], 49149);
}

ZTEST(SUITE, test_led_mix_with_gain)
{
	mlmw_led_cal_t cal = {
		.gain_q15 = {16384, 16384, 16384},
		.t_start = 0,
		.t_max = 50,
		.min_q15 = 32767
	};
	uint8_t rgb[3] = {255, 255, 255};
	uint8_t brightness = 255;
	int16_t temp_c = 0;
	uint16_t duty[3];

	mlmw_led_mix(&cal, rgb, brightness, temp_c, duty);
	EXPECT_EQ(duty[0], 32766);
	EXPECT_EQ(duty[1], 32766);
	EXPECT_EQ(duty[2], 32766);
}

ZTEST(SUITE, test_led_mix_with_gamma)
{
	mlmw_led_cal_t cal = {
		.gain_q15 = {32767, 32767, 32767},
		.t_start = 0,
		.t_max = 50,
		.min_q15 = 32767
	};
	uint8_t rgb[3] = {128, 128, 128};
	uint8_t brightness = 255;
	int16_t temp_c = 0;
	uint16_t duty[3];

	mlmw_led_mix(&cal, rgb, brightness, temp_c, duty);
	EXPECT_EQ(duty[0], 16511);
	EXPECT_EQ(duty[1], 16511);
	EXPECT_EQ(duty[2], 16511);
}

ZTEST(SUITE, test_gamma_boundary_values)
{
	EXPECT_EQ(mlmw_gamma(1), 1);
}

ZTEST(SUITE, test_gamma_mid)
{
	uint16_t result = mlmw_gamma(128);
	EXPECT_EQ(result, 16513);
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

ZTEST(SUITE, test_derate_q15_at_max)
{
	uint16_t result = mlmw_derate_q15(50, 0, 50, 16384);
	EXPECT_EQ(result, 16384);
}

ZTEST(SUITE, test_derate_q15_below_start)
{
	uint16_t result = mlmw_derate_q15(-10, 0, 50, 16384);
	EXPECT_EQ(result, 32767);
}

ZTEST(SUITE, test_derate_q15_zero_span)
{
	uint16_t result = mlmw_derate_q15(10, 20, 20, 16384);
	EXPECT_EQ(result, 32767);
}

ZTEST(SUITE, test_derate_q15_min_greater_than_one)
{
	uint16_t result = mlmw_derate_q15(30, 0, 50, 40000);
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

ZTEST(SUITE, test_led_mix_zero_rgb)
{
	mlmw_led_cal_t cal = {
		.gain_q15 = {32767, 32767, 32767},
		.t_start = 0,
		.t_max = 50,
		.min_q15 = 32767
	};
	uint8_t rgb[3] = {0, 0, 0};
	uint8_t brightness = 255;
	int16_t temp_c = 0;
	uint16_t duty[3];

	mlmw_led_mix(&cal, rgb, brightness, temp_c, duty);
	EXPECT_EQ(duty[0], 0);
	EXPECT_EQ(duty[1], 0);
	EXPECT_EQ(duty[2], 0);
}

ZTEST_SUITE(SUITE, NULL, NULL, NULL, NULL, NULL);
