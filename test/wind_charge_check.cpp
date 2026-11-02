// ============================================================================
//  Harness: the pure Auto Windcharge interception rules (agent/modules/wind_charge.h).
// ----------------------------------------------------------------------------
//  The cases below are the ways the solve was got wrong before:
//
//   1. The charge's own travel was treated as an exact 1.5 blocks per tick, so a
//      pearl a few blocks out was "reachable" before the charge could get there
//      and the aim was short. chargeDistance() is now the drag-aware series.
//   2. The thrower's inherited velocity was ignored, so a walking, sprinting or
//      gliding throw was missed: the charge drifts with the thrower and the aim
//      has to remove exactly that drift.
//   3. Ping Offset had nothing to act on. It now moves the meeting point down the
//      pearl's own flight by the ticks the throw is late.
//
//  Build + run:  test\build-wind-charge.bat   (prints "PASS" or the failing cases)
// ============================================================================
#include "../agent/modules/wind_charge.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace tyr::modules::windcharge;

static int gChecks = 0;
static int gFails = 0;

static void check(bool ok, const char* what) {
    ++gChecks;
    if (!ok) { ++gFails; std::printf("FAIL  %s\n", what); }
}

static void checkNear(double got, double want, double eps, const char* what) {
    ++gChecks;
    if (std::fabs(got - want) > eps) {
        ++gFails;
        std::printf("FAIL  %s\n        got  %.6f\n        want %.6f\n", what, got, want);
    }
}

static void checkEqInt(int got, int want, const char* what) {
    ++gChecks;
    if (got != want) {
        ++gFails;
        std::printf("FAIL  %s\n        got  %d\n        want %d\n", what, got, want);
    }
}

// The pearl's own flight: 1.5 blocks/tick of launch along +z, drag 0.99, gravity 0.03.
static std::vector<Vec3f> pearlPath(Vec3f pos, Vec3f vel, int ticks) {
    std::vector<Vec3f> path;
    path.push_back(pos);
    for (int i = 0; i < ticks; ++i) {
        pos = pos + vel;
        vel = vel * 0.99f;
        vel.y -= 0.03f;
        path.push_back(pos);
    }
    return path;
}

int main() {
    // ---- the charge's own flight ------------------------------------------------------------
    checkNear(chargeDistance(0), 0.0, 1e-6, "charge: 0 ticks is no travel");
    checkNear(chargeDistance(1), 1.5, 1e-5, "charge: one tick is 1.5 blocks");
    checkNear(chargeDistance(2), 2.985, 1e-4, "charge: two ticks are 2.985 blocks, not 3");
    checkNear(chargeDistance(20), 1.5 * (1.0 - std::pow(0.99, 20.0)) / 0.01, 1e-3,
              "charge: twenty ticks follow the drag series");
    bool monotonic = true;
    for (int t = 1; t <= 160; ++t)
        if (chargeDistance(t) <= chargeDistance(t - 1) - 1e-6) monotonic = false;
    check(monotonic, "charge: travel never goes backwards");
    check(chargeDistance(60) < 1.5f * 60.f, "charge: drag makes it slower than a flat 1.5/tick");
    check(chargeDistance(600) < 150.f, "charge: the travel is bounded by its own drag");

    // ---- Ping Offset (0-600 ms) in ticks ----------------------------------------------------
    checkEqInt(pingOffsetTicks(0), 0, "ping: 0 ms is no offset");
    checkEqInt(pingOffsetTicks(24), 0, "ping: 24 ms is under a tick");
    checkEqInt(pingOffsetTicks(25), 1, "ping: 25 ms rounds to one tick");
    checkEqInt(pingOffsetTicks(50), 1, "ping: 50 ms is exactly one tick");
    checkEqInt(pingOffsetTicks(100), 2, "ping: 100 ms is two ticks");
    checkEqInt(pingOffsetTicks(600), 12, "ping: 600 ms is the twelve tick maximum");
    checkEqInt(pingOffsetTicks(-100), 0, "ping: a negative offset is no offset");
    checkEqInt(pingOffsetTicks(5000), 12, "ping: above 600 ms tops out at twelve ticks");

    // ---- a standing throw, level with the ground --------------------------------------------
    const Vec3f eye = make(0.f, 1.62f, 0.f);
    constexpr float kTol = 0.6f;
    const Vec3f launch = make(0.f, 0.f, 1.5f);
    const std::vector<Vec3f> level = pearlPath(eye, launch, 80);
    const Meet straight = solve(level.data(), (int)level.size(), eye, make(0.f, 0.f, 0.f), 40, 0, 2.f, kTol);
    check(straight.valid(), "level: a standing level throw is caught");
    check(straight.tick >= 1 && straight.tick <= 40, "level: the meeting tick is inside Max Meet Ticks");
    check(straight.dir.z > 0.9f, "level: the charge leaves the eye along the throw");
    checkNear(length(straight.dir), 1.0, 1e-4, "level: the aim is a unit direction");
    check(straight.distance >= 2.f, "level: the meeting point is off the player's own feet");
    check(chargeDistance(straight.tick) + kTol >= straight.distance - 1e-3,
          "level: the charge can really cover the distance by that tick");

    // The first tick that works is used, not a later one that happens to be cheap.
    for (int earlier = 1; earlier < straight.tick; ++earlier) {
        const Vec3f want = level[(size_t)earlier];
        const Vec3f delta = want - eye;
        const float len = length(delta);
        const bool tooClose = len < 2.f;
        const bool outOfReach = chargeDistance(earlier) + kTol < len;
        check(tooClose || outOfReach, "level: no earlier tick was already a valid interception");
    }

    // ---- Max Meet Ticks is the gate that stops a throw that cannot get there -----------------
    const Meet tooSoon = solve(level.data(), (int)level.size(), eye, make(0.f, 0.f, 0.f), 1, 0, 2.f, kTol);
    check(!tooSoon.valid(), "meet: one tick is not enough to get off the player's own feet");
    const Meet inTime = solve(level.data(), (int)level.size(), eye, make(0.f, 0.f, 0.f), 2, 0, 2.f, kTol);
    check(inTime.valid(), "meet: the same throw goes out once Max Meet Ticks allows two ticks");
    checkEqInt(inTime.tick, 2, "meet: the earliest tick that works is the one used");

    // ---- a pearl that has already outrun the player cannot be caught ------------------------
    // Both projectiles leave at the same speed with the same drag, so a pearl that is already ten
    // blocks ahead stays ten blocks ahead: no Max Meet Ticks can turn that into an interception,
    // and the solver must say so rather than aiming a charge at nothing.
    const Vec3f farLaunch = make(0.f, 0.f, 1.5f);
    std::vector<Vec3f> far = pearlPath(make(0.f, 1.62f, 10.f), farLaunch, 80);
    const Meet unreachable = solve(far.data(), (int)far.size(), eye, make(0.f, 0.f, 0.f), 60, 0, 2.f, kTol);
    check(!unreachable.valid(), "far: a pearl already ten blocks ahead is never caught");

    // ---- a moving thrower: the inherited drift is removed ------------------------------------
    const Vec3f walker = make(0.f, 0.f, 0.42f);   // sprinting-ish forward speed, blocks/tick
    std::vector<Vec3f> moving = pearlPath(eye, make(0.f, 0.f, 1.5f + 0.42f), 80);
    const Meet movingMeet = solve(moving.data(), (int)moving.size(), eye, walker, 40, 0, 2.f, kTol);
    check(movingMeet.valid(), "moving: a walking throw is still caught");
    const float naive = length(moving[(size_t)movingMeet.tick] - eye);
    check(movingMeet.distance < naive - 0.1f,
          "moving: the inherited drift shortens the aim instead of being ignored");

    // Sideways drift too: the aim has to lean into it.
    const Vec3f strafer = make(0.3f, 0.f, 0.f);
    const Meet strafeMeet = solve(level.data(), (int)level.size(), eye, strafer, 40, 0, 2.f, kTol);
    check(strafeMeet.valid(), "strafe: a sideways drift still solves");
    check(strafeMeet.dir.x < -0.05f, "strafe: the aim leans against the drift the charge inherits");

    // ---- Ping Offset moves the meeting point down the pearl's flight -------------------------
    const Meet late = solve(level.data(), (int)level.size(), eye, make(0.f, 0.f, 0.f), 40,
                            pingOffsetTicks(150), 2.f, kTol);
    check(late.valid(), "ping: a ping-offset throw still finds a meeting point");
    check(late.distance > straight.distance + 0.1f,
          "ping: the offset aim lands further down the pearl's flight");

    // ---- a pearl too close to the player is refused ------------------------------------------
    std::vector<Vec3f> close = pearlPath(eye, make(0.f, 0.f, 0.4f), 80);
    const Meet feet = solve(close.data(), (int)close.size(), eye, make(0.f, 0.f, 0.f), 40, 0, 2.f, kTol);
    check(feet.valid(), "close: a slow pearl is still caught once it has travelled");
    check(feet.distance >= 2.f, "close: the charge never detonates on the player's own feet");

    // A pearl that only ever sits on the eye can never be aimed at.
    const std::vector<Vec3f> still(40, eye);
    const Meet onEye = solve(still.data(), (int)still.size(), eye, make(0.f, 0.f, 0.f), 40, 0, 2.f, kTol);
    check(!onEye.valid(), "close: a pearl at the eye is never a meeting point");

    // ---- degenerate inputs -------------------------------------------------------------------
    const Meet noTicks = solve(level.data(), (int)level.size(), eye, make(0.f, 0.f, 0.f), 0, 0, 2.f, kTol);
    check(!noTicks.valid(), "guard: Max Meet Ticks 0 never throws");
    const Meet noPath = solve(nullptr, 0, eye, make(0.f, 0.f, 0.f), 40, 0, 2.f, kTol);
    check(!noPath.valid(), "guard: no pearl path means no throw");
    const Meet shortPath = solve(level.data(), 3, eye, make(0.f, 0.f, 0.f), 40, 0, 2.f, kTol);
    check(!shortPath.valid() || (shortPath.tick + 0) < 3, "guard: the meeting tick stays inside the path");
    const Meet hugeOffset = solve(level.data(), (int)level.size(), eye, make(0.f, 0.f, 0.f), 40, 500, 2.f, kTol);
    check(hugeOffset.valid(), "guard: an offset past the path still solves");
    check(hugeOffset.distance >= 2.f, "guard: the clamped offset aim stays off the player's own feet");

    std::printf("%d checks, %d failed\n", gChecks, gFails);
    std::printf("%s\n", gFails == 0 ? "PASS" : "FAIL");
    return gFails == 0 ? 0 : 1;
}
