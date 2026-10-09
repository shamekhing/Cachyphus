// Renders every baked character sprite into a single PNG so the pixel art can
// be inspected without running the game (and without a GPU or a window).
//
//   ./cashyphus_dump_sprites [output.png]      (default: sprites_preview.png)

#include <cstdio>

#include "raylib.h"
#include "render/sprites.hpp"

using namespace cashyphus::art;

int main(int argc, char** argv) {
    const char* out = (argc > 1) ? argv[1] : "sprites_preview.png";

    const int badRows = SpriteBank::validateAll();
    std::printf("sprite validation: %d malformed sheet(s)\n", badRows);

    const int pad = 2;
    const int cellW = CHAR_W + pad * 2;
    const int cellH = CHAR_H + pad * 2;

    // Rows: idle(4), push(4), brace(4), collapse+walk(2)
    const SpriteId sheet[4][4] = {
        { SpriteId::IdleYouth,  SpriteId::IdleAdult,  SpriteId::IdleOld,  SpriteId::IdleFinal },
        { SpriteId::PushYouth,  SpriteId::PushAdult,  SpriteId::PushOld,  SpriteId::PushFinal },
        { SpriteId::BraceYouth, SpriteId::BraceAdult, SpriteId::BraceOld, SpriteId::BraceFinal },
        { SpriteId::Collapse,   SpriteId::Walk,       SpriteId::Count,    SpriteId::Count },
    };

    const int cols = 4, rows = 4;
    Image canvas = GenImageColor(cols * cellW, rows * cellH, Color{ 46, 42, 56, 255 });

    // A faint checker so transparent pixels are obvious.
    for (int y = 0; y < canvas.height; ++y) {
        for (int x = 0; x < canvas.width; ++x) {
            if (((x / 6) + (y / 6)) % 2 == 0) {
                ImageDrawPixel(&canvas, x, y, Color{ 56, 52, 68, 255 });
            }
        }
    }

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (sheet[r][c] == SpriteId::Count) continue;
            Image spr = SpriteBank::buildImage(sheet[r][c]);
            const Rectangle src{ 0, 0, (float)CHAR_W, (float)CHAR_H };
            const Rectangle dst{ (float)(c * cellW + pad), (float)(r * cellH + pad),
                                 (float)CHAR_W, (float)CHAR_H };
            ImageDraw(&canvas, spr, src, dst, WHITE);
            UnloadImage(spr);
        }
    }

    const int scale = 7;
    ImageResizeNN(&canvas, canvas.width * scale, canvas.height * scale);
    ExportImage(canvas, out);
    std::printf("wrote %s (%dx%d)\n", out, canvas.width, canvas.height);

    UnloadImage(canvas);
    return badRows == 0 ? 0 : 1;
}
