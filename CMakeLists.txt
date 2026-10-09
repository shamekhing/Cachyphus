cmake_minimum_required(VERSION 3.20)
project(cashyphus LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
#  Options
# ---------------------------------------------------------------------------
option(CASHYPHUS_BUILD_GAME   "Build the raylib game executable" ON)
option(CASHYPHUS_BUILD_TESTS  "Build the headless unit tests"    ON)
option(CASHYPHUS_DUMP_SPRITES "Build the sprite PNG dumper tool" ON)
option(USE_SYSTEM_RAYLIB      "Use an installed raylib instead of fetching one" OFF)
set(RAYLIB_TAG "5.5" CACHE STRING "raylib git tag to fetch (e.g. 5.5 or 6.0)")

# raylib needs to be told which backend to use. Under Emscripten that is "Web";
# without this it tries to configure desktop GLFW and fails looking for X11.
if(EMSCRIPTEN AND NOT DEFINED PLATFORM)
    set(PLATFORM "Web" CACHE STRING "raylib platform backend" FORCE)
endif()

# raylib brings its own windowing/audio; examples and games are not needed.
set(BUILD_EXAMPLES            OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES               OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS         OFF CACHE BOOL "" FORCE)
set(SUPPORT_MODULE_RMODELS    OFF CACHE BOOL "" FORCE)  # 3D not used
set(SUPPORT_MODULE_RAUDIO     ON  CACHE BOOL "" FORCE)

# ---------------------------------------------------------------------------
#  Core: pure logic, no raylib. This is what the tests exercise.
# ---------------------------------------------------------------------------
add_library(cashyphus_core STATIC
    src/core/sim.cpp
    src/core/game_state.cpp
    src/core/dialogue.cpp
    src/core/palette.cpp
)
target_include_directories(cashyphus_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(cashyphus_core PRIVATE -Wall -Wextra -Wpedantic)

# ---------------------------------------------------------------------------
#  Tests
# ---------------------------------------------------------------------------
if(CASHYPHUS_BUILD_TESTS)
    enable_testing()
    add_executable(cashyphus_tests tests/test_core.cpp)
    target_link_libraries(cashyphus_tests PRIVATE cashyphus_core)
    target_compile_options(cashyphus_tests PRIVATE -Wall -Wextra)
    add_test(NAME core COMMAND cashyphus_tests)
endif()

# ---------------------------------------------------------------------------
#  raylib
# ---------------------------------------------------------------------------
if(CASHYPHUS_BUILD_GAME OR CASHYPHUS_DUMP_SPRITES)
    if(USE_SYSTEM_RAYLIB)
        find_package(raylib REQUIRED)
        set(CASHYPHUS_RAYLIB raylib)
    else()
        include(FetchContent)
        FetchContent_Declare(raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG        ${RAYLIB_TAG}
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(raylib)
        set(CASHYPHUS_RAYLIB raylib)
    endif()
endif()

# ---------------------------------------------------------------------------
#  Game executable
# ---------------------------------------------------------------------------
if(CASHYPHUS_BUILD_GAME)
    add_executable(cashyphus
        src/main.cpp
        src/render/pixelart.cpp
        src/render/scene.cpp
        src/render/hud.cpp
        src/render/sprites.cpp
        src/render/font.cpp
        src/audio/synth.cpp
    )
    target_link_libraries(cashyphus PRIVATE cashyphus_core ${CASHYPHUS_RAYLIB})
    target_compile_options(cashyphus PRIVATE -Wall -Wextra)

    if(EMSCRIPTEN)
        # MINIFY_HTML=0 avoids a hard dependency on html-minifier-terser, which
        # is frequently absent on minimal Emscripten installs.
        set_target_properties(cashyphus PROPERTIES
            SUFFIX ".html"
            LINK_FLAGS "-sUSE_GLFW=3 -sASSERTIONS=1 -sALLOW_MEMORY_GROWTH=1 -sMINIFY_HTML=0 --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
        # Relink when the HTML shell changes.
        set_property(TARGET cashyphus APPEND PROPERTY
            LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
    endif()
endif()

# ---------------------------------------------------------------------------
#  Sprite dumper: renders the procedural sprite sheet to a PNG so the pixel art
#  can be eyeballed without a window or GPU. Never shipped.
# ---------------------------------------------------------------------------
if(CASHYPHUS_DUMP_SPRITES)
    add_executable(cashyphus_dump_sprites
        tools/dump_sprites.cpp
        src/render/pixelart.cpp
        src/render/sprites.cpp
    )
    target_link_libraries(cashyphus_dump_sprites PRIVATE cashyphus_core ${CASHYPHUS_RAYLIB})
    target_compile_options(cashyphus_dump_sprites PRIVATE -Wall -Wextra)
endif()
