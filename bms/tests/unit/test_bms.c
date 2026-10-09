/* Integration and fault-injection tests of bms_cycle(). */
#include "bms_test.h"
#include "bms/bms.h"

static bms_cfg_t cfg;
static bms_t b;
static bms_in_t in;
static bms_out_t out;
static bms_frame_t cmd;
static uint8_t vcu_cnt;
static uint32_t tick;

static void setup(void)
{
	uint8_t k;

	bms_default_cfg(&cfg);
	bms_init(&b, &cfg);
	for (k = 0u; k < BMS_MAX_CELLS; k++) {
		in.cell_mv[k] = 3800u;
	}
	for (k = 0u; k < BMS_MAX_TEMPS; k++) {
		in.temp_dc[k] = 250;
	}
	in.current_ma = 0;
	in.bus_mv = 0u;
	in.vcu_cmd = NULL;
	vcu_cnt = 0u;
	tick = 0u;
}

/* one 10 ms cycle; the cell voltages wiggle by 1 mV like a real ADC */
static void cycle(bool send, bool close, bool reset)
{
	uint8_t k;

	tick++;
	for (k = 0u; k < 12u; k++) {
		if ((in.cell_mv[k] > 1000u) && (in.cell_mv[k] < 4500u)) {
			in.cell_mv[k] = (uint16_t)(in.cell_mv[k] ^ 1u);
		}
	}
	if (send) {
		bms_vcu_cmd_t c = {close, reset};

		bms_can_pack_vcu_cmd(&cmd, &c, &vcu_cnt);
		in.vcu_cmd = cmd.data;
	} else {
		in.vcu_cmd = NULL;
	}
	bms_cycle(&b, &in, &out);
}

static bool all_open(void)
{
	return !out.contactors.main_pos && !out.contactors.main_neg && !out.contactors.precharge;
}

static void go_active(void)
{
	uint8_t k;

	for (k = 0u; k < 3u; k++) {
		cycle(true, false, false);
	}
	cycle(true, true, false);
	in.bus_mv = 45000u; /* precharge done */
	cycle(true, true, false);
}

/* verifies: BMS-REQ-010, BMS-REQ-014 */
BMS_TEST(bms_cycle, test_power_up_to_active)
{
	setup();
	cycle(true, false, false);
	CHECK_EQ(out.state, BMS_ST_INIT);
	CHECK_NEAR(b.soc[0].soc, bms_soc_from_ocv(cfg.ecm, 3.801f), 1e-6); /* SoC from OCV */
	cycle(true, false, false);
	CHECK_EQ(out.state, BMS_ST_STANDBY);
	cycle(true, true, false);
	CHECK_EQ(out.state, BMS_ST_PRECHARGE);
	CHECK(!out.contactors.main_pos && out.contactors.precharge);
	in.bus_mv = 45000u;
	cycle(true, true, false);
	CHECK_EQ(out.state, BMS_ST_ACTIVE);
	CHECK(out.contactors.main_pos && out.contactors.main_neg && !out.contactors.precharge);
}

/* verifies: BMS-REQ-001, BMS-REQ-013 */
BMS_TEST(bms_cycle, test_cell_ov_opens_contactors_in_same_cycle)
{
	setup();
	go_active();
	CHECK_EQ(out.state, BMS_ST_ACTIVE);
	in.cell_mv[6] = 4250u;
	cycle(true, true, false);
	CHECK_EQ(out.state, BMS_ST_ACTIVE);
	cycle(true, true, false);
	CHECK_EQ(out.state, BMS_ST_ACTIVE);
	cycle(true, true, false); /* 3rd sample: latched and open in this 10 ms cycle */
	CHECK_EQ(out.state, BMS_ST_FAULT);
	CHECK(all_open());
	CHECK_EQ(out.faults, BMS_F_CELL_OV);
	CHECK_EQ(out.balance_mask, 0);
}

/* verifies: BMS-REQ-006, BMS-REQ-012 */
BMS_TEST(bms_cycle, test_fault_reset_over_can)
{
	setup();
	go_active();
	in.temp_dc[2] = 700;
	cycle(true, true, false);
	cycle(true, true, false);
	cycle(true, true, false);
	CHECK_EQ(out.state, BMS_ST_FAULT);
	cycle(true, false, true); /* reset while still hot: refused */
	CHECK_EQ(out.state, BMS_ST_FAULT);
	in.temp_dc[2] = 300;
	cycle(true, true, false); /* cooled down, but no reset request yet */
	CHECK_EQ(out.state, BMS_ST_FAULT);
	cycle(true, false, true);
	CHECK_EQ(out.state, BMS_ST_STANDBY);
	CHECK_EQ(out.faults, 0);
}

/* verifies: BMS-REQ-020 */
BMS_TEST(bms_cycle, test_close_request_with_bad_crc_is_ignored)
{
	setup();
	cycle(true, false, false);
	cycle(true, false, false);
	CHECK_EQ(out.state, BMS_ST_STANDBY);
	{
		bms_vcu_cmd_t c = {true, false};

		bms_can_pack_vcu_cmd(&cmd, &c, &vcu_cnt);
		cmd.data[0] = (uint8_t)(cmd.data[0] ^ 0x01u); /* corrupted CRC */
		in.vcu_cmd = cmd.data;
		bms_cycle(&b, &in, &out);
	}
	CHECK_EQ(out.cmd_status, BMS_E2E_CRC_ERROR);
	CHECK((out.warnings & BMS_W_CMD_E2E) != 0u);
	CHECK_EQ(out.state, BMS_ST_STANDBY);
	CHECK(all_open());
}

/* verifies: BMS-REQ-020 */
BMS_TEST(bms_cycle, test_replayed_frame_with_frozen_counter_is_ignored)
{
	bms_frame_t old;
	uint8_t frozen;

	setup();
	cycle(true, false, false);
	cycle(true, false, false);
	/* a frame with close=1 whose alive counter equals the last accepted one */
	frozen = (uint8_t)((vcu_cnt - 1u) & 0x0Fu);
	{
		bms_vcu_cmd_t c = {true, false};

		bms_can_pack_vcu_cmd(&old, &c, &frozen);
	}
	in.vcu_cmd = old.data;
	bms_cycle(&b, &in, &out);
	CHECK_EQ(out.cmd_status, BMS_E2E_REPEATED);
	CHECK_EQ(out.state, BMS_ST_STANDBY);
}

/* verifies: BMS-REQ-021 */
BMS_TEST(bms_cycle, test_comm_loss_holds_active)
{
	uint8_t k;

	setup();
	go_active();
	for (k = 0u; k < 9u; k++) {
		cycle(false, false, false);
		CHECK_EQ(out.warnings & BMS_W_COMM_LOSS, 0);
	}
	cycle(false, false, false); /* 100 ms without a valid frame */
	CHECK((out.warnings & BMS_W_COMM_LOSS) != 0u);
	CHECK_EQ(out.state, BMS_ST_ACTIVE);
	CHECK(out.contactors.main_pos && out.contactors.main_neg);
	{
		bms_status_msg_t s;

		bms_can_unpack_status(out.tx[0].data, &s);
		CHECK(s.comm_loss);
	}
	in.cell_mv[0] = 2700u; /* a cell fault still opens during comm loss */
	cycle(false, false, false);
	cycle(false, false, false);
	cycle(false, false, false);
	CHECK_EQ(out.state, BMS_ST_FAULT);
	CHECK(all_open());
}

/* verifies: BMS-REQ-021 */
BMS_TEST(bms_cycle, test_comm_loss_drops_close_request_before_active)
{
	uint8_t k;

	setup();
	cycle(true, false, false);
	cycle(true, false, false);
	cycle(true, true, false);
	CHECK_EQ(out.state, BMS_ST_PRECHARGE);
	for (k = 0u; k < 10u; k++) {
		cycle(false, false, false);
	}
	CHECK_EQ(out.state, BMS_ST_STANDBY);
	CHECK(all_open());
}

/* verifies: BMS-REQ-007 */
BMS_TEST(bms_cycle, test_open_wire_injection)
{
	float soc_before;

	setup();
	go_active();
	in.current_ma = -10000;
	cycle(true, true, false);
	soc_before = b.soc[3].soc;
	in.cell_mv[3] = 0u;
	cycle(true, true, false);
	CHECK_NEAR(b.soc[3].soc, soc_before, 0.0); /* broken sample not fed to the EKF */
	cycle(true, true, false);
	cycle(true, true, false);
	CHECK_EQ(out.state, BMS_ST_FAULT);
	CHECK_EQ(out.faults, BMS_F_OPEN_WIRE);
	CHECK(all_open());
}

/* verifies: BMS-REQ-008 */
BMS_TEST(bms_cycle, test_stuck_sensor_injection)
{
	uint8_t k;
	uint16_t stuck = 3777u;

	setup();
	go_active();
	in.current_ma = -5000;
	for (k = 0u; k < 100u; k++) {
		in.cell_mv[9] = (uint16_t)(stuck ^ 1u); /* cycle() flips bit 0: ADC frozen at 3777 */
		cycle(true, true, false);
		CHECK_EQ(out.state, BMS_ST_ACTIVE);
	}
	in.cell_mv[9] = (uint16_t)(stuck ^ 1u);
	cycle(true, true, false);
	CHECK_EQ(out.state, BMS_ST_FAULT);
	CHECK_EQ(out.faults, BMS_F_SENSOR_STUCK);
}

/* verifies: BMS-REQ-018, BMS-REQ-019 */
BMS_TEST(bms_cycle, test_tx_frames_every_cycle)
{
	uint8_t k;
	bms_status_msg_t s;

	setup();
	go_active();
	for (k = 0u; k < 8u; k++) {
		cycle(true, true, false);
		CHECK_EQ(out.n_tx, 3);
		CHECK_EQ(out.tx[0].id, BMS_CAN_ID_STATUS);
		CHECK_EQ(out.tx[0].data[0], bms_e2e_crc(BMS_DID_STATUS, out.tx[0].data));
		CHECK_EQ(out.tx[1].data[0], bms_e2e_crc(BMS_DID_FAULTS, out.tx[1].data));
		CHECK_EQ(out.tx[2].data[0],
			 bms_e2e_crc((uint8_t)(BMS_DID_CELLV + (out.tx[2].data[1] >> 4)), out.tx[2].data));
		CHECK_EQ(out.tx[2].data[1] >> 4, (tick - 1u) % 4u); /* mux 0..3 for 12 cells */
		CHECK_EQ(out.tx[0].data[1] & 0x0Fu, (tick - 1u) & 0x0Fu); /* alive counter */
	}
	bms_can_unpack_status(out.tx[0].data, &s);
	CHECK_EQ(s.state, BMS_ST_ACTIVE);
	CHECK_EQ(s.pack_mv, 45600);
	CHECK(s.main_pos && s.main_neg && !s.fault);
}

/* verifies: BMS-REQ-017 */
BMS_TEST(bms_cycle, test_balancing_in_cycle)
{
	setup();
	in.cell_mv[4] = 3850u;
	cycle(true, false, false);
	cycle(true, false, false);
	CHECK_EQ(out.balance_mask, 1u << 4);
	{
		bms_status_msg_t s;

		bms_can_unpack_status(out.tx[0].data, &s);
		CHECK(s.balancing);
	}
}
