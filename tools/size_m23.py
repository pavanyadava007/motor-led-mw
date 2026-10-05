"""Code and data size of the middleware for Cortex-M23 (the core in MOTIX TLE99xx and
LITIX Interior), arm-none-eabi-gcc -Os. Writes results/size_m23.json."""

import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FLAGS = ["-mcpu=cortex-m23", "-mthumb", "-Os", "-std=c99", "-ffunction-sections",
         "-Wall", "-Wextra", "-Werror", "-fstack-usage", "-c"]


def main() -> None:
    rows = {}
    with tempfile.TemporaryDirectory() as d:
        for src in sorted((ROOT / "mlmw" / "src").glob("*.c")):
            obj = Path(d) / (src.stem + ".o")
            subprocess.run(["arm-none-eabi-gcc", *FLAGS, f"-I{ROOT / 'mlmw' / 'include'}",
                            str(src), "-o", str(obj)], check=True)
            out = subprocess.run(["arm-none-eabi-size", str(obj)], capture_output=True,
                                 text=True, check=True).stdout.split("\n")[1].split()
            su = obj.with_suffix(".su").read_text().split("\n")
            stack = max(int(ln.split("\t")[1]) for ln in su if ln.strip())
            rows[src.stem] = {"text": int(out[0]), "data": int(out[1]), "bss": int(out[2]),
                              "max_stack_frame": stack}
    gcc = subprocess.run(["arm-none-eabi-gcc", "--version"], capture_output=True,
                         text=True).stdout.split("\n")[0]
    total = {k: sum(r[k] for r in rows.values()) for k in ("text", "data", "bss")}
    total["max_stack_frame"] = max(r["max_stack_frame"] for r in rows.values())
    res = {"compiler": gcc, "flags": " ".join(FLAGS), "modules": rows, "total": total}
    (ROOT / "results").mkdir(exist_ok=True)
    (ROOT / "results" / "size_m23.json").write_text(json.dumps(res, indent=1))
    print(json.dumps(total))


if __name__ == "__main__":
    main()
