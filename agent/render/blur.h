// ============================================================================
//  Passive — real backdrop blur behind GUI panels.
//  Copies the default framebuffer once per frame, downsamples + applies a
//  separable Gaussian, and exposes drawing of a rounded blurred panel.
// ============================================================================
#pragma once

namespace tyr {
namespace blur {

bool init();
void shutdown();

// Per-frame (before GUI render): capture current default framebuffer content.
void capture();

// Draw the blurred backdrop, rounded, into current ImGui draw list at rect.
// Uses the pre-captured texture; call inside a window draw callback or before
// drawing the window (alpha=window bg alpha).
void drawPanel(float x, float y, float w, float h, float rounding, float alpha);

bool ready();

} // namespace blur
} // namespace tyr
