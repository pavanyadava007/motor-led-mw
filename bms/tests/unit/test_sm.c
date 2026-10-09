#include "bms_test.h"
#include "bms/bms.h"

static bms_cfg_t cfg;
static bms_sm_t sm;
static bms_sm_in_t in;

static void setup(void)
{
	bms_default_cfg(&cfg);
	bms_sm_init(&sm);
	in.init_done = true;
	in.close_req = false;
	in.reset_req = false;
	in.faults = 0u;
	in.pack_mv = 44000u;
	in.bus_mv = 0u;
}

static bool all_open(void)
{
	return !sm.cmd.main_pos && !sm.cmd.main_neg && !sm.cmd.precharge;
}

/* verifies: BMS-REQ-010 */
BMS_TEST(bms_sm, test_startup_sequence)
{
	setup();
	in.init_done = false;
	CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_INIT);
	CHECK(all_open());
	in.init_done = true;
	CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_STANDBY);
	CHECK(all_open());
	in.close_req = true;
	CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_PRECHARGE);
	CHECK(!sm.cmd.main_pos && sm.cmd.main_neg && sm.cmd.precharge);
	in.bus_mv = 41799u; /* 94.99 % of 44 V */
	CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_PRECHARGE);
	CHECK(!sm.cmd.main_pos);
	in.bus_mv = 41800u; /* 95 % */
	CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_ACTIVE);
	CHECK(sm.cmd.main_pos && sm.cmd.main_neg && !sm.cmd.precharge);
	in.close_req = false;
	CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_STANDBY);
	CHECK(all_open());
}

/* verifies: BMS-REQ-010 */
BMS_TEST(bms_sm, test_precharge_needs_pack_voltage)
{
	setup();
	(void)bms_sm_step(&sm, &cfg.sm, &in);
	in.close_req = true;
	in.pack_mv = 0u; /* no pack voltage measured: never "precharged" */
	in.bus_mv = 0u;
	CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_PRECHARGE);
	CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_PRECHARGE);
	in.close_req = false;
	CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_STANDBY);
}

/* verifies: BMS-REQ-011 */
BMS_TEST(bms_sm, test_precharge_timeout)
{
	uint8_t k;

	setup();
	(void)bms_sm_step(&sm, &cfg.sm, &in);
	in.close_req = true;
	in.bus_mv = 1000u;
	CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_PRECHARGE);
	/* 500 ms at 10 ms per cycle: 49 more cycles stay, the 50th times out */
	for (k = 0u; k < 49u; k++) {
		CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_PRECHARGE);
	}
	CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_FAULT);
	CHECK_EQ(sm.own_faults, BMS_F_PRECHARGE);
	CHECK(all_open());
}

/* verifies: BMS-REQ-013 */
BMS_TEST(bms_sm, test_fault_opens_from_every_state)
{
	int from;

	for (from = (int)BMS_ST_INIT; from <= (int)BMS_ST_ACTIVE; from++) {
		setup();
		sm.state = (bms_state_t)from;
		in.close_req = true;
		in.bus_mv = 44000u;
		in.faults = BMS_F_CELL_OT;
		CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_FAULT);
		CHECK(all_open());
	}
}

/* verifies: BMS-REQ-012 */
BMS_TEST(bms_sm, test_fault_exit_needs_reset_and_no_fault)
{
	setup();
	sm.state = BMS_ST_ACTIVE;
	in.close_req = true;
	in.bus_mv = 44000u;
	in.faults = BMS_F_CELL_UV;
	CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_FAULT);
	in.reset_req = true;
	CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_FAULT); /* fault still latched */
	in.faults = 0u;
	in.reset_req = false;
	CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_FAULT); /* no reset request */
	in.reset_req = true;
	CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_STANDBY); /* never straight to ACTIVE */
	CHECK(all_open());
}

/* verifies: BMS-REQ-011, BMS-REQ-012 */
BMS_TEST(bms_sm, test_precharge_fault_cleared_by_reset)
{
	uint8_t k;

	setup();
	(void)bms_sm_step(&sm, &cfg.sm, &in);
	in.close_req = true;
	for (k = 0u; k < 60u; k++) {
		(void)bms_sm_step(&sm, &cfg.sm, &in);
	}
	CHECK_EQ(sm.state, BMS_ST_FAULT);
	in.close_req = false;
	in.reset_req = true;
	CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_STANDBY);
	CHECK_EQ(sm.own_faults, 0);
	in.reset_req = false;
	in.close_req = true;
	CHECK_EQ(bms_sm_step(&sm, &cfg.sm, &in), BMS_ST_PRECHARGE);
	CHECK_EQ(sm.precharge_ms, 0); /* timer restarts on entry */
}
