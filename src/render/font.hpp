#pragma once

// =============================================================================
//  Pixel text
//  The game draws its UI with a real 8px bitmap face (Retro Pixel Arcade,
//  OFL), baked into the binary by tools/gen_font.py. raylib's built-in font
//  is smooth and vector-ish, which is exactly what 16-bit pixel art is not.
//
//  These helpers mirror the DrawText/MeasureText calls they replace, but are
//  pixel-exact: the glyphs are authored on a 12px cell, so draw at 12, 24 or
//  36 and every stroke lands on a whole pixel.
// =============================================================================

#include <cstddef>

#include "raylib.h"

namespace cashyphus::art::text {

// Cell metrics of the baked face. A text line is LINE_H tall.
constexpr int CELL_W = 8;
constexpr int CELL_H = 12;

// Natural (1x) size and the integer sizes worth using.
constexpr int SIZE_SMALL  = 12;
constexpr int SIZE_MEDIUM = 24;
constexpr int SIZE_LARGE  = 36;

// Loads the baked glyphs into a texture. Needs an open window / GPU context,
// so this is called from main() and never from the headless tools.
void load();
void unload();
bool ready();

// Width of `s` in pixels when drawn at `size`. Always an exact integer, so
// centred and right-aligned text never lands on a half pixel.
int width(const char* s, int size);

// Draw with the top-left of the line box at (x, y) -- the same convention as
// raylib's DrawText, so these are drop-in replacements.
void draw(const char* s, int x, int y, int size, Color c);
void drawCenter(const char* s, int y, int size, Color c);
void drawRight(const char* s, int rightX, int y, int size, Color c);

} // namespace cashyphus::art::text
