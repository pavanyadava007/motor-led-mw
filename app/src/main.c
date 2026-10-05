/* Motor + LED node: the mlmw middleware integrated on Zephyr.
 *
 * - control thread, 1 kHz: speed ramp + PI (mlmw), fault monitor, LED mixing;
 *   inside each 1 ms tick the hall "ISR" runs at 20 kHz: hall -> sector ->
 *   six-step gate mask (mlmw), applied to the simulated motor (plant.c)
 * - network thread: authenticated command frames over UDP/Ethernet, checked by
 *   mlmw_cmd_process with the MAC verified by PSA Crypto in TF-M (secure world)
 * - telemetry: one UDP datagram every 100 ms to the last command sender
 */
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>

#include "mac_psa.h"
#include "mlmw/bldc.h"
#include "mlmw/cmd.h"
#include "mlmw/led.h"
#include "net.h"
#include "plant.h"

#define SUBSTEPS 20u
#define SUBSTEP_US 50u
#define MAX_RPM 3000
#define RAMP_RPM_PER_MS 5
#define OC_LIMIT_MA 15000u

K_MUTEX_DEFINE(lock);

static struct {
	int16_t target_rpm;
	bool forward;
	int32_t sp;
	uint16_t duty;
	int32_t rpm;
	uint8_t fault;
	uint8_t led_rgb[3];
	uint8_t led_brightness;
	uint16_t led_duty[3];
	int16_t led_temp;
	uint16_t accepted;
	uint16_t rejected;
	int8_t last_status;
	bool clear_req;
} st = {.forward = true};

static mlmw_pi_t pi = {.kp_q15 = 3277, .ki_q15 = 655, .out_min = 0, .out_max = 1000};
static mlmw_fault_t fault;
static const mlmw_led_cal_t led_cal = {{32767, 29491, 26214}, 85, 125, 8192};

static void h_speed(void *u, int16_t rpm)
{
	st.target_rpm = rpm;
}

static void h_stop(void *u)
{
	st.target_rpm = 0;
}

static void h_led(void *u, uint8_t r, uint8_t g, uint8_t b, uint8_t br)
{
	st.led_rgb[0] = r;
	st.led_rgb[1] = g;
	st.led_rgb[2] = b;
	st.led_brightness = br;
}

static void h_clear(void *u)
{
	st.clear_req = true;
}

static const mlmw_cmd_handlers_t handlers = {h_speed, h_stop, h_led, h_clear};
static mlmw_cmd_ctx_t cmd;

static void control_thread(void)
{
	uint32_t now_ms = 0;
	uint32_t since_edge_us = 1000000;
	uint32_t period_us = 0;
	uint8_t last_hall = plant_hall();

	mlmw_fault_init(&fault, OC_LIMIT_MA, 5, 200, 0);
	while (1) {
		k_mutex_lock(&lock, K_FOREVER);
		if (st.clear_req) {
			mlmw_fault_clear(&fault, now_ms);
			mlmw_pi_reset(&pi);
			st.clear_req = false;
		}
		int32_t want = st.target_rpm;
		bool want_fwd = want >= 0;

		/* reverse only after the rotor has come down to (almost) zero */
		if (want_fwd != st.forward) {
			if (st.rpm < 50) {
				st.forward = want_fwd;
				mlmw_pi_reset(&pi);
			}
			want = 0;
		}
		int32_t target_abs = want < 0 ? -want : want;

		st.sp = mlmw_ramp(st.sp, target_abs, RAMP_RPM_PER_MS);
		bool running = st.sp > 0 && st.fault == MLMW_FAULT_NONE;

		if (running) {
			st.duty = (uint16_t)mlmw_pi_step(&pi, st.sp - st.rpm);
		} else {
			st.duty = 0;
			mlmw_pi_reset(&pi);
		}
		bool edge_in_tick = false;
		uint16_t peak_ma = 0;

		for (uint32_t k = 0; k < SUBSTEPS; k++) {
			uint8_t hall = plant_hall();

			if (hall != last_hall) {
				edge_in_tick = true;
				period_us = since_edge_us;
				since_edge_us = 0;
				last_hall = hall;
			}
			since_edge_us += SUBSTEP_US;
			int8_t sector = mlmw_hall_to_sector(hall);
			uint8_t mask = running ? mlmw_commutation(sector, st.forward) : 0u;

			plant_step(mask, st.duty, SUBSTEP_US);
			uint16_t ma = plant_current_ma();

			peak_ma = ma > peak_ma ? ma : peak_ma;
		}
		st.rpm = since_edge_us > 100000u ? 0 : (int32_t)mlmw_speed_rpm(period_us, PLANT_POLE_PAIRS);
		st.fault = mlmw_fault_update(&fault, peak_ma, now_ms, edge_in_tick, running && st.duty > 0);

		plant_led_step(st.led_duty, 1000);
		if (now_ms % 100u == 0u) {
			st.led_temp = plant_led_temp_c();
			mlmw_led_mix(&led_cal, st.led_rgb, st.led_brightness, st.led_temp, st.led_duty);
		}
		k_mutex_unlock(&lock);
		now_ms++;
		k_msleep(1);
	}
}

static void telemetry(uint32_t t_ms)
{
	uint8_t b[24];

	k_mutex_lock(&lock, K_FOREVER);
	b[0] = 'T';
	sys_put_be32(t_ms, &b[1]);
	sys_put_be16((uint16_t)st.target_rpm, &b[5]);
	sys_put_be16((uint16_t)(st.forward ? st.rpm : -st.rpm), &b[7]);
	sys_put_be16(st.duty, &b[9]);
	b[11] = st.fault;
	sys_put_be16((uint16_t)st.led_temp, &b[12]);
	sys_put_be16(st.led_duty[0], &b[14]);
	sys_put_be16(st.led_duty[1], &b[16]);
	sys_put_be16(st.led_duty[2], &b[18]);
	sys_put_be16(st.accepted, &b[20]);
	sys_put_be16(st.rejected, &b[22]);
	k_mutex_unlock(&lock);
	net_reply(b, sizeof(b));
}

static void network_thread(void)
{
	uint8_t buf[64];
	uint32_t next_tlm = 0;

	while (1) {
		int n = net_recv(buf, sizeof(buf), 10);

		if (n > 0) {
			k_mutex_lock(&lock, K_FOREVER);
			int s = mlmw_cmd_process(&cmd, buf, (size_t)n);

			st.last_status = (int8_t)s;
			if (s == MLMW_OK) {
				st.accepted++;
			} else {
				st.rejected++;
			}
			k_mutex_unlock(&lock);
			uint8_t ack[4] = {'A', n > 2 ? buf[1] : 0, n > 2 ? buf[2] : 0, (uint8_t)s};

			net_reply(ack, sizeof(ack));
		}
		uint32_t t = k_uptime_get_32();

		if (t >= next_tlm) {
			telemetry(t);
			next_tlm = t + 100;
		}
	}
}

K_THREAD_DEFINE(ctl, 2048, control_thread, NULL, NULL, NULL, 5, 0, 100);

int main(void)
{
	plant_init();
	int rc = mac_psa_init();

	printk("motor-led node: mlmw on %s, PSA key status %d\n", CONFIG_BOARD_TARGET, rc);
	mlmw_cmd_init(&cmd, mac_psa_verify, &handlers, NULL, MAX_RPM);
	if (net_open() != 0) {
		printk("socket failed\n");
		return 0;
	}
	printk("listening on udp/%d\n", NODE_UDP_PORT);
	network_thread();
	return 0;
}
