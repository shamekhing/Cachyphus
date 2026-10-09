#include "render/scene.hpp"

#include <cmath>
#include <cstring>

#include "core/config.hpp"
#include "core/roll.hpp"
#include "render/pixelart.hpp"

namespace cashyphus::scene {

using namespace cfg;
using art::CHAR_H;
using art::CHAR_W;
using art::lerpColor;
using art::SpriteBank;
using art::SpriteId;
using art::toColor;

// =============================================================================
//  Geometry
// =============================================================================
float World::hillY(float x) {
    const float t = clampf(x / HILL_LEN, 0.0f, 1.0f);
    return BASE_Y - RISE * std::pow(t, 1.35f);
}

float World::ballWorldX(float progress) {
    return clampf(progress, 0.0f, 1.0f) * HILL_LEN;
}

namespace {
constexpr float CAM_MAX_X = World::HILL_LEN - 224.0f;   // keep the summit in view
}

float cameraX(const Game& g) {
    const float ballX = World::ballWorldX(g.ballProgress());
    // The lower clamp is negative so that at progress 0 the ball still sits
    // comfortably inside the frame rather than hanging off the left edge.
    constexpr float CAM_MIN_X = -96.0f;
    if (g.phase() == Phase::WalkAway) {
        // The camera follows the person walking down and away from the hill.
        const float walkX = ballX - 44.0f - g.walkT() * 520.0f;
        return clampf(walkX - 168.0f, -520.0f, CAM_MAX_X);
    }
    return clampf(ballX - 96.0f, CAM_MIN_X, CAM_MAX_X);
}

// =============================================================================
//  Text helpers (default font, point filtered so it stays crisp)
// =============================================================================
void text(const char* s, int x, int y, Color c) { DrawText(s, x, y, 10, c); }
int  textWidth(const char* s) { return MeasureText(s, 10); }
void textCentered(const char* s, int y, Color c) {
    DrawText(s, (cfg::VIRTUAL_W - MeasureText(s, 10)) / 2, y, 10, c);
}

namespace {

// -----------------------------------------------------------------------------
//  Background
// -----------------------------------------------------------------------------
void drawSky(const Palette& pal) {
    const int mid = VIRTUAL_H / 2;
    for (int y = 0; y < VIRTUAL_H; ++y) {
        const Color c = y < mid
            ? lerpColor(toColor(pal.skyTop), toColor(pal.skyMid), static_cast<float>(y) / mid)
            : lerpColor(toColor(pal.skyMid), toColor(pal.skyBot),
                        static_cast<float>(y - mid) / (VIRTUAL_H - mid));
        DrawLine(0, y, VIRTUAL_W, y, c);
    }
}

void drawSun(const Palette& pal) {
    const Color c = toColor(pal.sun);
    DrawCircle(248, 40, 13, c);
    DrawCircle(248, 40, 9, Color{ c.r, c.g, c.b, 200 });
}

void drawParallax(const Palette& pal, float camX) {
    // Two layers of triangular mountains, scrolling at different rates.
    const Color far  = toColor(pal.mountainFar);
    const Color near = toColor(pal.mountainNear);

    const float off1 = std::fmod(camX * 0.15f, 96.0f);
    for (int i = -1; i < 6; ++i) {
        const float bx = static_cast<float>(i * 96) - off1;
        DrawTriangle({ bx,         110.0f },
                     { bx + 52.0f,  62.0f },
                     { bx + 104.0f, 110.0f }, far);
    }

    const float off2 = std::fmod(camX * 0.32f, 74.0f);
    for (int i = -1; i < 7; ++i) {
        const float bx = static_cast<float>(i * 74) - off2;
        DrawTriangle({ bx,         128.0f },
                     { bx + 38.0f,  86.0f },
                     { bx + 76.0f, 128.0f }, near);
    }
}

// The hill itself: one vertical strip per screen column.
void drawHill(const Palette& pal, float camX) {
    const Color nearC = toColor(pal.hillNear);
    const Color farC  = toColor(pal.hillFar);
    const Color edge  = toColor(pal.hillEdge);
    const Color path  = toColor(pal.path);

    // A softer ridge behind the main slope.
    for (int sx = 0; sx < VIRTUAL_W; ++sx) {
        const float wy = World::hillY(camX + sx) - 16.0f;
        DrawLine(sx, static_cast<int>(wy), sx, VIRTUAL_H, farC);
    }
    for (int sx = 0; sx < VIRTUAL_W; ++sx) {
        const int y = static_cast<int>(World::hillY(camX + sx));
        DrawLine(sx, y, sx, VIRTUAL_H, nearC);
        DrawPixel(sx, y, edge);
        DrawPixel(sx, y + 1, path);
    }
}

// -----------------------------------------------------------------------------
//  The money ball
// -----------------------------------------------------------------------------
void drawMouth(int cx, int my, BallMood mood, const Palette& pal) {
    const Color line = toColor(pal.ballFace);
    const Color dark = toColor(pal.outline);

    switch (mood) {
        case BallMood::Happy:
            DrawLine(cx - 7, my,     cx - 3, my + 3, line);
            DrawLine(cx - 3, my + 3, cx + 3, my + 3, line);
            DrawLine(cx + 3, my + 3, cx + 7, my,     line);
            break;
        case BallMood::Celebrating:
            DrawRectangle(cx - 7, my - 1, 15, 6, dark);
            DrawRectangle(cx - 5, my - 1, 11, 2, toColor(pal.ballFace));
            break;
        case BallMood::Smug:
            DrawLine(cx - 6, my + 2, cx + 2, my,     line);
            DrawLine(cx + 2, my,     cx + 7, my - 2, line);
            break;
        case BallMood::Anxious:
            DrawLine(cx - 6, my + 1, cx - 2, my - 1, line);
            DrawLine(cx - 2, my - 1, cx + 2, my + 1, line);
            DrawLine(cx + 2, my + 1, cx + 6, my - 1, line);
            break;
        case BallMood::Panic:
            DrawCircle(cx, my + 1, 4, dark);
            break;
        case BallMood::Desperate:
            DrawLine(cx - 7, my + 3, cx - 3, my,     line);
            DrawLine(cx - 3, my,     cx + 3, my,     line);
            DrawLine(cx + 3, my,     cx + 7, my + 3, line);
            break;
        case BallMood::Heartbroken:
            DrawLine(cx - 7, my + 3, cx - 3, my - 1, line);
            DrawLine(cx - 3, my - 1, cx + 3, my - 1, line);
            DrawLine(cx + 3, my - 1, cx + 7, my + 3, line);
            break;
    }
}

void drawEyes(int cx, int ey, BallMood mood, const Palette& pal) {
    const Color white = toColor(pal.ballFace);
    const Color dark  = toColor(pal.outline);
    const bool wide   = (mood == BallMood::Panic || mood == BallMood::Anxious);
    const bool closed = (mood == BallMood::Heartbroken);
    const float r = wide ? 3.0f : 2.0f;

    if (closed) {
        DrawLine(cx - 11, ey, cx - 3, ey, dark);
        DrawLine(cx + 3,  ey, cx + 11, ey, dark);
        return;
    }
    for (int s = -1; s <= 1; s += 2) {
        const int ex = cx + s * 7;
        DrawCircle(ex, ey, r, white);
        int dy = 0;
        if (mood == BallMood::Desperate) dy = 1;
        if (mood == BallMood::Celebrating) dy = -1;
        DrawPixel(ex, ey + dy, dark);
        DrawPixel(ex + 1, ey + dy, dark);
        DrawPixel(ex, ey + dy + 1, dark);
    }
}

// Distance from a point to a line segment, in the ball's surface space.
bool nearSegment(float px, float py, float x1, float y1, float x2, float y2, float w) {
    const float dx = x2 - x1, dy = y2 - y1;
    const float len2 = dx * dx + dy * dy;
    float t = len2 > 0.0f ? ((px - x1) * dx + (py - y1) * dy) / len2 : 0.0f;
    t = clampf(t, 0.0f, 1.0f);
    const float ddx = px - (x1 + t * dx);
    const float ddy = py - (y1 + t * dy);
    return ddx * ddx + ddy * ddy <= w * w;
}

// The "$" stamped on the ball's surface, sitting below the face. It rotates
// with the ball, which is the main thing that sells the roll.
bool dollarGlyph(float x, float y) {
    const float gy = y - 12.0f;
    return nearSegment(x, gy,  0.0f, -7.0f,  0.0f,  7.0f, 1.1f) ||  // stem
           nearSegment(x, gy, -4.0f, -5.0f,  4.0f, -5.0f, 1.1f) ||  // top bar
           nearSegment(x, gy, -4.0f,  0.0f,  4.0f,  0.0f, 1.1f) ||  // waist
           nearSegment(x, gy, -4.0f,  5.0f,  4.0f,  5.0f, 1.1f) ||  // bottom bar
           nearSegment(x, gy, -4.0f, -5.0f, -4.0f, -1.0f, 1.1f) ||  // upper hook
           nearSegment(x, gy,  4.0f,  1.0f,  4.0f,  5.0f, 1.1f);    // lower hook
}

// A few coins embedded in the surface, so the rotation is unmistakable.
bool coinHit(float x, float y) {
    static const float kCoins[4][2] = {
        { -15.0f,   6.0f }, { 14.0f,   9.0f },
        {   7.0f, -17.0f }, { -12.0f, -13.0f },
    };
    for (const auto& c : kCoins) {
        const float dx = x - c[0], dy = y - c[1];
        if (dx * dx + dy * dy <= 6.25f) return true;   // ~2.5 px radius
    }
    return false;
}

void drawBall(float cx, float cy, float r, float spinDeg, BallMood mood,
              const Palette& pal, bool topHat, bool sunglasses) {
    const Color light  = toColor(pal.ballLight);
    const Color mid    = toColor(pal.ballMid);
    const Color dark   = toColor(pal.ballDark);
    // The "$" needs to read against the gold bands, so push it toward the
    // outline colour.
    const Color band   = lerpColor(toColor(pal.ballBand), toColor(pal.outline), 0.6f);
    const Color shadow = toColor(pal.ballShadow);

    const int R = static_cast<int>(r);
    const int icx = static_cast<int>(cx), icy = static_cast<int>(cy);

    for (int dy = -R; dy <= R; ++dy) {
        for (int dx = -R; dx <= R; ++dx) {
            const float d2 = static_cast<float>(dx * dx + dy * dy);
            if (d2 > r * r) continue;

            // Volume: light from the upper-left.
            const float lit = clampf(0.55f - (dx * 0.5f + dy * 0.8f) / (1.7f * r), 0.0f, 1.0f);
            Color c = lit > 0.5f ? lerpColor(mid, light, (lit - 0.5f) * 2.0f)
                                 : lerpColor(dark, mid, lit * 2.0f);

            // Sample the ball's own surface, which turns with the roll. The
            // banknote seams, coins and the "$" all live in this rotating space,
            // so their movement across the ball is what reads as ROLLING rather
            // than sliding. The convention (positive spin = clockwise on
            // screen) lives in core/roll.hpp and is covered by tests.
            float rx = 0.0f, ry = 0.0f;
            rollSurface(static_cast<float>(dx), static_cast<float>(dy),
                        spinDeg, rx, ry);

            if ((static_cast<int>(std::floor(rx / 4.0f)) & 1) == 0) {
                c = lerpColor(c, dark, 0.45f);        // banknote seam
            }
            if (coinHit(rx, ry)) {
                c = lerpColor(c, light, 0.55f);       // embedded coin
            }
            if (dollarGlyph(rx, ry)) {
                c = band;                             // the "$" turns with it
            }

            // A rim so the ball reads against any background.
            if (d2 > (r - 1.4f) * (r - 1.4f)) c = lerpColor(c, shadow, 0.8f);

            DrawPixel(icx + dx, icy + dy, c);
        }
    }

    // Eyes and mouth stay upright so the ball is always "facing" you, exactly as
    // the design calls for -- everything else rolls.
    const int sx = icx, sy = icy + 12;

    // Face and props stay upright regardless of the roll.
    drawEyes(sx, sy - 22, mood, pal);
    if (sunglasses || mood == BallMood::Smug) {
        DrawRectangle(sx - 12, sy - 25, 10, 6, toColor(pal.outline));
        DrawRectangle(sx + 2,  sy - 25, 10, 6, toColor(pal.outline));
        DrawLine(sx - 2, sy - 23, sx + 2, sy - 23, toColor(pal.outline));
    }
    drawMouth(sx, sy - 12, mood, pal);

    if (topHat) {
        DrawRectangle(sx - 8, sy - 36, 16, 3, toColor(pal.outline));
        DrawRectangle(sx - 6, sy - 46, 12, 10, toColor(pal.outline));
    }
}

// -----------------------------------------------------------------------------
//  The human character
// -----------------------------------------------------------------------------
float characterWorldX(const Game& g) {
    const float ballX = World::ballWorldX(g.ballProgress());
    switch (g.phase()) {
        case Phase::WalkAway: return ballX - 44.0f - g.walkT() * 520.0f;
        case Phase::WalkIn: {
            const float t = clampf(g.phaseTime() / DUR_WALKIN, 0.0f, 1.0f);
            return ballX - 30.0f - (1.0f - t) * 90.0f;
        }
        default: return ballX - 30.0f;
    }
}

Texture2D characterTexture(const Game& g, const SpriteBank& sb) {
    const int stage = static_cast<int>(g.stage());
    switch (g.character()) {
        case CharState::Pushing:  return sb.pose(stage, 1);
        case CharState::Bracing:  return sb.pose(stage, 2);
        case CharState::Collapsing:
        case CharState::Dead:     return sb.get(SpriteId::Collapse);
        case CharState::Walking:  return sb.get(SpriteId::Walk);
        default:                  return sb.pose(stage, 0);
    }
}

void drawCharacter(const Game& g, const SpriteBank& sb, const Palette& pal, float camX) {
    if (g.character() == CharState::None) return;

    const Texture2D tex = characterTexture(g, sb);
    if (tex.id == 0) return;

    const float wx = characterWorldX(g);
    const float groundY = World::hillY(wx);

    // A tiny bob keeps the walk from looking like a slide.
    float bob = 0.0f;
    if (g.character() == CharState::Walking) {
        bob = std::sin(g.phaseTime() * 12.0f) * 1.0f;
    }

    const int sx = static_cast<int>(wx - camX) - CHAR_W / 2;
    const int sy = static_cast<int>(groundY - CHAR_H + 2 + bob);

    DrawRectangle(sx + 2, static_cast<int>(groundY) - 1, CHAR_W - 4, 2,
                  toColor(pal.ballShadow, 90));

    DrawTexture(tex, sx, sy, WHITE);
}

} // namespace

// =============================================================================
//  Speech bubble
// =============================================================================
void speechBubble(const char* text, int cx, int cy, const Palette& pal, int reveal) {
    if (text == nullptr) return;

    const int len0 = static_cast<int>(std::strlen(text));
    int len = reveal < len0 ? reveal : len0;
    if (len <= 0) return;
    if (len > 127) len = 127;

    char buf[128];
    std::memcpy(buf, text, static_cast<std::size_t>(len));
    buf[len] = '\0';

    const int w = MeasureText(buf, 10) + 10;
    const int h = 18;
    int bx = cx - w / 2;
    int by = cy - h - 8;
    if (bx < 2) bx = 2;
    if (bx + w > VIRTUAL_W - 2) bx = VIRTUAL_W - 2 - w;
    // Near the summit there is no room above the ball, so the bubble simply
    // hugs the top of the frame rather than flipping underneath itself.
    if (by < 2) by = 2;

    DrawRectangle(bx, by, w, h, toColor(pal.bubbleBg, 235));
    DrawRectangleLines(bx, by, w, h, toColor(pal.bubbleEdge));
    DrawTriangle({ static_cast<float>(cx) - 4.0f, static_cast<float>(by + h) },
                 { static_cast<float>(cx) + 4.0f, static_cast<float>(by + h) },
                 { static_cast<float>(cx),        static_cast<float>(by + h + 6) },
                 toColor(pal.bubbleEdge));
    DrawText(buf, bx + 5, by + 5, 10, toColor(pal.text));
}

// =============================================================================
//  Whole scene
// =============================================================================
void draw(const Game& g, const Palette& pal, const SpriteBank& sprites) {
    const float camX = cameraX(g);

    drawSky(pal);
    drawSun(pal);
    drawParallax(pal, camX);
    drawHill(pal, camX);

    const float ballWX = World::ballWorldX(g.ballProgress());
    const float groundY = World::hillY(ballWX);
    const float ballSX = ballWX - camX;
    const float ballSY = groundY - World::BALL_R;

    // Contact shadow on the slope.
    DrawEllipse(static_cast<int>(ballSX), static_cast<int>(groundY) - 1,
                24.0f, 5.0f, toColor(pal.ballShadow, 80));

    drawCharacter(g, sprites, pal, camX);

    const bool topHat = g.incarnation() >= 3;
    drawBall(ballSX, ballSY, World::BALL_R, g.ballSpin(), g.mood(), pal, topHat, false);

    // The ball always has something to say -- except on the title, on the
    // choice screen (its line is the panel header) and over the credits.
    const char* line = g.currentLine();
    const bool wantBubble = line != nullptr &&
                            g.phase() != Phase::Title &&
                            g.phase() != Phase::Choice &&
                            g.phase() != Phase::Credits;
    if (wantBubble) {
        const int reveal = static_cast<int>(g.lineAge() * 34.0f) + 1;
        speechBubble(line, static_cast<int>(ballSX),
                     static_cast<int>(ballSY - World::BALL_R), pal, reveal);
    }
}

} // namespace cashyphus::scene

