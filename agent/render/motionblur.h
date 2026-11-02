// ============================================================================
//  Passive — Motion Blur, mirroring the Natural Motion Blur mod natively.
//
//  The mod (modrinth.com/mod/natural-motion-blur) blends frames together instead
//  of adding a camera-only smear, and exposes: a blur method (velocity based,
//  frame blending, hybrid), blur strength, refresh-rate scaling, an accumulation
//  count and mix, and a uniform vs weighted blur profile. This renderer offers
//  the same knobs, done in one GPU pass per frame over the frame the game just
//  drew, so it costs the frame rate nothing that the mod itself would not.
//
//  The history is a half-resolution ring of frames. Every frame the backbuffer is
//  scaled into the newest slot and the older slots are drawn back over the frame -
//  offset along the direction the camera turned for velocity based, straight
//  blended for frame blending, both for hybrid - so standing still draws nothing
//  and a fast turn smears exactly as far as the settings ask.
// ============================================================================
#pragma once

namespace tyr {
namespace motionblur {

struct Params {
    int   method = 0;             // 0 = velocity, 1 = frame blending, 2 = hybrid
    float strength = 1.f;         // 0.1 - 3, the mod's own default is 1
    int   accumulationMax = 3;    // 1 - 8 earlier frames folded into the trail
    float accumulationMix = 0.6f; // 0 - 1 how strongly the accumulated frames are mixed back
    bool  weighted = true;        // true = weighted (decaying) profile, false = uniform
    bool  refreshScaling = true;  // scale by frame rate so the trail is the same at 60 and 240 fps
    float sensitivity = 1.f;      // 0.2 - 3, how strongly the camera turn feeds the trail
    float turnYaw = 0.f;          // this frame's camera turn, in degrees
    float turnPitch = 0.f;
    float deltaTime = 0.016f;     // seconds since the last frame (refresh scaling + FPS estimate)
};

// Fold the frame the game just rendered into the history ring and add the older frames to the
// overlay's background draw list according to `p`. Safe to call every frame while the module is on.
void compose(const Params& p);

// Drop the history textures (module off, world unloaded, or process shutdown). Cheap to call every
// frame: it does nothing when nothing is allocated.
void release();

bool ready();

} // namespace motionblur
} // namespace tyr
