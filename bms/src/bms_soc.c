/* Coulomb counting + EKF on a 1-RC ECM, float32 only, see bms/bms_soc.h. */
#include "bms/bms_soc.h"

float bms_exp_neg(float x)
{
	float y;
	float u = x;
	uint8_t k = 0u;
	uint8_t j;

	if (u <= 0.0f) {
		y = 1.0f;
	} else if (u > 20.0f) {
		y = 0.0f;
	} else {
		while (u > 0.125f) {
			u = u * 0.5f;
			k++;
		}
		/* 6-term Taylor series of exp(-u), Horner form */
		y = 1.0f + (u * (-1.0f + (u * (0.5f + (u * (-0.16666667f + (u * (0.041666668f +
			(u * -0.008333334f)))))))));
		for (j = 0u; j < k; j++) {
			y = y * y;
		}
	}
	return y;
}

static float clamp01(float s)
{
	float r = s;

	if (r < 0.0f) {
		r = 0.0f;
	}
	if (r > 1.0f) {
		r = 1.0f;
	}
	return r;
}

/* Linear interpolation in a table of n points on a uniform grid over 0..1. */
static float lut(const float *t, uint32_t n, float soc, float *slope)
{
	float pos = clamp01(soc) * (float)(n - 1u);
	uint32_t i = (uint32_t)pos;
	float frac;
	float d;

	if (i > (n - 2u)) {
		i = n - 2u;
	}
	frac = pos - (float)i;
	d = t[i + 1u] - t[i];
	if (slope != NULL) {
		*slope = d * (float)(n - 1u);
	}
	return t[i] + (frac * d);
}

float bms_ocv(const bms_ecm_t *m, float soc, float *slope)
{
	return lut(m->ocv_v, BMS_OCV_N, soc, slope);
}

float bms_soc_from_ocv(const bms_ecm_t *m, float v)
{
	float soc;
	uint32_t i = 0u;

	if (v <= m->ocv_v[0]) {
		soc = 0.0f;
	} else if (v >= m->ocv_v[BMS_OCV_N - 1u]) {
		soc = 1.0f;
	} else {
		while ((i < (BMS_OCV_N - 2u)) && (v >= m->ocv_v[i + 1u])) {
			i++;
		}
		soc = ((float)i + ((v - m->ocv_v[i]) / (m->ocv_v[i + 1u] - m->ocv_v[i]))) /
		      (float)(BMS_OCV_N - 1u);
	}
	return clamp01(soc);
}

void bms_soc_init(bms_soc_t *s, const bms_ecm_t *m, float soc0)
{
	s->soc = clamp01(soc0);
	s->v1 = 0.0f;
	s->p00 = m->p0_soc;
	s->p01 = 0.0f;
	s->p11 = m->p0_v1;
	s->soc_cc = soc0;
	s->v_pred = 0.0f;
}

void bms_soc_step(bms_soc_t *s, const bms_ecm_t *m, float i_a, float v_meas, float dt_s)
{
	float dq = (i_a * dt_s) / (3600.0f * m->capacity_ah);
	float r0 = lut(m->r0_ohm, BMS_PAR_N, s->soc, NULL);
	float r1 = lut(m->r1_ohm, BMS_PAR_N, s->soc, NULL);
	float tau = lut(m->tau_s, BMS_PAR_N, s->soc, NULL);
	float a = bms_exp_neg(dt_s / tau);
	float soc_p;
	float v1_p;
	float p00;
	float p01;
	float p11;
	float h;
	float ocv;
	float sv;

	s->soc_cc = s->soc_cc + dq;

	/* predict */
	soc_p = s->soc + dq;
	v1_p = (a * s->v1) + (r1 * (1.0f - a) * i_a);
	p00 = s->p00 + m->q_soc;
	p01 = a * s->p01;
	p11 = (a * a * s->p11) + m->q_v1;

	/* update with the terminal voltage */
	ocv = bms_ocv(m, soc_p, &h);
	s->v_pred = ocv + v1_p + (r0 * i_a);
	sv = (h * h * p00) + (2.0f * h * p01) + p11 + m->r_v;
	if (sv > 0.0f) {
		float hp0 = (h * p00) + p01;
		float hp1 = (h * p01) + p11;
		float k0 = hp0 / sv;
		float k1 = hp1 / sv;
		float e = v_meas - s->v_pred;

		soc_p = soc_p + (k0 * e);
		v1_p = v1_p + (k1 * e);
		p00 = p00 - (k0 * hp0);
		p01 = p01 - (k0 * hp1);
		p11 = p11 - (k1 * hp1);
	}
	s->soc = clamp01(soc_p);
	s->v1 = v1_p;
	s->p00 = p00;
	s->p01 = p01;
	s->p11 = p11;
}
