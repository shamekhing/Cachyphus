#pragma once

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

    // Looping music. `warm` switches to the major key used by the escape
    // ending; `thinned` drops voices as incarnations pile up.
    void musicStart();
    void musicStop();
    void musicSetMood(bool warm, bool thinned);
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
