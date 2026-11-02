// ============================================================================
//  Passive — GUI orchestrator (declarations).
// ============================================================================
#pragma once
#include <imgui.h>
#include <vector>
#include <string>

namespace tyr {
namespace gui {

// Bottom-right notification.
struct ToastData {
    std::string text;
    float       life;   // seconds remaining
    bool        tagged = false;  // rendered with the green "DEBUG MODE" tag
};

void init();        // toasts, animations, "loaded" notification
void shutdown();
void preFrame();    // per-frame animation and input state
void drawFrame();   // animated in-game interface and notification overlays

void toggleMenu();                 // called by WndProc on menu key (RSHIFT)
bool menuOpen();
float menuAnim();                  // 0..1 open/close animation value
int  menuKey();                    // VK code that toggles the menu
void setMenuKey(int vk);

// input plumbing (fed by hooks::WndProcCallback)
bool wantsMouse();                 // ImGui wants mouse -> swallow game input
bool wantsKeyboard();
bool inputFrozen();                // menu open -> freeze ALL game input (camera/keys)
bool cursorWanted();               // a visible OS pointer is wanted (menu or a game screen)
void onKey(int vk, bool down);     // WndProc -> here
void onMouse(int vk, bool down);
const std::vector<int>& keyTaps(); // keys tapped this frame (for keybinds)
const std::vector<int>& mouseTaps(); // mouse buttons pressed this frame
std::vector<int> allTaps();        // keys + mouse buttons tapped this frame
bool keyDown(int vk);
bool mouseDown(int vk);
bool anyDown(int vk);              // key OR mouse button currently held

// camera/frame interpolation for the native overlay
float tickDelta();                 // 0..1 interpolation alpha (approximated)
void  setTickDelta(float t);

// notifications (bottom-right toasts)
void notify(const char* text);
// Same, but drawn with the green "DEBUG MODE" tag. Used by the module action
// toasts that only appear while Debug Mode is on (modules::debugMode()).
void notifyTagged(const char* text);

} // namespace gui
} // namespace tyr
