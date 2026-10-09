#include "bms_test.h"
#include "bms/bms.h"

static uint8_t crc_of(const uint8_t *d, uint8_t n)
{
	return (uint8_t)(bms_crc8_j1850(0xFFu, d, n) ^ 0xFFu);
}

/* verifies: BMS-REQ-019 */
BMS_TEST(bms_e2e, test_crc8_j1850_reference_values)
{
	static const uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
	static const uint8_t a[] = {0x00, 0x00, 0x00, 0x00};
	static const uint8_t b[] = {0xF2, 0x01, 0x83};
	static const uint8_t c[] = {0x0F, 0xAA, 0x00, 0x55};
	static const uint8_t d[] = {0x33, 0x22, 0x55, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
	static const uint8_t e[] = {0xFF, 0xFF, 0xFF, 0xFF};

	CHECK_EQ(crc_of(check, 9u), 0x4B);
	CHECK_EQ(crc_of(a, 4u), 0x59);
	CHECK_EQ(crc_of(b, 3u), 0x37);
	CHECK_EQ(crc_of(c, 4u), 0x79);
	CHECK_EQ(crc_of(d, 9u), 0xCB);
	CHECK_EQ(crc_of(e, 4u), 0x74);
}

/* verifies: BMS-REQ-019 */
BMS_TEST(bms_e2e, test_protect_writes_counter_and_crc)
{
	uint8_t f[8] = {0, 0xA0, 1, 2, 3, 4, 5, 6};
	uint8_t cnt = 15u;

	bms_e2e_protect(f, 0x30u, &cnt);
	CHECK_EQ(f[1], 0xAF);  /* upper nibble kept, counter 15 */
	CHECK_EQ(cnt, 0);      /* wraps */
	CHECK_EQ(f[0], bms_e2e_crc(0x30u, f));
	CHECK(bms_e2e_crc(0x31u, f) != f[0]); /* data id is part of the CRC */
}

static bms_e2e_cfg_t ecfg = {0x30u, 2u, 100u};

static void frame(uint8_t f[8], uint8_t counter)
{
	uint8_t k;
	uint8_t c = counter;

	for (k = 0u; k < 8u; k++) {
		f[k] = 0u;
	}
	f[2] = 1u;
	bms_e2e_protect(f, 0x30u, &c);
}

/* verifies: BMS-REQ-020 */
BMS_TEST(bms_e2e, test_rx_sequence_checks)
{
	bms_e2e_rx_t rx;
	uint8_t f[8];

	bms_e2e_rx_init(&rx);
	frame(f, 5u);
	CHECK_EQ(bms_e2e_check(&rx, &ecfg, f, 10u), BMS_E2E_OK);
	frame(f, 6u);
	CHECK_EQ(bms_e2e_check(&rx, &ecfg, f, 10u), BMS_E2E_OK);
	CHECK_EQ(bms_e2e_check(&rx, &ecfg, f, 10u), BMS_E2E_REPEATED);
	frame(f, 8u); /* one frame lost: delta 2 is allowed */
	CHECK_EQ(bms_e2e_check(&rx, &ecfg, f, 10u), BMS_E2E_OK);
	frame(f, 11u); /* delta 3 */
	CHECK_EQ(bms_e2e_check(&rx, &ecfg, f, 10u), BMS_E2E_WRONG_SEQUENCE);
	frame(f, 12u); /* re-synchronised */
	CHECK_EQ(bms_e2e_check(&rx, &ecfg, f, 10u), BMS_E2E_OK);
	frame(f, 15u);
	(void)bms_e2e_check(&rx, &ecfg, f, 10u);
	frame(f, 0u); /* wrap 15 -> 0 */
	CHECK_EQ(bms_e2e_check(&rx, &ecfg, f, 10u), BMS_E2E_OK);
}

/* verifies: BMS-REQ-020 */
BMS_TEST(bms_e2e, test_rx_crc_error_every_bit)
{
	bms_e2e_rx_t rx;
	uint8_t f[8];
	uint8_t byte;
	uint8_t bit;

	for (byte = 0u; byte < 8u; byte++) {
		for (bit = 0u; bit < 8u; bit++) {
			bms_e2e_rx_init(&rx);
			frame(f, 3u);
			f[byte] = (uint8_t)(f[byte] ^ (1u << bit));
			CHECK_EQ(bms_e2e_check(&rx, &ecfg, f, 10u), BMS_E2E_CRC_ERROR);
			CHECK(!rx.have_last);
		}
	}
}

/* verifies: BMS-REQ-021 */
BMS_TEST(bms_e2e, test_rx_timeout_and_frozen_counter)
{
	bms_e2e_rx_t rx;
	uint8_t f[8];
	uint8_t k;

	bms_e2e_rx_init(&rx);
	frame(f, 1u);
	CHECK_EQ(bms_e2e_check(&rx, &ecfg, f, 10u), BMS_E2E_OK);
	for (k = 0u; k < 9u; k++) {
		CHECK_EQ(bms_e2e_check(&rx, &ecfg, NULL, 10u), BMS_E2E_NO_DATA);
	}
	CHECK_EQ(bms_e2e_check(&rx, &ecfg, NULL, 10u), BMS_E2E_TIMEOUT); /* 100 ms */
	/* frozen alive counter: frames keep coming but never count as new */
	bms_e2e_rx_init(&rx);
	CHECK_EQ(bms_e2e_check(&rx, &ecfg, f, 10u), BMS_E2E_OK);
	for (k = 0u; k < 9u; k++) {
		CHECK_EQ(bms_e2e_check(&rx, &ecfg, f, 10u), BMS_E2E_REPEATED);
	}
	CHECK_EQ(bms_e2e_check(&rx, &ecfg, f, 10u), BMS_E2E_TIMEOUT);
	frame(f, 2u);
	CHECK_EQ(bms_e2e_check(&rx, &ecfg, f, 10u), BMS_E2E_OK);
	CHECK_EQ(rx.ms_since_ok, 0);
}
