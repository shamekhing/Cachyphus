#include "core/palette.hpp"

#include "core/config.hpp"

namespace cashyphus {

RGB lerpRGB(RGB a, RGB b, float t) {
    t = cfg::clampf(t, 0.0f, 1.0f);
    return RGB{
        static_cast<std::uint8_t>(a.r + (b.r - a.r) * t),
        static_cast<std::uint8_t>(a.g + (b.g - a.g) * t),
        static_cast<std::uint8_t>(a.b + (b.b - a.b) * t),
    };
}

RGB shade(RGB c, float amount) {
    const float f = 1.0f + amount;
    auto ch = [](float v) -> std::uint8_t {
        return static_cast<std::uint8_t>(cfg::clampf(v, 0.0f, 255.0f));
    };
    return RGB{ ch(c.r * f), ch(c.g * f), ch(c.b * f) };
}

namespace {

Palette make(const RGB& skyTop, const RGB& skyMid, const RGB& skyBot,
             const RGB& sun, const RGB& mtnFar, const RGB& mtnNear,
             const RGB& hillFar, const RGB& hillNear, const RGB& hillEdge, const RGB& path,
             const RGB& bLight, const RGB& bMid, const RGB& bDark,
             const RGB& bShadow, const RGB& bBand, const RGB& bFace,
             const RGB& skin, const RGB& skinDark, const RGB& hair,
             const RGB& cloth, const RGB& clothDark, const RGB& outline,
             const RGB& uiText, const RGB& uiDim,
             const RGB& uiGood, const RGB& uiWarn, const RGB& uiBad) {
    Palette p{};
    p.skyTop = skyTop; p.skyMid = skyMid; p.skyBot = skyBot;
    p.sun = sun;
    p.mountainFar = mtnFar; p.mountainNear = mtnNear;
    p.hillFar = hillFar; p.hillNear = hillNear; p.hillEdge = hillEdge; p.path = path;
    p.ballLight = bLight; p.ballMid = bMid; p.ballDark = bDark;
    p.ballShadow = bShadow; p.ballBand = bBand; p.ballFace = bFace;
    p.skin = skin; p.skinDark = skinDark; p.hair = hair;
    p.cloth = cloth; p.clothDark = clothDark; p.outline = outline;
    p.bubbleBg = rgb(18, 16, 22); p.bubbleEdge = rgb(240, 236, 220); p.text = rgb(238, 234, 220);
    p.uiText = uiText; p.uiDim = uiDim;
    p.uiGood = uiGood; p.uiWarn = uiWarn; p.uiBad = uiBad;
    return p;
}

// Warm, sunlit youth: amber sky, green-gold hill, bright gold money.
Palette warm() {
    return make(
        rgb(126, 168, 206), rgb(196, 186, 150), rgb(232, 204, 150),   // sky
        rgb(248, 226, 150),                                            // sun
        rgb(150, 158, 168), rgb(120, 138, 132),                        // mountains
        rgb(120, 138, 92), rgb(96, 118, 66), rgb(74, 92, 48), rgb(150, 132, 92), // hill
        rgb(255, 232, 150), rgb(226, 186, 78), rgb(150, 112, 44),      // ball
        rgb(96, 70, 30), rgb(246, 226, 170), rgb(40, 30, 18),          // ball extras
        rgb(226, 178, 140), rgb(178, 130, 96), rgb(96, 62, 40),        // skin/hair
        rgb(96, 118, 150), rgb(66, 84, 112), rgb(28, 24, 30),          // cloth/outline
        rgb(238, 234, 220), rgb(150, 146, 136),
        rgb(120, 190, 120), rgb(226, 190, 90), rgb(206, 96, 84));
}

// Cold, drained summit: desaturated slate sky, sickly hill, dim brass money.
Palette cold() {
    return make(
        rgb(58, 66, 86), rgb(74, 78, 92), rgb(104, 100, 104),          // sky
        rgb(186, 182, 168),                                            // sun
        rgb(70, 74, 88), rgb(56, 60, 72),                              // mountains
        rgb(74, 74, 72), rgb(58, 58, 56), rgb(40, 40, 40), rgb(78, 76, 70), // hill
        rgb(178, 168, 128), rgb(132, 120, 82), rgb(78, 70, 48),        // ball
        rgb(44, 40, 30), rgb(150, 142, 112), rgb(30, 28, 24),          // ball extras
        rgb(186, 168, 156), rgb(140, 122, 110), rgb(196, 192, 186),    // skin/hair (gray)
        rgb(72, 74, 82), rgb(48, 50, 58), rgb(20, 20, 24),             // cloth/outline
        rgb(214, 212, 206), rgb(122, 120, 116),
        rgb(110, 150, 120), rgb(186, 168, 100), rgb(178, 92, 84));
}

// Life after the hill: warm gold light, living green, blue sky.
Palette walkColors() {
    return make(
        rgb(112, 158, 206), rgb(178, 200, 214), rgb(232, 226, 190),    // sky
        rgb(252, 236, 170),                                            // sun
        rgb(150, 168, 176), rgb(122, 148, 136),                        // mountains
        rgb(104, 150, 84), rgb(84, 132, 66), rgb(58, 100, 48), rgb(166, 148, 104), // hill
        rgb(206, 200, 188), rgb(160, 152, 140), rgb(104, 98, 90),      // ball (tarnished)
        rgb(80, 76, 68), rgb(178, 170, 156), rgb(40, 38, 36),
        rgb(232, 188, 150), rgb(186, 140, 104), rgb(104, 70, 44),
        rgb(120, 152, 176), rgb(86, 114, 142), rgb(28, 24, 30),
        rgb(242, 238, 224), rgb(140, 138, 128),
        rgb(120, 200, 120), rgb(232, 196, 96), rgb(200, 100, 88));
}

void lerpField(RGB& out, const RGB& a, const RGB& b, float t) { out = lerpRGB(a, b, t); }

} // namespace

Palette climbPalette(float progress) {
    const float p = cfg::clampf(progress, 0.0f, 1.0f);
    return blendPalette(warm(), cold(), p);
}

Palette walkPalette() { return walkColors(); }

Palette blendPalette(const Palette& a, const Palette& b, float t) {
    t = cfg::clampf(t, 0.0f, 1.0f);
    Palette p{};
    lerpField(p.skyTop, a.skyTop, b.skyTop, t);
    lerpField(p.skyMid, a.skyMid, b.skyMid, t);
    lerpField(p.skyBot, a.skyBot, b.skyBot, t);
    lerpField(p.sun, a.sun, b.sun, t);
    lerpField(p.mountainFar, a.mountainFar, b.mountainFar, t);
    lerpField(p.mountainNear, a.mountainNear, b.mountainNear, t);
    lerpField(p.hillFar, a.hillFar, b.hillFar, t);
    lerpField(p.hillNear, a.hillNear, b.hillNear, t);
    lerpField(p.hillEdge, a.hillEdge, b.hillEdge, t);
    lerpField(p.path, a.path, b.path, t);
    lerpField(p.ballLight, a.ballLight, b.ballLight, t);
    lerpField(p.ballMid, a.ballMid, b.ballMid, t);
    lerpField(p.ballDark, a.ballDark, b.ballDark, t);
    lerpField(p.ballShadow, a.ballShadow, b.ballShadow, t);
    lerpField(p.ballBand, a.ballBand, b.ballBand, t);
    lerpField(p.ballFace, a.ballFace, b.ballFace, t);
    lerpField(p.skin, a.skin, b.skin, t);
    lerpField(p.skinDark, a.skinDark, b.skinDark, t);
    lerpField(p.hair, a.hair, b.hair, t);
    lerpField(p.cloth, a.cloth, b.cloth, t);
    lerpField(p.clothDark, a.clothDark, b.clothDark, t);
    lerpField(p.outline, a.outline, b.outline, t);
    lerpField(p.bubbleBg, a.bubbleBg, b.bubbleBg, t);
    lerpField(p.bubbleEdge, a.bubbleEdge, b.bubbleEdge, t);
    lerpField(p.text, a.text, b.text, t);
    lerpField(p.uiText, a.uiText, b.uiText, t);
    lerpField(p.uiDim, a.uiDim, b.uiDim, t);
    lerpField(p.uiGood, a.uiGood, b.uiGood, t);
    lerpField(p.uiWarn, a.uiWarn, b.uiWarn, t);
    lerpField(p.uiBad, a.uiBad, b.uiBad, t);
    return p;
}

} // namespace cashyphus

