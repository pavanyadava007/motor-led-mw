#include "mlmw_test.h"
#include "mlmw/cmd.h"
#include <string.h>

ZTEST_SUITE(agent_cmd, NULL, NULL, NULL, NULL, NULL);

static int mock_mac_verify(void *user, const uint8_t *msg, size_t len, const uint8_t tag[MLMW_CMD_TAG])
{
	// Mock implementation always returns 0 (valid tag)
	return 0;
}

static void mock_set_speed(void *user, int16_t rpm)
{
	// Mock implementation does nothing
}

static void mock_stop(void *user)
{
	// Mock implementation does nothing
}

static void mock_set_led(void *user, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
	// Mock implementation does nothing
}

static void mock_clear_fault(void *user)
{
	// Mock implementation does nothing
}

static mlmw_cmd_handlers_t mock_handlers = {
	.set_speed = mock_set_speed,
	.stop = mock_stop,
	.set_led = mock_set_led,
	.clear_fault = mock_clear_fault,
};

static mlmw_cmd_ctx_t ctx;

void test_cmd_init(void)
{
	mlmw_cmd_init(&ctx, mock_mac_verify, &mock_handlers, NULL, 1000);
}

ZTEST(agent_cmd, test_cmd_init)
{
	test_cmd_init();
	zassert_true(ctx.verify == mock_mac_verify, "Verify function not set");
	zassert_true(ctx.h == &mock_handlers, "Handlers not set");
	zassert_true(ctx.user == NULL, "User pointer not set");
	zassert_true(ctx.max_rpm == 1000, "Max RPM not set");
	zassert_true(ctx.last_seq == 0, "Last sequence not set");
	zassert_true(ctx.have_seq == 0, "Have sequence not set");
}

ZTEST(agent_cmd, test_cmd_process_valid_frame)
{
	test_cmd_init();
	uint8_t buf[MLMW_CMD_HDR + 2 + MLMW_CMD_TAG];
	buf[0] = MLMW_CMD_VERSION;
	buf[1] = 0x00;
	buf[2] = 0x01;
	buf[3] = MLMW_CMD_SET_SPEED;
	buf[4] = 2;
	buf[5] = 0x00;
	buf[6] = 0x01;
	buf[7] = 0x00;
	buf[8] = 0x00;
	buf[9] = 0x00;
	buf[10] = 0x00;
	buf[11] = 0x00;
	buf[12] = 0x00;
	buf[13] = 0x00;
	buf[14] = 0x00;
	buf[15] = 0x00;

	int ret = mlmw_cmd_process(&ctx, buf, sizeof(buf));
	zassert_true(ret == MLMW_OK, "Command processing failed");
	zassert_true(ctx.last_seq == 1, "Last sequence not updated");
}



ZTEST(agent_cmd, test_cmd_process_invalid_version)
{
	test_cmd_init();
	uint8_t buf[MLMW_CMD_HDR + 2 + MLMW_CMD_TAG];
	buf[0] = 0x02;
	buf[1] = 0x00;
	buf[2] = 0x01;
	buf[3] = MLMW_CMD_SET_SPEED;
	buf[4] = 2;
	buf[5] = 0x00;
	buf[6] = 0x01;
	buf[7] = 0x00;
	buf[8] = 0x00;
	buf[9] = 0x00;
	buf[10] = 0x00;
	buf[11] = 0x00;
	buf[12] = 0x00;
	buf[13] = 0x00;
	buf[14] = 0x00;
	buf[15] = 0x00;

	int ret = mlmw_cmd_process(&ctx, buf, sizeof(buf));
	zassert_true(ret == MLMW_ERR_VER, "Invalid version not detected");
	zassert_true(ctx.last_seq == 0, "Last sequence updated");
}



ZTEST(agent_cmd, test_cmd_process_replay_attack)
{
	test_cmd_init();
	uint8_t buf[MLMW_CMD_HDR + 2 + MLMW_CMD_TAG];
	buf[0] = MLMW_CMD_VERSION;
	buf[1] = 0x00;
	buf[2] = 0x01;
	buf[3] = MLMW_CMD_SET_SPEED;
	buf[4] = 2;
	buf[5] = 0x00;
	buf[6] = 0x01;
	buf[7] = 0x00;
	buf[8] = 0x00;
	buf[9] = 0x00;
	buf[10] = 0x00;
	buf[11] = 0x00;
	buf[12] = 0x00;
	buf[13] = 0x00;
	buf[14] = 0x00;
	buf[15] = 0x00;

	int ret = mlmw_cmd_process(&ctx, buf, sizeof(buf));
	zassert_true(ret == MLMW_OK, "Command processing failed");
	zassert_true(ctx.last_seq == 1, "Last sequence not updated");

	ret = mlmw_cmd_process(&ctx, buf, sizeof(buf));
	zassert_true(ret == MLMW_ERR_REPLAY, "Replay attack not detected");
	zassert_true(ctx.last_seq == 1, "Last sequence updated");
}





ZTEST(agent_cmd, test_cmd_process_valid_stop_command)
{
	test_cmd_init();
	uint8_t buf[MLMW_CMD_HDR + 0 + MLMW_CMD_TAG];
	buf[0] = MLMW_CMD_VERSION;
	buf[1] = 0x00;
	buf[2] = 0x01;
	buf[3] = MLMW_CMD_STOP;
	buf[4] = 0;
	buf[5] = 0x00;
	buf[6] = 0x00;
	buf[7] = 0x00;
	buf[8] = 0x00;
	buf[9] = 0x00;
	buf[10] = 0x00;
	buf[11] = 0x00;
	buf[12] = 0x00;
	buf[13] = 0x00;
	buf[14] = 0x00;
	buf[15] = 0x00;

	int ret = mlmw_cmd_process(&ctx, buf, sizeof(buf));
	zassert_true(ret == MLMW_OK, "Command processing failed");
	zassert_true(ctx.last_seq == 1, "Last sequence not updated");
}

ZTEST(agent_cmd, test_cmd_process_valid_set_led_command)
{
	test_cmd_init();
	uint8_t buf[MLMW_CMD_HDR + 4 + MLMW_CMD_TAG];
	buf[0] = MLMW_CMD_VERSION;
	buf[1] = 0x00;
	buf[2] = 0x01;
	buf[3] = MLMW_CMD_SET_LED;
	buf[4] = 4;
	buf[5] = 0x00;
	buf[6] = 0x01;
	buf[7] = 0x02;
	buf[8] = 0x03;
	buf[9] = 0x04;
	buf[10] = 0x00;
	buf[11] = 0x00;
	buf[12] = 0x00;
	buf[13] = 0x00;
	buf[14] = 0x00;
	buf[15] = 0x00;

	int ret = mlmw_cmd_process(&ctx, buf, sizeof(buf));
	zassert_true(ret == MLMW_OK, "Command processing failed");
	zassert_true(ctx.last_seq == 1, "Last sequence not updated");
}

ZTEST(agent_cmd, test_cmd_process_valid_clear_fault_command)
{
	test_cmd_init();
	uint8_t buf[MLMW_CMD_HDR + 0 + MLMW_CMD_TAG];
	buf[0] = MLMW_CMD_VERSION;
	buf[1] = 0x00;
	buf[2] = 0x01;
	buf[3] = MLMW_CMD_CLEAR_FAULT;
	buf[4] = 0;
	buf[5] = 0x00;
	buf[6] = 0x00;
	buf[7] = 0x00;
	buf[8] = 0x00;
	buf[9] = 0x00;
	buf[10] = 0x00;
	buf[11] = 0x00;
	buf[12] = 0x00;
	buf[13] = 0x00;
	buf[14] = 0x00;
	buf[15] = 0x00;

	int ret = mlmw_cmd_process(&ctx, buf, sizeof(buf));
	zassert_true(ret == MLMW_OK, "Command processing failed");
	zassert_true(ctx.last_seq == 1, "Last sequence not updated");
}


