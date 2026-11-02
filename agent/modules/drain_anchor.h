// ============================================================================
//  Passive — Auto Drain's web rule: which cell a cobweb lands in.
//
//  Pure: no JNI and no ImGui, so test/drain_anchor_check.cpp pins exactly the
//  rule agent/modules/modules.cpp (updateWebDrain) uses.
//
//  A drain is a cobweb in a cell that really holds a water source, and the
//  crosshair reaches that cell in one of two shapes:
//
//    * the ray stops in the water itself (or in the cell it just filled). The
//      web goes into THAT cell, and the click is made against the TOP face of
//      the block directly under it - the shape a player leaves when they place
//      a block and pour water onto it. A water source with nothing solid under
//      it is refused, because clicking a fluid cell in mid-air is a placement
//      the server does not see (the "it still air places" report);
//    * the ray stops on the TOP face of a real block that has water in the cell
//      above it. The click goes against that top face and the web lands in the
//      cell above the block.
//
//  Only a top face anchors a drain. Any other face would put the web beside the
//  water instead of in it, which is a placement the server never sees.
// ============================================================================
#pragma once

namespace tyr {
namespace modules {
namespace drain {

// Where the cobweb goes and what the click is made against, both as offsets from the block the ray
// stopped on, plus the face of the clicked block the click uses.
struct Anchor {
    bool valid = false;
    int  placeDx = 0, placeDy = 0, placeDz = 0;   // the cell the cobweb lands in
    int  clickDx = 0, clickDy = 0, clickDz = 0;   // the block the click is made against
    int  face = 1;   // 0 = down, 1 = up, 2 = north, 3 = south, 4 = west, 5 = east
};

// `hitIsWater`  — the ray stopped in the fluid cell itself (water, or the cell it filled)
// `faceVisible` — a side was read off the hit result
// `faceY`       — that side's own Y step: +1 is the top face
// `hitFace`     — that side as a face id (1 = up), which is the face the click uses
// `waterInCell` — the cell the click would fill holds a water source
// `solidBelow`  — the cell directly under the block the ray stopped on holds a real block
inline Anchor resolve(bool hitIsWater, bool faceVisible, int faceY, int hitFace, bool waterInCell,
                      bool solidBelow) {
    Anchor a;
    if (!waterInCell) return a;                       // nothing to bury: refuse rather than air-place
    if (hitIsWater) {
        // The web lands in the fluid's own cell, but only against the top face of the block under
        // it. Without that block the click would be a placement on a fluid cell with nothing to
        // stand on - the air place this rule exists to refuse.
        if (!solidBelow) return a;
        a.valid = true;
        a.clickDy = -1;                                // click the block under the water...
        a.face = 1;                                    // ...on its top face (the web goes in the water cell)
        return a;
    }
    if (!faceVisible || faceY != 1 || hitFace != 1) return a;   // only a top face anchors
    a.valid = true;
    a.placeDy = 1;                                    // the web lands in the cell above the block
    a.face = 1;
    return a;
}

} // namespace drain
} // namespace modules
} // namespace tyr
