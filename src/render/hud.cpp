#include "render/hud.hpp"

#include <cstdio>

#include "core/config.hpp"
#include "render/pixelart.hpp"
#include "render/scene.hpp"

namespace cashyphus::hud {

using namespace cfg;
using art::toColor;

// =============================================================================
//  Small widgets
// =============================================================================
namespace {

void meter(int x, int y, int w, int h, float frac, Color fill, Color empty, Color border) {
    frac = clampf(frac, 0.0f, 1.0f);
    DrawRectangle(x - 1, y - 1, w + 2, h + 2, border);
    DrawRectangle(x, y, w, h, empty);
    const int fw = static_cast<int>(w * frac + 0.5f);
    if (fw > 0) DrawRectangle(x, y, fw, h, fill);
}

void panel(int x, int y, int w, int h, const Palette& pal) {
    DrawRectangle(x, y, w, h, toColor(pal.bubbleBg, 220));
    DrawRectangleLines(x, y, w, h, toColor(pal.bubbleEdge));
}

void label(const char* s, int x, int y, Color c) { DrawText(s, x, y, 10, c); }

void labelRight(const char* s, int rightX, int y, Color c) {
    DrawText(s, rightX - MeasureText(s, 10), y, 10, c);
}

void labelCenter(const char* s, int y, Color c) {
    DrawText(s, (VIRTUAL_W - MeasureText(s, 10)) / 2, y, 10, c);
}

// --- overlays ---------------------------------------------------------------

void drawTitle(const Palette& pal) {
    DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, Color{ 12, 10, 16, 150 });
    const Color gold = toColor(pal.ballBand);
    const Color txt  = toColor(pal.uiText);

    DrawText("CASHYPHUS", (VIRTUAL_W - MeasureText("CASHYPHUS", 30)) / 2, 44, 30, gold);
    labelCenter("One hill. One lifetime. One more push.", 82, txt);
    labelCenter("a pixel allegory about pushing money uphill", 96, toColor(pal.uiDim));

    if (static_cast<int>(GetTime() * 2.0f) % 2 == 0) {
        labelCenter("PRESS SPACE TO BEGIN", 124, txt);
    }
    labelCenter("SPACE  push      SHIFT  brace", 150, toColor(pal.uiDim));
    labelCenter("the hill is patient.", 164, toColor(pal.uiDim));
}

void drawChoice(const Palette& pal) {
    // Sits in the empty sky above the ball and the waiting character, so it
    // hides neither of them.
    const int w = 218, h = 64;
    const int x = (VIRTUAL_W - w) / 2;
    const int y = 20;
    panel(x, y, w, h, pal);

    const Color txt = toColor(pal.uiText);
    DrawText("Well, shall we?", x + 12, y + 6, 10, toColor(pal.ballBand));
    DrawRectangle(x + 10, y + 20, w - 20, 1, toColor(pal.uiDim));

    DrawText("KEEP PUSHING", x + 12, y + 27, 10, toColor(pal.uiWarn));
    DrawText("[SPACE]", x + w - 12 - MeasureText("[SPACE]", 10), y + 27, 10, txt);

    DrawText("WALK AWAY", x + 12, y + 45, 10, toColor(pal.uiGood));
    DrawText("[SHIFT]", x + w - 12 - MeasureText("[SHIFT]", 10), y + 45, 10, txt);
}

void drawWalkAwayText(const Game& g, const Palette& pal) {
    if (g.walkT() > 0.55f && g.walkT() < 0.96f) {
        DrawRectangle(0, 70, VIRTUAL_W, 24, Color{ 12, 10, 16, 140 });
        labelCenter("You cannot buy back the time you never spent.", 78, toColor(pal.uiText));
    }
}

void drawCredits(const Game& g, const Palette& pal) {
    DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, Color{ 8, 8, 10, 210 });
    const Color txt = toColor(pal.uiText);
    const Color dim = toColor(pal.uiDim);

    labelCenter("You can leave the hill.", 50, txt);
    labelCenter("But the hill will always find someone", 66, txt);
    labelCenter("willing to climb.", 80, txt);

    char buf[96];
    std::snprintf(buf, sizeof(buf), "incarnations spent pushing: %d", g.completedLives());
    labelCenter(buf, 106, dim);
    std::snprintf(buf, sizeof(buf), "times you chose to continue: %d", g.timesContinued());
    labelCenter(buf, 120, dim);

    if (static_cast<int>(GetTime() * 2.0f) % 2 == 0) {
        labelCenter("PRESS SPACE", 150, toColor(pal.ballBand));
    }
}

} // namespace

// =============================================================================
//  Roman numerals
// =============================================================================
const char* roman(int n) {
    static const char* const kRoman[] = {
        "0", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X",
        "XI", "XII", "XIII", "XIV", "XV", "XVI", "XVII", "XVIII", "XIX", "XX",
    };
    static char buf[16];
    if (n >= 1 && n <= 20) return kRoman[n];
    std::snprintf(buf, sizeof(buf), "%d", n);
    return buf;
}

// =============================================================================
//  HUD
// =============================================================================
void draw(const Game& g, const Palette& pal) {
    switch (g.phase()) {
        case Phase::Title:   drawTitle(pal);      return;
        case Phase::Credits: drawCredits(g, pal); return;
        default: break;
    }

    const Color txt   = toColor(pal.uiText);
    const Color dim   = toColor(pal.uiDim);
    const Color good  = toColor(pal.uiGood);
    const Color warn  = toColor(pal.uiWarn);
    const Color bad   = toColor(pal.uiBad);
    const Color empty = Color{ 40, 38, 44, 200 };
    const Color border = Color{ 20, 18, 22, 220 };

    // Top bar.
    DrawRectangle(0, 0, VIRTUAL_W, 14, Color{ 12, 10, 16, 150 });
    char buf[80];
    std::snprintf(buf, sizeof(buf), "INCARNATION %s", roman(g.incarnation()));
    label(buf, 6, 3, txt);
    std::snprintf(buf, sizeof(buf), "AGE %d", g.sim().ageYears());
    labelRight(buf, VIRTUAL_W - 6, 3, txt);

    // Bottom bar: only while a life is actually underway. The narrative
    // phases get a cleaner, more cinematic frame.
    if (g.phase() == Phase::Climb) {
        DrawRectangle(0, VIRTUAL_H - 30, VIRTUAL_W, 30, Color{ 12, 10, 16, 150 });
        const SimState& s = g.sim().state();

        label("STAMINA", 4, VIRTUAL_H - 27, dim);
        meter(70, VIRTUAL_H - 26, 62, 6, s.stamina, s.stamina < 0.25f ? bad : good, empty, border);

        label("GRIP", 4, VIRTUAL_H - 15, dim);
        meter(70, VIRTUAL_H - 14, 62, 6, s.grip,
              s.slipping ? bad : (s.grip < 0.3f ? warn : good), empty, border);
        if (s.slipping) label("SLIPPING!", 138, VIRTUAL_H - 15, bad);

        const int pbW = 96;
        const int pbX = VIRTUAL_W - pbW - 6;
        label("PROGRESS", pbX, VIRTUAL_H - 27, dim);
        meter(pbX, VIRTUAL_H - 15, pbW, 6, g.ballProgress(), warn, empty, border);
    }

    // Teach the controls on the first climb, above the meter bar so nothing
    // has to share space.
    // Teach the controls early in the first climb, tucked under the status bar
    // where the ball cannot reach.
    if (g.incarnation() <= 1 && g.phase() == Phase::Climb && g.ballProgress() < 0.5f) {
        DrawRectangle(0, 14, VIRTUAL_W, 16, Color{ 12, 10, 16, 190 });
        labelCenter("SPACE push      SHIFT brace", 17, txt);
    }

    if (g.phase() == Phase::Choice)   drawChoice(pal);
    if (g.phase() == Phase::WalkAway) drawWalkAwayText(g, pal);
}

} // namespace cashyphus::hud
