#include "bms_test.h"
#include "bms/bms.h"

static bms_cfg_t cfg;
static uint16_t v[BMS_MAX_CELLS];

static void setup(void)
{
	uint8_t k;

	bms_default_cfg(&cfg);
	for (k = 0u; k < BMS_MAX_CELLS; k++) {
		v[k] = 3900u;
	}
}

/* verifies: BMS-REQ-017 */
BMS_TEST(bms_balance, test_start_threshold_and_hysteresis)
{
	uint16_t mask;

	setup();
	v[3] = 3919u; /* 19 mV above min: below the start threshold */
	CHECK_EQ(bms_balance(0u, &cfg.bal, v, 12u, 0, true), 0);
	v[3] = 3920u;
	mask = bms_balance(0u, &cfg.bal, v, 12u, 0, true);
	CHECK_EQ(mask, 1u << 3);
	v[3] = 3906u; /* inside the band: stays on */
	mask = bms_balance(mask, &cfg.bal, v, 12u, 0, true);
	CHECK_EQ(mask, 1u << 3);
	v[3] = 3905u; /* at the stop threshold: off */
	mask = bms_balance(mask, &cfg.bal, v, 12u, 0, true);
	CHECK_EQ(mask, 0);
	v[3] = 3910u; /* off and below start: stays off */
	CHECK_EQ(bms_balance(mask, &cfg.bal, v, 12u, 0, true), 0);
}

/* verifies: BMS-REQ-017 */
BMS_TEST(bms_balance, test_inhibits)
{
	uint8_t k;

	setup();
	v[0] = 3950u;
	CHECK_EQ(bms_balance(0u, &cfg.bal, v, 12u, 0, false), 0);     /* fault */
	CHECK_EQ(bms_balance(0u, &cfg.bal, v, 12u, -501, true), 0);   /* discharging */
	CHECK_EQ(bms_balance(0u, &cfg.bal, v, 12u, -500, true), 1);   /* light load ok */
	CHECK_EQ(bms_balance(0u, &cfg.bal, v, 12u, 5000, true), 1);   /* charging ok */
	for (k = 0u; k < BMS_MAX_CELLS; k++) {
		v[k] = 3580u;
	}
	v[1] = 3605u; /* 25 mV above min and above 3.6 V */
	v[2] = 3599u; /* 19 mV above min: below start */
	CHECK_EQ(bms_balance(0u, &cfg.bal, v, 12u, 0, true), 1u << 1);
	v[1] = 3599u; /* below the 3.6 V floor */
	v[2] = 3620u;
	CHECK_EQ(bms_balance(0u, &cfg.bal, v, 12u, 0, true), 1u << 2);
}

/* verifies: BMS-REQ-017 */
BMS_TEST(bms_balance, test_max_bleeders_and_priority)
{
	uint16_t mask;

	setup();
	v[1] = 3950u;
	v[2] = 3950u;
	v[5] = 3950u;
	v[7] = 3950u;
	v[9] = 3950u;
	v[10] = 3950u;
	mask = bms_balance(0u, &cfg.bal, v, 12u, 0, true);
	CHECK_EQ(mask, (1u << 1) | (1u << 2) | (1u << 5) | (1u << 7));
	/* cells 9 and 10 keep priority once they are on */
	mask = bms_balance((uint16_t)((1u << 9) | (1u << 10)), &cfg.bal, v, 12u, 0, true);
	CHECK_EQ(mask, (1u << 9) | (1u << 10) | (1u << 1) | (1u << 2));
	/* cells beyond n_cells are ignored */
	setup();
	v[12] = 4000u;
	CHECK_EQ(bms_balance(0u, &cfg.bal, v, 12u, 0, true), 0);
}
