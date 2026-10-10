// Headless unit tests for the raylib-free core of CASHYPHUS.
// Run with:  ctest --test-dir build --output-on-failure

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "audio/voices.hpp"
#include "core/dialogue.hpp"
#include "core/game_state.hpp"
#include "core/music.hpp"
#include "core/palette.hpp"
#include "core/roll.hpp"
#include "core/sim.hpp"

using namespace cashyphus;
using namespace cashyphus::cfg;

static int g_fail = 0;
static int g_checks = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        ++g_checks;                                                            \
        if (!(cond)) {                                                         \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
            ++g_fail;                                                          \
        }                                                                      \
    } while (0)

static void section(const char* name) { std::printf("[ %s ]\n", name); }

// =============================================================================
//  Sim: stage boundaries and aging
// =============================================================================
static void test_stages_and_aging() {
    section("sim: stages & aging");
    CHECK(Sim::stageFor(0.00f) == Stage::Youth);
    CHECK(Sim::stageFor(0.24f) == Stage::Youth);
    CHECK(Sim::stageFor(0.25f) == Stage::Adult);
    CHECK(Sim::stageFor(0.49f) == Stage::Adult);
    CHECK(Sim::stageFor(0.50f) == Stage::Old);
    CHECK(Sim::stageFor(0.74f) == Stage::Old);
    CHECK(Sim::stageFor(0.75f) == Stage::Final);
    CHECK(Sim::stageFor(1.00f) == Stage::Final);

    // Slope strictly increases up the hill.
    CHECK(Sim::slopeAt(0.0f) < Sim::slopeAt(0.5f));
    CHECK(Sim::slopeAt(0.5f) < Sim::slopeAt(1.0f));

    Sim s;
    s.reset();
    CHECK(s.ageYears() == AGE_START);

    // Drive to the summit with a realistic push/brace rhythm. NOTE: a bounded
    // loop -- mashing alone no longer reaches the top, so an unbounded
    // "while (!reachedSummit())" here would spin forever.
    bool reached = false;
    bool resting = false;
    for (int f = 0; f < 60 * 300 && !reached; ++f) {
        if (s.state().stamina < 0.15f) resting = true;
        if (s.state().stamina > 0.70f) resting = false;
        SimInput in;
        in.active = true;
        if (resting)            in.brace = true;
        else if (f % 10 == 0)   in.push  = true;
        s.step(in, FIXED_DT);
        reached = s.reachedSummit();
    }
    CHECK(reached);
    CHECK(s.ageYears() == AGE_END);
    CHECK(s.stage() == Stage::Final);
}

// =============================================================================
//  Sim: push / stamina
// =============================================================================
static void test_push_and_stamina() {
    section("sim: push & stamina");
    Sim s;
    s.reset();
    SimInput in;
    in.active = true;

    in.push = true;
    s.step(in, FIXED_DT);
    CHECK(s.state().vel > 0.0f);
    CHECK(s.state().stamina < 1.0f);
    CHECK(s.state().pushes == 1);

    // Keep pushing on a realistic beat (one press every 9 frames, which is
    // inside the cooldown), and the tank must visibly drain.
    for (int i = 0; i < 90; ++i) {
        SimInput p;
        p.active = true;
        p.push = (i % 9 == 0);
        s.step(p, FIXED_DT);
    }
    CHECK(s.state().stamina < 0.9f);
    CHECK(s.state().progress > 0.0f);
    CHECK(s.state().pushes > 5);

    // Stamina never goes out of range no matter how hard we mash.
    in.push = true;
    for (int i = 0; i < 600; ++i) s.step(in, FIXED_DT);
    CHECK(s.state().stamina >= 0.0f);
    CHECK(s.state().stamina <= 1.0f);
}

// =============================================================================
//  Sim: the push cooldown. Spamming must not translate into extra shoves.
// =============================================================================
static void test_push_rate_is_capped() {
    section("sim: push rate is capped");

    Sim spam;
    spam.reset();
    SimInput in;
    in.active = true;
    in.push = true;
    for (int i = 0; i < 60; ++i) spam.step(in, FIXED_DT);   // 60 attempts in 1 second

    std::printf("  %d of 60 frame-perfect presses were accepted\n", spam.state().pushes);
    CHECK(spam.state().pushes >= 5);    // roughly one per cooldown...
    CHECK(spam.state().pushes <= 10);   // ...nowhere near one per frame
}

// =============================================================================
//  Sim: gravity rolls the ball back when you do nothing
// =============================================================================
static void test_gravity_rollback() {
    section("sim: gravity rollback");
    Sim s;
    s.reset();
    SimInput in;
    in.active = true;

    // Push at a human-ish rate (~6/s) for two seconds.
    for (int f = 0; f < 120; ++f) {
        in.push = (f % 10 == 0);
        s.step(in, FIXED_DT);
    }
    const float peak = s.state().progress;
    CHECK(peak > 0.0f);

    // Now let go entirely for ten seconds: the hill must win.
    in.push = false;
    in.brace = false;
    for (int f = 0; f < 600; ++f) s.step(in, FIXED_DT);
    CHECK(s.state().progress < peak);
    CHECK(s.state().vel <= 0.0f);
}

// =============================================================================
//  Sim: aging is monotonic. Losing ground must never make the character young
//  again -- that was a real bug: rolling back down visibly de-aged the sprite.
// =============================================================================
static void test_aging_never_reverses() {
    section("sim: aging never reverses");
    Sim s;
    s.reset();

    // Climb until the character is genuinely old (progress past 50%), using the
    // real push/brace rhythm -- grinding the key alone cannot get here any more.
    SimInput in;
    in.active = true;
    bool resting = false;
    for (int f = 0; f < 60 * 120 && s.state().progress < 0.52f; ++f) {
        if (s.state().stamina < 0.15f) resting = true;
        if (s.state().stamina > 0.70f) resting = false;
        in.push  = !resting && (f % 10 == 0);
        in.brace = resting;
        s.step(in, FIXED_DT);
    }
    CHECK(s.state().progress >= 0.52f);
    CHECK(s.stage() == Stage::Old);

    const Stage stageAtTop = s.stage();
    const int   ageAtTop   = s.ageYears();
    const float peakAtTop  = s.peakProgress();

    // Now let go completely and let the hill win.
    in.push = false;
    in.brace = false;
    for (int f = 0; f < 60 * 120; ++f) s.step(in, FIXED_DT);

    CHECK(s.state().progress < 0.3f);        // it really did roll back down
    CHECK(s.peakProgress() >= peakAtTop);    // the high-water mark survives
    CHECK(s.stage() == stageAtTop);          // but the body did NOT get younger
    CHECK(s.ageYears() >= ageAtTop);         // and neither did the years
}

// =============================================================================
//  Sim: bracing holds the ball, rebuilds stamina, and spends grip
// =============================================================================
static void test_brace_grip_and_slip() {
    section("sim: brace, grip, slip");
    Sim s;
    s.reset();
    SimInput in;
    in.active = true;

    in.push = true;
    for (int i = 0; i < 60; ++i) s.step(in, FIXED_DT);
    const float drained = s.state().stamina;
    CHECK(drained < 1.0f);

    in.push = false;
    in.brace = true;
    const float gripBefore = s.state().grip;
    for (int i = 0; i < 30; ++i) s.step(in, FIXED_DT);
    CHECK(s.state().stamina > drained);
    CHECK(s.state().grip < gripBefore);

    // Hold until the grip gives out -> the slipping latch trips.
    bool slipped = false;
    int heldFrames = 0;
    for (; heldFrames < 60 * 8 && !slipped; ++heldFrames) {
        s.step(in, FIXED_DT);
        if (s.isSlipping()) slipped = true;
    }
    CHECK(slipped);
    CHECK(s.state().grip < GRIP_RECOVER_THRESHOLD);
    std::printf("  grip lasted %.2f s before slipping\n", heldFrames * FIXED_DT);

    // Once slipping, bracing does nothing and grip starts recovering by itself.
    in.brace = false;
    for (int i = 0; i < 60 * 2; ++i) s.step(in, FIXED_DT);
    CHECK(s.state().grip > GRIP_RECOVER_THRESHOLD);
    CHECK(!s.isSlipping());
}

// =============================================================================
//  Sim: a mechanically-competent player can always finish (no softlock) and
//  passes through all four life stages doing so.
// =============================================================================
struct AutoClimbResult {
    float seconds = 0.0f;
    bool  reached = false;
    bool  sawYouth = false, sawAdult = false, sawOld = false, sawFinal = false;
    float maxProgress = 0.0f;
};

static AutoClimbResult autoClimb(int pushesPerSecond) {
    AutoClimbResult r;
    Sim sim;
    sim.reset();
    const int pushEvery = std::max(1, static_cast<int>(std::lround(60.0 / pushesPerSecond)));
    const int maxFrames = 60 * 900;   // 15-minute safety ceiling
    int frame = 0;
    bool resting = false;
    for (; frame < maxFrames && !sim.reachedSummit(); ++frame) {
        const SimState& s = sim.state();
        if (s.stamina < 0.15f) resting = true;    // spent -> brace and recover
        if (s.stamina > 0.70f) resting = false;   // recovered -> push again

        SimInput in;
        in.active = true;
        // Mash on the beat; between presses do nothing and let the ball roll.
        if (resting) {
            in.brace = true;
        } else if ((frame % pushEvery) == 0) {
            in.push = true;
        }

        sim.step(in, FIXED_DT);
        r.maxProgress = std::max(r.maxProgress, sim.state().progress);
        switch (sim.stage()) {
            case Stage::Youth: r.sawYouth = true; break;
            case Stage::Adult: r.sawAdult = true; break;
            case Stage::Old:   r.sawOld   = true; break;
            case Stage::Final: r.sawFinal = true; break;
        }
    }
    r.reached = sim.reachedSummit();
    r.seconds = frame * FIXED_DT;
    return r;
}

static void test_auto_climb_is_completable() {
    section("sim: every life can reach the summit");

    // A quick masher.
    const AutoClimbResult fast = autoClimb(6);
    std::printf("  climb @6 presses/s = %.1f s\n", fast.seconds);
    CHECK(fast.reached);
    CHECK(fast.sawYouth && fast.sawAdult && fast.sawOld && fast.sawFinal);
    // Target is roughly 30-45 s/life for competent play.
    CHECK(fast.seconds >= 20.0f);
    CHECK(fast.seconds <= 60.0f);

    // A moderate pace should be the sweet spot.
    const AutoClimbResult mid = autoClimb(4);
    std::printf("  climb @4 presses/s = %.1f s\n", mid.seconds);
    CHECK(mid.reached);
    CHECK(mid.seconds >= 25.0f);
    CHECK(mid.seconds <= 80.0f);

    // Even a slow, careful player must never be softlocked.
    const AutoClimbResult slow = autoClimb(3);
    std::printf("  climb @3 presses/s = %.1f s (slower, but still finishes)\n", slow.seconds);
    CHECK(slow.reached);
    CHECK(slow.seconds <= 120.0f);
}

// =============================================================================
//  Sim: mashing SPACE on an empty tank must NOT be a strategy. This was the
//  reported bug -- endless pushing walked the ball to the summit with 0 stamina.
// =============================================================================
static void test_mashing_is_not_a_strategy() {
    section("sim: mashing cannot substitute for resting");
    Sim s;
    s.reset();
    SimInput in;
    in.active = true;
    in.push = true;                  // one push on every single frame, never brace

    float peak = 0.0f;
    for (int f = 0; f < 60 * 600; ++f) {   // ten straight minutes of mashing
        s.step(in, FIXED_DT);
        peak = std::max(peak, s.state().progress);
        if (s.reachedSummit()) break;
    }

    std::printf("  endless mashing peaked at %.2f of the hill\n", static_cast<double>(peak));
    CHECK(!s.reachedSummit());          // the hill wins
    CHECK(peak < 0.5f);                 // and never even gets halfway
    CHECK(s.state().stamina < 0.05f);   // because the tank is bone dry
}

// =============================================================================
//  Sim: each incarnation makes the ball heavier
// =============================================================================
static void test_later_lives_are_heavier() {
    section("sim: each incarnation is heavier");

    Sim first;
    first.reset();
    first.setLife(1);

    Sim later;
    later.reset();
    later.setLife(10);

    CHECK(first.difficulty() < later.difficulty());

    // Identical input, identical time: the heavier ball travels less far.
    SimInput in;
    in.active = true;
    in.push = true;
    for (int f = 0; f < 60 * 3; ++f) {
        first.step(in, FIXED_DT);
        later.step(in, FIXED_DT);
    }
    CHECK(later.state().progress < first.state().progress);

    // The ramp is capped so it can never become unwinnable.
    Sim capped;
    capped.reset();
    capped.setLife(9999);
    const float maxDiff = 1.0f + CYCLE_GRAVITY_STEP * static_cast<float>(CYCLE_GRAVITY_CAP);
    CHECK(std::fabs(capped.difficulty() - maxDiff) < 1e-5f);
}

// =============================================================================
//  Sim: the body wears down. Aging must cost stamina AND grip efficiency, not
//  just raw push power -- an old frame tires sooner and holds on for less time.
// =============================================================================
static void pumpTo(Sim& s, float target, int maxFrames) {
    bool resting = false;
    for (int f = 0; f < maxFrames && s.state().progress < target; ++f) {
        if (s.state().stamina < 0.15f) resting = true;
        if (s.state().stamina > 0.70f) resting = false;
        SimInput in;
        in.active = true;
        in.push   = !resting && (f % 10 == 0);
        in.brace  = resting;
        s.step(in, FIXED_DT);
    }
}

static void test_aging_degrades_stamina_and_grip() {
    section("sim: aging degrades stamina and grip efficiency");

    Sim young;
    young.reset();
    young.setLife(1);

    Sim aged;
    aged.reset();
    aged.setLife(1);
    pumpTo(aged, 0.80f, 60 * 300);          // climb into the final years
    CHECK(aged.stage() == Stage::Final);
    CHECK(young.stage() == Stage::Youth);

    // --- stamina: the same shove costs an old body more ---
    // Recover first. This both fills the tank and guarantees the push cooldown
    // has expired, so we measure the cost of a shove rather than the absence of
    // one (an early version of this test silently measured a rejected press).
    auto staminaPerPush = [](Sim& s) {
        SimInput idle;
        idle.active = true;
        for (int i = 0; i < 60 * 30; ++i) {
            s.step(idle, FIXED_DT);
            if (i >= 12 && s.state().stamina >= 0.95f) break;
        }
        const int pushesBefore = s.state().pushes;
        const float before = s.state().stamina;
        SimInput in;
        in.active = true;
        in.push = true;
        s.step(in, FIXED_DT);
        const bool landed = s.state().pushes == pushesBefore + 1;
        return landed ? (before - s.state().stamina) : -1.0f;
    };
    const float youngCost = staminaPerPush(young);
    const float agedCost  = staminaPerPush(aged);
    CHECK(youngCost > 0.0f);   // -1 means the press was rejected, not measured
    CHECK(agedCost > 0.0f);
    std::printf("  stamina per push: young %.4f, aged %.4f\n",
                static_cast<double>(youngCost), static_cast<double>(agedCost));
    CHECK(agedCost > youngCost * 1.2f);

    // --- grip: an old hand loses its hold faster ---
    auto gripPerSecond = [](Sim& s) {
        SimInput idle;                       // let the grip fully recover first
        idle.active = true;
        for (int i = 0; i < 60 * 2; ++i) s.step(idle, FIXED_DT);
        const float before = s.state().grip;
        SimInput in;
        in.active = true;
        in.brace = true;
        for (int i = 0; i < 60; ++i) s.step(in, FIXED_DT);
        return before - s.state().grip;
    };
    const float youngGrip = gripPerSecond(young);
    const float agedGrip  = gripPerSecond(aged);
    std::printf("  grip per second:  young %.4f, aged %.4f\n",
                static_cast<double>(youngGrip), static_cast<double>(agedGrip));
    CHECK(agedGrip > youngGrip * 1.2f);
}

// The behavioural version of the sustainability check: put the simulation
// itself into the final years and confirm the climb still advances rather than
// stalling into a slip loop. (When both age curves were briefly too steep, this
// is exactly what failed: a life ran past the 900 s cap without finishing.)
static void test_old_age_can_still_climb() {
    section("sim: the final years can still gain ground");

    Sim s;
    s.reset();
    s.setLife(1);
    pumpTo(s, 0.80f, 60 * 300);
    CHECK(s.stage() == Stage::Final);

    const float before = s.state().progress;
    bool resting = false;
    for (int f = 0; f < 60 * 60; ++f) {          // one minute of steady play
        const SimState& st = s.state();
        if (st.stamina < 0.15f) resting = true;
        if (st.stamina > 0.70f) resting = false;
        SimInput in;
        in.active = true;
        in.brace  = resting;
        in.push   = !resting && (f % 10 == 0);
        s.step(in, FIXED_DT);
    }

    const float gained = s.state().progress - before;
    std::printf("  final years gained %.3f of the hill in 60 s\n", static_cast<double>(gained));
    CHECK(gained > 0.10f);                       // must not dead-end
}

// =============================================================================
//  Roll direction. Positive spin means CLOCKWISE on screen, which is the way a
//  ball turns when it rolls to the right, i.e. uphill. The sign was previously
//  inverted, so the ball span anticlockwise while climbing -- this pins it.
// =============================================================================
static void test_ball_roll_direction() {
    section("roll: uphill turns clockwise, downhill anticlockwise");

    // The "$" is stamped at the BOTTOM of the ball: surface coords (0, +12).
    const float fx = 0.0f, fy = 12.0f;
    const float rad = 0.01745329252f;

    // Where a surface point at (fx,fy) appears on screen, as an offset from the
    // ball's centre. This is the inverse of rollSurface(), derived from the
    // physical requirement, so it is an independent check of the convention
    // rather than a restatement of the implementation.
    auto screenX = [&](float spinDeg) {
        return fx * std::cos(spinDeg * rad) - fy * std::sin(spinDeg * rad);
    };
    auto screenY = [&](float spinDeg) {
        return fx * std::sin(spinDeg * rad) + fy * std::cos(spinDeg * rad);
    };

    // At rest the "$" hangs directly below the centre.
    CHECK(std::fabs(screenX(0.0f)) < 0.01f);
    CHECK(std::fabs(screenY(0.0f) - fy) < 0.01f);

    // Rolling UPHILL is a positive spin (clockwise). A point at the bottom of a
    // wheel rolling to the right must travel BACKWARDS, i.e. to the left.
    const float upX = screenX(90.0f);
    std::printf("  uphill  : the '$' at the bottom moves to x=%+.0f  (left = rolling right)\n", upX);
    CHECK(upX < -1.0f);
    CHECK(std::fabs(screenY(90.0f)) < 1.0f);

    // Rolling back DOWN is a negative spin (anticlockwise), so it travels
    // forward, to the right. Getting this backwards is the bug that was fixed.
    const float downX = screenX(-90.0f);
    std::printf("  downhill: the '$' at the bottom moves to x=%+.0f  (right = rolling left)\n", downX);
    CHECK(downX > 1.0f);

    // A half turn puts it at the top of the ball.
    CHECK(std::fabs(screenY(180.0f) + fy) < 0.01f);

    // Tie the test to the real implementation: feeding our derived screen
    // offset back through rollSurface() must return the original surface point.
    // If the sign inside rollSurface() ever flips, this reverses and fails.
    for (float spin : { -120.0f, -37.0f, 0.0f, 45.0f, 210.0f }) {
        float rx = 0.0f, ry = 0.0f;
        rollSurface(screenX(spin), screenY(spin), spin, rx, ry);
        CHECK(std::fabs(rx - fx) < 0.01f);
        CHECK(std::fabs(ry - fy) < 0.01f);
    }
}

// =============================================================================
//  Aging must be punishing, but it must never make a life impossible.
// =============================================================================
static void test_frailty_is_meaningful_but_survivable() {
    section("sim: aging is punishing, but never a dead end");

    // Steep and monotonic: every stage is weaker than the one before.
    for (int i = 1; i < 4; ++i) {
        CHECK(AGE_STAMINA_EFF[i] < AGE_STAMINA_EFF[i - 1]);
        CHECK(AGE_GRIP_EFF[i]    < AGE_GRIP_EFF[i - 1]);
    }
    const int last = 3;
    CHECK(1.0f / AGE_STAMINA_EFF[last] >= 1.5f);   // a shove costs >=50% more
    CHECK(AGE_STAMINA_EFF[last] <= 0.65f);         // recharge >=35% slower
    CHECK(1.0f / AGE_GRIP_EFF[last] >= 1.25f);     // grip drains >=25% faster
    CHECK(AGE_GRIP_EFF[last] <= 0.80f);            // grip recovers >=20% slower
}

// The rest cycle must stay sustainable at EVERY age. Choosing both age curves
// steeply made the final years dead-end: the slower stamina refill forces a
// longer brace, and the longer brace burns more grip than a burst can earn
// back, so the character slips forever and never gains ground. These numbers
// are a design invariant, checked here so that can never be tuned back in.
static void test_every_age_can_still_rest() {
    section("sim: every age can complete a rest cycle");

    for (int i = 0; i < 4; ++i) {
        const float se = AGE_STAMINA_EFF[i];
        const float ge = AGE_GRIP_EFF[i];
        const float burst  = 0.80f / (6.0f * PUSH_COST / se - STAMINA_REGEN_IDLE * se);
        const float rest   = 0.55f / (STAMINA_REGEN_BRACE * se);
        const float spent  = (GRIP_DRAIN / ge) * rest;
        const float earned = GRIP_REGEN * ge * burst;
        std::printf("  stage %d: burst %.2fs, rest %.2fs, grip spent %.2f vs earned %.2f\n",
                    i, static_cast<double>(burst), static_cast<double>(rest),
                    static_cast<double>(spent), static_cast<double>(earned));
        CHECK(earned > spent);
    }
}

// =============================================================================
//  Dialogue
// =============================================================================
static void test_dialogue() {
    section("dialogue");
    Rng rng;
    const Stage stages[4] = { Stage::Youth, Stage::Adult, Stage::Old, Stage::Final };
    for (Stage st : stages) {
        CHECK(selectLine(st, 1, rng) != nullptr);         // first life
        CHECK(selectLine(st, 5, rng) != nullptr);         // late lives
        CHECK(selectComplaint(st, 4, rng) != nullptr);    // nagging
    }
    CHECK(milestoneLine(5) != nullptr);
    CHECK(milestoneLine(100) != nullptr);
    CHECK(milestoneLine(7) == nullptr);                   // non-milestone
    CHECK(lines::walkLeaveCount >= 3);

    // The ball should be genuinely chatty: sample each stage and count the
    // distinct lines it can reach by the sixth life.
    for (Stage st : stages) {
        const char* seen[96];
        int n = 0;
        for (int i = 0; i < 800; ++i) {
            const char* l = selectLine(st, 6, rng);
            if (l == nullptr) continue;
            bool dup = false;
            for (int k = 0; k < n; ++k) if (seen[k] == l) dup = true;
            if (!dup && n < 96) seen[n++] = l;
        }
        const char* cs[96];
        int cn = 0;
        for (int i = 0; i < 800; ++i) {
            const char* l = selectComplaint(st, 6, rng);
            if (l == nullptr) continue;
            bool dup = false;
            for (int k = 0; k < cn; ++k) if (cs[k] == l) dup = true;
            if (!dup && cn < 96) cs[cn++] = l;
        }
        std::printf("  stage %d: %d ambient, %d nag lines\n", static_cast<int>(st), n, cn);
        CHECK(n >= 10);     // plenty of variety per life stage
        CHECK(cn >= 2);     // and something to complain with
    }
}

// =============================================================================
//  Palette
// =============================================================================
static void test_palette() {
    section("palette");
    const Palette warmTop = climbPalette(0.0f);
    const Palette coldTop = climbPalette(1.0f);
    const bool changed = warmTop.hillNear.r != coldTop.hillNear.r ||
                         warmTop.hillNear.g != coldTop.hillNear.g ||
                         warmTop.hillNear.b != coldTop.hillNear.b;
    CHECK(changed);   // the hill genuinely cools off on the way up

    const Palette mid = climbPalette(0.5f);
    CHECK(mid.hillNear.g <= std::max(warmTop.hillNear.g, coldTop.hillNear.g));
    CHECK(mid.hillNear.g >= std::min(warmTop.hillNear.g, coldTop.hillNear.g));

    const RGB x = lerpRGB(rgb(0, 0, 0), rgb(100, 200, 255), 0.5f);
    CHECK(x.r == 50 && x.g == 100 && x.b == 127);
}

// =============================================================================
//  Game: full state machine through several incarnations
// =============================================================================
enum class ChoicePolicy { Push, Walk };

struct GameRun {
    int  choiceCount = 0;
    int  firstChoiceIncarnation = 0;
    int  lastChoiceIncarnation = 0;
    int  livesCompleted = 0;
    bool reachedCredits = false;
    bool sawFarewell = false;   // the ball panicking as you leave
    bool sawReGreeting = false; // ...then instantly cheerful again
};

// Drives the whole game with an auto-player. With the Push policy it stops once
// `stopAfterChoices` choices have been offered; with the Walk policy it plays
// through to the credits.
static GameRun runGame(ChoicePolicy policy, int stopAfterChoices, int maxFrames = 60 * 6000) {
    Game g;
    GameRun r;
    const int pushEvery = 10;   // ~6 presses per second
    bool resting = false;
    for (int frame = 0; frame < maxFrames; ++frame) {
        Input in;
        switch (g.phase()) {
            case Phase::Title:
                in.anyPressed = true;
                in.pushPressed = true;
                break;
            case Phase::Climb: {
                const SimState& s = g.sim().state();
                if (s.stamina < 0.15f) resting = true;
                if (s.stamina > 0.70f) resting = false;
                if (resting) {
                    in.braceHeld = true;
                } else if ((frame % pushEvery) == 0) {
                    in.pushPressed = true;
                    in.pushHeld = true;
                }
                in.anyPressed = true;
                break;
            }
            case Phase::Choice:
                // The two answers, expressed with exactly the inputs the phone's
                // touch bridge produces: a tap is a SPACE press edge, a hold is a
                // SHIFT press edge. So "Push" here is the phone's tap and "Walk"
                // is the phone's press-and-hold, which is why the panel is
                // labelled TAP and HOLD rather than SPACE and SHIFT.
                ++r.choiceCount;
                if (r.firstChoiceIncarnation == 0) r.firstChoiceIncarnation = g.incarnation();
                r.lastChoiceIncarnation = g.incarnation();
                if (policy == ChoicePolicy::Push) { in.pushPressed = true; in.pushHeld = true; }
                else                              { in.bracePressed = true; }
                in.anyPressed = true;
                break;
            case Phase::Credits:
                r.reachedCredits = true;
                r.livesCompleted = g.completedLives();
                return r;
            case Phase::WalkAway:
                if (g.currentLine() == lines::walkLeave[0]) r.sawFarewell = true;
                if (g.currentLine() == lines::walkGreeting) r.sawReGreeting = true;
                break;
            default:
                break;
        }
        g.update(in, FIXED_DT);
        if (policy == ChoicePolicy::Push && r.choiceCount >= stopAfterChoices) {
            r.livesCompleted = g.completedLives();
            return r;
        }
    }
    r.livesCompleted = g.completedLives();
    return r;
}

// =============================================================================
//  Game: the ball must ROLL, not slide. The reported bug was that its surface
//  rotation was only driven during the climb, so it slid down the whole hill
//  with the texture frozen. Rotation is now derived from distance travelled.
// =============================================================================
static void test_ball_rolls_while_running_back() {
    section("game: the ball rolls instead of sliding");

    Game g;
    bool resting = false;
    int frame = 0;
    while (g.phase() != Phase::RollDown && frame < 60 * 900) {
        Input in;
        if (g.phase() == Phase::Title) {
            in.anyPressed = true;
            in.pushPressed = true;
        } else if (g.phase() == Phase::Climb) {
            const SimState& s = g.sim().state();
            if (s.stamina < 0.15f) resting = true;
            if (s.stamina > 0.70f) resting = false;
            if (resting)                  in.braceHeld = true;
            else if (frame % 10 == 0)   { in.pushPressed = true; in.pushHeld = true; }
            in.anyPressed = true;
        }
        g.update(in, FIXED_DT);
        ++frame;
    }
    CHECK(g.phase() == Phase::RollDown);

    // Let it run home and watch the rotation.
    const float spin0 = g.ballSpin();
    const float prog0 = g.ballProgress();
    for (int i = 0; i < 30; ++i) {
        Input in;                       // no input: gravity is doing the work
        g.update(in, FIXED_DT);
    }
    const float dSpin = g.ballSpin() - spin0;
    const float dProg = g.ballProgress() - prog0;

    std::printf("  rolled %.1f deg while travelling %.3f of the hill\n",
                static_cast<double>(dSpin), static_cast<double>(dProg));
    CHECK(dProg < 0.0f);                                            // went downhill
    CHECK(dSpin < -1.0f);                                           // and it TURNED
    CHECK(std::fabs(dSpin - dProg * ROLL_DEG_PER_PROGRESS) < 1.0f);  // true rolling
}

static void test_game_walk_away() {
    section("game: walk-away ending");
    const GameRun r = runGame(ChoicePolicy::Walk, 1);
    std::printf("  %d lives spent -> choice at incarnation %d -> walked away\n",
                CHOICE_AFTER_LIVES, r.firstChoiceIncarnation);
    CHECK(r.choiceCount == 1);
    CHECK(r.firstChoiceIncarnation == CHOICE_AFTER_LIVES + 1);   // the 4th life
    CHECK(r.livesCompleted == CHOICE_AFTER_LIVES);              // exactly three died first
    CHECK(r.reachedCredits);

    // The darker turn: the ball panics as you leave, then immediately puts its
    // cheerful salesman face back on for whoever is next.
    CHECK(r.sawFarewell);
    CHECK(r.sawReGreeting);
}

static void test_game_keep_pushing() {
    section("game: keep pushing re-offers the choice");
    const GameRun r = runGame(ChoicePolicy::Push, 2);
    std::printf("  choices seen: %d, lives spent: %d, last choice at incarnation %d\n",
                r.choiceCount, r.livesCompleted, r.lastChoiceIncarnation);
    CHECK(r.choiceCount >= 2);
    CHECK(r.firstChoiceIncarnation == CHOICE_AFTER_LIVES + 1);
    CHECK(r.lastChoiceIncarnation >= CHOICE_AFTER_LIVES + 2);
    CHECK(r.livesCompleted >= CHOICE_AFTER_LIVES + 1);
}

// =============================================================================
//  Music: the climb theme wears down, and never recovers
// =============================================================================
static void test_arrangement_decays() {
    section("music: arrangement wears down with incarnations");

    const core::Arrangement fresh = core::arrangementFor(0);
    CHECK(fresh.tone == 1.0f);
    CHECK(fresh.bits == 16);
    CHECK(fresh.hold == 1);
    CHECK(fresh.detune == 0.0f);

    // Every field moves one way and only one way. "Thinner and more
    // mechanical" has to be monotonic or it reads as a glitch, not as decay.
    for (int life = 1; life <= core::ARRANGEMENT_SPENT_LIVES + 4; ++life) {
        const core::Arrangement prev = core::arrangementFor(life - 1);
        const core::Arrangement cur  = core::arrangementFor(life);
        CHECK(cur.tone   <= prev.tone);
        CHECK(cur.bits   <= prev.bits);
        CHECK(cur.hold   >= prev.hold);
        CHECK(cur.gain   <= prev.gain);
        CHECK(cur.detune <= prev.detune);
    }

    const core::Arrangement spent = core::arrangementFor(core::ARRANGEMENT_SPENT_LIVES);
    std::printf("  life 0      : tone %.2f  %2d-bit  hold %d  detune %+.0f cents\n",
                fresh.tone, fresh.bits, fresh.hold, fresh.detune);
    std::printf("  life %2d     : tone %.2f  %2d-bit  hold %d  detune %+.0f cents\n",
                core::ARRANGEMENT_SPENT_LIVES, spent.tone, spent.bits, spent.hold, spent.detune);

    // It has to actually arrive somewhere audible, not just twitch.
    CHECK(spent.tone < fresh.tone * 0.5f);
    CHECK(spent.bits <= 8);
    CHECK(spent.hold >= 3);

    // And then it holds there: degrading forever would stop meaning anything.
    const core::Arrangement beyond = core::arrangementFor(core::ARRANGEMENT_SPENT_LIVES + 50);
    CHECK(beyond.tone == spent.tone);
    CHECK(beyond.bits == spent.bits);
    CHECK(beyond.hold == spent.hold);
    CHECK(beyond.detune == spent.detune);
}

// =============================================================================
//  Game: the one-time control hint retires itself
// =============================================================================
static void test_tutorial_retires() {
    Game g;
    CHECK(!g.tutorialDone());          // nothing shown before the first life

    // Leave the title with a push: the title frame dispatches to the title
    // case, so this press starts the climb without counting as a push.
    Input start;
    start.anyPressed  = true;
    start.pushPressed = true;
    g.update(start, FIXED_DT);
    CHECK(g.phase() == Phase::Climb);
    CHECK(!g.tutorialDone());

    // A push alone is not enough; the hint teaches two things.
    Input push;
    push.pushPressed = true;
    g.update(push, FIXED_DT);
    CHECK(!g.tutorialDone());

    // Neither is a brace on its own.
    Game h;
    Input go;
    go.anyPressed = true;              // leave the title WITHOUT pushing
    h.update(go, FIXED_DT);
    CHECK(h.phase() == Phase::Climb);
    Input brace;
    brace.braceHeld = true;
    h.update(brace, FIXED_DT);
    CHECK(!h.tutorialDone());

    // One push and one brace and it is done for good.
    g.update(brace, FIXED_DT);
    CHECK(g.tutorialDone());
}

// =============================================================================
//  Audio: the effects are textures, not DC
//
//  The effects used to be built from a xorshift re-seeded from the sample time
//  and advanced exactly once per sample. That is not a noise source: it made the
//  "scrape" a ramp from zero, the "rattle" the same value on every sample and
//  the "roll" a step that ended on a discontinuity -- a set of clicks, pops and
//  buzzes, which is what "the sound effects are static" sounds like. The
//  wave helpers had the same kind of bug: handed `f * t`, they compared it
//  against 0.5, so they returned a constant as well and the summit fanfare was a
//  DC pulse.
//
//  Every one of those failures is measurable on the rendered PCM, so that is
//  where it is measured: this suite runs with no audio device, and these are
//  exactly the properties a listener would describe as "static".
// =============================================================================
static void test_audio_voices() {
    section("audio: effect voices");
    namespace v = cashyphus::audio::voices;

    struct Expect {
        v::Voice    voice;
        const char* name;
        // True when the whole character of the voice is a noise texture, which
        // also forces its motion to be textural rather than a ramp. "roll" is a
        // low rumble with gravel in it, not a noise texture, so it is exempt --
        // its DC, step and tail checks still catch the old discontinuity.
        bool        textured;
        float       rmsMin;
    };
    const Expect table[] = {
        { v::Voice::Push,     "push",     false, 0.050f },
        { v::Voice::Scrape,   "scrape",   true,  0.010f },
        { v::Voice::Rattle,   "rattle",   true,  0.020f },
        { v::Voice::Breath,   "breath",   true,  0.006f },
        { v::Voice::Jingle,   "jingle",   false, 0.080f },
        { v::Voice::Bell,     "bell",     false, 0.040f },
        { v::Voice::Collapse, "collapse", false, 0.060f },
        { v::Voice::Roll,     "roll",     false, 0.040f },
        { v::Voice::Birds,    "birds",    false, 0.040f },
    };

    for (const Expect& e : table) {
        const int n = static_cast<int>(v::seconds(e.voice) * v::RATE);
        CHECK(n > 0);

        float peak = 0.0f, sum = 0.0f, sumSq = 0.0f, sumDelta = 0.0f;
        float first = 0.0f, last = 0.0f, prev = 0.0f;
        bool  finite = true;
        for (int i = 0; i < n; ++i) {
            const float s = v::sample(e.voice, i);
            if (!std::isfinite(s)) finite = false;
            if (i == 0) first = s;
            if (i > 0) sumDelta += std::fabs(s - prev);
            prev  = s;
            last  = s;
            peak  = std::max(peak, std::fabs(s));
            sum  += s;
            sumSq += s * s;
        }
        const float rms  = std::sqrt(sumSq / n);
        const float dc   = sum / n;
        const float step = sumDelta / (n - 1);

        std::printf("  %-8s peak %.3f  rms %.3f  dc %+.4f  step %.5f  step/rms %.3f\n",
                    e.name, peak, rms, dc, step, step / rms);

        CHECK(finite);
        CHECK(peak <= 0.80f);              // headroom: the mixer has no limiter
        CHECK(peak >= 0.02f);              // ...but it is not silence either
        CHECK(rms >= e.rmsMin);
        CHECK(std::fabs(dc) <= 0.05f);     // no DC offset to step the cone with
        CHECK(std::fabs(first) <= 0.03f);  // fades in from silence...
        CHECK(std::fabs(last) <= 0.03f);   // ...and back out, so it never clicks
        CHECK(step > 0.0005f);             // it moves: not a constant in an envelope
        if (e.textured) CHECK(step / rms > 0.08f);   // and the motion is texture, not a ramp
    }

    // The one thing the whole fix turns on: the helpers wrap a large phase.
    CHECK(std::fabs(v::square(3.25f) - 1.0f) < 1e-6f);
    CHECK(std::fabs(v::square(3.75f) + 1.0f) < 1e-6f);
    CHECK(std::fabs(v::tri(3.5f) + 1.0f) < 1e-6f);
}

// =============================================================================
//  Layout: the frame fills the display
// =============================================================================
// The game draws a portrait 1:2 playfield. On a 2.16:1 phone a fixed frame
// letterboxes, and on a 1170x2532 screen that threw away 15% of the picture -- the
// frame is grown to the display instead. These are the numbers that decide it, so
// they are checked here rather than eyeballed on whatever monitor is to hand.
static void test_frame_fit() {
    section("layout: the frame fills the display");

    // Phones: the frame must fill the screen exactly, with no padding at all.
    const struct { int w, h; } phones[] = {
        { 1170, 2532 }, { 1080, 2400 }, { 720, 1560 }, { 1440, 3120 },
    };
    for (const auto& s : phones) {
        const FrameFit f = frameFor(s.w, s.h);
        CHECK(f.width * f.scale == s.w);
        CHECK(f.height * f.scale == s.h);
        CHECK(f.width >= DESIGN_W && f.height >= DESIGN_H);   // never smaller than the art
        CHECK(f.height > f.width);                            // never landscape
        CHECK(f.width <= DESIGN_W * 6 / 5);                   // never too wide for the layout
    }

    // Desktop monitors: a portrait playfield cannot fill a 16:9 screen, so the sides
    // are what gets given up -- but the height is taken in full, which is exactly
    // what the old integer-scale letterbox wasted.
    const struct { int w, h; } desktops[] = {
        { 1440, 900 }, { 1920, 1080 }, { 2560, 1440 },
    };
    for (const auto& s : desktops) {
        const FrameFit f = frameFor(s.w, s.h);
        CHECK(f.height * f.scale == s.h);
        CHECK(f.width == DESIGN_W);
        CHECK(f.width * f.scale <= s.w);
        CHECK(f.height > f.width);
    }

    // A display that is already 1:2 gets the design frame untouched, which is what
    // makes the whole change a no-op on the art it was drawn for: at that size the
    // world is translated by nothing.
    const FrameFit d = frameFor(DESIGN_W * 2, DESIGN_H * 2);
    CHECK(d.scale == 2 && d.width == DESIGN_W && d.height == DESIGN_H);
    CHECK(frameTop() == 0);

    // Degenerate sizes must not produce a frame of zero, or a negative one.
    const FrameFit one = frameFor(1, 1);
    CHECK(one.scale >= 1 && one.width >= 1 && one.height >= 1);
    const FrameFit none = frameFor(0, 0);
    CHECK(none.scale >= 1 && none.width >= 1 && none.height >= 1);
}

int main() {
    std::printf("CASHYPHUS core tests\n====================\n");
    test_stages_and_aging();
    test_push_and_stamina();
    test_push_rate_is_capped();
    test_gravity_rollback();
    test_aging_never_reverses();
    test_brace_grip_and_slip();
    test_auto_climb_is_completable();
    test_mashing_is_not_a_strategy();
    test_later_lives_are_heavier();
    test_aging_degrades_stamina_and_grip();
    test_old_age_can_still_climb();
    test_frailty_is_meaningful_but_survivable();
    test_every_age_can_still_rest();
    test_ball_roll_direction();
    test_dialogue();
    test_palette();
    test_ball_rolls_while_running_back();
    test_game_walk_away();
    test_game_keep_pushing();
    test_tutorial_retires();
    test_arrangement_decays();
    test_audio_voices();
    test_frame_fit();

    std::printf("\n%d/%d checks passed\n", g_checks - g_fail, g_checks);
    return g_fail == 0 ? 0 : 1;
}
