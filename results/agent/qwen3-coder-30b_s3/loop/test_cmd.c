#include "mlmw_test.h"
#include "mlmw/cmd.h"
#include <string.h>

#define SUITE agent_cmd

static int16_t test_rpm;
static uint8_t test_led_r, test_led_g, test_led_b, test_led_brightness;
static int test_stop_called;
static int test_clear_fault_called;

static int mock_verify(void *user, const uint8_t *msg, size_t len,
			const uint8_t tag[MLMW_CMD_TAG])
{
	/* Mock verification that always succeeds */
	return 0;
}

static void test_set_speed(void *user, int16_t rpm)
{
	test_rpm = rpm;
}

static void test_stop(void *user)
{
	test_stop_called = 1;
}

static void test_set_led(void *user, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
	test_led_r = r;
	test_led_g = g;
	test_led_b = b;
	test_led_brightness = brightness;
}

static void test_clear_fault(void *user)
{
	test_clear_fault_called = 1;
}

static const mlmw_cmd_handlers_t test_handlers = {
	.set_speed = test_set_speed,
	.stop = test_stop,
	.set_led = test_set_led,
	.clear_fault = test_clear_fault,
};

ZTEST(SUITE, test_init)
{
	mlmw_cmd_ctx_t ctx;
	mlmw_cmd_init(&ctx, mock_verify, &test_handlers, NULL, 1000);

	EXPECT_EQ(ctx.verify, mock_verify);
	EXPECT_EQ(ctx.h, &test_handlers);
	EXPECT_EQ(ctx.user, NULL);
	EXPECT_EQ(ctx.max_rpm, 1000);
	EXPECT_EQ(ctx.last_seq, 0);
	EXPECT_EQ(ctx.have_seq, 0);
}

ZTEST(SUITE, test_process_short_frame)
{
	mlmw_cmd_ctx_t ctx;
	mlmw_cmd_init(&ctx, mock_verify, &test_handlers, NULL, 1000);

	uint8_t buf[] = {MLMW_CMD_VERSION, 0x00, 0x01, MLMW_CMD_SET_SPEED, 0x02};
	int ret = mlmw_cmd_process(&ctx, buf, sizeof(buf));

	EXPECT_EQ(ret, MLMW_ERR_LEN);
}

ZTEST(SUITE, test_process_wrong_length)
{
	mlmw_cmd_ctx_t ctx;
	mlmw_cmd_init(&ctx, mock_verify, &test_handlers, NULL, 1000);

	uint8_t buf[] = {MLMW_CMD_VERSION, 0x00, 0x01, MLMW_CMD_SET_SPEED, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	int ret = mlmw_cmd_process(&ctx, buf, sizeof(buf));

	EXPECT_EQ(ret, MLMW_ERR_LEN);
}























ZTEST(SUITE, test_process_null_context)
{
	mlmw_cmd_ctx_t ctx;
	mlmw_cmd_init(&ctx, mock_verify, &test_handlers, NULL, 1000);

	uint8_t buf[] = {MLMW_CMD_VERSION, 0x00, 0x01, MLMW_CMD_SET_SPEED, 0x02, 0x03, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	int ret = mlmw_cmd_process(NULL, buf, sizeof(buf));

	EXPECT_EQ(ret, MLMW_ERR_LEN);
}

ZTEST(SUITE, test_process_null_buffer)
{
	mlmw_cmd_ctx_t ctx;
	mlmw_cmd_init(&ctx, mock_verify, &test_handlers, NULL, 1000);

	int ret = mlmw_cmd_process(&ctx, NULL, 14);

	EXPECT_EQ(ret, MLMW_ERR_LEN);
}

ZTEST(SUITE, test_process_zero_length)
{
	mlmw_cmd_ctx_t ctx;
	mlmw_cmd_init(&ctx, mock_verify, &test_handlers, NULL, 1000);

	int ret = mlmw_cmd_process(&ctx, NULL, 0);

	EXPECT_EQ(ret, MLMW_ERR_LEN);
}









ZTEST_SUITE(SUITE, NULL, NULL, NULL, NULL, NULL);
