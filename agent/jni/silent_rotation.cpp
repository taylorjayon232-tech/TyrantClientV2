#include "silent_rotation.h"
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

// Java 21 classfile for a self-contained, loader-local helper. Its methods use reflection only;
// they have no native methods, so even an obsolete frame can never call into an unloaded DLL.
// The embedded bytes are generated from SilentRotationHelper.java with JDK 21.
#include "silent_rotation_helper.inc"

void JNICALL onClass(Ti* ti, JNIEnv* e, jclass klass, jobject, const char*, jobject,
                     jint size, const unsigned char* data, jint* newSize, unsigned char** newData) {
    if (!g_transformEnabled.load(std::memory_order_acquire) || !klass || size<=0 || !data) return;
    bool entity=e->IsSameObject(klass,mc::sym().entity)==JNI_TRUE;
    bool player=e->IsSameObject(klass,mc::sym().clientPlayer)==JNI_TRUE;
    if (!entity && !player) return;
    try {
        silent_transform::Bytes in(data,data+size), out;
        if (!silent_transform::transform(in,entity?"method_5724":"method_3136",
                                         "tyrant/internal/SilentRotationHelper",player,out)) {
            g_transformMask.fetch_or(entity?4:8,std::memory_order_relaxed); return;
        }
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
    g_transformMask.store(0); g_transformEnabled.store(true,std::memory_order_release);
    Error err=tiFn<Retransform>(ti,152)(ti,2,classes);
    g_transformEnabled.store(false,std::memory_order_release);
    if(err!=0 || g_transformMask.load()!=3) {
        trace::log("silent aim: class transformation failed (error=%d mask=%d)",int(err),g_transformMask.load());
        if(err==0) tiFn<Retransform>(ti,152)(ti,2,classes);
        tiFn<SetEvent>(ti,2)(ti,0,54,nullptr);
        tiFn<Error (JNICALL*)(Ti*)>(ti,127)(ti);
        e->DeleteGlobalRef(g_helper); g_helper=nullptr; return false;
    }
    g_ti=ti; trace::log("silent aim: movement and packet hooks installed"); return true;
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
    if(JNIEnv* e=env()) e->DeleteGlobalRef(g_helper);
    g_helper=nullptr; g_ti=nullptr;
}
} // namespace tyr::jni
