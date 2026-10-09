"""Download the McMaster LG 18650HG2 test data (Kollmeyer et al., Mendeley Data,
doi:10.17632/cp3473x7xv.3, CC BY 4.0) through the public Mendeley Data API.

Only the CSV files used by the SoC study are fetched (about 150 MB). Files are checked
against the SHA-256 published by the API. Output: bms/data/lg_hg2/<temp>/<file>.csv
(git-ignored).
"""

from __future__ import annotations

import hashlib
import json
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "bms" / "data" / "lg_hg2"
API = "https://data.mendeley.com/public-api/datasets/cp3473x7xv"
VERSION = 3
FOLDERS = {"25degC": "8b988119-7c3e-4d08-8341-bf1b3ec01353", "0degC": "366f9b27-0b4b-41a7-9010-c1bc0b7e5147"}
# C/20 discharge -> OCV curve, HPPC -> RC parameters, drive cycles -> held-out evaluation
WANT = ("C20DisCh", "HPPC", "UDDS", "US06", "LA92", "HWFET", "Mixed1", "Mixed2")


def _get(url: str) -> bytes:
    req = urllib.request.Request(url, headers={"User-Agent": "motor-led-mw-bms/0.2"})
    with urllib.request.urlopen(req, timeout=300) as r:
        return r.read()


def main() -> int:
    manifest = {}
    for temp, fid in FOLDERS.items():
        files = json.loads(_get(f"{API}/files?folder_id={fid}&version={VERSION}"))
        for f in files:
            name = f["filename"]
            if not name.endswith(".csv") or not any(f"_{w}." in name for w in WANT):
                continue
            dst = OUT / temp / name
            sha = f["content_details"]["sha256_hash"]
            if not (dst.exists() and hashlib.sha256(dst.read_bytes()).hexdigest() == sha):
                dst.parent.mkdir(parents=True, exist_ok=True)
                data = _get(f["content_details"]["download_url"])
                if hashlib.sha256(data).hexdigest() != sha:
                    print(f"sha256 mismatch: {name}", file=sys.stderr)
                    return 1
                dst.write_bytes(data)
            manifest[f"{temp}/{name}"] = sha
            print(temp, name, f["size"])
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
