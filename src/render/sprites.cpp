#include "render/sprites.hpp"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace cashyphus::art {

namespace {

// =============================================================================
//  Character char-maps (12 x 18)
//  '.' transparent   'o' outline   'h' hair     's' skin    'S' skin shadow
//  'c' cloth         'C' cloth dark  'b' boot   'w' highlight
// =============================================================================

// Standing, arms at the sides.
const char* const kIdle[] = {
    "............",
    "....oooo....",
    "...ohhhho...",
    "...hhhhhh...",
    "...hssssh...",
    "...osssso...",
    "....osso....",
    "..occcccco..",
    "..occcccco..",
    "..occcccco..",
    "..occcccco..",
    "...oCCCCo...",
    "...oc..co...",
    "...oc..co...",
    "...oc..co...",
    "...oc..co...",
    "..obb..bbo..",
    "............",
};

// Both arms thrust forward into the ball.
const char* const kPush[] = {
    "............",
    "....oooo....",
    "...ohhhho...",
    "...hhhhhh...",
    "...hssssh...",
    "...osssso...",
    "....osso....",
    "..occcccco..",
    "..occccccoss",
    "..occccccoss",
    "..occcccco..",
    "...oCCCCo...",
    "...oc..co...",
    "...oc..co...",
    "...oc..co...",
    "...oc..co...",
    "..obb..bbo..",
    "............",
};

// Crouched, feet planted wide, shoulders down.
const char* const kBrace[] = {
    "............",
    "............",
    "....oooo....",
    "...ohhhho...",
    "...hhhhhh...",
    "...hssssh...",
    "...osssso...",
    "....osso....",
    "..occcccco..",
    "..occcccco..",
    "..occcccco..",
    "..occcccco..",
    "...oCCCCo...",
    "...oc..co...",
    "..oc....co..",
    "..oc....co..",
    ".obb....bbo.",
    "............",
};

// Mid-stride, used for walking in and walking away.
const char* const kWalk[] = {
    "............",
    "....oooo....",
    "...ohhhho...",
    "...hhhhhh...",
    "...hssssh...",
    "...osssso...",
    "....osso....",
    "..occcccco..",
    "..occcccco..",
    "..occcccco..",
    "..occcccco..",
    "...oCCCCo...",
    "...oc..co...",
    "..oc....co..",
    ".oc......co.",
    "..oc....co..",
    "..ob....bo..",
    "............",
};

// Collapsed heap at the summit.
const char* const kCollapse[] = {
    "............",
    "............",
    "............",
    "............",
    "............",
    "............",
    "............",
    "............",
    "............",
    "............",
    "............",
    "............",
    "..ooooooo...",
    ".ohhcsssco..",
    ".ossscsssso.",
    "..ooooooooo.",
    "............",
    "............",
};

// =============================================================================
//  Palette
// =============================================================================
struct StageColors {
    Color hair;
    Color cloth;
    Color clothDark;
};

StageColors stageColors(int stage) {
    switch (stage) {
        case 0:  return { Color{ 86,  52,  34, 255}, Color{ 72,  96, 140, 255}, Color{ 48,  66, 100, 255} };
        case 1:  return { Color{ 64,  44,  32, 255}, Color{ 64, 104, 110, 255}, Color{ 42,  74,  80, 255} };
        case 2:  return { Color{168, 166, 162, 255}, Color{ 96,  94,  72, 255}, Color{ 66,  64,  48, 255} };
        default: return { Color{228, 228, 226, 255}, Color{ 92,  86,  80, 255}, Color{ 62,  58,  54, 255} };
    }
}

void buildPalette(int stage, Color out[CHAR_PALETTE_SIZE]) {
    const StageColors sc = stageColors(stage);
    out[0]  = Color{ 28,  24,  30, 255};   // outline
    out[1]  = sc.hair;                     // hair
    out[2]  = Color{226, 178, 140, 255};   // skin
    out[3]  = Color{176, 130,  96, 255};   // skin shadow
    out[4]  = sc.cloth;                    // cloth
    out[5]  = sc.clothDark;                // cloth dark
    out[6]  = Color{ 48,  36,  28, 255};   // boot
    out[7]  = Color{244, 238, 226, 255};   // highlight
    out[8]  = Color{ 20,  18,  22, 255};   // black
    out[9]  = Color{226, 186,  78, 255};   // gold
    out[10] = Color{150, 112,  44, 255};   // gold dark
    out[11] = Color{240, 240, 236, 255};   // white
}

// Lean the upper body forward to sell "old age" without authoring extra
// frames. Rows near the head move furthest; the legs stay planted.
void hunch(Image& img, int maxShift) {
    if (maxShift <= 0 || img.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) return;
    unsigned char* data = static_cast<unsigned char*>(img.data);
    const int stride = img.width * 4;
    const int topRows = 12;   // head + torso

    for (int y = 0; y < topRows; ++y) {
        const int shift = static_cast<int>(std::lround(
            static_cast<float>(maxShift) * (topRows - 1 - y) / (topRows - 1)));
        if (shift <= 0) continue;
        unsigned char* row = data + y * stride;
        for (int x = img.width - 1; x >= 0; --x) {
            const int sx = x - shift;
            unsigned char px[4] = { 0, 0, 0, 0 };
            if (sx >= 0) std::memcpy(px, row + sx * 4, 4);
            std::memcpy(row + x * 4, px, 4);
        }
    }
}

Image bakeImage(const char* const* rows, int stage) {
    Color pal[CHAR_PALETTE_SIZE];
    buildPalette(stage, pal);
    Image img = imageFromCharMap(CharMap{ rows, CHAR_W, CHAR_H }, pal);
    // Gray hair reads as "old"; a stoop reads as old AND tired.
    const int lean = (stage == 2) ? 1 : (stage == 3) ? 2 : 0;
    if (rows != kCollapse) hunch(img, lean);
    return img;
}

Texture2D bake(const char* const* rows, int stage) {
    Image img = bakeImage(rows, stage);
    Texture2D tex = LoadTextureFromImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    UnloadImage(img);
    return tex;
}

} // namespace

using S = SpriteId;

void SpriteBank::load() {
    auto& t = tex_;
    t[static_cast<int>(S::IdleYouth)] = bake(kIdle, 0);
    t[static_cast<int>(S::IdleAdult)] = bake(kIdle, 1);
    t[static_cast<int>(S::IdleOld)]   = bake(kIdle, 2);
    t[static_cast<int>(S::IdleFinal)] = bake(kIdle, 3);

    t[static_cast<int>(S::PushYouth)] = bake(kPush, 0);
    t[static_cast<int>(S::PushAdult)] = bake(kPush, 1);
    t[static_cast<int>(S::PushOld)]   = bake(kPush, 2);
    t[static_cast<int>(S::PushFinal)] = bake(kPush, 3);

    t[static_cast<int>(S::BraceYouth)] = bake(kBrace, 0);
    t[static_cast<int>(S::BraceAdult)] = bake(kBrace, 1);
    t[static_cast<int>(S::BraceOld)]   = bake(kBrace, 2);
    t[static_cast<int>(S::BraceFinal)] = bake(kBrace, 3);

    t[static_cast<int>(S::Collapse)] = bake(kCollapse, 3);
    t[static_cast<int>(S::Walk)]     = bake(kWalk, 0);   // always the young walker
}

void SpriteBank::unload() {
    for (auto& t : tex_) {
        if (t.id != 0) UnloadTexture(t);
    }
}

Texture2D SpriteBank::pose(int stageIndex, int poseIndex) const {
    static const SpriteId table[4][3] = {
        { S::IdleYouth, S::PushYouth, S::BraceYouth },
        { S::IdleAdult, S::PushAdult, S::BraceAdult },
        { S::IdleOld,   S::PushOld,   S::BraceOld   },
        { S::IdleFinal, S::PushFinal, S::BraceFinal },
    };
    const int s = stageIndex < 0 ? 0 : (stageIndex > 3 ? 3 : stageIndex);
    const int p = poseIndex  < 0 ? 0 : (poseIndex  > 2 ? 2 : poseIndex);
    return tex_[static_cast<int>(table[s][p])];
}

Image SpriteBank::buildImage(SpriteId id) {
    switch (id) {
        case S::IdleYouth: return bakeImage(kIdle, 0);
        case S::IdleAdult: return bakeImage(kIdle, 1);
        case S::IdleOld:   return bakeImage(kIdle, 2);
        case S::IdleFinal: return bakeImage(kIdle, 3);
        case S::PushYouth: return bakeImage(kPush, 0);
        case S::PushAdult: return bakeImage(kPush, 1);
        case S::PushOld:   return bakeImage(kPush, 2);
        case S::PushFinal: return bakeImage(kPush, 3);
        case S::BraceYouth: return bakeImage(kBrace, 0);
        case S::BraceAdult: return bakeImage(kBrace, 1);
        case S::BraceOld:   return bakeImage(kBrace, 2);
        case S::BraceFinal: return bakeImage(kBrace, 3);
        case S::Collapse:   return bakeImage(kCollapse, 3);
        case S::Walk:       return bakeImage(kWalk, 0);
        default:            return GenImageColor(CHAR_W, CHAR_H, BLANK);
    }
}

int SpriteBank::validateAll() {
    const CharMap maps[] = {
        { kIdle,     CHAR_W, CHAR_H },
        { kPush,     CHAR_W, CHAR_H },
        { kBrace,    CHAR_W, CHAR_H },
        { kWalk,     CHAR_W, CHAR_H },
        { kCollapse, CHAR_W, CHAR_H },
    };
    const char* names[] = { "idle", "push", "brace", "walk", "collapse" };
    int bad = 0;
    for (std::size_t i = 0; i < sizeof(maps) / sizeof(maps[0]); ++i) {
        const int b = validateCharMap(maps[i]);
        if (b != 0) {
            std::printf("  charmap '%s' has %d malformed row(s)\n", names[i], b);
            ++bad;
        }
    }
    return bad;
}

} // namespace cashyphus::art
