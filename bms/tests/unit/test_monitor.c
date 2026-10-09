#include "bms_test.h"
#include "bms/bms.h"

static bms_cfg_t cfg;
static bms_mon_t mon;
static uint16_t cells[BMS_MAX_CELLS];
static int16_t temps[BMS_MAX_TEMPS];

static void setup(void)
{
	uint8_t k;

	bms_default_cfg(&cfg);
	bms_mon_init(&mon);
	for (k = 0u; k < BMS_MAX_CELLS; k++) {
		cells[k] = (uint16_t)(3700u + k); /* distinct values, so no stuck detection */
	}
	for (k = 0u; k < BMS_MAX_TEMPS; k++) {
		temps[k] = 250;
	}
}

static bms_faults_t step(int32_t ma)
{
	uint8_t k;

	/* move every cell by 1 mV per sample like a real ADC under load */
	for (k = 0u; k < BMS_MAX_CELLS; k++) {
		cells[k] = (uint16_t)(cells[k] ^ 1u);
	}
	return bms_mon_step(&mon, &cfg.mon, cells, temps, ma);
}

/* verifies: BMS-REQ-001, BMS-REQ-005 */
BMS_TEST(bms_monitor, test_ov_latches_on_third_sample)
{
	setup();
	CHECK_EQ(step(0), 0);
	cells[5] = 4202u;
	CHECK_EQ(step(0) & BMS_F_CELL_OV, 0);
	CHECK_EQ(step(0) & BMS_F_CELL_OV, 0);
	CHECK_EQ(step(0) & BMS_F_CELL_OV, BMS_F_CELL_OV);
}

/* verifies: BMS-REQ-001 */
BMS_TEST(bms_monitor, test_ov_threshold_is_strict)
{
	uint8_t k;

	setup();
	for (k = 0u; k < 10u; k++) {
		cells[0] = (uint16_t)(4200u - (k & 1u)); /* 4200 / 4199: at the limit, not above */
		CHECK_EQ(bms_mon_step(&mon, &cfg.mon, cells, temps, 0) & BMS_F_CELL_OV, 0);
	}
}

/* verifies: BMS-REQ-005 */
BMS_TEST(bms_monitor, test_debounce_resets_on_good_sample)
{
	setup();
	cells[2] = 4250u;
	(void)step(0);
	(void)step(0);
	cells[2] = 4100u;
	(void)step(0);
	cells[2] = 4250u;
	(void)step(0);
	CHECK_EQ(step(0) & BMS_F_CELL_OV, 0);
	CHECK_EQ(step(0) & BMS_F_CELL_OV, BMS_F_CELL_OV);
}

/* verifies: BMS-REQ-002 */
BMS_TEST(bms_monitor, test_uv_latches)
{
	setup();
	cells[11] = 2790u;
	(void)step(0);
	(void)step(0);
	CHECK_EQ(step(0) & BMS_F_CELL_UV, BMS_F_CELL_UV);
	CHECK_EQ(mon.latched & BMS_F_CELL_OV, 0);
}

/* verifies: BMS-REQ-002 */
BMS_TEST(bms_monitor, test_uv_threshold_is_strict)
{
	uint8_t k;

	setup();
	for (k = 0u; k < 6u; k++) {
		cells[11] = (uint16_t)(2800u + (k & 1u));
		CHECK_EQ(bms_mon_step(&mon, &cfg.mon, cells, temps, 0) & BMS_F_CELL_UV, 0);
	}
}

/* verifies: BMS-REQ-003 */
BMS_TEST(bms_monitor, test_over_and_under_temperature)
{
	setup();
	temps[1] = 601;
	(void)step(0);
	(void)step(0);
	CHECK_EQ(step(0) & BMS_F_CELL_OT, BMS_F_CELL_OT);
	CHECK_EQ(mon.t_max_dc, 601);
	setup();
	temps[3] = -201;
	(void)step(0);
	(void)step(0);
	CHECK_EQ(step(0) & BMS_F_CELL_UT, BMS_F_CELL_UT);
	CHECK_EQ(mon.t_min_dc, -201);
	setup();
	temps[0] = 600;
	temps[1] = -200;
	(void)step(0);
	(void)step(0);
	(void)step(0);
	CHECK_EQ(step(0), 0);
}

/* verifies: BMS-REQ-004 */
BMS_TEST(bms_monitor, test_discharge_over_current)
{
	setup();
	CHECK_EQ(step(-20000), 0);
	CHECK_EQ(step(-20000), 0);
	CHECK_EQ(step(-20000), 0);
	(void)step(-20001);
	(void)step(-20001);
	CHECK_EQ(step(-20001), BMS_F_OC_DISCHARGE);
}

/* verifies: BMS-REQ-004 */
BMS_TEST(bms_monitor, test_charge_over_current)
{
	setup();
	CHECK_EQ(step(6000), 0);
	(void)step(6001);
	(void)step(6001);
	CHECK_EQ(step(6001), BMS_F_OC_CHARGE);
}

/* verifies: BMS-REQ-006 */
BMS_TEST(bms_monitor, test_fault_stays_latched_until_clear)
{
	uint8_t k;

	setup();
	cells[0] = 4300u;
	(void)step(0);
	(void)step(0);
	(void)step(0);
	CHECK(bms_mon_clear(&mon) == false); /* condition still active */
	CHECK_EQ(mon.latched, BMS_F_CELL_OV);
	cells[0] = 4100u;
	for (k = 0u; k < 20u; k++) {
		CHECK_EQ(step(0), BMS_F_CELL_OV);
	}
	CHECK(bms_mon_clear(&mon));
	CHECK_EQ(mon.latched, 0);
	CHECK_EQ(step(0), 0);
}

/* verifies: BMS-REQ-007 */
BMS_TEST(bms_monitor, test_open_wire_is_sensor_fault_not_uv)
{
	setup();
	cells[4] = 0u; /* broken sense wire reads ~0 V */
	(void)step(0);
	(void)step(0);
	CHECK_EQ(step(0), BMS_F_OPEN_WIRE);
	CHECK_EQ(mon.v_min_mv, 3700); /* the broken channel is not used for min/max */
	setup();
	cells[7] = 6553u; /* open wire pulled up */
	(void)step(0);
	(void)step(0);
	CHECK_EQ(step(0), BMS_F_OPEN_WIRE);
}

/* verifies: BMS-REQ-008 */
BMS_TEST(bms_monitor, test_stuck_cell_voltage_under_load)
{
	uint8_t k;

	setup();
	for (k = 0u; k < 99u; k++) {
		CHECK_EQ(bms_mon_step(&mon, &cfg.mon, cells, temps, -5000), 0);
	}
	/* 100th identical sample under 5 A: the first sample has no predecessor, so the
	 * counter reaches 100 on the 101st call */
	CHECK_EQ(bms_mon_step(&mon, &cfg.mon, cells, temps, -5000), 0);
	CHECK_EQ(bms_mon_step(&mon, &cfg.mon, cells, temps, -5000), BMS_F_SENSOR_STUCK);
}

/* verifies: BMS-REQ-008 */
BMS_TEST(bms_monitor, test_constant_voltage_at_rest_is_not_stuck)
{
	uint16_t k;

	setup();
	for (k = 0u; k < 300u; k++) {
		CHECK_EQ(bms_mon_step(&mon, &cfg.mon, cells, temps, 999), 0);
	}
}

/* verifies: BMS-REQ-009 */
BMS_TEST(bms_monitor, test_temperature_sensor_plausibility)
{
	setup();
	temps[2] = 1300; /* shorted NTC: implausible, not an over-temperature */
	(void)step(0);
	(void)step(0);
	CHECK_EQ(step(0), BMS_F_TEMP_SENSOR);
	setup();
	temps[2] = -500;
	(void)step(0);
	(void)step(0);
	CHECK_EQ(step(0), BMS_F_TEMP_SENSOR);
}
