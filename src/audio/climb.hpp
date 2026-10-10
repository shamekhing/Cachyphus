#pragma once

#include "core/music.hpp"

namespace cashyphus::audio {

// Plays the baked climb theme, worn down according to an Arrangement.
//
// The theme is a real CC0 chiptune rather than a runtime synth: the brief asks
// for "one repeating track with variations", and the variations are what this
// applies. Three stages, in the order the ear notices them:
//
//   1. a one-pole low-pass, which takes the sheen off first;
//   2. sample-and-hold, which lowers the effective sample rate;
//   3. quantisation, which is the "more mechanical" half of the brief.
//
// Then a small playback-rate error, so it drags slightly as it wears out.
class ClimbTheme {
public:
    void  reset();                        // back to the top of the loop
    void  setArrangement(const core::Arrangement& a);
    float next();                         // one degraded sample, -1..1

private:
    double            phase_  = 0.0;      // fractional index into the loop
    float             lp_     = 0.0f;     // low-pass state
    float             held_   = 0.0f;     // sample currently being held
    int               holdIx_ = 0;
    core::Arrangement arr_{};
};

} // namespace cashyphus::audio
