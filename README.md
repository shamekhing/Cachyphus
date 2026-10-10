# CASHYPHUS

[![CI](https://github.com/shamekhing/Cachyphus/actions/workflows/ci.yml/badge.svg)](https://github.com/shamekhing/Cachyphus/actions/workflows/ci.yml)
[![Pages](https://github.com/shamekhing/Cachyphus/actions/workflows/pages.yml/badge.svg)](https://github.com/shamekhing/Cachyphus/actions/workflows/pages.yml)
[![Play in your browser](https://img.shields.io/badge/play-in%20your%20browser-brightgreen)](https://shamekhing.github.io/Cachyphus/)

**One hill. One lifetime. One more push.**

A pixel-art allegory: a person spends their whole life pushing a giant, talking
ball of money up a hill. They age as they climb. At the summit they die. The ball
rolls back down. Someone else begins. After three lives, the game finally offers
you a choice: **keep pushing, or walk away.**

Built with **C++17 + raylib**. Sprites are generated in code; the soundtrack uses edited CC0 currency recordings.

---

## Controls

| Input | Action |
|---|---|
| **Left-click**, **SPACE** or **Up** | Push: shove the ball uphill, spending stamina |
| **Hold right-click**, **SHIFT** or **Down** | Brace: hold the ball still and recover stamina, spending grip |
| **Tap** / **left-click** / **SPACE** on the choice screen | Keep pushing (the cycle continues) |
| **Hold** / **right-click** / **SHIFT** on the choice screen | Walk away (the escape ending) |
| **M** | Mute or unmute audio |
| **[** / **]** | Lower or raise master volume |
| **F** (web build) | Fullscreen |

On a phone the sound button mutes audio. Gameplay uses the touch bridge in
`web/shell.html`: **tap = PUSH, press-and-hold = BRACE**. There is no swipe or
on-screen pad. The
same two verbs answer the ending, which is the one place a phone cannot copy a
keyboard: **tap keeps pushing, hold walks away.**

The two verbs and *when* to use them are the entire game. Topping up stamina
requires standing still, which the ball loudly objects to.

You cannot win by mashing: an exhausted push barely moves the ball and pushes
are rate-limited, so you have to work in bursts and rest. Each incarnation also
makes the ball heavier, so the rhythm gets tighter the longer you keep climbing.

---

## Building

raylib is pulled in automatically with CMake `FetchContent` (pinned to **5.5**),
so the only prerequisites are a C++17 compiler, CMake >= 3.20, and the usual
X11/OpenGL development packages.

### Quick start

`run.sh` wraps everything below:

```bash
./run.sh              # build if stale, then play
./run.sh --release    # optimised build
./run.sh --fresh      # wipe the build tree first (after CMake changes)
./run.sh --tests      # build and run the headless tests, don't play
./run.sh --web        # build the WebAssembly version and serve it on :8000
./run.sh --smoke      # build it, then load it in headless Chromium and fail
                      # if the page errors or the canvas never draws
```

### Linux (native)

```bash
cmake --preset debug          # first run downloads + builds raylib (~1-2 min)
cmake --build --preset debug -j
./build/debug/cashyphus
```

For an optimised build use `cmake --preset release` / `cmake --build --preset release`.

Options:

| CMake option | Default | Meaning |
|---|---|---|
| `CASHYPHUS_BUILD_GAME` | `ON` | build the game executable (needs raylib) |
| `CASHYPHUS_BUILD_TESTS` | `ON` | build the headless unit tests (needs **no** raylib) |
| `CASHYPHUS_DUMP_SPRITES` | `ON` | build the sprite-sheet preview tool |
| `USE_SYSTEM_RAYLIB` | `OFF` | use an installed raylib instead of fetching one |
| `RAYLIB_TAG` | `5.5` | which raylib tag to fetch (e.g. `6.0`) |

### Web / WebAssembly

Emscripten produces a page with a preloaded audio data package (the CMake build sets raylib `PLATFORM=Web` for you):

```bash
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j
# serve it (a plain file:// load will not work)
python3 -m http.server -d build-web 8000   # then open cashyphus.html
```

The web build emits `cashyphus.html`, `.js`, `.wasm`, and `.data`. The data file contains the runtime WAV and OGG assets. `tools/stage_site.py` gives all three binary files content-hashed names for publishing.

The page is built to be played on a phone as it stands: the game sizes its
portrait frame to the canvas, so the game fills the screen, and the canvas is
shown at its own pixel size -- scaled by one factor, capped at 1, never given a
shape of its own.
(raylib resizes the canvas element to the whole window on every resize, so a CSS
box of a different shape lets the browser squash the game.) **tap is PUSH,
press-and-hold is BRACE.** Both the layout and that touch bridge live in
`web/shell.html`, which is also where any load failure is reported -- a wasm
build that fails silently is a blank page, which is the worst possible failure
mode, so nothing is allowed to fail
quietly.

> **The web build has to drive its own main loop.** raylib never calls
> `emscripten_set_main_loop` itself, so a plain `while (!WindowShouldClose())`
> blocks the browser's main thread for ever and the tab never paints -- which is
> exactly what a blank page looks like. `src/main.cpp` therefore splits the frame
> into `frameStep()` and hands it to `emscripten_set_main_loop` under Emscripten,
> keeping the plain loop for native. The game state lives in a namespace-scope
> `App`, not in `main`'s locals, because that call unwinds the stack rather than
> returning.
>
> **Two more things a wasm page needs.** `Module.canvas` must be set explicitly,
> or Emscripten's GLFW init throws `reading 'addEventListener' of undefined` and
> `main()` never runs -- a black canvas with no error visible anywhere. And
> `-sASYNCIFY` is required, because raylib's web audio spins on
> `emscripten_sleep()` while the Web Audio context initialises; miniaudio says so
> in a comment right beside the call.
>
> Those fixes only survive publishing if the page and the wasm cannot drift apart.
> `tools/stage_site.py` publishes `cashyphus.<hash>.js` and
> `cashyphus.<hash>.wasm` and rewrites the references, so a browser still holding
> a copy from an earlier deploy can never pair the wrong pair together -- which is
> what produced `...compile your program with async support` in production.
>
> `tools/smoke_web.js` then loads the staged site in headless Chromium and fails
> on any console error, any unhandled rejection, or a canvas that never draws.
> The Pages workflow runs it before deploying, so a blank page cannot be
> published in the first place.

### Publishing a playable page (GitHub Pages)

`.github/workflows/pages.yml` builds the WebAssembly version and publishes it as
a static page. On a push to `main` it:

1. runs the headless tests (a few seconds, no raylib needed) as a gate;
2. cross-compiles the game with Emscripten;
3. stages just the three runtime files plus a `.nojekyll` marker;
4. uploads and deploys them to GitHub Pages.

**One-time setup:** in the repository, go to *Settings → Pages → Build and
deployment* and set **Source** to **GitHub Actions**. After that, every push to
`main` republishes the game at:

```
https://<owner>.github.io/<repo>/
```

You can also trigger it by hand from the *Actions* tab (`workflow_dispatch`).

`.github/workflows/ci.yml` is the separate fast feedback workflow: it builds and
runs the core tests in both Debug and Release on every push and pull request,
plus a full native build to catch compile errors in the raylib layer.

---

## Tests

The simulation, state machine, dialogue and palette live in `src/core/` and know
nothing about raylib. That makes them testable in milliseconds with no window,
no GPU and no audio device:

```bash
ctest --test-dir build/debug --output-on-failure
# or
./build/debug/cashyphus_tests
```

The suite covers stage/age boundaries, push & stamina, gravity rollback, the
grip/slip hysteresis, palette interpolation, dialogue selection, and a full
auto-played run through the state machine (three lives -> the choice -> both
endings). It also *measures pacing*: a competent auto-player finishes a life in
roughly 45-80 s, and even a slow masher is never softlocked.

---

## Project layout

```
src/
  core/       pure logic: no raylib, fully unit tested
    config.hpp           every tuning constant, in one table
    sim.hpp/.cpp         ball velocity, stamina, grip, aging
    game_state.hpp/.cpp  the CLIMB -> death -> reincarnation loop
    dialogue.hpp/.cpp    the ball's lines, banded by life stage
    palette.hpp/.cpp     warm-youth -> cold-summit colour model
    music.hpp/.cpp       the arrangement curve: how worn the climb theme is
  render/     raylib drawing
    pixelart.hpp/.cpp    char-map + palette -> texture (and CPU images)
    sprites.hpp/.cpp     the character sprite sheets + hunch-on-aging
    scene.hpp/.cpp       hill, parallax, ball, character, speech bubble
    hud.hpp/.cpp         meters, counters, title/choice/ending overlays
    font.hpp/.cpp        the baked 8px pixel face, and text helpers
    font_data.hpp        generated glyph table (tools/gen_font.py)
  audio/
    synth.hpp/.cpp       loads the effect WAVs, pools voices, mixes the two beds
    climb.hpp/.cpp       plays the baked climb theme, worn down by the curve
    music_data.hpp/.cpp  the baked theme itself (from tools/gen_music.py)
    voices.hpp           the nine procedural voices, for when a recording is absent
  main.cpp      window, 180x360 portrait render target, input, fixed-timestep loop
assets/audio/   the CC0 recordings the effects and the ending's bed are cut from,
                the edited effects, the walk-away ambience, and SOURCES.md
tests/          headless test binary
tools/          offline asset bakers and preview dumps
web/            Emscripten HTML shell
```

---

## How it works

**Fixed timestep.** The game advances in exact 1/60 s steps with an accumulator,
so the physics feel identical regardless of frame-rate.

**The simulation** (`core/sim.cpp`) is deliberately a simple, deterministic
velocity model rather than a rigid-body one:

* every push is an impulse scaled by *remaining stamina* and *age*. An exhausted
  shove (stamina 0) is worth only 5% of a fresh one, and pushes are rate-limited
  by a short cooldown, so grinding the key on an empty tank gets you nowhere.
  **Stamina is what actually buys progress**;
* aging wears down *efficiency*, not just raw power. Every life stage makes a
  push cost more stamina, stamina refill more slowly, grip drain faster and
  grip recover slower, so an old body works in shorter bursts and needs longer
  rests. Measured against youth, the final years cost **72% more stamina per
  shove**, refill **42% slower**, drain grip **39% faster** and recover it
  **28% slower**. The effect is large enough to see directly -- each quarter of
  the hill takes roughly **3.3 s / 9.9 s / 14.2 s / 28.5 s** as the body ages;
* gravity accelerates the ball downhill, multiplied by a slope that steepens
  with progress (the hill literally gets steeper) *and* by the life counter:
  every incarnation makes the ball 8% heavier, up to +40%;
* damping bleeds off speed, so the ball coasts and then starts rolling back;
* bracing cancels the ball's velocity, regenerates stamina and drains **grip**;
  when grip is exhausted the ball slips and you cannot hold it until grip
  recovers past a hysteresis threshold.

The ball also genuinely **rolls**: its surface rotation is derived from the
distance it actually travelled (`distance / radius`), so the banknote seams,
coins and the `$` turn by the correct amount while it is pushed, while it slips
backwards, and all the way back down the hill. Only the eyes and mouth stay
upright, so the ball is always facing you.

The direction convention lives in one place, `src/core/roll.hpp`: **positive
spin is clockwise on screen**, which is the way a wheel turns when it rolls to
the right, i.e. uphill. Because a rendering sign convention is easy to get
backwards (it was, once) and impossible to check by eyeballing a still frame,
it is covered by a test that feeds a derived screen offset back through the
rotation and asserts the two agree.

**One balancing trap worth knowing about:** the stamina and grip age penalties
*multiply*. A slower stamina refill forces a *longer* brace, and a longer brace
is exactly what burns grip -- so making both curves steep at once is not
"harder", it is broken: the character slips forever without gaining ground and
the climb becomes impossible. The grip curve is therefore deliberately gentler
than the stamina curve, and `cashyphus_tests` asserts that at *every* age the
grip earned while pushing still covers the grip spent while resting.

That combination is the whole game: pushing without resting stalls out, resting
without pushing gives ground away, and grip is what stops you resting forever.

Because age only *reduces effectiveness* and never ends a life, every
incarnation is completable   the game is not about being good enough, it is
about asking why you are climbing.

**The one design rule that matters most:** resting is mechanically *correct*.
Bracing restores stamina. But the money ball complains the moment you stop. The
mechanics quietly teach you that the ball's advice is unreliable   which is the
first step toward the ending where you simply walk away.

**Rendering** draws everything into a portrait render target with point filtering
and blits it to the window at a whole-number scale. The target is not fixed at
180x360: `cfg::frameFor()` takes the largest integer scale that still leaves a
portrait frame at least as wide as the art was drawn for, and then *derives* the
frame from it -- so the frame eats the whole display rather than leaving a margin
around a fixed one. A phone gets 195x422 at 2x, which is its entire 390x844 screen
with no padding at all, and it stays that way whatever the browser chrome is doing
to the viewport height; the same rule fills a 360x640 phone (180x320 at 2x) and a
tablet. The art is laid out in the 180x360 design frame and translated down by the
extra height, so a taller frame is more sky above an unchanged scene -- nothing is
stretched and nothing is cropped. Only a landscape monitor gives up anything, and
only the sides, because a hill needs somewhere to climb.

**Audio** is two things. The music is one real CC0 chiptune -- MintoDog's *Amusement park Stage*, credited in `assets/CREDITS.md` -- baked into the binary as a sample table and worn down as lifetimes pile up: a low-pass first, then sample-and-hold, then quantisation, then a little detune, all from the curve in `src/core/music.hpp`, so the tune stays recognisable however far the ball has got. The effects are the other thing: edited CC0 coin, banknote, paper and casino recordings from `assets/audio`, cached in a three-voice pool each so a player who mashes the push key overlaps coin tails rather than cutting them off, plus the natural bed that takes over when the player walks away. The main loop routes effects from accepted simulation events, crossfades the beds, and applies master, music, effects, dialogue, and ambience gains. M toggles mute and bracket keys adjust master volume. A missing or undecodable recording falls back to one of the nine procedural voices in `src/audio/voices.hpp` instead of to silence, and if the machine has no audio device at all the game still plays.

---

## Development tools

**Sprite preview**   renders every character frame to a PNG without needing a
window or GPU, which makes iterating on the pixel art painless:

```bash
./build/debug/cashyphus_dump_sprites sprites_preview.png
```

**Capture mode**   auto-plays the whole game with a scripted player and writes a
PNG on every phase change, so the narrative flow can be inspected at a glance:

```bash
mkdir -p shots && cd shots
../build/debug/cashyphus --capture shot
```

**Audio decode**   turns any audio raylib understands (OGG, MP3, FLAC, WAV) into
a plain 16-bit WAV, so an asset can be measured offline with no audio
toolchain installed:

```bash
./build/debug/cashyphus_decode track.ogg track.wav
```

**Asset bakers** regenerate the font table, the baked music and the committed
effect recordings; the last one checks them. None is needed for an ordinary build:

```bash
python3 tools/gen_font.py                  # assets/fonts/*.bdf  -> font_data.hpp
python3 tools/gen_music.py  <decoded.wav>  # the chiptune       -> music_data.cpp
python3 tools/build_audio.py               # retained recordings -> effect WAVs
python3 tools/verify_audio.py              # measure the effects, the bed and the table
```

Sources and licenses are in **[assets/audio/SOURCES.md](assets/audio/SOURCES.md)** and **[assets/CREDITS.md](assets/CREDITS.md)**.

**Input-bridge test**   checks the page's tap-vs-hold translation and the mouse
release safety net without needing a browser: it loads the built page's inline
script against a stub DOM and fires synthetic touch and mouse events at it. Run
it against a page you have already built:

```bash
node web/test_touch.js build-web/cashyphus.html
```

---

## Tuning / balancing

Every gameplay number lives in **`src/core/config.hpp`**, and that file belongs
to the raylib-free core, so editing it and rebuilding takes seconds (no raylib
recompile).

### The tuning loop

```bash
$EDITOR src/core/config.hpp
cmake --build --preset debug -j && ./build/debug/cashyphus_tests   # numbers
./build/debug/cashyphus                                            # feel
```

**The test binary is the tuning instrument.** As well as pass/fail it prints the
measurements you need to judge a change without guessing:

- climb time at **6, 4 and 3 presses per second** (a quick, a moderate and a slow
  player). If any of these stops finishing, you have introduced a softlock;
- the **high-water mark a pure key-masher reaches** (it must stay far below 1.0);
- **stamina cost per push** and **grip drained per second**, young vs aged;
- **grip spent vs earned per rest cycle**, for all four ages;
- **progress gained in 60 s during the final years** (the dead-end check).

### Baseline to compare against

| Measurement | Current value |
|---|---|
| Climb @6 / @4 / @3 presses per second | 55.9 s / 56.7 s / 65.8 s |
| Time per quarter (Youth / Adult / Old / Final) | 3.3 / 9.9 / 14.2 / 28.5 s |
| Masher high-water mark | 0.30 of the hill |
| Stamina per push, young to aged | 0.058 to 0.103 |
| Grip drained per second, young to aged | 0.26 to 0.36 |
| Grip spent vs earned, final years | 0.49 vs 0.63 |

### What each knob does

**Overall pace**

| Knob | Now | Effect |
|---|---|---|
| `PUSH_IMPULSE` | `0.027` | progress/s added per accepted press. The main dial for how long a life takes. |
| `PUSH_COST` | `0.060` | stamina per press. Higher gives shorter bursts and more resting. |
| `PUSH_COOLDOWN` | `0.13` | minimum gap between accepted presses (caps you at ~7.7/s). |
| `STAMINA_REGEN_BRACE` | `0.70` | refill rate while bracing. Raising it shortens lives. |

**Keeping mash-spamming dead**

| Knob | Now | Effect |
|---|---|---|
| `STAMINA_MIN_FACTOR` | `0.05` | power of a push at zero stamina. This is the whole reason mashing fails; nudge it towards `1.0` and endless pressing wins again. |
| `PUSH_COOLDOWN` | `0.13` | also limits how much a fast masher can gain. |

**How hard the hill is**

| Knob | Now | Effect |
|---|---|---|
| `GRAVITY` | `0.012` | downhill pull. Higher makes the ball roll back faster and further. |
| `SLOPE_GROWTH` | `0.50` | how much steeper the hill becomes with progress. |
| `DAMPING` | `0.90` | rolling friction. Lower lets the ball coast longer. |
| `MAX_SPEED` | `0.150` | speed clamp, in both directions. |
| `BRACE_ACCEL` | `0.35` | how firmly bracing kills momentum. |
| `GRIP_DRAIN` / `GRIP_REGEN` | `0.26` / `0.62` | how long you can hold the ball, and how fast grip returns. |
| `GRIP_RECOVER_THRESHOLD` | `0.25` | grip needed after a slip before you can brace again. |

**Ageing** (each array reads Youth, Adult, Old, Final)

| Knob | Now | Effect |
|---|---|---|
| `AGE_FACTOR` | `1.00 0.96 0.92 0.86` | raw push power |
| `AGE_STAMINA_EFF` | `1.00 0.86 0.72 0.58` | divides the stamina a push costs, multiplies the refill rate |
| `AGE_GRIP_EFF` | `1.00 0.90 0.80 0.72` | divides grip drain, multiplies grip recovery |
| `STAGE_YOUTH_END` / `_ADULT_END` / `_OLD_END` | `0.25 / 0.50 / 0.75` | where each life stage begins along the hill |
| `AGE_START` / `AGE_END` | `18` / `80` | what the `AGE` counter reads at the bottom and the summit |

**Each cycle getting harder**

| Knob | Now | Effect |
|---|---|---|
| `CYCLE_GRAVITY_STEP` | `0.080` | +8% downhill pull per incarnation |
| `CYCLE_GRAVITY_CAP` | `5` | stops the ramp after the 6th life (+40%) so it can never become unwinnable |

**Narrative pacing**

| Knob | Now | Effect |
|---|---|---|
| `CHOICE_AFTER_LIVES` | `3` | lives before the keep-pushing / walk-away choice appears |
| `LINE_MIN_TIME` | `3.5` | minimum spacing between the ball's ambient lines |
| `IDLE_COMPLAINT_AFTER` | `2.5` | how long you can stop pushing before it starts nagging |
| `DUR_COLLAPSE` / `_CELEBRATE` / `_SILENCE` / `_ROLLDOWN` / `_WALKIN` / `_WALKAWAY` | `2.0 / 1.6 / 1.2 / 2.0 / 1.5 / 6.0` | length of each story beat, in seconds |

**Presentation** (not gameplay): `VIRTUAL_W` / `VIRTUAL_H` (180x360 portrait),
`HILL_LEN`, `BALL_RADIUS`. Note `BALL_RADIUS` also feeds `ROLL_DEG_PER_PROGRESS`,
so changing it alters how fast the ball visibly spins as well as how big it looks.

### Recipes

**Lives are too long or too short.** Move `PUSH_IMPULSE` about 10% at a time.
Raising `STAMINA_REGEN_BRACE` also shortens lives, by shortening the rests. Do
*not* raise the base regen to cure a slow *late* climb: it buffs the young and
the old alike and flattens the ageing curve. That exact mistake was made once
already and made the game feel like ageing did nothing.

**Ageing should bite harder.** Lower `AGE_STAMINA_EFF[2]` and `[3]` (try `0.50`).
Leave `AGE_GRIP_EFF` alone and re-run the tests; the rest-cycle check will tell
you when you have gone too far.

**A life never finishes, or the final years are impossible.** You will see the
`every life can reach the summit` or `every age can complete a rest cycle`
checks fail. Undo the last change, or buy headroom with a higher `GRIP_REGEN` or
`STAMINA_REGEN_BRACE`.

**Later incarnations should be harder.** Raise `CYCLE_GRAVITY_STEP`. It is capped
by `CYCLE_GRAVITY_CAP` so the ramp can never make a climbing state unwinnable.

**I want mashing to be viable again.** Raise `STAMINA_MIN_FACTOR` towards `1.0`.
The `mashing cannot substitute for resting` check will start failing, which is
that test doing its job.

**Shorter or longer story beats.** The `DUR_*` values, in seconds.

### What the tests will stop you doing

The suite is deliberately opinionated about a few properties, so a bad value
fails loudly instead of quietly ruining the game:

- **No softlocks.** Every life must finish at 6, 4 and 3 presses per second.
- **Mashing must not work.** Pressing on every frame for ten minutes must peak
  below half the hill, with stamina pinned at zero.
- **Push rate stays capped.** 60 frame-perfect presses in one second must yield
  only about 5 to 10 accepted pushes.
- **Ageing is monotonic and never reverses.** Losing ground never gives years back.
- **Ageing is steep but survivable.** Final years: cost >=1.5x, refill <=0.65x,
  grip drain >=1.25x, grip recovery <=0.80x, and both arrays strictly decreasing.
- **Every age can complete a rest cycle.** Grip earned while pushing must cover
  grip spent while bracing at all four ages. This is the constraint that stops
  the two age penalties from compounding into an impossible climb, and it is the
  one to watch when tuning anything age- or grip-related.
- **The final years still gain ground**, rather than stalling in a slip loop.
- **The ball rolls the right way.** Positive spin is clockwise on screen.
- **The effects are textures, not clicks.** Every voice is rendered and measured
  with no audio device at all: no DC offset, no peak past the 0.8 headroom
  budget, and nothing that starts or ends on a step. A "noise" effect that is
  really a constant gated by an envelope is what static sounds like.

---

## Credits

Inspired by the myth of Sisyphus. raylib by Ramon Santamaria and contributors.
