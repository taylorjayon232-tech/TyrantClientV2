// ============================================================================
//  Passive — player detection rules (pure, no JNI).
// ----------------------------------------------------------------------------
//  Every player-facing module (AimAssist, Triggerbot, Auto Shield Break, AutoMace,
//  Ghost Hand, ESP, Nametags, Pearl Agro, the friends list) decides through the
//  detection service in modules.cpp, which feeds these rules from the live world.
//  Keeping the rules pure means they are unit-testable without a JVM
//  (test/detect_check.cpp) and cannot be "fixed" differently in each module.
//
//  Why the rules look like this: server plugins rewrite everything a plain "is this
//  a PlayerEntity" test would rely on. Display names carry rank prefixes, colour
//  codes and NPC markers, and a plugin NPC is a real PlayerEntity with a perfectly
//  normal-looking name — so a name test either misses the NPC, or (much worse)
//  marks a real player as a bot and the modules silently stop finding anyone. A
//  UUID cannot be decorated, so identity is keyed on it and the name is decoration.
// ============================================================================
#pragma once

#include <cctype>
#include <string>

namespace tyr { namespace modules { namespace detect {

inline std::string lowerCopy(const std::string& value) {
    std::string out = value;
    for (char& c : out) c = (char)std::tolower((unsigned char)c);
    return out;
}

// The character after a '§' / '&' formatting escape (0-9 a-f k-o r x).
inline bool isFormatCodeChar(char c) {
    const char l = (char)std::tolower((unsigned char)c);
    return (l >= '0' && l <= '9') || (l >= 'a' && l <= 'f') ||
           l == 'k' || l == 'l' || l == 'm' || l == 'n' || l == 'o' || l == 'r' || l == 'x';
}

// Removes what plugins paint names with: §/& colour and style codes, the UTF-8
// form of § (C2 A7), and the zero-width padding some NPC plugins hide in a name.
inline std::string stripNameDecorations(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        const unsigned char c = (unsigned char)raw[i];
        if (c == 0xC2 && i + 1 < raw.size() && (unsigned char)raw[i + 1] == 0xA7) {
            ++i;                                                        // consume the § itself ...
            if (i + 1 < raw.size() && isFormatCodeChar(raw[i + 1])) ++i; // ... and the code letter after it
            continue;
        }
        if ((c == '&' || c == 0xA7) && i + 1 < raw.size() && isFormatCodeChar(raw[i + 1])) { ++i; continue; }
        if (c == 0xE2 && i + 2 < raw.size() && (unsigned char)raw[i + 1] == 0x80) {
            const unsigned char z = (unsigned char)raw[i + 2];
            if (z == 0x8B || z == 0x8C || z == 0x8D) { i += 2; continue; }   // ZWSP / ZWNJ / ZWJ
        }
        out.push_back(raw[i]);
    }
    return out;
}

// The name a module should compare and display: formatting stripped, leading
// [rank] / (team) / <tag> blocks dropped, trailing ':' / '>' / '|' dropped and runs
// of spaces collapsed. "§c[VIP] Notch §7| §fShop" becomes "Notch | Shop" — that is
// what makes a friend or an NPC recognisable however the server dresses them up.
inline std::string cleanPlayerName(const std::string& raw) {
    std::string s = stripNameDecorations(raw);
    for (;;) {
        size_t begin = 0;
        while (begin < s.size() && (s[begin] == ' ' || s[begin] == '\t')) ++begin;
        if (begin) s.erase(0, begin);
        if (s.empty()) break;
        const char open = s[0];
        const char close = open == '[' ? ']' : (open == '(' ? ')' : (open == '<' ? '>' : (open == '{' ? '}' : '\0')));
        if (!close) break;
        const size_t end = s.find(close);
        if (end == std::string::npos || end > 24) break;      // not a tag: part of the name
        s.erase(0, end + 1);
    }
    while (!s.empty()) {
        const char c = s.back();
        if (c == ' ' || c == '\t' || c == ':' || c == '>' || c == '|' || c == '-' || c == '.') s.pop_back();
        else break;
    }
    std::string out;
    out.reserve(s.size());
    bool pendingSpace = false;
    for (char c : s) {
        if (c == ' ' || c == '\t') { pendingSpace = true; continue; }
        if (pendingSpace && !out.empty()) out.push_back(' ');
        pendingSpace = false;
        out.push_back(c);
    }
    return out;
}

// Entity.getUuidAsString() shape: 32 hex digits, with or without the dashes.
inline bool looksLikeUuid(const std::string& value) {
    size_t hex = 0;
    for (char c : value) {
        if (c == '-') continue;
        if (!std::isxdigit((unsigned char)c)) return false;
        ++hex;
    }
    return hex == 32;
}

// Comparison form: lower case, no dashes.
inline std::string uuidKey(const std::string& value) {
    std::string out;
    out.reserve(32);
    for (char c : value) {
        if (c == '-' || std::isspace((unsigned char)c)) continue;
        out.push_back((char)std::tolower((unsigned char)c));
    }
    return out;
}

// '1'..'5' for a canonical UUID, 0 when the text is not one. 4 = a real online
// account, 3 = a real offline-mode account, 1/2/5 = generated by a plugin or mod.
inline char uuidVersion(const std::string& value) {
    const std::string key = uuidKey(value);
    if (key.size() != 32) return 0;
    for (char c : key)
        if (!std::isxdigit((unsigned char)c)) return 0;
    return key[12];
}

// Whole-token marker match on an already lower-cased name. '_' and every other
// non-alphanumeric splits a token, so "NPC_Shop", "[npc] Guard" and "Shopkeeper
// (NPC)" match, while a real player called "npcguy", "robot" or "Guardian" does not.
inline bool nameHasToken(const std::string& lowerName, const char* token) {
    size_t len = 0;
    while (token[len]) ++len;
    if (len == 0 || lowerName.size() < len) return false;
    for (size_t at = lowerName.find(token); at != std::string::npos; at = lowerName.find(token, at + 1)) {
        const bool leftOk = at == 0 || !std::isalnum((unsigned char)lowerName[at - 1]);
        const size_t after = at + len;
        const bool rightOk = after >= lowerName.size() || !std::isalnum((unsigned char)lowerName[after]);
        if (leftOk && rightOk) return true;
    }
    return false;
}

// The NPC / bot words plugins put in a display name. Deliberately short, and every
// entry matches as a whole token only, so ordinary names are never caught.
inline bool isBotNameMarker(const std::string& lowerName) {
    static const char* const kMarkers[] = {
        "bot", "npc", "citizens", "shop", "shopkeeper", "merchant",
        "trader", "guard", "banker", "auction"
    };
    for (const char* marker : kMarkers)
        if (nameHasToken(lowerName, marker)) return true;
    return false;
}

// A real account's UUID: version 4 online, or version 3 from an offline-mode
// server. Version 3 is still a real player — calling it a bot used to switch every
// entity module off on offline / cracked servers, the loudest way this can go wrong.
inline bool isAccountUuid(char version, bool allowOffline) {
    if (version == '4') return true;
    if (version == 0) return false;
    return allowOffline && version == '3';
}

// "Detect by UUID": the UUID is one no real account is given.
inline bool uuidIsBotShape(char version, bool allowOffline) {
    if (version == 0) return false;              // unreadable UUID: never a verdict on its own
    return !isAccountUuid(version, allowOffline);
}

// A second entity wearing a name another UUID already owns is the plugin's copy of
// it — the shape NPC plugins take when they dress an NPC up as a real player. The
// copy is whichever entity is not account-like, so a real player is never dropped
// because an NPC happened to spawn with their name first.
inline bool isBotClone(const std::string& cleanLowerName,
                       const std::string& thisUuidKey, char thisVersion,
                       const std::string& ownerUuidKey, bool allowOffline,
                       const std::string& selfNameLower, const std::string& selfUuidKey) {
    if (cleanLowerName.empty()) return false;
    // Somebody wearing the local player's name is always an NPC: one server cannot
    // hold two accounts with the same name.
    if (!selfNameLower.empty() && cleanLowerName == selfNameLower &&
        !selfUuidKey.empty() && thisUuidKey != selfUuidKey) return true;
    if (ownerUuidKey.empty() || ownerUuidKey == thisUuidKey) return false;
    return !isAccountUuid(thisVersion, allowOffline);
}

}}}
