// ============================================================================
//  Passive — GL3 backdrop blur (frame capture + separable Gaussian + rounded
//  masked draw via ImGui).
// ============================================================================
#include "blur.h"
#include "glex.h"        // GL3 loader (macros for shader/FBO functions)
#include "glhelpers.h"

#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include "../core/agent.h"   // trace::log (GL error attribution probe)
#include <string>
#include <vector>

namespace tyr {
namespace blur {

namespace {

bool g_ready = false;
int  g_w = 0, g_h = 0;
int  g_capW = 0, g_capH = 0;

GLuint g_srcTex = 0, g_fboSrc = 0;   // full-res capture
GLuint g_tmpTexA = 0, g_fboA = 0;    // quarter-res ping
GLuint g_tmpTexB = 0, g_fboB = 0;    // quarter-res pong
GLuint g_progPre = 0;                // downsample
GLuint g_progBlur = 0;               // separable gaussian (dir uniform)
GLuint g_vao = 0, g_vbo = 0;

ImTextureID g_lastBlurred = 0;   // ImTextureID is unsigned long long in ImGui 1.91

const char* kVs = R"(#version 330
layout (location=0) in vec2 aPos;
layout (location=1) in vec2 aUv;
out vec2 vUv;
void main(){ vUv = aUv; gl_Position = vec4(aPos,0.0,1.0); })";

const char* kFsDown = R"(#version 330
uniform sampler2D uTex;
uniform vec2 uTexel;
in vec2 vUv; out vec4 oColor;
void main(){
    // 4-tap box downsample
    vec4 c = texture(uTex, vUv + vec2( uTexel.x,  uTexel.y));
    c += texture(uTex, vUv + vec2(-uTexel.x,  uTexel.y));
    c += texture(uTex, vUv + vec2( uTexel.x, -uTexel.y));
    c += texture(uTex, vUv + vec2(-uTexel.x, -uTexel.y));
    oColor = c * 0.25;
})";

const char* kFsBlur = R"(#version 330
uniform sampler2D uTex;
uniform vec2 uDir;      // direction * texel step
in vec2 vUv; out vec4 oColor;
void main(){
    // 9-tap gaussian
    float w[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec4 c = texture(uTex, vUv) * w[0];
    for (int i = 1; i < 5; ++i) {
        c += texture(uTex, vUv + uDir * float(i)) * w[i];
        c += texture(uTex, vUv - uDir * float(i)) * w[i];
    }
    oColor = c;
})";

GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { glDeleteShader(s); return 0; }
    return s;
}
GLuint link(const char* vs, const char* fs) {
    GLuint v = compile(GL_VERTEX_SHADER, vs);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) { if (v) glDeleteShader(v); if (f) glDeleteShader(f); return 0; }
    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { glDeleteProgram(p); return 0; }
    return p;
}

GLuint makeTex(int w, int h) {
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}
GLuint makeFbo(GLuint tex) {
    GLuint f = 0;
    glGenFramebuffers(1, &f);
    glBindFramebuffer(GL_FRAMEBUFFER, f);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    return f;
}

// All GL3 entry points go through the loader; if init() failed (GL1-only
// context) g_ready stays false and capture() becomes a cheap no-op.
#define GLEX_OK() (tyr::glx::g_glexReady)

void destroyGl() {
    if (g_srcTex)  { glDeleteTextures(1, &g_srcTex);  g_srcTex = 0; }
    if (g_tmpTexA) { glDeleteTextures(1, &g_tmpTexA); g_tmpTexA = 0; }
    if (g_tmpTexB) { glDeleteTextures(1, &g_tmpTexB); g_tmpTexB = 0; }
    if (g_fboSrc)  glDeleteFramebuffers(1, &g_fboSrc);
    if (g_fboA)    glDeleteFramebuffers(1, &g_fboA);
    if (g_fboB)    glDeleteFramebuffers(1, &g_fboB);
    if (g_progPre) glDeleteProgram(g_progPre);
    if (g_progBlur)glDeleteProgram(g_progBlur);
    if (g_vao) glDeleteVertexArrays(1, &g_vao);
    if (g_vbo) glDeleteBuffers(1, &g_vbo);
    g_fboSrc = g_fboA = g_fboB = g_progPre = g_progBlur = g_vao = g_vbo = 0;
    g_lastBlurred = 0;
}

void ensureSize(int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (w == g_w && h == g_h && g_srcTex) return;
    destroyGl();
    g_w = w; g_h = h;
    g_capW = w / 4; g_capH = h / 4;
    if (g_capW < 1) g_capW = 1;
    if (g_capH < 1) g_capH = 1;

    g_srcTex  = makeTex(w, h);      g_fboSrc = makeFbo(g_srcTex);
    g_tmpTexA = makeTex(g_capW, g_capH); g_fboA = makeFbo(g_tmpTexA);
    g_tmpTexB = makeTex(g_capW, g_capH); g_fboB = makeFbo(g_tmpTexB);
    g_progPre  = link(kVs, kFsDown);
    g_progBlur = link(kVs, kFsBlur);
    g_ready = g_progPre && g_progBlur;
}

struct QuadVert { float x, y, u, v; };
void drawFullscreenQuad() {
    if (!g_vao) {
        glGenVertexArrays(1, &g_vao);
        glGenBuffers(1, &g_vbo);
        glBindVertexArray(g_vao);
        glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
        const QuadVert q[4] = {
            { -1.f, -1.f, 0.f, 1.f },
            {  1.f, -1.f, 1.f, 1.f },
            {  1.f,  1.f, 1.f, 0.f },
            { -1.f,  1.f, 0.f, 0.f },
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(QuadVert), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(QuadVert), (void*)(2 * sizeof(float)));
    } else {
        glBindVertexArray(g_vao);
    }
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

// (Compositing is done via ImGui AddImageRounded in drawPanel(); no custom
// draw callback needed — the ImGui GL3 backend renders our blurred texture
// with the clip rect applied, and corner rounding comes from AddImageRounded.)

} // namespace

bool init()    { return true; }         // lazy; real init on first capture
void shutdown(){ destroyGl(); }

void capture() {
    if (!GLEX_OK()) return;                 // no GL3 -> blur disabled, GUI fine
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    ensureSize(vp[2], vp[3]);
    if (!g_ready) return;

    GLint prevFb = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFb);   // ONE binding on old Intel drivers

    // One-shot attribution probe (first 3 captures only): after each stage,
    // drain glGetError and name the failing stage in passive_agent.log. Cheap,
    // temporary, and turns "GL_INVALID_ENUM spam" into an exact call site.
    static int probeRuns = 0;
    const bool probe = probeRuns < 3;
    ++probeRuns;
    auto checkErr = [&](const char* stage) {
        if (!probe) return;
        GLenum err;
        while ((err = glGetError()) != GL_NO_ERROR) {
            const char* s = (err == GL_INVALID_ENUM) ? "GL_INVALID_ENUM"
                          : (err == GL_INVALID_OPERATION) ? "GL_INVALID_OPERATION"
                          : (err == GL_INVALID_VALUE) ? "GL_INVALID_VALUE"
                          : (err == GL_INVALID_FRAMEBUFFER_OPERATION) ? "GL_INVALID_FRAMEBUFFER_OP"
                          : (err == GL_OUT_OF_MEMORY) ? "GL_OUT_OF_MEMORY" : "GL_OTHER";
            tyr::trace::log("blur probe: %s -> %s", stage, s);
        }
    };

    // Copy the DEFAULT backbuffer (framebuffer 0) into g_srcTex. Whole-FBO
    // binds only: split READ/DRAW bindings are unreliable on Haswell-era Intel
    // drivers (this machine's ig75icd64 20.19.15.4835).
    //
    // glCopyTexSubImage2D fails with GL_INVALID_ENUM on that driver (proven by
    // the in-game probe), so first drain any errors the GAME left pending —
    // otherwise our attribution is wrong — then try the fast GPU copy. If it
    // errors even once, permanently switch to glReadPixels + glTexSubImage2D,
    // which every GL 3.3 driver honors.
    static bool s_useReadback = false;
    static bool s_copyFailedLogged = false;
    while (glGetError() != GL_NO_ERROR) {}          // drain game's stale errors

    glBindFramebuffer(GL_FRAMEBUFFER, 0);   // read from default backbuffer

    bool copyOk = true;
    if (!s_useReadback) {
        glBindTexture(GL_TEXTURE_2D, g_srcTex);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, g_w, g_h);
        copyOk = (glGetError() == GL_NO_ERROR);
        if (!copyOk) {
            s_useReadback = true;   // permanent for this session
            if (!s_copyFailedLogged) {
                s_copyFailedLogged = true;
                tyr::trace::log("blur: glCopyTexSubImage2D rejected by driver — switching to readback path");
            }
        }
    }
    if (s_useReadback) {
        static std::vector<unsigned char> s_pixels;
        s_pixels.resize((size_t)g_w * g_h * 4);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, g_w, g_h, GL_RGBA, GL_UNSIGNED_BYTE, s_pixels.data());
        copyOk = (glGetError() == GL_NO_ERROR);
        if (copyOk) {
            glBindTexture(GL_TEXTURE_2D, g_srcTex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_w, g_h, GL_RGBA, GL_UNSIGNED_BYTE, s_pixels.data());
            copyOk = (glGetError() == GL_NO_ERROR);
        }
    }
    checkErr("blur capture");
    if (!copyOk) { glBindFramebuffer(GL_FRAMEBUFFER, prevFb); return; }

    // downsample to A
    glBindFramebuffer(GL_FRAMEBUFFER, g_fboA);
    glViewport(0, 0, g_capW, g_capH);
    glUseProgram(g_progPre);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_srcTex);
    glUniform1i(glGetUniformLocation(g_progPre, "uTex"), 0);
    glUniform2f(glGetUniformLocation(g_progPre, "uTexel"), 1.f/g_w, 1.f/g_h);
    glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND);
    drawFullscreenQuad();
    checkErr("downsample pass");

    // blur H (A->B), blur V (B->A) x2 for extra softness
    glUseProgram(g_progBlur);
    glUniform1i(glGetUniformLocation(g_progBlur, "uTex"), 0);
    auto blurPass = [&](GLuint srcTex, GLuint dstFbo, float dx, float dy) {
        glBindFramebuffer(GL_FRAMEBUFFER, dstFbo);
        glBindTexture(GL_TEXTURE_2D, srcTex);
        glUniform2f(glGetUniformLocation(g_progBlur, "uDir"), dx / g_capW, dy / g_capH);
        drawFullscreenQuad();
    };
    for (int i = 0; i < 2; ++i) {
        blurPass(g_tmpTexA, g_fboB, 1.6f, 0.f);
        blurPass(g_tmpTexB, g_fboA, 0.f, 1.6f);
    }
    checkErr("blur passes");

    g_lastBlurred = (ImTextureID)(ImU64)g_tmpTexA;

    glBindFramebuffer(GL_FRAMEBUFFER, prevFb);   // whole bind restore
    glBindTexture(GL_TEXTURE_2D, 0);             // games hate surprise bindings
    glActiveTexture(GL_TEXTURE0);                // ensure unit 0 selected
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    glEnable(GL_BLEND);
    checkErr("state restore");
}

void drawPanel(float x, float y, float w, float h, float rounding, float alpha) {
    if (!g_ready || !GLEX_OK() || !g_lastBlurred) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (!dl) return;

    const ImU32 tint = IM_COL32(255, 255, 255, (int)(alpha * 255.f));
    dl->AddImageRounded(g_lastBlurred, ImVec2(x, y), ImVec2(x + w, y + h),
                        ImVec2(0.f, 1.f), ImVec2(1.f, 0.f), tint, rounding);
    // Darken slightly for contrast + accent rim handled by the caller.
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h),
                      IM_COL32(10, 10, 18, (int)(110 * alpha)), rounding);
}

bool ready() { return g_ready; }

} // namespace blur
} // namespace tyr
