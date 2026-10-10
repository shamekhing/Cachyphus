#pragma once

#include "core/game_state.hpp"
#include "core/palette.hpp"
#include "raylib.h"
#include "render/sprites.hpp"

namespace cashyphus::scene {

// Geometry of the one and only hill.
//
// In portrait the hill is a static diagonal across the frame, rising from the
// bottom-left corner towards the top-right. The ball rides it from progress 0
// to 1, so the climb visibly travels UP the screen rather than across it, and
// the summit lands where the brief's own sketch puts it: at the top.
struct World {
    // HILL_LEN and BALL_R come from the core config so the simulation can
    // derive the ball's true roll from distance travelled.
    static constexpr float HILL_LEN = cfg::HILL_LEN;
    static constexpr float BALL_R   = cfg::BALL_RADIUS;

    static constexpr float GROUND_BASE = 352.0f;   // ground y at screen x = 0
    static constexpr float GROUND_RISE = 272.0f;   // how far the ground climbs
    static constexpr float GROUND_CURVE = 1.15f;   // >1 = steepens to the right

    static constexpr float BALL_X0 = 26.0f;        // ball x at progress 0
    static constexpr float BALL_X1 = 166.0f;       // ball x at progress 1

    static float groundY(float screenX);                  // ground height at screen x
    static float ballX(float progress);                   // progress -> ball centre x
    static float ballY(float progress);                   // progress -> ball centre y
    static float contactY(float screenX);                 // ground under a given x
};

// Vertical parallax offset for the current frame: how far the background has
// sunk because of how high the ball has climbed.
float climbOffset(const Game& g);

// Draw the entire world: sky, parallax, hill, character, ball, speech bubble.
void draw(const Game& g, const Palette& pal, const art::SpriteBank& sprites);

// Draw a small speech bubble whose tail sits near (cx, cy). `reveal` is the
// number of characters to show (typewriter effect).
void speechBubble(const char* text, int cx, int cy, const Palette& pal, int reveal);

// Convenience wrappers around the default font.
void text(const char* s, int x, int y, Color c);
void textCentered(const char* s, int y, Color c);
int  textWidth(const char* s);

} // namespace cashyphus::scene
