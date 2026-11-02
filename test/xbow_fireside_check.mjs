// Acceptance check for the Xbow Macro fire-cell selection (agent/modules/modules.cpp, XbowStep::Fire).
// Mirrors the shipped rule: solve the shot, fly it one step per tick like drawTrajectoryPreview,
// collect every candidate (arrow samples with the lowest row their box sweeps, plus the cells on
// the shot line beside the cart) and keep only candidates that pass every way a flame becomes a
// ghost: the cell can hold fire, its top face is one the eye is above, the click is inside block
// reach (checked on the close cases; the long-range cases are geometry-only, because the macro
// itself refuses to start beyond block reach on the rail) and it does not ask for a wild turn of
// the view. Among the survivors the cell nearest the cart wins, ties going to the smallest turn;
// a candidate needing more than a 120 degree turn is only taken when every candidate asks for
// one - that is the fix for the placement spinning the player 180 degrees when they were looking
// down at blocks a block apart.
//
// The world in this harness is flat with no occluders, so the line-of-sight part of the shipped
// rule (xbowFireVisible) is trivially clear here and is checked in-game, not here.

const CART = { x: 0.5, y: 70, z: 0.5 };            // g_xbowRailX/Z = 0, g_xbowRailY = 70
const CARTC = { x: 0.5, y: 70.3, z: 0.5 };         // cart centre = railY + 0.3
const CART_CELL = { cx: 0, cz: 0 };
const SPEED = 3.15, DRAG = 0.99, GRAVITY = 0.05;
const STAND_OFF = 1.5;
const EYE_MARGIN = 0.5, SHOOTER_MARGIN = 1.5;      // row <= eye.y - 0.5, cell dist >= 1.5
const BLOCK_REACH = 4.55;                          // player.blockInteractionRange() + tolerance
const TURN_LIMIT = 120;                            // degrees; beyond this a candidate is a last resort

function solveThrowAngle(speed, gravity, horizontal, vertical) {
  if (horizontal < 0.05 || speed < 0.05 || gravity <= 0) return null;
  const v2 = speed * speed;
  const disc = v2 * v2 - gravity * (gravity * horizontal * horizontal + 2 * vertical * v2);
  if (disc < 0) return null;
  return -Math.atan((v2 - Math.sqrt(disc)) / (gravity * horizontal)) * 57.29577951;
}

const angleDeltaDeg = (from, to) => {
  let d = (to - from) % 360;
  if (d >= 180) d -= 360;
  if (d < -180) d += 360;
  return d;
};

// Flat ground everywhere with its top at CART.y: only the ground row itself can hold a flame
// (below it the cell is solid, above it there is nothing to burn on) - the server's own rules.
const canHoldFire = (cx, cz, row) => row === Math.floor(CART.y) && !(cx === CART_CELL.cx && cz === CART_CELL.cz);

// facingOffset: degrees the player's own yaw is turned away from the shot yaw (0 = looking at the
// cart, the natural way the macro is started). checkReach: whether block reach is asserted (the
// long cases are geometry-only - the macro never starts there).
function select(eye, facingOffset = 0, checkReach = null) {
  const toCart = { x: CARTC.x - eye.x, y: CARTC.y - eye.y, z: CARTC.z - eye.z };
  const flat = Math.hypot(toCart.x, toCart.z);
  if (flat < STAND_OFF) return { rejected: 'stand back' };
  const pitchDeg = solveThrowAngle(SPEED, GRAVITY, flat, toCart.y);
  if (pitchDeg === null) return { rejected: 'out of reach' };
  if (checkReach === null) checkReach = flat + 1.6 <= BLOCK_REACH + 0.5;
  const shotYawDeg = Math.atan2(-toCart.x, toCart.z) * 57.29577951;
  const facingYawDeg = shotYawDeg + facingOffset;

  // cells on the shot line, nearest the cart first: step out from the cart centre toward the eye
  const ux = (eye.x - CARTC.x) / flat, uz = (eye.z - CARTC.z) / flat;
  const path = [];
  let lastX = CART_CELL.cx, lastZ = CART_CELL.cz;
  for (let back = 0.05; back <= 3.5 && path.length < 8; back += 0.05) {
    const cx = Math.floor(CARTC.x + ux * back), cz = Math.floor(CARTC.z + uz * back);
    if (cx === CART_CELL.cx && cz === CART_CELL.cz) continue;
    if (cx === lastX && cz === lastZ) continue;
    lastX = cx; lastZ = cz;
    path.push({ cx, cz, back });
  }

  // the arrow's own per-tick samples, stopping at the cart; each carries the lowest row its box sweeps
  const yaw = Math.atan2(-toCart.x, toCart.z), p = pitchDeg * Math.PI / 180;
  let pos = { ...eye };
  let vel = { x: -Math.sin(yaw) * Math.cos(p) * SPEED, y: -Math.sin(p) * SPEED, z: Math.cos(yaw) * Math.cos(p) * SPEED };
  const samples = [];
  for (let tick = 0; tick < 16 && samples.length < 16; ++tick) {
    pos = { x: pos.x + vel.x, y: pos.y + vel.y, z: pos.z + vel.z };
    vel = { x: vel.x * DRAG, y: vel.y * DRAG - GRAVITY, z: vel.z * DRAG };
    if (pos.y < CART.y - 1) break;
    const cx = Math.floor(pos.x), cz = Math.floor(pos.z);
    if (cx === CART_CELL.cx && cz === CART_CELL.cz) break;      // never light fire in the cart's cell
    if ((pos.x - CARTC.x) * toCart.x + (pos.z - CARTC.z) * toCart.z >= 0) break;   // or past it
    samples.push({ cx, cz, row: Math.floor(pos.y - 0.25), y: pos.y, dist: Math.hypot(pos.x - eye.x, pos.y - eye.y, pos.z - eye.z), cartDist: Math.hypot(pos.x - CARTC.x, pos.y - CARTC.y, pos.z - CARTC.z) });
  }

  // collect the candidates that survive every ghost check; the ranking distance is how far the
  // cell is from the CART (nearest the cart wins), while the 1.5 block margin is from the eye.
  const candidates = [];
  let skippedByHeight = false;
  const consider = (cx, cy, cz, eyeDist, cartDist) => {
    if (!canHoldFire(cx, cz, cy)) return;
    if (cy > eye.y - EYE_MARGIN || eyeDist < SHOOTER_MARGIN) { skippedByHeight = true; return; }
    if (checkReach && Math.hypot(cx + 0.5 - eye.x, cy - eye.y, cz + 0.5 - eye.z) > BLOCK_REACH) { skippedByHeight = true; return; }
    const toAimX = cx + 0.5 - eye.x, toAimZ = cz + 0.5 - eye.z;
    const aimYawDeg = Math.atan2(-toAimX, toAimZ) * 57.29577951;
    candidates.push({ cx, cz, row: cy, dist: cartDist, turn: Math.abs(angleDeltaDeg(facingYawDeg, aimYawDeg)) });
  };
  for (let i = samples.length - 1; i >= 0; --i) consider(samples[i].cx, samples[i].row, samples[i].cz, samples[i].dist, samples[i].cartDist);
  for (const c of path)
    consider(c.cx, Math.floor(CART.y), c.cz,
      Math.hypot(c.cx + 0.5 - eye.x, Math.floor(CART.y) + 0.5 - eye.y, c.cz + 0.5 - eye.z),
      Math.hypot(c.cx + 0.5 - CARTC.x, Math.floor(CART.y) + 0.5 - CARTC.y, c.cz + 0.5 - CARTC.z));

  // nearest the cart first, ties to the smallest turn; a turn over 120 degrees only as a last resort
  let chosen = null;
  for (let mode = 0; mode < 2 && !chosen; ++mode) {
    for (const c of candidates) {
      if (mode === 0 && c.turn > TURN_LIMIT) continue;
      if (!chosen || c.dist < chosen.dist - 0.01 ||
          (Math.abs(c.dist - chosen.dist) <= 0.01 && c.turn < chosen.turn)) chosen = c;
    }
  }
  if (!chosen) return skippedByHeight ? { rejected: 'above eye or out of reach' } : { rejected: 'no room' };
  return { ...chosen, source: samples.some(s => s.cx === chosen.cx && s.cz === chosen.cz && s.row === chosen.row) ? 'sample' : 'line', path, samples, flat, candidates: candidates.length };
}

const cases = [
  // [name, eyeY, eyeX, eyeZ, expect, facingOffset]
  // expect: 'place' (the eye is above the lit face) or 'refuse' (every flame cell on the line is
  // above the eye: the old rule lit a ghost there).
  ['level, 8 west', CART.y + 1.62, CART.x - 8, CART.z, 'place', 0],
  ['level, 8 east', CART.y + 1.62, CART.x + 8, CART.z, 'place', 0],
  ['level, diag SW', CART.y + 1.62, CART.x - 5.66, CART.z - 5.66, 'place', 0],
  ['level, diag NE', CART.y + 1.62, CART.x + 5.66, CART.z + 5.66, 'place', 0],
  ['level, off-axis', CART.y + 1.62, CART.x - 6.93, CART.z - 4.0, 'place', 0],
  ['level, 20 west', CART.y + 1.62, CART.x - 20, CART.z, 'place', 0],
  // A cart one block up: the eye (ground + 1.62) is still 0.62 above the flame's face, so the
  // click is real and the flame goes down - only from two blocks up is the face out of reach.
  ['LEDGE 1 low, 5 west', CART.y - 1 + 1.62, CART.x - 5, CART.z, 'place', 0],
  ['LEDGE 1 low, 7 west', CART.y - 1 + 1.62, CART.x - 7, CART.z, 'place', 0],
  ['LEDGE 1 low, diag SW', CART.y - 1 + 1.62, CART.x - 5, CART.z - 5, 'place', 0],
  ['LEDGE 2 low, 6 west', CART.y - 2 + 1.62, CART.x - 6, CART.z, 'refuse', 0],
  ['LEDGE 2 low, 9 west', CART.y - 2 + 1.62, CART.x - 9, CART.z, 'refuse', 0],
  ['LEDGE 3 low, 8 west', CART.y - 3 + 1.62, CART.x - 8, CART.z, 'refuse', 0],
  ['close 4 west', CART.y + 1.62, CART.x - 4, CART.z, 'place', 0],
  // The 180-degree-spin report: two blocks from the cart looking the other way (the player was
  // looking down at the blocks a block apart). The placement must still find a flame - the
  // candidate that needs the least turn of the view, never a refusal and never a ghost cell.
  ['close, facing away', CART.y + 1.62, CART.x - 3.2, CART.z, 'place', 180],
  ['close, facing side', CART.y + 1.62, CART.x - 2.6, CART.z, 'place', 90],
];

let pass = 0, fail = 0, beside = 0, maxTurnReport = 0;
for (const [name, ey, ex, ez, expect, facing] of cases) {
  const eye = { x: ex, y: ey, z: ez };
  const s = select(eye, facing);
  if (expect === 'refuse') {
    const ok = !!s.rejected;
    ok ? ++pass : ++fail;
    console.log(`${ok ? 'PASS' : 'FAIL'}  ${name.padEnd(21)} ${s.rejected ? `refused (${s.rejected}) - the old rule lit a ghost cell here` : `LIT (${s.cx},${s.cz}) row ${s.row} - should have been refused`}`);
    continue;
  }
  if (s.rejected) { console.log(`FAIL  ${name.padEnd(21)} unexpectedly refused: ${s.rejected}`); ++fail; continue; }
  const toward = { x: CARTC.x - eye.x, z: CARTC.z - eye.z };
  const n = Math.hypot(toward.x, toward.z);
  const off = { x: (s.cx + 0.5) - CARTC.x, z: (s.cz + 0.5) - CARTC.z };
  const onSide = (off.x * toward.x + off.z * toward.z) / n < 0;
  const back = Math.hypot(off.x, off.z);
  const onLine = s.path.some(c => c.cx === s.cx && c.cz === s.cz) || s.samples.some(x => x.cx === s.cx && x.cz === s.cz);
  const reachable = s.row <= eye.y - EYE_MARGIN;
  const ok = onSide && onLine && reachable;
  ok ? ++pass : ++fail;
  if (back <= 1.45) ++beside;
  maxTurnReport = Math.max(maxTurnReport, s.turn);
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${name.padEnd(21)} fire (${String(s.cx).padStart(2)},${String(s.cz).padStart(2)}) row ${s.row}  `
    + `${back.toFixed(1)}b from the cart  ${onSide ? 'shooter side' : 'FAR SIDE'}  `
    + `${onLine ? 'on the shot' : 'OFF THE LINE'}  `
    + `${reachable ? 'eye above the lit face' : 'FACE ABOVE THE EYE'}  `
    + `turn ${s.turn.toFixed(0)}deg  ${s.source === 'sample' ? 'arrow sampled there' : 'line fallback'}`);
}
console.log(`\n${pass}/${pass + fail} match the shipped rule: every placed flame is on the shot line, on the shooter's side, lit from a face the eye is above, and never needs more than a ${TURN_LIMIT} degree turn when a candidate needing less exists (largest turn taken here: ${maxTurnReport.toFixed(0)}deg); every cell above the eye is refused instead of lit.`);
process.exit(fail ? 1 : 0);
