"""Fit the 1-RC ECM of the LG 18650HG2 per temperature on the TRAINING tests only:
C/20 discharge -> capacity and OCV table, HPPC -> R0/R1/tau tables, HPPC -> EKF noise.
Drive cycles are never used here (they are the held-out set of soc_eval.py).

Writes bms/src/bms_ecm_lg_hg2.c and results/bms/ecm_fit.json.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import numpy as np
from scipy.optimize import least_squares

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lgdata  # noqa: E402
from ecm_ref import EKF_DEFAULT, Ecm, ekf_run, simulate  # noqa: E402

ROOT = lgdata.ROOT
RES = ROOT / "results" / "bms"
TEMPS = {"25degC": "25c", "0degC": "0c"}
N_OCV, N_PAR = 21, 11


def hppc_rest_points(temp: str, q: float) -> tuple[np.ndarray, np.ndarray]:
    """(soc, voltage) at the end of every HPPC rest longer than 300 s (relaxed OCV)."""
    d = lgdata.load_hppc(temp)
    socs, vs = [], []
    start = 0
    for k in range(1, len(d.t) + 1):
        if k == len(d.t) or d.step[k] != d.step[start]:
            if d.status[start] == "PAU" and d.t[k - 1] - d.t[start] > 300.0:
                socs.append(1.0 + d.ah[k - 1] / q)
                vs.append(d.v[k - 1])
            start = k
    return np.array(socs), np.array(vs)


def capacity_and_ocv(temp: str, r_total: float) -> tuple[float, np.ndarray, dict]:
    d = lgdata.load(lgdata.find(temp, "C20DisCh"))
    m = d.status == "DCH"
    ah, v, i = d.ah[m], d.v[m], d.i[m]
    q = float(-ah.min())
    soc = 1.0 + ah / q
    # terminal voltage at C/20 minus the resistive drop gives a pseudo-OCV
    ocv_pts = v - i * r_total
    order = np.argsort(soc)
    grid = np.linspace(0.0, 1.0, N_OCV)
    ocv = np.interp(grid, soc[order], ocv_pts[order])
    # shift the C/20 curve to the relaxed HPPC rest voltages (OCV after > 300 s rest)
    rs, rv = hppc_rest_points(temp, q)
    delta = rv - np.interp(rs, grid, ocv)
    o = np.argsort(rs)
    ocv = ocv + np.interp(grid, rs[o], delta[o])
    for k in range(1, N_OCV):  # strictly increasing for the inverse lookup
        ocv[k] = max(ocv[k], ocv[k - 1] + 1e-4)
    info = {
        "c20_file": d.name,
        "capacity_ah": q,
        "c20_current_a": float(np.median(i)),
        "r_total_used_for_ocv_ohm": r_total,
        "hppc_rest_points": [[float(a), float(b)] for a, b in zip(rs, rv, strict=True)],
    }
    return q, ocv, info


def fit_rc(temp: str, q: float, ocv: np.ndarray) -> tuple[Ecm, dict]:
    d = lgdata.load_hppc(temp)
    soc = np.clip(1.0 + d.ah / q, 0.0, 1.0)
    dt = np.diff(d.t, prepend=d.t[0])
    base = Ecm(q, ocv, np.full(N_PAR, 0.03), np.full(N_PAR, 0.02), np.full(N_PAR, 30.0))

    def unpack(x: np.ndarray) -> Ecm:
        return Ecm(q, ocv, x[:N_PAR], x[N_PAR : 2 * N_PAR], x[2 * N_PAR :])

    def resid(x: np.ndarray) -> np.ndarray:
        return simulate(unpack(x), soc, d.i, dt) - d.v

    x0 = np.concatenate([base.r0, base.r1, base.tau])
    lo = np.concatenate([np.full(N_PAR, 1e-3), np.full(N_PAR, 1e-4), np.full(N_PAR, 1.0)])
    hi = np.concatenate([np.full(N_PAR, 0.3), np.full(N_PAR, 0.3), np.full(N_PAR, 600.0)])
    sol = least_squares(resid, x0, bounds=(lo, hi), x_scale=np.abs(x0), max_nfev=200)
    e = resid(sol.x)
    m = unpack(sol.x)
    return m, {
        "hppc_file": d.name,
        "samples": int(len(d.t)),
        "fit_rmse_v": float(np.sqrt(np.mean(e**2))),
        "fit_max_abs_v": float(np.max(np.abs(e))),
        "nfev": int(sol.nfev),
    }


def tune_noise(temp: str, m: Ecm) -> dict:
    """Small grid over (r_v, q_soc) on the HPPC record, starting 20 % SoC too low."""
    d = lgdata.load_hppc(temp)
    truth = 1.0 + d.ah / m.capacity_ah
    dt = np.diff(d.t, prepend=d.t[0])
    grid = []
    for r_v in (1e-4, 1e-3, 1e-2, 1e-1):
        for q_soc in (1e-10, 1e-8, 1e-6):
            p = dict(EKF_DEFAULT, r_v=r_v, q_soc=q_soc)
            est, _ = ekf_run(m, p, d.i[1:], d.v[1:], dt[1:], truth[0] - 0.2)
            rmse = float(np.sqrt(np.mean((est - truth[1:]) ** 2)))
            grid.append({"r_v": r_v, "q_soc": q_soc, "hppc_soc_rmse": rmse})
    best = min(grid, key=lambda g: g["hppc_soc_rmse"])
    return {"grid": grid, "chosen": dict(EKF_DEFAULT, r_v=best["r_v"], q_soc=best["q_soc"])}


def c_array(xs) -> str:
    return "{" + ", ".join(f"{float(np.float32(x))!r}f" for x in xs) + "}"


def main() -> None:
    out, cdefs = {}, []
    for temp, tag in TEMPS.items():
        q, ocv, cinfo = capacity_and_ocv(temp, 0.0)
        m, finfo = fit_rc(temp, q, ocv)
        # second pass: OCV corrected by the fitted resistance at mid SoC, then refit RC
        r_mid = float(m.r0[N_PAR // 2] + m.r1[N_PAR // 2])
        q, ocv, cinfo = capacity_and_ocv(temp, r_mid)
        m, finfo = fit_rc(temp, q, ocv)
        noise = tune_noise(temp, m)
        p = noise["chosen"]
        out[temp] = {
            "capacity": cinfo,
            "rc_fit": finfo,
            "noise_tuning_on_hppc": noise,
            "ocv_v": ocv.tolist(),
            "r0_ohm": m.r0.tolist(),
            "r1_ohm": m.r1.tolist(),
            "tau_s": m.tau.tolist(),
        }
        cdefs.append(
            f"const bms_ecm_t bms_ecm_lg_hg2_{tag} = {{\n"
            f"\t.capacity_ah = {float(np.float32(q))!r}f,\n"
            f"\t.ocv_v = {c_array(ocv)},\n\t.r0_ohm = {c_array(m.r0)},\n"
            f"\t.r1_ohm = {c_array(m.r1)},\n\t.tau_s = {c_array(m.tau)},\n"
            + "".join(
                f"\t.{k} = {float(np.float32(p[k]))!r}f,\n"
                for k in ("q_soc", "q_v1", "r_v", "p0_soc", "p0_v1")
            )
            + "};\n"
        )
        print(temp, "Q", round(q, 4), "HPPC fit RMSE V", round(finfo["fit_rmse_v"], 5), "noise", p)
    src = (
        "/* GENERATED by bms/tools/fit_ecm.py - do not edit.\n"
        " * LG 18650HG2 1-RC ECM fitted on the McMaster C/20 and HPPC tests (Kollmeyer et al.,\n"
        " * Mendeley Data doi:10.17632/cp3473x7xv.3, CC BY 4.0). Drive cycles not used. */\n"
        '#include "bms/bms_ecm_lg_hg2.h"\n\n' + "\n".join(cdefs)
    )
    (ROOT / "bms" / "src" / "bms_ecm_lg_hg2.c").write_text(src.replace("inf", "INFINITY"))
    RES.mkdir(parents=True, exist_ok=True)
    out["note"] = "fit uses C/20 + HPPC only; truth SoC = 1 + Ah/Q (tester Ah counter)"
    out["exp_check"] = math.exp(-0.1 / 30.0)
    (RES / "ecm_fit.json").write_text(json.dumps(out, indent=1))


if __name__ == "__main__":
    main()
