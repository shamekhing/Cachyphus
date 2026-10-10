#include "render/hud.hpp"

#include <algorithm>
#include <cmath>
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
//  The HUD says almost nothing in words.
//
//  The brief's interface sketch shows block meters, and words were what filled
//  the screen. So the interface is two thin bars and a row of pips. Age is the
//  only numeral, and only because the brief singles it out; incarnation is the
//  pips, not a number.
// =============================================================================
namespace {

// Two full-width bars stacked at the very top. At 4px tall and segmented, they
// are readable at phone size and leave the whole frame to the scene.
constexpr int BAR_X  = 5;
int barWidth() { return VIRTUAL_W - 2 * BAR_X; }
constexpr int BAR_H  = 4;
constexpr int CELLS  = 14;
constexpr int CELLY0 = 3;
constexpr int CELLY1 = 9;

// Backing and empty-cell colour for the bars.
const Color kEmpty = Color{ 44, 42, 48, 215 };

void bar(int y, float frac, Color fill) {
    frac = clampf(frac, 0.0f, 1.0f);
    const int lit = static_cast<int>(CELLS * frac + 0.5f);
    const int cw  = (barWidth() - (CELLS - 1)) / CELLS;
    for (int i = 0; i < CELLS; ++i) {
        const int x = BAR_X + i * (cw + 1);
        DrawRectangle(x, y, cw, BAR_H, i < lit ? fill : kEmpty);
    }
}

// Incarnation as a row of pips: filled for lives spent, hollow for the lives
// up to the first real choice. Reads at a glance and takes no words.
void pips(int lives, const Palette& pal) {
    const int   total = CHOICE_AFTER_LIVES;
    const Color on    = toColor(pal.ballBand);
    const Color off   = toColor(pal.uiDim, 140);
    for (int i = 0; i < total; ++i) {
        const int cx = 9 + i * 8;
        const int cy = 17;
        const bool  filled = i < lives;
        if (filled) {
            DrawCircle(cx, cy, 2, on);
        } else {
            DrawCircleLines(cx, cy, 2, off);
        }
    }
}

void panel(int x, int y, int w, int h, const Palette& pal) {
    DrawRectangle(x, y, w, h, toColor(pal.bubbleBg, 222));
    DrawRectangleLines(x, y, w, h, toColor(pal.bubbleEdge));
}

// --- overlays ---------------------------------------------------------------

void drawTitle(const Palette& pal) {
    DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, Color{ 12, 10, 16, 170 });
    const Color gold = toColor(pal.ballBand);
    const Color dim  = toColor(pal.uiDim);
    const Color txt  = toColor(pal.uiText);

    // The overlay is centred in the frame, so it follows half the frame's extra
    // height. Half, not all of it: the whole offset would push the block past the
    // middle, and on a frame shorter than the design it is negative -- which is how
    // the title ended up drawn across the meters on a phone with the URL bar up.
    const int top = frameTop() / 2;
    txt::drawCenter("CASHYPHUS", 58 + top, txt::SIZE_MEDIUM, gold);
    txt::drawCenter("One hill.", 94 + top, txt::SIZE_SMALL, txt);
    txt::drawCenter("One lifetime.", 106 + top, txt::SIZE_SMALL, txt);
    txt::drawCenter("One more push.", 118 + top, txt::SIZE_SMALL, dim);

    if (static_cast<int>(GetTime() * 2.0f) % 2 == 0) {
        txt::drawCenter("PRESS SPACE", 148 + top, txt::SIZE_SMALL, txt);
        txt::drawCenter("OR TAP / CLICK", 160 + top, txt::SIZE_SMALL, txt);
    }
}

void drawChoice(const Palette& pal) {
    const int w = 168, h = 78;
    const int x = (VIRTUAL_W - w) / 2;
    // Never above the status bars, however short the frame is.
    const int y = std::max(26, 46 + frameTop() / 2);
    panel(x, y, w, h, pal);

    const Color txt = toColor(pal.uiText);
    txt::drawCenter("Well, shall we?", y + 6, txt::SIZE_SMALL, toColor(pal.ballBand));
    DrawRectangle(x + 8, y + 20, w - 16, 1, toColor(pal.uiDim));

    // The two endings have to be legible on the device in front of you: a tap
    // and a hold on a phone, a click and a right-click on a desktop. The keys
    // still work (web/shell.html spells them out, and the README lists them),
    // but a phone has no Space key to read.
    txt::drawCenter("KEEP PUSHING", y + 26, txt::SIZE_SMALL, toColor(pal.uiWarn));
    txt::drawCenter("[TAP / CLICK]", y + 38, txt::SIZE_SMALL, txt);

    txt::drawCenter("WALK AWAY", y + 54, txt::SIZE_SMALL, toColor(pal.uiGood));
    txt::drawCenter("[HOLD / R-CLICK]", y + 66, txt::SIZE_SMALL, txt);
}

void drawWalkAwayText(const Game& g, const Palette& pal) {
    if (g.walkT() > 0.45f && g.walkT() < 0.96f) {
        const int top = frameTop() / 2;
        DrawRectangle(0, 146 + top, VIRTUAL_W, 26, Color{ 12, 10, 16, 150 });
        const Color ink = toColor(pal.uiText);
        txt::drawCenter("You cannot buy back", 150 + top, txt::SIZE_SMALL, ink);
        txt::drawCenter("time you never spent.", 161 + top, txt::SIZE_SMALL, ink);
    }
}

void drawCredits(const Game& g, const Palette& pal) {
    DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, Color{ 8, 8, 10, 214 });
    const Color txt = toColor(pal.uiText);
    const Color dim = toColor(pal.uiDim);

    // The frame is 180px and the baked face advances 8px per character, so a
    // line is 22 characters at the most. Anything longer is silently clipped by
    // the render target on BOTH sides -- which is how the ending screen used to
    // lose the first and last few letters of every sentence.
    const int top = frameTop() / 2;
    txt::drawCenter("You can leave.", 84 + top, txt::SIZE_SMALL, txt);
    txt::drawCenter("But the hill will", 96 + top, txt::SIZE_SMALL, txt);
    txt::drawCenter("always find someone", 108 + top, txt::SIZE_SMALL, txt);
    txt::drawCenter("willing to climb.", 120 + top, txt::SIZE_SMALL, txt);

    char buf[64];
    std::snprintf(buf, sizeof(buf), "lives spent: %d", g.completedLives());
    txt::drawCenter(buf, 156 + top, txt::SIZE_SMALL, dim);
    txt::drawCenter("times you kept", 170 + top, txt::SIZE_SMALL, dim);
    std::snprintf(buf, sizeof(buf), "pushing: %d", g.timesContinued());
    txt::drawCenter(buf, 182 + top, txt::SIZE_SMALL, dim);

    if (static_cast<int>(GetTime() * 2.0f) % 2 == 0) {
        txt::drawCenter("PRESS SPACE", 212 + top, txt::SIZE_SMALL, toColor(pal.ballBand));
    }
}

// A one-time, wordless hint at the two controls, planted in the thumb zone
// (bottom, a little right of centre) so it sits under the hand that has to act
// on it. An inner dot pulses -- tap to push -- and an outer ring ripples
// outward -- press and keep pressing to brace. The moment the player has done
// both, Game::tutorialDone() latches and this never draws again.
void drawTutorialRing(const Game& g, const Palette& pal) {
    if (g.phase() != Phase::Climb || g.tutorialDone()) return;

    const float t  = GetTime();
    const int   cx = VIRTUAL_W / 2 + 18;
    const int   cy = VIRTUAL_H - 46;

    // Inner dot: tap. Pulses gently so it reads as a thing to touch.
    const float dot = 2.0f + 1.6f * (0.5f + 0.5f * std::sin(t * 3.4f));
    DrawCircle(cx, cy, dot, toColor(pal.uiText, 150));

    // Outer ring: hold. Ripples outward and fades.
    const float rip = std::fmod(t * 0.7f, 1.0f);
    const float rr  = 9.0f + rip * 12.0f;
    DrawCircleLines(cx, cy, rr,
                    toColor(pal.uiWarn, static_cast<unsigned char>((1.0f - rip) * 200.0f)));
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

    const Color good = toColor(pal.uiGood);
    const Color warn = toColor(pal.uiWarn);
    const Color bad  = toColor(pal.uiBad);

    // Incarnation as pips at the top-left; age as a bare numeral at the
    // top-right. No labels, no counters spelled out in words.
    pips(g.completedLives(), pal);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d", g.sim().ageYears());
    txt::drawRight(buf, VIRTUAL_W - 6, 14, txt::SIZE_SMALL, toColor(pal.uiText));

    // The bars only exist while a life is actually underway; the narrative
    // phases get a clean frame.
    if (g.phase() == Phase::Climb) {
        const SimState& s = g.sim().state();
        bar(CELLY0, s.stamina, s.stamina < 0.25f ? bad : good);
        bar(CELLY1, s.grip, s.slipping ? bad : (s.grip < 0.3f ? warn : good));
        drawTutorialRing(g, pal);
    }

    if (g.phase() == Phase::Choice)   drawChoice(pal);
    if (g.phase() == Phase::WalkAway) drawWalkAwayText(g, pal);
}

} // namespace cashyphus::hud

