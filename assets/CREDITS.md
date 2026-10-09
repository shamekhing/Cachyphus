# Credits and licences

Everything the game draws or plays is generated in code, with one deliberate
exception: the pixel font, which is a designed typeface and cannot be
hand-authored convincingly.

## Fonts

### Retro Pixel Arcade

- **Source:** `assets/fonts/retro-pixel-arcade.bdf`
- **Author:** TakWolf  <https://github.com/TakWolf/retro-pixel-font>
- **Licence:** SIL Open Font License 1.1  see `assets/fonts/LICENSE-OFL`
- **Used for:** all text in the game.

The BDF is committed as the original, unmodified source. `tools/gen_font.py`
bakes it into `src/render/font_data.hpp`, a bit-packed glyph table that the
build compiles straight into the binary.

Two consequences worth knowing:

- the shipping game needs **no font file at runtime**, so the desktop binary
  stays self-contained and the Emscripten build needs no `--preload-file`;
- the build itself needs **no Python**  the generated header is committed, and
  the generator only has to be re-run when the font is replaced.

To regenerate after changing the BDF or the cell metrics:

```sh
python3 tools/gen_font.py
```

## Music

### Amusement park Stage  the climb theme

- **Author:** MintoDog  <https://opengameart.org/content/amusement-park-stage>
- **Licence:** CC0 1.0 (public domain)  no attribution required, credited anyway
- **Upstream:** `amusement_park_stage_bpm150.ogg`, 64 s, 150 BPM, tagged
  *positive* and *loopable*
- **Used for:** the repeating climb theme.

The brief asks for music that starts out *"motivational, almost comically
cheerful"* and becomes *"thinner and more mechanical"* over successive
incarnations. That is a composed track plus an arrangement that thins out, so
the game ships a real track and does the thinning itself at playback time 
see `src/core/music.hpp` for the curve and `src/audio/climb.cpp` for the
treatment.

The 64 s original is **not committed**: the game needs one looping section of
it, not 1.9 MB of compressed audio. `tools/gen_music.py` picks the eight-bar
section that stitches together most cleanly, crossfades the seam, folds it to
mono, resamples it, and writes `src/audio/music_data.cpp`  the same
bake-once-and-commit-the-result arrangement the font uses.

To rebuild the table:

```sh
# Decode the OGG to WAV first. raylib already carries the decoder, so this
# needs no audio toolchain.
build/debug/cashyphus_decode amusement_park_stage_bpm150.ogg /tmp/ap.wav
python3 tools/gen_music.py /tmp/ap.wav
```

`tools/gen_music.py` prints what it chose and why, including a major/minor
correlation. That correlation is reported for information only and does **not**
drive the selection: it fails to separate major from minor on pentatonic
chiptune, so it is not trustworthy enough to choose on.

### The escape motif

Synthesised at runtime in `src/audio/synth.cpp`, as is every sound effect.
