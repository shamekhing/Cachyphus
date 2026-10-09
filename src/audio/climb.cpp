#include "audio/climb.hpp"

#include <cmath>

#include "audio/music_data.hpp"

namespace cashyphus::audio {

namespace {
// The output rate the audio layer runs at. Kept local because climb.cpp only
// needs it to work out how fast to walk the baked table.
constexpr int OUT_RATE = 44100;
} // namespace

void ClimbTheme::reset() {
    phase_  = 0.0;
    lp_     = 0.0f;
    held_   = 0.0f;
    holdIx_ = 0;
}

void ClimbTheme::setArrangement(const core::Arrangement& a) { arr_ = a; }

float ClimbTheme::next() {
    const int    n    = musicdata::SAMPLE_COUNT;
    const double step = static_cast<double>(musicdata::SAMPLE_RATE) / OUT_RATE *
                        std::pow(2.0, static_cast<double>(arr_.detune) / 1200.0);

    // Sample-and-hold: reuse the current source sample for `hold` output
    // samples. This is what actually lowers the effective rate.
    if (arr_.hold > 1 && holdIx_ > 0) {
        --holdIx_;
    } else {
        const int   i0   = static_cast<int>(phase_);
        const int   i1   = (i0 + 1) % n;
        const float frac = static_cast<float>(phase_ - i0);
        // Linear interpolation, because the table is 22050 and we run at 44100.
        held_   = (musicdata::PCM[i0] * (1.0f - frac) + musicdata::PCM[i1] * frac) / 32768.0f;
        holdIx_ = arr_.hold - 1;
        phase_ += step;
        if (phase_ >= n) phase_ -= n;
    }

    // Brightness first.
    lp_ += arr_.tone * (held_ - lp_);

    float v = lp_;
    if (arr_.bits < 16) {
        const float levels = static_cast<float>(1 << (arr_.bits - 1));
        v = std::round(v * levels) / levels;
    }
    return v * arr_.gain;
}

} // namespace cashyphus::audio
