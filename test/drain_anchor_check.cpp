// ============================================================================
//  Harness: Auto Drain's web rule (agent/modules/drain_anchor.h) - which cell a
//  cobweb lands in, what the click is made against, and which shapes are refused.
//
//  Build + run:  test\build-drain-anchor.bat   (prints "PASS" or the failing cases)
// ============================================================================
#include "../agent/modules/drain_anchor.h"

#include <cstdio>

using tyr::modules::drain::Anchor;
using tyr::modules::drain::resolve;

static int g_checks = 0;
static int g_failed = 0;

static void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        ++g_failed;
        std::printf("FAIL  %s\n", what);
    }
}

int main() {
    // ---- the reported case: looking at the top face of the block under the water ----
    // Hit = a real block, side = UP (faceY +1, face id 1), the cell above holds the water.
    {
        const Anchor a = resolve(/*hitIsWater=*/false, /*faceVisible=*/true, /*faceY=*/1, /*hitFace=*/1,
                                 /*waterInCell=*/true, /*solidBelow=*/false);
        check(a.valid, "the top face of the block under a water source drains");
        check(a.placeDx == 0 && a.placeDy == 1 && a.placeDz == 0,
              "the web lands in the cell above that block");
        check(a.clickDx == 0 && a.clickDy == 0 && a.clickDz == 0,
              "the click is made against the block the ray stopped on");
        check(a.face == 1, "the click goes against that block's top face");
    }

    // ---- the water itself under the crosshair, with a real block under it ----
    // The web goes into the fluid's own cell, but only against the TOP face of the block below, so
    // the click can never be a placement on a fluid cell with nothing under it (the air place).
    {
        const Anchor a = resolve(true, true, 1, 1, true, /*solidBelow=*/true);
        check(a.valid, "looking into water that sits on a block drains");
        check(a.placeDx == 0 && a.placeDy == 0 && a.placeDz == 0,
              "the web lands in the water's own cell");
        check(a.clickDx == 0 && a.clickDy == -1 && a.clickDz == 0,
              "the click is made against the block directly under the water");
        check(a.face == 1, "and against that block's top face");
    }

    // ---- the water itself with nothing under it: refused, never air-placed ----
    {
        check(!resolve(true, true, 1, 1, true, /*solidBelow=*/false).valid,
              "water with nothing solid under it is refused");
        // Whatever side the ray read on the fluid, without a block below there is no anchor.
        check(!resolve(true, true, 0, 4, true, false).valid,
              "a side hit on floating water is refused too");
    }

    // ---- every other face is refused: the web would land beside the water ----
    {
        check(!resolve(false, true, 0, 2, true, false).valid,
              "the north face of a block does not anchor a drain");
        check(!resolve(false, true, 0, 5, true, false).valid,
              "the east face does not anchor a drain");
        check(!resolve(false, true, -1, 0, true, false).valid,
              "the bottom face does not anchor a drain");
        check(!resolve(false, false, 0, 1, true, false).valid,
              "a hit with no readable side is refused");
    }

    // ---- no water in the cell the click would fill: refused, never air-placed ----
    {
        check(!resolve(false, true, 1, 1, false, false).valid,
              "a top face with nothing but air above it is refused");
        check(!resolve(true, true, 1, 1, false, false).valid,
              "flowing water that is not a source is refused");
    }

    // ---- the invalid anchor reports no movement at all, so a stale value can never be placed ----
    {
        const Anchor a;
        check(!a.valid && a.placeDx == 0 && a.placeDy == 0 && a.placeDz == 0 &&
              a.clickDx == 0 && a.clickDy == 0 && a.clickDz == 0,
              "an invalid anchor carries no cell");
    }

    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    std::printf("%s\n", g_failed == 0 ? "PASS" : "FAILED");
    return g_failed == 0 ? 0 : 1;
}
