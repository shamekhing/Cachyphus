#include "audio/synth.hpp"

#include <algorithm>
#include <cmath>

#include "audio/voices.hpp"

namespace cashyphus::audio {

namespace {

// The effects themselves live in audio/voices.hpp: raylib-free, so they can be
// rendered and measured by the headless tests. This file only turns them into
// raylib Sounds and mixes the music on top.
using voices::Voice;

Sound make(Voice id) {
    const int n = static_cast<int>(voices::seconds(id) * voices::RATE);
    Wave w{};
    w.frameCount = n;
    w.sampleRate = voices::RATE;
    w.sampleSize = 16;
    w.channels   = 1;
    short* data = static_cast<short*>(MemAlloc(static_cast<unsigned int>(n) * sizeof(short)));
    for (int i = 0; i < n; ++i) {
        // Soft clip, not hard: the mixer adds every voice together with no
        // limiter of its own, so a peak that reaches full scale would otherwise
        // be squared off into the crackle that reads as static.
        const float v = std::tanh(voices::sample(id, i));
        data[i] = static_cast<short>(v * 30000.0f);
    }
    w.data = data;
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}

// -----------------------------------------------------------------------------
//  Looping music: a tiny step sequencer rendered by an audio-stream callback.
// -----------------------------------------------------------------------------
struct MusicState {
    int    step = 0;
    double counter = 0.0;      // samples into the current step
    double stepSamples = 0.0;
    float  gain = 0.30f;
    bool   warm = false;       // false = climb theme, true = escape motif
    bool   playing = false;
};

MusicState g_music;

// The climb theme, and whether it is in the mix at all. It is silenced
// outright rather than faded whenever the escape motif takes over: the brief
// is emphatic that the sudden absence of the pushing music is the point.
ClimbTheme g_climb;
bool       g_climbOn = true;

// Headroom for the baked track. It is normalised to 0.86, so staying below
// unity leaves room for the effects on top of it.
constexpr float kClimbLevel = 0.70f;

// Escape motif, in semitones relative to A4. The climb theme used to be an
// eight-step sequencer in this file as well; it is now a real composed track
// (see assets/CREDITS.md), so only the ending is still synthesised here.
const int kEscapeSteps[8] = { 3, 7, 10, 15, 10, 7, 5, 3 };

void musicCallback(void* buffer, unsigned int frames) {
    float* out = static_cast<float*>(buffer);
    if (!g_music.playing) {
        for (unsigned int i = 0; i < frames; ++i) out[i] = 0.0f;
        return;
    }
    if (g_music.stepSamples <= 0.0) g_music.stepSamples = voices::RATE * 0.28;

    for (unsigned int i = 0; i < frames; ++i) {
        if (g_music.counter >= g_music.stepSamples) {
            g_music.counter -= g_music.stepSamples;
            g_music.step = (g_music.step + 1) % 8;
        }
        const float t   = static_cast<float>(g_music.counter / g_music.stepSamples); // 0..1
        const float sec = static_cast<float>(g_music.counter / voices::RATE);        // seconds

        float sample = 0.0f;
        if (g_music.warm) {
            // The escape motif: a plain, warm resolution, heard only once the
            // player has actually walked away.
            const float f  = voices::noteFreq(kEscapeSteps[g_music.step]);
            const float eg = voices::env(t, 1.0f, 0.02f, 0.35f);
            // Both notes share that envelope. The bass used to be unenveloped,
            // which meant its phase jumped back to zero at every step boundary
            // -- a click every 0.28s, on a loop.
            sample += 0.55f * voices::tri(f * sec) * eg;
            if (g_music.step % 4 == 0) {
                sample += 0.50f * voices::sine(voices::noteFreq(kEscapeSteps[g_music.step] - 24) * sec) * eg;
            }
        } else if (g_climbOn) {
            // The baked climb theme, wearing down as incarnations pile up.
            sample += g_climb.next() * kClimbLevel;
        }
        sample *= g_music.gain;

        out[i] = std::clamp(sample, -1.0f, 1.0f);
        g_music.counter += 1.0;
    }
}

} // namespace

// =============================================================================
//  Synth
// =============================================================================
void Synth::init() {
    if (!IsAudioDeviceReady()) {
        ready_ = false;
        TraceLog(LOG_WARNING, "CASHYPHUS: no audio device, running silently");
        return;
    }
    ready_ = true;

    sPush_     = make(Voice::Push);
    sScrape_   = make(Voice::Scrape);
    sRattle_   = make(Voice::Rattle);
    sBreath_   = make(Voice::Breath);
    sJingle_   = make(Voice::Jingle);
    sBell_     = make(Voice::Bell);
    sCollapse_ = make(Voice::Collapse);
    sRoll_     = make(Voice::Roll);
    sBirds_    = make(Voice::Birds);

    music_ = LoadAudioStream(voices::RATE, 32, 1);
    SetAudioStreamCallback(music_, musicCallback);
    g_music.playing = true;
    PlayAudioStream(music_);
    SetAudioStreamVolume(music_, 0.0f);
}

void Synth::shutdown() {
    if (!ready_) return;
    musicStop();
    UnloadSound(sPush_);
    UnloadSound(sScrape_);
    UnloadSound(sRattle_);
    UnloadSound(sBreath_);
    UnloadSound(sJingle_);
    UnloadSound(sBell_);
    UnloadSound(sCollapse_);
    UnloadSound(sRoll_);
    UnloadSound(sBirds_);
    UnloadAudioStream(music_);
    ready_ = false;
}

void Synth::push()     { if (ready_) PlaySound(sPush_); }
void Synth::scrape()   { if (ready_) PlaySound(sScrape_); }
void Synth::rattle()   { if (ready_) PlaySound(sRattle_); }
void Synth::breath()   { if (ready_) PlaySound(sBreath_); }
void Synth::jingle()   { if (ready_) PlaySound(sJingle_); }
void Synth::bell()     { if (ready_) PlaySound(sBell_); }
void Synth::collapse() { if (ready_) PlaySound(sCollapse_); }
void Synth::rollDown() { if (ready_) PlaySound(sRoll_); }
void Synth::birds()    { if (ready_) PlaySound(sBirds_); }

void Synth::musicStart() {
    if (!ready_) return;
    g_music.playing = true;
    if (!IsAudioStreamPlaying(music_)) PlayAudioStream(music_);
}

void Synth::musicStop() {
    g_music.playing = false;
    if (ready_ && IsAudioStreamPlaying(music_)) StopAudioStream(music_);
}

void Synth::musicSetMood(bool warm) {
    // Handing over to the escape motif is the "sudden silence" the brief asks
    // for, so the climb theme is switched out rather than faded down.
    if (warm && !g_music.warm) g_climbOn = false;
    if (!warm && g_music.warm) {
        g_climbOn = true;
        g_climb.reset();
    }
    g_music.warm = warm;
}

void Synth::musicSetClimb(const core::Arrangement& a) { g_climb.setArrangement(a); }

void Synth::musicVolume(float v) {
    if (!ready_) return;
    SetAudioStreamVolume(music_, std::clamp(v, 0.0f, 1.0f));
}

} // namespace cashyphus::audio
