// ============================================================================
//  Passive — the DLL's published entry points (see exports.h).
//
//  Kept apart from agent.cpp so the bootstrap file stays about the bootstrap,
//  and so this pair of exports is small enough to audit at a glance: nothing
//  here does any work beyond recording the request the agent's own loop
//  already watches (tyr::g_requestUnload), which is exactly what the in-game
//  unload triggers set.
// ============================================================================
#define PASSIVE_AGENT_EXPORTS   // this file defines the exports (see exports.h)
#include "exports.h"
#include "agent.h"

#include <atomic>
#include <windows.h>

namespace {

// One unload per load: a second request would only race the teardown, and the
// flag itself is cleared when the DLL maps again (a fresh set of statics).
std::atomic<bool> g_unloadRequested{ false };

} // namespace

namespace tyr {

// A host that mapped this DLL (the Fabric bridge) owns the loader reference.
// Such an agent must not free itself: it would leave the host's reference as
// the last one, and the module would stay in the process after an uninject.
std::atomic<bool> g_hostManaged{ false };

// Set by the unload thread once hooks, JNI, ImGui and the log are gone. The
// host polls this before releasing its reference, so it never frees code that
// is still unwinding.
std::atomic<bool> g_unloadComplete{ false };

} // namespace tyr

extern "C" PASSIVE_AGENT_API int __cdecl passiveRequestUnload(void) {
    bool expected = false;
    if (!g_unloadRequested.compare_exchange_strong(expected, true)) return 0;
    tyr::g_requestUnload.store(true, std::memory_order_release);
    return 1;
}

extern "C" PASSIVE_AGENT_API void __cdecl passiveSetHostManaged(int managed) {
    tyr::g_hostManaged.store(managed != 0, std::memory_order_release);
}

extern "C" PASSIVE_AGENT_API int __cdecl passiveUnloadComplete(void) {
    return tyr::g_unloadComplete.load(std::memory_order_acquire) ? 1 : 0;
}

extern "C" PASSIVE_AGENT_API int __cdecl passiveAgentAbi(void) { return PASSIVE_AGENT_ABI; }
