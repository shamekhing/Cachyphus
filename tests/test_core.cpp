// Headless unit tests for the raylib-free core of CASHYPHUS.
// Run with:  ctest --test-dir build --output-on-failure

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/dialogue.hpp"
#include "core/game_state.hpp"
#include "core/palette.hpp"
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

    in.push = true;
    for (int i = 0; i < 10; ++i) s.step(in, FIXED_DT);
    CHECK(s.state().stamina < 0.9f);
    CHECK(s.state().progress > 0.0f);

    // Stamina never goes out of range no matter how hard we mash.
    in.push = true;
    for (int i = 0; i < 600; ++i) s.step(in, FIXED_DT);
    CHECK(s.state().stamina >= 0.0f);
    CHECK(s.state().stamina <= 1.0f);
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

static void test_game_walk_away() {
    section("game: walk-away ending");
    const GameRun r = runGame(ChoicePolicy::Walk, 1);
    std::printf("  4 lives spent -> choice at incarnation %d -> walked away\n",
                r.firstChoiceIncarnation);
    CHECK(r.choiceCount == 1);
    CHECK(r.firstChoiceIncarnation == CHOICE_AFTER_LIVES + 1);   // the 5th life
    CHECK(r.livesCompleted == CHOICE_AFTER_LIVES);              // exactly four died first
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

int main() {
    std::printf("CASHYPHUS core tests\n====================\n");
    test_stages_and_aging();
    test_push_and_stamina();
    test_gravity_rollback();
    test_aging_never_reverses();
    test_brace_grip_and_slip();
    test_auto_climb_is_completable();
    test_mashing_is_not_a_strategy();
    test_later_lives_are_heavier();
    test_dialogue();
    test_palette();
    test_game_walk_away();
    test_game_keep_pushing();

    std::printf("\n%d/%d checks passed\n", g_checks - g_fail, g_checks);
    return g_fail == 0 ? 0 : 1;
}
