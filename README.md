# CASHYPHUS

[![CI](https://github.com/shamekhing/Cachyphus/actions/workflows/ci.yml/badge.svg)](https://github.com/shamekhing/Cachyphus/actions/workflows/ci.yml)
[![Pages](https://github.com/shamekhing/Cachyphus/actions/workflows/pages.yml/badge.svg)](https://github.com/shamekhing/Cachyphus/actions/workflows/pages.yml)
[![Play in your browser](https://img.shields.io/badge/play-in%20your%20browser-brightgreen)](https://shamekhing.github.io/Cachyphus/)

**One hill. One lifetime. One more push.**

A pixel-art allegory: a person spends their whole life pushing a giant, talking
ball of money up a hill. They age as they climb. At the summit they die. The ball
rolls back down. Someone else begins. After three lives, the game finally offers
you a choice: **keep pushing, or walk away.**

Built with **C++17 + raylib**. Every sprite and every sound is generated in code,
so the repository contains no binary assets at all.

---

## Controls

| Input | Action |
|---|---|
| **Left-click**, **SPACE** or **Up** | Push: shove the ball uphill, spending stamina |
| **Hold right-click**, **SHIFT** or **Down** | Brace: hold the ball still and recover stamina, spending grip |
| **Tap** / **left-click** / **SPACE** on the choice screen | Keep pushing (the cycle continues) |
| **Hold** / **right-click** / **SHIFT** on the choice screen | Walk away (the escape ending) |
| **F** (web build) | Fullscreen |

On a phone there is nothing to click: the touch bridge in `web/shell.html` is the
whole control surface, and it is **tap = PUSH, press-and-hold = BRACE** and
nothing else -- there is no swipe, no second finger and no on-screen pad. The
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

Emscripten produces a self-contained page (the CMake build sets raylib `PLATFORM=Web` for you):

```bash
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j
# serve it (a plain file:// load will not work)
python3 -m http.server -d build-web 8000   # then open cashyphus.html
```

Because there are no asset files there is nothing to preload, so the whole game is
`cashyphus.html` + `cashyphus.js` + `cashyphus.wasm`, about 1.1 MB in total -- most
of the wasm is the baked music table.

The page is built to be played on a phone as it stands: the game letterboxes its
portrait 1:2 frame inside the canvas, and the canvas is shown at its own pixel
size -- scaled by one factor, capped at 1, never given a shape of its own.
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
    music.hpp/.cpp       how worn down the climb theme is, per incarnation
  render/     raylib drawing
    pixelart.hpp/.cpp    char-map + palette -> texture (and CPU images)
    sprites.hpp/.cpp     the character sprite sheets + hunch-on-aging
    scene.hpp/.cpp       hill, parallax, ball, character, speech bubble
    hud.hpp/.cpp         meters, counters, title/choice/ending overlays
    font.hpp/.cpp        the baked 8px pixel face, and text helpers
    font_data.hpp        generated glyph table (tools/gen_font.py)
  audio/
    voices.hpp           the nine effects, as pure functions of the sample index
    synth.hpp/.cpp       bakes them into Sounds + the escape motif
    climb.hpp/.cpp       plays the baked climb theme, worn down per life
    music_data.cpp       generated PCM table (tools/gen_music.py)
  main.cpp      window, 180x360 portrait render target, input, fixed-timestep loop
assets/         the committed source assets and their licences (CREDITS.md)
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

**Rendering** draws everything into a 180x360 portrait render target with point
filtering, then blits it to the window at an integer scale with letterboxing, so
the pixel art stays crisp at any window size.

**Audio** has no files. `audio/voices.hpp` is raylib-free and defines every
effect as a pure function of the sample index -- noise from an integer hash,
box-filtered to band-limit it and to take the DC out -- so each voice can be
rendered and measured by the headless tests. `audio/synth.cpp` bakes those into
44.1 kHz sample buffers, soft-clips them for headroom, and drives the looping
soundtrack through a raylib `AudioStream`. If the machine has no audio device the
whole layer becomes a no-op and the game runs silently.

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

**Asset bakers**   regenerate the two committed asset tables. Neither is needed
for an ordinary build -- the generated files are committed, which keeps the
build free of any Python dependency:

```bash
python3 tools/gen_font.py                # assets/fonts/*.bdf  -> font_data.hpp
python3 tools/gen_music.py track.wav     # a loop             -> music_data.cpp
```

Where those assets come from, and under what licences, is in
**[assets/CREDITS.md](assets/CREDITS.md)**.

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
