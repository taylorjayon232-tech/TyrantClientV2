// ============================================================================
//  Passive — the placement aim rule (Camera Sensitivity).
//
//  Pure: no JNI and no ImGui, so test/placement_aim_check.cpp exercises exactly
//  the rule agent/modules/modules.cpp uses when a Smooth placement (Stun Web's
//  web, the Xbow Macro's fire) walks the camera onto the block it places on.
//
//  The slider is 0-50 and it is the *whole* range that is usable: every setting has
//  to close on the block in a fraction of a second, and the top of the slider is
//  near-instant. The old mapping topped out at 20/s, which is half a second of
//  walking before the web or the flame may go out - the delay the Smooth option was
//  reported for. The walk is still eased, it just arrives (0 is ~0.3 s, 5 ~0.15 s,
//  10 ~0.1 s and 50 a couple of frames for a 90 degree turn).
// ============================================================================
#pragma once
#include <algorithm>
#include <cmath>

namespace tyr {
namespace modules {
namespace aim {

// "Camera Sensitivity" -> how fast the view closes on the placement's rotation,
// in units of 1/s (the exponent of the exponential walk): 14/s at 0, 25/s at 5 and
// 36/s at 10. The slider is 0-50 now (it was 0-10), so the top half is a genuinely
// fast turn - 50 is 124/s, a couple of frames for a 90 degree swing - while every
// setting at or below 10 keeps exactly the pace it always had. Nothing here waits
// for the walk to finish beyond the tolerance the caller passes, so the placement
// follows the view as soon as it is on the block.
inline float approachSpeed(float sensitivity) {
    return 14.0f + std::clamp(sensitivity, 0.f, 50.f) * 2.2f;
}

// The fraction of the remaining angle one frame covers. Frame-rate independent
// (exponential), clamped the way the placement aim clamps it: a frame can never
// cover more than half the remaining turn, so a slow slider is a real walk
// rather than a jump, and a wild deltaTime cannot snap the view.
inline float approachStep(float sensitivity, float deltaTime) {
    const float dt = std::clamp(deltaTime, 0.0005f, 0.1f);
    return std::clamp(1.0f - std::exp(-approachSpeed(sensitivity) * dt), 0.f, 0.5f);
}

} // namespace aim
} // namespace modules
} // namespace tyr
