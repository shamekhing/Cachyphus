#include "core/game_state.hpp"

namespace cashyphus {

using namespace cfg;

// -----------------------------------------------------------------------------
//  helpers
// -----------------------------------------------------------------------------
void Game::setLine(const char* text, bool complaint) {
    currentLine_   = text;
    lineAge_       = 0.0f;
    lineComplaint_ = complaint;
}

void Game::enterPhase(Phase p) {
    phase_     = p;
    phaseTime_ = 0.0f;
}

void Game::startLife() {
    sim_.reset();
    sim_.setLife(incarnation_);   // later lives face a heavier ball
    timeSincePush_ = 0.0f;
    lineTimer_     = 0.0f;
    ballSpin_      = 0.0f;
    lastProgress_  = 0.0f;
    deathT_        = 0.0f;
    walkT_         = 0.0f;
    strain_        = 0.0f;
    setLine(selectLine(Stage::Youth, incarnation_, rng_), false);
    enterPhase(Phase::Climb);
}

float Game::ballProgress() const {
    switch (phase_) {
        case Phase::Climb:
        case Phase::Collapse:
        case Phase::Celebrate:
        case Phase::Silence:
            return sim_.state().progress;
        case Phase::RollDown:
            // gravity takes it home
            return clampf(1.0f - phaseTime_ / DUR_ROLLDOWN, 0.0f, 1.0f);
        default:
            return 0.0f;   // parked at the bottom
    }
}

// -----------------------------------------------------------------------------
//  dialogue pacing
// -----------------------------------------------------------------------------
void Game::updateDialogue(const Input& in, float dt) {
    lineTimer_ += dt;

    const bool nagging = timeSincePush_ >= IDLE_COMPLAINT_AFTER;

    if (nagging) {
        // Complain as soon as the player stops; refresh occasionally.
        if (lineComplaint_ == false || lineTimer_ >= LINE_MIN_TIME) {
            const char* l = selectComplaint(sim_.stage(), incarnation_, rng_, currentLine_);
            if (l) setLine(l, true);
            lineTimer_ = 0.0f;
        }
    } else if (lineTimer_ >= LINE_MIN_TIME) {
        const char* l = selectLine(sim_.stage(), incarnation_, rng_, currentLine_);
        if (l) setLine(l, false);
        lineTimer_ = 0.0f;
    }

    if (in.pushPressed) timeSincePush_ = 0.0f;
}

// -----------------------------------------------------------------------------
//  mood + character pose
// -----------------------------------------------------------------------------
void Game::updateMoodAndPose(const Input& in) {
    charPose_ = CharState::None;
    switch (phase_) {
        case Phase::Title:
            mood_ = BallMood::Happy;
            break;

        case Phase::Climb: {
            charPose_ = in.pushPressed ? CharState::Pushing
                      : in.braceHeld  ? CharState::Bracing
                                      : CharState::Idle;
            if (timeSincePush_ > IDLE_COMPLAINT_AFTER * 1.6f)      mood_ = BallMood::Desperate;
            else if (timeSincePush_ > IDLE_COMPLAINT_AFTER)        mood_ = BallMood::Anxious;
            else if (sim_.stage() == Stage::Youth)                 mood_ = BallMood::Happy;
            else if (sim_.stage() == Stage::Adult)                 mood_ = BallMood::Smug;
            else                                                   mood_ = BallMood::Anxious;
            break;
        }

        case Phase::Collapse:  charPose_ = CharState::Collapsing; mood_ = BallMood::Celebrating; break;
        case Phase::Celebrate: charPose_ = CharState::Dead;       mood_ = BallMood::Celebrating; break;
        case Phase::Silence:   charPose_ = CharState::Dead;       mood_ = BallMood::Anxious;     break;

        case Phase::RollDown:  charPose_ = CharState::None;       mood_ = BallMood::Desperate;   break;
        case Phase::WalkIn:    charPose_ = CharState::Walking;    mood_ = BallMood::Happy;       break;

        case Phase::Choice:    charPose_ = CharState::Waiting;    mood_ = BallMood::Smug;        break;

        case Phase::WalkAway:  charPose_ = CharState::Walking;
            // Panic, then wounded, then -- as it spots the next person --
            // instantly cheerful again.
            mood_ = walkT_ < 0.45f ? BallMood::Panic
                  : walkT_ < 0.85f ? BallMood::Heartbroken
                                   : BallMood::Happy;
            break;

        case Phase::Credits:   charPose_ = CharState::None;       mood_ = BallMood::Happy;       break;
    }
}

// -----------------------------------------------------------------------------
//  main fixed step
// -----------------------------------------------------------------------------
void Game::update(const Input& in, float dt) {
    phaseTime_ += dt;
    lineAge_   += dt;   // drives the typewriter reveal in every phase

    switch (phase_) {
        case Phase::Title:
            if (in.anyPressed) {
                incarnation_    = 1;
                completedLives_ = 0;
                totalPushes_    = 0;
                timesContinued_ = 0;
                choiceUnlocked_ = false;
                walkLineIndex_  = 0;
                startLife();
            }
            break;

        case Phase::Climb: {
            SimInput si;
            si.push   = in.pushPressed;
            si.brace  = in.braceHeld;
            si.active = true;
            if (in.pushPressed) ++totalPushes_;
            sim_.step(si, dt);
            updateDialogue(in, dt);
            if (sim_.reachedSummit()) {
                ++completedLives_;
                if (completedLives_ >= CHOICE_AFTER_LIVES) choiceUnlocked_ = true;
                setLine(lines::summit, false);
                enterPhase(Phase::Collapse);
            }
            break;
        }

        case Phase::Collapse:
            deathT_ = clampf(phaseTime_ / DUR_COLLAPSE, 0.0f, 1.0f);
            if (phaseTime_ >= DUR_COLLAPSE) {
                const char* m = milestoneLine(completedLives_);
                setLine(m ? m : lines::summit, false);
                enterPhase(Phase::Celebrate);
            }
            break;

        case Phase::Celebrate:
            if (phaseTime_ >= DUR_CELEBRATE) { setLine(lines::silence, false); enterPhase(Phase::Silence); }
            break;

        case Phase::Silence:
            if (phaseTime_ >= DUR_SILENCE) enterPhase(Phase::RollDown);
            break;

        case Phase::RollDown:
            if (phaseTime_ >= DUR_ROLLDOWN) {
                // The next person has not pushed anything yet: young, whole,
                // and entirely unaware of what the hill costs.
                sim_.reset();
                enterPhase(Phase::WalkIn);
            }
            break;

        case Phase::WalkIn:
            if (phaseTime_ >= DUR_WALKIN) {
                ++incarnation_;
                if (choiceUnlocked_) { setLine(lines::choiceIntro, false); enterPhase(Phase::Choice); }
                else                 startLife();
            }
            break;

        case Phase::Choice:
            if (in.pushPressed) {
                ++timesContinued_;
                setLine(lines::continue_ok, false);
                startLife();
            } else if (in.bracePressed) {
                walkLineIndex_ = 0;
                walkLineTimer_ = 0.0f;
                setLine(lines::walkLeave[0], false);
                enterPhase(Phase::WalkAway);
            }
            break;

        case Phase::WalkAway:
            walkT_ = clampf(phaseTime_ / DUR_WALKAWAY, 0.0f, 1.0f);
            walkLineTimer_ += dt;
            if (walkLineTimer_ >= DUR_WALK_LINE && walkLineIndex_ < lines::walkLeaveCount - 1) {
                ++walkLineIndex_;
                walkLineTimer_ = 0.0f;
                setLine(lines::walkLeave[walkLineIndex_], false);
            }
            // The twist: one person leaving changes nothing. The moment they
            // are out of earshot the ball is cheerful again, selling the same
            // promise to whoever comes next.
            if (walkT_ > 0.85f && walkLineIndex_ < lines::walkLeaveCount) {
                walkLineIndex_ = lines::walkLeaveCount;
                setLine(lines::walkGreeting, false);
            }
            if (phaseTime_ >= DUR_WALKAWAY) enterPhase(Phase::Credits);
            break;

        case Phase::Credits:
            if (in.anyPressed && phaseTime_ > 2.0f) {
                // Back to the hill, as if it never ended.
                enterPhase(Phase::Title);
            }
            break;
    }

    updateMoodAndPose(in);
    strain_ = sim_.state().strain;

    // Rolling, not sliding. The ball's surface rotation is derived from how far
    // it physically moved this frame (distance / radius), so it turns the right
    // amount while being pushed, while slipping backwards, and all the way back
    // down the hill. Measuring displacement rather than velocity also means the
    // roll stays correct while the ball is coasting or being held.
    //
    // Sign convention (see core/roll.hpp): positive spin is CLOCKWISE on
    // screen, which is what a ball rolling to the right -- i.e. uphill -- does.
    // Climbing therefore adds to the spin and running back down subtracts from
    // it, so both directions turn the right way.
    const float progress = ballProgress();
    ballSpin_ += (progress - lastProgress_) * ROLL_DEG_PER_PROGRESS;
    lastProgress_ = progress;
}

} // namespace cashyphus
