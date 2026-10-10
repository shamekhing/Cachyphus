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

// --- a little state the audio layer needs between frames ---------------------
struct AudioCues {
    Phase prev = Phase::Title;
    Stage stage = Stage::Youth;
    bool bracing = false, low = false, slipping = false, backwards = false, gripGone = false, twist = false;
    int pushes = 0, reveal = 0;
    const char* line = nullptr;
    float master = 1.0f;
    bool muted = false;
};

void driveAudio(audio::Synth& synth, const Game& g, AudioCues& c, const Input&, float dt) {
    using E = audio::Synth::Effect;
    using T = audio::Synth::Track;
    // A second attempt matters on the web: the browser hands out the audio
    // context suspended and only lets it run from inside the player's first tap,
    // so the device can quite legitimately be missing when main() starts. init()
    // is cheap and idempotent, so keep asking until it succeeds -- otherwise the
    // page would stay silent for the whole session with no way back.
    if (!synth.ready()) synth.init();
    if (!synth.ready()) return;
    const Phase p = g.phase();
    const SimState& st = g.sim().state();
    const float music = (p == Phase::Collapse || p == Phase::Celebrate || p == Phase::Silence || p == Phase::RollDown) ? 0.0f : 0.35f;
    synth.setVolumes(c.muted ? 0.0f : c.master, music, 0.65f, 0.45f, 0.35f);
    if (p != c.prev) {
        if (p == Phase::Collapse) synth.play(E::Summit);
        if (p == Phase::Celebrate) synth.play(E::Death);
        if (p == Phase::WalkIn) synth.play(E::Reincarnation);
        if (p == Phase::Choice) synth.play(E::Choice);
        if (c.prev == Phase::Choice && p != Phase::Choice) synth.play(E::Choice);
        if (p == Phase::WalkAway) { synth.play(E::WalkAway); c.twist = false; }
        if (c.prev == Phase::Choice && p == Phase::Climb) synth.play(E::Reincarnation);
        c.prev = p;
    }
    if (p == Phase::Climb) {
        if (st.pushes > c.pushes) {
            const int i = (st.pushes - 1) % 4;
            const float agePitch = 1.0f - 0.08f * static_cast<int>(g.stage());
            synth.play(st.stamina > 0.87f && st.pushes % 4 == 1 ? E::Strong : static_cast<E>(i), agePitch * (0.97f + 0.02f * (st.pushes % 4)), 0.85f);
        }
        const bool brace = st.grip > 0.0f && !st.slipping && st.stamina < 0.999f && g.character() == CharState::Bracing;
        if (brace && !c.bracing) synth.play(E::BraceStart);
        if (!brace && c.bracing) synth.play(E::BraceEnd);
        c.bracing = brace;
        synth.loop(E::BraceLoop, brace ? 0.24f : 0.0f, dt);
        synth.loop(E::Roll, std::min(0.55f, std::fabs(st.vel) * 5.0f), dt);
        const bool backward = st.vel < -0.025f;
        if ((st.slipping && !c.slipping) || (backward && !c.backwards)) synth.play(E::Slip);
        if (st.stamina < 0.17f && !c.low) synth.play(E::Stamina);
        if (st.grip <= 0.0f && !c.gripGone) synth.play(E::Grip);
        if (g.stage() != c.stage) synth.play(E::Aging);
        c.low = st.stamina < 0.17f; c.slipping = st.slipping; c.backwards = backward; c.gripGone = st.grip <= 0.0f;
        c.stage = g.stage(); c.pushes = st.pushes;
    } else {
        c.bracing = false; c.low = false; c.slipping = false; c.backwards = false; c.gripGone = false;
        c.pushes = 0; c.stage = Stage::Youth;
        synth.loop(E::BraceLoop, 0, dt); synth.loop(E::Roll, 0, dt);
    }
    if (p == Phase::RollDown) synth.loop(E::Downhill, 0.4f, dt);
    else synth.loop(E::Downhill, 0, dt);
    // Dialogue is revealed at 34 characters/s in scene.cpp. Limit ticks to
    // every third visible character and only while the line is still appearing.
    if (g.currentLine() != c.line) { c.line = g.currentLine(); c.reveal = 0; }
    if (c.line && (p == Phase::Climb || p == Phase::Choice)) {
        int shown = std::min(static_cast<int>(std::strlen(c.line)), static_cast<int>(g.lineAge() * 34.0f) + 1);
        if (shown > c.reveal && shown % 3 == 0 && c.line[shown-1] != ' ') synth.play(E::Dialogue, 0.95f + 0.05f*(shown%3), 0.5f);
        c.reveal = shown;
    }
    // The score is one baked CC0 chiptune, worn down a little with every lifetime
    // the player has spent -- core/music.hpp decides how much, and the arrangement
    // is applied as the theme is played rather than baked six different ways. So the
    // tune stays recognisable however far the ball has got, and the ball's own age
    // is what the ear hears changing. Walking away swaps the whole thing for the
    // recorded natural bed; the dark twist at the end of that walk brings the theme
    // back, which is the joke.
    synth.setArrangement(core::arrangementFor(g.completedLives()));
    synth.select(p == Phase::WalkAway && g.walkT() < 0.85f ? T::Freedom : T::Chip);
    if (p == Phase::WalkAway && g.walkT() >= 0.85f && !c.twist) {
        synth.play(E::Reincarnation); synth.play(E::Push1, 1.0f, 0.5f); c.twist = true;
    }
    synth.update(dt);
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

// The frame is what fills the display: frameFor() chose a whole-number scale and
// grew the frame itself, so all that is left here is blitting it up at that scale,
// with at most a pixel of slack on each edge.
void present(const RenderTexture2D& target) {
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();

    const float dw = static_cast<float>(VIRTUAL_W * FRAME_SCALE);
    const float dh = static_cast<float>(VIRTUAL_H * FRAME_SCALE);
    const Rectangle src{ 0.0f, 0.0f,
                         static_cast<float>(target.texture.width),
                         -static_cast<float>(target.texture.height) };
    const Rectangle dst{ std::floor((sw - dw) * 0.5f), std::floor((sh - dh) * 0.5f), dw, dh };
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

// Pick the frame for the display, and rebuild the render target when it changes.
// Called once at startup and again whenever the window resizes -- which on the web
// is every time the page re-fits the canvas: a rotation, or the URL bar moving the
// visual viewport under a phone. Rebuilding is the expensive part, so it only
// happens when the answer actually differs.
void fitFrame() {
    App& a = g_app;
    const FrameFit f = frameFor(GetScreenWidth(), GetScreenHeight());
    if (a.target.id != 0 && f.width == VIRTUAL_W && f.height == VIRTUAL_H) return;

    VIRTUAL_W = f.width;
    VIRTUAL_H = f.height;
    FRAME_SCALE = f.scale;
    if (a.target.id != 0) UnloadRenderTexture(a.target);
    a.target = LoadRenderTexture(VIRTUAL_W, VIRTUAL_H);
    SetTextureFilter(a.target.texture, TEXTURE_FILTER_POINT);
    TraceLog(LOG_INFO, "CASHYPHUS: frame %dx%d at %dx in a %dx%d display",
             VIRTUAL_W, VIRTUAL_H, FRAME_SCALE, GetScreenWidth(), GetScreenHeight());
}

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
    // A resized window can mean a different frame. Re-checking is a pair of
    // GetScreenWidth/Height calls, and the frame only changes when the display is
    // actually a different shape.
    if (IsWindowResized() || a.target.id == 0) fitFrame();

    if (a.opts.capture) {
        a.accum += 20.0f * FIXED_DT;   // fast-forward so a walkthrough is quick
    } else {
        float frame = GetFrameTime();
        if (frame > MAX_FRAME) frame = MAX_FRAME;
        a.accum += frame;
    }

    if (!a.opts.capture) {
        if (IsKeyPressed(KEY_M)) a.cues.muted = !a.cues.muted;
        if (IsKeyPressed(KEY_LEFT_BRACKET)) a.cues.master = std::max(0.0f, a.cues.master - 0.1f);
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) a.cues.master = std::min(1.0f, a.cues.master + 0.1f);
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
    // On the web the canvas box is owned by the page, which re-fits it every frame;
    // this is only the initial backing size, and fitFrame() takes the real one from
    // the canvas as soon as it exists.
    InitWindow(VIRTUAL_W * 3, VIRTUAL_H * 3, "CASHYPHUS");
#else
    // On the desktop the window itself has to fit the display, and the first thing
    // a PC player should see is a window that does. So: take the monitor, leave
    // 96px of headroom for a title bar and a taskbar, work out the frame that fills
    // what is left, and open at exactly that size. On a 1440x900 screen that is
    // 360x804 instead of the fixed 360x720 this used to open at, which left a fifth
    // of the height unused.
    InitWindow(VIRTUAL_W * 2, VIRTUAL_H * 2, "CASHYPHUS");
    const FrameFit fit = frameFor(GetMonitorWidth(GetCurrentMonitor()),
                                  GetMonitorHeight(GetCurrentMonitor()) - 96);
    SetWindowSize(fit.width * fit.scale, fit.height * fit.scale);
    SetWindowMinSize(VIRTUAL_W, VIRTUAL_H);   // 1x, so every display can shrink it
#endif
    SetTargetFPS(a.opts.capture ? 0 : 60);

    // The whole UI draws with a real 8px bitmap face instead of raylib's
    // smooth default, which is what makes the interface read as 16-bit.
    art::text::load();

    // The portrait frame, sized to this display: as much of the screen as an
    // integer-scaled 1:2 playfield can take, with no letterbox padding.
    fitFrame();

    InitAudioDevice();
    a.synth.init();

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
