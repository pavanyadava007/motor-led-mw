#include "bms_test.h"
#include "bms/bms.h"
#include "bms/bms_ecm_lg_hg2.h"

/* exp(-x) reference values (computed with libm in double, rounded to 9 digits) */
/* verifies: BMS-REQ-016 */
BMS_TEST(bms_soc, test_exp_neg_accuracy)
{
	CHECK_NEAR(bms_exp_neg(0.0f), 1.0, 0.0);
	CHECK_NEAR(bms_exp_neg(-1.0f), 1.0, 0.0);
	CHECK_NEAR(bms_exp_neg(0.003333333f), 0.996672214, 1e-6);  /* dt 0.1 s, tau 30 s */
	CHECK_NEAR(bms_exp_neg(0.1f), 0.904837418, 1e-6);
	CHECK_NEAR(bms_exp_neg(0.5f), 0.606530660, 2e-6);
	CHECK_NEAR(bms_exp_neg(2.0f), 0.135335283, 1e-6);
	CHECK_NEAR(bms_exp_neg(10.0f), 4.53999298e-05, 1e-8);
	CHECK_NEAR(bms_exp_neg(25.0f), 0.0, 0.0);
}

/* verifies: BMS-REQ-014 */
BMS_TEST(bms_soc, test_ocv_table_round_trip)
{
	const bms_ecm_t *m = &bms_ecm_lg_hg2_25c;
	uint32_t k;

	for (k = 0u; k <= 20u; k++) {
		float s = (float)k / 20.0f;

		CHECK_NEAR(bms_soc_from_ocv(m, bms_ocv(m, s, NULL)), s, 1e-5);
	}
	CHECK_NEAR(bms_soc_from_ocv(m, 2.0f), 0.0, 0.0);
	CHECK_NEAR(bms_soc_from_ocv(m, 4.5f), 1.0, 0.0);
	CHECK_NEAR(bms_soc_from_ocv(m, (m->ocv_v[10] + m->ocv_v[11]) * 0.5f), 0.525, 1e-5);
}

/* verifies: BMS-REQ-014 */
BMS_TEST(bms_soc, test_ocv_slope_and_clamp)
{
	const bms_ecm_t *m = &bms_ecm_lg_hg2_25c;
	float slope = 0.0f;
	float v = bms_ocv(m, 0.525f, &slope);

	CHECK_NEAR(v, (m->ocv_v[10] + m->ocv_v[11]) * 0.5f, 1e-6);
	CHECK_NEAR(slope, (m->ocv_v[11] - m->ocv_v[10]) * 20.0f, 1e-4);
	CHECK_NEAR(bms_ocv(m, -0.3f, NULL), m->ocv_v[0], 0.0);
	CHECK_NEAR(bms_ocv(m, 1.3f, NULL), m->ocv_v[20], 0.0);
}

/* verifies: BMS-REQ-015 */
BMS_TEST(bms_soc, test_coulomb_counter_integrates_current)
{
	const bms_ecm_t *m = &bms_ecm_lg_hg2_25c;
	bms_soc_t s;
	uint32_t k;

	bms_soc_init(&s, m, 0.9f);
	/* 1 C discharge for 360 s in 0.1 s steps removes 10 % */
	for (k = 0u; k < 3600u; k++) {
		bms_soc_step(&s, m, -m->capacity_ah, 3.9f, 0.1f);
	}
	CHECK_NEAR(s.soc_cc, 0.8, 1e-4);
}

/* EKF on a model-generated trace (the model itself as plant, no noise): from a 20 %
 * initial error it has to come back to the true SoC. */
/* verifies: BMS-REQ-015 */
BMS_TEST(bms_soc, test_ekf_converges_on_model_plant)
{
	const bms_ecm_t *m = &bms_ecm_lg_hg2_25c;
	bms_soc_t est;
	float soc = 0.8f;
	float v1 = 0.0f;
	uint32_t k;

	bms_soc_init(&est, m, 0.6f);
	for (k = 0u; k < 6000u; k++) {
		float i = ((k / 100u) % 2u == 0u) ? -3.0f : 1.0f; /* 10 s pulses */
		float a = bms_exp_neg(0.1f / 30.0f);
		float v;

		soc = soc + (i * 0.1f) / (3600.0f * m->capacity_ah);
		v1 = (a * v1) + (0.02f * (1.0f - a) * i);
		v = bms_ocv(m, soc, NULL) + v1 + (0.02f * i);
		bms_soc_step(&est, m, i, v, 0.1f);
	}
	CHECK_NEAR(est.soc, soc, 0.02);
	CHECK_NEAR(est.soc_cc, soc - 0.2f, 1e-4); /* the counter keeps its initial error */
}

/* verifies: BMS-REQ-015 */
BMS_TEST(bms_soc, test_ekf_soc_stays_in_range)
{
	const bms_ecm_t *m = &bms_ecm_lg_hg2_25c;
	bms_soc_t est;
	uint32_t k;

	bms_soc_init(&est, m, 1.4f);
	CHECK_NEAR(est.soc, 1.0, 0.0);
	for (k = 0u; k < 100u; k++) {
		bms_soc_step(&est, m, 0.0f, 4.6f, 0.1f); /* voltage above any OCV */
		CHECK(est.soc <= 1.0f);
	}
	for (k = 0u; k < 100u; k++) {
		bms_soc_step(&est, m, 0.0f, 2.0f, 0.1f);
		CHECK(est.soc >= 0.0f);
	}
	CHECK(est.p00 > 0.0f);
	CHECK(est.p11 > 0.0f);
}
