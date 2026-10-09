/* Profiling driver for the emulated Cortex-M4F: 1000 BMS cycles (12 cells, 4 sensors)
 * with deterministic synthetic inputs: VCU close request every cycle, precharge done
 * after 5 cycles, then ACTIVE with a varying discharge/regen current and 1-3 mV ADC
 * noise. The QEMU plugin counts the instructions inside the library functions. */
#include "bms/bms.h"
#include "bms_io.h"

#define PROF_CYCLES 1000u

static bms_cfg_t cfg;
static bms_t bms;
static bms_in_t in;
static bms_out_t out;
static uint32_t prof_seed = 12345u;

/* own copy of the frame encoder, so that building the VCU frame does not execute
 * library code (the plugin counts every instruction inside the library ranges) */
static uint8_t prof_crc(uint8_t c, uint8_t d)
{
	uint8_t b;

	c ^= d;
	for (b = 0u; b < 8u; b++) {
		c = ((c & 0x80u) != 0u) ? (uint8_t)((uint8_t)(c << 1) ^ 0x1Du) : (uint8_t)(c << 1);
	}
	return c;
}

static void prof_vcu_frame(uint8_t f[8], uint8_t counter)
{
	uint8_t c = prof_crc(0xFFu, BMS_DID_VCU_CMD);
	uint8_t k;

	for (k = 0u; k < 8u; k++) {
		f[k] = 0u;
	}
	f[1] = (uint8_t)(counter & 0x0Fu);
	f[2] = 1u; /* close request */
	for (k = 1u; k < 8u; k++) {
		c = prof_crc(c, f[k]);
	}
	f[0] = (uint8_t)(c ^ 0xFFu);
}

static uint32_t prof_rand(void)
{
	prof_seed = (prof_seed * 1103515245u) + 12345u;
	return (prof_seed >> 16) & 0x7FFFu;
}

int pil_main(void)
{
	uint8_t cmd[8];
	uint32_t k;
	uint8_t j;
	uint32_t active = 0u;

	bms_default_cfg(&cfg);
	bms_init(&bms, &cfg);
	for (j = 0u; j < BMS_MAX_TEMPS; j++) {
		in.temp_dc[j] = 251;
	}
	for (k = 0u; k < PROF_CYCLES; k++) {
		int32_t phase = (int32_t)(k % 200u);

		in.current_ma = (phase < 150) ? (-8000 - (phase * 20)) : 3000; /* drive, then regen */
		for (j = 0u; j < 12u; j++) {
			in.cell_mv[j] = (uint16_t)(3750u + (j * 2u) + (prof_rand() % 4u) -
						   (uint32_t)((in.current_ma < 0) ? 40 : 0));
		}
		in.bus_mv = (k < 5u) ? 0u : 44000u;
		prof_vcu_frame(cmd, (uint8_t)k);
		in.vcu_cmd = cmd;
		bms_cycle(&bms, &in, &out);
		if (out.state == BMS_ST_ACTIVE) {
			active++;
		}
	}
	io_puts((active >= (PROF_CYCLES - 10u)) ? "PROF ACTIVE\n" : "PROF NOT ACTIVE\n");
	return (active >= (PROF_CYCLES - 10u)) ? 0 : 1;
}
