// ============================================================================
//  Passive — Xbow Macro stage timing rules.
// ----------------------------------------------------------------------------
//  Pure, no JNI and no ImGui, so test/xbow_timing_check.cpp exercises exactly
//  what the macro runs.
//
//  Two rules, and only one of them may add time the user did not ask for:
//
//   * The stage delay is the slider value. A rail delay of 0 ms means the rail
//     placement goes out on the next frame - nothing is added to it. (This used
//     to have a 50 ms floor, which made every slider below 50 ms behave
//     identically and the whole sequence feel slow.)
//
//   * An interaction that follows a hotbar swap waits for the swap's own tick.
//     That is the game's own floor, not a tuning choice: a player's held item
//     comes from the last update-selected-slot packet the server has processed,
//     so a placement that goes out in the same tick as the swap is validated
//     against the item that was still in hand. The server then rejects it and
//     the block the client already shows - above all the flame - is corrected
//     away a moment later.
// ============================================================================
#pragma once

#include <algorithm>

namespace tyr {
namespace modules {
namespace xbow {

// One game tick (20 TPS): how long the server needs to have seen a hotbar swap before it
// will accept an interaction that uses the item that was swapped in.
constexpr double kSwapSettleDelay = 0.05;

// Written out rather than std::clamp: this header is included by the harness on its own, and
// that compiles at the toolchain's default language level, where std::clamp does not exist.
inline int clampUserDelayMs(int userDelayMs) {
    if (userDelayMs < 0) return 0;
    if (userDelayMs > 600) return 600;
    return userDelayMs;
}

// The delay a stage really uses: exactly the slider value, in seconds. 0 ms is a real 0, so the
// stage acts on the very next frame instead of waiting out a floor of its own.
inline double stageDelaySeconds(int userDelayMs) {
    return clampUserDelayMs(userDelayMs) / 1000.0;
}

// The epsilon only absorbs the rounding noise of `now - stageStartedAt` (the game clock is a
// large double, and a decimal delay is not exact in binary), so a stage that is exactly at its
// delay counts as due instead of waiting an extra frame over a 1e-16 error. It cannot make a
// stage fire early in any way that matters: it is a nanosecond.
inline bool delayElapsed(double now, double stageStartedAt, int userDelayMs) {
    return (now - stageStartedAt) + 1e-9 >= stageDelaySeconds(userDelayMs);
}

// True once a hotbar change has had its own tick (or nothing was ever swapped).
inline bool slotSettled(double now, double swappedAt) {
    return swappedAt < 0.0 || (now - swappedAt) + 1e-9 >= kSwapSettleDelay;
}

} // namespace xbow
} // namespace modules
} // namespace tyr
