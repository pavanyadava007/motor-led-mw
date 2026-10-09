"""Code size, stack and emulated instruction counts of the BMS library.

- size: arm-none-eabi-gcc 13.3 objects of bms/src/*.c for Cortex-M4F, Cortex-M33 (FPU) and
  Cortex-M3 (soft float), at -Os and -O2 (text includes the read-only ECM tables)
- stack: -fstack-usage per function plus -fcallgraph-info=su; worst-case path from
  bms_cycle() through the static call graph (no recursion, no function pointers)
- external symbols the library needs (must be none or compiler helpers only)
- instruction counts: build/bms/arm/prof.elf (1000 cycles, 12 cells) in QEMU mps2-an386
  with the insn_range TCG plugin. These are EMULATED instruction counts, not CPU cycles
  and not a timing measurement.
Writes results/bms/profile.json.
"""

from __future__ import annotations

import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ARMBIN = os.environ.get("ARMBIN", "/opt/arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi/bin")
CC, SIZE, NM = f"{ARMBIN}/arm-none-eabi-gcc", f"{ARMBIN}/arm-none-eabi-size", f"{ARMBIN}/arm-none-eabi-nm"
SRC = sorted((ROOT / "bms" / "src").glob("*.c"))
CPUS = {
    "cortex-m4f": ["-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard"],
    "cortex-m33": ["-mcpu=cortex-m33", "-mthumb", "-mfpu=fpv5-sp-d16", "-mfloat-abi=hard"],
    "cortex-m3": ["-mcpu=cortex-m3", "-mthumb", "-mfloat-abi=soft"],
}
COMMON = [
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-ffp-contract=off",
    "-ffunction-sections",
    f"-I{ROOT / 'bms' / 'include'}",
    "-c",
]
ELF = ROOT / "build" / "bms" / "arm" / "prof.elf"
PLUGIN = ROOT / "build" / "bms" / "qemu" / "libinsn_range.so"
IMAGE = os.environ.get("MLMW_QEMU_IMAGE", "zqemu:8.2")


def compile_all(d: Path, cpu: str, opt: str, extra: list[str] | None = None) -> list[Path]:
    objs = []
    for s in SRC:
        o = d / f"{s.stem}.o"
        subprocess.run(
            [CC, *CPUS[cpu], opt, *COMMON, *(extra or []), str(s), "-o", str(o)], check=True, cwd=d
        )
        objs.append(o)
    return objs


def sizes(objs: list[Path]) -> dict:
    rows = {}
    for o in objs:
        t, dd, b = (
            subprocess.run([SIZE, str(o)], capture_output=True, text=True, check=True)
            .stdout.split("\n")[1]
            .split()[:3]
        )
        rows[o.stem] = {"text": int(t), "data": int(dd), "bss": int(b)}
    rows["total"] = {k: sum(r[k] for n, r in rows.items()) for k in ("text", "data", "bss")}
    return rows


def stack(d: Path) -> dict:
    """Worst-case stack of bms_cycle() from the -fcallgraph-info=su graph."""
    frames, edges = {}, {}
    for ci in d.glob("*.ci"):
        txt = ci.read_text()
        for m in re.finditer(r'node: \{ title: "([^"]+)" label: "[^"]*\\n[^"]*\\n(\d+) bytes', txt):
            frames[m.group(1)] = int(m.group(2))
        for m in re.finditer(r'edge: \{ sourcename: "([^"]+)" targetname: "([^"]+)"', txt):
            edges.setdefault(m.group(1), set()).add(m.group(2))

    def depth(f: str, seen: tuple) -> tuple[int, list[str]]:
        if f in seen:
            raise RuntimeError(f"recursion at {f}")
        best, path = 0, []
        for g in edges.get(f, ()):
            dd, p = depth(g, (*seen, f))
            if dd > best:
                best, path = dd, p
        return frames.get(f, 0) + best, [f, *path]

    def key(name: str) -> str:
        return next(k for k in frames if k == name or k.endswith(":" + name))

    top = key("bms_cycle")
    worst, path = depth(top, ())
    su = {}
    for f in d.glob("*.su"):
        for ln in f.read_text().splitlines():
            p = ln.split("\t")
            su[p[0].split(":")[-1]] = int(p[1])
    return {
        "bms_cycle_worst_case_bytes": worst,
        "worst_path": [p.split(":")[-1] for p in path],
        "largest_frame_bytes": max(su.values()),
        "frames": dict(sorted(su.items())),
    }


def externals(objs: list[Path]) -> list[str]:
    defined, undef = set(), set()
    for o in objs:
        for ln in subprocess.run(
            [NM, str(o)], capture_output=True, text=True, check=True
        ).stdout.splitlines():
            p = ln.split()
            if p[0] == "U":
                undef.add(p[1])
            elif len(p) == 3:
                defined.add(p[2])
    return sorted(undef - defined)


def insn_counts() -> dict:
    syms = {}
    for ln in subprocess.run(
        [NM, "-S", "--defined-only", str(ELF)], capture_output=True, text=True, check=True
    ).stdout.splitlines():
        p = ln.split()
        if len(p) == 4:
            syms[p[3]] = (int(p[0], 16) & ~1, (int(p[0], 16) & ~1) + int(p[1], 16), p[2])
    with tempfile.TemporaryDirectory() as d:
        objs = compile_all(Path(d), "cortex-m4f", "-O2")
        mod = {}
        for o in objs:
            names = [
                ln.split()[2]
                for ln in subprocess.run(
                    [NM, "--defined-only", str(o)], capture_output=True, text=True, check=True
                ).stdout.splitlines()
                if len(ln.split()) == 3 and ln.split()[1] in "tT"
            ]
            mod[o.stem] = [n for n in names if n in syms]
    lib = sorted({n for v in mod.values() for n in v})
    # libgcc / libc helpers the library calls at run time would also belong to the cycle
    helpers = [n for n in syms if n.startswith("__aeabi") or n in ("memset", "memcpy")]

    def rng(entry: str, names: list[str]) -> str:
        rr = [syms[entry][:2]] + [syms[n][:2] for n in names if n != entry and n in syms]
        return "@".join(f"{s:x}-{e:x}" for s, e in rr)

    groups = {
        "bms_cycle": ("bms_cycle", lib + helpers),
        "ekf_step": ("bms_soc_step", mod["bms_soc"]),
        "monitor": ("bms_mon_step", mod["bms_monitor"]),
        "state_machine": ("bms_sm_step", mod["bms_sm"]),
        "balance": ("bms_balance", mod["bms_balance"]),
        "crc_e2e": ("bms_e2e_check", ["age", "bms_e2e_crc", "bms_crc8_j1850", "bms_e2e_protect"]),
    }
    args = [f"g{k}={name}@{rng(e, n)}" for k, (name, (e, n)) in enumerate(groups.items())]
    log = ROOT / "build" / "bms" / "qemu" / "plugin.log"
    log.unlink(missing_ok=True)
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
        IMAGE,
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
        "-plugin",
        f"{PLUGIN}," + ",".join(args),
        "-d",
        "plugin",
        "-D",
        str(log),
    ]
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
    out = {"qemu_exit": p.returncode, "stdout": p.stdout.strip(), "groups": {}}
    for m in re.finditer(r"INSN_RANGE (\S+) insns=(\d+) calls=(\d+)", log.read_text()):
        n, c = int(m.group(2)), int(m.group(3))
        out["groups"][m.group(1)] = {
            "insns_total": n,
            "calls": c,
            "insns_per_call": round(n / c, 1) if c else None,
        }
    out["notes"] = (
        "ekf_step counts per EKF step (12 per cycle; the first cycle runs the OCV init instead). "
        "crc_e2e counts per cycle: 1 RX E2E check + 3 TX protect calls (4 CRCs over 8 bytes)."
    )
    return out


def main() -> None:
    import sys

    if "--crc-bitwise" in sys.argv:
        # same profile with the bit-serial CRC variant (-DBMS_CRC_BITWISE)
        bms = ROOT / "bms"
        subprocess.run(
            ["make", "-s", "-B", "-C", str(bms), "../build/bms/arm/prof.elf", "PROF_DEFS=-DBMS_CRC_BITWISE"],
            check=True,
        )
        out = ROOT / "results" / "bms" / "profile_crc_bitwise.json"
        res = insn_counts()
        res["variant"] = "bms_crc8_j1850 bit-serial (-DBMS_CRC_BITWISE)"
        out.write_text(json.dumps(res, indent=1))
        subprocess.run(["make", "-s", "-B", "-C", str(bms), "../build/bms/arm/prof.elf"], check=True)
        print(out.read_text()[:1500])
        return
    res: dict = {
        "compiler": subprocess.run([CC, "--version"], capture_output=True, text=True).stdout.splitlines()[0],
        "size_bytes": {},
        "note": "instruction counts are QEMU EMULATED instruction counts, not cycles",
    }
    for cpu in CPUS:
        for opt in ("-Os", "-O2"):
            with tempfile.TemporaryDirectory() as d:
                res["size_bytes"][f"{cpu} {opt}"] = sizes(compile_all(Path(d), cpu, opt))
    with tempfile.TemporaryDirectory() as d:
        objs = compile_all(Path(d), "cortex-m4f", "-Os", ["-fstack-usage", "-fcallgraph-info=su"])
        res["stack_cortex_m4f_Os"] = stack(Path(d))
        res["external_symbols_needed"] = externals(objs)
    with tempfile.TemporaryDirectory() as d:
        compile_all(Path(d), "cortex-m4f", "-O2", ["-fstack-usage", "-fcallgraph-info=su"])
        res["stack_cortex_m4f_O2"] = {k: v for k, v in stack(Path(d)).items() if k != "frames"}
    for ln in subprocess.run(
        [NM, "-S", str(ELF)], capture_output=True, text=True, check=True
    ).stdout.splitlines():
        p = ln.split()
        if len(p) == 4 and p[3] == "bms":
            res["ram_bms_context_bytes"] = int(p[1], 16)
    res["insn_counts_cortex_m4f_O2_emulated"] = insn_counts()
    res["profile_workload"] = "pil/prof_main.c: 1000 cycles, 12 cells, 4 temps, ACTIVE after cycle 6"
    (ROOT / "results" / "bms" / "profile.json").write_text(json.dumps(res, indent=1))
    print(json.dumps({k: v for k, v in res.items() if k not in ("size_bytes",)}, indent=1)[:3500])
    print({k: v["total"] for k, v in res["size_bytes"].items()})


if __name__ == "__main__":
    main()
