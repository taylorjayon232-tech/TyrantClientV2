#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace tyr::jni::silent_transform {
using Bytes = std::vector<uint8_t>;
inline uint16_t u2(const Bytes& b, size_t p) { return uint16_t((uint16_t(b[p]) << 8) | b[p+1]); }
inline uint32_t u4(const Bytes& b, size_t p) { return (uint32_t(u2(b,p)) << 16) | u2(b,p+2); }
inline void put2(Bytes& b, uint16_t v) { b.push_back(uint8_t(v >> 8)); b.push_back(uint8_t(v)); }
inline void putUtf(Bytes& b, const char* s) { b.push_back(1); put2(b, uint16_t(std::strlen(s))); b.insert(b.end(), s, s+std::strlen(s)); }
inline void putRef(Bytes& b, uint8_t tag, uint16_t a, uint16_t c) { b.push_back(tag); put2(b,a); put2(b,c); }
struct Cp { uint8_t tag=0; uint16_t a=0,b=0; std::string text; };

// Decode instruction boundaries rather than searching raw bytes (operands can resemble opcodes).
inline size_t instructionLength(const Bytes& b, size_t base, size_t pos, size_t end) {
    const uint8_t op=b[pos];
    if (op==0xaa || op==0xab) {
        size_t p=pos+1; while ((p-base)%4) ++p;
        if (p+8>end) return 0;
        if (op==0xaa) {
            if (p+12>end) return 0;
            int32_t low=int32_t(u4(b,p+4)), high=int32_t(u4(b,p+8));
            if (high<low || int64_t(high)-low > int64_t((end-p-12)/4)) return 0;
            return p+12+size_t(int64_t(high)-low+1)*4-pos;
        }
        int32_t n=int32_t(u4(b,p+4));
        if (n<0 || size_t(n)>(end-p-8)/8) return 0;
        return p+8+size_t(n)*8-pos;
    }
    if (op==0xc4) return pos+1<end ? (b[pos+1]==0x84 ? 6 : 4) : 0;
    if (op==0xc8 || op==0xc9 || op==0xb9 || op==0xba) return 5;
    if (op==0xc5) return 4;
    if ((op>=0x99 && op<=0xa8) || (op>=0xb2 && op<=0xb8) || op==0xbb || op==0xbd || op==0xc0 || op==0xc1 || op==0xc6 || op==0xc7 || op==0x11 || op==0x13 || op==0x14 || op==0x84) return 3;
    if (op==0x10 || op==0x12 || (op>=0x15 && op<=0x19) || (op>=0x36 && op<=0x3a) || op==0xa9 || op==0xbc) return 2;
    return 1;
}

// Returns false without changing out if the class layout or expected call sites differ.
inline bool transform(const Bytes& in, const char* targetMethod, const char* helperClass,
                      bool packets, Bytes& out, bool tryPick = false, bool* pickPatched = nullptr,
                      int* pickKind = nullptr) {
    if (pickPatched) *pickPatched = false;
    if (pickKind) *pickKind = 0;
    if (in.size()<10 || u4(in,0)!=0xcafebabe) return false;
    uint16_t count=u2(in,8); if (!count || count>65520) return false;
    std::vector<Cp> cp(count); size_t p=10;
    for (uint16_t i=1;i<count;++i) {
        if (p>=in.size()) return false;
        uint8_t tag=in[p++]; cp[i].tag=tag;
        size_t n=0;
        switch(tag) {
        case 1: if (p+2>in.size()) return false; n=u2(in,p); p+=2; if (p+n>in.size()) return false; cp[i].text.assign(reinterpret_cast<const char*>(&in[p]),n); p+=n; continue;
        case 3: case 4: n=4; break;
        case 5: case 6: n=8; if (++i>=count) return false; break;
        case 7: case 8: case 16: case 19: case 20: n=2; break;
        case 9: case 10: case 11: case 12: case 17: case 18: n=4; break;
        case 15: n=3; break;
        default: return false;
        }
        if (p+n>in.size()) return false;
        if (n==2) cp[i].a=u2(in,p);
        if (n==4 && tag!=3 && tag!=4) { cp[i].a=u2(in,p); cp[i].b=u2(in,p+2); }
        p+=n;
    }
    size_t cpEnd=p;
    auto utf=[&](uint16_t i)->std::string { return i<count && cp[i].tag==1 ? cp[i].text : std::string(); };
    auto methodName=[&](uint16_t i)->std::string {
        if (i>=count || cp[i].tag!=10 || cp[i].b>=count || cp[cp[i].b].tag!=12) return {};
        return utf(cp[cp[i].b].a);
    };
    auto skipAttributes=[&](size_t& at, uint16_t n)->bool {
        while(n--) { if(at+6>in.size()) return false; uint32_t len=u4(in,at+2); at+=6; if(len>in.size()-at) return false; at+=len; } return true;
    };
    if (p+8>in.size()) return false;
    uint16_t interfaces=u2(in,p+6); p+=8; if (size_t(interfaces)*2>in.size()-p) return false; p+=size_t(interfaces)*2;
    if(p+2>in.size()) return false; uint16_t fields=u2(in,p); p+=2;
    while(fields--) { if(p+8>in.size()) return false; uint16_t a=u2(in,p+6); p+=8; if(!skipAttributes(p,a)) return false; }
    if(p+2>in.size()) return false; uint16_t methods=u2(in,p); p+=2;
    auto methodRef=[&](uint16_t i, std::string& nm, std::string& ds)->bool {
        if (i>=count || cp[i].tag!=10 || cp[i].b>=count || cp[cp[i].b].tag!=12) return false;
        nm=utf(cp[cp[i].b].a); ds=utf(cp[cp[i].b].b); return true;
    };
    struct Patch { size_t pos; bool pitch; }; std::vector<Patch> patches;
    // Two ways into the crosshair pick, in order of preference:
    //  - method_76762 (getCrosshairTarget) always calls method_76763(Entity, double, double, float),
    //    the full block + entity raycast. Replacing THAT call runs on every frame and every view,
    //    which is what lets the aim own the pick while the camera looks at a wall.
    //  - checkCrosshairTargetRange(method_76764) only runs when the camera's own ray already found an
    //    entity, so it is kept as the fallback for a class whose pick method the scan cannot read.
    // Descriptors are the ones the runtime class file carries: Fabric's intermediary names
    // (net/minecraft/class_*), the same style the movement method below is matched with.
    std::vector<size_t> rangeSites;
    std::vector<size_t> fullSites;
    bool found=false;
    while(methods--) {
        if(p+8>in.size()) return false;
        const std::string methodNameHere=utf(u2(in,p+2));
        bool match=methodNameHere==targetMethod && utf(u2(in,p+4))=="()V";
        if(!packets) match=methodNameHere==targetMethod && utf(u2(in,p+4))=="(FLnet/minecraft/class_243;)V";
        const bool pickFullMethod=tryPick && methodNameHere=="method_76762";
        const bool pickRangeMethod=tryPick && methodNameHere=="method_76763";
        uint16_t attrs=u2(in,p+6); p+=8;
        while(attrs--) {
            if(p+6>in.size()) return false; uint16_t name=u2(in,p); uint32_t length=u4(in,p+2); p+=6;
            if(length>in.size()-p) return false;
            if(utf(name)=="Code" && length>=8) {
                uint32_t size=u4(in,p+4); size_t base=p+8, end=base+size;
                if(size>length-8 || end>in.size()) return false;
                if(match) { if(found) return false; found=true; }
                for(size_t pos=base;pos<end;) {
                    size_t step=instructionLength(in,base,pos,end); if(!step || step>end-pos) return false;
                    if(match && in[pos]==0xb6 && step==3) {
                        std::string name2=methodName(u2(in,pos+1));
                        if(name2=="method_36454" || (packets && name2=="method_36455")) patches.push_back({pos,name2=="method_36455"});
                    }
                    if(pickFullMethod && (in[pos]==0xb6 || in[pos]==0xb7) && step==3) {
                        std::string nm,ds;
                        if(methodRef(u2(in,pos+1),nm,ds) && nm=="method_76763" &&
                           ds=="(Lnet/minecraft/class_1297;DDF)Lnet/minecraft/class_239;")
                            fullSites.push_back(pos);
                    }
                    if(pickRangeMethod && in[pos]==0xb8 && step==3) {
                        std::string nm,ds;
                        if(methodRef(u2(in,pos+1),nm,ds) && nm=="method_76764" &&
                           ds=="(Lnet/minecraft/class_239;Lnet/minecraft/class_243;D)Lnet/minecraft/class_239;")
                            rangeSites.push_back(pos);
                    }
                    pos+=step;
                }
            } else if(match && utf(name)=="Code") return false;
            p+=length;
        }
    }
    if(!found || (!packets && patches.size()!=1) || (packets && patches.size()!=8)) return false;
    // Every method_76763 call inside the pick method is replaced, so a class that computes the pick
    // in more than one branch is still covered; the range fallback needs its one call site.
    const bool fullOk=tryPick && !fullSites.empty();
    const bool rangeOk=tryPick && !fullOk && rangeSites.size()==1;
    const bool pickOk=fullOk || rangeOk;
    Bytes extra;
    // Constant pool: helper class, descriptor, two method names and two Methodrefs.
    putUtf(extra,helperClass); extra.push_back(7); put2(extra,count);
    putUtf(extra,"(Ljava/lang/Object;)F");
    putUtf(extra,packets ? "packetYaw" : "movementYaw");
    putRef(extra,12,uint16_t(count+3),uint16_t(count+2)); putRef(extra,10,uint16_t(count+1),uint16_t(count+4));
    putUtf(extra,"packetPitch"); putRef(extra,12,uint16_t(count+6),uint16_t(count+2));
    putRef(extra,10,uint16_t(count+1),uint16_t(count+7));
    // Both pick helpers are always in the pool, so the patch below only has to choose a reference:
    // `pick` takes the same arguments checkCrosshairTargetRange already has on the stack, `pickFull`
    // takes the receiver (the player) first, which is exactly the shape of the method_76763 call.
    putUtf(extra,"tyrant/internal/SilentPickHelper");                                        // count+9
    extra.push_back(7); put2(extra,uint16_t(count+9));                                       // count+10 Class
    putUtf(extra,"pick");                                                                    // count+11
    putUtf(extra,"(Lnet/minecraft/class_239;Lnet/minecraft/class_243;D)Lnet/minecraft/class_239;"); // count+12
    putRef(extra,12,uint16_t(count+11),uint16_t(count+12));                                  // count+13
    putRef(extra,10,uint16_t(count+10),uint16_t(count+13));                                  // count+14
    putUtf(extra,"tyrant/internal/SilentPickHelper");                                        // count+15
    extra.push_back(7); put2(extra,uint16_t(count+15));                                      // count+16 Class
    putUtf(extra,"pickFull");                                                                // count+17
    putUtf(extra,"(Lnet/minecraft/class_746;Lnet/minecraft/class_1297;DDF)Lnet/minecraft/class_239;"); // count+18
    putRef(extra,12,uint16_t(count+17),uint16_t(count+18));                                  // count+19
    putRef(extra,10,uint16_t(count+16),uint16_t(count+19));                                  // count+20
    Bytes result; result.reserve(in.size()+extra.size());
    result.insert(result.end(),in.begin(),in.begin()+8); put2(result,uint16_t(count+21));
    result.insert(result.end(),in.begin()+10,in.begin()+cpEnd); result.insert(result.end(),extra.begin(),extra.end());
    result.insert(result.end(),in.begin()+cpEnd,in.end());
    for(const Patch& patch:patches) {
        size_t at=patch.pos+extra.size(); result[at]=0xb8;
        uint16_t ref=uint16_t(count+(patch.pitch?8:5)); result[at+1]=uint8_t(ref>>8); result[at+2]=uint8_t(ref);
    }
    if(pickOk) {
        const std::vector<size_t>& sites=fullOk?fullSites:rangeSites;
        const uint16_t ref=uint16_t(count+(fullOk?20:14));
        for(size_t site:sites) {
            size_t at=site+extra.size(); result[at]=0xb8;
            result[at+1]=uint8_t(ref>>8); result[at+2]=uint8_t(ref);
        }
    }
    if(pickPatched) *pickPatched=pickOk;
    // 1 = the pick method's own raycast call is replaced, 2 = the range-check fallback, 0 = neither.
    if(pickKind) *pickKind=pickOk?(fullOk?1:2):0;
    out=std::move(result); return true;
}
} // namespace tyr::jni::silent_transform
