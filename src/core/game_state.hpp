#pragma once

#include "core/dialogue.hpp"
#include "core/sim.hpp"

namespace cashyphus {

// Top-level narrative states. The whole game is a loop through these.
enum class Phase {
    Title,
    Climb,
    Collapse,   // character dies at the summit
    Celebrate,  // the ball celebrates
    Silence,    // one beat of nothing
    RollDown,   // the ball rolls back to the bottom
    WalkIn,     // a new person approaches
    Choice,     // SPACE keep pushing / SHIFT walk away
    WalkAway,   // the escape sequence
    Credits,
};

// What the human character is doing right now (drives which sprite is drawn).
enum class CharState { None, Idle, Pushing, Bracing, Collapsing, Dead, Walking, Waiting };

// The ball's animated expression.
enum class BallMood {
    Happy, Smug, Anxious, Panic, Desperate, Celebrating, Heartbroken,
};

struct Input {
    bool pushPressed  = false;  // SPACE edge
    bool pushHeld     = false;
    bool bracePressed = false;  // SHIFT edge
    bool braceHeld    = false;
    bool anyPressed   = false;
};

// The entire game simulation. ray-lib free so it can be unit tested headlessly.
class Game {
public:
    Game() = default;

    // Advance exactly one fixed step of dt seconds.
    void update(const Input& in, float dt);

    // --- renderer-facing accessors -------------------------------------------
    Phase   phase() const { return phase_; }
    float   phaseTime() const { return phaseTime_; }

    int     incarnation() const { return incarnation_; }
    int     completedLives() const { return completedLives_; }
    int     totalPushes() const { return totalPushes_; }
    int     timesContinued() const { return timesContinued_; }
    bool    choiceUnlocked() const { return choiceUnlocked_; }
    bool    tutorialDone() const { return tutorialDone_; }

    const Sim& sim() const { return sim_; }
    Stage      stage() const { return sim_.stage(); }
    CharState  character() const { return charPose_; }
    BallMood   mood() const { return mood_; }
    float      strain() const { return strain_; }

    float   ballProgress() const;   // where the ball sits (0..1) for rendering
    float   ballSpin() const { return ballSpin_; }
    float   deathT() const { return deathT_; }      // 0..1 collapse progress
    float   walkT() const { return walkT_; }        // 0..1 walk-away progress

    // --- dialogue -------------------------------------------------------------
    const char* currentLine() const { return currentLine_; }
    float       lineAge() const { return lineAge_; }
    bool        lineIsComplaint() const { return lineComplaint_; }

private:
    void startLife();
    void enterPhase(Phase p);
    void setLine(const char* text, bool complaint);
    void updateDialogue(const Input& in, float dt);
    void updateMoodAndPose(const Input& in);

    Phase phase_ = Phase::Title;
    float phaseTime_ = 0.0f;

    int   incarnation_   = 1;
    int   completedLives_ = 0;
    int   totalPushes_   = 0;
    int   timesContinued_ = 0;
    bool  choiceUnlocked_ = false;

    // The first-time control hint is retired the moment the player has both
    // pushed and braced at least once, and only ever comes back if the game is
    // restarted from the title.
    bool  pushedYet_    = false;
    bool  bracedYet_    = false;
    bool  tutorialDone_ = false;

    Sim   sim_;
    Rng   rng_{ 0x00C0FFEEu };

    CharState charPose_ = CharState::None;
    BallMood  mood_     = BallMood::Happy;
    float     strain_   = 0.0f;

    float timeSincePush_ = 0.0f;
    float lineTimer_     = 0.0f;
    float lineAge_       = 0.0f;
    float ballSpin_      = 0.0f;   // degrees; driven by distance travelled
    float lastProgress_  = 0.0f;   // last frame's ball position, for the roll
    bool  lineComplaint_ = false;

    float deathT_ = 0.0f;
    float walkT_  = 0.0f;
    int   walkLineIndex_ = 0;
    float walkLineTimer_ = 0.0f;

    const char* currentLine_ = nullptr;
};

} // namespace cashyphus
