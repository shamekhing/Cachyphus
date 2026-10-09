#!/usr/bin/env python3
"""Stage the web build for publishing, under content-hashed asset names.

Why this exists: GitHub Pages serves everything with `cache-control:
max-age=600`, so a browser can still be holding `cashyphus.js` from one deploy
while fetching `cashyphus.wasm` from the next. The two are not
interchangeable -- a JS without the Asyncify runtime paired with a wasm that
calls `emscripten_sleep` aborts with "Please compile your program with async
support", which is exactly what a player reported seeing. There was no way to
defend against it while both files shared a fixed name, because any cache may
legitimately hold either one.

Renaming both to their own content hash and rewriting the references means a JS
can only ever be fetched alongside the wasm it was built against: a stale copy
misses the link and is simply not used.

Both references are pinned from `web/shell.html`:

  <script ... src="cashyphus.js">   -> cashyphus.<hash>.js
  var CASHYPHUS_WASM = '...'        -> cashyphus.<hash>.wasm

Usage:
    python3 tools/stage_site.py <build-dir> <out-dir>
"""

from __future__ import annotations

import hashlib
import re
import sys
from pathlib import Path

JS_MARK = 'src="cashyphus.js"'
WASM_MARK = "var CASHYPHUS_WASM = 'cashyphus.wasm';"


def short_hash(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()[:10]


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2

    build = Path(sys.argv[1])
    out = Path(sys.argv[2])

    html = (build / "cashyphus.html").read_text(encoding="utf-8")
    js = (build / "cashyphus.js").read_bytes()
    wasm = (build / "cashyphus.wasm").read_bytes()

    js_name = f"cashyphus.{short_hash(js)}.js"
    wasm_name = f"cashyphus.{short_hash(wasm)}.wasm"

    if JS_MARK not in html:
        raise SystemExit(f"stage_site: could not find {JS_MARK} in the generated html")
    if WASM_MARK not in html:
        raise SystemExit(f"stage_site: could not find {WASM_MARK} in the shell")

    html = html.replace(JS_MARK, f'src="{js_name}"')
    html = html.replace(WASM_MARK, f"var CASHYPHUS_WASM = '{wasm_name}';")

    out.mkdir(parents=True, exist_ok=True)
    (out / "index.html").write_text(html, encoding="utf-8")
    (out / js_name).write_bytes(js)
    (out / wasm_name).write_bytes(wasm)
    # Skip Jekyll so nothing gets rewritten or dropped.
    (out / ".nojekyll").write_text("", encoding="utf-8")

    # Drop any previously staged copies, so a rebuild cannot leave orphans that
    # would just be more things for a browser to cache.
    wanted = {"index.html", ".nojekyll", js_name, wasm_name}
    for stale in out.iterdir():
        if stale.name not in wanted:
            stale.unlink()

    print(f"staged {out}")
    print(f"  index.html   {len(html):>9} bytes")
    print(f"  {js_name:<13} {len(js):>9} bytes")
    print(f"  {wasm_name:<13} {len(wasm):>9} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
