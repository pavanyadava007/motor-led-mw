"""Aggregate the agent study into results/summary.json and results/summary.md."""

from __future__ import annotations

import json
import statistics
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "agent"))
from agent import split  # noqa: E402

RES = ROOT / "results"
MODES = ["single", "loop", "mutant"]


def ms(xs: list[float]) -> str:
    if not xs:
        return "-"
    if len(xs) == 1:
        return f"{xs[0] * 100:.1f}"
    return f"{statistics.mean(xs) * 100:.1f} +- {statistics.stdev(xs) * 100:.1f}"


def main() -> int:
    mutants = json.loads((RES / "mutants.json").read_text())
    _, held = split(mutants["mutants"])
    held_ids = {m["id"] for m in held}
    ref = json.loads((RES / "ref_mutation.json").read_text())
    ref_killed = {r["id"] for r in ref["rows"] if r["killed"]}
    ref_held = len(ref_killed & held_ids) / len(held_ids)

    runs: dict[str, list[dict]] = {}
    for f in sorted((RES / "agent").glob("*/summary.json")):
        s = json.loads(f.read_text())
        runs.setdefault(s["model"], []).append(s)

    out: dict = {"mutants": mutants["stats"], "heldout_n": len(held_ids),
                 "reference": {"tests": 30, "all": ref["score"], "heldout": round(ref_held, 4)},
                 "models": {}}
    md = ["# Agent study summary", "",
          f"Mutants: {mutants['stats']['kept']} used ({mutants['stats']['candidates']} candidates, "
          f"{mutants['stats']['no_compile']} do not compile, {mutants['stats']['equivalent']} equivalent "
          f"by object code). Held-out half: {len(held_ids)} mutants the agent never sees.", "",
          f"Hand-written reference suite (30 tests): {ref['score'] * 100:.1f} % of all mutants, "
          f"{ref_held * 100:.1f} % of the held-out half.", "",
          "| model | runs | mode | held-out kill % (mean +- sd) | all mutants % | tests kept | "
          "ref + agent, held-out % |", "|---|---|---|---|---|---|---|"]
    for model, rs in runs.items():
        mo: dict = {"runs": len(rs), "modes": {}}
        for mode in MODES:
            ok = [r for r in rs if "heldout" in r["scores"].get(mode, {})]
            held_s = [r["scores"][mode]["heldout"] for r in ok]
            all_s = [r["scores"][mode]["all"] for r in ok]
            tests = [sum(i.get("tests", 0) for i in r[f"{mode}_cleanup"].values()) for r in ok]
            union = [len((set(r["scores"][mode]["killed_ids"]) | ref_killed) & held_ids) / len(held_ids)
                     for r in ok]
            mo["modes"][mode] = {"heldout": held_s, "all": all_s, "tests_kept": tests,
                                 "ref_union_heldout": union}
            md.append(f"| {model} | {len(ok)} | {mode} | {ms(held_s)} | {ms(all_s)} | "
                      f"{statistics.mean(tests) if tests else 0:.0f} | {ms(union)} |")
        first = [r["single_first_try"] for r in rs]
        mo["first_try_build_rate"] = round(
            sum(m["built"] for f in first for m in f.values()) / max(1, 4 * len(first)), 3)
        mo["first_try_files_all_pass"] = round(
            sum(m["built"] and m["failed"] == 0 and m["passed"] > 0 for f in first for m in f.values())
            / max(1, 4 * len(first)), 3)
        mo["llm_calls_mean"] = statistics.mean(r["llm_calls"] for r in rs)
        mo["output_tokens_mean"] = statistics.mean(r["output_tokens"] for r in rs)
        mo["llm_minutes_mean"] = round(statistics.mean(r["llm_seconds"] for r in rs) / 60, 1)
        mo["wall_minutes_mean"] = round(statistics.mean(r["wall_seconds_agent"] for r in rs) / 60, 1)
        out["models"][model] = mo
    md += ["", "| model | first-try files that build | first-try files with all tests passing | "
           "LLM calls per run | output tokens per run | agent wall time per run (min) |",
           "|---|---|---|---|---|---|"]
    for model, mo in out["models"].items():
        md.append(f"| {model} | {mo['first_try_build_rate'] * 100:.0f} % | "
                  f"{mo['first_try_files_all_pass'] * 100:.0f} % | {mo['llm_calls_mean']:.0f} | "
                  f"{mo['output_tokens_mean']:.0f} | {mo['wall_minutes_mean']} |")
    (RES / "summary.json").write_text(json.dumps(out, indent=1))
    (RES / "summary.md").write_text("\n".join(md) + "\n")
    print("\n".join(md))
    return 0


if __name__ == "__main__":
    sys.exit(main())
