// ============================================================================
//  Passive — Shader ESP rules: who gets the glow, and where the hand glow sits.
// ----------------------------------------------------------------------------
//  Pure: no JNI, no OpenGL, no ImGui, so test/shader_glow_check.cpp exercises
//  exactly what the module runs.
//
//  Verified against the 1.21.11 client jar before this was written:
//
//   * Minecraft's glowing outline is driven by a client-side entity flag
//     (Entity.setGlowing writes it, isGlowing reads it back on the client).
//     Setting that flag on a player or a dropped item makes the game itself draw
//     the outline around the rendered model — the skin, the armour, the held
//     item, the item model of a dropped stack. That is the only way to get a
//     true "outside their skin" glow: the frame's depth buffer is cleared
//     (CommandEncoder.clearDepthTexture) before the hand and the HUD are drawn,
//     so at swap time there is no world depth left to trace a silhouette from.
//
//   * The first-person item is not an entity, so the game will never outline it.
//     That one gets a glow we draw ourselves, anchored where the game puts the
//     hand (HeldItemRenderer.applyItemArmTransform: translate(0.56, -0.52,
//     -0.71) in view space), projected with the frame's own focal length so it
//     stays on the hand at any FOV.
// ============================================================================
#pragma once

#include <cmath>

namespace tyr {
namespace modules {
namespace glow {

// ---- which entities the game's own glow is switched on for ------------------
enum class Style { Vanilla, Custom };
enum class Kind { Ignored, Player, Item };

// Exact registry-path match: "player" must not match "player_..." and "item"
// must not match "item_frame".
inline bool sameId(const char* value, const char* id) {
    if (!value || !id) return false;
    while (*value && *id && *value == *id) { ++value; ++id; }
    return *value == '\0' && *id == '\0';
}

// The entity's registry path ("player", "item", "zombie") decides what is glowable.
inline Kind kindFor(const char* typePath) {
    if (sameId(typePath, "player")) return Kind::Player;
    if (sameId(typePath, "item"))   return Kind::Item;
    return Kind::Ignored;
}

// The glow pass re-renders every glowing entity, so the list is capped. Nearest
// entities are flagged first, and the cap only ever bites on an item pile.
const int kMaxGlowing = 64;

// A player or a dropped item, not yourself, inside Max distance, is worth glowing.
// `distance` is blocks from the camera to the entity centre.
inline bool shouldFlag(Kind kind, bool isSelf, float distance, float maxDistance) {
    if (kind == Kind::Ignored || isSelf) return false;
    if (!(distance >= 0.f) || !(maxDistance > 0.f)) return false;
    return distance <= maxDistance;
}

// What this frame does with one entity, given how the frame before left it.
struct Decision {
    bool flag;    // switch the glow on now
    bool clear;   // switch our own glow off again
};

inline Decision decide(Kind kind, bool isSelf, float distance, float maxDistance,
                       bool ours, bool alreadyGlowing) {
    if (!shouldFlag(kind, isSelf, distance, maxDistance)) {
        // Out of range, off the list, or the module is off: undo only our own glow.
        Decision d = { false, ours };
        return d;
    }
    // A glow that is already there is left exactly as it is: it may be the
    // server's own effect or another mod's, and clearing it would be this module
    // reaching outside itself. That entity is never recorded as ours either.
    if (alreadyGlowing) {
        Decision d = { false, false };
        return d;
    }
    Decision d = { true, false };
    return d;
}

// ---- the first-person hand glow ---------------------------------------------
// The game's own pivot for the item in hand, in view space (x right, y up,
// z forward). The item is modelled a little above that pivot, so the glow is
// centred where the item really is rather than on the wrist.
const float kHandPivotX = 0.56f;
const float kHandPivotY = -0.52f;
const float kHandPivotZ = 0.71f;
const float kHandItemRise = 0.20f;   // blocks above the pivot

struct HandGlow {
    float x = 0.f;      // screen pixels, quad centre
    float y = 0.f;
    float size = 0.f;   // quad edge, screen pixels
    bool  valid = false;
};

// `sizePercent` is the glow's width as a percentage of one block at the hand
// (5-60); `nudgeX/YPercent` shift it by a percentage of the screen height, the
// way Minecraft's item-in-hand scale differs a little per item and per modded
// hand setting.
inline HandGlow handGlow(float fovDegrees, float displayWidth, float displayHeight,
                         float sizePercent, float nudgeXPercent, float nudgeYPercent) {
    HandGlow g;
    if (!(fovDegrees > 1.f) || !(fovDegrees < 179.f)) return g;
    if (!(displayWidth > 1.f) || !(displayHeight > 1.f)) return g;
    const float focal = (displayHeight * 0.5f) /
                        std::tan(fovDegrees * 0.008726646259971648f);   // pi/360
    if (!(focal > 1.f)) return g;
    const float fraction = sizePercent * 0.01f;
    // The pivot is below the eye line and the item model sits above it, so the
    // glow's centre is (rise - pivotY) blocks up from the view axis.
    g.x = displayWidth * 0.5f + focal * (kHandPivotX / kHandPivotZ)
        + nudgeXPercent * 0.01f * displayHeight;
    g.y = displayHeight * 0.5f + focal * ((-kHandPivotY - kHandItemRise) / kHandPivotZ)
        + nudgeYPercent * 0.01f * displayHeight;
    g.size = focal * fraction / kHandPivotZ;
    // A hand off the edge of the screen (an extreme field of view) would draw the
    // glow in the air, so it counts as no glow at all.
    g.valid = (g.size > 1.f) && (g.size < displayHeight * 2.f) &&
              (g.x > 0.f) && (g.x < displayWidth) &&
              (g.y > 0.f) && (g.y < displayHeight);
    return g;
}

} // namespace glow
} // namespace modules
} // namespace tyr
