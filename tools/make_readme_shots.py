#!/usr/bin/env python3
"""Pick screenshots out of a capture run for the README.

`cashyphus --capture` already writes a PNG on every phase change, with the phase,
the life and the progress printed as it goes. That makes the shots reproducible,
but it does not make them *chooseable*: the files are numbered in the order they
were written and nothing says which one is a good picture of the game.

So this reads the run's log, picks by what the numbers mean -- a climb about half
way up, and the first choice screen -- and writes them to `assets/screenshots/`.
Picked and scaled, never drawn: if the art changes, run the capture again and
re-run this.

    mkdir -p /tmp/shots && cd /tmp/shots
    /path/to/cashyphus --capture s > capture.log
    python3 tools/make_readme_shots.py capture.log <repo>/assets/screenshots

Scaling: the capture tool enlarges by 3 for inspection, which is more than a README
needs and three times the bytes. Halving it is exact (each output pixel is a 2x2
block of the capture), so nothing is resampled, blurred, or half a pixel off --
which matters for a game whose art is one pixel wide in places. It lands at 1.5x
the frame.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

from PIL import Image

# game_state.hpp's Phase, in order.
PHASE = {0: "Title", 1: "Climb", 2: "Collapse", 3: "Celebrate", 4: "Silence",
         5: "RollDown", 6: "WalkIn", 7: "Choice", 8: "WalkAway", 9: "Credits"}

LINE = re.compile(r"captured (\S+) +\(phase=(\d+) life=(\d+) progress=([0-9.]+)\)")


def parse(log: Path) -> list[dict]:
    shots = []
    for m in LINE.finditer(log.read_text(encoding="utf-8", errors="replace")):
        shots.append({"file": m.group(1), "phase": int(m.group(2)),
                      "name": PHASE.get(int(m.group(2)), "?"),
                      "life": int(m.group(3)), "progress": float(m.group(4))})
    if not shots:
        raise SystemExit(f"make_readme_shots: no capture lines in {log}")
    return shots


def pick(shots: list[dict]) -> dict[str, dict]:
    """One shot per thing the README wants to show."""

    def first(phase: str) -> dict | None:
        return next((s for s in shots if s["name"] == phase), None)

    # A climb about half way up, on the first life: the ball is young, both meters
    # are in play, and the hill is not yet a wall of diagonal.
    climbs = [s for s in shots if s["name"] == "Climb" and s["life"] == 1]
    climb = min(climbs, key=lambda s: abs(s["progress"] - 0.55)) if climbs else first("Climb")

    chosen = {"climb": climb, "choice": first("Choice")}
    missing = [k for k, v in chosen.items() if v is None]
    if missing:
        raise SystemExit(f"make_readme_shots: the capture has no {', '.join(missing)} shot")
    return chosen


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2

    log, out = Path(sys.argv[1]), Path(sys.argv[2])
    shots = parse(log)
    picked = pick(shots)
    out.mkdir(parents=True, exist_ok=True)

    print(f"{len(shots)} captures in {log}")
    for name, shot in picked.items():
        src = log.parent / shot["file"]
        with Image.open(src) as im:
            if im.width % 2 or im.height % 2:
                raise SystemExit(f"make_readme_shots: {src} is an odd size; cannot halve it")
            half = im.resize((im.width // 2, im.height // 2), Image.NEAREST)
            dst = out / f"{name}.png"
            half.save(dst, optimize=True)
        print(f"  {name:<7} {shot['file']:<14} phase={shot['name']:<8} "
              f"progress={shot['progress']:.2f} -> {dst}  {dst.stat().st_size // 1024} KiB")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
