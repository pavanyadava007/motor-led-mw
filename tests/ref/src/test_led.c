#include <zephyr/ztest.h>
#include "mlmw/led.h"

ZTEST(ref_led, test_gamma_ends)
{
	zassert_equal(mlmw_gamma(0), 0);
	zassert_equal(mlmw_gamma(255), 65535);
	zassert_equal(mlmw_gamma(128), 16513); /* 65535*(128/255)^2 = 16512.99 */
}

ZTEST(ref_led, test_gamma_monotonic)
{
	for (int v = 1; v < 256; v++) {
		zassert_true(mlmw_gamma((uint8_t)v) > mlmw_gamma((uint8_t)(v - 1)));
	}
}

ZTEST(ref_led, test_derate)
{
	zassert_equal(mlmw_derate_q15(25, 85, 125, 8192), MLMW_Q15_ONE);
	zassert_equal(mlmw_derate_q15(125, 85, 125, 8192), 8192);
	zassert_equal(mlmw_derate_q15(150, 85, 125, 8192), 8192);
	uint16_t mid = mlmw_derate_q15(105, 85, 125, 8192);

	zassert_true(mid > 20470 && mid < 20490, "mid=%u", mid);
}

ZTEST(ref_led, test_mix_full_white)
{
	mlmw_led_cal_t cal = {{32767, 32767, 32767}, 85, 125, 8192};
	uint8_t rgb[3] = {255, 255, 255};
	uint16_t d[3];

	mlmw_led_mix(&cal, rgb, 255, 25, d);
	for (int c = 0; c < 3; c++) {
		zassert_true(d[c] >= 65530, "d=%u", d[c]);
	}
}

ZTEST(ref_led, test_mix_brightness_and_off)
{
	mlmw_led_cal_t cal = {{32767, 32767, 32767}, 85, 125, 8192};
	uint8_t rgb[3] = {255, 0, 255};
	uint16_t d[3];

	mlmw_led_mix(&cal, rgb, 0, 25, d);
	zassert_equal(d[0], 0);
	mlmw_led_mix(&cal, rgb, 128, 25, d);
	zassert_equal(d[1], 0);
	zassert_true(d[0] > 32000 && d[0] < 33000, "d0=%u", d[0]);
}

ZTEST(ref_led, test_mix_hot_is_dimmer)
{
	mlmw_led_cal_t cal = {{32767, 32767, 32767}, 85, 125, 8192};
	uint8_t rgb[3] = {200, 200, 200};
	uint16_t cool[3], hot[3];

	mlmw_led_mix(&cal, rgb, 255, 25, cool);
	mlmw_led_mix(&cal, rgb, 255, 130, hot);
	zassert_true(hot[0] < cool[0] / 3);
}

ZTEST_SUITE(ref_led, NULL, NULL, NULL, NULL, NULL);
