// ============================================================================
//  Passive — Module base class + settings model.
//
//  Every module gets a keybind. Mode "toggle" flips enabled on tap (default);
//  mode "trigger" just passes taps to the module body (Lunge Macro style) via
//  consumeTrigger(). ESC while capturing clears the bind.
// ============================================================================
#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include <memory>
#include <json.hpp>   // project-vendored nlohmann json

namespace tyr {

enum class SettingType { Bool, Float, Mode, Color, Key };

struct Setting {
    std::string  name;
    SettingType  type;
    bool         boolVal = false;
    float        floatVal = 0.f;
    float        min = 0.f, max = 1.f;
    ImVec4      color{ 1.f, 1.f, 1.f, 1.f };
    std::vector<std::string> modes;
    int          modeIdx = 0;
    int          key = 0;        // VK code, 0 = none

    Setting() = default;
    Setting(const char* n, bool v)
        : name(n), type(SettingType::Bool), boolVal(v) {}
    Setting(const char* n, float v, float mn, float mx)
        : name(n), type(SettingType::Float), floatVal(v), min(mn), max(mx) {}
    Setting(const char* n, std::initializer_list<const char*> ms, int idx = 0)
        : name(n), type(SettingType::Mode), modes(ms.begin(), ms.end()), modeIdx(idx) {}
    Setting(const char* n, ImVec4 c)
        : name(n), type(SettingType::Color), color(c) {}
};

class Module {
public:
    Module(const char* name, const char* category, const char* desc)
        : name_(name), category_(category), desc_(desc) {}
    virtual ~Module() = default;

    const std::string& name() const     { return name_; }
    const std::string& category() const { return category_; }
    const std::string& desc() const     { return desc_; }
    bool enabled() const                { return enabled_; }
    int  keybind() const                { return keybind_; }
    int  keybindMode() const            { return keybindMode_; }   // 0=toggle 1=trigger 2=togglebind
    void setKeybind(int vk)             { keybind_ = vk; }
    void setKeybindMode(int m)          { keybindMode_ = (m >= 0 && m <= 2) ? m : 0; }
    void toggle()                       { setEnabled(!enabled_); }
    std::vector<Setting>& settings()    { return settings_; }

    virtual void setEnabled(bool on) {
        if (enabled_ == on) return;
        enabled_ = on;
        if (on) onEnable(); else onDisable();
    }

    // Called every frame (render thread) with the real frame delta in seconds.
    // Toggle-bind (mode 2): a disabled module with a bind tap still gets one
    // update to run its macro body while it "arms" itself (LungeMacro pattern).
    virtual void update(float dt) { (void)dt; }

    // Trigger-mode keybinds queue taps here; the module body consumes them.
    bool consumeTrigger() {
        if (pendingTriggers_ <= 0) return false;
        --pendingTriggers_;
        return true;
    }
    bool hasTrigger() const { return pendingTriggers_ > 0; }
    void pushTrigger()      { if (pendingTriggers_ < 4) ++pendingTriggers_; }
    void clearTriggers()    { pendingTriggers_ = 0; }

    // Config persistence hooks for module-specific state.
    virtual void writeExtras(nlohmann::json&) const {}
    virtual void readExtras(const nlohmann::json&) {}

    // Targeting/debug diagnostics: a one-line "what am I seeing and why did I
    // (not) act" status. Drawn by the module itself when its Debug HUD setting
    // is on — makes misfires diagnosable in-game instead of via logs.
    void                setDebugLine(std::string s) { debugLine_ = std::move(s); }
    const std::string&  debugLine() const           { return debugLine_; }

protected:
    virtual void onEnable()  {}
    virtual void onDisable() {}

    void addSetting(const Setting& s) { settings_.push_back(s); }
    bool  boolVal(int idx) const  { return settings_[idx].boolVal; }
    float floatVal(int idx) const { return settings_[idx].floatVal; }
    int   modeIdx(int idx) const  { return settings_[idx].modeIdx; }
    const std::string& modeVal(int idx) const { return settings_[idx].modes[settings_[idx].modeIdx]; }

    std::string name_;
    std::string category_;
    std::string desc_;
    bool enabled_ = false;
    int  keybind_ = 0;             // VK code
    int  keybindMode_ = 0;         // 0 = toggle, 1 = trigger (macro), 2 = toggle-bind
    int  pendingTriggers_ = 0;
    std::string debugLine_;        // targeting debug HUD (empty = draw nothing)
    std::vector<Setting> settings_;
};

} // namespace tyr
