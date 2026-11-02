// ============================================================================
//  Passive — the head web rule (AutoWeb).
//
//  Pure: no JNI and no ImGui, so test/auto_web_check.mjs exercises exactly the
//  geometry agent/modules/modules.cpp uses when AutoWeb drops a cobweb behind a
//  player you just hit: one web in the cell against the block behind them, and
//  one in the cell above it.
//
//  Everything is in block cells. "Behind" is the horizontal step from the
//  target's own cell to the block their back is against, taken from the line
//  between the attacker and the target — the same direction the hit travels.
// ============================================================================
#pragma once

namespace tyr {
namespace modules {
namespace headweb {

// How far off the block behind them the target may be and still count as "right against it",
// in blocks. A player hugging a wall sits within a small fraction of a block of its face.
inline constexpr float kMaxGap = 0.4f;

struct Plan {
    bool valid = false;
    int dx = 0, dz = 0;        // the step from the target's cell to the block behind them
    int wallY = 0;             // the block behind them the webs lean on
    int lowY = 0, highY = 0;   // the two cells the webs go in (the target's own cells)
    int face = 1;              // the face of the wall block each placement clicks
    bool highOnWall = false;   // true: the high web leans on the wall too; false: it sits on the low web
};

// feetY is the cell the target stands in. behind0 / behind1 / behind2 answer whether the cell
// straight behind them holds a real block at feetY, feetY+1 and feetY+2. `gap` is the target's
// distance to the face of the block behind them, in blocks.
//
// The lowest solid cell behind them becomes the wall; the webs go into the two cells against it,
// which are the target's own cell at that level and the one above. The lower web always leans on
// the wall. The upper web leans on the wall too when the block behind one level up is solid, and
// otherwise sits on the lower web once it exists.
inline Plan plan(int feetY, int backX, int backZ, float gap,
                 bool behind0, bool behind1, bool behind2) {
    Plan p;
    if (backX == 0 && backZ == 0) return p;
    if (!(behind0 || behind1)) return p;
    if (!(gap >= 0.f) || gap > kMaxGap) return p;
    const int wallY = behind0 ? feetY : feetY + 1;
    p.valid = true;
    p.dx = backX;
    p.dz = backZ;
    p.wallY = wallY;
    p.lowY = wallY;
    p.highY = wallY + 1;
    // The face of the wall block that looks at the target: the one the placement clicks.
    // 0 = down, 1 = up, 2 = north (-Z), 3 = south (+Z), 4 = west (-X), 5 = east (+X).
    p.face = backX > 0 ? 4 : backX < 0 ? 5 : backZ > 0 ? 2 : 3;
    p.highOnWall = (p.highY == feetY + 1) ? behind1 : behind2;
    return p;
}

} // namespace headweb
} // namespace modules
} // namespace tyr
