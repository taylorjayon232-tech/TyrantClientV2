// ============================================================================
//  Harness: the pure Xbow Macro stage-timing rules (agent/modules/xbow_timing.h).
// ----------------------------------------------------------------------------
//  Two rules, and the cases below are the two ways they were got wrong:
//
//   1. A stage delay is the slider value and nothing more. The rule used to add
//      a one-tick (50 ms) floor, so every slider below 50 ms behaved
//      identically - "the values aren't changing even at 0 MS" - and every
//      stage was costed at slider + 50 ms. The cases below pin the slider as
//      both a lower and an upper bound: 0 ms means the next frame.
//
//   2. An interaction that follows a hotbar swap still waits for the swap's own
//      tick. That is what keeps the rail, cart and flame from being corrected
//      away, and it must not be weakened by a small slider value.
//
//  Build + run:  test\build-xbow-timing.bat   (prints "PASS" or the failing cases)
// ============================================================================
#include "../agent/modules/xbow_timing.h"

#include <cstdio>
#include <cmath>

using namespace tyr::modules::xbow;

static int gChecks = 0;
static int gFails = 0;
static const double kEps = 1e-6;

static void check(bool ok, const char* what) {
    ++gChecks;
    if (!ok) { ++gFails; std::printf("FAIL  %s\n", what); }
}

static void checkNear(double got, double want, const char* what) {
    ++gChecks;
    if (std::fabs(got - want) > kEps) {
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

int main() {
    const double tick = kSwapSettleDelay;

    // ---- the slider value is clamped to its own domain -------------------------
    checkEqInt(clampUserDelayMs(0), 0, "clamp: 0 stays 0");
    checkEqInt(clampUserDelayMs(300), 300, "clamp: 300 stays 300");
    checkEqInt(clampUserDelayMs(600), 600, "clamp: 600 stays 600");
    checkEqInt(clampUserDelayMs(-1), 0, "clamp: negatives fall to 0");
    checkEqInt(clampUserDelayMs(-5000), 0, "clamp: a wild negative falls to 0");
    checkEqInt(clampUserDelayMs(601), 600, "clamp: above the slider tops out at 600");
    checkEqInt(clampUserDelayMs(99999), 600, "clamp: a wild positive tops out at 600");

    // ---- the delay is the slider value, to the millisecond ---------------------
    checkNear(stageDelaySeconds(0), 0.0, "delay: 0 ms is no wait at all");
    checkNear(stageDelaySeconds(25), 0.025, "delay: 25 ms is 25 ms");
    checkNear(stageDelaySeconds(50), tick, "delay: 50 ms is one tick");
    checkNear(stageDelaySeconds(51), 0.051, "delay: 51 ms is taken as given");
    checkNear(stageDelaySeconds(100), 0.1, "delay: 100 ms is taken as given");
    checkNear(stageDelaySeconds(600), 0.6, "delay: 600 ms is taken as given");
    checkNear(stageDelaySeconds(-100), 0.0, "delay: a negative slider value is no wait");
    checkNear(stageDelaySeconds(5000), 0.6, "delay: 5000 ms is clamped to 600");
    checkNear(tick, 0.05, "one tick is 50 ms");

    // The live report: the values did not change anything below 50 ms, because the 0 ms
    // slider was silently worth a whole tick.
    check(stageDelaySeconds(0) < tick, "delay: 0 ms is faster than one tick");
    check(stageDelaySeconds(10) - stageDelaySeconds(0) > 0.009,
          "delay: 0 ms and 10 ms are different waits");
    check(stageDelaySeconds(50) - stageDelaySeconds(10) > 0.039,
          "delay: 10 ms and 50 ms are different waits");

    // Nothing the user can set is ever worth more than the value they asked for.
    bool sliderIsUpperBound = true;
    bool sliderIsNonNegative = true;
    bool sliderNeverUndercutsZero = true;
    for (int ms = -1000; ms <= 1000; ++ms) {
        const double got = stageDelaySeconds(ms);
        if (got > clampUserDelayMs(ms) / 1000.0 + kEps) sliderIsUpperBound = false;
        if (got < -kEps) sliderIsNonNegative = false;
        if (clampUserDelayMs(ms) > 0 && got <= 0.0) sliderNeverUndercutsZero = false;
    }
    check(sliderIsUpperBound, "delay: no slider value waits longer than it says");
    check(sliderIsNonNegative, "delay: no slider value produces a negative wait");
    check(sliderNeverUndercutsZero, "delay: every positive slider value waits something");

    // ---- delayElapsed ----------------------------------------------------------
    const double stageAt = 10.0;
    check(delayElapsed(stageAt, stageAt, 0), "elapsed: a 0 ms stage is due at its own instant");
    check(delayElapsed(stageAt + 0.0005, stageAt, 0), "elapsed: 0.5 ms into a 0 ms stage is due");
    check(delayElapsed(stageAt + 0.016, stageAt, 0), "elapsed: one 60 fps frame into a 0 ms stage is due");
    check(delayElapsed(stageAt + 0.5, stageAt, 0), "elapsed: half a second into a 0 ms stage is due");
    check(!delayElapsed(stageAt + 0.024, stageAt, 25), "elapsed: 24 ms into a 25 ms stage is too early");
    check(delayElapsed(stageAt + 0.025, stageAt, 25), "elapsed: 25 ms into a 25 ms stage is due");
    check(delayElapsed(stageAt + 0.049, stageAt, 25), "elapsed: a tick into a 25 ms stage is due");
    check(!delayElapsed(stageAt + 0.049, stageAt, 50), "elapsed: 49 ms into a 50 ms stage is too early");
    check(delayElapsed(stageAt + 0.05, stageAt, 50), "elapsed: 50 ms into a 50 ms stage is due");
    check(!delayElapsed(stageAt + 0.099, stageAt, 100), "elapsed: 99 ms into a 100 ms stage is too early");
    check(delayElapsed(stageAt + 0.1, stageAt, 100), "elapsed: 100 ms into a 100 ms stage is due");
    check(!delayElapsed(stageAt + 0.599, stageAt, 600), "elapsed: 599 ms into a 600 ms stage is too early");
    check(delayElapsed(stageAt + 0.6, stageAt, 600), "elapsed: 600 ms into a 600 ms stage is due");

    // A 0 ms stage fires strictly earlier than the smallest slider above it.
    check(delayElapsed(stageAt + 0.01, stageAt, 0) && !delayElapsed(stageAt + 0.01, stageAt, 25),
          "elapsed: a 0 ms stage is due where a 25 ms stage still is not");

    // ---- slotSettled: the interaction after a hotbar swap ----------------------
    check(slotSettled(0.0, -1.0), "settled: nothing was ever swapped");
    check(slotSettled(12345.0, -1.0), "settled: no swap is settled at any time");
    check(!slotSettled(0.0, 0.0), "settled: the swap's own instant is not settled");
    check(!slotSettled(0.049, 0.0), "settled: 49 ms after the swap is not settled");
    check(slotSettled(0.05, 0.0), "settled: exactly one tick after the swap is settled");
    check(slotSettled(5.0, 0.0), "settled: long after the swap is settled");
    check(!slotSettled(100.0, 99.99), "settled: 10 ms after a late swap is not settled");
    check(slotSettled(100.05, 99.99), "settled: a tick after a late swap is settled");

    // ---- the ordering property the safety fix is about -------------------------
    // A swap at t = 0 followed by a placement: the placement is refused while it
    // would share the swap's tick, and allowed once the tick has passed.
    const double swapAt = 0.0;
    check(!slotSettled(0.049, swapAt), "ordering: a placement 49 ms after the swap is refused");
    check(slotSettled(0.05, swapAt), "ordering: the same placement a tick later goes out");

    // ...and no slider value can shorten that: the gate is its own rule.
    bool gateHoldsForEverySlider = true;
    for (int ms = 0; ms <= 600; ++ms) {
        if (slotSettled(swapAt + 0.049, swapAt)) gateHoldsForEverySlider = false;
        if (!slotSettled(swapAt + kSwapSettleDelay, swapAt)) gateHoldsForEverySlider = false;
    }
    check(gateHoldsForEverySlider, "ordering: the swap gate is independent of the slider");

    // A stage that is due and settled goes out; that is the pair the macro checks.
    const bool dueAtZero = delayElapsed(swapAt + 0.05, swapAt, 0);
    check(dueAtZero && slotSettled(swapAt + 0.05, swapAt),
          "ordering: at 0 ms the stage is due the moment the swap's tick has passed");

    std::printf("%d checks, %d failed\n", gChecks, gFails);
    std::printf("%s\n", gFails == 0 ? "PASS" : "FAIL");
    return gFails == 0 ? 0 : 1;
}
