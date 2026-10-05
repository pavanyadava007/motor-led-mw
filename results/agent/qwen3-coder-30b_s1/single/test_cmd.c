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

static void mock_set_speed(void *user, int16_t rpm)
{
	test_rpm = rpm;
}

static void mock_stop(void *user)
{
	test_stop_called = 1;
}

static void mock_set_led(void *user, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
	test_led_r = r;
	test_led_g = g;
	test_led_b = b;
	test_led_brightness = brightness;
}

static void mock_clear_fault(void *user)
{
	test_clear_fault_called = 1;
}

static const mlmw_cmd_handlers_t handlers = {
	.set_speed = mock_set_speed,
	.stop = mock_stop,
	.set_led = mock_set_led,
	.clear_fault = mock_clear_fault
};

static mlmw_cmd_ctx_t ctx;

ZTEST(SUITE, test_init)
{
	mlmw_cmd_init(&ctx, mock_verify, &handlers, NULL, 1000);

	EXPECT_EQ(ctx.verify, mock_verify);
	EXPECT_EQ(ctx.h, &handlers);
	EXPECT_EQ(ctx.user, NULL);
	EXPECT_EQ(ctx.max_rpm, 1000);
	EXPECT_EQ(ctx.last_seq, 0);
	EXPECT_EQ(ctx.have_seq, 0);
}

ZTEST(SUITE, test_process_null_context)
{
	int ret = mlmw_cmd_process(NULL, (const uint8_t[]){0x01, 0x00, 0x01, 0x01, 0x00}, 10);
	EXPECT_EQ(ret, MLMW_ERR_LEN);
}

ZTEST(SUITE, test_process_null_buffer)
{
	mlmw_cmd_init(&ctx, mock_verify, &handlers, NULL, 1000);
	int ret = mlmw_cmd_process(&ctx, NULL, 10);
	EXPECT_EQ(ret, MLMW_ERR_LEN);
}

ZTEST(SUITE, test_process_too_short)
{
	mlmw_cmd_init(&ctx, mock_verify, &handlers, NULL, 1000);
	int ret = mlmw_cmd_process(&ctx, (const uint8_t[]){0x01, 0x00, 0x01, 0x01, 0x00}, 4);
	EXPECT_EQ(ret, MLMW_ERR_LEN);
}



ZTEST(SUITE, test_process_wrong_length)
{
	mlmw_cmd_init(&ctx, mock_verify, &handlers, NULL, 1000);
	int ret = mlmw_cmd_process(&ctx, (const uint8_t[]){0x01, 0x00, 0x01, 0x01, 0x02}, 10);
	EXPECT_EQ(ret, MLMW_ERR_LEN);
}









ZTEST(SUITE, test_process_stop)
{
	mlmw_cmd_init(&ctx, mock_verify, &handlers, NULL, 1000);
	test_stop_called = 0;
	uint8_t frame[] = {0x01, 0x00, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	int ret = mlmw_cmd_process(&ctx, frame, sizeof(frame));
	EXPECT_EQ(ret, MLMW_OK);
	EXPECT_EQ(test_stop_called, 1);
}



ZTEST(SUITE, test_process_clear_fault)
{
	mlmw_cmd_init(&ctx, mock_verify, &handlers, NULL, 1000);
	test_clear_fault_called = 0;
	uint8_t frame[] = {0x01, 0x00, 0x04, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	int ret = mlmw_cmd_process(&ctx, frame, sizeof(frame));
	EXPECT_EQ(ret, MLMW_OK);
	EXPECT_EQ(test_clear_fault_called, 1);
}

ZTEST(SUITE, test_process_unknown_command)
{
	mlmw_cmd_init(&ctx, mock_verify, &handlers, NULL, 1000);
	uint8_t frame[] = {0x01, 0x00, 0x05, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	int ret = mlmw_cmd_process(&ctx, frame, sizeof(frame));
	EXPECT_EQ(ret, MLMW_ERR_CMD);
}















ZTEST_SUITE(SUITE, NULL, NULL, NULL, NULL, NULL);
