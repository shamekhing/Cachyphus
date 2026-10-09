#include "core/music.hpp"

#include "core/config.hpp"

namespace cashyphus::core {

// The theme does not decay forever -- by the time the player has spent this
// many lifetimes it has become a thin, dragging, crushed version of itself and
// stays that way. Degrading past that point would stop reading as "worn out"
// and start reading as "broken".
Arrangement arrangementFor(int lives) {
    if (lives < 0) lives = 0;
    const float t = cfg::clampf(static_cast<float>(lives) /
                                    static_cast<float>(ARRANGEMENT_SPENT_LIVES),
                                0.0f, 1.0f);

    Arrangement a;
    // Brightness goes first: the top end is what makes a chiptune sound eager.
    a.tone   = cfg::lerpf(1.00f, 0.16f, t);
    // Then resolution, which is the "mechanical" half of the brief.
    a.bits   = static_cast<int>(cfg::lerpf(16.0f, 5.0f, t) + 0.5f);
    a.hold   = static_cast<int>(cfg::lerpf(1.0f, 4.0f, t) + 0.5f);
    a.gain   = cfg::lerpf(1.00f, 0.82f, t);
    // A little flat, as if the machine driving it is tiring too.
    a.detune = cfg::lerpf(0.0f, -35.0f, t);
    return a;
}

} // namespace cashyphus::core
