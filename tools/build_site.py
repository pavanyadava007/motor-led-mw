"""Build the static demo page (site/index.html) from results/.

Publish: python tools/build_site.py && python -c "from huggingface_hub import HfApi; \
HfApi().upload_folder(folder_path='site', repo_id='pavanyadava07/motor-led-mw', repo_type='space')"
"""

from __future__ import annotations

import html
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RES = ROOT / "results"
SITE = ROOT / "site"
REPO = "https://github.com/pavanyadava007/motor-led-mw"
# README sections shown on the page (plain Markdown only: tables, lists, images, links)
SECTIONS = ["What problem does this solve?", "Key results", "Detailed results", "Limits"]


def slug(title: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", title.lower()).strip("-")


def sections(md: str, names: list[str]) -> str:
    """The README sections with these '## ' titles, in this order."""
    parts = re.split(r"(?m)^(?=## )", md)
    by_title = {p.splitlines()[0][3:].strip(): p for p in parts if p.startswith("## ")}
    return "\n".join(by_title[n] for n in names)


def md_to_html(md: str) -> str:
    out: list[str] = []
    rows: list[list[str]] = []

    def inline(s: str) -> str:
        s = html.escape(s)
        s = re.sub(r"`([^`]+)`", r"<code>\1</code>", s)
        s = re.sub(r"\[([^\]]+)\]\(([^)]+)\)",
                   lambda m: f"<a href='{m.group(2) if m.group(2).startswith(('http', '#')) else REPO + '/blob/main/' + m.group(2)}'>{m.group(1)}</a>", s)
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
            out.append(f"<h2 id='{slug(line[3:])}'>{inline(line[3:])}</h2>")
        elif line.startswith("# "):
            continue
    flush_para()
    flush()
    return "\n".join(out)


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
th{font-weight:600}img{width:100%;height:auto;margin:8px 0}
</style></head><body><main>
<h1>motor-led-mw</h1>
<p class="sub">Small embedded C software for a car-style control unit: a brushless motor and an RGB LED driven over
signed commands (Zephyr + TrustZone TF-M, PSA Crypto), a battery management module with an EKF state-of-charge
estimator checked on real cell data, and mutation testing that measures whether the tests catch bugs, including tests
written by a local AI agent. All in QEMU (Arm MPS2 boards), no real hardware.
Jump to: <a href="#key-results">Key results</a>, <a href="#detailed-results">Detailed results</a>,
<a href="#limits">Limits</a>.
Code, diagrams and the algorithms explained: <a href="https://github.com/pavanyadava007/motor-led-mw">github.com/pavanyadava007/motor-led-mw</a></p>
__BODY__
</main></body></html>
"""


def main() -> None:
    readme = (ROOT / "README.md").read_text()
    results = sections(readme, SECTIONS)
    SITE.mkdir(exist_ok=True)
    page = PAGE.replace("__BODY__", md_to_html(results))
    (SITE / "index.html").write_text(page)
    (SITE / "README.md").write_text("---\ntitle: motor-led-mw\nemoji: 🔧\ncolorFrom: blue\ncolorTo: gray\n"
                                    "sdk: static\npinned: false\nlicense: mit\n---\n")
    print("site/index.html", len(page))


if __name__ == "__main__":
    main()
