#pragma once

#include "core/game_state.hpp"
#include "core/palette.hpp"

namespace cashyphus::hud {

// Draw the in-game meters, counters and any phase-specific overlay
// (title screen, the final choice, the two endings, the credits).
void draw(const Game& g, const Palette& pal);

// "I", "II", ... "XX", then plain digits after that.
const char* roman(int n);

// The mouse, in frame pixels. The frame is blitted into the middle of the
// display at a whole-number scale, and this undoes exactly that -- so the choice
// screen's buttons are hovered by the renderer and clicked by main.cpp through
// one conversion rather than two that could disagree.
struct Pointer { int x = 0, y = 0; };
Pointer pointerInFrame();

} // namespace cashyphus::hud
