#pragma once

#include "core/game_state.hpp"
#include "core/palette.hpp"

namespace cashyphus::hud {

// Draw the in-game meters, counters and any phase-specific overlay
// (title screen, the final choice, the two endings, the credits).
void draw(const Game& g, const Palette& pal);

// "I", "II", ... "XX", then plain digits after that.
const char* roman(int n);

} // namespace cashyphus::hud
