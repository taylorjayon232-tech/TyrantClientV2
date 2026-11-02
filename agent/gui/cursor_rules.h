// ============================================================================
//  Cursor visibility rules - pure, so the harness pins them without a window
//  or a JVM.
//
//  Two complaints shaped these rules.
//
//  1. The pointer was invisible in the inventory, the Escape menu and every other
//     Minecraft screen, clicks still landed, and it never came back until the game
//     was restarted. Windows keeps a *counted* display counter, not a flag:
//     ShowCursor(TRUE/FALSE) is +1 / -1 and the pointer is visible while the count
//     is >= 0. The old code drove it with two unconditional loops - `while
//     (ShowCursor(TRUE) < 0)` while the menu or a screen was up, and `while
//     (ShowCursor(FALSE) >= 0)` the moment they closed. The hide pass calls
//     ShowCursor at least once even when the game had just hidden the pointer
//     itself, so every close pushed the count one step further below -1, and past
//     that a single ShowCursor(TRUE) from the game can no longer climb back out.
//
//  2. Fixing that by showing the pointer whenever a screen *looked* open made it
//     visible in-world too - first and third person alike - because the client was
//     pushing its own idea of visibility instead of mirroring the game's.
//
//  So the client now shows a pointer only while Passive's own menu is up. Every
//  Minecraft screen is left to the game, which already switches GLFW to
//  CURSOR_NORMAL there; with the counter no longer drifted, that is enough. Leave
//  means "call nothing, set no shape, let the game decide", and it is the answer
//  whenever the client cannot be sure - which is what respecting Minecraft's cursor
//  visibility means from inside the process.
// ============================================================================
#pragma once

namespace tyr {
namespace gui {
namespace cursor {

// Leave  - the game's own cursor state stands; the client touches nothing.
// Hidden - take back a pointer the client itself showed.
// Shown  - the client needs the pointer (its menu is up).
enum class Want { Leave, Hidden, Shown };

// `showingForMenu` is the debt the client owes: the menu showed the pointer while
// the game was in-world, where the game keeps it hidden, so closing the menu has to
// take it back - but never while a Minecraft screen is up, because the game is
// showing its own pointer there and hiding it is the bug from complaint 1.
//
// `screenKnown` is false before the client can read the game at all (title screen,
// world load, a frame it cannot resolve). That is "unknown", never "in-world": an
// unknown frame leaves the pointer alone.
inline Want want(bool menuOpen, bool screenOpen, bool screenKnown, bool showingForMenu) {
    if (menuOpen) return Want::Shown;
    if (!screenKnown) return Want::Leave;      // cannot read the game: leave the pointer alone
    if (screenOpen) return Want::Leave;        // the game is showing its own pointer there
    if (showingForMenu) return Want::Hidden;   // in-world: take back what the menu showed
    return Want::Leave;
}

// Is a ShowCursor call needed at all? `shown` is what GetCursorInfo reports. Only a
// mismatch is worth a call: a call made "to be sure" is what walks the counted
// display counter off its boundary.
inline bool needsChange(bool shown, Want w) {
    return shown != (w == Want::Shown);
}

// How many ShowCursor calls one frame may spend getting the counter back to the
// boundary. Recovery is allowed to take a few steps (a counter left far below -1 is
// exactly complaint 1) but is bounded so a counter left somewhere absurd cannot
// stall a frame; the next frame continues where this one stopped.
constexpr int kMaxSteps = 32;

} // namespace cursor
} // namespace gui
} // namespace tyr
