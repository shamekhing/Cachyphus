#pragma once
#include <array>
#include "audio/voices.hpp"
#include "raylib.h"

namespace cashyphus::audio {
class Synth {
public:
    enum class Effect { Push1,Push2,Push3,Push4,Strong,BraceStart,BraceLoop,BraceEnd,Roll,Slip,Stamina,Grip,Aging,Summit,Death,Downhill,Reincarnation,Dialogue,Choice,WalkAway,Count };
    enum class Track { Base,Young,Adult,Old,Final,Endless,Mechanical,Freedom,Count };
    void init();
    void shutdown();
    bool ready() const { return ready_; }
    void update(float dt);
    void play(Effect e, float pitch=1.0f, float volume=1.0f);
    void loop(Effect e, float target, float dt);
    void select(Track t);
    void setVolumes(float master,float music,float effects,float dialogue,float ambience);
private:
    // A short effect is a small pool of voices over one decoded sample. A player
    // can land two pushes inside one coin tail, and a single Sound would cut the
    // first clink off mid-ring; aliases share the sample data, so three voices
    // cost three bookkeeping slots rather than three copies of the audio. Loops
    // stay single-stream: they are retriggered by gain, never by PlaySound.
    static constexpr int kVoices = 3;
    struct Clip {
        Sound sound[kVoices]{};
        int   voices  = 0;      // pool slots actually built, 0 when nothing loaded
        int   next    = 0;      // round-robin cursor
        Music stream{};
        bool  loaded  = false;
        bool  streaming = false;
        float gain = 0;
        voices::Voice standin = voices::Voice::Count;   // used when the WAV is absent
    };
    struct Song { Music music{}; bool loaded=false; };

    // The game still runs with no assets at all: an effect whose recording is
    // missing or fails to decode falls back to one of the nine procedural voices
    // in voices.hpp, synthesised once on demand. That header is pure functions
    // with no raylib in it, so the test suite measures this path directly.
    static voices::Voice standinFor(Effect e);
    void buildStandin(int index, Clip& c);
    void unloadClip(Clip& c);

    std::array<Clip,static_cast<int>(Effect::Count)> clips_{};
    std::array<Song,static_cast<int>(Track::Count)> songs_{};
    Track current_=Track::Base;
    Track wanted_=Track::Base;
    bool ready_=false;
    float master_=1,music_=0.35f,effects_=0.65f,dialogue_=0.45f,ambience_=0.35f;
    float songGain_=0;
    float musicActual_=0;
};
}
