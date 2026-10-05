#include "mlmw_test.h"
#include "mlmw/cmd.h"
#include <string.h>

#define SUITE agent_cmd

static int16_t g_rpm;
static uint8_t g_r, g_g, g_b, g_brightness;
static int g_stop_called;
static int g_clear_fault_called;

static int mock_verify(void *user, const uint8_t *msg, size_t len,
			const uint8_t tag[MLMW_CMD_TAG])
{
	/* Mock verification that always succeeds */
	return 0;
}

static void mock_set_speed(void *user, int16_t rpm)
{
	g_rpm = rpm;
}

static void mock_stop(void *user)
{
	g_stop_called = 1;
}

static void mock_set_led(void *user, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
	g_r = r;
	g_g = g;
	g_b = b;
	g_brightness = brightness;
}

static void mock_clear_fault(void *user)
{
	g_clear_fault_called = 1;
}

static const mlmw_cmd_handlers_t handlers = {
	.set_speed = mock_set_speed,
	.stop = mock_stop,
	.set_led = mock_set_led,
	.clear_fault = mock_clear_fault
};

static mlmw_cmd_ctx_t ctx;

static void setup(void *fixture)
{
	(void)fixture;
	g_rpm = 0;
	g_r = g_g = g_b = g_brightness = 0;
	g_stop_called = 0;
	g_clear_fault_called = 0;
	mlmw_cmd_init(&ctx, mock_verify, &handlers, NULL, 1000);
}

/* Test basic initialization */
ZTEST(SUITE, test_init)
{
	EXPECT_EQ(ctx.verify, mock_verify);
	EXPECT_EQ(ctx.h, &handlers);
	EXPECT_EQ(ctx.user, NULL);
	EXPECT_EQ(ctx.max_rpm, 1000);
	EXPECT_EQ(ctx.last_seq, 0);
	EXPECT_EQ(ctx.have_seq, 0);
}

/* Test valid SET_SPEED command */


/* Test valid STOP command */
ZTEST(SUITE, test_process_stop)
{
	uint8_t buf[] = {MLMW_CMD_VERSION, 0x00, 0x02, MLMW_CMD_STOP, 0x00,
			 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};

	int ret = mlmw_cmd_process(&ctx, buf, sizeof(buf));
	EXPECT_EQ(ret, MLMW_OK);
	EXPECT_EQ(g_stop_called, 1);
	EXPECT_EQ(ctx.last_seq, 2);
	EXPECT_EQ(ctx.have_seq, 1);
}

/* Test invalid version */


/* Test invalid length */


/* Test replay protection */


/* Test command out of range */


/* Test argument out of range for SET_SPEED */


/* Test argument length mismatch for SET_SPEED */


/* Test argument length mismatch for SET_LED */


/* Test argument length mismatch for STOP */


/* Test argument length mismatch for CLEAR_FAULT */
ZTEST(SUITE, test_process_clear_fault_wrong_length)
{
	uint8_t buf[] = {MLMW_CMD_VERSION, 0x00, 0x01, MLMW_CMD_CLEAR_FAULT, 0x02,
			 0x00, 0x00,
			 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};

	int ret = mlmw_cmd_process(&ctx, buf, sizeof(buf));
	EXPECT_EQ(ret, MLMW_ERR_ARG);
	EXPECT_EQ(ctx.last_seq, 1);
	EXPECT_EQ(ctx.have_seq, 1);
}

/* Test buffer too short */
ZTEST(SUITE, test_process_buffer_too_short)
{
	uint8_t buf[] = {MLMW_CMD_VERSION, 0x00, 0x01, MLMW_CMD_SET_SPEED, 0x02,
			 0x03, 0xE8};

	int ret = mlmw_cmd_process(&ctx, buf, sizeof(buf));
	EXPECT_EQ(ret, MLMW_ERR_LEN);
	EXPECT_EQ(ctx.last_seq, 0);
	EXPECT_EQ(ctx.have_seq, 0);
}

/* Test NULL context */
ZTEST(SUITE, test_process_null_context)
{
	uint8_t buf[] = {MLMW_CMD_VERSION, 0x00, 0x01, MLMW_CMD_SET_SPEED, 0x02,
			 0x03, 0xE8,
			 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};

	int ret = mlmw_cmd_process(NULL, buf, sizeof(buf));
	EXPECT_EQ(ret, MLMW_ERR_LEN);
}

/* Test NULL buffer */
ZTEST(SUITE, test_process_null_buffer)
{
	int ret = mlmw_cmd_process(&ctx, NULL, 10);
	EXPECT_EQ(ret, MLMW_ERR_LEN);
}

ZTEST_SUITE(SUITE, setup, NULL, NULL, NULL, NULL);
