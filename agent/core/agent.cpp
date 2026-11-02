// ============================================================================
//  Passive — agent core: DLL entry, render-thread bootstrap and UI wiring.
// ============================================================================
#include "agent.h"
#include "hooks.h"
#include "../gui/gui.h"
#include "../jni/jvm.h"
#include "../jni/mc.h"
#include "../modules/modules.h"
#include "../modules/java_modules.h"

#include <windows.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstdarg>
#include <shlobj.h>

namespace tyr {

std::atomic<bool> g_requestUnload{ false };
static std::atomic<bool> g_watchdogAlive{ false };   // pose watchdog thread is still running

// ---------------------------------------------------------------------------
// trace: tiny append-only log so failures are visible without a debugger.
// ---------------------------------------------------------------------------
namespace trace {

static FILE* g_file = nullptr;
static wchar_t g_path[MAX_PATH] = L"";

void setPath(const wchar_t* path) {
    if (path && !g_file) { wcsncpy_s(g_path, path, _TRUNCATE); }
}

const wchar_t* path() { return g_path; }

// Open the trace file with explicit sharing. The client's log has to stay
// readable while the game is running - by the user, by an editor, by a
// diagnostic tool - and the previous open mode denied that, so the only way to
// look at a live log was to close the game first.
static bool openLogFile(const wchar_t* use) {
    if (!use || !use[0]) return false;
    g_file = _wfsopen(use, L"ab", _SH_DENYNO);
    if (!g_file) return false;
    wcsncpy_s(g_path, use, _TRUNCATE);
    return true;
}

// A client that uninjected leaves its own append handle open inside javaw (the
// handle belongs to the process, not to the DLL, so unloading does not close
// it). While that handle is held the plain name cannot be opened at all. Fail
// over to a numbered sibling instead of silently recording nothing - losing
// the log is worse than the log having a slightly different name.
static bool openFallbackLog() {
    const wchar_t* base = g_path[0] ? g_path : L"passive_agent.log";
    if (openLogFile(base)) return true;
    const std::wstring full(base);
    const size_t dot = full.find_last_of(L'.');
    const std::wstring stem = (dot == std::wstring::npos) ? full : full.substr(0, dot);
    const std::wstring ext  = (dot == std::wstring::npos) ? L""   : full.substr(dot);
    for (int i = 1; i <= 9; ++i) {
        wchar_t alt[MAX_PATH]{};
        _snwprintf_s(alt, _TRUNCATE, L"%s_%d%s", stem.c_str(), i, ext.c_str());
        if (openLogFile(alt)) return true;
    }
    return false;
}

void close() {
    if (g_file) { fclose(g_file); g_file = nullptr; }
}

void log(const char* fmt, ...) {
    if (!g_file) {
        if (!openFallbackLog()) return;            // give up silently
    }
    SYSTEMTIME st; GetLocalTime(&st);
    char head[64];
    int n = _snprintf_s(head, sizeof(head), _TRUNCATE,
                        "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    fwrite(head, 1, size_t(n), g_file);
    char buf[512];
    va_list ap; va_start(ap, fmt);
    n = _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    if (n > 0) fwrite(buf, 1, size_t(n), g_file);
    fputc('\n', g_file);
    fflush(g_file);
}

} // namespace trace

// ---------------------------------------------------------------------------
// agent bootstrap state
// ---------------------------------------------------------------------------
namespace agent {

static std::atomic<bool> g_bootstrapped{ false };
static HANDLE g_handshakeMutex = nullptr;

bool ready() { return g_bootstrapped.load(); }

void publishHandshake() {
    if (g_handshakeMutex) return;
    wchar_t name[64]{};
    _snwprintf_s(name,_TRUNCATE,L"Passive_Agent_Mutex_%lu",(unsigned long)GetCurrentProcessId());
    g_handshakeMutex=CreateMutexW(nullptr,FALSE,name);
    if (g_handshakeMutex) trace::log("overlay handshake published");
    else trace::log("WARN: could not publish overlay handshake (error %lu)",GetLastError());
}

void removeHandshake() {
    if (g_handshakeMutex) { CloseHandle(g_handshakeMutex); g_handshakeMutex=nullptr; }
}

void firstFrameInit() {
    bool expected = false;
    if (!g_bootstrapped.compare_exchange_strong(expected, true)) return; // already ran

    trace::log("first render frame — bootstrapping on render thread");

    if (jni::init()) {
        trace::log("JNI attached to the running JVM");
        if (mc::resolveAll()) trace::log("Minecraft JNI mappings resolved");
        else trace::log("WARN: some Minecraft JNI mappings were not resolved");
    } else {
        trace::log("WARN: JNI could not attach; visual menu remains available");
    }

    gui::init();
    trace::log("agent ready — press RSHIFT in game");
}

} // namespace agent

namespace {

// Every render stage is isolated so a throw names itself in the log instead of
// leaving the player with a client that looks like it silently uninjected. The
// exception is re-thrown to the swap detour, which closes the ImGui frame this
// path opened: carrying on and drawing a half-built frame would trip ImGui's
// own sanity checks and take the game down with it.
void logStageThrow(const char* stage) {
    static int logged = 0;
    if (logged < 6) {
        ++logged;
        trace::log("render stage %s threw - frame aborted, client kept running", stage);
    }
}

// Per-frame JNI scope, held in a guard so a throw from a stage cannot leave the
// local reference frame pushed (or the frame's global references pinned) - a
// leak on a path that would otherwise never release them.
struct JniFrameScope {
    bool active = false;
    bool pushed = false;
    bool valid  = false;

    // Keep JNI locals scoped to this render frame; ESP reacquires entities each
    // frame and never stores Java object references across frames. Pushing a
    // local frame here is essential: this code runs on an already-attached Java
    // thread (not a native method), so local references created each frame are
    // NOT reclaimed until the thread detaches. Without the frame a single leaked
    // reference per frame eventually exhausts the local-reference capacity and
    // the overlay (ESP first, being the heaviest JNI user) silently stops drawing.
    JniFrameScope() {
        active = jni::ready() && mc::frame::resolved();
        if (!active) return;
        JNIEnv* frameEnv = jni::env();
        // The reserve is the depth every module's entity scan can hold at once: the world scans read
        // up to kEntityScan (1024) entities and keep a local reference per entity, so 512 was the
        // old ceiling and a scan that outgrew it silently lost entities. 4096 leaves room for the
        // scan plus everything else a frame builds.
        pushed = frameEnv && frameEnv->PushLocalFrame(4096) == 0;
        mc::frame::beginFrame();
        valid = true;
    }
    ~JniFrameScope() {
        if (!active) return;
        mc::frame::endFrame();
        if (pushed) {
            JNIEnv* frameEnv = jni::env();
            if (frameEnv) frameEnv->PopLocalFrame(nullptr);
        }
    }
};

void renderFrame() {
    // 0) One-time bootstrap now runs here, on the render thread.
    agent::firstFrameInit();

    // The frame path below needs an ImGui context, which only exists once a
    // real swap frame has run initGui(). Without this guard the fallback
    // one-shot (no render frames on a driver stack / test host) dereferenced
    // ImGui's null context and killed the host process.
    if (!hooks::guiInitialized()) return;

    agent::publishHandshake();

    // 1) Interface-level per-frame state (animations and notifications).
    try {
        gui::preFrame();
    } catch (...) {
        logStageThrow("gui::preFrame");
        throw;
    }

    {
        JniFrameScope frame;
        const float renderTickDelta = frame.valid ? mc::Client::renderTickDelta() : -1.f;
        try {
            modules::drawOverlay(renderTickDelta >= 0.f ? renderTickDelta : gui::tickDelta());
        } catch (...) {
            logStageThrow("modules::drawOverlay");
            throw;
        }
    }

    // Draw the interface over the Minecraft frame.
    try {
        gui::drawFrame();
    } catch (...) {
        logStageThrow("gui::drawFrame");
        throw;
    }

    // 5) Keep the WndProc hookup alive (window can be recreated).
    hooks::ensureWndProcHooked();
}

// ---------------------------------------------------------------------------
// Render wiring: the hook must know about renderFrame BEFORE the first swap.
// Earlier builds defined renderFrame but never called setRenderCallback, so
// the frame path existed yet nothing was ever connected to it — exactly the
// "hooks::install ok, then silence" symptom.
// ---------------------------------------------------------------------------
std::once_flag g_renderWired;
void wireRenderCallbackOnce() {
    std::call_once(g_renderWired, [] {
        hooks::setRenderCallback(&renderFrame);
        // Body/head rotation has to be written after the game's tick and before the model is drawn.
        hooks::setPreRenderCallback(&modules::applySilentBodyPose);
        // Input messages arrive before the game's tick: that is where a block place / break press
        // hands the server the camera rotation, so the action is never validated against the aim.
        hooks::setWndProcCallback(&modules::onInputMessage);
        trace::log("render callback wired (agent -> hooks)");
    });
}

// ---------------------------------------------------------------------------
// initThread: DLL bootstrap (log setup and hook install).
// Defined below, at namespace tyr scope; a forward declaration here lets the
// global-scope DllMain shim (bottom of file) reach it.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Unload worker: runs teardown on its own thread; hook teardown closes ImGui.
// ---------------------------------------------------------------------------
DWORD WINAPI unloadThread(LPVOID initThreadHandle) {
    HANDLE initHandle = static_cast<HANDLE>(initThreadHandle);
    if (initHandle) {
        WaitForSingleObject(initHandle, INFINITE);
        CloseHandle(initHandle);
    }
    trace::log("unload requested: tearing down");
    // The render hook tears down ImGui's OpenGL backend with the game's GL
    // context current. Wait for that frame to finish before removing hooks -
    // but never forever: if the game stopped swapping (minimised, context lost)
    // the hook will not run again, and blocking here would leave the client
    // half-unloaded instead of gone.
    bool cleanupDone = false;
    for (int i = 0; i < 2000 && !(cleanupDone = hooks::renderCleanupComplete()); ++i) Sleep(1);
    if (!cleanupDone) trace::log("WARN: GL cleanup never reported - unloading anyway");
    // The pose watchdog may be mid-write through JNI; wait for it to leave its loop before the
    // DLL's code can be freed under it. Bounded, so a wedged thread cannot hang the unload.
    for (int i = 0; i < 3000 && g_watchdogAlive.load(); ++i) Sleep(1);
    // That thread stores "gone" as the last thing it does, and still has to unwind through this
    // DLL's runtime afterwards. Give it that moment before the module can be unmapped under it.
    Sleep(30);
    hooks::uninstall();
    modules::java::shutdown();   // drop the Java host while the VM link is still up
    jni::detach();          // release the class-loader global ref, detach our threads
    gui::shutdown();
    agent::removeHandshake();

    // Release the log handle before the DLL is freed. It belongs to javaw, not
    // to this DLL, so leaking it locks passive_agent.log - and blocks the next
    // injection from logging anything - for the rest of the game's life.
    const bool hostManaged = g_hostManaged.load(std::memory_order_acquire);
    trace::log("teardown complete - releasing the log");
    if (hostManaged)
        trace::log("teardown complete - passiveUnloadComplete is set; the host releases the module");
    else
        trace::log("teardown complete - the module frees itself and leaves the process");
    trace::close();

    // A host that loaded this DLL (the Fabric bridge) owns the LoadLibrary
    // reference. Freeing ourselves here would only leave that reference behind,
    // so the module would remain mapped after the uninject. Report completion
    // and let the host release its own handle instead.
    if (hostManaged) {
        g_unloadComplete.store(true, std::memory_order_release);
        return 0;
    }

    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&unloadThread), &self);
    if (self) FreeLibraryAndExitThread(self, 0);
    return 0;
}

} // namespace

} // namespace tyr

// NOTE: DllMain MUST be at global scope. Defined inside namespace tyr it
// would never be found by the CRT startup stub, which silently falls back
// to its default no-op DllMain — the agent would load and do NOTHING
// (exactly the "injected but no log, no menu" bug).
namespace tyr { DWORD WINAPI initThread(LPVOID); }   // fwd: defined below
DWORD WINAPI tyrInitThread(LPVOID) { return tyr::initThread(nullptr); }

namespace tyr {

DWORD WINAPI initThread(LPVOID) {
    // Diagnostic log: next to javaw.exe when writable (Minecraft dir),
    // else %TEMP%. Written from the init thread only — no locking needed.
    {
        wchar_t exeDir[MAX_PATH] = L"";
        GetModuleFileNameW(nullptr, exeDir, MAX_PATH);
        wchar_t* slash = wcsrchr(exeDir, L'\\');
        if (slash) *(slash + 1) = 0;
        // %TEMP% only: always writable, one fixed path - the log is found without hunting for
        // whichever directory javaw.exe happens to live in.
        wchar_t tmp[MAX_PATH];
        if (GetTempPathW(MAX_PATH, tmp))
            trace::setPath((std::wstring(tmp) + L"passive_agent.log").c_str());
        else {
            wchar_t cand[MAX_PATH];
            _snwprintf_s(cand, _TRUNCATE, L"%spassive_agent.log", exeDir);
            FILE* probe = nullptr;
            if (_wfopen_s(&probe, cand, L"ab") == 0 && probe) { fclose(probe); trace::setPath(cand); }
        }
    }
    trace::log("==== passive agent dll mapped ====");

    // A failed install used to park this thread on Sleep(INFINITE), which left
    // the DLL loaded but inert: the injector then reported "the DLL loaded, but
    // the overlay did not initialize" and the client never came back - the same
    // result on screen as an eject. The swap target and the loader lock can
    // still be settling right after injection, so retry before giving up.
    bool installed = hooks::install();
    for (int attempt = 1; !installed && attempt <= 30 && !g_requestUnload.load(); ++attempt) {
        trace::log("hooks::install failed (attempt %d) — retrying", attempt);
        Sleep(2000);
        installed = hooks::install();
    }
    if (!installed) {
        trace::log("FATAL: hooks::install never succeeded (no wglSwapBuffers/SwapBuffers?)");
    } else {
        trace::log("hooks::install ok — waiting for first frame");
        wireRenderCallbackOnce();

        // Fallback: if no render frame arrives within 8 s, bootstrap GUI state so
        // the log distinguishes a hook issue from an interface rendering issue.
        for (int i = 0; i < 80 && hooks::frameCount() == 0; ++i) Sleep(100);
        if (hooks::frameCount() == 0) {
            trace::log("WARN: no render frame after 8s — hook target never invoked; fallback init");
            agent::firstFrameInit();
            renderFrame();                      // one-shot; safe no-op w/o ImGui ctx
            trace::log("fallback frame complete — overlay requires a firing frame hook");
        }

        // Pose watchdog (see modules::silentPoseWatchdogWrite). The game's tick can rewrite the
        // body/head rotation between our frame hooks and the moment the model is built; this thread
        // undoes that within ~100 us while the silent aim owns the pose, so no frame ever interpolates
        // between the aim and the camera. It only spins while the aim is holding - otherwise it sleeps.
        g_watchdogAlive.store(true);
        std::thread([] {
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));   // let the game settle
            LARGE_INTEGER freq{};
            QueryPerformanceFrequency(&freq);
            const LONGLONG step = freq.QuadPart / 10000;    // 100 us between writes
            LONGLONG nextAt = 0;
            while (!g_requestUnload.load(std::memory_order_relaxed)) {
                if (!modules::silentAimHoldingPose()) { Sleep(5); continue; }
                LARGE_INTEGER now{};
                QueryPerformanceCounter(&now);
                if (now.QuadPart >= nextAt) {
                    modules::silentPoseWatchdogWrite();
                    nextAt = now.QuadPart + step;
                } else {
                    YieldProcessor();
                }
            }
            g_watchdogAlive.store(false);   // teardown waits for this before freeing the DLL
        }).detach();
    }

    // ---- heartbeat: re-arm the hooks if the frame counter goes quiet ---------
    // The render detour lives on a function the game, the driver and any other
    // overlay all touch. If something else restores its prologue the frames just
    // stop and the menu never draws again - the "random uninject" report where
    // Minecraft itself keeps running. Notice a stalled counter with a live
    // window and re-apply the detour instead of sitting there dead.
    uintptr_t lastFrames   = hooks::frameCount();
    ULONGLONG lastProgress = GetTickCount64();
    ULONGLONG lastBeat     = lastProgress;
    int  rearmAttempts = 0;    // bounded per stall episode, so a game that is
                               // simply not rendering cannot churn the hooks
    while (!g_requestUnload.load()) {
        Sleep(120);
        const uintptr_t frames = hooks::frameCount();
        const ULONGLONG now = GetTickCount64();
        const HWND window = static_cast<HWND>(hooks::hwnd());
        const bool onScreen = window && IsWindow(window) &&
                              IsWindowVisible(window) && !IsIconic(window);

        // A once-a-minute liveness line. When the overlay goes quiet this is the
        // last thing in the log, so the frame count it names is how far the
        // client got before it stopped - the number a stall report needs.
        if (now - lastBeat >= 60000) {
            lastBeat = now;
            trace::log("heartbeat: %llu frames rendered, window %s",
                       (unsigned long long)frames, onScreen ? "on screen" : "hidden or minimised");
        }

        if (frames != lastFrames) {                // frames are flowing again
            lastFrames = frames;
            lastProgress = now;
            rearmAttempts = 0;
            continue;
        }
        if (now - lastProgress < 5000) continue;   // still fresh
        lastProgress = now;                        // at most one repair per 5 s
        // A hidden or minimised window is not swapping on purpose. Repairing it
        // would only churn the hooks and fill the log with false alarms.
        if (!installed || !onScreen || rearmAttempts >= 3) continue;
        if (rearmAttempts == 0)
            trace::log("render frames stalled for >5s — re-arming the hooks");
        ++rearmAttempts;
        hooks::rearm();
    }

    // ---- graceful unload (on a dedicated thread, see unloadThread) ----------
    HANDLE callerHandle = nullptr;
    const BOOL duplicated = DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(),
                                            &callerHandle, 0, FALSE, DUPLICATE_SAME_ACCESS);
    HANDLE worker = duplicated
        ? CreateThread(nullptr, 0, &unloadThread, callerHandle, 0, nullptr)
        : nullptr;
    if (worker) {
        CloseHandle(worker);
    } else {
        if (callerHandle) CloseHandle(callerHandle);
        trace::log("FATAL: could not start synchronized unload worker");
    }
    // initThread ends here; the unload thread frees the DLL.
    return 0;
}

} // namespace tyr

// ---------------------------------------------------------------------------
// DllMain — GLOBAL SCOPE. The CRT's DLL startup stub looks for ::DllMain
// specifically; an earlier version of this file defined it inside namespace
// tyr, so the stub silently used its built-in no-op and the agent never ran.
// ---------------------------------------------------------------------------
BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        if (HANDLE init = CreateThread(nullptr, 0, &tyrInitThread, nullptr, 0, nullptr))
            CloseHandle(init);
    } else if (reason == DLL_PROCESS_DETACH && reserved != nullptr) {
        // process termination: nothing safe to do; OS reclaims everything.
    }
    return TRUE;
}
