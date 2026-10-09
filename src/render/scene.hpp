#pragma once

#include "core/game_state.hpp"
#include "core/palette.hpp"
#include "raylib.h"
#include "render/sprites.hpp"

namespace cashyphus::scene {

// Geometry of the one and only hill, in world pixels.
struct World {
    // The hill is sized so the ball and the character always sit between the
    // top status bar (14 px) and the bottom meter bar (30 px).
    // HILL_LEN and BALL_R come from the core config so the simulation can
    // derive the ball's true roll from distance travelled.
    static constexpr float HILL_LEN = cfg::HILL_LEN;
    static constexpr float BASE_Y   = 148.0f;   // ground level at the bottom
    static constexpr float RISE     = 82.0f;    // how high the summit sits
    static constexpr float BALL_R   = cfg::BALL_RADIUS;

    static float hillY(float x);                       // ground height at world x
    static float ballWorldX(float progress);           // progress -> world x
};

// Camera scroll for the current frame (world x of the left screen edge).
float cameraX(const Game& g);

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
