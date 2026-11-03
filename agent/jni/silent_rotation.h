#pragma once
#include <jni.h>

namespace tyr::jni {
bool silentRotationStart(float yaw, float pitch);
void silentRotationUpdate(float yaw, float pitch);
// Stop substituting the aim in the game's movement packets without ending the session: the
// transform stays installed, the server is handed the player's real view, and the next
// silentRotationUpdate resumes the aim. Used while the player performs a world action (block place
// / break / item use), which must be validated against the camera, not against the silent aim.
void silentRotationSuspend();
void silentRotationStop();
void silentRotationShutdown();

// True when the aim is wired into the game's own crosshair pick (the raycast behind
// ClientPlayerEntity.getCrosshairTarget): the HUD attack indicator, the block outline and vanilla's
// own doAttack/doItemUse all resolve against the silent rotation, and the camera is untouched.
bool silentAimPickActive();
// The game's own camera pick for the current frame as a new local ref (caller deletes), or null.
// Only meaningful while silentAimPickActive(): `crosshairTarget` then holds the aim's pick.
jobject silentAimCameraPick();
}
