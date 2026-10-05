#include "plant.h"

#include "mlmw/bldc.h"

/* Torque is produced only when the applied gate pair matches the rotor sector:
 * the forward pair for the sector drives forward, the reverse pair drives backward,
 * any other pattern gives no torque. Enough to show whether commutation is right. */
static float theta_e;  /* electrical angle, degrees [0, 360) */
static float omega_e;  /* electrical speed, degrees per second */
static float load;
static float current_a;
static float led_temp = 25.0f;

/* tiny float helpers: the minimal libc used with TF-M has no libm */
static float fabsf_(float x)
{
	return x < 0.0f ? -x : x;
}

static float signf_(float x)
{
	return x < 0.0f ? -1.0f : 1.0f;
}

static const uint8_t hall_of_sector[6] = {1, 3, 2, 6, 4, 5};

#define K_TORQUE 9.0e5f /* deg/s^2 at full duty */
#define DAMPING 18.75f  /* 1/s: full duty settles near 4000 rpm */

void plant_init(void)
{
	theta_e = 10.0f;
	omega_e = 0.0f;
	load = 0.0f;
	current_a = 0.0f;
}

void plant_set_load(float l)
{
	load = l;
}

static int sector(void)
{
	return (int)(theta_e / 60.0f) % 6;
}

void plant_step(uint8_t gate_mask, uint16_t duty_pm, uint32_t dt_us)
{
	float dt = (float)dt_us * 1e-6f;
	float duty = (float)duty_pm / 1000.0f;
	int s = sector();
	float drive = 0.0f;

	if (gate_mask != 0u && gate_mask == mlmw_commutation((int8_t)s, true)) {
		drive = 1.0f;
	} else if (gate_mask != 0u && gate_mask == mlmw_commutation((int8_t)s, false)) {
		drive = -1.0f;
	}
	float acc = K_TORQUE * duty * drive - DAMPING * omega_e;

	if (fabsf_(omega_e) < 1.0f && fabsf_(acc) <= load) {
		acc = 0.0f;
		omega_e = 0.0f;
	} else {
		acc -= load * signf_(omega_e != 0.0f ? omega_e : acc);
	}
	omega_e += acc * dt;
	theta_e += omega_e * dt;
	while (theta_e >= 360.0f) {
		theta_e -= 360.0f;
	}
	while (theta_e < 0.0f) {
		theta_e += 360.0f;
	}
	/* phase current: duty times back-EMF headroom, 20 A at stall and full duty */
	float emf = fabsf_(omega_e) / 60000.0f;

	current_a = 20.0f * duty * (drive != 0.0f ? (1.0f - emf > 0.05f ? 1.0f - emf : 0.05f) : 0.0f);
}

uint8_t plant_hall(void)
{
	return hall_of_sector[sector()];
}

int32_t plant_rpm_true(void)
{
	return (int32_t)(omega_e / 360.0f * 60.0f / (float)PLANT_POLE_PAIRS);
}

uint16_t plant_current_ma(void)
{
	return (uint16_t)(current_a * 1000.0f);
}

void plant_led_step(const uint16_t duty[3], uint32_t dt_us)
{
	float p = ((float)duty[0] + duty[1] + duty[2]) / (3.0f * 65535.0f);
	float t_inf = 25.0f + 110.0f * p;
	float a = (float)dt_us * 1e-6f / 2.0f; /* 2 s thermal time constant */

	led_temp += (t_inf - led_temp) * (a > 1.0f ? 1.0f : a);
}

int16_t plant_led_temp_c(void)
{
	return (int16_t)led_temp;
}
