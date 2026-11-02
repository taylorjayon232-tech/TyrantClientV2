// ============================================================================
//  Passive — small OpenGL helpers shared by the blur pipeline.
// ============================================================================
#pragma once
#include "glex.h"   // windows.h + GL/gl.h + GL3 loader (must be first)

namespace tyr {
namespace glx {
    inline void enableBlendPremult() {
        glEnable(GL_BLEND);
        if (p_glBlendFuncSeparate)
            p_glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        else
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    inline void disableDepth() { glDisable(GL_DEPTH_TEST); }
    inline void disableCull()  { glDisable(GL_CULL_FACE); }
} // namespace glx
} // namespace tyr
