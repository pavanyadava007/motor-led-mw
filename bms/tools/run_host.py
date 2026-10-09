"""Run the host test layers and record per-test results in results/bms/host_tests.json:
the C unit tests (bms/tests/unit via tests/host/runner.c), optionally the same tests with
ASan + UBSan, and the Python cross-checks (bms/tests/py, pytest)."""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LINE = re.compile(r"^ (PASS|FAIL) - (test_\w+)", re.M)


def c_tests(target: str, exe: str) -> dict:
    subprocess.run(["make", "-s", "-C", "bms", exe], cwd=ROOT, check=True, capture_output=True)
    p = subprocess.run(
        [str(ROOT / "build" / "bms" / "host" / Path(exe).name)],
        cwd=ROOT / "bms",
        capture_output=True,
        text=True,
        env=dict(os.environ),
    )
    res = LINE.findall(p.stdout)
    return {
        "target": target,
        "exit_code": p.returncode,
        "passed": [t for s, t in res if s == "PASS"],
        "failed": [t for s, t in res if s == "FAIL"],
        "tail": p.stdout.strip().splitlines()[-1:],
    }


def main() -> int:
    out = {"c_unit": c_tests("host gcc", "../build/bms/host/bms_tests")}
    if "--asan-docker" in sys.argv:
        # this host has no sanitizer runtimes: run the ASan/UBSan build in the CI image
        img = os.environ.get("BMS_CI_IMAGE", "bms-ci:24.04")
        p = subprocess.run(
            [
                "docker",
                "run",
                "--rm",
                "--network",
                "none",
                "-v",
                f"{ROOT}:/w",
                "-w",
                "/w",
                img,
                "make",
                "-s",
                "-C",
                "bms",
                "B=/tmp/b",
                "test-asan",
            ],
            capture_output=True,
            text=True,
        )
        res = LINE.findall(p.stdout)
        out["c_unit_asan_ubsan"] = {
            "target": f"{img} gcc -fsanitize=address,undefined",
            "exit_code": p.returncode,
            "passed": [t for s_, t in res if s_ == "PASS"],
            "failed": [t for s_, t in res if s_ == "FAIL"],
            "tail": p.stdout.strip().splitlines()[-1:],
        }
    if "--asan" in sys.argv:
        out["c_unit_asan_ubsan"] = c_tests(
            "host gcc -fsanitize=address,undefined", "../build/bms/host/bms_tests_asan"
        )
    xml = ROOT / "build" / "bms" / "pytest.xml"
    p = subprocess.run(
        [sys.executable, "-m", "pytest", "-q", "bms/tests/py", f"--junitxml={xml}"],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    py: dict = {"exit_code": p.returncode, "passed": [], "failed": [], "functions": {}}
    for tc in ET.parse(xml).getroot().iter("testcase"):
        name = tc.get("name", "").split("[")[0]
        bad = tc.find("failure") is not None or tc.find("error") is not None
        py["failed" if bad else "passed"].append(tc.get("name"))
        py["functions"][name] = py["functions"].get(name, True) and not bad
    out["python"] = py
    (ROOT / "results" / "bms").mkdir(parents=True, exist_ok=True)
    (ROOT / "results" / "bms" / "host_tests.json").write_text(json.dumps(out, indent=1))
    ok = all(v["exit_code"] == 0 and not v["failed"] for v in out.values())
    print({k: (len(v["passed"]), len(v["failed"])) for k, v in out.items()})
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
