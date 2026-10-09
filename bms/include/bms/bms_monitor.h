/* Cell and pack monitoring with debounce and latched faults.
 *
 * Called once per BMS cycle with one sample of every cell voltage, every temperature
 * sensor and the pack current.
 *
 * Per sample and channel:
 *  - Plausibility first. A cell voltage < wire_min_mv or > wire_max_mv is an open wire
 *    or broken sense line (BMS_F_OPEN_WIRE); that cell is then NOT used for OV/UV, so a
 *    broken wire is reported as a sensor fault, not as a cell fault. A temperature
 *    < temp_min_dc or > temp_max_dc is a sensor fault (BMS_F_TEMP_SENSOR) and is not
 *    used for OT/UT.
 *  - Limits are strict: OV when v > ov_mv, UV when v < uv_mv, OT when t > ot_dc,
 *    UT when t < ut_dc, discharge OC when -i > oc_dis_ma, charge OC when i > oc_chg_ma.
 *  - Debounce: each condition has a consecutive-sample counter per channel. The fault
 *    latches when the counter reaches cfg->debounce (e.g. 3 = the 3rd consecutive
 *    sample). One good sample resets the counter to 0. Counters saturate at 255.
 *  - Stuck sensor: a cell voltage that keeps exactly the same raw value for
 *    stuck_samples consecutive samples while |i| >= stuck_min_ma latches
 *    BMS_F_SENSOR_STUCK (a real ADC reading under load always moves by a few LSB).
 *
 * Latched faults stay set until bms_mon_clear() is called AND no condition is active
 * (all debounce counters at 0); otherwise bms_mon_clear() leaves them set.
 */
#ifndef BMS_MONITOR_H
#define BMS_MONITOR_H

#include "bms/bms_types.h"

typedef struct {
	uint8_t n_cells;      /* 1..BMS_MAX_CELLS */
	uint8_t n_temps;      /* 1..BMS_MAX_TEMPS */
	uint16_t ov_mv;
	uint16_t uv_mv;
	int16_t ot_dc;
	int16_t ut_dc;
	int32_t oc_dis_ma;    /* magnitude, > 0 */
	int32_t oc_chg_ma;    /* magnitude, > 0 */
	uint8_t debounce;     /* consecutive samples to latch, >= 1 */
	uint16_t wire_min_mv;
	uint16_t wire_max_mv;
	int16_t temp_min_dc;
	int16_t temp_max_dc;
	uint8_t stuck_samples;
	int32_t stuck_min_ma;
} bms_mon_cfg_t;

typedef struct {
	uint8_t ov[BMS_MAX_CELLS];
	uint8_t uv[BMS_MAX_CELLS];
	uint8_t wire[BMS_MAX_CELLS];
	uint8_t stuck[BMS_MAX_CELLS];
	uint16_t last_mv[BMS_MAX_CELLS];
	uint8_t ot[BMS_MAX_TEMPS];
	uint8_t ut[BMS_MAX_TEMPS];
	uint8_t tsens[BMS_MAX_TEMPS];
	uint8_t oc_dis;
	uint8_t oc_chg;
	bool have_last;
	bms_faults_t latched;
	bms_faults_t active;  /* conditions present in the last sample (before debounce) */
	uint16_t v_min_mv;    /* over plausible cells of the last sample (0 if none) */
	uint16_t v_max_mv;
	int16_t t_min_dc;     /* over plausible sensors of the last sample (0 if none) */
	int16_t t_max_dc;
} bms_mon_t;

void bms_mon_init(bms_mon_t *m);

/* Process one sample. Returns the latched fault mask after this sample. */
bms_faults_t bms_mon_step(bms_mon_t *m, const bms_mon_cfg_t *cfg, const uint16_t *cell_mv,
			  const int16_t *temp_dc, int32_t current_ma);

/* Request a fault clear. Clears the latched mask only if no condition is active.
 * Returns true when the faults were cleared. */
bool bms_mon_clear(bms_mon_t *m);

#endif
