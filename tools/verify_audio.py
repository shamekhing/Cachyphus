#!/usr/bin/env python3
"""Check the rendered audio against what the game actually loads.

The effects and loops are committed files, so no build step would notice if one
went missing, silent, DC-offset, the wrong length, or stopped looping cleanly.
This reads the same names the game reads (out of src/audio/synth.cpp), measures
every file the loader would ask for, and fails loudly rather than quietly
shipping a placeholder. It also checks the two things the score promises: that
aging darkens it, and that the ambience bed is actually audible under the music.

    python3 tools/verify_audio.py

Needs NumPy, and the native build for cashyphus_decode (raylib's decoders are the
only ones here, and they are the same ones the game uses at runtime).
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import wave

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
A = ROOT / "assets/audio"
RATE = 44100
LOOP = 19.2                     # 32 beats at 100 BPM
LOOP_TOLERANCE = 0.01
problems = []


def note(ok, message):
    if not ok:
        problems.append(message)


def names(source, array):
    text = (ROOT / source).read_text(encoding="utf-8")
    m = re.search(re.escape(array) + r"\[\]\s*=\s*\{([^}]*)\}", text)
    if not m:
        raise SystemExit(f"verify_audio: no {array} in {source}")
    return re.findall(r'"([^"]+)"', m.group(1))


def load(path):
    with wave.open(str(path)) as w:
        n, rate, ch = w.getnframes(), w.getframerate(), w.getnchannels()
        a = np.frombuffer(w.readframes(n), "<i2").astype(np.float32)
        a = a.reshape(-1, ch) / 32768.0
    return a, rate


def decoder():
    for build in ("debug", "release"):
        d = ROOT / "build" / build / "cashyphus_decode"
        if d.exists():
            return d
    raise SystemExit("verify_audio: build the native game first (cashyphus_decode)")


def decode(path, into):
    out = Path(into) / (path.stem + ".wav")
    p = subprocess.run([str(decoder()), str(path), str(out)], capture_output=True)
    if p.returncode != 0 or not out.exists():
        raise SystemExit(f"verify_audio: raylib could not decode {path.name}")
    return load(out)


def centroid(a):
    x = a[:, 0]
    X = np.abs(np.fft.rfft(x * np.hanning(len(x))))
    f = np.fft.rfftfreq(len(x), 1 / RATE)
    return float((X * f).sum() / X.sum())


def edges(a):
    """Signal at both ends: a one-shot should start and end at rest, or the cone
    steps and that step is a click."""
    return float(max(abs(a[0]).max(), abs(a[-1]).max()))


def wrap_step(a):
    """Biggest sample-to-sample jump across the loop point, against the biggest
    jump inside the loop. A seam that fits is no worse than the music's own
    transients."""
    seq = np.concatenate((a[-64:], a[:64]))
    across = float(np.abs(np.diff(seq, axis=0)).max())
    inside = float(np.abs(np.diff(a, axis=0)).max())
    return across, inside


print(f"{'effect':<18}{'sec':>7}{'peak':>7}{'rms':>8}{'dc':>10}{'ends':>8}")
for name in names("src/audio/synth.cpp", "effectNames"):
    path = A / "sfx" / f"{name}.wav"
    if not path.exists():
        note(False, f"missing {path.relative_to(ROOT)}")
        print(f"{name:<18}  MISSING")
        continue
    a, rate = load(path)
    mono = a.mean(axis=1)
    sec, peak = len(a) / rate, float(np.abs(a).max())
    rms, dc = float(np.sqrt((mono**2).mean())), float(mono.mean())
    print(f"{name:<18}{sec:7.2f}{peak:7.3f}{rms:8.4f}{dc:+10.5f}{edges(a):8.3f}")
    note(rate == RATE and a.shape[1] == 1, f"{name}.wav is not {RATE} Hz mono")
    note(0.02 <= sec <= 3.0, f"{name}.wav is {sec:.2f}s, outside 0.02-3.0s")
    note(peak <= 0.99, f"{name}.wav peaks at {peak:.3f}, too close to clipping")
    note(peak >= 0.05 and rms >= 0.004,
         f"{name}.wav is effectively silent (peak {peak:.3f}, rms {rms:.4f})")
    note(abs(dc) <= 0.002, f"{name}.wav carries {dc:+.4f} of DC, a step at both edges")
    note(edges(a) <= 0.05, f"{name}.wav starts or ends at {edges(a):.3f}, which is a click")

songs = names("src/audio/synth.cpp", "trackNames")
print(f"\n{'loop':<20}{'sec':>7}{'peak':>7}{'rms':>8}{'dc':>9}{'seam':>15}  centroid")
tone = {}
with tempfile.TemporaryDirectory() as tmp:
    for name in songs:
        path = A / ("ambience" if name == "freedom" else "music") / f"{name}.ogg"
        if not path.exists():
            note(False, f"missing {path.relative_to(ROOT)}")
            print(f"{name:<20}  MISSING")
            continue
        a, rate = decode(path, tmp)
        sec, peak = len(a) / rate, float(np.abs(a).max())
        rms, dc = float(np.sqrt((a**2).mean())), float(a.mean())
        across, inside = wrap_step(a)
        c = centroid(a)
        print(f"{name:<20}{sec:7.3f}{peak:7.3f}{rms:8.4f}{dc:+9.5f}"
              f"{across:8.3f}/{inside:<7.3f}{c:9.0f}")
        note(rate == RATE and a.shape[1] == 2, f"{name}.ogg is not {RATE} Hz stereo")
        note(abs(sec - LOOP) <= LOOP_TOLERANCE,
             f"{name}.ogg is {sec:.3f}s, not the shared {LOOP}s bar")
        note(peak <= 0.99, f"{name}.ogg peaks at {peak:.3f}, too close to clipping")
        note(rms >= 0.004, f"{name}.ogg is effectively silent (rms {rms:.4f})")
        note(abs(dc) <= 0.002, f"{name}.ogg carries {dc:+.4f} of DC")
        note(across <= inside,
             f"{name}.ogg jumps {across:.3f} across the seam, more than its own {inside:.3f} inside it")
        tone[name] = (c, rms)

# The score ages: the late incarnations have to measure darker than the early ones.
early = [tone[n][0] for n in ("young", "base", "adult") if n in tone]
late = [tone[n][0] for n in ("old", "final", "endless") if n in tone]
if early and late:
    note(np.mean(late) < np.mean(early),
         f"aging does not darken the score: late {np.mean(late):.0f} Hz vs early {np.mean(early):.0f} Hz")
    print(f"\naging: early {np.mean(early):.0f} Hz -> late {np.mean(late):.0f} Hz")

# The ending's natural bed has to be audible under the music bus, or walking away
# plays to nobody.
if "freedom" in tone and early:
    music = np.mean([tone[n][1] for n in tone if n != "freedom"])
    ratio = tone["freedom"][1] / music
    note(ratio >= 0.25, f"freedom.ogg sits {20*np.log10(max(ratio,1e-6)):.1f} dB under the score")
    print(f"ambience: {ratio:.2f} of the score's level ({20*np.log10(max(ratio,1e-6)):+.1f} dB)")

if problems:
    print()
    for p in problems:
        print("verify_audio FAIL:", p)
    sys.exit(1)

print(f"\nverify_audio: {len(songs)} loops and every effect load, measure and loop cleanly")

