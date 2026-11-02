#include "../agent/jni/silent_class_transform.h"
#include <jni.h>
#include <cstring>
#include <cstdio>
using namespace tyr::jni::silent_transform;
struct Ti { void** functions; };
template<class F> F fn(Ti* ti,int slot){return reinterpret_cast<F>(ti->functions[slot-1]);}
struct Caps { unsigned int w[4]{}; };
using Hook=void (JNICALL*)(Ti*,JNIEnv*,jclass,jobject,const char*,jobject,jint,const unsigned char*,jint*,unsigned char**);
struct Callbacks { void* reserved[4]{}; Hook hook{}; };
static Ti* ti=nullptr; static jclass entity=nullptr,player=nullptr,helper=nullptr; static bool enabled=false; static int mask=0;
static void JNICALL onClass(Ti* ti,JNIEnv* e,jclass cls,jobject,const char*,jobject,jint size,const unsigned char* data,jint* outSize,unsigned char** outData){
 if(!enabled || !cls) return; bool a=e->IsSameObject(cls,entity), b=e->IsSameObject(cls,player); if(!a&&!b)return;
 Bytes out; if(!transform(Bytes(data,data+size),a?"method_5724":"method_3136","tyrant/internal/SilentRotationHelper",b,out)){mask|=a?4:8;return;}
 using Allocate=int (JNICALL*)(Ti*,jlong,unsigned char**); unsigned char* mem=nullptr;
 if(fn<Allocate>(ti,46)(ti,out.size(),&mem)!=0)return; std::memcpy(mem,out.data(),out.size());*outSize=(jint)out.size();*outData=mem;mask|=a?1:2;
}
extern "C" JNIEXPORT jboolean JNICALL Java_silent_1rotation_1jvm_1check_install(JNIEnv* e,jclass){
 JavaVM* vm=nullptr;e->GetJavaVM(&vm);if(vm->GetEnv((void**)&ti,0x30010200)!=JNI_OK)return false;
 Caps caps{};caps.w[1]=1u<<5;using Capability=int (JNICALL*)(Ti*,Caps*);if(fn<Capability>(ti,142)(ti,&caps))return false;
 entity=(jclass)e->NewGlobalRef(e->FindClass("net/minecraft/class_1297"));player=(jclass)e->NewGlobalRef(e->FindClass("net/minecraft/class_746"));helper=(jclass)e->NewGlobalRef(e->FindClass("tyrant/internal/SilentRotationHelper"));
 if(e->ExceptionCheck())return false;
 Callbacks cb{};cb.hook=onClass;using SetCB=int (JNICALL*)(Ti*,Callbacks*,jint);using Event=int (JNICALL*)(Ti*,jint,jint,jobject,...);using Retrans=int (JNICALL*)(Ti*,jint,const jclass*);
 if(fn<SetCB>(ti,122)(ti,&cb,sizeof(cb)) || fn<Event>(ti,2)(ti,1,54,nullptr))return false;
 jclass classes[]{entity,player};enabled=true;int err=fn<Retrans>(ti,152)(ti,2,classes);enabled=false;
 if(err||mask!=3)std::printf("retransform error=%d mask=%d\n",err,mask);
 return err==0&&mask==3;
}
extern "C" JNIEXPORT void JNICALL Java_silent_1rotation_1jvm_1check_update(JNIEnv* e,jclass,jboolean active,jfloat yaw,jfloat pitch){
 e->SetStaticFloatField(helper,e->GetStaticFieldID(helper,"yaw","F"),yaw);e->SetStaticFloatField(helper,e->GetStaticFieldID(helper,"pitch","F"),pitch);e->SetStaticBooleanField(helper,e->GetStaticFieldID(helper,"active","Z"),active);
}
extern "C" JNIEXPORT jboolean JNICALL Java_silent_1rotation_1jvm_1check_restore(JNIEnv*,jclass){
 using Retrans=int (JNICALL*)(Ti*,jint,const jclass*);jclass classes[]{entity,player};return fn<Retrans>(ti,152)(ti,2,classes)==0;
}
