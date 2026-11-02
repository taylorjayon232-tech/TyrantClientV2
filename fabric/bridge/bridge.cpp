// ============================================================================
//  Passive — Fabric bridge (passive_bridge.dll)
//
//  The Fabric jar cannot ship the client as Java: the client is C++ (MinHook
//  swap/WndProc hooks, ImGui, JNI module logic). So the jar carries the built
//  passive_agent.dll and this small shim, which is the only piece loaded with
//  System.load.
//
//  Why a shim at all, and not System.load on the agent itself: the JVM records
//  every library it loads and holds a handle for the life of the process. That
//  handle keeps the agent's refcount above zero, so the agent's own graceful
//  unload (FreeLibraryAndExitThread) could never actually unmap it - the DLL
//  would stay resident with its hooks torn down and the standalone injector
//  would still see it as loaded. Loading the agent through plain
//  LoadLibraryExW here leaves the JVM with no reference to it. The bridge's own
//  reference is released only after the agent finishes its teardown and reports
//  passiveUnloadComplete; this covers both bridge-driven uninjects and the
//  in-game SELF DESTRUCT / Ctrl+Shift+End chord.
//
//  The JNI surface it publishes (com.passive.fabric.PassiveNative):
//    nativeLoad(String dllPath) -> boolean   map the agent
//    nativeUnload()             -> boolean   ask it to unload itself, then wait
//    nativeLoaded()             -> boolean   is an agent mapped right now
//    nativeLastError()          -> String    why the last call said no
//    nativeBridgeAbi()          -> int       this bridge's ABI
//
//  Every decision the bridge makes is also written to the same
//  passive_agent.log the agent uses, so a launcher-side failure is diagnosable
//  from the one file the project already points people at.
// ============================================================================
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <tlhelp32.h>
#include <jni.h>

#include <string>
#include <vector>
#include <cstdio>
#include <cstdarg>
#include <atomic>

#include "exports.h"   // PASSIVE_AGENT_ABI — shared with the agent, so the pair cannot drift

namespace {

constexpr int kBridgeAbi = 1;

// How long to wait for a requested unload to actually take the module out of
// the process. The agent's teardown waits up to 2 s for the GL context to
// report cleanup before it removes the hooks, and only then frees itself.
constexpr int kUnloadWaitMs    = 10000;
constexpr int kUnloadPollMs    = 50;

std::wstring g_agentPath;    // last DLL this bridge mapped (diagnostics only)
std::wstring g_lastError;    // why the last call said no, for the mod to report

// The module this bridge mapped. The bridge owns one LoadLibrary reference to
// it, and that reference is the reason an in-game uninject used to leave the
// DLL in the process: the agent only ever dropped its own reference. The
// release watcher below frees this handle once the agent reports its teardown
// complete, which is the only safe moment to unmap it.
std::atomic<HMODULE> g_ownedModule{ nullptr };
std::atomic<bool> g_unloadRequested{ false };

// ---------------------------------------------------------------------------
// trace: the agent's log, opened and closed per line.
//
// The agent keeps one append handle for the life of a load (and closes it on
// the way out of an unload). The bridge logs so rarely - a few lines per
// load/unload - that opening for each line is both simpler and safer: it never
// holds the file, so it can never be the reason the agent's own reopen fails.
// ---------------------------------------------------------------------------
std::wstring logPath() {
    wchar_t exeDir[MAX_PATH] = L"";
    if (GetModuleFileNameW(nullptr, exeDir, MAX_PATH)) {
        if (wchar_t* slash = wcsrchr(exeDir, L'\\')) {
            *(slash + 1) = 0;
            return std::wstring(exeDir) + L"passive_agent.log";
        }
    }
    wchar_t tmp[MAX_PATH] = L"";
    if (GetTempPathW(MAX_PATH, tmp)) return std::wstring(tmp) + L"passive_agent.log";
    return L"passive_agent.log";
}

void trace(const char* fmt, ...) {
    const std::wstring path = logPath();
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"ab") != 0 || !f) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    char head[96];
    int n = _snprintf_s(head, sizeof(head), _TRUNCATE,
                        "[%02d:%02d:%02d.%03d] bridge(pid %lu): ",
                        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                        (unsigned long)GetCurrentProcessId());
    fwrite(head, 1, (size_t)n, f);

    char body[512];
    va_list ap;
    va_start(ap, fmt);
    n = _vsnprintf_s(body, sizeof(body), _TRUNCATE, fmt, ap);
    va_end(ap);
    if (n > 0) fwrite(body, 1, (size_t)n, f);
    fputc('\n', f);
    fclose(f);
}

// ---------------------------------------------------------------------------
// The mapped agent, found by module name.
//
// Everything is found through a fresh toolhelp snapshot rather than a handle
// this bridge kept: once an unload starts, the agent's module is freed from
// under us, and a remembered HMODULE would then be a stale base address that
// no Win32 call may be given.
// ---------------------------------------------------------------------------
bool isAgentModuleName(const wchar_t* name) {
    constexpr size_t kPrefix = 12;   // len(L"passive_agent")
    if (_wcsnicmp(name, L"passive_agent", kPrefix) != 0) return false;
    const size_t len = wcslen(name);
    if (len < kPrefix + 4) return false;
    return _wcsicmp(name + len - 4, L".dll") == 0;
}

std::vector<HMODULE> agentModules() {
    std::vector<HMODULE> out;
    const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) return out;
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Module32FirstW(snap, &entry)) {
        do {
            if (isAgentModuleName(entry.szModule)) out.push_back(entry.hModule);
        } while (Module32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return out;
}

// The agent publishes this mutex from its first rendered frame and removes it
// as the first step of teardown. Its absence is the earliest observable sign
// that an uninject has started; passiveUnloadComplete says when it is safe to
// release the module.
bool agentHandshakeExists() {
    wchar_t name[64]{};
    _snwprintf_s(name, _TRUNCATE, L"Passive_Agent_Mutex_%lu", (unsigned long)GetCurrentProcessId());
    HANDLE mutex = OpenMutexW(SYNCHRONIZE, FALSE, name);
    if (!mutex) return false;
    CloseHandle(mutex);
    return true;
}

bool moduleIsMapped(HMODULE module) {
    for (HMODULE mapped : agentModules()) {
        if (mapped == module) return true;
    }
    return false;
}

// Whether the mapped agent can report completion at all. A payload without the export can never
// answer, so the release watcher must not wait on it for the rest of the game's life.
bool hasTeardownExport(HMODULE module) {
    return module && GetProcAddress(module, "passiveUnloadComplete") != nullptr;
}

bool agentTeardownComplete(HMODULE module) {
    using CompleteFn = int(__cdecl*)();
    const auto complete = reinterpret_cast<CompleteFn>(GetProcAddress(module, "passiveUnloadComplete"));
    return complete != nullptr && complete() != 0;
}

// Releases the bridge's reference once the agent has finished tearing down.
// The agent is told at load time that the bridge owns that reference
// (passiveSetHostManaged), so it deliberately does not free itself. Waiting for
// passiveUnloadComplete keeps this from unmapping code the unload thread is
// still returning through.
DWORD WINAPI releaseAgentModule(LPVOID) {
    HMODULE seen = nullptr;
    bool sawHandshake = false;
    ULONGLONG absentSince = 0;

    for (;;) {
        Sleep(250);

        HMODULE owned = g_ownedModule.load(std::memory_order_acquire);
        if (!owned) {
            seen = nullptr;
            sawHandshake = false;
            absentSince = 0;
            continue;
        }
        if (owned != seen) {
            seen = owned;
            sawHandshake = false;
            absentSince = 0;
        }
        if (!moduleIsMapped(owned)) {
            HMODULE expected = owned;
            g_ownedModule.compare_exchange_strong(expected, nullptr);
            g_unloadRequested.store(false, std::memory_order_release);
            continue;
        }
        if (agentHandshakeExists()) {
            sawHandshake = true;
            absentSince = 0;
            continue;
        }
        // A client that never rendered has no handshake, but an explicit
        // bridge unload still has to be able to release it.
        if (!sawHandshake && !g_unloadRequested.load(std::memory_order_acquire)) continue;

        const ULONGLONG now = GetTickCount64();
        if (absentSince == 0) {
            absentSince = now;
            continue;
        }
        if (now - absentSince < 250) continue;
        // An agent confirms its teardown through passiveUnloadComplete. A payload without that export
        // can never confirm, and waiting on it forever leaves the DLL in the process after an
        // "uninject" - so for those the handshake staying away for a few seconds is the signal.
        if (!agentTeardownComplete(owned)) {
            if (hasTeardownExport(owned) || now - absentSince < 4000) continue;
            trace("the mapped agent has no passiveUnloadComplete; releasing after its handshake went away");
        }

        // The unload thread sets the flag and then returns. Give it time to
        // leave the DLL before the module is unmapped under it.
        Sleep(500);
        // Release every reference this bridge can reach, not just one: a second loader reference (a
        // copy something else mapped, or a re-inject that reused this module) would otherwise keep
        // the DLL resident even though the client had finished tearing itself down.
        for (int attempt = 0; attempt < 8 && moduleIsMapped(owned); ++attempt) {
            trace("releasing the agent module after its teardown (reference %d)", attempt + 1);
            if (!FreeLibrary(owned)) {
                trace("WARN: FreeLibrary on the agent module failed (error %lu)",
                      (unsigned long)GetLastError());
                break;
            }
            Sleep(50);
        }
        if (moduleIsMapped(owned)) {
            trace("WARN: the agent module is still mapped after its teardown - "
                  "a loader reference outside this bridge is holding it");
        }

        g_ownedModule.store(nullptr, std::memory_order_release);
        g_unloadRequested.store(false, std::memory_order_release);
        seen = nullptr;
        sawHandshake = false;
        absentSince = 0;
    }
    return 0;
}

using UnloadFn = int(__cdecl*)();

// The unload entry point of whatever agent is mapped, or nullptr.
UnloadFn mappedAgentUnloadEntry() {
    for (HMODULE module : agentModules()) {
        const auto fn = reinterpret_cast<UnloadFn>(GetProcAddress(module, "passiveRequestUnload"));
        if (fn) return fn;
    }
    return nullptr;
}

std::wstring toWide(JNIEnv* env, jstring text) {
    std::wstring out;
    if (!text) return out;
    const jchar* chars = env->GetStringChars(text, nullptr);
    if (!chars) return out;
    out.assign(reinterpret_cast<const wchar_t*>(chars), static_cast<size_t>(env->GetStringLength(text)));
    env->ReleaseStringChars(text, chars);
    return out;
}

jstring toJava(JNIEnv* env, const std::wstring& text) {
    return env->NewString(reinterpret_cast<const jchar*>(text.c_str()), static_cast<jsize>(text.size()));
}

jboolean fail(const wchar_t* why) {
    g_lastError = why;
    return JNI_FALSE;
}

} // namespace

extern "C" {

JNIEXPORT jint JNICALL Java_com_passive_fabric_PassiveNative_nativeBridgeAbi(JNIEnv*, jclass) {
    return kBridgeAbi;
}

JNIEXPORT jstring JNICALL Java_com_passive_fabric_PassiveNative_nativeLastError(JNIEnv* env, jclass) {
    return toJava(env, g_lastError);
}

JNIEXPORT jboolean JNICALL Java_com_passive_fabric_PassiveNative_nativeLoaded(JNIEnv*, jclass) {
    return agentModules().empty() ? JNI_FALSE : JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_passive_fabric_PassiveNative_nativeLoad(JNIEnv* env, jclass, jstring path) {
    g_lastError.clear();

    const std::wstring dll = toWide(env, path);
    if (dll.empty()) { trace("load refused: no dll path"); return fail(L"no DLL path was given"); }

    // Loading a second copy while one is mapped would hook the swap function
    // twice and draw two overlays: refuse, exactly like the injector does.
    if (!agentModules().empty()) {
        trace("load refused: an agent is already mapped");
        return fail(L"an agent is already mapped in this process");
    }

    const HMODULE module = LoadLibraryExW(dll.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!module) {
        const DWORD err = GetLastError();
        trace("load failed for the payload (error %lu)", (unsigned long)err);
        return fail(L"the agent DLL could not be loaded (see passive_agent.log for the error code)");
    }

    const auto abi = reinterpret_cast<int(__cdecl*)()>(GetProcAddress(module, "passiveAgentAbi"));
    if (!abi) {
        trace("load refused: the payload has no passiveAgentAbi export");
        FreeLibrary(module);
        return fail(L"the payload is not a Passive agent DLL");
    }
    const int found = abi();
    if (found != PASSIVE_AGENT_ABI) {
        trace("load refused: agent ABI %d, bridge wants %d", found, PASSIVE_AGENT_ABI);
        FreeLibrary(module);
        return fail(L"the jar's agent and bridge are from different builds");
    }

    // This bridge owns the LoadLibrary reference to the module. Tell the agent
    // before it can start any teardown, so its unload path leaves the actual
    // release to us instead of trying to free itself and leaving this reference
    // behind (the old stuck-DLL bug).
    using HostManagedFn = void(__cdecl*)(int);
    const auto hostManaged = reinterpret_cast<HostManagedFn>(
        GetProcAddress(module, "passiveSetHostManaged"));
    if (!hostManaged) {
        trace("load refused: the payload has no passiveSetHostManaged export");
        FreeLibrary(module);
        return fail(L"the payload is too old for this bridge (no host-managed unload)");
    }
    hostManaged(1);
    g_ownedModule.store(module, std::memory_order_release);
    g_unloadRequested.store(false, std::memory_order_release);

    g_agentPath = dll;
    trace("agent mapped (abi %d) from the jar payload; the bridge owns the module reference", found);
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_passive_fabric_PassiveNative_nativeUnload(JNIEnv*, jclass) {
    g_lastError.clear();

    if (agentModules().empty()) return fail(L"no agent is mapped in this process");

    const UnloadFn request = mappedAgentUnloadEntry();
    if (!request) {
        trace("unload refused: the mapped agent has no passiveRequestUnload export");
        return fail(L"the mapped agent does not offer an unload (it is an older build)");
    }

    // If this bridge mapped the client, its own reference is what keeps the
    // module resident after the agent's teardown. The release watcher uses this
    // flag to release it even when the client never published a handshake.
    if (g_ownedModule.load(std::memory_order_acquire)) {
        g_unloadRequested.store(true, std::memory_order_release);
    }

    trace("unload requested through the agent's own export");
    if (request() == 0) trace("the agent had already been asked to unload");

    for (int waited = 0; waited < kUnloadWaitMs; waited += kUnloadPollMs) {
        if (agentModules().empty()) {
            trace("unload complete: the agent's module left the process");
            g_agentPath.clear();
            return JNI_TRUE;
        }
        Sleep(kUnloadPollMs);
    }

    trace("unload timed out: the agent's module is still mapped after %d ms", kUnloadWaitMs);
    return fail(L"the agent is still mapped after its unload was requested");
}

} // extern "C"

// Starts the release watcher when the JVM loads the bridge. The watcher has to
// outlive every client load: it is what releases the module reference this
// bridge owns after an in-game uninject, not just a bridge-driven one.
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*) {
    static std::atomic<bool> started{ false };
    bool expected = false;
    if (started.compare_exchange_strong(expected, true)) {
        if (HANDLE watcher = CreateThread(nullptr, 0, &releaseAgentModule, nullptr, 0, nullptr)) {
            CloseHandle(watcher);
        }
    }
    return JNI_VERSION_1_8;
}
