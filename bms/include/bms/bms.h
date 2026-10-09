/* BMS top level: one call of bms_cycle() per 10 ms cycle (cfg.sm.cycle_ms).
 *
 * Order inside one cycle:
 *   1. VCU command frame: E2E check (bms_e2e.h). An OK frame updates close/reset
 *      requests. Without an OK frame for cmd e2e timeout_ms the COMM_LOSS warning is
 *      set; then close_req is forced false in INIT/STANDBY/PRECHARGE, but in ACTIVE the
 *      last valid close request is HELD (no un-commanded loss of propulsion because of
 *      a data-link loss; the monitor still opens the contactors on any cell fault).
 *   2. Monitoring (bms_monitor.h); a fault reset request clears latched faults if no
 *      condition is active.
 *   3. State machine (bms_sm.h), contactor commands.
 *   4. SoC: on the first cycle every cell estimator is initialised from the OCV table
 *      (rest assumption); afterwards one EKF step per cell with the pack current.
 *      Pack SoC = minimum cell SoC (EKF).
 *   5. Balancing (bms_balance.h).
 *   6. CAN: BMS_Status, BMS_Faults and one BMS_CellV frame (mux round robin).
 * The first cycle also sets init_done, so INIT -> STANDBY happens on cycle 2 at the
 * earliest. A cell sample that is implausible (open wire) is not fed to its estimator.
 */
#ifndef BMS_H
#define BMS_H

#include "bms/bms_balance.h"
#include "bms/bms_can.h"
#include "bms/bms_e2e.h"
#include "bms/bms_monitor.h"
#include "bms/bms_sm.h"
#include "bms/bms_soc.h"

typedef struct {
	bms_mon_cfg_t mon;
	bms_sm_cfg_t sm;
	bms_bal_cfg_t bal;
	bms_e2e_cfg_t cmd_e2e;
	const bms_ecm_t *ecm;
} bms_cfg_t;

typedef struct {
	uint16_t cell_mv[BMS_MAX_CELLS];
	int16_t temp_dc[BMS_MAX_TEMPS];
	int32_t current_ma;
	uint32_t bus_mv;
	const uint8_t *vcu_cmd;  /* payload of a VCU_Cmd frame received this cycle, or NULL */
} bms_in_t;

typedef struct {
	bms_contactors_t contactors;
	bms_state_t state;
	bms_faults_t faults;
	uint32_t warnings;
	uint16_t balance_mask;
	uint16_t soc_permille;
	bms_e2e_status_t cmd_status;
	bms_frame_t tx[3];
	uint8_t n_tx;
} bms_out_t;

typedef struct {
	const bms_cfg_t *cfg;
	bms_mon_t mon;
	bms_sm_t sm;
	bms_soc_t soc[BMS_MAX_CELLS];
	bms_e2e_rx_t cmd_rx;
	bms_vcu_cmd_t cmd;
	bool soc_ready;
	bool init_done;
	uint16_t bal_mask;
	uint8_t cnt_status;
	uint8_t cnt_faults;
	uint8_t cnt_cellv;
	uint8_t cellv_mux;
} bms_t;

/* Default configuration for a 12s pack of LG 18650HG2 cells (limits from the cell
 * datasheet window, 10 ms cycle). ecm points to the 25 degC parameter set. */
void bms_default_cfg(bms_cfg_t *cfg);

void bms_init(bms_t *b, const bms_cfg_t *cfg);

void bms_cycle(bms_t *b, const bms_in_t *in, bms_out_t *out);

#endif
