// ============================================================================
//  Passive — Injector settings
//  The settings tab's state and its on-disk form (passive_injector.ini next to
//  the exe). Everything here is plain data so injector.cpp can draw it with
//  stock ImGui widgets and the auto-injection thread can read it.
// ============================================================================
#pragma once

#include <string>

struct InjectorSettings {
    // Auto-injection: load Passive into every running Minecraft instance that does not have it.
    bool autoInject = false;
    int  autoInjectDelayMs = 1500;      // 250-10000 ms between passes
    bool autoInjectWindowedOnly = true; // only instances that own a visible window
    bool autoInjectSkipUninjected = true; // never auto-inject an instance the user uninjected

    bool closeAfterInject = false;      // close the injector once an injection succeeds
    bool alwaysOnTop = true;            // keep the injector above other windows
    bool rememberTab = true;            // reopen on the tab that was last used
    int  lastTab = 0;                   // 0 = Inject, 1 = Multi Instance
    bool confirmBeforeInject = false;   // reserved for the main button's confirmation
    bool autoRefreshList = true;        // rescan the instance list on a timer
    int  refreshIntervalMs = 2000;      // 500-10000 ms
    bool showFullTitles = true;         // full window title vs. an ellipsized one
};

// The one settings object the injector and its auto-injection thread share.
InjectorSettings& injectorSettings();

// Load at startup (missing or corrupt file keeps the defaults) and write the
// current values next to the exe. save() writes to the path load() was given.
void loadInjectorSettings(const std::wstring& iniPath);
void saveInjectorSettings();

// Auto-injection state, implemented in injector.cpp so the settings view can
// report it and the user can reset it.
int  injectorAutoSkipCount();   // instances the user uninjected (never auto again)
int  injectorAutoSeenCount();   // instances auto-injection knows Passive has been in
void injectorClearAutoSkips();  // forget them all
