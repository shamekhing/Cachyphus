// =============================================================================
//  cashyphus_decode -- decode any audio raylib understands to a plain 16-bit
//  WAV.
//
//  raylib already carries decoders for WAV, OGG, MP3, FLAC and QOA, so this
//  turns an asset we have picked from a licence point of view into something
//  that can be measured offline (and listened to) with no extra toolchain.
//  Used by tools/gen_music.py's authoring loop; never shipped.
//
//      cashyphus_decode <input> <output.wav>
// =============================================================================
#include <cstdio>

#include "raylib.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <input> <output.wav>\n", argv[0]);
        return 2;
    }

    Wave w = LoadWave(argv[1]);
    if (w.frameCount == 0 || w.data == nullptr) {
        std::fprintf(stderr, "could not decode: %s\n", argv[1]);
        return 1;
    }

    std::printf("decoded %s: %d Hz, %d ch, %u frames, %.2f s\n",
                argv[1], w.sampleRate, w.channels, w.frameCount,
                static_cast<double>(w.frameCount) / w.sampleRate);

    const bool ok = ExportWave(w, argv[2]);
    UnloadWave(w);

    if (!ok) {
        std::fprintf(stderr, "could not write: %s\n", argv[2]);
        return 1;
    }
    std::printf("wrote %s\n", argv[2]);
    return 0;
}
