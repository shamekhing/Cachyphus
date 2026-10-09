#pragma once

#include "core/config.hpp"

namespace cashyphus {

// The four life stages the character passes through as the ball climbs.
enum class Stage : int { Youth = 0, Adult = 1, Old = 2, Final = 3 };

struct SimInput {
    bool push   = false;  // edge: SPACE pressed on this step
    bool brace  = false;  // level: SHIFT held
    bool active = true;   // false freezes the simulation (death sequences)
};

struct SimState {
    float progress = 0.0f;  // 0..1 along the hill (current position)
    float peak     = 0.0f;  // furthest the ball has ever been *this life*
    float vel      = 0.0f;  // progress / second (+ = uphill)
    float stamina  = 1.0f;  // 0..1
    float grip     = 1.0f;  // 0..1
    bool  slipping = false; // latched when grip is exhausted
    float strain   = 0.0f;  // 0..1 smoothed "exhaustion" for audio/visuals
    float pushWait = 0.0f;  // seconds until the next push may register
    int   pushes   = 0;     // accepted (non-cooldown) pushes this life
    float elapsed  = 0.0f;  // seconds since this climb began
};

// Deterministic, raylib-free ball/climb simulation.
class Sim {
public:
    void reset();

    // Which incarnation this is. Every life after the first makes the ball a
    // little heavier, so the climbs get tighter.
    void setLife(int life);
    float difficulty() const { return difficulty_; }

    bool canPush() const { return st_.pushWait <= 0.0f; }

    void step(const SimInput& in, float dt);

    const SimState& state() const { return st_; }

    // Aging is driven by how far the ball has ever got -- rolling back down
    // does not make the character younger again.
    Stage stage() const { return stageFor(st_.peak); }
    int   ageYears() const;
    float stageBlend() const;   // 0..1 within the current (peak-based) stage
    float peakProgress() const { return st_.peak; }
    bool  reachedSummit() const { return st_.progress >= 1.0f; }
    bool  isSlipping() const { return st_.slipping; }

    static Stage stageFor(float progress);
    static float slopeAt(float progress);

private:
    SimState st_;
    float    difficulty_ = 1.0f;   // gravity multiplier from setLife()
};

} // namespace cashyphus
