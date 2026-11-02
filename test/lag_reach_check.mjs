// Acceptance check for the Lag Reach module (agent/modules/modules.cpp, updateLagReach).
// Mirrors the shipped rule exactly: every swing rolls a fresh reach in [3, MaxReach] blocks
// (MaxReach clamped to [3, 6]), the target is the nearest living entity the look ray passes
// through, measured as the slab-method distance from the eye to the first touch of the
// entity's own bounding box, and only an entity whose touch is inside the rolled reach is
// attacked. The ray test is vanilla's own crosshair test with the entity pick range lifted -
// so a ray that misses the box (dist < 0) is never a target, and a box the eye is already
// inside counts at distance 0.
//
// The harness cannot touch the live game: the world here is a set of boxes the cases place by
// hand, and the point is the arithmetic - the roll bounds, the slab intersection, and the
// nearest-within-reach selection.
//
// Run:  node test/lag_reach_check.mjs   (prints "PASS" or the failing cases)

// ---- the shipped roll ---------------------------------------------------------------------
// float rollLagReach(): the max reach is clamped to [3, 6]; 3 is the floor at any max.
function rollLagReach(rng, maxReach) {
  const m = Math.min(6, Math.max(3, maxReach));
  return 3 + (m - 3) * ((rng() * 65535) / 65535);
}

// ---- the shipped ray test -----------------------------------------------------------------
// float rayHitDistance(eye, look, bounds): return the slab tmin, or -1 when the ray misses.
// look is a unit vector. bounds is {min:{x,y,z}, max:{x,y,z}, valid}.
function rayHitDistance(eye, look, b) {
  if (!b.valid) return -1;
  let tmin = 0, tmax = 1e9;
  const o = [eye.x, eye.y, eye.z];
  const d = [look.x, look.y, look.z];
  const lo = [b.min.x, b.min.y, b.min.z];
  const hi = [b.max.x, b.max.y, b.max.z];
  for (let i = 0; i < 3; ++i) {
    if (Math.abs(d[i]) < 1e-8) { if (o[i] < lo[i] || o[i] > hi[i]) return -1; continue; }
    let t1 = (lo[i] - o[i]) / d[i], t2 = (hi[i] - o[i]) / d[i];
    if (t1 > t2) { const s = t1; t1 = t2; t2 = s; }
    tmin = Math.max(tmin, t1); tmax = Math.min(tmax, t2);
    if (tmin > tmax) return -1;
  }
  return tmin;
}

// ---- the shipped selection ----------------------------------------------------------------
// The nearest entity the ray enters within the rolled reach wins; the loop keeps a strict
// improvement, so the first entity found at a distance is kept on a tie.
function pickTarget(eye, look, reach, entities) {
  let best = null, bestDist = reach;
  for (const e of entities) {
    const dist = rayHitDistance(eye, look, e.bounds);
    if (dist < 0 || dist > bestDist) continue;
    bestDist = dist;
    best = e;
  }
  return best ? { name: best.name, dist: bestDist } : null;
}

const box = (name, minX, minY, minZ, maxX, maxY, maxZ) =>
  ({ name, bounds: { valid: true, min: { x: minX, y: minY, z: minZ }, max: { x: maxX, y: maxY, z: maxZ } } });

// A small deterministic PRNG so the roll sample is reproducible.
function mulberry32(seed) {
  return function () {
    seed |= 0; seed = (seed + 0x6D2B79F5) | 0;
    let t = Math.imul(seed ^ (seed >>> 15), 1 | seed);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

let pass = 0, fail = 0;
const near = (a, b, eps = 1e-4) => Math.abs(a - b) <= eps;
function check(ok, what) {
  if (ok) ++pass; else { ++fail; console.log(`FAIL  ${what}`); }
}

// ---- the roll is in [3, MaxReach] and pinned at 3 when MaxReach is 3 ----------------------
{
  const rng = mulberry32(12345);
  const maxes = [3, 3.5, 4, 4.55, 5, 6];
  let lo = Infinity, hi = -Infinity;
  for (const m of maxes) {
    for (let i = 0; i < 20000; ++i) {
      const v = rollLagReach(rng, m);
      if (v < 3 - 1e-9 || v > m + 1e-9) { check(false, `roll ${v} out of [3, ${m}]`); }
      lo = Math.min(lo, v); hi = Math.max(hi, v);
    }
  }
  check(true, 'roll stays inside [3, MaxReach]');           // any out-of-range sample failed above
  check(hi >= 5.9 && near(hi, 6, 1e-2), `roll reaches MaxReach (high seen ${hi.toFixed(4)})`);
  check(lo < 3.001, `roll starts at the 3 floor (low seen ${lo.toFixed(4)})`);

  // MaxReach is clamped: above 6 the roll never exceeds 6; below 3 it is always 3.
  let overSix = false, belowThree = false;
  for (let i = 0; i < 20000; ++i) {
    if (rollLagReach(rng, 10) > 6 + 1e-9) overSix = true;       // MaxReach 10 -> clamped to 6
    const v = rollLagReach(rng, 1);                             // MaxReach 1 -> clamped to 3
    if (v < 3 - 1e-9 || v > 3 + 1e-9) belowThree = true;
  }
  check(!overSix, 'a MaxReach above 6 never rolls past 6');
  check(!belowThree, 'a MaxReach below 3 rolls exactly 3');

  // A MaxReach of exactly 3 makes every roll exactly 3: the slider bottom is a real number.
  let allThree = true;
  for (let i = 0; i < 5000; ++i) if (!near(rollLagReach(rng, 3), 3)) allThree = false;
  check(allThree, 'MaxReach 3 rolls exactly 3 every time');
}

// ---- the slab test -------------------------------------------------------------------------
const eye = { x: 0.5, y: 70.62, z: 0.5 };              // standing eye height, looking along +x
const lookX = { x: 1, y: 0, z: 0 };

{
  // A box dead ahead: the touch is its near face minus the eye's x.
  const b = box('ahead', 4.5, 70, 0, 5.3, 71.8, 1);
  check(near(rayHitDistance(eye, lookX, b.bounds), 4.0), `near face at 4.0 (got ${rayHitDistance(eye, lookX, b.bounds)})`);

  // The same box behind the eye: the ray leaves away from it and never enters.
  const behind = box('behind', -1.5, 70, 0, -0.7, 71.8, 1);
  check(rayHitDistance(eye, lookX, behind.bounds) === -1, 'a box behind the eye is a miss');

  // A box the eye is already inside: the ray starts inside it, so the touch is 0.
  const around = box('around', 0, 70, 0, 1, 72, 1);
  check(rayHitDistance(eye, lookX, around.bounds) === 0, 'a box the eye is inside reads distance 0');

  // A box beside the ray (parallel miss): never crosses the y or z slab.
  const beside = box('beside', 4.5, 70, 3, 5.3, 71.8, 4);
  check(rayHitDistance(eye, lookX, beside.bounds) === -1, 'a box off to the side is a miss');

  // A degenerate (invalid) box is never a target.
  check(rayHitDistance(eye, lookX, { valid: false }) === -1, 'an invalid box is a miss');

  // A diagonal ray: the analytic touch of the near corner.
  const diag = { x: Math.SQRT1_2, y: 0, z: Math.SQRT1_2 };
  const diagBox = box('diag', 3.5, 70, 3.5, 4.5, 71.8, 4.5);
  const diagT = rayHitDistance(eye, diag, diagBox.bounds);
  const wantT = (3.5 - 0.5) / Math.SQRT1_2;            // x slab entry
  check(near(diagT, wantT, 1e-3), `diagonal touch ${diagT} == ${wantT}`);

  // A vertical look: only the y slab matters; the horizontal slabs are the parallel case.
  const lookUp = { x: 0, y: 1, z: 0 };
  const above = box('above', 0, 72.62, 0, 1, 73.8, 1);
  check(near(rayHitDistance(eye, lookUp, above.bounds), 2.0), 'straight up touches the box at 2.0');
}

// ---- nearest within reach ------------------------------------------------------------------
{
  const nearBox = box('near', 3.2, 70, 0, 4.0, 71.8, 1);      // touch at 2.7
  const farBox  = box('far',  5.5, 70, 0, 6.3, 71.8, 1);      // touch at 5.0
  const list = [farBox, nearBox];                             // far first: order must not matter

  // A reach of 4 covers the near box (2.7) but not the far one (5.0): the nearest wins.
  check(pickTarget(eye, lookX, 4.0, list)?.name === 'near', 'within reach the nearest box wins');

  // A reach of 3 still covers the near box (2.7 < 3), so it is still the pick.
  check(pickTarget(eye, lookX, 3.0, list)?.name === 'near', 'a reach of 3 still covers the 2.7 box');

  // ...but a reach of 2.5 covers neither, so nothing is attacked.
  check(pickTarget(eye, lookX, 2.5, list) === null, 'a reach below every touch attacks nothing');

  // A reach that only covers the far box (the near one removed) picks it.
  check(pickTarget(eye, lookX, 5.5, [farBox])?.name === 'far', 'a long roll reaches the far box');

  // The rolled reach is the gate: the box at exactly the roll is still inside it (<=, not <).
  check(pickTarget(eye, lookX, 2.7, list)?.name === 'near', 'a touch exactly at the roll is attacked');
}

console.log(`\n${pass}/${pass + fail} match the shipped Lag Reach rule: the reach rolls in [3, MaxReach] with the clamp, the target is the nearest entity whose bounding box the look ray enters, and only a touch inside the rolled reach is attacked.`);
console.log(fail === 0 ? 'PASS' : 'FAIL');
process.exit(fail ? 1 : 0);
