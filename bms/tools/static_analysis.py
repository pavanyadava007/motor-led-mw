"""Static analysis of bms/src: cppcheck (--enable=all, plus the MISRA C 2012 addon when
available) and a strict compile with host gcc and arm-none-eabi-gcc.
Writes results/bms/static_analysis.json.

cppcheck runs in the ubuntu:24.04 image of bms/docker/Dockerfile.ci (cppcheck 2.13, same
package as the GitHub runner) unless --local is given. The MISRA addon has no rule texts
offline (they are licensed by MISRA), so findings are listed by rule number only.
"""

from __future__ import annotations

import collections
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
IMAGE = os.environ.get("BMS_CI_IMAGE", "bms-ci:24.04")
ARMBIN = os.environ.get("ARMBIN", "/opt/arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi/bin")
WARN = [
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-pedantic",
    "-Wconversion",
    "-Wsign-conversion",
    "-Wshadow",
    "-Wcast-qual",
    "-Wstrict-prototypes",
    "-Wmissing-prototypes",
    "-Wdouble-promotion",
    "-ffp-contract=off",
]
TEMPLATE = "{file}:{line}:{severity}:{id}:{message}"


def cppcheck(local: bool) -> dict:
    base = [
        "cppcheck",
        "--enable=all",
        "--std=c99",
        "--inline-suppr",
        "--suppress=missingIncludeSystem",
        "--suppress=unusedFunction",
        "--suppress=checkersReport",
        "-Ibms/include",
        f"--template={TEMPLATE}",
    ]
    runs = {}
    for name, extra in (("cppcheck", []), ("misra", ["--addon=misra"])):
        cmd = [*base, *extra, "bms/src"]
        if not local:
            cmd = ["docker", "run", "--rm", "--network", "none", "-v", f"{ROOT}:/w", "-w", "/w", IMAGE, *cmd]
        p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=900)
        rows = [ln for ln in p.stderr.splitlines() if re.match(r"^[^:]+:\d+:\w+:", ln)]
        sev = collections.Counter(ln.split(":")[2] for ln in rows)
        ids = collections.Counter(ln.split(":")[3] for ln in rows)
        runs[name] = {
            "exit_code": p.returncode,
            "findings": len(rows),
            "by_severity": dict(sev),
            "by_id": dict(ids.most_common()),
            "rows": rows,
        }
    ver = ["cppcheck", "--version"]
    if not local:
        ver = ["docker", "run", "--rm", IMAGE, *ver]
    runs["version"] = subprocess.run(ver, capture_output=True, text=True).stdout.strip()
    return runs


def strict_compile() -> dict:
    out = {}
    for name, cc, flags in (
        ("host_gcc", "gcc", ["-O2"]),
        (
            "arm_none_eabi_gcc",
            f"{ARMBIN}/arm-none-eabi-gcc",
            ["-Os", "-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard"],
        ),
    ):
        w = [x for x in WARN if x != "-Werror"]
        warnings = 0
        for src in sorted((ROOT / "bms" / "src").glob("*.c")):
            p = subprocess.run(
                [cc, *w, *flags, "-Ibms/include", "-c", str(src), "-o", os.devnull],
                cwd=ROOT,
                capture_output=True,
                text=True,
            )
            warnings += p.stderr.count("warning:") + p.stderr.count("error:")
        ver = subprocess.run([cc, "--version"], capture_output=True, text=True).stdout.splitlines()[0]
        out[name] = {"compiler": ver, "flags": " ".join(w + flags), "warnings": warnings}
    return out


def main() -> int:
    local = "--local" in sys.argv
    res = {"strict_compile": strict_compile()}
    cc = cppcheck(local)
    res["cppcheck"] = cc["cppcheck"]
    res["misra_addon"] = cc["misra"]
    res["cppcheck_version"] = cc["version"]
    (ROOT / "results" / "bms" / "static_analysis.json").write_text(json.dumps(res, indent=1))
    print(
        json.dumps(
            {
                "strict": {k: v["warnings"] for k, v in res["strict_compile"].items()},
                "cppcheck": res["cppcheck"]["by_severity"],
                "misra": res["misra_addon"]["by_id"],
                "version": res["cppcheck_version"],
            },
            indent=1,
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
