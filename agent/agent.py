"""Test-writing agent for the middleware, scored on the emulated target.

For each module (lin, bldc, led, cmd) a local code model writes a Zephyr ztest
file from the header (the specification) and the source. Three modes, each one
continuing from the previous, so one run yields three snapshots:

  single  one generation per module, no feedback
  loop    the agent sees compiler errors and on-target results (QEMU, Cortex-M3)
          and rewrites the file, up to --fix-iters times
  mutant  the agent also sees mutants that its tests do not kill, taken only from
          the TRAIN half of the mutants, and adds tests (--mutant-rounds rounds)

Every snapshot then goes through the same cleanup: a file that does not build
is dropped, and tests that fail on the original code are removed (a regression
suite must pass on the code it protects). Scores are computed by tools/mutate.py
on the HELD-OUT half, which the agent never sees.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import sys
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import mutate  # noqa: E402
import target  # noqa: E402

MODULES = ["lin", "bldc", "led", "cmd"]
OLLAMA = "http://127.0.0.1:11436/api/chat"

SYSTEM = """You are an embedded software test engineer. You write Zephyr ztest unit tests in C99
for one module of a small middleware library for automotive motor control and LED nodes.
The tests run on an Arm Cortex-M3 target (Zephyr, QEMU), not on a PC.

Rules:
- #include "mlmw_test.h" (it includes <zephyr/ztest.h>) and the module header given to you
  (e.g. "mlmw/lin.h"), plus <string.h> if needed.
- Check values with EXPECT_EQ(actual, expected), EXPECT_TRUE(cond), EXPECT_FALSE(cond) from mlmw_test.h.
  On failure they print the actual and the expected value. zassert_mem_equal is also allowed.
- Define tests with ZTEST(SUITE, test_name); every test name starts with test_. Define the suite once at
  the end with ZTEST_SUITE(SUITE, NULL, NULL, NULL, NULL, NULL) (or with a before function if you need one).
- Write MANY SMALL tests: one behaviour per test, at most 3 checks per test. A test stops at its first
  failing check, so small tests keep the good checks alive.
- Derive expected values from the specification in the header comments and work them out by hand.
  Do not call the function under test to compute its own expected value.
- Cover normal values, boundaries (exactly at a limit, one above, one below), invalid inputs and every branch.
- No dynamic memory, no printf.
- Reply with exactly one complete C file in a single ```c code block and nothing else."""


def chat(model: str, messages: list[dict], seed: int, temperature: float) -> tuple[str, dict]:
    body = json.dumps({"model": model, "messages": messages, "stream": False,
                       "options": {"temperature": temperature, "seed": seed,
                                   "num_ctx": 16384, "num_predict": 6144}}).encode()
    req = urllib.request.Request(OLLAMA, data=body, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=1800) as r:
        d = json.load(r)
    return d["message"]["content"], {"prompt_tokens": d.get("prompt_eval_count", 0),
                                     "output_tokens": d.get("eval_count", 0),
                                     "seconds": d.get("total_duration", 0) / 1e9}


def extract_c(text: str) -> str:
    m = re.findall(r"```(?:c|C)?\s*\n(.*?)```", text, re.S)
    return (max(m, key=len) if m else text).strip() + "\n"


def split_tests(src: str) -> list[tuple[str, int, int]]:
    """(test_name, start, end) spans of every ZTEST(...) { ... } block."""
    spans = []
    for m in re.finditer(r"ZTEST(?:_F)?\s*\(\s*\w+\s*,\s*(\w+)\s*\)", src):
        i = src.find("{", m.end())
        if i < 0:
            continue
        depth = 0
        for j in range(i, len(src)):
            if src[j] == "{":
                depth += 1
            elif src[j] == "}":
                depth -= 1
                if depth == 0:
                    spans.append((m.group(1), m.start(), j + 1))
                    break
    return spans


def remove_tests(src: str, names: set[str]) -> str:
    for name, s, e in sorted(split_tests(src), key=lambda t: -t[1]):
        if name in names:
            src = src[:s] + src[e:]
    return src


def failure_report(r: target.RunResult) -> str:
    if not r.built:
        keep = re.compile(r"error|undefined reference|multiple definition|implicit declaration|"
                          r"incompatible|too (few|many) arguments")
        errs = [ln.strip() for ln in r.log.splitlines()
                if keep.search(ln) and "FATAL ERROR: command exited" not in ln
                and "ld returned" not in ln and not ln.startswith("[")]
        return "BUILD FAILED. Compiler and linker messages:\n" + "\n".join(dict.fromkeys(errs))[:4000]
    if not r.finished:
        return "The test image did not finish within the time limit (hang or crash).\n" + r.log[-1500:]
    lines = [ln for ln in r.log.splitlines()
             if "Assertion failed" in ln or "FAIL - " in ln or "Expected" in ln or ": actual " in ln
             or ln.strip().startswith("ASSERTION") or " is not " in ln]
    return ("These tests FAIL on the current implementation: " + ", ".join(r.failed)
            + ". Tests that are not listed passed; keep them unchanged.\nOn-target output:\n"
            + "\n".join(lines[:40]) + "\n\nThe suite must pass on the current code (it becomes the "
            "regression suite). For each failing check, recompute the expected value from the "
            "specification. If the actual value printed above agrees with the specification, use it. "
            "If you cannot resolve a check, delete that check, not the whole test.")


class Agent:
    def __init__(self, model: str, seed: int, out: Path, fix_iters: int, temperature: float):
        self.model, self.seed, self.out = model, seed, out
        self.fix_iters, self.temperature = fix_iters, temperature
        self.log: list[dict] = []
        self.msgs: dict[str, list[dict]] = {}

    def _ask(self, mod: str, user: str, stage: str) -> str:
        self.msgs[mod].append({"role": "user", "content": user})
        text, usage = chat(self.model, self.msgs[mod], self.seed, self.temperature)
        self.msgs[mod].append({"role": "assistant", "content": text})
        self.log.append({"module": mod, "stage": stage, **usage})
        return extract_c(text)

    def _context(self, mod: str) -> str:
        hdr = (ROOT / "mlmw" / "include" / "mlmw" / f"{mod}.h").read_text()
        src = (ROOT / "mlmw" / "src" / f"{mod}.c").read_text()
        return (f"Module: {mod}. Use suite name agent_{mod}. Write the file test_{mod}.c.\n\n"
                f"Header mlmw/{mod}.h (the specification):\n```c\n{hdr}```\n\n"
                f"Implementation mlmw/src/{mod}.c:\n```c\n{src}```")

    def build_dir(self, mod: str) -> Path:
        return ROOT / "build" / "agent" / self.out.name / mod

    def check(self, mod: str, code: str) -> target.RunResult:
        d = self.out / "work" / mod
        d.mkdir(parents=True, exist_ok=True)
        (d / f"test_{mod}.c").write_text(code)
        return target.build_and_run(d, self.build_dir(mod), timeout=30)

    def single(self, mod: str) -> str:
        self.msgs[mod] = [{"role": "system", "content": SYSTEM}]
        return self._ask(mod, self._context(mod), "single")

    def fix_loop(self, mod: str, code: str, stage: str,
                 fallback: str | None = None) -> tuple[str, list[dict]]:
        """Repair until the file builds and passes on the original code. Returns the best
        version seen (builds, most passing tests), so a bad repair never loses a working file."""
        trace = []
        best, best_key = fallback, (-1, -1)
        if fallback is not None:
            r0 = self.check(mod, fallback)
            best_key = (int(r0.built), len(r0.passed) - len(r0.failed))
        for k in range(self.fix_iters + 1):
            r = self.check(mod, code)
            trace.append({"built": r.built, "finished": r.finished, "passed": len(r.passed),
                          "failed": len(r.failed)})
            key = (int(r.built), len(r.passed) - len(r.failed))
            if key > best_key:
                best, best_key = code, key
            if r.success or k == self.fix_iters:
                break
            code = self._ask(mod, failure_report(r) + "\n\nFix the test file. "
                             "Reply with the complete corrected file.", stage)
        return (best if best is not None else code), trace

    def mutant_round(self, mod: str, code: str, survivors: list[dict]) -> str:
        items = "\n\n".join(
            f"Mutant {k + 1}: line {m['line']} of {m['file']}\n- original: {m['orig']}\n"
            f"+ mutated:  {m['mutant']}" for k, m in enumerate(survivors[:8]))
        return self._ask(mod, "Your current tests pass on the original code, but they also pass on these "
                         "changed versions of the implementation (mutants), so they would not catch "
                         "these bugs:\n\n" + items + "\n\nAdd or strengthen tests so that each mutant "
                         "is caught, while all tests still pass on the original code. Some mutants may "
                         "be equivalent (same behaviour); skip those. Keep all existing tests. Reply "
                         "with the complete file.", "mutant")


def cleanup(agent: Agent, mod: str, code: str) -> tuple[str, dict]:
    """Drop a file that does not build; remove tests that fail on the original code."""
    info = {"built": True, "removed": [], "tests": 0}
    for _ in range(4):
        r = agent.check(mod, code)
        if not r.built:
            info["built"] = False
            return "", info
        if not r.finished:
            # a hang hides which test is at fault: drop the test that was running
            started = re.findall(r"START - (test_\w+)", r.log)
            if not started:
                return "", {**info, "built": False}
            code = remove_tests(code, {started[-1]})
            info["removed"].append(started[-1])
            continue
        if r.success:
            info["tests"] = len(r.passed)
            return code, info
        code = remove_tests(code, set(r.failed))
        info["removed"] += r.failed
    return "", {**info, "built": False}


def split(mutants: list[dict]) -> tuple[list[dict], list[dict]]:
    train, held = [], []
    for m in mutants:
        (train if int(hashlib.sha1(m["id"].encode()).hexdigest(), 16) % 2 == 0 else held).append(m)
    return train, held


def snapshot(agent: Agent, name: str, codes: dict[str, str]) -> dict:
    d = agent.out / name
    shutil.rmtree(d, ignore_errors=True)
    d.mkdir(parents=True)
    infos = {}
    for mod in MODULES:
        clean, info = cleanup(agent, mod, codes[mod])
        infos[mod] = info
        if clean:
            (d / f"test_{mod}.c").write_text(clean)
    if not any(d.glob("*.c")):
        (d / "test_empty.c").write_text('#include <zephyr/ztest.h>\nZTEST(agent_empty, test_nothing) {}\n'
                                        'ZTEST_SUITE(agent_empty, NULL, NULL, NULL, NULL, NULL);\n')
    return infos


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--model", default="qwen3-coder:30b")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--temperature", type=float, default=0.2)
    ap.add_argument("--fix-iters", type=int, default=4)
    ap.add_argument("--mutant-rounds", type=int, default=2)
    ap.add_argument("--workers", type=int, default=12)
    ap.add_argument("--tag", default=None)
    args = ap.parse_args()

    tag = args.tag or f"{args.model.replace(':', '-').replace('.', '')}_s{args.seed}"
    out = ROOT / "results" / "agent" / tag
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir(parents=True)
    agent = Agent(args.model, args.seed, out, args.fix_iters, args.temperature)
    all_mutants = json.loads((ROOT / "results" / "mutants.json").read_text())["mutants"]
    train, held = split(all_mutants)
    summary: dict = {"model": args.model, "seed": args.seed, "temperature": args.temperature,
                     "fix_iters": args.fix_iters, "mutant_rounds": args.mutant_rounds,
                     "train_mutants": len(train), "heldout_mutants": len(held)}
    t0 = time.time()

    codes = {m: agent.single(m) for m in MODULES}
    first = {m: agent.check(m, codes[m]) for m in MODULES}
    summary["single_first_try"] = {m: {"built": r.built, "passed": len(r.passed), "failed": len(r.failed)}
                                   for m, r in first.items()}
    summary["single_cleanup"] = snapshot(agent, "single", codes)

    traces = {}
    for m in MODULES:
        codes[m], traces[m] = agent.fix_loop(m, codes[m], "loop")
    summary["loop_trace"] = traces
    summary["loop_cleanup"] = snapshot(agent, "loop", codes)

    for rnd in range(args.mutant_rounds):
        for m in MODULES:
            clean, _ = cleanup(agent, m, codes[m])
            if not clean:
                continue
            codes[m] = clean
            d = out / "work" / f"mut_{m}"
            shutil.rmtree(d, ignore_errors=True)
            d.mkdir(parents=True)
            (d / f"test_{m}.c").write_text(clean)
            mine = [x for x in train if x["file"] == f"{m}.c"]
            res = mutate.run_suite(d, f"{tag}_{m}_r{rnd}", min(args.workers, max(1, len(mine))),
                                   mutants=mine, quiet=True)
            surv = [x for x in mine if any(r["id"] == x["id"] and not r["killed"]
                                           for r in res.get("rows", []))]
            summary.setdefault("mutant_rounds_log", []).append(
                {"round": rnd, "module": m, "train_mutants": len(mine), "survived": len(surv)})
            if surv:
                new = agent.mutant_round(m, clean, surv)
                codes[m], _ = agent.fix_loop(m, new, "mutant", fallback=clean)
    summary["mutant_cleanup"] = snapshot(agent, "mutant", codes)

    summary["llm_calls"] = len(agent.log)
    summary["output_tokens"] = sum(x["output_tokens"] for x in agent.log)
    summary["llm_seconds"] = round(sum(x["seconds"] for x in agent.log), 1)
    summary["wall_seconds_agent"] = round(time.time() - t0, 1)

    held_ids = {m["id"] for m in held}
    scores = {}
    for snap in ("single", "loop", "mutant"):
        res = mutate.run_suite(out / snap, f"{tag}_{snap}", args.workers, quiet=True)
        if "rows" not in res:
            scores[snap] = {"error": res.get("error")}
            continue
        rows = res["rows"]
        h = [r for r in rows if r["id"] in held_ids]
        scores[snap] = {"all": round(sum(r["killed"] for r in rows) / len(rows), 4),
                        "heldout": round(sum(r["killed"] for r in h) / len(h), 4),
                        "heldout_killed": sum(r["killed"] for r in h), "heldout_n": len(h),
                        "killed_ids": sorted(r["id"] for r in rows if r["killed"])}
    summary["scores"] = scores
    (out / "agent_log.json").write_text(json.dumps(agent.log, indent=1))
    (out / "conversations.json").write_text(json.dumps(agent.msgs, indent=1))
    (out / "summary.json").write_text(json.dumps(summary, indent=1))
    print(json.dumps({"tag": tag, **{k: {kk: vv for kk, vv in v.items() if kk != "killed_ids"}
                                     for k, v in scores.items()}}, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
