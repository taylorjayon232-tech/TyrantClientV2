// ============================================================================
//  Passive — a readable name out of a translatable text component.
//
//  Pure: no JNI and no ImGui, so test/text_name_check.cpp exercises exactly the
//  rule agent/jni/mc.cpp (textToString) applies when it reads an item's display
//  name. Every plain item name is a *translatable* component with no literal
//  content, so the game hands back the component's own dump
//
//      translation{key='item.minecraft.golden_apple', args=[]}
//
//  and that dump is what the HUD plaque used to print for a golden apple. The
//  key inside it is read out and made readable instead ("Golden Apple"), and the
//  caller only falls back to the raw item id when the dump holds no key at all.
// ============================================================================
#pragma once
#include <cctype>
#include <string>

namespace tyr {
namespace modules {
namespace names {

// "item.minecraft.golden_apple" -> "Golden Apple": the namespace and any registry prefix before
// the last '.' or ':' are dropped, underscores and slashes become spaces, and each word is
// capitalised. A key with no prefix at all is prettified as it stands.
inline std::string prettifyKey(const std::string& key) {
    const size_t dot = key.find_last_of(".:");
    const std::string name = dot == std::string::npos ? key : key.substr(dot + 1);
    std::string out;
    out.reserve(name.size());
    bool wordStart = true;
    for (const char c : name) {
        if (c == '_' || c == '/') { out.push_back(' '); wordStart = true; continue; }
        out.push_back(wordStart ? (char)std::toupper((unsigned char)c) : c);
        wordStart = false;
    }
    return out;
}

// The readable name out of a translatable component's own toString dump, or empty when the dump
// holds no translation key. The key is written between quotes (single or double) or bare; it ends
// at the first quote, comma, bracket, brace or space, so the dump's own `args=[...]` tail is never
// swallowed into the name.
inline std::string nameFromDump(const std::string& dump) {
    const size_t at = dump.find("key=");
    if (at == std::string::npos) return {};
    size_t i = at + 4;
    while (i < dump.size() && (dump[i] == '\'' || dump[i] == '"' || dump[i] == '[' || dump[i] == ' ')) ++i;
    std::string key;
    while (i < dump.size()) {
        const char c = dump[i];
        if (c == '\'' || c == '"' || c == ',' || c == ']' || c == '}' || c == ' ') break;
        key.push_back(c);
        ++i;
    }
    return key.empty() ? std::string{} : prettifyKey(key);
}

} // namespace names
} // namespace modules
} // namespace tyr
