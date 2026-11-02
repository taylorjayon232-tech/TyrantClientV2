// ============================================================================
//  Passive — animated interface widgets.
//
//  These replace ImGui's stock Checkbox and Combo so every module setting looks
//  and moves identically: a rounded switch whose knob slides, and a dropdown
//  whose frame, chevron and popup animate instead of snapping open. The popup
//  is deliberately a lighter panel with an accent border, because the report
//  was that its background matched the panel so closely it was hard to see
//  where the list began.
//
//  Both keep ImGui's signatures and behaviour, so a call site only has to swap
//  `ImGui::Checkbox(` for `ui::Checkbox(` (and `ImGui::BeginCombo` /
//  `ImGui::EndCombo` for `ui::BeginCombo` / `ui::EndCombo`). Every colour they
//  draw is multiplied by ImGui's style alpha, so the interface's existing
//  fade-out still fades them.
// ============================================================================
#pragma once
#include <imgui.h>

namespace tyr {
namespace gui {
namespace ui {

// Animated switch. Drop-in for ImGui::Checkbox: returns true on the frame the
// value was flipped, and the whole row (switch + label) is the hit box.
bool Checkbox(const char* label, bool* v);

// Animated dropdown frame + popup. Drop-in for ImGui::BeginCombo /
// ImGui::EndCombo: while the popup is open the caller draws its own rows with
// ImGui::Selectable, exactly as it did before.
bool BeginCombo(const char* label, const char* previewValue, ImGuiComboFlags flags = 0);
void EndCombo();

// Ease factor for one frame at `dt` seconds: how far an animated value moves
// towards its target this frame (frame-rate independent, never overshoots).
float animationStep(float dt);

// Forget every animation value. Called when the menu closes so the next open
// starts from a settled state instead of replaying a half-finished slide.
void resetAnimations();

} // namespace ui
} // namespace gui
} // namespace tyr
