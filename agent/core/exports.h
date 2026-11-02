// ============================================================================
//  Passive — published entry points of passive_agent.dll.
//
//  The standalone injector maps the DLL with CreateRemoteThread(LoadLibraryW)
//  and never calls into it: the DLL starts itself from DllMain. A host that
//  loads it in-process instead (the Fabric jar's bridge, passive_bridge.dll)
//  needs the same two things the injector's user gets from the game itself -
//  a version to check the pair against, and a way to ask for the graceful
//  unload the Ctrl+Shift+End chord performs. Both live here so the export
//  surface is one file.
// ============================================================================
#pragma once

// Bumped whenever this surface changes. The bridge refuses to drive a DLL it
// does not understand, so a mismatched pair fails loudly instead of half-way.
// 2: a host may own the module reference (passiveSetHostManaged) and the agent
//    reports teardown completion instead of freeing itself.
#define PASSIVE_AGENT_ABI 2

// The agent builds these as exports (exports.cpp defines PASSIVE_AGENT_EXPORTS).
// A host that only needs the declarations - the Fabric bridge, which includes
// this file for PASSIVE_AGENT_ABI - must not export symbols it does not define,
// so the attribute travels with the macro rather than sitting in the header.
#ifdef PASSIVE_AGENT_EXPORTS
#define PASSIVE_AGENT_API __declspec(dllexport)
#else
#define PASSIVE_AGENT_API
#endif

extern "C" {

// Ask the agent to tear itself down: the swap and WndProc hooks are removed,
// ImGui and the JNI references are released, and the log handle is closed - the
// same path the in-game SELF DESTRUCT button and the Ctrl+Shift+End chord take.
// A standalone-loaded agent then frees itself with FreeLibraryAndExitThread; a
// host-managed one (passiveSetHostManaged) reports passiveUnloadComplete and
// lets its host release the module reference instead.
//
// Returns 1 if this call started the teardown, 0 if one was already asked for.
// The teardown runs on its own thread, so a caller that needs the module gone
// must watch for the module to leave the process rather than wait on this.
PASSIVE_AGENT_API int __cdecl passiveRequestUnload(void);

// Tell the agent that a host (the Fabric bridge) holds the LoadLibrary
// reference that keeps this module mapped. A host-managed agent tears itself
// down but does not call FreeLibraryAndExitThread - the host releases its own
// reference after passiveUnloadComplete says the teardown is over. Without
// this, a jar-loaded agent can never leave the process: the bridge's reference
// keeps the module resident even after the agent's self-free.
PASSIVE_AGENT_API void __cdecl passiveSetHostManaged(int managed);

// 1 once the teardown has finished and the module is safe to release. Only
// meaningful for a host-managed agent; the agent sets it just before its unload
// thread returns. A host must wait for this before calling FreeLibrary.
PASSIVE_AGENT_API int __cdecl passiveUnloadComplete(void);

// The ABI this DLL was built with. See PASSIVE_AGENT_ABI.
PASSIVE_AGENT_API int __cdecl passiveAgentAbi(void);

} // extern "C"
