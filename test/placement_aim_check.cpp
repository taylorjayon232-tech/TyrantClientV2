// ============================================================================
//  Harness: the pure placement aim rule (agent/modules/placement_aim.h) - the
//  Camera Sensitivity the Stun Web and Xbow Macro cards added. The slider is 0-50
//  (it was 0-10) so the top half can be genuinely fast, while every setting at or
//  below 10 keeps the pace it always had; every setting still has to close on the
//  block in a fraction of a second, because the walk these placements used to make
//  took about half a second - the delay reported as "Smooth waits, then places".
//
//  Build + run:  test\build-placement-aim.bat   (prints "PASS" or the failing cases)
// ============================================================================
#include "../agent/modules/placement_aim.h"

#include <cstdio>
#include <cmath>
#include <vector>

using namespace tyr::modules::aim;

static int g_checks = 0;
static int g_failed = 0;

static void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        ++g_failed;
        std::printf("FAIL  %s\n", what);
    }
}

// The fraction of the remaining angle covered over `seconds`, accumulated frame by frame the way
// the placement aim walks it (a fresh approachStep per frame).
static float walkedFraction(float sensitivity, float seconds, float frame) {
    float remaining = 1.f;
    for (float t = 0.f; t < seconds; t += frame)
        remaining -= remaining * approachStep(sensitivity, frame);
    return 1.f - remaining;
}

int main() {
    // ---- the slider is clamped, and the middle is the pace the placements use ----
    check(std::abs(approachSpeed(5.f) - 25.f) < 1e-4f, "sensitivity 5 is the 25/s pace the placements walk at");
    check(std::abs(approachSpeed(0.f) - 14.f) < 1e-4f, "sensitivity 0 is the slow end");
    check(std::abs(approachSpeed(10.f) - 36.f) < 1e-4f, "sensitivity 10 still walks at the pace it always had");
    check(std::abs(approachSpeed(50.f) - 124.f) < 1e-4f, "sensitivity 50 is the new fast end");
    check(approachSpeed(-3.f) == approachSpeed(0.f), "below the slider is clamped to the slow end");
    check(approachSpeed(99.f) == approachSpeed(50.f), "above the slider is clamped to the fast end");

    // ---- faster the higher the slider: strictly monotonic, so 50 always beats 49 ----
    bool monotonic = true;
    for (int i = 1; i <= 50; ++i)
        if (!(approachSpeed((float)i) > approachSpeed((float)(i - 1)))) monotonic = false;
    check(monotonic, "each step of the slider is faster than the one below it");

    // ---- the step never snaps and never stalls ----
    bool sane = true;
    for (int i = 0; i <= 50; ++i) {
        for (float dt = 0.0005f; dt <= 0.2f; dt += 0.0166f) {
            const float step = approachStep((float)i, dt);
            if (!(step > 0.f) || step > 0.5f) sane = false;
        }
    }
    check(sane, "a frame always moves towards the target and never covers more than half of it");
    check(approachStep(50.f, 0.f) < 0.5f, "a zero deltaTime cannot snap the view");
    check(approachStep(50.f, 5.f) <= 0.5f, "a huge deltaTime cannot snap either");
    check(approachStep(0.f, 0.0001f) > 0.f, "the smallest frame still makes progress");

    // ---- what the user sees: higher sensitivity reaches the block sooner ----
    const float slow = walkedFraction(0.f, 0.25f, 1.f / 60.f);
    const float mid  = walkedFraction(5.f, 0.25f, 1.f / 60.f);
    const float fast = walkedFraction(10.f, 0.25f, 1.f / 60.f);
    check(slow < mid && mid < fast, "0.25 s of a Smooth placement walks furthest at the highest slider");
    check(fast > 0.95f, "the top of the slider is nearly on the block inside a quarter of a second");
    // The fix: no setting may still be walking when it matters. A 90 degree turn is on the block
    // (inside the 1.5 degree tolerance, so 98.3% of the arc) well under half a second even at the
    // bottom of the slider, and this is where the old 4..20/s mapping failed - it needed most of a
    // second at 0 and half of one at 5.
    bool arrivesInTime = true;
    for (int i = 0; i <= 10; ++i)
        if (walkedFraction((float)i, 0.35f, 1.f / 60.f) < 0.985f) arrivesInTime = false;
    check(arrivesInTime, "every sensitivity is on the block inside 0.35 s of a 90 degree turn");
    check(walkedFraction(5.f, 0.20f, 1.f / 60.f) > 0.985f,
          "the middle of the slider is on the block inside 0.2 s");
    check(walkedFraction(0.f, 0.25f, 1.f / 60.f) > 0.95f,
          "even the slow end is nearly on the block inside a quarter of a second");
    check(std::abs(walkedFraction(5.f, 0.0166f, 1.f / 60.f) - 0.34f) < 0.05f,
          "one frame at the default slider is a nudge, still well under the half-arc cap");

    // ---- the new top of the slider: a 90 degree turn is on the block in a couple of frames ----
    // The per-frame step is capped at half the remaining turn (so a frame is never a snap), which
    // the very top of the slider is limited by: six frames - a tenth of a second - is 98.4%.
    check(walkedFraction(50.f, 0.15f, 1.f / 60.f) > 0.985f,
          "the top of the slider closes a 90 degree turn inside 0.15 s");
    check(walkedFraction(25.f, 0.1f, 1.f / 60.f) > walkedFraction(10.f, 0.1f, 1.f / 60.f),
          "halfway up the new range is faster than the old maximum");

    // ---- frame-rate independence: the same wall time walks the same distance ----
    const float at60 = walkedFraction(5.f, 0.3f, 1.f / 60.f);
    const float at144 = walkedFraction(5.f, 0.3f, 1.f / 144.f);
    check(std::abs(at60 - at144) < 0.02f, "the walk does not depend on the frame rate");

    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    std::printf("%s\n", g_failed == 0 ? "PASS" : "FAILED");
    return g_failed == 0 ? 0 : 1;
}
