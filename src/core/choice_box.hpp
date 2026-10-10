#pragma once

// =============================================================================
//  The choice screen's message box
//
//  The last decision in the game -- one more push, or walk away -- used to be
//  four lines of text with a legend under each. It was readable and completely
//  unclickable: the two answers are the only things in the game worth pointing
//  at, and a left click anywhere (including on the words WALK AWAY) meant "keep
//  pushing". So the answers are buttons now.
//
//  Geometry only, and raylib-free, so the renderer, the input path and the tests
//  all read the same rectangle for a button -- a box drawn in one place and
//  hit-tested in another is a box that eventually disagrees with itself.
// =============================================================================

#include "core/config.hpp"

namespace cashyphus::core {

struct Rect { int x = 0, y = 0, w = 0, h = 0; };

// The two answers, in the order they are drawn: 0 on top (keep pushing), 1 below
// it (walk away).
enum ChoiceOption { CHOICE_NONE = -1, CHOICE_PUSH = 0, CHOICE_WALK = 1, CHOICE_COUNT = 2 };

// What each button says, and how to answer it. The legends name the keys and the
// gestures because the box has to serve a desktop (a click, a right-click) and a
// phone (a tap, a press-and-hold that the page turns into SHIFT) with the same
// two words.
extern const char* const choiceLabel[CHOICE_COUNT];    // "KEEP PUSHING", "WALK AWAY"
extern const char* const choiceLegend[CHOICE_COUNT];

// The baked face advances 8px per character (render/font.hpp CELL_W, which
// hud.cpp static_asserts against this). Core cannot include that header -- it
// pulls in raylib -- so the one number the box needs from it is restated here.
inline constexpr int GLYPH_W = 8;

struct ChoiceBox {
    Rect box;                    // the message panel, in frame pixels
    int  lineY = 0;              // the ball's question, inside the top of the box
    int  ruleY = 0;              // the divider under it
    Rect button[CHOICE_COUNT];
};

// Where the box and its buttons go in a frameW x frameH frame.
ChoiceBox choiceBoxFor(int frameW, int frameH);

// Which button holds (x, y) in frame pixels, or CHOICE_NONE. Half-open rects --
// the right and bottom edges belong to the next pixel -- and the gaps inside the
// box answer nothing at all, so a click that misses cannot decide anything.
int choiceBoxHit(const ChoiceBox& b, int x, int y);

} // namespace cashyphus::core
