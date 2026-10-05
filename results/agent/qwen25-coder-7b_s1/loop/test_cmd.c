#include "mlmw_test.h"
#include "mlmw/cmd.h"
#include <string.h>

ZTEST_SUITE(agent_cmd, NULL, NULL, NULL, NULL, NULL);

static int mock_mac_verify(void *user, const uint8_t *msg, size_t len, const uint8_t tag[MLMW_CMD_TAG])
{
	return 0; // Always accept the tag
}

static void mock_set_speed(void *user, int16_t rpm)
{
	// Do nothing
}

static void mock_stop(void *user)
{
	// Do nothing
}

static void mock_set_led(void *user, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
	// Do nothing
}

static void mock_clear_fault(void *user)
{
	// Do nothing
}

static mlmw_cmd_handlers_t mock_handlers = {
	.set_speed = mock_set_speed,
	.stop = mock_stop,
	.set_led = mock_set_led,
	.clear_fault = mock_clear_fault,
};

static mlmw_cmd_ctx_t ctx;

void setUp(void)
{
	mlmw_cmd_init(&ctx, mock_mac_verify, &mock_handlers, NULL, 1000);
}

ZTEST(agent_cmd, test_init)
{
	mlmw_cmd_ctx_t ctx2;
	mlmw_cmd_init(&ctx2, mock_mac_verify, &mock_handlers, NULL, 1000);
	EXPECT_EQ(ctx2.verify, mock_mac_verify);
	EXPECT_EQ(ctx2.h, &mock_handlers);
	EXPECT_EQ(ctx2.user, NULL);
	EXPECT_EQ(ctx2.max_rpm, 1000);
	EXPECT_EQ(ctx2.last_seq, 0);
	EXPECT_EQ(ctx2.have_seq, 0);
}



ZTEST(agent_cmd, test_process_invalid_length)
{
	uint8_t buf[] = {1, 0, 1, 0x01, 0x00, 0x00, 0x00, 0x00};
	int ret = mlmw_cmd_process(&ctx, buf, sizeof(buf));
	EXPECT_EQ(ret, MLMW_ERR_LEN);
}


















