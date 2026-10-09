"""Mutation testing of bms/src/*.c with the operator set of tools/mutate.py (operator
swaps, constants +-1, statement deletion). Mutants that do not compile for Cortex-M4F or
whose ARM object code equals the original (equivalent) are dropped, as in tools/mutate.py.
Each remaining mutant is linked with the unit tests and run ON THE HOST (gcc), which is
much faster than QEMU; the tests are the same sources that run on the emulated target.
bms_ecm_lg_hg2.c (generated parameter tables) is not mutated.
Writes results/bms/mutation.json.
"""

from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mutate  # noqa: E402

BMS = ROOT / "bms"
WORK = ROOT / "build" / "bms" / ("mut" + ("_x" if "--exclude" in sys.argv else ""))
ARMCC = (
    os.environ.get("ARMBIN", "/opt/arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi/bin")
    + "/arm-none-eabi-gcc"
)
ARMFLAGS = [
    "-mcpu=cortex-m4",
    "-mthumb",
    "-mfpu=fpv4-sp-d16",
    "-mfloat-abi=hard",
    "-Os",
    "-std=c99",
    "-ffp-contract=off",
    "-ffunction-sections",
    "-c",
    f"-I{BMS / 'include'}",
]
HOSTFLAGS = ["-std=c99", "-O1", "-ffp-contract=off", f"-I{BMS / 'include'}", f"-I{BMS / 'tests' / 'unit'}"]
SKIP = {"bms_ecm_lg_hg2.c"}


def obj_hash(path: Path) -> str | None:
    with tempfile.TemporaryDirectory() as d:
        o = Path(d) / "x.o"
        if subprocess.run([ARMCC, *ARMFLAGS, str(path), "-o", str(o)], capture_output=True).returncode != 0:
            return None
        b = Path(d) / "x.bin"
        subprocess.run(
            [
                ARMCC.replace("gcc", "objcopy"),
                "-O",
                "binary",
                "--only-section=.text*",
                "--only-section=.rodata*",
                str(o),
                str(b),
            ],
            check=True,
        )
        return hashlib.sha256(b.read_bytes()).hexdigest()


def generate() -> tuple[list[dict], dict]:
    stats = {"candidates": 0, "no_compile": 0, "equivalent": 0}
    out = []
    for f in sorted((BMS / "src").glob("*.c")):
        if f.name in SKIP:
            continue
        src = f.read_text()
        lines = src.split("\n")
        seen = {obj_hash(f)}
        for ln, desc, orig, new in mutate.candidates(src):
            stats["candidates"] += 1
            m = lines.copy()
            m[ln] = new
            text = "\n".join(m)
            with tempfile.TemporaryDirectory() as d:
                p = Path(d) / f.name
                p.write_text(text)
                h = obj_hash(p)
            if h is None:
                stats["no_compile"] += 1
                continue
            if h in seen:
                stats["equivalent"] += 1
                continue
            seen.add(h)
            out.append(
                {
                    "id": f"{f.stem}-{len(out):04d}",
                    "file": f.name,
                    "line": ln + 1,
                    "op": desc,
                    "orig": orig.strip(),
                    "mutant": new.strip(),
                    "source": text,
                }
            )
    stats["kept"] = len(out)
    return out, stats


def main() -> int:
    import argparse

    ap = argparse.ArgumentParser()
    ap.add_argument("--exclude", default="", help="test file to leave out, e.g. test_details.c")
    ap.add_argument("--out", default="mutation.json")
    args = ap.parse_args()
    t0 = time.time()
    mutants, stats = generate()
    WORK.mkdir(parents=True, exist_ok=True)
    srcs = sorted((BMS / "src").glob("*.c"))
    tests = [t for t in sorted((BMS / "tests" / "unit").glob("*.c")) if t.name != args.exclude]
    tests.append(BMS / "tests" / "host" / "runner.c")
    objdir = WORK / "obj"
    objdir.mkdir(exist_ok=True)
    for s in srcs + tests:  # unchanged objects compiled once
        subprocess.run(
            ["gcc", *HOSTFLAGS, "-c", str(s), "-o", str(objdir / f"{s.parent.name}_{s.stem}.o")], check=True
        )

    def link_run(k: int, m: dict | None) -> dict:
        d = WORK / f"w{k}"
        d.mkdir(exist_ok=True)
        objs = []
        for s in srcs + tests:
            if m is not None and s.parent.name == "src" and s.name == m["file"]:
                ms = d / m["file"]
                ms.write_text(m["source"])
                o = d / "mut.o"
                if subprocess.run(
                    ["gcc", *HOSTFLAGS, "-c", str(ms), "-o", str(o)], capture_output=True
                ).returncode:
                    return {"killed": True, "reason": "host_build"}
                objs.append(str(o))
            else:
                objs.append(str(objdir / f"{s.parent.name}_{s.stem}.o"))
        exe = d / "t"
        subprocess.run(["gcc", *objs, "-o", str(exe)], check=True)
        try:
            p = subprocess.run([str(exe)], capture_output=True, text=True, timeout=20)
        except subprocess.TimeoutExpired:
            return {"killed": True, "reason": "timeout"}
        failed = [ln.split(" - ")[1].split()[0] for ln in p.stdout.splitlines() if ln.startswith(" FAIL")]
        if p.returncode == 0:
            return {"killed": False, "reason": "survived"}
        return {"killed": True, "reason": "fail" if failed else "crash", "failed_tests": failed}

    base = link_run(0, None)
    if base["killed"]:
        print("baseline fails", base)
        return 1
    with ThreadPoolExecutor(12) as ex:
        res = list(ex.map(lambda km: link_run(km[0] % 12, km[1]), enumerate(mutants)))
    rows = []
    for m, r in zip(mutants, res, strict=True):
        rows.append({k: m[k] for k in ("id", "file", "line", "op", "orig", "mutant")} | r)
    killed = sum(r["killed"] for r in rows)
    per_file = {}
    for r in rows:
        f = per_file.setdefault(r["file"], {"mutants": 0, "killed": 0})
        f["mutants"] += 1
        f["killed"] += int(r["killed"])
    for f in per_file.values():
        f["score"] = round(f["killed"] / f["mutants"], 4)
    out = {
        "generator": stats,
        "mutants": len(rows),
        "killed": killed,
        "score": round(killed / len(rows), 4),
        "by_reason": {
            k: sum(r["reason"] == k for r in rows)
            for k in ("fail", "crash", "timeout", "host_build", "survived")
        },
        "per_file": per_file,
        "runner": "host gcc, bms/tests/unit (same tests as on the emulated target)",
        "excluded_test_file": args.exclude or None,
        "seconds": round(time.time() - t0, 1),
        "survivors": [r for r in rows if not r["killed"]],
        "rows": rows,
    }
    (ROOT / "results" / "bms" / args.out).write_text(json.dumps(out, indent=1))
    print(json.dumps({k: v for k, v in out.items() if k not in ("rows", "survivors")}, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
