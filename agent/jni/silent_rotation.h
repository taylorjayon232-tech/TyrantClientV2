#pragma once

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
}
