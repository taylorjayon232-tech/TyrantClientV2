// ============================================================================
//  Harness: the pure Shader ESP rules (agent/modules/shader_glow.h).
// ----------------------------------------------------------------------------
//  These rules decide two things, and both are easy to get wrong in a way that
//  only shows up in game:
//
//   * which entities switch Minecraft's own glowing outline on, and - far more
//     importantly - which ones switch it back off. A flag this module set must
//     be released when the module goes off, when the style changes or when an
//     entity leaves Max distance, and a glow that was already there (a server
//     effect, another mod) must never be touched.
//   * where the first-person hand glow lands, which is a projection of the view
//     space pivot the game uses for the item in hand, so it has to move with the
//     field of view the way the game's own hand does.
//
//  Build + run:  test\build-shader-glow.bat   (prints "PASS" or the failing cases)
// ============================================================================
#include "../agent/modules/shader_glow.h"

#include <cstdio>
#include <cmath>

using namespace tyr::modules::glow;

static int gChecks = 0;
static int gFails = 0;

static void check(bool ok, const char* what) {
    ++gChecks;
    if (!ok) { ++gFails; std::printf("FAIL  %s\n", what); }
}

static void checkNear(double got, double want, double tol, const char* what) {
    ++gChecks;
    if (!(std::fabs(got - want) <= tol)) {
        ++gFails;
        std::printf("FAIL  %s\n        got  %.4f\n        want %.4f (+-%.4f)\n", what, got, want, tol);
    }
}

static void checkInt(int got, int want, const char* what) {
    ++gChecks;
    if (got != want) {
        ++gFails;
        std::printf("FAIL  %s\n        got  %d\n        want %d\n", what, got, want);
    }
}

static const double kPi = 3.14159265358979323846;

// 1920x1080 at 70 degrees: the focal length the game would use, and the screen
// position of the hand pivot (0.56, -0.52, -0.71) with the item 0.20 blocks above it.
static const double kFocal70 = 540.0 / std::tan(35.0 * kPi / 180.0);

int main() {
    // ---- which registry paths are glowable ---------------------------------
    checkInt((int)kindFor("player"), (int)Kind::Player, "\"player\" is a player");
    checkInt((int)kindFor("item"),   (int)Kind::Item,   "\"item\" is a dropped item");
    checkInt((int)kindFor("zombie"), (int)Kind::Ignored, "a zombie is not glowable");
    checkInt((int)kindFor("item_frame"), (int)Kind::Ignored, "\"item_frame\" is not \"item\"");
    checkInt((int)kindFor("itemize"), (int)Kind::Ignored, "\"itemize\" is not \"item\"");
    checkInt((int)kindFor("player_x"), (int)Kind::Ignored, "\"player_x\" is not \"player\"");
    checkInt((int)kindFor("players"), (int)Kind::Ignored, "\"players\" is not \"player\"");
    checkInt((int)kindFor(""), (int)Kind::Ignored, "an empty path is ignored");
    checkInt((int)kindFor(0), (int)Kind::Ignored, "a null path is ignored");
    checkInt((int)kindFor("PLAYER"), (int)Kind::Ignored, "registry paths are lower case");
    checkInt(kMaxGlowing, 64, "the glow list is capped at 64 entities");

    // ---- who is worth flagging --------------------------------------------
    check(shouldFlag(Kind::Player, false, 10.f, 64.f), "a player in range is flagged");
    check(shouldFlag(Kind::Item, false, 63.9f, 64.f), "a dropped item in range is flagged");
    check(shouldFlag(Kind::Player, false, 64.f, 64.f), "exactly at Max distance still counts");
    check(!shouldFlag(Kind::Player, false, 64.01f, 64.f), "just past Max distance does not");
    check(!shouldFlag(Kind::Player, true, 1.f, 64.f), "the local player is never flagged");
    check(!shouldFlag(Kind::Ignored, false, 1.f, 64.f), "a non-glowable entity is never flagged");
    check(!shouldFlag(Kind::Player, false, 1.f, 0.f), "Max distance 0 (module off) flags nothing");
    check(!shouldFlag(Kind::Player, false, 1.f, -5.f), "a negative Max distance flags nothing");
    check(!shouldFlag(Kind::Player, false, -1.f, 64.f), "a negative distance is not in range");
    const float nanDistance = std::nanf("");
    check(!shouldFlag(Kind::Player, false, nanDistance, 64.f), "a NaN distance is not in range");

    // ---- what each frame does with one entity ------------------------------
    {
        const Decision d = decide(Kind::Player, false, 10.f, 64.f, false, false);
        check(d.flag && !d.clear, "a fresh target is flagged and nothing is cleared");
    }
    {
        const Decision d = decide(Kind::Player, false, 90.f, 64.f, true, true);
        check(!d.flag && d.clear, "out of range: our own glow is handed back");
    }
    {
        const Decision d = decide(Kind::Player, false, 90.f, 64.f, false, false);
        check(!d.flag && !d.clear, "out of range and not ours: nothing happens");
    }
    {
        const Decision d = decide(Kind::Player, false, 5.f, 64.f, false, true);
        check(!d.flag && !d.clear, "an existing glow is never touched or claimed");
    }
    {
        const Decision d = decide(Kind::Player, false, 5.f, 64.f, true, true);
        check(!d.flag && !d.clear, "our own glow that is still on is left alone");
    }
    {
        const Decision d = decide(Kind::Player, false, 5.f, 64.f, true, false);
        check(d.flag && !d.clear, "our flag was cleared server-side: it is re-asserted");
    }
    {
        const Decision d = decide(Kind::Item, false, 5.f, 0.f, true, true);
        check(!d.flag && d.clear, "module off: our own glow is handed back");
    }
    {
        const Decision d = decide(Kind::Item, false, 5.f, 0.f, false, false);
        check(!d.flag && !d.clear, "module off and not ours: nothing happens");
    }
    {
        const Decision d = decide(Kind::Player, true, 5.f, 64.f, true, true);
        check(!d.flag && d.clear, "anything of ours is released, even the local player");
    }
    {
        const Decision d = decide(Kind::Ignored, false, 5.f, 64.f, true, true);
        check(!d.flag && d.clear, "an entity that stops being glowable is released");
    }

    // ---- the first-person hand glow ---------------------------------------
    {
        const HandGlow g = handGlow(0.f, 1920.f, 1080.f, 25.f, 0.f, 0.f);
        check(!g.valid, "a zero field of view draws nothing");
    }
    {
        const HandGlow g = handGlow(179.5f, 1920.f, 1080.f, 25.f, 0.f, 0.f);
        check(!g.valid, "an impossible field of view draws nothing");
    }
    {
        const HandGlow g = handGlow(std::nanf(""), 1920.f, 1080.f, 25.f, 0.f, 0.f);
        check(!g.valid, "a NaN field of view draws nothing");
    }
    {
        const HandGlow g = handGlow(70.f, 0.f, 1080.f, 25.f, 0.f, 0.f);
        check(!g.valid, "a zero-width viewport draws nothing");
    }
    {
        const HandGlow g = handGlow(70.f, 1920.f, 1.f, 25.f, 0.f, 0.f);
        check(!g.valid, "a one-pixel-tall viewport draws nothing");
    }
    {
        const HandGlow g = handGlow(70.f, 1920.f, 1080.f, 0.f, 0.f, 0.f);
        check(!g.valid, "a zero-size glow draws nothing");
    }

    const HandGlow base = handGlow(70.f, 1920.f, 1080.f, 25.f, 0.f, 0.f);
    check(base.valid, "1920x1080 at 70 degrees draws the hand glow");
    checkNear((double)base.x, 960.0 + kFocal70 * (0.56 / 0.71), 0.5, "the hand glow sits on the game's hand pivot");
    checkNear((double)base.y, 540.0 + kFocal70 * ((0.52 - 0.20) / 0.71), 0.5, "the hand glow sits a little above the wrist");
    checkNear((double)base.size, kFocal70 * 0.25 / 0.71, 0.5, "Hand Glow Size 25 is a quarter block at the hand");
    check(base.x > 960.f, "the hand glow is in the right half");
    check(base.y > 540.f, "the hand glow is in the lower half");
    check(base.x < 1920.f && base.y < 1080.f, "the default hand glow is on screen");

    // It has to follow the field of view the way the game's own hand does: a
    // wider field of view pushes it towards the centre and shrinks it.
    const HandGlow wide = handGlow(90.f, 1920.f, 1080.f, 25.f, 0.f, 0.f);
    check(wide.valid, "90 degrees still draws the hand glow");
    check(wide.x < base.x, "a wider field of view moves the glow towards the centre");
    check(wide.y < base.y, "a wider field of view moves the glow up");
    check(wide.size < base.size, "a wider field of view shrinks the glow");
    checkNear((double)wide.size, (540.0 / std::tan(45.0 * kPi / 180.0)) * 0.25 / 0.71, 0.5,
              "90 degrees: the glow is sized by that field of view");

    // An extreme (very narrow) field of view puts the hand off the screen, and a
    // glow floating in the air is worse than none.
    const HandGlow narrow = handGlow(30.f, 1920.f, 1080.f, 25.f, 0.f, 0.f);
    check(!narrow.valid, "a very narrow field of view puts the hand off screen: nothing is drawn");

    // The card's nudges are percentages of the screen height, so they are the
    // same nudge at every resolution.
    const HandGlow nudged = handGlow(70.f, 1920.f, 1080.f, 25.f, 10.f, -10.f);
    checkNear((double)nudged.x, (double)base.x + 108.0, 0.5, "Nudge X of 10% moves the glow 10% of the screen height");
    checkNear((double)nudged.y, (double)base.y - 108.0, 0.5, "Nudge Y of -10% moves the glow up by the same amount");

    const HandGlow big = handGlow(70.f, 1920.f, 1080.f, 50.f, 0.f, 0.f);
    checkNear((double)big.size, (double)base.size * 2.0, 0.5, "Hand Glow Size 50 is twice the size of 25");
    const HandGlow small = handGlow(70.f, 1920.f, 1080.f, 5.f, 0.f, 0.f);
    checkNear((double)small.size, (double)base.size * 0.2, 0.5, "Hand Glow Size 5 is a fifth of the size of 25");
    check(small.valid, "the smallest Hand Glow Size still draws");
    const HandGlow largest = handGlow(70.f, 1920.f, 1080.f, 60.f, 0.f, 0.f);
    check(largest.valid, "the largest Hand Glow Size still draws");

    // A wider window keeps the glow on the hand and not at a fixed corner.
    const HandGlow ultrawide = handGlow(70.f, 2560.f, 1080.f, 25.f, 0.f, 0.f);
    check(ultrawide.valid, "an ultrawide window still draws the hand glow");
    checkNear((double)ultrawide.x, (double)base.x + 320.0, 0.5, "the glow keeps its pixel offset from the centre");

    if (gFails == 0) {
        std::printf("PASS  %d checks\n", gChecks);
        return 0;
    }
    std::printf("FAILED %d of %d checks\n", gFails, gChecks);
    return 1;
}
