# Passive — the Fabric jar

A normal Fabric mod jar that delivers the native client. Drop it in the instance's `mods\` folder and Passive
is running when the game starts: no injector, no DLL beside the launcher, no other mod.

```text
fabric\build-jar.bat            ->  fabric\dist\passive-fabric-1.0.2.jar
```

Then copy that jar into `.minecraft\mods\` (or the instance's `minecraft\mods\`). **RSHIFT** opens the menu;
**Ctrl+Shift+End** uninjects (Settings > General > SELF DESTRUCT does the same in two clicks).

## What is in the jar

| Entry | What it is |
| --- | --- |
| `passive/passive_agent.dll` | the built client — the ImGui menu and every module, byte for byte the DLL the injector ships |
| `passive/passive_bridge.dll` | the loader shim (`bridge/bridge.cpp`) |
| `passive/assets/fonts/Inter-*.ttf` | the faces the interface draws with |
| `fabric.mod.json` | id `passive`, environment `client`, `minecraft ~1.21.11`, `java >=21` |
| `com/passive/fabric/*.class` | the loader: `PassiveFabricMod`, `NativeLoader`, `PassiveNative` |

Nothing about the client is reimplemented in Java. The jar's only job is to get the built DLL into the game
process and out of it again — the menu, the modules, the hooks, the JNI layer and Silent Aim's one-shot
look packet (`PlayerMoveC2SPacket.LookAndOnGround`, sent immediately before the attack and handed back to
the camera on the next tick) are the C++ client, untouched. No mixins, so nothing in the jar depends on a class being loaded before the game touches it.

## How it loads

`com.passive.fabric.PassiveFabricMod` (the Fabric client entry point) calls `NativeLoader.ensureLoaded`, which:

1. unpacks the payload into `<gameDir>\config\passive\native\` — files are rewritten only when the jar carries
   a different build, and the client is always written as **`passive_agent.dll`**;
2. writes the Inter faces to `assets\fonts\` **beside** that DLL, because the interface resolves its faces
   relative to the DLL, not the game directory;
3. loads `passive_bridge.dll` with `System.load`, and lets the bridge map the client.

## Why there is a bridge at all

`System.load` makes the JVM hold a handle to the library for the life of the process. That handle keeps the
client's refcount above zero, so the client's own graceful unload — the one behind Ctrl+Shift+End and SELF
DESTRUCT, which ends in `FreeLibraryAndExitThread` — could never actually remove the module. The client would
stay resident with its hooks torn down and `passive_injector.exe` would still report it as loaded.

So the JVM loads only the bridge, and the bridge maps the client with plain `LoadLibraryExW`. The bridge owns
that one load reference. The client is told this at load time (`passiveSetHostManaged`), so its own unload path
runs the full teardown but deliberately does **not** call `FreeLibraryAndExitThread` — that would only drop
the client's own reference and leave the bridge's behind, which is the stuck-DLL bug. Instead the client sets
`passiveUnloadComplete`, and the bridge's release watcher calls `FreeLibrary` on the handle it owns. The module
really leaves the process, including after an in-game uninject:

```text
jar: Ctrl+Shift+End  ->  client tears down (hooks off, ImGui down, JNI released, log closed)
                     ->  client reports passiveUnloadComplete
                     ->  bridge frees the module reference it holds
                     ->  passive_injector.exe can inject the same DLL again, and does
```

The bridge publishes `nativeLoad` / `nativeUnload` / `nativeLoaded` / `nativeLastError` / `nativeBridgeAbi`
for `com.passive.fabric.PassiveNative`. It refuses a client whose ABI does not match its own, and it refuses to
map a second client while one is already mapped — the same rule the injector's UI applies, so an instance
can never end up with two overlays. Agent ABI 2 is the host-managed unload pair: a jar from an older build is
refused rather than left resident.

Because a jar-loaded client is mapped under exactly `passive_agent.dll`, the injector's already-loaded probe
(and its already-loaded marker in the instance list) keeps telling the truth about an instance running the
jar.

## Where things are written

| Path | What |
| --- | --- |
| `<gameDir>\config\passive\native\passive_agent.dll` | the mapped client |
| `<gameDir>\config\passive\native\passive_bridge_<hash>.dll` | the bridge (hashed, so a jar update can drop a new one beside a running one) |
| `<gameDir>\config\passive\native\assets\fonts\` | the interface's faces |
| beside the runtime's `javaw.exe`, else `%TEMP%` | `passive_agent.log` (client and bridge both log there) |
| `%APPDATA%\Passive\configs` | presets, as with an injected client |

## Building it

```text
fabric\build-jar.bat
```

It needs the same Visual Studio 2022 + CMake the native build needs, plus a JDK 21 (`JAVA_HOME`). It runs the
native build first, compiles the three loader classes against `fabric-loader` (fetched once into `fabric\lib\`
for the compile only — it is *not* bundled into the jar), and packs the jar. The version lives in two places
that the check compares: `resources\fabric.mod.json` and `JAR_VERSION` in `build-jar.bat`.

After changing anything in `agent/`, `injector/` or `fabric/bridge/`, re-run `fabric\build-jar.bat` so the
jar carries the new DLL — the check fails loudly if the jar's payload and the native build disagree.

## Checking it

```text
node test\fabric_jar_check.mjs
```

36 checks over the built artifacts: the jar's payload against the native build byte for byte, the metadata a
Fabric loader requires, the loader classes' bytecode level — then a live JVM that runs the mod's own loader,
maps the client, presents real frames and watches the client hook the swap function, render, build its ImGui
font atlas and publish its overlay handshake. It then uninjects through the bridge and lets the real
`passive_injector.exe` inject the same process, which has to answer `INJECT-OK dll mapped; overlay handshake
ok`. Machine-local dependencies (a JDK 21, the Prism instance's Minecraft client jar and LWJGL 3.3.x
libraries) are found automatically; `TYRANT_PRISM` and `TYRANT_MC_JAR` override the search.

The jar is **not** launched by Fabric Loader in that check — mod discovery and `onInitializeClient` inside a
live Minecraft are reasoned, not observed.

## Notes and limits

- Windows x64 only, like the client itself; on any other OS the loader refuses with a clear message rather
  than failing later.
- Frozen at Minecraft 1.21.11, as the rest of the project is.
- A failed load never takes the game down: the loader reports the reason on stderr and in the log and lets
  Minecraft run.
- One client per process. If the standalone injector already mapped the DLL, the jar steps aside (and the
  injector keeps working); if the jar mapped it, the injector reports the instance as already loaded.
