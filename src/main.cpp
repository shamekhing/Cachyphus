// =============================================================================
//  CASHYPHUS - entry point
//  One hill. One lifetime. One more push.
// =============================================================================
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#include "audio/synth.hpp"
#include "core/config.hpp"
#include "core/game_state.hpp"
#include "core/music.hpp"
#include "core/palette.hpp"
#include "raylib.h"
#include "render/font.hpp"
#include "render/hud.hpp"
#include "render/pixelart.hpp"
#include "render/scene.hpp"
#include "render/sprites.hpp"

using namespace cashyphus;
using namespace cashyphus::cfg;

namespace {

// --- input -------------------------------------------------------------------
// Two verbs, and on a desktop two mouse buttons: LEFT-CLICK pushes, a held
// RIGHT-CLICK braces -- which is also how the choice screen is answered
// (left-click keeps pushing, right-click walks away). SPACE/UP and SHIFT/DOWN
// stay mapped so the keyboard still works on its own.
//
// Touch is not handled here: web/shell.html turns a tap into SPACE and a
// press-and-hold into SHIFT, so a phone reaches exactly these same two verbs
// through the same code path rather than a third one.
Input readInput() {
    Input in;
    in.pushPressed  = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_UP) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    in.pushHeld     = IsKeyDown(KEY_SPACE)    || IsKeyDown(KEY_UP)    || IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    in.bracePressed = IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_RIGHT_SHIFT) ||
                      IsKeyPressed(KEY_DOWN) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    in.braceHeld    = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ||
                      IsKeyDown(KEY_DOWN) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    in.anyPressed   = in.pushPressed || in.bracePressed;
    return in;
}

// --- palette for the current frame ------------------------------------------
Palette framePalette(const Game& g) {
    if (g.phase() == Phase::Credits) {
        return walkPalette();
    }
    if (g.phase() == Phase::WalkAway) {
        const float t = clampf(g.phaseTime() / 3.0f, 0.0f, 1.0f);
        return blendPalette(climbPalette(1.0f), walkPalette(), t);
    }
    return climbPalette(g.ballProgress());
}

// --- music levels per phase --------------------------------------------------
float musicLevel(const Game& g) {
    switch (g.phase()) {
        case Phase::Title:     return 0.30f;
        case Phase::Climb:     return 0.45f;
        case Phase::Collapse:
        case Phase::Celebrate:
        case Phase::Silence:
        case Phase::RollDown:  return 0.0f;      // the summit is silent
        case Phase::WalkIn:    return 0.25f;
        case Phase::Choice:    return 0.30f;
        case Phase::WalkAway:  return 0.45f;
        case Phase::Credits:   return 0.40f;
    }
    return 0.0f;
}

// --- a little state the audio layer needs between frames ---------------------
struct AudioCues {
    Phase prev = Phase::Title;
    float scrapeTimer = 0.0f;
    float breathTimer = 0.0f;
    float rattleTimer = 0.0f;
};

void driveAudio(audio::Synth& synth, const Game& g, AudioCues& cues, const Input& in,
                float dt) {
    // One-shots on phase transitions.
    if (g.phase() != cues.prev) {
        switch (g.phase()) {
            case Phase::Collapse:  synth.collapse(); break;
            case Phase::Celebrate:
                if (g.completedLives() < 20) synth.jingle();
                break;
            case Phase::RollDown:  synth.rollDown(); break;
            case Phase::WalkIn:    synth.bell();     break;
            case Phase::WalkAway:  synth.birds();    break;
            default: break;
        }
        cues.prev = g.phase();
    }

    if (!synth.ready()) return;

    // Pushing thump.
    if (g.phase() == Phase::Climb && in.pushPressed) synth.push();

    // Footstep scrape while planting your feet.
    if (g.phase() == Phase::Climb && in.braceHeld) {
        cues.scrapeTimer -= dt;
        if (cues.scrapeTimer <= 0.0f) { synth.scrape(); cues.scrapeTimer = 0.42f; }
    } else {
        cues.scrapeTimer = 0.0f;
    }

    // Coins rattling while the ball is actually rolling.
    if (g.phase() == Phase::Climb && std::fabs(g.sim().state().vel) > 0.004f) {
        cues.rattleTimer -= dt;
        if (cues.rattleTimer <= 0.0f) { synth.rattle(); cues.rattleTimer = 0.30f; }
    } else if (g.phase() == Phase::RollDown) {
        cues.rattleTimer -= dt;
        if (cues.rattleTimer <= 0.0f) { synth.rattle(); cues.rattleTimer = 0.18f; }
    } else {
        cues.rattleTimer = 0.0f;
    }

    // Heavy breathing when the character is spent.
    if (g.phase() == Phase::Climb && g.sim().state().strain > 0.45f) {
        cues.breathTimer -= dt;
        if (cues.breathTimer <= 0.0f) { synth.breath(); cues.breathTimer = 1.1f; }
    } else {
        cues.breathTimer = 0.0f;
    }

    // Music mood, how worn down the climb theme has become, and level.
    synth.musicSetMood(g.phase() == Phase::WalkAway || g.phase() == Phase::Credits);
    synth.musicSetClimb(core::arrangementFor(g.completedLives()));
    synth.musicVolume(musicLevel(g));
}

// --- optional capture mode (used to eyeball the game without playing it) -----
struct Options {
    bool capture = false;
    std::string prefix = "shot";
};

Options parseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--capture") == 0) {
            o.capture = true;
            if (i + 1 < argc) o.prefix = argv[++i];
        }
    }
    return o;
}

struct CaptureState {
    bool  resting = false;
    int   frames = 0;         // simulation steps since the capture began
    int   shotIndex = 0;
    int   sinceShotSteps = 0;
    int   choiceWait = 0;     // linger on the choice screen so it gets captured
    Phase lastPhase = Phase::Title;
};

// A simple auto-player so the capture mode can drive the whole game.
Input autoInput(const Game& g, CaptureState& cs) {
    Input in;
    switch (g.phase()) {
        case Phase::Title:
            in.anyPressed = true; in.pushPressed = true;
            break;
        case Phase::Climb: {
            const SimState& s = g.sim().state();
            if (s.stamina < 0.15f) cs.resting = true;
            if (s.stamina > 0.70f) cs.resting = false;
            if (cs.resting) in.braceHeld = true;
            else if (cs.frames % 10 == 0) { in.pushPressed = true; in.pushHeld = true; }
            in.anyPressed = true;
            break;
        }
        case Phase::Choice:
            // Linger a moment so the choice menu is visible in a capture,
            // then walk away to show the escape ending.
            ++cs.choiceWait;
            if (cs.choiceWait > 90) { in.bracePressed = true; in.anyPressed = true; }
            break;
        default:
            break;
    }
    return in;
}

void saveTarget(const RenderTexture2D& target, const std::string& file) {
    Image img = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&img);
    ImageResizeNN(&img, img.width * 3, img.height * 3);   // 3x for inspection
    ExportImage(img, file.c_str());
    UnloadImage(img);
}

// --- letterboxed presentation ------------------------------------------------
void present(const RenderTexture2D& target) {
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();
    int scale = sw / VIRTUAL_W;
    if (sh / VIRTUAL_H < scale) scale = sh / VIRTUAL_H;
    if (scale < 1) scale = 1;

    const float dw = static_cast<float>(VIRTUAL_W * scale);
    const float dh = static_cast<float>(VIRTUAL_H * scale);
    const Rectangle src{ 0.0f, 0.0f,
                         static_cast<float>(target.texture.width),
                         -static_cast<float>(target.texture.height) };
    const Rectangle dst{ (sw - dw) * 0.5f, (sh - dh) * 0.5f, dw, dh };
    DrawTexturePro(target.texture, src, dst, { 0.0f, 0.0f }, 0.0f, WHITE);
}

// -----------------------------------------------------------------------------
//  The running game, in one object.
//
//  This has to outlive main() on the web: emscripten_set_main_loop unwinds the
//  C stack instead of returning, which destroys main()'s locals, so the state
//  lives at namespace scope where the frame callback can still reach it.
// -----------------------------------------------------------------------------
struct App {
    Options         opts;
    RenderTexture2D target{};
    audio::Synth    synth;
    art::SpriteBank sprites;
    Game            game;
    AudioCues       cues;
    CaptureState    cap;
    float           accum   = 0.0f;
    bool            running = true;
};

App g_app;

void renderFrame() {
    const Palette pal = framePalette(g_app.game);
    BeginTextureMode(g_app.target);
    ClearBackground(art::toColor(pal.skyTop));
    scene::draw(g_app.game, pal, g_app.sprites);
    hud::draw(g_app.game, pal);
    EndTextureMode();
}

// One frame: fixed-step simulation, then present. Identical on both platforms;
// only what drives it differs.
void frameStep() {
    App& a = g_app;
    if (WindowShouldClose()) { a.running = false; return; }

    if (a.opts.capture) {
        a.accum += 20.0f * FIXED_DT;   // fast-forward so a walkthrough is quick
    } else {
        float frame = GetFrameTime();
        if (frame > MAX_FRAME) frame = MAX_FRAME;
        a.accum += frame;
    }

    const Input realIn = a.opts.capture ? Input{} : readInput();
    bool firstStep = true;
    while (a.accum >= FIXED_DT) {
        Input step;
        if (a.opts.capture) {
            // The auto-player must see every simulation step so its push beat
            // lands at a realistic rate.
            step = autoInput(a.game, a.cap);
            ++a.cap.frames;
            ++a.cap.sinceShotSteps;
        } else {
            step = realIn;
            if (!firstStep) {
                step.pushPressed = false;
                step.bracePressed = false;
                step.anyPressed = false;
            }
        }
        a.game.update(step, FIXED_DT);
        driveAudio(a.synth, a.game, a.cues, step, FIXED_DT);
        a.accum -= FIXED_DT;
        firstStep = false;
    }

    renderFrame();

    BeginDrawing();
    ClearBackground(BLACK);
    present(a.target);
    EndDrawing();

    if (a.opts.capture) {
        const bool phaseChanged = a.game.phase() != a.cap.lastPhase;
        const bool periodic = a.game.phase() == Phase::Climb && a.cap.sinceShotSteps > 15 * 60;
        if (phaseChanged || periodic) {
            char name[512];
            std::snprintf(name, sizeof(name), "%s_%02d.png", a.opts.prefix.c_str(),
                          a.cap.shotIndex++);
            saveTarget(a.target, name);
            std::printf("captured %s  (phase=%d life=%d progress=%.2f)\n",
                        name, static_cast<int>(a.game.phase()), a.game.incarnation(),
                        a.game.ballProgress());
            a.cap.sinceShotSteps = 0;
            a.cap.lastPhase = a.game.phase();
        }
        if (a.game.phase() == Phase::Credits && a.game.phaseTime() > 3.0f) a.running = false;
    }
}

#if defined(__EMSCRIPTEN__)
void webFrame() {
    frameStep();
    if (!g_app.running) emscripten_cancel_main_loop();
}
#endif

} // namespace

int main(int argc, char** argv) {
    App& a = g_app;
    a.opts = parseArgs(argc, argv);

    int flags = FLAG_WINDOW_RESIZABLE;
    if (!a.opts.capture) flags |= FLAG_VSYNC_HINT;
    SetConfigFlags(flags);

#if defined(__EMSCRIPTEN__)
    // On the web the canvas is sized by CSS, so the backing resolution is raised
    // instead: 3x stays crisp when the letterbox scales it up on a phone.
    InitWindow(VIRTUAL_W * 3, VIRTUAL_H * 3, "CASHYPHUS");
#else
    // On the desktop the window itself has to fit the display. A hard-coded 3x
    // is 540x1080 -- exactly the height of a 1080p screen -- so the title bar
    // and the dock get pushed off the bottom and the first thing a PC player
    // sees is a window that does not fit their monitor. Open at 2x, then grow
    // to the largest integer scale the screen can actually hold, keeping 96px
    // of headroom for a title bar and a taskbar.
    InitWindow(VIRTUAL_W * 2, VIRTUAL_H * 2, "CASHYPHUS");
    int scale = 3;
    const int room = GetMonitorHeight(GetCurrentMonitor()) - 96;
    while (scale > 1 && VIRTUAL_H * scale > room) --scale;
    SetWindowSize(VIRTUAL_W * scale, VIRTUAL_H * scale);
    SetWindowMinSize(VIRTUAL_W, VIRTUAL_H);   // 1x, so every display can shrink it
#endif
    SetTargetFPS(a.opts.capture ? 0 : 60);

    // The whole UI draws with a real 8px bitmap face instead of raylib's
    // smooth default, which is what makes the interface read as 16-bit.
    art::text::load();

    // 180x360 portrait render target, integer-scaled to the window.
    a.target = LoadRenderTexture(VIRTUAL_W, VIRTUAL_H);
    SetTextureFilter(a.target.texture, TEXTURE_FILTER_POINT);

    InitAudioDevice();
    a.synth.init();
    a.synth.musicStart();

    a.sprites.load();

    // Report any malformed sprite rows once, at startup.
    const int badRows = art::SpriteBank::validateAll();
    if (badRows != 0) TraceLog(LOG_WARNING, "CASHYPHUS: %d malformed sprite sheet(s)", badRows);

    // Capture mode: grab the title screen before anything moves.
    if (a.opts.capture) {
        renderFrame();
        saveTarget(a.target, a.opts.prefix + "_title.png");
        std::printf("captured title\n");
    }

#if defined(__EMSCRIPTEN__)
    // raylib does not drive a main loop on the web -- it expects the game to.
    // A plain while() here would block the browser's main thread for ever and
    // the tab would never paint, which is exactly what a blank page looks like.
    // simulate_infinite_loop = 1 means this call does not return, so the
    // teardown below is native-only.
    emscripten_set_main_loop(webFrame, 0, 1);
#else
    while (a.running) frameStep();

    a.synth.shutdown();
    CloseAudioDevice();
    a.sprites.unload();
    art::text::unload();
    UnloadRenderTexture(a.target);
    CloseWindow();
#endif
    return 0;
}
