// ============================================================================
//  Harness: the Auto Pot slot rule (agent/modules/pot_slot_rules.h) - where a
//  potion is filled into and thrown from under each Pot Slot Type.
//
//  The bug this pins down: every potion was grabbed into the first checked slot,
//  so a second checked slot was never restocked and picking per-potion slots was
//  impossible. Each Slot Holds 1 Different has to leave the potions in different
//  slots, 1 Pot Only has to keep exactly one, and Custom has to obey the
//  potion's own slot list.
//
//  Build + run:  test\build-pot-slots.bat   (prints "PASS" or the failing cases)
// ============================================================================
#include "../agent/modules/pot_slot_rules.h"

#include <cmath>
#include <cstdio>

using namespace tyr::modules::potslots;

static int g_checks = 0;
static int g_failed = 0;

static void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        ++g_failed;
        std::printf("FAIL  %s\n", what);
    }
}

// A hotbar state: which slots this potion may use, which are empty, and which hold
// a potion something still needs.
struct Table {
    bool allowed[9] = {};
    bool empty[9] = {};
    bool needed[9] = {};
};

// Checked Pot Slots 1, 2 and 3 (indexes 0, 1, 2), the rest unchecked.
static Table threeChecked() {
    Table t;
    t.allowed[0] = t.allowed[1] = t.allowed[2] = true;
    for (int s = 0; s < 9; ++s) t.empty[s] = true;
    return t;
}

int main() {
    // ---- Each Slot Holds 1 Different: potions keep their own slots ----
    {
        Table t = threeChecked();
        t.empty[0] = false; t.needed[0] = true;          // slot 1 already holds strength
        check(targetSlot(t.allowed, t.empty, t.needed, Type::EachDifferent) == 1,
              "a second potion goes into the next empty checked slot, not over the first");
    }
    {
        Table t = threeChecked();
        // slot 1 holds a potion that is still wanted, slot 2 is empty: the empty slot wins.
        t.empty[0] = false; t.needed[0] = true;
        check(targetSlot(t.allowed, t.empty, t.needed, Type::EachDifferent) == 1,
              "an empty checked slot is preferred over one holding a wanted potion");
    }
    {
        Table t = threeChecked();
        // every checked slot is full: slot 1 holds a potion nothing needs (spare), slot 2 a
        // potion that is still wanted. The spare is replaced, the wanted one is not.
        t.empty[0] = t.empty[1] = t.empty[2] = false;
        t.needed[1] = t.needed[2] = true;
        check(targetSlot(t.allowed, t.empty, t.needed, Type::EachDifferent) == 0,
              "a slot holding an unneeded potion is replaced before a wanted one");
    }
    {
        Table t = threeChecked();
        t.empty[0] = t.empty[1] = t.empty[2] = false;    // all full with wanted potions
        t.needed[0] = t.needed[1] = t.needed[2] = true;
        check(targetSlot(t.allowed, t.empty, t.needed, Type::EachDifferent) == 0,
              "when every checked slot holds a wanted potion the first is restocked last");
    }
    {
        Table t = threeChecked();
        t.allowed[0] = true; t.allowed[1] = t.allowed[2] = false;
        t.empty[0] = false; t.needed[0] = true;          // the only unchecked slot is not usable
        t.allowed[5] = true; t.empty[5] = true;
        check(targetSlot(t.allowed, t.empty, t.needed, Type::EachDifferent) == 5,
              "an unchecked slot is never a target, even while a checked one is full");
    }

    // ---- 1 Pot Only: a single potion between the checked slots ----
    {
        Table t = threeChecked();
        t.empty[0] = false; t.needed[0] = true;          // slot 1 holds the potion already
        check(targetSlot(t.allowed, t.empty, t.needed, Type::SinglePot) == 0,
              "1 Pot Only reuses the first checked slot and leaves the others empty");
    }
    {
        Table t = threeChecked();
        t.empty[0] = false; t.empty[1] = false; t.empty[2] = false;
        t.needed[0] = t.needed[1] = t.needed[2] = true;
        check(targetSlot(t.allowed, t.empty, t.needed, Type::SinglePot) == 0,
              "1 Pot Only never spills into a second checked slot");
    }

    // ---- Custom: the potion's own slot list decides ----
    {
        Table t;                                          // nothing checked in Pot Slots
        t.allowed[6] = t.allowed[8] = true;               // only slots 7 and 9 are this potion's
        for (int s = 0; s < 9; ++s) t.empty[s] = true;
        check(targetSlot(t.allowed, t.empty, t.needed, Type::Custom) == 6,
              "Custom fills the first slot the potion itself was given");
        t.empty[6] = false; t.needed[6] = true;
        check(targetSlot(t.allowed, t.empty, t.needed, Type::Custom) == 8,
              "Custom moves to the potion's next own slot");
    }
    {
        Table t;                                          // no slot at all for this potion
        for (int s = 0; s < 9; ++s) t.empty[s] = true;
        check(targetSlot(t.allowed, t.empty, t.needed, Type::Custom) == 0,
              "a potion with no Custom slot selected has nowhere to go (caller reports it)");
    }

    // ---- the same rule drives the throw: a potion is only ever thrown from a slot
    //      its type allows, so the destination is also the throw source ----
    {
        Table t = threeChecked();
        t.empty[1] = false; t.needed[1] = true;           // strength in slot 2, slot 1 empty
        const int dest = targetSlot(t.allowed, t.empty, t.needed, Type::EachDifferent);
        check(dest == 0 && t.allowed[dest],
              "the chosen destination is always one of the allowed (checked) slots");
    }

    // ---- the refill plan: one slot per potion, and nothing planned twice ----
    // The reported bug: with a single checked Pot Slot the plan filled it for every missing buff at
    // once, so the module swapped strength in, then speed in, then strength back, and never threw
    // anything. A slot claimed by one want must not be planned into again until that potion is
    // thrown and its effect is live.
    {
        Table t;                                         // one checked slot only
        t.allowed[0] = true;
        t.empty[0] = false; t.needed[0] = true;          // strength already sits there
        bool claimed[9] = {};
        claimed[0] = true;                               // the first want took it
        const bool allowed[9] = {true, false, false, false, false, false, false, false, false};
        check(refillSlot(allowed, t.empty, t.needed, claimed) == -1,
              "a second missing buff is never swapped into the one slot a wanted potion already holds");
    }
    {
        // Two checked slots, both empty: the two potions go into different slots, not both into
        // the first one (the plan has to remember what it already filled).
        const bool allowed[9] = {true, true, false, false, false, false, false, false, false};
        bool empty[9] = {}, needed[9] = {}, claimed[9] = {};
        empty[0] = empty[1] = true;
        const int first = refillSlot(allowed, empty, needed, claimed);
        check(first == 0, "the first potion of a plan fills the first empty checked slot");
        claimed[first] = true; empty[first] = false; needed[first] = true;
        const int second = refillSlot(allowed, empty, needed, claimed);
        check(second == 1, "the second potion of the same plan fills the next empty checked slot");
        claimed[second] = true; empty[second] = false; needed[second] = true;
        check(refillSlot(allowed, empty, needed, claimed) == -1,
              "a third potion waits in the inventory instead of displacing one of them");
    }
    {
        // An empty slot always wins over one holding a potion nothing wants.
        const bool allowed[9] = {true, true, false, false, false, false, false, false, false};
        bool empty[9] = {}, needed[9] = {}, claimed[9] = {};
        empty[1] = true;                                 // slot 2 empty, slot 1 holds a spare
        check(refillSlot(allowed, empty, needed, claimed) == 1,
              "an empty checked slot is filled before a spare potion is replaced");
    }
    {
        // A slot holding a potion that is still wanted is never used, even when it is the only one.
        const bool allowed[9] = {true, false, false, false, false, false, false, false, false};
        bool empty[9] = {}, needed[9] = {}, claimed[9] = {};
        needed[0] = true;
        check(refillSlot(allowed, empty, needed, claimed) == -1,
              "a slot holding a wanted potion is left alone");
    }
    {
        // A slot outside the allowed list is never a destination, whatever it holds.
        const bool allowed[9] = {false, false, false, false, false, true, false, false, false};
        bool empty[9] = {}, needed[9] = {}, claimed[9] = {};
        empty[4] = empty[5] = empty[6] = true;
        check(refillSlot(allowed, empty, needed, claimed) == 5,
              "only a slot this potion may use is ever a refill destination");
    }

    // ---- the throw gate: 65 degrees or steeper counts as looking down ----
    // The throw used to demand 80, so a view that was plainly down refused the potion.
    check(!pitchReady(64.9f), "64.9 degrees is not far enough down to throw");
    check(pitchReady(65.f), "65 degrees is the throw gate");
    check(pitchReady(89.f), "near-straight down throws");
    check(!pitchReady(-65.f), "looking up never counts as down");

    // ---- 180 Pot: the yaw the turn walks to, kept in (-180, 180] ----
    auto yawDelta = [](float a, float b) {
        float d = std::fmod(a - b + 540.f, 360.f);
        if (d < 0.f) d += 360.f;
        return d - 180.f;
    };
    check(std::abs(yawDelta(turnTarget(0.f, 180.f), 0.f)) > 179.9f,
          "180 degrees turns exactly behind the player");
    check(std::abs(yawDelta(turnTarget(170.f, 30.f), 170.f) - 30.f) < 0.01f,
          "a 30 degree turn is 30 degrees on the horizontal axis");
    check(std::abs(yawDelta(turnTarget(-170.f, -30.f), -170.f) + 30.f) < 0.01f,
          "turning the other way works across the -180 wrap");
    check(std::abs(yawDelta(turnTarget(90.f, 0.f), 90.f)) < 0.01f,
          "0 degrees leaves the yaw alone");
    check(turnTarget(0.f, 180.f) <= 180.f && turnTarget(0.f, 180.f) > -180.f,
          "the target yaw stays normalised");

    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    std::printf("%s\n", g_failed == 0 ? "PASS" : "FAILED");
    return g_failed == 0 ? 0 : 1;
}
