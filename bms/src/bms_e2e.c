/* CRC-8 SAE J1850 and E2E protection, see bms/bms_e2e.h. */
#include "bms/bms_e2e.h"

#ifdef BMS_CRC_BITWISE
/* Bit-serial reference version, kept for the profiling comparison (-DBMS_CRC_BITWISE). */
uint8_t bms_crc8_j1850(uint8_t crc, const uint8_t *data, uint8_t len)
{
	uint8_t c = crc;
	uint8_t i;
	uint8_t b;

	for (i = 0u; i < len; i++) {
		c ^= data[i];
		for (b = 0u; b < 8u; b++) {
			if ((c & 0x80u) != 0u) {
				c = (uint8_t)((uint8_t)(c << 1) ^ 0x1Du);
			} else {
				c = (uint8_t)(c << 1);
			}
		}
	}
	return c;
}
#else
/* Nibble table: CRC of the 4-bit value n shifted into the top of the register
 * (16 bytes of ROM, 2 lookups per byte). Profiling on the emulated Cortex-M4F showed the
 * bit-serial loop at 38 % of the BMS cycle (results/bms/profile_crc_bitwise.json). */
static const uint8_t crc_nibble[16] = {0x00u, 0x1Du, 0x3Au, 0x27u, 0x74u, 0x69u, 0x4Eu, 0x53u, 0xE8u, 0xF5u, 0xD2u, 0xCFu, 0x9Cu, 0x81u, 0xA6u, 0xBBu};

uint8_t bms_crc8_j1850(uint8_t crc, const uint8_t *data, uint8_t len)
{
	uint8_t c = crc;
	uint8_t i;

	for (i = 0u; i < len; i++) {
		c ^= data[i];
		c = (uint8_t)((uint8_t)(c << 4) ^ crc_nibble[c >> 4]);
		c = (uint8_t)((uint8_t)(c << 4) ^ crc_nibble[c >> 4]);
	}
	return c;
}
#endif

uint8_t bms_e2e_crc(uint8_t data_id, const uint8_t frame[8])
{
	uint8_t c = bms_crc8_j1850(0xFFu, &data_id, 1u);

	c = bms_crc8_j1850(c, &frame[1], 7u);
	return (uint8_t)(c ^ 0xFFu);
}

void bms_e2e_protect(uint8_t frame[8], uint8_t data_id, uint8_t *counter)
{
	frame[1] = (uint8_t)((frame[1] & 0xF0u) | (*counter & 0x0Fu));
	frame[0] = bms_e2e_crc(data_id, frame);
	*counter = (uint8_t)((*counter + 1u) & 0x0Fu);
}

void bms_e2e_rx_init(bms_e2e_rx_t *rx)
{
	rx->last_counter = 0u;
	rx->have_last = false;
	rx->ms_since_ok = 0u;
}

static void age(bms_e2e_rx_t *rx, uint16_t elapsed_ms)
{
	uint32_t t = (uint32_t)rx->ms_since_ok + elapsed_ms;

	rx->ms_since_ok = (t > 0xFFFFu) ? 0xFFFFu : (uint16_t)t;
}

bms_e2e_status_t bms_e2e_check(bms_e2e_rx_t *rx, const bms_e2e_cfg_t *cfg, const uint8_t *frame,
			       uint16_t elapsed_ms)
{
	bms_e2e_status_t st;

	if (frame == NULL) {
		age(rx, elapsed_ms);
		st = BMS_E2E_NO_DATA;
	} else if (bms_e2e_crc(cfg->data_id, frame) != frame[0]) {
		age(rx, elapsed_ms);
		st = BMS_E2E_CRC_ERROR;
	} else {
		uint8_t cnt = (uint8_t)(frame[1] & 0x0Fu);
		uint8_t delta = (uint8_t)((uint8_t)(cnt - rx->last_counter) & 0x0Fu);

		if (!rx->have_last) {
			st = BMS_E2E_OK;
		} else if (delta == 0u) {
			st = BMS_E2E_REPEATED;
		} else if (delta > cfg->max_delta) {
			st = BMS_E2E_WRONG_SEQUENCE;
		} else {
			st = BMS_E2E_OK;
		}
		if (st != BMS_E2E_REPEATED) {
			rx->last_counter = cnt;
			rx->have_last = true;
		}
		if (st == BMS_E2E_OK) {
			rx->ms_since_ok = 0u;
		} else {
			age(rx, elapsed_ms);
		}
	}
	if ((st != BMS_E2E_OK) && (rx->ms_since_ok >= cfg->timeout_ms)) {
		st = BMS_E2E_TIMEOUT;
	}
	return st;
}
