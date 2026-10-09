#pragma once

#include "raylib.h"
#include "render/pixelart.hpp"

namespace cashyphus::art {

// Every character frame the game can draw. Four life stages, three poses,
// plus the two one-off poses (collapsing at the summit, walking away).
enum class SpriteId {
    IdleYouth, IdleAdult, IdleOld, IdleFinal,
    PushYouth, PushAdult, PushOld, PushFinal,
    BraceYouth, BraceAdult, BraceOld, BraceFinal,
    Collapse,
    Walk,
    Count,
};

constexpr int CHAR_W = 12;
constexpr int CHAR_H = 18;

// Owns every baked texture for the human character.
class SpriteBank {
public:
    void load();
    void unload();

    Texture2D get(SpriteId id) const { return tex_[static_cast<int>(id)]; }

    // Convenience: pick a stage/pose texture for the game state.
    Texture2D pose(int stageIndex, int poseIndex) const;

    // Exposed for the sprite dumper / validation.
    static int validateAll();

    // Build a CPU-side image for any sprite without touching the GPU. Used by
    // the sprite dumper so the art can be previewed with no window.
    static Image buildImage(SpriteId id);

private:
    Texture2D tex_[static_cast<int>(SpriteId::Count)]{};
};

} // namespace cashyphus::art
