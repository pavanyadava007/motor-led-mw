"""Host-side client for the node in QEMU: sends signed command frames over UDP,
checks the acks and records telemetry. Writes results/node_demo.json.

Frame: ver | seq(be16) | cmd | len | payload | AES-128-CMAC truncated to 8 bytes.
The demo key matches app/src/mac_psa.c (demo provisioning only).
"""

from __future__ import annotations

import json
import socket
import struct
import sys
import time
from pathlib import Path

from cryptography.hazmat.primitives.ciphers import algorithms
from cryptography.hazmat.primitives.cmac import CMAC

ROOT = Path(__file__).resolve().parents[1]
KEY = b"mlmw-demo-key-01"
ADDR = ("127.0.0.1", 5555)
STATUS = {0: "OK", -1: "ERR_LEN", -2: "ERR_VER", -3: "ERR_MAC", -4: "ERR_REPLAY",
          -5: "ERR_CMD", -6: "ERR_ARG"}


def frame(seq: int, cmd: int, payload: bytes = b"", key: bytes = KEY) -> bytes:
    body = bytes([1]) + struct.pack(">H", seq) + bytes([cmd, len(payload)]) + payload
    c = CMAC(algorithms.AES(key))
    c.update(body)
    return body + c.finalize()[:8]


class Node:
    def __init__(self) -> None:
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.settimeout(0.05)
        self.tlm: list[dict] = []
        self.t0 = time.time()

    def _pump(self, until: float, want_ack: bool = False):
        while time.time() < until:
            try:
                b, _ = self.s.recvfrom(256)
            except TimeoutError:
                continue
            if b[:1] == b"T" and len(b) == 24:
                t, tgt, rpm, duty, fault, temp, r, g, bl, acc, rej = struct.unpack(">IhhHBhHHHHH", b[1:])
                self.tlm.append({"host_s": round(time.time() - self.t0, 3), "node_ms": t, "target": tgt,
                                 "rpm": rpm, "duty": duty, "fault": fault, "led_temp": temp,
                                 "led_duty": [r, g, bl], "accepted": acc, "rejected": rej})
            elif b[:1] == b"A" and want_ack:
                return struct.unpack(">Hb", b[1:4])
        return None

    def send(self, data: bytes, timeout: float = 2.0):
        t = time.time()
        self.s.sendto(data, ADDR)
        ack = self._pump(t + timeout, want_ack=True)
        rtt = round((time.time() - t) * 1000, 1)
        return (STATUS.get(ack[1], ack[1]) if ack else None), rtt

    def watch(self, seconds: float):
        n = len(self.tlm)
        self._pump(time.time() + seconds)
        return self.tlm[n:]


def settle_time(trace: list[dict], target: int, tol: float = 0.05) -> float | None:
    """Node time (s) from the first sample until rpm stays within tol of target."""
    if not trace:
        return None
    t_first = trace[0]["node_ms"]
    inside_since = None
    for x in trace:
        ok = abs(x["rpm"] - target) <= abs(target) * tol
        inside_since = (inside_since or x["node_ms"]) if ok else None
    return None if inside_since is None else round((inside_since - t_first) / 1000, 2)


def main() -> int:
    n = Node()
    steps = []

    def step(name: str, data: bytes, expect: str):
        got, rtt = n.send(data)
        steps.append({"step": name, "expect": expect, "got": got, "rtt_ms": rtt, "pass": got == expect})
        print(f"{name:34s} expect {expect:10s} got {got}  ({rtt} ms)")

    # wait for the node to boot (first ack)
    for _ in range(60):
        got, _ = n.send(frame(1, 0x10, bytes([0, 0, 0, 0])), timeout=1.0)
        if got is not None:
            break
    seq = 2
    step("set LED amber", frame(seq, 0x10, bytes([255, 128, 0, 200])), "OK")
    seq += 1
    f_speed = frame(seq, 0x01, struct.pack(">h", 2000))
    step("set speed +2000 rpm", f_speed, "OK")
    fwd = n.watch(3.0)
    step("replay of the same frame", f_speed, "ERR_REPLAY")
    bad = bytearray(frame(seq + 1, 0x01, struct.pack(">h", 2500)))
    bad[-1] ^= 0x01
    step("tag with one bit flipped", bytes(bad), "ERR_MAC")
    step("frame signed with a wrong key", frame(seq + 1, 0x01, struct.pack(">h", 2500),
                                                key=b"not-the-real-key"), "ERR_MAC")
    seq += 1
    step("speed above limit (5000)", frame(seq, 0x01, struct.pack(">h", 5000)), "ERR_ARG")
    seq += 1
    step("unknown command 0x77", frame(seq, 0x77), "ERR_CMD")
    seq += 1
    step("set speed -1500 rpm (reverse)", frame(seq, 0x01, struct.pack(">h", -1500)), "OK")
    rev = n.watch(4.0)
    seq += 1
    step("stop", frame(seq, 0x02), "OK")
    stop = n.watch(2.0)
    seq += 1
    step("set LED white, full brightness", frame(seq, 0x10, bytes([255, 255, 255, 255])), "OK")
    led = n.watch(8.0)

    out = {
        "steps": steps,
        "all_pass": all(s["pass"] for s in steps),
        "forward": {"target": 2000, "final_rpm": fwd[-1]["rpm"] if fwd else None,
                    "settle_s_5pct": settle_time(fwd, 2000),
                    "peak_rpm": max((x["rpm"] for x in fwd), default=None)},
        "reverse": {"target": -1500, "final_rpm": rev[-1]["rpm"] if rev else None,
                    "settle_s_5pct": settle_time(rev, -1500)},
        "stop": {"final_rpm": stop[-1]["rpm"] if stop else None},
        "led": {"temp_start": led[0]["led_temp"] if led else None,
                "temp_end": led[-1]["led_temp"] if led else None,
                "duty_start": led[0]["led_duty"] if led else None,
                "duty_end": led[-1]["led_duty"] if led else None},
        "faults_seen": sorted({x["fault"] for x in n.tlm}),
        "telemetry_samples": len(n.tlm),
        "rtt_ms_median": sorted(s["rtt_ms"] for s in steps)[len(steps) // 2],
        "note": "QEMU emulation of mps2-an521 on a cloud VM; times are not hardware timings",
    }
    (ROOT / "results").mkdir(exist_ok=True)
    (ROOT / "results" / "node_demo.json").write_text(json.dumps(out, indent=1))
    (ROOT / "results" / "node_telemetry.json").write_text(json.dumps(n.tlm))
    print(json.dumps({k: v for k, v in out.items() if k != "steps"}, indent=1))
    return 0 if out["all_pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
