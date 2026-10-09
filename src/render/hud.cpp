#include "render/hud.hpp"

#include <cstdio>

#include "core/config.hpp"
#include "render/font.hpp"
#include "render/pixelart.hpp"
#include "render/scene.hpp"

namespace cashyphus::hud {

using namespace cfg;
namespace txt = art::text;
using art::toColor;

// =============================================================================
//  Small widgets
//
//  The brief prints the interface as block meters:
//      STAMINA  ███████░░░
//      GRIP     ████░░░░░░
//  so the bars are drawn as discrete cells rather than a smooth fill. The gaps
//  are the background showing through, which is what makes them read as retro
//  rather than as a modern progress bar.
// =============================================================================
namespace {

constexpr int METER_CELLS  = 10;
constexpr int METER_CELL_W = 5;
constexpr int METER_GAP    = 1;
constexpr int METER_H      = 6;
constexpr int METER_W = METER_CELLS * METER_CELL_W + (METER_CELLS - 1) * METER_GAP;
constexpr int LABEL_X = 5;
constexpr int METER_X = 62;

// Rows of the status bar sit 11px apart: the face has a 12px line box with 7px
// of ink centred in it.
constexpr int ROW_H = 11;

void meter(int x, int y, float frac, Color fill, Color empty) {
    frac = clampf(frac, 0.0f, 1.0f);
    const int lit = static_cast<int>(METER_CELLS * frac + 0.5f);
    for (int i = 0; i < METER_CELLS; ++i) {
        DrawRectangle(x + i * (METER_CELL_W + METER_GAP), y, METER_CELL_W, METER_H,
                      i < lit ? fill : empty);
    }
}

void panel(int x, int y, int w, int h, const Palette& pal) {
    DrawRectangle(x, y, w, h, toColor(pal.bubbleBg, 220));
    DrawRectangleLines(x, y, w, h, toColor(pal.bubbleEdge));
}

// -----------------------------------------------------------------------------
//  Hill progress indicator
//
//  The brief marks this optional and sketches it as a slope with the summit at
//  the top and the ball somewhere on the way up. A bar tells the player a
//  number; this tells them how much hill is left.
// -----------------------------------------------------------------------------
void hillGauge(int x, int y, int w, int h, float progress, const Palette& pal) {
    // Drawn on the dark status bar, so it uses the UI ramp rather than the
    // terrain ramp: hillEdge and path go almost black near the summit, which
    // is exactly where the player most needs to read this.
    const Color slope = toColor(pal.uiDim);
    const Color peak  = toColor(pal.uiText);
    const Color ball  = toColor(pal.ballBand);
    const Color rim   = toColor(pal.ballDark);

    const int run = w - 8;   // leave room for the summit marker
    for (int i = 0; i < run; ++i) {
        const int py = y + h - (i * h) / run;
        DrawPixel(x + i, py, slope);
        DrawPixel(x + i, py + 1, slope);
    }

    // Summit peak. Drawn as integer scanlines rather than a GPU triangle so
    // every edge lands on a whole pixel, and so it cannot be lost to render
    // state the way a raw primitive can.
    const int sx   = x + run;
    const int half = 6;
    for (int r = 0; r < h; ++r) {
        const int hw = (half * r) / (h - 1);
        DrawRectangle(sx - hw, y + r, hw * 2 + 1, 1, peak);
    }

    // The ball, at its height on the hill.
    const float t  = clampf(progress, 0.0f, 1.0f);
    const int   travel = static_cast<int>(run * t);
    const int   bx = x + travel;
    const int   by = y + h - (travel * h) / run;
    DrawCircle(bx, by - 3, 4, rim);
    DrawCircle(bx, by - 3, 3, ball);
}

// --- overlays ---------------------------------------------------------------

void drawTitle(const Palette& pal) {
    DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, Color{ 12, 10, 16, 170 });
    const Color gold = toColor(pal.ballBand);
    const Color txt  = toColor(pal.uiText);
    const Color dim  = toColor(pal.uiDim);

    txt::drawCenter("CASHYPHUS", 30, txt::SIZE_LARGE, gold);
    txt::drawCenter("One hill. One lifetime. One more push.", 72, txt::SIZE_SMALL, txt);
    txt::drawCenter("an allegory about pushing money uphill", 86, txt::SIZE_SMALL, dim);

    if (static_cast<int>(GetTime() * 2.0f) % 2 == 0) {
        txt::drawCenter("PRESS SPACE TO BEGIN", 118, txt::SIZE_SMALL, txt);
    }
    txt::drawCenter("SPACE: PUSH        SHIFT: BRACE", 144, txt::SIZE_SMALL, dim);
    txt::drawCenter("the hill is patient.", 158, txt::SIZE_SMALL, dim);
}

void drawChoice(const Palette& pal) {
    // Sits in the empty sky above the ball and the waiting character, so it
    // hides neither of them.
    const int w = 232, h = 72;
    const int x = (VIRTUAL_W - w) / 2;
    const int y = 18;
    panel(x, y, w, h, pal);

    const Color txt = toColor(pal.uiText);
    txt::draw("Well, shall we?", x + 12, y + 5, txt::SIZE_SMALL, toColor(pal.ballBand));
    DrawRectangle(x + 10, y + 21, w - 20, 1, toColor(pal.uiDim));

    txt::draw("KEEP PUSHING", x + 12, y + 26, txt::SIZE_SMALL, toColor(pal.uiWarn));
    txt::drawRight("[SPACE]", x + w - 12, y + 26, txt::SIZE_SMALL, txt);

    txt::draw("WALK AWAY", x + 12, y + 48, txt::SIZE_SMALL, toColor(pal.uiGood));
    txt::drawRight("[SHIFT]", x + w - 12, y + 48, txt::SIZE_SMALL, txt);
}

void drawWalkAwayText(const Game& g, const Palette& pal) {
    if (g.walkT() > 0.55f && g.walkT() < 0.96f) {
        // Split over two lines: the sentence is 45 characters, wider than the
        // screen at 8px a glyph.
        DrawRectangle(0, 62, VIRTUAL_W, 30, Color{ 12, 10, 16, 140 });
        const Color ink = toColor(pal.uiText);
        txt::drawCenter("You cannot buy back the time", 66, txt::SIZE_SMALL, ink);
        txt::drawCenter("you never spent.", 78, txt::SIZE_SMALL, ink);
    }
}

void drawCredits(const Game& g, const Palette& pal) {
    DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, Color{ 8, 8, 10, 210 });
    const Color txt = toColor(pal.uiText);
    const Color dim = toColor(pal.uiDim);

    txt::drawCenter("You can leave the hill.", 44, txt::SIZE_SMALL, txt);
    txt::drawCenter("But the hill will always find someone", 60, txt::SIZE_SMALL, txt);
    txt::drawCenter("willing to climb.", 76, txt::SIZE_SMALL, txt);

    char buf[96];
    std::snprintf(buf, sizeof(buf), "incarnations spent pushing: %d", g.completedLives());
    txt::drawCenter(buf, 102, txt::SIZE_SMALL, dim);
    std::snprintf(buf, sizeof(buf), "times you chose to continue: %d", g.timesContinued());
    txt::drawCenter(buf, 116, txt::SIZE_SMALL, dim);

    if (static_cast<int>(GetTime() * 2.0f) % 2 == 0) {
        txt::drawCenter("PRESS SPACE", 146, txt::SIZE_SMALL, toColor(pal.ballBand));
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

    // Top bar: incarnation on the left, age on the right, as the brief lays it
    // out.
    DrawRectangle(0, 0, VIRTUAL_W, 16, Color{ 12, 10, 16, 150 });
    char buf[80];
    std::snprintf(buf, sizeof(buf), "INCARNATION: %s", roman(g.incarnation()));
    txt::draw(buf, LABEL_X, 2, txt::SIZE_SMALL, txt);
    std::snprintf(buf, sizeof(buf), "AGE: %d", g.sim().ageYears());
    txt::drawRight(buf, VIRTUAL_W - LABEL_X, 2, txt::SIZE_SMALL, txt);

    // Bottom bar: only while a life is actually underway. The narrative phases
    // get a cleaner, more cinematic frame.
    if (g.phase() == Phase::Climb) {
        const int barH = ROW_H * 3 + 1;
        const int barY = VIRTUAL_H - barH;
        DrawRectangle(0, barY, VIRTUAL_W, barH, Color{ 12, 10, 16, 150 });
        const SimState& s = g.sim().state();

        // Stamina. It turns red on the way down so an emptying tank is felt as
        // well as read.
        txt::draw("STAMINA", LABEL_X, barY + 2, txt::SIZE_SMALL, dim);
        meter(METER_X, barY + 4, s.stamina, s.stamina < 0.25f ? bad : good, empty);

        // Grip, plus the slip warning.
        txt::draw("GRIP", LABEL_X, barY + 2 + ROW_H, txt::SIZE_SMALL, dim);
        meter(METER_X, barY + 4 + ROW_H, s.grip,
              s.slipping ? bad : (s.grip < 0.3f ? warn : good), empty);
        if (s.slipping) {
            txt::draw("SLIPPING!", METER_X + METER_W + 6, barY + 2 + ROW_H, txt::SIZE_SMALL, bad);
        }

        // The two controls, exactly as the brief writes them.
        txt::draw("SPACE: PUSH", LABEL_X, barY + 2 + ROW_H * 2, txt::SIZE_SMALL, txt);
        txt::drawRight("SHIFT: BRACE", VIRTUAL_W - LABEL_X, barY + 2 + ROW_H * 2,
                       txt::SIZE_SMALL, txt);

        // Hill progress: how much hill is left, rather than just a number.
        hillGauge(VIRTUAL_W - 96, barY + 5, 88, 13, g.ballProgress(), pal);
    }

    if (g.phase() == Phase::Choice)   drawChoice(pal);
    if (g.phase() == Phase::WalkAway) drawWalkAwayText(g, pal);
}

} // namespace cashyphus::hud
