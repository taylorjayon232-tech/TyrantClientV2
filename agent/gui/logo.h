// ============================================================================
//  Passive - brand mark.
//
//  The official Passive logo (assets/passive-logo.png, shipped beside the DLL)
//  is decoded with Windows Imaging Component and uploaded once as a GL
//  texture. Every brand surface draws this texture; the old lettered monogram
//  is only a fallback for a build whose asset is missing.
//
//  Call from the render thread with the GL context current.
// ============================================================================
#pragma once
#include <imgui.h>

namespace tyr {
namespace gui {

// The logo texture, or 0 while it has not loaded (or the file is missing).
ImTextureID logoTexture();

// True once the file has been looked for. A missing logo is not retried every
// frame; the caller keeps its monogram fallback.
bool logoResolved();

// Draws the mark inside [p0,p1], tinted and faded. Returns false when there is
// no texture so the caller can draw its fallback instead.
bool drawLogo(ImDrawList* dl, ImVec2 p0, ImVec2 p1, ImU32 tint, float opacity);

} // namespace gui
} // namespace tyr
