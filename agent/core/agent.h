// ============================================================================
//  Passive — agent core (declarations)
// ============================================================================
#pragma once
#include <atomic>

namespace tyr {

extern std::atomic<bool> g_requestUnload;

// Set by passiveSetHostManaged. A host-managed agent tears down but leaves the
// module mapped, so the host can release the load reference it owns.
extern std::atomic<bool> g_hostManaged;

// Set by the unload thread at the very end of teardown; the host releases the
// module only after seeing this.
extern std::atomic<bool> g_unloadComplete;

// Lightweight in-process trace log, written next to javaw.exe (or %TEMP%).
// Opened in development mode to diagnose "nothing happens" reports.
namespace trace {
void log(const char* fmt, ...);
void setPath(const wchar_t* path);   // call before first log()
const wchar_t* path();
// Release the log handle. Called on the way out of an unload: the handle
// belongs to javaw, not to this DLL, so leaving it open keeps
// passive_agent.log locked for the rest of the game's life - which both hides
// the next session's log and blocks anyone from reading this one.
void close();
} // namespace trace

// One-time UI bootstrap that runs on the render thread after the first frame.
namespace agent {
void firstFrameInit();          // idempotent; safe to call every frame
void publishHandshake();        // called once the overlay render context is ready
void removeHandshake();
bool ready();                   // bootstrap finished (success or failure)
} // namespace agent

namespace info {
    constexpr const char* kName    = "Passive";
    constexpr const char* kVersion = "1.0.2";
    constexpr const char* kGame    = "Minecraft 1.21.11 (Fabric)";
}

} // namespace tyr
