// Acceptance check for two shipped rules that carry no world state:
//   * Triggerbot "Criticals" (agent/modules/modules.cpp, updateTriggerbot / playerWouldCrit)
//   * Diagonal Pearl Catch's intercept solver and Dynamic Timing gate
//     (agent/modules/modules.cpp, updateDiagonalPearlCatch / diagPearlPosAfter)
//
// The harness mirrors the arithmetic exactly. It cannot touch the live game: the pearl here is a
// free-flight model the cases drive by hand (isAir is always true unless a case says otherwise),
// and the point is the geometry - where the charge's drag-accurate flight meets the pearl's arc,
// whether the charge's flight really passes through the pearl (a near miss is not a catch), and
// when each Criticals mode lets a swing out.
//
// Run:  node test/spear_crit_pearl_check.mjs   (prints "PASS" or the failing cases)

// ---- the shipped phrasing ----------------------------------------------------------------
// float chargeDistance(int t): 1.5 * (1 - 0.99^t) / 0.01 blocks after t ticks.
function chargeDistance(t) {
  return 1.5 * (1 - Math.pow(0.99, t)) / 0.01;
}

// Vec3 diagPearlPosAfter(pos, vel, ticks): advance the pearl, stopping at the first cell that is
// not air (the same model simulatePearlLanding uses). Returns { pos, vel } after the advance.
function pearlPosAfter(pos, vel, ticks, isAir = () => true) {
  let p = { ...pos }, v = { ...vel };
  for (let i = 0; i < ticks; ++i) {
    p = { x: p.x + v.x, y: p.y + v.y, z: p.z + v.z };
    v = { x: v.x * 0.99, y: v.y * 0.99 - 0.03, z: v.z * 0.99 };
    if (!isAir(p)) break;
  }
  return { pos: p, vel: v };
}

const sub = (a, b) => ({ x: a.x - b.x, y: a.y - b.y, z: a.z - b.z });
const len = (a) => Math.sqrt(a.x * a.x + a.y * a.y + a.z * a.z);

// The shipped solve: the pearl's own flight is sampled once (61 ticks), then every candidate
// meeting tick is scored by the real distance from the pearl's position on that tick to the
// charge's own segment for that tick. A wind charge only detonates when its swept segment really
// passes through the pearl, so being two blocks away is a miss, not a catch - that distinction is
// the whole point of the score. The best scoring tick is the one aimed at, and a tie keeps the
// nearest one (which is also what keeps a charge from being aimed at the thrower's own feet).
function solveCatch(eye, pearlPos, pearlVel, isAir = () => true) {
  const path = [{ ...pearlPos }];
  {
    let p = { ...pearlPos }, v = { ...pearlVel }, landed = false;
    for (let t = 1; t <= 61; ++t) {
      if (!landed) {
        p = { x: p.x + v.x, y: p.y + v.y, z: p.z + v.z };
        v = { x: v.x * 0.99, y: v.y * 0.99 - 0.03, z: v.z * 0.99 };
        if (!isAir(p)) landed = true;
      }
      path.push({ ...p });
    }
  }
  let bestMiss = Infinity, aim = { ...eye }, tick = 0, haveAim = false;
  for (let meet = 1; meet <= 60; ++meet) {
    const dir = sub(path[meet], eye);
    const l = len(dir);
    if (l < 1) continue;
    const d = { x: dir.x / l, y: dir.y / l, z: dir.z / l };
    let miss = Infinity;
    for (let k = 1; k <= 61; ++k) {
      const rel = sub(path[k], eye);
      const along = Math.min(Math.max(rel.x * d.x + rel.y * d.y + rel.z * d.z,
                                      chargeDistance(k - 1)), chargeDistance(k));
      const closest = { x: eye.x + d.x * along, y: eye.y + d.y * along, z: eye.z + d.z * along };
      const dist = len(sub(path[k], closest));
      if (dist < miss) miss = dist;
    }
    if (miss < bestMiss) { bestMiss = miss; aim = { ...path[meet] }; tick = meet; haveAim = true; }
  }
  return { bestMiss, aim, aimDist: len(sub(aim, eye)), tick, haveAim };
}

// The shipped view direction: yaw/pitch (degrees) -> unit vector, matching the client's axes.
function lookDir(yawDeg, pitchDeg) {
  const y = yawDeg * Math.PI / 180, p = pitchDeg * Math.PI / 180;
  return { x: -Math.sin(y) * Math.cos(p), y: -Math.sin(p), z: Math.cos(y) * Math.cos(p) };
}

// ---- the shipped Criticals rule ----------------------------------------------------------
// bool playerWouldCrit(player): airborne, not gliding, and has been falling.
function playerWouldCrit(p) { return !p.onGround && !p.gliding && p.fallDistance > 0; }

// The decision updateTriggerbot makes each frame it has a valid target. Mirrors the mode gate:
// Default always swings; OnlyCrit swings only on a crit; PrioCrit swings on a crit, and otherwise
// only once the wait since the last crit has elapsed (the wait is refreshed by every crit).
function wouldSwing(mode, crit, now, state, prioWaitMs) {
  if (mode === 'Default') return true;
  if (!crit && mode === 'OnlyCrit') return false;
  if (crit) { state.waitSince = now; return true; }
  return now - state.waitSince >= prioWaitMs / 1000;
}

let pass = 0, fail = 0;
const near = (a, b, eps = 1e-6) => Math.abs(a - b) <= eps;
function check(ok, what) { if (ok) ++pass; else { ++fail; console.log(`FAIL  ${what}`); } }

// ---- criticals: the airborne + fall signal captures the jump and knockback crits ----------
{
  check(!playerWouldCrit({ onGround: true, gliding: false, fallDistance: 5 }), 'a grounded hit is not a crit');
  check(!playerWouldCrit({ onGround: false, gliding: false, fallDistance: 0 }), 'an airborne hit with no fall is not a crit');
  check(!playerWouldCrit({ onGround: false, gliding: true, fallDistance: 3 }), 'an elytra glide is not a crit');
  check(playerWouldCrit({ onGround: false, gliding: false, fallDistance: 0.1 }), 'a jump off the ground is a crit');
  // Knockback: the same airborne + fall signal, reached without the player ever jumping.
  check(playerWouldCrit({ onGround: false, gliding: false, fallDistance: 0.4 }), 'a knockback launch is a crit');
}

// ---- criticals: each mode's swing gate ---------------------------------------------------
{
  const wait = 400;
  // Default never holds a swing, crit or not.
  let s = { waitSince: 0 };
  check(wouldSwing('Default', false, 10, s, wait), 'Default swings on a plain hit');
  check(wouldSwing('Default', true, 10, s, wait), 'Default swings on a crit');

  // OnlyCrit never swings without a crit.
  s = { waitSince: 0 };
  check(!wouldSwing('OnlyCrit', false, 10, s, wait), 'Only Crit holds a plain hit');
  check(wouldSwing('OnlyCrit', true, 10, s, wait), 'Only Crit swings on a crit');

  // PrioCrit holds a plain hit only until the wait has elapsed, then falls back.
  s = { waitSince: 5 };
  check(!wouldSwing('PrioCrit', false, 5.2, s, wait), 'Prio Crit holds a plain hit inside the wait');
  check(wouldSwing('PrioCrit', false, 5.5, s, wait), 'Prio Crit falls back after the wait');
  // A crit refreshes the wait, so the fallback is pushed out again.
  s = { waitSince: 5 };
  check(wouldSwing('PrioCrit', true, 5.2, s, wait), 'Prio Crit swings on a crit');
  check(s.waitSince === 5.2, 'a crit refreshes the Prio Crit wait');
  check(!wouldSwing('PrioCrit', false, 5.4, s, wait), 'after a crit the wait starts over');
}

// ---- intercept: at the throw instant the charge and the pearl coincide ---------------------
{
  const eye = { x: 0.5, y: 70.62, z: 0.5 };
  const vel = { x: 1.5, y: 0, z: 0 };                     // level, pearl still at the eye
  const r = solveCatch(eye, eye, vel);
  // The pearl and the charge share the same launch, so the charge passes through the pearl every
  // tick: the only miss left is the pearl's own gravity sag, a fraction of a block.
  check(r.bestMiss < 0.3, `a pearl still at the eye is passed through (miss ${r.bestMiss.toFixed(3)})`);
  check(r.aimDist > 1.0, 'the throw-instant meeting point is a tick or more out');
}

// ---- intercept: a pearl with a head start is short by exactly that head start ---------------
// Both fly at 1.5 blocks/tick with the same 1% drag, so a charge thrown later can never close
// the gap - the miss it can report is the head start. That is what makes the catch a function of
// HOW FAR the pearl already is, which is what Dynamic Timing gates on.
{
  const eye = { x: 0.5, y: 70.62, z: 0.5 };
  const vel = { x: 1.5, y: 0, z: 0 };
  const inFlight = pearlPosAfter(eye, vel, 2);            // ~3 blocks of head start
  const headStart = len(sub(inFlight.pos, eye));
  const r = solveCatch(eye, inFlight.pos, inFlight.vel);
  check(Math.abs(r.bestMiss - headStart) < 0.8, `the miss is the head start (miss ${r.bestMiss.toFixed(3)} vs ${headStart.toFixed(3)})`);

  // A shallow (30 degree) throw keeps the same property: the charge follows the same ray and is
  // short by the head start all the way out, so the score stays a miss and never reaches zero.
  const dir = lookDir(0, 30);
  const shallowVel = { x: dir.x * 1.5, y: dir.y * 1.5, z: dir.z * 1.5 };
  const shallow = pearlPosAfter(eye, shallowVel, 2);
  const rs = solveCatch(eye, shallow.pos, shallow.vel);
  check(rs.bestMiss > 1.5, `a 30 degree throw with a head start stays a miss (miss ${rs.bestMiss.toFixed(3)})`);
}

// ---- intercept: the solve follows the pearl's LIVE velocity ---------------------------------
// Two pearls at the same position, one wind-charged and faster: the aim must unfold further along
// each tick for the faster one, because the solver is driven by the velocity it was handed.
{
  const eye = { x: 0.5, y: 70.62, z: 0.5 };
  const start = { x: 3.0, y: 70.62, z: 0.5 };
  const slow = solveCatch(eye, start, { x: 1.5, y: 0, z: 0 });
  const fast = solveCatch(eye, start, { x: 3.0, y: 0, z: 0 }); // boosted by a wind charge
  check(fast.aimDist > slow.aimDist + 0.5, `a boosted pearl's aim follows its live velocity (${fast.aimDist.toFixed(3)} > ${slow.aimDist.toFixed(3)})`);
}

// ---- Dynamic Timing gate: the pearl has to be in the window where a catch is really possible
// (inside the charge's explosion reach, and not detonating on the thrower's feet) -------------
{
  const kCatchTolerance = 0.6, kMinInterceptDistance = 2.0;
  const gate = (r) => r.haveAim && r.bestMiss <= kCatchTolerance && r.aimDist >= kMinInterceptDistance;
  const eye = { x: 0.5, y: 70.62, z: 0.5 };
  const vel = { x: 1.5, y: 0, z: 0 };

  // A pearl a couple of blocks out on a level throw is the case the user reported: the charge
  // follows it on the same ray and stays the head start behind it, so it streaks past the pearl
  // (which looks exactly like a hit) without ever touching it. It has to be refused.
  const mid = pearlPosAfter(eye, vel, 2);
  const nearMiss = solveCatch(eye, mid.pos, mid.vel);
  check(nearMiss.bestMiss > kCatchTolerance,
        `a charge that would streak past the pearl is refused (miss ${nearMiss.bestMiss.toFixed(3)})`);
  check(!gate(nearMiss), 'the near miss does not pass the gate');

  // A pearl still at the thrower's feet is refused (the "under you" charge).
  const atFeet = solveCatch(eye, eye, vel);
  check(!gate(atFeet), `a charge at the thrower's feet is refused (dist ${atFeet.aimDist.toFixed(3)})`);

  // A pearl that already landed beside the thrower is refused.
  const landed = solveCatch(eye, eye, { x: 1.5, y: -0.2, z: 0 }, () => false);
  check(!gate(landed), 'a pearl landed at your feet is refused');

  // A pearl past the charge's own reach is no catch at all.
  const farAway = solveCatch(eye, { x: 500, y: 70.62, z: 0.5 }, vel);
  check(!gate(farAway), 'a pearl out of the charge\'s reach is no catch');
}

// ---- the two closed forms the solve rests on ---------------------------------------------
{
  check(near(chargeDistance(1), 1.5), 'the charge covers 1.5 blocks on its first tick');
  check(chargeDistance(2) > chargeDistance(1), 'the charge only ever gains distance');
  check(chargeDistance(60) < 1.5 / 0.01, 'the charge never passes its 150 block terminal distance');
  const out = pearlPosAfter({ x: 0, y: 0, z: 0 }, { x: 1.5, y: 0, z: 0 }, 1);
  check(near(out.pos.x, 1.5), 'the pearl advances one tick by its velocity');
  check(near(out.vel.x, 1.5 * 0.99), 'the pearl keeps 99% of its speed each tick');
  check(near(out.vel.y, -0.03), 'the pearl is pulled down by gravity');
}

console.log(`\n${pass}/${pass + fail} match the shipped rules: a crit is airborne + falling (jump or knockback), the Criticals modes gate the swing accordingly, and the pearl catch solves the charge's drag-accurate flight against the pearl's own arc with a Dynamic Timing gate.`);
console.log(fail === 0 ? 'PASS' : 'FAIL');
process.exit(fail ? 1 : 0);
