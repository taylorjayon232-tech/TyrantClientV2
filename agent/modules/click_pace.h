// ============================================================================
//  Passive — Drag Click Friend pacing rules.
// ----------------------------------------------------------------------------
//  Pure maths, no JNI and no ImGui, so the harness (test/click_pace_check.cpp)
//  exercises the exact rules the module runs. The module only turns the result
//  into left clicks at the game window.
//
//  Minimum CPS, Maximum CPS, Acceleration and Drag Click Pause are the four
//  cards on the module. Acceleration 1 ramps slowest (4 s from Minimum to
//  Maximum), 10 the fastest (0.4 s). With a pause set the clicker holds Maximum
//  for a second, stops for the pause, then starts the ramp again; without one it
//  ramps up and stays there for as long as the button is held.
// ============================================================================
#pragma once

#include <cmath>

namespace tyr {
namespace modules {
namespace pace {

// Card domains. Kept here so the module and the harness agree on the clamps.
constexpr double kMinCpsLimit      = 1.0;
constexpr double kMaxCpsLimit      = 40.0;
constexpr int    kMinAcceleration  = 1;
constexpr int    kMaxAcceleration  = 10;
constexpr double kMaxPauseSeconds  = 3.0;
constexpr double kSlowestRampSeconds = 4.0;   // Acceleration 1
constexpr double kHoldSeconds      = 1.0;     // at Maximum, before a pause

inline double clampD(double value, double low, double high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

// Seconds the ramp needs to reach Maximum CPS at this Acceleration.
inline double rampSeconds(int acceleration) {
    return kSlowestRampSeconds /
           clampD((double)acceleration, (double)kMinAcceleration, (double)kMaxAcceleration);
}

// CPS reached `elapsed` seconds into a ramp. A Maximum below Minimum is read as
// "no ramp" and the two collapse onto Minimum, so the clicker never slows down
// when the two sliders cross.
inline double cpsAt(double minCps, double maxCps, int acceleration, double elapsed) {
    double low = clampD(minCps, kMinCpsLimit, kMaxCpsLimit);
    double high = clampD(maxCps, kMinCpsLimit, kMaxCpsLimit);
    if (high < low) high = low;
    if (elapsed <= 0.0) return low;
    const double ramp = rampSeconds(acceleration);
    const double t = ramp <= 0.0 ? 1.0 : clampD(elapsed / ramp, 0.0, 1.0);
    return low + (high - low) * t;
}

struct Cycle {
    double cps = 0.0;      // clicks per second right now
    bool   clicking = false;   // false only while the pause is running
};

// The clicker's phase for a hold that has lasted `elapsed` seconds.
inline Cycle cycleAt(double minCps, double maxCps, int acceleration,
                     double pauseSeconds, double elapsed) {
    const double pause = clampD(pauseSeconds, 0.0, kMaxPauseSeconds);
    const double ramp  = rampSeconds(acceleration);
    const double hold  = kHoldSeconds;
    const double period = ramp + hold + pause;
    if (pause <= 0.0 || elapsed < ramp + hold) {
        return { cpsAt(minCps, maxCps, acceleration, elapsed), true };
    }
    // Inside a period: ramp, then hold at Maximum, then the pause. The next period
    // starts the ramp again ("re-drag clicks").
    const double inPeriod = elapsed - std::floor(elapsed / period) * period;
    if (inPeriod < ramp) return { cpsAt(minCps, maxCps, acceleration, inPeriod), true };
    if (inPeriod < ramp + hold) return { cpsAt(minCps, maxCps, acceleration, ramp), true };
    return { 0.0, false };
}

// Click scheduler: `due(now, interval)` reports at most one click per interval and
// never queues a backlog, so a frame that took longer than one interval (a hitch, a
// resized window) still produces exactly one click instead of a burst.
struct Timer {
    double next = 0.0;
    bool   armed = false;

    bool due(double now, double interval) {
        if (!(interval > 0.0)) return false;
        if (!armed) { armed = true; next = now; }
        if (now + 1e-9 < next) return false;
        next += interval;
        if (next <= now) next = now + interval;
        return true;
    }

    void reset() { armed = false; next = 0.0; }
};

} // namespace pace
} // namespace modules
} // namespace tyr
