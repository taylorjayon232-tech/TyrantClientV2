// ============================================================================
//  Harness: the pure drag-click pacing rules (agent/modules/click_pace.h).
// ----------------------------------------------------------------------------
//  Drag Click Friend turns these numbers into left clicks at the game window, so
//  the cases below are the ones a player feels: the ramp from Minimum to Maximum
//  CPS, the pause that stops the clicks and then re-drag clicks, and a click
//  scheduler that never bursts after a slow frame.
//
//  Build + run:  test\build-click-pace.bat   (prints "PASS" or the failing cases)
// ============================================================================
#include "../agent/modules/click_pace.h"

#include <cmath>
#include <cstdio>

namespace pace = tyr::modules::pace;

static int gChecks = 0;
static int gFails = 0;
static const double kTol = 1e-6;

static void check(bool ok, const char* what) {
    ++gChecks;
    if (!ok) { ++gFails; std::printf("FAIL  %s\n", what); }
}

static void checkNear(double got, double want, const char* what) {
    ++gChecks;
    if (!(std::fabs(got - want) <= kTol)) {
        ++gFails;
        std::printf("FAIL  %s\n        got  %.10g\n        want %.10g\n", what, got, want);
    }
}

static void checkEqInt(long long got, long long want, const char* what) {
    ++gChecks;
    if (got != want) {
        ++gFails;
        std::printf("FAIL  %s\n        got  %lld\n        want %lld\n", what, got, want);
    }
}

int main() {
    // ---- the ramp: Acceleration 1 is the slowest, 10 the fastest ------------
    checkNear(pace::rampSeconds(1), 4.0, "ramp: acceleration 1 takes 4 s");
    checkNear(pace::rampSeconds(5), 0.8, "ramp: acceleration 5 takes 0.8 s");
    checkNear(pace::rampSeconds(10), 0.4, "ramp: acceleration 10 takes 0.4 s");
    checkNear(pace::rampSeconds(0), pace::rampSeconds(1), "ramp: below the range clamps to the slowest");
    checkNear(pace::rampSeconds(11), pace::rampSeconds(10), "ramp: above the range clamps to the fastest");
    checkNear(pace::rampSeconds(-4), pace::rampSeconds(1), "ramp: negative clamps to the slowest");

    // ---- the ramp's rate ---------------------------------------------------
    checkNear(pace::cpsAt(8, 20, 5, 0.0), 8.0, "cps: starts at Minimum");
    checkNear(pace::cpsAt(8, 20, 5, -1.0), 8.0, "cps: a negative elapsed time stays at Minimum");
    checkNear(pace::cpsAt(8, 20, 5, 0.4), 14.0, "cps: half way through the ramp");
    checkNear(pace::cpsAt(8, 20, 5, 0.8), 20.0, "cps: reaches Maximum at the end of the ramp");
    checkNear(pace::cpsAt(8, 20, 5, 5.0), 20.0, "cps: stays at Maximum past the ramp");
    check(pace::cpsAt(8, 20, 5, 0.2) < pace::cpsAt(8, 20, 5, 0.5), "cps: rises with elapsed time");
    check(pace::cpsAt(8, 20, 5, 0.5) < pace::cpsAt(8, 20, 5, 0.79), "cps: keeps rising to the end");
    checkNear(pace::cpsAt(8, 20, 10, 0.2), 14.0, "cps: a faster acceleration ramps in half the time");
    checkNear(pace::cpsAt(20, 8, 5, 4.0), 20.0, "cps: Maximum below Minimum collapses onto Minimum");
    checkNear(pace::cpsAt(0, 90, 5, 0.0), 1.0, "cps: Minimum below the range clamps to 1");
    checkNear(pace::cpsAt(8, 90, 5, 4.0), 40.0, "cps: Maximum above the range clamps to 40");
    checkNear(pace::cpsAt(8, 8, 5, 4.0), 8.0, "cps: an equal pair never moves");

    // ---- the pause cycle ---------------------------------------------------
    // Acceleration 10: ramp 0.4 s, hold 1.0 s, then a 2 s pause -> a 3.4 s period.
    check(pace::cycleAt(8, 20, 10, 2.0, 0.0).clicking, "cycle: clicks immediately");
    checkNear(pace::cycleAt(8, 20, 10, 2.0, 0.0).cps, 8.0, "cycle: the first click is at Minimum");
    check(pace::cycleAt(8, 20, 10, 2.0, 0.4).clicking, "cycle: still clicking at the top of the ramp");
    checkNear(pace::cycleAt(8, 20, 10, 2.0, 0.4).cps, 20.0, "cycle: Maximum at the top of the ramp");
    checkNear(pace::cycleAt(8, 20, 10, 2.0, 1.3).cps, 20.0, "cycle: Maximum is held for a second");
    check(!pace::cycleAt(8, 20, 10, 2.0, 1.4).clicking, "cycle: the pause starts after ramp + hold");
    checkNear(pace::cycleAt(8, 20, 10, 2.0, 1.5).cps, 0.0, "cycle: no clicks during the pause");
    check(!pace::cycleAt(8, 20, 10, 2.0, 3.3).clicking, "cycle: the pause runs to the end of the period");
    check(pace::cycleAt(8, 20, 10, 2.0, 3.4).clicking, "cycle: re-drag clicks after the pause");
    checkNear(pace::cycleAt(8, 20, 10, 2.0, 3.4).cps, 8.0, "cycle: the ramp restarts at Minimum");
    checkNear(pace::cycleAt(8, 20, 10, 2.0, 3.8).cps, 20.0, "cycle: back to Maximum inside the new period");
    check(!pace::cycleAt(8, 20, 10, 2.0, 5.0).clicking, "cycle: the second period pauses again");
    check(pace::cycleAt(8, 20, 10, 0.0, 60.0).clicking, "cycle: without a pause it keeps clicking");
    checkNear(pace::cycleAt(8, 20, 10, 0.0, 60.0).cps, 20.0, "cycle: without a pause it stays at Maximum");
    check(pace::cycleAt(8, 20, 10, 99.0, 4.6).clicking, "cycle: a pause above the range clamps to 3 s");
    checkNear(pace::cycleAt(8, 20, 10, 99.0, 4.6).cps, 14.0, "cycle: the clamped period ramps again at 4.6 s");

    // ---- the click scheduler ----------------------------------------------
    {
        pace::Timer timer;
        check(timer.due(0.0, 0.05), "timer: the first click is immediate");
        check(!timer.due(0.02, 0.05), "timer: no second click inside the interval");
        check(timer.due(0.05, 0.05), "timer: the next click lands one interval later");
    }
    {
        pace::Timer timer;
        long long clicks = 0;
        for (int i = 0; i < 1000; ++i) if (timer.due(i * 0.001, 1.0 / 20.0)) ++clicks;
        checkEqInt(clicks, 20, "timer: 1 second at 20 CPS is 20 clicks");
    }
    {
        pace::Timer timer;
        check(timer.due(0.0, 0.05), "timer: armed before the long frame");
        check(timer.due(0.2, 0.05), "timer: a 0.2 s frame still produces a click");
        check(!timer.due(0.24, 0.05), "timer: no burst after the long frame");
        check(timer.due(0.25, 0.05), "timer: the schedule continues after the long frame");
    }
    {
        pace::Timer timer;
        check(!timer.due(1.0, 0.0), "timer: a zero interval never clicks");
        check(!timer.due(1.1, -1.0), "timer: a negative interval never clicks");
    }
    {
        pace::Timer timer;
        check(timer.due(0.0, 0.05), "timer: fires before the reset");
        timer.reset();
        check(timer.due(9.0, 0.05), "timer: reset re-arms to click immediately");
        check(!timer.due(9.01, 0.05), "timer: the interval applies again after a reset");
    }

    std::printf("%d checks, %d failed\n", gChecks, gFails);
    std::printf("%s\n", gFails == 0 ? "PASS" : "FAIL");
    return gFails == 0 ? 0 : 1;
}
