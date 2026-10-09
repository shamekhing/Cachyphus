#pragma once

#include "audio/climb.hpp"
#include "core/music.hpp"
#include "raylib.h"

namespace cashyphus::audio {

// Every sound in the game is synthesised at runtime -- there are no audio
// files. If the machine has no audio device the whole class becomes a no-op so
// the game still runs silently.
class Synth {
public:
    void init();
    void shutdown();

    bool ready() const { return ready_; }

    // One-shot effects.
    void push();       // heavy thump
    void scrape();     // bracing footstep
    void rattle();     // coins / the ball rolling
    void breath();     // heavy breathing, used when stamina is low
    void jingle();     // summit celebration
    void bell();       // reincarnation
    void collapse();   // the character dies
    void rollDown();   // the ball heading home
    void birds();      // only heard once you walk away

    // Looping music. The climb theme is a real CC0 track baked into the binary
    // and worn down per incarnation; `warm` switches instead to the gentle
    // escape motif heard only after walking away.
    void musicStart();
    void musicStop();
    void musicSetMood(bool warm);
    void musicSetClimb(const core::Arrangement& a);
    void musicVolume(float v);   // 0..1

private:
    bool ready_ = false;
    Sound sPush_{};
    Sound sScrape_{};
    Sound sRattle_{};
    Sound sBreath_{};
    Sound sJingle_{};
    Sound sBell_{};
    Sound sCollapse_{};
    Sound sRoll_{};
    Sound sBirds_{};
    AudioStream music_{};
};

} // namespace cashyphus::audio
