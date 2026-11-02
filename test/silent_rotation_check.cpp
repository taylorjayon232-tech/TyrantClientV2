#include "../agent/jni/silent_class_transform.h"
#include <fstream>
#include <iostream>
#include <iterator>
using namespace tyr::jni::silent_transform;
int main(int argc, char** argv) {
    if (argc!=4) return 2;
    for (int i=1;i<=2;++i) {
        std::ifstream f(argv[i],std::ios::binary);
        Bytes in((std::istreambuf_iterator<char>(f)),{}), out;
        if (!transform(in,i==1?"method_5724":"method_3136","tyrant/internal/SilentRotationHelper",i==2,out)) {
            std::cerr << "transform failed: " << argv[i] << '\n'; return 1;
        }
        std::ofstream dst(std::string(argv[3])+(i==1?"/entity.class":"/player.class"),std::ios::binary);
        dst.write(reinterpret_cast<const char*>(out.data()),out.size());
        if(!dst) return 1;
        Bytes truncated=in; truncated.resize(20); Bytes unused;
        if(transform(truncated,"method_3136","tyrant/internal/SilentRotationHelper",true,unused)) return 1;
    }
    std::cout << "actual class transformations and malformed-input rejection passed\n";
}
