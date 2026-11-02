// ============================================================================
//  Passive — hook implementation.
//
//  Render path: wglSwapBuffers is called by Minecraft's main render loop with
//  the GL context current on the render thread — the perfect per-frame entry
//  point for the ImGui interface.
//
//  Input path : the original WndProc is chained; we observe keys/mouse when
//  Minecraft is focused. GUI-consumed input is swallowed so clicks don't
//  reach the game while the menu is open.
//
//  GL state safety: Minecraft leaves arbitrary state bound. We save the bits
//  ImGui's GL3 backend + our own draws disturb, and restore them afterwards.
// ============================================================================
#include "hooks.h"
#include "agent.h"
#include "../gui/gui.h"
#include "../gui/theme.h"

#include <MinHook.h>
#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_win32.h>

#include <windows.h>
#include <winuser.h>
#include "../render/glex.h"   // GL3 loader (glBlendFuncSeparate, FBO/VAO enums...)
#ifndef GL_DEBUG_OUTPUT
#define GL_DEBUG_OUTPUT 0x92E0
#define GL_DEBUG_OUTPUT_SYNCHRONOUS 0x8242
#endif
#ifndef GL_CONTEXT_PROFILE_MASK
#define GL_CONTEXT_PROFILE_MASK 0x9126
#endif
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH  0x0CF2
#define GL_UNPACK_SKIP_ROWS   0x0CF3
#define GL_UNPACK_SKIP_PIXELS 0x0CF4
#endif
#include <mutex>
#include <atomic>
#include <cstring>
#include <chrono>
#include <winuser.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace tyr {
namespace hooks {

// Declared before the anonymous namespace below so the pre-render hook installer (inside it) can
// reach hookOne, which is defined further down. Declaring it inside the anonymous namespace instead
// would create a second entity of the same name and make the call ambiguous.
static bool hookOne(void* target, void* detour, void** real);

namespace {

using SwapBuffersFn = BOOL(WINAPI*)(HDC);

std::atomic<uintptr_t> g_frameCount{ 0 };
std::atomic<uint32_t> g_activeDetours{ 0 };
std::atomic<uint32_t> g_activeWndProc{ 0 };
std::atomic<bool> g_renderCleanupDone{ false };
static bool         g_cbThrew         = false; // render callback threw once (logged)
static int          g_initFailLogged  = 0;     // first initGui failures -> agent log
static int          g_wndProcHookLogs = 0;     // bounded WndProc hook / re-hook logging
static int          g_wndProcForeignLogs = 0;  // bounded "chain taken over" logging
static int          g_glErrLogged     = 0;   // first GL errors drained -> agent log
static std::atomic<int> g_glDrainCount{0};   // total GL errors drained (diagnostics HUD)
static std::atomic<int> g_glLastErr{0};      // last drained GL error code
HWND   g_hwnd = nullptr;
HDC    g_hdc  = nullptr;
bool   g_guiInitialized = false;
std::atomic<WNDPROC> g_origWndProc{ nullptr };

struct DetourScope {
    DetourScope() { g_activeDetours.fetch_add(1, std::memory_order_acq_rel); }
    ~DetourScope() { g_activeDetours.fetch_sub(1, std::memory_order_acq_rel); }
};
struct WndProcScope {
    WndProcScope() {
        g_activeDetours.fetch_add(1, std::memory_order_acq_rel);
        g_activeWndProc.fetch_add(1, std::memory_order_acq_rel);
    }
    ~WndProcScope() {
        g_activeWndProc.fetch_sub(1, std::memory_order_acq_rel);
        g_activeDetours.fetch_sub(1, std::memory_order_acq_rel);
    }
};

void restoreWndProcAndWait() {
    const WNDPROC original=g_origWndProc.load(std::memory_order_acquire);
    if (g_hwnd && original) {
        SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
    }
    while (g_activeWndProc.load(std::memory_order_acquire) != 0) Sleep(1);
    g_origWndProc.store(nullptr,std::memory_order_release);
}

// ---------------------------------------------------------------------------
// Window binding.
//
// More than one Minecraft can be open at once (that is the point of the multi
// instance injector), and each one creates a top level window of class
// "GLFW30". FindWindowW(L"GLFW30", nullptr) searches the WHOLE desktop, so it
// can return another instance's window: the overlay would then lay its
// interface out against that window's client rect - a minimised instance
// reports 0x0, which makes ImGui build an empty frame, so the menu turns
// invisible and the text is left scrambled - and every synthetic key click
// would be posted to the wrong process as well.
//
// The swap chain's DC is authoritative and always belongs to THIS process, so
// the window is derived from it and re-adopted if it ever changes (window
// recreation, fullscreen toggle).
// ---------------------------------------------------------------------------
struct WindowSearch {
    DWORD pid = 0;
    HWND  titled = nullptr;
    HWND  untitled = nullptr;
};

BOOL CALLBACK findOwnGlfwWindowProc(HWND hwnd, LPARAM param) {
    WindowSearch* search = reinterpret_cast<WindowSearch*>(param);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != search->pid) return TRUE;                 // another process
    wchar_t cls[64]{};
    if (GetClassNameW(hwnd, cls, 64) <= 0) return TRUE;
    if (wcsncmp(cls, L"GLFW", 4) != 0) return TRUE;     // not a GLFW window
    wchar_t title[2]{};
    GetWindowTextW(hwnd, title, 2);
    if (title[0] != 0) { if (!search->titled)   search->titled   = hwnd; }
    else               { if (!search->untitled) search->untitled = hwnd; }
    return TRUE;                                        // the titled window wins
}

HWND findOwnGlfwWindow() {
    WindowSearch search;
    search.pid = GetCurrentProcessId();
    EnumWindows(&findOwnGlfwWindowProc, reinterpret_cast<LPARAM>(&search));
    return search.titled ? search.titled : search.untitled;
}

void adoptSwapWindow(HWND hwnd, HDC dc) {
    if (!hwnd) return;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId()) return;   // never chain to another process
    if (hwnd == g_hwnd) { if (dc) g_hdc = dc; return; }

    // Whatever window was held was the wrong one: undo its WndProc chain (the
    // call can only succeed for our own process) and let ensureWndProcHooked()
    // re-hook the real window on this frame.
    const WNDPROC stale = g_origWndProc.load(std::memory_order_acquire);
    if (stale && g_hwnd) {
        SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(stale));
    }
    g_origWndProc.store(nullptr, std::memory_order_release);

    g_hwnd = hwnd;
    if (dc) g_hdc = dc;

    // Re-point the ImGui Win32 backend so DisplaySize, DPI and mouse routing all
    // come from the window we are actually presenting to.
    if (g_guiInitialized) {
        ImGui_ImplWin32_Shutdown();
        ImGui_ImplWin32_Init(hwnd);
    }

    char cls[64] = "?";
    wchar_t wideCls[64] = L"?";
    GetClassNameW(hwnd, wideCls, 64);
    WideCharToMultiByte(CP_UTF8, 0, wideCls, -1, cls, sizeof(cls), nullptr, nullptr);
    tyr::trace::log("swap window adopted (hwnd=%p class=%s)", (void*)hwnd, cls);
}

// ---------------------------------------------------------------------------
// One-shot render diagnostics.
//
// The interface is authored in the window's CLIENT space and the backend maps
// it onto the GL VIEWPORT, so the two have to agree. When display scaling or a
// fullscreen/resolution transition leaves them different, the whole menu is
// projected into the wrong rectangle: clipped, squashed, and with the text
// unreadable - exactly the "invisible and scrambled" report. Print both once,
// and again whenever the client size changes, so a broken run carries the
// numbers needed to see why instead of needing another round trip.
// ---------------------------------------------------------------------------
void logRenderDiagnostics(int clientW, int clientH,
                          int unpackRowLength, int unpackSkipRows, int unpackSkipPixels) {
    struct Diag {
        int  cw = 0, ch = 0;
        bool logged = false;
    };
    static Diag diag;
    if (diag.logged && diag.cw == clientW && diag.ch == clientH) return;
    diag.logged = true;
    diag.cw = clientW;
    diag.ch = clientH;

    const ImGuiIO& io = ImGui::GetIO();

    GLint viewport[4] = { 0, 0, 0, 0 };
    glGetIntegerv(GL_VIEWPORT, viewport);
    GLint framebuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);

    int glMajor = 0, glMinor = 0;
    const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    if (glVersion) std::sscanf(glVersion, "%d.%d", &glMajor, &glMinor);
    GLint profileMask = 0;
    if (glMajor * 100 + glMinor * 10 >= 320)
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profileMask);

    char windowClass[64] = "?";
    wchar_t wideClass[64] = L"?";
    GetClassNameW(g_hwnd, wideClass, 64);
    WideCharToMultiByte(CP_UTF8, 0, wideClass, -1, windowClass, sizeof(windowClass), nullptr, nullptr);

    const ImFontAtlas* atlas = io.Fonts;
    tyr::trace::log("render diag: window=%s client=%dx%d glViewport=%d,%d %dx%d fbo=%d profile=0x%x "
                    "display=%.0fx%.0f scale=%.2fx%.2f fonts=%d atlas=%dx%d fontTex=%llu "
                    "gameUnpack(row/skipRows/skipPixels)=%d/%d/%d gl=%s",
                    windowClass, clientW, clientH,
                    (int)viewport[0], (int)viewport[1], (int)viewport[2], (int)viewport[3],
                    (int)framebuffer, (unsigned)profileMask,
                    io.DisplaySize.x, io.DisplaySize.y,
                    io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y,
                    atlas ? atlas->Fonts.Size : 0,
                    atlas ? atlas->TexWidth : 0, atlas ? atlas->TexHeight : 0,
                    (unsigned long long)(atlas ? atlas->TexID : 0),
                    unpackRowLength, unpackSkipRows, unpackSkipPixels,
                    glVersion ? glVersion : "?");
}

RenderCallback  g_renderCb  = nullptr;
WndProcCallback g_wndProcCb = nullptr;
TickCallback    g_tickCb    = nullptr;
PreRenderCallback g_preRenderCb = nullptr;
// The thread that swaps buffers - the game's render thread. glClear is only
// allowed to run the pre-render callback on that thread, because only there is
// jni::env() the right JNIEnv.
std::atomic<unsigned long> g_renderThreadId{0};



// ---------------------------------------------------------------------------
// GL state capture — save/restore the small set Minecraft leaves dirty.
// ---------------------------------------------------------------------------
static bool supportsDebugOutput() {
    static int cached = -1;
    if (cached >= 0) return cached != 0;

    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    int major = 0, minor = 0;
    if (version) {
        if (std::sscanf(version, "%d.%d", &major, &minor) == 2) {
            if (major > 4 || (major == 4 && minor >= 3)) {
                cached = 1;
                return true;
            }
        }
    }

    // GL_EXTENSIONS via glGetString is invalid in core contexts (including
    // Minecraft's Intel OpenGL 3.3 context). Enumerate extensions through the
    // OpenGL 3.0 API instead, so capability detection does not inject a GL error
    // into every hooked frame.
    if (major >= 3) {
        using GetStringiFn = const GLubyte*(APIENTRY*)(GLenum, GLuint);
        const PROC address = wglGetProcAddress("glGetStringi");
        const std::uintptr_t procValue = reinterpret_cast<std::uintptr_t>(address);
        if (procValue > 3 && procValue != static_cast<std::uintptr_t>(-1)) {
            const auto getStringi = reinterpret_cast<GetStringiFn>(address);
            constexpr GLenum kGlNumExtensions = 0x821D;
            GLint extensionCount = 0;
            glGetIntegerv(kGlNumExtensions, &extensionCount);
            for (GLint i = 0; i < extensionCount; ++i) {
                const char* extension = reinterpret_cast<const char*>(
                    getStringi(GL_EXTENSIONS, static_cast<GLuint>(i)));
                if (extension && std::strcmp(extension, "GL_KHR_debug") == 0) {
                    cached = 1;
                    return true;
                }
            }
        }
    }
    cached = 0;
    return false;
}

struct GlState {
    GLint viewport[4];
    GLint fb = 0;   // ONE framebuffer binding: the split DRAW/READ enums generate
                    // GL_INVALID_ENUM on Haswell Intel drivers (this machine) and the
                    // 2016 driver ignores them anyway - the flood was the UI flicker.
    GLint prog = 0;
    GLint vao = 0;
    GLint arrayBuf = 0, elementBuf = 0;
    GLboolean blend, depthTest, scissorTest, cullFace;
    GLint blendSrcRgb, blendDstRgb, blendSrcA, blendDstA;
    GLfloat clearColor[4];
    GLboolean colorMask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
    GLfloat blendColor[4] = { 0.f, 0.f, 0.f, 0.f };
    GLint activeTex = 0, texBinding0 = 0;
    GLint pixelPackAlignment = 4, pixelUnpackAlignment = 4;
    GLint unpackRowLength = 0, unpackSkipRows = 0, unpackSkipPixels = 0;

    void save() {
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fb);
        glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuf);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementBuf);
        blend       = glIsEnabled(GL_BLEND);
        depthTest   = glIsEnabled(GL_DEPTH_TEST);
        scissorTest = glIsEnabled(GL_SCISSOR_TEST);
        cullFace    = glIsEnabled(GL_CULL_FACE);
        glGetIntegerv(GL_BLEND_SRC_RGB,   &blendSrcRgb);
        glGetIntegerv(GL_BLEND_DST_RGB,   &blendDstRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcA);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstA);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
        glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
        glGetFloatv(GL_BLEND_COLOR, blendColor);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTex);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texBinding0);
        glGetIntegerv(GL_PACK_ALIGNMENT,   &pixelPackAlignment);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &pixelUnpackAlignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH,  &unpackRowLength);
        glGetIntegerv(GL_UNPACK_SKIP_ROWS,   &unpackSkipRows);
        glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &unpackSkipPixels);
    }
    void restore() {
        glPixelStorei(GL_PACK_ALIGNMENT,   pixelPackAlignment);
        glPixelStorei(GL_UNPACK_ALIGNMENT, pixelUnpackAlignment);
        glPixelStorei(GL_UNPACK_ROW_LENGTH,  unpackRowLength);
        glPixelStorei(GL_UNPACK_SKIP_ROWS,   unpackSkipRows);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, unpackSkipPixels);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texBinding0);
        glActiveTexture(activeTex);
        glBlendColor(blendColor[0], blendColor[1], blendColor[2], blendColor[3]);
        glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        glUseProgram(prog);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, arrayBuf);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuf);
        if (blend)       glEnable(GL_BLEND);       else glDisable(GL_BLEND);
        if (depthTest)   glEnable(GL_DEPTH_TEST);  else glDisable(GL_DEPTH_TEST);
        if (scissorTest) glEnable(GL_SCISSOR_TEST);else glDisable(GL_SCISSOR_TEST);
        if (cullFace)    glEnable(GL_CULL_FACE);   else glDisable(GL_CULL_FACE);
        glBlendFuncSeparate(blendSrcRgb, blendDstRgb, blendSrcA, blendDstA);
        glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    }
};

// ---------------------------------------------------------------------------
bool initGui(HWND hwnd) {
    if (g_guiInitialized) return true;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;   // don't drop imgui.ini next to Minecraft

    gui::applyTheme();              // fonts + dark surfaces + coral accent

    // A half-built context must never survive a failure: the interface's fonts
    // are static handles into the context's atlas, so a retry that created a
    // second context while holding the first one's fonts would draw every label
    // with a texture the backend no longer owns - scrambled text. Tear the
    // context down instead and rebuild it cleanly on the next frame.
    if (!ImGui_ImplWin32_Init(hwnd)) {
        ImGui::DestroyContext();
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 130")) {
        ImGui_ImplWin32_Shutdown();   // live here: Win32_Init above succeeded
        ImGui::DestroyContext();
        return false;
    }

    g_guiInitialized = true;
    return true;
}

void shutdownGui() {
    if (!g_guiInitialized) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_guiInitialized = false;
}

// ---------------------------------------------------------------------------
// Hooked swap entry points.
// MC/LWJGL may end a frame through wglSwapBuffers OR straight through GDI's
// SwapBuffers (driver-dependent); some stacks call both every frame. Hook
// BOTH, funnel into one common per-frame handler, and de-dupe calls that
// arrive within a few ms of each other.// ---------------------------------------------------------------------------
SwapBuffersFn g_realWgl = nullptr;
SwapBuffersFn g_realGdi = nullptr;

// glClear is the first GL call of a frame, so it runs after the game's tick (which rewrites
// body/head rotation) and before any world geometry is drawn. Hooking it gives the silent aim the
// one window in which its rotation survives to the screen.
typedef void (WINAPI* GlClearFn)(unsigned int);
static GlClearFn g_realGlClear = nullptr;    // opengl32 export
static GlClearFn g_realGlClearDrv = nullptr; // wglGetProcAddress (what LWJGL actually calls)
typedef void (WINAPI* GlViewportFn)(int, int, int, int);
static GlViewportFn g_realGlViewport = nullptr;
static std::atomic<int> g_preRenderCalls{0};
static void preRenderTick() {
    const unsigned long renderTid = g_renderThreadId.load(std::memory_order_relaxed);
    if (!g_preRenderCb || !renderTid || GetCurrentThreadId() != renderTid) return;
    // No time-based throttle here on purpose. An earlier version skipped writes inside a 0.5 ms
    // window, and a skip on the call that follows the game's tick re-introduces exactly the flicker
    // this hook exists to remove - a missed write costs a few float stores, a missed frame is visible.
    if (g_preRenderCalls.fetch_add(1, std::memory_order_relaxed) == 0)
        trace::log("pre-render hook fired (body pose is now written before the world draw)");
    g_preRenderCb();
}
static void WINAPI hkGlClear(unsigned int mask) {
    preRenderTick();
    if (g_realGlClear) g_realGlClear(mask);
}
static void WINAPI hkGlClearDrv(unsigned int mask) {
    preRenderTick();
    if (g_realGlClearDrv) g_realGlClearDrv(mask);
}
static void WINAPI hkGlViewport(int x, int y, int w, int h) {
    preRenderTick();
    if (g_realGlViewport) g_realGlViewport(x, y, w, h);
}

// Drawing is the last thing that happens before geometry leaves for the screen. A pose write issued
// here is as close to the model as a native hook can get, and it is what closes the one remaining
// gap: in this version the game tick can run on its own thread, so it may land BETWEEN the
// start-of-frame write and the entity draw. Every such tick put the camera's rotation on screen for
// a frame, which is the twitch. Corrected at the next draw call, the wrong rotation survives for at
// most 0.1 ms instead of a whole frame.
typedef void (WINAPI* GlDrawArraysFn)(unsigned int, int, int);
typedef void (WINAPI* GlDrawElementsFn)(unsigned int, int, unsigned int, const void*);
static GlDrawArraysFn   g_realGlDrawArrays = nullptr,   g_realGlDrawArraysDrv = nullptr;
static GlDrawElementsFn g_realGlDrawElements = nullptr, g_realGlDrawElementsDrv = nullptr;
static std::atomic<long long> g_lastDrawWriteAt{0};
static std::atomic<int> g_drawWrites{0};
static void drawRenderTick() {
    const unsigned long renderTid = g_renderThreadId.load(std::memory_order_relaxed);
    if (!g_preRenderCb || !renderTid || GetCurrentThreadId() != renderTid) return;
    const long long now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const long long last = g_lastDrawWriteAt.load(std::memory_order_relaxed);
    if (now - last < 100) return;             // at most one write per 0.1 ms
    g_lastDrawWriteAt.store(now, std::memory_order_relaxed);
    if (g_drawWrites.fetch_add(1, std::memory_order_relaxed) == 0)
        trace::log("draw hook fired (pose is re-written at draw time)");
    g_preRenderCb();
}
static void WINAPI hkGlDrawArrays(unsigned int mode, int first, int count) {
    drawRenderTick();
    if (g_realGlDrawArrays) g_realGlDrawArrays(mode, first, count);
}
static void WINAPI hkGlDrawArraysDrv(unsigned int mode, int first, int count) {
    drawRenderTick();
    if (g_realGlDrawArraysDrv) g_realGlDrawArraysDrv(mode, first, count);
}
static void WINAPI hkGlDrawElements(unsigned int mode, int count, unsigned int type, const void* indices) {
    drawRenderTick();
    if (g_realGlDrawElements) g_realGlDrawElements(mode, count, type, indices);
}
static void WINAPI hkGlDrawElementsDrv(unsigned int mode, int count, unsigned int type, const void* indices) {
    drawRenderTick();
    if (g_realGlDrawElementsDrv) g_realGlDrawElementsDrv(mode, count, type, indices);
}

BOOL WINAPI hkSwapBuffersCommon(HDC hdc, SwapBuffersFn real);
BOOL WINAPI hkSwapBuffersCommon(HDC hdc, SwapBuffersFn real);
// The GL entry points must be resolved on the render thread: wglGetProcAddress answers only for a
// thread that has a current GL context, and installing from the init thread (which has none) silently
// resolved nothing - the reason the pre-render writer never ran.
// The pre-render hooks (glClear / glViewport) are resolved on the render thread: see below.

static void installPreRenderHooksFromRenderThread() {
    static std::once_flag once;
    std::call_once(once, [] {
        HMODULE ogl = GetModuleHandleW(L"opengl32.dll");
        if (!ogl) return;
        typedef void* (WINAPI* WglGetProcAddressFn)(const char*);
        auto wglGet = reinterpret_cast<WglGetProcAddressFn>(GetProcAddress(ogl, "wglGetProcAddress"));
        void* exportClear = reinterpret_cast<void*>(GetProcAddress(ogl, "glClear"));
        void* exportViewport = reinterpret_cast<void*>(GetProcAddress(ogl, "glViewport"));
        void* driverClear = wglGet ? wglGet("glClear") : nullptr;
        void* driverViewport = wglGet ? wglGet("glViewport") : nullptr;
        bool okClear = exportClear && hookOne(exportClear, reinterpret_cast<void*>(&hkGlClear),
                                              reinterpret_cast<void**>(&g_realGlClear));
        if (driverClear && driverClear != exportClear)
            okClear = okClear || hookOne(driverClear, reinterpret_cast<void*>(&hkGlClearDrv),
                                         reinterpret_cast<void**>(&g_realGlClearDrv));
        bool okView = exportViewport && hookOne(exportViewport, reinterpret_cast<void*>(&hkGlViewport),
                                                reinterpret_cast<void**>(&g_realGlViewport));
        if (driverViewport && driverViewport != exportViewport)
            okView = okView || hookOne(driverViewport, reinterpret_cast<void*>(&hkGlViewport),
                                       reinterpret_cast<void**>(&g_realGlViewport));
        // Draw calls: the pose write that lands closest to the model actually being built.
        void* exportArrays = reinterpret_cast<void*>(GetProcAddress(ogl, "glDrawArrays"));
        void* exportElems  = reinterpret_cast<void*>(GetProcAddress(ogl, "glDrawElements"));
        void* driverArrays = wglGet ? wglGet("glDrawArrays") : nullptr;
        void* driverElems  = wglGet ? wglGet("glDrawElements") : nullptr;
        bool okDraw = exportArrays && hookOne(exportArrays, reinterpret_cast<void*>(&hkGlDrawArrays),
                                              reinterpret_cast<void**>(&g_realGlDrawArrays));
        if (driverArrays && driverArrays != exportArrays)
            okDraw = okDraw || hookOne(driverArrays, reinterpret_cast<void*>(&hkGlDrawArraysDrv),
                                       reinterpret_cast<void**>(&g_realGlDrawArraysDrv));
        okDraw = (exportElems && hookOne(exportElems, reinterpret_cast<void*>(&hkGlDrawElements),
                                         reinterpret_cast<void**>(&g_realGlDrawElements))) || okDraw;
        if (driverElems && driverElems != exportElems)
            okDraw = okDraw || hookOne(driverElems, reinterpret_cast<void*>(&hkGlDrawElementsDrv),
                                       reinterpret_cast<void**>(&g_realGlDrawElementsDrv));
        trace::log("install: glClear hook=%s, glViewport hook=%s, glDraw* hook=%s (resolved on the render thread)",
                   okClear ? "ok" : "FAILED", okView ? "ok" : "FAILED", okDraw ? "ok" : "FAILED");
    });
}

BOOL WINAPI hkWglSwapBuffers(HDC hdc) {
    DetourScope active;
    return hkSwapBuffersCommon(hdc, g_realWgl);
}
BOOL WINAPI hkGdiSwapBuffers(HDC hdc) {
    DetourScope active;
    return hkSwapBuffersCommon(hdc, g_realGdi);
}

BOOL WINAPI hkSwapBuffersCommon(HDC hdc, SwapBuffersFn real) {
    if (!real) return SwapBuffers(hdc);
    g_renderThreadId.store(GetCurrentThreadId(), std::memory_order_relaxed);
    installPreRenderHooksFromRenderThread();

    // De-dupe: when both hooks are live, MC's wgl call typically ends in GDI
    // SwapBuffers within the same frame. Render on the wgl call every frame;
    // the GDI call only renders if wgl hasn't fired for >50ms (pure-GDI
    // driver stack). A time-window heuristic that let BOTH render in the
    // same frame double-drew the overlay onto a half-presented frame and caused flicker.
    static std::atomic<ULONGLONG> s_lastWglRenderMs{ 0 };
    const ULONGLONG now = GetTickCount64();
    const bool isWgl = (real != g_realGdi) || !g_realGdi;
    bool renderThisCall;
    if (isWgl) {
        renderThisCall = true;
        s_lastWglRenderMs.store(now, std::memory_order_relaxed);
    } else {
        const ULONGLONG last = s_lastWglRenderMs.load(std::memory_order_relaxed);
        renderThisCall = (now - last) > 50;   // wgl dead? GDI takes over.
    }

    // One-time GL3 loader init (MC's context is current here).
    static bool glInited = false;
    if (!glInited) { glInited = true; tyr::glx::init(); }

    // Bind to the window that owns this swap chain. The DC is per-process, so
    // it can never resolve to another Minecraft instance the way a class-name
    // search on the whole desktop can. Re-checked every frame so a recreated
    // window (fullscreen toggle, driver reset) is picked up too.
    if (hdc) adoptSwapWindow(WindowFromDC(hdc), hdc);
    if (!g_hwnd) {
        g_hdc = hdc;
        g_hwnd = findOwnGlfwWindow();          // last resort; still this process
    }

    if (g_renderCb && renderThisCall && g_hwnd) {
        static bool loggedFirst = false;
        if (!loggedFirst) {
            loggedFirst = true;
            tyr::trace::log("render frame 1 (hwnd=%p)", (void*)g_hwnd);
        }

        GlState st;
        st.save();

        // The font atlas and the item icons are uploaded from tightly packed CPU
        // buffers, but ImGui's GL backend only zeroes GL_UNPACK_ROW_LENGTH of the
        // pixel-store state. Minecraft uploads its own atlases with ROW_LENGTH
        // plus SKIP_ROWS / SKIP_PIXELS set and leaves whatever it last used
        // behind, and an inherited skip offset shears a row-major upload - the
        // glyphs come out as scrambled noise, from the very first frame after
        // injection, exactly while the rest of the interface still draws. Zero
        // the state those uploads depend on and let GlState::restore put the
        // game's values back afterwards.
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

        // Minecraft sometimes enables KHR_debug on the driver side, but not
        // every context supports it. Forcing GL_DEBUG_OUTPUT on a non-debug
        // context emits GL_INVALID_ENUM and can crash some Intel drivers.
        // Only toggle it when the active context actually advertises support.
        if (supportsDebugOutput()) {
            glDisable(GL_DEBUG_OUTPUT);
            glDisable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        }

        // The overlay must always composite onto the REAL backbuffer. Sodium
        // leaves post-processing FBOs bound at swap time on some frames —
        // drawing ImGui there would hide the interface when that FBO is discarded.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        if (tyr::g_requestUnload.load(std::memory_order_acquire)) {
            // OpenGL resources must be released on this render thread while
            // Minecraft's GL context is current. The unload worker only removes
            // hooks and frees the DLL after this frame has completely returned.
            restoreWndProcAndWait();
            shutdownGui();
            g_renderCleanupDone.store(true, std::memory_order_release);
        } else if (initGui(g_hwnd)) {
            // A minimised window reports an empty client rect. Laying the
            // interface out against 0x0 builds an empty frame and leaves the
            // menu invisible/squashed, so skip the frame and try again on the
            // next swap (the window is restored by then).
            RECT client{};
            GetClientRect(g_hwnd, &client);
            if ((client.right - client.left) > 0 && (client.bottom - client.top) > 0) {
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplWin32_NewFrame();
                logRenderDiagnostics(client.right - client.left, client.bottom - client.top,
                                     st.unpackRowLength, st.unpackSkipRows, st.unpackSkipPixels);
                ImGui::NewFrame();

                // Draw the interface over the completed Minecraft frame.
                //
                // The interface runs hundreds of JNI calls per frame. A stray
                // C++ exception used to escape the detour and unwind into the
                // game's own render path, where it reaches std::terminate - the
                // "client crashed for no reason" report. Contain it, keep the
                // ImGui frame balanced and carry on; the next frame retries.
                try {
                    g_renderCb();
                } catch (...) {
                    if (!g_cbThrew) {
                        g_cbThrew = true;
                        tyr::trace::log("render callback threw - frame skipped, client kept running");
                    }
                    ImGui::EndFrame();   // close the half-built frame cleanly
                }

                ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            } else {
                static bool loggedEmpty = false;
                if (!loggedEmpty) {
                    loggedEmpty = true;
                    tyr::trace::log("window has no client area yet — interface skipped");
                }
            }
            if (tyr::g_requestUnload.load(std::memory_order_acquire)) {
                restoreWndProcAndWait();
                shutdownGui();
                g_renderCleanupDone.store(true, std::memory_order_release);
            }
        } else if (tyr::g_requestUnload.load(std::memory_order_acquire)) {
            g_renderCleanupDone.store(true, std::memory_order_release);
        } else {
            // initGui failed and is retried next frame. Log the first few so a
            // client that never manages to draw explains itself in the log
            // instead of looking like it silently uninjected.
            if (g_initFailLogged < 3) {
                ++g_initFailLogged;
                tyr::trace::log("initGui failed (attempt %d) - retrying next frame", g_initFailLogged);
            }
        }

        st.restore();

        // GL error containment (Haswell Intel driver): the overlay pushes GL
        // calls the 2016 driver reports as GL_INVALID_ENUM (debug context).
        // Drain AFTER restore so nothing we do reaches the game's debug
        // logger - and log the first 3 with the surrounding frame number so a
        // future breakage still attributes itself.
        {
            GLenum err;
            int n = 0;
            while ((err = glGetError()) != GL_NO_ERROR) {
                if (g_glErrLogged < 3) {
                    ++g_glErrLogged;
                    tyr::trace::log("gl: 0x%x drained after frame %u", err, (unsigned)g_frameCount);
                }
                ++n;
                ++g_glDrainCount;
                g_glLastErr = (int)err;
            }
            (void)n;
        }

        ++g_frameCount;
    }

    return real(hdc);
}

// ---------------------------------------------------------------------------
// Windows reports both Shift keys as VK_SHIFT in some window procedures.
// Shift is distinguished by its scan code (right Shift is 0x36); unlike many
// right-side keys it does not use the extended-key bit.
// ---------------------------------------------------------------------------
static int normalizeVk(int vk, LPARAM lParam) {
    if (vk == VK_SHIFT) {
        const UINT scanCode = (UINT)((lParam >> 16) & 0xff);
        const UINT extendedScanCode = scanCode | ((lParam & (1u << 24)) ? 0xe000u : 0u);
        const UINT mapped = MapVirtualKeyW(extendedScanCode, MAPVK_VSC_TO_VK_EX);
        if (mapped == VK_RSHIFT) return VK_RSHIFT;
        if (mapped == VK_LSHIFT) return VK_LSHIFT;
        return scanCode == 0x36u ? VK_RSHIFT : VK_LSHIFT;
    }
    return vk;
}

// A modifier is held? GetKeyState reflects the state of the message queue this
// WndProc runs under, GetAsyncKeyState the physical keyboard. Accepting either
// keeps the emergency chord working no matter when the modifier transition
// lands relative to the KeyDown of the last key in the chord.
static bool modifierHeld(int vk) {
    return ((GetKeyState(vk) & 0x8000) != 0) || ((GetAsyncKeyState(vk) & 0x8000) != 0);
}

// ---------------------------------------------------------------------------
// Hooked WndProc
// ---------------------------------------------------------------------------
LRESULT CALLBACK hkWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WndProcScope active;
    const WNDPROC original=g_origWndProc.load(std::memory_order_acquire);
    // Input we posted ourselves (syntheticKey / syntheticClick) carries a tag
    // bit GLFW ignores. It must reach the game untouched, but it must never
    // look like the player's own key press or mouse tap to Passive — no keybind
    // or module may fire from input this client generated.
    const bool synthKey = (msg == WM_KEYDOWN || msg == WM_KEYUP ||
                           msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) &&
                          (lParam & (1 << 29)) != 0;
    const bool synthMouse = (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) &&
                            (wParam & 0x0080u) != 0;
    const bool synthetic = synthKey || synthMouse;
    // Feed our own input state first (menu toggle and key capture).
    // While the menu is open the game must receive NOTHING — but we still
    // record state so keybind capture works inside the GUI.
    const bool frozen = gui::inputFrozen();
    if (!synthetic) switch (msg) {
        case WM_KEYDOWN: case WM_SYSKEYDOWN: {
            const int vk = normalizeVk((int)wParam, lParam);
            // Emergency unload is a deliberate chord. A bare END used to unload
            // the client outright, so a single stray press - or a module bound to
            // END - ejected the DLL from a perfectly healthy game: literally the
            // "it uninjected itself and now nothing works" report. Require
            // Ctrl+Shift so no single key, keybind or hotkey tool can do it by
            // accident, and name the trigger in the log when it does fire.
            if (vk == VK_END && modifierHeld(VK_CONTROL) && modifierHeld(VK_SHIFT)) {
                tyr::trace::log("unload requested: emergency chord Ctrl+Shift+End");
                g_requestUnload.store(true);
            }
            gui::onKey(vk, true);
            break;
        }
        case WM_KEYUP: case WM_SYSKEYUP:
            gui::onKey(normalizeVk((int)wParam, lParam), false);
            break;
        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: gui::onMouse(VK_LBUTTON, true);  break;
        case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: gui::onMouse(VK_RBUTTON, true);  break;
        case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: gui::onMouse(VK_MBUTTON, true);  break;
        case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
            gui::onMouse((HIWORD(wParam) & XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2, true);
            break;
        case WM_LBUTTONUP: gui::onMouse(VK_LBUTTON, false); break;
        case WM_RBUTTONUP: gui::onMouse(VK_RBUTTON, false); break;
        case WM_MBUTTONUP: gui::onMouse(VK_MBUTTON, false); break;
        case WM_XBUTTONUP:
            gui::onMouse((HIWORD(wParam) & XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2, false);
            break;
        default: break;
    }

    if (g_guiInitialized && !synthetic) {
        // WM_SETCURSOR first, and owned by the client while a cursor is wanted: GLFW
        // answers it in CURSOR_DISABLED with SetCursor(NULL), and a null cursor over
        // the client area is a pointer the player cannot see while every click still
        // lands (the inventory / Escape menu going invisible). ImGui still picks the
        // shape - resize arrows, the text I-beam - so the interface keeps its cursors.
        if (msg == WM_SETCURSOR && (lParam & 0xffff) == HTCLIENT && gui::cursorWanted()) {
            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
            if (!GetCursor()) SetCursor(LoadCursorA(nullptr, IDC_ARROW));
            return TRUE;
        }
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
            if (gui::wantsMouse() || gui::wantsKeyboard())
                return TRUE;                    // swallowed: menu owns input
        }
        if (gui::wantsMouse() && (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST))
            return TRUE;
        if (gui::wantsKeyboard() && (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_CHAR || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP))
            return TRUE;
    }

    // Full freeze while the menu is open: the game gets zero input. ImGui has
    // already been fed above, so everything from here down is game-facing.
    if (frozen) {
        if (msg >= WM_KEYFIRST && msg <= WM_KEYLAST)   return 0;  // keys + chars
        if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) return 0; // mouse + wheel
        if (msg == WM_INPUT)                            return 0;  // raw camera
    }

    if (g_wndProcCb) {
        const bool consumed = g_wndProcCb(hwnd, (uint32_t)msg, (uint64_t)wParam, (int64_t)lParam);
        if (consumed) return TRUE;
    }
    (void)lParam;
    return original ? CallWindowProcW(original, hwnd, msg, wParam, lParam)
                    : DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
WNDPROC findMinecraftWndProc(HWND hwnd) {
    // Minecraft (LWJGL/GLFW) sets its own wndproc; grabbing the current one and
    // chaining from it is standard for injected overlays.
    return reinterpret_cast<WNDPROC>(
        GetWindowLongPtrW(hwnd, GWLP_WNDPROC));
}

} // namespace

void setRenderCallback(RenderCallback cb)  { g_renderCb = cb; }
void setWndProcCallback(WndProcCallback cb){ g_wndProcCb = cb; }
void setTickCallback(TickCallback cb)      { g_tickCb = cb; }
void setPreRenderCallback(PreRenderCallback cb) { g_preRenderCb = cb; }
uintptr_t preRenderCalls() { return (uintptr_t)g_preRenderCalls.load(std::memory_order_relaxed); }
uintptr_t drawWrites()    { return (uintptr_t)g_drawWrites.load(std::memory_order_relaxed); }
void* hwnd() { return g_hwnd; }
uintptr_t frameCount() { return g_frameCount.load(); }
bool guiInitialized() { return g_guiInitialized; }
bool renderCleanupComplete() { return g_renderCleanupDone.load(std::memory_order_acquire); }
int  glDrainCount() { return g_glDrainCount.load(std::memory_order_relaxed); }
int  glLastErr()    { return g_glLastErr.load(std::memory_order_relaxed); }

// Create+enable one detour. Tolerant of a re-run: install() is retried after a
// failure and rearm() re-applies the detours, and MinHook reports the second
// attempt as ALREADY_CREATED / ALREADY_INITIALIZED rather than as success.
static bool hookOne(void* target, void* detour, void** real) {
    if (!target) return false;
    MH_STATUS s = MH_CreateHook(target, detour, real);
    if (s != MH_OK && s != MH_ERROR_ALREADY_CREATED) return false;
    s = MH_EnableHook(target);
    return s == MH_OK || s == MH_ERROR_ENABLED;
}

bool install() {
    g_renderCleanupDone.store(false, std::memory_order_release);
    const MH_STATUS init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) return false;

    // Primary: wglSwapBuffers (opengl32) — Minecraft Java renders with OpenGL.
    HMODULE ogl = GetModuleHandleW(L"opengl32.dll");
    if (!ogl) ogl = LoadLibraryW(L"opengl32.dll");
    bool wglHooked = false;
    if (ogl) {
        if (void* target = reinterpret_cast<void*>(GetProcAddress(ogl, "wglSwapBuffers")))
            wglHooked = hookOne(target, reinterpret_cast<void*>(&hkWglSwapBuffers),
                                reinterpret_cast<void**>(&g_realWgl));
    }

    // Fallback/alternate: GDI SwapBuffers (gdi32) — some driver stacks bypass
    // wglSwapBuffers entirely. Hook it too; the handler de-dupes double calls.
    bool gdiHooked = false;
    if (HMODULE gdi = GetModuleHandleW(L"gdi32.dll")) {
        if (void* target = reinterpret_cast<void*>(GetProcAddress(gdi, "SwapBuffers")))
            gdiHooked = hookOne(target, reinterpret_cast<void*>(&hkGdiSwapBuffers),
                                reinterpret_cast<void**>(&g_realGdi));
    }

    // The pre-render hooks (glClear / glViewport) are NOT installed here: wglGetProcAddress only
    // answers for a thread with a current GL context, and install() runs on the init thread. They
    // are resolved and hooked from the first swap - see installPreRenderHooksFromRenderThread().

    tyr::trace::log("install: wglSwapBuffers hook=%s, gdi SwapBuffers hook=%s",
                    wglHooked ? "ok" : "FAILED", gdiHooked ? "ok" : "FAILED");
    if (!wglHooked && !gdiHooked) return false;

    // Immediate window pickup, scoped to THIS process: Minecraft's GLFW window
    // usually exists before we are injected, so grabbing it here gets WndProc
    // working even if frames stall. A desktop-wide search would also match
    // another running instance's window, which must never happen.
    if (HWND w = findOwnGlfwWindow()) {
        g_hwnd = w;
        g_hdc  = GetDC(w);
        wchar_t cls[64] = L"?";
        GetClassNameW(w, cls, 64);
        tyr::trace::log("window found at install (hwnd=%p class=%ls)", (void*)w, cls);
    }

    // WndProc hook happens lazily once hwnd is known (first frame).
    return true;
}

void ensureWndProcHooked() {
    if (!g_hwnd || !IsWindow(g_hwnd)) return;
    const WNDPROC cur = findMinecraftWndProc(g_hwnd);
    if (cur == &hkWndProc) return;          // still chained onto the window
    if (!cur) return;                       // window is going away

    const WNDPROC chained = g_origWndProc.load(std::memory_order_acquire);
    if (chained && cur != chained) {
        // Something else installed its own procedure on top of ours. Re-hooking
        // here would drop the game's own handler out of the chain, so leave the
        // window alone and record it: the menu key is polled every frame as
        // well, so the interface stays reachable without this chain.
        if (g_wndProcForeignLogs < 3) {
            ++g_wndProcForeignLogs;
            tyr::trace::log("wndproc chain was taken over by another procedure - leaving it in place");
        }
        return;
    }

    g_origWndProc.store(cur,std::memory_order_release);
    SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&hkWndProc));
    wchar_t cls[64] = L"?";
    GetClassNameW(g_hwnd, cls, 64);
    if (g_wndProcHookLogs < 3) {
        ++g_wndProcHookLogs;
        if (chained)
            tyr::trace::log("wndproc re-hooked after something restored the original chain (hwnd=%p class=%ls)",
                            (void*)g_hwnd, cls);
        else
            tyr::trace::log("wndproc hooked (hwnd=%p class=%ls)", (void*)g_hwnd, cls);
    }
}

void uninstall() {
    restoreWndProcAndWait();
    MH_DisableHook(MH_ALL_HOOKS);
    // Do not release MinHook trampolines or this DLL while an already-entered
    // WndProc/swap detour can still return through our code.
    while (g_activeDetours.load(std::memory_order_acquire) != 0) Sleep(1);
    // Let the final detour execute its return after dropping the active count.
    Sleep(25);
    MH_Uninitialize();
}

// ---------------------------------------------------------------------------
// rearm: re-apply the detours after the frame counter has gone quiet.
//
// The render detour lives on a function that the game, the graphics driver and
// any other overlay all patch. When something else restores the prologue our
// frames simply stop while Minecraft keeps running - exactly the "random
// uninject" report: the game is fine, the menu just never draws again.
// MinHook still believes the hooks are enabled after that, so MH_EnableHook on
// its own is a no-op; disabling first writes our saved original bytes back and
// the following enable re-applies the detour.
// ---------------------------------------------------------------------------
void rearm() {
    if (tyr::g_requestUnload.load(std::memory_order_acquire)) return;

    // The window may have been recreated (fullscreen toggle, driver reset) while
    // we were not receiving frames, which would also leave g_hwnd stale.
    if (HWND w = findOwnGlfwWindow()) {
        if (w != g_hwnd) adoptSwapWindow(w, GetDC(w));
    }
    // A minimised / hidden window is not swapping on purpose - do not churn the
    // hooks there.
    if (!g_hwnd || !IsWindow(g_hwnd) || IsIconic(g_hwnd) || !IsWindowVisible(g_hwnd)) return;

    MH_DisableHook(MH_ALL_HOOKS);
    MH_EnableHook(MH_ALL_HOOKS);
    ensureWndProcHooked();

    char cls[64] = "?";
    wchar_t wideCls[64] = L"?";
    GetClassNameW(g_hwnd, wideCls, 64);
    WideCharToMultiByte(CP_UTF8, 0, wideCls, -1, cls, sizeof(cls), nullptr, nullptr);
    tyr::trace::log("rearm: detours re-applied (hwnd=%p class=%s)", (void*)g_hwnd, cls);
}

// ---------------------------------------------------------------------------
// Synthetic input. Posted to the Minecraft window so GLFW's Win32 message loop
// sees ordinary transitions; the tag bits above keep Passive's own bookkeeping
// (keybinds, mouse taps, ImGui) out of the way.
// ---------------------------------------------------------------------------
void syntheticKey(int vk, bool down) {
    if (!g_hwnd || vk <= 0) return;
    const UINT scan = MapVirtualKeyW((UINT)vk, MAPVK_VK_TO_VSC) & 0xffu;
    // bit 24 = extended key, bit 29 = our tag (ignored by GLFW), bit 30 =
    // previous key state (set on key-up).
    LPARAM lp = (LPARAM)((scan << 16) | (1u << 29) | (down ? 0u : (1u << 30)));
    PostMessageW(g_hwnd, down ? WM_KEYDOWN : WM_KEYUP, (WPARAM)vk, lp);
}

void syntheticClick(bool down) {
    if (!g_hwnd) return;
    POINT p{};
    if (!GetCursorPos(&p)) return;
    ScreenToClient(g_hwnd, &p);
    WPARAM mods = 0x0080u;                       // synthetic tag (unused by GLFW)
    if (GetKeyState(VK_SHIFT)  & 0x8000) mods |= MK_SHIFT;
    if (GetKeyState(VK_CONTROL)& 0x8000) mods |= MK_CONTROL;
    if (GetKeyState(VK_MENU)   & 0x8000) mods |= 0x0020;   // MK_ALT
    if (down) mods |= MK_LBUTTON;
    PostMessageW(g_hwnd, down ? WM_LBUTTONDOWN : WM_LBUTTONUP,
                 mods, MAKELPARAM((SHORT)p.x, (SHORT)p.y));
}

} // namespace hooks
} // namespace tyr
