#pragma once

#include <cmath>

#include "core/config.hpp"

namespace cashyphus {

// ===========================================================================
//  Ball surface rotation: ONE shared convention for physics and rendering.
//
//  `spinDeg` is how far the ball's SURFACE has turned, in degrees, where a
//  POSITIVE value means CLOCKWISE AS SEEN ON SCREEN.
//
//  Uphill is to the right, so a ball being pushed up the hill has a positive
//  spin, and a ball running back down goes anticlockwise. That is the direction
//  a wheel turns when it travels to the right.
//
//  Given a pixel offset (dx, dy) from the ball's centre, `rollSurface` returns
//  the point on the ball's own surface visible there. Drawing surface features
//  (banknote seams, coins, the "$") at those coordinates is what makes the ball
//  read as rolling rather than sliding.
//
//  Keeping this in one place matters: the sign was previously applied inline in
//  the renderer and was inverted, so the ball span anticlockwise while climbing.
// ===========================================================================
inline void rollSurface(float dx, float dy, float spinDeg, float& rx, float& ry) {
    const float phi = spinDeg * 0.01745329252f;   // degrees -> radians
    const float c = std::cos(phi);
    const float s = std::sin(phi);
    // Screen y points down, so this matrix is a CLOCKWISE turn of the surface
    // by `spinDeg`; we invert it to look up the surface under each screen pixel.
    rx =  dx * c + dy * s;
    ry = -dx * s + dy * c;
}

} // namespace cashyphus
