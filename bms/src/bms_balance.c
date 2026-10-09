/* Passive balancing decision, see bms/bms_balance.h. */
#include "bms/bms_balance.h"

uint16_t bms_balance(uint16_t prev_mask, const bms_bal_cfg_t *cfg, const uint16_t *cell_mv,
		     uint8_t n_cells, int32_t current_ma, bool no_fault)
{
	uint16_t out = 0u;
	uint8_t n = (n_cells <= BMS_MAX_CELLS) ? n_cells : (uint8_t)BMS_MAX_CELLS;
	bool allowed = no_fault && (current_ma >= -cfg->max_dis_ma);

	if (allowed && (n > 0u)) {
		uint16_t want = 0u;
		uint16_t vmin = 0xFFFFu;
		uint8_t on = 0u;
		uint8_t k;

		for (k = 0u; k < n; k++) {
			if (cell_mv[k] < vmin) {
				vmin = cell_mv[k];
			}
		}
		for (k = 0u; k < n; k++) {
			uint16_t diff = (uint16_t)(cell_mv[k] - vmin);
			bool was_on = ((prev_mask >> k) & 1u) != 0u;
			bool high_enough = cell_mv[k] >= cfg->min_cell_mv;

			if (high_enough && (was_on ? (diff > cfg->stop_mv) : (diff >= cfg->start_mv))) {
				want |= (uint16_t)(1u << k);
			}
		}
		/* cells that were already on keep priority, then lowest index */
		for (k = 0u; k < n; k++) {
			uint16_t bit = (uint16_t)(1u << k);

			if (((want & prev_mask & bit) != 0u) && (on < cfg->max_on)) {
				out |= bit;
				on++;
			}
		}
		for (k = 0u; k < n; k++) {
			uint16_t bit = (uint16_t)(1u << k);

			if (((want & bit) != 0u) && ((out & bit) == 0u) && (on < cfg->max_on)) {
				out |= bit;
				on++;
			}
		}
	}
	return out;
}
