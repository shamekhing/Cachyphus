#pragma once

#include <algorithm>

// =============================================================================
//  CASHYPHUS - central tuning table
//  Every gameplay "magic number" lives here so the feel can be balanced in one
//  place. Values are expressed in *progress units* (0..1 = bottom..summit) and
//  seconds, never in pixels.
// =============================================================================

namespace cashyphus::cfg {

// --- Virtual resolution (portrait: the game is built for a phone) ------------
// The brief recommends 320x180, but its own interface sketch draws the hill
// rising to a summit at the TOP of the frame -- a vertical composition. That is
// what this is: the same pixel budget, rotated, so the climb reads as climbing.
// 180x360 is exactly 1:2, so 2x is 360x720 and modern phones letterbox by
// only a few percent.
constexpr int   VIRTUAL_W = 180;
constexpr int   VIRTUAL_H = 360;

// --- Fixed simulation step ----------------------------------------------------
constexpr float FIXED_DT  = 1.0f / 60.0f;
constexpr float MAX_FRAME = 0.25f;   // clamp huge hiccups

// --- Hill shape: slope(p) = SLOPE_BASE + SLOPE_GROWTH * p ----------------------
constexpr float SLOPE_BASE   = 1.0f;
constexpr float SLOPE_GROWTH = 0.50f;

// --- Ball dynamics ------------------------------------------------------------
constexpr float GRAVITY   = 0.012f;  // downhill accel (progress/s^2) * slope
constexpr float DAMPING   = 0.90f;   // rolling friction (1/s)
constexpr float MAX_SPEED = 0.150f;  // progress/s clamp

// --- Push (SPACE) -------------------------------------------------------------
// An exhausted shove is nearly worthless: stamina is what actually buys
// progress, so mashing the key on an empty tank cannot beat the hill.
constexpr float PUSH_IMPULSE       = 0.0270f; // progress/s added per press
constexpr float PUSH_COST          = 0.0600f; // stamina spent per press
constexpr float STAMINA_MIN_FACTOR = 0.05f;   // push strength floor at 0 stamina
constexpr float PUSH_COOLDOWN      = 0.13f;   // min gap between accepted pushes (~7.7/s)

// --- Stamina regeneration -----------------------------------------------------
// These are the YOUTH rates; frailty below scales them down with age. Keep the
// base honest -- raising it to paper over a slow climb also buffs the old body
// and hides the whole point of aging.
constexpr float STAMINA_REGEN_BRACE = 0.70f;  // 1/s while bracing
constexpr float STAMINA_REGEN_IDLE  = 0.10f;  // 1/s while not bracing

// --- Brace (SHIFT) ------------------------------------------------------------
// Grip is what stops you resting forever. Note the interaction with age: a
// slower stamina refill means a LONGER brace, and a longer brace is exactly
// what burns grip -- so the two age penalties multiply. The base economy is
// therefore kept a little generous so that an old character can still hold on
// long enough to recover; otherwise the rest cycle is unsustainable and the
// climb becomes impossible.
constexpr float BRACE_ACCEL            = 0.35f; // progress/s^2 opposing rollback
constexpr float GRIP_DRAIN             = 0.26f; // 1/s while bracing   (~3.8s hold)
constexpr float GRIP_REGEN             = 0.62f; // 1/s while not bracing
constexpr float GRIP_RECOVER_THRESHOLD = 0.25f; // hysteresis after a slip

// --- Aging --------------------------------------------------------------------
constexpr float STAGE_YOUTH_END = 0.25f;
constexpr float STAGE_ADULT_END = 0.50f;
constexpr float STAGE_OLD_END   = 0.75f;
constexpr int   AGE_START       = 18;
constexpr int   AGE_END         = 80;

// How much raw shove the character can still put out (Youth..Final).
constexpr float AGE_FACTOR[4] = { 1.00f, 0.96f, 0.92f, 0.86f };

// Frailty: aging also wears down the body's *efficiency*, not just its power.
// Stamina efficiency scales how fast the tank refills (and divides how much
// each push costs); grip efficiency scales how fast grip recovers (and divides
// how long the ball can be held).
//
// By the final years, compared with youth, a shove costs ~72% more stamina,
// the tank refills ~42% slower, grip drains ~39% faster and recovers ~28%
// slower. The rhythm goes from long bursts and short holds to short bursts
// that need long holds, which is what "older and weaker" has to feel like.
//
// The grip curve is deliberately gentler than the stamina curve. Making BOTH
// steep is not harder in a satisfying way, it is broken: see the note on grip
// above. These values were chosen against measurement, not by eye.
constexpr float AGE_STAMINA_EFF[4] = { 1.00f, 0.86f, 0.72f, 0.58f };
constexpr float AGE_GRIP_EFF[4]    = { 1.00f, 0.90f, 0.80f, 0.72f };

// --- World geometry (one source of truth for physics and rendering) ----------
constexpr float HILL_LEN    = 900.0f;  // world x of the summit, in pixels
constexpr float BALL_RADIUS = 26.0f;   // the money ball's radius, in pixels

// Rolling without slipping: a ball turns distance/radius radians, so a full
// 0..1 trip along the hill is this many degrees. The renderer rotates the
// ball's surface by exactly this much, which is what makes it roll instead of
// slide -- including while it runs back down.
constexpr float ROLL_DEG_PER_PROGRESS = (HILL_LEN / BALL_RADIUS) * 57.29577951f;

// --- Per-cycle difficulty -----------------------------------------------------
// The hill gets a little crueller every lifetime: the ball pulls back harder and
// faster, so later incarnations demand a tighter push/brace rhythm.
constexpr float CYCLE_GRAVITY_STEP = 0.080f; // +8% downhill pull per life
constexpr int   CYCLE_GRAVITY_CAP  = 5;      // ...up to +40% from the 6th life on

// --- Progression / narrative --------------------------------------------------
constexpr int   CHOICE_AFTER_LIVES  = 4;    // choice appears once 4 lives are spent
constexpr float IDLE_COMPLAINT_AFTER = 2.5f; // ball nags after this long without a push
constexpr float LINE_MIN_TIME        = 3.5f; // min spacing between ambient lines

// --- Phase durations (seconds) ------------------------------------------------
constexpr float DUR_COLLAPSE  = 2.0f;
constexpr float DUR_CELEBRATE = 1.6f;
constexpr float DUR_SILENCE   = 1.2f;
constexpr float DUR_ROLLDOWN  = 2.0f;
constexpr float DUR_WALKIN    = 1.5f;
constexpr float DUR_WALKAWAY  = 6.0f;
constexpr float DUR_WALK_LINE = 1.2f;   // spacing of the ball's panicked lines

// --- Small helpers ------------------------------------------------------------
inline float clampf(float v, float lo, float hi) {
    return std::min(hi, std::max(lo, v));
}
inline float approachf(float cur, float target, float maxDelta) {
    if (cur < target) return std::min(cur + maxDelta, target);
    return std::max(cur - maxDelta, target);
}
inline float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

} // namespace cashyphus::cfg
