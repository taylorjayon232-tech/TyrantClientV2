// ============================================================================
//  Passive — animated widget implementations.
// ============================================================================
#include "anim.h"
#include "theme.h"

#include <imgui_internal.h>   // ImLerp (ImU32 overload)

namespace tyr {
namespace gui {

bool drawToggleAt(ImDrawList* dl, ImVec2 pos, float width, float height, float t) {
    const ImU32 offCol = IM_COL32(58, 60, 78, 210);
    const ImU32 onCol  = accentColorU32(0.95f);
    const ImU32 bg = ImLerp(offCol, onCol, t);

    dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), bg, height * 0.5f);
    if (t > 0.02f)
        dl->AddRect(ImVec2(pos.x - 1.5f, pos.y - 1.5f),
                    ImVec2(pos.x + width + 1.5f, pos.y + height + 1.5f),
                    accentColorU32(0.30f * t), height * 0.5f + 1.5f, 0, 2.f);

    const float knobR = height * 0.5f - 2.5f;
    const float cx = pos.x + 2.5f + knobR + t * (width - 5.f - knobR * 2.f);
    const float cy = pos.y + height * 0.5f;
    dl->AddCircleFilled(ImVec2(cx, cy), knobR, IM_COL32(245, 246, 250, 255));
    return false; // pure draw helper; hit-testing done by caller
}

bool Toggle(const char* id, bool* v, float width, float height) {
    ImGui::PushID(id);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const bool clicked = ImGui::InvisibleButton(id, ImVec2(width, height));
    if (clicked) *v = !*v;

    // anim value stored per-ID via ImGui state storage
    ImGuiStorage* st = ImGui::GetStateStorage();
    const ImGuiID key = ImGui::GetID("anim");
    float t = st->GetFloat(key, *v ? 1.f : 0.f);
    t = ease(t, *v ? 1.f : 0.f, 16.f, ImGui::GetIO().DeltaTime);
    st->SetFloat(key, t);

    drawToggleAt(dl, pos, width, height, t);
    ImGui::PopID();
    return clicked;
}

bool PillButton(const char* label, const ImVec2& size, bool accentOn) {
    ImGui::PushID(label);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool clicked = ImGui::InvisibleButton(label, size);
    const bool hovered = ImGui::IsItemHovered();

    const float rounding = size.y * 0.5f;
    if (accentOn) {
        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), accentColorU32(0.85f), rounding);
        if (hovered) drawGlowRect(dl, pos, ImVec2(pos.x + size.x, pos.y + size.y), accentColorU32(0.6f), rounding, 1.5f);
        dl->AddText(ImVec2(pos.x + (size.x - ImGui::CalcTextSize(label).x) * 0.5f,
                           pos.y + (size.y - ImGui::CalcTextSize(label).y) * 0.5f),
                    IM_COL32(16, 14, 26, 255), label);
    } else {
        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                          hovered ? accentColorU32(0.28f) : IM_COL32(52, 54, 74, 200), rounding);
        dl->AddText(ImVec2(pos.x + (size.x - ImGui::CalcTextSize(label).x) * 0.5f,
                           pos.y + (size.y - ImGui::CalcTextSize(label).y) * 0.5f),
                    IM_COL32(232, 234, 242, 255), label);
    }
    ImGui::PopID();
    return clicked;
}

void HoverGlow(float strength) {
    if (!ImGui::IsItemHovered()) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();
    dl->AddRect(p0, p1, accentColorU32(0.35f * strength), ImGui::GetStyle().FrameRounding, 0, 1.6f);
}

} // namespace gui
} // namespace tyr
