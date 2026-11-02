#include "profiles.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <cctype>
#include <algorithm>
#include <iterator>

namespace tyr { namespace profiles {
namespace {
namespace fs = std::filesystem;

fs::path root() {
    wchar_t appData[MAX_PATH]{};
    DWORD n = GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH);
    if (!n || n >= MAX_PATH) {
        wchar_t temp[MAX_PATH]{};
        n = GetTempPathW(MAX_PATH, temp);
        return fs::path(temp, temp + (n && n < MAX_PATH ? n : 0)) / L"Passive" / L"configs";
    }
    return fs::path(appData) / L"Passive" / L"configs";
}

std::string safeName(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (unsigned char c : input) {
        if (std::isalnum(c) || c == '_' || c == '-') out.push_back((char)c);
        else if (c == ' ' && !out.empty() && out.back() != '-') out.push_back('-');
        if (out.size() >= 48) break;
    }
    while (!out.empty() && (out.back() == '-' || out.back() == '_')) out.pop_back();
    return out;
}
fs::path profilePath(const std::string& name) {
    return root() / (safeName(name) + ".json");
}
bool valid(const std::string& name) { return !safeName(name).empty(); }
}

std::vector<std::string> list() {
    std::vector<std::string> out;
    std::error_code ec;
    fs::create_directories(root(), ec);
    for (fs::directory_iterator it(root(), ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().extension() == L".json" && it->path().parent_path() == root())
            out.push_back(it->path().stem().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}
bool create(const std::string& name, const std::string& contents, std::string& error) {
    if (!valid(name)) { error = "Use letters, numbers, spaces, hyphens or underscores for a preset name."; return false; }
    std::error_code ec;
    fs::create_directories(root(), ec);
    const auto path = profilePath(name);
    if (fs::exists(path, ec)) { error = "A preset with that name already exists."; return false; }
    return write(name, contents, error);
}
bool write(const std::string& name, const std::string& contents, std::string& error) {
    if (!valid(name)) { error = "Invalid preset name."; return false; }
    std::error_code ec;
    fs::create_directories(root(), ec);
    std::ofstream file(profilePath(name), std::ios::binary | std::ios::trunc);
    if (!file) { error = "Could not write the preset file."; return false; }
    file.write(contents.data(), (std::streamsize)contents.size());
    if (!file) { error = "The preset could not be saved completely."; return false; }
    return true;
}
bool read(const std::string& name, std::string& contents, std::string& error) {
    if (!valid(name)) { error = "Invalid preset name."; return false; }
    std::ifstream file(profilePath(name), std::ios::binary);
    if (!file) { error = "Preset not found or could not be opened."; return false; }
    contents.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}
bool remove(const std::string& name, std::string& error) {
    if (!valid(name)) { error = "Invalid preset name."; return false; }
    std::error_code ec;
    if (!fs::remove(profilePath(name), ec) || ec) { error = "Preset could not be deleted."; return false; }
    return true;
}
bool exportCopy(const std::string& name, std::string& pathOut, std::string& error) {
    if (!valid(name)) { error = "Invalid preset name."; return false; }
    std::error_code ec;
    const fs::path dir = root() / L"exports";
    fs::create_directories(dir, ec);
    if (ec) { error = "Could not create the export folder."; return false; }
    const fs::path from = profilePath(name);
    const fs::path to = dir / (safeName(name) + ".json");
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    if (ec) { error = "Save the preset before exporting it."; return false; }
    pathOut = std::string("Passive/configs/exports/") + safeName(name) + ".json";
    return true;
}
} }
