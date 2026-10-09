/* CAN messages of the BMS (classic CAN, 8 bytes, Intel/little-endian signals).
 * bms/dbc/bms.dbc describes the same layout; tests decode the packed frames with the
 * DBC (cantools) to check that both agree.
 *
 * 0x100 BMS_Status   every cycle, data id 0x10
 *   b0 CRC | b1[3:0] alive | b1[7:4] state | b2..3 pack voltage, uint16, 0.1 V
 *   b4..5 pack current, int16, 0.1 A (+ = charge) | b6 SoC, uint8, 0.5 %
 *   b7 bit0 main_pos, bit1 main_neg, bit2 precharge, bit3 balancing, bit4 fault, bit5 comm loss
 * 0x101 BMS_Faults   every cycle, data id 0x11
 *   b0 CRC | b1[3:0] alive | b2..5 latched faults uint32 | b6 max temp | b7 min temp
 *   (temperatures: uint8, 1 degC, offset -40)
 * 0x110 BMS_CellV    one frame per cycle, multiplexed, data id 0x20 + mux
 *   b0 CRC | b1[3:0] alive | b1[7:4] mux m | b2..3, b4..5, b6..7 cell 3m, 3m+1, 3m+2 in mV
 *   (unused slots carry 0xFFFF)
 * 0x200 VCU_Cmd      received, data id 0x30
 *   b0 CRC | b1[3:0] alive | b2 bit0 close request, bit1 fault reset request
 * Physical values are saturated to the signal range before scaling.
 */
#ifndef BMS_CAN_H
#define BMS_CAN_H

#include "bms/bms_types.h"

#define BMS_CAN_ID_STATUS 0x100u
#define BMS_CAN_ID_FAULTS 0x101u
#define BMS_CAN_ID_CELLV 0x110u
#define BMS_CAN_ID_VCU_CMD 0x200u
#define BMS_DID_STATUS 0x10u
#define BMS_DID_FAULTS 0x11u
#define BMS_DID_CELLV 0x20u
#define BMS_DID_VCU_CMD 0x30u

typedef struct {
	uint32_t id;
	uint8_t data[8];
} bms_frame_t;

typedef struct {
	uint8_t state;
	uint32_t pack_mv;
	int32_t current_ma;
	uint16_t soc_permille;  /* 0..1000 */
	bool main_pos;
	bool main_neg;
	bool precharge;
	bool balancing;
	bool fault;
	bool comm_loss;
} bms_status_msg_t;

typedef struct {
	uint32_t faults;
	int16_t t_max_dc;
	int16_t t_min_dc;
} bms_faults_msg_t;

typedef struct {
	bool close_req;
	bool reset_req;
} bms_vcu_cmd_t;

/* Packers fill f (id + payload) and apply E2E protection with *counter. */
void bms_can_pack_status(bms_frame_t *f, const bms_status_msg_t *s, uint8_t *counter);
void bms_can_pack_faults(bms_frame_t *f, const bms_faults_msg_t *m, uint8_t *counter);
/* mux = frame index; cells 3*mux .. 3*mux+2 from cell_mv[0..n_cells-1]. */
void bms_can_pack_cellv(bms_frame_t *f, uint8_t mux, const uint16_t *cell_mv, uint8_t n_cells,
			uint8_t *counter);
void bms_can_pack_vcu_cmd(bms_frame_t *f, const bms_vcu_cmd_t *c, uint8_t *counter);

/* Unpackers decode the payload only (no E2E check). */
void bms_can_unpack_status(const uint8_t d[8], bms_status_msg_t *s);
void bms_can_unpack_vcu_cmd(const uint8_t d[8], bms_vcu_cmd_t *c);

#endif
