/* Simplified BLDC motor + LED thermal model, so the node runs without hardware. */
#ifndef PLANT_H
#define PLANT_H

#include <stdint.h>

#define PLANT_POLE_PAIRS 2u

void plant_init(void);
/* Advance by dt_us with the gate mask and duty (0..1000 per mille) applied. */
void plant_step(uint8_t gate_mask, uint16_t duty_pm, uint32_t dt_us);
uint8_t plant_hall(void);
int32_t plant_rpm_true(void);
uint16_t plant_current_ma(void);
/* LED board temperature, driven by the sum of the three PWM duties. */
void plant_led_step(const uint16_t duty[3], uint32_t dt_us);
int16_t plant_led_temp_c(void);
/* Test hook: add a load torque (simulates a blocked rotor when large). */
void plant_set_load(float load);

#endif
