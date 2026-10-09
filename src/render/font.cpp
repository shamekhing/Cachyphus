#include "render/font.hpp"

#include <cstring>

#include "core/config.hpp"
#include "render/font_data.hpp"
#include "raylib.h"

namespace cashyphus::art::text {
namespace {

::Font g_font{};
bool   g_ready = false;

} // namespace

void load() {
    if (g_ready) return;

    const int w = fontdata::GLYPH_COUNT * fontdata::CELL_W;
    const int h = fontdata::CELL_H;

    Image img = GenImageColor(w, h, BLANK);
    for (int g = 0; g < fontdata::GLYPH_COUNT; ++g) {
        for (int y = 0; y < fontdata::CELL_H; ++y) {
            const unsigned char bits = fontdata::GLYPHS[g][y];
            if (bits == 0) continue;
            for (int x = 0; x < fontdata::CELL_W; ++x) {
                if (bits & (0x80u >> x)) {
                    ImageDrawPixel(&img, g * fontdata::CELL_W + x, y, WHITE);
                }
            }
        }
    }

    g_font.baseSize     = fontdata::CELL_H;
    g_font.glyphCount   = fontdata::GLYPH_COUNT;
    g_font.glyphPadding = 0;
    g_font.texture      = LoadTextureFromImage(img);
    // Nearest-neighbour, or the whole point of a bitmap face is lost.
    SetTextureFilter(g_font.texture, TEXTURE_FILTER_POINT);

    g_font.recs   = static_cast<Rectangle*>(MemAlloc(sizeof(Rectangle) * g_font.glyphCount));
    g_font.glyphs = static_cast<GlyphInfo*>(MemAlloc(sizeof(GlyphInfo) * g_font.glyphCount));
    for (int i = 0; i < g_font.glyphCount; ++i) {
        g_font.recs[i] = Rectangle{ static_cast<float>(i * fontdata::CELL_W), 0.0f,
                                    static_cast<float>(fontdata::CELL_W),
                                    static_cast<float>(fontdata::CELL_H) };
        // raylib 5.5's GlyphInfo carries an Image, not a pointer, so build it
        // member-wise rather than with a brace list that would leave the Image
        // half-initialised.
        GlyphInfo g{};
        g.value    = fontdata::FIRST_CHAR + i;
        g.offsetX  = 0;
        g.offsetY  = 0;
        g.advanceX = fontdata::CELL_W;
        g_font.glyphs[i] = g;
    }

    UnloadImage(img);
    g_ready = true;
}

void unload() {
    if (!g_ready) return;
    UnloadTexture(g_font.texture);
    MemFree(g_font.recs);
    MemFree(g_font.glyphs);
    g_font  = ::Font{};
    g_ready = false;
}

bool ready() { return g_ready; }

int width(const char* s, int size) {
    if (s == nullptr) return 0;
    const int n = static_cast<int>(std::strlen(s));
    if (!g_ready) return MeasureText(s, size);
    // Every glyph advances exactly one cell, so the line is a clean multiple.
    return n * fontdata::CELL_W * size / fontdata::CELL_H;
}

void draw(const char* s, int x, int y, int size, Color c) {
    if (s == nullptr || s[0] == '\0') return;
    if (!g_ready) {
        DrawText(s, x, y, size, c);
        return;
    }
    DrawTextEx(g_font, s, Vector2{ static_cast<float>(x), static_cast<float>(y) },
               static_cast<float>(size), 0.0f, c);
}

void drawCenter(const char* s, int y, int size, Color c) {
    draw(s, (cfg::VIRTUAL_W - width(s, size)) / 2, y, size, c);
}

void drawRight(const char* s, int rightX, int y, int size, Color c) {
    draw(s, rightX - width(s, size), y, size, c);
}

} // namespace cashyphus::art::text
