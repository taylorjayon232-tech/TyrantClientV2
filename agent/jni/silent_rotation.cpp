#include "silent_rotation.h"
#include "ids.h"
#include "silent_class_transform.h"
#include "jvm.h"
#include "mc.h"
#include "../core/agent.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

namespace tyr::jni {
namespace {
// JVMTI's documented interface table is ABI-stable. The project vendors JNI headers but not
// jvmti.h, so declare only the table entries used here rather than depend on a machine-local JDK.
struct Ti { void** functions; };
struct Capabilities { unsigned int words[4]{}; };
template<class F> F tiFn(Ti* ti, int slot) { return reinterpret_cast<F>(ti->functions[slot-1]); }
using Error = jint;
using ClassHook = void (JNICALL*)(Ti*, JNIEnv*, jclass, jobject, const char*, jobject, jint,
                                  const unsigned char*, jint*, unsigned char**);
struct Callbacks { void* before[4]{}; ClassHook classFileLoad{}; };
Ti* g_ti=nullptr;
jclass g_helper=nullptr;
jfieldID g_active=nullptr, g_yaw=nullptr, g_pitch=nullptr;
std::atomic<bool> g_transformEnabled{false};
std::atomic<int> g_transformMask{0};
std::mutex g_mutex;
// Set once an install attempt fails. Retrying every frame re-runs the class transform on the game
// thread each frame, which is what dropped the FPS; a failed install stays failed for this session.
bool g_installFailed=false;
// Pick-level targeting. The game's crosshair pick is computed inside ClientPlayerEntity when the
// renderer asks for it, so it is replaced there: an extra class with one native method is the call
// target, and the retransform swaps the class's own checkCrosshairTargetRange call for it.
jclass g_pickClass=nullptr;
jmethodID g_getCrosshair=nullptr;
jmethodID g_pickFull=nullptr;          // method_76763(Entity, double, double, float) -> HitResult
jfieldID g_renderYaw=nullptr, g_renderPitch=nullptr, g_lastRenderYaw=nullptr, g_lastRenderPitch=nullptr;
jfieldID g_targetedEntity=nullptr;
jobject g_cameraPick=nullptr;      // global ref: the game's own pick for this frame
std::atomic<bool> g_pickReady{false};
std::atomic<bool> g_pickMask{false};     // the transform swapped the pick call
std::atomic<int>  g_pickKind{0};         // 1 = full pick, 2 = range-check fallback
std::atomic<bool> g_pickPatched{false};  // ...and it is installed in the live game classes
// Set while the helper calls the game's own pick again; the nested call must keep its result.
thread_local bool g_inPick=false;

// Java 21 classfile for a self-contained, loader-local helper. Its methods use reflection only;
// they have no native methods, so even an obsolete frame can never call into an unloaded DLL.
// The embedded bytes are generated from SilentRotationHelper.java with JDK 21.
#include "silent_rotation_helper.inc"

// The pick helper is built at runtime: two public static native methods whose descriptors are exactly
// the ones of the game calls they replace, so the retransformed class can call them with the arguments
// it already has on the stack. `pickFull` takes the receiver (the player) first, which is the shape of
// the method_76763 call inside the pick method; `pick` matches the older checkCrosshairTargetRange
// patch and stays as the fallback. Nothing is compiled ahead of time, so it needs no JDK on the build
// machine.
silent_transform::Bytes pickHelperClass() {
    using namespace silent_transform;
    Bytes b;
    b.push_back(0xCA); b.push_back(0xFE); b.push_back(0xBA); b.push_back(0xBE);
    put2(b, 0); put2(b, 61);                        // minor, major (Java 17; the game runs 21)
    put2(b, 9);                                      // constant pool count
    putUtf(b, "tyrant/internal/SilentPickHelper");   // 1
    b.push_back(7); put2(b, 1);                      // 2 Class #1
    putUtf(b, "java/lang/Object");                   // 3
    b.push_back(7); put2(b, 3);                      // 4 Class #3
    putUtf(b, "pick");                               // 5
    putUtf(b, "(Lnet/minecraft/class_239;Lnet/minecraft/class_243;D)Lnet/minecraft/class_239;"); // 6
    putUtf(b, "pickFull");                           // 7
    putUtf(b, "(Lnet/minecraft/class_746;Lnet/minecraft/class_1297;DDF)Lnet/minecraft/class_239;"); // 8
    put2(b, 0x0031);                                 // public final super
    put2(b, 2); put2(b, 4);                          // this, super
    put2(b, 0); put2(b, 0);                          // interfaces, fields
    put2(b, 2);                                      // methods
    put2(b, 0x0109); put2(b, 5); put2(b, 6); put2(b, 0);   // public static native pick
    put2(b, 0x0109); put2(b, 7); put2(b, 8); put2(b, 0);   // public static native pickFull
    put2(b, 0);                                      // class attributes
    return b;
}

// Called from inside the game's own crosshair computation. The pick the game already made (and range
// checked) is kept for the camera logic, then the same computation is repeated with
// renderYaw/renderPitch set to the silent rotation: that is the pick the aim would produce, and it is
// what the game stores, so the HUD indicator, the block outline and doAttack/doItemUse all see it in
// the same frame. The four fields are restored before returning, so the camera never moves.
jobject JNICALL pickCrosshairNative(JNIEnv* e, jclass, jobject hit, jobject cameraPos, jdouble range) {
    (void)cameraPos; (void)range;
    if (!e) return hit;
    // Nested call: the recomputation below is already running with the aim in the fields, so the
    // range check inside it must keep what it computed.
    if (g_inPick) return hit;
    if (!g_helper || !mc::sym().minecraft || !mc::sym().mc_getInstance || !mc::sym().mc_player) return hit;
    mc::LocalRef client(e->CallStaticObjectMethod(mc::sym().minecraft, mc::sym().mc_getInstance));
    if (e->ExceptionCheck()) { jni::clearException(e, "pick getInstance"); return hit; }
    if (!client) return hit;
    mc::LocalRef player(e->GetObjectField(client.get(), mc::sym().mc_player));
    if (e->ExceptionCheck()) { jni::clearException(e, "pick player"); return hit; }
    if (!player) return hit;
    // The camera's own pick is what decides whether a press is a block action; the field no longer
    // holds it while the aim is on, so this frame's copy is kept here.
    if (g_cameraPick) e->DeleteGlobalRef(g_cameraPick);
    g_cameraPick = e->NewGlobalRef(hit);
    if (e->ExceptionCheck()) jni::clearException(e, "pick cache");
    if (e->GetStaticBooleanField(g_helper, g_active) != JNI_TRUE || !g_getCrosshair ||
        !g_renderYaw || !g_renderPitch || !g_lastRenderYaw || !g_lastRenderPitch) return hit;
    const float yaw = e->GetStaticFloatField(g_helper, g_yaw);
    const float pitch = e->GetStaticFloatField(g_helper, g_pitch);
    const float was[4] = {
        e->GetFloatField(player.get(), g_renderYaw), e->GetFloatField(player.get(), g_renderPitch),
        e->GetFloatField(player.get(), g_lastRenderYaw), e->GetFloatField(player.get(), g_lastRenderPitch)};
    e->SetFloatField(player.get(), g_renderYaw, yaw);
    e->SetFloatField(player.get(), g_renderPitch, pitch);
    e->SetFloatField(player.get(), g_lastRenderYaw, yaw);
    e->SetFloatField(player.get(), g_lastRenderPitch, pitch);
    g_inPick = true;
    jobject aim = e->CallObjectMethod(player.get(), g_getCrosshair, jfloat(1.f), player.get());
    g_inPick = false;
    const bool failed = e->ExceptionCheck() != JNI_FALSE;
    if (failed) jni::clearException(e, "pick recompute");
    e->SetFloatField(player.get(), g_renderYaw, was[0]);
    e->SetFloatField(player.get(), g_renderPitch, was[1]);
    e->SetFloatField(player.get(), g_lastRenderYaw, was[2]);
    e->SetFloatField(player.get(), g_lastRenderPitch, was[3]);
    if (e->ExceptionCheck()) jni::clearException(e, "pick restore");
    if (failed || !aim) return hit;
    // The HUD's target state is the pick's entity, set in the same frame the pick is made so it can
    // never be one frame behind or left over from the camera pick.
    if (g_targetedEntity) {
        jobject target = nullptr;
        if (mc::hit::typeOf(aim) == 2) target = mc::hit::entity(aim);
        e->SetObjectField(client.get(), g_targetedEntity, target);
        if (e->ExceptionCheck()) jni::clearException(e, "pick target entity");
        if (target) e->DeleteLocalRef(target);
    }
    return aim;
}

// The full pick. ClientPlayerEntity.method_76762 always calls method_76763(Entity, double, double,
// float) to compute the crosshair target, so the transform swaps that call for this one. It receives
// everything the game already resolved - the player, the camera entity and the two interaction ranges
// - which means the hook runs on every frame and every view, not only when the camera's own ray
// already found an entity. That was the hole: aiming at a player while looking at a wall never reached
// the old range-check hook, so the game kept the wall's block pick and its outline stayed on.
//
// The camera's pick is computed first and kept for the block-action decision. The same computation is
// then repeated with renderYaw/renderPitch set to the silent rotation; the game stores that result as
// its own crosshairTarget, so the HUD attack indicator, the block outline, doAttack and doItemUse all
// read the aim in the same frame. The four fields are restored before returning, so the camera never
// moves.
jobject JNICALL pickFullNative(JNIEnv* e, jclass, jobject player, jobject camera,
                               jdouble blockRange, jdouble entityRange, jfloat tickProgress) {
    if (!e || !player || !g_pickFull) return nullptr;
    jvalue args[4];
    args[0].l = camera;
    args[1].d = blockRange;
    args[2].d = entityRange;
    args[3].f = tickProgress;
    if (g_inPick) {   // reentered from the recomputation: keep it plain, with no second swap
        jobject plain = e->CallObjectMethodA(player, g_pickFull, args);
        if (e->ExceptionCheck()) { jni::clearException(e, "pick reentrant"); return nullptr; }
        return plain;
    }
    jobject cameraPick = e->CallObjectMethodA(player, g_pickFull, args);
    if (e->ExceptionCheck()) { jni::clearException(e, "pick camera"); return nullptr; }
    if (cameraPick) {
        if (g_cameraPick) e->DeleteGlobalRef(g_cameraPick);
        g_cameraPick = e->NewGlobalRef(cameraPick);
        if (e->ExceptionCheck()) jni::clearException(e, "camera pick cache");
    }
    if (!g_helper || e->GetStaticBooleanField(g_helper, g_active) != JNI_TRUE ||
        !g_renderYaw || !g_renderPitch || !g_lastRenderYaw || !g_lastRenderPitch) return cameraPick;
    const float yaw = e->GetStaticFloatField(g_helper, g_yaw);
    const float pitch = e->GetStaticFloatField(g_helper, g_pitch);
    const float was[4] = {
        e->GetFloatField(player, g_renderYaw), e->GetFloatField(player, g_renderPitch),
        e->GetFloatField(player, g_lastRenderYaw), e->GetFloatField(player, g_lastRenderPitch)};
    e->SetFloatField(player, g_renderYaw, yaw);
    e->SetFloatField(player, g_renderPitch, pitch);
    e->SetFloatField(player, g_lastRenderYaw, yaw);
    e->SetFloatField(player, g_lastRenderPitch, pitch);
    g_inPick = true;
    jobject aim = e->CallObjectMethodA(player, g_pickFull, args);
    g_inPick = false;
    const bool failed = e->ExceptionCheck() != JNI_FALSE;
    if (failed) jni::clearException(e, "pick aim");
    e->SetFloatField(player, g_renderYaw, was[0]);
    e->SetFloatField(player, g_renderPitch, was[1]);
    e->SetFloatField(player, g_lastRenderYaw, was[2]);
    e->SetFloatField(player, g_lastRenderPitch, was[3]);
    if (e->ExceptionCheck()) jni::clearException(e, "pick restore");
    if (failed || !aim) return cameraPick;
    // The HUD's target state comes from this field, so it is written in the same call that produced
    // the pick: it can never be a frame behind, and it is cleared when the aim's pick is not an entity.
    if (mc::sym().minecraft && mc::sym().mc_getInstance && g_targetedEntity) {
        mc::LocalRef client(e->CallStaticObjectMethod(mc::sym().minecraft, mc::sym().mc_getInstance));
        if (e->ExceptionCheck()) jni::clearException(e, "pick client");
        else if (client) {
            jobject target = nullptr;
            if (mc::hit::typeOf(aim) == 2) target = mc::hit::entity(aim);
            e->SetObjectField(client.get(), g_targetedEntity, target);
            if (e->ExceptionCheck()) jni::clearException(e, "pick target entity");
            if (target) e->DeleteLocalRef(target);
        }
    }
    if (cameraPick) e->DeleteLocalRef(cameraPick);
    return aim;
}

void JNICALL onClass(Ti* ti, JNIEnv* e, jclass klass, jobject, const char*, jobject,
                     jint size, const unsigned char* data, jint* newSize, unsigned char** newData) {
    if (!g_transformEnabled.load(std::memory_order_acquire) || !klass || size<=0 || !data) return;
    bool entity=e->IsSameObject(klass,mc::sym().entity)==JNI_TRUE;
    bool player=e->IsSameObject(klass,mc::sym().clientPlayer)==JNI_TRUE;
    if (!entity && !player) return;
    try {
        silent_transform::Bytes in(data,data+size), out;
        bool pick=false; int pickKind=0;
        if (!silent_transform::transform(in,entity?"method_5724":"method_3136",
                                         "tyrant/internal/SilentRotationHelper",player,out,
                                         player && g_pickReady.load(std::memory_order_acquire), &pick,
                                         &pickKind)) {
            g_transformMask.fetch_or(entity?4:8,std::memory_order_relaxed); return;
        }
        if (pick) { g_pickMask.store(true,std::memory_order_relaxed);
                    g_pickKind.store(pickKind,std::memory_order_relaxed); }
        using Allocate=Error (JNICALL*)(Ti*,jlong,unsigned char**);
        unsigned char* bytes=nullptr;
        if(tiFn<Allocate>(ti,46)(ti,jlong(out.size()),&bytes)!=0 || !bytes) return;
        std::memcpy(bytes,out.data(),out.size()); *newSize=jint(out.size()); *newData=bytes;
        g_transformMask.fetch_or(entity?1:2,std::memory_order_relaxed);
    } catch (...) {
        g_transformMask.fetch_or(entity?4:8,std::memory_order_relaxed);
    }
}

bool install(JNIEnv* e) {
    if (!vm() || !mc::sym().entity || !mc::sym().clientPlayer || !classLoader()) return false;
    Ti* ti=nullptr;
    if(vm()->GetEnv(reinterpret_cast<void**>(&ti),0x30010200)!=JNI_OK || !ti) return false;
    using Caps=Error (JNICALL*)(Ti*,Capabilities*);
    Capabilities potential{}, want{}; want.words[1]=1u<<5;
    if(tiFn<Caps>(ti,140)(ti,&potential)!=0 || !(potential.words[1]&want.words[1]) ||
       tiFn<Caps>(ti,142)(ti,&want)!=0) {
        trace::log("silent aim: JVM class retransformation unavailable");
        tiFn<Error (JNICALL*)(Ti*)>(ti,127)(ti); return false;
    }
    jclass helper=findClass(e,"passive.internal.SilentRotationHelper");
    if(!helper) {
        helper=e->DefineClass("tyrant/internal/SilentRotationHelper",classLoader(),
                              reinterpret_cast<const jbyte*>(kSilentRotationHelper),jint(sizeof(kSilentRotationHelper)));
        if(e->ExceptionCheck()) { clearException(e,"silent helper class"); helper=nullptr; }
    }
    if(!helper) { tiFn<Error (JNICALL*)(Ti*)>(ti,127)(ti); return false; }
    g_helper=static_cast<jclass>(e->NewGlobalRef(helper)); e->DeleteLocalRef(helper);
    if (!g_helper) { clearException(e,"silent helper reference"); tiFn<Error (JNICALL*)(Ti*)>(ti,127)(ti); return false; }
    g_active=e->GetStaticFieldID(g_helper,"active","Z");
    g_yaw=e->GetStaticFieldID(g_helper,"yaw","F");
    g_pitch=e->GetStaticFieldID(g_helper,"pitch","F");
    if(e->ExceptionCheck() || !g_active || !g_yaw || !g_pitch) {
        clearException(e,"silent helper fields"); e->DeleteGlobalRef(g_helper); g_helper=nullptr;
        tiFn<Error (JNICALL*)(Ti*)>(ti,127)(ti); return false;
    }
    // Pick-level targeting is best-effort: when the class cannot be built the aim keeps the
    // field-write behaviour instead of taking the movement/packet transform down with it.
    if (!g_pickClass) {
        jclass pickCls=findClass(e,"tyrant.internal.SilentPickHelper");
        if (!pickCls) {
            silent_transform::Bytes bytes=pickHelperClass();
            pickCls=e->DefineClass("tyrant/internal/SilentPickHelper",classLoader(),
                                   reinterpret_cast<const jbyte*>(bytes.data()),jint(bytes.size()));
            if(e->ExceptionCheck()) { clearException(e,"silent pick class"); pickCls=nullptr; }
        }
        if (pickCls) {
            JNINativeMethod methods[2]={
                {const_cast<char*>("pick"),
                 const_cast<char*>("(Lnet/minecraft/class_239;Lnet/minecraft/class_243;D)Lnet/minecraft/class_239;"),
                 reinterpret_cast<void*>(&pickCrosshairNative)},
                {const_cast<char*>("pickFull"),
                 const_cast<char*>("(Lnet/minecraft/class_746;Lnet/minecraft/class_1297;DDF)Lnet/minecraft/class_239;"),
                 reinterpret_cast<void*>(&pickFullNative)}};
            if(e->RegisterNatives(pickCls,methods,2)==0) g_pickClass=jclass(e->NewGlobalRef(pickCls));
            else clearException(e,"silent pick native");
            e->DeleteLocalRef(pickCls);
        }
        if (g_pickClass) {
            g_getCrosshair=e->GetMethodID(mc::sym().clientPlayer,"method_76762",
                "(FLnet/minecraft/class_1297;)Lnet/minecraft/class_239;");
            g_pickFull=e->GetMethodID(mc::sym().clientPlayer,"method_76763",
                "(Lnet/minecraft/class_1297;DDF)Lnet/minecraft/class_239;");
            g_renderYaw=e->GetFieldID(mc::sym().clientPlayer,"field_3932","F");
            g_renderPitch=e->GetFieldID(mc::sym().clientPlayer,"field_3916","F");
            g_lastRenderYaw=e->GetFieldID(mc::sym().clientPlayer,"field_3931","F");
            g_lastRenderPitch=e->GetFieldID(mc::sym().clientPlayer,"field_3914","F");
            g_targetedEntity=e->GetFieldID(mc::sym().minecraft,fld::kClient_targetedEntity,
                                           "Lnet/minecraft/class_1297;");
            if(e->ExceptionCheck() || !g_getCrosshair || !g_pickFull || !g_renderYaw || !g_renderPitch ||
               !g_lastRenderYaw || !g_lastRenderPitch) {
                clearException(e,"silent pick members");
                e->DeleteGlobalRef(g_pickClass); g_pickClass=nullptr;
            }
        }
        g_pickReady.store(g_pickClass!=nullptr);
    }
    Callbacks cb{}; cb.classFileLoad=onClass;
    using SetCallbacks=Error (JNICALL*)(Ti*,const Callbacks*,jint);
    using SetEvent=Error (JNICALL*)(Ti*,jint,jint,jobject,...);
    using Retransform=Error (JNICALL*)(Ti*,jint,const jclass*);
    if(tiFn<SetCallbacks>(ti,122)(ti,&cb,sizeof(cb))!=0 || tiFn<SetEvent>(ti,2)(ti,1,54,nullptr)!=0) {
        e->DeleteGlobalRef(g_helper); g_helper=nullptr; tiFn<Error (JNICALL*)(Ti*)>(ti,127)(ti); return false;
    }
    // Force initialization before changing any game class; a linkage error must not be left
    // waiting to crash the game on its next movement tick.
    e->GetStaticBooleanField(g_helper,g_active);
    if(e->ExceptionCheck()) {
        clearException(e,"silent helper initialization"); tiFn<SetEvent>(ti,2)(ti,0,54,nullptr);
        tiFn<Error (JNICALL*)(Ti*)>(ti,127)(ti); e->DeleteGlobalRef(g_helper); g_helper=nullptr; return false;
    }
    jclass classes[]{mc::sym().entity,mc::sym().clientPlayer};
    g_transformMask.store(0); g_pickMask.store(false); g_pickKind.store(0); g_transformEnabled.store(true,std::memory_order_release);
    Error err=tiFn<Retransform>(ti,152)(ti,2,classes);
    g_transformEnabled.store(false,std::memory_order_release);
    if(err!=0 || g_transformMask.load()!=3) {
        trace::log("silent aim: class transformation failed (error=%d mask=%d)",int(err),g_transformMask.load());
        if(err==0) tiFn<Retransform>(ti,152)(ti,2,classes);
        tiFn<SetEvent>(ti,2)(ti,0,54,nullptr);
        tiFn<Error (JNICALL*)(Ti*)>(ti,127)(ti);
        g_pickPatched.store(false); g_pickReady.store(false); g_pickMask.store(false); g_pickKind.store(0);
        e->DeleteGlobalRef(g_helper); g_helper=nullptr; return false;
    }
    g_pickPatched.store(g_pickReady.load() && g_pickMask.load(),std::memory_order_relaxed);
    g_ti=ti;
    trace::log(g_pickPatched.load()
        ? "silent aim: movement and packet hooks installed, pick-level targeting on"
        : "silent aim: movement and packet hooks installed (pick-level targeting unavailable)");
    return true;
}
} // namespace

bool silentRotationStart(float yaw,float pitch) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_installFailed) return false;
    JNIEnv* e=env(); if(!e) return false;
    if (!g_ti && !install(e)) { g_installFailed=true; return false; }
    e->SetStaticFloatField(g_helper,g_yaw,yaw); e->SetStaticFloatField(g_helper,g_pitch,pitch);
    e->SetStaticBooleanField(g_helper,g_active,JNI_TRUE);
    if(e->ExceptionCheck()) { clearException(e,"silent rotation start"); return false; }
    return true;
}
void silentRotationUpdate(float yaw,float pitch) {
    JNIEnv* e=env(); if(!e || !g_helper) return;
    e->SetStaticFloatField(g_helper,g_yaw,yaw); e->SetStaticFloatField(g_helper,g_pitch,pitch);
    // Re-asserting the flag is what resumes a session that was suspended for a world action: the
    // transform is still installed, so this is the whole resume - no reinstall, no extra packet.
    e->SetStaticBooleanField(g_helper,g_active,JNI_TRUE);
    clearException(e,"silent rotation update");
}
void silentRotationSuspend() {
    JNIEnv* e=env(); if(!e || !g_helper) return;
    e->SetStaticBooleanField(g_helper,g_active,JNI_FALSE);
    clearException(e,"silent rotation suspend");
}
void silentRotationStop() {
    JNIEnv* e=env(); if(!e || !g_helper) return;
    e->SetStaticBooleanField(g_helper,g_active,JNI_FALSE); clearException(e,"silent rotation stop");
}
void silentRotationShutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    silentRotationStop(); if(!g_ti) return;
    using Retransform=Error (JNICALL*)(Ti*,jint,const jclass*);
    using SetEvent=Error (JNICALL*)(Ti*,jint,jint,jobject,...);
    jclass classes[]{mc::sym().entity,mc::sym().clientPlayer};
    Error err=tiFn<Retransform>(g_ti,152)(g_ti,2,classes);
    if(err!=0) trace::log("silent aim: restoring game classes failed (%d); inactive Java helper remains",int(err));
    tiFn<SetEvent>(g_ti,2)(g_ti,0,54,nullptr);
    tiFn<Error (JNICALL*)(Ti*)>(g_ti,127)(g_ti);
    g_pickPatched.store(false); g_pickReady.store(false); g_pickMask.store(false); g_pickKind.store(0); g_inPick=false;
    if(JNIEnv* e=env()) {
        e->DeleteGlobalRef(g_helper);
        if(g_cameraPick) { e->DeleteGlobalRef(g_cameraPick); g_cameraPick=nullptr; }
        if(g_pickClass) { e->DeleteGlobalRef(g_pickClass); g_pickClass=nullptr; }
    }
    g_helper=nullptr; g_ti=nullptr;
}

bool silentAimPickActive() { return g_pickPatched.load(std::memory_order_acquire); }

// Which pick path the transform installed: 1 = the pick method's own raycast call, 2 = the old
// range-check fallback, 0 = neither (the crosshair field write is then all the build has).
int silentAimPickMode() {
    if (!g_pickPatched.load(std::memory_order_acquire)) return 0;
    return g_pickKind.load(std::memory_order_relaxed);
}

jobject silentAimCameraPick() {
    JNIEnv* e=env();
    if(!e || !g_cameraPick) return nullptr;
    jobject ref=e->NewLocalRef(g_cameraPick);
    jni::clearException(e,"camera pick ref");
    return ref;
}
} // namespace tyr::jni
