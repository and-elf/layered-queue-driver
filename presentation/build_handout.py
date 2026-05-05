#!/usr/bin/env python3
"""Build a printable handout (HTML + PDF) from slides/ markdown files.

Each slide becomes one printed page: chapter, title, key points,
optional code, and the speaker notes ("Manus") below.

Output:
  handout.html  - browser-previewable
  handout.pdf   - rendered via headless chromium
"""

import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SLIDES = ROOT / "slides"


def parse_frontmatter(text: str) -> tuple[dict, str]:
    m = re.match(r"^---\n(.*?)\n---\n(.*)$", text, re.DOTALL)
    if not m:
        return {}, text
    fm: dict[str, str] = {}
    for line in m.group(1).split("\n"):
        km = re.match(r"^([a-zA-Z_][a-zA-Z0-9_]*):\s*(.*)$", line)
        if not km:
            continue
        v = km.group(2).strip()
        if (v.startswith('"') and v.endswith('"')) or (v.startswith("'") and v.endswith("'")):
            v = v[1:-1]
        fm[km.group(1)] = v
    return fm, m.group(2)


def split_notes(body: str) -> tuple[str, str]:
    lines = body.split("\n")
    for i, line in enumerate(lines):
        if line.strip() == "Note:":
            content = "\n".join(lines[:i]).rstrip()
            notes = "\n".join(lines[i + 1:]).strip()
            return content, notes
    return body.rstrip(), ""


def parse_cards(body: str) -> list[dict]:
    sections: list[dict] = []
    cur: dict | None = None
    for line in body.split("\n"):
        h = re.match(r"^##\s+(.+)$", line)
        if h:
            if cur:
                sections.append(cur)
            cur = {"label": h.group(1).strip(), "items": []}
            continue
        b = re.match(r"^-\s+(.+)$", line)
        if b and cur:
            cur["items"].append(b.group(1).strip())
    if cur:
        sections.append(cur)
    return sections


def esc(s: str) -> str:
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def inline(s: str) -> str:
    s = esc(s)
    s = re.sub(r"`([^`]+)`", r"<code>\1</code>", s)
    s = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", s)
    s = re.sub(r"\*([^*]+)\*", r"<em>\1</em>", s)
    return s


def render_notes(notes: str) -> str:
    if not notes:
        return ""
    paras = [
        f"<p>{inline(p.strip().replace(chr(10), ' '))}</p>"
        for p in re.split(r"\n\s*\n", notes)
        if p.strip()
    ]
    return f'<div class="notes"><h4>Manus</h4>{"".join(paras)}</div>'


def render_slide(idx: int, total: int, fm: dict, content: str, notes: str) -> str:
    layout = fm.get("layout", "")
    parts = [f'<section class="slide layout-{layout}">']
    parts.append(f'<div class="slide-num">{idx:02d} / {total:02d}</div>')

    if layout in ("title", "cards", "end"):
        if fm.get("chapter"):
            parts.append(f'<div class="chapter">{inline(fm["chapter"])}</div>')
        if fm.get("title"):
            parts.append(f"<h1>{inline(fm['title'])}</h1>")
        if fm.get("subtitle"):
            parts.append(f"<h2>{inline(fm['subtitle'])}</h2>")
        if fm.get("filmstrip"):
            parts.append(f'<div class="filmstrip">{inline(fm["filmstrip"])}</div>')

        if layout in ("cards", "end"):
            cards = parse_cards(content)
            parts.append('<div class="cards">')
            for c in cards:
                parts.append('<div class="card">')
                parts.append(f'<div class="label">{inline(c["label"])}</div>')
                parts.append("<ul>")
                for it in c["items"]:
                    parts.append(f"<li>{inline(it)}</li>")
                parts.append("</ul></div>")
            parts.append("</div>")

    elif layout == "code":
        if fm.get("caption"):
            parts.append(f'<div class="caption">{inline(fm["caption"])}</div>')
        parts.append(f"<pre><code>{esc(content.strip())}</code></pre>")

    elif layout == "quote":
        parts.append(f"<blockquote>{inline(content.strip())}")
        if fm.get("attrib"):
            parts.append(f'<div class="attrib">{inline(fm["attrib"])}</div>')
        parts.append("</blockquote>")

    else:
        parts.append(f'<p style="color:#c00">Unknown layout: {esc(layout or "?")}</p>')

    parts.append(render_notes(notes))
    parts.append("</section>")
    return "\n".join(parts)


PAGE_CSS = """
@page { size: A4; margin: 18mm 18mm 20mm 18mm; }

body {
  font-family: "Inter", "Helvetica Neue", Arial, sans-serif;
  color: #111;
  background: #fff;
  line-height: 1.45;
  font-size: 10.5pt;
  margin: 0;
}

h1, h2, h3, h4 { margin: 0; padding: 0; line-height: 1.15; }

.slide {
  display: block;
  page-break-after: always;
  break-after: page;
  min-height: 257mm;
}
.slide:last-child {
  page-break-after: auto;
  break-after: auto;
  min-height: 0;
}

.slide-num {
  color: #aaa;
  font-family: "JetBrains Mono", monospace;
  font-size: 8pt;
  text-align: right;
  margin-bottom: 0.6em;
}

.chapter {
  color: #b88a00;
  letter-spacing: 0.25em;
  text-transform: uppercase;
  font-size: 9pt;
  margin-bottom: 0.4em;
}

h1 { font-size: 22pt; font-weight: 700; margin-bottom: 0.3em; }
h2 { font-size: 14pt; color: #555; font-weight: 400; margin-bottom: 0.4em; }

.layout-title h1, .layout-end h1 { font-size: 26pt; }

.filmstrip {
  border-top: 2px dashed #ccc;
  border-bottom: 2px dashed #ccc;
  padding: 0.4em 0;
  margin: 0.6em 0;
  color: #777;
  font-style: italic;
  font-size: 10pt;
}

.cards {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 0.8em;
  margin: 0.8em 0 1em;
}
.card {
  border: 1px solid #ddd;
  padding: 0.6em 0.9em;
  background: #fafafa;
}
.card .label {
  color: #b88a00;
  font-size: 8pt;
  letter-spacing: 0.2em;
  text-transform: uppercase;
  margin-bottom: 0.4em;
}
.card ul { list-style: none; margin: 0; padding: 0; }
.card li {
  padding: 0.12em 0 0.12em 1em;
  text-indent: -1em;
  font-size: 10pt;
}
.card li::before { content: "—  "; color: #b88a00; }

.caption {
  color: #888;
  font-family: "JetBrains Mono", monospace;
  font-size: 9pt;
  margin-bottom: 0.5em;
  letter-spacing: 0.1em;
  text-transform: uppercase;
}

pre {
  background: #f6f6f6;
  border: 1px solid #ddd;
  padding: 0.8em 1em;
  margin: 0.4em 0 1em;
  font-family: "JetBrains Mono", "SF Mono", Menlo, monospace;
  font-size: 9pt;
  line-height: 1.4;
  white-space: pre-wrap;
  word-break: break-word;
}
pre code { background: none; padding: 0; font-family: inherit; }

blockquote {
  border-left: 4px solid #ddd;
  padding: 0.4em 1em;
  margin: 1em 0;
  font-style: italic;
  color: #333;
  font-size: 13pt;
}
blockquote .attrib {
  display: block;
  margin-top: 0.4em;
  color: #777;
  font-style: normal;
  font-size: 9pt;
}

.notes {
  border-top: 1px solid #e0e0e0;
  margin-top: 1.2em;
  padding-top: 0.8em;
}
.notes h4 {
  font-size: 8pt;
  letter-spacing: 0.25em;
  text-transform: uppercase;
  color: #b88a00;
  margin-bottom: 0.4em;
}
.notes p { margin: 0 0 0.6em; }

code {
  font-family: "JetBrains Mono", monospace;
  font-size: 0.92em;
  background: #f0f0f0;
  padding: 0.05em 0.3em;
  border-radius: 2px;
}
"""


def main() -> int:
    manifest = json.loads((SLIDES / "manifest.json").read_text())
    files = manifest["slides"]
    total = len(files)
    title = manifest.get("title", "Handout")

    rendered = []
    for idx, fname in enumerate(files, 1):
        text = (SLIDES / fname).read_text()
        fm, body = parse_frontmatter(text)
        content, notes = split_notes(body)
        rendered.append(render_slide(idx, total, fm, content, notes))

    html = f"""<!doctype html>
<html lang="sv">
<head>
<meta charset="utf-8">
<title>{esc(title)} — Handout</title>
<style>{PAGE_CSS}</style>
</head>
<body>
{chr(10).join(rendered)}
</body>
</html>
"""

    out_html = ROOT / "handout.html"
    out_pdf = ROOT / "handout.pdf"
    out_html.write_text(html)
    print(f"wrote {out_html}")

    cmd = [
        "chromium",
        "--headless=new",
        "--disable-gpu",
        "--no-sandbox",
        "--no-pdf-header-footer",
        f"--print-to-pdf={out_pdf}",
        f"file://{out_html}",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout, file=sys.stderr)
        print(r.stderr, file=sys.stderr)
        return r.returncode
    print(f"wrote {out_pdf}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
