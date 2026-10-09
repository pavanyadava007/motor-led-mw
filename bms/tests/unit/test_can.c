#include "bms_test.h"
#include "bms/bms.h"

/* verifies: BMS-REQ-018 */
BMS_TEST(bms_can, test_status_round_trip)
{
	bms_status_msg_t s = {3u, 44123u, -12345, 873u, true, true, false, true, false, true};
	bms_status_msg_t r;
	bms_frame_t f;
	uint8_t cnt = 7u;

	bms_can_pack_status(&f, &s, &cnt);
	CHECK_EQ(f.id, BMS_CAN_ID_STATUS);
	CHECK_EQ(f.data[0], bms_e2e_crc(BMS_DID_STATUS, f.data));
	CHECK_EQ(f.data[1], 0x37);
	bms_can_unpack_status(f.data, &r);
	CHECK_EQ(r.state, 3);
	CHECK_EQ(r.pack_mv, 44100);       /* 0.1 V resolution */
	CHECK_EQ(r.current_ma, -12300);   /* 0.1 A, truncated toward zero */
	CHECK_EQ(r.soc_permille, 870);    /* 0.5 % resolution */
	CHECK(r.main_pos && r.main_neg && !r.precharge && r.balancing && !r.fault && r.comm_loss);
}

/* verifies: BMS-REQ-018 */
BMS_TEST(bms_can, test_status_saturation)
{
	bms_status_msg_t s = {1u, 9000000u, 5000000, 1200u, false, false, false, false, true, false};
	bms_status_msg_t r;
	bms_frame_t f;
	uint8_t cnt = 0u;

	bms_can_pack_status(&f, &s, &cnt);
	bms_can_unpack_status(f.data, &r);
	CHECK_EQ(r.pack_mv, 6553500);
	CHECK_EQ(r.current_ma, 3276700);
	CHECK_EQ(r.soc_permille, 1000);
	s.current_ma = -5000000;
	bms_can_pack_status(&f, &s, &cnt);
	bms_can_unpack_status(f.data, &r);
	CHECK_EQ(r.current_ma, -3276800);
	CHECK(r.fault);
}

/* verifies: BMS-REQ-018 */
BMS_TEST(bms_can, test_faults_frame)
{
	bms_faults_msg_t m = {0x80000201u, 655, -450};
	bms_frame_t f;
	uint8_t cnt = 0u;

	bms_can_pack_faults(&f, &m, &cnt);
	CHECK_EQ(f.id, BMS_CAN_ID_FAULTS);
	CHECK_EQ(f.data[2], 0x01);
	CHECK_EQ(f.data[3], 0x02);
	CHECK_EQ(f.data[5], 0x80);
	CHECK_EQ(f.data[6], 105); /* 65 degC + 40 */
	CHECK_EQ(f.data[7], 0);   /* -45 degC saturates at -40 */
	CHECK_EQ(f.data[0], bms_e2e_crc(BMS_DID_FAULTS, f.data));
}

/* verifies: BMS-REQ-018 */
BMS_TEST(bms_can, test_cell_voltage_mux)
{
	uint16_t v[BMS_MAX_CELLS];
	bms_frame_t f;
	uint8_t cnt = 0u;
	uint8_t k;

	for (k = 0u; k < BMS_MAX_CELLS; k++) {
		v[k] = (uint16_t)(3000u + k);
	}
	bms_can_pack_cellv(&f, 3u, v, 11u, &cnt);
	CHECK_EQ(f.id, BMS_CAN_ID_CELLV);
	CHECK_EQ(f.data[1] >> 4, 3);
	CHECK_EQ(f.data[2] | (f.data[3] << 8), 3009);
	CHECK_EQ(f.data[4] | (f.data[5] << 8), 3010);
	CHECK_EQ(f.data[6] | (f.data[7] << 8), 0xFFFF); /* cell 11 does not exist */
	CHECK_EQ(f.data[0], bms_e2e_crc((uint8_t)(BMS_DID_CELLV + 3u), f.data));
}

/* verifies: BMS-REQ-018 */
BMS_TEST(bms_can, test_vcu_cmd_round_trip)
{
	bms_vcu_cmd_t c = {true, true};
	bms_vcu_cmd_t r = {false, false};
	bms_frame_t f;
	uint8_t cnt = 0u;

	bms_can_pack_vcu_cmd(&f, &c, &cnt);
	bms_can_unpack_vcu_cmd(f.data, &r);
	CHECK(r.close_req && r.reset_req);
	CHECK_EQ(f.data[2], 3);
	CHECK_EQ(cnt, 1);
}
