#include "render/scene.hpp"

#include <cmath>
#include <cstring>

#include "core/config.hpp"
#include "core/roll.hpp"
#include "render/font.hpp"
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
//
//  No camera scroll: the hill itself is fixed across the portrait frame and the
//  ball travels along it. Everything else (parallax, clouds) moves vertically
//  with progress, so height is what the screen is telling you about.
// =============================================================================
float World::groundY(float screenX) {
    const float t = clampf(screenX / VIRTUAL_W, 0.0f, 1.5f);
    return GROUND_BASE - GROUND_RISE * std::pow(t, GROUND_CURVE);
}

// Analytic slope of the surface, dy/dx: the exact derivative of groundY above.
// Negative on the way up (screen y grows downwards) and it steepens as the hill
// climbs. Shadows use it to lie flat along the hill instead of hovering over it.
float World::groundSlope(float screenX) {
    const float t = clampf(screenX / VIRTUAL_W, 0.001f, 1.5f);
    return -GROUND_RISE * GROUND_CURVE * std::pow(t, GROUND_CURVE - 1.0f) / VIRTUAL_W;
}

float World::ballX(float progress) {
    const float p = clampf(progress, 0.0f, 1.0f);
    return BALL_X0 + (ballX1() - BALL_X0) * p;
}

float World::ballY(float progress) {
    return groundY(ballX(progress)) - BALL_R;
}

float World::contactY(float screenX) { return groundY(screenX); }

float climbOffset(const Game& g) {
    if (g.phase() == Phase::WalkAway || g.phase() == Phase::Credits) return 0.0f;
    return clampf(g.ballProgress(), 0.0f, 1.0f);
}

// =============================================================================
//  Text helpers -- every one of them goes through the baked 8px pixel face.
// =============================================================================
void text(const char* s, int x, int y, Color c) {
    art::text::draw(s, x, y, art::text::SIZE_SMALL, c);
}
int  textWidth(const char* s) { return art::text::width(s, art::text::SIZE_SMALL); }
void textCentered(const char* s, int y, Color c) {
    art::text::drawCenter(s, y, art::text::SIZE_SMALL, c);
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
    // Pinned near the right edge rather than at x=142, so it stays in the sky
    // however wide the display grew the frame (142 was 180 - 38).
    const int sx = VIRTUAL_W - 38;
    DrawCircle(sx, 44, 11, c);
    DrawCircle(sx, 44, 7, Color{ c.r, c.g, c.b, 200 });
}

// -----------------------------------------------------------------------------
//  Clouds
//
//  The brief lists clouds among the background elements. Without them the top
//  half of a 320x180 frame is a flat wash of sky, which is most of why the
//  scene read as empty.
// -----------------------------------------------------------------------------
// `rise` is 0..1 progress: the clouds sink through the sky as the ball climbs,
// which is what makes gaining height legible on a tall screen. They are drawn
// before the hill, so anything below the ground line is covered by terrain.
void drawClouds(const Palette& pal, float rise) {
    // Mixed from the sky's own colour rather than hard-coded, so the clouds go
    // pale and cold along with everything else on the way to the summit.
    const Color body = lerpColor(toColor(pal.skyTop), toColor(pal.uiText), 0.52f);

    const float drift = static_cast<float>(GetTime()) * 3.0f;
    constexpr float BAND_TOP = 18.0f;
    // The band runs from just under the top of the frame down to just above the
    // hill, so a taller frame gets clouds across the whole extra sky rather than
    // a stripe of it. At the design height this is the 274 the clouds were
    // placed with.
    const float BAND_H = static_cast<float>(VIRTUAL_H) - 86.0f;

    for (int i = 0; i < 7; ++i) {
        const float x = std::fmod(static_cast<float>(i * 53) + drift,
                                  VIRTUAL_W + 60.0f) - 40.0f;
        const float baseY = 26.0f + static_cast<float>((i * 67) % 210);

        // Drop as you climb, wrapped so a cloud leaves the bottom and returns at
        // the top rather than vanishing for the rest of the run.
        float y = std::fmod(baseY + rise * BAND_H - BAND_TOP, BAND_H);
        if (y < 0.0f) y += BAND_H;
        y += BAND_TOP;

        const int xi = static_cast<int>(x);
        const int yi = static_cast<int>(y);
        const int s  = (i % 3 == 0) ? 2 : 1;

        // A puff: a flat base with three overlapping blobs sitting on it.
        DrawRectangle(xi, yi + 4 * s, 14 * s, 3 * s, body);
        DrawCircle(xi + 4 * s,  yi + 4 * s, 3.0f * s, body);
        DrawCircle(xi + 8 * s,  yi + 3 * s, 4.0f * s, body);
        DrawCircle(xi + 12 * s, yi + 4 * s, 3.0f * s, body);
    }
}

void drawParallax(const Palette& pal, float rise) {
    // Two rows of mountains standing on a horizon that sinks as the ball climbs.
    // On a tall screen, height is told by the background falling away rather
    // than by scrolling sideways, so both layers drop with progress.
    const Color far  = toColor(pal.mountainFar);
    const Color near = toColor(pal.mountainNear);

    const float sink = rise * 150.0f;

    // Both rows run to the right edge of the frame rather than stopping after a
    // fixed number of peaks: the frame is as wide as the display needs now, and a
    // range that ran out part-way would leave a bare horizon on a tablet.
    const float h1 = 172.0f + sink;
    for (float bx = -34.0f; bx < static_cast<float>(VIRTUAL_W) + 40.0f; bx += 76.0f) {
        DrawTriangle({ bx,         h1 },
                     { bx + 38.0f, h1 - 48.0f },
                     { bx + 76.0f, h1 }, far);
    }

    const float h2 = 216.0f + sink;
    for (float bx = -22.0f; bx < static_cast<float>(VIRTUAL_W) + 36.0f; bx += 58.0f) {
        DrawTriangle({ bx,         h2 },
                     { bx + 29.0f, h2 - 42.0f },
                     { bx + 58.0f, h2 }, near);
    }
}

// Deterministic 0..1 hash of a screen cell, so the terrain detail and the
// vegetation sit in fixed spots and never swim as the ball moves.
float hash01(int a, int b) {
    unsigned int h = static_cast<unsigned int>(a * 374761393u + b * 668265263u);
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= (h >> 16u);
    return static_cast<float>(h & 0xFFFFFFu) / 16777215.0f;
}

// -----------------------------------------------------------------------------
//  Ground shadows
//
//  The hill is steep -- close to 60 degrees up top -- and a screen-axis-aligned
//  ellipse is simply the wrong shape for a shadow on it. Half of such an ellipse
//  lands on the sky side of the surface line, where it reads as a grey blob
//  floating beside the ball, and the half that lands on the hill is smeared
//  across the surface instead of lying along it.
//
//  Shadows are therefore rasterised in the surface's own frame: stretched along
//  the uphill/downhill tangent, squashed against the surface normal, and clipped
//  so nothing is ever drawn above the ground line. The result hugs the slope.
// -----------------------------------------------------------------------------

// Where a ball centred at (cx, cy) actually touches the hill. The ball is drawn
// with a plain vertical offset -- centred one radius above the ground line --
// which on a slope puts its true point of contact noticeably UP the hill from
// its centre column. Dropping a perpendicular onto the surface from the centre
// finds it, so the shadow can be anchored there rather than under the middle.
void contactPoint(float cx, float cy, float& outX, float& outY) {
    const float m  = World::groundSlope(cx);
    const float gy = World::groundY(cx);
    const float t  = (cy - gy) * m / (1.0f + m * m);   // projection along the surface
    outX = cx + t;
    outY = gy + m * t;
}

// A soft shadow lying on the slope: an ellipse whose long axis follows the
// surface and whose short axis presses into it, with everything above the ground
// line discarded. That clip is what stops it bleeding into the sky.
void slopeShadow(float cx, float cy, float halfLong, float halfThick, Color color) {
    const float m   = World::groundSlope(cx);
    const float inv = 1.0f / std::sqrt(1.0f + m * m);
    const float tx  = inv,      ty = m * inv;   // unit tangent, +x = uphill
    const float nx  = -m * inv, ny = inv;       // unit normal, +y = into the hill

    const int reach = static_cast<int>(halfLong) + 2;
    const int x0 = static_cast<int>(cx) - reach;
    const int x1 = static_cast<int>(cx) + reach;
    const int y0 = static_cast<int>(cy) - reach;
    const int y1 = static_cast<int>(cy) + reach;

    for (int sy = y0; sy <= y1; ++sy) {
        for (int sx = x0; sx <= x1; ++sx) {
            const float ox = static_cast<float>(sx) - cx;
            const float oy = static_cast<float>(sy) - cy;
            const float u  = ox * tx + oy * ty;    // along the surface
            const float v  = ox * nx + oy * ny;    // ...and into it
            if (v < 0.0f) continue;                // sky side: never draw
            const float fu = u / halfLong, fv = v / halfThick;
            if (fu * fu + fv * fv > 1.0f) continue;
            DrawPixel(sx, sy, color);
        }
    }
}

// Grass tufts and stones sitting on the surface line, placed by the hash so
// they are in the same spot every frame.
void drawVegetation(const Palette& pal) {
    const Color tuft  = lerpColor(toColor(pal.hillFar), toColor(pal.uiGood), 0.30f);
    const Color stone = toColor(pal.hillEdge);
    for (int col = 0; col < VIRTUAL_W; col += 6) {
        const float gy = World::groundY(static_cast<float>(col));
        if (gy > VIRTUAL_H + 4.0f) continue;
        const float r0 = hash01(col, 17);
        const float r1 = hash01(col, 91);
        const int   x  = col + static_cast<int>(hash01(col, 43) * 4.0f);
        const int   y  = static_cast<int>(gy);
        if (r0 < 0.34f) {                        // a grass tuft: three thin blades
            const int bh = 2 + static_cast<int>(r1 * 3.0f);
            DrawLine(x,     y, x - 1, y - bh,     tuft);
            DrawLine(x + 1, y, x + 1, y - bh - 1, tuft);
            DrawLine(x + 2, y, x + 3, y - bh,     tuft);
        } else if (r0 < 0.46f) {                 // a small stone half-buried
            const int sw = 2 + static_cast<int>(r1 * 3.0f);
            DrawRectangle(x, y - 2, sw, 2, stone);
        }
    }
}

// A flag planted at the top of the hill: the summit the whole climb is aimed
// at, visible from the first push.
void drawSummit(const Palette& pal) {
    const int   x0   = VIRTUAL_W - 10;
    const int   y0   = static_cast<int>(World::groundY(static_cast<float>(x0)));
    const Color pole = toColor(pal.hillEdge);
    const Color flag = toColor(pal.uiWarn);
    DrawLine(x0, y0, x0, y0 - 14, pole);
    DrawLine(x0, y0 - 14, x0 + 7, y0 - 11, flag);
    DrawLine(x0 + 7, y0 - 11, x0, y0 - 8, flag);
}

// The hill: one vertical strip per screen column, filling everything below the
// diagonal surface. The surface rises from the bottom-left corner to the
// top-right, so the ball travelling along it climbs the frame. It is filled in
// two tones -- a lit band hugging the surface and a darker body beneath -- and
// carries the brief's terrain (speckle, a winding path, vegetation) and a
// summit marker, so the slope reads as a place rather than a fill.
void drawHill(const Palette& pal) {
    const Color nearC = toColor(pal.hillNear);
    const Color edge  = toColor(pal.hillEdge);
    const Color path  = toColor(pal.path);
    const Color body  = lerpColor(nearC, toColor(pal.outline), 0.42f);
    const Color dark  = lerpColor(nearC, body, 0.5f);

    for (int sx = 0; sx < VIRTUAL_W; ++sx) {
        const int y  = static_cast<int>(World::groundY(static_cast<float>(sx)));
        const int cl = std::min(y + 22, VIRTUAL_H);
        DrawLine(sx, y,  sx, cl,        nearC);   // lit surface band
        DrawLine(sx, cl, sx, VIRTUAL_H, body);    // darker body beneath
        DrawPixel(sx, y, edge);
    }

    // Static speckle in the lit band, so the surface reads as ground.
    for (int sx = 0; sx < VIRTUAL_W; sx += 3) {
        const int gy = static_cast<int>(World::groundY(static_cast<float>(sx)));
        if (hash01(sx, 5) < 0.3f)
            DrawPixel(sx, gy + 3 + static_cast<int>(hash01(sx, 9) * 14.0f), dark);
    }

    // A winding path hugging the surface, meandering a touch as it climbs.
    for (int sx = 0; sx < VIRTUAL_W; ++sx) {
        const float wob = std::sin(static_cast<float>(sx) * 0.11f) * 2.0f;
        const int   y   = static_cast<int>(World::groundY(static_cast<float>(sx)) + 2.0f + wob);
        DrawLine(sx, y, sx, y + 1, path);
    }

    drawVegetation(pal);
    drawSummit(pal);
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
// Where the person stands on screen. Measured as an arc length along the slope
// rather than a horizontal gap: the slope steepens to the right, and a flat
// horizontal offset would leave the character separated from the ball by half
// the screen on the steep final stretch.
float characterScreenX(const Game& g) {
    const float bx = World::ballX(g.ballProgress());

    // d(ground)/dx at the ball, from the curve.
    const float t    = clampf(bx / VIRTUAL_W, 0.001f, 1.5f);
    const float g_   = -World::GROUND_RISE * World::GROUND_CURVE *
                       std::pow(t, World::GROUND_CURVE - 1.0f) / VIRTUAL_W;
    const float back = 32.0f / std::sqrt(1.0f + g_ * g_);

    switch (g.phase()) {
        case Phase::WalkAway: {
            // Walk away downhill and to the left -- the way they arrived -- and
            // keep going until they are out of the frame, leaving the ball behind
            // on the hill. This used to be clampf(walk, 14, VIRTUAL_W - 44), and 14
            // is where the walk already starts from, so the character stood still
            // and bobbed for the whole ending: the walk was in the code and never
            // once on the screen.
            const float gone = clampf((g.walkT() - WALK_LEAVE_AT) /
                                      (WALK_LEAVE_BY - WALK_LEAVE_AT), 0.0f, 1.0f);
            const float out  = (bx - back) + static_cast<float>(CHAR_W) + 4.0f;
            return bx - back - gone * out;
        }
        case Phase::WalkIn: {
            const float tin = clampf(g.phaseTime() / DUR_WALKIN, 0.0f, 1.0f);
            return bx - back - (1.0f - tin) * 74.0f;
        }
        default: return bx - back;   // hands on the ball, not a gap between them
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

void drawCharacter(const Game& g, const SpriteBank& sb, const Palette& pal) {
    if (g.character() == CharState::None) return;

    const Texture2D tex = characterTexture(g, sb);
    if (tex.id == 0) return;

    const float wx = characterScreenX(g);
    const float gy = World::groundY(wx);

    // A tiny bob keeps the walk from looking like a slide.
    float bob = 0.0f;
    if (g.character() == CharState::Walking) {
        bob = std::sin(g.phaseTime() * 12.0f) * 1.0f;
    }

    const int sx = static_cast<int>(wx) - CHAR_W / 2;
    const int sy = static_cast<int>(gy - CHAR_H + 2 + bob);

    // Feet shadow: same slope-hugging treatment as the ball, just smaller. A
    // horizontal bar under the boots read as a step, not a shadow, on a hill
    // this steep.
    slopeShadow(wx, gy, 9.0f, 4.0f, toColor(pal.ballShadow, 95));

    DrawTexture(tex, sx, sy, WHITE);
}

} // namespace

// =============================================================================
//  Speech bubble
//
//  The ball talks a lot and the brief insists the bubble stays small, so the
//  text wraps rather than stretching the box across the screen.
// =============================================================================
constexpr int MAX_BUBBLE_W = 158;   // must fit inside the 180px portrait frame

void speechBubble(const char* text, int cx, int cy, const Palette& pal, int reveal) {
    if (text == nullptr) return;

    const int len0 = static_cast<int>(std::strlen(text));
    int len = reveal < len0 ? reveal : len0;
    if (len <= 0) return;
    if (len > 127) len = 127;

    char buf[128];
    std::memcpy(buf, text, static_cast<std::size_t>(len));
    buf[len] = '\0';

    // The baked face advances a full 8px per character, so several of the
    // ball's longer lines would run clean off the screen unwrapped.
    constexpr int MAX_CHARS = (MAX_BUBBLE_W - 10) / art::text::CELL_W;
    constexpr int MAX_LINES = 3;
    char lines[MAX_LINES][128] = {};
    int  nlines = 1;
    int  col    = 0;
    for (const char* p = buf; *p != '\0';) {
        const char* ws = p;
        while (*p != '\0' && *p != ' ') ++p;
        const int wlen = static_cast<int>(p - ws);
        while (*p == ' ') ++p;

        if (col > 0 && col + 1 + wlen > MAX_CHARS && nlines < MAX_LINES) {
            ++nlines;
            col = 0;
        }
        if (col > 0) lines[nlines - 1][col++] = ' ';
        for (int i = 0; i < wlen && col < 127; ++i) lines[nlines - 1][col++] = ws[i];
        lines[nlines - 1][col] = '\0';
    }

    int w = 0;
    for (int i = 0; i < nlines; ++i) {
        const int lw = art::text::width(lines[i], art::text::SIZE_SMALL);
        if (lw > w) w = lw;
    }
    w += 10;
    const int h = nlines * art::text::CELL_H + 6;

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

    const Color ink = toColor(pal.text);
    for (int i = 0; i < nlines; ++i) {
        art::text::draw(lines[i], bx + 5, by + 3 + i * art::text::CELL_H,
                        art::text::SIZE_SMALL, ink);
    }
}

// =============================================================================
//  Whole scene
// =============================================================================
void draw(const Game& g, const Palette& pal, const SpriteBank& sprites) {
    const float rise = climbOffset(g);

    drawSky(pal);
    drawSun(pal);
    drawClouds(pal, rise);

    // Everything from here down is the world, laid out in the design frame. A
    // display taller than 1:2 grows the frame instead of padding it, and the world
    // is translated down by exactly that much. At the design size this translate
    // is zero, so the picture is pixel for pixel what it always was; on a phone the
    // extra height is more sky above a scene that has not moved or stretched. The
    // sky and clouds stay above the push, because they belong to the frame.
    // Everything from here down is the world, laid out in the design frame. A
    // display taller than 1:2 grows the frame instead of padding it, and the world
    // is translated down by exactly that much. At the design size this offset is
    // zero, so the picture is pixel for pixel what it always was; on a phone the
    // extra height is more sky above a scene that has not moved or stretched. The
    // sky and clouds stay above the push, because they belong to the frame.
    const Camera2D world{{ 0.0f, static_cast<float>(frameTop()) }, { 0.0f, 0.0f }, 0.0f, 1.0f };
    BeginMode2D(world);

    drawParallax(pal, rise);
    drawHill(pal);

    const float bx = World::ballX(g.ballProgress());
    const float by = World::ballY(g.ballProgress());   // centre, resting on the slope

    // Contact shadow. Anchored at the point the ball actually touches the slope
    // (uphill of its centre column, because it is offset vertically) and laid
    // along the surface, so it reads as the ball meeting the hill -- not as a
    // grey blob floating in the sky beside it.
    float shx = bx, shy = by;
    contactPoint(bx, by, shx, shy);
    slopeShadow(shx, shy, 34.0f, 6.0f, toColor(pal.ballShadow, 90));

    drawCharacter(g, sprites, pal);

    const bool topHat = g.incarnation() >= 3;
    drawBall(bx, by, World::BALL_R, g.ballSpin(), g.mood(), pal, topHat, false);

    // The ball always has something to say -- except on the title, on the
    // choice screen (its line is the panel header) and over the credits.
    const char* line = g.currentLine();
    const bool wantBubble = line != nullptr &&
                            g.phase() != Phase::Title &&
                            g.phase() != Phase::Choice &&
                            g.phase() != Phase::Credits;
    if (wantBubble) {
        const int reveal = static_cast<int>(g.lineAge() * 34.0f) + 1;
        speechBubble(line, static_cast<int>(bx),
                     static_cast<int>(by - World::BALL_R), pal, reveal);
    }

    EndMode2D();
}

} // namespace cashyphus::scene

