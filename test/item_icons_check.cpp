// ============================================================================
//  Harness: the potion-icon rules (agent/render/itemicons.h). The GPU half of
//  itemicons needs the game's JVM and GL context, but the two decisions that
//  actually go wrong in-game - which bottle a stack uses, and what colour its
//  liquid is tinted - are pure, so they are pinned here.
//
//  Build + run:  test\build-item-icons.bat   (prints "PASS" or the failing cases)
// ============================================================================
#include "../agent/render/itemicons.h"

#include <cstdio>
#include <string>

using tyr::itemicons::potionBaseId;
using tyr::itemicons::potionTint;

static int g_checks = 0;
static int g_failed = 0;

static void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        ++g_failed;
        std::printf("FAIL  %s\n", what);
    }
}

int main() {
    // ---- which bottle a stack draws ------------------------------------------------------
    check(potionBaseId("potion") == "potion", "a drinking potion uses the potion bottle");
    check(potionBaseId("minecraft:splash_potion") == "splash_potion",
          "a splash potion uses the splash bottle (the reported strength-potion icon)");
    check(potionBaseId("lingering_potion") == "lingering_potion",
          "a lingering potion uses the lingering bottle");
    check(potionBaseId("minecraft:potion") == "potion", "the namespace is stripped");
    check(potionBaseId("golden_apple").empty(), "a non-potion item has no potion bottle");
    check(potionBaseId("").empty(), "an empty id has no potion bottle");

    // ---- the vanilla potion colours ------------------------------------------------------
    check(potionTint("strength") == 0xFF932423u, "strength is its own colour");
    check(potionTint("speed") == 0xFF7CAFC6u, "speed is its own colour");
    check(potionTint("fire_resistance") == 0xFFE49A3Au, "fire resistance is its own colour");
    check(potionTint("regeneration") == 0xFFCD5CABu, "regeneration is its own colour");
    check(potionTint("slow_falling") == 0xFFFFEFD1u, "slow falling is its own colour");
    check(potionTint("wind_charged") == 0xFFBDC9FFu, "wind charged is its own colour");
    check(potionTint("weaving") == 0xFF786297u, "weaving is its own colour");
    check(potionTint("turtle_master") == 0xFF7BA4A2u, "turtle master is its own colour");
    check(potionTint("minecraft:strength") == 0xFF932423u, "a namespaced potion id still tints");

    // ---- the long_/strong_ variants are the same colour as their base potion ------------
    check(potionTint("long_strength") == potionTint("strength"),
          "long_ is not a colour of its own");
    check(potionTint("strong_strength") == potionTint("strength"),
          "strong_ is not a colour of its own");
    check(potionTint("minecraft:long_speed") == potionTint("speed"),
          "both the namespace and the prefix are stripped");
    check(potionTint("strong_healing") == 0xFFF82423u, "a prefixed variant still finds its colour");

    // ---- a custom colour (potion_contents) outranks the registry colour ------------------
    check(potionTint("strength", 0xFF00FF00u) == 0xFF00FF00u,
          "a potion_contents custom colour wins");
    check(potionTint("strength", 0x00000000u) == 0xFF932423u,
          "an unset custom colour (alpha 0) does not blank the potion");
    check(potionTint("strength", 0xFF000000u) == 0xFF000000u,
          "an opaque black custom colour is honoured, not treated as unset");

    // ---- a potion this build cannot name still gets a liquid ----------------------------
    check((potionTint("some_future_potion") >> 24) != 0,
          "an unknown potion keeps an opaque tint instead of vanishing");
    check(potionTint("some_future_potion") == potionTint(""),
          "an unknown potion and no potion at all share the neutral colour");

    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    std::printf("%s\n", g_failed == 0 ? "PASS" : "FAILED");
    return g_failed == 0 ? 0 : 1;
}
