"""Python (float64) reference of the C model in bms/src/bms_soc.c: same equations, same
table lookup, same EKF. Used for the fit and as a second check of the C code."""

from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np

EKF_DEFAULT = {"q_soc": 1e-8, "q_v1": 1e-7, "r_v": 1e-3, "p0_soc": 0.04, "p0_v1": 1e-4}


@dataclass
class Ecm:
    capacity_ah: float
    ocv: np.ndarray
    r0: np.ndarray
    r1: np.ndarray
    tau: np.ndarray


def lut(t: np.ndarray, soc: float) -> tuple[float, float]:
    n = len(t)
    pos = min(max(soc, 0.0), 1.0) * (n - 1)
    i = min(int(pos), n - 2)
    d = float(t[i + 1] - t[i])
    return float(t[i]) + (pos - i) * d, d * (n - 1)


def simulate(m: Ecm, soc: np.ndarray, i: np.ndarray, dt: np.ndarray) -> np.ndarray:
    """Terminal voltage for a given (true) SoC trajectory, used by the fit."""
    g = np.linspace(0.0, 1.0, len(m.r0))
    s = np.clip(soc, 0.0, 1.0)
    r0 = np.interp(s, g, m.r0)
    r1 = np.interp(s, g, m.r1)
    a = np.exp(-dt / np.interp(s, g, m.tau))
    b = (r1 * (1.0 - a) * i).tolist()
    al = a.tolist()
    v1 = [0.0] * len(al)
    acc = 0.0
    for k in range(len(al)):
        acc = al[k] * acc + b[k]
        v1[k] = acc
    ocv = np.interp(s, np.linspace(0.0, 1.0, len(m.ocv)), m.ocv)
    return ocv + np.array(v1) + r0 * i


def ekf_run(m: Ecm, p: dict, i: np.ndarray, v: np.ndarray, dt: np.ndarray, soc0: float):
    soc, v1, p00, p01, p11, cc = min(max(soc0, 0.0), 1.0), 0.0, p["p0_soc"], 0.0, p["p0_v1"], soc0
    est = np.empty(len(i))
    ccs = np.empty(len(i))
    for k, (ik, vk, dk) in enumerate(zip(i.tolist(), v.tolist(), dt.tolist(), strict=True)):
        dq = ik * dk / (3600.0 * m.capacity_ah)
        r0, _ = lut(m.r0, soc)
        r1, _ = lut(m.r1, soc)
        tau, _ = lut(m.tau, soc)
        a = math.exp(-dk / tau)
        cc += dq
        sp = soc + dq
        v1p = a * v1 + r1 * (1.0 - a) * ik
        q00, q01, q11 = p00 + p["q_soc"], a * p01, a * a * p11 + p["q_v1"]
        ocv, h = lut(m.ocv, sp)
        vp = ocv + v1p + r0 * ik
        sv = h * h * q00 + 2.0 * h * q01 + q11 + p["r_v"]
        hp0, hp1 = h * q00 + q01, h * q01 + q11
        k0, k1 = hp0 / sv, hp1 / sv
        e = vk - vp
        sp, v1p = sp + k0 * e, v1p + k1 * e
        p00, p01, p11 = q00 - k0 * hp0, q01 - k0 * hp1, q11 - k1 * hp1
        soc, v1 = min(max(sp, 0.0), 1.0), v1p
        est[k], ccs[k] = soc, cc
    return est, ccs
