"""Loader for the McMaster LG 18650HG2 CSV files (Digatron export).

Columns used: Prog Time (h:m:s since program start), Step, Status, Voltage [V],
Current [A] (+ = charge), Temperature [C], Capacity [Ah] (running Ah counter of the tester).
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "bms" / "data" / "lg_hg2"


@dataclass
class Test:
    name: str
    t: np.ndarray  # s, from 0
    step: np.ndarray
    status: np.ndarray
    v: np.ndarray
    i: np.ndarray
    temp: np.ndarray
    ah: np.ndarray


def _prog_time(s: str) -> float:
    h, m, sec = s.split(":")
    return int(h) * 3600 + int(m) * 60 + float(sec)


def load(rel: str) -> Test:
    t, step, status, v, i, temp, ah = [], [], [], [], [], [], []
    with open(DATA / rel) as f:
        for line in f:
            p = line.rstrip("\n").split(",")
            if len(p) < 13 or not p[1].isdigit():
                continue
            t.append(_prog_time(p[3]))
            step.append(int(p[1]))
            status.append(p[2])
            v.append(float(p[8]))
            i.append(float(p[9]))
            temp.append(float(p[10]))
            ah.append(float(p[11]))
    ta = np.array(t)
    # drop duplicated timestamps (the logger repeats the last row of a step)
    keep = np.concatenate([[True], np.diff(ta) > 1e-6])
    return Test(
        rel,
        (ta - ta[0])[keep],
        np.array(step)[keep],
        np.array(status)[keep],
        np.array(v)[keep],
        np.array(i)[keep],
        np.array(temp)[keep],
        np.array(ah)[keep],
    )


def find(temp: str, kind: str) -> str:
    hits = sorted((DATA / temp).glob(f"*_{kind}.csv"))
    if not hits:
        raise FileNotFoundError(f"{temp}/{kind}: run bms/tools/fetch_data.py")
    return f"{temp}/{hits[0].name}"


def _cut(d: Test, n: int) -> Test:
    return Test(d.name, d.t[:n], d.step[:n], d.status[:n], d.v[:n], d.i[:n], d.temp[:n], d.ah[:n])


def load_hppc(temp: str) -> Test:
    """HPPC record from full to empty. Some files reset the tester Ah counter after the
    last pulse and then recharge the cell: the reset is removed (the rest after the
    discharge is kept) and the record is cut where the recharge starts."""
    d = load(find(temp, "HPPC"))
    ah = d.ah.copy()
    for k in np.where(np.abs(np.diff(ah)) > 0.3)[0]:
        ah[k + 1 :] -= ah[k + 1] - ah[k]
    d.ah = ah
    kmin = int(np.argmin(ah))
    n = len(ah)
    for k in range(kmin, len(ah)):
        if ah[k] - ah[kmin] > 0.05:
            n = k
            break
    return _cut(d, n)
