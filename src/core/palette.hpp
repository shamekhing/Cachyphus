#pragma once

#include <cstdint>

namespace cashyphus {

struct RGB {
    std::uint8_t r = 0, g = 0, b = 0;
};

constexpr RGB rgb(int r, int g, int b) {
    return RGB{ static_cast<std::uint8_t>(r),
                static_cast<std::uint8_t>(g),
                static_cast<std::uint8_t>(b) };
}

RGB lerpRGB(RGB a, RGB b, float t);
RGB shade(RGB c, float amount);   // >0 lighten, <0 darken

// A full scene palette. Kept as plain 8-bit RGB so it is raylib-free; the
// render layer converts to raylib Color.
struct Palette {
    RGB skyTop, skyMid, skyBot;
    RGB sun;
    RGB mountainFar, mountainNear;
    RGB hillFar, hillNear, hillEdge, path;

    RGB ballLight, ballMid, ballDark, ballShadow, ballBand, ballFace;

    RGB skin, skinDark, hair, cloth, clothDark, outline;

    RGB bubbleBg, bubbleEdge, text;
    RGB uiText, uiDim, uiGood, uiWarn, uiBad;
};

// Warm, hopeful youth palette lerped toward a cold, drained summit palette.
Palette climbPalette(float progress);

// The warm/green palette seen only after walking away.
Palette walkPalette();

Palette blendPalette(const Palette& a, const Palette& b, float t);

} // namespace cashyphus
