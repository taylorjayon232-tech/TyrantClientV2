// ============================================================================
//  Passive — Auto Windcharge interception rules.
// ----------------------------------------------------------------------------
//  Pure, no JNI and no ImGui, so test/wind_charge_check.cpp exercises exactly
//  what the module runs.
//
//  The wind charge leaves the eye at 1.5 blocks/tick and loses 1% of its speed
//  per tick, so after `tick` ticks it has covered
//      1.5 * (1 - 0.99^tick) / 0.01
//  blocks. The pearl flies with the same launch speed and the same drag, but it
//  also falls (0.03/tick), so the straight line from the eye to where the pearl
//  will be is shorter than the distance the charge has to fly to get there -
//  which is exactly why a level throw can be caught at all.
//
//  Both projectiles inherit the thrower's own velocity. That drift is the whole
//  reason a moving or gliding throw used to be missed: the aim has to be solved
//  against the pearl's position with the charge's inherited drift removed, not
//  against a pearl that the charge is treated as flying at 1.5 blocks/tick from
//  a standstill.
// ============================================================================
#pragma once

#include <cmath>

namespace tyr {
namespace modules {
namespace windcharge {

struct Vec3f {
    float x = 0.f, y = 0.f, z = 0.f;
};

inline Vec3f make(float x, float y, float z) { return Vec3f{x, y, z}; }
inline Vec3f operator+(const Vec3f& a, const Vec3f& b) { return Vec3f{a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3f operator-(const Vec3f& a, const Vec3f& b) { return Vec3f{a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3f operator*(const Vec3f& a, float k) { return Vec3f{a.x * k, a.y * k, a.z * k}; }
inline float dot(const Vec3f& a, const Vec3f& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length(const Vec3f& a) { return std::sqrt(dot(a, a)); }

// Launch speed of a wind charge, in blocks per tick.
constexpr float kChargeSpeed = 1.5f;

// Blocks covered by a wind charge after `tick` ticks of its flight.
inline float chargeDistance(int tick) {
    if (tick <= 0) return 0.f;
    return kChargeSpeed * (1.f - std::pow(0.99f, (float)tick)) / 0.01f;
}

// Ping Offset (0-600 ms) in ticks: the meeting point is moved this far down the pearl's flight,
// which is where the pearl really is by the time a throw made on this connection leaves the hand.
inline int pingOffsetTicks(int pingOffsetMs) {
    const int ms = pingOffsetMs < 0 ? 0 : (pingOffsetMs > 600 ? 600 : pingOffsetMs);
    return (ms + 25) / 50;
}

// A solved interception. `tick > 0` means the charge can be thrown now; `dir` is the unit direction
// it has to leave the eye along.
struct Meet {
    int   tick = -1;
    Vec3f dir{};
    float distance = 0.f;   // eye to the meeting point along that direction
    bool  valid() const { return tick > 0; }
};

// Find the first tick at which the charge's own travel reaches the pearl's position on that tick
// (shifted by `offsetTicks` for ping). `path[0..count-1]` are the pearl's per-tick positions, and
// `throwerVelocity` is the velocity both projectiles inherit.
//
// A meeting point closer than `minAimDistance` is refused: a charge detonated on the player's own
// feet is the "straight up under you" case this module exists to avoid.
//
// `tolerance` is how close the charge has to get for the two hitboxes to touch. It matters because
// a pearl falls while the charge does not: by tick k the pearl is 1.5*(1-0.99^k)/0.01 blocks out
// and a little below that line, so the charge aimed at it always arrives a hair short. Without the
// tolerance nothing would ever be "reachable" - the exact-preach rule is unsatisfiable for any
// throw with gravity on it.
inline Meet solve(const Vec3f* path, int count, const Vec3f& eye, const Vec3f& throwerVelocity,
                  int maxMeetTicks, int offsetTicks, float minAimDistance, float tolerance) {
    Meet out;
    if (!path || count <= 0 || maxMeetTicks <= 0) return out;
    const Vec3f driftPerBlock = throwerVelocity * (1.f / kChargeSpeed);
    for (int meet = 1; meet <= maxMeetTicks; ++meet) {
        if (meet >= count) break;
        const float travel = chargeDistance(meet);
        const Vec3f drift = driftPerBlock * travel;
        const Vec3f base = path[meet] - eye - drift;
        const float baseLength = length(base);
        if (baseLength < minAimDistance) continue;        // too close: it would detonate on the player
        if (travel + tolerance < baseLength) continue;    // further than the charge can reach this tick
        // The meeting tick is decided on the pearl's own flight; the ping offset then moves the aim
        // down that flight, which is where the pearl is by the time the charge really leaves the
        // hand on a high-ping connection. Aiming past the tick that can be reached is deliberate:
        // a charge that detonates a little short still explodes beside the pearl, while one aimed
        // at where the pearl was is behind it.
        int index = meet + (offsetTicks > 0 ? offsetTicks : 0);
        if (index >= count) index = count - 1;
        const Vec3f delta = path[index] - eye - drift;
        const float len = length(delta);
        out.tick = meet;
        out.dir = delta * (1.f / len);
        out.distance = len;
        return out;
    }
    return out;
}

} // namespace windcharge
} // namespace modules
} // namespace tyr
