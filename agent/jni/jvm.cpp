// ============================================================================
//  Passive — JNI foundations: VM handle, thread attach, class resolution
//  through Fabric's Knot ClassLoader, and lifecycle helpers.
// ============================================================================
#include "jvm.h"
#include "silent_rotation.h"
#include "ids.h"
#include "../core/agent.h"   // trace::log — self-reporting classloader hunt

#include <jni.h>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_set>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>

namespace tyr {
namespace jni {

static JavaVM*        g_vm        = nullptr;
static jobject        g_classLoader = nullptr;
// The Clarity jar's own loader. Kept alive as a global ref: the game loader cannot see
// need.clarity.*, and the child delegates to it, so game classes still resolve normally.
static jobject        g_clarityClassLoader = nullptr;
static bool           g_clarityLoaded = false;
static jmethodID      g_loadClassMid = nullptr;
static std::vector<std::string> g_errors;
static std::mutex     g_errMutex;
static std::unordered_set<std::string> g_reportedExceptions;
static std::atomic<int> g_failedLookups{0};   // failed JNI id lookups (resolve quality metric)
static std::atomic<int> g_totalLookups{0};    // all JNI id lookups attempted (diagnostics)
static std::atomic<int> g_excDumps{0};        // rate-limiter for ExceptionDescribe

void ensureClassLoader(); // fwd: probe-then-hunt, defined below
void releaseClarityRefs(JNIEnv* env); // fwd: defined with the Clarity bridge at the bottom

// ---------------------------------------------------------------------------
// JNIEnv is per-thread. The old code cached ONE env in a global, captured on
// whichever thread ran the bootstrap - normally the render thread, but the
// 8-second fallback bootstraps on the init thread, and every later render frame
// then handed the init thread's JNIEnv to JNI calls running on the render
// thread. Using another thread's JNIEnv is undefined behaviour, and it is one
// of the ways a native call can abort the JVM or leave the overlay dead. Always
// resolve and cache the CALLING thread's env instead.
// ---------------------------------------------------------------------------
static thread_local JNIEnv* t_env      = nullptr;   // this thread's env, cached
static thread_local bool    t_attached = false;     // WE attached this thread (only then may we detach it)

static JNIEnv* threadEnv() {
    if (t_env) return t_env;
    if (!g_vm) return nullptr;
    JNIEnv* e = nullptr;
    const jint state = g_vm->GetEnv(reinterpret_cast<void**>(&e), JNI_VERSION_1_8);
    if (state == JNI_OK) { t_env = e; return t_env; }   // already a Java thread (the render thread)
    if (state == JNI_EDETACHED) {
        JavaVMAttachArgs args{ JNI_VERSION_1_8, const_cast<char*>("Passive"), nullptr };
        if (g_vm->AttachCurrentThread(reinterpret_cast<void**>(&e), &args) == JNI_OK) {
            t_env = e;
            t_attached = true;
            return t_env;
        }
    }
    return nullptr;
}

// Detach only a thread WE attached: Minecraft owns the render thread, so
// detaching that one on unload would corrupt the game's own JNI usage.
static void releaseThreadEnv() {
    if (g_vm && t_attached) g_vm->DetachCurrentThread();
    t_env = nullptr;
    t_attached = false;
}

JavaVM* vm()          { return g_vm; }
JNIEnv* env()         { return threadEnv(); }
jobject classLoader() { return g_classLoader; }   // global reference (never deleted here)
bool    ready()       { return g_vm && g_classLoader && threadEnv() != nullptr; }

void pushError(const char* what) {
    std::lock_guard<std::mutex> lock(g_errMutex);
    if (g_errors.size() < 64) g_errors.emplace_back(what);
}
std::vector<std::string> drainErrors() {
    std::lock_guard<std::mutex> lock(g_errMutex);
    std::vector<std::string> out;
    out.swap(g_errors);
    return out;
}

int failedLookups() { return g_failedLookups.load(std::memory_order_relaxed); }
int totalLookups()  { return g_totalLookups.load(std::memory_order_relaxed); }

void clearException(JNIEnv* env, const char* where) {
    if (env && env->ExceptionCheck()) {
        // Never call ExceptionDescribe on the game render thread. Java's
        // logger turns a repeated JNI miss into an unbounded log flood.
        g_excDumps.fetch_add(1, std::memory_order_relaxed);
        env->ExceptionClear();
        std::lock_guard<std::mutex> lock(g_errMutex);
        if (g_reportedExceptions.emplace(where ? where : "jni").second && g_errors.size() < 64)
            g_errors.emplace_back(where ? where : "jni");
    }
}

// ---------------------------------------------------------------------------
// init: locate the JVM (already created by Minecraft), attach this thread
// (the hooked render thread), and grab Fabric's Knot ClassLoader so we can
// resolve net.minecraft.* classes (FindClass cannot see them from a native
// thread's system classloader).
// ---------------------------------------------------------------------------
bool init() {
    if (ready()) return true;

    // JNI_GetCreatedJavaVMs lives in the JVM, not in any import library.
    // Resolve it from the loaded jvm.dll at runtime (the injector's remote
    // thread loads us after Minecraft's JVM is up, so it is always present).
    using GetCreatedVMsFn = jint (JNICALL*)(JavaVM**, jsize, jsize*);
    static GetCreatedVMsFn getVMs = nullptr;
    if (!getVMs) {
        HMODULE jvm = ::GetModuleHandleW(L"jvm.dll");
        if (!jvm) {
            // Some launchers ship the JVM under a different module name.
            HMODULE h = nullptr;
            do {
                MODULEENTRY32W me{};
                me.dwSize = sizeof(me);
                HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, ::GetCurrentProcessId());
                if (snap == INVALID_HANDLE_VALUE) break;
                if (::Module32FirstW(snap, &me)) {
                    do {
                        if (::GetProcAddress(me.hModule, "JNI_GetCreatedJavaVMs")) {
                            h = me.hModule; break;
                        }
                    } while (::Module32NextW(snap, &me));
                }
                ::CloseHandle(snap);
                jvm = h;
            } while (false);
        }
        if (!jvm) { pushError("jvm.dll not found (not a Java process?)"); return false; }
        getVMs = reinterpret_cast<GetCreatedVMsFn>(::GetProcAddress(jvm, "JNI_GetCreatedJavaVMs"));
        if (!getVMs) { pushError("JNI_GetCreatedJavaVMs missing"); return false; }
    }

    jsize n = 0;
    if (getVMs(&g_vm, 1, &n) != JNI_OK || n < 1 || !g_vm) {
        pushError("JNI_GetCreatedJavaVMs failed");
        return false;
    }

    // Attach (or reuse) THIS thread and remember its env for every later call.
    JNIEnv* env = threadEnv();
    if (!env) {
        pushError("AttachCurrentThread failed");
        g_vm = nullptr;   // leave no half-initialised VM behind
        return false;
    }

    // Legacy single-source acquisition replaced by ensureClassLoader() below:
    // the render thread's CCL is often NULL in production, which silently
    // landed us on the SYSTEM loader (cannot see net.minecraft.*).
    const jclass clCls = env->FindClass("java/lang/ClassLoader");
    if (!clCls) { clearException(env, "ClassLoader"); return false; }
    const jmethodID loadClassMid = env->GetMethodID(clCls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    if (!loadClassMid) { clearException(env, "loadClass"); return false; }
    g_loadClassMid = loadClassMid;

    ensureClassLoader();
    if (!g_classLoader) { pushError("no classloader could load class_310"); return false; }

    // Both loader and loadClass are in place, so this is the one moment the Clarity jar can
    // be attached. Failure is not fatal: the cosmetic modules simply have nothing to drive.
    injectClarityJar();

    // Safety net (the Identifier-crash class): never leave a pending Java
    // exception behind after init — a pending exception turns the NEXT JNI
    // call into undefined behavior, which has killed the JVM before.
    if (env && env->ExceptionCheck() == JNI_TRUE) {
        pushError("init: pending Java exception cleared at boot");
        clearException(env, "init: pending Java exception");
    }
    return true;
}

// ---------------------------------------------------------------------------
// probeLoads: does this loader actually load net.minecraft.class_310?
// loadClass needs the DOTTED binary name (slash form is only for FindClass).
// ---------------------------------------------------------------------------
static bool probeLoads(JNIEnv* env, jobject cl) {
    if (!env || !cl || !g_loadClassMid) return false;
    const jstring name = env->NewStringUTF("net.minecraft.class_310");
    if (!name) { env->ExceptionClear(); return false; }
    const jclass c = static_cast<jclass>(
        env->CallObjectMethod(cl, g_loadClassMid, name));
    env->DeleteLocalRef(name);
    if (env->ExceptionCheck()) { clearException(env, "probeLoads"); return false; }
    if (c) env->DeleteLocalRef(c);
    return c != nullptr;
}

// ---------------------------------------------------------------------------
// storeLoader: promote a candidate to the global classloader.
// ---------------------------------------------------------------------------
static void storeLoader(JNIEnv* env, jobject cl, const char* source) {
    if (g_classLoader) env->DeleteGlobalRef(g_classLoader);
    g_classLoader = env->NewGlobalRef(cl);
    trace::log("classloader acquired via %s (probe loads class_310: yes)", source);
}

// ---------------------------------------------------------------------------
// ensureClassLoader: probe-then-hunt. Order:
//   1. current thread's context classloader
//   2. system classloader
//   3. EVERY live thread's context classloader (Knot lives on one of them)
//   4. FindClass("net/minecraft/class_310") + Class.getClassLoader()
// Every candidate is PROVEN with probeLoads; winner is logged by name.
// ---------------------------------------------------------------------------
void ensureClassLoader() {
    JNIEnv* env = threadEnv();
    if (!env || !g_vm || !g_loadClassMid) { pushError("ensureClassLoader: no env"); return; }

    if (g_classLoader && probeLoads(env, g_classLoader)) return; // already good

    const jclass threadCls = env->FindClass("java/lang/Thread");
    if (!threadCls) { clearException(env, "Thread"); return; }
    const jmethodID currentThreadMid = env->GetStaticMethodID(threadCls, "currentThread", "()Ljava/lang/Thread;");
    const jmethodID ctxClMid         = env->GetMethodID(threadCls, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
    if (!currentThreadMid || !ctxClMid) { clearException(env, "Thread methods"); return; }

    const jclass clCls = env->FindClass("java/lang/ClassLoader");
    if (!clCls) { clearException(env, "ClassLoader"); return; }
    const jmethodID sysClMid = env->GetStaticMethodID(clCls, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");

    const jobject self = env->CallStaticObjectMethod(threadCls, currentThreadMid);
    clearException(env, "currentThread");

    // 1) current thread's CCL
    if (self) {
        const jobject cl = env->CallObjectMethod(self, ctxClMid);
        clearException(env, "ctxClassLoader(self)");
        if (cl) {
            if (probeLoads(env, cl)) { storeLoader(env, cl, "current thread CCL"); env->DeleteLocalRef(cl); env->DeleteLocalRef(self); return; }
            env->DeleteLocalRef(cl);
        }
        env->DeleteLocalRef(self);
    }

    // 2) system loader
    if (sysClMid) {
        const jobject cl = env->CallStaticObjectMethod(clCls, sysClMid);
        clearException(env, "sysClassLoader");
        if (cl) {
            if (probeLoads(env, cl)) { storeLoader(env, cl, "system loader"); env->DeleteLocalRef(cl); return; }
            env->DeleteLocalRef(cl);
        }
    }

    // 3) every live thread's CCL via Thread.getAllStackTraces().keySet().
    //    (Thread.enumerate(Thread[]) was tried before but throws
    //    ArrayStoreException: ThreadGroup.enumerate requires an exact Thread[].)
    int scanned = 0;
    {
        const jmethodID allMid = env->GetStaticMethodID(threadCls, "getAllStackTraces", "()Ljava/util/Map;");
        const jmethodID keySetMid = allMid ? env->GetMethodID(env->FindClass("java/util/Map"), "keySet", "()Ljava/util/Set;") : nullptr;
        jclass setCls = keySetMid ? env->FindClass("java/util/Set") : nullptr;
        const jmethodID iterMid = setCls ? env->GetMethodID(setCls, "iterator", "()Ljava/util/Iterator;") : nullptr;
        jclass itCls = iterMid ? env->FindClass("java/util/Iterator") : nullptr;
        const jmethodID hasNextMid = itCls ? env->GetMethodID(itCls, "hasNext", "()Z") : nullptr;
        const jmethodID nextMid    = itCls ? env->GetMethodID(itCls, "next", "()Ljava/lang/Object;") : nullptr;
        if (allMid && keySetMid && iterMid && hasNextMid && nextMid) {
            const jobject map = env->CallStaticObjectMethod(threadCls, allMid);
            clearException(env, "getAllStackTraces");
            if (map) {
                const jobject set = env->CallObjectMethod(map, keySetMid);
                clearException(env, "keySet");
                if (set) {
                    const jobject it = env->CallObjectMethod(set, iterMid);
                    clearException(env, "set iterator");
                    while (it && env->CallBooleanMethod(it, hasNextMid)) {
                        const jobject t = env->CallObjectMethod(it, nextMid);
                        if (!t) break;
                        ++scanned;
                        const jobject cl = env->CallObjectMethod(t, ctxClMid);
                        clearException(env, "ctxClassLoader(scan)");
                        env->DeleteLocalRef(t);
                        if (cl) {
                            if (probeLoads(env, cl)) {
                                storeLoader(env, cl, "thread-scan CCL");
                                trace::log("thread-scan: %d threads scanned, winner found", scanned);
                                env->DeleteLocalRef(cl); env->DeleteLocalRef(it);
                                env->DeleteLocalRef(set); env->DeleteLocalRef(map);
                                return;
                            }
                            env->DeleteLocalRef(cl);
                        }
                    }
                    if (it) env->DeleteLocalRef(it);
                    env->DeleteLocalRef(set);
                }
                env->DeleteLocalRef(map);
            }
        } else {
            clearException(env, "getAllStackTraces wiring");
        }
        if (itCls) env->DeleteLocalRef(itCls);
        if (setCls) env->DeleteLocalRef(setCls);
    }
    trace::log("thread-scan found nothing across %d threads — trying class-of-class path", scanned);

    // 4) FindClass on the render thread (it RUNS Minecraft Java frames, so
    //    FindClass resolves through Knot here) -> Class.getClassLoader().
    {
        const jclass direct = env->FindClass("net/minecraft/class_310");
        clearException(env, "direct FindClass class_310");
        if (direct) {
            const jclass classCls = env->FindClass("java/lang/Class");
            if (classCls) {
                const jmethodID cclMid = env->GetMethodID(classCls, "getClassLoader", "()Ljava/lang/ClassLoader;");
                if (cclMid) {
                    const jobject cl = env->CallObjectMethod(direct, cclMid);
                    clearException(env, "Class.getClassLoader");
                    if (cl) {
                        if (probeLoads(env, cl)) {
                            storeLoader(env, cl, "class_310.getClassLoader");
                            env->DeleteLocalRef(cl); env->DeleteLocalRef(classCls); env->DeleteLocalRef(direct);
                            return;
                        }
                        env->DeleteLocalRef(cl);
                    }
                }
                env->DeleteLocalRef(classCls);
            }
            env->DeleteLocalRef(direct);
        }
    }
    pushError("ensureClassLoader: all sources failed to load class_310");
}

void shutdown() {
    silentRotationShutdown();
    JNIEnv* e = threadEnv();
    // Detaching the render thread is wrong (Minecraft owns it) — just drop refs.
    if (e && g_classLoader) e->DeleteGlobalRef(g_classLoader);
    g_classLoader = nullptr;
    releaseThreadEnv();
    g_vm = nullptr;
}

void detach() {
    silentRotationShutdown();
    // Safe unload path (DestroyJavaVM would hang/kill the game and must NEVER
    // be called from an injected agent). Release the class-loader global
    // reference and detach the threads we attached ourselves.
    JNIEnv* e = threadEnv();
    if (e) {
        releaseClarityRefs(e);
        if (g_clarityClassLoader) e->DeleteGlobalRef(g_clarityClassLoader);
        g_clarityClassLoader = nullptr;
        if (g_classLoader) e->DeleteGlobalRef(g_classLoader);
    }
    g_clarityLoaded = false;
    g_classLoader = nullptr;
    g_loadClassMid = nullptr;
    releaseThreadEnv();
    g_vm = nullptr;
}

// ---------------------------------------------------------------------------
// findClass: resolve through Knot. Converts "net.minecraft.class_310" ->
// "net/minecraft/class_310" internally.
// ---------------------------------------------------------------------------
jclass findClass(JNIEnv* env, const char* dotted) {
    ++g_totalLookups;
    if (!env || !g_classLoader || !g_loadClassMid) return nullptr;
    // ClassLoader.loadClass takes the DOTTED binary name ("net.minecraft.class_310").
    // The previous slash-form conversion guaranteed ClassNotFoundException.
    const jstring jname = env->NewStringUTF(dotted);
    if (!jname) { env->ExceptionClear(); return nullptr; }
    const jclass cls = static_cast<jclass>(
        env->CallObjectMethod(g_classLoader, g_loadClassMid, jname));
    env->DeleteLocalRef(jname);
    if (env->ExceptionCheck()) {
        clearException(env, "findClass");
        return nullptr;
    }
    return cls;
}

jfieldID field(JNIEnv* env, jclass cls, const char* name, const char* sig) {
    ++g_totalLookups;
    if (!cls) return nullptr;
    jfieldID id = env->GetFieldID(cls, name, sig);
    if (!id) { ++g_failedLookups; clearException(env, "field"); }
    return id;
}
jfieldID sfield(JNIEnv* env, jclass cls, const char* name, const char* sig) {
    ++g_totalLookups;
    if (!cls) return nullptr;
    jfieldID id = env->GetStaticFieldID(cls, name, sig);
    if (!id) { ++g_failedLookups; clearException(env, "static field"); }
    return id;
}
jmethodID method(JNIEnv* env, jclass cls, const char* name, const char* sig) {
    ++g_totalLookups;
    if (!cls) return nullptr;
    jmethodID id = env->GetMethodID(cls, name, sig);
    if (!id) { ++g_failedLookups; clearException(env, "method"); }
    return id;
}
jmethodID smethod(JNIEnv* env, jclass cls, const char* name, const char* sig) {
    ++g_totalLookups;
    if (!cls) return nullptr;
    jmethodID id = env->GetStaticMethodID(cls, name, sig);
    if (!id) { ++g_failedLookups; clearException(env, "static method"); }
    return id;
}

jobject getObj(JNIEnv* env, jobject obj, jfieldID fid) {
    if (!obj || !fid) return nullptr;
    return env->GetObjectField(obj, fid);
}
jobject sgetObj(JNIEnv* env, jclass cls, jfieldID fid) {
    if (!cls || !fid) return nullptr;
    return env->GetStaticObjectField(cls, fid);
}

jfloat getFloat(JNIEnv* env, jobject obj, jfieldID fid, float fallback) {
    if (!obj || !fid) return fallback;
    return env->GetFloatField(obj, fid);
}
jint getInt(JNIEnv* env, jobject obj, jfieldID fid, jint fallback) {
    if (!obj || !fid) return fallback;
    return env->GetIntField(obj, fid);
}
jdouble getDouble(JNIEnv* env, jobject obj, jfieldID fid, jdouble fallback) {
    if (!obj || !fid) return fallback;
    return env->GetDoubleField(obj, fid);
}
jboolean getBool(JNIEnv* env, jobject obj, jfieldID fid, jboolean fallback) {
    if (!obj || !fid) return fallback;
    return env->GetBooleanField(obj, fid);
}

jstring toJString(JNIEnv* env, const char* s) {
    return env ? env->NewStringUTF(s ? s : "") : nullptr;
}

std::string fromJString(JNIEnv* env, jstring js) {
    if (!env || !js) return {};
    const char* c = env->GetStringUTFChars(js, nullptr);
    std::string out = c ? c : "";
    if (c) env->ReleaseStringUTFChars(js, c);
    return out;
}

// Resolve a logical "Class.field" description lazily; used by the debug overlay.
jobject enumConst(JNIEnv* env, jclass cls, const char* fieldName) {
    if (!cls) return nullptr;
    const jfieldID fid = sfield(env, cls, fieldName, "Lnet/minecraft/class_1304;");
    // Note: enum signature differs per class; callers resolve sig themselves via
    // sfield directly. This helper is for EquipmentSlot-shaped enums only.
    return sgetObj(env, cls, fid);
}

// ---------------------------------------------------------------------------
// Clarity cosmetics bridge
//
// The five visual modules are drawn by the Clarity Java mod, which ships as a jar beside the
// client and is loaded here into a child URLClassLoader whose parent is the game's own loader.
// The game keeps seeing its own classes; the jar's need.clarity.* classes resolve through the
// child via clarityClass below.
//
// Everything is resolved ONCE, in bindClarity, and cached as global references: drawOverlay
// runs every frame, and a per-frame loadClass storm for five modules plus their base class is
// not free. Nothing here is allowed to hurt the game - a missing jar, class, field or method
// just leaves that part of the bridge disabled.
// ---------------------------------------------------------------------------
namespace {

struct ClarityToggle {
    const char* className;      // need.clarity.modules.visual.*
    const char* instanceField;  // the static field holding the module singleton
    jclass      cls;            // global ref to the module class
    jfieldID    instance;
    jmethodID   ctor;           // fallback when the singleton field is still null
};

ClarityToggle g_clToggles[] = {
    { "need.clarity.modules.visual.PlayerModel", "instance", nullptr, nullptr, nullptr },
    { "need.clarity.modules.visual.Wings",       "instance", nullptr, nullptr, nullptr },
    { "need.clarity.modules.visual.Hats",        "instance", nullptr, nullptr, nullptr },
    { "need.clarity.modules.visual.Capes",       "INSTANCE", nullptr, nullptr, nullptr },
    { "need.clarity.modules.visual.ViewModel",   "instance", nullptr, nullptr, nullptr },
};
constexpr int kClarityToggleCount = (int)(sizeof(g_clToggles) / sizeof(g_clToggles[0]));

jclass    g_clModuleBase      = nullptr;  // need.clarity.modules.ModuleBase
jmethodID g_clSetEnabled      = nullptr;
jclass    g_clWorldRender     = nullptr;  // need.clarity.WorldRenderEvent
jmethodID g_clWorldRenderFire = nullptr;
jclass    g_clPerFrame        = nullptr;  // need.clarity.PerFrameEvents
jmethodID g_clPerFrameFire    = nullptr;
jclass    g_clPoseStack       = nullptr;  // com.mojang.blaze3d.vertex.PoseStack
jmethodID g_clPoseStackInit   = nullptr;
bool      g_clBound           = false;

// need.clarity.* lives only in the jar's child loader, so it is asked directly instead of through
// findClass: routing it through the game loader would raise a ClassNotFoundException on every
// lookup and clutter the diagnostics panel with failures that are expected. Returns a local ref.
jclass clarityClass(JNIEnv* env, const char* dotted) {
    if (!env || !g_clarityClassLoader || !g_loadClassMid) return nullptr;
    const jstring jname = env->NewStringUTF(dotted);
    if (!jname) { env->ExceptionClear(); return nullptr; }
    const jclass cls = static_cast<jclass>(
        env->CallObjectMethod(g_clarityClassLoader, g_loadClassMid, jname));
    env->DeleteLocalRef(jname);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
    return cls;
}

// Promote a local class reference to a global one so it survives past this frame.
jclass keepClass(JNIEnv* env, jclass cls) {
    if (!env || !cls) return nullptr;
    jclass g = static_cast<jclass>(env->NewGlobalRef(cls));
    if (env->ExceptionCheck()) { clearException(env, "clarity class ref"); return nullptr; }
    return g;
}

void bindClarity(JNIEnv* env) {
    if (g_clBound) return;
    g_clBound = true;

    jclass base = clarityClass(env, "need.clarity.modules.ModuleBase");
    if (base) {
        g_clModuleBase = keepClass(env, base);
        g_clSetEnabled = env->GetMethodID(base, "setEnabled", "(Z)V");
        clearException(env, "ModuleBase.setEnabled");
    }
    for (int i = 0; i < kClarityToggleCount; ++i) {
        ClarityToggle& c = g_clToggles[i];
        jclass cls = clarityClass(env, c.className);
        if (!cls) continue;
        std::string sig = "L";
        for (const char* p = c.className; *p; ++p) sig += (*p == '.') ? '/' : *p;
        sig += ";";
        c.instance = env->GetStaticFieldID(cls, c.instanceField, sig.c_str());
        clearException(env, "clarity module instance field");
        if (!c.instance) continue;          // no singleton to drive: leave this one out
        c.ctor = env->GetMethodID(cls, "<init>", "()V");
        clearException(env, "clarity module ctor");
        c.cls = keepClass(env, cls);
    }
    jclass worldRender = clarityClass(env, "need.clarity.WorldRenderEvent");
    if (worldRender) {
        g_clWorldRender = keepClass(env, worldRender);
        g_clWorldRenderFire = env->GetStaticMethodID(worldRender, "fire",
            "(Lcom/mojang/blaze3d/vertex/PoseStack;F)V");
        clearException(env, "WorldRenderEvent.fire");
    }
    jclass perFrame = clarityClass(env, "need.clarity.PerFrameEvents");
    if (perFrame) {
        g_clPerFrame = keepClass(env, perFrame);
        g_clPerFrameFire = env->GetStaticMethodID(perFrame, "fire", "()V");
        clearException(env, "PerFrameEvents.fire");
    }
    jclass poseStack = clarityClass(env, "com.mojang.blaze3d.vertex.PoseStack");
    if (poseStack) {
        g_clPoseStack = keepClass(env, poseStack);
        g_clPoseStackInit = env->GetMethodID(poseStack, "<init>", "()V");
        clearException(env, "PoseStack ctor");
    }
}

// Where the jar lives. First existing file wins, in this order:
//   1. the caller's explicit path, 2. %PASSIVE_CLARITY_JAR%,
//   3. beside passive_agent.dll (the usual install), 4. the game's working directory and its
//      mods\ folder, 5. %APPDATA%\Passive.
std::string clarityJarPath(const char* custom) {
    if (custom && custom[0]) return custom;

    std::vector<std::string> roots;
    char env[MAX_PATH];
    // GetEnvironmentVariableA leaves the buffer untouched when the value does not fit, so the
    // result must be bounded before the buffer is read as a string.
    DWORD envLen = GetEnvironmentVariableA("PASSIVE_CLARITY_JAR", env, MAX_PATH);
    if (envLen > 0 && envLen < MAX_PATH) roots.emplace_back(env);

    HMODULE self = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&clarityJarPath), &self)) {
        char dll[MAX_PATH]{};
        if (GetModuleFileNameA(self, dll, MAX_PATH)) {
            std::string dir(dll);
            const size_t slash = dir.find_last_of("\\/");
            if (slash != std::string::npos) roots.push_back(dir.substr(0, slash + 1));
        }
    }
    roots.emplace_back("");   // the game's working directory
    envLen = GetEnvironmentVariableA("APPDATA", env, MAX_PATH);
    if (envLen > 0 && envLen < MAX_PATH) roots.push_back(std::string(env) + "\\Passive\\");

    static const char* const kNames[] = {
        "clarity-1.2.0.jar", "clarity.jar",
        "mods\\clarity-1.2.0.jar", "mods\\clarity.jar",
    };
    for (const std::string& root : roots) {
        for (const char* name : kNames) {
            const std::string candidate = root + name;
            const DWORD attrs = GetFileAttributesA(candidate.c_str());
            if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY))
                return candidate;
        }
    }
    return {};
}

} // namespace

// External linkage: detach() early in this file drops the cached references before the JVM is
// released. Everything it touches is declared above in the anonymous namespace.
void releaseClarityRefs(JNIEnv* env) {
    if (!env) return;
    for (ClarityToggle& c : g_clToggles) {
        if (c.cls) env->DeleteGlobalRef(c.cls);
        c.cls = nullptr; c.instance = nullptr; c.ctor = nullptr;
    }
    jclass* const refs[] = { &g_clModuleBase, &g_clWorldRender, &g_clPerFrame, &g_clPoseStack };
    for (jclass* r : refs) { if (*r) env->DeleteGlobalRef(*r); *r = nullptr; }
    g_clSetEnabled = nullptr; g_clWorldRenderFire = nullptr;
    g_clPerFrameFire = nullptr; g_clPoseStackInit = nullptr;
    g_clBound = false;
}

bool clarityAvailable() { return g_clarityLoaded; }

bool injectClarityJar(const char* customJarPath) {
    if (g_clarityLoaded) return true;
    JNIEnv* env = threadEnv();
    if (!env || !g_classLoader || !g_loadClassMid) return false;

    const std::string jarPath = clarityJarPath(customJarPath);
    if (jarPath.empty()) {
        pushError("clarity: jar not found (the cosmetic modules stay inert)");
        trace::log("clarity: no jar found - expected clarity-1.2.0.jar beside passive_agent.dll, "
                   "in the game directory, or in %%APPDATA%%\\Passive (set PASSIVE_CLARITY_JAR to override)");
        return false;
    }
    trace::log("clarity: loading %s", jarPath.c_str());

    // File -> URI -> URL, then a URLClassLoader parented on the game's own loader.
    jclass fileCls = env->FindClass("java/io/File");
    jmethodID fileInit = fileCls ? env->GetMethodID(fileCls, "<init>", "(Ljava/lang/String;)V") : nullptr;
    jmethodID toURIMid = fileCls ? env->GetMethodID(fileCls, "toURI", "()Ljava/net/URI;") : nullptr;
    if (!fileInit || !toURIMid) { clearException(env, "clarity File"); return false; }

    jstring jpath = env->NewStringUTF(jarPath.c_str());
    jobject fileObj = env->NewObject(fileCls, fileInit, jpath);
    env->DeleteLocalRef(jpath);
    jobject uriObj = fileObj ? env->CallObjectMethod(fileObj, toURIMid) : nullptr;
    if (fileObj) env->DeleteLocalRef(fileObj);
    if (!uriObj) { clearException(env, "clarity toURI"); return false; }

    jclass uriCls = env->FindClass("java/net/URI");
    jmethodID toURLMid = uriCls ? env->GetMethodID(uriCls, "toURL", "()Ljava/net/URL;") : nullptr;
    jobject urlObj = toURLMid ? env->CallObjectMethod(uriObj, toURLMid) : nullptr;
    env->DeleteLocalRef(uriObj);
    if (!urlObj) { clearException(env, "clarity toURL"); return false; }

    jclass urlCls = env->FindClass("java/net/URL");
    jclass urlLoaderCls = env->FindClass("java/net/URLClassLoader");
    jobjectArray urls = urlCls ? env->NewObjectArray(1, urlCls, urlObj) : nullptr;
    env->DeleteLocalRef(urlObj);
    jmethodID loaderInit = urlLoaderCls
        ? env->GetMethodID(urlLoaderCls, "<init>", "([Ljava/net/URL;Ljava/lang/ClassLoader;)V") : nullptr;
    if (!urls || !loaderInit) { clearException(env, "clarity URLClassLoader"); return false; }

    jobject loaderObj = env->NewObject(urlLoaderCls, loaderInit, urls, g_classLoader);
    env->DeleteLocalRef(urls);
    if (!loaderObj) { clearException(env, "clarity loader"); return false; }

    if (g_clarityClassLoader) env->DeleteGlobalRef(g_clarityClassLoader);
    g_clarityClassLoader = env->NewGlobalRef(loaderObj);
    env->DeleteLocalRef(loaderObj);
    if (!g_clarityClassLoader) { clearException(env, "clarity loader ref"); return false; }

    // URLClassLoader.loadClass, called through the instance (ClassLoader.loadClass is protected).
    jmethodID loadClassMid = env->GetMethodID(urlLoaderCls, "loadClass",
                                             "(Ljava/lang/String;)Ljava/lang/Class;");
    if (!loadClassMid) { clearException(env, "clarity loadClass"); return false; }

    // The jar resolves its own bundled assets through the calling thread's context loader. Point
    // that thread at the child loader for the boot sequence and keep the game's loader to hand,
    // so a jar that turns out to be unusable leaves the thread exactly as it found it.
    jobject prevCcl = nullptr;
    jmethodID getCclMid = nullptr, setCclMid = nullptr;
    jobject curThread = nullptr;
    if (jclass threadCls = env->FindClass("java/lang/Thread")) {
        jmethodID currentThreadMid = env->GetStaticMethodID(threadCls, "currentThread", "()Ljava/lang/Thread;");
        getCclMid = env->GetMethodID(threadCls, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
        setCclMid = env->GetMethodID(threadCls, "setContextClassLoader", "(Ljava/lang/ClassLoader;)V");
        clearException(env, "clarity Thread methods");
        if (currentThreadMid && getCclMid && setCclMid) {
            curThread = env->CallStaticObjectMethod(threadCls, currentThreadMid);
            if (curThread) {
                prevCcl = env->CallObjectMethod(curThread, getCclMid);
                env->CallVoidMethod(curThread, setCclMid, g_clarityClassLoader);
                clearException(env, "clarity setContextClassLoader");
            }
        }
    }

    auto loadWithClarity = [&](const char* dotted) -> jclass {
        jstring name = env->NewStringUTF(dotted);
        jclass c = static_cast<jclass>(env->CallObjectMethod(g_clarityClassLoader, loadClassMid, name));
        env->DeleteLocalRef(name);
        if (env->ExceptionCheck()) { clearException(env, "clarity class load"); return nullptr; }
        return c;
    };

    // The jar's own bootstrap, then its module registry, then its entry point. Each step is
    // optional: a jar missing one of them still gets as far as it can.
    if (jclass boot = loadWithClarity("need.clarity.sec.Bootstrap")) {
        if (jmethodID init = env->GetStaticMethodID(boot, "init", "()V"))
            env->CallStaticVoidMethod(boot, init);
        clearException(env, "Bootstrap.init");
        env->DeleteLocalRef(boot);
    }
    if (jclass modBoot = loadWithClarity("need.clarity.sec.ModuleBootstrap")) {
        if (jmethodID loadRemote = env->GetStaticMethodID(modBoot, "loadRemoteModules", "()V"))
            env->CallStaticVoidMethod(modBoot, loadRemote);
        clearException(env, "ModuleBootstrap.loadRemoteModules");
        env->DeleteLocalRef(modBoot);
    }

    bool started = false;
    if (jclass main = loadWithClarity("need.clarity.Clarity")) {
        if (jmethodID ctor = env->GetMethodID(main, "<init>", "()V")) {
            if (jmethodID onInit = env->GetMethodID(main, "onInitialize", "()V")) {
                if (jobject inst = env->NewObject(main, ctor)) {
                    env->CallVoidMethod(inst, onInit);
                    env->DeleteLocalRef(inst);
                    started = true;
                }
            }
        }
        clearException(env, "Clarity.onInitialize");
        env->DeleteLocalRef(main);
    }

    if (!started && curThread && setCclMid) {
        env->CallVoidMethod(curThread, setCclMid, prevCcl);
        clearException(env, "clarity restore context loader");
    }
    if (curThread) env->DeleteLocalRef(curThread);
    if (prevCcl) env->DeleteLocalRef(prevCcl);

    g_clarityLoaded = started;
    if (started) trace::log("clarity: %s initialised - the cosmetic modules can drive it", jarPath.c_str());
    else         trace::log("clarity: %s loaded but need.clarity.Clarity did not initialise", jarPath.c_str());
    return started;
}

void updateClarityVisuals(float tickDelta, const ClarityToggles& toggles) {
    if (!g_clarityLoaded) return;
    JNIEnv* env = threadEnv();
    if (!env) return;
    bindClarity(env);

    const bool enabled[kClarityToggleCount] = {
        toggles.playerModel, toggles.wings, toggles.headwear, toggles.capes, toggles.viewModel
    };
    if (g_clSetEnabled) {
        for (int i = 0; i < kClarityToggleCount; ++i) {
            ClarityToggle& c = g_clToggles[i];
            if (!c.cls || !c.instance) continue;
            jobject inst = env->GetStaticObjectField(c.cls, c.instance);
            if (env->ExceptionCheck()) { clearException(env, "clarity instance"); inst = nullptr; }
            if (!inst && c.ctor) {
                inst = env->NewObject(c.cls, c.ctor);
                if (env->ExceptionCheck()) { clearException(env, "clarity instance ctor"); inst = nullptr; }
            }
            if (!inst) continue;
            env->CallVoidMethod(inst, g_clSetEnabled, enabled[i] ? JNI_TRUE : JNI_FALSE);
            clearException(env, "ModuleBase.setEnabled");
            env->DeleteLocalRef(inst);
        }
    }

    // The jar's render hooks are event driven, so a frame without these two calls draws nothing.
    jobject pose = nullptr;
    if (g_clPoseStack && g_clPoseStackInit) {
        pose = env->NewObject(g_clPoseStack, g_clPoseStackInit);
        if (env->ExceptionCheck()) { clearException(env, "PoseStack ctor"); pose = nullptr; }
    }
    if (g_clWorldRender && g_clWorldRenderFire) {
        env->CallStaticVoidMethod(g_clWorldRender, g_clWorldRenderFire, pose, (jfloat)tickDelta);
        clearException(env, "WorldRenderEvent.fire");
    }
    if (pose) env->DeleteLocalRef(pose);
    if (g_clPerFrame && g_clPerFrameFire) {
        env->CallStaticVoidMethod(g_clPerFrame, g_clPerFrameFire);
        clearException(env, "PerFrameEvents.fire");
    }
}

} // namespace jni
} // namespace tyr
