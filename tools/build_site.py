"""Build the static demo page (site/index.html) from results/.

Publish: python tools/build_site.py && python -c "from huggingface_hub import HfApi; \
HfApi().upload_folder(folder_path='site', repo_id='pavanyadava07/motor-led-mw', repo_type='space')"
"""

from __future__ import annotations

import html
import json
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RES = ROOT / "results"
SITE = ROOT / "site"


def md_to_html(md: str) -> str:
    out: list[str] = []
    rows: list[list[str]] = []

    def inline(s: str) -> str:
        s = html.escape(s)
        s = re.sub(r"`([^`]+)`", r"<code>\1</code>", s)
        return re.sub(r"\*\*([^*]+)\*\*", r"<b>\1</b>", s)

    def flush() -> None:
        if rows:
            head, body = rows[0], rows[2:]
            out.append("<div class='tw'><table><thead><tr>"
                       + "".join(f"<th>{inline(c)}</th>" for c in head) + "</tr></thead><tbody>"
                       + "".join("<tr>" + "".join(f"<td>{inline(c)}</td>" for c in r) + "</tr>"
                                 for r in body) + "</tbody></table></div>")
            rows.clear()

    para: list[str] = []

    def flush_para() -> None:
        if para:
            out.append(f"<p>{inline(' '.join(para))}</p>")
            para.clear()

    for line in md.splitlines():
        img = re.match(r"!\[([^\]]*)\]\(([^)]+)\)", line.strip())
        if img:
            flush_para()
            flush()
            name = Path(img.group(2)).name
            shutil.copy(ROOT / img.group(2), SITE / name)
            out.append(f"<img src='{name}' alt='{html.escape(img.group(1))}'>")
            continue
        if line.strip() and not line.startswith(("|", "#", "- ", "```")):
            para.append(line.strip())
            continue
        flush_para()
        if line.startswith("- "):
            para.append("\u2022 " + line[2:])
            continue
        if line.startswith("|"):
            rows.append([c.strip() for c in line.strip().strip("|").split("|")])
            continue
        flush()
        if line.startswith("### "):
            out.append(f"<h3>{inline(line[4:])}</h3>")
        elif line.startswith("## "):
            anchor = " id='bms'" if line.startswith("## BMS") else ""
            out.append(f"<h2{anchor}>{inline(line[3:])}</h2>")
        elif line.startswith("# "):
            continue
    flush_para()
    flush()
    return "\n".join(out)


def telemetry_svg() -> str:
    """Speed trace of the node demo as an inline SVG line chart."""
    t = json.loads((RES / "node_telemetry.json").read_text())
    if not t:
        return ""
    t0 = t[0]["node_ms"]
    xs = [(x["node_ms"] - t0) / 1000 for x in t]
    w, h, pad = 760, 260, 44
    xmax = max(xs) or 1
    ymin, ymax = -2500, 2500

    def px(x: float) -> float:
        return pad + (w - 2 * pad) * x / xmax

    def py(y: float) -> float:
        return h - pad - (h - 2 * pad) * (y - ymin) / (ymax - ymin)

    def path(key: str) -> str:
        return " ".join(f"{'M' if i == 0 else 'L'}{px(x):.1f},{py(v[key]):.1f}"
                        for i, (x, v) in enumerate(zip(xs, t, strict=True)))

    grid = "".join(f"<line x1='{pad}' x2='{w - pad}' y1='{py(v):.1f}' y2='{py(v):.1f}' class='g'/>"
                   f"<text x='{pad - 6}' y='{py(v) + 4:.1f}' class='ax' text-anchor='end'>{v}</text>"
                   for v in (-2000, -1000, 0, 1000, 2000))
    xt = "".join(f"<text x='{px(s):.1f}' y='{h - pad + 16}' class='ax' text-anchor='middle'>{s} s</text>"
                 for s in range(0, int(xmax) + 1, 2))
    return (f"<svg viewBox='0 0 {w} {h}' role='img' aria-label='Speed target and measured speed over time'>"
            f"{grid}{xt}<path d='{path('target')}' class='tg'/><path d='{path('rpm')}' class='rp'/>"
            f"<text x='{w - pad}' y='{pad - 12}' class='lg' text-anchor='end'>"
            f"<tspan class='ltg'>target rpm</tspan>  <tspan class='lrp'>measured rpm</tspan></text></svg>")


PAGE = """<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>motor-led-mw</title>
<style>
:root{--bg:#fbfbfa;--fg:#1c1c1c;--mut:#5c5c5c;--line:#e2e2de;--acc:#2a5db0;--acc2:#c2571a;--code:#f2f2ef}
@media (prefers-color-scheme:dark){:root{--bg:#141414;--fg:#e8e8e6;--mut:#a3a3a0;--line:#2e2e2c;--acc:#7aa7ff;--acc2:#f0a060;--code:#1f1f1e}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.55 system-ui,-apple-system,Segoe UI,sans-serif}
main{max-width:1000px;margin:0 auto;padding:24px 16px 64px}h1{font-size:26px;margin:0 0 4px}h2{margin-top:36px;font-size:20px}
h3{font-size:16px;margin-top:24px}.sub{color:var(--mut)}a{color:var(--acc)}code{background:var(--code);padding:1px 4px;border-radius:4px}
.tw{overflow-x:auto}table{border-collapse:collapse;width:100%;margin:8px 0;font-size:14px}th,td{border-bottom:1px solid var(--line);padding:6px 8px;text-align:left;vertical-align:top}
th{font-weight:600}svg,img{width:100%;height:auto;margin:8px 0}.g{stroke:var(--line)}.ax,.lg{fill:var(--mut);font-size:11px}
.tg{fill:none;stroke:var(--mut);stroke-width:1.5;stroke-dasharray:4 3}.rp{fill:none;stroke:var(--acc);stroke-width:2}
.ltg{fill:var(--mut)}.lrp{fill:var(--acc)}
</style></head><body><main>
<h1>motor-led-mw</h1>
<p class="sub">Motor control and LED middleware in C on Zephyr with TrustZone (TF-M), an Ethernet stack and PSA Crypto,
plus an AI agent that writes the unit tests and runs them on the target, and (since v0.2.0) a battery management (BMS)
module with SoC estimation validated on real cell data. All in QEMU (Arm MPS2 boards), no real hardware.
Jump to: <a href="#bms">BMS module</a>.
Code: <a href="https://github.com/pavanyadava007/motor-led-mw">github.com/pavanyadava007/motor-led-mw</a></p>
<h2>Node demo: speed over time</h2>
<p>Signed UDP commands to the node in QEMU: +2000 rpm, rejected frames (replay, bad tag, wrong key, over limit),
-1500 rpm, stop. Measured speed comes from the hall period, as the firmware sees it.</p>
__SVG__
__BODY__
</main></body></html>
"""


def main() -> None:
    readme = (ROOT / "README.md").read_text()
    results = readme[readme.index("## Results"):readme.index("## Run it")]
    SITE.mkdir(exist_ok=True)
    page = PAGE.replace("__SVG__", telemetry_svg()).replace("__BODY__", md_to_html(results))
    (SITE / "index.html").write_text(page)
    (SITE / "README.md").write_text("---\ntitle: motor-led-mw\nemoji: 🔧\ncolorFrom: blue\ncolorTo: gray\n"
                                    "sdk: static\npinned: false\nlicense: mit\n---\n")
    print("site/index.html", len(page))


if __name__ == "__main__":
    main()
