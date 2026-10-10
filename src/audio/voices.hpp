#pragma once

// =============================================================================
//  The nine sound effects, as pure functions of the sample index.
//
//  These used to be lambdas inside synth.cpp, each building its own little
//  xorshift generator, seeded from the sample time and advanced exactly once:
//
//      Noise n; n.s = (std::uint32_t)(t * 1000000.0f) + 7u;
//      ... n.next() ...
//
//  A xorshift seeded from a smooth function of time and stepped a single time is
//  not a noise source -- xor and shift are linear, so nearby seeds give nearby
//  results. Measured, the "grit" and the "scrape" came out as a straight ramp
//  from zero (+0.0009, +0.0037, +0.0065, ...) and the ones with a fixed seed
//  came out as the SAME value on every sample (+0.5027 for the rattle, +0.6482
//  for the roll). So every supposed hiss, scuff, rattle and rumble was a DC step
//  gated by an envelope: clicks, pops and a buzz, which is what "the sound
//  effects are static" sounds like.
//
//  Here each voice is a pure function of the sample index and the noise comes
//  from an integer hash instead. That buys three things:
//
//    * the noise is genuinely noisy, and reproducible, so tests can measure it;
//    * box filtering it is stateless, because a moving average is just more
//      hash calls at earlier indices;
//    * DC can be removed by subtracting a longer average -- the difference of
//      two low-passes is a band-pass, so there is no offset left to step the
//      speaker cone with.
//
//  The other lesson is levels. raylib sums every voice with no limiter of its
//  own, so anything near full scale clips as soon as two effects land together.
//  Every voice here peaks well under unity by design, and `make()` still runs
//  the encoded sample through tanh, so no peak can ever be a hard corner.
// =============================================================================

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace cashyphus::audio::voices {

// The rate everything here is authored at (and the rate raylib is given).
constexpr int RATE = 44100;

enum class Voice {
    Push, Scrape, Rattle, Breath, Jingle, Bell, Collapse, Roll, Birds, Count,
};

// --- noise primitives --------------------------------------------------------

// Integer hash -> white noise in [-1, 1). Full avalanche, so neighbouring
// indices are uncorrelated; stateless, so a voice stays a pure function of time.
inline float noise(int i) {
    std::uint32_t x = static_cast<std::uint32_t>(i) * 0x9E3779B9u + 0x85EBCA6Bu;
    x ^= x >> 16; x *= 0x7FEB352Du;
    x ^= x >> 15; x *= 0x846CA68Bu;
    x ^= x >> 16;
    return static_cast<float>(static_cast<std::int32_t>(x)) * (1.0f / 2147483648.0f);
}

// Box-filtered noise: a moving average over `span` samples. A cheap, stateless
// low-pass, which is what takes the fizz off a raw hash. span <= 1 is raw.
inline float softNoise(int i, int span) {
    if (span <= 1) return noise(i);
    float sum = 0.0f;
    for (int k = 0; k < span; ++k) sum += noise(i - k);
    return sum / static_cast<float>(span);
}

// Band-limited noise with no offset: a short average minus a much longer one.
// The difference of two low-passes is a band-pass, so this has no DC at all.
inline float bandNoise(int i, int fast, int slow) {
    return softNoise(i, fast) - softNoise(i, slow);
}

// --- tiny synthesis helpers --------------------------------------------------
//
// `phase` is in CYCLES, and the caller hands over a big number: `square(f * t)`
// with f in the hundreds and t in seconds. sine() wraps that for free, but
// square() and tri() do not -- comparing `f * t` against 0.5 is false for every
// sample after the first half-millisecond, so those two used to return a
// CONSTANT for the whole note. The summit fanfare was a negative DC pulse and
// the escape motif's triangle arpeggio was a saturating drone. Hence frac().
inline float frac(float x) { return x - std::floor(x); }

inline float sine(float phase)   { return std::sin(phase * 6.2831853f); }
inline float square(float phase) { return frac(phase) < 0.5f ? 1.0f : -1.0f; }
inline float tri(float phase)    { const float p = frac(phase); return 4.0f * std::fabs(p - 0.5f) - 1.0f; }

inline float env(float t, float dur, float attack, float release) {
    if (t < 0.0f || t >= dur) return 0.0f;
    const float a = attack  > 0.0f ? std::min(1.0f, t / attack) : 1.0f;
    const float r = release > 0.0f ? std::min(1.0f, (dur - t) / release) : 1.0f;
    return a * r;
}

inline float noteFreq(int semitoneFromA4) {
    return 440.0f * std::pow(2.0f, semitoneFromA4 / 12.0f);
}

// --- voices ------------------------------------------------------------------

inline float seconds(Voice v) {
    switch (v) {
        case Voice::Push:     return 0.20f;
        case Voice::Scrape:   return 0.16f;
        case Voice::Rattle:   return 0.45f;
        case Voice::Breath:   return 0.75f;
        case Voice::Jingle:   return 0.70f;
        case Voice::Bell:     return 1.20f;
        case Voice::Collapse: return 0.60f;
        case Voice::Roll:     return 1.10f;
        case Voice::Birds:    return 1.60f;
        default:              return 0.0f;
    }
}

// One mono sample. Every voice fades in from and out to silence, so no effect
// ever starts or stops on a step, and none of them carries a DC offset.
inline float sample(Voice v, int i) {
    const float t = static_cast<float>(i) / static_cast<float>(RATE);

    switch (v) {
        case Voice::Push: {
            // A shove: a low body dropping in pitch, with a short band-limited
            // scuff on the attack rather than a burst of hiss.
            const float sweep = 150.0f - 100.0f * std::min(1.0f, t / 0.16f);
            const float body  = sine(sweep * t) * env(t, 0.20f, 0.004f, 0.18f);
            const float scuff = bandNoise(i, 4, 36) * env(t, 0.04f, 0.001f, 0.038f);
            return body * 0.52f + scuff * 0.20f;
        }

        case Voice::Scrape: {
            // A foot dragged over dirt: muffled noise, no top end to hiss.
            return softNoise(i, 40) * env(t, 0.16f, 0.010f, 0.10f) * 0.34f;
        }

        case Voice::Rattle: {
            // Coins: nine short metallic clicks, each three inharmonic partials
            // ringing down, jittered so the rhythm is not a metronome.
            float v = 0.0f;
            for (int k = 0; k < 9; ++k) {
                const float lt = t - 0.048f * static_cast<float>(k);
                if (lt < 0.0f || lt >= 0.055f) continue;
                const float jitter = 0.5f + 0.5f * noise(97 * k + 11);
                const float f = 1850.0f + 900.0f * jitter;
                const float ring = std::exp(-70.0f * lt);
                v += (sine(f * lt) * 0.62f + sine(f * 2.76f * lt) * 0.22f +
                      sine(f * 5.40f * lt) * 0.10f) * ring;
            }
            return v * 0.30f;
        }

        case Voice::Breath: {
            // One slow breath, muffled right down so it reads as air, not fizz.
            const float swell = std::sin(t / 0.75f * 3.14159f);
            return softNoise(i, 56) * swell * 0.22f;
        }

        case Voice::Jingle: {
            // Summit fanfare: three rising square notes, each with its own decay.
            static const int seq[3] = { 7, 12, 19 };
            const int   idx = t < 0.22f ? 0 : (t < 0.44f ? 1 : 2);
            const float lt  = t - 0.22f * static_cast<float>(idx);
            const float f   = noteFreq(seq[idx]);
            return square(f * t) * env(lt, 0.26f, 0.005f, 0.14f) *
                   std::exp(-2.0f * lt) * 0.34f;
        }

        case Voice::Bell: {
            // A bell: inharmonic partials, each ringing at its own rate.
            const float f = 660.0f;
            return (sine(f * t) * 0.55f + sine(f * 2.76f * t) * 0.26f +
                    sine(f * 5.40f * t) * 0.12f) * std::exp(-2.6f * t) * 0.62f;
        }

        case Voice::Collapse: {
            // Weight hitting the ground: a falling thud with a dry crunch.
            const float thud = sine((90.0f - 55.0f * std::min(1.0f, t / 0.3f)) * t) *
                               std::exp(-6.0f * t);
            const float crunch = bandNoise(i, 3, 24) * env(t, 0.22f, 0.002f, 0.22f);
            return thud * 0.55f + crunch * 0.18f;
        }

        case Voice::Roll: {
            // The ball heading home: a low rumble over gravel, fading out so it
            // never ends on a step.
            const float rise   = std::min(1.0f, t / 0.35f);
            const float fade   = 1.0f - std::min(1.0f, std::max(0.0f, (t - 0.70f) / 0.40f));
            const float rumble = sine(56.0f * t) * 0.24f;
            const float gravel = softNoise(i, 64) * 0.34f;
            return (rumble + gravel) * rise * fade;
        }

        case Voice::Birds: {
            // Four chirps, heard only once the player walks away.
            float v = 0.0f;
            static const float starts[4] = { 0.05f, 0.42f, 0.80f, 1.20f };
            static const float freqs[4]  = { 2600.0f, 3000.0f, 2400.0f, 2800.0f };
            for (int k = 0; k < 4; ++k) {
                const float lt = t - starts[k];
                if (lt < 0.0f || lt > 0.25f) continue;
                const float w = 1.0f + 0.25f * std::sin(lt * 90.0f);   // chirp wobble
                v += sine(freqs[k] * w * lt) * env(lt, 0.25f, 0.01f, 0.18f) * 0.30f;
            }
            return v;
        }

        default: return 0.0f;
    }
}

} // namespace cashyphus::audio::voices
