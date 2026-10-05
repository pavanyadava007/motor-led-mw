#include "mlmw/led.h"

uint16_t mlmw_gamma(uint8_t v)
{
	uint32_t sq = (uint32_t)v * v;

	return (uint16_t)((sq * 65535u + 32512u) / 65025u);
}

uint16_t mlmw_derate_q15(int16_t temp_c, int16_t t_start, int16_t t_max, uint16_t min_q15)
{
	uint32_t lo = min_q15 > MLMW_Q15_ONE ? MLMW_Q15_ONE : min_q15;

	if (temp_c <= t_start) {
		return MLMW_Q15_ONE;
	}
	if (t_max <= t_start || temp_c >= t_max) {
		return (uint16_t)lo;
	}
	uint32_t span = (uint32_t)(t_max - t_start);
	uint32_t pos = (uint32_t)(temp_c - t_start);

	return (uint16_t)(MLMW_Q15_ONE - ((MLMW_Q15_ONE - lo) * pos) / span);
}

void mlmw_led_mix(const mlmw_led_cal_t *cal, const uint8_t rgb[3], uint8_t brightness,
		  int16_t temp_c, uint16_t duty[3])
{
	uint32_t derate = mlmw_derate_q15(temp_c, cal->t_start, cal->t_max, cal->min_q15);

	for (int c = 0; c < 3; c++) {
		uint32_t x = mlmw_gamma(rgb[c]);

		x = (x * brightness) / 255u;
		x = (x * cal->gain_q15[c]) >> 15;
		x = (x * derate) >> 15;
		duty[c] = (uint16_t)(x > 0xFFFFu ? 0xFFFFu : x);
	}
}
