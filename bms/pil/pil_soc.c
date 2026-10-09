/* Processor-in-the-loop SoC run: streams recorded (dt, current, voltage) samples through
 * bms_soc_step() and writes (soc_cc, soc_ekf, v_pred) per sample. The same file is built
 * for the host (stdio) and for the emulated Cortex-M4F (semihosting).
 *
 * Input : float32[4] records. Header {-1, soc0, ecm_id, 0} starts a new run
 *         (ecm_id 0 = 25 degC set, 1 = 0 degC set); data {dt_s, current_a, voltage_v, 0}.
 * Output: float32[3] per data record.
 */
#include "bms/bms_ecm_lg_hg2.h"
#include "bms/bms_soc.h"
#include "bms_io.h"

#define CHUNK 256u

static float in_buf[CHUNK * 4u];
static float out_buf[CHUNK * 3u];

int pil_main(void)
{
	int hi = io_open("build/bms/pil/pil_in.bin", 0);
	int ho = io_open("build/bms/pil/pil_out.bin", 1);
	bms_soc_t s;
	const bms_ecm_t *m = &bms_ecm_lg_hg2_25c;
	int total = 0;
	int n;
	int k;
	uint32_t o;

	if ((hi < 0) || (ho < 0)) {
		io_puts("PIL: cannot open files\n");
		return 1;
	}
	bms_soc_init(&s, m, 1.0f);
	for (;;) {
		n = io_read(hi, in_buf, (int)sizeof(in_buf)) / (int)(4u * sizeof(float));
		if (n <= 0) {
			break;
		}
		o = 0u;
		for (k = 0; k < n; k++) {
			const float *r = &in_buf[(uint32_t)k * 4u];

			if (r[0] < 0.0f) {
				m = (r[2] > 0.5f) ? &bms_ecm_lg_hg2_0c : &bms_ecm_lg_hg2_25c;
				bms_soc_init(&s, m, r[1]);
			} else {
				bms_soc_step(&s, m, r[1], r[2], r[0]);
				out_buf[o] = s.soc_cc;
				out_buf[o + 1u] = s.soc;
				out_buf[o + 2u] = s.v_pred;
				o += 3u;
				total++;
			}
		}
		(void)io_write(ho, out_buf, (int)(o * sizeof(float)));
	}
	io_close(hi);
	io_close(ho);
	io_puts(total > 0 ? "PIL done\n" : "PIL: no data\n");
	return (total > 0) ? 0 : 1;
}
