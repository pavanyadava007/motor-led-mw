/* Contactor state machine (one call per BMS cycle).
 *
 *   INIT --init_done, no fault--> STANDBY --close_req--> PRECHARGE --bus >= pct of pack--> ACTIVE
 *     any state --latched fault--> FAULT (all contactors open in the SAME call)
 *     PRECHARGE --!close_req--> STANDBY ; ACTIVE --!close_req--> STANDBY
 *     PRECHARGE --elapsed >= precharge_timeout_ms--> FAULT, latches BMS_F_PRECHARGE
 *     FAULT --reset_req and no latched fault--> STANDBY
 *
 * Contactor commands per state (true = closed):
 *   INIT, STANDBY, FAULT : main_pos=0 main_neg=0 precharge=0
 *   PRECHARGE            : main_pos=0 main_neg=1 precharge=1
 *   ACTIVE               : main_pos=1 main_neg=1 precharge=0
 * Precharge completes when bus_mv * 100 >= pack_mv * precharge_pct (pack_mv > 0).
 * The precharge timer counts cycle_ms per call spent in PRECHARGE, starting at 0 on entry.
 */
#ifndef BMS_SM_H
#define BMS_SM_H

#include "bms/bms_types.h"

typedef enum {
	BMS_ST_INIT = 0,
	BMS_ST_STANDBY = 1,
	BMS_ST_PRECHARGE = 2,
	BMS_ST_ACTIVE = 3,
	BMS_ST_FAULT = 4
} bms_state_t;

typedef struct {
	uint16_t cycle_ms;
	uint16_t precharge_timeout_ms;
	uint8_t precharge_pct;  /* 1..100 */
} bms_sm_cfg_t;

typedef struct {
	bool init_done;
	bool close_req;
	bool reset_req;
	bms_faults_t faults;  /* latched faults from the monitor */
	uint32_t pack_mv;
	uint32_t bus_mv;
} bms_sm_in_t;

typedef struct {
	bool main_pos;
	bool main_neg;
	bool precharge;
} bms_contactors_t;

typedef struct {
	bms_state_t state;
	uint16_t precharge_ms;
	bms_faults_t own_faults;  /* faults found by the state machine (BMS_F_PRECHARGE) */
	bms_contactors_t cmd;
} bms_sm_t;

void bms_sm_init(bms_sm_t *sm);

/* One step. Returns the new state; sm->cmd holds the contactor commands. */
bms_state_t bms_sm_step(bms_sm_t *sm, const bms_sm_cfg_t *cfg, const bms_sm_in_t *in);

#endif
