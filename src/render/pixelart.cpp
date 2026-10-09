#include "render/pixelart.hpp"

namespace cashyphus::art {

int charIndex(char c) {
    switch (c) {
        case 'o': return 0;   // outline
        case 'h': return 1;   // hair
        case 's': return 2;   // skin
        case 'S': return 3;   // skin shadow
        case 'c': return 4;   // cloth
        case 'C': return 5;   // cloth dark
        case 'b': return 6;   // boot
        case 'w': return 7;   // highlight
        case 'k': return 8;   // black / eye
        case 'g': return 9;   // gold
        case 'G': return 10;  // gold dark
        case 'x': return 11;  // white / eye
        default:  return -1;  // '.' and anything else = transparent
    }
}

int validateCharMap(const CharMap& cm) {
    int bad = 0;
    for (int y = 0; y < cm.height; ++y) {
        const char* row = cm.rows[y];
        int len = 0;
        while (row[len] != '\0') ++len;
        if (len != cm.width) ++bad;
    }
    return bad;
}

Image imageFromCharMap(const CharMap& cm, const Color* palette) {
    Image img = GenImageColor(cm.width, cm.height, BLANK);
    for (int y = 0; y < cm.height; ++y) {
        const char* row = cm.rows[y];
        for (int x = 0; x < cm.width; ++x) {
            const char c = row[x];
            if (c == '\0') break;             // short row -> rest is transparent
            const int idx = charIndex(c);
            if (idx < 0 || idx >= CHAR_PALETTE_SIZE) continue;
            ImageDrawPixel(&img, x, y, palette[idx]);
        }
    }
    return img;
}

Texture2D textureFromCharMap(const CharMap& cm, const Color* palette) {
    Image img = imageFromCharMap(cm, palette);
    Texture2D tex = LoadTextureFromImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    UnloadImage(img);
    return tex;
}

} // namespace cashyphus::art
