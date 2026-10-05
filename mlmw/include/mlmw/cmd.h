/* Authenticated command frames for a motor + LED node.
 *
 * Frame: ver(1) | seq(2, big endian) | cmd(1) | len(1) | payload(len) | tag(8)
 * The tag covers every byte before it. The middleware does no crypto itself:
 * the platform passes a verify callback (on the demo target it calls PSA
 * Crypto, so the key stays in the TrustZone secure side).
 */
#ifndef MLMW_CMD_H
#define MLMW_CMD_H

#include <stddef.h>
#include <stdint.h>

#define MLMW_CMD_VERSION 1u
#define MLMW_CMD_HDR 5u
#define MLMW_CMD_TAG 8u

enum {
	MLMW_CMD_SET_SPEED = 0x01,   /* payload: int16 rpm, big endian */
	MLMW_CMD_STOP = 0x02,        /* payload: none */
	MLMW_CMD_SET_LED = 0x10,     /* payload: r, g, b, brightness */
	MLMW_CMD_CLEAR_FAULT = 0x20, /* payload: none */
};

enum {
	MLMW_OK = 0,
	MLMW_ERR_LEN = -1,    /* too short, or len field does not match the buffer */
	MLMW_ERR_VER = -2,    /* unknown version */
	MLMW_ERR_MAC = -3,    /* tag check failed */
	MLMW_ERR_REPLAY = -4, /* seq not greater than the last accepted one */
	MLMW_ERR_CMD = -5,    /* unknown command */
	MLMW_ERR_ARG = -6,    /* payload length or value out of range */
};

typedef int (*mlmw_mac_verify_fn)(void *user, const uint8_t *msg, size_t len,
				  const uint8_t tag[MLMW_CMD_TAG]);

typedef struct {
	void (*set_speed)(void *user, int16_t rpm);
	void (*stop)(void *user);
	void (*set_led)(void *user, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);
	void (*clear_fault)(void *user);
} mlmw_cmd_handlers_t;

typedef struct {
	mlmw_mac_verify_fn verify; /* returns 0 when the tag is valid */
	const mlmw_cmd_handlers_t *h;
	void *user;
	int16_t max_rpm;           /* |rpm| above this is MLMW_ERR_ARG */
	uint16_t last_seq;
	uint8_t have_seq;          /* 0 until the first frame is accepted */
} mlmw_cmd_ctx_t;

void mlmw_cmd_init(mlmw_cmd_ctx_t *c, mlmw_mac_verify_fn verify,
		   const mlmw_cmd_handlers_t *h, void *user, int16_t max_rpm);

/* Checks run in this order: length, version, tag, replay, command, arguments.
 * A frame that fails any check calls no handler and does not move last_seq.
 * A frame with a valid tag and fresh seq but a bad command or argument
 * still moves last_seq (it was authentic, so it must not be replayed). */
int mlmw_cmd_process(mlmw_cmd_ctx_t *c, const uint8_t *buf, size_t n);

#endif
