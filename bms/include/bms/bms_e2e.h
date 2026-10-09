/* End-to-end protection of CAN payloads, in the style of AUTOSAR E2E profile 1/11:
 * CRC-8 SAE J1850 plus a 4-bit alive counter.
 *
 * Frame layout (8 bytes): byte 0 = CRC, byte 1 bits 0..3 = alive counter (0..15,
 * wraps), byte 1 bits 4..7 and bytes 2..7 = payload.
 * CRC = CRC-8 SAE J1850 (poly 0x1D, init 0xFF, final XOR 0xFF) over the data id byte
 * followed by bytes 1..7. The data id is not sent; it binds a payload to its message,
 * so a frame copied to a different CAN id fails the check.
 *
 * Receiver (bms_e2e_check), called once per cycle with the frame or NULL:
 *   NULL frame          -> NO_DATA, or TIMEOUT once no OK frame for >= timeout_ms
 *   CRC mismatch        -> CRC_ERROR (counter state unchanged)
 *   first frame         -> OK (counter accepted)
 *   delta == 0          -> REPEATED (alive counter frozen)
 *   delta > max_delta   -> WRONG_SEQUENCE (frames lost); counter re-synchronised
 *   otherwise           -> OK            delta = (counter - last) mod 16
 * Only OK resets the timeout timer, so a frozen counter or a stream of bad CRCs ends
 * in TIMEOUT as well.
 */
#ifndef BMS_E2E_H
#define BMS_E2E_H

#include "bms/bms_types.h"

typedef enum {
	BMS_E2E_OK = 0,
	BMS_E2E_NO_DATA = 1,
	BMS_E2E_REPEATED = 2,
	BMS_E2E_WRONG_SEQUENCE = 3,
	BMS_E2E_CRC_ERROR = 4,
	BMS_E2E_TIMEOUT = 5
} bms_e2e_status_t;

typedef struct {
	uint8_t data_id;
	uint8_t max_delta;  /* >= 1 */
	uint16_t timeout_ms;
} bms_e2e_cfg_t;

typedef struct {
	uint8_t last_counter;
	bool have_last;
	uint16_t ms_since_ok;
} bms_e2e_rx_t;

/* CRC-8 SAE J1850 of len bytes, starting value crc (use 0xFF for a fresh CRC; the
 * final XOR with 0xFF is NOT applied, see bms_e2e_crc). Check value: the CRC of
 * "123456789" with init 0xFF and final XOR 0xFF is 0x4B. */
uint8_t bms_crc8_j1850(uint8_t crc, const uint8_t *data, uint8_t len);

/* CRC of a frame as defined above. */
uint8_t bms_e2e_crc(uint8_t data_id, const uint8_t frame[8]);

/* Writes *counter into byte 1 (low nibble), the CRC into byte 0, then increments
 * *counter modulo 16. */
void bms_e2e_protect(uint8_t frame[8], uint8_t data_id, uint8_t *counter);

void bms_e2e_rx_init(bms_e2e_rx_t *rx);

bms_e2e_status_t bms_e2e_check(bms_e2e_rx_t *rx, const bms_e2e_cfg_t *cfg, const uint8_t *frame,
			       uint16_t elapsed_ms);

#endif
