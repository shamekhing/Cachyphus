#include "audio/synth.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>

namespace cashyphus::audio {

namespace {

constexpr int RATE = 44100;

// Deterministic noise so builds are reproducible.
struct Noise {
    std::uint32_t s = 0x1234567u;
    float next() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return static_cast<float>(static_cast<std::int32_t>(s)) / 2147483648.0f;
    }
};

float sine(float phase)  { return std::sin(phase * 6.2831853f); }
float square(float phase) { return phase < 0.5f ? 1.0f : -1.0f; }
float tri(float phase)   { return 4.0f * std::fabs(phase - 0.5f) - 1.0f; }

float env(float t, float dur, float attack, float release) {
    if (t < 0.0f) return 0.0f;
    if (t >= dur) return 0.0f;
    const float a = attack  > 0.0f ? std::min(1.0f, t / attack) : 1.0f;
    const float r = release > 0.0f ? std::min(1.0f, (dur - t) / release) : 1.0f;
    return a * r;
}

Sound make( float seconds, const std::function<float(float)>& gen) {
    const int n = static_cast<int>(seconds * RATE);
    Wave w{};
    w.frameCount = n;
    w.sampleRate = RATE;
    w.sampleSize = 16;
    w.channels   = 1;
    short* data = static_cast<short*>(MemAlloc(static_cast<unsigned int>(n) * sizeof(short)));
    for (int i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / RATE;
        const float v = std::clamp(gen(t), -1.0f, 1.0f);
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
    bool   warm = false;       // false = climb (minor), true = escape (major)
    bool   thinned = false;    // voices drop away after many incarnations
    bool   playing = false;
};

MusicState g_music;

float noteFreq(int semitoneFromA4) {
    return 440.0f * std::pow(2.0f, semitoneFromA4 / 12.0f);
}

// Climb motif (A minor pentatonic, 8 steps) and escape motif (C major), in
// semitones relative to A4.
const int kClimbSteps[8]  = { 0, 3, 7, 3, 5, 3, 0, -5 };
const int kEscapeSteps[8] = { 3, 7, 10, 15, 10, 7, 5, 3 };

void musicCallback(void* buffer, unsigned int frames) {
    float* out = static_cast<float*>(buffer);
    if (!g_music.playing) {
        for (unsigned int i = 0; i < frames; ++i) out[i] = 0.0f;
        return;
    }
    if (g_music.stepSamples <= 0.0) g_music.stepSamples = RATE * 0.28;

    for (unsigned int i = 0; i < frames; ++i) {
        if (g_music.counter >= g_music.stepSamples) {
            g_music.counter -= g_music.stepSamples;
            g_music.step = (g_music.step + 1) % 8;
        }
        const float t   = static_cast<float>(g_music.counter / g_music.stepSamples); // 0..1
        const float sec = static_cast<float>(g_music.counter / RATE);                // seconds

        const int* pattern = g_music.warm ? kEscapeSteps : kClimbSteps;
        const float f = noteFreq(pattern[g_music.step]);

        float sample = 0.0f;
        // Lead voice: the amplitude envelope starts at zero so per-note phase
        // resets never click.
        sample += 0.55f * tri(f * sec) * env(t, 1.0f, 0.02f, 0.35f);
        // Bass on the downbeat, dropped once the arrangement is thinned out.
        if (!g_music.thinned && (g_music.step % 4 == 0)) {
            sample += 0.5f * sine(noteFreq(pattern[g_music.step] - 24) * sec);
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

    sPush_ = make(0.20f, [](float t) {
        Noise n; n.s = static_cast<std::uint32_t>(t * 1000000.0f) + 7u;
        const float sweep = 170.0f - 118.0f * std::min(1.0f, t / 0.16f);
        const float body  = sine(sweep * t) * env(t, 0.20f, 0.004f, 0.18f);
        const float grit  = n.next() * env(t, 0.05f, 0.001f, 0.05f) * 0.5f;
        return (body + grit) * 0.9f;
    });

    sScrape_ = make(0.16f, [](float t) {
        Noise n; n.s = static_cast<std::uint32_t>(t * 900000.0f) + 31u;
        float lp = 0.0f;
        lp += (n.next() - lp) * 0.18f;   // crude one-pole low pass
        return lp * env(t, 0.16f, 0.006f, 0.10f) * 0.8f;
    });

    sRattle_ = make(0.42f, [](float t) {
        Noise n; n.s = 4242u;
        float v = 0.0f;
        for (int k = 0; k < 9; ++k) {
            const float tk = 0.045f * k;
            const float e = env(t - tk, 0.05f, 0.002f, 0.045f);
            if (e > 0.0f) v += n.next() * e;
        }
        return v * 0.5f;
    });

    sBreath_ = make(0.75f, [](float t) {
        Noise n; n.s = 999u;
        float lp = 0.0f;
        lp += (n.next() - lp) * 0.05f;
        const float shape = std::sin(t / 0.75f * 3.14159f);   // in-and-out
        return lp * shape * 0.7f;
    });

    sJingle_ = make(0.70f, [](float t) {
        const int seq[3] = { 7, 12, 19 };   // E5, A5, E6-ish (relative to A4)
        const int idx = t < 0.22f ? 0 : (t < 0.44f ? 1 : 2);
        const float f = noteFreq(seq[idx]);
        return square(f * t) * env(t - 0.22f * idx, 0.26f, 0.005f, 0.20f) * 0.5f;
    });

    sBell_ = make(1.20f, [](float t) {
        const float a = sine(880.0f * t) * std::exp(-3.0f * t);
        const float b = sine(1320.0f * t) * std::exp(-4.5f * t) * 0.6f;
        const float c = sine(1760.0f * t) * std::exp(-6.0f * t) * 0.35f;
        return (a + b + c) * 0.6f;
    });

    sCollapse_ = make(0.60f, [](float t) {
        Noise n; n.s = 77u;
        const float thud = sine((90.0f - 55.0f * std::min(1.0f, t / 0.3f)) * t) * std::exp(-6.0f * t);
        const float dirt = n.next() * env(t, 0.25f, 0.002f, 0.25f) * 0.4f;
        return (thud + dirt) * 0.9f;
    });

    sRoll_ = make(1.10f, [](float t) {
        Noise n; n.s = 5150u;
        float lp = 0.0f;
        lp += (n.next() - lp) * 0.30f;
        const float rise = std::min(1.0f, t / 0.9f);
        return lp * rise * 0.55f;
    });

    sBirds_ = make(1.60f, [](float t) {
        float v = 0.0f;
        const float starts[4] = { 0.05f, 0.42f, 0.80f, 1.20f };
        const float freqs[4]  = { 2600.0f, 3000.0f, 2400.0f, 2800.0f };
        for (int k = 0; k < 4; ++k) {
            const float lt = t - starts[k];
            if (lt < 0.0f || lt > 0.25f) continue;
            const float w = 1.0f + 0.25f * std::sin(lt * 90.0f);   // chirp wobble
            v += sine(freqs[k] * w * lt) * env(lt, 0.25f, 0.01f, 0.18f) * 0.35f;
        }
        return v;
    });

    music_ = LoadAudioStream(RATE, 32, 1);
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

void Synth::musicSetMood(bool warm, bool thinned) {
    g_music.warm = warm;
    g_music.thinned = thinned;
}

void Synth::musicVolume(float v) {
    if (!ready_) return;
    SetAudioStreamVolume(music_, std::clamp(v, 0.0f, 1.0f));
}

} // namespace cashyphus::audio
