/* Detail tests added after the first mutation run (results/bms/mutation.json listed the
 * surviving mutants): exact boundaries, every status bit, saturation paths, and EKF golden
 * values from the float64 Python mirror bms/tools/ecm_ref.py. */
#include "bms_test.h"
#include "bms/bms.h"
#include "bms/bms_ecm_lg_hg2.h"

/* Golden values: 300 steps, dt 0.1 s, current -6 A / +2 A in 5 s blocks, voltage
 * 3.85 - 0.0004 k + 0.02 i, start soc 0.6, 25 degC set. Expected values from
 * ecm_ref.py (float64); tolerances cover the float32 rounding of the C code. */
/* verifies: BMS-REQ-015, BMS-REQ-016 */
BMS_TEST(bms_details, test_ekf_golden_values)
{
	const bms_ecm_t *m = &bms_ecm_lg_hg2_25c;
	bms_soc_t s;
	uint32_t k;

	bms_soc_init(&s, m, 0.6f);
	for (k = 0u; k < 300u; k++) {
		float i = (((k / 50u) % 2u) == 0u) ? -6.0f : 2.0f;
		float v = (3.85f - (0.0004f * (float)k)) + (0.02f * i);

		bms_soc_step(&s, m, i, v, 0.1f);
	}
	CHECK_NEAR(s.soc, 0.573432438834815, 2e-5);
	CHECK_NEAR(s.v1, -0.014813338676906953, 2e-5);
	CHECK_NEAR(s.p00, 7.243538282701745e-05, 2e-7);
	CHECK_NEAR(s.p01, -3.545610165923164e-05, 2e-7);
	CHECK_NEAR(s.p11, 3.530762853228996e-05, 2e-7);
	CHECK_NEAR(s.v_pred, 3.8289819716851854, 2e-5);
}

/* verifies: BMS-REQ-015 */
BMS_TEST(bms_details, test_coulomb_step_exact)
{
	const bms_ecm_t *m = &bms_ecm_lg_hg2_25c;
	bms_soc_t s;

	bms_soc_init(&s, m, 0.5f);
	bms_soc_step(&s, m, -m->capacity_ah, 3.7f, 360.0f); /* 1 C for 6 min = 10 % */
	CHECK_NEAR(s.soc_cc, 0.4, 1e-6);
	bms_soc_step(&s, m, 2.0f * m->capacity_ah, 3.7f, 180.0f);
	CHECK_NEAR(s.soc_cc, 0.5, 1e-6);
}

/* verifies: BMS-REQ-018 */
BMS_TEST(bms_details, test_every_status_bit)
{
	bms_status_msg_t s = {0u, 0u, 0, 0u, false, false, false, false, false, false};
	bms_status_msg_t r;
	bms_frame_t f;
	uint8_t cnt = 0u;
	uint8_t k;

	bms_can_pack_status(&f, &s, &cnt);
	CHECK_EQ(f.data[7], 0);
	for (k = 0u; k < 6u; k++) {
		s.main_pos = (k == 0u);
		s.main_neg = (k == 1u);
		s.precharge = (k == 2u);
		s.balancing = (k == 3u);
		s.fault = (k == 4u);
		s.comm_loss = (k == 5u);
		bms_can_pack_status(&f, &s, &cnt);
		CHECK_EQ(f.data[7], 1u << k);
		bms_can_unpack_status(f.data, &r);
		CHECK_EQ(r.main_pos, k == 0u);
		CHECK_EQ(r.main_neg, k == 1u);
		CHECK_EQ(r.precharge, k == 2u);
		CHECK_EQ(r.balancing, k == 3u);
		CHECK_EQ(r.fault, k == 4u);
		CHECK_EQ(r.comm_loss, k == 5u);
	}
}

/* verifies: BMS-REQ-018 */
BMS_TEST(bms_details, test_packers_clear_the_frame)
{
	bms_faults_msg_t m = {0u, 2200, 0};
	bms_frame_t f;
	uint8_t cnt = 0u;
	uint8_t k;

	for (k = 0u; k < 8u; k++) {
		f.data[k] = 0xA5u;
	}
	bms_can_pack_faults(&f, &m, &cnt);
	CHECK_EQ(f.data[1], 0); /* counter 0, upper nibble cleared */
	CHECK_EQ(f.data[2] | f.data[3] | f.data[4] | f.data[5], 0);
	CHECK_EQ(f.data[6], 255); /* 220 degC + 40 saturates at 255 */
	CHECK_EQ(f.data[7], 40);
}

/* verifies: BMS-REQ-001, BMS-REQ-002, BMS-REQ-007 */
BMS_TEST(bms_details, test_active_mask_boundaries)
{
	bms_cfg_t cfg;
	bms_mon_t mon;
	uint16_t v[BMS_MAX_CELLS];
	int16_t t[BMS_MAX_TEMPS];
	uint8_t k;

	bms_default_cfg(&cfg);
	cfg.mon.stuck_min_ma = 100000; /* constant test values: keep stuck detection out */
	bms_mon_init(&mon);
	for (k = 0u; k < BMS_MAX_CELLS; k++) {
		v[k] = 3700u;
	}
	for (k = 0u; k < BMS_MAX_TEMPS; k++) {
		t[k] = 250;
	}
	v[0] = 4200u;
	v[1] = 2800u;
	v[2] = 500u;  /* lowest plausible value */
	v[3] = 5000u; /* highest plausible value, also over-voltage */
	t[0] = 600;
	t[1] = -200;
	t[2] = -400;
	t[3] = 1250;
	(void)bms_mon_step(&mon, &cfg.mon, v, t, 6000); /* exactly the charge limit */
	CHECK_EQ(mon.active & BMS_F_OPEN_WIRE, 0);
	CHECK_EQ(mon.active & BMS_F_TEMP_SENSOR, 0);
	CHECK_EQ(mon.active & (BMS_F_OC_CHARGE | BMS_F_OC_DISCHARGE), 0);
	CHECK_EQ(mon.active & BMS_F_CELL_OV, BMS_F_CELL_OV);       /* from cell 3 only */
	CHECK_EQ(mon.active & BMS_F_CELL_UV, BMS_F_CELL_UV);       /* from cell 2 only */
	CHECK_EQ(mon.active & (BMS_F_CELL_OT | BMS_F_CELL_UT), BMS_F_CELL_OT | BMS_F_CELL_UT);
	CHECK_EQ(mon.v_min_mv, 500);
	CHECK_EQ(mon.v_max_mv, 5000);
	CHECK_EQ(mon.t_min_dc, -400);
	CHECK_EQ(mon.t_max_dc, 1250);
	v[2] = 3700u;
	v[3] = 3700u;
	t[2] = 250;
	t[3] = 250;
	(void)bms_mon_step(&mon, &cfg.mon, v, t, -20000); /* exactly the discharge limit */
	CHECK_EQ(mon.active, 0); /* exactly at the limits: no condition */
	CHECK_EQ(mon.v_min_mv, 2800);
	CHECK_EQ(mon.v_max_mv, 4200);
	CHECK_EQ(mon.t_min_dc, -200);
	CHECK_EQ(mon.t_max_dc, 600);
	v[4] = 499u;
	v[5] = 5001u;
	t[2] = -401; /* n_temps is 4: sensors 0..3 */
	t[3] = 1251;
	(void)bms_mon_step(&mon, &cfg.mon, v, t, -20001);
	CHECK_EQ(mon.active, BMS_F_OPEN_WIRE | BMS_F_TEMP_SENSOR | BMS_F_OC_DISCHARGE);
	CHECK(bms_mon_clear(&mon) == false);
}

/* verifies: BMS-REQ-007, BMS-REQ-009 */
BMS_TEST(bms_details, test_all_channels_broken)
{
	bms_cfg_t cfg;
	bms_mon_t mon;
	uint16_t v[BMS_MAX_CELLS];
	int16_t t[BMS_MAX_TEMPS];
	uint8_t k;

	bms_default_cfg(&cfg);
	bms_mon_init(&mon);
	for (k = 0u; k < BMS_MAX_CELLS; k++) {
		v[k] = 0u;
	}
	for (k = 0u; k < BMS_MAX_TEMPS; k++) {
		t[k] = 2000;
	}
	(void)bms_mon_step(&mon, &cfg.mon, v, t, 0);
	CHECK_EQ(mon.v_min_mv, 0);
	CHECK_EQ(mon.v_max_mv, 0);
	CHECK_EQ(mon.t_min_dc, 0);
	CHECK_EQ(mon.t_max_dc, 0);
}

/* An open wire that heals must not inherit OV/UV debounce counts from before the break. */
/* verifies: BMS-REQ-005, BMS-REQ-007 */
BMS_TEST(bms_details, test_open_wire_resets_cell_counters)
{
	bms_cfg_t cfg;
	bms_mon_t mon;
	uint16_t v[BMS_MAX_CELLS];
	int16_t t[BMS_MAX_TEMPS];
	uint8_t k;

	bms_default_cfg(&cfg);
	bms_mon_init(&mon);
	for (k = 0u; k < BMS_MAX_CELLS; k++) {
		v[k] = (uint16_t)(3700u + k);
	}
	for (k = 0u; k < BMS_MAX_TEMPS; k++) {
		t[k] = 250;
	}
	v[0] = 4300u;
	(void)bms_mon_step(&mon, &cfg.mon, v, t, 0);
	(void)bms_mon_step(&mon, &cfg.mon, v, t, 0); /* OV count 2 */
	v[0] = 0u;
	(void)bms_mon_step(&mon, &cfg.mon, v, t, 0); /* one broken sample */
	v[0] = 4300u;
	CHECK_EQ(bms_mon_step(&mon, &cfg.mon, v, t, 0) & BMS_F_CELL_OV, 0);
	v[1] = 2700u;
	(void)bms_mon_step(&mon, &cfg.mon, v, t, 0);
	v[1] = 0u;
	(void)bms_mon_step(&mon, &cfg.mon, v, t, 0);
	v[1] = 2700u;
	(void)bms_mon_step(&mon, &cfg.mon, v, t, 0);
	CHECK_EQ(mon.latched & BMS_F_CELL_UV, 0);
	t[0] = 700;
	(void)bms_mon_step(&mon, &cfg.mon, v, t, 0);
	(void)bms_mon_step(&mon, &cfg.mon, v, t, 0);
	t[0] = 3000;
	(void)bms_mon_step(&mon, &cfg.mon, v, t, 0);
	t[0] = 700;
	(void)bms_mon_step(&mon, &cfg.mon, v, t, 0);
	CHECK_EQ(mon.latched & BMS_F_CELL_OT, 0);
	t[1] = -300;
	(void)bms_mon_step(&mon, &cfg.mon, v, t, 0);
	(void)bms_mon_step(&mon, &cfg.mon, v, t, 0);
	t[1] = -3000;
	(void)bms_mon_step(&mon, &cfg.mon, v, t, 0);
	t[1] = -300;
	(void)bms_mon_step(&mon, &cfg.mon, v, t, 0);
	CHECK_EQ(mon.latched & BMS_F_CELL_UT, 0);
}

/* verifies: BMS-REQ-008 */
BMS_TEST(bms_details, test_stuck_needs_load_threshold)
{
	bms_cfg_t cfg;
	bms_mon_t mon;
	uint16_t v[BMS_MAX_CELLS];
	int16_t t[BMS_MAX_TEMPS];
	uint16_t k;

	bms_default_cfg(&cfg);
	cfg.mon.stuck_samples = 3u;
	for (k = 0u; k < BMS_MAX_CELLS; k++) {
		v[k] = 3700u;
	}
	for (k = 0u; k < BMS_MAX_TEMPS; k++) {
		t[k] = 250;
	}
	bms_mon_init(&mon);
	for (k = 0u; k < 4u; k++) {
		(void)bms_mon_step(&mon, &cfg.mon, v, t, 1000); /* exactly 1 A charge: loaded */
	}
	CHECK_EQ(mon.latched, BMS_F_SENSOR_STUCK);
	bms_mon_init(&mon);
	for (k = 0u; k < 4u; k++) {
		(void)bms_mon_step(&mon, &cfg.mon, v, t, -1000);
	}
	CHECK_EQ(mon.latched, BMS_F_SENSOR_STUCK);
	bms_mon_init(&mon);
	for (k = 0u; k < 4u; k++) {
		(void)bms_mon_step(&mon, &cfg.mon, v, t, -999);
	}
	CHECK_EQ(mon.latched, 0);
	CHECK_EQ(mon.active, 0); /* identical values at rest are not even a condition */
}

/* verifies: BMS-REQ-017 */
BMS_TEST(bms_details, test_balance_edges)
{
	bms_cfg_t cfg;
	uint16_t v[BMS_MAX_CELLS];
	uint8_t k;

	bms_default_cfg(&cfg);
	for (k = 0u; k < BMS_MAX_CELLS; k++) {
		v[k] = 3600u;
	}
	v[0] = 3620u; /* exactly start above min and exactly at the 3.6 V floor for the others */
	v[11] = 3000u; /* outside n_cells = 11: not part of the minimum */
	CHECK_EQ(bms_balance(0u, &cfg.bal, v, 11u, 0, true), 1u);
	CHECK_EQ(bms_balance(0u, &cfg.bal, v, 0u, 0, true), 0);
	v[0] = 3600u;
	v[1] = 3640u;
	CHECK_EQ(bms_balance(0u, &cfg.bal, v, 2u, 0, true), 2u);
	/* exactly max_on cells already on, a fifth one qualifies: stays at 4 */
	for (k = 0u; k < 6u; k++) {
		v[k] = 3700u;
	}
	v[6] = 3600u;
	CHECK_EQ(bms_balance(0x0Fu, &cfg.bal, v, 7u, 0, true), 0x0F);
	CHECK_EQ(bms_balance(0x30u, &cfg.bal, v, 7u, 0, true), 0x33);
}

/* verifies: BMS-REQ-011, BMS-REQ-021 */
BMS_TEST(bms_details, test_timer_saturation)
{
	bms_cfg_t cfg;
	bms_sm_t sm;
	bms_sm_in_t in = {true, true, false, 0u, 1u, 0u};
	bms_e2e_rx_t rx;
	bms_e2e_cfg_t ecfg = {0x30u, 2u, 0xFFFFu};
	uint32_t k;

	bms_default_cfg(&cfg);
	cfg.sm.cycle_ms = 1000u;
	cfg.sm.precharge_timeout_ms = 0xFFFFu;
	bms_sm_init(&sm);
	(void)bms_sm_step(&sm, &cfg.sm, &in);
	(void)bms_sm_step(&sm, &cfg.sm, &in);
	for (k = 0u; k < 64u; k++) {
		CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_PRECHARGE);
	}
	CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_PRECHARGE); /* 65000 ms */
	CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_FAULT);     /* saturates at 65535 */
	CHECK_EQ(sm.precharge_ms, 0xFFFF);
	in.pack_mv = 1u; /* 1 mV pack, bus 0 V: not precharged */
	bms_e2e_rx_init(&rx);
	for (k = 0u; k < 65u; k++) {
		CHECK_EQ(bms_e2e_check(&rx, &ecfg, NULL, 1000u), BMS_E2E_NO_DATA);
	}
	CHECK_EQ(bms_e2e_check(&rx, &ecfg, NULL, 1000u), BMS_E2E_TIMEOUT);
	CHECK_EQ(rx.ms_since_ok, 0xFFFF);
	bms_e2e_rx_init(&rx);
	CHECK_EQ(rx.ms_since_ok, 0);
	CHECK(!rx.have_last);
}

/* verifies: BMS-REQ-010 */
BMS_TEST(bms_details, test_precharge_with_tiny_pack_voltage)
{
	bms_cfg_t cfg;
	bms_sm_t sm;
	bms_sm_in_t in = {true, true, false, 0u, 1u, 1u};

	bms_default_cfg(&cfg);
	bms_sm_init(&sm);
	sm.state = BMS_ST_PRECHARGE;
	CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_ACTIVE); /* 1 mV >= 95 % of 1 mV */
}

/* verifies: BMS-REQ-020 */
BMS_TEST(bms_details, test_cycle_reports_e2e_warnings)
{
	bms_cfg_t cfg;
	bms_t b;
	bms_in_t in;
	bms_out_t out;
	bms_frame_t f;
	bms_vcu_cmd_t c = {false, false};
	uint8_t cnt = 0u;
	uint8_t k;

	bms_default_cfg(&cfg);
	bms_init(&b, &cfg);
	for (k = 0u; k < BMS_MAX_CELLS; k++) {
		in.cell_mv[k] = 3700u;
	}
	for (k = 0u; k < BMS_MAX_TEMPS; k++) {
		in.temp_dc[k] = 250;
	}
	in.current_ma = 0;
	in.bus_mv = 0u;
	bms_can_pack_vcu_cmd(&f, &c, &cnt);
	in.vcu_cmd = f.data;
	bms_cycle(&b, &in, &out);
	CHECK_EQ(out.warnings, 0);
	bms_cycle(&b, &in, &out); /* same frame again */
	CHECK_EQ(out.cmd_status, BMS_E2E_REPEATED);
	CHECK_EQ(out.warnings, BMS_W_CMD_E2E);
	cnt = 9u;
	bms_can_pack_vcu_cmd(&f, &c, &cnt);
	bms_cycle(&b, &in, &out);
	CHECK_EQ(out.cmd_status, BMS_E2E_WRONG_SEQUENCE);
	CHECK_EQ(out.warnings, BMS_W_CMD_E2E);
	in.vcu_cmd = NULL;
	bms_cycle(&b, &in, &out);
	CHECK_EQ(out.warnings, 0);
}

/* verifies: BMS-REQ-014, BMS-REQ-015 */
BMS_TEST(bms_details, test_cycle_soc_outputs)
{
	bms_cfg_t cfg;
	bms_t b;
	bms_in_t in;
	bms_out_t out;
	bms_status_msg_t s;
	uint8_t k;
	float soc0;

	bms_default_cfg(&cfg);
	cfg.mon.n_cells = 10u;
	bms_init(&b, &cfg);
	for (k = 0u; k < BMS_MAX_CELLS; k++) {
		in.cell_mv[k] = 3900u;
		in.temp_dc[k % BMS_MAX_TEMPS] = 250;
	}
	in.cell_mv[7] = 3700u;   /* weakest cell sets the pack SoC */
	in.cell_mv[10] = 3000u;  /* beyond n_cells: ignored */
	in.current_ma = -10000;
	in.bus_mv = 0u;
	in.vcu_cmd = NULL;
	bms_cycle(&b, &in, &out);
	soc0 = bms_soc_from_ocv(cfg.ecm, 3.7f);
	CHECK_EQ(out.soc_permille, (uint16_t)((soc0 * 1000.0f) + 0.5f));
	CHECK_EQ(out.tx[2].data[1] >> 4, 0);
	for (k = 1u; k <= 100u; k++) {
		bms_cycle(&b, &in, &out);
		CHECK_EQ(out.tx[2].data[1] >> 4, k % 4u); /* 10 cells: mux 0..3 */
	}
	/* 100 cycles of 10 ms at -10 A on the Coulomb counter of every cell */
	CHECK_NEAR(b.soc[0].soc_cc - bms_soc_from_ocv(cfg.ecm, 3.9f), -10.0 / (3600.0 * cfg.ecm->capacity_ah),
		   2e-5);
	CHECK_NEAR(b.soc[10].soc_cc, 0.0, 0.0); /* not touched */
	bms_can_unpack_status(out.tx[0].data, &s);
	CHECK_EQ(s.pack_mv, 38800);
	CHECK(!s.balancing);
	CHECK_EQ(s.soc_permille, (out.soc_permille / 5u) * 5u);
}
