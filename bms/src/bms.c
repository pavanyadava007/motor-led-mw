/* BMS top level, see bms/bms.h. */
#include "bms/bms.h"
#include "bms/bms_ecm_lg_hg2.h"

void bms_default_cfg(bms_cfg_t *cfg)
{
	cfg->mon.n_cells = 12u;
	cfg->mon.n_temps = 4u;
	cfg->mon.ov_mv = 4200u;
	cfg->mon.uv_mv = 2800u;
	cfg->mon.ot_dc = 600;   /* 60.0 degC */
	cfg->mon.ut_dc = -200;  /* -20.0 degC */
	cfg->mon.oc_dis_ma = 20000;
	cfg->mon.oc_chg_ma = 6000;
	cfg->mon.debounce = 3u;
	cfg->mon.wire_min_mv = 500u;
	cfg->mon.wire_max_mv = 5000u;
	cfg->mon.temp_min_dc = -400;
	cfg->mon.temp_max_dc = 1250;
	cfg->mon.stuck_samples = 100u;  /* 1 s at 10 ms */
	cfg->mon.stuck_min_ma = 1000;
	cfg->sm.cycle_ms = 10u;
	cfg->sm.precharge_timeout_ms = 500u;
	cfg->sm.precharge_pct = 95u;
	cfg->bal.start_mv = 20u;
	cfg->bal.stop_mv = 5u;
	cfg->bal.min_cell_mv = 3600u;
	cfg->bal.max_dis_ma = 500;
	cfg->bal.max_on = 4u;
	cfg->cmd_e2e.data_id = BMS_DID_VCU_CMD;
	cfg->cmd_e2e.max_delta = 2u;
	cfg->cmd_e2e.timeout_ms = 100u;
	cfg->ecm = &bms_ecm_lg_hg2_25c;
}

void bms_init(bms_t *b, const bms_cfg_t *cfg)
{
	uint8_t k;

	b->cfg = cfg;
	bms_mon_init(&b->mon);
	bms_sm_init(&b->sm);
	for (k = 0u; k < BMS_MAX_CELLS; k++) {
		bms_soc_init(&b->soc[k], cfg->ecm, 0.0f);
	}
	bms_e2e_rx_init(&b->cmd_rx);
	b->cmd.close_req = false;
	b->cmd.reset_req = false;
	b->soc_ready = false;
	b->init_done = false;
	b->bal_mask = 0u;
	b->cnt_status = 0u;
	b->cnt_faults = 0u;
	b->cnt_cellv = 0u;
	b->cellv_mux = 0u;
}

static void handle_cmd(bms_t *b, const bms_in_t *in, bms_out_t *out)
{
	const bms_cfg_t *cfg = b->cfg;
	bms_e2e_status_t st = bms_e2e_check(&b->cmd_rx, &cfg->cmd_e2e, in->vcu_cmd, cfg->sm.cycle_ms);

	out->cmd_status = st;
	out->warnings = 0u;
	b->cmd.reset_req = false;
	if (st == BMS_E2E_OK) {
		bms_can_unpack_vcu_cmd(in->vcu_cmd, &b->cmd);
	} else if ((st == BMS_E2E_CRC_ERROR) || (st == BMS_E2E_WRONG_SEQUENCE) ||
		   (st == BMS_E2E_REPEATED)) {
		out->warnings |= BMS_W_CMD_E2E;
	} else {
		/* NO_DATA keeps the last request; TIMEOUT handled below */
	}
	if (b->cmd_rx.ms_since_ok >= cfg->cmd_e2e.timeout_ms) {
		out->warnings |= BMS_W_COMM_LOSS;
		if (b->sm.state != BMS_ST_ACTIVE) {
			b->cmd.close_req = false;
		}
	}
}

static void run_soc(bms_t *b, const bms_in_t *in, uint16_t *soc_permille)
{
	const bms_cfg_t *cfg = b->cfg;
	uint8_t n = (cfg->mon.n_cells <= BMS_MAX_CELLS) ? cfg->mon.n_cells : (uint8_t)BMS_MAX_CELLS;
	float i_a = (float)in->current_ma * 0.001f;
	float dt = (float)cfg->sm.cycle_ms * 0.001f;
	float smin = 1.0f;
	uint8_t k;

	for (k = 0u; k < n; k++) {
		uint16_t mv = in->cell_mv[k];
		bool ok = (mv >= cfg->mon.wire_min_mv) && (mv <= cfg->mon.wire_max_mv);
		float v = (float)mv * 0.001f;

		if (ok) {
			if (!b->soc_ready) {
				bms_soc_init(&b->soc[k], cfg->ecm, bms_soc_from_ocv(cfg->ecm, v));
			} else {
				bms_soc_step(&b->soc[k], cfg->ecm, i_a, v, dt);
			}
		}
		if (b->soc[k].soc < smin) {
			smin = b->soc[k].soc;
		}
	}
	b->soc_ready = true;
	*soc_permille = (uint16_t)((smin * 1000.0f) + 0.5f);
}

static uint32_t pack_mv(const bms_t *b, const bms_in_t *in)
{
	uint32_t sum = 0u;
	uint8_t k;

	for (k = 0u; (k < b->cfg->mon.n_cells) && (k < BMS_MAX_CELLS); k++) {
		sum += in->cell_mv[k];
	}
	return sum;
}

void bms_cycle(bms_t *b, const bms_in_t *in, bms_out_t *out)
{
	const bms_cfg_t *cfg = b->cfg;
	bms_sm_in_t smi;
	bms_status_msg_t st;
	bms_faults_msg_t fm;
	uint8_t n_mux;

	handle_cmd(b, in, out);

	(void)bms_mon_step(&b->mon, &cfg->mon, in->cell_mv, in->temp_dc, in->current_ma);
	if (b->cmd.reset_req) {
		(void)bms_mon_clear(&b->mon);
	}

	smi.init_done = b->init_done;
	smi.close_req = b->cmd.close_req;
	smi.reset_req = b->cmd.reset_req;
	smi.faults = b->mon.latched;
	smi.pack_mv = pack_mv(b, in);
	smi.bus_mv = in->bus_mv;
	out->state = bms_sm_step(&b->sm, &cfg->sm, &smi);
	out->contactors = b->sm.cmd;
	out->faults = b->mon.latched | b->sm.own_faults;

	run_soc(b, in, &out->soc_permille);
	b->init_done = true;

	b->bal_mask = bms_balance(b->bal_mask, &cfg->bal, in->cell_mv, cfg->mon.n_cells, in->current_ma,
				  out->faults == 0u);
	out->balance_mask = b->bal_mask;

	st.state = (uint8_t)out->state;
	st.pack_mv = smi.pack_mv;
	st.current_ma = in->current_ma;
	st.soc_permille = out->soc_permille;
	st.main_pos = out->contactors.main_pos;
	st.main_neg = out->contactors.main_neg;
	st.precharge = out->contactors.precharge;
	st.balancing = out->balance_mask != 0u;
	st.fault = out->faults != 0u;
	st.comm_loss = (out->warnings & BMS_W_COMM_LOSS) != 0u;
	bms_can_pack_status(&out->tx[0], &st, &b->cnt_status);
	fm.faults = out->faults;
	fm.t_max_dc = b->mon.t_max_dc;
	fm.t_min_dc = b->mon.t_min_dc;
	bms_can_pack_faults(&out->tx[1], &fm, &b->cnt_faults);
	bms_can_pack_cellv(&out->tx[2], b->cellv_mux, in->cell_mv, cfg->mon.n_cells, &b->cnt_cellv);
	n_mux = (uint8_t)((cfg->mon.n_cells + 2u) / 3u);
	b->cellv_mux = (uint8_t)(b->cellv_mux + 1u);
	if (b->cellv_mux >= n_mux) {
		b->cellv_mux = 0u;
	}
	out->n_tx = 3u;
}
