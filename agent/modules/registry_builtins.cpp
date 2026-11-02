// ============================================================================
//  Passive — built-in module registration (single entry point).
// ============================================================================
#include "modules.h"

namespace tyr {
namespace modules {

// Defined at the bottom of each module translation unit:
void addBuiltins();               // combat.cpp
void registerPlayerModules();     // player.cpp
void registerMovementModules();   // movement.cpp
void registerMacroModules();      // macros.cpp
void registerRenderModules();     // render.cpp
void registerClientModules();     // client.cpp

void registerBuiltins() {
    addBuiltins();                // Combat: AutoMace, TriggerBot, ShieldBreaker, Velocity
    registerPlayerModules();      // Player: AutoTool, FastPlace
    registerMovementModules();    // Movement: Sprint
    registerMacroModules();       // Macros: execution-based macros
    registerRenderModules();      // Render: ESP, NameTags, Fullbright
    registerClientModules();      // Client: Watermark, ArrayList
}

} // namespace modules
} // namespace tyr
