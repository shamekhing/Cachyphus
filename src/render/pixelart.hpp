#pragma once

#include <algorithm>
#include <cstdint>

#include "core/palette.hpp"
#include "raylib.h"

namespace cashyphus::art {

// Convert our raylib-free RGB to a raylib Color.
inline Color toColor(RGB c) { return Color{ c.r, c.g, c.b, 255 }; }
inline Color toColor(RGB c, unsigned char a) { return Color{ c.r, c.g, c.b, a }; }

inline Color lerpColor(Color a, Color b, float t) {
    auto ch = [t](unsigned char x, unsigned char y) -> unsigned char {
        const float v = x + (static_cast<float>(y) - x) * t;
        return static_cast<unsigned char>(std::clamp(v, 0.0f, 255.0f));
    };
    return Color{ ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), ch(a.a, b.a) };
}

// A sprite described as rows of ASCII. Each character indexes `palette`; the
// characters '.' and ' ' are transparent.
//
//   'o' outline   'h' hair     's' skin     'S' skin shadow
//   'c' cloth     'C' cloth dark  'b' boot  'w' highlight
//   'k' black/eye 'g' gold      'G' gold dark 'x' white/eye
struct CharMap {
    const char* const* rows;
    int width;
    int height;
};

// Index used by `lookupChar`, or -1 for transparent.
int charIndex(char c);

// Number of palette slots required by the character map scheme.
constexpr int CHAR_PALETTE_SIZE = 12;

// Build a CPU-side image (no GPU / window required). Rows shorter than `width`
// are treated as transparent, so authoring mistakes degrade gracefully.
Image imageFromCharMap(const CharMap& cm, const Color* palette);

// Build a Texture2D from the same data.
Texture2D textureFromCharMap(const CharMap& cm, const Color* palette);

// Validate that every row is exactly `width` characters. Returns the number of
// malformed rows (0 == perfect) and is used by the sprite dumper / tests.
int validateCharMap(const CharMap& cm);

} // namespace cashyphus::art
