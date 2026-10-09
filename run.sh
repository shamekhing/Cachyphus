#!/usr/bin/env bash
#
# Build (if needed) and run CASHYPHUS.
#
#   ./run.sh              build if stale, then play (native, debug)
#   ./run.sh --release    same, but an optimised build
#   ./run.sh --fresh      wipe the build tree first (use after CMake changes)
#   ./run.sh --tests      build and run the headless test suite, don't play
#   ./run.sh --web        build the WebAssembly version and serve it locally
#   ./run.sh --port 9000  serve --web on another port
#
# Nothing needs installing beyond a compiler, CMake and OpenGL headers: raylib
# is fetched on the first build, and every sprite and sound is generated in
# code, so there is no asset directory to find at runtime. --web additionally
# needs the Emscripten SDK (emcmake) on PATH.
#
set -euo pipefail

cd "$(dirname "$0")"

PRESET=debug
CMD=run
PORT=8000
FRESH=0
JOBS="$(nproc 2>/dev/null || echo 4)"

usage() { sed -n '3,17p' "$0" | sed 's/^#\{1,\} \{0,1\}//'; }

while [ $# -gt 0 ]; do
  case "$1" in
    --debug)   PRESET=debug ;;
    --release) PRESET=release ;;
    --fresh)   FRESH=1 ;;
    --tests)   CMD=tests ;;
    --web)     CMD=web ;;
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
if [ "$CMD" = web ]; then
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
    # Pages serves index.html at the site root and the HTML loads its sibling
    # cashyphus.js by relative path, so staging is exactly a rename.
    SITE=build-web/site
    mkdir -p "$SITE"
    cp build-web/cashyphus.html "$SITE/index.html"
    cp build-web/cashyphus.js   "$SITE/"
    cp build-web/cashyphus.wasm "$SITE/"
    echo
    echo "==> http://localhost:$PORT/     (Ctrl-C to stop)"
    echo "    Open it over http, not file:// -- the browser will not"
    echo "    instantiate the wasm from a file URL."
    cd "$SITE" && exec python3 -m http.server "$PORT"
    ;;

  run)
    if [ ! -x "build/$PRESET/cashyphus" ]; then
      echo "run.sh: build/$PRESET/cashyphus is missing" >&2
      exit 1
    fi
    echo "==> launching (SPACE push, SHIFT brace, F fullscreen)"
    exec "build/$PRESET/cashyphus"
    ;;
esac
