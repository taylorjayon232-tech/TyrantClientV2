// ============================================================================
//  Passive — Motion Blur implementation (Natural Motion Blur parity).
//
//  A half-resolution ring of up to nine frames forms the history. Each frame the
//  frame the game just drew is blitted (GPU-side, scaled down) into the newest
//  slot and the older slots are drawn back over it under the chosen method:
//
//    Velocity Based   the history is offset along the camera turn, so a fast turn
//                     leaves a directional smear and standing still draws nothing.
//    Frame Blending   the history is blended straight over the frame (no offset),
//                     which is the mod's accumulation approach: it removes the
//                     stroboscopic steps between frames rather than smearing.
//    Hybrid           both at once - offset and accumulated.
//
//  Accumulation Max is how many earlier frames are used, Accumulation Mix how
//  strongly they come back, Weighted vs Uniform how the frames are weighted, and
//  Refresh Rate Scaling keeps the perceived trail the same at 60 and 240 fps.
// ============================================================================
#include "motionblur.h"
#include "glex.h"
#include "../core/agent.h"   // trace::log

#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace tyr {
namespace motionblur {

namespace {

constexpr int kMaxSlots = 9;         // the newest frame plus up to eight folded into the trail
constexpr float kMaxOffset = 0.07f;  // a single sample never shifts more than this in UV space

bool   g_ready = false;
int    g_w = 0, g_h = 0;             // the size of the frame the history is captured from
int    g_hw = 0, g_hh = 0;           // the half-resolution history size
GLuint g_tex[kMaxSlots] = {};        // lazily created together: all slots share the capture size
GLuint g_fbo = 0;
int    g_head = 0;                   // the slot written this frame
int    g_count = 0;                  // frames folded in so far (the trail needs at least two)

GLuint makeTex(int w, int h) {
    GLuint t = 0;
    glGenTextures(1, &t);
    if (!t) return 0;
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

void destroyGl() {
    for (int i = 0; i < kMaxSlots; ++i)
        if (g_tex[i]) { glDeleteTextures(1, &g_tex[i]); g_tex[i] = 0; }
    if (g_fbo) { glDeleteFramebuffers(1, &g_fbo); g_fbo = 0; }
    g_ready = false;
    g_count = 0;
    g_head = 0;
}

bool ensureSize(int w, int h) {
    if (w <= 0 || h <= 0) return false;
    if (g_ready && w == g_w && h == g_h) return true;
    destroyGl();
    g_w = w; g_h = h;
    g_hw = std::max(1, w / 2); g_hh = std::max(1, h / 2);
    // Every slot is allocated at once: the ring is small (nine half-resolution RGBA frames), and
    // creating one mid-frame would mean a driver allocation on a frame that is already drawing.
    for (int i = 0; i < kMaxSlots; ++i) {
        g_tex[i] = makeTex(g_hw, g_hh);
        if (!g_tex[i]) { destroyGl(); return false; }
    }
    glGenFramebuffers(1, &g_fbo);
    if (!g_fbo) { destroyGl(); return false; }
    g_ready = true;
    return true;
}

} // namespace

bool ready() { return g_ready; }

void release() {
    if (g_tex[0] || g_fbo) destroyGl();
}

void compose(const Params& p) {
    if (!tyr::glx::g_glexReady) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    if (display.x < 2.f || display.y < 2.f) return;
    if (!ensureSize((int)(display.x + 0.5f), (int)(display.y + 0.5f))) return;

    // ---- capture: the frame the game just drew, scaled into the newest slot ----
    GLint prevFb = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFb);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_tex[g_head], 0);
    glBlitFramebuffer(0, 0, g_w, g_h, 0, 0, g_hw, g_hh, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    // Whole-framebuffer restore: split read/draw bindings are unreliable on older Intel drivers
    // (see blur.cpp), so the binding is put back as one value.
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFb);
    glBindTexture(GL_TEXTURE_2D, 0);

    // ---- settings ----
    const int   method    = std::clamp(p.method, 0, 2);
    const float strength  = std::clamp(p.strength, 0.1f, 3.f);
    const int   want      = std::clamp(p.accumulationMax, 1, kMaxSlots - 1);
    const float mix       = std::clamp(p.accumulationMix, 0.f, 1.f);
    const float sens      = std::clamp(p.sensitivity, 0.2f, 3.f);
    // Frame rate from the frame's own delta; a zero or absurd delta would otherwise make the
    // scaling factor explode on the frame a world loads.
    const float dt        = std::clamp(p.deltaTime, 1.f / 1000.f, 0.25f);
    const float fps       = 1.f / dt;
    // Refresh Rate Scaling: at 144 fps the trail is the reference, below that every frame carries
    // more of the smear and above it less, so the same turn looks the same on any monitor.
    const float refresh   = p.refreshScaling ? std::clamp(144.f / fps, 0.5f, 2.f) : 1.f;

    // The camera's turn drives everything: no turn means no smear and no visible mixback, so a
    // player standing still pays nothing on screen for having the module on.
    const float turn = std::sqrt(p.turnYaw * p.turnYaw + p.turnPitch * p.turnPitch);
    const float feed = std::clamp(turn / 12.f * sens, 0.f, 1.f);
    const int   trail = std::min(g_count - 1, want);

    const auto advance = [&]() {
        g_head = (g_head + 1) % kMaxSlots;
        if (g_count < kMaxSlots) ++g_count;
    };

    if (trail < 1 || feed <= 0.004f) { advance(); return; }

    // ---- profile: how each older frame is weighted ----
    float weights[kMaxSlots] = {};
    float weightSum = 0.f;
    for (int i = 1; i <= trail; ++i) {
        // Weighted (the mod's default) decays with age; uniform gives every sample the same say,
        // which is what a long, even frame-blend needs.
        weights[i] = p.weighted ? std::pow(0.62f, (float)(i - 1)) : 1.f;
        weightSum += weights[i];
    }
    if (weightSum < 1e-4f) weightSum = 1.f;

    // Total amount mixed back this frame. Velocity Based uses the strength alone; the two blending
    // methods scale by Accumulation Mix, so at 0 they are off and at 1 they are at full strength.
    const float base = strength * feed * refresh * 0.42f;
    const float total = (method == 0) ? base : base * mix;

    // Velocity Based (and Hybrid): screen-space offset along the turn, in UV units. Yaw turns the
    // world sideways, pitch up/down, and the sign makes the trail lag behind the turn rather than
    // lead it.
    const float ox = std::clamp(-p.turnYaw * 0.0016f * sens, -kMaxOffset, kMaxOffset);
    const float oy = std::clamp( p.turnPitch * 0.0016f * sens, -kMaxOffset, kMaxOffset);

    for (int i = 1; i <= trail; ++i) {
        const int slot = ((g_head - i) % kMaxSlots + kMaxSlots) % kMaxSlots;
        const float a = std::clamp(total * (weights[i] / weightSum), 0.f, 0.9f);
        if (a <= 0.004f) continue;
        // Frame Blending draws the history where it is; the other two walk each older sample
        // further along the turn, which is what turns the accumulation into a directional trail.
        const float k = (method == 1) ? 0.f : (float)i;
        const ImVec2 uv0(0.f + ox * k, 1.f + oy * k);      // the history is bottom-up
        const ImVec2 uv1(1.f + ox * k, 0.f + oy * k);
        dl->AddImage((ImTextureID)(ImU64)g_tex[slot], ImVec2(0.f, 0.f), display, uv0, uv1,
                     IM_COL32(255, 255, 255, (int)(a * 255.f)));
    }

    advance();
}

} // namespace motionblur
} // namespace tyr
