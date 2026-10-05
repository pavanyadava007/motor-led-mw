#include "mlmw/lin.h"

uint8_t mlmw_lin_pid(uint8_t id)
{
	uint8_t i = id & 0x3Fu;
	uint8_t p0 = ((i >> 0) ^ (i >> 1) ^ (i >> 2) ^ (i >> 4)) & 1u;
	uint8_t p1 = (~((i >> 1) ^ (i >> 3) ^ (i >> 4) ^ (i >> 5))) & 1u;

	return (uint8_t)(i | (p0 << 6) | (p1 << 7));
}

bool mlmw_lin_pid_valid(uint8_t pid)
{
	return mlmw_lin_pid(pid & 0x3Fu) == pid;
}

uint8_t mlmw_lin_checksum(uint8_t pid, const uint8_t *data, size_t len, bool enhanced)
{
	uint16_t sum = 0;
	uint8_t id = pid & 0x3Fu;

	if (enhanced && id != MLMW_LIN_ID_MASTER_REQ && id != MLMW_LIN_ID_SLAVE_RESP) {
		sum = pid;
	}
	for (size_t k = 0; k < len; k++) {
		sum += data[k];
		if (sum > 0xFFu) {
			sum = (uint16_t)(sum - 0xFFu);
		}
	}
	return (uint8_t)(~sum);
}
