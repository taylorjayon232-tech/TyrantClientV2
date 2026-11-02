// ============================================================================
//  Passive — the Auto Pot slot rule.
//
//  Pure: no JNI and no ImGui, so test/pot_slots_check.cpp exercises exactly the
//  rule agent/modules/modules.cpp uses to pick the hotbar slot a potion is filled
//  into and thrown from under each Pot Slot Type.
//
//  The reported problem was that "Pot Refill After Use" could not be pointed at
//  slots: every potion was grabbed into the first checked slot, so a second
//  checked slot was never restocked and a Custom layout did not exist at all.
//  The destination is therefore a rule of its own now, and it lives here.
// ============================================================================
#pragma once

#include <cmath>

namespace tyr {
namespace modules {
namespace potslots {

// Potion Slot Type. Mirrors modules::PotSlotType in modules.h, kept local so this
// header stays freestanding.
enum class Type { EachDifferent, SinglePot, Custom };

// Picks the destination slot for one potion.
//  allowed — the hotbar slots this potion may use (the checked Pot Slots, or its
//            own Custom list). false means the slot is not this potion's to use.
//  empty   — the slot holds nothing.
//  needed  — the slot holds a potion that is still on the want list, so replacing
//            it would throw away a potion the player is about to need.
//
//  EachDifferent fills an empty allowed slot first, which is what keeps one
//  different potion in each checked slot; a slot holding a potion nothing needs
//  is the fallback, and a slot holding a needed potion is only replaced when
//  every allowed slot already holds one.
//  SinglePot keeps a single potion between the checked slots, so it always
//  returns the first allowed slot: the other checked slots stay empty.
//  Custom changes nothing but the allowed list; it picks like EachDifferent
//  inside the potion's own slots.
//  Returns 0 when no slot is allowed.
inline int targetSlot(const bool (&allowed)[9], const bool (&empty)[9],
                      const bool (&needed)[9], Type type) {
    const bool singlePot = type == Type::SinglePot;
    int first = -1, emptySlot = -1, spare = -1;
    for (int s = 0; s < 9; ++s) {
        if (!allowed[s]) continue;
        if (first < 0) first = s;
        if (singlePot) continue;                    // one potion: the first slot is it
        if (empty[s]) { if (emptySlot < 0) emptySlot = s; continue; }
        if (!needed[s] && spare < 0) spare = s;
    }
    if (first < 0) return 0;
    if (singlePot) return first;
    if (emptySlot >= 0) return emptySlot;
    if (spare >= 0) return spare;
    return first;
}

// The slot a refill may put one potion into, given what every hotbar slot holds right now.
//
//  claimed - a slot an earlier move in this same plan already fills (or already holds the right
//            potion), so it is spoken for.
//  empty   - the slot holds nothing.
//  wanted  - the slot holds a potion one of the outstanding wants still needs, so taking it would
//            only push a potion the module is about to throw back into the inventory.
//
//  An empty slot is preferred and a slot holding nothing wanted is the fallback; a slot holding a
//  wanted potion is never used. Returns -1 when this plan has no room left.
//
//  This is the rule that stops two missing buffs trading places in one slot: with a single checked
//  Pot Slot the first want claims it, and the second want is told -1 until the first potion has been
//  thrown and its effect is live.
inline int refillSlot(const bool (&allowed)[9], const bool (&empty)[9],
                      const bool (&wanted)[9], const bool (&claimed)[9]) {
    int fallback = -1;
    for (int s = 0; s < 9; ++s) {
        if (!allowed[s] || claimed[s]) continue;
        if (empty[s]) return s;
        if (!wanted[s] && fallback < 0) fallback = s;
    }
    return fallback;
}

// Looking far enough down to throw: 65 degrees or steeper. The throw used to demand 80, so a view
// that was clearly down still never threw - every degree below 80 refused the potion.
inline bool pitchReady(float pitch, float threshold = 65.f) { return pitch >= threshold; }

// 180 Pot: the yaw a throw turns to, `degrees` horizontally from the view it started on, kept in
// (-180, 180] so walking towards it takes the short way around instead of the long one.
inline float turnTarget(float yaw, float degrees) {
    float target = std::fmod(yaw + degrees + 180.f, 360.f);
    if (target < 0.f) target += 360.f;
    target -= 180.f;
    if (target <= -180.f) target += 360.f;   // an exact half turn reports 180, not -180
    return target;
}

} // namespace potslots
} // namespace modules
} // namespace tyr
