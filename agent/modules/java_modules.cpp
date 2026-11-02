// ============================================================================
//  Passive — Java module host, native half.
//
//  Load order, once, on the first frame the client can see a world:
//    1. resolve com.passive.fabric.modules.NativeApi through the game's loader
//    2. RegisterNatives, so every Java module tick has typed game access
//    3. read ModuleManager.getInstance().describe() and build the C++ cards
//    4. hand the descriptor to modules::registerJavaEntries, which swaps the
//       C++ cards the port replaced for the Java ones
//
//  Every frame after that: push enable-state changes, then call tick(dt).
//  A Java call never raises into the game; failures are cleared and logged.
// ============================================================================
#include "java_modules.h"

#include "../core/agent.h"
#include "../gui/gui.h"
#include "../jni/jvm.h"
#include "../jni/mc.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace tyr { namespace modules { namespace java {
namespace {

std::vector<Module> g_modules;
jobject g_manager = nullptr;
jmethodID g_tick = nullptr, g_setEnabled = nullptr, g_describe = nullptr, g_shutdown = nullptr;
jmethodID g_setBool = nullptr, g_setInt = nullptr, g_setFloat = nullptr, g_setMode = nullptr;
bool g_ready = false;
bool g_tried = false;
nlohmann::json g_pendingSettings = nlohmann::json::object();   // loaded before the host was ready

const char* const kManagerClass = "com.passive.fabric.modules.ModuleManager";
const char* const kManagerSig   = "Lcom/passive/fabric/modules/ModuleManager;";
const char* const kApiClass     = "com.passive.fabric.modules.NativeApi";

Category categoryOf(const std::string& name) {
    if (name == "Combat")   return Category::Combat;
    if (name == "Mace")     return Category::Mace;
    if (name == "Movement") return Category::Movement;
    if (name == "Spear")    return Category::Spear;
    if (name == "Visual")   return Category::Visual;
    if (name == "Crystal")  return Category::Crystal;
    if (name == "Macros")   return Category::Macros;
    return Category::Misc;
}

Tag tagOf(const std::string& name) {
    if (name == "Experimental") return Tag::Experimental;
    if (name == "Unsafe")       return Tag::Unsafe;
    if (name == "Blatant")      return Tag::Blatant;
    return Tag::Safe;
}

SettingType typeOf(const std::string& name) {
    if (name == "int")   return SettingType::Int;
    if (name == "float") return SettingType::Float;
    if (name == "mode")  return SettingType::Mode;
    return SettingType::Bool;
}

// ---- NativeApi implementations -------------------------------------------------------------

jobject localPlayer() { return mc::Client::player(); }

jboolean JNICALL nReady(JNIEnv*, jclass) {
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return JNI_FALSE;
    return (mc::Client::player() && mc::Client::world()) ? JNI_TRUE : JNI_FALSE;
}

jboolean JNICALL nScreenOpen(JNIEnv*, jclass) {
    return mc::Client::screenOpen() ? JNI_TRUE : JNI_FALSE;
}

jboolean JNICALL nInputFrozen(JNIEnv*, jclass) {
    return gui::inputFrozen() ? JNI_TRUE : JNI_FALSE;
}

jfloat JNICALL nHealth(JNIEnv*, jclass) {
    jobject player = localPlayer();
    return player ? (jfloat)mc::Player(player).health() : 0.f;
}

jint JNICALL nHurtTime(JNIEnv*, jclass) {
    jobject player = localPlayer();
    return player ? (jint)mc::LivingEntity(player).hurtTime() : 0;
}

jboolean JNICALL nOnGround(JNIEnv*, jclass) {
    jobject player = localPlayer();
    return (player && mc::LivingEntity(player).onGround()) ? JNI_TRUE : JNI_FALSE;
}

jdoubleArray JNICALL nPos(JNIEnv* env, jclass) {
    jobject player = localPlayer();
    if (!player) return nullptr;
    const Vec3 p = mc::Player(player).pos();
    const jdouble xyz[3] = { (jdouble)p.x, (jdouble)p.y, (jdouble)p.z };
    jdoubleArray out = env->NewDoubleArray(3);
    if (!out) { env->ExceptionClear(); return nullptr; }
    env->SetDoubleArrayRegion(out, 0, 3, xyz);
    return out;
}

jdoubleArray JNICALL nVelocity(JNIEnv* env, jclass) {
    jobject player = localPlayer();
    if (!player) return nullptr;
    const Vec3 v = mc::Player(player).velocity();
    const jdouble xyz[3] = { (jdouble)v.x, (jdouble)v.y, (jdouble)v.z };
    jdoubleArray out = env->NewDoubleArray(3);
    if (!out) { env->ExceptionClear(); return nullptr; }
    env->SetDoubleArrayRegion(out, 0, 3, xyz);
    return out;
}

jboolean JNICALL nSetVelocity(JNIEnv*, jclass, jdouble x, jdouble y, jdouble z) {
    jobject player = localPlayer();
    if (!player) return JNI_FALSE;
    mc::Player(player).setVelocity(Vec3((float)x, (float)y, (float)z));
    return JNI_TRUE;
}

jboolean JNICALL nPlayerWithin(JNIEnv* env, jclass, jdouble x, jdouble y, jdouble z, jdouble radius) {
    jobject world = mc::Client::world();
    jobject self = mc::Client::player();
    if (!world || !self) return JNI_FALSE;

    const double r2 = radius * radius;
    bool found = false;
    for (jobject entity : mc::collectEntities(world, 256)) {
        if (!entity) continue;
        const bool isPlayer = env->IsInstanceOf(entity, mc::sym().player) == JNI_TRUE;
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (isPlayer && env->IsSameObject(entity, self) != JNI_TRUE) {
            mc::LivingEntity living(entity);
            if (living.alive()) {
                const Vec3 d = living.pos() - Vec3((float)x, (float)y, (float)z);
                if (d.len2() <= (float)r2) found = true;
            }
        }
        env->DeleteLocalRef(entity);
    }
    return found ? JNI_TRUE : JNI_FALSE;
}

jboolean JNICALL nSetJumpKey(JNIEnv*, jclass, jboolean pressed) {
    return mc::ext::setKey(mc::ext::Key::Jump, pressed == JNI_TRUE) ? JNI_TRUE : JNI_FALSE;
}

jboolean JNICALL nSetSprintKey(JNIEnv*, jclass, jboolean pressed) {
    return mc::ext::setKey(mc::ext::Key::Sprint, pressed == JNI_TRUE) ? JNI_TRUE : JNI_FALSE;
}

void JNICALL nLog(JNIEnv* env, jclass, jstring message) {
    if (!message) return;
    trace::log("[java] %s", jni::fromJString(env, message).c_str());
}

bool registerNatives(JNIEnv* env) {
    jclass api = jni::findClass(env, kApiClass);
    if (!api) {
        trace::log("java modules: %s not found (standalone injector has no Java payload)", kApiClass);
        return false;
    }
    const JNINativeMethod methods[] = {
        { const_cast<char*>("ready"),        const_cast<char*>("()Z"),                       (void*)&nReady },
        { const_cast<char*>("screenOpen"),   const_cast<char*>("()Z"),                       (void*)&nScreenOpen },
        { const_cast<char*>("inputFrozen"),  const_cast<char*>("()Z"),                       (void*)&nInputFrozen },
        { const_cast<char*>("health"),       const_cast<char*>("()F"),                       (void*)&nHealth },
        { const_cast<char*>("hurtTime"),     const_cast<char*>("()I"),                       (void*)&nHurtTime },
        { const_cast<char*>("onGround"),     const_cast<char*>("()Z"),                       (void*)&nOnGround },
        { const_cast<char*>("pos"),          const_cast<char*>("()[D"),                      (void*)&nPos },
        { const_cast<char*>("velocity"),     const_cast<char*>("()[D"),                      (void*)&nVelocity },
        { const_cast<char*>("setVelocity"),  const_cast<char*>("(DDD)Z"),                    (void*)&nSetVelocity },
        { const_cast<char*>("playerWithin"), const_cast<char*>("(DDDD)Z"),                   (void*)&nPlayerWithin },
        { const_cast<char*>("setJumpKey"),   const_cast<char*>("(Z)Z"),                      (void*)&nSetJumpKey },
        { const_cast<char*>("setSprintKey"), const_cast<char*>("(Z)Z"),                      (void*)&nSetSprintKey },
        { const_cast<char*>("log"),          const_cast<char*>("(Ljava/lang/String;)V"),     (void*)&nLog },
    };
    const jint count = (jint)(sizeof(methods) / sizeof(methods[0]));
    if (env->RegisterNatives(api, methods, count) != 0) {
        jni::clearException(env, "RegisterNatives(NativeApi)");
        trace::log("java modules: RegisterNatives failed - module ticks would crash, host disabled");
        return false;
    }
    return true;
}

// ---- descriptor ----------------------------------------------------------------------------

jmethodID staticMethod(JNIEnv* env, jclass cls, const char* name, const char* sig) {
    return jni::smethod(env, cls, name, sig);
}

bool readDescriptor(JNIEnv* env) {
    jstring text = (jstring)env->CallObjectMethod(g_manager, g_describe);
    if (env->ExceptionCheck() || !text) {
        jni::clearException(env, "ModuleManager.describe");
        return false;
    }
    const std::string body = jni::fromJString(env, text);
    env->DeleteLocalRef(text);

    const nlohmann::json list = nlohmann::json::parse(body, nullptr, false);
    if (list.is_discarded() || !list.is_array()) {
        trace::log("java modules: describe() did not return a module array");
        return false;
    }

    g_modules.clear();
    g_modules.reserve(list.size());
    for (const auto& item : list) {
        if (!item.is_object()) continue;
        Module module;
        module.name = item.value("name", std::string());
        if (module.name.empty()) continue;
        module.description  = item.value("description", std::string());
        module.categoryName = item.value("category", std::string("Misc"));
        module.tagName      = item.value("tag", std::string("Safe"));
        module.category     = categoryOf(module.categoryName);
        module.tag          = tagOf(module.tagName);
        module.enabled      = item.value("enabled", false);
        module.lastEnabled  = module.enabled;
        if (item.contains("settings") && item["settings"].is_array()) {
            for (const auto& s : item["settings"]) {
                if (!s.is_object()) continue;
                Setting setting;
                setting.key = s.value("key", std::string());
                if (setting.key.empty()) continue;
                setting.label = s.value("label", setting.key);
                setting.type  = typeOf(s.value("type", std::string("bool")));
                switch (setting.type) {
                    case SettingType::Int:
                        setting.intValue = s.value("value", 0);
                        setting.minInt   = s.value("min", 0);
                        setting.maxInt   = s.value("max", 0);
                        break;
                    case SettingType::Float:
                        setting.floatValue = s.value("value", 0.f);
                        setting.minFloat   = s.value("min", 0.f);
                        setting.maxFloat   = s.value("max", 0.f);
                        break;
                    case SettingType::Mode:
                        setting.modeIndex = s.value("value", 0);
                        if (s.contains("options") && s["options"].is_array()) {
                            for (const auto& option : s["options"])
                                if (option.is_string()) setting.options.push_back(option.get<std::string>());
                        }
                        break;
                    case SettingType::Bool:
                    default:
                        setting.type      = SettingType::Bool;
                        setting.boolValue = s.value("value", false);
                        break;
                }
                module.settings.push_back(std::move(setting));
            }
        }
        g_modules.push_back(std::move(module));
    }

    // Java cards replace the C++ cards of the modules they now own. The list is only rebuilt
    // once, here, after g_modules is final: entries hold pointers into it.
    std::vector<Entry> cards;
    cards.reserve(g_modules.size());
    for (Module& module : g_modules) {
        cards.push_back(Entry{ module.name.c_str(), module.description.c_str(), module.category,
                               &module.enabled, &module.keybind, false, module.tag });
    }
    tyr::modules::registerJavaEntries(cards);
    return true;
}

jobject callManager(JNIEnv* env, jclass cls, const char* name, const char* sig) {
    jmethodID id = staticMethod(env, cls, name, sig);
    if (!id) return nullptr;
    return env->CallStaticObjectMethod(cls, id);
}

void syncEnabled(JNIEnv* env) {
    if (!g_ready || !g_manager) return;
    for (Module& module : g_modules) {
        if (module.enabled == module.lastEnabled) continue;
        jstring name = env->NewStringUTF(module.name.c_str());
        if (name) {
            env->CallVoidMethod(g_manager, g_setEnabled, name, module.enabled ? JNI_TRUE : JNI_FALSE);
            env->DeleteLocalRef(name);
            if (env->ExceptionCheck()) jni::clearException(env, "ModuleManager.setEnabled");
        }
        module.lastEnabled = module.enabled;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool init() {
    if (g_ready) return true;
    if (g_tried) return false;
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return false;
    JNIEnv* env = jni::env();
    if (!env) return false;

    g_tried = true;   // one honest attempt: a host that is absent stays absent for this session

    if (!registerNatives(env)) return false;

    jclass manager = jni::findClass(env, kManagerClass);
    if (!manager) {
        trace::log("java modules: %s not found; the native modules keep running", kManagerClass);
        return false;
    }
    jobject instance = callManager(env, manager, "getInstance", (std::string("()") + kManagerSig).c_str());
    if (!instance) {
        jni::clearException(env, "ModuleManager.getInstance");
        trace::log("java modules: ModuleManager.getInstance() returned nothing");
        return false;
    }
    g_manager = env->NewGlobalRef(instance);
    env->DeleteLocalRef(instance);
    if (!g_manager) {
        trace::log("java modules: could not pin ModuleManager");
        return false;
    }

    g_tick = jni::method(env, manager, "tick", "(F)V");
    g_setEnabled = jni::method(env, manager, "setEnabled", "(Ljava/lang/String;Z)V");
    g_describe = jni::method(env, manager, "describe", "()Ljava/lang/String;");
    g_shutdown = jni::method(env, manager, "shutdown", "()V");
    g_setBool = jni::method(env, manager, "setSettingBool", "(Ljava/lang/String;Ljava/lang/String;Z)V");
    g_setInt = jni::method(env, manager, "setSettingInt", "(Ljava/lang/String;Ljava/lang/String;I)V");
    g_setFloat = jni::method(env, manager, "setSettingFloat", "(Ljava/lang/String;Ljava/lang/String;F)V");
    g_setMode = jni::method(env, manager, "setSettingMode", "(Ljava/lang/String;Ljava/lang/String;I)V");
    if (!g_tick || !g_setEnabled || !g_describe) {
        trace::log("java modules: ModuleManager is missing tick/setEnabled/describe");
        env->DeleteGlobalRef(g_manager);
        g_manager = nullptr;
        return false;
    }

    if (!readDescriptor(env)) {
        env->DeleteGlobalRef(g_manager);
        g_manager = nullptr;
        return false;
    }

    g_ready = true;
    trace::log("java modules: %zu modules registered from %s", g_modules.size(), kManagerClass);

    // Config values loaded before the classes were up are applied now that the settings exist.
    if (!g_pendingSettings.empty()) {
        const nlohmann::json pending = g_pendingSettings;
        g_pendingSettings.clear();
        loadSettings(pending);
    }
    return true;
}

void tick(float dt) {
    if (!g_ready || !g_manager) return;
    JNIEnv* env = jni::env();
    if (!env) return;
    syncEnabled(env);
    env->CallVoidMethod(g_manager, g_tick, (jfloat)dt);
    if (env->ExceptionCheck()) jni::clearException(env, "ModuleManager.tick");
}

void disableAll() {
    for (Module& module : g_modules) module.enabled = false;
    if (!g_ready) return;
    JNIEnv* env = jni::env();
    if (!env) return;
    syncEnabled(env);
}

void shutdown() {
    if (g_manager) {
        JNIEnv* env = jni::env();
        if (env) {
            if (g_shutdown) {
                env->CallVoidMethod(g_manager, g_shutdown);
                if (env->ExceptionCheck()) jni::clearException(env, "ModuleManager.shutdown");
            }
            env->DeleteGlobalRef(g_manager);
        }
    }
    // g_modules is left alive on purpose: the module cards still point at its fields while the
    // client tears down, and the DLL is unloaded right after.
    g_manager = nullptr;
    g_tick = g_setEnabled = g_describe = g_shutdown = nullptr;
    g_setBool = g_setInt = g_setFloat = g_setMode = nullptr;
    g_ready = false;
}

bool ready() { return g_ready; }

bool owns(const std::string& name) {
    return std::any_of(g_modules.begin(), g_modules.end(),
                       [&](const Module& m) { return m.name == name; });
}

Module* find(const std::string& name) {
    for (Module& module : g_modules)
        if (module.name == name) return &module;
    return nullptr;
}

const std::vector<Module>& modules() { return g_modules; }

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

void setBool(Module& module, Setting& setting, bool value) {
    setting.boolValue = value;
    if (!g_ready || !g_setBool) return;
    JNIEnv* env = jni::env();
    if (!env) return;
    jstring name = env->NewStringUTF(module.name.c_str());
    jstring key = env->NewStringUTF(setting.key.c_str());
    if (name && key) {
        env->CallVoidMethod(g_manager, g_setBool, name, key, value ? JNI_TRUE : JNI_FALSE);
        if (env->ExceptionCheck()) jni::clearException(env, "setSettingBool");
    }
    if (name) env->DeleteLocalRef(name);
    if (key) env->DeleteLocalRef(key);
}

void setInt(Module& module, Setting& setting, int value) {
    setting.intValue = std::clamp(value, setting.minInt, setting.maxInt);
    if (!g_ready || !g_setInt) return;
    JNIEnv* env = jni::env();
    if (!env) return;
    jstring name = env->NewStringUTF(module.name.c_str());
    jstring key = env->NewStringUTF(setting.key.c_str());
    if (name && key) {
        env->CallVoidMethod(g_manager, g_setInt, name, key, (jint)setting.intValue);
        if (env->ExceptionCheck()) jni::clearException(env, "setSettingInt");
    }
    if (name) env->DeleteLocalRef(name);
    if (key) env->DeleteLocalRef(key);
}

void setFloat(Module& module, Setting& setting, float value) {
    setting.floatValue = std::max(setting.minFloat, std::min(setting.maxFloat, value));
    if (!g_ready || !g_setFloat) return;
    JNIEnv* env = jni::env();
    if (!env) return;
    jstring name = env->NewStringUTF(module.name.c_str());
    jstring key = env->NewStringUTF(setting.key.c_str());
    if (name && key) {
        env->CallVoidMethod(g_manager, g_setFloat, name, key, (jfloat)setting.floatValue);
        if (env->ExceptionCheck()) jni::clearException(env, "setSettingFloat");
    }
    if (name) env->DeleteLocalRef(name);
    if (key) env->DeleteLocalRef(key);
}

void setMode(Module& module, Setting& setting, int value) {
    const int last = setting.options.empty() ? 0 : (int)setting.options.size() - 1;
    setting.modeIndex = std::clamp(value, 0, last);
    if (!g_ready || !g_setMode) return;
    JNIEnv* env = jni::env();
    if (!env) return;
    jstring name = env->NewStringUTF(module.name.c_str());
    jstring key = env->NewStringUTF(setting.key.c_str());
    if (name && key) {
        env->CallVoidMethod(g_manager, g_setMode, name, key, (jint)setting.modeIndex);
        if (env->ExceptionCheck()) jni::clearException(env, "setSettingMode");
    }
    if (name) env->DeleteLocalRef(name);
    if (key) env->DeleteLocalRef(key);
}

float cardHeight(const std::string& name, bool expanded) {
    if (!expanded) return 0.f;
    for (const Module& module : g_modules) {
        if (module.name != name) continue;
        float y = 76.f;   // the same first row every card's options use
        for (const Setting& setting : module.settings) {
            switch (setting.type) {
                case SettingType::Bool:  y += 30.f; break;
                case SettingType::Mode:  y += 46.f; break;
                case SettingType::Int:
                case SettingType::Float: y += 37.f; break;
            }
        }
        return y + 50.f;   // the keybind block and the card's bottom padding
    }
    return 0.f;
}

nlohmann::json settingsJson() {
    nlohmann::json out = nlohmann::json::object();
    for (const Module& module : g_modules) {
        for (const Setting& setting : module.settings) {
            switch (setting.type) {
                case SettingType::Bool:  out[module.name][setting.key] = setting.boolValue; break;
                case SettingType::Int:   out[module.name][setting.key] = setting.intValue; break;
                case SettingType::Float: out[module.name][setting.key] = setting.floatValue; break;
                case SettingType::Mode:  out[module.name][setting.key] = setting.modeIndex; break;
            }
        }
    }
    return out;
}

void loadSettings(const nlohmann::json& json) {
    if (!json.is_object()) return;
    if (!g_ready) {
        // Config load runs before the first frame; keep the values until the host is up.
        for (auto it = json.begin(); it != json.end(); ++it) g_pendingSettings[it.key()] = it.value();
        return;
    }
    for (Module& module : g_modules) {
        const auto moduleIt = json.find(module.name);
        if (moduleIt == json.end() || !moduleIt->is_object()) continue;
        for (Setting& setting : module.settings) {
            const auto value = moduleIt->find(setting.key);
            if (value == moduleIt->end()) continue;
            switch (setting.type) {
                case SettingType::Bool:  if (value->is_boolean()) setBool(module, setting, value->get<bool>()); break;
                case SettingType::Int:   if (value->is_number_integer()) setInt(module, setting, value->get<int>()); break;
                case SettingType::Float: if (value->is_number()) setFloat(module, setting, value->get<float>()); break;
                case SettingType::Mode:  if (value->is_number_integer()) setMode(module, setting, value->get<int>()); break;
            }
        }
    }
}

} } }
