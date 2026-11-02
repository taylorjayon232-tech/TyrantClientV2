// ============================================================================
//  Passive — Injector settings storage
//  A flat key=value file so the settings survive a restart without pulling a
//  JSON library into the injector. Any unreadable or malformed value keeps the
//  default that is already in InjectorSettings.
// ============================================================================
#include "injector_settings.h"

#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::wstring g_iniPath;
InjectorSettings g_settings;

std::string trim(const std::string& value) {
    const size_t begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

bool asBool(const std::string& value, bool fallback) {
    const std::string v = trim(value);
    if (v == "1" || v == "true" || v == "yes" || v == "on") return true;
    if (v == "0" || v == "false" || v == "no" || v == "off") return false;
    return fallback;
}

int asInt(const std::string& value, int fallback) {
    const std::string v = trim(value);
    if (v.empty()) return fallback;
    char* end = nullptr;
    const long parsed = std::strtol(v.c_str(), &end, 10);
    if (end == v.c_str() || (end && *end != 0)) return fallback;
    return (int)parsed;
}

} // namespace

InjectorSettings& injectorSettings() { return g_settings; }

void loadInjectorSettings(const std::wstring& iniPath) {
    g_iniPath = iniPath;
    if (iniPath.empty()) return;
    std::ifstream file(iniPath.c_str(), std::ios::binary);
    if (!file) return;                       // first run: the defaults are the settings

    std::string line;
    while (std::getline(file, line)) {
        const std::string clean = trim(line);
        if (clean.empty() || clean[0] == '#' || clean[0] == ';' || clean[0] == '[') continue;
        const size_t eq = clean.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = trim(clean.substr(0, eq));
        const std::string value = clean.substr(eq + 1);

        if (key == "autoInject") g_settings.autoInject = asBool(value, g_settings.autoInject);
        else if (key == "autoInjectDelayMs") g_settings.autoInjectDelayMs = asInt(value, g_settings.autoInjectDelayMs);
        else if (key == "autoInjectWindowedOnly") g_settings.autoInjectWindowedOnly = asBool(value, g_settings.autoInjectWindowedOnly);
        else if (key == "autoInjectSkipUninjected") g_settings.autoInjectSkipUninjected = asBool(value, g_settings.autoInjectSkipUninjected);
        else if (key == "closeAfterInject") g_settings.closeAfterInject = asBool(value, g_settings.closeAfterInject);
        else if (key == "alwaysOnTop") g_settings.alwaysOnTop = asBool(value, g_settings.alwaysOnTop);
        else if (key == "rememberTab") g_settings.rememberTab = asBool(value, g_settings.rememberTab);
        else if (key == "lastTab") g_settings.lastTab = asInt(value, g_settings.lastTab);
        else if (key == "confirmBeforeInject") g_settings.confirmBeforeInject = asBool(value, g_settings.confirmBeforeInject);
        else if (key == "autoRefreshList") g_settings.autoRefreshList = asBool(value, g_settings.autoRefreshList);
        else if (key == "refreshIntervalMs") g_settings.refreshIntervalMs = asInt(value, g_settings.refreshIntervalMs);
        else if (key == "showFullTitles") g_settings.showFullTitles = asBool(value, g_settings.showFullTitles);
    }

    g_settings.autoInjectDelayMs = std::clamp(g_settings.autoInjectDelayMs, 250, 10000);
    g_settings.refreshIntervalMs = std::clamp(g_settings.refreshIntervalMs, 500, 10000);
    g_settings.lastTab = std::clamp(g_settings.lastTab, 0, 1);
}

void saveInjectorSettings() {
    if (g_iniPath.empty()) return;
    g_settings.autoInjectDelayMs = std::clamp(g_settings.autoInjectDelayMs, 250, 10000);
    g_settings.refreshIntervalMs = std::clamp(g_settings.refreshIntervalMs, 500, 10000);
    g_settings.lastTab = std::clamp(g_settings.lastTab, 0, 1);

    std::ostringstream out;
    out << "# Passive injector settings (written automatically)\n";
    out << "autoInject=" << (g_settings.autoInject ? 1 : 0) << "\n";
    out << "autoInjectDelayMs=" << g_settings.autoInjectDelayMs << "\n";
    out << "autoInjectWindowedOnly=" << (g_settings.autoInjectWindowedOnly ? 1 : 0) << "\n";
    out << "autoInjectSkipUninjected=" << (g_settings.autoInjectSkipUninjected ? 1 : 0) << "\n";
    out << "closeAfterInject=" << (g_settings.closeAfterInject ? 1 : 0) << "\n";
    out << "alwaysOnTop=" << (g_settings.alwaysOnTop ? 1 : 0) << "\n";
    out << "rememberTab=" << (g_settings.rememberTab ? 1 : 0) << "\n";
    out << "lastTab=" << g_settings.lastTab << "\n";
    out << "confirmBeforeInject=" << (g_settings.confirmBeforeInject ? 1 : 0) << "\n";
    out << "autoRefreshList=" << (g_settings.autoRefreshList ? 1 : 0) << "\n";
    out << "refreshIntervalMs=" << g_settings.refreshIntervalMs << "\n";
    out << "showFullTitles=" << (g_settings.showFullTitles ? 1 : 0) << "\n";

    // Write beside the target and swap it in, so a crash mid-write cannot leave
    // half a settings file behind.
    std::wstring temp = g_iniPath + L".tmp";
    {
        std::ofstream file(temp.c_str(), std::ios::binary | std::ios::trunc);
        if (!file) return;
        const std::string text = out.str();
        file.write(text.data(), (std::streamsize)text.size());
        if (!file) { file.close(); DeleteFileW(temp.c_str()); return; }
    }
    if (!MoveFileExW(temp.c_str(), g_iniPath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(temp.c_str());
    }
}
