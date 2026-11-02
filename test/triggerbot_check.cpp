// ============================================================================
//  Harness: the pure Triggerbot rules (agent/modules/triggerbot_rules.h).
// ----------------------------------------------------------------------------
//  Triggerbot turns these answers into a swing at the game's own crosshair
//  target, so the cases below are the ones the port has to get right: which
//  item counts as a sword or an axe, a random delay that always lands inside
//  the weapon's window (even when the two ends were written inverted), the
//  angle that decides whether a raised shield is in the way, and the zombie
//  family "Stray Bypass" widens the target test to.
//
//  Build + run:  test\build-triggerbot.bat   (prints "PASS" or the failing cases)
// ============================================================================
#include "../agent/modules/triggerbot_rules.h"

#include <cmath>
#include <cstdio>

namespace trigger = tyr::modules::trigger;

static int gChecks = 0;
static int gFails = 0;

static void check(bool ok, const char* what) {
    ++gChecks;
    if (!ok) { ++gFails; std::printf("FAIL  %s\n", what); }
}

static void checkNear(float got, float want, float tol, const char* what) {
    ++gChecks;
    if (!(std::fabs(got - want) <= tol)) {
        ++gFails;
        std::printf("FAIL  %s\n        got  %.4f\n        want %.4f\n", what, got, want);
    }
}

static void checkEqInt(long long got, long long want, const char* what) {
    ++gChecks;
    if (got != want) {
        ++gFails;
        std::printf("FAIL  %s\n        got  %lld\n        want %lld\n", what, got, want);
    }
}

int main() {
    // ---- what the held item is ---------------------------------------------
    check(trigger::weaponOf("diamond_sword") == trigger::Weapon::Sword, "weapons: a diamond sword is a sword");
    check(trigger::weaponOf("netherite_sword") == trigger::Weapon::Sword, "weapons: a netherite sword is a sword");
    check(trigger::weaponOf("wooden_sword") == trigger::Weapon::Sword, "weapons: a wooden sword is a sword");
    check(trigger::weaponOf("iron_axe") == trigger::Weapon::Axe, "weapons: an iron axe is an axe");
    check(trigger::weaponOf("netherite_axe") == trigger::Weapon::Axe, "weapons: a netherite axe is an axe");
    check(trigger::weaponOf("iron_pickaxe") == trigger::Weapon::None, "weapons: a pickaxe is not an axe");
    check(trigger::weaponOf("diamond_shovel") == trigger::Weapon::None, "weapons: a shovel is neither");
    check(trigger::weaponOf("mace") == trigger::Weapon::None, "weapons: the mace is neither");
    check(trigger::weaponOf("trident") == trigger::Weapon::None, "weapons: a trident is neither");
    check(trigger::weaponOf("shield") == trigger::Weapon::None, "weapons: a shield is neither");
    check(trigger::weaponOf("") == trigger::Weapon::None, "weapons: an empty id is neither");
    check(trigger::weaponOf("_axe") == trigger::Weapon::Axe, "weapons: a bare _axe id matches");
    check(trigger::weaponOf("axe") == trigger::Weapon::None, "weapons: an id without the underscore does not");

    // ---- the zombie family (Stray Bypass) -----------------------------------
    check(trigger::isZombieType("zombie"), "zombies: zombie");
    check(trigger::isZombieType("husk"), "zombies: husk");
    check(trigger::isZombieType("drowned"), "zombies: drowned");
    check(trigger::isZombieType("zombie_villager"), "zombies: zombie villager");
    check(!trigger::isZombieType("skeleton"), "zombies: a skeleton is not a zombie");
    check(!trigger::isZombieType("zombie_horse"), "zombies: a zombie horse is a horse, as in the Java check");
    check(!trigger::isZombieType("player"), "zombies: a player is not a zombie");
    check(!trigger::isZombieType(""), "zombies: an empty type is not a zombie");

    // ---- the delay roll ------------------------------------------------------
    checkEqInt(trigger::rollDelay(540, 550, 0), 540, "delay: the lowest roll is the window's low end");
    checkEqInt(trigger::rollDelay(540, 550, 10), 550, "delay: the highest roll is the window's high end");
    checkEqInt(trigger::rollDelay(540, 550, 11), 540, "delay: the modulo wraps back to the low end");
    checkEqInt(trigger::rollDelay(540, 550, 5), 545, "delay: a middle roll is inside the window");
    checkEqInt(trigger::rollDelay(600, 600, 7), 600, "delay: an equal pair never moves");
    checkEqInt(trigger::rollDelay(800, 780, 0), 780, "delay: an inverted window is sorted, low end first");
    checkEqInt(trigger::rollDelay(800, 780, 20), 800, "delay: an inverted window still reaches its high end");
    checkEqInt(trigger::rollDelay(-50, 50, 0), 0, "delay: a negative low end clamps to zero");
    checkEqInt(trigger::rollDelay(4000, 9000, 0), 4000, "delay: the low end is kept when the high clamps");
    checkEqInt(trigger::rollDelay(4000, 9000, 1000), 5000, "delay: a roll above the ceiling clamps to it");
    checkEqInt(trigger::rollDelay(0, 0, 123), 0, "delay: a zero window is zero");

    // Every roll has to land inside the window, whatever the random value.
    {
        bool allInside = true;
        for (unsigned r = 0; r < 4000; r += 7) {
            const int ms = trigger::rollDelay(540, 550, r * 2654435761u);
            if (ms < 540 || ms > 550) allInside = false;
        }
        check(allInside, "delay: a thousand rolls stay inside the sword window");
    }
    {
        bool allInside = true;
        for (unsigned r = 0; r < 4000; r += 11) {
            const int ms = trigger::rollDelay(780, 800, r * 40503u);
            if (ms < 780 || ms > 800) allInside = false;
        }
        check(allInside, "delay: a thousand rolls stay inside the axe window");
    }
    {
        const int top = trigger::rollDelay(540, 550, 0xFFFFFFFFu);
        check(top >= 540 && top <= 550, "delay: the largest random value still lands in the window");
    }

    // ---- the shield angle ----------------------------------------------------
    // Yaw 0 faces +Z. An attacker straight ahead is 0 degrees away; behind is 180.
    checkNear(trigger::shieldAngle(0.f, 0.f, 1.f), 0.f, 0.01f, "shield: a target facing us is 0 degrees");
    checkNear(trigger::shieldAngle(0.f, 0.f, -1.f), 180.f, 0.01f, "shield: a target facing away is 180 degrees");
    checkNear(trigger::shieldAngle(0.f, 1.f, 0.f), 90.f, 0.01f, "shield: a target facing sideways is 90 degrees");
    // Yaw 90 faces -X.
    checkNear(trigger::shieldAngle(90.f, -1.f, 0.f), 0.f, 0.01f, "shield: yaw 90 faces -X");
    checkNear(trigger::shieldAngle(90.f, 1.f, 0.f), 180.f, 0.01f, "shield: yaw 90 looks away from +X");
    // Yaw 180 faces -Z.
    checkNear(trigger::shieldAngle(180.f, 0.f, -1.f), 0.f, 0.01f, "shield: yaw 180 faces -Z");
    // Diagonals: facing +Z with the attacker 45 degrees off.
    checkNear(trigger::shieldAngle(0.f, 0.7071f, 0.7071f), 45.f, 0.05f, "shield: a 45 degree attacker reads 45");
    // Standing on top of the player counts as shielded, not as a free hit.
    checkNear(trigger::shieldAngle(123.f, 0.f, 0.f), 0.f, 0.01f, "shield: no horizontal offset counts as in front");
    // The module's threshold: under 90 is held, over 90 is hit.
    check(trigger::shieldAngle(0.f, 0.f, 1.f) < 90.f, "shield: a shield facing us is inside the 90 degree rule");
    check(!(trigger::shieldAngle(0.f, 0.f, -1.f) < 90.f), "shield: a shield facing away is outside it");
    check(trigger::shieldAngle(0.f, 0.8f, 0.6f) < 90.f, "shield: 53 degrees is still held");
    check(!(trigger::shieldAngle(0.f, 0.6f, -0.8f) < 90.f), "shield: 127 degrees is hit through");

    std::printf("%d checks, %d failed\n", gChecks, gFails);
    std::printf("%s\n", gFails == 0 ? "PASS" : "FAIL");
    return gFails == 0 ? 0 : 1;
}
