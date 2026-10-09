"""SoC validation on HELD-OUT drive cycles of the real LG 18650HG2 data (McMaster,
Kollmeyer et al., CC BY 4.0). The ECM and the EKF noise were fitted on C/20 + HPPC only
(fit_ecm.py); none of these cycles was used for fitting or tuning.

The SAME C code (bms/src/bms_soc.c) runs
  - on the host (x86-64, gcc, float32 SSE)               -> metrics
  - on an emulated Cortex-M4F (QEMU mps2-an386, FPU)     -> parity check against the host
and a float64 Python mirror (ecm_ref.py) gives a third reference.

Reference SoC = 1 + Ah / Q, from the tester's Ah counter (0.1 % FS current accuracy) and
the C/20 capacity Q at that temperature. Scenarios per cycle:
  init_true      estimators start at the reference SoC
  init_minus20   estimators start 20 % SoC too low (0.8 instead of 1.0)
  init_true_bias_minus50mA  correct start, but the current sensor reads 50 mA more
                 discharge than the lab reference (offset seen by both estimators)
Writes results/bms/soc_eval.json and PNG plots.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lgdata  # noqa: E402
from ecm_ref import Ecm, ekf_run  # noqa: E402

ROOT = lgdata.ROOT
RES = ROOT / "results" / "bms"
PIL = ROOT / "build" / "bms" / "pil"
HOST = ROOT / "build" / "bms" / "host" / "pil_soc"
ELF = ROOT / "build" / "bms" / "arm" / "pil_soc.elf"
QEMU_IMAGE = os.environ.get("MLMW_QEMU_IMAGE", "zqemu:8.2")
CYCLES = {
    "25degC": ["UDDS", "US06", "LA92", "Mixed1", "Mixed2"],
    "0degC": ["UDDS", "US06", "LA92", "HWFET", "Mixed1", "Mixed2"],
}
EXCLUDED = {"25degC/HWFET": "file 551_HWFET.csv holds only a 10 min rest at 4.18 V, no drive cycle"}
SCEN = {"init_true": (0.0, 0.0), "init_minus20": (-0.2, 0.0), "init_true_bias_minus50mA": (0.0, -0.05)}
ECM_ID = {"25degC": 0.0, "0degC": 1.0}


def metrics(err: np.ndarray, t: np.ndarray) -> dict:
    late = t >= 600.0
    stay = np.where(np.abs(err) > 0.05)[0]
    conv = 0.0 if len(stay) == 0 else float(t[min(stay[-1] + 1, len(t) - 1)])
    return {
        "rmse_pct": float(np.sqrt(np.mean(err**2)) * 100),
        "max_abs_pct": float(np.max(np.abs(err)) * 100),
        "max_abs_after_600s_pct": float(np.max(np.abs(err[late])) * 100) if late.any() else None,
        "final_abs_pct": float(abs(err[-1]) * 100),
        "time_to_stay_within_5pct_s": conv,
    }


def build_runs(fit: dict) -> list[dict]:
    runs = []
    for temp, cycles in CYCLES.items():
        q = float(np.float32(fit[temp]["capacity"]["capacity_ah"]))
        for cyc in cycles:
            d = lgdata.load(lgdata.find(temp, cyc))
            truth = 1.0 + d.ah / q
            for scen, (off, bias) in SCEN.items():
                runs.append(
                    {
                        "temp": temp,
                        "cycle": cyc,
                        "file": d.name,
                        "scenario": scen,
                        "q": q,
                        "t": d.t[1:],
                        "dt": np.diff(d.t),
                        "i": d.i[1:] + bias,
                        "v": d.v[1:],
                        "truth": truth[1:],
                        "soc0": float(truth[0] + off),
                        "i_min": float(d.i.min()),
                        "i_max": float(d.i.max()),
                        "temp_mean_c": float(d.temp.mean()),
                        "hours": float(d.t[-1] / 3600),
                    }
                )
    return runs


def write_input(runs: list[dict]) -> None:
    PIL.mkdir(parents=True, exist_ok=True)
    parts = []
    for r in runs:
        parts.append(np.array([[-1.0, r["soc0"], ECM_ID[r["temp"]], 0.0]], dtype="<f4"))
        rec = np.zeros((len(r["dt"]), 4), dtype="<f4")
        rec[:, 0], rec[:, 1], rec[:, 2] = r["dt"], r["i"], r["v"]
        parts.append(rec)
    np.concatenate(parts).tofile(PIL / "pil_in.bin")


def run_host() -> np.ndarray:
    (PIL / "pil_out.bin").unlink(missing_ok=True)
    subprocess.run([str(HOST)], cwd=ROOT, check=True, capture_output=True)
    return np.fromfile(PIL / "pil_out.bin", dtype="<f4").reshape(-1, 3)


def run_qemu() -> tuple[np.ndarray | None, dict]:
    (PIL / "pil_out.bin").unlink(missing_ok=True)
    cmd = [
        "docker",
        "run",
        "--rm",
        "--network",
        "none",
        "--user",
        f"{os.getuid()}:{os.getgid()}",
        "-v",
        f"{ROOT}:{ROOT}",
        "-w",
        str(ROOT),
        QEMU_IMAGE,
        "qemu-system-arm",
        "-M",
        "mps2-an386",
        "-cpu",
        "cortex-m4",
        "-nographic",
        "-monitor",
        "none",
        "-semihosting-config",
        "enable=on,target=native",
        "-kernel",
        str(ELF),
    ]
    t0 = time.time()
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=3600)
    info = {
        "exit_code": p.returncode,
        "stdout": (p.stdout + p.stderr)[-200:],
        "host_wall_s": round(time.time() - t0, 1),
        "machine": "QEMU 8.2 mps2-an386 Cortex-M4F (emulated)",
    }
    if p.returncode != 0 or not (PIL / "pil_out.bin").exists():
        return None, info
    return np.fromfile(PIL / "pil_out.bin", dtype="<f4").reshape(-1, 3), info


def plot(runs: list[dict], out_host: np.ndarray, offs: list[int], picks: list[tuple[str, str]]) -> list[str]:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    files = []
    fig, axes = plt.subplots(len(picks), 1, figsize=(9, 3.1 * len(picks)), dpi=110)
    for ax, (temp, cyc) in zip(np.atleast_1d(axes), picks, strict=True):
        k = next(
            j
            for j, r in enumerate(runs)
            if r["temp"] == temp and r["cycle"] == cyc and r["scenario"] == "init_minus20"
        )
        r, o = runs[k], out_host[offs[k] : offs[k] + len(runs[k]["dt"])]
        th = r["t"] / 3600
        ax.plot(th, r["truth"] * 100, color="#52514e", lw=2.0, label="reference (lab Ah counter)")
        ax.plot(th, o[:, 0] * 100, color="#eb6834", lw=1.6, label="Coulomb counting")
        ax.plot(th, o[:, 1] * 100, color="#2a78d6", lw=1.6, label="EKF (1-RC ECM)")
        ax.set_title(
            f"{cyc} at {temp.replace('degC', ' degC')}: estimators start 20 % SoC too low",
            fontsize=10,
            loc="left",
            color="#0b0b0b",
        )
        ax.set_ylabel("SoC (%)", color="#52514e")
        ax.set_ylim(-20, 105)
        ax.grid(True, color="#e6e5e0", lw=0.8)
        for s in ("top", "right"):
            ax.spines[s].set_visible(False)
        ax.tick_params(colors="#52514e")
        ax.legend(frameon=False, fontsize=8, loc="upper right")
    np.atleast_1d(axes)[-1].set_xlabel("time (h)", color="#52514e")
    fig.text(
        0.01,
        0.005,
        "Real cell data: LG 18650HG2, McMaster (Kollmeyer et al.), CC BY 4.0. "
        "Held-out cycles. Estimates from the C code on the host.",
        fontsize=7,
        color="#52514e",
    )
    fig.tight_layout(rect=(0, 0.02, 1, 1))
    f = RES / "soc_heldout.png"
    fig.savefig(f, facecolor="#fcfcfb")
    files.append(str(f.relative_to(ROOT)))
    plt.close(fig)
    return files


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-qemu", action="store_true")
    ap.add_argument("--no-python-ref", action="store_true")
    args = ap.parse_args()
    fit = json.loads((RES / "ecm_fit.json").read_text())
    runs = build_runs(fit)
    write_input(runs)
    offs = list(np.cumsum([0] + [len(r["dt"]) for r in runs])[:-1])
    out_host = run_host()
    assert out_host.shape[0] == sum(len(r["dt"]) for r in runs)
    res: dict = {
        "data": "LG 18650HG2 (McMaster, Kollmeyer et al.), Mendeley Data doi:10.17632/cp3473x7xv.3, "
        "CC BY 4.0. REAL measured data.",
        "held_out": CYCLES,
        "excluded": EXCLUDED,
        "scenarios": {k: {"soc0_offset": v[0], "current_bias_a": v[1]} for k, v in SCEN.items()},
        "estimator": "C float32 bms_soc_step, 1-RC ECM, params from C/20 + HPPC only",
        "samples_per_scenario": int(sum(len(r["dt"]) for r in runs) // len(SCEN)),
        "runs": [],
    }
    for k, r in enumerate(runs):
        o = out_host[offs[k] : offs[k] + len(r["dt"])].astype(np.float64)
        row = {
            kk: r[kk]
            for kk in ("temp", "cycle", "file", "scenario", "soc0", "i_min", "i_max", "temp_mean_c", "hours")
        }
        row["samples"] = len(r["dt"])
        row["reference_soc_end_pct"] = float(r["truth"][-1] * 100)
        row["cc"] = metrics(o[:, 0] - r["truth"], r["t"])
        row["ekf"] = metrics(o[:, 1] - r["truth"], r["t"])
        vp = o[:, 2]
        row["ekf_voltage_residual_rmse_mv"] = float(np.sqrt(np.mean((vp - r["v"]) ** 2)) * 1000)
        res["runs"].append(row)
    # Python float64 mirror, scenario init_minus20 only
    if not args.no_python_ref:
        diffs = []
        for k, r in enumerate(runs):
            if r["scenario"] != "init_minus20":
                continue
            e = fit[r["temp"]]
            m = Ecm(
                r["q"],
                np.array(e["ocv_v"]),
                np.array(e["r0_ohm"]),
                np.array(e["r1_ohm"]),
                np.array(e["tau_s"]),
            )
            p = e["noise_tuning_on_hppc"]["chosen"]
            est, _ = ekf_run(
                m,
                p,
                r["i"].astype(np.float32).astype(np.float64),
                r["v"].astype(np.float32).astype(np.float64),
                r["dt"].astype(np.float32).astype(np.float64),
                r["soc0"],
            )
            diffs.append(float(np.max(np.abs(est - out_host[offs[k] : offs[k] + len(r["dt"]), 1]))))
        res["c_float32_vs_python_float64_max_abs_soc_diff"] = max(diffs)
    if not args.no_qemu:
        out_t, info = run_qemu()
        res["qemu"] = info
        if out_t is not None and out_t.shape == out_host.shape:
            bit = out_t.view(np.uint32) == out_host.view(np.uint32)
            res["parity_target_vs_host"] = {
                "outputs_compared": int(out_host.size),
                "bit_exact_fraction": float(bit.mean()),
                "max_abs_diff": float(np.max(np.abs(out_t.astype(np.float64) - out_host))),
                "note": "Cortex-M4F single-precision FPU (emulated) vs x86-64 SSE, gcc -ffp-contract=off",
            }
        else:
            res["parity_target_vs_host"] = None
    res["plots"] = plot(runs, out_host, offs, [("25degC", "UDDS"), ("0degC", "US06")])
    summ = {}
    for row in res["runs"]:
        summ.setdefault(row["scenario"], {})[f"{row['temp']}/{row['cycle']}"] = {
            "cc_rmse": round(row["cc"]["rmse_pct"], 2),
            "ekf_rmse": round(row["ekf"]["rmse_pct"], 2),
            "ekf_max_after_600s": round(row["ekf"]["max_abs_after_600s_pct"], 2),
        }
    res["summary_pct"] = summ
    (RES / "soc_eval.json").write_text(json.dumps(res, indent=1))
    print(json.dumps(summ, indent=1))
    print({k: res.get(k) for k in ("parity_target_vs_host", "c_float32_vs_python_float64_max_abs_soc_diff")})


if __name__ == "__main__":
    main()
