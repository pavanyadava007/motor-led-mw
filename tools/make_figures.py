"""README figures from the committed result files (no new measurements).

results/node_telemetry.json -> results/figures/node_demo.png   (speed + LED derating over node time)
results/summary.json        -> results/figures/mutation_scores.png (held-out mutants killed per test source)
"""

from __future__ import annotations

import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
RES = ROOT / "results"
OUT = RES / "figures"

SURFACE = "#fcfcfb"
INK = "#0b0b0b"
INK2 = "#52514e"
GRID = "#e4e3df"
BLUE = "#2a78d6"
ORANGE = "#eb6834"
GRAY = "#8a8984"

plt.rcParams.update({
    "figure.facecolor": SURFACE, "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE,
    "axes.edgecolor": GRID, "axes.labelcolor": INK2, "xtick.color": INK2, "ytick.color": INK2,
    "text.color": INK, "font.size": 10, "axes.spines.top": False, "axes.spines.right": False,
    "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.8,
})


def node_demo() -> Path:
    t = json.loads((RES / "node_telemetry.json").read_text())
    t0 = t[0]["node_ms"]
    x = [(s["node_ms"] - t0) / 1000 for s in t]
    fig, (a1, a2, a3) = plt.subplots(3, 1, figsize=(9, 6.6), sharex=True,
                                     gridspec_kw={"height_ratios": [2.2, 1, 1]})
    a1.plot(x, [s["target"] for s in t], color=GRAY, lw=1.5, ls="--", label="commanded speed")
    a1.plot(x, [s["rpm"] for s in t], color=BLUE, lw=2, label="measured speed (from hall period)")
    a1.set_ylabel("rpm")
    a1.legend(loc="upper right", frameon=False)
    a1.set_title("Node demo in QEMU: signed UDP commands drive the simulated motor and LED board",
                 loc="left", fontsize=11)
    a2.plot(x, [s["led_temp"] for s in t], color=ORANGE, lw=2)
    a2.axhline(85, color=GRAY, lw=1, ls=":")
    a2.text(x[0], 87, "derating starts at 85 C", color=INK2, fontsize=9)
    a2.set_ylabel("LED board, C")
    a3.plot(x, [s["led_duty"][0] for s in t], color=ORANGE, lw=2)
    a3.set_ylabel("red PWM duty")
    a3.set_xlabel("node time (s)")
    fig.text(0.01, 0.005, "Source: results/node_telemetry.json. Emulated board, software motor/LED models, "
             "not hardware timings.", color=INK2, fontsize=8)
    fig.tight_layout(rect=(0, 0.02, 1, 1))
    p = OUT / "node_demo.png"
    fig.savefig(p, dpi=110)
    plt.close(fig)
    return p


def mutation_scores() -> Path:
    s = json.loads((RES / "summary.json").read_text())
    rows: list[tuple[str, float, float, str]] = []
    names = {"qwen3-coder:30b": "Qwen3-Coder 30B", "qwen2.5-coder:7b": "Qwen2.5-Coder 7B"}
    for model in ("qwen2.5-coder:7b", "qwen3-coder:30b"):
        for mode in ("single", "loop", "mutant"):
            v = s["models"][model]["modes"][mode]["heldout"]
            m = sum(v) / len(v)
            sd = (sum((a - m) ** 2 for a in v) / (len(v) - 1)) ** 0.5
            rows.append((f"{names[model]}, {mode}", m * 100, sd * 100, BLUE))
    rows.append(("hand-written reference (30 tests)", s["reference"]["heldout"] * 100, 0.0, GRAY))
    u = s["models"]["qwen3-coder:30b"]["modes"]["mutant"]["ref_union_heldout"]
    rows.append(("reference + 30B agent (mutant mode)", sum(u) / len(u) * 100, 0.0, ORANGE))

    fig, ax = plt.subplots(figsize=(9, 4.2))
    y = list(range(len(rows)))[::-1]
    ax.barh(y, [r[1] for r in rows], color=[r[3] for r in rows], height=0.62)
    sd_rows = [(yi, r) for yi, r in zip(y, rows, strict=True) if r[2] > 0]
    ax.errorbar([r[1] for _, r in sd_rows], [yi for yi, _ in sd_rows], xerr=[r[2] for _, r in sd_rows],
                fmt="none", ecolor=INK2, lw=1, capsize=3)
    for yi, r in zip(y, rows, strict=True):
        ax.text(r[1] + r[2] + 1.2, yi, f"{r[1]:.1f} %", va="center", color=INK, fontsize=9)
    ax.set_yticks(y, [r[0] for r in rows])
    ax.set_xlim(0, 100)
    ax.grid(axis="y", visible=False)
    ax.set_xlabel("held-out mutants killed (%), mean of 3 seeds, bars = 1 sd")
    ax.set_title("Which tests catch injected bugs? (93 held-out mutants)",
                 loc="left", fontsize=11)
    fig.text(0.01, 0.01, "Blue: agent-written tests alone. Gray: hand-written suite. Orange: both together. "
             "Source: results/summary.json", color=INK2, fontsize=8)
    fig.tight_layout(rect=(0, 0.03, 1, 1))
    p = OUT / "mutation_scores.png"
    fig.savefig(p, dpi=110)
    plt.close(fig)
    return p


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for p in (node_demo(), mutation_scores()):
        print(p.relative_to(ROOT), p.stat().st_size, "bytes")


if __name__ == "__main__":
    main()
