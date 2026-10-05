"""Build a ztest image for the emulated target and run it in QEMU.

The middleware and the test sources are passed as directories, so the same
code path serves the reference suite, agent-written suites and mutants.
Target: Zephyr board mps2/an385 (Arm Cortex-M3) in qemu-system-arm 8.2,
which runs in the zqemu:8.2 container (see docker/Dockerfile).
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import threading
import time
import uuid
from dataclasses import asdict, dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HARNESS = ROOT / "tests" / "harness"
BOARD = os.environ.get("MLMW_BOARD", "mps2/an385")
QEMU_IMAGE = os.environ.get("MLMW_QEMU_IMAGE", "zqemu:8.2")
QEMU_MACHINE = {"mps2/an385": ("cortex-m3", "mps2-an385")}

RESULT_RE = re.compile(r"^\s*(PASS|FAIL|SKIP) - (test_\w+)", re.M)


@dataclass
class RunResult:
    built: bool
    finished: bool = False
    success: bool = False
    passed: list[str] = field(default_factory=list)
    failed: list[str] = field(default_factory=list)
    seconds: float = 0.0
    log: str = ""

    def short(self) -> dict:
        d = asdict(self)
        d["log"] = self.log[-4000:]
        return d


def build(test_dir: Path, build_dir: Path, mlmw_dir: Path | None = None,
          pristine: bool = False) -> tuple[bool, str]:
    cmd = ["west", "build", "-b", BOARD, "-d", str(build_dir), str(HARNESS)]
    if pristine:
        cmd.insert(2, "-p")
    cmd += ["--", f"-DTEST_SRC_DIR={Path(test_dir).resolve()}"]
    if mlmw_dir is not None:
        cmd.append(f"-DMLMW_DIR={Path(mlmw_dir).resolve()}")
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
    log = p.stdout + p.stderr
    return p.returncode == 0, log


def run(build_dir: Path, timeout: float = 60.0) -> RunResult:
    elf = Path(build_dir).resolve() / "zephyr" / "zephyr.elf"
    cpu, machine = QEMU_MACHINE[BOARD]
    name = f"mlmw-qemu-{uuid.uuid4().hex[:10]}"
    mount = str(elf.parent)
    cmd = ["docker", "run", "--rm", "-i", "--network", "none", "--name", name,
           "-v", f"{mount}:{mount}:ro", QEMU_IMAGE, "qemu-system-arm",
           "-cpu", cpu, "-machine", machine, "-nographic", "-vga", "none", "-net", "none",
           "-icount", "shift=7,align=off,sleep=off", "-rtc", "clock=vm",
           "-kernel", str(elf)]
    t0 = time.time()
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            stdin=subprocess.DEVNULL)
    out: list[str] = []
    done = threading.Event()
    state = {"finished": False, "success": False}

    def reader() -> None:
        assert proc.stdout is not None
        buf = ""
        for raw in iter(lambda: proc.stdout.readline(), b""):
            line = raw.decode("utf-8", "replace")
            out.append(line)
            buf = line
            if "PROJECT EXECUTION SUCCESSFUL" in buf:
                state.update(finished=True, success=True)
                break
            if "PROJECT EXECUTION FAILED" in buf:
                state["finished"] = True
                break
        done.set()

    th = threading.Thread(target=reader, daemon=True)
    th.start()
    done.wait(timeout)
    subprocess.run(["docker", "kill", name], capture_output=True)
    try:
        proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        proc.kill()
    finished, success = state["finished"], state["success"]
    log = "".join(out)
    res = RunResult(built=True, finished=finished, success=success,
                    seconds=round(time.time() - t0, 2), log=log)
    for status, test in RESULT_RE.findall(log):
        (res.passed if status == "PASS" else res.failed if status == "FAIL" else []).append(test)
    return res


def build_and_run(test_dir: Path, build_dir: Path, mlmw_dir: Path | None = None,
                  pristine: bool = False, timeout: float = 60.0) -> RunResult:
    ok, log = build(test_dir, build_dir, mlmw_dir, pristine)
    if not ok:
        return RunResult(built=False, log=log)
    return run(build_dir, timeout)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("tests", type=Path, help="directory with ztest *.c files")
    ap.add_argument("--build-dir", type=Path, default=ROOT / "build" / "target")
    ap.add_argument("--mlmw", type=Path, default=None)
    ap.add_argument("--pristine", action="store_true")
    args = ap.parse_args()
    r = build_and_run(args.tests, args.build_dir, args.mlmw, args.pristine)
    d = r.short()
    d.pop("log")
    print(json.dumps(d, indent=1))
    if not r.success:
        print(r.log[-3000:], file=sys.stderr)
    return 0 if r.success else 1


if __name__ == "__main__":
    sys.exit(main())
