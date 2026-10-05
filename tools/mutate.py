"""Mutation testing of the middleware on the emulated target.

generate: writes every single-change mutant of mlmw/src/*.c to results/mutants.json.
  Operators: relational and logical operator swaps, arithmetic and shift swaps,
  removal of '!', integer constant +1/-1, deletion of plain statements.
  Mutants that do not compile for Cortex-M3, or whose object code is identical
  to the original (equivalent mutants), are dropped.
run: builds and runs one test suite against every mutant in QEMU, in parallel
  workers, and writes results/<name>_mutation.json. A mutant is killed when the
  suite fails, crashes or times out.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import target  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
MLMW = ROOT / "mlmw"
RESULTS = ROOT / "results"
CC = "arm-none-eabi-gcc"
CFLAGS = ["-mcpu=cortex-m3", "-mthumb", "-Os", "-std=c99", "-ffunction-sections", "-c"]

TOKEN = re.compile(
    r"->|\+\+|--|\+=|-=|<<=|>>=|<=|>=|==|!=|&&|\|\||<<|>>|"
    r"0x[0-9A-Fa-f]+u?l*|\b\d+u?l*\b|[<>+\-*/!]"
)
SWAPS = {
    "<": ["<=", ">"], "<=": ["<", ">"], ">": [">=", "<"], ">=": [">", "<"],
    "==": ["!="], "!=": ["=="], "&&": ["||"], "||": ["&&"],
    "+": ["-"], "-": ["+"], "*": ["/"], "/": ["*"], ">>": ["<<"], "<<": [">>"],
    "++": ["--"], "!": [""],
}
SKIP_LINE = re.compile(r"^\s*(#|/\*|\*|//)")
DECL = re.compile(r"^\s*(return|static|const|struct|enum|typedef|case|default|break|"
                  r"u?int\d+_t|int|bool|size_t|void|uint32_t|for|if|else|while|switch)\b")
STMT = re.compile(r"^\s*[\w\->\[\]\.\(\)\*]+.*;\s*$")


def _num_mutants(tok: str) -> list[str]:
    m = re.match(r"(0x[0-9A-Fa-f]+|\d+)(u?l*)$", tok)
    if not m:
        return []
    val = int(m.group(1), 0)
    suf = m.group(2)
    hexa = m.group(1).startswith("0x")
    out = []
    for v in (val + 1, val - 1):
        if v >= 0:
            out.append((hex(v) if hexa else str(v)) + suf)
    return out


def candidates(src: str) -> list[tuple[int, str, str, str]]:
    """(line_no, description, original_line, mutated_line) for one file."""
    lines = src.split("\n")
    out = []
    in_comment = False
    for i, line in enumerate(lines):
        stripped = line.strip()
        if in_comment:
            if "*/" in stripped:
                in_comment = False
            continue
        if stripped.startswith("/*") and "*/" not in stripped:
            in_comment = True
            continue
        if SKIP_LINE.match(line) or not stripped:
            continue
        code = line.split("/*")[0].split("//")[0]
        for m in TOKEN.finditer(code):
            tok = m.group(0)
            repl = SWAPS.get(tok, []) + _num_mutants(tok)
            for r in repl:
                new = code[: m.start()] + r + code[m.end():]
                desc = f"'{tok}' -> '{r}'" if r else f"remove '{tok}'"
                out.append((i, desc, line, new))
        if STMT.match(code) and not DECL.match(code) and "{" not in code and "}" not in code:
            indent = code[: len(code) - len(code.lstrip())]
            out.append((i, "delete statement", line, indent + ";"))
    return out


def _obj_hash(path: Path) -> str | None:
    with tempfile.TemporaryDirectory() as d:
        obj = Path(d) / "x.o"
        p = subprocess.run([CC, *CFLAGS, f"-I{MLMW / 'include'}", str(path), "-o", str(obj)],
                           capture_output=True)
        if p.returncode != 0:
            return None
        text = subprocess.run(["arm-none-eabi-objcopy", "-O", "binary", "--only-section=.text*",
                               "--only-section=.rodata*", str(obj), str(Path(d) / "x.bin")],
                              capture_output=True)
        if text.returncode != 0:
            return None
        return hashlib.sha256((Path(d) / "x.bin").read_bytes()).hexdigest()


def generate() -> list[dict]:
    mutants = []
    stats = {"candidates": 0, "no_compile": 0, "equivalent": 0}
    for f in sorted((MLMW / "src").glob("*.c")):
        src = f.read_text()
        base = _obj_hash(f)
        seen = {base}
        lines = src.split("\n")
        for ln, desc, orig, new in candidates(src):
            stats["candidates"] += 1
            mutated = lines.copy()
            mutated[ln] = new
            text = "\n".join(mutated)
            with tempfile.TemporaryDirectory() as d:
                p = Path(d) / f.name
                p.write_text(text)
                h = _obj_hash(p)
            if h is None:
                stats["no_compile"] += 1
                continue
            if h in seen:
                stats["equivalent"] += 1
                continue
            seen.add(h)
            mutants.append({"id": f"{f.stem}-{len(mutants):03d}", "file": f.name,
                            "line": ln + 1, "op": desc, "orig": orig.strip(),
                            "mutant": new.strip(), "source": text})
    stats["kept"] = len(mutants)
    RESULTS.mkdir(exist_ok=True)
    (RESULTS / "mutants.json").write_text(json.dumps({"stats": stats, "mutants": mutants}, indent=1))
    print(json.dumps(stats))
    return mutants


def run_suite(suite: Path, name: str, workers: int, timeout: float = 20.0,
              mutants: list[dict] | None = None, quiet: bool = False) -> dict:
    data = json.loads((RESULTS / "mutants.json").read_text())
    mutants = mutants if mutants is not None else data["mutants"]
    work = ROOT / "build" / "mut" / name
    shutil.rmtree(work, ignore_errors=True)
    queues = [mutants[k::workers] for k in range(workers)]

    def worker(k: int, todo: list[dict]) -> list[dict]:
        wdir = work / f"w{k}"
        lib = wdir / "mlmw"
        shutil.copytree(MLMW, lib)
        bdir = wdir / "build"
        base = target.build_and_run(suite, bdir, lib, pristine=True, timeout=timeout)
        if not base.success:
            return [{"id": "__baseline__", "killed": None, "log": base.log[-2000:]}]
        res = []
        for m in todo:
            dst = lib / "src" / m["file"]
            dst.write_text(m["source"])
            r = target.build_and_run(suite, bdir, lib, timeout=timeout)
            shutil.copy(MLMW / "src" / m["file"], dst)
            killed = not r.success
            reason = ("build" if not r.built else "timeout" if not r.finished
                      else "fail" if killed else "survived")
            res.append({"id": m["id"], "killed": killed, "reason": reason,
                        "failed_tests": r.failed})
        # restore the build to the original sources for the next run
        target.build(suite, bdir, lib)
        return res

    t0 = time.time()
    with ThreadPoolExecutor(workers) as ex:
        parts = list(ex.map(worker, range(workers), queues))
    rows = [r for p in parts for r in p]
    shutil.rmtree(work, ignore_errors=True)
    if any(r["id"] == "__baseline__" for r in rows):
        out = {"suite": name, "error": "suite fails on the original code",
               "log": next(r["log"] for r in rows if r["id"] == "__baseline__")}
    else:
        killed = sum(r["killed"] for r in rows)
        out = {"suite": name, "mutants": len(rows), "killed": killed,
               "score": round(killed / len(rows), 4) if rows else 0.0,
               "by_reason": {k: sum(r["reason"] == k for r in rows)
                             for k in ("fail", "timeout", "build", "survived")},
               "seconds": round(time.time() - t0, 1), "rows": rows}
    if not quiet:
        (RESULTS / f"{name}_mutation.json").write_text(json.dumps(out, indent=1))
        print(json.dumps({k: v for k, v in out.items() if k not in ("rows", "log")}))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("generate")
    r = sub.add_parser("run")
    r.add_argument("suite", type=Path)
    r.add_argument("--name", required=True)
    r.add_argument("--workers", type=int, default=8)
    args = ap.parse_args()
    if args.cmd == "generate":
        generate()
    else:
        run_suite(args.suite, args.name, args.workers)
    return 0


if __name__ == "__main__":
    sys.exit(main())
