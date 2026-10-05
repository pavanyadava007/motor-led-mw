#include <zephyr/ztest.h>
#include <string.h>
#include "mlmw/cmd.h"

/* Test double for the MAC: tag = 8 copies of the XOR of the message. */
static int fake_verify(void *user, const uint8_t *msg, size_t len, const uint8_t tag[8])
{
	uint8_t x = 0;

	ARG_UNUSED(user);
	for (size_t k = 0; k < len; k++) {
		x ^= msg[k];
	}
	for (int k = 0; k < 8; k++) {
		if (tag[k] != x) {
			return -1;
		}
	}
	return 0;
}

static size_t frame(uint8_t *b, uint16_t seq, uint8_t cmd, const uint8_t *p, uint8_t len)
{
	uint8_t x = 0;

	b[0] = 1;
	b[1] = seq >> 8;
	b[2] = seq & 0xFF;
	b[3] = cmd;
	b[4] = len;
	memcpy(&b[5], p, len);
	for (int k = 0; k < 5 + len; k++) {
		x ^= b[k];
	}
	memset(&b[5 + len], x, 8);
	return 5 + len + 8;
}

static int16_t got_rpm;
static int calls;
static uint8_t got_led[4];

static void h_speed(void *u, int16_t rpm) { got_rpm = rpm; calls++; }
static void h_stop(void *u) { calls++; }
static void h_led(void *u, uint8_t r, uint8_t g, uint8_t b, uint8_t br)
{
	got_led[0] = r; got_led[1] = g; got_led[2] = b; got_led[3] = br;
	calls++;
}
static void h_clear(void *u) { calls++; }

static const mlmw_cmd_handlers_t H = {h_speed, h_stop, h_led, h_clear};
static mlmw_cmd_ctx_t ctx;

static void before(void *f)
{
	mlmw_cmd_init(&ctx, fake_verify, &H, NULL, 3000);
	calls = 0;
	got_rpm = 0;
}

ZTEST(ref_cmd, test_set_speed)
{
	uint8_t b[32], p[2] = {0xFC, 0x18}; /* -1000 */
	size_t n = frame(b, 1, MLMW_CMD_SET_SPEED, p, 2);

	zassert_equal(mlmw_cmd_process(&ctx, b, n), MLMW_OK);
	zassert_equal(got_rpm, -1000);
}

ZTEST(ref_cmd, test_speed_limit)
{
	uint8_t b[32], p[2] = {0x0B, 0xB9}; /* 3001 */
	size_t n = frame(b, 1, MLMW_CMD_SET_SPEED, p, 2);

	zassert_equal(mlmw_cmd_process(&ctx, b, n), MLMW_ERR_ARG);
	zassert_equal(calls, 0);
}

ZTEST(ref_cmd, test_set_led)
{
	uint8_t b[32], p[4] = {1, 2, 3, 4};
	size_t n = frame(b, 1, MLMW_CMD_SET_LED, p, 4);

	zassert_equal(mlmw_cmd_process(&ctx, b, n), MLMW_OK);
	zassert_mem_equal(got_led, p, 4);
}

ZTEST(ref_cmd, test_bad_tag)
{
	uint8_t b[32];
	size_t n = frame(b, 1, MLMW_CMD_STOP, NULL, 0);

	b[n - 1] ^= 1;
	zassert_equal(mlmw_cmd_process(&ctx, b, n), MLMW_ERR_MAC);
	zassert_equal(calls, 0);
}

ZTEST(ref_cmd, test_replay)
{
	uint8_t b[32];
	size_t n = frame(b, 5, MLMW_CMD_STOP, NULL, 0);

	zassert_equal(mlmw_cmd_process(&ctx, b, n), MLMW_OK);
	zassert_equal(mlmw_cmd_process(&ctx, b, n), MLMW_ERR_REPLAY);
	n = frame(b, 4, MLMW_CMD_STOP, NULL, 0);
	zassert_equal(mlmw_cmd_process(&ctx, b, n), MLMW_ERR_REPLAY);
	n = frame(b, 6, MLMW_CMD_STOP, NULL, 0);
	zassert_equal(mlmw_cmd_process(&ctx, b, n), MLMW_OK);
	zassert_equal(calls, 2);
}

ZTEST(ref_cmd, test_length_and_version)
{
	uint8_t b[32];
	size_t n = frame(b, 1, MLMW_CMD_STOP, NULL, 0);

	zassert_equal(mlmw_cmd_process(&ctx, b, n - 1), MLMW_ERR_LEN);
	zassert_equal(mlmw_cmd_process(&ctx, b, 4), MLMW_ERR_LEN);
	b[0] = 2;
	zassert_equal(mlmw_cmd_process(&ctx, b, n), MLMW_ERR_VER);
}

ZTEST(ref_cmd, test_unknown_command)
{
	uint8_t b[32];
	size_t n = frame(b, 1, 0x77, NULL, 0);

	zassert_equal(mlmw_cmd_process(&ctx, b, n), MLMW_ERR_CMD);
}

ZTEST_SUITE(ref_cmd, NULL, NULL, before, NULL, NULL);
