#include "core/sim.hpp"

#include <cmath>

namespace cashyphus {

using namespace cfg;

void Sim::reset() { st_ = SimState{}; }   // difficulty_ deliberately survives

void Sim::setLife(int life) {
    const int n = std::min(std::max(life - 1, 0), CYCLE_GRAVITY_CAP);
    difficulty_ = 1.0f + CYCLE_GRAVITY_STEP * static_cast<float>(n);
}

Stage Sim::stageFor(float p) {
    if (p < STAGE_YOUTH_END) return Stage::Youth;
    if (p < STAGE_ADULT_END) return Stage::Adult;
    if (p < STAGE_OLD_END)   return Stage::Old;
    return Stage::Final;
}

float Sim::slopeAt(float p) {
    return SLOPE_BASE + SLOPE_GROWTH * clampf(p, 0.0f, 1.0f);
}

int Sim::ageYears() const {
    // Peak, not current position: you never get the years back.
    const float p = clampf(st_.peak, 0.0f, 1.0f);
    return AGE_START + static_cast<int>(std::lround(p * (AGE_END - AGE_START)));
}

float Sim::stageBlend() const {
    const float p = clampf(st_.peak, 0.0f, 1.0f);
    switch (stageFor(p)) {
        case Stage::Youth: return p / STAGE_YOUTH_END;
        case Stage::Adult: return (p - STAGE_YOUTH_END) / (STAGE_ADULT_END - STAGE_YOUTH_END);
        case Stage::Old:   return (p - STAGE_ADULT_END) / (STAGE_OLD_END - STAGE_ADULT_END);
        case Stage::Final: return (p - STAGE_OLD_END) / (1.0f - STAGE_OLD_END);
    }
    return 0.0f;
}

void Sim::step(const SimInput& in, float dt) {
    if (!in.active || dt <= 0.0f) return;
    SimState& s = st_;
    s.elapsed += dt;

    // Frailty: an aging body both shoves less and recovers worse. Aging is
    // measured from the peak, so losing ground never un-wears the years.
    const int   ageStage   = static_cast<int>(stageFor(s.peak));
    const float staminaEff = AGE_STAMINA_EFF[ageStage];
    const float gripEff    = AGE_GRIP_EFF[ageStage];

    // 1. Push: an impulse scaled by remaining stamina and age. A push only
    //    registers once the character has recovered their footing, and an
    //    exhausted shove (stamina 0) is worth almost nothing -- which is why
    //    endless key-mashing cannot substitute for actually resting.
    s.pushWait = std::max(0.0f, s.pushWait - dt);
    if (in.push && s.pushWait <= 0.0f) {
        const float sf = STAMINA_MIN_FACTOR + (1.0f - STAMINA_MIN_FACTOR) * s.stamina;
        const float af = AGE_FACTOR[ageStage];
        s.vel += PUSH_IMPULSE * sf * af;
        // An old frame pays more for the same shove.
        s.stamina = clampf(s.stamina - PUSH_COST / staminaEff, 0.0f, 1.0f);
        s.pushWait = PUSH_COOLDOWN;
        s.pushes++;
    }

    // 2. Gravity pulls the ball back down the (ever-steeper, heavier) hill,
    //    damping eats speed.
    s.vel -= GRAVITY * slopeAt(s.progress) * difficulty_ * dt;
    s.vel *= std::max(0.0f, 1.0f - DAMPING * dt);

    // 3. Brace holds the ball in place, drains grip and rebuilds stamina.
    const bool bracing = in.brace && !s.slipping && s.grip > 0.0f;
    if (bracing) {
        s.vel = approachf(s.vel, 0.0f, BRACE_ACCEL * dt);
        // Weak hands lose their hold sooner...
        s.grip -= (GRIP_DRAIN / gripEff) * dt;
        // ...and old lungs refill more slowly.
        s.stamina = clampf(s.stamina + STAMINA_REGEN_BRACE * staminaEff * dt, 0.0f, 1.0f);
        if (s.grip <= 0.0f) {
            s.grip = 0.0f;
            s.slipping = true;   // grip gone: must let go and recover
        }
    } else {
        s.grip = clampf(s.grip + GRIP_REGEN * gripEff * dt, 0.0f, 1.0f);
        if (s.slipping && s.grip >= GRIP_RECOVER_THRESHOLD) s.slipping = false;
        s.stamina = clampf(s.stamina + STAMINA_REGEN_IDLE * staminaEff * dt, 0.0f, 1.0f);
    }

    // 4. Clamp and integrate.
    s.vel = clampf(s.vel, -MAX_SPEED, MAX_SPEED);
    s.progress += s.vel * dt;
    if (s.progress <= 0.0f) {
        s.progress = 0.0f;
        if (s.vel < 0.0f) s.vel = 0.0f;   // grounded at the bottom
    }
    if (s.progress >= 1.0f) s.progress = 1.0f;   // summit

    // Age is remembered, never undone: record the high-water mark.
    if (s.progress > s.peak) s.peak = s.progress;

    // 5. Smoothed strain indicator (used for breathing sfx / sprite wobble).
    const float target = clampf((1.0f - s.stamina) * 0.7f +
                                (static_cast<float>(static_cast<int>(stageFor(s.peak))) / 3.0f) * 0.3f,
                                0.0f, 1.0f);
    s.strain += (target - s.strain) * std::min(1.0f, dt * 3.0f);
}

} // namespace cashyphus
