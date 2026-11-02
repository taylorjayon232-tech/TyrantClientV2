// Passive's in-game interface.
//
// The overlay is drawn with ImGui primitives so it stays resolution-aware and
// keeps all hit targets in the same coordinate space as the rendered panel.
// The visual language is a graphite shell with platinum accents: a brand band and a horizontal
// command bar of categories, rounded surfaces, and fine outline icons.
#include "gui.h"
#include "theme.h"
#include "anim.h"
#include "logo.h"
#include "cursor_rules.h"
#include "widgets.h"
#include "../core/agent.h"
#include "../core/hooks.h"
#include "../modules/modules.h"
#include "../modules/java_modules.h"
#include "../config/profiles.h"
#include "../jni/jvm.h"
#include "../jni/mc.h"

#include <imgui.h>
#include <json.hpp>
#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <cstring>
#include <cctype>
#include <unordered_map>
#include <vector>

namespace tyr {
namespace gui {

namespace {

enum class Page : int { Combat, Mace, Misc, Movement, Spear, Visual, Crystal, Macros, Configs, HudEditor, Settings };
enum class SettingsPage : int { General, Appearance, Controls, Friends, About };

struct Hit {
    bool clicked = false;
    bool hovered = false;
    float hover = 0.f;
};

bool g_menuOpen = false;
// OS cursor state: decided once a frame by preFrame(), read by hooks::hkWndProc.
bool g_cursorWanted = false;     // the menu needs a pointer this frame
bool g_cursorClipped = false;
bool g_menuShowedCursor = false; // the menu's own ShowCursor is still outstanding
AnimBool g_menuAnim;
AnimBool g_configAnim;
float g_pageFade = 1.f;
float g_tickDelta = 0.f;

std::vector<int> g_taps;
std::vector<int> g_mouseTaps;
bool g_keys[256] = {};
bool g_mouse[7] = {};

std::vector<ToastData> g_toasts;
int g_menuKey = VK_RSHIFT;
Page g_page = Page::Visual;
SettingsPage g_settingsPage = SettingsPage::General;
// Default interface: the module library. `g_showAllModules` is the "All modules" view (every
// category in one grid); choosing a category in the sidebar narrows it. `g_gridDetail` is the
// module whose settings have taken the library over, and the filter chips are All / Enabled /
// Favorites. Favorites are a per-interface preference saved with the configuration.
bool g_showAllModules = true;
std::string g_gridDetail;
std::unordered_map<std::string, bool> g_favorites;
int g_libraryFilter = 0;                 // 0 all, 1 enabled, 2 favorites
constexpr float kSidebarW = 208.f;
bool g_configOpen = false;
bool g_captureMenuKey = false;
bool g_confirmUnload = false;      // SELF DESTRUCT armed, waiting for the second click
double g_confirmUnloadAt = 0.0;    // when it was armed (disarms itself after a few seconds)
std::string g_captureModuleBind;
bool g_animations = true;
bool g_compact = false;
bool g_softGlow = true;
bool g_closeWithEscape = true;
enum class ModuleStyle : int { Default, Rounded, Squared, Minimal, Outline };
const char* kModuleStyleNames[] = { "Default", "Rounded", "Squared", "Minimal", "Outline" };
ModuleStyle g_moduleStyle = ModuleStyle::Default;
// Module Shapes are a second, independent axis: Rectangle keeps the vertical
// list; Squared turns every module card into a square laid out in a 3-wide grid
// with a square configuration panel beside the grid.
enum class ModuleShape : int { Rectangle, Squared };
const char* kModuleShapeNames[] = { "Rectangle", "Squared" };
// Shader ESP: Minecraft's own glowing outline, or Passive's drawn glow.
const char* kShaderGlowStyleNames[] = { "Vanilla (game outline)", "Custom (drawn)" };
ModuleShape g_moduleShape = ModuleShape::Rectangle;
// Change Interface (Settings > General > Interface): the Default layout, or the Dropdown
// layout - a content area of collapsible category sections, each holding its own module rows with
// custom toggle buttons and a settings view per module, plus the draggable tab rail beside them.
// It has its own control styling and none of the Default UI's Module Style / Module Shape
// selectors, and it shares every module's state and every option with the Default interface.
enum class InterfaceStyle : int { Default, Dropdown };
const char* kInterfaceStyleNames[] = { "Default", "Dropdown" };
const char* kInterfaceStyleHelp[] = {
    "Command bar tabs, one category per page. The current interface.",
    "Collapsible category sections: unfold one to reach its modules, then open a module's settings."
};
InterfaceStyle g_interfaceStyle = InterfaceStyle::Default;

// The eight category pages in one table, so the command bar and the Dropdown layout's draggable
// tabs can never drift apart.
constexpr int kCategoryCount = 8;
const char* const kCategoryNames[kCategoryCount] = { "Combat", "Mace", "Misc", "Movement",
                                                     "Spear", "Visual", "Crystal", "Macros" };

// ---- HUD Editor ---------------------------------------------------------------------------
// Its own feature, separate from the HUD module: small screen elements the player can place
// anywhere. The list is deliberately data-driven so the next update can add ping, pots, armour
// and the rest without touching the editor again.
struct HudElement {
    const char* id;
    const char* label;
    ImVec2 pos;        // normalised top-left inside the screen (0..1)
    bool enabled;
};
HudElement g_hudElements[] = {
    { "fps",         "FPS",         ImVec2(0.015f, 0.030f), true },
    { "armor",       "Armor",       ImVec2(0.015f, 0.100f), true },
    { "coordinates", "Coordinates", ImVec2(0.015f, 0.170f), true },
    { "keystrokes",  "Keystrokes",  ImVec2(0.860f, 0.820f), true },
};
constexpr int kHudElementCount = (int)(sizeof(g_hudElements) / sizeof(g_hudElements[0]));
int g_hudDrag = -1;
bool g_hudEditorEnabled = true;
const Page kCategoryPages[kCategoryCount] = { Page::Combat, Page::Mace, Page::Misc, Page::Movement,
                                             Page::Spear, Page::Visual, Page::Crystal, Page::Macros };
const int kCategoryIcons[kCategoryCount] = { 6, 7, 8, 9, 10, 11, 12, 13 };

// Dropdown layout state: the order the category tabs sit in, the tab being dragged right now,
// and whether the Dropdown card style is in use.
std::vector<int> g_dropdownOrder;
int g_dropdownDrag = -1;
bool g_dropdownDragMoved = false;
// Drag-to-reorder state for the Dropdown layout: the top tab bar and the six module columns, each
// with the order its items sit in. The grabbed item keeps its slot index while it is carried.
struct DragReorder { int slot = -1; bool moved = false; float startX = 0.f; float grabOffset = 0.f; };
DragReorder g_topTabDrag, g_columnDrag;
std::vector<int> g_topTabOrder = { 0, 1, 2, 3, 4 };
std::vector<int> g_columnOrder = { 0, 1, 2, 3, 4, 5 };
// Where the open module's box sits: the anchor column's left and right edges and the row's top, in
// content coordinates. A negative left edge means the box is centred (opened from a search result).
float g_boxLeft = -1.f, g_boxRight = 0.f, g_boxTop = 0.f;
constexpr float kDropdownBoxW = 360.f;
constexpr float kDropdownBoxH = 420.f;
bool g_dropdownCards = false;
// True while a module inspector draws the settings surface. It hides the old
// library card header so the new configuration page owns the whole panel.
bool g_inspectorCards = false;
// The Dropdown interface's own presentation state: `g_dropdownConfig*` is the module whose
// settings view has taken the content area over, and `g_dropdownToggleAnim` is the animated fill
// of each module's toggle button, keyed by module name.
constexpr float kDropdownTabH   = 34.f;  // a category tab's height
constexpr float kDropdownRowH   = 34.f;  // a module row's height
constexpr float kDropdownRowGap = 6.f;   // the gap between two module rows
std::string g_dropdownConfigName;
int g_dropdownConfigCat = -1;
bool g_dropdownConfigOpen = false;
float g_dropdownConfigAnim = 0.f;
std::unordered_map<std::string,float> g_dropdownToggleAnim;
bool g_styleCards = false;   // true only while module-tab cards are being drawn
float g_uiScale = 1.f;
float g_panelOpacity = 0.96f;
int g_accentIndex = 0;
// The injection splash: set when the interface comes up, cleared when the animation has finished.
double g_splashStart = -1.0;
constexpr double kSplashSeconds = 4.4;
char g_searchText[96] = {};
char g_profileName[64] = {};
char g_friendName[64] = {};
char g_cleanItemName[64] = {};
std::string g_selectedProfile;
std::unordered_map<std::string, float> g_moduleVisibility;
std::unordered_map<std::string, float> g_modulePositions;
std::unordered_map<std::string, bool> g_moduleExpanded;
std::unordered_map<std::string, float> g_cfgPanelAnim;  // Squared: attached config panel open animation
std::unordered_map<std::string, float> g_cfgScroll;     // Squared: attached config panel scroll (px)
float g_appearanceScroll = 0.f;                          // Settings > Appearance: column scroll (px)

constexpr float kBaseWidth = 950.f;
constexpr float kBaseHeight = 600.f;

ImU32 kInk = IM_COL32(238, 241, 246, 255);
ImU32 kMuted = IM_COL32(163, 169, 181, 255);
ImU32 kDim = IM_COL32(111, 117, 128, 255);
ImU32 kPanel = IM_COL32(22, 24, 28, 255);
ImU32 kSidebar = IM_COL32(18, 20, 24, 255);
ImU32 kCard = IM_COL32(30, 33, 38, 255);
ImU32 kCardLight = IM_COL32(38, 42, 49, 255);
ImU32 kLine = IM_COL32(72, 78, 90, 255);
// Sits on an accent fill: the accent is a light platinum, so labels on it have to be dark.
const ImU32 kOnAccent = IM_COL32(16, 18, 22, 255);

struct AccentPreset {
    const char* name;
    float hue;
    float saturation;
    float value;
};
constexpr AccentPreset kAccentPresets[] = {
    { "Coral", 0.030f, 0.68f, 0.94f },
    { "Amber", 0.095f, 0.74f, 0.96f },
    { "Violet", 0.755f, 0.58f, 0.94f },
    { "Azure", 0.555f, 0.62f, 0.92f },
    { "Mint", 0.405f, 0.55f, 0.88f },
};

const ImVec4 kDefaultThemeColors[] = {
    ImVec4(0.086f,0.094f,0.110f,1.f), ImVec4(0.071f,0.078f,0.094f,1.f),
    ImVec4(0.118f,0.129f,0.149f,1.f), ImVec4(0.149f,0.165f,0.192f,1.f),
    ImVec4(0.282f,0.306f,0.353f,1.f), ImVec4(0.933f,0.945f,0.965f,1.f),
    ImVec4(0.639f,0.663f,0.710f,1.f), ImVec4(0.800f,0.840f,0.900f,1.f)
};
ImVec4 g_themeColors[8] = {
    kDefaultThemeColors[0],kDefaultThemeColors[1],kDefaultThemeColors[2],kDefaultThemeColors[3],
    kDefaultThemeColors[4],kDefaultThemeColors[5],kDefaultThemeColors[6],kDefaultThemeColors[7]
};
int g_selectedThemeColor = 7;

const char* kThemeColorNames[] = { "Background", "Sidebar", "Cards", "Raised cards", "Borders", "Primary text", "Secondary text", "Accent" };

ImU32 packThemeColor(ImVec4 c) {
    return IM_COL32((int)(std::clamp(c.x,0.f,1.f)*255.f),(int)(std::clamp(c.y,0.f,1.f)*255.f),
                    (int)(std::clamp(c.z,0.f,1.f)*255.f),(int)(std::clamp(c.w,0.f,1.f)*255.f));
}

ImVec4 unpackThemeColor(ImU32 c) {
    return ImVec4(((c >> IM_COL32_R_SHIFT) & 0xffu)/255.f,
                  ((c >> IM_COL32_G_SHIFT) & 0xffu)/255.f,
                  ((c >> IM_COL32_B_SHIFT) & 0xffu)/255.f,
                  ((c >> IM_COL32_A_SHIFT) & 0xffu)/255.f);
}

void applyThemeColors() {
    kPanel = packThemeColor(g_themeColors[0]); kSidebar = packThemeColor(g_themeColors[1]);
    kCard = packThemeColor(g_themeColors[2]); kCardLight = packThemeColor(g_themeColors[3]);
    kLine = packThemeColor(g_themeColors[4]); kInk = packThemeColor(g_themeColors[5]);
    kMuted = packThemeColor(g_themeColors[6]);
    kDim = IM_COL32((int)(g_themeColors[6].x*180.f),(int)(g_themeColors[6].y*180.f),
                    (int)(g_themeColors[6].z*180.f),(int)(g_themeColors[6].w*255.f));
    setAccentColor(g_themeColors[7]);
}

void resetAppearance() {
    for (int i=0;i<8;++i) g_themeColors[i]=kDefaultThemeColors[i];
    g_selectedThemeColor=7; g_accentIndex=0; applyThemeColors();
    g_moduleStyle=ModuleStyle::Default; g_moduleShape=ModuleShape::Rectangle;
    g_interfaceStyle=InterfaceStyle::Default;
    g_uiScale=1.f; g_panelOpacity=.96f; g_animations=true; g_compact=false; g_softGlow=true;
}

float clamp01(float value) {
    return std::max(0.f, std::min(1.f, value));
}

// ---- Theme presets ---------------------------------------------------------------------------
// One click repaints every slot of the palette at once. Each preset is a full set of the eight
// theme colors (Background, Sidebar, Cards, Raised cards, Borders, Primary text, Secondary text,
// Accent); Smooth fade background eases the interface from the old set to the new one instead of
// cutting to it.
const char* const kThemePresetNames[8] = {
    "Red Ruby", "Platinum", "Emerald", "Sapphire", "Amethyst", "Golden", "Obsidian", "Rose"
};
const char* const kThemePresetIds[8] = {
    "theme-ruby", "theme-platinum", "theme-emerald", "theme-sapphire",
    "theme-amethyst", "theme-golden", "theme-obsidian", "theme-rose"
};
const ImVec4 kThemePresetColors[8][8] = {
    { // Red Ruby
        ImVec4(0.110f,0.062f,0.070f,1.f), ImVec4(0.094f,0.052f,0.060f,1.f),
        ImVec4(0.152f,0.082f,0.092f,1.f), ImVec4(0.190f,0.100f,0.112f,1.f),
        ImVec4(0.360f,0.160f,0.180f,1.f), ImVec4(0.980f,0.930f,0.930f,1.f),
        ImVec4(0.740f,0.600f,0.620f,1.f), ImVec4(0.930f,0.200f,0.270f,1.f) },
    { // Platinum
        ImVec4(0.860f,0.870f,0.890f,1.f), ImVec4(0.800f,0.812f,0.836f,1.f),
        ImVec4(0.922f,0.930f,0.941f,1.f), ImVec4(0.968f,0.972f,0.980f,1.f),
        ImVec4(0.700f,0.716f,0.748f,1.f), ImVec4(0.160f,0.172f,0.200f,1.f),
        ImVec4(0.420f,0.440f,0.480f,1.f), ImVec4(0.450f,0.482f,0.552f,1.f) },
    { // Emerald
        ImVec4(0.062f,0.106f,0.086f,1.f), ImVec4(0.052f,0.090f,0.074f,1.f),
        ImVec4(0.086f,0.145f,0.117f,1.f), ImVec4(0.106f,0.176f,0.140f,1.f),
        ImVec4(0.180f,0.330f,0.260f,1.f), ImVec4(0.930f,0.980f,0.955f,1.f),
        ImVec4(0.600f,0.740f,0.668f,1.f), ImVec4(0.200f,0.850f,0.550f,1.f) },
    { // Sapphire
        ImVec4(0.062f,0.078f,0.118f,1.f), ImVec4(0.052f,0.066f,0.102f,1.f),
        ImVec4(0.086f,0.110f,0.164f,1.f), ImVec4(0.108f,0.138f,0.200f,1.f),
        ImVec4(0.180f,0.240f,0.380f,1.f), ImVec4(0.930f,0.955f,0.990f,1.f),
        ImVec4(0.610f,0.668f,0.780f,1.f), ImVec4(0.250f,0.550f,0.980f,1.f) },
    { // Amethyst (the default palette family)
        ImVec4(0.102f,0.086f,0.114f,1.f), ImVec4(0.090f,0.075f,0.102f,1.f),
        ImVec4(0.133f,0.114f,0.145f,1.f), ImVec4(0.157f,0.129f,0.165f,1.f),
        ImVec4(0.294f,0.243f,0.290f,1.f), ImVec4(0.961f,0.941f,0.941f,1.f),
        ImVec4(0.678f,0.643f,0.678f,1.f), ImVec4(0.720f,0.450f,0.980f,1.f) },
    { // Golden
        ImVec4(0.114f,0.096f,0.062f,1.f), ImVec4(0.098f,0.082f,0.052f,1.f),
        ImVec4(0.160f,0.134f,0.086f,1.f), ImVec4(0.196f,0.164f,0.104f,1.f),
        ImVec4(0.380f,0.310f,0.170f,1.f), ImVec4(0.990f,0.968f,0.912f,1.f),
        ImVec4(0.780f,0.716f,0.576f,1.f), ImVec4(0.980f,0.760f,0.250f,1.f) },
    { // Obsidian
        ImVec4(0.052f,0.052f,0.072f,1.f), ImVec4(0.044f,0.044f,0.062f,1.f),
        ImVec4(0.074f,0.074f,0.100f,1.f), ImVec4(0.094f,0.094f,0.124f,1.f),
        ImVec4(0.190f,0.190f,0.260f,1.f), ImVec4(0.940f,0.940f,0.985f,1.f),
        ImVec4(0.620f,0.620f,0.720f,1.f), ImVec4(0.600f,0.550f,0.950f,1.f) },
    { // Rose
        ImVec4(0.114f,0.076f,0.094f,1.f), ImVec4(0.098f,0.064f,0.082f,1.f),
        ImVec4(0.160f,0.106f,0.130f,1.f), ImVec4(0.196f,0.130f,0.158f,1.f),
        ImVec4(0.380f,0.220f,0.300f,1.f), ImVec4(0.990f,0.945f,0.960f,1.f),
        ImVec4(0.780f,0.640f,0.706f,1.f), ImVec4(0.980f,0.450f,0.650f,1.f) }
};
int g_themePreset = -1;                       // -1 = a slot was tuned by hand
bool g_themeSmoothFade = false;
// Fade Color background: the main panel slowly breathes between the theme's background colour
// and a soft accent tint of it, so the surface never sits perfectly still.
bool g_fadeColorBackground = false;
ImVec4 g_themeFadeFrom[8], g_themeFadeTo[8];
float g_themeFade = 1.f;                      // 0..1 progress of the current preset fade

void applyThemePreset(int index) {
    if (index < 0 || index >= 8) return;
    g_themePreset = index;
    for (int i = 0; i < 8; ++i) {
        g_themeFadeFrom[i] = g_themeColors[i];
        g_themeFadeTo[i] = kThemePresetColors[index][i];
    }
    if (g_themeSmoothFade && g_animations) {
        g_themeFade = 0.f;                     // tickThemeFade walks the colors over
        return;
    }
    for (int i = 0; i < 8; ++i) g_themeColors[i] = g_themeFadeTo[i];
    g_themeFade = 1.f;
    applyThemeColors();
}

// Smooth fade background: the interface eases from the old palette to the new one over a little
// more than half a second, repainting through applyThemeColors each frame.
void tickThemeFade(float dt) {
    if (g_themeFade >= 1.f || dt <= 0.f) return;
    g_themeFade = clamp01(g_themeFade + dt / 0.65f);
    const float t = g_themeFade * g_themeFade * (3.f - 2.f * g_themeFade);   // smoothstep
    for (int i = 0; i < 8; ++i) {
        g_themeColors[i].x = g_themeFadeFrom[i].x + (g_themeFadeTo[i].x - g_themeFadeFrom[i].x) * t;
        g_themeColors[i].y = g_themeFadeFrom[i].y + (g_themeFadeTo[i].y - g_themeFadeFrom[i].y) * t;
        g_themeColors[i].z = g_themeFadeFrom[i].z + (g_themeFadeTo[i].z - g_themeFadeFrom[i].z) * t;
        g_themeColors[i].w = g_themeFadeFrom[i].w + (g_themeFadeTo[i].w - g_themeFadeFrom[i].w) * t;
    }
    applyThemeColors();
}

float clampf(float value, float low, float high) {
    return std::max(low, std::min(high, value));
}

ImU32 alpha(ImU32 color, float amount) {
    const unsigned oldA = (color >> IM_COL32_A_SHIFT) & 0xFFu;
    const unsigned newA = (unsigned)clampf((float)oldA * clamp01(amount), 0.f, 255.f);
    return (color & ~IM_COL32_A_MASK) | (newA << IM_COL32_A_SHIFT);
}

ImU32 blend(ImU32 a, ImU32 b, float amount) {
    amount = clamp01(amount);
    const auto channel = [amount](ImU32 x, ImU32 y, int shift) -> ImU32 {
        const float xf = (float)((x >> shift) & 0xFFu);
        const float yf = (float)((y >> shift) & 0xFFu);
        return (ImU32)(xf + (yf - xf) * amount) << shift;
    };
    return channel(a, b, IM_COL32_R_SHIFT) |
           channel(a, b, IM_COL32_G_SHIFT) |
           channel(a, b, IM_COL32_B_SHIFT) |
           channel(a, b, IM_COL32_A_SHIFT);
}

ImVec2 screenPos(ImVec2 origin, float x, float y, float scale) {
    return ImVec2(origin.x + x * scale, origin.y + y * scale);
}

ImFont* textFont(bool strong) {
    return strong ? fontSemibold() : fontRegular();
}

void drawText(ImDrawList* dl, ImVec2 origin, float x, float y, const char* value,
              ImU32 color, float logicalSize, float scale, float opacity,
              bool strong = false) {
    if (!value || !*value) return;
    ImFont* font = textFont(strong);
    const float size = logicalSize * scale;
    const ImVec2 pos = screenPos(origin, x, y, scale);
    const ImU32 tint = alpha(color, opacity);
    if (font) dl->AddText(font, size, pos, tint, value);
    else dl->AddText(pos, tint, value);
}

float measureText(const char* value, float logicalSize, float scale, bool strong = false) {
    ImFont* font = textFont(strong);
    if (!font || !value) return ImGui::CalcTextSize(value ? value : "").x * scale;
    return font->CalcTextSizeA(logicalSize * scale, FLT_MAX, 0.f, value).x;
}

// The official Passive mark. The lettered tile remains as the fallback for a
// build whose assets/passive-logo.png is missing, so no brand surface can end
// up blank.
void drawBrandMark(ImDrawList* dl, ImVec2 origin, float x, float y, float size,
                   float scale, float opacity) {
    const ImVec2 p0 = screenPos(origin, x, y, scale);
    const ImVec2 p1 = screenPos(origin, x + size, y + size, scale);
    if (drawLogo(dl, p0, p1, IM_COL32(248, 250, 255, 255), opacity)) return;
    dl->AddRectFilled(p0, p1, alpha(accentColorU32(), opacity), 7.f * scale);
    drawText(dl, origin, x + size * 0.28f, y + size * 0.14f, "P", kOnAccent,
             size * 0.52f, scale, opacity, true);
}

void drawCard(ImDrawList* dl, ImVec2 origin, float x, float y, float width, float height,
              float scale, float opacity, bool accentEdge = false) {
    const ImVec2 p0 = screenPos(origin, x, y, scale);
    const ImVec2 p1 = screenPos(origin, x + width, y + height, scale);
    float rounding = 14.f * scale;
    ImU32 fill = alpha(kCard, opacity);
    ImU32 border = alpha(kLine, opacity * 0.58f);
    bool drawBorder = true;
    if (g_dropdownCards) {
        // The Dropdown interface draws its own card: flat, square and quieter than the Default
        // one, with a short accent edge so a section still reads as a unit.
        rounding = 3.f * scale;
        fill = alpha(kCard, opacity * 0.86f);
        border = alpha(kLine, opacity * 0.45f);
        accentEdge = true;
    } else if (g_styleCards) {
        switch (g_moduleStyle) {
            case ModuleStyle::Rounded:  rounding = 26.f * scale; break;
            case ModuleStyle::Squared:  rounding = 2.f * scale; break;
            case ModuleStyle::Minimal:  rounding = 6.f * scale; drawBorder = false;
                                        fill = alpha(kCard, opacity * 0.72f); accentEdge = false; break;
            case ModuleStyle::Outline:  rounding = 10.f * scale;
                                        fill = alpha(kCard, opacity * 0.22f);
                                        border = alpha(accentColorU32(0.75f), opacity); break;
            default: break;
        }
    }
    dl->AddRectFilled(p0, p1, fill, rounding);
    if (drawBorder) dl->AddRect(p0, p1, border, rounding, 0, 1.f * scale);
    if (accentEdge) {
        const ImVec2 a0 = screenPos(origin, x + 18.f, y, scale);
        const ImVec2 a1 = screenPos(origin, x + width - 18.f, y + 2.f, scale);
        dl->AddRectFilled(a0, a1, alpha(accentColorU32(0.72f), opacity), 1.f * scale);
    }
}

Hit hitBox(const char* id, float x, float y, float width, float height,
           float scale, float speed = 16.f) {
    Hit result;
    ImGui::PushID(id);
    ImGui::SetCursorPos(ImVec2(x * scale, y * scale));
    result.clicked = ImGui::InvisibleButton("##hit", ImVec2(width * scale, height * scale));
    result.hovered = ImGui::IsItemHovered();
    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID hoverKey = ImGui::GetID("hover");
    float value = storage->GetFloat(hoverKey, result.hovered ? 1.f : 0.f);
    value = g_animations ? ease(value, result.hovered ? 1.f : 0.f, speed, ImGui::GetIO().DeltaTime)
                         : (result.hovered ? 1.f : 0.f);
    storage->SetFloat(hoverKey, value);
    result.hover = value;
    ImGui::PopID();
    return result;
}

bool drawButton(ImDrawList* dl, ImVec2 origin, const char* id, const char* label,
                float x, float y, float width, float height, float scale, float opacity,
                bool primary = false, bool compact = false) {
    const Hit hit = hitBox(id, x, y, width, height, scale);
    const ImVec2 p0 = screenPos(origin, x, y, scale);
    const ImVec2 p1 = screenPos(origin, x + width, y + height, scale);
    const ImU32 idle = primary ? accentColorU32(0.92f) : kCardLight;
    const ImU32 over = primary ? IM_COL32(226, 232, 242, 255) : IM_COL32(52, 58, 68, 255);
    const ImU32 fill = alpha(blend(idle, over, hit.hover * 0.78f), opacity);
    const float rounding = compact ? 9.f * scale : height * scale * 0.5f;
    dl->AddRectFilled(p0, p1, fill, rounding);
    if (!primary)
        dl->AddRect(p0, p1, alpha(blend(kLine, accentColorU32(), hit.hover * 0.38f), opacity * 0.7f),
                    rounding, 0, 1.f * scale);
    if (g_softGlow && hit.hover > 0.05f)
        drawGlowRect(dl, p0, p1, alpha(accentColorU32(0.46f * hit.hover), opacity),
                     rounding, 1.f * scale);
    if (label) {
        const float tw = measureText(label, 12.f, scale, true);
        drawText(dl, origin, x + (width - tw / scale) * 0.5f,
                 y + (height - 12.f) * 0.5f, label,
                 primary ? kOnAccent : kInk,
                 12.f, scale, opacity, true);
    }
    return hit.clicked;
}

void drawIcon(ImDrawList* dl, ImVec2 center, int type, ImU32 color, float scale) {
    const float s = scale;
    const auto pt = [center, s](float x, float y) {
        return ImVec2(center.x + x * s, center.y + y * s);
    };
    const auto line = [dl, color, s, &pt](float x0, float y0, float x1, float y1) {
        dl->AddLine(pt(x0, y0), pt(x1, y1), color, 1.7f * s);
    };
    switch (type) {
        case 0: // workspace / home
            line(-9, -1, 0, -9); line(0, -9, 9, -1);
            line(-7, -2, -7, 8); line(7, -2, 7, 8);
            line(-7, 8, -2, 8); line(2, 8, 7, 8);
            line(-2, 8, -2, 2); line(-2, 2, 2, 2); line(2, 2, 2, 8);
            break;
        case 1: // appearance
            line(-9, -6, 9, -6); line(-9, 0, 9, 0); line(-9, 6, 9, 6);
            dl->AddCircleFilled(pt(-3, -6), 2.4f * s, color);
            dl->AddCircleFilled(pt(4, 0), 2.4f * s, color);
            dl->AddCircleFilled(pt(-1, 6), 2.4f * s, color);
            break;
        case 2: // controls
            dl->AddRect(pt(-8, -7), pt(8, 7), color, 3.f * s, 0, 1.6f * s);
            line(-4, -3, -4, 3); line(0, -3, 0, 3); line(4, -3, 4, 3);
            dl->AddCircleFilled(pt(-4, -1), 1.6f * s, color);
            dl->AddCircleFilled(pt(0, 2), 1.6f * s, color);
            dl->AddCircleFilled(pt(4, -2), 1.6f * s, color);
            break;
        case 3: // about
            dl->AddCircle(center, 8.f * s, color, 0, 1.6f * s);
            dl->AddCircleFilled(pt(0, -4), 1.15f * s, color);
            line(0, -1, 0, 5);
            break;
        case 4: // settings gear
            dl->AddCircle(center, 7.f * s, color, 0, 1.6f * s);
            dl->AddCircle(center, 2.4f * s, color, 0, 1.5f * s);
            for (int i = 0; i < 8; ++i) {
                const float a = (float)i * 0.78539816339f;
                line(std::cos(a) * 7.f, std::sin(a) * 7.f,
                     std::cos(a) * 9.f, std::sin(a) * 9.f);
            }
            break;
        case 5: // close
            line(-5, -5, 5, 5); line(-5, 5, 5, -5);
            break;
        case 6: { // combat / sword: tip, blade, crossguard, grip and pommel, drawn along the same
                  // 45 degree diagonal as the mace so the two category marks sit differently
            const float c = 0.70710678f, sn = 0.70710678f;
            const auto rot = [&](float x, float y) {
                return pt(x * c - y * sn, x * sn + y * c);
            };
            dl->AddQuadFilled(rot(0, -12), rot(2, -9), rot(2, 2.5f), rot(0, 2.5f), color);
            dl->AddQuadFilled(rot(0, -12), rot(-2, -9), rot(-2, 2.5f), rot(0, 2.5f), color);
            dl->AddLine(rot(-6, 2.5f), rot(6, 2.5f), color, 1.7f * s);
            dl->AddQuadFilled(rot(-6, 1.4f), rot(6, 1.4f), rot(6, 3.6f), rot(-6, 3.6f), color);
            dl->AddQuadFilled(rot(-1.3f, 3.6f), rot(1.3f, 3.6f), rot(1.3f, 8.4f), rot(-1.3f, 8.4f), color);
            dl->AddCircleFilled(rot(0, 9.8f), 1.9f * s, color, 12);
            break;
        }
        case 7: { // mace: a sledgehammer, head and handle rotated 45 degrees
            const float c = 0.70710678f, sn = 0.70710678f;
            const auto rot = [&](float x, float y) {
                return pt(x * c - y * sn, x * sn + y * c);
            };
            // handle: a vertical stick in local space, rotated into the diagonal
            dl->AddLine(rot(0, -2), rot(0, 9), color, 2.2f * s);
            // head: a rounded-looking block across the top of the handle
            dl->AddQuadFilled(rot(-6, -9), rot(6, -9), rot(6, -4), rot(-6, -4), color);
            break;
        }
        case 8: // misc / wrench
            dl->AddCircle(center, 6.5f * s, color, 0, 2.f * s);
            dl->AddCircleFilled(center, 2.1f * s, alpha(kPanel, 1.f));
            line(4, 4, 9, 9); line(6, 6, 9, 4); line(6, 6, 4, 9);
            break;
        case 9: // movement / running
            dl->AddCircleFilled(pt(2, -7), 2.f * s, color);
            line(1, -4, -2, 1); line(-2, 1, -8, 0); line(-2, 1, 4, 3);
            line(0, -3, 6, -1); line(-2, 1, -6, 8); line(1, 2, 5, 8);
            break;
        case 10: // spear
            line(-8, 8, 6, -6); line(-9, 9, -6, 6);
            line(4, -4, 9, -9); line(9, -9, 7, -3); line(9, -9, 3, -7);
            break;
        case 11: // visuals / eye
            dl->AddBezierCubic(pt(-9, 0), pt(-4, -7), pt(4, -7), pt(9, 0), color, 1.7f * s);
            dl->AddBezierCubic(pt(-9, 0), pt(-4, 7), pt(4, 7), pt(9, 0), color, 1.7f * s);
            dl->AddCircle(center, 3.f * s, color, 0, 1.7f * s);
            dl->AddCircleFilled(center, 1.f * s, color);
            break;
        case 12: // End Crystal
            line(0,-10,7,-4); line(7,-4,0,10); line(0,10,-7,-4); line(-7,-4,0,-10);
            line(-4,-4,4,4); line(4,-4,-4,4); line(-7,-4,7,-4);
            break;
        case 13: // keyboard
            dl->AddRect(pt(-10,-7),pt(10,7),color,3.f*s,0,1.7f*s);
            for (int row=0;row<2;++row) for (int col=0;col<4;++col)
                dl->AddRectFilled(pt(-7.f+col*4.2f,-4.f+row*4.f),pt(-5.2f+col*4.2f,-2.2f+row*4.f),color,0.5f*s);
            line(-5,4,5,4);
            break;
    }
}

bool drawIconButton(ImDrawList* dl, ImVec2 origin, const char* id, int icon,
                    float x, float y, float size, float scale, float opacity) {
    const Hit hit = hitBox(id, x, y, size, size, scale);
    const ImVec2 p0 = screenPos(origin, x, y, scale);
    const ImVec2 p1 = screenPos(origin, x + size, y + size, scale);
    if (hit.hover > 0.01f)
        dl->AddRectFilled(p0, p1, alpha(blend(kPanel, kCardLight, hit.hover), opacity),
                          11.f * scale);
    const ImVec2 center = screenPos(origin, x + size * 0.5f, y + size * 0.5f, scale);
    drawIcon(dl, center, icon, alpha(hit.hover > 0.1f ? accentColorU32() : kMuted, opacity), scale);
    if (g_softGlow && hit.hover > 0.7f)
        drawGlowRect(dl, p0, p1, alpha(accentColorU32(0.3f), opacity), 11.f * scale, scale);
    return hit.clicked;
}

void drawToggle(ImDrawList* dl, ImVec2 origin, const char* id, float x, float y,
                float width, float height, float scale, float opacity, bool& value) {
    ImGui::PushID(id);
    ImGui::SetCursorPos(ImVec2(x * scale, y * scale));
    const bool clicked = ImGui::InvisibleButton("##switch", ImVec2(width * scale, height * scale));
    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID key = ImGui::GetID("switchAnim");
    if (clicked) value = !value;
    float t = storage->GetFloat(key, value ? 1.f : 0.f);
    t = g_animations ? ease(t, value ? 1.f : 0.f, 17.f, ImGui::GetIO().DeltaTime)
                     : (value ? 1.f : 0.f);
    storage->SetFloat(key, t);
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();

    const ImVec2 p0 = screenPos(origin, x, y, scale);
    const ImVec2 p1 = screenPos(origin, x + width, y + height, scale);
    const ImU32 off = hovered ? IM_COL32(82, 90, 104, 255) : IM_COL32(60, 66, 78, 255);
    const float trackRounding = g_dropdownCards ? 3.f * scale : height * scale * 0.5f;
    dl->AddRectFilled(p0, p1, alpha(blend(off, accentColorU32(0.92f), t), opacity),
                      trackRounding);
    if (g_dropdownCards) dl->AddRect(p0, p1, alpha(kLine, opacity * 0.55f), trackRounding, 0, scale);
    const float radius = height * scale * 0.5f - 3.f * scale;
    const float left = p0.x + height * scale * 0.5f;
    const float right = p1.x - height * scale * 0.5f;
    const float cx = left + (right - left) * t;
    const float cy = (p0.y + p1.y) * 0.5f;
    if (g_dropdownCards)
        dl->AddRectFilled(ImVec2(cx - radius, cy - radius), ImVec2(cx + radius, cy + radius),
                          alpha(IM_COL32(247, 249, 252, 255), opacity), 2.f * scale);
    else
        dl->AddCircleFilled(ImVec2(cx, cy), radius, alpha(IM_COL32(247, 249, 252, 255), opacity));
}

void drawToggleRow(ImDrawList* dl, ImVec2 origin, const char* id,
                   const char* title, const char* subtitle, float x, float y,
                   float width, float scale, float opacity, bool& value) {
    drawText(dl, origin, x, y, title, kInk, 13.f, scale, opacity, true);
    drawText(dl, origin, x, y + 19.f, subtitle, kDim, 10.f, scale, opacity);
    drawToggle(dl, origin, id, x + width - 52.f, y + 7.f, 42.f, 22.f,
               scale, opacity, value);
}

// The one slider implementation: a filled value bar (no track+thumb), an
// animated fill that eases toward the real value, and a value readout that can
// be clicked to type an exact number. Every module/config slider calls this.
ImGuiID g_sliderEditingId = 0;          // 0 = no value field is being typed in
char    g_sliderEditBuffer[32] = {0};
bool    g_sliderEditFocus = false;      // focus the field on the frame it opens

void drawSlider(ImDrawList* dl, ImVec2 origin, const char* id, const char* label,
                const char* suffix, float x, float y, float width, float scale,
                float opacity, float& value, float low, float high) {
    ImGui::PushID(id);
    const bool percent = suffix && *suffix && std::string(suffix) == "%";
    const float barY = y + (g_dropdownCards ? 20.f : 23.f);
    const float barH = g_dropdownCards ? 6.f : 11.f;
    const float radius = g_dropdownCards ? 1.f * scale : barH * 0.5f * scale;

    // ---- drag surface: the whole bar, so the fill can be dragged anywhere ----
    ImGui::SetCursorPos(ImVec2(x * scale, (barY - 3.f) * scale));
    ImGui::InvisibleButton("##track", ImVec2(width * scale, (barH + 6.f) * scale));
    const bool dragging = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();
    if (dragging && high > low) {
        const float t = clamp01((ImGui::GetIO().MousePos.x - (origin.x + x * scale)) /
                                std::max(1.f, width * scale));
        value = low + (high - low) * t;
    }

    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID hoverKey = ImGui::GetID("##barHover");
    float hover = storage->GetFloat(hoverKey, 0.f);
    hover = g_animations ? ease(hover, (hovered || dragging) ? 1.f : 0.f, 15.f, ImGui::GetIO().DeltaTime)
                         : ((hovered || dragging) ? 1.f : 0.f);
    storage->SetFloat(hoverKey, hover);

    // ---- value readout (click to type an exact value) ----
    char formatted[40];
    if (suffix && *suffix) snprintf(formatted, sizeof(formatted), "%.0f%s", value * (percent ? 100.f : 1.f), suffix);
    else snprintf(formatted, sizeof(formatted), "%.2f", value);
    const float valueWidth = measureText(formatted, 11.f, scale) / scale;
    const float valueX = x + width - valueWidth;
    const ImGuiID valueId = ImGui::GetID("##value");
    const bool editing = (g_sliderEditingId == valueId);
    bool valueHovered = false;
    if (!editing) {
        ImGui::SetCursorPos(ImVec2((valueX - 5.f) * scale, (y - 3.f) * scale));
        ImGui::InvisibleButton("##valueText", ImVec2((valueWidth + 10.f) * scale, 17.f * scale));
        valueHovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            g_sliderEditingId = valueId;
            g_sliderEditFocus = true;
            snprintf(g_sliderEditBuffer, sizeof(g_sliderEditBuffer), "%.2f",
                     value * (percent ? 100.f : 1.f));
        }
    } else {
        ImGui::SetCursorPos(ImVec2((valueX - 6.f) * scale, (y - 4.f) * scale));
        ImGui::SetNextItemWidth((valueWidth + 14.f) * scale);
        if (g_sliderEditFocus) { ImGui::SetKeyboardFocusHere(); g_sliderEditFocus = false; }
        const bool entered = ImGui::InputText("##valueEdit", g_sliderEditBuffer,
                                             sizeof(g_sliderEditBuffer),
                                             ImGuiInputTextFlags_EnterReturnsTrue |
                                             ImGuiInputTextFlags_AutoSelectAll |
                                             ImGuiInputTextFlags_CharsScientific);
        const bool deactivated = ImGui::IsItemDeactivated();
        const bool cancelled = ImGui::IsKeyPressed(ImGuiKey_Escape);
        if (entered || deactivated || cancelled) {
            if (!cancelled) {
                const float parsed = (float)atof(g_sliderEditBuffer);
                value = clampf(percent ? parsed / 100.f : parsed, low, high);
            }
            g_sliderEditingId = 0;
        }
        valueHovered = true;
    }

    // ---- animated fill ----
    const float target = high > low ? clamp01((value - low) / (high - low)) : 0.f;
    const ImGuiID animKey = ImGui::GetID("##fill");
    float fill = storage->GetFloat(animKey, target);
    fill = g_animations ? ease(fill, target, 14.f, ImGui::GetIO().DeltaTime) : target;
    storage->SetFloat(animKey, fill);
    ImGui::PopID();

    drawText(dl, origin, x, y, label, kInk, 12.f, scale, opacity, true);
    drawText(dl, origin, valueX, y, formatted, valueHovered ? kInk : kMuted,
             11.f, scale, opacity, valueHovered);

    const ImVec2 track0 = screenPos(origin, x, barY, scale);
    const ImVec2 track1 = screenPos(origin, x + width, barY + barH, scale);
    dl->AddRectFilled(track0, track1, alpha(IM_COL32(58, 64, 75, 255), opacity), radius);
    if (fill > 0.002f) {
        const ImVec2 fillEnd(track0.x + (track1.x - track0.x) * fill, track1.y);
        dl->AddRectFilled(track0, fillEnd,
                          alpha(blend(accentColorU32(), IM_COL32(255, 255, 255, 255), 0.16f * hover), opacity),
                          radius);
        const float edge = 2.f * scale;
        dl->AddRectFilled(ImVec2(std::max(track0.x, fillEnd.x - edge), track0.y), fillEnd,
                          alpha(IM_COL32(247, 249, 252, 210), opacity), radius);
    }
    if (hover > 0.01f || valueHovered)
        dl->AddRect(track0, track1, alpha(accentColorU32(0.38f), opacity * std::max(hover, valueHovered ? 0.6f : 0.f)),
                    radius, 0, 1.2f * scale);
}

// Integer slider on the shared value-bar widget, so every module setting looks
// and behaves exactly like the interface sliders (animated fill + a value
// readout that can be clicked and typed into). Never passes "%" as the suffix:
// that suffix means "stored 0..1, displayed 0..100" in drawSlider.
void drawIntSlider(ImDrawList* dl, ImVec2 origin, const char* id, const char* label,
                   float x, float y, float width, float scale, float opacity,
                   int& value, int low, int high) {
    float v = (float)value;
    drawSlider(dl, origin, id, label, "", x, y, width, scale, opacity,
               v, (float)low, (float)high);
    value = std::clamp((int)std::lround(v), low, high);
}

// Declared here: the header and the command bar sit above these definitions and count each tab's
// modules from the live list.
modules::Category categoryForPage(Page page);
int categoryModuleCount(int cat);
void ensureDropdownOrder();
void drawDropdownTabs(ImDrawList* dl, ImVec2 origin, float width, float topY,
                      float scale, float opacity);

// Platinum layout: categories are a horizontal command bar under the brand band, not a vertical
// rail. Each chip carries its own module count, and the active chip is marked by a full-height
// brass rule plus a graphite fill, so the selected section is readable at a glance.
void drawTopNav(ImDrawList* dl, ImVec2 origin, float width, float scale, float opacity) {
    const float startX = 28.f;
    const float y = 64.f;
    const float h = 30.f;
    float x = startX;
    for (int i = 0; i < kCategoryCount; ++i) {
        const bool selected = g_page == kCategoryPages[i];
        int moduleCount = 0;
        for (const auto& entry : modules::entries())
            if (entry.category == categoryForPage(kCategoryPages[i])) ++moduleCount;

        char label[32];
        snprintf(label, sizeof(label), "%s", kCategoryNames[i]);
        char countText[8];
        snprintf(countText, sizeof(countText), "%d", moduleCount);

        const float labelW = measureText(label, 10.f, scale, true) / scale;
        const float countW = measureText(countText, 8.f, scale) / scale;
        // Room for the category's own icon, which sits before the name the way the old rail's did.
        const float iconW = 18.f;
        const float chipW = labelW + countW + 30.f + iconW;
        if (x + chipW > width - 28.f) break;

        const Hit hit = hitBox(kCategoryNames[i], x, y, chipW, h, scale);
        const ImVec2 p0 = screenPos(origin, x, y, scale);
        const ImVec2 p1 = screenPos(origin, x + chipW, y + h, scale);
        if (selected || hit.hover > 0.01f) {
            const ImU32 fill = selected ? blend(kPanel, kCardLight, 1.f) : kCard;
            dl->AddRectFilled(p0, p1, alpha(fill, opacity * (selected ? 1.f : hit.hover)), 4.f * scale);
        }
        if (selected) {
            const ImVec2 bar0 = screenPos(origin, x, y, scale);
            const ImVec2 bar1 = screenPos(origin, x + chipW, y + 2.f, scale);
            dl->AddRectFilled(bar0, bar1, alpha(accentColorU32(), opacity), scale);
        }
        drawIcon(dl, screenPos(origin, x + 10.f + iconW * 0.5f, y + h * 0.5f, scale),
                 kCategoryIcons[i],
                 alpha(selected ? accentColorU32() : (hit.hover > 0.3f ? kInk : kMuted), opacity),
                 scale);
        drawText(dl, origin, x + 10.f + iconW, y + 8.f, label,
                 alpha(selected ? kInk : (hit.hover > 0.3f ? kInk : kMuted), opacity),
                 10.f, scale, opacity, selected);
        drawText(dl, origin, x + chipW - countW - 9.f, y + 11.f, countText,
                 alpha(selected ? accentColorU32() : kDim, opacity), 8.f, scale, opacity, false);
        if (hit.hovered && !selected) ImGui::SetTooltip("%s  -  %d modules", kCategoryNames[i], moduleCount);
        if (hit.clicked && !selected) {
            g_page = kCategoryPages[i];
            g_pageFade = g_animations ? 0.f : 1.f;
            g_configOpen = false;
            g_searchText[0] = '\0';
        }
        x += chipW + 6.f;
    }
}

const char* pageTitle(Page page) {
    switch (page) {
        case Page::Combat: return "Combat";
        case Page::Mace: return "Mace";
        case Page::Misc: return "Misc";
        case Page::Movement: return "Movement";
        case Page::Spear: return "Spear";
        case Page::Visual: return "Visual";
        case Page::Crystal: return "Crystal";
        case Page::Macros: return "Macros";
        case Page::Configs: return "Configurations";
        case Page::HudEditor: return "HUD Editor";
        case Page::Settings: return "Settings";
        default: return "Visual";
    }
}

bool isCategoryPage(Page page) {
    return page == Page::Combat || page == Page::Mace || page == Page::Misc ||
           page == Page::Movement || page == Page::Spear || page == Page::Visual ||
           page == Page::Crystal || page == Page::Macros;
}

void drawHeader(ImDrawList* dl, ImVec2 origin, float width, float scale, float opacity) {
    // The Dropdown interface's own slim strip: brand at the left, the draggable page tabs centred,
    // search and close on the right, and the category columns start right under it. It used to
    // spend a whole second band on the brand plus a tab strip, which pushed the columns halfway
    // down the window.
    if (g_interfaceStyle == InterfaceStyle::Dropdown) {
        const float brandBreath = 0.86f + 0.14f * (0.5f + 0.5f * std::sin((float)ImGui::GetTime() * 1.25f));
        drawBrandMark(dl, origin, 20.f, 12.f, 24.f, scale, opacity * brandBreath);
        drawText(dl, origin, 52.f, 11.f, "PASSIVE", kInk, 10.5f, scale, opacity, true);
        drawText(dl, origin, 52.f, 25.f, pageTitle(g_page), kDim, 6.f, scale, opacity, true);
        if (isCategoryPage(g_page)) {
            const float searchW = 150.f;
            ImGui::SetCursorPos(ImVec2((width - searchW - 184.f) * scale, 14.f * scale));
            ImGui::PushID("dropdown-search");
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 14.f * scale);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9.f * scale, 6.f * scale));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.105f, 0.115f, opacity));
            ImGui::SetNextItemWidth(searchW * scale);
            ImGui::InputTextWithHint("##search", "search...", g_searchText, sizeof(g_searchText));
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
            ImGui::PopID();
        }
        // The Dropdown shell has no sidebar, so its three non-category pages live here.
        if (drawIconButton(dl, origin, "dd-open-configs", 1, width - 146.f, 15.f, 24.f, scale, opacity)) {
            g_page = Page::Configs;
            g_pageFade = g_animations ? 0.f : 1.f;
        }
        if (drawIconButton(dl, origin, "dd-open-hud", 11, width - 110.f, 15.f, 24.f, scale, opacity)) {
            g_page = Page::HudEditor;
            g_pageFade = g_animations ? 0.f : 1.f;
        }
        if (drawIconButton(dl, origin, "dd-open-settings", 4, width - 74.f, 15.f, 24.f, scale, opacity)) {
            g_page = Page::Settings;
            g_pageFade = g_animations ? 0.f : 1.f;
        }
        if (drawIconButton(dl, origin, "dropdown-close", 5, width - 38.f, 15.f, 24.f, scale, opacity))
            toggleMenu();
        dl->AddRectFilled(screenPos(origin, 20.f, 55.f, scale), screenPos(origin, width - 20.f, 56.f, scale),
                          alpha(kLine, opacity * 0.45f), 0.f);
        return;
    }
    // Two bands: a brand bar (monogram, wordmark, page heading, actions) and a horizontal command bar
    // of categories. (The Default interface uses its own library shell now; this path is kept for
    // older configurations that still draw the command-bar layout.)
    const bool dropdownUi = g_interfaceStyle == InterfaceStyle::Dropdown;

    // ---- brand bar --------------------------------------------------------------------------
    const float brandBreath = 0.86f + 0.14f * (0.5f + 0.5f * std::sin((float)ImGui::GetTime() * 1.25f));
    drawBrandMark(dl, origin, 28.f, 17.f, 24.f, scale, opacity * brandBreath);
    drawText(dl, origin, 58.f, 18.f, "PASSIVE", kInk, 12.f, scale, opacity, true);
    drawText(dl, origin, 58.f, 33.f, "NATIVE CLIENT", kDim, 7.f, scale, opacity, true);

    // Page heading sits inline with the brand, separated by a hairline, instead of under it.
    const ImVec2 div0 = screenPos(origin, 158.f, 16.f, scale);
    const ImVec2 div1 = screenPos(origin, 159.f, 42.f, scale);
    dl->AddRectFilled(div0, div1, alpha(kLine, opacity), scale);
    drawText(dl, origin, 172.f, 21.f, pageTitle(g_page), kInk, 16.f, scale, opacity, true);
    if (isCategoryPage(g_page)) {
        const char* subtitle = g_page == Page::Visual ? "Visual modules and on-screen information."
            : "Single-player utility modules.";
        drawText(dl, origin, 172.f, 40.f, subtitle, kDim, 8.f, scale, opacity);
    }

    char version[32];
    snprintf(version, sizeof(version), "v%s", info::kVersion);
    if (!isCategoryPage(g_page)) {
        const float pillW = measureText(version, 9.f, scale, true) / scale + 18.f;
        const float pillX = width - 190.f - pillW;
        const ImVec2 p0 = screenPos(origin, pillX, 22.f, scale);
        const ImVec2 p1 = screenPos(origin, pillX + pillW, 40.f, scale);
        dl->AddRectFilled(p0, p1, alpha(kCard, opacity), 3.f * scale);
        dl->AddRect(p0, p1, alpha(kLine, opacity), 3.f * scale, 0, scale);
        drawText(dl, origin, pillX + 9.f, 26.f, version, kMuted, 9.f, scale, opacity, true);
    }

    if (dropdownUi) {
        drawDropdownTabs(dl, origin, width, 64.f, scale, opacity);
    } else {
        drawTopNav(dl, origin, width, scale, opacity);
    }

    if (isCategoryPage(g_page)) {
        // Sits left of the preferences and close buttons, which own the last 92px of the band.
        const float searchX = width - 254.f;
        ImGui::SetCursorPos(ImVec2(searchX * scale, 20.f * scale));
        ImGui::PushID("module-search");
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.f * scale);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.f * scale, 4.f * scale));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.11f,0.115f,0.125f,opacity));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.93f,0.94f,0.96f,opacity));
        ImGui::SetNextItemWidth(150.f * scale);
        ImGui::InputTextWithHint("##search", "Search modules", g_searchText, sizeof(g_searchText));
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        ImGui::PopID();
    }

    if (drawIconButton(dl, origin, "open-preferences", 4, width - 92.f, 20.f,
                       30.f, scale, opacity)) {
        g_page = Page::Settings;
        g_settingsPage = SettingsPage::General;
        g_pageFade = g_animations ? 0.f : 1.f;
    }
    if (drawIconButton(dl, origin, "close-menu", 5, width - 54.f, 20.f,
                       30.f, scale, opacity)) toggleMenu();

    // Header rule: under the command bar for Platinum, under the tab strip for Dropdown.
    const float sepY = 102.f;
    const ImVec2 sep0 = screenPos(origin, 24.f, sepY, scale);
    const ImVec2 sep1 = screenPos(origin, width - 24.f, sepY + 1.f, scale);
    dl->AddRectFilled(sep0, sep1, alpha(kLine, opacity * 0.8f), scale);
}


void drawHeroArtwork(ImDrawList* dl, ImVec2 origin, float x, float y, float scale, float opacity) {
    const ImVec2 center = screenPos(origin, x + 108.f, y + 77.f, scale);
    const float r = 45.f * scale;
    dl->AddCircleFilled(center, r, alpha(accentColorU32(0.1f), opacity));
    dl->AddCircle(center, r, alpha(accentColorU32(0.28f), opacity), 0, 1.f * scale);
    dl->AddCircle(center, r * 0.72f, alpha(accentColorU32(0.18f), opacity), 0, scale);
    const ImVec2 tile0 = screenPos(origin, x + 63.f, y + 30.f, scale);
    const ImVec2 tile1 = screenPos(origin, x + 153.f, y + 120.f, scale);
    dl->AddRectFilled(tile0, tile1, alpha(IM_COL32(34, 38, 45, 230), opacity), 14.f * scale);
    dl->AddRect(tile0, tile1, alpha(accentColorU32(0.24f), opacity), 14.f * scale, 0, scale);
    drawBrandMark(dl, origin, x + 87.f, y + 50.f, 42.f, scale, opacity);
    for (int i = 0; i < 3; ++i) {
        const float sx = x + 171.f + i * 15.f;
        const ImVec2 dot0 = screenPos(origin, sx, y + 55.f, scale);
        const ImVec2 dot1 = screenPos(origin, sx + 5.f, y + 60.f, scale);
        dl->AddRectFilled(dot0, dot1, alpha(accentColorU32(0.55f - i * 0.12f), opacity), 2.f * scale);
    }
}

void drawWorkspace(ImDrawList* dl, ImVec2 origin, float scale, float opacity,
                   float x, float y, float width, float height) {
    const float heroH = std::min(158.f, height * 0.36f);
    drawCard(dl, origin, x, y, width, heroH, scale, opacity, true);
    drawText(dl, origin, x + 24.f, y + 22.f, "START FRESH",
             accentColorU32(), 9.f, scale, opacity, true);
    drawText(dl, origin, x + 24.f, y + 47.f, "A cleaner canvas.",
             kInk, 25.f, scale, opacity, true);
    drawText(dl, origin, x + 24.f, y + 86.f,
             "The old module catalog is cleared. This build has no gameplay modules.",
             kMuted, 11.f, scale, opacity);
    drawText(dl, origin, x + 24.f, y + 107.f,
             "Use Appearance and Controls to shape the interface.",
             kDim, 10.f, scale, opacity);
    drawHeroArtwork(dl, origin, x + width - 230.f, y, scale, opacity);

    const float gap = g_compact ? 10.f : 14.f;
    const float lowerY = y + heroH + gap;
    const float lowerH = std::max(130.f, height - heroH - gap);
    const float leftW = width * 0.56f;
    const float rightX = x + leftW + gap;
    const float rightW = width - leftW - gap;

    drawCard(dl, origin, x, lowerY, leftW, lowerH, scale, opacity);
    drawText(dl, origin, x + 20.f, lowerY + 18.f, "MODULE CATALOG",
             kDim, 9.f, scale, opacity, true);
    drawText(dl, origin, x + leftW - 46.f, lowerY + 14.f, "0",
             accentColorU32(), 18.f, scale, opacity, true);
    const float emptyY = lowerY + 66.f;
    const ImVec2 emptyCenter = screenPos(origin, x + 42.f, emptyY + 8.f, scale);
    dl->AddCircle(emptyCenter, 15.f * scale, alpha(accentColorU32(0.52f), opacity), 0, 1.5f * scale);
    dl->AddCircle(emptyCenter, 5.f * scale, alpha(accentColorU32(0.82f), opacity), 0, 1.3f * scale);
    drawText(dl, origin, x + 68.f, emptyY - 3.f, "Nothing is active.",
             kInk, 15.f, scale, opacity, true);
    drawText(dl, origin, x + 68.f, emptyY + 22.f,
             "There are no gameplay features in this build.",
             kMuted, 10.f, scale, opacity);
    drawText(dl, origin, x + 20.f, lowerY + lowerH - 24.f,
             "0 AVAILABLE  /  0 ENABLED", kDim, 8.f, scale, opacity, true);

    drawCard(dl, origin, rightX, lowerY, rightW, lowerH, scale, opacity);
    drawText(dl, origin, rightX + 18.f, lowerY + 17.f, "QUICK SETTINGS",
             kDim, 9.f, scale, opacity, true);
    const float rowGap = g_compact ? 52.f : 59.f;
    drawToggleRow(dl, origin, "quick-motion", "Interface motion",
                  "Tab and control transitions", rightX + 18.f, lowerY + 48.f,
                  rightW - 36.f, scale, opacity, g_animations);
    drawToggleRow(dl, origin, "quick-glow", "Accent glow",
                  "Soft coral hover highlights", rightX + 18.f, lowerY + 48.f + rowGap,
                  rightW - 36.f, scale, opacity, g_softGlow);
    const float buttonY = lowerY + lowerH - 48.f;
    if (drawButton(dl, origin, "customize-interface", "CUSTOMIZE INTERFACE",
                    rightX + 18.f, buttonY, rightW - 36.f, 32.f, scale, opacity, true, true)) {
        g_page = Page::Settings;
        g_settingsPage = SettingsPage::Appearance;
        g_pageFade = g_animations ? 0.f : 1.f;
    }
}

void drawAccentSwatches(ImDrawList* dl, ImVec2 origin, float x, float y,
                        float scale, float opacity) {
    const float size = 34.f;
    const float gap = 12.f;
    for (int i = 0; i < (int)(sizeof(kAccentPresets) / sizeof(kAccentPresets[0])); ++i) {
        const float bx = x + i * (size + gap);
        ImGui::PushID(i);
        ImGui::SetCursorPos(ImVec2(bx * scale, y * scale));
        const bool clicked = ImGui::InvisibleButton("##accent", ImVec2(size * scale, size * scale));
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();

        ImVec4 rgb;
        ImGui::ColorConvertHSVtoRGB(kAccentPresets[i].hue, kAccentPresets[i].saturation,
                                    kAccentPresets[i].value, rgb.x, rgb.y, rgb.z);
        const ImU32 color = IM_COL32((int)(rgb.x * 255.f), (int)(rgb.y * 255.f),
                                     (int)(rgb.z * 255.f), 255);
        const ImVec2 p0 = screenPos(origin, bx, y, scale);
        const ImVec2 p1 = screenPos(origin, bx + size, y + size, scale);
        const float rounding = 10.f * scale;
        dl->AddRectFilled(p0, p1, alpha(color, opacity), rounding);
        if (i == g_accentIndex || hovered)
            dl->AddRect(p0, p1, alpha(i == g_accentIndex ? kInk : color, opacity),
                        rounding, 0, (i == g_accentIndex ? 2.f : 1.f) * scale);
        if (clicked) {
            g_accentIndex = i;
            setAccentHue(kAccentPresets[i].hue,
                         kAccentPresets[i].saturation, kAccentPresets[i].value);
        }
    }
}

void drawAppearance(ImDrawList* dl, ImVec2 origin, float scale, float opacity,
                    float x, float y, float width, float height) {
    const float gap = g_compact ? 10.f : 14.f;
    const float leftW = width * 0.58f;
    const float rightX = x + leftW + gap;
    const float rightW = width - leftW - gap;

    drawCard(dl, origin, x, y, leftW, height, scale, opacity, true);
    drawText(dl, origin, x + 20.f, y + 18.f, "ACCENT COLOR",
             kDim, 9.f, scale, opacity, true);
    drawText(dl, origin, x + 20.f, y + 37.f, "Choose a palette.",
             kInk, 17.f, scale, opacity, true);
    drawText(dl, origin, x + 20.f, y + 63.f, "The accent animates smoothly across the interface.",
             kMuted, 10.f, scale, opacity);
    drawAccentSwatches(dl, origin, x + 20.f, y + 96.f, scale, opacity);

    drawSlider(dl, origin, "ui-scale", "Interface scale", "%",
               x + 20.f, y + 153.f, leftW - 40.f, scale, opacity,
               g_uiScale, 0.80f, 1.75f);
    drawSlider(dl, origin, "panel-opacity", "Surface opacity", "%",
               x + 20.f, y + 211.f, leftW - 40.f, scale, opacity,
               g_panelOpacity, 0.84f, 1.f);

    drawText(dl, origin, x + 20.f, y + height - 26.f, "FIGMA-INSPIRED CORAL PALETTE",
             kDim, 8.f, scale, opacity, true);

    drawCard(dl, origin, rightX, y, rightW, height, scale, opacity);
    drawText(dl, origin, rightX + 18.f, y + 18.f, "MOTION & DETAIL",
             kDim, 9.f, scale, opacity, true);
    const float rowGap = g_compact ? 58.f : 68.f;
    drawToggleRow(dl, origin, "appearance-motion", "Smooth motion",
                  "Fade and slide transitions", rightX + 18.f, y + 55.f,
                  rightW - 36.f, scale, opacity, g_animations);
    drawToggleRow(dl, origin, "appearance-glow", "Accent glow",
                  "Subtle hover emphasis", rightX + 18.f, y + 55.f + rowGap,
                  rightW - 36.f, scale, opacity, g_softGlow);
    drawToggleRow(dl, origin, "appearance-compact", "Compact spacing",
                  "Tighter cards and rows", rightX + 18.f, y + 55.f + rowGap * 2.f,
                  rightW - 36.f, scale, opacity, g_compact);
    if (drawButton(dl, origin, "reset-appearance", "RESET APPEARANCE",
                    rightX + 18.f, y + height - 50.f, rightW - 36.f, 32.f,
                    scale, opacity, false, true)) {
        g_accentIndex = 0;
        setAccentHue(kAccentPresets[0].hue, kAccentPresets[0].saturation,
                     kAccentPresets[0].value);
        g_uiScale = 1.f;
        g_panelOpacity = 0.96f;
        g_animations = true;
        g_compact = false;
        g_softGlow = true;
    }
}

const char* keyName(int vk) {
    static char buffer[16];
    if (vk == 0) return "UNBOUND";
    switch (vk) {
        case VK_ESCAPE: return "ESC";
        case VK_TAB: return "TAB";
        case VK_SHIFT: case VK_LSHIFT: return "LSHIFT";
        case VK_RSHIFT: return "RSHIFT";
        case VK_CONTROL: return "CTRL";
        case VK_MENU: return "ALT";
        case VK_SPACE: return "SPACE";
        case VK_RETURN: return "ENTER";
        case VK_BACK: return "BACKSPACE";
        case VK_CAPITAL: return "CAPS";
        case VK_DELETE: return "DEL";
        case VK_INSERT: return "INS";
        case VK_HOME: return "HOME";
        case VK_END: return "END";
        case VK_PRIOR: return "PGUP";
        case VK_NEXT: return "PGDN";
        case VK_LEFT: return "LEFT";
        case VK_RIGHT: return "RIGHT";
        case VK_UP: return "UP";
        case VK_DOWN: return "DOWN";
        default: break;
    }
    if (vk >= '0' && vk <= '9') { snprintf(buffer, sizeof(buffer), "%c", (char)vk); return buffer; }
    if (vk >= 'A' && vk <= 'Z') { snprintf(buffer, sizeof(buffer), "%c", (char)vk); return buffer; }
    if (vk >= VK_F1 && vk <= VK_F12) {
        snprintf(buffer, sizeof(buffer), "F%d", vk - VK_F1 + 1);
        return buffer;
    }
    snprintf(buffer, sizeof(buffer), "0x%02X", vk);
    return buffer;
}

void captureMenuKey() {
    if (!g_captureMenuKey) return;
    for (int vk : g_taps) {
        if (vk == VK_ESCAPE) {
            g_captureMenuKey = false;
            notify("Key capture cancelled");
        } else {
            g_menuKey = vk;
            g_captureMenuKey = false;
            std::string message("Menu key set to ");
            message.append(keyName(vk));
            notify(message.c_str());
        }
        break;
    }
}

void captureModuleKey() {
    if (g_captureModuleBind.empty()) return;
    for (int vk : g_taps) {
        if (vk == VK_ESCAPE) {
            // ESC while capturing sets the bind to None instead of changing nothing, so an
            // accidental Escape can always unbind a module again.
            for (const modules::Entry& entry : modules::entries()) {
                if (g_captureModuleBind == entry.name && entry.keybind) {
                    *entry.keybind = 0;
                    std::string message = entry.name;
                    message += " keybind set to None";
                    notify(message.c_str());
                    break;
                }
            }
        } else {
            for (const modules::Entry& entry : modules::entries()) {
                if (g_captureModuleBind == entry.name && entry.keybind) {
                    *entry.keybind = vk == VK_BACK ? 0 : vk;
                    std::string message = entry.name;
                    message += *entry.keybind ? " keybind set to " : " keybind cleared";
                    if (*entry.keybind) message += keyName(*entry.keybind);
                    notify(message.c_str());
                    break;
                }
            }
        }
        g_captureModuleBind.clear();
        break;
    }
}

void drawControls(ImDrawList* dl, ImVec2 origin, float scale, float opacity,
                  float x, float y, float width, float height) {
    const float gap = g_compact ? 10.f : 14.f;
    const float leftW = width * 0.58f;
    const float rightX = x + leftW + gap;
    const float rightW = width - leftW - gap;

    drawCard(dl, origin, x, y, leftW, height, scale, opacity, true);
    drawText(dl, origin, x + 20.f, y + 19.f, "MENU ACCESS",
             kDim, 9.f, scale, opacity, true);
    drawText(dl, origin, x + 20.f, y + 48.f, "Open and close Passive",
             kInk, 17.f, scale, opacity, true);
    drawText(dl, origin, x + 20.f, y + 77.f,
             "Choose a key that feels comfortable in-game.",
             kMuted, 10.f, scale, opacity);
    drawText(dl, origin, x + 20.f, y + 132.f, "MENU KEY",
             kDim, 9.f, scale, opacity, true);

    char bindText[64];
    snprintf(bindText, sizeof(bindText), "%s%s",
             g_captureMenuKey ? "PRESS A KEY" : keyName(g_menuKey),
             g_captureMenuKey ? "  /  ESC TO CANCEL" : "");
    drawText(dl, origin, x + 20.f, y + 154.f, bindText,
             g_captureMenuKey ? accentColorU32() : kInk, 16.f, scale, opacity, true);
    if (drawButton(dl, origin, "capture-menu-key", g_captureMenuKey ? "WAITING..." : "CHANGE KEY",
                   x + 20.f, y + 194.f, 164.f, 34.f, scale, opacity, true, true)) {
        g_captureMenuKey = true;
        g_taps.clear();
    }
    captureMenuKey();

    drawText(dl, origin, x + 20.f, y + height - 27.f,
             "Ctrl+Shift+End is the emergency unload chord.", kDim, 9.f, scale, opacity);

    drawCard(dl, origin, rightX, y, rightW, height, scale, opacity);
    drawText(dl, origin, rightX + 18.f, y + 19.f, "BEHAVIOUR",
             kDim, 9.f, scale, opacity, true);
    drawToggleRow(dl, origin, "close-escape", "Close with Escape",
                  "Dismiss the menu from any page", rightX + 18.f, y + 58.f,
                  rightW - 36.f, scale, opacity, g_closeWithEscape);
    drawToggleRow(dl, origin, "controls-motion", "Smooth motion",
                  "Ease panel and tab changes", rightX + 18.f, y + 126.f,
                  rightW - 36.f, scale, opacity, g_animations);
    drawText(dl, origin, rightX + 18.f, y + height - 44.f,
             "Mouse and keyboard input pause while this menu is open.",
             kDim, 9.f, scale, opacity);
}

void drawAbout(ImDrawList* dl, ImVec2 origin, float scale, float opacity,
               float x, float y, float width, float height) {
    drawCard(dl, origin, x, y, width, height, scale, opacity, true);
    drawBrandMark(dl, origin, x + 30.f, y + 35.f, 48.f, scale, opacity);
    drawText(dl, origin, x + 98.f, y + 39.f, "PASSIVE",
             kInk, 22.f, scale, opacity, true);
    drawText(dl, origin, x + 98.f, y + 69.f, info::kGame,
             kMuted, 11.f, scale, opacity);
    drawText(dl, origin, x + 98.f, y + 88.f, "Author: Sword",
             kInk, 11.f, scale, opacity, true);
    drawText(dl, origin, x + 30.f, y + 124.f,
             "A native ImGui overlay focused on a clean, configurable interface.",
             kMuted, 12.f, scale, opacity);
    drawText(dl, origin, x + 30.f, y + 151.f,
             "Modules span combat, crystal, macros, misc, and visual tools.",
             kDim, 10.f, scale, opacity);

    const float rowY = y + 207.f;
    const float cardW = (width - 60.f) / 3.f;
    const char* labels[] = { "RENDERING", "INPUT", "FEATURES" };
    const char* values[] = { "IMGUI / OPENGL", "MINHOOK", "8 MODULES" };
    for (int i = 0; i < 3; ++i) {
        const float cx = x + 20.f + i * (cardW + 10.f);
        drawCard(dl, origin, cx, rowY, cardW, 66.f, scale, opacity);
        drawText(dl, origin, cx + 12.f, rowY + 12.f, labels[i],
                 kDim, 8.f, scale, opacity, true);
        drawText(dl, origin, cx + 12.f, rowY + 34.f, values[i],
                 kInk, 10.f, scale, opacity, true);
    }
}

bool containsInsensitive(const char* value, const char* query) {
    if (!query || !*query) return true;
    if (!value) return false;
    std::string hay(value), needle(query);
    std::transform(hay.begin(),hay.end(),hay.begin(),[](unsigned char c){return (char)std::tolower(c);});
    std::transform(needle.begin(),needle.end(),needle.begin(),[](unsigned char c){return (char)std::tolower(c);});
    return hay.find(needle)!=std::string::npos;
}

modules::Category categoryForPage(Page page) {
    switch (page) {
        case Page::Combat:return modules::Category::Combat;
        case Page::Mace:return modules::Category::Mace;
        case Page::Misc:return modules::Category::Misc;
        case Page::Movement:return modules::Category::Movement;
        case Page::Spear:return modules::Category::Spear;
        case Page::Crystal:return modules::Category::Crystal;
        case Page::Macros:return modules::Category::Macros;
        default:return modules::Category::Visual;
    }
}

// How many modules a category holds, for the Dropdown layout's tab badges.
int categoryModuleCount(int cat) {
    if (cat < 0 || cat >= kCategoryCount) return 0;
    const modules::Category wanted = categoryForPage(kCategoryPages[cat]);
    int count = 0;
    for (const auto& entry : modules::entries())
        if (entry.category == wanted) ++count;
    return count;
}

// The Dropdown layout's tab order, filled with the natural order the first time it is used.
void ensureDropdownOrder() {
    if ((int)g_dropdownOrder.size() == kCategoryCount) return;
    std::vector<int> order;
    for (int i = 0; i < kCategoryCount; ++i) order.push_back(i);
    g_dropdownOrder = order;
}

void setWidgetCursor(float x,float y,float scale) {
    ImGui::SetCursorPos(ImVec2(x*scale,y*scale));
}

float moduleCardHeight(const char* name,bool expanded) {
    if (!expanded) return 84.f;
    const std::string module=name?name:"";
    // A Java module's body is drawn from its own descriptor, so its height is counted from its
    // settings instead of a hand-written number here.
    const float javaH=modules::java::cardHeight(module,true);
    if (javaH>0.f) return javaH;
    if (module=="ESP") return 585.f;
    if (module=="AutoAnchor") return 365.f;
    if (module=="Auto Shield Break") return 405.f;
    if (module=="Auto Drain") return 436.f;
    if (module=="Auto Pressure Plate Web") return 585.f;
    if (module=="Arraylist") return 165.f;
    if (module=="Fast Place") return 230.f;
    if (module=="Sprint") return 150.f;
    if (module=="Auto Jump Reset") return 190.f;
    if (module=="AimAssist") return 520.f;
    if (module=="Triggerbot") return 370.f;
    if (module=="AutoMace") return 468.f;
    if (module=="Anti-Bot") return 190.f;
    if (module=="Nametags") return 425.f;
    if (module=="HUD") return 1930.f;
    if (module=="StaticFov") return 170.f;
    if (module=="Auto Totem") return 376.f;
    if (module=="AutoClean") return 360.f;
    if (module=="Trajectory Preview") return 190.f;
    if (module=="Item Scroll") return 170.f;
    if (module=="Cart Refill") return 320.f;
    if (module=="Instant Cart Macro") return 312.f;
    if (module=="Pearl Catch") return 250.f;
    if (module=="Pearl Agro") return 400.f;
    if (module=="Shader ESP")
        return modules::shaderEspSettings().style==modules::ShaderGlowStyle::Custom ? 610.f : 505.f;
    if (module=="Drag Click Friend") return 300.f;
    if (module=="Stun Web") return 380.f;
    if (module=="Walk Trail") return 440.f;
    if (module=="Logout Spots") return 470.f;
    if (module=="Motion Blur") return 470.f;
    if (module=="Player Model") return 230.f;
    if (module=="Wings") return 410.f;
    if (module=="Headwear") return 350.f;
    if (module=="Capes") return 240.f;
    if (module=="ViewModel") return 480.f;
    return 300.f;
}

void drawModuleKeybind(ImDrawList* dl,ImVec2 origin,const modules::Entry& entry,
                       float x,float y,float width,float scale,float opacity) {
    if (!entry.keybind) return;
    drawText(dl,origin,x,y,"KEYBINDS / DEFAULT",kDim,8.f,scale,opacity,true);
    drawText(dl,origin,x,y+15.f,entry.macro?"Execute key":"Toggle module",kMuted,8.f,scale,opacity);
    const std::string buttonId=std::string("keybind-")+entry.name;
    const bool waiting=g_captureModuleBind==entry.name;
    const std::string label=waiting?"PRESS A KEY":"SET KEYBIND";
    if (drawButton(dl,origin,buttonId.c_str(),label.c_str(),x+width-154.f,y+2.f,
                   134.f,27.f,scale,opacity,false,true)) {
        g_captureModuleBind=entry.name;
        g_taps.clear();
    }
    if (!waiting) {
        const int key=*entry.keybind;
        const std::string value=key==0?"UNBOUND":keyName(key);
        drawText(dl,origin,x+width-246.f,y+10.f,value.c_str(),kInk,8.f,scale,opacity,true);
    }
}

// A Java module's options: the card body is built from the module's own descriptor, so a ported
// module gets the same checkboxes, sliders and dropdowns the native cards draw without a branch
// per module here. Returns false when `entry` is not a Java module (the native chain runs then).
bool drawJavaModuleOptions(ImDrawList* dl,ImVec2 origin,const modules::Entry& entry,
                           float x,float y,float width,float scale,float opacity) {
    modules::java::Module* module=modules::java::find(entry.name?entry.name:"");
    if (!module) return false;
    float rowY=y+76.f;
    int index=0;
    for (modules::java::Setting& setting:module->settings) {
        ImGui::PushID(index++);
        switch (setting.type) {
            case modules::java::SettingType::Bool: {
                const bool before=setting.boolValue;
                setWidgetCursor(x+20.f,rowY,scale);
                ui::Checkbox(setting.label.c_str(),&setting.boolValue);
                if (before!=setting.boolValue) modules::java::setBool(*module,setting,setting.boolValue);
                rowY+=30.f;
                break;
            }
            case modules::java::SettingType::Int: {
                const int before=setting.intValue;
                char id[128];
                snprintf(id,sizeof(id),"java-%s-%s",module->name.c_str(),setting.key.c_str());
                drawIntSlider(dl,origin,id,setting.label.c_str(),x+20.f,rowY,width-40.f,scale,opacity,
                              setting.intValue,setting.minInt,setting.maxInt);
                if (before!=setting.intValue) modules::java::setInt(*module,setting,setting.intValue);
                rowY+=37.f;
                break;
            }
            case modules::java::SettingType::Float: {
                const float before=setting.floatValue;
                char id[128];
                snprintf(id,sizeof(id),"java-%s-%s",module->name.c_str(),setting.key.c_str());
                drawSlider(dl,origin,id,setting.label.c_str(),"",x+20.f,rowY,width-40.f,scale,opacity,
                           setting.floatValue,setting.minFloat,setting.maxFloat);
                if (before!=setting.floatValue) modules::java::setFloat(*module,setting,setting.floatValue);
                rowY+=37.f;
                break;
            }
            case modules::java::SettingType::Mode: {
                drawText(dl,origin,x+20.f,rowY,setting.label.c_str(),kDim,8.f,scale,opacity,true);
                const char* current=(setting.modeIndex>=0&&setting.modeIndex<(int)setting.options.size())
                                        ?setting.options[(size_t)setting.modeIndex].c_str():"-";
                setWidgetCursor(x+20.f,rowY+16.f,scale);
                ImGui::SetNextItemWidth(196.f*scale);
                if (ui::BeginCombo("##mode",current)) {
                    for (int i=0;i<(int)setting.options.size();++i)
                        if (ImGui::Selectable(setting.options[(size_t)i].c_str(),setting.modeIndex==i))
                            modules::java::setMode(*module,setting,i);
                    ui::EndCombo();
                }
                rowY+=46.f;
                break;
            }
        }
        ImGui::PopID();
    }
    drawModuleKeybind(dl,origin,entry,x+20.f,rowY+4.f,width-40.f,scale,opacity);
    return true;
}

// Shorten a line with an ellipsis so it fits a squared card's width.
std::string ellipsize(const std::string& text, float logicalSize, float scale, float maxWidth) {
    const float limit = std::max(8.f, maxWidth) * scale;
    if (measureText(text.c_str(), logicalSize, scale) <= limit) return text;
    std::string out = text;
    while (!out.empty() && measureText((out + "...").c_str(), logicalSize, scale) > limit)
        out.pop_back();
    return out + "...";
}

// ---- Module tag chips ----------------------------------------------------------------------
// Every module carries a Tag (Safe / Experimental / Unsafe / Blatant) drawn as a small chip
// beside its name: a symbol in the tag's colour plus the tag's name. The symbol is drawn from
// shapes - a warning triangle or a tick - so it renders the same with any font. Hovering the
// chip explains what the tag means.
ImU32 tagColor(modules::Tag tag) {
    switch (tag) {
        case modules::Tag::Safe:         return IM_COL32(96, 224, 150, 255);    // green
        case modules::Tag::Experimental: return IM_COL32(240, 200, 80, 255);    // yellow
        case modules::Tag::Unsafe:       return IM_COL32(235, 90, 80, 255);     // red
        default:                          return IM_COL32(232, 60, 96, 255);     // Blatant: harsh red-pink
    }
}

const char* tagName(modules::Tag tag) {
    switch (tag) {
        case modules::Tag::Safe:         return "SAFE";
        case modules::Tag::Experimental: return "EXPERIMENTAL";
        case modules::Tag::Unsafe:       return "UNSAFE";
        default:                          return "BLATANT";
    }
}

const char* tagHelpText(modules::Tag tag) {
    switch (tag) {
        case modules::Tag::Safe:         return "Safe: very safe to use on servers, very unlikely to be detected.";
        case modules::Tag::Experimental: return "Experimental: a beta feature that will still get updates and improvements.";
        case modules::Tag::Unsafe:       return "Unsafe: can lead to a ban on most anticheats.";
        default:                          return "Blatant: very obvious and detectable, and can lead to a manual ban.";
    }
}

// The chip's width in logical pixels for a tag at a 7.5 px label.
float moduleTagWidth(modules::Tag tag, float scale) {
    return measureText(tagName(tag), 7.5f, scale, true) / scale + 24.f;
}

// Draws the chip with its top-left corner at logical (x, y); the chip is 14 px tall.
void drawModuleTag(ImDrawList* dl, ImVec2 origin, const modules::Entry& entry,
                   float x, float y, float scale, float opacity) {
    const ImU32 col = tagColor(entry.tag);
    const float w = moduleTagWidth(entry.tag, scale);
    const ImVec2 p0 = screenPos(origin, x, y, scale);
    const ImVec2 p1 = screenPos(origin, x + w, y + 14.f, scale);
    dl->AddRectFilled(p0, p1, alpha(blend(kCard, col, 0.16f), opacity), 4.f * scale);
    dl->AddRect(p0, p1, alpha(blend(kLine, col, 0.6f), opacity), 4.f * scale, 0, scale);
    const float cy = (p0.y + p1.y) * 0.5f;
    if (entry.tag == modules::Tag::Safe) {
        // A tick, drawn as two strokes so no font glyph is needed.
        const float ix = p0.x + 6.f * scale;
        dl->AddLine(ImVec2(ix, cy - 0.5f * scale), ImVec2(ix + 2.2f * scale, cy + 2.4f * scale),
                    alpha(col, opacity), 1.6f * scale);
        dl->AddLine(ImVec2(ix + 2.2f * scale, cy + 2.4f * scale), ImVec2(ix + 6.f * scale, cy - 3.f * scale),
                    alpha(col, opacity), 1.6f * scale);
    } else {
        // A warning triangle with an exclamation mark inside it.
        const float ix = p0.x + 6.5f * scale;
        dl->AddTriangle(ImVec2(ix, cy - 3.4f * scale),
                        ImVec2(ix - 3.6f * scale, cy + 2.6f * scale),
                        ImVec2(ix + 3.6f * scale, cy + 2.6f * scale), alpha(col, opacity), 1.2f * scale);
        dl->AddLine(ImVec2(ix, cy - 0.6f * scale), ImVec2(ix, cy + 0.8f * scale), alpha(col, opacity), 1.4f * scale);
        dl->AddCircleFilled(ImVec2(ix, cy + 2.f * scale), 0.7f * scale, alpha(col, opacity));
    }
    drawText(dl, origin, x + 14.f, y + 3.f, tagName(entry.tag), col, 7.5f, scale, opacity, true);
    // Hovering the chip explains the tag.
    if (ImGui::IsMouseHoveringRect(p0, p1)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26.f);
        ImGui::TextUnformatted(tagHelpText(entry.tag));
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// The Dropdown interface puts each module behind its own rounded button instead of a switch: the
// whole row is the control, it carries the module's name, and the state is written inside it
// (ON / OFF, or MACRO for a module that runs from its keybind). On fills with the accent, off is
// a plain outline, and both keep the rounded corners the rest of the layout uses.
bool drawDropdownModuleButton(ImDrawList* dl, ImVec2 origin, const char* id, const modules::Entry& entry,
                              float x, float y, float w, float h, float scale, float opacity) {
    const Hit hit = hitBox(id, x, y, w, h, scale);
    const bool on = entry.enabled && *entry.enabled;
    const ImVec2 p0 = screenPos(origin, x, y, scale);
    const ImVec2 p1 = screenPos(origin, x + w, y + h, scale);
    const float rounding = 9.f * scale;
    const ImU32 fill = on
        ? blend(kCard, accentColorU32(), 0.34f + 0.10f * hit.hover)
        : blend(kCard, kCardLight, 0.35f + 0.65f * hit.hover);
    dl->AddRectFilled(p0, p1, alpha(fill, opacity), rounding);
    dl->AddRect(p0, p1, alpha(on ? accentColorU32() : kLine, opacity * (on ? 1.f : 0.8f)),
                rounding, 0, (on ? 1.6f : 1.1f) * scale);
    if (hit.hover > 0.01f && !on)
        dl->AddRect(p0, p1, alpha(accentColorU32(0.55f), opacity * hit.hover), rounding, 0, 1.2f * scale);
    drawText(dl, origin, x + 14.f, y + (h - 13.f) * 0.5f, entry.name, on ? kInk : kMuted,
             12.5f, scale, opacity, true);
    {   // the tag chip sits between the module's name and its ON/OFF state
        const float nameW = measureText(entry.name, 12.5f, scale, true) / scale;
        drawModuleTag(dl, origin, entry, x + 24.f + nameW, y + (h - 14.f) * 0.5f, scale, opacity);
    }
    const char* state = entry.macro ? "MACRO" : (on ? "ON" : "OFF");
    drawText(dl, origin, x + w - 44.f, y + (h - 9.f) * 0.5f, state,
             on ? accentColorU32() : kDim, 8.5f, scale, opacity, true);
    return hit.clicked;
}

// The HUD card: the long list of look-and-feel options, grouped so it stays readable. It is a
// separate function because it does not share the per-card two-column rhythm the other cards use,
// and it owns the only interactive 9x9 grid in the interface (the Draw Board).
void drawHudOptions(ImDrawList* dl, ImVec2 origin, float pageX, float pageW, float cardY,
                    float scale, float opacity) {
    modules::HudSettings& s = modules::hudSettings();
    ImGui::PushID("hud-options");
    const float left = pageX + 20.f;
    const float col2 = pageX + pageW * 0.5f + 20.f;
    const float halfW = pageW * 0.5f - 40.f;
    const auto section = [&](float y, const char* title) {
        drawText(dl, origin, left, y, title, kDim, 8.f, scale, opacity, true);
    };
    // A titled colour swatch. `id` keeps each ColorEdit4 a separate widget (they all share one ID
    // scope), and AlphaPreviewHalf makes the alpha readable without opening the picker.
    const auto colorRow = [&](float y, float x, float width, const char* label, ImVec4& c, const char* id) {
        drawText(dl, origin, x, y, label, kMuted, 8.f, scale, opacity, true);
        setWidgetCursor(x, y + 13.f, scale);
        ImGui::PushID(id);
        ImGui::SetNextItemWidth(width * scale);
        ImGui::ColorEdit4("##c", &c.x,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar |
            ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_PickerHueBar);
        ImGui::PopID();
    };

    // ---- elements ----
    section(cardY + 76.f, "HUD ELEMENTS");
    setWidgetCursor(left, cardY + 92.f, scale);  ui::Checkbox("Custom hotbar",&s.customHotbar);
    setWidgetCursor(col2, cardY + 92.f, scale);  ui::Checkbox("Hotbar animations",&s.hotbarAnimations);
    setWidgetCursor(left, cardY + 122.f, scale); ui::Checkbox("Rounded hotbar",&s.hotbarRounded);
    setWidgetCursor(col2, cardY + 122.f, scale); ui::Checkbox("Slot numbers",&s.hotbarNumbers);
    setWidgetCursor(left, cardY + 152.f, scale); ui::Checkbox("Item counts",&s.hotbarItemCount);
    setWidgetCursor(col2, cardY + 152.f, scale); ui::Checkbox("Held item plaque",&s.itemNamePopup);
    setWidgetCursor(left, cardY + 182.f, scale); ui::Checkbox("Bar panels",&s.panels);
    setWidgetCursor(col2, cardY + 182.f, scale); ui::Checkbox("Attack speed bar",&s.attackSpeedBar);
    setWidgetCursor(left, cardY + 212.f, scale); ui::Checkbox("Custom crosshair",&s.customCrosshair);
    setWidgetCursor(col2, cardY + 212.f, scale); ui::Checkbox("Cover vanilla hotbar/bars",&s.coverVanilla);

    // ---- hotbar look ----
    section(cardY + 248.f, "HOTBAR");
    drawSlider(dl,origin,"hud-hotbar-scale","Hotbar scale","",left,cardY+264.f,pageW-40.f,scale,opacity,s.hotbarScale,0.6f,1.6f);
    drawSlider(dl,origin,"hud-hotbar-round","Corner rounding","",left,cardY+301.f,pageW-40.f,scale,opacity,s.hotbarRounding,0.f,16.f);
    drawSlider(dl,origin,"hud-hotbar-anim","Animation speed","",left,cardY+338.f,pageW-40.f,scale,opacity,s.hotbarAnimSpeed,4.f,30.f);
    colorRow(cardY + 380.f, left, halfW, "HOTBAR ACCENT", s.hotbarAccent, "hotbarAccent");
    colorRow(cardY + 380.f, col2, halfW, "SLOT BACKGROUND", s.slotBackground, "slotBg");

    // ---- bars / panels ----
    section(cardY + 425.f, "BARS & PANELS");
    drawSlider(dl,origin,"hud-panel-scale","Panel scale","",left,cardY+441.f,pageW-40.f,scale,opacity,s.panelScale,0.6f,1.6f);
    drawIntSlider(dl,origin,"hud-panel-offset","Panel offset above hotbar (px)",left,cardY+478.f,pageW-40.f,scale,opacity,s.panelOffsetY,0,120);
    setWidgetCursor(left, cardY + 515.f, scale); ui::Checkbox("Panel icons",&s.panelIcons);
    setWidgetCursor(col2, cardY + 515.f, scale); ui::Checkbox("Panel values",&s.panelNumbers);
    colorRow(cardY + 548.f, left, halfW, "HEALTH", s.healthColor, "health");
    colorRow(cardY + 548.f, col2, halfW, "ARMOR", s.armorColor, "armor");
    colorRow(cardY + 591.f, left, halfW, "FOOD", s.foodColor, "food");
    colorRow(cardY + 591.f, col2, halfW, "XP", s.xpColor, "xp");
    colorRow(cardY + 634.f, left, halfW, "PANEL BACKGROUND", s.panelBackground, "panelBg");

    // ---- held-item plaque ----
    section(cardY + 679.f, "HELD ITEM PLAQUE");
    drawSlider(dl,origin,"hud-name-scale","Plaque scale","",left,cardY+695.f,pageW-40.f,scale,opacity,s.itemNameScale,0.6f,1.6f);
    drawSlider(dl,origin,"hud-name-hold","Plaque hold time","ms",left,cardY+732.f,pageW-40.f,scale,opacity,s.itemNameHoldMs,300.f,5000.f);
    setWidgetCursor(left, cardY + 769.f, scale); ui::Checkbox("Item name icons",&s.itemNameIcons);

    // ---- attack speed bar ----
    section(cardY + 805.f, "ATTACK SPEED BAR");
    drawSlider(dl,origin,"hud-atk-scale","Attack bar scale","",left,cardY+821.f,pageW-40.f,scale,opacity,s.attackSpeedScale,0.6f,1.6f);
    colorRow(cardY + 863.f, left, halfW, "ATTACK SPEED COLOR", s.attackSpeedColor, "atkColor");

    // ---- crosshair ----
    section(cardY + 908.f, "CROSSHAIR");
    drawText(dl,origin,left,cardY+924.f,"PRESET",kDim,8.f,scale,opacity,true);
    setWidgetCursor(left, cardY + 940.f, scale);
    ImGui::PushID("hud-crosshair-style");
    ImGui::SetNextItemWidth(240.f * scale);
    static const char* const kCrosshairNames[] = { "Cross", "Dot", "Cross + Dot", "Circle", "Circle + Dot",
                                                   "T-Shape", "Chevron", "Arrows", "Draw Board" };
    const int styleIndex = std::clamp((int)s.crosshairStyle, 0, 8);
    if (ui::BeginCombo("##style", kCrosshairNames[styleIndex])) {
        for (int i = 0; i < 9; ++i)
            if (ImGui::Selectable(kCrosshairNames[i], styleIndex == i))
                s.crosshairStyle = (modules::CrosshairStyle)i;
        ui::EndCombo();
    }
    ImGui::PopID();
    drawSlider(dl,origin,"hud-cross-size","Crosshair size","",left,cardY+977.f,pageW-40.f,scale,opacity,s.crosshairSize,0.4f,3.f);
    drawSlider(dl,origin,"hud-cross-gap","Crosshair gap","",left,cardY+1014.f,pageW-40.f,scale,opacity,s.crosshairGap,0.f,12.f);
    drawSlider(dl,origin,"hud-cross-thick","Crosshair thickness","",left,cardY+1051.f,pageW-40.f,scale,opacity,s.crosshairThickness,0.5f,5.f);
    drawSlider(dl,origin,"hud-cross-dot","Crosshair dot","",left,cardY+1088.f,pageW-40.f,scale,opacity,s.crosshairDot,0.5f,6.f);
    drawSlider(dl,origin,"hud-cross-rot","Rotation","deg",left,cardY+1125.f,pageW-40.f,scale,opacity,s.crosshairRotation,-180.f,180.f);
    setWidgetCursor(left, cardY + 1162.f, scale); ui::Checkbox("Outline",&s.crosshairOutline);
    setWidgetCursor(col2, cardY + 1162.f, scale); ui::Checkbox("Opens while attacking",&s.crosshairDynamic);
    colorRow(cardY + 1197.f, left, halfW, "CROSSHAIR COLOR", s.crosshairColor, "crossColor");

    // ---- draw board ----
    section(cardY + 1242.f, "DRAW BOARD");
    drawText(dl,origin,left,cardY+1258.f,"Paint a 9x9 crosshair; the Preset must be Draw Board for it to show.",kDim,8.f,scale,opacity);
    const float cell = 15.f;
    for (int row = 0; row < 9; ++row) {
        setWidgetCursor(left, cardY + 1278.f + row * (cell + 2.f), scale);
        ImGui::PushID(row);
        for (int col = 0; col < 9; ++col) {
            ImGui::PushID(col);
            const bool on = s.drawBoardPixels[row * 9 + col];
            ImGui::PushStyleColor(ImGuiCol_Button, on ? accentColor() : ImVec4(0.16f, 0.13f, 0.19f, 1.f));
            if (ImGui::Button("##px", ImVec2(cell * scale, cell * scale)))
                s.drawBoardPixels[row * 9 + col] = !on;
            ImGui::PopStyleColor();
            ImGui::PopID();
            if (col < 8) ImGui::SameLine(0.f, 2.f * scale);
        }
        ImGui::PopID();
    }
    drawSlider(dl,origin,"hud-board-pixel","Draw board pixel size","",pageX+20.f+9.f*(cell+2.f)+16.f,
               cardY+1278.f+140.f,pageW-40.f-9.f*(cell+2.f)-16.f,scale,opacity,s.drawBoardPixel,1.f,6.f);

    // ---- the game's own HUD, and the bar behaviour --------------------------------------------
    // The game hides its whole HUD as one switch, and that switch takes the first-person hand, the
    // chat, the Tab list and the scoreboard with it. This client never touches it: Cover draws an
    // opaque plate over the game's own hotbar and bar rows instead.
    section(cardY + 1460.f, "VANILLA HUD");
    drawText(dl,origin,left,cardY+1476.f,"Cover draws the plate over the game's hotbar and bars; hand, chat, Tab and scoreboard are never hidden.",kDim,8.f,scale,opacity);
    drawSlider(dl,origin,"hud-plate-pad","Plate padding","px",left,cardY+1492.f,pageW-40.f,scale,opacity,s.platePadding,0.f,24.f);
    colorRow(cardY + 1534.f, left, halfW, "PLATE COLOR", s.plateColor, "plateColor");

    section(cardY + 1580.f, "BAR BEHAVIOUR");
    setWidgetCursor(left, cardY + 1596.f, scale); ui::Checkbox("Bars slide to the new value",&s.barAnimations);
    setWidgetCursor(col2, cardY + 1596.f, scale); ui::Checkbox("Heart beats below 2 hearts",&s.heartBeat);
    setWidgetCursor(left, cardY + 1626.f, scale); ui::Checkbox("Absorption: yellow health bar",&s.absorbHealth);
    drawSlider(dl,origin,"hud-bar-anim","Bar animation speed","",left,cardY+1656.f,pageW-40.f,scale,opacity,s.barAnimSpeed,3.f,30.f);
    colorRow(cardY + 1698.f, left, halfW, "ABSORPTION", s.absorbColor, "absorbColor");

    section(cardY + 1744.f, "CROSSHAIR INDICATOR");
    drawText(dl,origin,left,cardY+1760.f,"While the crosshair is on a living entity inside the range it is drawn in this colour.",kDim,8.f,scale,opacity);
    setWidgetCursor(left, cardY + 1776.f, scale); ui::Checkbox("Enabled",&s.crosshairIndicator);
    drawSlider(dl,origin,"hud-ind-range","Range","blocks",left,cardY+1806.f,pageW-40.f,scale,opacity,s.crosshairIndicatorRange,1.f,64.f);
    colorRow(cardY + 1848.f, left, halfW, "INDICATOR COLOR", s.crosshairIndicatorColor, "indColor");
    ImGui::PopID();
}

// Draws the cards of `list` from `layoutY` downwards (layoutY moves along with the stack) and
// counts matches and visible cards into the two counters. Both interfaces draw through this, so
// every module option below a card behaves identically in each of them.
void drawModuleCards(ImDrawList* dl, ImVec2 origin, float scale, float opacity,
                     float x, float width, const std::vector<const modules::Entry*>& list,
                     float& layoutY, int& matchedCount, int& visibleCount) {
    g_styleCards=true;
    {
        float fr=4.f;
        switch (g_moduleStyle) {
            case ModuleStyle::Rounded: fr=12.f; break;
            case ModuleStyle::Squared: fr=0.f; break;
            case ModuleStyle::Minimal: fr=2.f; break;
            case ModuleStyle::Outline: fr=6.f; break;
            default: break;
        }
        // The Dropdown interface squares off the ImGui controls it draws inline.
        if (g_dropdownCards) fr=0.f;
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,fr*scale);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding,fr*scale);
    }
    // Squared shape: a 3-wide grid of square cards (row/column instead of a
    // vertical stack). Every card is `squaredCell` wide and tall.
    // Squared is a Default-interface shape: its selector is hidden in the Dropdown layout, so a
    // Squared left over from a preset must not turn the Dropdown sections into three-wide grids.
    const bool squared = !g_dropdownCards && !g_inspectorCards && g_moduleShape==ModuleShape::Squared;
    const float squaredGap = 12.f;
    const float squaredCell = (width - 2.f*squaredGap) / 3.f;
    std::unordered_map<std::string,float> targets;
    std::unordered_map<std::string,float> gridX;
    int gridIndex=0;
    const float top=layoutY;   // the stack's first row (the grid's own origin when Squared)
    for (const modules::Entry* entry:list) {
        if (containsInsensitive(entry->name,g_searchText)||containsInsensitive(entry->description,g_searchText)) {
            if (squared) {
                const int col=gridIndex%3, row=gridIndex/3;
                gridX[entry->name]=x+col*(squaredCell+squaredGap);
                targets[entry->name]=top+row*(squaredCell+squaredGap);
                ++gridIndex;
            } else {
                targets[entry->name]=layoutY;
                layoutY+=moduleCardHeight(entry->name,g_moduleExpanded[entry->name])+10.f;
            }
            ++matchedCount;
        }
    }
    // Squared shape draws in two passes: every card first, then the expanded
    // configuration panels, so a panel is always painted over the cards below
    // it instead of fighting them for the same rows. Rectangle keeps its single
    // pass and its original in-card layout.
    for (int pass=0; pass<(squared?2:1); ++pass) {
    for (const modules::Entry* entry : list) {
        const bool matched=containsInsensitive(entry->name,g_searchText)||containsInsensitive(entry->description,g_searchText);
        bool& expanded=g_moduleExpanded[entry->name];
        float& visibility=g_moduleVisibility[entry->name];
        float cardX=x, cardY=top, cardW=squared?squaredCell:width, cardH=squaredCell, vis=1.f;
        float clipTop=0.f, clipBottom=0.f;
        if (pass==0) {
            const float target=matched?1.f:0.f;
            visibility += (target-visibility)*(1.f-std::exp(-14.f*ImGui::GetIO().DeltaTime));
            auto position=g_modulePositions.try_emplace(entry->name,matched?targets[entry->name]+14.f:top).first;
            float& pos=position->second;
            if (matched) pos += (targets[entry->name]-pos)*(1.f-std::exp(-14.f*ImGui::GetIO().DeltaTime));
            if (visibility<0.015f) {
                if (!matched) { g_modulePositions.erase(entry->name); g_moduleVisibility.erase(entry->name); }
                continue;
            }
            ++visibleCount;
            cardH = squared ? squaredCell : moduleCardHeight(entry->name,expanded);
            cardX = (squared && matched) ? gridX[entry->name] : x;
            cardY = pos;
            vis = visibility;
            cardW = squared ? squaredCell : width;
            if (!g_inspectorCards)
                drawCard(dl,origin,cardX,cardY,cardW,cardH,scale,opacity*vis,true);
            // In the Dropdown layout the name lives inside the module's own button (drawn below),
            // so the card itself does not repeat it and the button sits on the header row.
            if (!g_dropdownCards && !g_inspectorCards) {
                drawText(dl,origin,cardX+(squared?14.f:20.f),cardY+(squared?14.f:18.f),entry->name,
                         kInk,squared?13.f:16.f,scale,opacity*vis,true);
                // The module's tag chip: beside the name on a Rectangle card, and under the
                // description on a Squared one, where the name row is too narrow to share.
                if (squared)
                    drawModuleTag(dl,origin,*entry,cardX+14.f,cardY+52.f,scale,opacity*vis);
                else {
                    const float nameW=measureText(entry->name,16.f,scale,true)/scale;
                    drawModuleTag(dl,origin,*entry,cardX+30.f+nameW,cardY+20.f,scale,opacity*vis);
                }
            }
            if (squared)
                drawText(dl,origin,cardX+14.f,cardY+34.f,
                         ellipsize(entry->description,9.f,scale,cardW-28.f).c_str(),
                         kMuted,9.f,scale,opacity*vis,true);
            else
                drawText(dl,origin,cardX+20.f,cardY+45.f,entry->description,kMuted,10.f,scale,opacity*vis);
        } else {
            if (!matched) continue;
            const auto posIt=g_modulePositions.find(entry->name);
            const auto visIt=g_moduleVisibility.find(entry->name);
            const auto gridIt=gridX.find(entry->name);
            if (posIt==g_modulePositions.end()||visIt==g_moduleVisibility.end()||gridIt==gridX.end()) continue;
            vis=visIt->second;
            if (vis<0.015f) continue;
            float& panelAnim=g_cfgPanelAnim[entry->name];
            panelAnim = g_animations ? ease(panelAnim,expanded?1.f:0.f,13.f,ImGui::GetIO().DeltaTime)
                                     : (expanded?1.f:0.f);
            if (panelAnim<=0.015f) { g_cfgPanelAnim.erase(entry->name); g_cfgScroll.erase(entry->name); continue; }
            cardX=gridIt->second;
            cardY=posIt->second;
            cardH=squaredCell;
            // The panel is a second square attached directly under the card:
            // no gap, a seam that merges the two card fills and an accent line.
            const float panelTop=cardY+cardH;
            const float panelH=cardH*panelAnim;
            drawCard(dl,origin,cardX,panelTop,cardH,panelH,scale,opacity*vis,true);
            dl->AddRectFilled(screenPos(origin,cardX+1.f,panelTop-3.f,scale),
                              screenPos(origin,cardX+cardH-1.f,panelTop+3.f,scale),
                              alpha(kCard,opacity*vis),0.f);
            dl->AddRectFilled(screenPos(origin,cardX+10.f,panelTop-1.f,scale),
                              screenPos(origin,cardX+cardH-10.f,panelTop+1.f,scale),
                              alpha(accentColorU32(0.85f),opacity*vis),1.f*scale);
            drawText(dl,origin,cardX+14.f,panelTop+9.f,"CONFIGURATION",kDim,8.f,scale,opacity*vis,true);
            // Scroll the option stack inside the square when it is taller.
            const float contentTop=panelTop+40.f;
            const float visibleH=std::max(24.f,(panelH-48.f));
            const float contentH=std::max(0.f,moduleCardHeight(entry->name,true)-76.f+30.f);
            float& scroll=g_cfgScroll[entry->name];
            const float maxScroll=std::max(0.f,contentH-visibleH);
            const ImVec2 hp0=screenPos(origin,cardX,panelTop,scale);
            const ImVec2 hp1=screenPos(origin,cardX+cardH,panelTop+panelH,scale);
            if (ImGui::IsMouseHoveringRect(hp0,hp1) && ImGui::GetIO().MouseWheel!=0.f)
                scroll=clampf(scroll-ImGui::GetIO().MouseWheel*26.f,0.f,maxScroll);
            scroll=clampf(scroll,0.f,maxScroll);
            cardY=panelTop-scroll;
            cardH=panelH;
            clipTop=contentTop;
            clipBottom=panelTop+panelH-6.f;
            if (maxScroll>0.5f) {
                const float thumbH=std::max(16.f,visibleH*(visibleH/std::max(visibleH,contentH)));
                const float thumbY=contentTop+(visibleH-thumbH)*clamp01(scroll/maxScroll);
                dl->AddRectFilled(screenPos(origin,cardX+cardH-6.f,thumbY,scale),
                                  screenPos(origin,cardX+cardH-3.f,thumbY+thumbH,scale),
                                  alpha(accentColorU32(0.8f),opacity*vis),2.f*scale);
            }
        }
        if (pass==0 && matched && !g_inspectorCards) {
            ImGui::PushID(entry->name);
            // Squared cards keep their controls on the bottom edge so a long module
            // name can never collide with them; Rectangle keeps the original spots.
            // The Dropdown layout's header row is the module's button, so its expand control
            // moves into the button's right-hand corner and stays clear of it.
            const bool dropdownRow = g_dropdownCards && !squared;
            const float expandX = squared ? cardX+cardW-40.f : (dropdownRow ? cardX+cardW-48.f : cardX+cardW-116.f);
            const float expandY = squared ? cardY+cardH-40.f : cardY+16.f;
            const float chevX   = squared ? cardX+cardW-28.f : (dropdownRow ? cardX+cardW-34.f : cardX+cardW-104.f);
            const float chevY   = squared ? cardY+cardH-27.f : cardY+29.f;
            const float toggleX = squared ? cardX+14.f       : cardX+cardW-76.f;
            const float toggleY = squared ? cardY+cardH-42.f : cardY+18.f;
            const std::string expandId=std::string("expand-")+entry->name;
            if (drawButton(dl,origin,expandId.c_str(),nullptr,expandX,expandY,
                           24.f,26.f,scale,opacity*visibility,false,true)) expanded=!expanded;
            {   // expand / collapse chevron: points right when collapsed, down when expanded
                const ImVec2 c=screenPos(origin,chevX,chevY,scale);
                const float u=scale;
                const ImU32 chevron=alpha(kInk,opacity*visibility);
                if (expanded) {
                    dl->AddLine(ImVec2(c.x-5.f*u,c.y-2.5f*u),ImVec2(c.x,c.y+3.f*u),chevron,2.f*u);
                    dl->AddLine(ImVec2(c.x,c.y+3.f*u),ImVec2(c.x+5.f*u,c.y-2.5f*u),chevron,2.f*u);
                } else {
                    dl->AddLine(ImVec2(c.x-2.5f*u,c.y-5.f*u),ImVec2(c.x+3.f*u,c.y),chevron,2.f*u);
                    dl->AddLine(ImVec2(c.x+3.f*u,c.y),ImVec2(c.x-2.5f*u,c.y+5.f*u),chevron,2.f*u);
                }
            }
            if (entry->enabled) {
                const bool wasEnabled=*entry->enabled;
                if (dropdownRow) {
                    const std::string buttonId=std::string("module-button-")+entry->name;
                    if (drawDropdownModuleButton(dl,origin,buttonId.c_str(),*entry,
                                                 cardX+10.f,cardY+10.f,cardW-72.f,34.f,
                                                 scale,opacity*visibility))
                        *entry->enabled=!*entry->enabled;
                } else {
                    drawToggle(dl,origin,(std::string("module-")+entry->name).c_str(),toggleX,toggleY,
                               54.f,24.f,scale,opacity,*entry->enabled);
                }
                if (wasEnabled!=*entry->enabled)
                    modules::notifyModuleToggled(entry->name,*entry->enabled);
            } else {
                drawText(dl,origin,squared?cardX+14.f:cardX+cardW-75.f,
                         squared?cardY+cardH-35.f:cardY+25.f,"MACRO",accentColorU32(),8.f,scale,opacity,true);
            }
            ImGui::PopID();
        }

        // Options: drawn straight into the card for Rectangle, and into the
        // separate attached square panel on the second pass for Squared.
        const bool drawOptions = (pass==0) ? (expanded && !squared) : true;
        if (drawOptions) {
            ImGui::PushID(entry->name);
            const float pageX = cardX;
            const float pageW = cardW;
            if (pass==1) dl->PushClipRect(screenPos(origin,cardX+1.f,clipTop,scale),
                                          screenPos(origin,cardX+cardW-1.f,clipBottom,scale),true);
            if (expanded && drawJavaModuleOptions(dl,origin,*entry,pageX,cardY,pageW,scale,opacity)) {
                // Java module: the descriptor above drew its settings and keybind.
            } else if (expanded && std::string(entry->name)=="Arraylist") {
                drawText(dl,origin,pageX+20.f,cardY+76.f,"Enabled modules appear on the right edge, sorted A to Z.",
                         kDim,9.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+91.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="HUD") {
                drawHudOptions(dl,origin,pageX,pageW,cardY,scale,opacity);
            } else if (expanded && std::string(entry->name)=="ESP") {
                modules::EspSettings& settings=modules::espSettings();
                drawText(dl,origin,pageX+20.f,cardY+78.f,"ESP MODE",kDim,8.f,scale,opacity,true);
                setWidgetCursor(pageX+20.f,cardY+96.f,scale);
                ImGui::PushID("esp-mode");
                ImGui::SetNextItemWidth(196.f*scale);
                const char* modeName=settings.mode==modules::EspMode::TwoD?"2D ESP":"3D ESP";
                if (ui::BeginCombo("##mode",modeName)) {
                    bool two=settings.mode==modules::EspMode::TwoD;
                    if (ImGui::Selectable("2D ESP",two)) settings.mode=modules::EspMode::TwoD;
                    if (ImGui::Selectable("3D ESP",!two)) settings.mode=modules::EspMode::ThreeD;
                    ui::EndCombo();
                }
                ImGui::PopID();
                setWidgetCursor(pageX+20.f,cardY+132.f,scale);
                if (settings.mode==modules::EspMode::ThreeD) ui::Checkbox("Only Outline (3D)",&settings.onlyOutline);
                setWidgetCursor(pageX+20.f,cardY+162.f,scale);
                ui::Checkbox("Allow ESP For Entities",&settings.allowEntities);
                setWidgetCursor(pageX+20.f,cardY+192.f,scale);
                ui::Checkbox("Allow ESP For Players",&settings.allowPlayers);
                drawText(dl,origin,pageX+20.f,cardY+224.f,"ESP COLOR",kDim,8.f,scale,opacity,true);
                setWidgetCursor(pageX+20.f,cardY+240.f,scale);
                ImGui::PushID("esp-color");
                ImGui::SetNextItemWidth(205.f*scale);
                ImGui::ColorPicker4("##color",&settings.color.x,
                    ImGuiColorEditFlags_PickerHueBar|ImGuiColorEditFlags_NoSidePreview|
                    ImGuiColorEditFlags_NoSmallPreview|ImGuiColorEditFlags_NoInputs|
                    ImGuiColorEditFlags_NoOptions|ImGuiColorEditFlags_AlphaBar);
                ImGui::PopID();
                drawText(dl,origin,pageX+240.f,cardY+224.f,"FRIEND COLOR",kDim,8.f,scale,opacity,true);
                setWidgetCursor(pageX+240.f,cardY+240.f,scale);
                ImGui::PushID("esp-friend-color");
                ImGui::SetNextItemWidth(205.f*scale);
                ImGui::ColorPicker4("##friendcolor",&settings.friendColor.x,
                    ImGuiColorEditFlags_PickerHueBar|ImGuiColorEditFlags_NoSidePreview|
                    ImGuiColorEditFlags_NoSmallPreview|ImGuiColorEditFlags_NoInputs|
                    ImGuiColorEditFlags_NoOptions|ImGuiColorEditFlags_AlphaBar);
                ImGui::PopID();
                setWidgetCursor(pageX+20.f,cardY+468.f,scale); ui::Checkbox("Highlight friends",&settings.highlightFriends);
                setWidgetCursor(pageX+240.f,cardY+468.f,scale); ui::Checkbox("Show 3D health",&settings.showHealth3d);
                drawText(dl,origin,pageX+20.f,cardY+500.f,"2D uses a health bar; 3D draws an indicator when enabled.",kDim,8.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+518.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Instant Cart Macro") {
                modules::InstantCartMacroSettings& settings=modules::instantCartMacroSettings();
                ImGui::PushID("instant-cart-options");
                drawIntSlider(dl,origin,"instantcart-bow","Bow Draw Time (ms)",pageX+20.f,cardY+77.f,pageW-40.f,scale,opacity,settings.bowChargeMs,50,900);
                drawIntSlider(dl,origin,"instantcart-rail","Rail Delay (ms)",pageX+20.f,cardY+114.f,pageW-40.f,scale,opacity,settings.railDelayMs,0,600);
                drawIntSlider(dl,origin,"instantcart-cart","Cart Delay (ms)",pageX+20.f,cardY+151.f,pageW-40.f,scale,opacity,settings.cartDelayMs,0,600);
                setWidgetCursor(pageX+20.f,cardY+188.f,scale); ui::Checkbox("Swap Back",&settings.swapBack);
                ImGui::PopID();
                drawText(dl,origin,pageX+20.f,cardY+222.f,"Fires a partial bow straight away, then places a rail and a TNT minecart",kDim,8.f,scale,opacity);
                drawText(dl,origin,pageX+20.f,cardY+238.f,"with the Use Item key. Look at a block within block range for the two",kDim,8.f,scale,opacity);
                drawText(dl,origin,pageX+20.f,cardY+254.f,"placements - the bind itself needs nothing in view.",kDim,8.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+276.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Pearl Catch") {
                modules::PearlCatchSettings& settings=modules::pearlCatchSettings();
                ImGui::PushID("pearl-catch-options");
                drawIntSlider(dl,origin,"pearlcatch-pearl-delay","Pearl Delay (ms)",pageX+20.f,cardY+77.f,pageW-40.f,scale,opacity,settings.pearlDelayMs,0,600);
                drawIntSlider(dl,origin,"pearlcatch-wind-delay","Wind Charge Delay (ms)",pageX+20.f,cardY+114.f,pageW-40.f,scale,opacity,settings.windChargeDelayMs,0,600);
                setWidgetCursor(pageX+20.f,cardY+151.f,scale); ui::Checkbox("Swap Back",&settings.swapBack);
                ImGui::PopID();
                drawText(dl,origin,pageX+20.f,cardY+184.f,"Tap the bind to throw an ender pearl, then a wind charge, both from",kDim,8.f,scale,opacity);
                drawText(dl,origin,pageX+20.f,cardY+200.f,"the hotbar. The charge catches the pearl and lifts its flight.",kDim,8.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+218.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="AutoAnchor") {
                modules::AutoAnchorSettings& settings=modules::autoAnchorSettings();
                ImGui::PushID("auto-anchor-options");
                drawIntSlider(dl,origin,"autoanchor-totem-slot","Totem slot (1-9)",pageX+20.f,cardY+77.f,pageW-40.f,scale,opacity,settings.totemSlot,1,9);
                drawIntSlider(dl,origin,"autoanchor-glowstone-delay","Glowstone delay (ms)",pageX+20.f,cardY+112.f,pageW-40.f,scale,opacity,settings.glowstoneDelayMs,0,600);
                drawIntSlider(dl,origin,"autoanchor-totem-delay","Totem delay (ms)",pageX+20.f,cardY+147.f,pageW-40.f,scale,opacity,settings.totemDelayMs,0,600);
                ImGui::PopID();
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+207.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Triggerbot") {
                modules::TriggerbotSettings& settings=modules::triggerbotSettings();
                ImGui::PushID("triggerbot-options");
                const float rightX=pageX+pageW*0.5f+6.f;
                setWidgetCursor(pageX+20.f,cardY+77.f,scale); ui::Checkbox("All Items",&settings.allItems);
                setWidgetCursor(pageX+20.f,cardY+107.f,scale); ui::Checkbox("Check Shield",&settings.checkShield);
                setWidgetCursor(pageX+20.f,cardY+137.f,scale); ui::Checkbox("Click Simulation",&settings.clickSimulation);
                setWidgetCursor(rightX,cardY+77.f,scale); ui::Checkbox("Stray Bypass",&settings.strayBypass);
                setWidgetCursor(rightX,cardY+107.f,scale); ui::Checkbox("All Entities",&settings.allEntities);
                setWidgetCursor(rightX,cardY+137.f,scale); ui::Checkbox("Same Player",&settings.samePlayer);
                drawText(dl,origin,pageX+20.f,cardY+172.f,"CRITICALS",kDim,8.f,scale,opacity,true);
                setWidgetCursor(pageX+20.f,cardY+188.f,scale);
                ImGui::PushID("triggerbot-criticals");
                ImGui::SetNextItemWidth(196.f*scale);
                const char* critName=settings.criticals==modules::CritMode::PrioCrit?"Prio Crit":
                    (settings.criticals==modules::CritMode::OnlyCrit?"Only Crit":"Default");
                if (ui::BeginCombo("##criticals",critName)) {
                    if (ImGui::Selectable("Default",settings.criticals==modules::CritMode::Default))
                        settings.criticals=modules::CritMode::Default;
                    if (ImGui::Selectable("Prio Crit",settings.criticals==modules::CritMode::PrioCrit))
                        settings.criticals=modules::CritMode::PrioCrit;
                    if (ImGui::Selectable("Only Crit",settings.criticals==modules::CritMode::OnlyCrit))
                        settings.criticals=modules::CritMode::OnlyCrit;
                    ui::EndCombo();
                }
                ImGui::PopID();
                const float halfW=(pageW-40.f-12.f)*0.5f;
                drawIntSlider(dl,origin,"triggerbot-sword-min","Sword min (ms)",pageX+20.f,cardY+232.f,halfW,scale,opacity,settings.swordDelayMinMs,0,1000);
                drawIntSlider(dl,origin,"triggerbot-sword-max","Sword max (ms)",pageX+32.f+halfW,cardY+232.f,halfW,scale,opacity,settings.swordDelayMaxMs,0,1000);
                drawIntSlider(dl,origin,"triggerbot-axe-min","Axe min (ms)",pageX+20.f,cardY+269.f,halfW,scale,opacity,settings.axeDelayMinMs,0,1000);
                drawIntSlider(dl,origin,"triggerbot-axe-max","Axe max (ms)",pageX+32.f+halfW,cardY+269.f,halfW,scale,opacity,settings.axeDelayMaxMs,0,1000);
                ImGui::PopID();
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+312.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="AutoMace") {
                modules::AutoMaceSettings& settings=modules::autoMaceSettings();
                ImGui::PushID("automace-options");
                setWidgetCursor(pageX+20.f,cardY+77.f,scale); ui::Checkbox("StunSlam (axe first)",&settings.stunSlam);
                setWidgetCursor(pageX+20.f,cardY+107.f,scale); ui::Checkbox("Elytra Swap",&settings.elytraSwap);
                setWidgetCursor(pageX+20.f,cardY+137.f,scale); ui::Checkbox("Swap Back",&settings.swapBack);
                const float halfW=(pageW-40.f-12.f)*0.5f;
                const float rightX=pageX+32.f+halfW;
                drawSlider(dl,origin,"automace-range","Range (blocks)","",pageX+20.f,cardY+180.f,halfW,scale,opacity,settings.range,1.f,6.f);
                drawIntSlider(dl,origin,"automace-hit-min","Hit delay min (ticks)",pageX+20.f,cardY+217.f,halfW,scale,opacity,settings.hitDelayMinTicks,0,20);
                drawIntSlider(dl,origin,"automace-hit-max","Hit delay max (ticks)",pageX+20.f,cardY+254.f,halfW,scale,opacity,settings.hitDelayMaxTicks,0,20);
                drawIntSlider(dl,origin,"automace-cool-min","Cooldown min (ticks)",pageX+20.f,cardY+291.f,halfW,scale,opacity,settings.cooldownMinTicks,0,40);
                drawIntSlider(dl,origin,"automace-cool-max","Cooldown max (ticks)",pageX+20.f,cardY+328.f,halfW,scale,opacity,settings.cooldownMaxTicks,0,40);
                drawIntSlider(dl,origin,"automace-reswap-min","Reswap min (ticks)",rightX,cardY+180.f,halfW,scale,opacity,settings.reswapMinTicks,0,40);
                drawIntSlider(dl,origin,"automace-reswap-max","Reswap max (ticks)",rightX,cardY+217.f,halfW,scale,opacity,settings.reswapMaxTicks,0,40);
                drawIntSlider(dl,origin,"automace-axe-ticks","Axe ticks",rightX,cardY+254.f,halfW,scale,opacity,settings.axeTicks,0,20);
                drawIntSlider(dl,origin,"automace-mace-ticks","Mace ticks",rightX,cardY+291.f,halfW,scale,opacity,settings.maceTicks,0,20);
                drawIntSlider(dl,origin,"automace-swap-ticks","Swap ticks",rightX,cardY+328.f,halfW,scale,opacity,settings.swapTicks,0,20);
                drawSlider(dl,origin,"automace-fall-height","Minimum fall height (blocks)","",pageX+20.f,cardY+365.f,pageW-40.f,scale,opacity,settings.minFallHeight,0.f,20.f);
                ImGui::PopID();
                drawText(dl,origin,pageX+20.f,cardY+402.f,"It only arms once the fall is at least this tall; a landing cancels the smash.",kDim,8.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+424.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Auto Shield Break") {
                modules::AutoShieldBreakSettings& settings=modules::autoShieldBreakSettings();
                ImGui::PushID("auto-shield-break-options");
                setWidgetCursor(pageX+20.f,cardY+77.f,scale); ui::Checkbox("Only when looking",&settings.onlyWhenLooking);
                setWidgetCursor(pageX+20.f,cardY+107.f,scale); ImGui::SetNextItemWidth(195.f*scale);
                const char* mode=settings.mode==modules::ShieldBreakMode::Auto?"Auto":"OnHit";
                if (ui::BeginCombo("Mode",mode)) {
                    if (ImGui::Selectable("Auto",settings.mode==modules::ShieldBreakMode::Auto)) settings.mode=modules::ShieldBreakMode::Auto;
                    if (ImGui::Selectable("OnHit",settings.mode==modules::ShieldBreakMode::OnHit)) settings.mode=modules::ShieldBreakMode::OnHit;
                    ui::EndCombo();
                }
                setWidgetCursor(pageX+20.f,cardY+147.f,scale); ui::Checkbox("Swap Back",&settings.swapBack);
                setWidgetCursor(pageX+20.f,cardY+177.f,scale); ui::Checkbox("Sword Only",&settings.swordOnly);
                drawIntSlider(dl,origin,"shieldbreak-swap-delay","Swap delay (ms)",pageX+20.f,cardY+214.f,pageW-40.f,scale,opacity,settings.swapDelayMs,0,600);
                drawIntSlider(dl,origin,"shieldbreak-retry-delay","Retry delay (ms)",pageX+20.f,cardY+251.f,pageW-40.f,scale,opacity,settings.retryDelayMs,1,600);
                ImGui::PopID();
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+310.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Auto Drain") {
                modules::AutoBucketDrainSettings& settings=modules::autoBucketDrainSettings();
                ImGui::PushID("auto-drain-options");
                drawText(dl,origin,pageX+20.f,cardY+50.f,"MODE",kDim,8.f,scale,opacity,true);
                setWidgetCursor(pageX+20.f,cardY+66.f,scale);
                ImGui::PushID("drain-mode");
                ImGui::SetNextItemWidth(196.f*scale);
                const char* drainModeName=settings.mode==modules::DrainMode::Web?"Web Draining":"Bucket Draining";
                if (ui::BeginCombo("##mode",drainModeName)) {
                    if (ImGui::Selectable("Bucket Draining",settings.mode==modules::DrainMode::Bucket))
                        settings.mode=modules::DrainMode::Bucket;
                    if (ImGui::Selectable("Web Draining",settings.mode==modules::DrainMode::Web))
                        settings.mode=modules::DrainMode::Web;
                    ui::EndCombo();
                }
                ImGui::PopID();
                if (settings.mode==modules::DrainMode::Bucket) {
                    setWidgetCursor(pageX+20.f,cardY+112.f,scale); ui::Checkbox("Ignore Own Water",&settings.ignoreOwnWater);
                    setWidgetCursor(pageX+20.f,cardY+142.f,scale); ui::Checkbox("Swap Back",&settings.swapBack);
                    drawIntSlider(dl,origin,"autodrain-bucket-swap-delay","Swap delay (ms)",pageX+20.f,cardY+179.f,pageW-40.f,scale,opacity,settings.swapDelayMs,0,600);
                    setWidgetCursor(pageX+20.f,cardY+216.f,scale); ui::Checkbox("Silent Swap (experimental)",&settings.silentSwap);
                    setWidgetCursor(pageX+20.f,cardY+246.f,scale); ui::Checkbox("Use Offhand Bucket",&settings.useOffhand);
                } else {
                    setWidgetCursor(pageX+20.f,cardY+112.f,scale); ui::Checkbox("Prefer Buckets If No Webs",&settings.webPreferBuckets);
                    setWidgetCursor(pageX+20.f,cardY+142.f,scale); ui::Checkbox("Ignore Own Water",&settings.webIgnoreOwnWater);
                    setWidgetCursor(pageX+20.f,cardY+172.f,scale); ui::Checkbox("SwapBack",&settings.webSwapBack);
                    setWidgetCursor(pageX+20.f,cardY+202.f,scale); ui::Checkbox("Use Offhand Web",&settings.webUseOffhand);
                    drawIntSlider(dl,origin,"autodrain-web-swap-delay","Swap Delay (MS)",pageX+20.f,cardY+239.f,pageW-40.f,scale,opacity,settings.webSwapDelayMs,0,600);
                    setWidgetCursor(pageX+20.f,cardY+276.f,scale); ui::Checkbox("Silent Swap",&settings.webSilentSwap);
                    setWidgetCursor(pageX+20.f,cardY+306.f,scale); ui::Checkbox("Silent Place",&settings.webSilentPlace);
                    drawText(dl,origin,pageX+20.f,cardY+342.f,"Look at the top face of a block with water above it, or at the water itself, to bury it.",kDim,8.f,scale,opacity);
                    drawText(dl,origin,pageX+20.f,cardY+358.f,"Silent Place sends the placement rotation and puts your camera straight back.",kDim,8.f,scale,opacity);
                }
                ImGui::PopID();
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,settings.mode==modules::DrainMode::Bucket?cardY+300.f:cardY+386.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Auto Pressure Plate Web") {
                modules::AutoPressurePlateWebSettings& settings=modules::autoPressurePlateWebSettings();
                ImGui::PushID("pressure-plate-web-options");
                setWidgetCursor(pageX+20.f,cardY+77.f,scale); ui::Checkbox("Swap Back",&settings.swapBack);
                drawIntSlider(dl,origin,"platedrain-swap-delay","Swap delay (ms)",pageX+20.f,cardY+114.f,pageW-40.f,scale,opacity,settings.swapDelayMs,0,600);
                setWidgetCursor(pageX+20.f,cardY+151.f,scale); ui::Checkbox("Silent Swap (experimental)",&settings.silentSwap);
                setWidgetCursor(pageX+20.f,cardY+181.f,scale); ui::Checkbox("Use Offhand Cobweb",&settings.useOffhand);
                setWidgetCursor(pageX+20.f,cardY+211.f,scale); ui::Checkbox("Only If NO Web",&settings.onlyIfNoWeb);
                setWidgetCursor(pageX+20.f,cardY+246.f,scale); ui::Checkbox("Pressure Plate Assist",&settings.plateAssist);
                ImGui::BeginDisabled(!settings.plateAssist);
                drawSlider(dl,origin,"plate-assist-range","Assist Range (blocks)","",pageX+20.f,cardY+283.f,pageW-40.f,scale,opacity,settings.assistRange,1.f,6.f);
                drawSlider(dl,origin,"plate-assist-fov","Assist FOV (degrees)","",pageX+20.f,cardY+320.f,pageW-40.f,scale,opacity,settings.assistFov,10.f,360.f);
                drawSlider(dl,origin,"plate-assist-speed","Assist Speed","",pageX+20.f,cardY+357.f,pageW-40.f,scale,opacity,settings.assistSpeed,1.f,30.f);
                setWidgetCursor(pageX+20.f,cardY+394.f,scale); ui::Checkbox("Assist Vertical",&settings.assistVertical);
                ImGui::EndDisabled();
                ImGui::PopID();
                drawText(dl,origin,pageX+20.f,cardY+432.f,"Pressure Plate Assist pulls the view onto a pressure plate in range the way",kDim,8.f,scale,opacity);
                drawText(dl,origin,pageX+20.f,cardY+448.f,"AimAssist does a target. Only If NO Web leaves a plate that already has a",kDim,8.f,scale,opacity);
                drawText(dl,origin,pageX+20.f,cardY+464.f,"cobweb above it alone.",kDim,8.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+495.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Fast Place") {
                modules::FastPlaceSettings& settings=modules::fastPlaceSettings();
                ImGui::PushID("fast-place-options");
                setWidgetCursor(pageX+20.f,cardY+77.f,scale); ui::Checkbox("Only XP",&settings.onlyXp);
                ImGui::BeginDisabled(settings.onlyXp);
                setWidgetCursor(pageX+20.f,cardY+107.f,scale); ui::Checkbox("Allow Blocks",&settings.allowBlocks);
                setWidgetCursor(pageX+20.f,cardY+137.f,scale); ui::Checkbox("Allow Items",&settings.allowItems);
                ImGui::EndDisabled();
                ImGui::PopID();
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+180.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Sprint") {
                drawText(dl,origin,pageX+20.f,cardY+76.f,"Holds the vanilla sprint key; hunger and sneaking rules still apply.",kDim,9.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+100.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Auto Jump Reset") {
                modules::AutoJumpResetSettings& settings=modules::autoJumpResetSettings();
                ImGui::PushID("auto-jump-reset-options");
                drawIntSlider(dl,origin,"autojump-accuracy","Accuracy (%)",pageX+20.f,cardY+77.f,pageW-40.f,scale,opacity,settings.accuracy,0,100);
                ImGui::PopID();
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+130.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="AimAssist") {
                modules::AimAssistSettings& settings=modules::aimAssistSettings();
                ImGui::PushID("aim-assist-options");
                setWidgetCursor(pageX+20.f,cardY+77.f,scale); ui::Checkbox("Target players",&settings.allowPlayers);
                setWidgetCursor(pageX+20.f,cardY+107.f,scale); ui::Checkbox("Target entities",&settings.allowEntities);
                drawSlider(dl,origin,"aimassist-range","Range (blocks)","",pageX+20.f,cardY+137.f,pageW-40.f,scale,opacity,settings.range,1.f,8.f);
                drawSlider(dl,origin,"aimassist-fov","FOV (degrees)","",pageX+20.f,cardY+174.f,pageW-40.f,scale,opacity,settings.fov,10.f,360.f);
                drawSlider(dl,origin,"aimassist-speed","Speed","",pageX+20.f,cardY+211.f,pageW-40.f,scale,opacity,settings.speed,1.f,40.f);
                setWidgetCursor(pageX+20.f,cardY+248.f,scale); ui::Checkbox("Vertical aim",&settings.vertical);
                setWidgetCursor(pageX+20.f,cardY+278.f,scale); ui::Checkbox("Only while holding attack",&settings.onlyWhileAttacking);
                setWidgetCursor(pageX+20.f,cardY+308.f,scale); ui::Checkbox("Weapon Only (track only while holding a weapon)",&settings.weaponOnly);
                drawText(dl,origin,pageX+20.f,cardY+344.f,"ACCURACY",kDim,8.f,scale,opacity,true);
                setWidgetCursor(pageX+20.f,cardY+360.f,scale);
                ImGui::PushID("aim-accuracy");
                ImGui::SetNextItemWidth(196.f*scale);
                const char* accuracyName=settings.accuracy==modules::AimAccuracy::Head?"Head":
                    (settings.accuracy==modules::AimAccuracy::Loose?"Loose":"Center");
                if (ui::BeginCombo("##accuracy",accuracyName)) {
                    if (ImGui::Selectable("Center",settings.accuracy==modules::AimAccuracy::Center))
                        settings.accuracy=modules::AimAccuracy::Center;
                    if (ImGui::Selectable("Head",settings.accuracy==modules::AimAccuracy::Head))
                        settings.accuracy=modules::AimAccuracy::Head;
                    if (ImGui::Selectable("Loose",settings.accuracy==modules::AimAccuracy::Loose))
                        settings.accuracy=modules::AimAccuracy::Loose;
                    ui::EndCombo();
                }
                ImGui::PopID();
                setWidgetCursor(pageX+20.f,cardY+398.f,scale); ui::Checkbox("Silent AimAssist",&settings.silent);
                setWidgetCursor(pageX+20.f,cardY+428.f,scale); ui::Checkbox("No Glide (off while an elytra is gliding)",&settings.noGlide);
                ImGui::PopID();
                drawText(dl,origin,pageX+20.f,cardY+460.f,"Center aims at the body, Head at the head, Loose anywhere in the hitbox.",kDim,8.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+484.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Anti-Bot") {
                modules::AntiBotSettings& settings=modules::antiBotSettings();
                ImGui::PushID("anti-bot-options");
                setWidgetCursor(pageX+20.f,cardY+77.f,scale); ui::Checkbox("Detect by name",&settings.checkNames);
                setWidgetCursor(pageX+20.f,cardY+107.f,scale); ui::Checkbox("Detect by UUID",&settings.checkUuid);
                setWidgetCursor(pageX+20.f,cardY+137.f,scale); ui::Checkbox("Allow offline UUIDs",&settings.allowOfflineUuid);
                setWidgetCursor(pageX+20.f,cardY+167.f,scale); ui::Checkbox("Detect NPC clones",&settings.checkClones);
                ImGui::PopID();
                drawText(dl,origin,pageX+20.f,cardY+197.f,"Keeps the entity modules off bots and NPCs.",kDim,8.f,scale,opacity);
                drawText(dl,origin,pageX+20.f,cardY+215.f,"Offline UUIDs (v3) are real players; clones reuse a name.",kDim,8.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+233.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Nametags") {
                modules::NametagsSettings& settings=modules::nametagsSettings();
                ImGui::PushID("nametags-options");
                setWidgetCursor(pageX+20.f,cardY+77.f,scale); ui::Checkbox("Show name",&settings.showName);
                setWidgetCursor(pageX+20.f,cardY+107.f,scale); ui::Checkbox("Show health",&settings.showHealth);
                setWidgetCursor(pageX+20.f,cardY+137.f,scale); ui::Checkbox("Show distance",&settings.showDistance);
                setWidgetCursor(pageX+20.f,cardY+167.f,scale); ui::Checkbox("Show armor",&settings.showArmor);
                setWidgetCursor(pageX+20.f,cardY+197.f,scale); ui::Checkbox("Show held item",&settings.showHeldItem);
                setWidgetCursor(pageX+20.f,cardY+227.f,scale); ui::Checkbox("Item icons",&settings.icons);
                setWidgetCursor(pageX+20.f,cardY+257.f,scale); ui::Checkbox("Single Show (only the looked-at entity)",&settings.singleShow);
                drawSlider(dl,origin,"nametags-max-distance","Max distance (blocks)","",pageX+20.f,cardY+297.f,pageW-40.f,scale,opacity,settings.maxDistance,8.f,128.f);
                drawSlider(dl,origin,"nametags-scale","Scale","",pageX+20.f,cardY+334.f,pageW-40.f,scale,opacity,settings.scale,0.5f,2.f);
                ImGui::PopID();
                drawText(dl,origin,pageX+20.f,cardY+368.f,"Icons come from the game's own textures; off uses plain text labels.",kDim,8.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+386.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="StaticFov") {
                modules::StaticFovSettings& settings=modules::staticFovSettings();
                ImGui::PushID("static-fov-options");
                drawSlider(dl,origin,"staticfov-fov","Field of view (degrees)","",pageX+20.f,cardY+77.f,pageW-40.f,scale,opacity,settings.fov,30.f,179.f);
                ImGui::PopID();
                drawText(dl,origin,pageX+20.f,cardY+110.f,"Applies above the vanilla 30-110 slider limit.",kDim,8.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+130.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Auto Totem") {
                modules::AutoTotemSettings& settings=modules::autoTotemSettings();
                ImGui::PushID("auto-totem-options");
                drawText(dl,origin,pageX+20.f,cardY+76.f,"TYPE",kDim,8.f,scale,opacity,true);
                setWidgetCursor(pageX+20.f,cardY+92.f,scale);
                ImGui::PushID("autototem-type");
                ImGui::SetNextItemWidth(196.f*scale);
                const char* totemType=settings.type==modules::TotemType::SMP?"SMP":"Vanilla / Crystal";
                if (ui::BeginCombo("##type",totemType)) {
                    if (ImGui::Selectable("Vanilla / Crystal",settings.type==modules::TotemType::Vanilla))
                        settings.type=modules::TotemType::Vanilla;
                    if (ImGui::Selectable("SMP",settings.type==modules::TotemType::SMP))
                        settings.type=modules::TotemType::SMP;
                    ui::EndCombo();
                }
                ImGui::PopID();
                if (settings.type==modules::TotemType::SMP) {
                    drawSlider(dl,origin,"autototem-smp-hearts","Health Threshold (hearts)","",pageX+20.f,cardY+130.f,pageW-40.f,scale,opacity,settings.smpHealthHearts,0.5f,10.f);
                    drawIntSlider(dl,origin,"autototem-smp-delay","Swap Delay (ms)",pageX+20.f,cardY+167.f,pageW-40.f,scale,opacity,settings.smpSwapDelayMs,0,600);
                    drawIntSlider(dl,origin,"autototem-smp-shield","Shield slot (1-9)",pageX+20.f,cardY+204.f,pageW-40.f,scale,opacity,settings.smpShieldSlot,1,9);
                    setWidgetCursor(pageX+20.f,cardY+241.f,scale); ui::Checkbox("Silent",&settings.silent);
                    ImGui::PopID();
                    drawText(dl,origin,pageX+20.f,cardY+270.f,"Below the threshold a totem is held; above it the shield goes back. Vanilla keeps the totem always.",kDim,8.f,scale,opacity);
                    drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+294.f,pageW-40.f,scale,opacity);
                } else {
                    setWidgetCursor(pageX+20.f,cardY+128.f,scale); ui::Checkbox("Refill off hand",&settings.refillOffhand);
                    setWidgetCursor(pageX+20.f,cardY+158.f,scale); ui::Checkbox("Totem Refill slot",&settings.refillHotbarSlot);
                    ImGui::BeginDisabled(!settings.refillHotbarSlot);
                    drawIntSlider(dl,origin,"autototem-refill-slot","Refill slot (1-9)",pageX+20.f,cardY+195.f,pageW-40.f,scale,opacity,settings.hotbarSlot,1,9);
                    ImGui::EndDisabled();
                    drawIntSlider(dl,origin,"autototem-delay","Delay (ms)",pageX+20.f,cardY+232.f,pageW-40.f,scale,opacity,settings.delayMs,0,600);
                    setWidgetCursor(pageX+20.f,cardY+269.f,scale); ui::Checkbox("Silent",&settings.silent);
                    ImGui::PopID();
                    drawText(dl,origin,pageX+20.f,cardY+298.f,"Off: the inventory is opened and the totem is moved while you watch.",kDim,8.f,scale,opacity);
                    drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+322.f,pageW-40.f,scale,opacity);
                }
            } else if (expanded && std::string(entry->name)=="AutoClean") {
                modules::AutoCleanSettings& settings=modules::autoCleanSettings();
                ImGui::PushID("autoclean-options");
                setWidgetCursor(pageX+20.f,cardY+77.f,scale); ui::Checkbox("Silent Clean (off = inventory is shown)",&settings.silentClean);
                drawIntSlider(dl,origin,"autoclean-scan-delay","Scan Delay (ms)",pageX+20.f,cardY+114.f,pageW-40.f,scale,opacity,settings.scanDelayMs,0,600);
                setWidgetCursor(pageX+20.f,cardY+151.f,scale);
                ImGui::PushID("clean-item");
                ImGui::SetNextItemWidth(210.f*scale);
                ImGui::InputTextWithHint("##clean-item","item id (e.g. dirt)",g_cleanItemName,sizeof(g_cleanItemName));
                ImGui::PopID();
                if (drawButton(dl,origin,"add-clean-item","ADD ITEM",pageX+238.f,cardY+149.f,110.f,24.f,scale,opacity,false,true)) {
                    const std::string cleanId=g_cleanItemName;
                    if (cleanId.empty()) notify("Type an item id first");
                    else if (std::find(settings.customItems.begin(),settings.customItems.end(),cleanId)!=settings.customItems.end())
                        notify("That item is already in the AutoClean list");
                    else { settings.customItems.push_back(cleanId); g_cleanItemName[0]='\0'; notify("Item added to AutoClean list"); }
                }
                drawText(dl,origin,pageX+20.f,cardY+182.f,"CUSTOM ITEMS",kDim,8.f,scale,opacity,true);
                if (settings.customItems.empty())
                    drawText(dl,origin,pageX+20.f,cardY+198.f,"None - only the built-in junk list is dropped.",kDim,8.f,scale,opacity);
                int shownItems=0;
                for (size_t i=0;i<settings.customItems.size()&&shownItems<5;++i) {
                    const float ry=cardY+198.f+shownItems*26.f;
                    drawText(dl,origin,pageX+20.f,ry+6.f,settings.customItems[i].c_str(),kMuted,9.f,scale,opacity);
                    const std::string removeId=std::string("clean-remove-")+std::to_string(i);
                    if (drawButton(dl,origin,removeId.c_str(),"X",pageX+200.f,ry,22.f,22.f,scale,opacity,false,true)) {
                        settings.customItems.erase(settings.customItems.begin()+(long)i);
                        break;
                    }
                    ++shownItems;
                }
                ImGui::PopID();
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+320.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Trajectory Preview") {
                drawText(dl,origin,pageX+20.f,cardY+77.f,"Draws the 3D flight path of the held projectile as a ribbon, and outlines",kDim,9.f,scale,opacity);
                drawText(dl,origin,pageX+20.f,cardY+97.f,"the block it will land on in yellow.",kDim,9.f,scale,opacity);
                drawText(dl,origin,pageX+20.f,cardY+117.f,"Ender pearls, eggs, snowballs, potions and a charged crossbow.",kDim,9.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+144.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Item Scroll") {
                drawText(dl,origin,pageX+20.f,cardY+77.f,"Hold Shift in your inventory and hover an item to auto-click it.",kDim,9.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+100.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Cart Refill") {
                modules::CartRefillSettings& settings=modules::cartRefillSettings();
                ImGui::PushID("cart-refill-options");
                drawText(dl,origin,pageX+20.f,cardY+76.f,"WATCHED HOTBAR SLOTS",kDim,8.f,scale,opacity,true);
                for (int i=0;i<9;++i) {
                    const float cx=pageX+20.f+(float)(i%3)*150.f;
                    const float cy=cardY+96.f+(float)(i/3)*30.f;
                    setWidgetCursor(cx,cy,scale);
                    ImGui::PushID(i);
                    ui::Checkbox(("Slot "+std::to_string(i+1)).c_str(),&settings.slots[i]);
                    ImGui::PopID();
                }
                drawIntSlider(dl,origin,"cartrefill-delay","Refill Delay (ms)",pageX+20.f,cardY+200.f,pageW-40.f,scale,opacity,settings.delayMs,0,600);
                setWidgetCursor(pageX+20.f,cardY+237.f,scale); ui::Checkbox("Silent Refill (off = opens the inventory)",&settings.silent);
                ImGui::PopID();
                drawText(dl,origin,pageX+20.f,cardY+268.f,"Stocks watched slots; waits for the Xbow Macro to finish before refilling.",kDim,8.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+285.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Pearl Agro") {
                modules::PearlAgroSettings& settings=modules::pearlAgroSettings();
                ImGui::PushID("pearl-agro-options");
                drawIntSlider(dl,origin,"pearlagro-delay","Agro Delay (ms)",pageX+20.f,cardY+77.f,pageW-40.f,scale,opacity,settings.agroDelayMs,0,600);
                drawSlider(dl,origin,"pearlagro-fov","FOV","degrees",pageX+20.f,cardY+114.f,pageW-40.f,scale,opacity,settings.fov,10.f,360.f);
                setWidgetCursor(pageX+20.f,cardY+151.f,scale); ui::Checkbox("Silent Aim",&settings.silentAim);
                setWidgetCursor(pageX+20.f,cardY+181.f,scale);
                {
                    const char* looks[]={"Instant","Smooth"};
                    const char* look=looks[(int)settings.look];
                    ImGui::SetNextItemWidth((pageW-40.f)*scale);
                    if (ui::BeginCombo("##pearlagro-look",look)) {
                        for (int i=0;i<2;++i)
                            if (ImGui::Selectable(looks[i],(int)settings.look==i))
                                settings.look=(modules::WebLookMode)i;
                        ui::EndCombo();
                    }
                }
                ImGui::BeginDisabled(settings.look!=modules::WebLookMode::Smooth);
                drawSlider(dl,origin,"pearlagro-sens","Camera Sensitivity","",pageX+20.f,cardY+218.f,pageW-40.f,scale,opacity,settings.cameraSensitivity,0.f,50.f);
                ImGui::EndDisabled();
                ImGui::PopID();
                drawText(dl,origin,pageX+20.f,cardY+257.f,"A target's pearl sets the spot; your pearl is thrown to land right on them.",kDim,8.f,scale,opacity);
                drawText(dl,origin,pageX+20.f,cardY+273.f,"FOV: they also have to be inside this cone in front of you, so agro only",kDim,8.f,scale,opacity);
                drawText(dl,origin,pageX+20.f,cardY+289.f,"answers somebody you are facing.",kDim,8.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+322.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Shader ESP") {
                modules::ShaderEspSettings& settings=modules::shaderEspSettings();
                ImGui::PushID("shader-esp-options");
                // Glow style: the game's own outline around the rendered model, or the
                // glow Passive draws (which is the only one that can carry a colour).
                drawText(dl,origin,pageX+20.f,cardY+77.f,"GLOW STYLE",kDim,8.f,scale,opacity,true);
                setWidgetCursor(pageX+20.f,cardY+96.f,scale);
                ImGui::PushID("shaderesp-style");
                ImGui::SetNextItemWidth(pageW-40.f);
                if (ui::BeginCombo("##style",kShaderGlowStyleNames[(int)settings.style])) {
                    for (int i=0;i<2;++i) {
                        if (ImGui::Selectable(kShaderGlowStyleNames[i],(int)settings.style==i) &&
                            (int)settings.style!=i) {
                            settings.style=(modules::ShaderGlowStyle)i;
                            notify((std::string("Shader ESP: ")+kShaderGlowStyleNames[i]).c_str());
                        }
                    }
                    ui::EndCombo();
                }
                ImGui::PopID();
                float noteRow=0.f;
                if (settings.style==modules::ShaderGlowStyle::Vanilla) {
                    // Distance, then the item in your own hand: the game never outlines
                    // it, so Passive draws it from the item's texture (nudge it if your
                    // hand settings sit differently).
                    drawSlider(dl,origin,"shaderesp-distance","Max distance (blocks)","",pageX+20.f,cardY+137.f,pageW-40.f,scale,opacity,settings.maxDistance,8.f,128.f);
                    setWidgetCursor(pageX+20.f,cardY+174.f,scale);
                    ui::Checkbox("Glow the item in your hand",&settings.handGlow);
                    drawIntSlider(dl,origin,"shaderesp-hand-size","Hand Glow Size (%)",pageX+20.f,cardY+211.f,pageW-40.f,scale,opacity,settings.handSize,5,60);
                    drawIntSlider(dl,origin,"shaderesp-hand-x","Hand Glow Nudge X (%)",pageX+20.f,cardY+248.f,pageW-40.f,scale,opacity,settings.handOffsetX,-30,30);
                    drawIntSlider(dl,origin,"shaderesp-hand-y","Hand Glow Nudge Y (%)",pageX+20.f,cardY+285.f,pageW-40.f,scale,opacity,settings.handOffsetY,-30,30);
                    drawIntSlider(dl,origin,"shaderesp-hand-rot","Hand Glow Rotation (deg)",pageX+20.f,cardY+322.f,pageW-40.f,scale,opacity,settings.handRotation,-90,90);
                    drawText(dl,origin,pageX+20.f,cardY+364.f,"Minecraft draws the outline around the model itself: skin, armour, the held item and",kDim,8.f,scale,opacity);
                    drawText(dl,origin,pageX+20.f,cardY+380.f,"dropped item models, white at its own width. The glow in your own hand is drawn by Passive.",kDim,8.f,scale,opacity);
                    noteRow=406.f;
                } else {
                    drawIntSlider(dl,origin,"shaderesp-thickness","Shader Thickness (1-5)",pageX+20.f,cardY+137.f,pageW-40.f,scale,opacity,settings.thickness,1,5);
                    drawSlider(dl,origin,"shaderesp-opacity","Shader Opacity","%",pageX+20.f,cardY+174.f,pageW-40.f,scale,opacity,settings.opacity,0.f,1.f);
                    drawSlider(dl,origin,"shaderesp-distance","Max distance (blocks)","",pageX+20.f,cardY+211.f,pageW-40.f,scale,opacity,settings.maxDistance,8.f,128.f);
                    drawText(dl,origin,pageX+20.f,cardY+256.f,"ESP COLOR",kDim,8.f,scale,opacity,true);
                    setWidgetCursor(pageX+20.f,cardY+272.f,scale);
                    ImGui::PushID("shaderesp-color");
                    ImGui::SetNextItemWidth(205.f*scale);
                    ImGui::ColorPicker4("##color",&settings.color.x,
                        ImGuiColorEditFlags_PickerHueBar|ImGuiColorEditFlags_NoSidePreview|
                        ImGuiColorEditFlags_NoSmallPreview|ImGuiColorEditFlags_NoInputs|
                        ImGuiColorEditFlags_NoOptions|ImGuiColorEditFlags_AlphaBar);
                    ImGui::PopID();
                    drawText(dl,origin,pageX+240.f,cardY+256.f,"FRIEND ESP COLOR",kDim,8.f,scale,opacity,true);
                    setWidgetCursor(pageX+240.f,cardY+272.f,scale);
                    ImGui::PushID("shaderesp-friend-color");
                    ImGui::SetNextItemWidth(205.f*scale);
                    ImGui::ColorPicker4("##friendcolor",&settings.friendColor.x,
                        ImGuiColorEditFlags_PickerHueBar|ImGuiColorEditFlags_NoSidePreview|
                        ImGuiColorEditFlags_NoSmallPreview|ImGuiColorEditFlags_NoInputs|
                        ImGuiColorEditFlags_NoOptions|ImGuiColorEditFlags_AlphaBar);
                    ImGui::PopID();
                    drawText(dl,origin,pageX+20.f,cardY+508.f,"A drawn glow of Shader Thickness passes around every player and dropped item, at Shader",kDim,8.f,scale,opacity);
                    drawText(dl,origin,pageX+20.f,cardY+524.f,"Opacity; friends take their own colour. Vanilla uses the game's outline instead.",kDim,8.f,scale,opacity);
                    noteRow=550.f;
                }
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+noteRow,pageW-40.f,scale,opacity);
                ImGui::PopID();
            } else if (expanded && std::string(entry->name)=="Drag Click Friend") {
                modules::DragClickSettings& settings=modules::dragClickSettings();
                ImGui::PushID("drag-click-options");
                drawIntSlider(dl,origin,"dragclick-min-cps","Minimum CPS (1-40)",pageX+20.f,cardY+77.f,pageW-40.f,scale,opacity,settings.minCps,1,40);
                drawIntSlider(dl,origin,"dragclick-max-cps","Maximum CPS (1-40)",pageX+20.f,cardY+114.f,pageW-40.f,scale,opacity,settings.maxCps,1,40);
                drawIntSlider(dl,origin,"dragclick-acceleration","Acceleration Speed (1-10)",pageX+20.f,cardY+151.f,pageW-40.f,scale,opacity,settings.acceleration,1,10);
                drawSlider(dl,origin,"dragclick-pause","Drag Click Pause (seconds)","s",pageX+20.f,cardY+188.f,pageW-40.f,scale,opacity,settings.pauseSeconds,0.f,3.f);
                ImGui::PopID();
                drawText(dl,origin,pageX+20.f,cardY+222.f,"Hold attack: the CPS ramps from Minimum to Maximum while the button stays down.",kDim,8.f,scale,opacity);
                drawText(dl,origin,pageX+20.f,cardY+238.f,"A pause stops the clicking for that long, then the ramp starts again.",kDim,8.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+262.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Stun Web") {
                modules::StunWebSettings& settings=modules::stunWebSettings();
                ImGui::PushID("stun-web-options");
                drawText(dl,origin,pageX+20.f,cardY+77.f,"WEB LOOK",kDim,8.f,scale,opacity,true);
                setWidgetCursor(pageX+20.f,cardY+93.f,scale);
                ImGui::PushID("stunweb-look");
                ImGui::SetNextItemWidth(196.f*scale);
                const char* stunLook=settings.look==modules::WebLookMode::Smooth?"Smooth":"Instant";
                if (ui::BeginCombo("##look",stunLook)) {
                    if (ImGui::Selectable("Instant",settings.look==modules::WebLookMode::Instant))
                        settings.look=modules::WebLookMode::Instant;
                    if (ImGui::Selectable("Smooth",settings.look==modules::WebLookMode::Smooth))
                        settings.look=modules::WebLookMode::Smooth;
                    ui::EndCombo();
                }
                ImGui::PopID();
                // The slider only exists for the Smooth look (Instant snaps there in one frame);
                // Silent Place keeps the camera still and Look Back returns it afterwards, so the
                // rows are pinned and the dim line explains the slider when it is not in use.
                if (settings.look==modules::WebLookMode::Smooth)
                    drawSlider(dl,origin,"stunweb-camsens","Camera Sensitivity","",pageX+20.f,cardY+129.f,pageW-40.f,scale,opacity,settings.cameraSensitivity,0.f,50.f);
                else
                    drawText(dl,origin,pageX+20.f,cardY+133.f,"Camera Sensitivity is used by the Smooth look.",kDim,8.f,scale,opacity);
                setWidgetCursor(pageX+20.f,cardY+166.f,scale); ui::Checkbox("Silent Place",&settings.silentPlace);
                setWidgetCursor(pageX+20.f,cardY+194.f,scale); ui::Checkbox("Look Back",&settings.lookBack);
                setWidgetCursor(pageX+20.f,cardY+222.f,scale); ui::Checkbox("Players only",&settings.allowPlayers);
                drawIntSlider(dl,origin,"stunweb-place-delay","Cobweb Place Delay (ms)",pageX+20.f,cardY+250.f,pageW-40.f,scale,opacity,settings.placeDelayMs,0,600);
                ImGui::PopID();
                drawText(dl,origin,pageX+20.f,cardY+287.f,"Add-on to Auto Shield Break: after the axe stun lands, a second click goes out and a",kDim,8.f,scale,opacity);
                drawText(dl,origin,pageX+20.f,cardY+303.f,"cobweb is placed under the target (needs a cobweb in the hotbar and block reach).",kDim,8.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+334.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Logout Spots") {
                modules::LogoutSpotsSettings& settings=modules::logoutSpotsSettings();
                ImGui::PushID("logout-spots-options");
                drawText(dl,origin,pageX+20.f,cardY+76.f,"LOGOUT COLOR",kDim,8.f,scale,opacity,true);
                setWidgetCursor(pageX+20.f,cardY+92.f,scale);
                ImGui::PushID("logout-color");
                ImGui::SetNextItemWidth(205.f*scale);
                ImGui::ColorPicker4("##logoutcolor",&settings.color.x,
                    ImGuiColorEditFlags_PickerHueBar|ImGuiColorEditFlags_NoSidePreview|
                    ImGuiColorEditFlags_NoSmallPreview|ImGuiColorEditFlags_NoInputs|
                    ImGuiColorEditFlags_NoOptions|ImGuiColorEditFlags_AlphaBar);
                ImGui::PopID();
                drawSlider(dl,origin,"logoutspots-max-distance","Max distance (blocks)","",pageX+20.f,cardY+330.f,pageW-40.f,scale,opacity,settings.maxDistance,8.f,512.f);
                setWidgetCursor(pageX+20.f,cardY+367.f,scale); ui::Checkbox("Only players",&settings.onlyPlayers);
                ImGui::PopID();
                drawText(dl,origin,pageX+20.f,cardY+398.f,"A box is left where a player stood when they left the world, with their name over it.",kDim,8.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+422.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Player Model") {
                // ---- Clarity cosmetics: these five drive the jar, not the client ----------------
                modules::PlayerModelSettings& settings=modules::playerModelSettings();
                ImGui::PushID("playermodel-options");
                drawSlider(dl,origin,"playermodel-scale","Model Scale","",pageX+20.f,cardY+77.f,pageW-40.f,scale,opacity,settings.scale,0.5f,2.0f);
                setWidgetCursor(pageX+20.f,cardY+120.f,scale);
                ui::Checkbox("Through Walls",&settings.throughWalls);
                setWidgetCursor(pageX+20.f,cardY+148.f,scale);
                ui::Checkbox("Show Timing Overlay",&settings.showTiming);
                ImGui::PopID();
                drawText(dl,origin,pageX+20.f,cardY+180.f,"Drawn by the Clarity jar (clarity-1.2.0.jar beside the client).",kDim,8.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+200.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Wings") {
                modules::WingsSettings& settings=modules::wingsSettings();
                ImGui::PushID("wings-options");
                drawText(dl,origin,pageX+20.f,cardY+77.f,"WING STYLE",kDim,8.f,scale,opacity,true);
                setWidgetCursor(pageX+20.f,cardY+93.f,scale);
                ImGui::PushID("wings-style-combo");
                ImGui::SetNextItemWidth(200.f*scale);
                const char* wingNames[] = { "Angel", "Dragon", "Demon", "Butterfly", "Crystal", "Feathered" };
                int currentStyle = (int)settings.style;
                if (ui::BeginCombo("##style", wingNames[currentStyle])) {
                    for (int i = 0; i < 6; ++i) {
                        if (ImGui::Selectable(wingNames[i], currentStyle == i)) settings.style = (modules::WingStyle)i;
                    }
                    ui::EndCombo();
                }
                ImGui::PopID();
                drawSlider(dl,origin,"wings-scale","Wing Scale","",pageX+20.f,cardY+135.f,pageW-40.f,scale,opacity,settings.scale,0.5f,2.0f);
                drawSlider(dl,origin,"wings-opacity","Opacity","%",pageX+20.f,cardY+172.f,pageW-40.f,scale,opacity,settings.opacity,0.1f,1.0f);
                drawSlider(dl,origin,"wings-flapspeed","Flap Speed","",pageX+20.f,cardY+209.f,pageW-40.f,scale,opacity,settings.flapSpeed,0.5f,3.0f);
                drawSlider(dl,origin,"wings-yoffset","Y Offset","",pageX+20.f,cardY+246.f,pageW-40.f,scale,opacity,settings.yOffset,-1.0f,1.0f);
                setWidgetCursor(pageX+20.f,cardY+290.f,scale);
                ui::Checkbox("Animate Flapping",&settings.animate);
                setWidgetCursor(pageX+20.f,cardY+318.f,scale);
                ui::Checkbox("Through Walls",&settings.throughWalls);
                ImGui::PopID();
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+355.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Headwear") {
                modules::HeadwearSettings& settings=modules::headwearSettings();
                ImGui::PushID("headwear-options");
                drawText(dl,origin,pageX+20.f,cardY+77.f,"HEADWEAR STYLE",kDim,8.f,scale,opacity,true);
                setWidgetCursor(pageX+20.f,cardY+93.f,scale);
                ImGui::PushID("headwear-style-combo");
                ImGui::SetNextItemWidth(200.f*scale);
                const char* headwearNames[] = { "Flame Crown", "Floating Crown", "Fox Ears", "Ice Crown", "Laurel Wreath", "Orbiting Orbs", "Reindeer Antlers", "Royal Crown", "Runic Halo", "Spiked Crown", "Stag Antlers", "Thorn Halo", "Tiara", "Unicorn Horn", "Viking Horns" };
                int currentHw = (int)settings.style;
                if (ui::BeginCombo("##hwstyle", headwearNames[currentHw])) {
                    for (int i = 0; i < 15; ++i) {
                        if (ImGui::Selectable(headwearNames[i], currentHw == i)) settings.style = (modules::HeadwearStyle)i;
                    }
                    ui::EndCombo();
                }
                ImGui::PopID();
                drawSlider(dl,origin,"headwear-scale","Cosmetic Scale","",pageX+20.f,cardY+135.f,pageW-40.f,scale,opacity,settings.scale,0.5f,2.0f);
                drawSlider(dl,origin,"headwear-yoffset","Y Offset","",pageX+20.f,cardY+172.f,pageW-40.f,scale,opacity,settings.yOffset,-0.5f,0.5f);
                drawSlider(dl,origin,"headwear-rotspeed","Rotation Speed","",pageX+20.f,cardY+209.f,pageW-40.f,scale,opacity,settings.rotationSpeed,0.5f,3.0f);
                setWidgetCursor(pageX+20.f,cardY+252.f,scale);
                ui::Checkbox("Through Walls",&settings.throughWalls);
                ImGui::PopID();
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+290.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Capes") {
                modules::CapesSettings& settings=modules::capesSettings();
                ImGui::PushID("capes-options");
                drawText(dl,origin,pageX+20.f,cardY+77.f,"CAPE TEXTURE",kDim,8.f,scale,opacity,true);
                setWidgetCursor(pageX+20.f,cardY+93.f,scale);
                ImGui::PushID("capes-style-combo");
                ImGui::SetNextItemWidth(200.f*scale);
                const char* capeNames[] = { "Clarity Aurora", "Clarity Circuit", "Clarity Flow", "Clarity Nebula", "Clarity Prism", "15th Anniversary", "Cherry Blossom", "Cobalt", "Migrator", "Minecon 2011", "Minecon 2016", "Mojang", "Prismarine", "Purple Heart", "Twisted" };
                int currentCape = (int)settings.style;
                if (ui::BeginCombo("##capestyle", capeNames[currentCape])) {
                    for (int i = 0; i < 15; ++i) {
                        if (ImGui::Selectable(capeNames[i], currentCape == i)) settings.style = (modules::CapeStyle)i;
                    }
                    ui::EndCombo();
                }
                ImGui::PopID();
                setWidgetCursor(pageX+20.f,cardY+135.f,scale);
                ui::Checkbox("Cape Physics Wave",&settings.physics);
                setWidgetCursor(pageX+20.f,cardY+163.f,scale);
                ui::Checkbox("Movement Wave Motion",&settings.movementWave);
                ImGui::PopID();
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+195.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="ViewModel") {
                modules::ViewModelSettings& settings=modules::viewModelSettings();
                ImGui::PushID("viewmodel-options");
                drawSlider(dl,origin,"vm-posx","Pos X","",pageX+20.f,cardY+77.f,pageW-40.f,scale,opacity,settings.posX,-1.5f,1.5f);
                drawSlider(dl,origin,"vm-posy","Pos Y","",pageX+20.f,cardY+114.f,pageW-40.f,scale,opacity,settings.posY,-1.5f,1.5f);
                drawSlider(dl,origin,"vm-posz","Pos Z","",pageX+20.f,cardY+151.f,pageW-40.f,scale,opacity,settings.posZ,-1.5f,1.5f);
                drawSlider(dl,origin,"vm-rotx","Rot X","",pageX+20.f,cardY+188.f,pageW-40.f,scale,opacity,settings.rotX,-180.f,180.f);
                drawSlider(dl,origin,"vm-roty","Rot Y","",pageX+20.f,cardY+225.f,pageW-40.f,scale,opacity,settings.rotY,-180.f,180.f);
                drawSlider(dl,origin,"vm-rotz","Rot Z","",pageX+20.f,cardY+262.f,pageW-40.f,scale,opacity,settings.rotZ,-180.f,180.f);
                drawSlider(dl,origin,"vm-scale","Scale","",pageX+20.f,cardY+299.f,pageW-40.f,scale,opacity,settings.scale,0.1f,2.0f);
                setWidgetCursor(pageX+20.f,cardY+336.f,scale); ui::Checkbox("Off Hand",&settings.offHand);
                drawSlider(dl,origin,"vm-opacity","Opacity","%",pageX+20.f,cardY+364.f,pageW-40.f,scale,opacity,settings.opacity,0.1f,1.0f);
                ImGui::PopID();
                drawText(dl,origin,pageX+20.f,cardY+400.f,"Passive draws this first-person model itself; the vanilla hand is left untouched.",kDim,8.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+425.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Walk Trail") {
                modules::WalkTrailSettings& settings=modules::walkTrailSettings();
                ImGui::PushID("walk-trail-options");
                drawText(dl,origin,pageX+20.f,cardY+77.f,"TRAIL COLOR",kDim,8.f,scale,opacity,true);
                setWidgetCursor(pageX+20.f,cardY+93.f,scale);
                ImGui::PushID("walktrail-color");
                ImGui::SetNextItemWidth(205.f*scale);
                ImGui::ColorPicker4("##color",&settings.color.x,
                    ImGuiColorEditFlags_PickerHueBar|ImGuiColorEditFlags_NoSidePreview|
                    ImGuiColorEditFlags_NoSmallPreview|ImGuiColorEditFlags_NoInputs|
                    ImGuiColorEditFlags_NoOptions|ImGuiColorEditFlags_AlphaBar);
                ImGui::PopID();
                drawSlider(dl,origin,"walktrail-life","Trail Length (seconds)","s",pageX+20.f,cardY+175.f,pageW-40.f,scale,opacity,settings.lifetime,0.5f,10.f);
                drawSlider(dl,origin,"walktrail-width","Trail Width (px)","",pageX+20.f,cardY+212.f,pageW-40.f,scale,opacity,settings.width,1.f,8.f);
                drawIntSlider(dl,origin,"walktrail-points","Max Points",pageX+20.f,cardY+249.f,pageW-40.f,scale,opacity,settings.maxPoints,20,600);
                drawSlider(dl,origin,"walktrail-fade","Fade","%",pageX+20.f,cardY+286.f,pageW-40.f,scale,opacity,settings.fade,0.f,1.f);
                setWidgetCursor(pageX+20.f,cardY+330.f,scale); ui::Checkbox("Animated Pulse",&settings.animate);
                setWidgetCursor(pageX+20.f,cardY+358.f,scale); ui::Checkbox("Through Walls",&settings.throughWalls);
                ImGui::PopID();
                drawText(dl,origin,pageX+20.f,cardY+390.f,"A grey trail follows you as you walk and fades out behind you.",kDim,8.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+415.f,pageW-40.f,scale,opacity);
            } else if (expanded && std::string(entry->name)=="Motion Blur") {
                modules::MotionBlurSettings& settings=modules::motionBlurSettings();
                ImGui::PushID("motion-blur-options");
                setWidgetCursor(pageX+20.f,cardY+77.f,scale); ui::Checkbox("Enabled",&settings.enabled);
                setWidgetCursor(pageX+20.f,cardY+107.f,scale);
                {
                    const char* methods[]={"Velocity Based","Frame Blending","Hybrid"};
                    const char* method=methods[(int)settings.method];
                    ImGui::SetNextItemWidth((pageW-40.f)*scale);
                    if (ui::BeginCombo("##motionblur-method",method)) {
                        for (int i=0;i<3;++i)
                            if (ImGui::Selectable(methods[i],(int)settings.method==i))
                                settings.method=(modules::BlurMethod)i;
                        ui::EndCombo();
                    }
                }
                drawSlider(dl,origin,"motionblur-strength","Blur Strength","",pageX+20.f,cardY+144.f,pageW-40.f,scale,opacity,settings.strength,0.1f,3.f);
                setWidgetCursor(pageX+20.f,cardY+181.f,scale); ui::Checkbox("Refresh Rate Scaling",&settings.refreshScaling);
                drawIntSlider(dl,origin,"motionblur-accum","Accumulation Max (frames)",pageX+20.f,cardY+214.f,pageW-40.f,scale,opacity,settings.accumulationMax,1,8);
                drawSlider(dl,origin,"motionblur-mix","Accumulation Mix","",pageX+20.f,cardY+251.f,pageW-40.f,scale,opacity,settings.accumulationMix,0.f,1.f);
                setWidgetCursor(pageX+20.f,cardY+288.f,scale);
                {
                    const char* profiles[]={"Uniform","Weighted"};
                    const char* profile=profiles[(int)settings.profile];
                    ImGui::SetNextItemWidth((pageW-40.f)*scale);
                    if (ui::BeginCombo("##motionblur-profile",profile)) {
                        for (int i=0;i<2;++i)
                            if (ImGui::Selectable(profiles[i],(int)settings.profile==i))
                                settings.profile=(modules::BlurProfile)i;
                        ui::EndCombo();
                    }
                }
                drawSlider(dl,origin,"motionblur-sens","Turn Sensitivity","",pageX+20.f,cardY+325.f,pageW-40.f,scale,opacity,settings.sensitivity,0.2f,3.f);
                ImGui::PopID();
                drawText(dl,origin,pageX+20.f,cardY+360.f,"Natural Motion Blur's feature set, drawn natively: Velocity Based smears along",kDim,8.f,scale,opacity);
                drawText(dl,origin,pageX+20.f,cardY+376.f,"the turn, Frame Blending mixes the older frames, Hybrid does both. Refresh",kDim,8.f,scale,opacity);
                drawText(dl,origin,pageX+20.f,cardY+392.f,"Rate Scaling keeps the trail the same length at 60 and 240 fps.",kDim,8.f,scale,opacity);
                drawModuleKeybind(dl,origin,*entry,pageX+20.f,cardY+418.f,pageW-40.f,scale,opacity);
            }
            if (pass==1) dl->PopClipRect();
            ImGui::PopID();
        }
    }
    }
    ImGui::PopStyleVar(2);
    g_styleCards=false;
}

// The new module configuration page, shared by the library and the Dropdown layout. It replaces
// the old expanded card: a header with the mark, category, tag and a real switch, a left column
// that names what the module is, and a flat options surface on the right. The settings controls
// themselves still come from drawModuleCards (one implementation for every option), with the old
// card chrome switched off.
float drawModuleInspector(ImDrawList* dl, ImVec2 origin, float scale, float opacity,
                          float x, float y, float width, const modules::Entry& entry,
                          bool dropdown, float screenHeight) {
    const float gap = 14.f, headerH = 102.f;
    drawCard(dl, origin, x, y, width, headerH, scale, opacity, true);
    dl->AddRectFilled(screenPos(origin, x + 1.f, y + 1.f, scale),
                      screenPos(origin, x + 4.f, y + headerH - 1.f, scale),
                      alpha(accentColorU32(0.9f), opacity), 2.f * scale);

    int cat = 0;
    for (int i = 0; i < kCategoryCount; ++i)
        if (entry.category == categoryForPage(kCategoryPages[i])) { cat = i; break; }
    const float iconSize = 44.f;
    dl->AddRectFilled(screenPos(origin, x + 18.f, y + 20.f, scale),
                      screenPos(origin, x + 18.f + iconSize, y + 20.f + iconSize, scale),
                      alpha(blend(kCardLight, accentColorU32(), 0.10f), opacity), 10.f * scale);
    dl->AddRect(screenPos(origin, x + 18.f, y + 20.f, scale),
                screenPos(origin, x + 18.f + iconSize, y + 20.f + iconSize, scale),
                alpha(accentColorU32(0.30f), opacity), 10.f * scale, 0, scale);
    drawIcon(dl, screenPos(origin, x + 18.f + iconSize * 0.5f, y + 20.f + iconSize * 0.5f, scale),
             kCategoryIcons[cat], alpha(accentColorU32(), opacity), 0.84f * scale);

    drawText(dl, origin, x + 76.f, y + 17.f, entry.name, kInk, 18.f, scale, opacity, true);
    drawModuleTag(dl, origin, entry, x + 88.f + measureText(entry.name, 18.f, scale, true) / scale,
                  y + 22.f, scale, opacity);
    drawText(dl, origin, x + 76.f, y + 46.f,
             ellipsize(entry.description, 10.f, scale, width - 300.f).c_str(), kMuted, 10.f, scale, opacity);
    char path[72];
    snprintf(path, sizeof(path), "%s  /  %s", kCategoryNames[cat], entry.macro ? "MACRO" : "MODULE");
    drawText(dl, origin, x + 76.f, y + 70.f, path, kDim, 7.5f, scale, opacity, true);

    if (entry.enabled) {
        const bool on = *entry.enabled;
        const float swW = 74.f, swH = 26.f;
        const float swX = x + width - swW - 18.f, swY = y + 20.f;
        const Hit sw = hitBox((std::string("inspector-switch-") + entry.name).c_str(),
                              swX, swY, swW, swH, scale);
        dl->AddRectFilled(screenPos(origin, swX, swY, scale),
                          screenPos(origin, swX + swW, swY + swH, scale),
                          alpha(on ? accentColorU32(0.94f) : blend(kCard, kCardLight, sw.hover * 0.8f), opacity),
                          6.f * scale);
        drawText(dl, origin, swX + 13.f, swY + 7.f, on ? "ENABLED" : "DISABLED",
                 on ? kOnAccent : (sw.hover > 0.2f ? kInk : kMuted), 8.f, scale, opacity, true);
        if (sw.clicked) {
            *entry.enabled = !*entry.enabled;
            modules::notifyModuleToggled(entry.name, *entry.enabled);
        }
    } else {
        drawText(dl, origin, x + width - 92.f, y + 26.f, "MACRO", accentColorU32(), 8.5f,
                 scale, opacity, true);
    }

    const float bodyY = y + headerH + 12.f;
    const float leftW = width >= 640.f ? 208.f : 0.f;
    const float panelX = x + (leftW > 0.f ? leftW + gap : 0.f);
    const float panelW = width - (leftW > 0.f ? leftW + gap : 0.f);
    const float panelH = std::max(240.f, moduleCardHeight(entry.name, true) - 22.f);
    if (leftW > 0.f) {
        drawCard(dl, origin, x, bodyY, leftW, panelH, scale, opacity);
        drawText(dl, origin, x + 16.f, bodyY + 16.f, "MODULE INFO", kDim, 8.f, scale, opacity, true);
        float iy = bodyY + 42.f;
        const auto infoLine = [&](const char* label, const char* value) {
            drawText(dl, origin, x + 16.f, iy, label, kDim, 7.5f, scale, opacity, true);
            drawText(dl, origin, x + 16.f, iy + 15.f, value, kInk, 10.f, scale, opacity, true);
            iy += 44.f;
        };
        infoLine("CATEGORY", kCategoryNames[cat]);
        infoLine("SAFETY", tagName(entry.tag));
        infoLine("TYPE", entry.macro ? "Macro" : "Toggle");
        char keyText[32];
        if (entry.keybind && *entry.keybind != 0) snprintf(keyText, sizeof(keyText), "%s", keyName(*entry.keybind));
        else snprintf(keyText, sizeof(keyText), "UNBOUND");
        infoLine("KEYBIND", keyText);
        dl->AddRectFilled(screenPos(origin, x + 16.f, iy + 2.f, scale),
                          screenPos(origin, x + leftW - 16.f, iy + 3.f, scale),
                          alpha(kLine, opacity * 0.45f), 0.f);
        drawText(dl, origin, x + 16.f, iy + 18.f, "CHANGES SAVE WITH", kDim, 7.f, scale, opacity, true);
        drawText(dl, origin, x + 16.f, iy + 33.f, "YOUR CONFIGURATION PRESET", kMuted, 8.5f, scale, opacity, true);
    }

    drawCard(dl, origin, panelX, bodyY, panelW, panelH, scale, opacity, true);
    drawText(dl, origin, panelX + 20.f, bodyY + 16.f, "CONFIGURATION", kDim, 8.5f, scale, opacity, true);
    drawText(dl, origin, panelX + panelW - 86.f, bodyY + 16.f, "LIVE OPTIONS", kDim, 7.f, scale, opacity, true);
    dl->AddRectFilled(screenPos(origin, panelX + 20.f, bodyY + 33.f, scale),
                      screenPos(origin, panelX + panelW - 20.f, bodyY + 34.f, scale),
                      alpha(kLine, opacity * 0.45f), 0.f);

    const bool wasDropdownCards = g_dropdownCards;
    const bool wasExpanded = g_moduleExpanded[entry.name];
    g_dropdownCards = dropdown;
    g_inspectorCards = true;
    g_moduleExpanded[entry.name] = true;
    float layoutY = bodyY - 33.f;
    int matched = 0, visible = 0;
    // The inspector is a page, not a card that scrolls into view: it must be fully
    // present on the frame it opens, so its card-animation state is seeded here.
    g_moduleVisibility[entry.name] = 1.f;
    g_modulePositions[entry.name] = layoutY;
    g_cfgPanelAnim[entry.name] = 1.f;
    const std::vector<const modules::Entry*> one = { &entry };
    ImGui::PushID((std::string("inspector-") + entry.name).c_str());
    drawModuleCards(dl, origin, scale, opacity, panelX, panelW, one, layoutY, matched, visible);
    ImGui::PopID();
    g_moduleExpanded[entry.name] = wasExpanded;
    g_inspectorCards = false;
    g_dropdownCards = wasDropdownCards;

    const float bottom = bodyY + panelH + 20.f;
    (void)screenHeight;
    ImGui::SetCursorPos(ImVec2(0.f, std::max(0.f, bottom * scale)));
    ImGui::Dummy(ImVec2(1.f, 1.f));
    return bottom;
}

// ---- Change Interface: the Dropdown layout --------------------------------------------------
// The Dropdown client's top bar: one dark pill centred under the title holding Modules, Configs,
// Friends, SelfDestruct and Settings. A tab opens its page when clicked, and dragging it moves it
// along the bar.
const char* const kTopTabNames[5] = { "Modules", "Configs", "Friends", "Client", "Settings" };

// Opens the page a top tab stands for. Modules shows the module columns; SelfDestruct and Settings
// both open the Settings page's General section, where the self-destruct control lives.
void openDropdownTab(int tab) {
    g_dropdownConfigOpen = false;
    g_dropdownConfigName.clear();
    g_dropdownConfigCat = -1;
    if (tab == 0) {
        if (!isCategoryPage(g_page)) g_page = Page::Combat;
    } else if (tab == 1) {
        g_page = Page::Configs;
    } else if (tab == 2) {
        g_page = Page::Settings;
        g_settingsPage = SettingsPage::Friends;
    } else {
        g_page = Page::Settings;
        g_settingsPage = SettingsPage::General;
    }
    g_pageFade = g_animations ? 0.f : 1.f;
}

// Whether a top tab marks the page on screen. SelfDestruct is an action, so it never does.
bool dropdownTabSelected(int tab) {
    if (tab == 0) return isCategoryPage(g_page);
    if (tab == 1) return g_page == Page::Configs;
    if (tab == 2) return g_page == Page::Settings && g_settingsPage == SettingsPage::Friends;
    if (tab == 4) return g_page == Page::Settings && g_settingsPage == SettingsPage::General;
    return false;
}

// Lays a row of items out in `order`: each item's left edge in `xs` and width in `ws`, the row
// centred in `width`. Returns the row's total width.
float layoutDragRow(const std::vector<int>& order, const char* const* names, float width, float scale,
                    std::vector<float>& xs, std::vector<float>& ws) {
    xs.clear();
    ws.clear();
    float total = 0.f;
    for (int id : order) {
        const float w = measureText(names[id], 12.f, scale, true) / scale + 36.f;
        ws.push_back(w);
        total += w;
    }
    float x = (width - total) * 0.5f;
    for (float w : ws) {
        xs.push_back(x);
        x += w;
    }
    return total;
}

// One frame of a drag-to-reorder row. `xs` and `ws` give each item's left edge and width for the
// current order, and the row spans `y` to `y + h`. A press that moves past a few pixels grabs the
// item: it follows the mouse and swaps places with any neighbour its centre reaches. A press
// released without moving is a click. Returns the item clicked this frame, or -1.
int updateDragReorder(DragReorder& d, std::vector<int>& order, const std::vector<float>& xs,
                      const std::vector<float>& ws, float y, float h, ImVec2 origin, float scale) {
    const ImGuiIO& io = ImGui::GetIO();
    const float mx = (io.MousePos.x - origin.x) / scale;
    const float my = (io.MousePos.y - origin.y) / scale;
    const int n = (int)order.size();
    if (d.slot < 0) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            for (int i = 0; i < n; ++i) {
                if (mx >= xs[(size_t)i] && mx < xs[(size_t)i] + ws[(size_t)i] && my >= y && my < y + h) {
                    d.slot = i;
                    d.startX = mx;
                    d.grabOffset = mx - xs[(size_t)i];
                    d.moved = false;
                    break;
                }
            }
        }
        return -1;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const int clicked = d.moved ? -1 : order[(size_t)d.slot];
        d.slot = -1;
        return clicked;
    }
    if (!d.moved && std::fabs(mx - d.startX) > 4.f) d.moved = true;
    if (d.moved) {
        // The grabbed item's centre decides which slot it belongs in.
        const float centre = mx - d.grabOffset + ws[(size_t)d.slot] * 0.5f;
        for (int i = 0; i < n; ++i) {
            if (centre >= xs[(size_t)i] && centre < xs[(size_t)i] + ws[(size_t)i]) {
                if (i != d.slot) {
                    const int id = order[(size_t)d.slot];
                    order.erase(order.begin() + d.slot);
                    order.insert(order.begin() + i, id);
                    d.slot = i;
                }
                break;
            }
        }
    }
    return -1;
}

void drawDropdownTabs(ImDrawList* dl, ImVec2 origin, float width, float topY,
                      float scale, float opacity) {
    // The strip shares the Platinum command bar's row, so the two interfaces differ in what fills
    // that band rather than in where the content begins.
    const float pillY = topY, pillH = 34.f;
    std::vector<float> xs, ws;
    const float total = layoutDragRow(g_topTabOrder, kTopTabNames, width, scale, xs, ws);
    const int clicked = updateDragReorder(g_topTabDrag, g_topTabOrder, xs, ws, pillY, pillH, origin, scale);
    if (clicked >= 0) openDropdownTab(clicked);
    // A reorder changes the slots, so the row is laid out again before it is drawn.
    const float left = (width - layoutDragRow(g_topTabOrder, kTopTabNames, width, scale, xs, ws)) * 0.5f;
    dl->AddRectFilled(screenPos(origin, left - 6.f, pillY, scale),
                      screenPos(origin, left + total + 6.f, pillY + pillH, scale),
                      alpha(IM_COL32(0, 0, 0, 255), opacity), pillH * 0.5f * scale);
    const float mouseX = (ImGui::GetIO().MousePos.x - origin.x) / scale;
    const int dragSlot = (g_topTabDrag.slot >= 0 && g_topTabDrag.moved) ? g_topTabDrag.slot : -1;
    // The dragged tab is drawn last, so it rides above its neighbours.
    for (int pass = 0; pass < 2; ++pass) {
        for (size_t k = 0; k < g_topTabOrder.size(); ++k) {
            const bool isDragged = (int)k == dragSlot;
            if (isDragged != (pass == 1)) continue;
            const int id = g_topTabOrder[k];
            const float tw = ws[k];
            float tx = xs[k];
            if (isDragged) tx = clampf(mouseX - g_topTabDrag.grabOffset, left, left + total - tw);
            const Hit hit = hitBox((std::string("dd-top-") + kTopTabNames[id]).c_str(), tx, pillY, tw, pillH, scale);
            const bool selected = dropdownTabSelected(id);
            const ImVec2 p0 = screenPos(origin, tx + 4.f, pillY + 4.f, scale);
            const ImVec2 p1 = screenPos(origin, tx + tw - 4.f, pillY + pillH - 4.f, scale);
            if (selected) {
                dl->AddRectFilled(p0, p1, alpha(accentColorU32(0.92f), opacity), (pillH - 8.f) * 0.5f * scale);
            } else if (isDragged || hit.hover > 0.01f) {
                const float fade = isDragged ? 1.f : hit.hover;
                dl->AddRectFilled(p0, p1, alpha(IM_COL32(46, 50, 58, 255), opacity * fade),
                                  (pillH - 8.f) * 0.5f * scale);
            }
            drawText(dl, origin, tx + 18.f, pillY + (pillH - 13.f) * 0.5f, kTopTabNames[id],
                     selected ? kOnAccent : kInk, 12.f, scale, opacity, true);
        }
    }
}

// The Dropdown interface's module control. It is deliberately not a switch: the control is a
// rounded colour block with the state written inside it, so on and off are told apart by the fill
// and the label instead of by a knob's position. The fill eases between the two states, hover
// lifts it and a press darkens it, so a click reads back immediately.
bool drawDropdownToggleButton(ImDrawList* dl, ImVec2 origin, const char* id, const modules::Entry& entry,
                              float x, float y, float width, float height,
                              float scale, float opacity, float& anim) {
    const Hit hit = hitBox(id, x, y, width, height, scale);
    const bool on = entry.enabled && *entry.enabled;
    const bool down = hit.hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    anim = g_animations ? ease(anim, on ? 1.f : 0.f, 16.f, ImGui::GetIO().DeltaTime)
                        : (on ? 1.f : 0.f);
    const ImVec2 p0 = screenPos(origin, x, y, scale);
    const ImVec2 p1 = screenPos(origin, x + width, y + height, scale);
    const float rounding = 5.f * scale;
    const ImU32 idle = blend(kCard, kCardLight, 0.42f + 0.46f * hit.hover);
    ImU32 fill = blend(idle, accentColorU32(0.94f), anim);
    if (down) fill = blend(fill, kInk, 0.14f);
    dl->AddRectFilled(p0, p1, alpha(fill, opacity), rounding);
    dl->AddRect(p0, p1, alpha(blend(kLine, accentColorU32(), anim), opacity * (0.62f + 0.38f * anim)),
                rounding, 0, (1.f + 0.5f * anim) * scale);
    const char* label = entry.macro ? "MACRO" : (on ? "ON" : "OFF");
    const float tw = measureText(label, 8.5f, scale, true) / scale;
    drawText(dl, origin, x + (width - tw) * 0.5f, y + (height - 11.f) * 0.5f, label,
             on ? kOnAccent : (entry.macro ? kInk : kMuted), 8.5f, scale, opacity, true);
    if (entry.enabled && hit.hovered)
        ImGui::SetTooltip(on ? "Switch %s off" : "Switch %s on", entry.name);
    return hit.clicked;
}

// One module row inside an unfolded section: the module's name, its tag chip and its toggle
// button. The body opens the module's settings view and the button switches the module, and the
// two are separate hit areas, so reaching for the settings can never flip the module by accident.
void drawDropdownModuleRow(ImDrawList* dl, ImVec2 origin, const modules::Entry& entry,
                           float x, float y, float width, float scale, float opacity, float fade) {
    const float buttonW = 76.f, buttonH = 22.f;
    const float bodyW = std::max(60.f, width - buttonW - 30.f);
    const Hit row = hitBox((std::string("dd-row-") + entry.name).c_str(), x, y, bodyW, kDropdownRowH, scale);
    const ImVec2 p0 = screenPos(origin, x, y, scale);
    const ImVec2 p1 = screenPos(origin, x + width, y + kDropdownRowH, scale);
    const bool on = entry.enabled && *entry.enabled;
    dl->AddRectFilled(p0, p1, alpha(blend(kCard, kCardLight, 0.16f + 0.44f * row.hover),
                                    opacity * fade), 3.f * scale);
    if (row.hover > 0.01f)
        dl->AddRect(p0, p1, alpha(accentColorU32(), opacity * fade * row.hover * 0.55f),
                    3.f * scale, 0, scale);
    drawText(dl, origin, x + 16.f, y + (kDropdownRowH - 13.f) * 0.5f, entry.name,
             on ? kInk : kMuted, 12.5f, scale, opacity * fade, true);
    const float nameW = measureText(entry.name, 12.5f, scale, true) / scale;
    drawModuleTag(dl, origin, entry, x + 26.f + nameW, y + (kDropdownRowH - 14.f) * 0.5f,
                  scale, opacity * fade);
    float& anim = g_dropdownToggleAnim[entry.name];
    if (entry.enabled &&
        drawDropdownToggleButton(dl, origin, (std::string("dd-tog-") + entry.name).c_str(), entry,
                                 x + width - buttonW - 12.f, y + (kDropdownRowH - buttonH) * 0.5f,
                                 buttonW, buttonH, scale, opacity * fade, anim)) {
        const bool wasEnabled = *entry.enabled;
        *entry.enabled = !*entry.enabled;
        if (wasEnabled != *entry.enabled)
            modules::notifyModuleToggled(entry.name, *entry.enabled);
    } else if (!entry.enabled) {
        drawText(dl, origin, x + width - buttonW - 12.f, y + (kDropdownRowH - 11.f) * 0.5f, "MACRO",
                 alpha(accentColorU32(), opacity * fade), 8.5f, scale, opacity * fade, true);
    }
    // A chevron that only shows while the row is aimed at, so the list stays one clean line per
    // module until a settings view is actually being looked for.
    if (entry.enabled && row.hover > 0.2f) {
        const ImVec2 c = screenPos(origin, x + width - buttonW - 28.f, y + kDropdownRowH * 0.5f, scale);
        const float u = scale;
        const ImU32 col = alpha(kMuted, opacity * fade * row.hover);
        dl->AddLine(ImVec2(c.x - 3.5f * u, c.y - 2.f * u), ImVec2(c.x + 1.5f * u, c.y + 3.f * u), col, 1.6f * u);
        dl->AddLine(ImVec2(c.x + 1.5f * u, c.y + 3.f * u), ImVec2(c.x - 3.5f * u, c.y + 1.f * u), col, 1.6f * u);
    }
    if (row.clicked) {
        g_boxLeft = -1.f;
        g_dropdownConfigName = entry.name;
        g_dropdownConfigOpen = true;
        g_dropdownConfigCat = -1;   // resolved from the entry itself when the view draws
        g_moduleExpanded[entry.name] = true;
        g_cfgScroll.erase(entry.name);
        g_cfgPanelAnim.erase(entry.name);
    }
}

// The settings view: the way back, the module's name with its category icon and tag, a short
// description, and then the module's own card with every one of its settings under it. The card
// comes from the shared renderer, so a setting behaves exactly as it does in the Default
// interface and no option is written twice.
float drawDropdownConfigView(ImDrawList* dl, ImVec2 origin, float scale, float opacity,
                             float x, float y, float width, float anim) {
    const modules::Entry* entry = nullptr;
    int cat = 0;
    for (const auto& candidate : modules::entries()) {
        if (g_dropdownConfigName == candidate.name) { entry = &candidate; break; }
    }
    if (!entry) { g_dropdownConfigOpen = false; g_dropdownConfigName.clear(); return 0.f; }
    for (int i = 0; i < kCategoryCount; ++i)
        if (entry->category == categoryForPage(kCategoryPages[i])) { cat = i; break; }
    g_dropdownConfigCat = cat;
    const float a = opacity * clamp01(anim);
    const float headH = 64.f;
    drawCard(dl, origin, x, y, width, headH, scale, a);
    if (drawButton(dl, origin, "dd-config-back", "BACK", x + 16.f, y + 20.f, 68.f, 24.f, scale, a, false, true))
        g_dropdownConfigOpen = false;
    drawIcon(dl, screenPos(origin, x + 108.f, y + 32.f, scale), kCategoryIcons[cat],
             alpha(accentColorU32(), a), scale);
    drawText(dl, origin, x + 126.f, y + 12.f, entry->name, kInk, 16.f, scale, a, true);
    drawModuleTag(dl, origin, *entry, x + 136.f + measureText(entry->name, 16.f, scale, true) / scale,
                  y + 18.f, scale, a);
    char path[64];
    snprintf(path, sizeof(path), "SETTINGS  /  %s", kCategoryNames[cat]);
    const float pathW = measureText(path, 8.f, scale, true) / scale;
    drawText(dl, origin, x + width - pathW - 20.f, y + 15.f, path, kDim, 8.f, scale, a, true);
    drawText(dl, origin, x + 126.f, y + 38.f,
             ellipsize(entry->description, 10.f, scale, width - 150.f).c_str(), kMuted, 10.f, scale, a);
    // The module's card, expanded: its state, its settings and its keybind, all from the shared
    // card renderer. `layoutY` comes back holding the bottom of what was drawn.
    float layoutY = y + headH + 10.f;
    int matched = 0, visible = 0;
    const std::vector<const modules::Entry*> one = { entry };
    // The module's own card is drawn with the Dropdown layout's flat styling, then the flag is
    // handed back exactly as it was found.
    const bool wasDropdownCards = g_dropdownCards;
    g_dropdownCards = true;
    drawModuleCards(dl, origin, scale, a, x, width, one, layoutY, matched, visible);
    g_dropdownCards = wasDropdownCards;
    return std::max(headH + 10.f, layoutY - y) + 20.f;
}

// The Dropdown layout's module list: six columns (Player, Render, Combat, Movement, Misc, World),
// each a dark card of its modules. A column's header is dragged to move it along the row, and a
// row's `+` opens that module's box. Returns the bottom edge of the tallest column.
float drawDropdownColumns(ImDrawList* dl, ImVec2 origin, float scale, float opacity,
                          float x, float y, float width) {
    // The reference dropdown: one column per category, each with its own colour, every module in
    // that category as a single row. The old layout folded categories into six arbitrary columns
    // (Player / Render / World), which is exactly what the rebrand asked to remove - there is no
    // Render column now, it is Visual, and no invented Player or Cart group at all.
    struct Column { const char* title; modules::Category category; ImU32 color; };
    static const Column columns[8] = {
        { "Combat",   modules::Category::Combat,   IM_COL32(116, 196, 106, 255) },
        { "Crystal",  modules::Category::Crystal,  IM_COL32(176, 124, 255, 255) },
        { "Mace",     modules::Category::Mace,     IM_COL32(240, 106, 168, 255) },
        { "Spear",    modules::Category::Spear,    IM_COL32(240, 160, 80, 255) },
        { "Movement", modules::Category::Movement, IM_COL32(95, 208, 192, 255) },
        { "Visual",   modules::Category::Visual,   IM_COL32(192, 198, 212, 255) },
        { "Misc",     modules::Category::Misc,     IM_COL32(232, 200, 96, 255) },
        { "Macros",   modules::Category::Macros,   IM_COL32(224, 106, 106, 255) },
    };
    constexpr int kColumns = 8;
    const float gap = 8.f, headH = 28.f, rowH = 21.f, rowGap = 1.f;
    const float colW = (width - gap * (kColumns - 1)) / kColumns;
    if ((int)g_columnOrder.size() != kColumns) {
        g_columnOrder.clear();
        for (int i = 0; i < kColumns; ++i) g_columnOrder.push_back(i);
    }
    std::vector<float> xs, ws;
    for (int k = 0; k < kColumns; ++k) {
        xs.push_back(x + (float)k * (colW + gap));
        ws.push_back(colW);
    }
    updateDragReorder(g_columnDrag, g_columnOrder, xs, ws, y, headH, origin, scale);
    const float mouseX = (ImGui::GetIO().MousePos.x - origin.x) / scale;
    const int dragSlot = (g_columnDrag.slot >= 0 && g_columnDrag.moved) ? g_columnDrag.slot : -1;
    float bottom = y;
    // The dragged column is drawn last, so it rides above the others.
    for (int pass = 0; pass < 2; ++pass) {
        for (int k = 0; k < kColumns; ++k) {
            if ((k == dragSlot) != (pass == 1)) continue;
            const Column& col = columns[(size_t)g_columnOrder[(size_t)k]];
            float cx = xs[(size_t)k];
            if (k == dragSlot) cx = clampf(mouseX - g_columnDrag.grabOffset, x, x + width - colW);
            std::vector<const modules::Entry*> list;
            for (const auto& entry : modules::entries())
                if (entry.category == col.category) list.push_back(&entry);
            std::sort(list.begin(), list.end(), [](auto a, auto b) {
                return std::string(a->name) < std::string(b->name);
            });
            dl->AddCircleFilled(screenPos(origin, cx + 4.f, y + 10.f, scale), 2.6f * scale,
                                alpha(col.color, opacity));
            drawText(dl, origin, cx + 11.f, y + 2.f, col.title, col.color, 12.5f, scale, opacity, true);
            char count[8];
            snprintf(count, sizeof(count), "%d", (int)list.size());
            const float cw = measureText(count, 7.f, scale, true) / scale;
            drawText(dl, origin, cx + colW - cw - 2.f, y + 5.f, count, alpha(kDim, opacity), 7.f, scale, opacity, true);
            const float cardBottom = y + headH + 6.f + (float)list.size() * (rowH + rowGap) + 4.f;
            dl->AddRectFilled(screenPos(origin, cx, y + headH, scale),
                              screenPos(origin, cx + colW, cardBottom, scale),
                              alpha(IM_COL32(7, 8, 10, 210), opacity), 5.f * scale);
            dl->AddRect(screenPos(origin, cx, y + headH, scale),
                        screenPos(origin, cx + colW, cardBottom, scale),
                        alpha(kLine, opacity * 0.35f), 5.f * scale, 0, scale);
            float ry = y + headH + 5.f;
            for (const modules::Entry* entry : list) {
                const Hit hit = hitBox((std::string("dd-col-") + entry->name).c_str(),
                                       cx + 2.f, ry, colW - 4.f, rowH, scale);
                const bool on = entry->enabled && *entry->enabled;
                if (hit.hover > 0.01f || on)
                    dl->AddRectFilled(screenPos(origin, cx + 2.f, ry, scale),
                                      screenPos(origin, cx + colW - 2.f, ry + rowH, scale),
                                      alpha(blend(IM_COL32(22, 24, 28, 255), accentColorU32(), on ? 0.20f : 0.0f),
                                            opacity * (on ? 1.f : hit.hover)), 3.f * scale);
                if (on)
                    dl->AddCircleFilled(screenPos(origin, cx + 7.f, ry + rowH * 0.5f, scale), 1.7f * scale,
                                        alpha(col.color, opacity));
                drawText(dl, origin, cx + 12.f, ry + (rowH - 11.f) * 0.5f,
                         ellipsize(entry->name, 10.5f, scale, colW - 18.f).c_str(),
                         on ? kInk : kMuted, 10.5f, scale, opacity, on);
                if (hit.clicked) {
                    g_dropdownConfigName = entry->name;
                    g_dropdownConfigOpen = true;
                    g_dropdownConfigCat = -1;
                    g_boxLeft = cx;
                    g_boxRight = cx + colW;
                    g_boxTop = ry;
                    g_moduleExpanded[entry->name] = true;
                    g_cfgScroll.erase(entry->name);
                    g_cfgPanelAnim.erase(entry->name);
                }
                ry += rowH + rowGap;
            }
            bottom = std::max(bottom, cardBottom);
        }
    }
    return bottom;
}

// The Dropdown layout's content: the selected tab's modules as a flat list of rows, or the open
// module's settings view. A search reaches across every category, so a typed search lists its
// matches in one place instead of making the user walk the tabs looking for them.
void drawDropdownModulePage(ImDrawList* dl, ImVec2 origin, float scale, float opacity,
                            float x, float y, float width, float height);

// The Dropdown layout's list: the module columns, or - while a search is typed - the modules the
// search matches. The open module's box is drawn over it by drawDropdownModulePage.
void drawDropdownModuleList(ImDrawList* dl, ImVec2 origin, float scale, float opacity,
                            float x, float y, float width, float height) {
    (void)height;
    // Everything below is drawn with the Dropdown layout's own card and control styling.
    g_dropdownCards = true;
    ensureDropdownOrder();

    // The list: the selected tab's own category, or - while a search is typed - every module the
    // search matches, whatever category it lives in.
    const bool filtering = g_searchText[0] != '\0';
    if (!filtering) {
        const float bottom = drawDropdownColumns(dl, origin, scale, opacity, x, y, width);
        ImGui::SetCursorPos(ImVec2(x * scale, std::max(0.f, bottom * scale)));
        ImGui::Dummy(ImVec2(std::max(1.f, width * scale), 1.f));
        g_dropdownCards = false;
        return;
    }
    int tabCat = 0;
    for (int i = 0; i < kCategoryCount; ++i)
        if (kCategoryPages[i] == g_page) { tabCat = i; break; }
    drawText(dl, origin, x + 2.f, y, filtering ? "SEARCH RESULTS" : kCategoryNames[tabCat],
             kInk, 15.f, scale, opacity, true);
    if (!filtering) {
        char countText[32];
        snprintf(countText, sizeof(countText), "%d MODULES", categoryModuleCount(tabCat));
        const float titleW = measureText(kCategoryNames[tabCat], 15.f, scale, true) / scale;
        drawText(dl, origin, x + titleW + 14.f, y + 3.f, countText, kDim, 8.f, scale, opacity, true);
    }
    float cursorY = y + 26.f;
    std::vector<const modules::Entry*> list;
    for (const auto& entry : modules::entries()) {
        const bool inTab = !filtering && entry.category == categoryForPage(kCategoryPages[tabCat]);
        const bool matches = filtering && (containsInsensitive(entry.name, g_searchText) ||
                                           containsInsensitive(entry.description, g_searchText));
        if (inTab || matches) list.push_back(&entry);
    }
    std::sort(list.begin(), list.end(), [](auto a, auto b) {
        return std::string(a->name) < std::string(b->name);
    });
    for (const modules::Entry* entry : list) {
        drawDropdownModuleRow(dl, origin, *entry, x, cursorY, width, scale, opacity, 1.f);
        cursorY += kDropdownRowH + kDropdownRowGap;
    }
    if (list.empty()) {
        drawCard(dl, origin, x, cursorY, width, 100.f, scale, opacity);
        drawText(dl, origin, x + 20.f, cursorY + 22.f,
                 filtering ? "NO MATCHES" : "NO MODULES IN THIS CATEGORY", kDim, 9.f, scale, opacity, true);
        drawText(dl, origin, x + 20.f, cursorY + 50.f,
                 filtering ? "Try another module name." : "This category is ready for future modules.",
                 kMuted, 11.f, scale, opacity);
        cursorY += 110.f;
    }
    // Scroll on what was drawn: this invisible item is the bottom of the content.
    ImGui::SetCursorPos(ImVec2(x * scale, std::max(0.f, cursorY * scale)));
    ImGui::Dummy(ImVec2(std::max(1.f, width * scale), 1.f));
    g_dropdownCards = false;
}

void drawModulePage(ImDrawList* dl, ImVec2 origin, float scale, float opacity,
                    float x,float y,float width,float height) {
    // Change Interface: the Dropdown layout lists the category its tab selected instead of
    // drawing the Default interface's cards.
    if (g_interfaceStyle == InterfaceStyle::Dropdown) {
        drawDropdownModulePage(dl, origin, scale, opacity, x, y, width, height);
        return;
    }
    std::vector<const modules::Entry*> list;
    for (const auto& entry : modules::entries())
        if (entry.category==categoryForPage(g_page)) list.push_back(&entry);
    std::sort(list.begin(),list.end(),[](auto a,auto b){return std::string(a->name)<b->name;});
    if (list.empty()) {
        drawCard(dl,origin,x,y,width,height,scale,opacity);
        drawText(dl,origin,x+24.f,y+26.f,"NO MODULES IN THIS CATEGORY",kDim,9.f,scale,opacity,true);
        drawText(dl,origin,x+24.f,y+56.f,"This category is ready for future single-player modules.",kMuted,12.f,scale,opacity);
        return;
    }
    float layoutY=y;
    int matchedCount=0, visibleCount=0;
    drawModuleCards(dl,origin,scale,opacity,x,width,list,layoutY,matchedCount,visibleCount);
    if (!matchedCount && !visibleCount && g_searchText[0]) {
        drawCard(dl,origin,x,y,width,100.f,scale,opacity);
        drawText(dl,origin,x+20.f,y+22.f,"NO MATCHES",kDim,9.f,scale,opacity,true);
        drawText(dl,origin,x+20.f,y+50.f,"Try another module name.",kMuted,11.f,scale,opacity);
    }
}

// The Dropdown layout's page: the module list, or the module's own full configuration page. The
// old interface drew the library card again inside a floating box; that is gone. Opening a row
// now replaces the list with the same new inspector the library uses, skinned for Dropdown.
void drawDropdownModulePage(ImDrawList* dl, ImVec2 origin, float scale, float opacity,
                            float x, float y, float width, float height) {
    const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.f, 0.1f);
    g_dropdownConfigAnim = g_animations ? ease(g_dropdownConfigAnim, g_dropdownConfigOpen ? 1.f : 0.f, 15.f, dt)
                                       : (g_dropdownConfigOpen ? 1.f : 0.f);
    const modules::Entry* entry = nullptr;
    for (const auto& candidate : modules::entries())
        if (candidate.name == g_dropdownConfigName) { entry = &candidate; break; }
    if (!g_dropdownConfigOpen || !entry) {
        drawDropdownModuleList(dl, origin, scale, opacity, x, y, width, height);
        if (g_dropdownConfigAnim <= 0.012f) {
            if (!g_dropdownConfigName.empty()) g_moduleExpanded[g_dropdownConfigName] = false;
            g_dropdownConfigName.clear();
            g_dropdownConfigCat = -1;
        }
        return;
    }

    const float anim = clamp01(g_dropdownConfigAnim);
    const float a = opacity * anim;
    g_dropdownCards = true;
    if (drawButton(dl, origin, "dd-inspector-back", "BACK TO MODULES", x, y, 142.f, 28.f, scale, a, false, true)) {
        g_dropdownConfigOpen = false;
        return;
    }
    char crumb[80];
    snprintf(crumb, sizeof(crumb), "MODULES  /  %s", entry->name);
    drawText(dl, origin, x + 154.f, y + 7.f, crumb, kDim, 7.5f, scale, a, true);
    drawText(dl, origin, x + width - 66.f, y + 7.f, "CONFIG",
             alpha(accentColorU32(), a), 8.f, scale, a, true);
    const float slide = (1.f - anim) * 14.f;
    ImGui::SetCursorPos(ImVec2(x * scale, (y + 38.f + slide) * scale));
    drawModuleInspector(dl, origin, scale, a, x, y + 38.f + slide, width, *entry, true, height);
    g_dropdownCards = false;
}

nlohmann::json captureConfiguration() {
    nlohmann::json j;
    j["version"]=1;
    j["modules"]=nlohmann::json::parse(modules::serialize());
    j["ui"]={{"scale",g_uiScale},{"opacity",g_panelOpacity},{"animations",g_animations},
             {"compact",g_compact},{"softGlow",g_softGlow},{"closeWithEscape",g_closeWithEscape},
             {"menuKey",g_menuKey},{"moduleStyle",(int)g_moduleStyle},
             {"moduleShape",(int)g_moduleShape},{"interface",(int)g_interfaceStyle}};
    // The Dropdown interface keeps the tab order the user dragged into place.
    nlohmann::json dropdownOrder=nlohmann::json::array();
    for (int cat : g_dropdownOrder) dropdownOrder.push_back(cat);
    j["ui"]["dropdownOrder"]=dropdownOrder;
    j["appearance"]["colors"]=nlohmann::json::array();
    for (const ImVec4 c:g_themeColors) j["appearance"]["colors"].push_back({c.x,c.y,c.z,c.w});
    j["appearance"]["themePreset"]=g_themePreset;
    j["appearance"]["smoothFade"]=g_themeSmoothFade;
    j["appearance"]["fadeColorBackground"]=g_fadeColorBackground;
    j["hudEditor"]["enabled"]=g_hudEditorEnabled;
    j["hudEditor"]["elements"]=nlohmann::json::array();
    for (const HudElement& element:g_hudElements)
        j["hudEditor"]["elements"].push_back({{"id",element.id},{"x",element.pos.x},
                                              {"y",element.pos.y},{"enabled",element.enabled}});
    return j;
}

bool applyConfiguration(const std::string& text) {
    try {
        const auto j=nlohmann::json::parse(text);
        if (!j.is_object()||j.value("version",0)!=1) return false;
        if (j.contains("modules")&&!modules::deserialize(j["modules"].dump())) return false;
        if (j.contains("ui")) {
            const auto u=j["ui"];
            g_uiScale=clampf(u.value("scale",1.f),.8f,1.75f);
            g_panelOpacity=clampf(u.value("opacity",.96f),.5f,1.f);
            g_animations=u.value("animations",true);g_compact=u.value("compact",false);
            g_softGlow=u.value("softGlow",true);g_closeWithEscape=u.value("closeWithEscape",true);
            g_menuKey=std::clamp(u.value("menuKey",(int)VK_RSHIFT),1,255);
            g_moduleStyle=(ModuleStyle)std::clamp(u.value("moduleStyle",0),0,4);
            g_moduleShape=(ModuleShape)std::clamp(u.value("moduleShape",0),0,1);
            g_interfaceStyle=(InterfaceStyle)std::clamp(u.value("interface",0),0,1);
            if (u.contains("dropdownOrder")&&u["dropdownOrder"].is_array()) {
                std::vector<int> order;
                std::vector<bool> seen(kCategoryCount,false);
                for (const auto& v:u["dropdownOrder"]) {
                    if (!v.is_number_integer()) continue;
                    const int cat=std::clamp(v.get<int>(),0,kCategoryCount-1);
                    if (seen[(size_t)cat]) continue;
                    seen[(size_t)cat]=true;
                    order.push_back(cat);
                }
                if ((int)order.size()==kCategoryCount) g_dropdownOrder=order;
            }
        }
        if (j.contains("appearance")&&j["appearance"].contains("colors")&&j["appearance"]["colors"].is_array()) {
            const auto& colors=j["appearance"]["colors"];
            for (size_t i=0;i<8&&i<colors.size();++i) if (colors[i].is_array()&&colors[i].size()>=4)
                g_themeColors[i]=ImVec4(clampf(colors[i][0].get<float>(),0.f,1.f),clampf(colors[i][1].get<float>(),0.f,1.f),
                    clampf(colors[i][2].get<float>(),0.f,1.f),clampf(colors[i][3].get<float>(),0.f,1.f));
            applyThemeColors();
        }
        if (j.contains("appearance")) {
            g_themePreset=j["appearance"].value("themePreset",-1);
            g_themeSmoothFade=j["appearance"].value("smoothFade",false);
            g_fadeColorBackground=j["appearance"].value("fadeColorBackground",false);
        }
        if (j.contains("hudEditor")) {
            const auto h=j["hudEditor"];
            g_hudEditorEnabled=h.value("enabled",true);
            if (h.contains("elements")&&h["elements"].is_array()) {
                for (const auto& saved:h["elements"]) {
                    if (!saved.is_object()) continue;
                    const std::string id=saved.value("id","");
                    for (HudElement& element:g_hudElements) {
                        if (id != element.id) continue;
                        element.pos=ImVec2(clampf(saved.value("x",element.pos.x),0.f,1.f),
                                           clampf(saved.value("y",element.pos.y),0.f,1.f));
                        element.enabled=saved.value("enabled",element.enabled);
                        break;
                    }
                }
            }
        }
        return true;
    } catch (...) { return false; }
}

// The Settings page's own vertical rail. It replaced the horizontal tab row when
// Configurations became a full page: the sections now read down the left edge, with the
// selected one carrying an accent bar, and the content gets the rest of the panel.
void drawSettingsRail(ImDrawList* dl, ImVec2 origin, float scale, float opacity,
                      float x, float y, float width, float height) {
    drawCard(dl, origin, x, y, width, height, scale, opacity, true);
    drawBrandMark(dl, origin, x + 18.f, y + 18.f, 28.f, scale, opacity);
    drawText(dl, origin, x + 54.f, y + 17.f, "SETTINGS", kInk, 12.f, scale, opacity, true);
    drawText(dl, origin, x + 54.f, y + 34.f, "CLIENT PREFERENCES", kDim, 6.5f, scale, opacity, true);
    dl->AddRectFilled(screenPos(origin, x + 16.f, y + 62.f, scale),
                      screenPos(origin, x + width - 16.f, y + 63.f, scale),
                      alpha(kLine, opacity * 0.45f), 0.f);

    struct RailItem { const char* label; const char* hint; int icon; SettingsPage page; };
    const RailItem items[] = {
        { "General",     "Interface & behaviour", 4,  SettingsPage::General },
        { "Appearance",  "Themes & colours",     12, SettingsPage::Appearance },
        { "Controls",    "Keys & mouse",         2,  SettingsPage::Controls },
        { "Friends",     "People to ignore",     9,  SettingsPage::Friends },
        { "About",       "Build information",    13, SettingsPage::About },
    };
    float iy = y + 78.f;
    for (const RailItem& item : items) {
        const bool selected = g_settingsPage == item.page;
        const Hit hit = hitBox((std::string("settings-rail-") + item.label).c_str(),
                               x + 12.f, iy, width - 24.f, 42.f, scale);
        const ImVec2 r0 = screenPos(origin, x + 12.f, iy, scale);
        const ImVec2 r1 = screenPos(origin, x + width - 12.f, iy + 42.f, scale);
        if (selected || hit.hover > 0.01f)
            dl->AddRectFilled(r0, r1, alpha(blend(kCard, accentColorU32(), selected ? 0.13f : 0.055f * hit.hover), opacity), 8.f * scale);
        if (selected)
            dl->AddRectFilled(screenPos(origin, x + 12.f, iy + 10.f, scale),
                              screenPos(origin, x + 14.f, iy + 32.f, scale),
                              alpha(accentColorU32(), opacity), 1.f * scale);
        drawIcon(dl, screenPos(origin, x + 30.f + hit.hover * 2.f, iy + 21.f, scale), item.icon,
                 alpha(selected ? accentColorU32() : kMuted, opacity), 0.72f * scale);
        drawText(dl, origin, x + 46.f + hit.hover * 2.f, iy + 10.f, item.label,
                 alpha(selected ? kInk : kMuted, opacity), 10.5f, scale, opacity, selected);
        drawText(dl, origin, x + 46.f + hit.hover * 2.f, iy + 25.f, item.hint,
                 alpha(kDim, opacity), 7.f, scale, opacity);
        if (hit.clicked && !selected) {
            g_settingsPage = item.page;
            g_pageFade = g_animations ? 0.f : 1.f;
        }
        iy += 46.f;
    }
    drawText(dl, origin, x + 18.f, y + height - 42.f, "PASSIVE  NATIVE  CLIENT", kDim, 6.5f, scale, opacity, true);
    drawText(dl, origin, x + 18.f, y + height - 28.f, info::kVersion, alpha(accentColorU32(), opacity), 8.f, scale, opacity, true);
}

// Settings > Friends: add friends by username (or from the players in the world)
// and remove them again. The list is saved with the module config.
void drawFriends(ImDrawList* dl,ImVec2 origin,float scale,float opacity,
                 float x,float y,float width,float height) {
    const float gap=14.f;
    const float leftW=width*.52f;
    const float rightX=x+leftW+gap;
    const float rightW=width-leftW-gap;

    drawCard(dl,origin,x,y,leftW,height,scale,opacity,true);
    drawText(dl,origin,x+18.f,y+17.f,"ADD FRIEND",kDim,9.f,scale,opacity,true);
    drawText(dl,origin,x+18.f,y+37.f,"Type a username to mark them as a friend.",kMuted,10.f,scale,opacity);
    setWidgetCursor(x+16.f,y+64.f,scale);
    ImGui::PushID("friend-name");
    ImGui::SetNextItemWidth(leftW-32.f);
    ImGui::InputTextWithHint("##friend-name","username",g_friendName,sizeof(g_friendName));
    ImGui::PopID();
    if (drawButton(dl,origin,"add-friend","ADD TO FRIENDS",x+16.f,y+98.f,leftW-32.f,30.f,scale,opacity,true,true)) {
        const std::string name=g_friendName;
        if (name.empty()) notify("Type a username first");
        else if (modules::addFriend(name)) { g_friendName[0]='\0'; notify("Friend added"); }
        else notify("That user is already a friend");
    }
    drawText(dl,origin,x+18.f,y+142.f,"PLAYERS IN THIS SERVER",kDim,8.f,scale,opacity,true);
    const std::vector<std::string> nearby=modules::nearbyPlayerNames();
    if (nearby.empty())
        drawText(dl,origin,x+18.f,y+163.f,"No other players are in this world.",kDim,9.f,scale,opacity);
    float rowY=y+162.f;
    int shownRows=0;
    for (const std::string& name:nearby) {
        if (shownRows>=8||rowY>y+height-34.f) break;
        const bool already=modules::isFriend(name);
        drawText(dl,origin,x+22.f,rowY+6.f,name.c_str(),already?kMuted:kInk,10.f,scale,opacity,already);
        if (!already) {
            const std::string addId=std::string("add-nearby-")+name;
            if (drawButton(dl,origin,addId.c_str(),"ADD",x+leftW-88.f,rowY,70.f,22.f,scale,opacity,false,true)) {
                if (modules::addFriend(name)) notify("Friend added");
            }
        }
        rowY+=28.f;
        ++shownRows;
    }

    drawCard(dl,origin,rightX,y,rightW,height,scale,opacity);
    drawText(dl,origin,rightX+18.f,y+17.f,"FRIENDS",kDim,9.f,scale,opacity,true);
    const std::vector<std::string> list=modules::friends();
    drawText(dl,origin,rightX+18.f,y+37.f,
             list.empty()?"Modules will ignore players you add here.":"Modules will not act on these players.",
             kMuted,10.f,scale,opacity);
    if (list.empty())
        drawText(dl,origin,rightX+18.f,y+66.f,"No friends yet.",kDim,10.f,scale,opacity);
    float friendY=y+64.f;
    for (const std::string& name:list) {
        if (friendY>y+height-30.f) break;
        const std::string removeId=std::string("remove-friend-")+name;
        if (drawIconButton(dl,origin,removeId.c_str(),5,rightX+rightW-46.f,friendY+1.f,26.f,scale,opacity)) {
            if (modules::removeFriend(name)) notify("Friend removed");
            break;
        }
        drawText(dl,origin,rightX+22.f,friendY+7.f,name.c_str(),kInk,10.f,scale,opacity,true);
        friendY+=32.f;
    }
}

void drawSettingsPage(ImDrawList* dl,ImVec2 origin,float scale,float opacity,
                      float x,float y,float width,float height) {
    const float railW = 196.f;
    drawSettingsRail(dl, origin, scale, opacity, x, y, railW, height);
    x += railW + 16.f;
    width -= railW + 16.f;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*opacity);
    const float cy=y, ch=height;
    if (g_settingsPage==SettingsPage::General) {
        const float gap=14.f,rightX=x+width*.55f+gap,rightW=width*.45f-gap;
        drawCard(dl,origin,x,cy,width*.55f,ch,scale,opacity,true);
        drawText(dl,origin,x+20.f,cy+18.f,"INTERFACE",kDim,9.f,scale,opacity,true);
        drawSlider(dl,origin,"ui-scale-settings","Interface scale","%",x+20.f,cy+55.f,width*.55f-40.f,scale,opacity,g_uiScale,.80f,1.75f);
        drawSlider(dl,origin,"opacity-settings","Surface opacity","%",x+20.f,cy+115.f,width*.55f-40.f,scale,opacity,g_panelOpacity,.5f,1.f);
        drawToggleRow(dl,origin,"general-debug","Debug Mode","Modules report their actions in Debug Mode toasts",x+20.f,cy+174.f,width*.55f-40.f,scale,opacity,modules::debugMode());
        drawToggleRow(dl,origin,"general-notify","Notifications","Friend joins and the module you just toggled",x+20.f,cy+228.f,width*.55f-40.f,scale,opacity,modules::notificationsEnabled());
        drawText(dl,origin,x+20.f,cy+282.f,"MOTION & DETAIL",kDim,9.f,scale,opacity,true);
        drawToggleRow(dl,origin,"general-motion","Smooth animations","Tabs and controls transition smoothly",x+20.f,cy+302.f,width*.55f-40.f,scale,opacity,g_animations);
        drawToggleRow(dl,origin,"general-compact","Compact spacing","Reduce panel and card gaps",x+20.f,cy+356.f,width*.55f-40.f,scale,opacity,g_compact);
        drawCard(dl,origin,rightX,cy,rightW,ch,scale,opacity);
        drawText(dl,origin,rightX+18.f,cy+18.f,"BEHAVIOUR",kDim,9.f,scale,opacity,true);
        drawToggleRow(dl,origin,"general-glow","Accent glow","Subtle emphasis on hover",rightX+18.f,cy+56.f,rightW-36.f,scale,opacity,g_softGlow);
        drawToggleRow(dl,origin,"general-escape","Close with Escape","Allow Escape to close the menu",rightX+18.f,cy+122.f,rightW-36.f,scale,opacity,g_closeWithEscape);
        // ---- Change Interface - the whole menu, one dropdown ----
        drawText(dl,origin,rightX+18.f,cy+190.f,"CHANGE INTERFACE",kDim,9.f,scale,opacity,true);
        drawText(dl,origin,rightX+18.f,cy+207.f,"Switch the whole menu between Passive's interfaces.",kMuted,9.f,scale,opacity);
        setWidgetCursor(rightX+18.f,cy+228.f,scale);
        ImGui::PushID("change-interface");
        ImGui::SetNextItemWidth(rightW-36.f);
        if (ui::BeginCombo("##interface",kInterfaceStyleNames[(int)g_interfaceStyle])) {
            for (int i=0;i<2;++i) {
                if (ImGui::Selectable(kInterfaceStyleNames[i],(int)g_interfaceStyle==i) &&
                    (int)g_interfaceStyle!=i) {
                    g_interfaceStyle=(InterfaceStyle)i;
                    // Leaving the Dropdown layout closes any module settings view it had open, so
                    // returning to it never lands on a stale module.
                    g_dropdownConfigOpen=false;
                    g_dropdownConfigName.clear();
                    g_dropdownConfigCat=-1;
                    g_pageFade=g_animations?0.f:1.f;
                    notify((std::string("Interface: ")+kInterfaceStyleNames[i]).c_str());
                }
            }
            ui::EndCombo();
        }
        ImGui::PopID();
        drawText(dl,origin,rightX+18.f,cy+258.f,kInterfaceStyleHelp[(int)g_interfaceStyle],kDim,8.f,scale,opacity);
        drawText(dl,origin,rightX+18.f,cy+ch-79.f,"CLIENT SHUTDOWN",kDim,8.f,scale,opacity,true);
        // Two steps, because one stray click used to unload the client outright
        // (and wipe the settings with it). The first click arms the button and
        // says so in the log; only a second click within a few seconds really
        // unloads, and doing nothing disarms it again.
        {
            const double now=ImGui::GetTime();
            if (g_confirmUnload && now-g_confirmUnloadAt>5.0) g_confirmUnload=false;
            if (drawButton(dl,origin,"self-destruct",
                           g_confirmUnload?"CONFIRM UNLOAD":"SELF DESTRUCT / UNLOAD",
                           rightX+18.f,cy+ch-55.f,rightW-36.f,34.f,scale,opacity,false,true)) {
                if (g_confirmUnload) {
                    g_confirmUnload=false;
                    modules::resetDefaults();
                    tyr::trace::log("unload requested: SELF DESTRUCT button (Settings > General)");
                    notify("Passive unload requested");
                    g_requestUnload.store(true);
                } else {
                    g_confirmUnload=true;
                    g_confirmUnloadAt=now;
                    tyr::trace::log("unload armed: SELF DESTRUCT button (Settings > General) - click again to confirm");
                    notify("Click SELF DESTRUCT again to confirm");
                }
            }
        }
    } else if (g_settingsPage==SettingsPage::Appearance) {
        const float gap=14.f,leftW=width*.54f,rightX=x+width*.54f+gap,rightW=width*.46f-gap;
        // The Appearance columns scroll as one, between the tab row and the pinned Reset button, so a
        // short window reaches every control instead of Reset landing on the theme chips.
        const float cyFixed=cy, vBot=cy+ch-50.f, contentH=490.f;
        const float maxScroll=std::max(0.f,contentH-(vBot-cyFixed));
        if (ImGui::IsMouseHoveringRect(screenPos(origin,x,cyFixed,scale),screenPos(origin,x+width,vBot,scale)))
            g_appearanceScroll-=ImGui::GetIO().MouseWheel*26.f;
        g_appearanceScroll=clampf(g_appearanceScroll,0.f,maxScroll);
        const float sy=cyFixed-g_appearanceScroll;
        dl->PushClipRect(screenPos(origin,x,cyFixed,scale),screenPos(origin,x+width,vBot,scale),true);
        drawCard(dl,origin,x,cyFixed,leftW,ch,scale,opacity,true);
        drawText(dl,origin,x+20.f,sy+17.f,"COLOR PICKER",kDim,9.f,scale,opacity,true);
        drawText(dl,origin,x+20.f,sy+37.f,kThemeColorNames[g_selectedThemeColor],kInk,16.f,scale,opacity,true);
        setWidgetCursor(x+14.f,sy+61.f,scale);
        ImGui::PushID("theme-color-wheel");
        // Keep the widget inside the left card so its invisible interaction
        // rectangles cannot cover the selectable color rows on the right.
        ImGui::SetNextItemWidth(210.f*scale);
        if (ImGui::ColorPicker4("##picker",&g_themeColors[g_selectedThemeColor].x,
            ImGuiColorEditFlags_PickerHueBar|ImGuiColorEditFlags_NoSidePreview|
            ImGuiColorEditFlags_NoSmallPreview|ImGuiColorEditFlags_NoInputs|
            ImGuiColorEditFlags_NoOptions|ImGuiColorEditFlags_AlphaBar)) { g_themePreset=-1; applyThemeColors(); }
        ImGui::PopID();
        // ---- Module appearance: style + shape (moved down from the header) ----
        // Module appearance lives in its own section card: module style and
        // module shape together, so both selectors read as one setting group.
        const float secX=x+16.f, secY=sy+286.f, secW=leftW-32.f, secH=168.f;
        drawCard(dl,origin,secX,secY,secW,secH,scale,opacity,true);
        drawText(dl,origin,secX+14.f,secY+14.f,"MODULE APPEARANCE",kDim,9.f,scale,opacity,true);
        if (g_interfaceStyle==InterfaceStyle::Default) {
            drawText(dl,origin,secX+14.f,secY+32.f,"How the module cards are drawn.",kMuted,10.f,scale,opacity);
            drawText(dl,origin,secX+14.f,secY+56.f,"STYLE",kDim,8.f,scale,opacity,true);
            const float styleW=(secW-28.f-4.f*6.f)/5.f;
            for (int i=0;i<5;++i) {
                const std::string styleId=std::string("ui-style-")+kModuleStyleNames[i];
                if (drawButton(dl,origin,styleId.c_str(),kModuleStyleNames[i],
                               secX+14.f+i*(styleW+6.f),secY+72.f,styleW,25.f,
                               scale,opacity,(int)g_moduleStyle==i,true)) g_moduleStyle=(ModuleStyle)i;
            }
            drawText(dl,origin,secX+14.f,secY+105.f,"SHAPE",kDim,8.f,scale,opacity,true);
            const float shapeW=(secW-28.f-6.f)/2.f;
            for (int i=0;i<2;++i) {
                const std::string shapeId=std::string("ui-shape-")+kModuleShapeNames[i];
                if (drawButton(dl,origin,shapeId.c_str(),kModuleShapeNames[i],
                               secX+14.f+i*(shapeW+6.f),secY+121.f,shapeW,25.f,
                               scale,opacity,(int)g_moduleShape==i,true)) g_moduleShape=(ModuleShape)i;
            }
            drawText(dl,origin,secX+14.f,secY+151.f,"Squared lays modules out in a 3-wide grid of square cards.",
                     kDim,8.f,scale,opacity);
        } else {
            // The Dropdown interface draws its own controls, so the Default UI's selectors go away.
            drawText(dl,origin,secX+14.f,secY+36.f,"Module Style and Module Shape belong to the",kMuted,10.f,scale,opacity);
            drawText(dl,origin,secX+14.f,secY+54.f,"Default interface.",kMuted,10.f,scale,opacity);
            drawText(dl,origin,secX+14.f,secY+84.f,"The Dropdown interface draws its own cards,",kDim,9.f,scale,opacity);
            drawText(dl,origin,secX+14.f,secY+100.f,"sliders and switches, so both are hidden here.",kDim,9.f,scale,opacity);
        }
        drawCard(dl,origin,rightX,cyFixed,rightW,ch,scale,opacity);
        drawText(dl,origin,rightX+18.f,sy+17.f,"UI COMPONENT",kDim,9.f,scale,opacity,true);
        drawText(dl,origin,rightX+18.f,sy+37.f,"Choose a part to recolor.",kMuted,10.f,scale,opacity);
        for (int i=0;i<8;++i) {
            const float ry=sy+62.f+i*34.f;
            const Hit hit=hitBox(kThemeColorNames[i],rightX+16.f,ry,rightW-32.f,29.f,scale);
            const ImVec2 p0=screenPos(origin,rightX+16.f,ry,scale),p1=screenPos(origin,rightX+rightW-16.f,ry+29.f,scale);
            if (i==g_selectedThemeColor||hit.hovered) dl->AddRectFilled(p0,p1,alpha(kCardLight,opacity),7.f*scale);
            const ImVec2 sw0=screenPos(origin,rightX+25.f,ry+7.f,scale),sw1=screenPos(origin,rightX+43.f,ry+22.f,scale);
            dl->AddRectFilled(sw0,sw1,packThemeColor(g_themeColors[i]),4.f*scale);
            drawText(dl,origin,rightX+53.f,ry+7.f,kThemeColorNames[i],i==g_selectedThemeColor?kInk:kMuted,9.f,scale,opacity,i==g_selectedThemeColor);
            if (hit.clicked) g_selectedThemeColor=i;
        }
        // ---- Theme: one click repaints every slot of the palette at once ----
        drawText(dl,origin,rightX+16.f,sy+349.f,"THEME",kDim,9.f,scale,opacity,true);
        const float chipW=(rightW-32.f-3.f*6.f)/4.f;
        for (int i=0;i<8;++i) {
            const float chipX=rightX+16.f+(float)(i%4)*(chipW+6.f);
            const float chipY=sy+365.f+(float)(i/4)*32.f;
            if (drawButton(dl,origin,kThemePresetIds[i],kThemePresetNames[i],chipX,chipY,chipW,26.f,
                           scale,opacity,g_themePreset==i,true))
                applyThemePreset(i);
        }
        setWidgetCursor(rightX+16.f,sy+435.f,scale);
        ui::Checkbox("Smooth fade background",&g_themeSmoothFade);
        setWidgetCursor(rightX+16.f,sy+459.f,scale);
        ui::Checkbox("Fade color background",&g_fadeColorBackground);
        dl->PopClipRect();
        if (maxScroll>0.5f) {
            const float trackH=vBot-cyFixed;
            const float thumbH=std::max(24.f,trackH*trackH/contentH);
            const float thumbY=cyFixed+(trackH-thumbH)*clamp01(g_appearanceScroll/maxScroll);
            dl->AddRectFilled(screenPos(origin,x+width-5.f,thumbY,scale),screenPos(origin,x+width-2.f,thumbY+thumbH,scale),
                              alpha(kMuted,opacity*0.6f),2.f*scale);
        }
        if (drawButton(dl,origin,"reset-appearance","RESET TO DEFAULT",rightX+16.f,cyFixed+ch-43.f,rightW-32.f,30.f,scale,opacity,false,true)) {
            resetAppearance();
            g_themePreset=-1; g_themeSmoothFade=false; g_themeFade=1.f;   // back to the default palette, no fade in flight
            g_fadeColorBackground=false;
            notify("Appearance reset to defaults");
        }
    } else if (g_settingsPage==SettingsPage::Controls) {
        drawControls(dl,origin,scale,opacity,x,cy,width,ch);
    } else if (g_settingsPage==SettingsPage::Friends) {
        drawFriends(dl,origin,scale,opacity,x,cy,width,ch);
    } else {
        drawCard(dl,origin,x,cy,width,ch,scale,opacity,true);
        drawText(dl,origin,x+22.f,cy+22.f,"PASSIVE",accentColorU32(),18.f,scale,opacity,true);
        drawText(dl,origin,x+22.f,cy+56.f,"Native C++ and ImGui overlay for Minecraft 1.21.11.",kInk,12.f,scale,opacity);
        drawText(dl,origin,x+22.f,cy+83.f,"The current feature set is intended for single-player worlds.",kMuted,10.f,scale,opacity);
        drawText(dl,origin,x+22.f,cy+115.f,"Injection uses MinHook; module state is read through the Java JNI bridge.",kMuted,10.f,scale,opacity);
    }
    ImGui::PopStyleVar();
}

// Configurations is its own page now - not a tab buried in Settings. The page is a preset
// library: a slim creation strip and the saved configurations on the left, a large selected
// configuration card on the right. It deliberately does not reuse the old two-card Settings look.
void drawConfigsPage(ImDrawList* dl, ImVec2 origin, float scale, float opacity,
                     float x, float y, float width, float height) {
    const auto saved = profiles::list();
    drawText(dl, origin, x, y, "CONFIGURATIONS", kDim, 7.5f, scale, opacity, true);
    drawText(dl, origin, x, y + 14.f, "Your presets", kInk, 21.f, scale, opacity, true);
    char count[64];
    snprintf(count, sizeof(count), "%d saved  ·  modules, themes and layout together", (int)saved.size());
    drawText(dl, origin, x + measureText("Your presets", 21.f, scale, true) / scale + 14.f, y + 22.f,
             count, kDim, 8.f, scale, opacity);
    if (drawButton(dl, origin, "cfg-new-page", "+  NEW CONFIGURATION", x + width - 178.f, y + 8.f,
                   178.f, 30.f, scale, opacity, true, true)) {
        g_profileName[0] = '\0';
        notify("Type a name in the creation strip");
    }
    dl->AddRectFilled(screenPos(origin, x, y + 54.f, scale), screenPos(origin, x + width, y + 55.f, scale),
                      alpha(kLine, opacity * 0.5f), 0.f);

    const float bodyY = y + 72.f;
    const float gap = 16.f, leftW = std::min(310.f, width * 0.38f);
    const float rightX = x + leftW + gap, rightW = width - leftW - gap;
    const float bodyH = std::max(220.f, height - 72.f);

    // ---- creation strip + saved list -----------------------------------------------------------
    drawCard(dl, origin, x, bodyY, leftW, bodyH, scale, opacity, true);
    drawText(dl, origin, x + 18.f, bodyY + 16.f, "CREATE A CONFIGURATION", kDim, 8.f, scale, opacity, true);
    drawText(dl, origin, x + 18.f, bodyY + 32.f, "Name it, then save the current client into it.", kMuted, 9.f, scale, opacity);
    setWidgetCursor(x + 16.f, bodyY + 56.f, scale);
    ImGui::PushID("configs-page-name");
    ImGui::SetNextItemWidth((leftW - 32.f) * scale);
    ImGui::InputTextWithHint("##name", "configuration name", g_profileName, sizeof(g_profileName));
    ImGui::PopID();
    if (drawButton(dl, origin, "configs-page-create", "CREATE FROM CURRENT", x + 16.f, bodyY + 88.f,
                   leftW - 32.f, 30.f, scale, opacity, true, true)) {
        std::string error;
        const std::string name = g_profileName;
        if (profiles::create(name, captureConfiguration().dump(2), error)) {
            g_selectedProfile = name;
            g_profileName[0] = '\0';
            notify("Configuration created");
        } else notify(error.c_str());
    }
    drawText(dl, origin, x + 18.f, bodyY + 138.f, "SAVED CONFIGURATIONS", kDim, 8.f, scale, opacity, true);
    float rowY = bodyY + 158.f;
    if (saved.empty()) {
        drawText(dl, origin, x + 18.f, rowY + 6.f, "Nothing saved yet.", kDim, 9.f, scale, opacity);
        drawText(dl, origin, x + 18.f, rowY + 24.f, "Create the first one above.", kDim, 8.f, scale, opacity);
    }
    const float rowH = 40.f;
    for (const auto& name : saved) {
        if (rowY + rowH > bodyY + bodyH - 16.f) break;
        const bool active = g_selectedProfile == name;
        const Hit hit = hitBox((std::string("configs-row-") + name).c_str(), x + 12.f, rowY, leftW - 24.f, rowH, scale);
        const ImVec2 r0 = screenPos(origin, x + 12.f, rowY, scale);
        const ImVec2 r1 = screenPos(origin, x + leftW - 12.f, rowY + rowH, scale);
        if (active || hit.hover > 0.01f)
            dl->AddRectFilled(r0, r1, alpha(blend(kCard, accentColorU32(), active ? 0.14f : 0.06f * hit.hover), opacity), 7.f * scale);
        if (active)
            dl->AddRectFilled(screenPos(origin, x + 12.f, rowY + 9.f, scale),
                              screenPos(origin, x + 14.f, rowY + rowH - 9.f, scale),
                              alpha(accentColorU32(), opacity), 1.f * scale);
        drawText(dl, origin, x + 26.f, rowY + 8.f, name.c_str(), active ? kInk : kMuted, 10.5f,
                 scale, opacity, active);
        drawText(dl, origin, x + 26.f, rowY + 24.f, active ? "SELECTED" : "PRESET",
                 active ? accentColorU32() : kDim, 6.5f, scale, opacity, true);
        if (hit.clicked) g_selectedProfile = name;
        rowY += rowH + 4.f;
    }

    // ---- selected configuration ---------------------------------------------------------------
    drawCard(dl, origin, rightX, bodyY, rightW, bodyH, scale, opacity, true);
    if (g_selectedProfile.empty()) {
        const ImVec2 mark((rightX + rightW * 0.5f) * scale, (bodyY + bodyH * 0.40f) * scale);
        (void)mark;
        drawBrandMark(dl, origin, rightX + rightW * 0.5f - 34.f, bodyY + bodyH * 0.26f, 68.f, scale, opacity * 0.75f);
        drawText(dl, origin, rightX + 24.f, bodyY + bodyH * 0.26f + 84.f, "NO CONFIGURATION SELECTED",
                 kInk, 12.f, scale, opacity, true);
        drawText(dl, origin, rightX + 24.f, bodyY + bodyH * 0.26f + 106.f,
                 "Pick a preset on the left, or create one from the current client.",
                 kMuted, 9.5f, scale, opacity);
    } else {
        drawBrandMark(dl, origin, rightX + 24.f, bodyY + 20.f, 38.f, scale, opacity);
        drawText(dl, origin, rightX + 76.f, bodyY + 22.f, g_selectedProfile.c_str(), kInk, 19.f, scale, opacity, true);
        drawText(dl, origin, rightX + 76.f, bodyY + 48.f, "SAVED CONFIGURATION", kDim, 7.f, scale, opacity, true);
        drawText(dl, origin, rightX + 24.f, bodyY + 84.f,
                 "Loading replaces the current modules, appearance and layout.", kMuted, 9.5f, scale, opacity);
        drawText(dl, origin, rightX + 24.f, bodyY + 102.f,
                 "Saving writes the client as it is right now into this configuration.", kDim, 8.5f, scale, opacity);
        dl->AddRectFilled(screenPos(origin, rightX + 20.f, bodyY + 126.f, scale),
                          screenPos(origin, rightX + rightW - 20.f, bodyY + 127.f, scale),
                          alpha(kLine, opacity * 0.45f), 0.f);
        drawText(dl, origin, rightX + 24.f, bodyY + 142.f, "INCLUDES", kDim, 7.f, scale, opacity, true);
        const char* includes[] = { "Modules & bounds", "Appearance", "HUD layout", "Interface mode" };
        for (int i = 0; i < 4; ++i) {
            const float cx = rightX + 24.f + (float)(i % 2) * (rightW * 0.5f - 16.f);
            const float cy = bodyY + 162.f + (float)(i / 2) * 26.f;
            dl->AddCircleFilled(screenPos(origin, cx + 3.f, cy + 5.f, scale), 2.2f * scale, alpha(accentColorU32(0.8f), opacity));
            drawText(dl, origin, cx + 12.f, cy, includes[i], kMuted, 8.5f, scale, opacity);
        }
        const float bw = (rightW - 56.f) * 0.5f;
        if (drawButton(dl, origin, "configs-page-load", "LOAD CONFIGURATION", rightX + 24.f, bodyY + bodyH - 116.f,
                       bw, 34.f, scale, opacity, true, true)) {
            std::string text, error;
            if (profiles::read(g_selectedProfile, text, error) && applyConfiguration(text)) notify("Configuration loaded");
            else notify(error.empty() ? "Configuration is invalid" : error.c_str());
        }
        if (drawButton(dl, origin, "configs-page-save", "SAVE CURRENT", rightX + 32.f + bw, bodyY + bodyH - 116.f,
                       bw, 34.f, scale, opacity, false, true)) {
            std::string error;
            if (profiles::write(g_selectedProfile, captureConfiguration().dump(2), error)) notify("Configuration saved");
            else notify(error.empty() ? "Could not save" : error.c_str());
        }
        if (drawButton(dl, origin, "configs-page-export", "EXPORT JSON", rightX + 24.f, bodyY + bodyH - 72.f,
                       116.f, 28.f, scale, opacity, false, true)) {
            std::string path, error;
            if (profiles::write(g_selectedProfile, captureConfiguration().dump(2), error) &&
                profiles::exportCopy(g_selectedProfile, path, error)) notify((std::string("Exported: ") + path).c_str());
            else notify(error.empty() ? "Could not export" : error.c_str());
        }
        if (drawButton(dl, origin, "configs-page-delete", "DELETE", rightX + 148.f, bodyY + bodyH - 72.f,
                       92.f, 28.f, scale, opacity, false, true)) {
            std::string error;
            if (profiles::remove(g_selectedProfile, error)) {
                notify("Configuration deleted");
                g_selectedProfile.clear();
            } else notify(error.empty() ? "Could not delete" : error.c_str());
        }
        drawText(dl, origin, rightX + 254.f, bodyY + bodyH - 64.f,
                 "Exports live in Passive/configs/exports.", kDim, 7.f, scale, opacity);
    }
}

void drawConfigDrawer(ImDrawList* dl, ImVec2 origin, float width, float height,
                      float scale, float opacity) {
    const float open = g_configAnim;
    if (open <= 0.015f) return;
    const float drawerWidth = 292.f;
    const float x = width - drawerWidth + (1.f - ease01(open)) * drawerWidth;
    const ImVec2 veil0 = origin;
    const ImVec2 veil1 = screenPos(origin, width, height, scale);
    dl->AddRectFilled(veil0, veil1, alpha(IM_COL32(6, 7, 10, 180), open * opacity));
    const ImVec2 p0 = screenPos(origin, x, 0.f, scale);
    const ImVec2 p1 = screenPos(origin, x + drawerWidth, height, scale);
    dl->AddRectFilled(p0, p1, alpha(IM_COL32(28, 31, 37, 252), open * opacity),
                      16.f * scale, ImDrawFlags_RoundCornersLeft);
    dl->AddRect(p0, p1, alpha(kLine, open * opacity * 0.8f), 16.f * scale,
                ImDrawFlags_RoundCornersLeft, 1.f * scale);
    drawText(dl, origin, x + 20.f, 24.f, "QUICK CONFIGURATION",
             accentColorU32(), 9.f, scale, open * opacity, true);
    drawText(dl, origin, x + 20.f, 44.f, "Interface preferences",
             kInk, 16.f, scale, open * opacity, true);
    if (drawIconButton(dl, origin, "close-drawer", 5, x + drawerWidth - 52.f,
                       18.f, 34.f, scale, open * opacity)) g_configOpen = false;

    const float rowW = drawerWidth - 40.f;
    drawToggleRow(dl, origin, "drawer-motion", "Smooth motion",
                  "Tab and panel transitions", x + 20.f, 94.f, rowW,
                  scale, open * opacity, g_animations);
    drawToggleRow(dl, origin, "drawer-glow", "Accent glow",
                  "Soft hover highlights", x + 20.f, 160.f, rowW,
                  scale, open * opacity, g_softGlow);
    drawToggleRow(dl, origin, "drawer-compact", "Compact spacing",
                  "Reduce gaps and row height", x + 20.f, 226.f, rowW,
                  scale, open * opacity, g_compact);
    drawText(dl, origin, x + 20.f, 310.f, "CURRENT MENU KEY",
             kDim, 8.f, scale, open * opacity, true);
    drawText(dl, origin, x + 20.f, 331.f, keyName(g_menuKey),
             kInk, 15.f, scale, open * opacity, true);
    if (drawButton(dl, origin, "drawer-controls", "OPEN CONTROLS",
                   x + 20.f, height - 76.f, rowW, 34.f,
                   scale, open * opacity, true, true)) {
        g_page = Page::Settings;
        g_settingsPage = SettingsPage::Controls;
        g_pageFade = g_animations ? 0.f : 1.f;
        g_configOpen = false;
    }
    if (drawButton(dl, origin, "drawer-done", "DONE",
                   x + 20.f, height - 34.f, rowW, 28.f,
                   scale, open * opacity, false, true)) g_configOpen = false;
}

// ============================================================================
//  Default interface - the module library
// ----------------------------------------------------------------------------
//  The layout from the reference: a permanent left rail (brand, navigation,
//  categories, profile), a thin command bar above the content, and a three-wide
//  grid of module cards. A card's body opens its settings in place of the grid;
//  the switch on the card flips the module and the star marks a favorite. The
//  card grid is only a shell - every option still comes from drawModuleCards, so
//  no setting is written twice and the Dropdown interface stays identical.
// ============================================================================
int iconForCategory(modules::Category category) {
    switch (category) {
        case modules::Category::Combat:   return 6;
        case modules::Category::Mace:     return 7;
        case modules::Category::Misc:     return 8;
        case modules::Category::Movement: return 9;
        case modules::Category::Spear:    return 10;
        case modules::Category::Visual:   return 11;
        case modules::Category::Crystal:  return 12;
        default:                          return 13;
    }
}

void drawStar(ImDrawList* dl, ImVec2 centre, float radius, ImU32 color, bool filled, float scale) {
    ImVec2 pts[10];
    for (int i = 0; i < 10; ++i) {
        const float angle = -1.57079633f + (float)i * 0.62831853f;
        const float r = (i % 2 == 0 ? radius : radius * 0.44f) * scale;
        pts[i] = ImVec2(centre.x + std::cos(angle) * r, centre.y + std::sin(angle) * r);
    }
    if (filled) {
        for (int i = 0; i < 5; ++i)
            dl->AddTriangleFilled(centre, pts[(i * 2 + 1) % 10], pts[(i * 2 + 2) % 10], color);
    } else {
        dl->AddPolyline(pts, 10, color, ImDrawFlags_Closed, 1.2f * scale);
    }
}

// The left rail. Every row is a real hit target; the selection follows the page on screen so the
// rail always says where the user is.
void drawSidebar(ImDrawList* dl, ImVec2 origin, float height, float scale, float opacity) {
    const float w = kSidebarW;
    const ImVec2 p0 = screenPos(origin, 0.f, 0.f, scale);
    const ImVec2 p1 = screenPos(origin, w, height, scale);
    dl->AddRectFilled(p0, p1, alpha(kSidebar, opacity), 18.f * scale, ImDrawFlags_RoundCornersLeft);
    dl->AddRectFilled(screenPos(origin, w - 1.f, 0.f, scale), screenPos(origin, w, height, scale),
                      alpha(kLine, opacity * 0.5f), 0.f);

    // ---- brand ----
    const float brandBreath = 0.86f + 0.14f * (0.5f + 0.5f * std::sin((float)ImGui::GetTime() * 1.25f));
    drawBrandMark(dl, origin, 18.f, 16.f, 36.f, scale, opacity * brandBreath);
    drawText(dl, origin, 64.f, 17.f, "PASSIVE", kInk, 16.f, scale, opacity, true);
    drawText(dl, origin, 64.f, 37.f, "NATIVE CLIENT", kDim, 7.f, scale, opacity, true);
    dl->AddRectFilled(screenPos(origin, 18.f, 64.f, scale), screenPos(origin, w - 18.f, 65.f, scale),
                      alpha(kLine, opacity * 0.42f), 0.f);

    // ---- navigation ----
    struct NavItem { const char* label; int icon; };
    const NavItem nav[7] = {
        { "Modules", 0 }, { "Configurations", 1 }, { "Friends", 9 },
        { "Keybinds", 2 }, { "HUD editor", 11 }, { "Themes", 12 }, { "Settings", 4 },
    };
    const auto navSelected = [](int index) -> bool {
        if (index == 0) return isCategoryPage(g_page);
        if (index == 1) return g_page == Page::Configs;
        if (index == 2) return g_page == Page::Settings && g_settingsPage == SettingsPage::Friends;
        if (index == 3) return g_page == Page::Settings && g_settingsPage == SettingsPage::Controls;
        if (index == 4) return g_page == Page::HudEditor;
        if (index == 5) return g_page == Page::Settings && g_settingsPage == SettingsPage::Appearance;
        return g_page == Page::Settings && g_settingsPage == SettingsPage::General;
    };
    float y = 80.f;
    for (int i = 0; i < 7; ++i) {
        const bool selected = navSelected(i);
        const Hit hit = hitBox((std::string("side-nav-") + nav[i].label).c_str(), 12.f, y, w - 24.f, 25.f, scale);
        const float nudge = hit.hover * 2.f;
        const ImVec2 r0 = screenPos(origin, 12.f, y, scale);
        const ImVec2 r1 = screenPos(origin, w - 12.f, y + 25.f, scale);
        if (selected || hit.hover > 0.01f)
            dl->AddRectFilled(r0, r1, alpha(blend(kSidebar, accentColorU32(), selected ? 0.16f : 0.07f * hit.hover), opacity), 7.f * scale);
        if (selected) {
            const float glow = 0.72f + 0.28f * (0.5f + 0.5f * std::sin((float)ImGui::GetTime() * 2.1f));
            dl->AddRectFilled(screenPos(origin, 12.f, y + 6.f, scale), screenPos(origin, 14.f, y + 19.f, scale),
                              alpha(accentColorU32(glow), opacity), 1.f * scale);
        }
        drawIcon(dl, screenPos(origin, 28.f + nudge, y + 12.5f, scale), nav[i].icon,
                 alpha(selected ? accentColorU32() : (hit.hover > 0.2f ? kInk : kMuted), opacity), 0.8f * scale);
        drawText(dl, origin, 42.f + nudge, y + 6.f, nav[i].label,
                 alpha(selected ? kInk : kMuted, opacity), 10.5f, scale, opacity, selected);
        if (hit.clicked) {
            g_gridDetail.clear();
            switch (i) {
                case 0: g_showAllModules = true; g_page = Page::Visual; g_pageFade = g_animations ? 0.f : 1.f; break;
                case 1: g_page = Page::Configs; g_pageFade = g_animations ? 0.f : 1.f; break;
                case 2: g_page = Page::Settings; g_settingsPage = SettingsPage::Friends; g_pageFade = g_animations ? 0.f : 1.f; break;
                case 3: g_page = Page::Settings; g_settingsPage = SettingsPage::Controls; g_pageFade = g_animations ? 0.f : 1.f; break;
                case 4: g_page = Page::HudEditor; g_pageFade = g_animations ? 0.f : 1.f; break;
                case 5: g_page = Page::Settings; g_settingsPage = SettingsPage::Appearance; g_pageFade = g_animations ? 0.f : 1.f; break;
                default: g_page = Page::Settings; g_settingsPage = SettingsPage::General; g_pageFade = g_animations ? 0.f : 1.f; break;
            }
        }
        y += 26.f;
    }

    // ---- categories ----
    drawText(dl, origin, 20.f, 296.f, "CATEGORIES", kDim, 7.5f, scale, opacity, true);
    y = 312.f;
    for (int i = 0; i < kCategoryCount; ++i) {
        const bool selected = !g_showAllModules && g_page == kCategoryPages[i] && g_gridDetail.empty();
        const Hit hit = hitBox((std::string("side-cat-") + kCategoryNames[i]).c_str(), 16.f, y, w - 30.f, 21.f, scale);
        if (selected)
            dl->AddRectFilled(screenPos(origin, 16.f, y, scale), screenPos(origin, w - 14.f, y + 21.f, scale),
                              alpha(blend(kSidebar, accentColorU32(), 0.10f), opacity), 6.f * scale);
        drawIcon(dl, screenPos(origin, 28.f, y + 10.5f, scale), kCategoryIcons[i],
                 alpha(selected ? accentColorU32() : (hit.hover > 0.2f ? kInk : kMuted), opacity), 0.62f * scale);
        drawText(dl, origin, 40.f, y + 5.f, kCategoryNames[i],
                 alpha(selected ? kInk : kMuted, opacity), 9.5f, scale, opacity, selected);
        char count[8];
        snprintf(count, sizeof(count), "%d", categoryModuleCount(i));
        const float cw = measureText(count, 7.5f, scale, true) / scale;
        drawText(dl, origin, w - 22.f - cw, y + 6.f, count, alpha(kDim, opacity), 7.5f, scale, opacity, true);
        if (hit.clicked) {
            g_page = kCategoryPages[i];
            g_showAllModules = false;
            g_gridDetail.clear();
            g_libraryFilter = 0;
            g_searchText[0] = '\0';
            g_pageFade = g_animations ? 0.f : 1.f;
        }
        y += 21.5f;
    }

    // ---- profile ----
    const float py = height - 76.f;
    dl->AddRectFilled(screenPos(origin, 12.f, py, scale), screenPos(origin, w - 12.f, py + 58.f, scale),
                      alpha(kCard, opacity), 9.f * scale);
    dl->AddRect(screenPos(origin, 12.f, py, scale), screenPos(origin, w - 12.f, py + 58.f, scale),
                alpha(kLine, opacity * 0.55f), 9.f * scale, 0, scale);
    const ImVec2 av0 = screenPos(origin, 22.f, py + 12.f, scale);
    const ImVec2 av1 = screenPos(origin, 56.f, py + 46.f, scale);
    dl->AddRectFilled(av0, av1, alpha(accentColorU32(0.24f), opacity), 8.f * scale);
    drawText(dl, origin, 32.f, py + 19.f, "M", kInk, 13.f, scale, opacity, true);
    std::string user = "Player";
    if (jni::ready() && mc::frame::valid()) user = mc::Client::username();
    if (user.empty()) user = "Player";
    drawText(dl, origin, 64.f, py + 13.f, user.c_str(), kInk, 10.f, scale, opacity, true);
    drawText(dl, origin, 64.f, py + 29.f, "PREMIUM  ·  LIFETIME", kDim, 6.5f, scale, opacity, true);
    dl->AddCircleFilled(screenPos(origin, w - 24.f, py + 29.f, scale), 3.f * scale, alpha(IM_COL32(94, 222, 140, 255), opacity));
}

// The command bar above the content: where the user is, search, and the quick configuration drawer.
void drawTopBar(ImDrawList* dl, ImVec2 origin, float width, float scale, float opacity) {
    drawIcon(dl, screenPos(origin, kSidebarW + 20.f, 24.f, scale), 0,
             alpha(kMuted, opacity), 0.66f * scale);
    const char* title = isCategoryPage(g_page) ? "Modules" : pageTitle(g_page);
    drawText(dl, origin, kSidebarW + 36.f, 16.f, title, kInk, 12.5f, scale, opacity, true);
    char crumb[64];
    if (g_page == Page::Settings) {
        const char* section = g_settingsPage == SettingsPage::General ? "GENERAL" :
            (g_settingsPage == SettingsPage::Appearance ? "APPEARANCE" :
            (g_settingsPage == SettingsPage::Controls ? "CONTROLS" :
            (g_settingsPage == SettingsPage::Friends ? "FRIENDS" : "ABOUT")));
        snprintf(crumb, sizeof(crumb), "SETTINGS  /  %s", section);
    } else if (g_page == Page::Configs) snprintf(crumb, sizeof(crumb), "PRESETS  /  PROFILES");
    else if (g_page == Page::HudEditor) snprintf(crumb, sizeof(crumb), "LAYOUT  /  SCREEN ELEMENTS");
    else if (!g_gridDetail.empty()) snprintf(crumb, sizeof(crumb), "MODULES  /  CONFIGURATION");
    else snprintf(crumb, sizeof(crumb), "MODULE LIBRARY");
    drawText(dl, origin, kSidebarW + 36.f, 32.f, crumb, kDim, 6.5f, scale, opacity, true);

    if (isCategoryPage(g_page)) {
        // search field
        const float searchW = 190.f;
        const float searchX = width - 28.f - 24.f - 8.f - 120.f - 10.f - searchW;
        ImGui::SetCursorPos(ImVec2(searchX * scale, 14.f * scale));
        ImGui::PushID("library-search");
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 15.f * scale);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(11.f * scale, 7.f * scale));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.105f, 0.115f, opacity));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.30f, 0.32f, 0.35f, opacity));
        ImGui::SetNextItemWidth(searchW * scale);
        ImGui::InputTextWithHint("##search", "search modules...", g_searchText, sizeof(g_searchText));
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        ImGui::PopID();
        if (drawButton(dl, origin, "quick-configs", "Configurations", width - 28.f - 24.f - 8.f - 120.f,
                       14.f, 120.f, 28.f, scale, opacity, false, true)) {
            g_page = Page::Configs;
            g_pageFade = g_animations ? 0.f : 1.f;
        }
    }
    if (drawIconButton(dl, origin, "top-close", 5, width - 40.f, 16.f, 24.f, scale, opacity)) toggleMenu();
    dl->AddRectFilled(screenPos(origin, kSidebarW, 56.f, scale), screenPos(origin, width, 57.f, scale),
                      alpha(kLine, opacity * 0.42f), 0.f);
}

// One card in the library grid.
void drawLibraryCard(ImDrawList* dl, ImVec2 origin, const modules::Entry& entry,
                     float x, float y, float w, float h, float scale, float opacity) {
    const bool on = entry.enabled && *entry.enabled;
    const Hit body = hitBox((std::string("lib-card-") + entry.name).c_str(), x, y, w - 58.f, h, scale);
    const ImVec2 p0 = screenPos(origin, x, y, scale);
    const ImVec2 p1 = screenPos(origin, x + w, y + h, scale);
    dl->AddRectFilled(p0, p1, alpha(blend(kCard, kCardLight, body.hover * 0.45f), opacity), 9.f * scale);
    dl->AddRect(p0, p1, alpha(blend(kLine, accentColorU32(), body.hover * 0.45f), opacity * 0.62f),
                9.f * scale, 0, scale);
    if (on)
        dl->AddRect(p0, p1, alpha(accentColorU32(0.42f), opacity), 9.f * scale, 0, scale);

    // icon tile
    dl->AddRectFilled(screenPos(origin, x + 12.f, y + 12.f, scale), screenPos(origin, x + 40.f, y + 40.f, scale),
                      alpha(blend(kCard, accentColorU32(), 0.10f), opacity), 7.f * scale);
    dl->AddRect(screenPos(origin, x + 12.f, y + 12.f, scale), screenPos(origin, x + 40.f, y + 40.f, scale),
                alpha(accentColorU32(0.22f), opacity), 7.f * scale, 0, scale);
    drawIcon(dl, screenPos(origin, x + 26.f, y + 26.f, scale), iconForCategory(entry.category),
             alpha(accentColorU32(), opacity), 0.72f * scale);

    drawText(dl, origin, x + 48.f, y + 12.f, ellipsize(entry.name, 11.5f, scale, w - 104.f).c_str(),
             kInk, 11.5f, scale, opacity, true);
    drawText(dl, origin, x + 48.f, y + 30.f, tagName(entry.tag), tagColor(entry.tag), 6.5f, scale, opacity, true);
    drawText(dl, origin, x + 12.f, y + 49.f, ellipsize(entry.description, 8.5f, scale, w - 24.f).c_str(),
             kMuted, 8.5f, scale, opacity);
    dl->AddRectFilled(screenPos(origin, x + 12.f, y + h - 20.f, scale), screenPos(origin, x + w - 12.f, y + h - 19.f, scale),
                      alpha(kLine, opacity * 0.30f), 0.f);
    drawText(dl, origin, x + 12.f, y + h - 14.f, entry.macro ? "MACRO  ·  OPEN" : "SETTINGS  ·  OPEN",
             kDim, 7.f, scale, opacity, true);

    // switch (top-right)
    const bool hasSwitch = entry.enabled != nullptr;
    const float swX = x + w - 50.f, swY = y + 12.f;
    float& anim = g_dropdownToggleAnim[entry.name];
    anim = g_animations ? ease(anim, on ? 1.f : 0.f, 16.f, ImGui::GetIO().DeltaTime) : (on ? 1.f : 0.f);
    if (hasSwitch) {
        const Hit hit = hitBox((std::string("lib-switch-") + entry.name).c_str(), swX, swY, 32.f, 17.f, scale);
        const ImVec2 s0 = screenPos(origin, swX, swY, scale);
        const ImVec2 s1 = screenPos(origin, swX + 32.f, swY + 17.f, scale);
        dl->AddRectFilled(s0, s1, alpha(blend(IM_COL32(50, 54, 62, 255), accentColorU32(), anim), opacity), 8.5f * scale);
        const float kx = swX + 8.5f + anim * 15.f;
        dl->AddCircleFilled(screenPos(origin, kx, swY + 8.5f, scale), 5.5f * scale,
                            alpha(IM_COL32(246, 247, 250, 255), opacity));
        if (hit.clicked) {
            const bool was = *entry.enabled;
            *entry.enabled = !*entry.enabled;
            if (was != *entry.enabled) modules::notifyModuleToggled(entry.name, *entry.enabled);
        }
    } else {
        drawText(dl, origin, swX + 2.f, swY + 4.f, "MACRO", alpha(accentColorU32(), opacity), 7.f, scale, opacity, true);
    }

    // favorite star (bottom-right)
    const bool fav = g_favorites[entry.name];
    const Hit star = hitBox((std::string("lib-star-") + entry.name).c_str(), x + w - 30.f, y + h - 26.f, 20.f, 20.f, scale);
    drawStar(dl, screenPos(origin, x + w - 20.f, y + h - 16.f, scale), 5.5f,
             alpha(fav ? accentColorU32() : (star.hover > 0.2f ? kInk : kDim), opacity), fav, scale);
    if (star.clicked) g_favorites[entry.name] = !fav;

    if (body.hovered && !body.clicked)
        ImGui::SetTooltip("%s\n%s", entry.name, entry.description);
    if (body.clicked) {
        g_gridDetail = entry.name;
        g_moduleExpanded[entry.name] = true;
        g_searchText[0] = '\0';
        g_cfgScroll.erase(entry.name);
        g_cfgPanelAnim.erase(entry.name);
        g_pageFade = g_animations ? 0.f : 1.f;
    }
}

// The library itself: heading, filter chips and the three-wide card grid (or one module's settings
// while a card is open). Runs inside the content scroll child, so the library scrolls as one piece.
void drawLibraryPage(ImDrawList* dl, ImVec2 origin, float scale, float opacity, float width) {
    if (!g_gridDetail.empty()) {
        const modules::Entry* entry = nullptr;
        for (const auto& candidate : modules::entries())
            if (g_gridDetail == candidate.name) { entry = &candidate; break; }
        if (!entry) { g_gridDetail.clear(); return; }
        if (drawButton(dl, origin, "library-back", "BACK", 0.f, 0.f, 64.f, 24.f, scale, opacity, false, true)) {
            g_gridDetail.clear();
            g_moduleExpanded[entry->name] = false;
            g_pageFade = g_animations ? 0.f : 1.f;
            return;
        }
        ImGui::SetCursorPos(ImVec2(0.f, 34.f * scale));
        drawModuleInspector(dl, origin, scale, opacity, 0.f, 34.f, width, *entry, false, 0.f);
        return;
    }

    // ---- heading ----
    drawText(dl, origin, 0.f, 0.f, "MODULE LIBRARY", kDim, 7.5f, scale, opacity, true);
    const char* heading = g_showAllModules ? "All modules" : pageTitle(g_page);
    drawText(dl, origin, 0.f, 14.f, heading, kInk, 20.f, scale, opacity, true);
    int total = 0, enabled = 0;
    for (const auto& entry : modules::entries()) {
        if (!g_showAllModules && entry.category != categoryForPage(g_page)) continue;
        ++total;
        if (entry.enabled && *entry.enabled) ++enabled;
    }
    char countText[64];
    snprintf(countText, sizeof(countText), "%d modules  ·  %d enabled", total, enabled);
    drawText(dl, origin, measureText(heading, 20.f, scale, true) / scale + 14.f, 22.f,
             countText, kDim, 8.f, scale, opacity);

    // ---- filter chips ----
    const char* chips[4] = { "All", "Enabled", "Favorites", "Gamemodes" };
    float cx = 0.f;
    for (int i = 0; i < 4; ++i) {
        const bool selected = i < 3 && g_libraryFilter == i;
        const bool enabledChip = i < 3;
        const float chipW = measureText(chips[i], 8.5f, scale, true) / scale + (i == 3 ? 34.f : 24.f);
        const Hit hit = hitBox((std::string("lib-chip-") + chips[i]).c_str(), cx, 45.f, chipW, 24.f, scale);
        const ImVec2 c0 = screenPos(origin, cx, 45.f, scale);
        const ImVec2 c1 = screenPos(origin, cx + chipW, 69.f, scale);
        dl->AddRectFilled(c0, c1, alpha(selected ? accentColorU32(0.92f)
                                                : blend(kCard, kCardLight, hit.hover * 0.8f), opacity), 7.f * scale);
        if (!selected) dl->AddRect(c0, c1, alpha(kLine, opacity * 0.55f), 7.f * scale, 0, scale);
        drawText(dl, origin, cx + 12.f, 51.f, chips[i],
                 selected ? kOnAccent : (enabledChip ? kMuted : kDim), 8.5f, scale, opacity, selected);
        if (i == 3) {
            const ImVec2 ch = screenPos(origin, cx + chipW - 13.f, 57.f, scale);
            dl->AddLine(ImVec2(ch.x - 3.f * scale, ch.y - 1.5f * scale), ImVec2(ch.x, ch.y + 1.5f * scale), alpha(kDim, opacity), 1.3f * scale);
            dl->AddLine(ImVec2(ch.x, ch.y + 1.5f * scale), ImVec2(ch.x + 3.f * scale, ch.y - 1.5f * scale), alpha(kDim, opacity), 1.3f * scale);
        }
        if (hit.clicked && enabledChip) g_libraryFilter = i;
        cx += chipW + 7.f;
    }
    if (!g_showAllModules) {
        const float backW = measureText("CLEAR FILTER", 7.5f, scale, true) / scale + 20.f;
        if (drawButton(dl, origin, "lib-clear-cat", "CLEAR FILTER", width - backW, 45.f, backW, 24.f, scale, opacity, false, true)) {
            g_showAllModules = true;
            g_libraryFilter = 0;
            g_page = Page::Visual;
        }
    }

    // ---- grid ----
    std::vector<const modules::Entry*> list;
    for (const auto& entry : modules::entries()) {
        if (!g_showAllModules && entry.category != categoryForPage(g_page)) continue;
        if (g_libraryFilter == 1 && !(entry.enabled && *entry.enabled)) continue;
        if (g_libraryFilter == 2 && !g_favorites[entry.name]) continue;
        if (g_searchText[0] && !containsInsensitive(entry.name, g_searchText) &&
            !containsInsensitive(entry.description, g_searchText)) continue;
        list.push_back(&entry);
    }
    std::sort(list.begin(), list.end(), [](auto a, auto b) {
        return std::string(a->name) < std::string(b->name);
    });
    const float gap = 12.f;
    const float cellW = (width - gap * 2.f) / 3.f;
    const float cellH = 92.f;
    const float startY = 84.f;
    if (list.empty()) {
        drawCard(dl, origin, 0.f, startY, width, 96.f, scale, opacity);
        drawText(dl, origin, 20.f, startY + 24.f, "NO MODULES MATCH", kDim, 9.f, scale, opacity, true);
        drawText(dl, origin, 20.f, startY + 48.f, "Try another search, or clear the filter chip.", kMuted, 10.f, scale, opacity);
        ImGui::SetCursorPos(ImVec2(0.f, (startY + 110.f) * scale));
        ImGui::Dummy(ImVec2(1.f, 1.f));
        return;
    }
    for (size_t i = 0; i < list.size(); ++i) {
        const int col = (int)(i % 3), row = (int)(i / 3);
        drawLibraryCard(dl, origin, *list[i], (float)col * (cellW + gap),
                        startY + (float)row * (cellH + gap), cellW, cellH, scale, opacity);
    }
    const int rows = ((int)list.size() + 2) / 3;
    ImGui::SetCursorPos(ImVec2(0.f, (startY + (float)rows * (cellH + gap) + 6.f) * scale));
    ImGui::Dummy(ImVec2(1.f, 1.f));
}

// ---- HUD Editor ---------------------------------------------------------------------------
// One small widget definition, drawn identically on screen and on the editor's canvas, so the
// preview is the real thing rather than a picture of it.
float hudWidgetWidth(const char* id) {
    const std::string key = id ? id : "";
    if (key == "coordinates") return 128.f;
    if (key == "keystrokes") return 118.f;
    if (key == "armor") return 96.f;
    return 92.f;
}

std::string hudWidgetValue(const char* id) {
    const std::string key = id ? id : "";
    if (key == "fps") {
        char value[32];
        snprintf(value, sizeof(value), "%.0f", ImGui::GetIO().Framerate);
        return value;
    }
    if (key == "coordinates") {
        if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return "—";
        jobject playerObj = mc::Client::player();
        if (!playerObj) return "—";
        const Vec3 p = mc::Player(playerObj).pos();
        char value[64];
        snprintf(value, sizeof(value), "%d  %d  %d", (int)std::floor(p.x), (int)std::floor(p.y), (int)std::floor(p.z));
        return value;
    }
    if (key == "armor") {
        if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return "—";
        jobject playerObj = mc::Client::player();
        if (!playerObj) return "—";
        char value[32];
        snprintf(value, sizeof(value), "%d", mc::Player(playerObj).armorPoints());
        return value;
    }
    return "";
}

void drawHudWidget(ImDrawList* dl, ImVec2 origin, const char* id, const char* label, float x, float y,
                   float scale, float opacity) {
    const float w = hudWidgetWidth(id), h = 38.f;
    const ImVec2 p0 = screenPos(origin, x, y, scale);
    const ImVec2 p1 = screenPos(origin, x + w, y + h, scale);
    dl->AddRectFilled(p0, p1, alpha(IM_COL32(12, 14, 18, 210), opacity), 7.f * scale);
    dl->AddRect(p0, p1, alpha(blend(kLine, accentColorU32(), 0.35f), opacity * 0.75f), 7.f * scale, 0, scale);
    dl->AddRectFilled(ImVec2(p0.x, p0.y + 5.f * scale), ImVec2(p0.x + 2.f * scale, p1.y - 5.f * scale),
                      alpha(accentColorU32(0.8f), opacity), scale);
    drawText(dl, origin, x + 10.f, y + 6.f, label, kDim, 7.f, scale, opacity, true);
    if (std::string(id ? id : "") == "keystrokes") {
        const float key = 18.f;
        const float kx = x + w - key * 3.f - 10.f, ky = y + 16.f;
        const struct { int code; const char* text; float ox, oy; } keys[] = {
            { 'W', "W", 1.f, 0.f }, { 'A', "A", 0.f, 1.f }, { 'S', "S", 1.f, 1.f }, { 'D', "D", 2.f, 1.f },
        };
        for (const auto& k : keys) {
            const ImVec2 k0 = screenPos(origin, kx + k.ox * (key + 2.f), ky + k.oy * (key + 2.f), scale);
            const ImVec2 k1(k0.x + key * scale, k0.y + key * scale);
            const bool down = keyDown(k.code);
            dl->AddRectFilled(k0, k1, alpha(down ? accentColorU32(0.9f) : IM_COL32(30, 33, 40, 230), opacity), 3.f * scale);
            drawText(dl, origin, kx + k.ox * (key + 2.f) + 5.5f, ky + k.oy * (key + 2.f) + 3.f,
                     k.text, down ? kOnAccent : kMuted, 7.5f, scale, opacity, true);
        }
        return;
    }
    const std::string value = hudWidgetValue(id);
    drawText(dl, origin, x + 10.f, y + 19.f, value.c_str(), kInk, 12.5f, scale, opacity, true);
}

void drawHudOverlay() {
    if (!g_hudEditorEnabled || g_menuOpen || g_menuAnim > 0.2f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x < 2.f || io.DisplaySize.y < 2.f) return;
    const float scale = std::max(0.8f, io.DisplaySize.y / 720.f);
    for (const HudElement& element : g_hudElements) {
        if (!element.enabled) continue;
        const float w = hudWidgetWidth(element.id);
        const float x = std::clamp(element.pos.x, 0.f, 1.f) * std::max(1.f, io.DisplaySize.x / scale - w);
        const float y = std::clamp(element.pos.y, 0.f, 1.f) * std::max(1.f, io.DisplaySize.y / scale - 38.f);
        drawHudWidget(dl, ImVec2(0.f, 0.f), element.id, element.label, x, y, scale, 0.96f);
    }
}

void drawHudEditorPage(ImDrawList* dl, ImVec2 origin, float scale, float opacity,
                       float x, float y, float width, float height) {
    drawText(dl, origin, x, y, "HUD EDITOR", kDim, 7.5f, scale, opacity, true);
    drawText(dl, origin, x, y + 14.f, "Screen elements", kInk, 21.f, scale, opacity, true);
    drawText(dl, origin, x + measureText("Screen elements", 21.f, scale, true) / scale + 14.f, y + 22.f,
             "place the small readouts anywhere  ·  separate from the HUD module", kDim, 8.f, scale, opacity);
    if (drawButton(dl, origin, "hud-reset", "RESET PLACEMENT", x + width - 134.f, y + 8.f, 134.f, 30.f,
                   scale, opacity, false, true)) {
        const ImVec2 defaults[] = { ImVec2(0.015f, 0.030f), ImVec2(0.015f, 0.100f),
                                    ImVec2(0.015f, 0.170f), ImVec2(0.860f, 0.820f) };
        for (int i = 0; i < kHudElementCount; ++i) g_hudElements[i].pos = defaults[i];
        notify("HUD element positions reset");
    }

    const float bodyY = y + 58.f, bodyH = std::max(230.f, height - 58.f);
    const float leftW = 246.f, gap = 14.f;
    drawCard(dl, origin, x, bodyY, leftW, bodyH, scale, opacity, true);
    drawText(dl, origin, x + 18.f, bodyY + 16.f, "ELEMENTS", kDim, 8.f, scale, opacity, true);
    drawText(dl, origin, x + 18.f, bodyY + 32.f, "Toggle what appears on screen.", kMuted, 9.f, scale, opacity);
    drawToggleRow(dl, origin, "hud-master", "HUD ELEMENTS", "Master switch for the readouts",
                  x + 14.f, bodyY + 52.f, leftW - 28.f, scale, opacity, g_hudEditorEnabled);
    float rowY = bodyY + 100.f;
    for (int i = 0; i < kHudElementCount; ++i) {
        HudElement& element = g_hudElements[i];
        const Hit hit = hitBox((std::string("hud-toggle-") + element.id).c_str(), x + 12.f, rowY,
                               leftW - 24.f, 34.f, scale);
        const ImVec2 r0 = screenPos(origin, x + 12.f, rowY, scale);
        const ImVec2 r1 = screenPos(origin, x + leftW - 12.f, rowY + 34.f, scale);
        if (hit.hover > 0.01f) dl->AddRectFilled(r0, r1, alpha(kCardLight, opacity * 0.5f), 6.f * scale);
        dl->AddCircleFilled(screenPos(origin, x + 27.f, rowY + 17.f, scale), 2.4f * scale,
                            alpha(element.enabled ? accentColorU32() : kDim, opacity));
        drawText(dl, origin, x + 38.f, rowY + 9.f, element.label, element.enabled ? kInk : kMuted,
                 10.f, scale, opacity, element.enabled);
        drawText(dl, origin, x + leftW - 48.f, rowY + 10.f, element.enabled ? "ON" : "OFF",
                 element.enabled ? accentColorU32() : kDim, 7.f, scale, opacity, true);
        if (hit.clicked) element.enabled = !element.enabled;
        rowY += 38.f;
    }
    drawText(dl, origin, x + 18.f, bodyY + bodyH - 64.f, "MORE READOUTS", kDim, 7.f, scale, opacity, true);
    drawText(dl, origin, x + 18.f, bodyY + bodyH - 48.f, "Ping, potions and armour arrive next update.", kDim, 8.f, scale, opacity);

    const float canvasX = x + leftW + gap, canvasW = width - leftW - gap;
    drawCard(dl, origin, canvasX, bodyY, canvasW, bodyH, scale, opacity, true);
    drawText(dl, origin, canvasX + 18.f, bodyY + 16.f, "LAYOUT", kDim, 8.f, scale, opacity, true);
    drawText(dl, origin, canvasX + 18.f, bodyY + 32.f, "Drag a readout to move it.", kMuted, 9.f, scale, opacity);
    const float viewX = canvasX + 18.f, viewY = bodyY + 54.f;
    const float viewW = canvasW - 36.f, viewH = std::max(120.f, bodyH - 80.f);
    dl->AddRectFilled(screenPos(origin, viewX, viewY, scale),
                      screenPos(origin, viewX + viewW, viewY + viewH, scale),
                      alpha(IM_COL32(12, 14, 18, 210), opacity), 9.f * scale);
    dl->AddRect(screenPos(origin, viewX, viewY, scale),
                screenPos(origin, viewX + viewW, viewY + viewH, scale),
                alpha(kLine, opacity * 0.65f), 9.f * scale, 0, scale);
    for (float gx = viewX + 20.f; gx < viewX + viewW; gx += 32.f)
        dl->AddLine(screenPos(origin, gx, viewY, scale), screenPos(origin, gx, viewY + viewH, scale),
                    alpha(kLine, opacity * 0.10f), scale);
    for (float gy = viewY + 20.f; gy < viewY + viewH; gy += 32.f)
        dl->AddLine(screenPos(origin, viewX, gy, scale), screenPos(origin, viewX + viewW, gy, scale),
                    alpha(kLine, opacity * 0.10f), scale);
    drawText(dl, origin, viewX + 12.f, viewY + viewH - 17.f, "SCREEN PREVIEW", kDim, 6.5f, scale, opacity, true);

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) g_hudDrag = -1;
    for (int i = 0; i < kHudElementCount; ++i) {
        HudElement& element = g_hudElements[i];
        if (!element.enabled) continue;
        const float w = hudWidgetWidth(element.id);
        const float elX = viewX + 10.f + element.pos.x * std::max(1.f, viewW - 20.f - w);
        const float elY = viewY + 10.f + element.pos.y * std::max(1.f, viewH - 20.f - 38.f);
        const Hit hit = hitBox((std::string("hud-drag-") + element.id).c_str(), elX, elY, w, 38.f, scale);
        if (hit.clicked) g_hudDrag = i;
        drawHudWidget(dl, origin, element.id, element.label, elX, elY, scale, opacity * (g_hudDrag == i ? 0.75f : 1.f));
        if (g_hudDrag == i) {
            const float nx = (mouse.x / scale - viewX - 10.f) / std::max(1.f, viewW - 20.f - w);
            const float ny = (mouse.y / scale - viewY - 10.f) / std::max(1.f, viewH - 20.f - 38.f);
            element.pos = ImVec2(std::clamp(nx, 0.f, 1.f), std::clamp(ny, 0.f, 1.f));
        }
    }
}

void drawToasts() {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImGuiIO& io = ImGui::GetIO();
    float y = io.DisplaySize.y - 16.f;
    const char* kDebugTag = "DEBUG MODE";
    const ImVec2 tagSize = ImGui::CalcTextSize(kDebugTag);
    for (auto it = g_toasts.rbegin(); it != g_toasts.rend(); ++it) {
        const float fade = clamp01(std::min(it->life / 0.35f, 1.f));
        const ImVec2 textSize = ImGui::CalcTextSize(it->text.c_str());
        const float chipW = it->tagged ? tagSize.x + 14.f : 0.f;
        const float width = textSize.x + chipW + 34.f;
        const float height = textSize.y + 16.f;
        const ImVec2 p0(io.DisplaySize.x - width - 16.f, y - height);
        const ImVec2 p1(io.DisplaySize.x - 16.f, y);
        dl->AddRectFilled(p0, p1, alpha(kPanel, fade * 0.95f), 10.f);
        dl->AddRect(p0, p1, alpha(accentColorU32(0.8f), fade), 10.f, 0, 1.2f);
        dl->AddCircleFilled(ImVec2(p0.x + 11.f, (p0.y + p1.y) * 0.5f),
                            2.5f, alpha(accentColorU32(), fade));
        float textX = p0.x + 20.f;
        if (it->tagged) {
            // Green DEBUG MODE chip in front of the message.
            const ImVec2 chip0(textX, p0.y + 6.f);
            const ImVec2 chip1(textX + chipW, p1.y - 6.f);
            dl->AddRectFilled(chip0, chip1, alpha(IM_COL32(34, 92, 62, 235), fade), 6.f);
            dl->AddRect(chip0, chip1, alpha(IM_COL32(96, 224, 150, 200), fade), 6.f, 0, 1.f);
            dl->AddText(ImVec2(chip0.x + 7.f, chip0.y + 2.f),
                        alpha(IM_COL32(118, 238, 168, 255), fade), kDebugTag);
            textX += chipW + 8.f;
        }
        dl->AddText(ImVec2(textX, p0.y + 8.f), alpha(kInk, fade), it->text.c_str());
        y -= height + 8.f;
    }
}

void drawMainWindow(float menuOpacity) {
    const ImGuiIO& io = ImGui::GetIO();
    // The open animation is a real pop: the whole panel grows from 94% to 100% while it rises the
    // last few pixels into place, so the interface arrives after the splash instead of cutting in.
    const float open = ease01(menuOpacity);
    const float appear = 0.94f + 0.06f * open;
    const float maxByWidth = io.DisplaySize.x * 0.92f / kBaseWidth;
    const float maxByHeight = io.DisplaySize.y * 0.90f / kBaseHeight;
    const float scale = clampf(std::min({ g_uiScale, maxByWidth, maxByHeight }) * appear, 0.55f, 1.75f);
    const float width = kBaseWidth * scale;
    const float height = kBaseHeight * scale;
    const ImVec2 pos((io.DisplaySize.x - width) * 0.5f,
                     (io.DisplaySize.y - height) * 0.5f + (1.f - open) * 20.f * scale);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavFocus;
    if (ImGui::Begin("Passive Interface###passive_root", nullptr, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 origin = ImGui::GetWindowPos();
        const float panelAlpha = menuOpacity * g_panelOpacity;

        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        for (int i = 3; i >= 0; --i) {
            const float spread = (float)(i + 1) * 3.f * scale;
            const ImVec2 s0(origin.x - spread, origin.y - spread);
            const ImVec2 s1(origin.x + width + spread, origin.y + height + spread);
            bg->AddRectFilled(s0, s1,
                IM_COL32(0, 0, 0, (int)((14 - i * 2) * menuOpacity)),
                (18.f + spread) * scale);
        }

        const ImVec2 p0 = origin;
        const ImVec2 p1(origin.x + width, origin.y + height);
        // Fade Color background: a slow blend between the theme background and an accent tint
        // of it, on a lazy wave, so the panel colour fades over time instead of sitting still.
        ImU32 panelFill = kPanel;
        if (g_fadeColorBackground) {
            const float t = 0.5f + 0.5f * std::sin((float)ImGui::GetTime() * 0.5f);
            panelFill = blend(kPanel, blend(kPanel, accentColorU32(), 0.35f), t);
        }
        dl->AddRectFilled(p0, p1, alpha(panelFill, panelAlpha), 18.f * scale);
        dl->AddRect(p0, p1, alpha(IM_COL32(122, 132, 150, 255), panelAlpha * 0.58f),
                    18.f * scale, 0, 1.f * scale);
        // A slow platinum glint travels along the top edge, so the shell is alive even before
        // the pointer touches anything. It is one quad and one clip, and it fades with the panel.
        {
            const float travel = (float)((ImGui::GetTime() * 0.11) - std::floor(ImGui::GetTime() * 0.11));
            const float shimmerW = 130.f * scale;
            const float shimmerX = p0.x - shimmerW + (width + shimmerW) * travel;
            dl->PushClipRect(p0, ImVec2(p1.x, p0.y + 2.f * scale), true);
            dl->AddRectFilled(ImVec2(shimmerX, p0.y), ImVec2(shimmerX + shimmerW, p0.y + 1.5f * scale),
                              alpha(accentColorU32(0.55f), panelAlpha), 0.f);
            dl->PopClipRect();
        }
        // Which interface is drawing: the library shell or the Dropdown tab layout.
        const bool dropdownUi = g_interfaceStyle == InterfaceStyle::Dropdown;

        if (dropdownUi) drawHeader(dl, origin, width / scale, scale, panelAlpha);
        else {
            drawSidebar(dl, origin, height / scale, scale, panelAlpha);
            drawTopBar(dl, origin, width / scale, scale, panelAlpha);
        }

        const float pageOpacity = g_animations ? ease01(g_pageFade) : 1.f;
        const float slide = (1.f - pageOpacity) * 10.f;
        const float contentX = dropdownUi ? 24.f : kSidebarW + 24.f;
        const float contentY = (dropdownUi ? 78.f : 72.f) + slide;
        const float contentW = width / scale - contentX - (dropdownUi ? 24.f : 24.f);
        const float contentH = height / scale - contentY - 26.f;
        const float contentAlpha = panelAlpha * pageOpacity;
        if (g_page==Page::Settings)
            drawSettingsPage(dl,origin,scale,contentAlpha,contentX,contentY,contentW,contentH);
        else if (g_page==Page::Configs)
            drawConfigsPage(dl,origin,scale,contentAlpha,contentX,contentY,contentW,contentH);
        else if (g_page==Page::HudEditor)
            drawHudEditorPage(dl,origin,scale,contentAlpha,contentX,contentY,contentW,contentH);
        else {
            ImGui::SetCursorPos(ImVec2(contentX*scale,contentY*scale));
            if (ImGui::BeginChild("module-page-scroll",ImVec2(contentW*scale,contentH*scale),false,
                                  ImGuiWindowFlags_NoBackground)) {
                ImVec2 pageOrigin=ImGui::GetWindowPos();
                pageOrigin.y-=ImGui::GetScrollY();
                if (dropdownUi)
                    drawDropdownModulePage(ImGui::GetWindowDrawList(),pageOrigin,scale,contentAlpha,
                                           0.f,0.f,contentW,contentH);
                else
                    drawLibraryPage(ImGui::GetWindowDrawList(),pageOrigin,scale,contentAlpha,contentW);
            }
            ImGui::EndChild();
        }

        drawText(dl, origin, contentX, height / scale - 18.f,
                 "MINECRAFT 1.21.11  /  FABRIC", kDim, 7.5f, scale, panelAlpha, true);
        char hintBuf[64];
        snprintf(hintBuf, sizeof(hintBuf), "%s  MENU   /   RSHIFT  TO CLOSE", keyName(g_menuKey));
        const char* hint = hintBuf;
        const float hintWidth = measureText(hint, 7.5f, scale, true) / scale;
        drawText(dl, origin, width / scale - hintWidth - 26.f,
                 height / scale - 18.f, hint, kDim, 7.5f, scale, panelAlpha, true);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

} // namespace

void onKey(int vk, bool down) {
    if (vk <= 0 || vk > 255) return;
    const bool was = g_keys[vk];
    g_keys[vk] = down;
    if (!down || was) return;

    g_taps.push_back(vk);
    if (g_captureMenuKey || !g_captureModuleBind.empty()) return;
    if (vk == g_menuKey) {
        toggleMenu();
    } else if (vk == VK_ESCAPE && g_menuOpen && g_closeWithEscape) {
        toggleMenu();
    }
}

void onMouse(int vk, bool down) {
    if (vk < 0 || vk > 6) return;
    const bool was = g_mouse[vk];
    g_mouse[vk] = down;
    if (down && !was) g_mouseTaps.push_back(vk);
}

bool keyDown(int vk) { return vk > 0 && vk < 256 && g_keys[vk]; }
bool mouseDown(int vk) { return vk >= 0 && vk < 7 && g_mouse[vk]; }
bool anyDown(int vk) { return keyDown(vk) || mouseDown(vk); }
const std::vector<int>& keyTaps() { return g_taps; }
const std::vector<int>& mouseTaps() { return g_mouseTaps; }

std::vector<int> allTaps() {
    std::vector<int> taps = g_taps;
    taps.insert(taps.end(), g_mouseTaps.begin(), g_mouseTaps.end());
    return taps;
}

int menuKey() { return g_menuKey; }
void setMenuKey(int vk) {
    if (vk > 0 && vk <= 255) g_menuKey = vk;
}

bool wantsMouse() {
    return g_menuAnim > 0.5f && ImGui::GetIO().WantCaptureMouse;
}
bool wantsKeyboard() {
    return g_menuAnim > 0.5f && ImGui::GetIO().WantCaptureKeyboard;
}
bool inputFrozen() {
    return g_menuOpen || g_menuAnim > 0.5f;
}
bool cursorWanted() { return g_cursorWanted; }

void toggleMenu() {
    static ULONGLONG lastToggle = 0;
    const ULONGLONG now = GetTickCount64();
    if (now - lastToggle < 150) return;
    lastToggle = now;
    g_menuOpen = !g_menuOpen;
    trace::log("menu %s", g_menuOpen ? "OPEN" : "closed");
}

bool menuOpen() { return g_menuOpen; }
float menuAnim() { return g_menuAnim; }
float tickDelta() { return g_tickDelta; }
void setTickDelta(float value) { g_tickDelta = clamp01(value); }

void notify(const char* message) {
    g_toasts.push_back({ message ? message : "", 2.8f });
    if (g_toasts.size() > 5) g_toasts.erase(g_toasts.begin());
}

// Module action toasts (Debug Mode): same toast, plus the green DEBUG MODE chip.
void notifyTagged(const char* message) {
    g_toasts.push_back({ message ? message : "", 3.6f, true });
    if (g_toasts.size() > 5) g_toasts.erase(g_toasts.begin());
}

// The animation state of every animated widget (checkbox knobs, dropdown chevrons and popups) is
// keyed by ImGui id and would otherwise remember where it was when the menu closed, so the next open
// started with the old values. It is cleared once the menu has fully faded out.
void init() {
    applyThemeColors();
    g_accentIndex = 0;
    g_splashStart = ImGui::GetTime();
    notify("Passive interface ready  /  RSHIFT");
}

// ---------------------------------------------------------------------------
// OS cursor
// ---------------------------------------------------------------------------
// Windows keeps a *counted* display counter (ShowCursor is +1/-1, the pointer is
// visible while the count is >= 0), so every call here changes state permanently.
// `g_cursorWanted` is set once a frame by preFrame() and read by hooks::hkWndProc,
// which answers WM_SETCURSOR itself while a cursor is wanted; both run on the
// window's own thread, so a plain bool is enough.
namespace {

// GetCursorInfo can fail - a locked session, no visible desktop, a window that has
// not presented yet. An unreadable state must not be read as "hidden", or the
// counter would be walked one way for as long as the query keeps failing.
bool cursorShownNow(bool& shown) {
    CURSORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetCursorInfo(&info)) return false;
    shown = (info.flags & CURSOR_SHOWING) != 0;
    return true;
}

// Walk the display counter to the wanted end of its range and stop as soon as the
// pointer is in that state - the "already right, so call nothing" case is the whole
// point. The old code called ShowCursor unconditionally on every transition, which
// pushed the counter one step below -1 per toggle until a single ShowCursor(TRUE)
// from the game could no longer bring the pointer back (the reported "invisible in
// every screen, clicks still land, gone until restart"). See cursor_rules.h.
// Returns true once the pointer is in the wanted state (or the state cannot be read,
// in which case nothing more is owed). `changed` reports whether the display counter
// was actually touched, which is what creates (or clears) the menu's debt.
bool applyCursor(cursor::Want want, bool* changed = nullptr) {
    if (want == cursor::Want::Leave) return true;           // the game's state stands
    const bool wantedShown = want == cursor::Want::Shown;
    for (int i = 0; i < cursor::kMaxSteps; ++i) {
        bool shown = false;
        if (!cursorShownNow(shown)) return true;            // unreadable: touch nothing
        if (!cursor::needsChange(shown, want)) return true; // already there: call nothing
        ShowCursor(wantedShown ? TRUE : FALSE);
        if (changed) *changed = true;
    }
    return false;                                            // more steps next frame
}
} // namespace

void shutdown() {
    g_toasts.clear();
    g_menuOpen=false;
    g_menuAnim.v=0.f;
    ClipCursor(nullptr);
    g_cursorClipped=false;
    g_cursorWanted=false;
    g_menuShowedCursor=false;
    applyCursor(cursor::Want::Shown);   // hand a visible pointer back
}

void preFrame() {
    const ImGuiIO& io = ImGui::GetIO();
    static double tickAccumulator = 0.0;
    tickAccumulator += io.DeltaTime * 20.0;
    g_tickDelta = (float)(tickAccumulator - std::floor(tickAccumulator));

    // Minecraft owns the pointer. The client only shows one for its own menu, and only
    // takes a pointer back that it showed itself - every screen, every perspective and
    // the in-world view stay exactly as the game left them (cursor_rules.h).
    const bool menuOpen = inputFrozen();
    const bool screenKnown = jni::ready() && mc::frame::valid();
    const bool screenOpen = screenKnown ? mc::Client::screenOpen() : false;
    const cursor::Want want = cursor::want(menuOpen, screenOpen, screenKnown, g_menuShowedCursor);
    g_cursorWanted = want == cursor::Want::Shown;
    // One line per change, so a report of "the pointer is wrong" says which side of the
    // decision the client was on instead of needing another build to find out.
    static int loggedWant = -1;
    if (loggedWant != (int)want) {
        loggedWant = (int)want;
        trace::log("cursor: %s (menu=%d screen=%d screenKnown=%d showedForMenu=%d)",
                   g_cursorWanted ? "shown" : (want == cursor::Want::Hidden ? "taken back" : "game's own"),
                   menuOpen, screenOpen, screenKnown, g_menuShowedCursor);
    }
    if (HWND hwnd = (HWND)hooks::hwnd()) {
        // A background window must not change the desktop's pointer, and Windows does
        // not release a clip when focus is lost, so the clip is dropped instead of
        // being left holding the pointer over whatever the player alt-tabbed to.
        const bool focused = GetForegroundWindow() == hwnd;
        if (g_cursorWanted && focused) {
            RECT windowRect{};
            if (GetWindowRect(hwnd, &windowRect)) ClipCursor(&windowRect);
            g_cursorClipped = true;
        } else if (g_cursorClipped) {
            ClipCursor(nullptr);
            g_cursorClipped = false;
        }
        // A game screen can be showing its own pointer with no shape behind it: GLFW answers
        // CURSOR_DISABLED with SetCursor(NULL) and nothing re-sets one on a frame where the
        // mouse does not move. Filling it in is safe by construction - it only happens when
        // GetCursorInfo already reports the pointer as shown - so it can never reveal one the
        // game is hiding (in-world the counter is -1, so this does nothing there).
        if (focused && screenOpen && !g_cursorWanted) {
            bool shown = false;
            if (cursorShownNow(shown) && shown && !GetCursor())
                SetCursor(LoadCursorA(nullptr, IDC_ARROW));
        }
        if (focused && want != cursor::Want::Leave) {
            // The menu also needs a shape: GLFW answers WM_SETCURSOR in CURSOR_DISABLED
            // with SetCursor(NULL), and nothing re-sets one on a frame where the mouse
            // does not move. Only the menu asserts the arrow - a widget that wants another
            // shape (I-beam, resize) has already set one and keeps it.
            if (g_cursorWanted && !GetCursor())
                SetCursor(LoadCursorA(nullptr, IDC_ARROW));
            bool changed = false;
            const bool settled = applyCursor(want, &changed);
            if (g_cursorWanted && changed)    g_menuShowedCursor = true;   // remember the debt
            else if (!g_cursorWanted && settled) g_menuShowedCursor = false; // paid it back
        }
    }

    static bool wasMenuKeyDown = false;
    const SHORT keyState = GetAsyncKeyState(g_menuKey);
    const bool menuKeyDownNow = (keyState & 0x8000) != 0;
    if (!g_captureMenuKey && g_captureModuleBind.empty() && menuKeyDownNow && !wasMenuKeyDown) {
        toggleMenu();
    }
    wasMenuKeyDown = menuKeyDownNow;

    captureModuleKey();
    for (ToastData& toast : g_toasts) toast.life -= io.DeltaTime;
    g_toasts.erase(std::remove_if(g_toasts.begin(), g_toasts.end(),
        [](const ToastData& toast) { return toast.life <= 0.f; }), g_toasts.end());

    if (g_animations) {
        g_menuAnim.step(g_menuOpen, io.DeltaTime, 15.f);
        g_configAnim.step(g_configOpen, io.DeltaTime, 18.f);
        g_pageFade = std::min(1.f, g_pageFade + io.DeltaTime * 7.5f);
    } else {
        g_menuAnim.v = g_menuOpen ? 1.f : 0.f;
        g_configAnim.v = g_configOpen ? 1.f : 0.f;
        g_pageFade = 1.f;
    }
    tickTheme(io.DeltaTime);
    ImGuiStyle& style=ImGui::GetStyle();
    style.Colors[ImGuiCol_Text]=unpackThemeColor(kInk);
    style.Colors[ImGuiCol_TextDisabled]=unpackThemeColor(kMuted);
    style.Colors[ImGuiCol_WindowBg]=unpackThemeColor(kPanel);
    style.Colors[ImGuiCol_ChildBg]=unpackThemeColor(kCard);
    style.Colors[ImGuiCol_PopupBg]=unpackThemeColor(kPanel);
    style.Colors[ImGuiCol_Border]=unpackThemeColor(kLine);
    style.Colors[ImGuiCol_FrameBg]=unpackThemeColor(kCard);
    style.Colors[ImGuiCol_FrameBgHovered]=unpackThemeColor(kCardLight);
    style.Colors[ImGuiCol_FrameBgActive]=unpackThemeColor(kCardLight);
    style.Colors[ImGuiCol_CheckMark]=accentColor();
    style.Colors[ImGuiCol_SliderGrab]=accentColor();
    style.Colors[ImGuiCol_SliderGrabActive]=accentColor();
    style.Colors[ImGuiCol_Button]=unpackThemeColor(kCard);
    style.Colors[ImGuiCol_ButtonHovered]=ImVec4(accentColor().x,accentColor().y,accentColor().z,.28f);
    style.Colors[ImGuiCol_ButtonActive]=ImVec4(accentColor().x,accentColor().y,accentColor().z,.42f);
}

// The game hides its own crosshair the moment a Minecraft screen is open, so the
// inventory, the Escape menu and the settings pages lose the aim point. Draw the
// same cross here while a game screen is up; in-world nothing is drawn, so the
// player still sees exactly one vanilla crosshair.
void drawScreenCrosshair() {
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return;
    if (!mc::Client::screenOpen()) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 c(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    const float s = std::max(1.f, io.DisplaySize.y / 720.f);
    const float arm = 5.f * s, gap = 2.f * s, thick = std::max(1.f, 1.5f * s);
    for (int pass = 0; pass < 2; ++pass) {
        const float t = pass == 0 ? thick * 2.f : thick;
        const ImU32 col = pass == 0 ? IM_COL32(0, 0, 0, 120) : IM_COL32(255, 255, 255, 215);
        dl->AddRectFilled(ImVec2(c.x - arm, c.y - t * 0.5f), ImVec2(c.x - gap, c.y + t * 0.5f), col);
        dl->AddRectFilled(ImVec2(c.x + gap, c.y - t * 0.5f), ImVec2(c.x + arm, c.y + t * 0.5f), col);
        dl->AddRectFilled(ImVec2(c.x - t * 0.5f, c.y - arm), ImVec2(c.x + t * 0.5f, c.y - gap), col);
        dl->AddRectFilled(ImVec2(c.x - t * 0.5f, c.y + gap), ImVec2(c.x + t * 0.5f, c.y + arm), col);
    }
}

// ---- Injection splash ------------------------------------------------------------------------
// "Welcome to Passive": an animated logo drawn when the client first maps into the game. It is pure
// draw-list geometry (no textures), so it works on any resolution and costs nothing once it ends.
// The mark is a diamond ring around a rounded tile carrying the T, with three dots orbiting it, a
// slow spin, a scale-in with a small overshoot and a staggered fade for each letter of the title.
void drawSplash() {
    if (g_splashStart < 0.0) return;
    const double now = ImGui::GetTime();
    const double elapsed = now - g_splashStart;
    if (elapsed < 0.0) return;
    if (elapsed > kSplashSeconds) {
        g_splashStart = -1.0;
        // The interface follows the splash instead of waiting for a key press: it opens on the next
        // frame, and its normal open animation grows it from 94% into place, so the hand-off is a
        // smooth pop rather than the library appearing at full size the instant the logo goes away.
        if (!g_menuOpen) { g_menuOpen = true; g_menuAnim.v = 0.f; }
        return;
    }
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl) return;
    const ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x < 2.f || io.DisplaySize.y < 2.f) return;
    const float ui = std::max(1.f, io.DisplaySize.y / 720.f);
    const float t = (float)elapsed;
    // Fade in over the first 0.45 s, out over the last 0.8 s, so the logo never pops.
    const float fadeIn = std::clamp(t / 0.45f, 0.f, 1.f);
    const float fadeOut = std::clamp((float)(kSplashSeconds - elapsed) / 0.8f, 0.f, 1.f);
    const float a = std::min(fadeIn, fadeOut);
    if (a <= 0.01f) return;
    // Scale-in with a small overshoot: 0.72 -> 1 over 0.55 s, easing out.
    const float grow = 1.f - std::pow(1.f - std::clamp(t / 0.55f, 0.f, 1.f), 3.f);
    const float scale = (0.72f + 0.28f * grow) * ui;
    const ImVec2 centre(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f - 30.f * ui);
    const float tile = 34.f * scale;
    const float angle = t * 1.1f;
    // Outer ring: two rotated squares, one thin and one faint, giving the diamond a slow spin.
    const auto corner = [&](float radius, float at) {
        return ImVec2(centre.x + std::cos(at) * radius, centre.y + std::sin(at) * radius);
    };
    for (int ring = 0; ring < 2; ++ring) {
        const float radius = (52.f + ring * 9.f) * scale;
        const float spin = angle * (ring == 0 ? 1.f : -0.6f);
        ImVec2 pts[4];
        for (int i = 0; i < 4; ++i) pts[i] = corner(radius, spin + (float)i * 1.57079633f + 0.78539816f);
        for (int i = 0; i < 4; ++i)
            dl->AddLine(pts[i], pts[(i + 1) % 4],
                        alpha(accentColorU32(ring == 0 ? 0.75f : 0.28f), a * (ring == 0 ? 1.f : 0.8f)),
                        (ring == 0 ? 1.8f : 1.1f) * scale);
    }
    // Orbiting dots, one per accent step, each lagging the one before it.
    for (int i = 0; i < 3; ++i) {
        const float at = angle * 1.4f + (float)i * 2.0943951f;
        const ImVec2 p = corner(52.f * scale, at);
        dl->AddCircleFilled(p, (3.2f - i * 0.6f) * scale, alpha(accentColorU32(0.9f - i * 0.22f), a), 16);
    }
    // The official mark sits inside the spinning diamond, with a soft accent
    // halo behind it. A fallback tile keeps the splash complete if the asset is
    // missing from this build.
    const float pulse = 0.5f + 0.5f * std::sin(t * 3.2f);
    const ImVec2 halo(centre.x, centre.y);
    for (int i = 4; i >= 1; --i)
        dl->AddCircleFilled(halo, (tile + (float)i * 9.f * scale),
                            alpha(accentColorU32(0.045f + 0.012f * pulse), a), 48);
    const ImVec2 mark0(centre.x - tile * 1.28f, centre.y - tile * 1.28f);
    const ImVec2 mark1(centre.x + tile * 1.28f, centre.y + tile * 1.28f);
    ImFont* bold = ImGui::GetFont();
    if (!drawLogo(dl, mark0, mark1, IM_COL32(250, 252, 255, 255), a)) {
        dl->AddRectFilled(mark0, mark1, alpha(accentColorU32(), a), 16.f * scale);
        if (bold) {
            const float glyph = 44.f * scale;
            const ImVec2 glyphSize = bold->CalcTextSizeA(glyph, FLT_MAX, 0.f, "P");
            dl->AddText(bold, glyph,
                        ImVec2(centre.x - glyphSize.x * 0.5f, centre.y - glyphSize.y * 0.5f),
                        alpha(kOnAccent, a), "P");
        }
    }
    // "WELCOME TO PASSIVE", one letter at a time so the tracking and the stagger both read as designed.
    const char* title = "WELCOME TO PASSIVE";
    const float titleSize = 17.f * ui;
    const float tracking = 4.2f * ui;
    if (bold) {
        float width = 0.f;
        for (const char* p = title; *p; ++p) width += bold->CalcTextSizeA(titleSize, FLT_MAX, 0.f, p, p + 1).x + tracking;
        width -= tracking;
        const float baseY = centre.y + 78.f * ui;
        float x = centre.x - width * 0.5f;
        int index = 0;
        for (const char* p = title; *p; ++p, ++index) {
            const float w = bold->CalcTextSizeA(titleSize, FLT_MAX, 0.f, p, p + 1).x;
            // Each letter arrives 18 ms after the one before it, and rises a few pixels into place.
            const float letterT = std::clamp((t - 0.35f - index * 0.018f) / 0.30f, 0.f, 1.f);
            const float ease = 1.f - std::pow(1.f - letterT, 2.f);
            if (letterT > 0.f) {
                char glyphText[2] = { *p, '\0' };
                dl->AddText(bold, titleSize, ImVec2(x, baseY + (1.f - ease) * 8.f * ui),
                            alpha(kInk, a * ease), glyphText);
            }
            x += w + tracking;
        }
        const float subT = std::clamp((t - 0.9f) / 0.6f, 0.f, 1.f);
        if (subT > 0.f) {
            const char* sub = "single-player utility client";
            const float subSize = 10.f * ui;
            const float subW = bold->CalcTextSizeA(subSize, FLT_MAX, 0.f, sub).x;
            dl->AddText(bold, subSize, ImVec2(centre.x - subW * 0.5f, baseY + 28.f * ui),
                        alpha(kMuted, a * subT), sub);
        }
    }
}

void drawFrame() {
    tickThemeFade(ImGui::GetIO().DeltaTime);   // preset colour fades run whether or not the page is open
    const float menuOpacity = g_menuAnim;
    drawScreenCrosshair();   // kept under the interface panels
    drawHudOverlay();        // the HUD Editor's screen elements (hidden while the menu is up)
    if (menuOpacity > 0.005f) {
        const ImGuiIO& io = ImGui::GetIO();
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        bg->AddRectFilled(ImVec2(0.f, 0.f), io.DisplaySize,
                          IM_COL32(5, 4, 7, (int)(142.f * menuOpacity)));
        // The interface fades as one piece. The drawn panels already take the menu animation as an
        // opacity, but the module option widgets are real ImGui widgets, which fade with the style's
        // alpha and nothing else - so they used to stay fully visible for the split second between
        // the panel fading and the window being skipped, then vanish. One style alpha here covers
        // every widget the interface owns, so the two can never separate.
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, menuOpacity);
        drawMainWindow(menuOpacity);
        ImGui::PopStyleVar();
    } else if (!g_menuOpen) {
        ui::resetAnimations();   // fully closed: forget the widget animation states
    }
    drawSplash();
    drawToasts();
    g_taps.clear();
    g_mouseTaps.clear();
}

} // namespace gui
} // namespace tyr
