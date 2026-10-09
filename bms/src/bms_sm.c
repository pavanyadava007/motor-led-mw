/* Contactor state machine, see bms/bms_sm.h. */
#include "bms/bms_sm.h"

static void set_cmd(bms_sm_t *sm, bool pos, bool neg, bool pre)
{
	sm->cmd.main_pos = pos;
	sm->cmd.main_neg = neg;
	sm->cmd.precharge = pre;
}

void bms_sm_init(bms_sm_t *sm)
{
	sm->state = BMS_ST_INIT;
	sm->precharge_ms = 0u;
	sm->own_faults = 0u;
	set_cmd(sm, false, false, false);
}

static bool precharge_done(const bms_sm_cfg_t *cfg, const bms_sm_in_t *in)
{
	uint64_t bus = (uint64_t)in->bus_mv * 100u;
	uint64_t pack = (uint64_t)in->pack_mv * (uint64_t)cfg->precharge_pct;

	return (in->pack_mv > 0u) && (bus >= pack);
}

bms_state_t bms_sm_step(bms_sm_t *sm, const bms_sm_cfg_t *cfg, const bms_sm_in_t *in)
{
	bms_state_t next = sm->state;
	bool fault = ((in->faults | sm->own_faults) != 0u);

	switch (sm->state) {
	case BMS_ST_INIT:
		if (fault) {
			next = BMS_ST_FAULT;
		} else if (in->init_done) {
			next = BMS_ST_STANDBY;
		} else {
			next = BMS_ST_INIT;
		}
		break;
	case BMS_ST_STANDBY:
		if (fault) {
			next = BMS_ST_FAULT;
		} else if (in->close_req) {
			next = BMS_ST_PRECHARGE;
			sm->precharge_ms = 0u;
		} else {
			next = BMS_ST_STANDBY;
		}
		break;
	case BMS_ST_PRECHARGE:
		if (fault) {
			next = BMS_ST_FAULT;
		} else if (!in->close_req) {
			next = BMS_ST_STANDBY;
		} else if (precharge_done(cfg, in)) {
			next = BMS_ST_ACTIVE;
		} else {
			uint32_t t = (uint32_t)sm->precharge_ms + cfg->cycle_ms;

			sm->precharge_ms = (t > 0xFFFFu) ? 0xFFFFu : (uint16_t)t;
			if (sm->precharge_ms >= cfg->precharge_timeout_ms) {
				sm->own_faults |= BMS_F_PRECHARGE;
				next = BMS_ST_FAULT;
			}
		}
		break;
	case BMS_ST_ACTIVE:
		if (fault) {
			next = BMS_ST_FAULT;
		} else if (!in->close_req) {
			next = BMS_ST_STANDBY;
		} else {
			next = BMS_ST_ACTIVE;
		}
		break;
	case BMS_ST_FAULT:
	default:
		if (in->reset_req && (in->faults == 0u)) {
			sm->own_faults = 0u;
			next = BMS_ST_STANDBY;
		} else {
			next = BMS_ST_FAULT;
		}
		break;
	}
	sm->state = next;
	if (next == BMS_ST_PRECHARGE) {
		set_cmd(sm, false, true, true);
	} else if (next == BMS_ST_ACTIVE) {
		set_cmd(sm, true, true, false);
	} else {
		set_cmd(sm, false, false, false);
	}
	return next;
}
