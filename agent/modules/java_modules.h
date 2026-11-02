// ============================================================================
//  Passive — Java module host.
//
//  The interface, keybinds, packet hooks and per-frame JNI scope stay native.
//  Module decisions live in com/passive/fabric/modules, loaded through the
//  game's own class loader on the first frame. This header is the native half:
//  module descriptors for the C++ cards, the per-frame tick, the settings
//  mirrors the config panel edits, and the lifecycle calls.
//
//  When the Java classes are not present (the standalone injector has no Fabric
//  mod to carry them), init() answers false and every C++ module keeps running
//  untouched. A ported module is only removed from the C++ list when its Java
//  replacement really registered.
// ============================================================================
#pragma once

#include "modules.h"
#include <json.hpp>
#include <string>
#include <vector>

namespace tyr { namespace modules { namespace java {

enum class SettingType { Bool, Int, Float, Mode };

// One setting exactly as the Java descriptor described it. The value here is the interface's
// source of truth; edits are mirrored back into the Java Setting object.
struct Setting {
    SettingType type = SettingType::Bool;
    std::string key;
    std::string label;
    bool  boolValue = false;
    int   intValue = 0, minInt = 0, maxInt = 0;
    float floatValue = 0.f, minFloat = 0.f, maxFloat = 0.f;
    std::vector<std::string> options;
    int   modeIndex = 0;
};

// One registered Java module. The Entry appended to g_entries points at `name`, `description`,
// `enabled` and `keybind`, so this object must outlive the entry list (it does: g_modules is
// filled once, before any entry is created, and never reallocated again).
struct Module {
    std::string name;
    std::string description;
    std::string categoryName;
    std::string tagName;
    Category category = Category::Misc;
    Tag tag = Tag::Safe;
    bool enabled = false;
    bool lastEnabled = false;   // last state pushed to Java
    int keybind = 0;
    std::vector<Setting> settings;
};

// Loads the Java host, registers NativeApi and appends the module entries. Safe to call every
// frame: it does nothing after the first success, and nothing at all when the classes are absent.
bool init();

// Pushes enable-state changes, then runs the Java modules for this frame.
void tick(float dt);

// Disables every Java module (releasing held keys) without unloading the host.
void disableAll();

// Says goodbye to the Java host. Called before JNI detach on uninject; entries stay valid.
void shutdown();

bool ready();
bool owns(const std::string& name);
Module* find(const std::string& name);
const std::vector<Module>& modules();

// Interface edits: update the mirror and the Java Setting in one call.
void setBool(Module& module, Setting& setting, bool value);
void setInt(Module& module, Setting& setting, int value);
void setFloat(Module& module, Setting& setting, float value);
void setMode(Module& module, Setting& setting, int value);

// Expanded card body height for a Java module (0 when `name` is not one), so the interface
// reserves exactly the space the generic settings drawer fills.
float cardHeight(const std::string& name, bool expanded);

// Config persistence for the Java settings (stored beside the C++ module settings).
nlohmann::json settingsJson();
void loadSettings(const nlohmann::json& json);

} } }
