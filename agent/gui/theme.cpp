// ============================================================================
//  Passive — theme implementation.
// ============================================================================
#include "theme.h"
#include "anim.h"

#include <imgui.h>
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace tyr {
namespace gui {

namespace {
float g_hue = 0.58f, g_sat = 0.12f, g_val = 0.90f;
ImFont* f_regular = nullptr;
ImFont* f_medium  = nullptr;
ImFont* f_semibold = nullptr;
ImVec4 g_targetAccent{ 0.80f, 0.84f, 0.90f, 1.f };
ImVec4 g_accent{ 0.80f, 0.84f, 0.90f, 1.f };
}

void setAccentHue(float hue01, float sat, float val) {
    g_hue = hue01; g_sat = sat; g_val = val;
    ImVec4 c;
    ImGui::ColorConvertHSVtoRGB(hue01, sat, val, c.x, c.y, c.z);
    g_targetAccent = ImVec4(c.x, c.y, c.z, 1.f);
}
void setAccentColor(const ImVec4& color) {
    g_targetAccent = ImVec4(std::max(0.f,std::min(1.f,color.x)),
                            std::max(0.f,std::min(1.f,color.y)),
                            std::max(0.f,std::min(1.f,color.z)),
                            std::max(0.f,std::min(1.f,color.w)));
    ImGui::ColorConvertRGBtoHSV(g_targetAccent.x,g_targetAccent.y,g_targetAccent.z,g_hue,g_sat,g_val);
}
void tickTheme(float dt) {
    const float t = 1.f - std::exp(-12.f * (dt > 0.f ? dt : 0.f));
    g_accent.x += (g_targetAccent.x - g_accent.x) * t;
    g_accent.y += (g_targetAccent.y - g_accent.y) * t;
    g_accent.z += (g_targetAccent.z - g_accent.z) * t;
}
ImVec4 accentColor() { return g_accent; }
ImU32  accentColorU32(float alpha) {
    return IM_COL32((int)(g_accent.x * 255), (int)(g_accent.y * 255),
                    (int)(g_accent.z * 255), (int)(alpha * 255));
}
ImU32 accentFaded(float a0, float a1) { (void)a0; (void)a1; return accentColorU32(0.5f); }

bool loadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    HMODULE moduleHandle = nullptr;

    wchar_t modulePath[MAX_PATH] = L"";
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&loadFonts), &moduleHandle)) {
        GetModuleFileNameW(moduleHandle, modulePath, MAX_PATH);
    }

    wchar_t moduleDir[MAX_PATH] = L"";
    if (modulePath[0]) {
        wchar_t* slash = wcsrchr(modulePath, L'\\');
        if (slash) {
            *slash = 0;
            wcsncpy_s(moduleDir, modulePath, _TRUNCATE);
        }
    }

    std::vector<std::wstring> candidates = {
        moduleDir[0] ? (std::wstring(moduleDir) + L"\\assets\\fonts\\Inter-Regular.ttf") : L"",
        L"C:\\Windows\\Fonts\\segoeui.ttf",
        L"C:\\Windows\\Fonts\\segoeuib.ttf",
    };

    std::vector<std::wstring> mediumCandidates = {
        moduleDir[0] ? (std::wstring(moduleDir) + L"\\assets\\fonts\\Inter-Medium.ttf") : L"",
        L"C:\\Windows\\Fonts\\segoeuib.ttf",
        L"C:\\Windows\\Fonts\\segoeui.ttf",
    };

    std::vector<std::wstring> semiboldCandidates = {
        moduleDir[0] ? (std::wstring(moduleDir) + L"\\assets\\fonts\\Inter-SemiBold.ttf") : L"",
        L"C:\\Windows\\Fonts\\segoeuib.ttf",
        L"C:\\Windows\\Fonts\\segoeui.ttf",
    };

    auto addIfExists = [&](const std::wstring& path, float size) -> ImFont* {
        if (path.empty()) return nullptr;

        const int count = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (count <= 1) return nullptr;

        std::string utf8(static_cast<size_t>(count), '\0');
        if (WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, utf8.data(), count, nullptr, nullptr) != count)
            return nullptr;
        utf8.resize(static_cast<size_t>(count - 1));
        return io.Fonts->AddFontFromFileTTF(utf8.c_str(), size);
    };

    for (size_t i = 0; i < candidates.size(); ++i) {
        if (ImFont* r = addIfExists(candidates[i], 15.f)) {
            f_regular = r;
            if (ImFont* m = addIfExists(mediumCandidates[i], 15.f)) f_medium = m;
            if (ImFont* s = addIfExists(semiboldCandidates[i], 15.f)) f_semibold = s;
            io.FontDefault = f_regular;
            return true;
        }
    }
    return false;
}

ImFont* fontRegular()  { return f_regular; }
ImFont* fontMedium()   { return f_medium ? f_medium : f_regular; }
ImFont* fontSemibold() { return f_semibold ? f_semibold : f_medium; }

void styleGlass() {
    ImGuiStyle& s = ImGui::GetStyle();
    // Platinum: flat planes and cut corners, deliberately unlike the rounded glass look.
    s.WindowRounding    = 6.f;
    s.ChildRounding     = 4.f;
    s.FrameRounding     = 3.f;
    s.PopupRounding     = 5.f;
    s.GrabRounding      = 2.f;
    s.TabRounding       = 3.f;
    s.ScrollbarRounding = 4.f;
    s.WindowPadding     = ImVec2(12, 12);
    s.FramePadding      = ImVec2(9, 6);
    s.ItemSpacing       = ImVec2(9, 7);
    s.ItemInnerSpacing  = ImVec2(7, 5);
    s.WindowBorderSize  = 1.f;
    s.ChildBorderSize   = 1.f;
    s.FrameBorderSize   = 0.f;
    s.WindowTitleAlign  = ImVec2(0.5f, 0.5f);
    s.ScrollbarSize     = 10.f;
    s.GrabMinSize       = 8.f;
    ImVec4* colors = s.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.93f, 0.94f, 0.96f, 1.f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.52f, 0.55f, 0.59f, 1.f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.058f, 0.063f, 0.98f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.072f, 0.075f, 0.081f, 0.95f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.072f, 0.075f, 0.081f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.62f, 0.66f, 0.72f, 0.45f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.115f, 0.120f, 0.128f, 1.f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.165f, 0.172f, 0.182f, 1.f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.205f, 0.214f, 0.225f, 1.f);
    colors[ImGuiCol_CheckMark] = accentColor();
    colors[ImGuiCol_SliderGrab] = accentColor();
    colors[ImGuiCol_SliderGrabActive] = accentColor();
    colors[ImGuiCol_Button] = ImVec4(0.145f, 0.150f, 0.160f, 1.f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.225f, 0.235f, 0.250f, 1.f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.300f, 0.315f, 0.335f, 1.f);
}

static int g_glassPushed = 0;   // count-proof: exact push count, consumed by popGlassStyle

void pushGlassStyle() {
    g_glassPushed = 20;
    ImGui::PushStyleColor(ImGuiCol_WindowBg,        ImVec4(0.050f, 0.052f, 0.056f, 0.94f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,         ImVec4(0.068f, 0.071f, 0.076f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg,         ImVec4(0.058f, 0.061f, 0.066f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border,          ImVec4(0.72f, 0.76f, 0.82f, 0.22f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,         ImVec4(0.105f, 0.110f, 0.118f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,  ImVec4(0.145f, 0.152f, 0.162f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,   ImVec4(0.180f, 0.188f, 0.200f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg,         ImVec4(0.045f, 0.047f, 0.051f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,   ImVec4(0.070f, 0.073f, 0.079f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark,       accentColor());
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,      accentColor());
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ImVec4(accentColor().x * 1.1f, accentColor().y * 1.1f, accentColor().z * 1.1f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Header,          ImVec4(accentColor().x, accentColor().y, accentColor().z, 0.16f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,   ImVec4(accentColor().x, accentColor().y, accentColor().z, 0.28f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,    ImVec4(accentColor().x, accentColor().y, accentColor().z, 0.38f));
    ImGui::PushStyleColor(ImGuiCol_Button,          ImVec4(0.125f, 0.130f, 0.140f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,   ImVec4(accentColor().x, accentColor().y, accentColor().z, 0.30f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,    ImVec4(accentColor().x, accentColor().y, accentColor().z, 0.45f));
    ImGui::PushStyleColor(ImGuiCol_Text,            ImVec4(0.94f, 0.95f, 0.97f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled,    ImVec4(0.52f, 0.55f, 0.60f, 1.f));
    styleGlass();
}

void popGlassStyle() {
    ImGui::PopStyleColor(g_glassPushed);   // pops exactly what pushGlassStyle pushed
    g_glassPushed = 0;
    // NOTE: styleGlass() mutates the style directly (no PushStyleVar), so
    // nothing to pop here — keep counts balanced.
}

ImU32 col(float r, float g, float b, float a) {
    return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(a * 255));
}

void drawGlowRect(ImDrawList* dl, ImVec2 p0, ImVec2 p1, ImU32 c, float rounding, float thickness) {
    // soft outer glow: several expanding strokes with decreasing alpha
    const float a = (float)((c >> IM_COL32_A_SHIFT) & 0xFF);
    for (int i = 1; i <= 3; ++i) {
        const float k = 1.f - (i - 1) / 3.f;
        const ImU32 cc = (c & ~IM_COL32_A_MASK) | (ImU32)((a * 0.22f * k)) << IM_COL32_A_SHIFT;
        dl->AddRect(ImVec2(p0.x - i * 1.5f, p0.y - i * 1.5f),
                    ImVec2(p1.x + i * 1.5f, p1.y + i * 1.5f), cc, rounding + i * 1.5f, 0, thickness);
    }
}

void drawAccentUnderline(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float h) {
    dl->AddRectFilled(ImVec2(p0.x, p1.y - h), p1, accentColorU32(0.95f), h);
}

void applyTheme() {
    loadFonts();
    styleGlass();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigWindowsMoveFromTitleBarOnly = false;
}

} // namespace gui
} // namespace tyr
