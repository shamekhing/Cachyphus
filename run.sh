#!/usr/bin/env bash
#
# Build (if needed) and run CASHYPHUS.
#
#   ./run.sh              build if stale, then play (native, debug)
#   ./run.sh --release    same, but an optimised build
#   ./run.sh --fresh      wipe the build tree first (use after CMake changes)
#   ./run.sh --tests      build and run the headless test suite, don't play
#   ./run.sh --web        build the WebAssembly version and serve it locally
#   ./run.sh --smoke      build it, then load it in headless Chromium and fail
#                         if the page has errors or the canvas never draws
#   ./run.sh --port 9000  serve --web on another port
#   ./run.sh --itch       build it, then pack a zip to put on itch.io
#
# The game loads its audio from assets/audio at runtime; run this script from
# any directory to keep the asset path rooted at the repository. --web additionally
# needs the Emscripten SDK (emcmake) on PATH.
#
set -euo pipefail

cd "$(dirname "$0")"

PRESET=debug
CMD=run
PORT=8000
FRESH=0
JOBS="$(nproc 2>/dev/null || echo 4)"

usage() { sed -n '3,18p' "$0" | sed 's/^#\{1,\} \{0,1\}//'; }

while [ $# -gt 0 ]; do
  case "$1" in
    --debug)   PRESET=debug ;;
    --release) PRESET=release ;;
    --fresh)   FRESH=1 ;;
    --tests)   CMD=tests ;;
    --web)     CMD=web ;;
    --smoke)   CMD=smoke ;;
    --itch)    CMD=itch ;;
    --port)    PORT="${2:?--port needs a number}"; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "run.sh: unknown option '$1'" >&2; usage >&2; exit 2 ;;
  esac
  shift
done

if [ "$FRESH" = 1 ]; then
  echo "==> removing build directories"
  rm -rf build build-web
fi

# --- build -------------------------------------------------------------------
if [ "$CMD" = web ] || [ "$CMD" = smoke ] || [ "$CMD" = itch ]; then
  command -v emcmake >/dev/null 2>&1 || {
    echo "run.sh: emcmake not found. Install the Emscripten SDK and source its env." >&2
    exit 1
  }
  echo "==> configuring web build"
  emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release \
    -DCASHYPHUS_BUILD_TESTS=OFF -DCASHYPHUS_DUMP_SPRITES=OFF >/dev/null
  echo "==> building web"
  cmake --build build-web -j"$JOBS"
else
  echo "==> configuring ($PRESET)"
  cmake --preset "$PRESET" >/dev/null
  echo "==> building ($PRESET)"
  cmake --build --preset "$PRESET" -j"$JOBS"
fi

# --- run ---------------------------------------------------------------------
case "$CMD" in
  tests)
    ctest --test-dir "build/$PRESET" --output-on-failure
    ;;

  web)
    # Content-hash the js/wasm so a cached page can never pair them with the
    # wrong copy of each other.
    python3 tools/stage_site.py build-web build-web/site
    echo
    echo "==> http://localhost:$PORT/     (Ctrl-C to stop)"
    echo "    Open it over http, not file:// -- the browser will not"
    echo "    instantiate the wasm from a file URL."
    cd build-web/site && exec python3 -m http.server "$PORT"
    ;;

  smoke)
    python3 tools/stage_site.py build-web build-web/site
    echo "==> running the browser smoke test"
    node tools/smoke_web.js build-web/site
    ;;

  itch)
    # The self-contained shape: the game itself, nothing fetched from anywhere
    # else. For the couple of kilobytes that play the published copy instead,
    # run pack_itch.py without --self-contained.
    python3 tools/pack_itch.py --self-contained build-web
    ;;

  run)
    if [ ! -x "build/$PRESET/cashyphus" ]; then
      echo "run.sh: build/$PRESET/cashyphus is missing" >&2
      exit 1
    fi
    echo "==> launching (left-click push, hold right-click brace; SPACE/SHIFT keys)"
    exec "build/$PRESET/cashyphus"
    ;;
esac
