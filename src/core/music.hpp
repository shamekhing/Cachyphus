#pragma once

// =============================================================================
//  How worn down the climb theme is.
//
//  The brief: the music "should initially feel motivational, almost comically
//  cheerful. Over successive incarnations, the arrangement becomes thinner and
//  more mechanical."
//
//  So every field below moves in exactly one direction as lives pile up, and
//  the whole curve lives here -- raylib-free, in core -- so the tests can pin
//  it down without an audio device.
// =============================================================================

namespace cashyphus::core {

struct Arrangement {
    float tone   = 1.0f;   // 1 = full brightness, 0 = muffled. Low-pass amount.
    int   bits   = 16;     // quantisation; fewer bits reads as grittier
    int   hold   = 1;      // sample-and-hold factor; >1 lowers the effective rate
    float gain   = 1.0f;   // output level
    float detune = 0.0f;   // playback-rate error in cents (negative = dragging)
};

// The curve reaches its floor here and stays there.
inline constexpr int ARRANGEMENT_SPENT_LIVES = 12;

Arrangement arrangementFor(int lives);

} // namespace cashyphus::core
