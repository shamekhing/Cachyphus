# CASHYPHUS

[![CI](https://github.com/shamekhing/Cachyphus/actions/workflows/ci.yml/badge.svg)](https://github.com/shamekhing/Cachyphus/actions/workflows/ci.yml)
[![Pages](https://github.com/shamekhing/Cachyphus/actions/workflows/pages.yml/badge.svg)](https://github.com/shamekhing/Cachyphus/actions/workflows/pages.yml)
[![Play in your browser](https://img.shields.io/badge/play-in%20your%20browser-brightgreen)](https://shamekhing.github.io/Cachyphus/)

**One hill. One lifetime. One more push.**

A pixel-art allegory: a person spends their whole life pushing a giant, talking
ball of money up a hill. They age as they climb. At the summit they die. The ball
rolls back down. Someone else begins. After four lives, the game finally offers
you a choice: **keep pushing, or walk away.**

Built with **C++17 + raylib**. Every sprite and every sound is generated in code,
so the repository contains no binary assets at all.

---

## Controls

| Key | Action |
|---|---|
| **SPACE** (or Up / Left-mouse) | Push shove the ball uphill, spending stamina |
| **SHIFT** (or Down) | Brace / hold the ball still and recover stamina, spending grip |
| **SPACE** on the choice screen | Keep pushing (the cycle continues) |
| **SHIFT** on the choice screen | Walk away (the escape ending) |

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
`cashyphus.html` + `cashyphus.js` + `cashyphus.wasm`, about 580 KB in total.

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
auto-played run through the state machine (four lives -> the choice -> both
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
  render/     raylib drawing
    pixelart.hpp/.cpp    char-map + palette -> texture (and CPU images)
    sprites.hpp/.cpp     the character sprite sheets + hunch-on-aging
    scene.hpp/.cpp       hill, parallax, ball, character, speech bubble
    hud.hpp/.cpp         meters, counters, title/choice/ending overlays
  audio/
    synth.hpp/.cpp   procedural SFX + a looping sequenced soundtrack
  main.cpp      window, 320x180 render target, input, fixed-timestep loop
tests/          headless test binary
tools/          sprite-sheet preview dumper
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

**Rendering** draws everything into a 320x180 render target with point
filtering, then blits it to the window at an integer scale with letterboxing, so
the pixel art stays crisp at any window size.

**Audio** has no files: `audio/synth.cpp` fills sample buffers at 44.1 kHz to
build every effect, and drives a small step-sequencer through a raylib
`AudioStream` for the looping soundtrack. If the machine has no audio device the
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

---

## Balancing

Everything tunable lives in `src/core/config.hpp`: push impulse, stamina cost
and the push cooldown, gravity and damping, the slope curve, grip
drain/recovery, the four aging stages, the per-cycle weight ramp
(`CYCLE_GRAVITY_STEP` / `CYCLE_GRAVITY_CAP`), phase durations and the dialogue
pacing.

Change a number, then run `cashyphus_tests`: it prints the climb times at 6, 4
and 3 presses/second, plus the high-water mark a pure key-masher can reach, so
you can see exactly what a tuning change did before playing.

---

## Credits

Inspired by the myth of Sisyphus. raylib by Ramon Santamaria and contributors.
