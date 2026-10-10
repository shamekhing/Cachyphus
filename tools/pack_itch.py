#!/usr/bin/env python3
"""Pack a build for itch.io.

itch.io's browser player wants a zip whose top level *is* the game: an
`index.html` and whatever it loads, no enclosing folder. That is what this
writes, in the two shapes the game actually admits:

    --hosted (default)
        A zip of a few kilobytes holding `web/itch/index.html`, the page that
        plays the copy already published on GitHub Pages. Nothing to rebuild,
        nothing to re-upload when the game changes, and it cannot fall out of
        step with the game -- but a player needs the network.

    --self-contained <build-dir>
        The whole WebAssembly build, staged under content hashes exactly as the
        published site is staged, plus the same credits. No dependency on
        Pages; a ~3 MB upload that has to be redone on every change.

Either way the zip carries the credits and the font's OFL text. An itch upload
is a distribution, and the OFL asks for its licence to travel with the font --
which here is baked into the wasm (`src/render/font_data.hpp`).

Usage:
    python3 tools/pack_itch.py [--hosted | --self-contained <build-dir>] [<out-dir>]

The zip lands in <out-dir> (default `dist/itch`, which is gitignored). On the
project's page: *Edit game -> Uploads -> "This file will be played in the
browser"*, then pick the zip. The cover image, screenshots and page copy are
separate fields, and separate work.
"""

from __future__ import annotations

import re
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WRAPPER = ROOT / "web" / "itch" / "index.html"

# Files that must travel with any distributed build.
ATTACHED = {
    "CREDITS.md": ROOT / "assets" / "CREDITS.md",
    "SOURCES.md": ROOT / "assets" / "audio" / "SOURCES.md",
    "LICENSE-OFL.txt": ROOT / "assets" / "fonts" / "LICENSE-OFL",
}

CONTROLS = """\
    LEFT CLICK  / SPACE      push
    RIGHT CLICK / SHIFT      hold to brace, or to let go and leave
    M                        mute
    [ / ]                    volume down / up
    F                        fullscreen
"""

HOSTED_NOTE = """\
The game is not inside this file: it is the page that plays the copy published
at https://shamekhing.github.io/Cachyphus/ -- so this build needs the network,
and in exchange it always plays the newest version of the game. There is no
copy in here that could fall out of step with it.
"""

LOCAL_NOTE = """\
Everything needed to play is in this file: the build is complete and fetches
nothing from anywhere else.
"""

README = """\
CASHYPHUS -- one hill. One lifetime. One more push.

Controls
{controls}
{note}
Credits and licences
    CREDITS.md        who made what
    SOURCES.md        every recording, and where it came from
    LICENSE-OFL.txt   the typeface's licence, which travels with the font
"""


def readme(note: str) -> str:
    return README.format(controls=CONTROLS, note=note)


def self_contained(build: Path) -> dict[str, bytes]:
    """Stage the build the way the site is staged, and hand back its files."""
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run(
            [sys.executable, str(ROOT / "tools" / "stage_site.py"), str(build), tmp],
            check=True,
        )
        return {
            p.name: p.read_bytes()
            for p in sorted(Path(tmp).iterdir())
            if not p.name.startswith(".")
        }


def collect(args: list[str]) -> tuple[str, dict[str, bytes]]:
    if args and args[0] == "--self-contained":
        if len(args) < 2:
            raise SystemExit("pack_itch: --self-contained needs a build directory")
        files = self_contained(Path(args[1]))
        mode, note = "self-contained", LOCAL_NOTE
    elif not args or args[0] == "--hosted":
        files = {"index.html": WRAPPER.read_bytes()}
        mode, note = "hosted", HOSTED_NOTE
    else:
        raise SystemExit(f"pack_itch: unknown option '{args[0]}'")

    files["README.txt"] = readme(note).encode()
    for name, path in ATTACHED.items():
        files[name] = path.read_bytes()
    return mode, files


def verify(zpath: Path) -> list[str]:
    """Re-read the zip and check it against what itch and a browser will do."""
    problems: list[str] = []
    with zipfile.ZipFile(zpath) as z:
        names = z.namelist()
        if "index.html" not in names:
            problems.append("index.html is not at the top level")
        nested = [n for n in names if "/" in n]
        if nested:
            problems.append(f"nested entries: {nested}")
        hidden = [n for n in names if Path(n).name.startswith(".")]
        if hidden:
            problems.append(f"dotfiles: {hidden}")

        html = z.read("index.html").decode("utf-8")
        # Relative references only: a URL carries a colon, a fragment a hash.
        for ref in re.findall(r'(?:src|href)="([^":#]+)"', html):
            if ref not in names:
                problems.append(f"index.html asks for '{ref}', which is not in the zip")

        loader = html + "".join(
            z.read(n).decode("utf-8", "replace") for n in names if n.endswith(".js")
        )
        for ref in sorted(set(re.findall(r"cashyphus\.[0-9a-f]{10}\.(?:js|wasm|data)", loader))):
            if ref not in names:
                problems.append(f"the loader asks for '{ref}', which is not in the zip")
    return problems


def main() -> int:
    args = sys.argv[1:]
    out = ROOT / "dist" / "itch"
    # An output directory, if given, is the last argument -- and a build
    # directory is the argument after --self-contained, never the last one.
    if args and not args[-1].startswith("--") and args[-2:-1] != ["--self-contained"]:
        out = Path(args.pop())

    mode, files = collect(args)
    out.mkdir(parents=True, exist_ok=True)
    zpath = out / f"cashyphus-itch-{mode}.zip"
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        for name, data in files.items():
            z.writestr(name, data)

    problems = verify(zpath)
    print(f"packed {zpath}  ({zpath.stat().st_size / 1024:.1f} KiB, {mode})")
    for name in sorted(files):
        print(f"  {name:<22} {len(files[name]):>9} bytes")
    if problems:
        print("\nTHIS ZIP WOULD NOT PLAY:", file=sys.stderr)
        for p in problems:
            print(f"  - {p}", file=sys.stderr)
        return 1

    print("\n  upload: the game's page -> Edit game -> Uploads ->")
    print('          "This file will be played in the browser" -> this zip')
    if not any((ROOT / n).exists() for n in ("LICENSE", "LICENSE.md", "COPYING")):
        print("\n  note: the repository has no LICENSE file, so what the code may be")
        print("        reused for is undefined. Worth settling before a public page.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
