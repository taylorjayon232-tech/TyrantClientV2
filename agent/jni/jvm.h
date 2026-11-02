// ============================================================================
//  Passive — JNI foundations (declarations)
// ============================================================================
#pragma once
#include <jni.h>
#include <string>
#include <vector>

namespace tyr {
namespace jni {

bool     init();
void     shutdown();   // full teardown (only if we owned the VM — we never do)
void     detach();     // safe unload: DetachCurrentThread only, drop global refs
JavaVM*  vm();
JNIEnv*  env();
// The class loader the game's own classes came from (a global reference, or null
// before the hunt succeeds). Its getResourceAsStream reaches the game jar's
// assets, which the system loader may not see at all.
jobject  classLoader();
bool     ready();
void     ensureClassLoader(); // probe-then-hunt: guarantee findClass sees net.minecraft.*


// ---- Clarity cosmetics bridge --------------------------------------------------------------
// The five visual modules (Player Model / Wings / Headwear / Capes / ViewModel) have no native
// renderer: they are drawn by the Clarity Java mod, loaded at boot from a jar that must sit
// beside the agent. When that jar is missing the bridge stays disabled and says so in the log;
// the module cards still work, they just have nothing to drive.
struct ClarityToggles {
    bool playerModel = false;
    bool wings       = false;
    bool headwear    = false;
    bool capes       = false;
    bool viewModel   = false;
    bool any() const { return playerModel || wings || headwear || capes || viewModel; }
};
bool injectClarityJar(const char* jarPath = nullptr);   // false when no jar could be found
bool clarityAvailable();
void updateClarityVisuals(float tickDelta, const ClarityToggles& toggles);

void pushError(const char* what);
std::vector<std::string> drainErrors();
void clearException(JNIEnv* env, const char* where);

jclass    findClass(JNIEnv* env, const char* dottedName);
int       failedLookups();   // count of failed JNI id lookups (0 = all symbols resolved)
int       totalLookups();    // count of ALL JNI id lookups attempted (for diagnostics)
jfieldID  field  (JNIEnv* env, jclass cls, const char* name, const char* sig);
jfieldID  sfield (JNIEnv* env, jclass cls, const char* name, const char* sig);
jmethodID method (JNIEnv* env, jclass cls, const char* name, const char* sig);
jmethodID smethod(JNIEnv* env, jclass cls, const char* name, const char* sig);

jobject  getObj (JNIEnv* env, jobject obj, jfieldID fid);
jobject  sgetObj(JNIEnv* env, jclass cls, jfieldID fid);
jfloat   getFloat (JNIEnv* env, jobject obj, jfieldID fid, float   fallback = 0.f);
jint     getInt   (JNIEnv* env, jobject obj, jfieldID fid, jint    fallback = 0);
jdouble  getDouble(JNIEnv* env, jobject obj, jfieldID fid, jdouble fallback = 0.0);
jboolean getBool  (JNIEnv* env, jobject obj, jfieldID fid, jboolean fallback = JNI_FALSE);

jstring     toJString(JNIEnv* env, const char* s);
std::string fromJString(JNIEnv* env, jstring js);

} // namespace jni
} // namespace tyr
