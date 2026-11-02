// ============================================================================
//  Passive — GUI theme: glassmorphism style, accent system, Inter fonts.
// ============================================================================
#pragma once
#include <imgui.h>

namespace tyr {
namespace gui {

// ---- accent system ---------------------------------------------------------
// Single HSV accent drives the whole UI (default electric purple).
void  setAccentHue(float hue01, float sat = 0.72f, float val = 0.95f);
void  setAccentColor(const ImVec4& color);
void  tickTheme(float dt);
ImVec4 accentColor();                 // RGBA 0..1
ImU32  accentColorU32(float alpha = 1.f);
ImU32  accentFaded(float a0, float a1); // vertical gradient pair start

// ---- style -----------------------------------------------------------------
void applyTheme();     // fonts + style (call once after ImGui::CreateContext)
void styleGlass();     // pushable glass style values (roundings, padding...)

// Rounded "glass panel" push: background + border + optional inner glow.
void pushGlassStyle();
void popGlassStyle();

// ---- fonts -----------------------------------------------------------------
bool loadFonts();      // Inter Regular/Medium/SemiBold from assets/fonts
ImFont* fontRegular();
ImFont* fontMedium();
ImFont* fontSemibold();

// ---- helpers ---------------------------------------------------------------
ImU32  col(float r, float g, float b, float a);
void   drawGlowRect(ImDrawList* dl, ImVec2 p0, ImVec2 p1, ImU32 col, float rounding, float thickness);
void   drawAccentUnderline(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float h);

} // namespace gui
} // namespace tyr
