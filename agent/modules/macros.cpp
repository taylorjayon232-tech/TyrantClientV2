// ============================================================================
//  Passive — macro modules.
//  Macro modules are structured as trigger-style actions: bind fires once,
//  the module executes a sequence, and it does not behave like a traditional
//  toggleable combat/render module.
// ============================================================================
#include "module.h"
#include "modules.h"
#include "../core/agent.h"
#include "../jni/mc.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace tyr {
namespace modules {

class MacroModule : public Module {
public:
    MacroModule(const char* name, const char* desc)
        : Module(name, "Macros", desc) {
        setKeybindMode(1); // trigger-style: press bind to fire once
    }

    void update(float dt) override {
        if (!isExecuting_) {
            if (!hasTrigger()) return;
            consumeTrigger();
            startSequence();
            return;
        }

        timer_ -= dt;
        if (timer_ > 0.f) return;

        if (stageIndex_ < steps_.size()) {
            executeStep(steps_[stageIndex_]);
            ++stageIndex_;
            if (stageIndex_ < steps_.size()) {
                timer_ = nextDelayMs_() / 1000.0f;
            } else {
                isExecuting_ = false;
                stageIndex_ = 0;
            }
        } else {
            isExecuting_ = false;
            stageIndex_ = 0;
        }
    }

    void startSequence() {
        isExecuting_ = true;
        stageIndex_ = 0;
        timer_ = 0.0f;
        setDebugLine("macro running");
        trace::log("macro triggered: %s", name().c_str());
        if (!steps_.empty()) {
            executeStep(steps_[0]);
            ++stageIndex_;
            if (stageIndex_ < steps_.size()) {
                timer_ = nextDelayMs_() / 1000.0f;
            } else {
                isExecuting_ = false;
                stageIndex_ = 0;
            }
        }
    }

protected:
    struct Step {
        std::string label;
        float delayMs;
    };

    void addStep(const char* label, float delayMs) {
        steps_.push_back({ label, delayMs });
    }

    virtual void executeStep(const Step& step) = 0;

    float nextDelayMs_() const {
        if (steps_.empty() || stageIndex_ >= steps_.size()) return 0.0f;
        return steps_[stageIndex_].delayMs;
    }

    bool isExecuting_ = false;
    std::vector<Step> steps_;
    size_t stageIndex_ = 0;
    float timer_ = 0.0f;
};

namespace {

bool matchesItemId(jobject stack, std::initializer_list<const char*> ids) {
    if (!stack) return false;
    const std::string itemId = mc::stk::itemId(stack);
    if (itemId.empty()) return false;
    for (const char* id : ids) {
        if (itemId == id) return true;
    }
    return false;
}

bool selectItemById(jobject inventory, std::initializer_list<const char*> ids) {
    if (!inventory) return false;
    jobject list = mc::inv::mainList(inventory);
    if (!list) return false;
    const int count = mc::inv::listSize(list);
    for (int slot = 0; slot < std::min(count, 9); ++slot) {
        jobject stack = mc::inv::getFromList(list, slot);
        if (matchesItemId(stack, ids)) {
            mc::inv::setSelectedSlotMethod(inventory, slot);
            return true;
        }
    }
    for (int slot = 9; slot < std::min(count, 36); ++slot) {
        jobject stack = mc::inv::getFromList(list, slot);
        if (matchesItemId(stack, ids)) {
            mc::inv::swapWithHotbar(inventory, slot);
            mc::inv::setSelectedSlotMethod(inventory, 0);
            return true;
        }
    }
    return false;
}

bool tryPlaceCurrentBlock() {
    jobject hit = mc::Client::crosshair();
    if (!hit || mc::hit::typeOf(hit) != 1) return false;
    return mc::Client::interactAtCrosshair();
}

void aimAtTarget(const Vec3& from, const Vec3& target, mc::Player& player) {
    const Vec3 delta = target - from;
    const float horiz = std::sqrt(delta.x * delta.x + delta.z * delta.z);
    if (horiz < 1e-4f) return;

    const float yawDeg = std::atan2(delta.x, delta.z) * 180.0f / 3.14159265358979323846f;
    const float pitchDeg = -std::atan2(delta.y, horiz) * 180.0f / 3.14159265358979323846f;
    player.setYaw(yawDeg);
    player.setPitch(pitchDeg);
}

} // namespace

class XbowCartMacro : public MacroModule {
public:
    XbowCartMacro()
        : MacroModule("Xbow Cart Macro", "Rail -> Cart -> Flint & Steel -> Crossbow") {
        setEnabled(true);
        setKeybindMode(1);

        addSetting(Setting("Rail Place Min Delay", 80.0f, 1.0f, 600.0f));
        addSetting(Setting("Rail Place Max Delay", 120.0f, 1.0f, 600.0f));
        addSetting(Setting("Cart Place Min Delay", 80.0f, 1.0f, 600.0f));
        addSetting(Setting("Cart Place Max Delay", 120.0f, 1.0f, 600.0f));
        addSetting(Setting("Fire Place Min Delay", 110.0f, 1.0f, 600.0f));
        addSetting(Setting("Fire Place Max Delay", 170.0f, 1.0f, 600.0f));
        addSetting(Setting("Crossbow Shoot Min Delay", 80.0f, 1.0f, 600.0f));
        addSetting(Setting("Crossbow Shoot Max Delay", 120.0f, 1.0f, 600.0f));
        addSetting(Setting("Silent Fire Place", false));

        addStep("Rail", 100.0f);
        addStep("Cart", 110.0f);
        addStep("Flint & Steel", 120.0f);
        addStep("Crossbow", 100.0f);
    }

protected:
    void executeStep(const Step& step) override {
        setDebugLine("step: " + step.label);
        trace::log("macro step: %s (%s)", name().c_str(), step.label.c_str());

        JNIEnv* e = jni::env();
        jobject player = mc::Client::player();
        if (!e || !player) return;

        mc::Player p(player);
        jobject inv = p.inventory();
        if (!inv) return;

        if (step.label == "Rail") {
            if (!selectItemById(inv, { "rail" })) {
                setDebugLine("rail not found");
                return;
            }
            if (!tryPlaceCurrentBlock()) {
                setDebugLine("rail placement failed");
            }
            return;
        }

        if (step.label == "Cart") {
            if (!selectItemById(inv, { "minecart", "tnt_minecart" })) {
                setDebugLine("cart not found");
                return;
            }
            if (!tryPlaceCurrentBlock()) {
                setDebugLine("cart placement failed");
            }
            return;
        }

        if (step.label == "Flint & Steel") {
            if (!selectItemById(inv, { "flint_and_steel" })) {
                setDebugLine("flint and steel not found");
                return;
            }

            jobject hit = mc::Client::crosshair();
            if (hit && mc::hit::typeOf(hit) == 1 && !boolVal(7)) {
                const Vec3 target = mc::hit::pos(hit);
                const Vec3 eye = mc::Entity(player).eyePos();
                aimAtTarget(eye, target, p);
            }

            if (!tryPlaceCurrentBlock()) {
                setDebugLine("fire placement failed");
            }
            return;
        }

        if (step.label == "Crossbow") {
            if (!selectItemById(inv, { "crossbow" })) {
                setDebugLine("crossbow not found");
                return;
            }

            jobject hit = mc::Client::crosshair();
            if (hit) {
                const Vec3 target = mc::hit::pos(hit);
                const Vec3 eye = mc::Entity(player).eyePos();
                aimAtTarget(eye, target, p);
            }

            hit = mc::Client::crosshair();
            if (hit) {
                jobject ent = mc::hit::entity(hit);
                if (ent) {
                    mc::Client::attackEntity(ent);
                    return;
                }
            }

            mc::Client::doAttack();
        }
    }
};

void registerMacroModules() {
    all().push_back(std::unique_ptr<Module>(new XbowCartMacro()));
}

} // namespace modules
} // namespace tyr
