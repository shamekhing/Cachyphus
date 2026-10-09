#pragma once

#include <cstdint>

#include "core/sim.hpp"

namespace cashyphus {

// Tiny deterministic xorshift RNG so dialogue choice is reproducible in tests.
struct Rng {
    std::uint32_t s = 0x2545F491u;
    std::uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float unit() { return static_cast<float>(next() >> 8) / 16777216.0f; }
    int range(int n) { return n <= 0 ? 0 : static_cast<int>(next() % static_cast<std::uint32_t>(n)); }
};

// Ambient line for the current life stage / incarnation.
// `avoid` is the previous line so the ball does not repeat itself immediately.
const char* selectLine(Stage stage, int life, Rng& rng, const char* avoid = nullptr);

// The ball nags when the player stops pushing.
const char* selectComplaint(Stage stage, int life, Rng& rng, const char* avoid = nullptr);

// Escalating lines that fire when the total number of lives spent hits a
// milestone (5, 10, 20, 50, 100). Returns nullptr otherwise.
const char* milestoneLine(int livesCompleted);

// Fixed narrative lines for the scripted beats.
namespace lines {
extern const char* const summit;      // "WE DID IT!"
extern const char* const silence;     // "...Hello?"
extern const char* const choiceIntro; // "Well, shall we?"
extern const char* const continue_ok; // "That's the spirit!"
extern const char* const walkLeave[]; // ball panicking as the player walks off
extern const int         walkLeaveCount;
extern const char* const walkGreeting; // "Hello, future millionaire!" for the next person
} // namespace lines

} // namespace cashyphus
