#include "mlmw/cmd.h"

void mlmw_cmd_init(mlmw_cmd_ctx_t *c, mlmw_mac_verify_fn verify,
		   const mlmw_cmd_handlers_t *h, void *user, int16_t max_rpm)
{
	c->verify = verify;
	c->h = h;
	c->user = user;
	c->max_rpm = max_rpm;
	c->last_seq = 0;
	c->have_seq = 0;
}

static int dispatch(mlmw_cmd_ctx_t *c, uint8_t cmd, const uint8_t *p, uint8_t len)
{
	const mlmw_cmd_handlers_t *h = c->h;

	switch (cmd) {
	case MLMW_CMD_SET_SPEED: {
		if (len != 2u) {
			return MLMW_ERR_ARG;
		}
		int16_t rpm = (int16_t)(((uint16_t)p[0] << 8) | p[1]);

		if (rpm > c->max_rpm || rpm < -c->max_rpm) {
			return MLMW_ERR_ARG;
		}
		if (h && h->set_speed) {
			h->set_speed(c->user, rpm);
		}
		return MLMW_OK;
	}
	case MLMW_CMD_STOP:
		if (len != 0u) {
			return MLMW_ERR_ARG;
		}
		if (h && h->stop) {
			h->stop(c->user);
		}
		return MLMW_OK;
	case MLMW_CMD_SET_LED:
		if (len != 4u) {
			return MLMW_ERR_ARG;
		}
		if (h && h->set_led) {
			h->set_led(c->user, p[0], p[1], p[2], p[3]);
		}
		return MLMW_OK;
	case MLMW_CMD_CLEAR_FAULT:
		if (len != 0u) {
			return MLMW_ERR_ARG;
		}
		if (h && h->clear_fault) {
			h->clear_fault(c->user);
		}
		return MLMW_OK;
	default:
		return MLMW_ERR_CMD;
	}
}

int mlmw_cmd_process(mlmw_cmd_ctx_t *c, const uint8_t *buf, size_t n)
{
	if (c == NULL || buf == NULL || n < MLMW_CMD_HDR + MLMW_CMD_TAG) {
		return MLMW_ERR_LEN;
	}
	uint8_t len = buf[4];

	if (n != (size_t)MLMW_CMD_HDR + len + MLMW_CMD_TAG) {
		return MLMW_ERR_LEN;
	}
	if (buf[0] != MLMW_CMD_VERSION) {
		return MLMW_ERR_VER;
	}
	if (c->verify == NULL ||
	    c->verify(c->user, buf, MLMW_CMD_HDR + len, &buf[MLMW_CMD_HDR + len]) != 0) {
		return MLMW_ERR_MAC;
	}
	uint16_t seq = (uint16_t)(((uint16_t)buf[1] << 8) | buf[2]);

	if (c->have_seq && seq <= c->last_seq) {
		return MLMW_ERR_REPLAY;
	}
	c->last_seq = seq;
	c->have_seq = 1;
	return dispatch(c, buf[3], &buf[MLMW_CMD_HDR], len);
}
