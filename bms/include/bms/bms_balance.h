/* Passive balancing decision (bleed resistors), threshold with hysteresis.
 *
 * Allowed only when `allowed` is true (the caller passes: no latched fault and the
 * pack current is within -max_dis_ma .. +inf, i.e. charging, resting or light load).
 * When not allowed, every bleed switch is off.
 * For cell k with v = cell_mv[k] and vmin = minimum cell voltage:
 *   switch k turns ON  when v - vmin >= start_mv and v >= min_cell_mv
 *   switch k stays ON  while v - vmin >  stop_mv  and v >= min_cell_mv
 *   otherwise OFF.    (start_mv > stop_mv gives the hysteresis band)
 * At most max_on switches are on at the same time (thermal limit of the board);
 * when more cells qualify, the ones that are already on keep priority, then the
 * lowest index.
 */
#ifndef BMS_BALANCE_H
#define BMS_BALANCE_H

#include "bms/bms_types.h"

typedef struct {
	uint16_t start_mv;
	uint16_t stop_mv;
	uint16_t min_cell_mv;
	int32_t max_dis_ma;
	uint8_t max_on;
} bms_bal_cfg_t;

/* Returns the new bleed mask (bit k = cell k). prev_mask is the mask of the last call. */
uint16_t bms_balance(uint16_t prev_mask, const bms_bal_cfg_t *cfg, const uint16_t *cell_mv,
		     uint8_t n_cells, int32_t current_ma, bool no_fault);

#endif
