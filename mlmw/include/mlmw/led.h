/* RGB LED colour mixing with gamma, per-channel calibration and thermal derating. */
#ifndef MLMW_LED_H
#define MLMW_LED_H

#include <stdint.h>

#define MLMW_Q15_ONE 32767u

/* Gamma 2.0: round(65535 * (v / 255)^2). 0 -> 0, 255 -> 65535. */
uint16_t mlmw_gamma(uint8_t v);

/* Derating factor in Q15. temp <= t_start: 32767. temp >= t_max: min_q15.
 * In between: linear from 32767 down to min_q15. If t_max <= t_start the
 * step happens at t_start (temp > t_start gives min_q15). */
uint16_t mlmw_derate_q15(int16_t temp_c, int16_t t_start, int16_t t_max, uint16_t min_q15);

typedef struct {
	uint16_t gain_q15[3]; /* per-channel calibration (R, G, B), Q15 */
	int16_t t_start;
	int16_t t_max;
	uint16_t min_q15;
} mlmw_led_cal_t;

/* duty[c] = gamma(rgb[c]) * brightness / 255 * gain[c] * derate, all in 16-bit PWM units,
 * rounded down at each step, never above 65535. */
void mlmw_led_mix(const mlmw_led_cal_t *cal, const uint8_t rgb[3], uint8_t brightness,
		  int16_t temp_c, uint16_t duty[3]);

#endif
