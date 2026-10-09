/* State of charge: Coulomb counting and an Extended Kalman Filter on a 1-RC
 * equivalent circuit model (ECM). Arithmetic is IEEE-754 binary32 (float) only:
 * no libm, no double, so the same code gives the same bits on an FPU-less Cortex-M3
 * (soft float), a Cortex-M4F/M33 (single-precision FPU) and an x86-64 host (SSE),
 * provided FP contraction is off (-ffp-contract=off).
 *
 * Model (current i in A, POSITIVE = CHARGE; dt in s):
 *   soc[k+1] = soc[k] + eta * i * dt / (3600 * Q)        eta = 1 (no charge loss)
 *   v1[k+1]  = a * v1[k] + R1(soc) * (1 - a) * i          a = exp(-dt / tau(soc))
 *   v[k]     = OCV(soc[k]) + v1[k] + R0(soc) * i[k]
 * OCV(soc) is a table on a uniform SoC grid of BMS_OCV_N points (0 .. 1), linear in
 * between; R0, R1 and tau are tables on a uniform grid of BMS_PAR_N points. Outside
 * 0..1 the tables are clamped. exp(-x) uses bms_exp_neg() (range halving + 5-term
 * Taylor, relative error < 1e-6 for 0 <= x <= 20).
 *
 * EKF: state x = [soc, v1], F = diag(1, a), H = [dOCV/dsoc, 1] (slope of the table
 * segment), process noise diag(q_soc, q_v1), measurement noise r_v. After each update
 * soc is clamped to [0, 1]. The Coulomb counter runs in parallel from its own initial
 * value and is never corrected.
 *
 * What the model does NOT include: OCV hysteresis, temperature dependence inside one
 * parameter set (one set per temperature), ageing / capacity fade, a second RC pair.
 */
#ifndef BMS_SOC_H
#define BMS_SOC_H

#include "bms/bms_types.h"

#define BMS_OCV_N 21u
#define BMS_PAR_N 11u

typedef struct {
	float capacity_ah;
	float ocv_v[BMS_OCV_N];  /* OCV at soc = k / (BMS_OCV_N - 1), strictly increasing */
	float r0_ohm[BMS_PAR_N]; /* at soc = k / (BMS_PAR_N - 1) */
	float r1_ohm[BMS_PAR_N];
	float tau_s[BMS_PAR_N];  /* > 0 */
	float q_soc;             /* process noise variance per step */
	float q_v1;
	float r_v;               /* measurement noise variance, V^2 */
	float p0_soc;            /* initial variance of soc */
	float p0_v1;
} bms_ecm_t;

typedef struct {
	float soc;      /* EKF estimate, 0..1 */
	float v1;       /* RC voltage, V */
	float p00;      /* covariance */
	float p01;
	float p11;
	float soc_cc;   /* Coulomb-counting estimate (unclamped) */
	float v_pred;   /* model voltage before the update, V */
} bms_soc_t;

/* exp(-x) for x >= 0 without libm. */
float bms_exp_neg(float x);

/* OCV and its slope (V per unit SoC) at soc. slope may be NULL. */
float bms_ocv(const bms_ecm_t *m, float soc, float *slope);

/* Inverse OCV lookup (rest voltage -> soc), clamped to 0..1. */
float bms_soc_from_ocv(const bms_ecm_t *m, float v);

void bms_soc_init(bms_soc_t *s, const bms_ecm_t *m, float soc0);

/* One step with current i_a (A, + = charge), measured terminal voltage v_meas (V) and
 * step length dt_s (s, > 0). Updates both the Coulomb counter and the EKF. */
void bms_soc_step(bms_soc_t *s, const bms_ecm_t *m, float i_a, float v_meas, float dt_s);

#endif
