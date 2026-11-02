#!/usr/bin/env node
// ============================================================================
//  Passive -- check the Fabric jar really delivers the client.
//
//  The jar claims three things. This harness runs all three against the built
//  artifacts, on a live JVM:
//
//    1. It carries the client. The jar's payload (passive/passive_agent.dll,
//       passive/passive_bridge.dll, the Inter faces, fabric.mod.json) is read out
//       of the zip and compared with what the native build produced, and the
//       metadata is checked against what Fabric Loader requires.
//    2. It maps the client into the game process. A real JVM runs the mod's own
//       com.passive.fabric.NativeLoader (not a copy of its logic) and a real
//       GLFW window presents frames, because the client draws from the swap
//       hook: the log then has to show the hooks installing and the overlay
//       bootstrapping on a render frame.
//    3. Uninjecting from the jar version leaves the process clean, so the
//       standalone injector can inject the same DLL again. The client is asked
//       to unload through the bridge, and then the real passive_injector.exe
//       injects that process and has to report the mapped DLL and the overlay
//       handshake.
//
//  Run it after `fabric\build-jar.bat` (which builds the native side too).
//
//    node test/fabric_jar_check.mjs
//
//  Machine-local dependencies, like the other live harnesses here: a JDK 21,
//  the Prism instance's Minecraft client jar and LWJGL 3.3.3 libraries (found
//  automatically, TYRANT_PRISM / TYRANT_MC_JAR override).
// ============================================================================
import { spawn, spawnSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const work = path.join(root, 'test', 'build', 'fabric-jar-check');

let passed = 0;
const failures = [];
function check(ok, label) {
  if (ok) { passed++; console.log(`  ok   ${label}`); }
  else { failures.push(label); console.log(`  FAIL ${label}`); }
  return ok;
}
function skip(label, why) {
  console.log(`  skip ${label} (${why})`);
}

// ---------------------------------------------------------------------------
// Minimal zip reader: the jar is the artifact under test, so read it directly
// instead of shelling out to a tool that might not be on PATH.
// ---------------------------------------------------------------------------
function readZip(file) {
  const buf = fs.readFileSync(file);
  let eocd = -1;
  const floor = Math.max(0, buf.length - 22 - 0xFFFF);
  for (let i = buf.length - 22; i >= floor; --i) {
    if (buf.readUInt32LE(i) === 0x06054b50) { eocd = i; break; }
  }
  if (eocd < 0) throw new Error(`not a zip archive: ${file}`);
  const count = buf.readUInt16LE(eocd + 10);
  let off = buf.readUInt32LE(eocd + 16);
  const entries = new Map();
  for (let i = 0; i < count; ++i) {
    if (buf.readUInt32LE(off) !== 0x02014b50) throw new Error(`bad central directory at ${off}`);
    const method = buf.readUInt16LE(off + 10);
    const compSize = buf.readUInt32LE(off + 20);
    const nameLen = buf.readUInt16LE(off + 28);
    const extraLen = buf.readUInt16LE(off + 30);
    const commentLen = buf.readUInt16LE(off + 32);
    const localOff = buf.readUInt32LE(off + 42);
    const name = buf.toString('utf8', off + 46, off + 46 + nameLen);
    const lNameLen = buf.readUInt16LE(localOff + 26);
    const lExtraLen = buf.readUInt16LE(localOff + 28);
    const dataStart = localOff + 30 + lNameLen + lExtraLen;
    const raw = buf.subarray(dataStart, dataStart + compSize);
    entries.set(name, method === 0 ? Buffer.from(raw) : zlib.inflateRawSync(raw));
    off += 46 + nameLen + extraLen + commentLen;
  }
  return entries;
}

function walk(dir, out = []) {
  let listing;
  try { listing = fs.readdirSync(dir, { withFileTypes: true }); } catch { return out; }
  for (const entry of listing) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) walk(full, out);
    else out.push(full);
  }
  return out;
}

function newest(files) {
  return files.filter(f => fs.existsSync(f))
              .sort((a, b) => fs.statSync(b).mtimeMs - fs.statSync(a).mtimeMs)[0];
}

function findJdk() {
  const home = process.env.JAVA_HOME;
  if (home && fs.existsSync(path.join(home, 'bin', 'javac.exe'))) {
    return { javac: path.join(home, 'bin', 'javac.exe'), java: path.join(home, 'bin', 'java.exe') };
  }
  const which = (exe) => {
    const r = spawnSync('where', [exe], { encoding: 'utf8' });
    return r.status === 0 ? r.stdout.split(/\r?\n/)[0].trim() : null;
  };
  return { javac: which('javac.exe'), java: which('java.exe') };
}

const sleep = (ms) => new Promise(r => setTimeout(r, ms));

async function waitFor(label, timeoutMs, predicate) {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    let value;
    try { value = predicate(); } catch { value = false; }
    if (value) return value;
    if (Date.now() > deadline) throw new Error(`timed out after ${timeoutMs} ms waiting for ${label}`);
    await sleep(100);
  }
}

// ---------------------------------------------------------------------------
// Artifacts
// ---------------------------------------------------------------------------
console.log('Passive Fabric jar check');
console.log('');

const distJars = fs.existsSync(path.join(root, 'fabric', 'dist'))
  ? fs.readdirSync(path.join(root, 'fabric', 'dist'))
      .filter(n => /^passive-fabric-.*\.jar$/.test(n))
      .map(n => path.join(root, 'fabric', 'dist', n))
  : [];
const jarPath = newest(distJars);
if (!jarPath) {
  console.error('No fabric/dist/passive-fabric-*.jar: run fabric\\build-jar.bat first.');
  process.exit(1);
}
const injectorExe = path.join(root, 'build', 'Release', 'passive_injector.exe');
const agentDll = path.join(root, 'build', 'Release', 'passive_agent.dll');
const bridgeDll = path.join(root, 'build', 'Release', 'passive_bridge.dll');
for (const needed of [injectorExe, agentDll, bridgeDll]) {
  if (!fs.existsSync(needed)) {
    console.error(`Missing ${needed}: run build.bat (or fabric\\build-jar.bat) first.`);
    process.exit(1);
  }
}
console.log(`jar      ${jarPath}`);
console.log(`injector ${injectorExe}`);

const jdk = findJdk();
if (!jdk.javac || !jdk.java) {
  console.error('No JDK found: set JAVA_HOME to a JDK 21 (javac and java are both needed).');
  process.exit(1);
}

const prism = process.env.TYRANT_PRISM || path.join(os.homedir(), 'AppData', 'Roaming', 'PrismLauncher');
const mcJar = process.env.TYRANT_MC_JAR
  || newest(walk(path.join(prism, 'instances')).filter(f => f.endsWith('client-intermediary.jar')));
if (!mcJar) {
  console.error(`No Minecraft client jar under ${prism}\\instances. Set TYRANT_MC_JAR to one.`);
  process.exit(1);
}
const libraries = walk(path.join(prism, 'libraries'));
const lwjglJar = (name) => newest(libraries.filter(f => new RegExp(`(^|[\\\\/])${name}-3\\.[0-9.]+\\.[0-9]+\\.jar$`).test(f)));
const runtimeJars = [
  lwjglJar('lwjgl'),
  lwjglJar('lwjgl-glfw'),
  lwjglJar('lwjgl-opengl'),
  newest(libraries.filter(f => /fabric-loader-.*\.jar$/.test(f) && !/sources/.test(f))),
].filter(Boolean);
const nativesJars = [
  lwjglJar('lwjgl-natives-windows'),
  lwjglJar('lwjgl-glfw-natives-windows'),
  lwjglJar('lwjgl-opengl-natives-windows'),
].filter(Boolean);
if (runtimeJars.length < 3 || nativesJars.length < 2) {
  console.error(`LWJGL 3.3.x libraries not found under ${prism}\\libraries (set TYRANT_PRISM, or install the instance).`);
  process.exit(1);
}
console.log(`minecraft ${mcJar}`);

// ---------------------------------------------------------------------------
// 1. The jar carries the client
// ---------------------------------------------------------------------------
console.log('');
console.log('1) the jar carries the client');

const entries = readZip(jarPath);
const text = (name) => (entries.get(name) || Buffer.alloc(0)).toString('utf8');
const sameFile = (name, file) => entries.has(name)
  && Buffer.compare(entries.get(name), fs.readFileSync(file)) === 0;

const metadata = (() => {
  try { return JSON.parse(text('fabric.mod.json')); } catch { return null; }
})();
check(metadata !== null, 'fabric.mod.json is present and parses as JSON');
if (metadata) {
  check(metadata.id === 'passive', `fabric.mod.json id is "passive" (${metadata.id})`);
  check(metadata.environment === 'client', 'the mod declares environment "client"');
  const entrypoints = (metadata.entrypoints && metadata.entrypoints.client) || [];
  check(entrypoints.includes('com.passive.fabric.PassiveFabricMod'),
        'the client entry point is com.passive.fabric.PassiveFabricMod');
  check(!!(metadata.depends && metadata.depends.fabricloader && metadata.depends.minecraft && metadata.depends.java),
        'the mod declares its fabricloader / minecraft / java requirements');
  const jarVersion = path.basename(jarPath).replace(/^passive-fabric-|\.jar$/g, '');
  check(metadata.version === jarVersion, `the jar name matches the mod version (${jarVersion})`);
}
check(entries.has('com/passive/fabric/PassiveFabricMod.class'), 'the entry point class is in the jar');
check(entries.has('com/passive/fabric/NativeLoader.class') && entries.has('com/passive/fabric/PassiveNative.class'),
      'the loader and the JNI declaration classes are in the jar');
check(![...entries.keys()].some(n => n.startsWith('net/fabricmc/')),
      'the jar does not bundle fabric-loader classes (the loader is already running)');

const classMajor = (() => {
  const bytes = entries.get('com/passive/fabric/PassiveFabricMod.class');
  return bytes && bytes.length > 8 ? bytes.readUInt16BE(6) : -1;
})();
check(classMajor === 65, `the loader classes target Java 21 (bytecode major ${classMajor})`);

check(sameFile('passive/passive_agent.dll', agentDll),
      'passive/passive_agent.dll is byte-for-byte the built client (all of the menu and module logic)');
check(sameFile('passive/passive_bridge.dll', bridgeDll),
      'passive/passive_bridge.dll is byte-for-byte the built bridge');
for (const font of ['Inter-Regular.ttf', 'Inter-Medium.ttf', 'Inter-SemiBold.ttf']) {
  check(sameFile(`passive/assets/fonts/${font}`, path.join(root, 'assets', 'fonts', font)),
        `the jar carries the ${font} face the interface draws with`);
}

// ---------------------------------------------------------------------------
// 2. A live JVM maps the client through the mod's own loader
// ---------------------------------------------------------------------------
console.log('');
console.log('2) a live JVM loads the jar payload and maps the client');

fs.rmSync(work, { recursive: true, force: true });
fs.mkdirSync(work, { recursive: true });
const classesDir = path.join(work, 'classes');
const nativesDir = path.join(work, 'natives');
const gameDir = path.join(work, 'game');
fs.mkdirSync(classesDir, { recursive: true });
fs.mkdirSync(gameDir, { recursive: true });

const compile = spawnSync(jdk.javac, [
  '-encoding', 'UTF-8', '--release', '21',
  '-cp', [jarPath, ...runtimeJars].join(path.delimiter),
  '-d', classesDir,
  path.join('test', 'FabricJarStage.java'),
], { cwd: root, encoding: 'utf8' });
if (compile.status !== 0) {
  console.error('javac failed for test/FabricJarStage.java:');
  console.error(compile.stdout || '');
  console.error(compile.stderr || '');
  process.exit(1);
}

// Start from an empty log so every marker below belongs to this run.
const logCandidates = [
  path.join(path.dirname(jdk.java), 'passive_agent.log'),
  path.join(os.tmpdir(), 'passive_agent.log'),
];
for (const candidate of logCandidates) {
  for (let i = 0; i <= 9; ++i) {
    const file = i === 0 ? candidate : candidate.replace(/\.log$/, `_${i}.log`);
    try { fs.rmSync(file, { force: true }); } catch { /* a locked log is not fatal */ }
  }
}

const runCp = [classesDir, jarPath, mcJar, ...runtimeJars].join(path.delimiter);
const child = spawn(jdk.java, [
  '-cp', runCp,
  'FabricJarStage', gameDir, nativesDir, ...nativesJars,
], { cwd: root, stdio: ['pipe', 'pipe', 'pipe'] });

const stage = new Map();
let stdoutBuffer = '';
let stderrText = '';
let exited = null;
child.stdout.on('data', (chunk) => {
  stdoutBuffer += chunk.toString('utf8');
  let index;
  while ((index = stdoutBuffer.indexOf('\n')) >= 0) {
    const line = stdoutBuffer.slice(0, index).replace(/\r$/, '');
    stdoutBuffer = stdoutBuffer.slice(index + 1);
    if (line.startsWith('STAGE-')) {
      const eq = line.indexOf('=');
      if (eq > 0) stage.set(line.slice(6, eq), line.slice(eq + 1));
    }
    console.log(`  | ${line}`);
  }
});
child.stderr.on('data', (chunk) => { stderrText += chunk.toString('utf8'); });
child.on('exit', (code) => { exited = code === null ? -1 : code; });

const logFile = () => {
  const existing = logCandidates.filter(f => fs.existsSync(f));
  return existing.find(f => fs.statSync(f).size > 0) || existing[0] || logCandidates[0];
};
// The bridge and the agent write to different logs depending on where they run, so read every
// candidate; the first non-empty file alone can hold only the bridge's first line.
const logText = () => logCandidates
  .flatMap(c => [c, c.replace(/\.log$/, '_1.log')])
  .filter(f => fs.existsSync(f))
  .map(f => { try { return fs.readFileSync(f, 'utf8'); } catch { return ''; } })
  .join('\n');
const logHas = (needle) => logText().includes(needle);
const logCount = (needle) => logText().split(needle).length - 1;
const alive = () => {
  if (exited !== null) throw new Error(`the JVM exited early (code ${exited})${stderrText ? `: ${stderrText}` : ''}`);
  return true;
};

try {
  await waitFor('the loader to map the client', 90000, () => alive() && stage.get('MAPPED') === 'true');
  check(true, 'the mod\'s loader mapped the client into a live JVM');
  check(stage.get('BRIDGE-ABI') === '1', `the bridge reports its ABI (${stage.get('BRIDGE-ABI')})`);
  // The standalone injector decides whether an instance already has Passive by
  // looking for a module named exactly passive_agent.dll, so a jar-loaded client
  // has to be mapped under that name - otherwise the injector would map a second
  // copy into a process that already has one.
  check(/[\\/]passive_agent\.dll$/.test(stage.get('AGENT') || ''),
        `the client is mapped as passive_agent.dll, the name the injector probes for (${stage.get('AGENT')})`);

  // The interface reads its faces from beside the DLL, not from the game
  // directory, so the extraction has to put them there.
  const unpacked = path.join(gameDir, 'config', 'passive', 'native');
  for (const font of ['Inter-Regular.ttf', 'Inter-Medium.ttf', 'Inter-SemiBold.ttf']) {
    const placed = path.join(unpacked, 'assets', 'fonts', font);
    check(fs.existsSync(placed)
          && Buffer.compare(fs.readFileSync(placed), fs.readFileSync(path.join(root, 'assets', 'fonts', font))) === 0,
          `the loader put ${font} beside the mapped DLL`);
  }

  await waitFor('the GL window', 30000, () => alive() && stage.get('GL') === 'ready');
  check(stage.get('GL') === 'ready', 'a GLFW window with a real GL context is presenting frames');
  console.log(`  agent mapped from ${stage.get('AGENT')}`);

  await waitFor('the swap hook to install', 60000, () => alive() && logHas('hooks::install ok'));
  check(true, 'the client hooked the swap function (hooks::install ok)');
  await waitFor('a rendered frame', 30000, () => alive() && /render frame \d+/.test(logText()));
  check(true, 'the client rendered from that hook (render frame N)');
  await waitFor('the overlay bootstrap', 60000, () => alive() && logHas('agent ready — press RSHIFT in game'));
  check(true, 'the overlay bootstrapped on the render thread (agent ready)');
  // The ImGui side of the client, running from the jar: an atlas was built and
  // uploaded, and the interface is drawing into the window we present to.
  const diagnostic = (logText().match(/render diag: (.*)/) || [])[1] || '';
  check(/fonts=3/.test(diagnostic) && /atlas=\d+x\d+/.test(diagnostic),
        'the ImGui font atlas was built and uploaded from the jar payload');
  console.log(`  diag ${diagnostic}`);
  check(logHas('Minecraft JNI mappings resolved'), 'the client resolved the Minecraft mappings it needs');
  const resolved = (logText().match(/resolve complete: (\d+) failed lookups/) || [])[1];
  if (resolved === undefined) {
    skip('the optional symbol lookups', 'the client never reached mc::resolveAll');
  } else {
    // Reported, not asserted: these are the entity, world and network lookups,
    // and they read a live MinecraftClient - which a bare JVM with the client
    // jar on its classpath does not have. The same DLL in the real game logs
    // "resolve complete: 0 failed lookups", and mc::resolveAll succeeds both
    // places, so the count here says nothing about the jar's payload.
    console.log(`  note ${resolved} optional lookups missed (no running game in this JVM; 0 in game)`);
  }

  // -------------------------------------------------------------------------
  // 3. Uninject from the jar, then re-inject with the standalone injector
  // -------------------------------------------------------------------------
  console.log('');
  console.log('3) uninject from the jar, re-inject with the standalone injector');

  const pid = stage.get('PID');
  child.stdin.write('unload\n');
  await waitFor('the client to unload', 30000, () => alive() && stage.get('UNLOAD') === 'true');
  check(stage.get('MAPPED') === 'false', 'the client\'s module left the process (the jar\'s uninject is real)');
  check(logHas('unload requested: tearing down'), 'the client logged its own teardown');
  await waitFor('the teardown to finish', 20000, () => alive() && logHas('teardown complete - releasing the log'));
  check(true, 'the teardown completed and released its log');

  const injection = spawnSync(injectorExe, ['--pid', pid], { encoding: 'utf8', timeout: 120000 });
  const injectorOut = `${injection.stdout || ''}${injection.stderr || ''}`.trim();
  console.log(`  injector exit ${injection.status}: ${injectorOut.replace(/\r?\n/g, ' | ')}`);
  check(injection.status === 0, 'passive_injector.exe injected after the jar version uninjected');
  check(injectorOut.includes('dll mapped'), 'the injector found the DLL mapped in the process');
  check(injectorOut.includes('overlay handshake ok'), 'the injected client reached its overlay handshake');

  await waitFor('the injected client to map', 30000,
                () => alive() && logCount('==== passive agent dll mapped ====') >= 2);
  check(logCount('==== passive agent dll mapped ====') >= 2,
        'the log shows a second mapping: the same DLL, loaded by the injector');
  child.stdin.write('status\n');
  await waitFor('the injected client to answer as mapped', 20000,
                () => alive() && stage.get('MAPPED') === 'true');
  check(true, 'the process reports the client mapped again');

  // The bridge can also unload a client it did not map: same export, same path.
  child.stdin.write('unload\n');
  await waitFor('the injected client to unload', 30000,
                () => alive() && stage.get('MAPPED') === 'false');
  check(logCount('unload requested: tearing down') >= 2, 'the bridge unloaded the injector-mapped client too');

  child.stdin.write('exit\n');
  await waitFor('the harness to exit', 20000, () => exited !== null);
} catch (failure) {
  failures.push(String(failure.message || failure));
  console.log(`  FAIL ${failure.message || failure}`);
  if (stderrText) console.log(`  jvm stderr: ${stderrText.split(/\r?\n/).slice(0, 12).join('\n  ')}`);
  try { child.kill(); } catch { /* already gone */ }
}

try { child.stdin.end(); } catch { /* already closed */ }
if (exited === null) {
  await sleep(500);
  try { child.kill(); } catch { /* already gone */ }
}

// ---------------------------------------------------------------------------
console.log('');
console.log(`${passed} passed, ${failures.length} failed`);
for (const failure of failures) console.log(`  - ${failure}`);
process.exit(failures.length === 0 ? 0 : 1);
