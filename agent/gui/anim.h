// ============================================================================
//  Passive — animation helpers for the GUI (eases, animated toggles).
// ============================================================================
#pragma once
#include <imgui.h>
#include <cmath>
#include <algorithm>

namespace tyr {
namespace gui {

// Exponential ease toward target; frame-rate independent.
inline float ease(float current, float target, float speed, float dt) {
    const float t = 1.f - std::exp(-speed * dt);
    return current + (target - current) * t;
}

// Smoothly animated boolean (0..1).
struct AnimBool {
    float v = 0.f;
    void step(bool on, float dt, float speed = 14.f) { v = ease(v, on ? 1.f : 0.f, speed, dt); }
    operator float() const { return v; }
};

// Smoothstep 0..1 — used for open/close slide/fade curves.
inline float ease01(float t) {
    t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
    return t * t * (3.f - 2.f * t);
}

// Animated toggle pill at an explicit screen position (no layout impact).
// t: animation 0..1. Returns true when clicked.
bool drawToggleAt(ImDrawList* dl, ImVec2 pos, float width, float height, float t);

// Animated toggle switch — returns true when clicked. Draws a soft pill.
bool Toggle(const char* id, bool* v, float width = 34.f, float height = 18.f);

// Rounded "pill" button; returns true on click. accentOn: highlighted state.
bool PillButton(const char* label, const ImVec2& size, bool accentOn = false);

// Hover glow helper: call inside a window for the current item rect.
void HoverGlow(float strength = 0.5f);

} // namespace gui
} // namespace tyr
