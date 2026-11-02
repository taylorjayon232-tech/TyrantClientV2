// Acceptance check for the two rules this round added that carry no world state:
//   * AutoWeb's head web (agent/modules/head_web.h, used by updateAutoWeb in modules.cpp)
//   * Triggerbot V2's OnHit Allowance window (agent/modules/modules.cpp, updateAutoShieldBreak)
//
// The harness mirrors the arithmetic exactly. It cannot touch the live game: the "world" here is
// a hand-written block lookup driven by the cases, and the point is the geometry - which cell the
// two cobwebs go in, which face each placement clicks, and when a Triggerbot V2 swing still counts
// as the click Auto Shield Break's OnHit mode is waiting for.
//
// Run:  node test/auto_web_check.mjs   (prints "PASS" or the failing cases)

const kMaxGap = 0.4;              // head_web.h
const kOnHitAllowanceWindow = 0.15; // modules.cpp

// ---- the shipped head-web rule ----------------------------------------------------------
// headweb::plan(feetY, backX, backZ, gap, behind0, behind1, behind2)
function plan(feetY, backX, backZ, gap, behind0, behind1, behind2) {
  const p = { valid: false, dx: 0, dz: 0, wallY: 0, lowY: 0, highY: 0, face: 1, highOnWall: false };
  if (backX === 0 && backZ === 0) return p;
  if (!(behind0 || behind1)) return p;
  if (!(gap >= 0) || gap > kMaxGap) return p;
  const wallY = behind0 ? feetY : feetY + 1;
  p.valid = true;
  p.dx = backX;
  p.dz = backZ;
  p.wallY = wallY;
  p.lowY = wallY;
  p.highY = wallY + 1;
  p.face = backX > 0 ? 4 : backX < 0 ? 5 : backZ > 0 ? 2 : 3;
  p.highOnWall = (p.highY === feetY + 1) ? behind1 : behind2;
  return p;
}

// The caller's side: the horizontal step from us through the target, and how far the target is
// from the face of the block behind them. Mirrors updateAutoWeb.
function geometry(self, target) {
  const bx = Math.floor(target.x), bz = Math.floor(target.z);
  const feetY = Math.floor(target.y);
  const dx = target.x - self.x, dz = target.z - self.z;
  let backX = 0, backZ = 0;
  if (Math.abs(dx) >= Math.abs(dz)) backX = dx >= 0 ? 1 : -1;
  else backZ = dz >= 0 ? 1 : -1;
  let gap = 0;
  if (backX > 0) gap = (bx + 1) - target.x;
  else if (backX < 0) gap = target.x - bx;
  else if (backZ > 0) gap = (bz + 1) - target.z;
  else gap = target.z - bz;
  return { bx, bz, feetY, backX, backZ, gap };
}

// ---- the shipped OnHit Allowance gate ---------------------------------------------------
// updateAutoShieldBreak (OnHit mode): a real left click, or -- with OnHit Allowance on --
// a Triggerbot V2 swing inside the window.
function onHitAllowed(clicked, allowance, now, tapAt) {
  const allowed = allowance && now - tapAt <= kOnHitAllowanceWindow;
  return clicked || allowed;
}

let pass = 0, fail = 0;
function check(ok, what) { if (ok) ++pass; else { ++fail; console.log(`FAIL  ${what}`); } }

// ---- head web: the two cells go against the block behind them, at its level and one above -----
{
  // Target at feet Y = 70, wall three blocks tall to the east.
  const self = { x: 0.5, y: 70.62, z: 0.5 };
  const target = { x: 2.98, y: 70.0, z: 0.5 };            // hugging the east face of cell 3
  const g = geometry(self, target);
  check(g.bx === 2 && g.bz === 0, 'the target cell is found');
  check(g.backX === 1 && g.backZ === 0, 'the block behind them is the one we are looking through');
  check(g.gap < 0.05, `hugging the wall reads as a small gap (${g.gap.toFixed(3)})`);

  const p = plan(g.feetY, g.backX, g.backZ, g.gap, true, true, true);
  check(p.valid, 'a wall three blocks tall is a head web');
  check(p.wallY === 70, 'the wall is taken at the target\'s own feet level');
  check(p.lowY === 70 && p.highY === 71, 'the webs go in the target\'s own cell and the one above (their head)');
  check(p.face === 4, 'the west face of an east wall is the face clicked');
  check(p.highOnWall, 'the upper web leans on the wall when the block above is solid');

  // A one-block wall: the wall is at their feet, the upper web has to sit on the lower web.
  const p1 = plan(70, 1, 0, 0.02, true, false, false);
  check(p1.valid && p1.wallY === 70 && p1.lowY === 70 && p1.highY === 71, 'a one-block wall still webs both cells');
  check(!p1.highOnWall, 'with nothing solid above, the upper web sits on the lower web');

  // Only the block at body height is solid (a slab-height shelf): the wall is that block.
  const p2 = plan(70, 1, 0, 0.02, false, true, false);
  check(p2.valid && p2.wallY === 71 && p2.lowY === 71 && p2.highY === 72, 'a body-height block puts the pair one level up');
  check(!p2.highOnWall, 'and the upper web still sits on the lower web when nothing is behind it');
  const p2b = plan(70, 1, 0, 0.02, false, true, true);
  check(p2b.highOnWall, 'a body-height wall with a solid cell above leans the upper web on the wall');
}

// ---- head web: every direction maps to the right face -----------------------------------------
{
  const cases = [
    { b: [1, 0], face: 4 },    // wall to the east -> click its west face
    { b: [-1, 0], face: 5 },   // west  -> east face
    { b: [0, 1], face: 2 },    // south -> north face
    { b: [0, -1], face: 3 },   // north -> south face
  ];
  for (const c of cases) {
    const p = plan(70, c.b[0], c.b[1], 0.0, true, true, true);
    check(p.valid && p.face === c.face, `direction (${c.b[0]},${c.b[1]}) clicks face ${c.face}`);
  }
}

// ---- head web: it refuses anything that is not a real head web ---------------------------------
{
  check(!plan(70, 0, 0, 0.0, true, true, true).valid, 'no direction is refused');
  check(!plan(70, 1, 0, 0.0, false, false, false).valid, 'no block behind them is refused');
  check(!plan(70, 1, 0, 0.0, false, false, true).valid, 'a block only high above is refused (they are not against it)');
  check(!plan(70, 1, 0, 0.9, true, true, true).valid, 'standing a block away from the wall is refused');
  check(!plan(70, 1, 0, kMaxGap + 0.01, true, true, true).valid, 'just past the gap limit is refused');
  check(plan(70, 1, 0, kMaxGap, true, true, true).valid, 'exactly at the gap limit is accepted');
  check(!plan(70, 1, 0, NaN, true, true, true).valid, 'an unreadable gap is refused');
}

// ---- head web: the gap is measured to the face they are against, not to the cell centre -------
{
  const self = { x: 0.5, y: 70.62, z: 0.5 };
  // Standing in the middle of a cell with the wall to the east: 0.5 away, so not a head web.
  const mid = geometry(self, { x: 2.5, y: 70.0, z: 0.5 });
  check(Math.abs(mid.gap - 0.5) < 1e-9, `mid-cell reads a 0.5 gap (${mid.gap.toFixed(3)})`);
  check(!plan(mid.feetY, mid.backX, mid.backZ, mid.gap, true, true, true).valid, 'a mid-cell target is not webbed');
  // Same cell, pressed against the west wall instead: the direction flips, the gap is small.
  const west = geometry({ x: 4.5, y: 70.62, z: 0.5 }, { x: 2.02, y: 70.0, z: 0.5 });
  check(west.backX === -1, 'a target west of us steps west to the wall behind them');
  check(west.gap < 0.05, `pressed against the west wall reads a small gap (${west.gap.toFixed(3)})`);
  const pw = plan(west.feetY, west.backX, west.backZ, west.gap, true, true, true);
  check(pw.valid && pw.face === 5, 'and clicks the east face of that wall');
}

// ---- OnHit Allowance: the swing the shield break is waiting for -------------------------------
{
  check(onHitAllowed(true, false, 10, -100), 'a real click always arms OnHit mode');
  check(!onHitAllowed(false, true, 10, -100), 'without a recent swing, OnHit mode still waits');

  // A swing at t = 10 keeps the shield break armed for the window.
  check(onHitAllowed(false, true, 10.0, 10.0), 'the swing itself arms OnHit mode');
  check(onHitAllowed(false, true, 10.14, 10.0), 'so does the frame just inside the window');
  check(!onHitAllowed(false, true, 10.16, 10.0), 'and a frame past the window does not');

  // Auto Shield Break runs before the triggerbot in a frame, so the window has to survive at
  // least the next frame, and a full tick of latency, with room to spare.
  const frame = 1 / 60;
  check(onHitAllowed(false, true, 10 + frame, 10), 'the next frame sees the swing');
  check(onHitAllowed(false, true, 10 + frame * 7, 10), 'seven frames later the swing still counts');
  check(!onHitAllowed(false, true, 10 + frame * 20, 10), 'twenty frames later it is long gone');

  // With OnHit Allowance off, a triggerbot swing is invisible to the shield break.
  check(!onHitAllowed(false, false, 10 + frame, 10), 'with the switch off the swing does not arm OnHit mode');
}

console.log(`\n${pass}/${pass + fail} match the shipped rules: a head web goes in the target's own cell and the one above it, against the block behind them, with the face of the wall clicked, and a Triggerbot V2 swing arms Auto Shield Break's OnHit mode for one short window.`);
console.log(fail === 0 ? 'PASS' : 'FAIL');
process.exit(fail ? 1 : 0);
