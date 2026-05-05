#!/usr/bin/env python3
"""Bundle the presentation into a single self-contained HTML file.

Reads:
  deterministic-cut.html  (the dev shell — single source of truth for view)
  slides/manifest.json    (slide order)
  slides/*.md             (slide content + speaker notes)

Writes:
  dist/index.html         (one file, ready for GitHub Pages or email)

The dev shell's loader is dual-mode: if it finds embedded
<script type="text/markdown" data-slide="..."> blocks it uses them,
otherwise it falls back to fetch(). This script just embeds the slides.
"""

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SLIDES = ROOT / "slides"
SHELL = ROOT / "deterministic-cut.html"


def main(out: Path) -> int:
    manifest = json.loads((SLIDES / "manifest.json").read_text())
    shell = SHELL.read_text()

    blocks = [
        '<script id="slides-manifest" type="application/json">'
        + json.dumps(manifest, ensure_ascii=False)
        + "</script>"
    ]

    for fname in manifest["slides"]:
        text = (SLIDES / fname).read_text()
        if "</script>" in text:
            print(
                f"error: {fname} contains literal </script> — cannot embed safely",
                file=sys.stderr,
            )
            return 1
        blocks.append(
            f'<script type="text/markdown" data-slide="{fname}">\n{text}\n</script>'
        )

    embedded = "\n".join(blocks)

    if "</body>" not in shell:
        print("error: shell missing </body>", file=sys.stderr)
        return 1
    bundle = shell.replace("</body>", embedded + "\n</body>")

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(bundle)
    size_kb = out.stat().st_size // 1024
    print(f"wrote {out}  ({size_kb} KB, {len(manifest['slides'])} slides)")
    return 0


if __name__ == "__main__":
    if len(sys.argv) > 1:
        out = Path(sys.argv[1]).resolve()
    else:
        out = (ROOT.parent / "docs" / "index.html").resolve()
    sys.exit(main(out))
