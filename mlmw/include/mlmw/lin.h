/* LIN 2.x helpers: protected identifier and frame checksum. */
#ifndef MLMW_LIN_H
#define MLMW_LIN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MLMW_LIN_ID_MASTER_REQ 0x3Cu
#define MLMW_LIN_ID_SLAVE_RESP 0x3Du

/* Protected identifier for a 6-bit frame id (bits above 5 are ignored).
 * P0 = ID0 ^ ID1 ^ ID2 ^ ID4, P1 = !(ID1 ^ ID3 ^ ID4 ^ ID5). */
uint8_t mlmw_lin_pid(uint8_t id);

/* True if both parity bits of a received PID are correct. */
bool mlmw_lin_pid_valid(uint8_t pid);

/* Checksum byte: 8-bit sum with carry wrap-around, inverted.
 * enhanced = include the PID (LIN 2.x). Diagnostic frames 0x3C and 0x3D
 * always use the classic checksum, whatever 'enhanced' says. */
uint8_t mlmw_lin_checksum(uint8_t pid, const uint8_t *data, size_t len, bool enhanced);

#endif
