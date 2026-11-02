// ============================================================================
//  Harness: the OS cursor rules (agent/gui/cursor_rules.h) - when a pointer is
//  wanted, and how the counted display counter is moved without stranding it.
//
//  The bug this pins down: the pointer was invisible in every Minecraft screen
//  (only Tyrant's own interface re-showed it), clicks still landed, and it never
//  came back until the game restarted. ShowCursor is counted, not a flag, and the
//  old loops called it unconditionally on every transition, so the counter walked
//  one step further from the boundary each time until a single ShowCursor(TRUE)
//  from the game could no longer climb back out.
//
//  The second half pins the opposite mistake: showing the pointer whenever a screen
//  looked open made it visible in-world, first and third person alike. The client
//  now shows a pointer only for its own menu and otherwise leaves the game's own
//  cursor state alone.
//
//  Build + run:  test\build-cursor.bat   (prints "PASS" or the failing cases)
// ============================================================================
#include "../agent/gui/cursor_rules.h"

#include <cstdio>

using namespace tyr::gui::cursor;

static int g_checks = 0;
static int g_failed = 0;

static void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        ++g_failed;
        std::printf("FAIL  %s\n", what);
    }
}

// The OS display counter as Windows keeps it: every ShowCursor call moves it by one
// and the pointer is visible while it is >= 0.
struct Counter {
    int count = 0;
    int call(bool show) { return count += show ? 1 : -1; }
    bool visible() const { return count >= 0; }
};

// What the old preFrame did on a transition: call ShowCursor at least once, whatever
// the pointer was doing before.
static void oldShow(Counter& c) { while (c.call(true) < 0) {} }
static void oldHide(Counter& c) { while (c.call(false) >= 0) {} }

// What the fix does: only call ShowCursor while GetCursorInfo disagrees with the
// wanted state, at most kMaxSteps times per frame. Leave means no call at all.
static void newApply(Counter& c, Want w) {
    if (w == Want::Leave) return;
    for (int i = 0; i < kMaxSteps && needsChange(c.visible(), w); ++i)
        c.call(w == Want::Shown);
}

int main() {
    // ---- when a pointer is wanted ----
    check(want(true, false, true, false) == Want::Shown, "the menu shows a pointer in-world");
    check(want(true, true, true, false) == Want::Shown, "the menu shows a pointer over a game screen");
    check(want(true, false, false, false) == Want::Shown, "the menu wants a pointer even without a readable frame");
    check(want(false, true, true, false) == Want::Leave,
          "a Minecraft screen is left to the game, which shows its own pointer");
    check(want(false, false, true, false) == Want::Leave,
          "in-world with nothing owed the client touches nothing (first and third person stay as the game has them)");
    check(want(false, false, false, false) == Want::Leave,
          "an unreadable frame (title screen, world load) never moves the pointer");

    // ---- the debt the menu owes, and when it must not be paid ----
    check(want(false, false, true, true) == Want::Hidden,
          "closing the menu in-world takes back the pointer the menu showed");
    check(want(false, true, true, true) == Want::Leave,
          "but never over a Minecraft screen, where the game is showing its own pointer");
    check(want(false, true, true, false) == Want::Leave, "an open screen with nothing owed is untouched");
    check(want(false, false, false, true) == Want::Leave,
          "an unreadable frame is not used to pay the debt on a guess");

    // ---- only a mismatch is worth a call ----
    check(!needsChange(true, Want::Shown), "an already visible pointer is never touched");
    check(!needsChange(false, Want::Hidden), "an already hidden pointer is never touched");
    check(needsChange(false, Want::Shown), "a hidden pointer is shown");
    check(needsChange(true, Want::Hidden), "a visible pointer is hidden");

    // ---- the reported failure, reproduced with the old hide pass ----
    // `while (ShowCursor(FALSE) >= 0)` calls ShowCursor at least once even when the
    // pointer is already hidden - the game itself had just hidden it on entering
    // CURSOR_DISABLED - so every closing pass sank the counter one step further below
    // the boundary. Past -1, the game's own single ShowCursor(TRUE) (what GLFW does
    // when it leaves CURSOR_DISABLED for a screen) can no longer show the pointer.
    {
        Counter old;
        for (int pass = 0; pass < 3; ++pass) oldHide(old);
        check(old.count == -3, "each old hide pass spends a call even when already hidden");
        old.call(true);     // the game's own ShowCursor(TRUE)
        check(!old.visible(), "so one game-side ShowCursor(TRUE) no longer shows the pointer");
    }
    {
        // The old show pass did climb back, which is why the pointer reappeared in the
        // interface and nowhere else: Tyrant re-showed it there, the game could not.
        Counter old;
        old.count = -3;
        oldShow(old);
        check(old.visible() && old.count == 0, "only an explicit re-show climbs out of the stranded counter");
    }

    // ---- the fix: the counter never leaves {-1, 0}, so one call toggles it ----
    {
        Counter fixed;
        for (int cycle = 0; cycle < 10; ++cycle) {
            newApply(fixed, Want::Shown);
            check(fixed.visible(), "every cycle shows the pointer");
            newApply(fixed, Want::Hidden);
            check(!fixed.visible(), "every cycle hides it again");
        }
        check(fixed.count == -1, "ten show/hide cycles leave the counter exactly at -1");
        fixed.call(true);   // the game's own ShowCursor(TRUE)
        check(fixed.visible(), "one game-side ShowCursor(TRUE) brings the pointer back");
    }

    {
        // Hiding a pointer that is already hidden must spend no call at all - that is the
        // difference between the old loops and this one.
        Counter idle;
        idle.count = -1;
        newApply(idle, Want::Hidden);
        check(idle.count == -1, "a hide pass over an already hidden pointer calls nothing");
    }

    // ---- recovery: a counter already stranded far below -1 is repaired ----
    {
        Counter stranded;
        stranded.count = -20;                   // left behind by an earlier session
        newApply(stranded, Want::Shown);
        check(stranded.visible(), "a stranded counter is walked back up to a visible pointer");
        check(stranded.count == 0, "and settles on exactly 0, so the game's own hide still works");
        newApply(stranded, Want::Hidden);
        check(stranded.count == -1, "hiding again settles on exactly -1");
    }
    {
        // A counter left positive (some other overlay called ShowCursor(TRUE) twice).
        Counter high;
        high.count = 3;
        newApply(high, Want::Hidden);
        check(!high.visible() && high.count == -1,
              "a counter left above 0 is walked down to exactly -1 instead of being ignored");
    }

    // ---- a full menu session in-world, then the client stops touching it ----
    {
        Counter c;
        c.count = -1;                       // the game hid the pointer before the menu opened
        newApply(c, Want::Shown);
        check(c.visible(), "the menu shows the pointer");
        newApply(c, Want::Hidden);
        check(!c.visible() && c.count == -1,
              "closing the menu puts the counter back exactly where the game had it");
        for (int i = 0; i < 20; ++i) newApply(c, Want::Leave);
        check(c.count == -1, "afterwards the client makes no further ShowCursor call");
    }
    {
        // In a Minecraft screen the client is not in charge at all: twenty frames of
        // Leave must not spend a single call, whatever the counter looks like.
        Counter game;
        game.count = 0;
        for (int i = 0; i < 20; ++i) newApply(game, Want::Leave);
        check(game.count == 0 && game.visible(),
              "a game screen keeps the pointer the game showed, untouched by the client");
    }

    // ---- the frame budget is bounded, and repeated frames finish the job ----
    {
        Counter deep;
        deep.count = -(kMaxSteps + 5);
        newApply(deep, Want::Shown);
        check(!deep.visible(), "one frame does not pretend to finish an absurd counter");
        newApply(deep, Want::Shown);
        check(deep.visible(), "the next frame does finish it");
    }

    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    std::printf("%s\n", g_failed == 0 ? "PASS" : "FAILED");
    return g_failed == 0 ? 0 : 1;
}
