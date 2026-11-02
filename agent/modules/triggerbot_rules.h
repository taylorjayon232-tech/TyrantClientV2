// ============================================================================
//  Passive — Triggerbot rules.
// ----------------------------------------------------------------------------
//  Pure maths and string tests, no JNI and no ImGui, so the harness
//  (test/triggerbot_check.cpp) exercises the exact rules the module runs. The
//  module only turns the answers into a swing at the game's own crosshair
//  target.
//
//  Ported from Argon's Trigger Bot: a sword and an axe each get their own
//  random delay window, "All Items" paces everything with the sword window,
//  "Check Shield" leaves a target whose raised shield faces the player alone,
//  and "Stray Bypass" widens the target test to the zombie family.
// ============================================================================
#pragma once

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>

namespace tyr {
namespace modules {
namespace trigger {

constexpr int   kMaxDelayMs = 5000;      // the card's slider ceiling
constexpr float kPi = 3.14159265358979f;

// What the held item is, as far as the card's pacing is concerned.
enum class Weapon { None, Sword, Axe };

// Item id -> weapon. The ids are the registry paths ("diamond_sword"), which
// are the same text in every language, unlike an item's display name. A
// pickaxe ends in "kaxe", not "_axe", so only real axes match.
inline Weapon weaponOf(const std::string& id) {
    const auto endsWith = [&id](const char* suffix) {
        const size_t n = std::strlen(suffix);
        return id.size() >= n && id.compare(id.size() - n, n, suffix) == 0;
    };
    if (endsWith("_sword")) return Weapon::Sword;
    if (endsWith("_axe")) return Weapon::Axe;
    return Weapon::None;
}

// Argon tests `instanceof ZombieEntity`; these are the registry types that are
// that class in 1.21. ("zombie_horse" is a horse, not a zombie, and is left out
// exactly as the Java check would leave it out.)
inline bool isZombieType(const std::string& type) {
    return type == "zombie" || type == "husk" || type == "drowned" || type == "zombie_villager";
}

// One delay from a weapon's window. Both ends are clamped and sorted, so a
// hand-written preset can never invert the window or stall the module; a pair
// that collapses still swings (on the low end) instead of never firing.
// `random` is the caller's raw random value (the module passes std::rand()).
inline int rollDelay(int minMs, int maxMs, unsigned random) {
    int lo = std::clamp(minMs, 0, kMaxDelayMs);
    int hi = std::clamp(maxMs, 0, kMaxDelayMs);
    if (hi < lo) std::swap(lo, hi);
    if (hi <= lo) return lo;
    return lo + (int)(random % (unsigned)(hi - lo + 1));
}

// Angle between the target's own look and the line back to the player, in
// degrees. dx/dz point from the target to the player. A raised shield sits in
// front of its owner, so a small angle means the shield is between the two and
// the swing should be held; a target standing on top of the player counts as
// shielded (angle 0).
inline float shieldAngle(float yawDeg, float dx, float dz) {
    const float len = std::sqrt(dx * dx + dz * dz);
    if (len < 1e-3f) return 0.f;
    const float yaw = yawDeg * kPi / 180.f;
    const float lx = -std::sin(yaw);        // Minecraft yaw 0 faces +Z, 90 faces -X
    const float lz = std::cos(yaw);
    const float c = (lx * dx + lz * dz) / len;
    return std::acos(std::clamp(c, -1.f, 1.f)) * 180.f / kPi;
}

} // namespace trigger
} // namespace modules
} // namespace tyr
