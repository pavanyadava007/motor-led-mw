/* BLDC block commutation (six-step) building blocks, fixed point. */
#ifndef MLMW_BLDC_H
#define MLMW_BLDC_H

#include <stdbool.h>
#include <stdint.h>

/* Gate mask bits returned by mlmw_commutation(). */
#define MLMW_AH 0x20u
#define MLMW_AL 0x10u
#define MLMW_BH 0x08u
#define MLMW_BL 0x04u
#define MLMW_CH 0x02u
#define MLMW_CL 0x01u

/* Hall code (bit2 = A, bit1 = B, bit0 = C) to sector 0..5.
 * Forward order of codes: 1, 3, 2, 6, 4, 5 -> sectors 0..5. Codes 0 and 7 are invalid: -1. */
int8_t mlmw_hall_to_sector(uint8_t hall);

/* Gate mask for a sector. Forward: sector 0 drives A high / B low, then
 * A->C, B->C, B->A, C->A, C->B. Reverse swaps the high and low side of each pair.
 * Invalid sector (outside 0..5): 0 (all gates off). */
uint8_t mlmw_commutation(int8_t sector, bool forward);

/* Mechanical speed in rpm from the time between two hall edges.
 * Six edges per electrical revolution: rpm = 60e6 / (period_us * 6 * pole_pairs).
 * period_us == 0 or pole_pairs == 0 returns 0. */
uint32_t mlmw_speed_rpm(uint32_t hall_period_us, uint8_t pole_pairs);

/* Move 'current' toward 'target' by at most max_step (max_step <= 0 means no change). */
int32_t mlmw_ramp(int32_t current, int32_t target, int32_t max_step);

/* PI controller, gains in Q15. Output is clamped to [out_min, out_max].
 * Anti-windup: the integrator is clamped to the same limits and is not
 * advanced while the output is saturated in the direction of the error. */
typedef struct {
	int32_t kp_q15;
	int32_t ki_q15;
	int32_t out_min;
	int32_t out_max;
	int32_t integ;
} mlmw_pi_t;

void mlmw_pi_reset(mlmw_pi_t *pi);
int32_t mlmw_pi_step(mlmw_pi_t *pi, int32_t err);

/* Fault monitor: over-current with debounce, stall detection. Faults latch. */
enum { MLMW_FAULT_NONE = 0, MLMW_FAULT_OVERCURRENT = 1, MLMW_FAULT_STALL = 2 };

typedef struct {
	uint16_t oc_limit_ma;      /* current strictly above this counts as over-current */
	uint8_t oc_debounce;       /* consecutive samples needed to latch */
	uint32_t stall_timeout_ms; /* running without a hall edge for longer than this */
	uint8_t oc_count;
	uint32_t last_edge_ms;
	uint8_t code;              /* latched fault code */
} mlmw_fault_t;

void mlmw_fault_init(mlmw_fault_t *f, uint16_t oc_limit_ma, uint8_t oc_debounce,
		     uint32_t stall_timeout_ms, uint32_t now_ms);
/* Returns the latched fault code after this sample. */
uint8_t mlmw_fault_update(mlmw_fault_t *f, uint16_t current_ma, uint32_t now_ms,
			  bool hall_edge, bool running);
void mlmw_fault_clear(mlmw_fault_t *f, uint32_t now_ms);

#endif
