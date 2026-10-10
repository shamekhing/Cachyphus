#include "core/choice_box.hpp"

#include <algorithm>

namespace cashyphus::core {

const char* const choiceLabel[CHOICE_COUNT]  = { "KEEP PUSHING", "WALK AWAY" };
const char* const choiceLegend[CHOICE_COUNT] = { "CLICK / SPACE", "HOLD / SHIFT" };

namespace {

constexpr int PAD      = 6;                       // box padding, and button inset
constexpr int GAP      = 6;                       // between the parts
constexpr int LINE_H   = 12;                      // one line of the baked face
constexpr int BUTTON_H = 2 * LINE_H + 4;          // label + legend, a little air
constexpr int MIN_W    = 150;                     // fit for a 180px frame
constexpr int MAX_W    = 200;                     // wider than this reads as a bar

// The meters own the top of the frame: the bars end at 13, the pips at 19 and
// the age numeral at 26. Nothing of the box may sit above this.
constexpr int TOP_LIMIT = 28;

} // namespace

ChoiceBox choiceBoxFor(int frameW, int frameH) {
    ChoiceBox b;
    if (frameW < 1 || frameH < 1) return b;

    b.box.w = std::clamp(frameW - 20, MIN_W, MAX_W);
    b.box.h = PAD + LINE_H + GAP + 1 + GAP + BUTTON_H + GAP + BUTTON_H + PAD;
    b.box.x = (frameW - b.box.w) / 2;
    b.box.y = std::max(TOP_LIMIT, (frameH - b.box.h) / 2);
    // A frame can be shorter than the box would like (a phone with the URL bar
    // up is 295 tall, not 360). Slide it up rather than let it hang off the
    // bottom, and never slide it under the meters.
    if (b.box.y + b.box.h > frameH - 2) b.box.y = frameH - 2 - b.box.h;
    if (b.box.y < TOP_LIMIT) b.box.y = TOP_LIMIT;

    b.lineY = b.box.y + PAD;
    b.ruleY = b.lineY + LINE_H + GAP;

    const int bx = b.box.x + PAD;
    const int bw = b.box.w - 2 * PAD;
    b.button[CHOICE_PUSH] = Rect{ bx, b.ruleY + 1 + GAP, bw, BUTTON_H };
    b.button[CHOICE_WALK] = Rect{ bx, b.button[CHOICE_PUSH].y + BUTTON_H + GAP, bw, BUTTON_H };
    return b;
}

int choiceBoxHit(const ChoiceBox& b, int x, int y) {
    for (int i = 0; i < CHOICE_COUNT; ++i) {
        const Rect& r = b.button[i];
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return i;
    }
    return CHOICE_NONE;
}

} // namespace cashyphus::core
