#!/usr/bin/env python3
"""Bake the climb theme into an embedded PCM table.

The brief asks for "one repeating track with variations" that starts out
"motivational, almost comically cheerful" and gets "thinner and more
mechanical" over successive incarnations. That is a composed track plus an
arrangement that thins out, so the game ships a real CC0 chiptune and does the
thinning itself at playback time (see src/audio/music.cpp).

This script does the offline half: it finds a musically sensible loop in the
source, splices it seamlessly, and writes it out as a small PCM table.

Source : "Amusement park Stage" by MintoDog -- CC0 (public domain).
         https://opengameart.org/content/amusement-park-stage
         Tagged "positive" and "loopable", 150 BPM, which is what the brief
         asks of the climb theme.
Input  : the source as a WAV. It is an OGG upstream, so decode it first:
             build/debug/cashyphus_decode amusement_park_stage_bpm150.ogg s.wav
         The 64 s original is not committed -- the *output* below is.
Output : src/audio/music_data.cpp / .hpp

Note on mode detection: an earlier plan ranked candidate loops by major-vs-minor
correlation, but that measure turned out not to discriminate on pentatonic
chiptune (two very different tracks both scored within +/-0.1 of a tie). It is
still reported, for transparency, but it does not drive the choice. Brightness,
level and splice quality do.

Usage:
    python3 tools/gen_music.py /path/to/amusement_park_stage_bpm150.wav
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
DST_CPP = ROOT / "src" / "audio" / "music_data.cpp"
DST_HPP = ROOT / "src" / "audio" / "music_data.hpp"

TRACK_TITLE = "Amusement park Stage"
TRACK_AUTHOR = "MintoDog"
SOURCE_URL = "https://opengameart.org/sites/default/files/amusement_park_stage_bpm150.ogg"

OUT_RATE = 22050     # chiptune lives well below this, and it halves the table
BARS = 8             # eight bars: long enough not to nag, short enough to loop
BEATS_PER_BAR = 4
XFADE_S = 0.04       # seam crossfade
TARGET_PEAK = 0.86


def load_mono(path: Path) -> tuple[np.ndarray, int]:
    """Minimal RIFF reader; OGA uploads are usually float WAVs, which the
    stdlib `wave` module rejects outright."""
    blob = path.read_bytes()
    if blob[:4] != b"RIFF" or blob[8:12] != b"WAVE":
        raise SystemExit("not a RIFF/WAVE file")

    pos, fmt, pcm = 12, None, None
    while pos + 8 <= len(blob):
        cid = blob[pos:pos + 4]
        csz = int.from_bytes(blob[pos + 4:pos + 8], "little")
        body = blob[pos + 8:pos + 8 + csz]
        if cid == b"fmt ":
            fmt = body
        elif cid == b"data":
            pcm = body
        pos += 8 + csz + (csz & 1)
    if fmt is None or pcm is None:
        raise SystemExit("missing fmt or data chunk")

    tag = int.from_bytes(fmt[0:2], "little")
    nch = int.from_bytes(fmt[2:4], "little")
    rate = int.from_bytes(fmt[4:8], "little")
    bits = int.from_bytes(fmt[14:16], "little")

    if tag == 3:
        x = np.frombuffer(pcm, dtype="<f4").astype(np.float64)
    elif tag == 1 and bits == 16:
        x = np.frombuffer(pcm, dtype="<i2").astype(np.float64) / 32768.0
    else:
        raise SystemExit(f"unsupported tag={tag} bits={bits}")

    if nch > 1:
        x = x.reshape(-1, nch).mean(axis=1)
    return x, rate


def onset_envelope(x: np.ndarray, rate: int, hop: int = 256, win: int = 1024):
    nfr = 1 + (len(x) - win) // hop
    idx = np.arange(win)[None, :] + hop * np.arange(nfr)[:, None]
    spec = np.abs(np.fft.rfft(x[idx] * np.hanning(win), axis=1))
    flux = np.maximum(0.0, np.diff(spec, axis=0)).sum(axis=1)
    return flux - flux.mean(), rate / hop


def estimate_beat(flux: np.ndarray, fps: float) -> float:
    """Strongest periodicity in a 'workout tempo' window, in seconds."""
    ac = np.correlate(flux, flux, "full")[len(flux) - 1:]
    lo, hi = int(fps * 60 / 200), int(fps * 60 / 90)   # 200..90 BPM
    lag = lo + int(np.argmax(ac[lo:hi]))
    return lag / fps


# Krumhansl-Schmuckler key profiles, used only to tell cheerful from sad.
MAJOR = np.array([6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88])
MINOR = np.array([6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17])


def frame_features(x: np.ndarray, rate: int, win: int = 4096, hop: int = 1024):
    """Per-frame chroma, energy and spectral centroid.

    Computed once for the whole track so candidate loops can be scored from
    prefix sums rather than re-analysing every one.
    """
    nfr = 1 + (len(x) - win) // hop
    idx = np.arange(win)[None, :] + hop * np.arange(nfr)[:, None]
    spec = np.abs(np.fft.rfft(x[idx] * np.hanning(win), axis=1))
    freqs = np.fft.rfftfreq(win, 1 / rate)

    band = (freqs >= 55) & (freqs <= 2200)
    midi = 69 + 12 * np.log2(np.maximum(freqs[band], 1e-9) / 440.0)
    pc = np.round(midi).astype(int) % 12
    onehot = np.zeros((int(band.sum()), 12))
    onehot[np.arange(int(band.sum())), pc] = 1.0

    chroma = spec[:, band] @ onehot
    energy = spec.sum(axis=1) + 1e-9
    centroid = (spec * freqs).sum(axis=1) / energy
    return chroma, energy, centroid, rate / hop


def major_margin(chroma: np.ndarray) -> float:
    """How much more major than minor a chroma vector is, in [-2, 2]."""
    c = chroma - chroma.mean()
    nc = np.linalg.norm(c) + 1e-12

    def corr(t):
        t = t - t.mean()
        return float(np.dot(c, t) / (nc * np.linalg.norm(t)))

    best_major = max(corr(np.roll(MAJOR, r)) for r in range(12))
    best_minor = max(corr(np.roll(MINOR, r)) for r in range(12))
    return best_major - best_minor


def best_loop_start(x: np.ndarray, rate: int, loop_len: int, beat: int):
    """Pick the loop that is brightest, best arranged, and joins cleanly.

    Selection runs on brightness, level and splice quality. Key correlation is
    deliberately not used to choose: it fails to separate major from minor on
    pentatonic chiptune, so trusting it would be trusting noise.
    """
    edge = int(0.03 * rate)
    total_beats = (len(x) - loop_len) // beat
    # Wide: the most cheerful section is not reliably in the middle of a track.
    lo, hi = total_beats // 10, max(total_beats // 10 + 1, total_beats * 92 // 100)

    chroma, energy, centroid, fps = frame_features(x, rate)
    cs_c = np.vstack((np.zeros(12), np.cumsum(chroma, axis=0)))
    cs_e = np.concatenate(([0.0], np.cumsum(energy)))
    cs_x = np.concatenate(([0.0], np.cumsum(centroid)))
    mean_energy = cs_e[-1] / max(1, len(x) / rate * fps)

    def mean_of(csum, f0: int, f1: int):
        return (csum[f1] - csum[f0]) / max(1, f1 - f0)

    best, best_score, best_info = lo * beat, None, (0.0, 0.0, 0.0, 0.0)
    for k in range(lo, hi):
        s = k * beat
        if s < edge or s + loop_len + edge > len(x):
            continue
        f0 = int(s / rate * fps)
        f1 = int((s + loop_len) / rate * fps)
        if f1 - f0 < 4 or f1 >= len(cs_e):
            continue

        margin = major_margin(mean_of(cs_c, f0, f1))
        bright = mean_of(cs_x, f0, f1) / (rate / 2)            # 0..1 of Nyquist
        loud = min(mean_of(cs_e, f0, f1) / (mean_energy + 1e-9), 2.0)
        seam = float(np.sqrt(((x[s - edge:s] - x[s + loop_len - edge:s + loop_len]) ** 2).mean()))

        score = 1.0 * bright + 0.1 * loud - 3.0 * seam
        if best_score is None or score > best_score:
            best, best_score, best_info = s, score, (margin, bright, loud, seam)
    return best, (best_score if best_score is not None else 0.0), best_info


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    src = Path(sys.argv[1])
    x, rate = load_mono(src)
    print(f"source          {src.name}  {rate} Hz  {len(x) / rate:.1f} s")

    flux, fps = onset_envelope(x, rate)
    beat_s = estimate_beat(flux, fps)
    beat = max(1, int(round(beat_s * rate)))
    print(f"beat            {beat_s:.3f} s  ({60 / beat_s:.0f} BPM)")

    loop_len = beat * BEATS_PER_BAR * BARS
    if loop_len > len(x) // 2:
        loop_len = beat * BEATS_PER_BAR * 4
    print(f"loop            {loop_len / rate:.2f} s  ({loop_len // beat} beats, "
          f"{loop_len / (beat * BEATS_PER_BAR):.0f} bars)")

    start, score, info = best_loop_start(x, rate, loop_len, beat)
    margin, bright, loud, seam = info
    print(f"chosen section  major-margin {margin:+.3f}  brightness {bright * 100:.0f}% "
          f"of Nyquist  level x{loud:.2f}  seam {seam:.5f}")
    loop = x[start:start + loop_len].copy()

    # Crossfade the seam so the join is inaudible: the loop's head is blended
    # with the audio immediately preceding it, which is what makes it wrap.
    xf = int(XFADE_S * rate)
    if xf > 0 and len(loop) > 2 * xf and start >= xf:
        head = loop[:xf].copy()
        tail = x[start - xf:start]
        t = np.linspace(0.0, 1.0, xf)
        loop[:xf] = tail * (1.0 - t) + head * t

    n_out = int(len(loop) * OUT_RATE / rate)
    loop = np.interp(np.arange(n_out) / OUT_RATE * rate, np.arange(len(loop)), loop)

    peak = float(np.abs(loop).max())
    if peak > 0:
        loop *= TARGET_PEAK / peak
    pcm = np.clip(np.round(loop * 32767.0), -32768, 32767).astype("<i2")

    DST_HPP.write_text(
        "// Generated by tools/gen_music.py -- do not edit by hand.\n"
        "//\n"
        f'// "{TRACK_TITLE}" by {TRACK_AUTHOR}, CC0 (public domain).\n'
        f"// {SOURCE_URL}\n"
        "\n#pragma once\n\n#include <cstddef>\n\n"
        "namespace cashyphus::audio::musicdata {\n\n"
        f"constexpr int SAMPLE_RATE  = {OUT_RATE};\n"
        f"constexpr int SAMPLE_COUNT = {len(pcm)};\n\n"
        "extern const short PCM[SAMPLE_COUNT];\n\n"
        "} // namespace cashyphus::audio::musicdata\n",
        encoding="utf-8")

    values = np.asarray(pcm, dtype=int).tolist()
    with DST_CPP.open("w", encoding="utf-8") as f:
        f.write("// Generated by tools/gen_music.py -- do not edit by hand.\n")
        f.write(f'// "{TRACK_TITLE}" by {TRACK_AUTHOR}, CC0 (public domain).\n')
        f.write(f"// {SOURCE_URL}\n")
        f.write('#include "audio/music_data.hpp"\n\n')
        f.write("namespace cashyphus::audio::musicdata {\n\n")
        f.write("const short PCM[SAMPLE_COUNT] = {\n")
        for i in range(0, len(values), 16):
            f.write("    " + ",".join(str(v) for v in values[i:i + 16]) + ",\n")
        f.write("};\n\n} // namespace cashyphus::audio::musicdata\n")

    print(f"loop start      {start / rate:.3f} s   score {score:.6f}")
    print(f"wrote           {DST_CPP.relative_to(ROOT)}  ({len(pcm)} samples, "
          f"{len(pcm) / OUT_RATE:.2f} s, {DST_CPP.stat().st_size / 1e6:.1f} MB)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
