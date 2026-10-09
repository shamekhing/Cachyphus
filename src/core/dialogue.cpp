#include "core/dialogue.hpp"

#include <cstddef>

namespace cashyphus {

namespace {

struct Line {
    int         minLife;   // earliest incarnation this line may appear (1 = first life)
    Stage       stage;
    const char* text;
};

// --- Ambient lines, banded by life stage and incarnation ----------------------
const Line kAmbient[] = {
    // Youth
    { 1, Stage::Youth, "Hello, future millionaire!" },
    { 1, Stage::Youth, "Push me! Great things await!" },
    { 1, Stage::Youth, "You're going places, kid!" },
    { 1, Stage::Youth, "Just a bit further." },
    { 1, Stage::Youth, "Think of what you'll have at the top." },
    { 1, Stage::Youth, "This is worth it." },
    { 1, Stage::Youth, "You're not like the others." },
    { 1, Stage::Youth, "We'll be rich by lunchtime." },
    { 1, Stage::Youth, "Money doesn't grow on trees. That's why you push me uphill!" },
    { 2, Stage::Youth, "Ah! A fresh start!" },
    { 2, Stage::Youth, "You look stronger than the last one!" },
    { 2, Stage::Youth, "Fresh legs. I love fresh legs." },
    { 3, Stage::Youth, "Don't look at the others. Look at me." },
    { 5, Stage::Youth, "Another one. They keep coming." },
    { 8, Stage::Youth, "You have such a promising face." },

    // Adulthood
    { 1, Stage::Adult, "See? Hard work pays off!" },
    { 1, Stage::Adult, "A little more effort never hurt anyone." },
    { 1, Stage::Adult, "Don't slow down. Someone else might get ahead." },
    { 1, Stage::Adult, "Work-life balance? Never heard of her." },
    { 1, Stage::Adult, "You've already come this far." },
    { 1, Stage::Adult, "Don't waste all that effort now." },
    { 1, Stage::Adult, "You can rest later." },
    { 1, Stage::Adult, "If you stop, it was all for nothing." },
    { 1, Stage::Adult, "Compound interest, kid. Look it up." },
    { 2, Stage::Adult, "We're basically entrepreneurs." },
    { 2, Stage::Adult, "Nobody ever got anywhere by resting." },
    { 3, Stage::Adult, "The last one understood me. Eventually." },
    { 4, Stage::Adult, "What else would you even do?" },
    { 6, Stage::Adult, "At this rate we'll be legends." },
    { 8, Stage::Adult, "I've been calling this a startup for a century." },
    { 10, Stage::Adult, "The hill is not the point. I'm the point." },

    // Old age
    { 1, Stage::Old, "You've come too far to stop now." },
    { 1, Stage::Old, "Rest? You can rest when we're rich." },
    { 1, Stage::Old, "Imagine how proud everyone will be!" },
    { 1, Stage::Old, "You're not exhausted. You're experiencing growth!" },
    { 1, Stage::Old, "Your life has meaning because of me." },
    { 1, Stage::Old, "Pokey little hill. Easy money." },
    { 1, Stage::Old, "You're one big break away." },
    { 1, Stage::Old, "Everyone else is pushing too." },
    { 2, Stage::Old, "We're like a family!" },
    { 3, Stage::Old, "They just didn't appreciate the journey." },
    { 4, Stage::Old, "Everyone pushes something." },
    { 4, Stage::Old, "Fifty percent of the way to something. Probably." },
    { 6, Stage::Old, "Persistence. That's the whole trick." },
    { 8, Stage::Old, "We'll retire any day now." },
    { 12, Stage::Old, "You're doing better than most. Possibly." },

    // Final years
    { 1, Stage::Final, "Almost there!" },
    { 1, Stage::Final, "One more push!" },
    { 1, Stage::Final, "Push." },
    { 1, Stage::Final, "You wouldn't give up now, would you?" },
    { 1, Stage::Final, "Have you tried pushing harder?" },
    { 1, Stage::Final, "Without me, what are you?" },
    { 1, Stage::Final, "Don't stop now. We're so close." },
    { 1, Stage::Final, "You were nothing before me." },
    { 3, Stage::Final, "You're different." },
    { 4, Stage::Final, "You're not thinking of quitting, are you?" },
    { 4, Stage::Final, "You'll do better than the others. You always say that." },
    { 6, Stage::Final, "I don't remember their names either." },
    { 8, Stage::Final, "Just push." },
    { 12, Stage::Final, "We've come so far. Let's go further." },
};

// --- Complaints (the player stopped pushing) ----------------------------------
const Line kComplaints[] = {
    { 1, Stage::Youth, "Why'd you stop?" },
    { 1, Stage::Youth, "Have you tried pushing harder?" },
    { 1, Stage::Youth, "Are you tired? I'm not tired." },
    { 1, Stage::Adult, "This is a team effort!" },
    { 1, Stage::Adult, "Don't let me roll back!" },
    { 1, Stage::Adult, "I can feel you judging me." },
    { 1, Stage::Old, "We're so close. Keep pushing!" },
    { 1, Stage::Old, "Don't you want to be somebody?" },
    { 1, Stage::Old, "Rest is for people without dreams." },
    { 1, Stage::Final, "Push. Please." },
    { 1, Stage::Final, "Don't think. Push." },
    { 2, Stage::Adult, "We had a deal." },
    { 3, Stage::Final, "You need me. You know that, right?" },
    { 5, Stage::Final, "Hello? Hello?!" },
    { 3, Stage::Old, "You're losing money standing there." },
    { 6, Stage::Youth, "Don't look at me like that." },
    { 8, Stage::Old, "You're the only one who understands me." },
    { 12, Stage::Final, "There's no one else. Just you." },
};

template <std::size_t N>
const char* pick(const Line (&table)[N], Stage stage, int life, Rng& rng, const char* avoid) {
    const char* candidates[N];
    int count = 0;
    for (std::size_t i = 0; i < N; ++i) {
        if (table[i].stage == stage && life >= table[i].minLife) {
            candidates[count++] = table[i].text;
        }
    }
    if (count == 0) {
        // Fall back to any line valid for this life.
        for (std::size_t i = 0; i < N; ++i) {
            if (life >= table[i].minLife) candidates[count++] = table[i].text;
        }
    }
    if (count == 0) return nullptr;

    const char* chosen = candidates[rng.range(count)];
    if (chosen == avoid && count > 1) {
        chosen = candidates[(rng.range(count - 1) + 1) % count];
    }
    return chosen;
}

const struct Milestone { int lives; const char* text; } kMilestones[] = {
    { 5,   "You're very committed!" },
    { 10,  "Look at everything we've accomplished!" },
    { 15,  "We're making real progress. Probably." },
    { 20,  "Do you remember why we started?" },
    { 30,  "Is this still fun for you?" },
    { 50,  "Surely this is enough... right?" },
    { 75,  "You could have learned an instrument." },
    { 100, "Please don't leave me." },
};

} // namespace

const char* selectLine(Stage stage, int life, Rng& rng, const char* avoid) {
    return pick(kAmbient, stage, life, rng, avoid);
}

const char* selectComplaint(Stage stage, int life, Rng& rng, const char* avoid) {
    return pick(kComplaints, stage, life, rng, avoid);
}

const char* milestoneLine(int livesCompleted) {
    for (const auto& m : kMilestones) {
        if (livesCompleted == m.lives) return m.text;
    }
    return nullptr;
}

namespace lines {
const char* const summit      = "WE DID IT!";
const char* const silence     = "...Hello?";
const char* const choiceIntro = "Well, shall we?";
const char* const continue_ok = "That's the spirit!";

const char* const walkLeave[] = {
    "Wait.",
    "Where are you going?",
    "But we're almost rich!",
    "You can't just leave!",
    "What about everything we worked for?",
};
const int walkLeaveCount = static_cast<int>(sizeof(walkLeave) / sizeof(walkLeave[0]));

const char* const walkGreeting = "Hello, future millionaire!";
} // namespace lines

} // namespace cashyphus
