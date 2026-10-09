"""Build the portable BMS unit tests as a ztest image for mps2/an385 (Cortex-M3) and run
them in QEMU, reusing tools/target.py. Writes results/bms/target_tests.json."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import target  # noqa: E402


def main() -> int:
    bdir = ROOT / "build" / "bms" / "ztest"
    p = subprocess.run(
        ["west", "build", "-p", "-b", target.BOARD, "-d", str(bdir), str(ROOT / "bms" / "tests" / "target")],
        capture_output=True,
        text=True,
        timeout=900,
    )
    if p.returncode != 0:
        print((p.stdout + p.stderr)[-4000:])
        return 1
    r = target.run(bdir, timeout=120)
    out = {
        "board": target.BOARD,
        "machine": "QEMU 8.2 mps2-an385 Cortex-M3 (emulated, soft float)",
        "finished": r.finished,
        "success": r.success,
        "passed": len(r.passed),
        "failed": len(r.failed),
        "failed_tests": r.failed,
        "passed_tests": r.passed,
        "host_wall_s": r.seconds,
    }
    (ROOT / "results" / "bms").mkdir(parents=True, exist_ok=True)
    (ROOT / "results" / "bms" / "target_tests.json").write_text(json.dumps(out, indent=1))
    print(json.dumps({k: v for k, v in out.items() if k != "passed_tests"}))
    if not r.success:
        print(r.log[-3000:])
    return 0 if r.success else 1


if __name__ == "__main__":
    sys.exit(main())
