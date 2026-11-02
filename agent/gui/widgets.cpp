// ============================================================================
//  Passive — animated interface widgets (implementation).
//
//  Everything is drawn through the window's own draw list, so the widgets need
//  no extra textures or fonts and cost one rounded rect plus a few primitives
//  each. Animation state is keyed by ImGuiID (the label, hashed by ImGui), so
//  two settings with the same visible text on different cards still animate
//  independently, and a widget that scrolls out of view simply resumes where
//  it was.
// ============================================================================
#include "widgets.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace tyr {
namespace gui {
namespace ui {

namespace {

// The interface accent. Fixed rather than read from the theme: the switch and
// the popup border have to stay visible against the dark cards whatever the
// user's accent colour happens to be.
const ImVec4 kAccent{ 0.94f, 0.38f, 0.30f, 1.0f };
const ImVec4 kTrackOff{ 0.26f, 0.23f, 0.28f, 1.0f };
const ImVec4 kTrackHover{ 0.34f, 0.30f, 0.36f, 1.0f };
const ImVec4 kPopupBg{ 0.115f, 0.098f, 0.130f, 0.99f };

// Every animated widget value: 0 = off / closed, 1 = on / open.
std::unordered_map<ImGuiID, float> g_values;
// When each dropdown's popup was opened, so its fade-in has a start time.
std::unordered_map<ImGuiID, double> g_openedAt;

// One open dropdown: what BeginCombo pushed, so EndCombo can undo exactly that.
struct ComboFrame {
    bool  alphaPushed = false;
    int   styleVars   = 0;
    int   styleColors = 0;
};
std::vector<ComboFrame> g_frames;      // supports a combo inside another popup

ImU32 packed(const ImVec4& c, float alphaScale = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(
        ImVec4(c.x, c.y, c.z, c.w * alphaScale * ImGui::GetStyle().Alpha));
}

ImVec4 mix(const ImVec4& a, const ImVec4& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                  a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

float easeOut(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

// Moves `value` towards `target` at the shared animation rate.
void approach(float& value, float target, float step) {
    value += (target - value) * step;
    if (std::fabs(target - value) < 0.001f) value = target;
}

// A small chevron that flips between pointing down (0) and up (1).
void drawChevron(ImDrawList* dl, ImVec2 centre, float size, float flip, ImU32 col) {
    const float armY  = centre.y - size * 0.42f + flip * size * 0.84f;
    const float apexY = centre.y + size * 0.42f - flip * size * 0.84f;
    dl->AddLine(ImVec2(centre.x - size * 0.62f, armY), ImVec2(centre.x, apexY), col, 1.7f);
    dl->AddLine(ImVec2(centre.x + size * 0.62f, armY), ImVec2(centre.x, apexY), col, 1.7f);
}

} // namespace

float animationStep(float dt) {
    return 1.0f - std::exp(-16.0f * std::clamp(dt, 0.0f, 0.1f));
}

void resetAnimations() {
    g_values.clear();
    g_openedAt.clear();
    g_frames.clear();
}

bool Checkbox(const char* label, bool* v) {
    if (!v) return false;
    const ImGuiStyle& style = ImGui::GetStyle();
    const float dt = ImGui::GetIO().DeltaTime;
    const float step = animationStep(dt);

    const ImGuiID id = ImGui::GetID(label);
    float& anim = g_values[id];
    approach(anim, *v ? 1.0f : 0.0f, step);

    const float fontSize = ImGui::GetFontSize();
    const float height = std::max(fontSize * 0.95f, 17.0f);
    const float trackW = height * 1.9f;
    const float gap = std::max(fontSize * 0.55f, 7.0f);
    const float rowH = std::max(height, ImGui::GetFrameHeight());
    const float labelW = ImGui::CalcTextSize(label).x;

    ImGui::PushID(label);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    // The whole row is the hit box: clicking the label toggles the value, the
    // way ImGui's own checkbox does.
    const bool pressed = ImGui::InvisibleButton("##row", ImVec2(trackW + gap + labelW, rowH));
    const bool hovered = ImGui::IsItemHovered();
    if (pressed) *v = !*v;
    ImGui::PopID();

    const float trackTop = p0.y + (rowH - height) * 0.5f;
    const float rounding = height * 0.5f;
    const ImVec2 t0(p0.x, trackTop);
    const ImVec2 t1(p0.x + trackW, trackTop + height);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float on = easeOut(anim);
    ImVec4 track = mix(hovered ? kTrackHover : kTrackOff, kAccent, on * 0.92f);
    if (hovered) track = mix(track, ImVec4(1.f, 1.f, 1.f, 1.f), 0.06f);

    // A soft halo while the switch is on, so an enabled module reads at a glance.
    if (on > 0.01f)
        dl->AddRectFilled(ImVec2(t0.x - 2.0f, t0.y - 2.0f), ImVec2(t1.x + 2.0f, t1.y + 2.0f),
                          packed(kAccent, 0.18f * on), rounding + 2.0f);

    dl->AddRectFilled(t0, t1, packed(track), rounding);
    dl->AddRect(t0, t1, packed(hovered ? kAccent : ImVec4(0.42f, 0.38f, 0.44f, 1.f), 0.75f),
                rounding, 0, 1.0f);

    const float knobR = height * 0.5f - 2.5f;
    const float knobX = t0.x + height * 0.5f + on * (trackW - height);
    const ImVec2 knob(knobX, trackTop + height * 0.5f);
    dl->AddCircleFilled(ImVec2(knob.x, knob.y + 1.0f), knobR, packed(ImVec4(0.f, 0.f, 0.f, 0.35f)));
    dl->AddCircleFilled(knob, knobR, packed(mix(ImVec4(0.82f, 0.80f, 0.84f, 1.f),
                                                ImVec4(1.f, 0.98f, 0.97f, 1.f), on)));

    if (label && label[0]) {
        ImFont* font = ImGui::GetFont();
        const ImVec2 textPos(t1.x + gap, p0.y + (rowH - fontSize) * 0.5f);
        dl->AddText(font, fontSize, textPos,
                    ImGui::GetColorU32(ImGuiCol_Text), label);
    }
    (void)style;
    return pressed;
}

bool BeginCombo(const char* label, const char* previewValue, ImGuiComboFlags flags) {
    (void)flags;
    const ImGuiStyle& style = ImGui::GetStyle();
    const float dt = ImGui::GetIO().DeltaTime;
    const float step = animationStep(dt);
    const float alpha = style.Alpha;

    const ImGuiID id = ImGui::GetID(label);
    const float frameH = ImGui::GetFrameHeight();
    const float width = std::max(ImGui::CalcItemWidth(), frameH * 3.0f);
    const float rounding = std::max(4.0f, frameH * 0.34f);
    float& openAnim = g_values[id];

    ImGui::PushID(label);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton("##frame", ImVec2(width, frameH));
    const bool hovered = ImGui::IsItemHovered();
    if (pressed) {
        ImGui::OpenPopup("##popup");
        g_openedAt[id] = ImGui::GetTime();
    }
    // Read after opening, so the click frame is already treated as open: the
    // chevron and the popup fade both start moving on that frame instead of one
    // frame later.
    const bool isOpen = ImGui::IsPopupOpen("##popup");
    approach(openAnim, isOpen ? 1.0f : 0.0f, step);

    // ---- the closed control -------------------------------------------------
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p1(p0.x + width, p0.y + frameH);
    ImVec4 frameBg = style.Colors[ImGuiCol_FrameBg];
    if (hovered || isOpen) frameBg = mix(frameBg, ImVec4(1.f, 1.f, 1.f, 1.f), 0.07f);
    dl->AddRectFilled(p0, p1, packed(frameBg), rounding);
    dl->AddRect(p0, p1,
                packed(hovered || isOpen ? kAccent : style.Colors[ImGuiCol_Border],
                       hovered || isOpen ? 0.85f : 0.70f),
                rounding, 0, 1.0f);

    const float fontSize = ImGui::GetFontSize();
    const char* preview = (previewValue && previewValue[0]) ? previewValue : "";
    const float chevronSize = fontSize * 0.42f;
    const float textPad = frameH * 0.42f;
    dl->PushClipRect(ImVec2(p0.x + 2.0f, p0.y), ImVec2(p1.x - chevronSize * 2.6f, p1.y), true);
    dl->AddText(ImGui::GetFont(), fontSize,
                ImVec2(p0.x + textPad, p0.y + (frameH - fontSize) * 0.5f),
                ImGui::GetColorU32(ImGuiCol_Text), preview);
    dl->PopClipRect();
    drawChevron(dl, ImVec2(p1.x - textPad, p0.y + frameH * 0.5f), chevronSize,
                easeOut(openAnim), packed(kAccent, isOpen ? 1.0f : 0.75f));

    // ---- the popup ----------------------------------------------------------
    // Pushed before BeginPopup so the rounded corners, padding and border apply
    // to the window itself rather than to its contents.
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 9.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(7.0f, 7.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    constexpr int kVars = 5;

    ImGui::PushStyleColor(ImGuiCol_PopupBg, kPopupBg);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.26f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.36f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.46f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.97f, 0.94f, 0.95f, 1.f));
    constexpr int kColors = 6;

    // Fade the list in over the same 0.12 s the chevron turns. Style alpha is
    // what the interface's own close fade uses, so the two compose.
    bool alphaPushed = false;
    if (isOpen) {
        const auto at = g_openedAt.find(id);
        const float t = at == g_openedAt.end() ? 1.0f
                        : (float)((ImGui::GetTime() - at->second) / 0.12);
        const float fade = easeOut(t);
        if (fade < 0.995f) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha * fade);
            alphaPushed = true;
        }
    }

    ImGui::SetNextWindowPos(ImVec2(p0.x, p1.y + 4.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, FLT_MAX));
    if (!ImGui::BeginPopup("##popup", ImGuiWindowFlags_AlwaysAutoResize)) {
        if (alphaPushed) ImGui::PopStyleVar();
        ImGui::PopStyleColor(kColors);
        ImGui::PopStyleVar(kVars);
        ImGui::PopID();
        return false;
    }
    g_frames.push_back({ alphaPushed, kVars, kColors });
    return true;
}

void EndCombo() {
    if (g_frames.empty()) {          // defensive: keep the window stack balanced
        ImGui::EndPopup();
        return;
    }
    const ComboFrame frame = g_frames.back();
    g_frames.pop_back();
    if (frame.alphaPushed) ImGui::PopStyleVar();
    ImGui::EndPopup();
    ImGui::PopStyleColor(frame.styleColors);
    ImGui::PopStyleVar(frame.styleVars);
    ImGui::PopID();
}

} // namespace ui
} // namespace gui
} // namespace tyr
