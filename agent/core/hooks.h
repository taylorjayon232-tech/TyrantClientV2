// ============================================================================
//  Passive — MinHook plumbing: wglSwapBuffers (render + input + modules) and
//  the game's WndProc (keys while Minecraft has focus).
// ============================================================================
#pragma once
#include <cstdint>

namespace tyr {
namespace hooks {

using RenderCallback  = void(*)();
using WndProcCallback = bool(*)(void* hwnd, uint32_t msg, uint64_t wParam, int64_t lParam);
using TickCallback    = void(*)();
// Runs on the render thread just *before* the frame's GL clear, i.e. after the
// game's tick has already rewritten body/head rotation and before the world is
// drawn. That is the only window in which a silent aim can hold the model.
using PreRenderCallback = void(*)();

void setRenderCallback(RenderCallback cb);
void setWndProcCallback(WndProcCallback cb);
void setTickCallback(TickCallback cb);
void setPreRenderCallback(PreRenderCallback cb);

// How many times the pre-render callback has run. 0 means the hook never fired and the body pose
// is only being written at end of frame.
uintptr_t preRenderCalls();
uintptr_t drawWrites();   // pose writes issued at draw time (the closest point to the model)

bool install();     // find targets + MH_CreateHook + MH_EnableHook
void uninstall();
void rearm();       // re-apply the detours after the frame counter went quiet
void ensureWndProcHooked();   // lazily hook the game's WndProc once hwnd known

// Minecraft window handle + device context of the hooked swapchain.
void*      hwnd();
uintptr_t  frameCount();   // increments every hooked frame

// GL error containment stats (Diagnostics panel in the menu).
int  glDrainCount();
int  glLastErr();

// Synthetic input posted straight to the Minecraft window, so GLFW and the
// game see ordinary key/button transitions (Item Scroll auto-clicking, opening
// the inventory for the non-silent Auto Totem / Auto Clean paths).
// Messages are tagged; Passive's own WndProc bookkeeping ignores the tag while
// GLFW and the game still receive them normally, so keybinds and mouse taps
// never fire from input we generated.
void syntheticKey(int vk, bool down);   // WM_KEYDOWN / WM_KEYUP at the game
void syntheticClick(bool down);         // left button at the current cursor pos

// True once the ImGui context + GL backends are live (a real swap frame ran).
// Frame-path GUI code must no-op until then.
bool guiInitialized();
bool renderCleanupComplete();

} // namespace hooks
} // namespace tyr
