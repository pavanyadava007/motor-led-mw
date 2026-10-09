/* CAN message packing, see bms/bms_can.h. */
#include "bms/bms_can.h"
#include "bms/bms_e2e.h"

static void clear(bms_frame_t *f, uint32_t id)
{
	uint8_t k;

	f->id = id;
	for (k = 0u; k < 8u; k++) {
		f->data[k] = 0u;
	}
}

static void put_u16(uint8_t *d, uint16_t v)
{
	d[0] = (uint8_t)(v & 0xFFu);
	d[1] = (uint8_t)(v >> 8);
}

static uint16_t get_u16(const uint8_t *d)
{
	return (uint16_t)((uint16_t)d[0] | (uint16_t)((uint16_t)d[1] << 8));
}

static uint8_t temp_byte(int16_t t_dc)
{
	int32_t c = ((int32_t)t_dc / 10) + 40;

	if (c < 0) {
		c = 0;
	}
	if (c > 255) {
		c = 255;
	}
	return (uint8_t)c;
}

void bms_can_pack_status(bms_frame_t *f, const bms_status_msg_t *s, uint8_t *counter)
{
	uint32_t dv = s->pack_mv / 100u;
	int32_t da = s->current_ma / 100;
	uint32_t soc = s->soc_permille / 5u;
	uint8_t bits = 0u;

	clear(f, BMS_CAN_ID_STATUS);
	if (dv > 0xFFFFu) {
		dv = 0xFFFFu;
	}
	if (da > 32767) {
		da = 32767;
	}
	if (da < -32768) {
		da = -32768;
	}
	if (soc > 200u) {
		soc = 200u;
	}
	f->data[1] = (uint8_t)((s->state & 0x0Fu) << 4);
	put_u16(&f->data[2], (uint16_t)dv);
	put_u16(&f->data[4], (uint16_t)(int16_t)da);
	f->data[6] = (uint8_t)soc;
	bits |= s->main_pos ? 0x01u : 0u;
	bits |= s->main_neg ? 0x02u : 0u;
	bits |= s->precharge ? 0x04u : 0u;
	bits |= s->balancing ? 0x08u : 0u;
	bits |= s->fault ? 0x10u : 0u;
	bits |= s->comm_loss ? 0x20u : 0u;
	f->data[7] = bits;
	bms_e2e_protect(f->data, BMS_DID_STATUS, counter);
}

void bms_can_pack_faults(bms_frame_t *f, const bms_faults_msg_t *m, uint8_t *counter)
{
	clear(f, BMS_CAN_ID_FAULTS);
	f->data[2] = (uint8_t)(m->faults & 0xFFu);
	f->data[3] = (uint8_t)((m->faults >> 8) & 0xFFu);
	f->data[4] = (uint8_t)((m->faults >> 16) & 0xFFu);
	f->data[5] = (uint8_t)((m->faults >> 24) & 0xFFu);
	f->data[6] = temp_byte(m->t_max_dc);
	f->data[7] = temp_byte(m->t_min_dc);
	bms_e2e_protect(f->data, BMS_DID_FAULTS, counter);
}

void bms_can_pack_cellv(bms_frame_t *f, uint8_t mux, const uint16_t *cell_mv, uint8_t n_cells,
			uint8_t *counter)
{
	uint8_t k;

	clear(f, BMS_CAN_ID_CELLV);
	f->data[1] = (uint8_t)((mux & 0x0Fu) << 4);
	for (k = 0u; k < 3u; k++) {
		uint32_t idx = ((uint32_t)mux * 3u) + k;
		uint16_t v = (idx < n_cells) ? cell_mv[idx] : 0xFFFFu;

		put_u16(&f->data[2u + (2u * k)], v);
	}
	bms_e2e_protect(f->data, (uint8_t)(BMS_DID_CELLV + (mux & 0x0Fu)), counter);
}

void bms_can_pack_vcu_cmd(bms_frame_t *f, const bms_vcu_cmd_t *c, uint8_t *counter)
{
	clear(f, BMS_CAN_ID_VCU_CMD);
	f->data[2] = (uint8_t)((c->close_req ? 0x01u : 0u) | (c->reset_req ? 0x02u : 0u));
	bms_e2e_protect(f->data, BMS_DID_VCU_CMD, counter);
}

void bms_can_unpack_status(const uint8_t d[8], bms_status_msg_t *s)
{
	s->state = (uint8_t)(d[1] >> 4);
	s->pack_mv = (uint32_t)get_u16(&d[2]) * 100u;
	s->current_ma = (int32_t)(int16_t)get_u16(&d[4]) * 100;
	s->soc_permille = (uint16_t)((uint16_t)d[6] * 5u);
	s->main_pos = (d[7] & 0x01u) != 0u;
	s->main_neg = (d[7] & 0x02u) != 0u;
	s->precharge = (d[7] & 0x04u) != 0u;
	s->balancing = (d[7] & 0x08u) != 0u;
	s->fault = (d[7] & 0x10u) != 0u;
	s->comm_loss = (d[7] & 0x20u) != 0u;
}

void bms_can_unpack_vcu_cmd(const uint8_t d[8], bms_vcu_cmd_t *c)
{
	c->close_req = (d[2] & 0x01u) != 0u;
	c->reset_req = (d[2] & 0x02u) != 0u;
}
