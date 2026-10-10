# Credits and licences

Everything the game draws is generated in code. Two things are not: the pixel
font, which is a designed typeface and cannot be hand-authored convincingly, and
the audio, which is edited CC0 recordings catalogued in
`assets/audio/SOURCES.md`.

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

## Audio

All current game sounds and music are derived from CC0 coin, paper, and nature recordings. See [audio/SOURCES.md](audio/SOURCES.md) for original filenames, authors, URLs, licenses, and processing details. The earlier MintoDog chiptune and runtime synthesized effects are no longer used by the game build.
