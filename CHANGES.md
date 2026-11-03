# Passive - Silent Aim is computed inside the game's own crosshair pick

- **The aim owns the pick now, instead of writing it after the frame.** The game builds its pick in
  `ClientPlayerEntity.getCrosshairTarget`, and that one value drives the HUD attack indicator, the
  block outline, and vanilla's own `doAttack` / `doItemUse`. Silent Aim rewrites that computation
  (the single `checkCrosshairTargetRange` call it passes through) so the pick is raycast from the
  silent rotation: the indicator draws on the aim's target, the block the camera looks at is no
  longer outlined, and a click is answered by the game itself against the aim - in the same frame,
  with the camera, body and model untouched. The end-of-frame `crosshairTarget` write stays only as
  the fallback for a build where the transform cannot install.
- **`targetedEntity` moves with the pick.** It is set from the aim's pick inside the same call, so the
  HUD's "target acquired" state can never be one frame behind or left over from the camera's pick.
- **Block actions stand down while the aim holds a target.** With the pick on the target there is no
  block under the crosshair, so breaking and placing do not start; the camera handoff is used only by
  the field-write fallback.
- **One attack, not two.** When the pick-level aim is installed the module no longer sends its own
  delayed `attackEntity`: vanilla's click already hits the aim's target, so that swing is skipped.

# Passive - AutoMace rebuilt around the real Attack click, in milliseconds

## Passive - StunSlam retries the stun, No Glide, Triggerbot waits for the charge

- **StunSlam no longer spends the mace on a raised shield.** After the axe swing the target is
  checked again: a shield still up means the swing did not stun (it landed before the server had the
  shield, or the shield came back), so the axe goes back in and swings again - at most three times -
  instead of the mace hitting a shield. The smash re-checks the shield too, so a target that re-raises
  it gets the axe again.
- **AutoMace has a No Glide checkbox.** An elytra glide cannot land a smash, so with it on the
  sequence does not arm while the player is gliding. Saved with the other AutoMace settings.
- **Triggerbot waits for the weapon's own attack charge.** The millisecond window still paces the
  card, but the click also waits until the attack cooldown has recovered, so a swing never lands on a
  weapon that has not recharged. The window and the charge gate together: whichever is later wins.
- **Drag Click emits every click slot a frame owes.** The scheduler used to answer one yes/no per
  frame, so a card set to 40 CPS topped out at the frame rate (~8). Each due slot is now its own
  complete click, capped at six per frame so a hitch cannot queue a burst.
- **Silent Aim hands the game's crosshair pick the aim target.** The HUD attack indicator and the
  block outline both read `crosshairTarget`; while the aim holds a target that field is set to it,
  so the indicator shows and the block behind the camera stops being outlined while the view stays put.

## AutoMace
- **The whole card clicks now, and it clicks the real thing.** Click Simulation is not a setting any
  more: every AutoMace swing is a left click posted to the game window, so the game handles it through
  its own Attack And Destroy binding - its crosshair pick, its hand swing and the click a CPS HUD
  counts. Triggerbot clicks the same way, so an attack from a card and an attack from the keyboard are
  the same attack.
- **No more waiting for the weapon's attack charge.** The sequence does exactly what the card says:
  swap to the mace, then click at the configured **Hit delay**. StunSlam hits the shield with the
  hotbar axe, waits the configured **Axe delay** - the time the shield needs to come down - then puts
  the mace in and clicks at the Hit delay. Nothing waits for the attack speed to refill.
- **Every delay is milliseconds.** Hit delay, Axe delay, Cooldown, Reswap and Swap delay are ms
  sliders (the old per-tick sliders and the Reswap "ticks" labels are gone), and the engine paces on
  the same millisecond values the sliders show.
- **Elytra Swap works.** The chestplate in the hotbar is selected and put on with a vanilla right
  click, which takes the elytra off in the same call - no inventory screen, and no menu clicks that
  only ever worked while the player already had their inventory open. It runs in StunSlam too (after
  the axe, before the mace), and the elytra is put back afterwards when it lands in the hotbar.
- **The AutoAim step is gone from AutoMace.** The sequence no longer turns the camera onto the target
  between the axe and the mace, so there is no choppy snap mid smash: it swaps, then clicks.

# Passive - AutoMace Click Simulation, the StunSlam's mace hit, and a real uninject

## AutoMace / StunSlam
- **StunSlam lands the mace hit again.** The fall is read once, when the sequence ARMS; the mace step no
  longer re-tests it. The axe hit and the wait for the attack charge both spend the fall, so the old
  second check saw the ground a few ticks after the axe and cancelled the smash - exactly the "it stuns
  the shield but never hits with the mace" report. The mace is also confirmed to be the held item before
  the swing, so a hotbar selection that did not take is retried instead of swinging the axe again.
- **Click Simulation** (new checkbox on the AutoMace card, on by default) makes every swing
  `MinecraftClient.doAttack()` - the call the physical Attack And Destroy key makes - so the game sends
  one attack and one swing against its own crosshair pick and its own attack charge, instead of a raw
  `attackEntity` plus a second hand swing. A swing the game refuses (the charge the stun slam's axe just
  emptied, or a target the axe knocked a few degrees off the crosshair) is retried on a later frame
  rather than dropped: the step aims back onto the target and waits for the charge, bounded by the axe's
  five-second shield disable. With Click Simulation off the module keeps the direct attack at the
  tracked entity, which has no charge gate.

## Uninject / unload
- An uninject now really takes the module out of the process on every path. The bridge releases every
  loader reference it can reach instead of only one, and says so in `passive_agent.log` when something
  outside it still holds the DLL. A payload without `passiveUnloadComplete` is released once its
  handshake has been gone for a few seconds instead of being waited on forever (that wait is what left
  an old build mapped until the launcher restarted).
- Teardown gives the pose-watchdog thread its moment to leave this DLL's own code before the hooks come
  out and the module can be unmapped under it.

# Passive 1.0.2 - silent aim tracking, placement settling, the new library shell

## Java module host (hybrid)
- The module layer can now be written in Java without giving up the native client. `passive_agent.dll`
  keeps what it is good at - injection, the ImGui interface, the packet hooks, the per-frame JNI scope -
  and loads `com.passive.fabric.modules.ModuleManager` through the game's own class loader on the first
  frame that can see a world. `NativeApi` is registered with `RegisterNatives` before anything ticks, so
  a Java module reaches the same game objects the C++ modules used (`KeyBinding.setPressed`, the
  player's velocity, the health / hurt time reads) with typed, null-safe calls.
- The Java side publishes its modules once, as a descriptor (`describe()`). The native host turns that
  into the interface's own cards, swaps out the C++ card of every module now owned by Java, and draws
  the settings from the descriptor, so a Java module gets checkboxes, sliders and dropdowns with no C++
  switch per module. Enable-state edits are pushed to Java on the frame they happen; settings edits go
  straight to the Java `Setting`.
- First two ports, end to end: **Sprint** and **Auto Jump Reset** (both release their held key on
  disable, on the menu opening and on uninject). Every other module still runs native; a module moves
  over by being added to `ModuleManager` with the same name, at which point its C++ card is replaced.
- Nothing breaks without the Java payload: a standalone injector mapping only the DLL logs that the host
  is absent and keeps the C++ modules. Configs save the Java settings under `javaModules` next to the
  native ones, and a preset loaded before the classes are up is applied as soon as they register.
- Uninject now calls `ModuleManager.shutdown()` before the JVM link is dropped, so a Java module cannot
  hold sprint or jump after the client has left.

## Unload / uninject
- A jar-loaded client now really leaves the process when it is uninjected. The Fabric bridge maps
  `passive_agent.dll` with `LoadLibraryExW`, so the bridge owns one loader reference; the client's own
  `FreeLibraryAndExitThread` only dropped its own reference and the module stayed mapped, which is why the
  launcher had to be restarted. The bridge now marks the client host-managed, the client reports
  `passiveUnloadComplete` after hooks/ImGui/JNI/log teardown, and the bridge releases its reference. The same
  path covers the in-game SELF DESTRUCT / Ctrl+Shift+End chord, not just `PassiveNative.unload()`.
- Agent ABI is 2 (host-managed unload + completion report); a bridge refuses an older payload and a jar from
  an older build is refused instead of leaving a resident DLL behind.

## AimAssist / Silent Aim
- A silent click no longer depends on the attack cooldown this frame. Minecraft processes the player's
  click in its own input handler on the tick, before the overlay frame runs, and that swing spends the
  attack charge even when it hit only air. The old check read the cooldown after that swing, so every
  silent hit whose camera was not already on the target was refused - which is why a hit still needed the
  real view on the player. The arm now uses the charge the click was actually made at (the previous
  frame's value), and the hit itself no longer re-checks the cooldown after the movement tick.
- World actions now own their own rotation. A block place, block break or item use is validated against
  the view the player really has, so the aim hands the server the camera for the duration of the action and
  drops any click that was waiting for its aim packet. The press is caught in the WndProc hook, before the
  game's tick sends the place / break packet, so the first action of a press is covered too - not just the
  ones after it. While the handoff is live the movement-packet transform is suspended, so no aim rotation
  is sent at all (one camera look packet at the press when the server was holding a different angle, and
  nothing after it); the body and head follow the camera like vanilla. The aim keeps solving in the
  background and takes the rotation back a moment after the button is released, through the game's own
  movement packet - no extra look packet from the transform path (the no-transform fallback sends its own
  look packet, as it always has).
- A left click that is not breaking a block ends the handoff immediately and is answered by the silent
  aim, so a shield / eat hold no longer blocks combat clicks. A left click on a block is a break (vanilla
  behaviour: mine the block, do not swing past it), and a right click is a place / use.
- Silent Aim rides the game's own movement packets again: the JVMTI transform gives
  `sendMovementPackets` the eased aim, so the server receives the rotation every tick while a target
  is in the cone instead of only at the click, and no extra look packet is sent for the tracking.
  The camera stays where the player put it while the character's body and head hold the same aim
  the server is told. A target that leaves the cone for a fraction of a second keeps the session
  alive (0.25 s grace), and a one-frame input/screen check gets 0.15 s, so one dropped frame can no
  longer release the aim and snap the rotation back.
- The hit is one attack per physical press: holding the button no longer repeats it. The crosshair
  does not have to be on the target for the silent swing - the module attacks the entity it solved
  the aim for, with the game's own `attackEntity` path.
- A silent hit no longer sends a `LookAndOnGround` of its own. The click arms the exact solved
  aim, the next ordinary movement packet carries it through the transform, and the attack waits
  for that packet. The old extra look was usually identical to the rotation the server already
  had, which is Grim's `AimDuplicateLook`, and every one was an extra flying packet for its timer.
- Silent Aim no longer asks the camera where the target is once a session is live. The camera cone
  still picks the first target; after that the aim the server is being told is a second reference,
  so turning the view onto a block (or anywhere else) keeps the target tracked and the next click
  still hits it. Looking at another player switches to that one, because the camera search runs
  first and the tracked target is only the fallback.
- The silent hit is gated on the game tick, not on the previous-tick camera rotation. It armed the
  exact aim and then compared it with `Entity.lastYaw` (the camera), so a click could only hit when
  the real view happened to be on the target; with the camera anywhere else the armed click timed
  out without ever attacking. The arm now waits for `Entity.age` to change - the tick whose
  movement packet carried the pinned aim - and attacks then. No look packet is sent.
- A manual look packet (AutoMace aim, silent placement, instant placement) now also stamps
  `lastYawClient`/`lastPitchClient`, the pair `sendMovementPackets` compares against. Without it the
  game sent the identical rotation again on the next tick, which Grim reports as AimDuplicateLook.
- Ending a silent-aim session no longer sends an extra camera look either; the next ordinary
  movement packet hands the real view back, so an uninject/target loss cannot produce a duplicate.
- Head accuracy uses the target's own eye position (clamped inside its hitbox) instead of a
  percentage of the box height, so vertical aim lands on the head/centre point it names.

## Placement modules
- Stun Web and Auto Pressure Plate Web wait for the hotbar swap's own game tick before the
  interaction. The old same-frame swap-and-place was sent with the previous slot still selected on
  the server, which is the high-ping delay and the occasional refused web.
- Stun Web Silent Place retries a refused click at most once on a later frame instead of sending
  the identical click twice in one frame. Both silent placements use the same one-shot silent aim
  path, so a player not looking down still gets the placement.
- Auto Drain's web drain is back to its pre-1.0.2 behaviour: the top-face rule from
  `agent/modules/drain_anchor.h` and the **Silent Place** toggle, with the same-frame swap. The
  1.0.2 AimAssist rotation / instant look-back and the swap-tick wait were removed.
- Auto Jump Reset keeps its jump-on-the-hit-tick velocity write and real Jump key press.

## Interface
- The Default interface is the library shell (  brand/sidebar, module grid, filter chips, favorites,
  profile footer) and the Dropdown interface is the eight active category columns: Combat, Crystal,
  Mace, Spear, Movement, Visual, Misc and Macros. No Cart/Player/Render columns and no modules that
  are not in the client.
- The duplicated Dropdown tab strip above the columns is gone; the header is brand + search only.
- After the injection splash the interface opens itself and grows/fades into place with the normal
  open animation.
- Injector chrome is graphite/platinum throughout; version is 1.0.2.

## Modules
- Pearl Catch is back on the Mace tab.
- AutoMace lives under Mace and carries its Minimum Fall Height slider.

# Passive - Triggerbot trims, Criticals, AutoMace and StunSlam

## Triggerbot
- Removed Work In Screen, While Use, On Left Click, Only Crit Sword, Only Crit Axe, Swing Hand,
  While Ascending and Use Shield, along with the Shield hold slider. The card is down to All Items,
  Check Shield, Click Simulation, Stray Bypass, All Entities, Same Player and the two delay windows.
- New **Criticals** dropdown: **Default** always swings on the delay, **Prio Crit** holds the due
  swing while no critical hit is on offer and takes the plain hit after one more window, **Only Crit**
  never swings without one.
- Every Triggerbot swing is announced through a shared swing signal, so **Auto Shield Break's OnHit
  mode** answers it exactly as it answers the player's own click, once per swing.

## AutoMace / StunSlam
- New **AutoMace** (Combat): crosshair on a player inside range, elytra swapped for a carried
  chestplate, mace selected, smash on a random tick delay, elytra and hotbar slot restored after.
- **StunSlam** opens the sequence with the hotbar axe, retries once while the shield is still up,
  then brings the mace in. **Axe ticks**, **Mace ticks** and **Swap ticks** pace the steps, with
  **Swap Back**, **Elytra Swap**, Range, Hit delay, Cooldown and Reswap windows.
- The armour swap is three ordinary left clicks through the player's own screen handler (menu slot 6
  for the chest), so the server performs the swap instead of the client poking stacks.

## Interface
- The category tabs carry their icons again, before the name and module count.
- The injector's remaining purple chrome is graphite/platinum and its header reads PASSIVE.

# Passive - Triggerbot, ported from Argon

## New module
- **Triggerbot** (Combat, Blatant): the Argon Trigger Bot in the native client. While the game's own
  crosshair is on an acceptable entity it swings at it on a random delay - swords and axes have their
  own Minimum / Maximum window, and **All Items** paces any held item with the sword window.
- Every switch from the Java card is here: **Work In Screen**, **While Use**, **On Left Click**,
  **Check Shield**, **Only Crit Sword / Axe**, **Swing Hand**, **While Ascending**, **Click Simulation**,
  **Stray Bypass**, **All Entities**, **Use Shield** with its **Shield hold**, and **Same Player**. The
  Friends list and Anti-Bot still keep the card off a friend or a bot, as the Java version's
  `antiAttack` check did.
- The swing is the game's own attack (`interactionManager.attackEntity`, or a left click posted to the
  game window with Click Simulation), so reach, the attack cooldown and shield handling stay vanilla's
  decision. Between swings **Use Shield** raises the off-hand shield with the vanilla use key for
  Shield hold ms and drops it again before the next swing.
- The rules (weapon classification, the delay roll, the shield angle and the zombie types) live in
  `agent/modules/triggerbot_rules.h`; `test\build-triggerbot.bat` exercises them with 47 checks.

# Passive - command-bar layout, platinum shell

## Interface layout
- The vertical category rail is gone. A brand band (monogram, wordmark, page title, search, actions) runs
  along the top of the panel and a **command bar of category chips** sits under it, each chip carrying its
  module count and the active one filled and underlined. Content spans the full panel width below the bar.
- The **Dropdown** interface's draggable tab strip moved into that same band, so both interfaces begin
  their content on the same row: they differ in what fills the bar, not in where the page starts.
- The header search field no longer sits under the Preferences and Close buttons. Those are aligned to a
  24 px right margin and the field keeps a 12 px gap from them.
- Settings > General: the motion group fits inside its card again instead of running past the bottom.

## Palette
- Surfaces are neutral graphite and the default accent is **platinum**, so the accent the theme sets is
  the one the interface actually paints with. The chrome mixed from the old plum palette (panel edge,
  drawer, dropdown hover, hero tile) is neutral too.

## Rebrand
- Tyrant is now **Passive** everywhere: `passive_agent.dll`, `passive_bridge.dll`, `passive_injector.exe`,
  `passive-fabric-1.0.0.jar`, `com.passive.fabric.*`, `%APPDATA%\Passive\configs` and the docs.

# Passive - Auto Mace V2 rebuilt, Auto Windcharge, Pearl Agro FOV

## Auto Mace V2: rebuilt, not patched
- The engine is rewritten from scratch and shares nothing with AutoMace. A **scan** scores every
  living target inside **Attack Range** by fall distance, closeness and how near the view it is, and
  takes the best one. The crosshair is no longer the gate, which is the reported "I am falling, I am
  looking at a player, and nothing happens": a target in reach a few degrees off the crosshair is
  found and turned onto.
- A **plan** of timed steps (aim, equip, swing) is built once and consumed exactly once each. That is
  what makes **AutoMace Retry** real: a rejected step ends the plan and waits the retry (or the stun
  cooldown) instead of re-arming every frame, so the "rapid clicking" is gone.
- The swing is `MinecraftClient.doAttack()` - the exact call the physical Attack And Destroy key makes
  - and it only goes out when the game's own crosshair pick **is the target** and the attack cooldown
  is full. A swing that used to be thrown at empty air (which reads as "it clicks but the mace does
  nothing") is now refused and retried.
- **Auto Aim** (new, on by default) turns the view onto the target's hitbox before every swing, with
  the rotation sent as a full move packet, so the hit is validated against the view the player sees.
- **StunSlam** is ordered axe first (shield down), then mace, with a real tick between every action and
  a retry on the axe's own step. One smash per fall; a wind burst that throws the player back up arms
  the next one.

## Auto Windcharge (Mace) replaces Diagonal Pearl Catch
- The macro is now a **toggle**. Every pearl that appears at the player is followed as a live entity
  and the wind charge's flight is solved against the pearl's per-tick positions: the charge goes out
  on the first tick its own travel reaches the pearl, which is the boost that puts the pearl (and the
  player who teleports to it) on the high ground.
- New settings: **Max Catch Ticks** (1-100, how old a pearl may be), **Max Meet Ticks** (1-160, how
  soon the interception has to be possible), **Ping Offset** (0-600 ms, moves the meeting point down
  the pearl's flight for throw latency) and **Silent Aim**.
- The charge is used only after the hotbar swap has had its own tick, and `agent/modules/wind_charge.h`
  holds the math so `test/build-wind-charge.bat` checks exactly what the module runs.

## Pearl Agro: FOV
- **FOV** (10-360 degrees) is the cone the last-hit player has to be inside before the pearl is
  thrown, so agro answers somebody the player is actually facing instead of turning round for them.

## Lunge Macro: no more ghost lunges
- A jab is only sent once the spear swap has had its own game tick (`xbow::slotSettled`, the same
  rule the other swap-then-act modules use). Frames run faster than ticks, so the old "next frame"
  could still put the slot change and the attack in one tick - the server resolved the swing against
  the item it still had, the lunge never happened on its side and the swing came back as a phantom.

## Combat category icon
The sword is drawn along a 45 degree diagonal (blade, crossguard, grip and pommel rotated together)
so it no longer reads as an upright dagger.


# Passive - Auto Mace V2, Elytra, module fixes and the animated interface

## New modules
- **Auto Mace V2** (Mace): a falling target, one smash, with its own state so the original AutoMace
  card is untouched. Minimal Fall Height, Players Only, Swap Back, StunSlam with a Custom / MMC type,
  Swap / Axe Swap / Mace Swap / StunSlam Cooldown sliders, Prefer Density, Smart Breach (Breach above
  or level with the target in the air, Density below), Auto UnEquip Elytra, Ignore Gliding, Only On
  Own Click and AutoMace Retry. No spear settings are needed: a held spear is simply another item.
  MMC keeps the Custom order but floors every timing at 100 ms, so a 1 ms slider is not sent as a swap
  the server drops.
- **Elytra** (Macros): the bind uses the elytra in the hotbar (the game's own use-item path, which puts
  the chestplate into that slot), and a second press uses the chestplate again. Armour Swap Delay and
  Swap Back Chestplate.
- **Knockback Displacement** (Combat, blatant): every hit of the player's own turns the view
  Horizontal Turn degrees for one tick and puts it straight back. The swing carries the turn; the
  camera never stays on it.

## Fixes
- **Lunge Macro**: the visible path restores the original slot **after** the lunge instead of in the
  same frame (Swap Back Delay, 25 ms default), and a lunge that cannot go out now hands the original
  item back at a 0.75 s ceiling instead of leaving the spear in hand until its cooldown clears.
- **Xbow Macro, fire place**: the flame is placed with the explicit block interaction the minecart step
  uses, not by holding the use key for two ticks. The overlay runs after the game tick, so a hold that
  was released before the next tick never placed anything and the macro gave up on a fire it had
  already aimed at.
- **Auto Totem**: what needs moving is decided **before** the inventory is opened, and the screen is
  closed only on a pass that finds nothing left to do. With no totem in the inventory it used to open,
  find nothing, close and reopen for ever.
- **Breach Swap**: the swap is armed on the player's attack input **before** the tick that resolves it
  (once per press), requires a charged swing, and is sword-only. Waiting for the tap this overlay sees
  after the tick meant the hit landed on the sword and only the visible swap happened.
- **Diagonal Pearl Catch**: the solver counts the thrower's velocity on the wind charge as well as on
  the pearl (both inherit it), so walking, sprinting and gliding intercepts are aimed at the flight the
  charge really takes instead of a standing-still launch.
- **Pearl Agro**: only a real swing of the player's own records the last-hit target, a pearl found near
  the player is only accepted when that target is the closest player to it, and a new last hit restarts
  the answer. Added a Look dropdown (Instant / Smooth) and Camera Sensitivity: Smooth throws on the
  solved rotation in the same frame and then walks the camera onto it, so it is as fast as Instant.
- **Auto Shield**: Mace Protect - a player falling with a mace inside the threshold raises the shield
  and keeps it up while they are still coming down.
- **Auto Pressure Plate Web**: Pressure Plate Assist (range, FOV, speed, vertical) aims at a plate in
  view the way AimAssist aims at a target, and Only If NO Web leaves plates that already have a cobweb
  above them alone.
- **HUD item icons**: block items fall back to their block texture (cobweb, slabs, logs, fluids), so
  they no longer fall back to a text label, and potions draw the bottle plus the tinted liquid overlay
  (custom potion colours included) from the resource pack, so a splash Strength potion looks like one.

## Interface
- Motion Blur now mirrors the Natural Motion Blur mod natively: Blur Method (Velocity Based / Frame
  Blending / Hybrid), Blur Strength, Refresh Rate Scaling, Accumulation Max, Accumulation Mix, Blur
  Profile (Uniform / Weighted) and Turn Sensitivity.
- **Dropdowns and checkboxes** are animated: a rounded switch with a sliding knob and halo, and a
  dropdown with an animated chevron, a rounded popup, an accent border and a clearly separated panel
  colour, fading in over 0.12 s (`agent/gui/widgets.*`, drop-in replacements for ImGui's widgets).
- **Closing fade**: the interface's style alpha follows the menu animation now, so module options fade
  with the panel instead of staying visible for a frame and then vanishing.
- **Injection splash**: an animated Tyrant mark with orbiting dots and a staggered "WELCOME TO
  TYRANT" on the first seconds after the client is mapped in.
- The Combat tab icon is a drawn sword (blade, crossguard, grip, pommel) instead of the old glyph.

# Passive - the mouse pointer follows Minecraft's own cursor instead of fighting it

## Mouse pointer: invisible everywhere except the interface, and gone until restart
- The report: the OS pointer was invisible in the inventory, the Escape menu, chests and every other
  Minecraft screen - it only appeared inside Tyrant's own interface - clicks still landed, and it never
  came back until the game was restarted.
- Cause, part one: Windows keeps a **counted** display counter (`ShowCursor(TRUE/FALSE)` is +1/-1 and the
  pointer is visible while the count is >= 0), and the old `gui::preFrame` drove it with two loops that
  called it on every transition - `while (ShowCursor(TRUE) < 0)` while the menu or a screen was up, and
  `while (ShowCursor(FALSE) >= 0)` the moment they closed. The hide pass calls `ShowCursor` at least once
  even when the game has just hidden the pointer itself on entering `CURSOR_DISABLED`, so each close sank
  the counter one step further below -1. Past -1, the game's own single `ShowCursor(TRUE)` (what GLFW does
  when a screen opens) can no longer make the pointer visible, which is exactly "invisible until restart"
  with the clicks still working.
- Second report after the first fix: the pointer then became **always visible, in first and third person
  alike**, because the client had started pushing its own idea of visibility into the world view.
- The rule now lives in `agent/gui/cursor_rules.h` (pure, so `test\build-cursor.bat` pins it): the client
  shows a pointer **only for its own menu**. Every Minecraft screen is left to the game, which already
  switches GLFW to `CURSOR_NORMAL` there - with the counter no longer drifted, one `ShowCursor(TRUE)` is
  all it needs. An open screen and an unreadable frame (title screen, world load) both answer "leave the
  pointer alone", so nothing the client cannot see can ever move it.
- The one thing the client still owes is its own debt: the menu showed the pointer while the game was
  in-world, where the game keeps it hidden, so closing the menu takes it back - once, and only if the
  client actually changed the state - and then stops touching the counter entirely. It never takes the
  pointer back while a Minecraft screen is open.
- `preFrame` moves the counter only while `GetCursorInfo` disagrees with that, at most 32 steps a frame, so
  it stays at exactly 0 (shown) or -1 (hidden) - the invariant one `ShowCursor` call relies on - and a
  counter stranded by an older build is walked back over the next frames instead of needing a restart.
- `hooks::hkWndProc` answers `WM_SETCURSOR` itself **only while the menu is up**: ImGui still picks the
  shape (resize arrows, the text I-beam), the arrow is filled in when nothing is set, and GLFW never gets
  the chance to answer the menu with a null cursor. `preFrame` asserts the same arrow for the menu on a
  frame where the mouse does not move. In-world and in every game screen `WM_SETCURSOR` stays the game's.
- A game screen whose own pointer has no shape (GLFW's `CURSOR_DISABLED` answer is `SetCursor(NULL)`, and
  nothing re-sets one on a still frame) gets the arrow filled in - only ever when `GetCursorInfo` already
  reports the pointer as shown, so this cannot reveal one the game is hiding. In-world it does nothing.
- The clip is held only while the menu is open and the game is in the foreground, and is dropped on focus
  loss instead of being left on the desktop.
- One log line per state change - `cursor: shown / taken back / game's own (menu= screen= screenKnown=
  showedForMenu=)` - so a wrong pointer names the decision that produced it.

# Passive - Silent Aim owns all three movement packets in the DLL, and Auto Pot refills what it can throw

## Silent Aim: LookAndOnGround, PositionAndOnGround and Full, all sent from the DLL
- All three real `PlayerMoveC2SPacket` subclasses are resolved and sent from native code, so nothing
  about the aim depends on a Java payload or on the Fabric jar. Each was checked against
  `mappings/mappings.tiny`: `class_2828$class_2831` is LookAndOnGround, `class_2829` is
  PositionAndOnGround, `class_2828$class_2830` is Full - and every one is verified assignable to
  `class_2828` before it is used. The resolve pass logs the set:
  `move packets: ready (look=LookAndOnGround position=PositionAndOnGround full=Full)`.
- What each can and cannot say in 1.21 shapes how the aim uses them:
  - **LookAndOnGround** - yaw, pitch, onGround, horizontalCollision. Rotation alone; this is what the
    aim repeats every frame, and what the non-silent one-shot paths (Xbow Macro, Diagonal Pearl / Pearl
    Agro with Silent off) deliberately leave the camera on.
  - **PositionAndOnGround** - x, y, z, onGround, horizontalCollision. It carries **no rotation at all**,
    so on its own it never aims anything; it is only ever sent next to a rotation.
  - **Full** - position *and* rotation in one packet, which is what makes the server hold the player's
    position and the aim together.
- A session opens with **Full** (the player's own position with the aimed rotation), continues with
  **LookAndOnGround** alone every frame - a position packet per frame would claim movement the game is
  about to send itself - and closes with **Full** again, so the server is never left holding an aim from
  a packet that moved nothing. The one-shot modules (Auto Spear, Pearl Agro, Diagonal Pearl Catch) send
  **Full** in the same tick as their action, so the aim is already what the server holds when the use or
  interact arrives.
- The fallbacks are explicit rather than silent: Full, else PositionAndOnGround then LookAndOnGround,
  else the rotation alone. The `horizontalCollision` boolean 1.21.2+ added is handled by trying the wide
  constructors first and the narrow ones second.

## Auto Pot: one potion per slot, and a refill that finishes
- The reported "it does not refill" was the plan fighting itself. `potRefillPlan` gathered a move for
  **every** missing buff before the inventory was touched, but only the first move was sent per pass and
  every one of them targeted the same destination slot. With a single checked Pot Slot (the default) the
  module swapped Strength in, then Speed in, then Strength back, one Inventory Delay apart, and never
  reached a throw at all. The plan now carries its own picture of the hotbar: `claimed` marks a slot it
  has already filled, `wanted` marks a slot holding a potion something still needs, and a potion is only
  moved into a slot that is empty or holds nothing wanted (`potslots::refillSlot`,
  `agent/modules/pot_slot_rules.h`). A second missing buff waits in the inventory until the first potion
  has been thrown and its effect is live - which is also what 1 Pot Only already promised.
- **1 Pot Only** keeps one potion between the checked slots and never displaces a potion it still needs;
  **Custom** still stocks every selected slot, but two potion kinds pointed at the same slot no longer
  take turns emptying it.
- **Faster throws**: a refill move with **Silent Inventory** on no longer waits a full Inventory Delay
  (the client has already applied the swap locally, and the packets stay in order on the wire), and
  changing the selected hotbar slot costs one frame instead of the slider - a slot change is one
  number-key packet, not a slot move. The old 120 ms hold plus a full Inventory Delay between potions is
  gone; the beat after a throw is 120 ms, so a potion thrown at your feet has broken and its effect is
  up before the next one is considered.
- `test/pot_slots_check.cpp` pins the refill rule: one checked slot holding a wanted potion plans nothing
  for the next buff, a plan fills two empty slots with two different potions, an empty slot wins over a
  spare, and a slot outside the potion's own list is never a destination (27 checks).

## The injected client's Silent Aim: LookAndOnGround and PositionAndOnGround
- Both are resolved by their pinned intermediary names (`net.minecraft.class_2828$class_2831` for
  LookAndOnGround, `net.minecraft.class_2829` for PositionAndOnGround), each checked to really be a
  `PlayerMoveC2SPacket` before it is used, and the resolve pass now logs both:
  `look packet: ready (...), position packet: ready (PositionAndOnGround)`.
- **PositionAndOnGround carries no rotation in 1.21** - it is `(x, y, z, onGround, horizontalCollision)`.
  So silent aim sends it *with* a look packet rather than instead of one: opening and closing a session
  sends the player's own position first and the aimed (or real) rotation straight after, and every frame
  in between is the rotation alone (`LookAndOnGround`), because a position packet per frame would claim
  movement the game is about to send itself. The one-shot modules (Auto Spear, Pearl Agro, Diagonal
  Pearl Catch) do the same around their action, so the server holds the position and the aim in the order
  the action is read.
- The non-silent paths (Xbow Macro, and Diagonal Pearl / Pearl Agro with Silent off) still send a look
  packet on its own and leave the view on the shot, which is the point of those paths.

# Passive - every click a module sends is the player's own, and two new modules

## Player fetching: the list is read deep enough to keep the players in it
- Every player-facing module (ESP, Nametags, Shader ESP, Triggerbot, Triggerbot V2, Auto Shield Break,
  AutoMace, AimAssist, Ghost Hand, Lag Reach, Stun Web, AutoWeb and the rest) reads the world's entity
  list through one scan. `ClientWorld.getEntities()` hands that list back in hash order, and that order
  moves as entities come and go, so with a cap of 256 in a world holding more - or in one where enough
  local references had already been handed out - the SAME player could be inside the scan window on one
  frame and outside it on the next. That is the reported "it finds the players, then loses them again".
  The scan window is now 1024, and the per-frame JNI scope reserves capacity for all of it
  (`PushLocalFrame(4096)` in `agent/core/agent.cpp`), so a tracked player stays inside the scan and no
  reference in the scan is ever dropped by an exhausted local-reference table.
- Dropped entities are still released one by one, so the wider window costs no JNI references.

## Triggerbot V2 attacks through the game's own key
- A V2 swing is now a posted left click (the same `hooks::syntheticClick` path Drag Click Friend uses)
  instead of a direct `doAttack()` call, so the game performs its own attack on its own tick: the miss
  delay, the hand state and the swing animation are the game's, not a re-implementation. The press is
  held for 40 ms and released on a later frame - a press and a release posted inside one frame are
  coalesced by the game's message pump - so the click the game's tick reads is a real button
  transition, and no second click is posted until the first one is released. **Allow
  entities**, **Allow players**, **Require full attack charge** (`attackCooldownProgress(1) >= 0.99`),
  **Require weapon**, **Criticals** / **Prio Crit wait**, **Attack delay**, the friends / Anti-Bot gate
  and the busy-hand gate are all unchanged and all still apply before the click goes out. The original
  Triggerbot keeps `doAttack()`.

## AutoMace: one click, one smash
- **Only On Your Own Click** now latches: the smash arms on a fresh PRESS of the attack button and the
  button has to come back up before another can arm. Before, a held button re-armed the sequence on
  every frame a target sat under the crosshair while you were falling into range - the "it clicks super
  fast the moment I fall into the target's range, and then once or twice more on the stun slam" it was
  reported for. With the latch, one press is exactly one smash (and one stun-slam sequence when Stun
  Slam is on).
- **Retry Mace** and **Stun Slam speed** are unchanged; only the arming edge moved.

## Breach Swap answers your own click, and only a fresh press
- The swap hangs on a fresh PRESS of the attack button (or a swing another module sent this frame, so
  Auto Shield Break's OnHit click still arms it). A held button re-armed on every frame of the hold,
  which is the "it clicks for me" it was reported for. One press is one swap.
- The mace with Breach is swapped in on that press and held across the crit window (airborne + falling,
  the same signal Triggerbot's Criticals uses), so the sword's own swing charge pays for a mace hit.
  Nothing is attacked from Breach Swap itself.

## Lag Reach and Ghost Hand: no autoclicking, and no hits through blocks
- **Lag Reach** fires on the rising edge of your own left button only - one press is one reach - instead
  of reaching out every 0.1 s while the button stayed down. It also marches the extended line from the
  eye to the hitbox in 0.2-block steps and refuses the hit when a solid block is in the way, so the
  longer reach can no longer hit a player behind a wall (`rayReachesThroughAir`). The 3..Max Reach roll,
  the friends / Anti-Bot gate and the vanilla miss delay are unchanged.
- **Ghost Hand** latches per physical press: it assists on the press frame only and the button has to
  come back up before another assist can fire. A web fight keeps the button down, and every frame of
  that hold used to be a fresh chance to swing - the "it still autoclicks while players are in webs"
  this answers. The click still has to land on a COBWEB and the target still has to be standing in one.

## Web Drain: only the top face, and only with something to place against
- The rule moved into `agent/modules/drain_anchor.h` (pure, checked by `test/drain_anchor_check.cpp`).
  Only a TOP face anchors a drain now. When the ray stops in the water itself the web still goes into
  the water's own cell, but only against the top face of the block directly UNDER that cell - a water
  source with nothing solid beneath it is refused rather than clicked into the fluid, which is the
  "it still air places" report. Any other face is refused, because a web beside the water is a
  placement the server never sees. The placement cell is re-validated on the frame the click goes out.

## Instant Cart Macro: no look-at-block to start, and the game's own Use key
- Binding no longer requires a block under the crosshair: the bow is drawn straight away, and the block
  is only needed when the rail placement actually happens. Looking away while the bow charges no longer
  refuses the sequence.
- The rail and the TNT minecart are placed by holding the game's own **Use Item / Place Block** key
  (`mc::ext::Key::Use`) so the game's tick performs both placements, exactly as a right click would,
  instead of a synthetic interact call. The rail cell is still read off the face the crosshair is on,
  the block still has to be inside block reach, and Swap Back / the delays are unchanged.

## Auto Pot: the potion really leaves the hand
- The throw holds the game's own **Use Item / Place Block** key and keeps the pitch down until the
  game's tick has read the use, then releases it and hands the view back - so the throw is the game's
  own use with all of its checks, which is what makes the potion leave the hand. Nothing is called
  synthetically.
- The release and the view hand-back now run at the top of the module, before anything decides whether
  another potion is still wanted. Before, the effect appearing (or the slot emptying) mid-throw could
  step straight past the release, which left the use key held and the camera on the floor - part of the
  "the pots are not thrown" and the detectability it was reported for. Turning the module off, or
  landing in water, releases the key as well.

## Trajectory Preview: a 3D arc that ends on the block it lands on
- The path is a 3D ribbon now: every step is a quad with real thickness in the world, offset along the
  segment's own horizontal right and tapering with distance, instead of a 2D polyline on the overlay.
- The block the shot actually lands on is outlined in **yellow** (fill plus outline, drawn as a world
  box), with a marker on the path where the flight stops. The arc starts at the player's own look (not
  the camera's), which is what vanilla spawns the projectile from, so it matches the real flight in
  third person too. The drag-accurate physics per item is unchanged, and a crossbow still shows nothing
  until it is charged.

## Motion Blur (Visual, safe) - new
- Fast camera turning smears the last couple of frames behind the current one, mixed along the
  direction the camera turned; standing still draws nothing. **Motion Blur Strength**, **Motion Blur
  Speed** and **Turn Sensitivity** are on the card. The whole effect is one GPU blit plus at most two
  quads through the existing render history ring, so nothing is taken off the frame rate.

## Auto Shield (Combat, experimental) - new
- Compatible with **Triggerbot V2** only: a swing from that card is the trigger (the same tick stamp
  Auto Shield Break's OnHit mode reads, so the two follow-ups can never disagree about when it swung).
  After **Shield Delay** the off-hand shield goes up through the game's own **Use Item / Place Block**
  key and comes back down after a drawn duration, so the shield is up for the hit and away again.
- **Require weapon** (on by default) only arms it while you are holding a weapon. **Shield Delay**
  0-600 ms, **Shield Minimum Hold Time** 0-1000 ms and **Shield Maximum Hold Time** 0-1000 ms (the hold
  is drawn between the two). It stands down while a hand is busy, and turning it off (or opening a
  screen) releases the key immediately.

## Camera Sensitivity is 0-50 now
- **Stun Web**, the **Xbow Macro** and **Auto Pot**'s Camera Sensitivity slider runs 0-50 (it was 0-10),
  so the top half is a genuinely fast turn: 50 is 124/s, a couple of frames for a 90 degree swing, while
  every setting at or below 10 keeps exactly the pace it always had (5 is 25/s, 10 is 36/s). The rule
  lives in `agent/modules/placement_aim.h` and `test/placement_aim_check.cpp` pins the new top end.
- **Look Back** walks the view back through the shared smooth rule, and the Smooth placement's own
  seeding was fixed so the walk starts from the rotation the player really held.

## Verification
- Built with MSVC (Release) at `/W4`: **0 errors, 0 warnings**. Every harness was rerun after the last
  edit and all exit 0: `test/build-placement-aim.bat` (20 checks, 0 failed), `test/build-drain-anchor.bat`
  (17, 0 failed), `test/build-click-pace.bat` (47), `test/build-detect.bat` (65), `test/build-text-name.bat`
  (17), `test/build-shader-glow.bat` (58), `test/build-xbow-timing.bat` (48), plus the node checks
  `test/auto_web_check.mjs` (38/38), `test/lag_reach_check.mjs` (18/18),
  `test/spear_crit_pearl_check.mjs` (30/30), `test/xbow_fireside_check.mjs` (15/15) and
  `test/fabric_jar_check.mjs` (36 passed, 0 failed - it loads the rebuilt client into a live JVM and
  drives it to its overlay handshake). The Fabric jar was rebuilt with the new DLL.
- No Minecraft was driven by hand for this round, so the in-world feel is reasoned and harness-checked,
  not observed: the motion blur, Auto Shield's timing, the trajectory ribbon and every click path above
  have only been exercised in code, in the harnesses and in the JVM load check.

# Passive - the vanilla hotbar is really covered, the Dropdown layout is a tab bar, and the macros wait for your click

## The HUD plate covers the game's hotbar outright
- The plate behind Tyrant's HUD is opaque by default now (`plateOpacity`, saved with the presets). At
  any translucency the game's own hotbar and bar rows underneath it showed through the drawn HUD,
  which is the "I can still see the vanilla hotbar" this answers.
- The GUI-scale probe has a fallback: when it cannot resolve the game's own scale the plate is sized
  from the framebuffer height instead of backing only Tyrant's HUD, so the vanilla bar cannot show
  around the edges of the plate.
- Held-item names are readable again. A plain item name is a *translatable* text component, and the
  client's text read had no way to resolve one: it fell back to the component's own dump, which is
  why a golden apple printed "translation{key='item.minecraft.golden_apple', args=[]}" in the plaque.
  The translation key is now read out of that dump and made readable ("Golden Apple"), with the
  prettified item id as the last resort, and that fixes the name everywhere it is drawn. The rule
  itself lives in `agent/modules/text_name.h` and is checked by `test/text_name_check.cpp`.

## Change Interface: the Dropdown layout is a top tab bar now
- The left rail is gone in Dropdown mode. A horizontal strip of category tabs sits under the header,
  each with its category's icon, name and module count, the selected tab filled and underlined.
  Dragging one tab onto another reorders them and the order is saved, exactly as the rail's was. The
  content area spans the whole panel instead of sitting beside a sidebar.
- The collapsible category sections went with the rail. The selected tab shows its modules as a flat
  list of rows - name, safety chip, the custom ON / OFF / MACRO toggle button and a settings chevron
  on hover - and a search covers every category at once under SEARCH RESULTS.
- A row click still opens that module's own settings view (BACK, the category icon, name, chip, path,
  description, then the module's card). Its options are drawn with the Dropdown skin - flat, square
  cards, square toggles and thin sliders - through the same shared widget helpers, so both interfaces
  keep sharing every option and every byte of module state.

## Stun Web / Xbow Macro: Smooth no longer pauses before the action
- The Smooth walk closed on the placement's rotation at 4..20/s, which is up to half a second of
  turning before the web or the flame was allowed to go out - the reported "0.5 s delay, then it
  places the web / lights the fire". The rule is now 14..36/s across the same Camera Sensitivity
  slider, so every setting is on the block inside 0.35 s and the middle of the slider inside 0.2 s,
  and the placement follows the moment the view arrives. `test/placement_aim_check.cpp` pins it.
- **Look Back** is a smooth turn now instead of a snap: the view the player held before the action is
  walked back at Camera Sensitivity by one shared rule (Stun Web, the Xbow Macro's shot and Auto
  Pot), and the walk is dropped the moment the player moves the mouse, so their own aim always wins.

## Auto Drain: Web Draining works from the top face
- The drain only armed when the ray itself landed in the water, so the shape a player leaves when they
  place a block and pour water onto it - crosshair on that block's top face, the water in the cell
  above - was refused. Looking at the TOP face of any block now anchors the drain, and the cell that
  has to hold the water is the one the click would fill; any other face is refused, because a web
  beside the water is a placement the server never sees. The rule lives in
  `agent/modules/drain_anchor.h` and is checked by `test/drain_anchor_check.cpp`.
- **Silent Place**: the rotation the placement is validated against goes onto the player and out to
  the server, then the camera is put straight back, so the web lands where the crosshair is without
  the view ever moving. The crosshair still has to be on the top face - the client-side hit is what
  the placement is built from.

## Auto Pot
- **Speed potions are repotted.** The speed effect's registry id is `speed`, but the potion that
  grants it is the Potion of Swiftness, so the potion's own text never says "speed" and the search
  never matched one. The token list now carries "swiftness" as well.
- **Inventory Delay is honoured.** The slot change was paced by a fixed 50 ms whatever the slider
  said, and the visible inventory stayed on screen for a fixed 250 ms; both follow the slider now, so
  a 2 ms setting is a 2 ms setting (the game's own tick is still the real floor).
- **Look Down** dropdown (Instant / Smooth) with **Camera Sensitivity**: Instant snaps the pitch down
  in the frame it throws; Smooth walks it down and throws the moment it is really there, with no pause
  of its own between the aim and the throw. **Look Back** walks the view back to where the look-down
  started, and without it the pitch is put back in the same frame - the camera is never left staring
  at the floor. Turning the module off mid-walk puts the view back too.

## AutoMace
- **Only On Your Own Click** (on by default): the card no longer swings by itself. The smash waits
  for the player's own attack input, which is the "blatant autoclicking when in range" it was.
- **Stun Slam speed** (0-600 ms): how long the axe stun is held before the mace comes in. At 0 it is
  the next frame, the pace the slam always had.
- **Anti Spear Stun Slam**: with a spear in hand when the slam arms, the sequence is spear -> axe ->
  mace and the spear is never put back in between. An axe step that could not finish used to restore
  the spear first, which is the "spear, axe, spear, mace" order this removes.
- **Retry Mace** is the wait before the module may arm again on a new fall, and with Only On Your Own
  Click it is finally observable: nothing arms without your own click.

## Breach Swap answers a real left click
- The trigger is the same signal Auto Shield Break's **OnHit** mode uses: the left button held, a
  fresh press this frame, or a swing another module sent this frame.
- The full-attack-charge requirement is gone. The click that triggers the swap is the click that
  spends that charge and this code runs after the game has already processed the tick, so at the
  frame the click is visible the charge reads ~0 and every real critical click was refused. The crit
  test is now airborne + falling - the same signal Triggerbot's Criticals uses - and the crit window
  below is what holds the mace in hand for the hit.

## Verification
- Built with MSVC (Release) at `/W4`: **0 errors, 0 warnings**. Every rule this round changed is
  pinned by a harness, rerun after the last edit, all exit 0: `test/build-text-name.bat` (17 checks,
  0 failed - the new one), `test/build-placement-aim.bat` (17), `test/build-drain-anchor.bat` (13),
  `test/build-xbow-timing.bat` (48), `test/build-click-pace.bat` (47), `test/build-detect.bat` (65)
  and `test/build-shader-glow.bat`, plus the node checks `test/auto_web_check.mjs` (38/38),
  `test/lag_reach_check.mjs` (18/18), `test/spear_crit_pearl_check.mjs` (30/30),
  `test/xbow_fireside_check.mjs` (15/15) and `test/fabric_jar_check.mjs`.
- No Minecraft on this machine, so the in-world result is reasoned and harness-checked, not observed:
  the HUD plate over the vanilla hotbar, the Dropdown tab strip, the web/flame/fire placements and
  Auto Pot's throw have only been exercised in code and in the harnesses above.

# Passive - the Dropdown interface is an interface of its own

## Change Interface: Dropdown rebuilt
The Dropdown layout was a rearrangement of the tab layout. It is now a second interface in its own
right, while the Default layout is left exactly as it was and both read and write the same module
state and saved configuration, so switching between them changes the presentation and nothing else.
- The page is a stack of collapsible category sections, in the order the rail was dragged into. Each
  header carries the category's own icon and name, its module count and a chevron that eases between
  its folded (right) and unfolded (down) shapes; a section's rows are revealed with a growing clip
  rectangle and a fade, so opening and closing slides rather than snaps. A search holds every
  section open and filters rows across all of them at once.
- Module controls are custom toggle buttons, not switches and not checkboxes: a rounded block with
  ON / OFF (or MACRO, for a module that has no state) written inside it, its fill and border easing
  between the two states, hover lifting it and a press shading it. Clicking it flips the module
  through the same path the Default interface uses, side effects included. The row body beside it is
  a separate hit area that opens the module's settings, so reaching for settings cannot toggle a
  module by accident.
- A row click opens a dedicated settings view: BACK, the module's category icon, its name, its
  safety chip, a `SETTINGS / category` path, a one-line description of what the module does and
  then the module's own card - every setting, slider and keybind - drawn by the shared renderer
  (`drawModuleCards`), which the Default interface draws through as well. No option is written
  twice, and the view fades in and out instead of snapping.

# Passive - the game's own HUD stays, absorption turns the health bar yellow, and a pearl catch that really collides

## The HUD no longer hides the game's HUD
- Hiding the HUD is one switch in vanilla, and it takes the first-person hand, the chat, the Tab
  list, the scoreboard and the boss bar away with the hotbar and the bars. Tyrant never touches that
  switch any more: **Cover Vanilla** (new, on by default) draws an opaque plate over the game's own
  hotbar and bar rows instead, so the drawn HUD is the HUD you see while the hand, chat, Tab and
  scoreboard stay exactly the game's.
- The plate is placed at the game's own GUI scale, which is read from the game's window through a
  self-checking probe - the pinned yarn dump does not name `getScaledWidth`/`getScaledHeight`/
  `getScaleFactor`, so every `()I` method the Window class declares is tried and the only triple
  that satisfies the game's own invariant (`scaledWidth * scale == framebufferWidth`) is used. When
  no scale resolves, the plate simply backs Tyrant's own HUD.
- The panel rows were *measured* from the screen's own top edge while the panel boxes stayed by the
  hotbar (the row origin was missing the panel's Y), which is why the health / armour / food / XP
  rows appeared at the top of the screen instead of inside the VITALS / SUBSTANCE boxes.

## New HUD behaviour
- **Absorption**: while absorption is held the health row is yellow and its maximum becomes the
  health plus the absorption held right now, so the row covers the shielding. The moment it is
  gone the row is the plain red health bar again.
- **Bar animation**: every row slides towards its new value instead of snapping to it.
- **Heart beat**: below two hearts the heart glyph pulses, and it pulses faster the lower the
  health is.
- **Crosshair indicator**: while the crosshair ray is on a living entity inside the range, the
  crosshair is drawn in the indicator colour - a little larger than the game's own, which stays
  visible as its core. Nothing has to be hidden for it, so it works with the game's crosshair.
- The HUD card gained the plate options, the bar behaviour switches and the indicator settings;
  the Diagonal Pearl Catch card gained **Catch Tolerance**.

## A pearl catch that collides instead of only looking like it did
The old solve picked the tick whose charge reach came closest to the pearl's distance from the eye
and accepted anything inside a three-block radius. A wind charge and an ender pearl are both about a
quarter of a block wide, so a three-block "catch" is a charge that streaks past the pearl: it looks
exactly like a collision and is not one. The solve now samples the pearl's flight once and scores
every candidate meeting tick by the real distance from the pearl's position on that tick to the
charge's own swept segment for that tick, so the charge only goes out when its flight actually
passes through the pearl. Catch Tolerance (default 0.6 blocks) is how much miss is still accepted.

## The shield icon is a texture again
- The shield's inventory icon is built from the shield *model* (a base plate plus the wood pattern)
rather than an item texture, which is why the HUD drew the word "Shield" for it. The icon now falls
back to the shield model's own base-plate texture.

# Passive - a custom HUD, a single nametag, a truer silent aim and a pearl that is really tracked

## New module: HUD (Visual)
- The HUD module draws Tyrant's own HUD instead of the game's. A rounded, animated hotbar (fade-in,
  a pop on the slot you switch to, slot numbers, stack counts, real item icons) sits at the bottom
  centre; two separated rounded panels sit above it, the left one carrying the health and armour
  bars and the right one the food and XP bars, each with a vector glyph (heart, shield, drumstick,
  orb) and its values; a held-item plaque with the item's icon slides in above the hotbar whenever
  the held stack changes; an attack speed bar under the crosshair fills as the swing charges; and a
  custom crosshair is drawn from the chosen preset (Cross, Dot, Cross + Dot, Circle, Circle + Dot,
  T-Shape, Chevron, Arrows) or from the 9x9 Draw Board the card lets you paint.
- **Hide vanilla HUD** holds `GameOptions.hudHidden` true every frame while the module is enabled and
  hands the previous value back the moment it is switched off (and on reset), so the game draws none
  of its own HUD (hotbar, bars, crosshair) and the drawn one really is the HUD. With it on, a
  crosshair is always drawn - the chosen custom one, or a plain white cross if Custom crosshair is off.
- The card exposes the element switches, the hotbar scale / corner rounding / animation speed and
  its colours, the panel scale, offset, glyphs, values, rounding and the health / armour / food / XP
  / panel colours, the plaque scale, hold time and icons, the attack bar scale and colour, and the
  crosshair preset, size, gap, thickness, dot, rotation, outline, dynamic opening and colour.
- New JNI reads back it: `LivingEntity.getArmor()` (armour points), `HungerManager.getFoodLevel()`
  and the player's experience level / progress, plus a direct read/write of `GameOptions.hudHidden`.
  Every one is null-tolerant, so a missing symbol returns a safe default instead of breaking a frame.

## Nametags: Single Show
- The card gains **Single Show**. With it on, a board is drawn only for the player or entity the
  crosshair is actually on (the game's own crosshair pick), instead of one above every player in view.

## Silent Aim is now silent in third person too
- "Silent" meant the aimed rotation was written onto the player and put back inside the same call.
  The camera never moved, but the write still leaked into the entity's own rotation, so in F5 the
  model visibly turned onto the target - the one thing "silent" is supposed to hide.
- `silentAimSend` now sends the aimed rotation as a look packet and **never touches the player's
  rotation**, so the first-person view and the third-person model both stay exactly where the player
  put them. Vanilla only sends a rotation packet of its own when the rotation changed, and this does
  not change it, so nothing overwrites the aim on the server.
- The one-shot silent aim paths (Auto Spear, Diagonal Pearl Catch, Pearl Agro) go through one shared
  helper: the aim is written for the single action packet that carries the rotation (a use item / an
  interact), restored immediately, and a look packet then hands the camera's own view back to the
  server. Nothing visible turns, and the server is never left looking at the aim.

## Diagonal Pearl Catch: the pearl is tracked, not guessed
- The pearl this module threw is now identified by where the previous frame put it rather than by
  which pearl is nearest the player. While gliding the player is far from the pearl, and "nearest the
  eye" used to snap onto an old pearl or another player's, resetting the solve mid-flight - the
  reported "the angle acts up while gliding". A tracked match that lands more than 6 blocks away is
  refused, so an unrelated pearl is never aimed at.
- The real flight is sampled every frame it can be read, and a frame that cannot read the pearl
  advances those samples instead of re-deriving the arc from the throw, so Dynamic Timing keeps
  working off the trajectory the pearl is really on.
- A thrown pearl inherits the thrower's own velocity on top of the look-direction launch, which is
  why a glide throw flies flatter and further; that velocity is folded into the first frame's guess.

# Passive - a Fabric jar build: the same client, loaded from the mods folder

## Fabric jar (`fabric/`)
- `fabric\build-jar.bat` produces `fabric\dist\tyrant-fabric-1.0.0.jar`, a normal Fabric mod jar. Drop it
  into the instance's `mods\` folder and the client is there when the game starts - no injector, no DLL to
  copy beside the launcher, and no other mod needed. It carries the built `tyrant_agent.dll` (the ImGui
  menu and every module, byte for byte the build the injector ships), the Inter faces the interface draws
  with, and a small loader.
- `fabric.mod.json` (id `tyrant`, environment `client`, entry point `com.tyrant.fabric.TyrantFabricMod`,
  `minecraft ~1.21.11`, `java >=21`) and three Java classes. The jar bundles no Fabric API or loader
  classes and asks for nothing else.
- At startup the loader unpacks the payload into `<gameDir>\config\tyrant\native\`. The client keeps its
  exact name `tyrant_agent.dll` and the fonts land in `assets\fonts\` beside it, because the interface
  reads its faces from beside the DLL rather than from the game directory. Files are rewritten only when
  the jar carries a different build.
- Nothing else about the client changes: **RSHIFT** opens the menu, every module, the presets
  (`%APPDATA%\Tyrant\configs`) and `tyrant_agent.log` behave exactly as they do for an injected client.

## Why the jar needs a bridge, and why uninjecting still works
- `System.load` would leave the JVM holding a handle to the client for the life of the process, so the
  client's own unload (`FreeLibraryAndExitThread`) could never actually remove the module. It would sit
  there with its hooks torn down and `tyrant_injector.exe` would still report it as loaded - the exact
  thing the jar version has to avoid.
- So the jar loads only `tyrant_bridge.dll` (`fabric/bridge/bridge.cpp`: no hooks, no ImGui, ~250 lines)
  and the bridge maps the client with plain `LoadLibraryW`. Nothing in the JVM references the client, so
  **Ctrl+Shift+End** / Settings > General > SELF DESTRUCT genuinely unloads it and the standalone injector
  can inject the same DLL afterwards.
- The agent publishes `tyrantRequestUnload()` and `tyrantAgentAbi()` (`agent/core/exports.h`); the bridge
  asks for an unload through that export and waits for the module to leave the process. It refuses a build
  whose ABI does not match its own, and refuses to map a second client while one is already mapped - the
  same rule the injector's UI applies.
- A jar-loaded client is mapped under exactly `tyrant_agent.dll`, which is the name the injector's
  already-loaded probe looks for, so an instance running the jar is never injected twice by accident.

## Verified here
- Whole tree (agent, injector and bridge) compiled and linked clean at `/W4 /permissive- /EHsc /utf-8`,
  **exit 0, no warnings**; `dumpbin /exports` confirms the two new exports on the agent and the five JNI
  entry points on the bridge.
- **New** `node test/fabric_jar_check.mjs` -> **36/36, exit 0**, driving the built artifacts:
  * the jar's payload is byte-for-byte the native build (client, bridge and all three faces), the metadata
    carries the id, client environment, entry point and version a Fabric loader needs, the loader classes
    are Java 21 bytecode, and no loader classes are bundled;
  * a live JVM runs the mod's own `NativeLoader` (not a copy of its logic): the client maps as
    `tyrant_agent.dll`, the loader places the faces beside it, the swap hook installs, the client renders
    from it, builds and uploads its ImGui font atlas, and publishes its overlay handshake;
  * the client is unmapped by its own unload - module gone from the process, teardown complete, log
    released;
  * the real `tyrant_injector.exe --pid` then injects that process: **`INJECT-OK dll mapped; overlay
    handshake ok`, exit 0** - the same DLL, loaded again by the injector;
  * and the bridge can unload a client the injector mapped, through the same export.

## Not verified
- **The jar was not launched by Fabric Loader inside Minecraft.** `fabric.mod.json`, the payload and the
  loader are exercised as above, but mod discovery, `onInitializeClient` and the menu in a live 1.21.11
  instance are reasoned, not observed. The harness JVM has no running game, so the agent reports 131
  optional symbol lookups missed there (the entity, world and network reads that need a live
  `MinecraftClient`); the identical DLL logs `resolve complete: 0 failed lookups` in the game.

# Passive - Triggerbot V2 with OnHit Allowance, and AutoWeb's head web

## New modules

### Combat > Triggerbot V2
- A second Combat card beside Triggerbot with the **same switches** (Allow entities, Allow players,
  Require full attack charge, Require weapon, the **Criticals** dropdown - Default / Prio Crit / Only
  Crit - with **Prio Crit wait**, and **Attack delay**), plus one the original does not have. The two
  cards keep their own settings and their own pacing state and share every rule: one swing per frame
  at a valid target under the crosshair, gated by friends / Anti-Bot, the vanilla miss delay and the
  vanilla restrictions (nothing while a screen is open, while eating or while your own shield is up).
- **OnHit Allowance** (new checkbox): a Triggerbot V2 swing counts as the left click **Auto Shield
  Break**'s **OnHit** mode is waiting for. Auto Shield Break runs earlier in the frame than the
  triggerbot does, so the swing is remembered for a short window (0.15 s, a little over a tick) and
  the shield break picks it up on the next frame - the two work together instead of the shield break
  needing a physical click on top of the triggerbot's own attack. With the switch off, nothing about
  the original Triggerbot or Auto Shield Break changes.

### Combat > AutoWeb
- The experiment that follows Stun Web, kept as its own card while the head web is tested. It carries
  the whole shield-stun follow-up in a **Stun Web** section of its own (Web Look, Camera Sensitivity,
  Silent Place, Look Back, Players only, Cobweb Place Delay - the same settings and the same meaning
  as the Stun Web card), and the two cards never fight over the hotbar: a sequence belongs to the card
  that armed it, and AutoWeb drives it when both are on.
- **Allow Head Web** (new checkbox, AutoWeb section): when a hit of your own - a left click, or a swing
  another module sent this frame - lands on a player standing right against a solid block, a cobweb is
  dropped into the cell against that block (the cell they are standing in) and one into the cell above
  it (their head). The block behind them is the next cell on the line between you and them; they count
  as "against it" within 0.4 blocks of its face, and both webs go through the same Web Look / Camera
  Sensitivity / Silent Place as every other placement - the lower web against the wall's face, the
  upper one against the wall when the block behind one level up is solid and otherwise on top of the
  web just placed. It needs a cobweb in the hotbar and block reach, never webs your own cell, and
  stands down while a stun-web attempt is mid-flight.
- The geometry lives in `agent/modules/head_web.h` (pure: no JNI, no ImGui), next to the existing pure
  rules, so the cell choice and the face each placement clicks are testable on their own.

## Verified here
- Whole tree compiled and linked clean at `/W4 /permissive- /EHsc /utf-8` into
  `build_verify\Release\` (`tyrant_agent.dll` + `tyrant_injector.exe`), **exit 0, no warnings**.
- **New** `node test/auto_web_check.mjs` -> **38/38, exit 0**: the pair of cobweb cells is the
  target's own cell and the one above it, the wall is taken at the first solid level behind them, a
  one-block wall leaves the upper web on the lower web while a taller one leans both on the wall,
  each of the four directions clicks the face that looks at the target, the gap is measured to the
  face (mid-cell is refused, hugging it is not), and a Triggerbot V2 swing arms Auto Shield Break's
  OnHit mode for its window while a plain frame past it does not.
- The existing harnesses are untouched by this round and still pass: `lag_reach_check.mjs` 18/18,
  `spear_crit_pearl_check.mjs` 29/29, `xbow_fireside_check.mjs` 15/15.

## Not verified
- **Nothing was run inside Minecraft.** The head web's two placements, the face the server accepts,
  the triggerbot's own attack feeding Auto Shield Break's OnHit mode, and every timing constant are
  reasoned and harness-checked, not observed in a world.

# Passive - Spear modules, Triggerbot criticals, an Instant Cart macro, and a pearl catch that follows the pearl

## New modules

### Spear > Auto Spear
- While a spear is held, the spear is **right-clicked on a cadence** at a valid target in front of it,
  so its poke lands without the player clicking. Configurations: **Allow players**, **Allow entities**,
  **Range** (slider, 1-8 blocks, eye to hitbox), **Use delay** (50-1000 ms between pokes), **Only when
  looking** (poke only while the crosshair is on the target), **Silent Aim** (the view is turned onto
  the target with the rotation the server is given and put straight back - the camera never moves)
  and **No Glide**.

### Spear > Instant Spear Kill
- The spear's damage scales with the user's velocity, so on the frame the spear is used this writes a
  large **client-side velocity** to the player. Configurations: **Simulated Speed** (5-100 blocks/tick),
  **Hold Ticks** (1-10), **Add downward velocity** (the falling component the spear also scales off),
  **Silent** (restore the real velocity the same frame, so the movement the server is told about is
  left alone), **Require target** and **Target Range**.

### Macros > Instant Cart Macro
- Bind -> a **partially charged bow** shot, then a **rail** and a **TNT minecart**. Both placements
  require a **block under the crosshair**: the bind is refused with a notice when the ray reached no
  block, and the same check runs again at the rail step. Configurations: **Bow Draw Time** (slider,
  50-900 ms - bounded below a full charge so the shot is never a full draw), **Rail Delay**, **Cart
  Delay** and **Swap Back**.

## Triggerbot: a Criticals dropdown
- New **Criticals** dropdown: **Default** (unchanged - every valid swing goes out with no stalls),
  **Prio Crit** (hold the swing while a crit is on offer, then fall back to a plain hit after **Prio
  Crit wait**, 0-2000 ms) and **Only Crit** (only ever swing when the hit would crit).
- A crit is read as **airborne and falling**. That is the jump crit and the **knockback crit** alike -
  a knockback launches the player without them ever leaving the ground through their own input, so the
  airborne + fall signal is what captures both; the module does not require a jump. The card grew the
  dropdown and the Prio Crit wait slider.

## Diagonal Pearl Catch: it follows the live pearl
- The catch no longer re-derives the pearl's arc from the throw: it **finds the ender-pearl entity the
  module just threw** and solves against that entity's **own position and velocity**. That is what
  makes a **wind-charged pearl** (a pearl a wind charge has already boosted, so it flies further and
  flatter) and a **30 degree or lower throw** solvable at all - the charge is aimed where the pearl
  really is rather than at a model's version of it, which is what used to aim at the ground on
  shallow, landing-early throws.
- New **Dynamic Timing** checkbox: instead of the fixed **Wind Charge Delay**, the module watches the
  pearl every frame and throws the charge itself the moment a real catch exists. Because both fly at
  the same 1.5 blocks/tick, the charge can only ever land within its own explosion reach of the pearl,
  so a catch is gated on the miss being inside that reach **and** the meeting point being off the
  thrower's feet. A pearl that has already landed (or is still too close) ends the attempt with a
  notice instead of throwing a charge at the ground.

## Verified here
- Whole tree compiled and linked clean at `/W4 /permissive- /EHsc /utf-8` into
  `build_verify\Release\` (`tyrant_agent.dll` + `tyrant_injector.exe`), **exit 0, no warnings**.
- The existing harnesses are untouched by this round and all pass: `xbow_fireside_check.mjs` 15/15,
  `lag_reach_check.mjs` 18/18.
- **New** `node test/spear_crit_pearl_check.mjs` -> **29/29, exit 0**: the crit rule is airborne +
  falling (jump and knockback), each Criticals mode gates the swing as described (Default always,
  Only Crit never without a crit, Prio Crit only after the wait), and the pearl solve matches the
  charge's drag-accurate flight against the pearl's arc - the throw-instant pearl is met exactly, a
  pearl with a head start is short by exactly that head start, a boosted pearl's aim follows its live
  velocity further out, and the Dynamic Timing gate accepts only a catch inside the explosion reach
  and off the thrower's feet.

## Not verified
- **Nothing was run inside Minecraft.** `build\Release` could not be relinked this round because the
  injected `tyrant_agent.dll` was loaded (link error LNK1104); the clean result above is
  `build_verify\Release`. **Instant Spear Kill's server acceptance is entirely empirical** - the
  module writes a client-side velocity, and whether the server's spear reads it is untested. The
  Auto Spear cadence, the Instant Cart bow draw/release, the Criticals timing and the pearl catch's
  Dynamic Timing window have only been checked in code and in the harness above.

# Passive - Lag Reach, module safety tags, and the placements that stand down instead of burning items

## New module: Lag Reach (Misc)
- Bind it, hold attack, and the **crosshair ray is tested against every living entity's own bounding
  box**: vanilla's entity pick stops at the ~3 block interaction range, so the target is found here
  instead - the nearest entity whose hitbox the look ray enters. Every swing **rolls a fresh reach
  between 3 blocks and Max Reach**, so the range the packets carry never sits on one number.
- **Max Reach** (slider, 3-6, clamped) bounds the roll. The hit test is the slab method against the
  entity's interpolated box, so a ray that misses the box is never a target and a box the eye is
  already inside counts at distance 0.
- The swing only ever goes out while the player is **holding the attack key themselves** - the
  module extends the reach of a real click, it does not click for you, and it never fires on the
  frame another module already sent an attack. The server still decides which hits it accepts.

## Module tags: SAFE / EXPERIMENTAL / UNSAFE / BLATANT
- Every module card now carries a **tag chip** beside its name: a drawn tick for **Safe**, a warning
  triangle for **Experimental**, **Unsafe** and **Blatant** - green, yellow, red and red-pink - and
  **hovering the chip explains what the tag means**. The symbol is built from shapes, so it renders
  the same with any font, and the chip is drawn in all three layouts (card, squared card and the
  Dropdown button). The tag lives on the entry itself, so a new module cannot ship untagged.

## Keybinds: Escape unbinds, and typing in chat never toggles a module
- **Escape while capturing a module's bind now sets that bind to None**, so an accidental Escape can
  always unbind a module again instead of leaving the capture open. (The menu key's own capture still
  cancels on Escape, which is what that one should do.)
- **No module keybind fires while a screen - chat included - is open**: a letter typed into chat can
  no longer toggle the module bound to the same letter. The macros take the same guard, so a bind
  cannot fire a hotbar sequence while you are typing either.

## Cobweb Macro: no swap for a placement that cannot happen
- The macro now checks the crosshair hit **before** it swaps to the cobweb and stands down with a
  notice when there is no block under it. The game returns a **MISS-typed hit** when the ray reached
  nothing (looking at the sky), and a hotbar swap for a placement the server would refuse only moved
  the hotbar for nothing. Silent Swap and the 45 ms swap delay are unchanged.

## Stun Web: arms only on a real stun
- Auto Shield Break remembers the player it just hit, and the web **only arms once that player has
  really stopped blocking** - the server telling us the shield was disabled - inside the stun window.
  A target still holding the shield when the window expires is left alone rather than webbed through
  a raised shield.

## Diagonal Pearl Catch: the charge meets the pearl's arc, not a flat line
- The wind charge does not fly a flat 1.5 blocks/tick: like every projectile it loses **1% of its
  speed each tick**, so it covers `1.5 * (1 - 0.99^t) / 0.01` blocks by tick t. Solving the meeting
  point against the flat rate aimed the charge past the pearl at any real distance - it only looked
  right up close. The catch tick is now where the charge's **drag-accurate flight** meets the pearl's
  own arc, so the delay slider moves the meeting point along the curve instead of past it.

## Crosshair hits: a MISS-typed BlockHitResult is not a block
- The game returns a `BlockHitResult` with type `MISS` when the crosshair ray reached no block
  (looking at the sky). `mc::hit::typeOf` decided by `instanceof` alone and read that as a block hit,
  so **every placement built on it became an air place** - a click the server refuses while the
  client kept its own prediction. The hit's own `HitResult.Type` now decides, with the old
  `instanceof` rule kept as the fallback for a version whose `getType` cannot be read.

## Verified here
- Whole tree compiled and linked clean at `/W4 /permissive- /EHsc /utf-8` into
  `build_verify\Release\` (`tyrant_agent.dll` + `tyrant_injector.exe`), **exit 0, no warnings**.
  `build\Release` was rebuilt through `build.bat` as well.
- The existing harnesses are untouched by this round and all pass: `xbow_fireside_check.mjs` 15/15,
  xbow-timing 48, placement-aim 15, detect 65, shader-glow 58.
- **New** `node test/lag_reach_check.mjs` -> **18/18, exit 0**: the roll stays inside [3, MaxReach]
  with the clamp and is exactly 3 at the slider's bottom; the slab test returns the near-face
  distance, reads a box the eye is inside at 0, and refuses boxes that are behind, beside, or
  invalid; and only an entity whose touch is inside the rolled reach is attacked.

## Not verified
- **Nothing was run inside Minecraft.** Lag Reach's rolled reach is still the server's call - a
  long-range hit may simply be rejected - so the Max Reach slider is empirical; the ray test and the
  tag chips have only been checked in code and in the harnesses above.

# Passive - shield-stun webs under the target's feet, a fire that cannot ghost, UI themes, and the Mace row

## Stun Web: Cobweb Place Delay, and webs that cannot land on you
- **Cobweb Place Delay** (new slider, 0-600 ms, default 0): a straight wait between the stun and
  the placement, so the web can be timed to go down as the knockback carries the target. The
  placement itself is the cell under the target's feet, found by walking down to the first open
  cell with a real block under it.
- The knockback prediction is gone. It aimed the web at a predicted landing spot solved from the
  target's velocity, and a bad velocity reading - a near-zero or jittery one, which is what the
  plugin's sideways shove often reads as - put that spot on the player themselves or past the
  target on the far side. That was the reported "placing on me, or a 180 turn and the web
  opposite of them". The web always goes under the target's current feet column now.
- **The web can never catch the player instead.** At shield-stun range the target's column can be
  the one the player is standing in; when the cell the web would go into is the player's own feet
  or head cell, the attempt stands down rather than webbing the player.

## Xbow Macro: the fire cannot ghost, cannot spin you, and Look Back hands the view back
- **Long range ghosting** was the click, not the block: the server resolves a placement with its
  own ray from the eye, and at longer range a hill or an edge block somewhere along the line made
  that ray refuse the click while the client kept the flame it had predicted. Every flame cell is
  now a candidate only when its top face is inside block reach **and** the line from the eye to it
  is clear (sampled through the world), and the step says so instead of ghosting when no cell
  qualifies. The Flint And Steel delay itself is untouched.
- **The 180 degree spin** (looking down at blocks a block apart, the flame behind you): among the
  cells that pass every check, the one needing the **smallest turn of the view** wins whenever the
  nearest-the-cart cells would need more than 120 degrees; a flame behind you is only lit when
  every candidate is behind you.
- **Look Back** (new switch on the card): the view the player held before the macro ran is
  remembered at the bind, and the shot is still built on the solved rotation while the camera and
  the rotation the server holds are handed straight back the moment the crossbow is fired -
  during the shooting, not after it.

## Settings > Appearance: Theme
- New **THEME** section with eight one-click palettes that repaint every slot at once: **Red Ruby**,
  **Platinum** (the light one), **Emerald**, **Sapphire**, **Amethyst**, **Golden**, **Obsidian** and
  **Rose**. Hand-tuning a color in the picker marks the theme as custom again.
- **Smooth fade background**: the interface eases from the old palette to the new one over about
  two thirds of a second instead of cutting - the fade runs whether or not the page is open. The
  preset and the fade switch are saved with the rest of the appearance.

## AutoMace: the second smash after a wind burst
- A wind burst mace launches you straight back up off the smash. That launch never touches the
  ground, so the fall it came from never ended by landing and the "one smash per fall" flag stayed
  set through the whole flight - the descent it turns into is now recognised as a fresh fall the
  moment the player is airborne and rising, and the mace arms again on the way down.

## Breach Swap: the crit that never swapped
- The swap was gated on the vanilla miss-time field on top of the real attack charge, and that
  field can hold every swap back on a reading that has nothing to do with the swing; the charge
  alone (the same value vanilla decides crits with) is what gates it now. An axe in hand counts as
  the melee weapon for the swap, not only a sword or a mace.

## New module: Diagonal Pearl Catch (Mace, macro)
- Bind -> an ender pearl is thrown along your view, then a **wind charge is angled onto its
  flight**: the pearl's arc is the vanilla one (1.5 blocks/tick launch, 0.03 gravity, 0.99 drag),
  the charge flies the straight line to the point the pearl has reached by the time the charge
  gets there, and the explosion catches you diagonally at the pearl instead of straight up under
  you. **Silent Aim** keeps the camera still while the charge is aimed.
- **Wind Charge Delay (0-1000 ms)** times when the charge is aimed and thrown. A pearl curves,
  and the meeting point is solved from where the pearl **is** when the delay ends - the arc is
  advanced by the ticks that have already passed first. Aligning from where the pearl was thrown
  is what made the charge line up too early on the trajectory; the longer the delay, the further
  along the curve the two meet.

## Verified here
- Whole tree compiled and linked clean at `/W4 /permissive- /utf-8` into `build_verify\Release\`
  (`tyrant_agent.dll` + `tyrant_injector.exe`), **exit 0, no warnings**. The canonical
  `build\Release` path is rewritten by `build.bat` when the game is closed.
- `node test/xbow_fireside_check.mjs` -> **15/15**, exit 0, now mirroring the candidate rule: the
  ranking is nearest-the-cart with the turn as the tiebreak inside 120 degrees, the three
  two-and-three-block-ledge cases assert the refusal the old rule hid a ghost behind, and two new
  cases aim the player away from the cart to prove the placement still finds a flame instead of
  spinning or refusing.
- The other harnesses are untouched by this round (their headers did not change) and passed last
  round: xbow-timing 48, detect 65, click-pace 47, shader-glow 58, placement-aim 15.

## Not verified
- **Nothing was run inside Minecraft.** The wind-charge meeting point models the charge as a
  straight 1.5 blocks/tick line, and the theme fade has only been watched in the code, not on
  screen. Stun Web's placement follows the target's live position each frame, but the right
  Cobweb Place Delay is empirical - the plugin shapes the knockback, so start small and tune.

# Passive - Stun Web places webs again, Nametags read wear and stacks through anything, and the Xbow fire no longer ghosts
Three reported regressions, all with the same shape: the client went through the motions while the
server refused the packet, and what the player saw was the client's own prediction.

## Stun Web: webs were never actually landing
- The sequence aimed at **the cell under the target's feet** and clicked the top face of the block
  below it - which is air whenever the target is moving (the stun lifts them, which is the exact
  moment this module runs). A click on air is a placement the server refuses, so no web went down.
  The block to light against is now found by walking **down from their feet**: the first open cell
  with a real block straight under it, and the web goes on that block's top face - under them
  wherever they actually are. A web already in the way still stands the attempt down, and when the
  block reads fail outright the old cell under their feet is used as before.
- The promised **second click** (the double tap the shield-stun window wants) was gated on the
  vanilla miss time, which is always still running 0.06 s after the first hit - it was dropped every
  time. It now goes out on the beat, reach permitting, and the web still goes down either way.
- **Web Look Smooth could not finish a turn.** The stall watchdog (1.4 s) runs off the last real
  progress, and waiting for the view to walk onto the cell was not counted as progress - any turn
  longer than about 17 degrees at the default pace killed the sequence before the placement. The
  aim wait now counts as progress, and a new hard deadline (the attempt's own age, 8 s) is what
  still ends a walk that never lands.

## Nametags: durability and stacks regardless of icon, name or plugin
- The durability bar only existed **behind the icon texture**: an item whose texture could not be
  loaded (a plugin's custom or renamed gear - exactly the gear that most needs reading) or **Icons
  off** showed the name with no wear at all, and the held item lost its bar the same way. The bar is
  now drawn in both paths, read from the stack's own accessors with the component-text fallback, so
  it shows for any item whatever it is called.
- The held row always keeps its slot and its label (item name and stack count) and now draws its
  bar too; the slot was also skipped (with the row shifted) whenever the texture failed to load.
  Every part of this data comes from the stack itself, so a plugin hiding or mangling a player's
  name cannot take any of it down with the name.

## Xbow Macro: the fire was a client-side ghost
- The flame is lit against **the top face of the block under its cell** - and a server that resolves
  the click against the rotation it holds (its own raycast) never hits a top face the eye is not
  above. On a shot taken from below the cart that is every cell at the cart's own row, which is
  where the old rule always put the flame: the interaction was refused and the flame existed only
  in this client, which is the ghost the arrow then flew through. The cell is now taken from the
  arrow's own per-tick samples at the **lowest row its box sweeps**, and only at a row the eye is
  above. When every holdable cell on the line is above the eye (the cart two or more blocks up),
  the step says so and stops instead of lighting a flame that cannot stand.
- The confirmation before the shot now waits out a **real round trip**: the client predicts the
  flame the moment the interaction goes out, so the old check passed on the prediction alone and a
  refused flame went straight to the shot. A flame the server corrected away is now lit once more,
  and a second refusal ends with a message instead of a wasted arrow.
- Water in the flame's cell or under it is no longer a candidate (both sides refuse a fire there),
  and the Smooth turn onto the lit block counts as progress for the stage watchdog, so a long walk
  no longer ends the sequence mid-aim.

## Verified here
- Whole tree compiled and linked clean at `/W4 /permissive- /utf-8`, **exit 0, no warnings**, into
  `build_verify\Release\tyrant_agent.dll`. The canonical `build\Release\tyrant_agent.dll` could not
  be rewritten this round because Minecraft was running with the injected agent in it (the link
  step fails with LNK1104 while the file is loaded); `build.bat` run after closing the game puts the
  same artifact in its usual place.
- The project's own harnesses, rerun after the last edit: `test/build-xbow-timing.bat` (48 checks),
  `test/build-detect.bat` (65), `test/build-click-pace.bat` (47), `test/build-shader-glow.bat` (58),
  `test/build-placement-aim.bat` (15) - all 0 failed, exit 0.
- `node test/xbow_fireside_check.mjs` -> **13/13**, exit 0, now mirroring the shipped rule exactly:
  every placed flame is on the shot line, on the shooter's side, ignites the arrow's box at that
  row and is lit from a face the eye is above; the three cases where the cart sits two or three
  blocks above the shooter now assert the **refusal** the old rule used to paper over with a ghost.

## Not verified
- **Nothing was run inside Minecraft** - the game was up with the previous build in it. The web
  landing under a moving target, the nametag wear through a missing texture and the detonation of a
  real flame are what a live run has to confirm.

# Passive - the ZIP's injector, the cosmetics drawn by the client itself, Walk Trail, and the placement options

The injector from the supplied ZIP is in place and built. The five Clarity cosmetics no longer need
the Clarity jar - the client draws them itself, in the world - and Walk Trail joins Visual as a
sixth Visual card that needs nothing external either. Stun Web and the Xbow Macro gain **Camera
Sensitivity** (Smooth only) and **Silent Place**, Stun Web gains **Look Back** and now stands down
while a cobweb is already around the target, and the Dropdown interface puts every module behind
its own rounded button instead of a switch.

## Injector (from the ZIP)
- `injector/injector.cpp` is now the ZIP's **Injector v1.0.0**: dark purple palette, a DWM-rounded
  window with the immersive dark title bar, an animated tab bar whose indicator slides between
  **Inject** and **Multi Instance**, status cards for `tyrant_agent.dll`, the detected process and
  the startup handshake, and the multi-instance list with its per-instance "tyrant loaded" state.
- The CLI path (`--pid`) and the handshake are unchanged, so injecting from outside the window still
  works exactly as before, and the one-button tab keeps the same worker as the multi-instance tab.
- Built: `build\Release\tyrant_injector.exe` (the previous exe was a stale one from an earlier
  build). The new source only adds `dwmapi` to the libraries - it brings its own
  `#pragma comment(lib, "dwmapi.lib")`, so `CMakeLists.txt` did not have to change.

## The cosmetics are drawn by the client now (Player Model, Wings, Headwear, Capes)
- These four used to be switches on an external Java mod. They are rendered in the client instead:
  every shape is built in the world from the player's own position and facing and projected onto the
  screen through the camera's own basis, exactly the way ESP and the trajectory preview are. No jar
  is needed, and the game's own models are never touched.
- **Player Model** draws its own blocky body (legs, torso, head) for every player at the configured
  scale, with Through Walls and Show Timing (the drawn scale sits next to the body).
- **Wings** draws two feathered quads from the shoulders, one per side, at the chosen style's span
  and height; Animate flaps them at Flap Speed, Y Offset moves the root, Opacity thins the fill and
  Through Walls is honoured per style. The six styles differ in span, height and colour.
- **Headwear** draws the style's own shape above the head: a crown of spikes, a flat halo ring, two
  horns / antlers / ears, or three orbiting orbs. Rotation Speed turns it, Y Offset lifts it.
- **Capes** draws a cloth quad from the shoulders with a wave fed by your own speed (Movement Wave)
  and a slow physics sway (Cape Physics); the fifteen textures are the styles' own colours plus a
  trim hem.
- The Clarity jar bridge is still there and still loaded if the jar is present - it is simply no
  longer what makes the cards do something.

## Walk Trail (Visual)
- A new Visual module. A point is sampled from your own feet 20 times a second and kept for **Trail
  Length** seconds; the trail is drawn as one grey line that fades out with each point's age, with a
  soft pulse travelling along it (Animated Pulse) so it keeps moving while you stand still.
- Options: Trail Colour (grey by default), Trail Length (0.5-10 s), Trail Width (1-8 px), Max Points
  (20-600), Fade (0-1), Animated Pulse and Through Walls. The trail is cleared when the module is
  switched off, when there is no world, and by Reset to Default.
- The overlay has no depth buffer of its own, so Through Walls off means: each point's visibility is
  decided when it is *recorded* (a short block march from the eye at that moment) and points that
  were behind a wall are dropped from the line.

## ViewModel, drawn by the client (Visual)
- The hand item is drawn from the item's own texture at the position Minecraft puts the hand
  (the same anchor the Shader ESP hand glow uses), with the card's Pos X/Y/Z, Rot X/Y/Z, Scale and
  Opacity applied to it: X/Y move it across and up the screen, Z reads as size, Rot Z turns it in
  the plane of the view, Rot X/Y squash it the way a turn away from the camera would.
- **Off Hand** draws the off-hand item on the other side of the view, and an untouched card (0
  offsets, scale 1) draws nothing at all, so the module never doubles the vanilla hand.
- The hand's place on screen comes from the same anchor the Shader ESP hand glow computes (the
  item's own projection at the base FOV), so Shader ESP's Hand Glow Size / Nudge X-Y move the drawn
  view model too - they are both "where my hand is".

## Change Interface: the Dropdown layout's module buttons
- In the **Dropdown** interface a module is no longer a card with a switch: the header row is the
  module's own rounded button, carrying its name, with **ON / OFF** (or **MACRO** for a keybind
  module) written inside it. On fills with the accent, off is a plain outline that lights on hover,
  and clicking the button toggles the module the way the switch did.
- The expand arrow moves into the button's right-hand corner so nothing overlaps, and the draw
  options below still open in place. The **Default** interface keeps its cards and switches
  unchanged - the button is only what the Dropdown layout draws.
- Switching Interface still swaps the whole window over to the other layout: the sidebar's page
  stack is replaced by the draggable dropdown tabs and their unfolded sections.

## Stun Web
- **Camera Sensitivity (0-10)**, shown when Web Look is **Smooth**: the speed the view walks down to
  the cell. 5 is the pace it always had (nothing changes for an untouched config), 0 is deliberately
  slow and 10 is nearly a snap. Instant snaps there in one frame, so there is nothing to tune.
- **Silent Place**: the placement is sent with the rotation the server is given and the camera is
  put straight back, so the cobweb goes down without the view ever moving down. It works with both
  Web Look types.
- **Look Back**: the direction the view held when the shield stun landed is remembered, and once the
  web is down the view (and the previous-tick rotation the renderer interpolates from) is handed
  back to it, with the rotation sent to the server. Silent Place never moved the camera, so Look Back
  has nothing to do there.
- **It only runs while the target is not already inside a cobweb.** A web in the cell under their
  feet was already checked; now the cell their feet are in and the one their head is in are checked
  too, and the sequence stands down rather than burning a cobweb on a catch that has already
  happened. Standing in a web yourself stands it down as well.
- Camera Sensitivity, Silent Place and Look Back are saved with the config, read back on load and
  cleared by Reset to Default.

## Xbow Macro
- **Camera Sensitivity (0-10)** under the Fire placement type, shown when it is **Smooth**, with the
  same meaning as Stun Web's: 5 is the pace the fire step always walked at.
- **Silent Place**: the fire is lit against the rotation the server is given and the camera never
  turns, so the flint and steel step is invisible.
- Both are serialized, read back and reset with the rest of the Xbow config.
- "Cart Macro" is this card: the rail -> cart -> flint and steel -> crossbow sequence. The
  `Xbow Cart Macro` class in `agent/modules/macros.cpp` is part of the older `Module` registry, whose
  single entry point `registerBuiltins()` has no caller and whose module list is never consumed by
  anything, so it is dead code and is not drawn on any page. The live registry is `modules::entries()`,
  which is what this card, the Stun Web card and every other card come from.

## Verified here
- `cmd /c build.bat` -> `build\Release\tyrant_agent.dll` and `build\Release\tyrant_injector.exe`,
  **exit 0**, `/W4 /permissive- /utf-8`, **no warnings** from either target.
- Both artifacts are newer than every source they are built from (checked by epoch).
- The built injector was launched: it came up as `tyrant_injector.exe` and stayed running with no
  `tyrant_injector_crash.txt` written (its own vectored crash logger), so the window, the D3D11
  device, the ImGui backends, the bundled fonts and the main loop all came up. It was closed again
  afterwards.
- The project's own harnesses still pass, rerun after the last edit:
  `test/build-xbow-timing.bat` (48 checks), `test/build-detect.bat` (65), `test/build-click-pace.bat`
  (47), `test/build-shader-glow.bat` (58) and `node test/xbow_fireside_check.mjs` (13/13) - all exit 0.
- The new placement aim rule is a pure header (`agent/modules/placement_aim.h`), so it is tested the
  way the other rules are: `test/build-placement-aim.bat` -> **15 checks, 0 failed**, exit 0. It
  proves the slider is clamped and monotonic, that **5 is exactly the 12/s pace these placements
  always walked at** (so an untouched config behaves as before), that a single frame can never cover
  more than half the remaining turn (no snapping on a wild deltaTime), that the top of the slider is
  nearly on the block inside a quarter of a second while the bottom is still walking, and that the
  walk is frame-rate independent.
- 73 wiring checks over the new work: the injector's v1.0.0 markers, every new option present in
  `modules.h`, its GUI row in `gui.cpp`, its aim call site, its serialize / deserialize / reset
  entry, the Walk Trail registry + overlay wiring, the native renderer's entry points, the placement
  aim header and harness, and the Dropdown button - plus the round-9 features (Stun Web, Web Look,
  fire placement, the dropdown tab order, the Clarity bridge) still wired. 73/73, exit 0.

## Not verified
- **Nothing was run inside Minecraft.** The build proves the code compiles, links and is wired; the
  cosmetics, the trail and the drawn view model have not been looked at in a real world.
- The injector's window, D3D11 device and ImGui loop are confirmed to come up (it was launched and
  closed); the injection itself, the multi-instance list and the per-instance state were not tried
  against a running Minecraft.

# Passive - the five Clarity cosmetic modules, brought onto the current build

The cosmetics from the older Clarity branch now live on top of everything this build already had.
Nothing from the current tree was dropped to make room: Stun Web, Logout Spots, the Web Look
selector, the Xbow fire placement type, the SMP Auto Totem, the drawn Shader ESP style and the
friends list are all still here, and each keeps working.

## Player Model, Wings, Headwear, Capes, ViewModel (Visual)
- Five cards, with the full option set the Clarity branch had: model scale plus Through Walls and
  Show Timing; six wing styles with scale, opacity, flap speed, Y offset, animate and through-walls;
  fifteen headwear styles with scale, Y offset and rotation speed; fifteen cape textures with the
  physics and movement waves; and first-person Pos/Rot X-Y-Z with scale.
- Every option is saved: the five blocks are written into the module config, read back on load, and
  cleared by Reset to Default.

## How they draw, and what they need
- These five have no renderer inside the client. They call into the **Clarity** Java mod, which is
  loaded at boot from `clarity-1.2.0.jar` into a child classloader whose parent is the game's own.
- The jar is **not** shipped with the client - it is a separate file. Drop it beside
  `tyrant_agent.dll` (or in the game directory, its `mods\` folder, or `%APPDATA%\Tyrant`), or point
  `TYRANT_CLARITY_JAR` at it. With the jar absent the cards still work and still save; the log line
  says the jar was not found, and there is simply nothing for them to draw.
- The bridge resolves the five module classes once and caches them, rather than re-resolving them
  every frame, and all five share one call per frame. A missing class, field or method disables just
  that piece instead of raising into the game.
- The five class lookups also feed `findClass`: the game's own loader is asked first, so a name that
  exists both in the game and in the jar still resolves to the game's class.

# Passive - Stun Web, Logout Spots, a Fire placement type, an SMP Auto Totem, and a Ghost Hand that only fires on a real click
A batch round: two new modules, three module changes, and friends riding along with the configurations.

## Stun Web (Combat, experimental)
- An add-on to **Auto Shield Break**. The moment its axe hit lands on a raised shield the target is
  remembered; a short beat later a second click goes out (the double click the shield-stun window wants),
  and then the view is turned onto the cell under the target's feet, a cobweb is swapped into the hand and
  dropped there. The upward motion the stun gives them is caught in a web instead of a launch.
- **Web Look** chooses how the view is turned onto that cell: **Instant** snaps to it (like the cart
  macro's own fire placement), **Smooth** walks it there the way AimAssist does. Either way the web only
  goes out once the view is actually on the block, and only from block reach - out of range, nothing is
  placed and the attempt ends. It needs a cobweb in the hotbar; "Players only" is the module's own switch.
- The follow-up never opens a screen or synthesises input: it only rides the ordinary number-key hotbar
  swap and the ordinary block interaction, and it releases the swap back when it is done.

## Logout Spots (Visual)
- Remembers where a player was standing when they left the world and draws a box around that spot with
  their name over it, so a logout is a place you can look at rather than a name that vanished.
- **Logout color** is a full colour wheel (black by default, which is the reported look), plus **Max
  distance** and **Only players**. One marker per player, refreshed if they are seen again and bounded to
  256 entries, so a long session cannot grow the list without limit.

## Xbow Macro: Fire placement type
- New **Fire placement type** dropdown on the Xbow Macro card: **Instant** (the old behaviour - the view
  snaps onto the fire block) or **Smooth** (the view eases onto it like AimAssist). The light still only
  goes out once the view is on the block, so Instant and Smooth place identically and only the camera
  movement differs.
- The Recharge step already held the vanilla use key through the whole reload; that is unchanged.

## Auto Totem: Type
- New **Type** dropdown. **Vanilla / Crystal** is the behaviour as it was - the off hand is kept stocked
  with a totem. **SMP** only holds the totem while the health is at or below **Health Threshold (hearts)**
  (3 by default), swaps a **shield** back into the off hand once the health recovers, and paces both swaps
  with **Swap Delay (ms)**. Both swaps use the ordinary F-key slot packet, so nothing but vanilla
  interaction happens; the existing **Silent** switch still decides whether the inventory is opened.

## Ghost Hand: a real click only
- Ghost Hand used to fire on *every* frame the attack button was held while the crosshair was on a cobweb
  - a hold read as "clicking for me". It now fires on the rising edge of the player's own left mouse button
  only (`gui::mouseTaps()`), so one physical click is one assist and holding attack is left alone. The
  synthetic clicks Drag Click Friend posts are tagged and never counted either way.

## Auto Pot: on land only
- Re-potting now waits until the player is **on the ground and out of water** before it looks down and uses
  the potion, which is the reported "only on land, not in water". The potion search also falls back to the
  stack's own readable name when the component read does not expose the effect, so a potion that the
  component text missed can still be recognised.

## Friends and configurations
- The friends list is part of the module configuration and therefore already rode along with a preset.
  The one gap was the other direction: loading a preset saved **before** the list existed cleared it. A
  config with no `friends` key now leaves the current friends alone, so an older preset can never silently
  empty the list. (Presets are `%APPDATA%\Tyrant\configs`.)

## Verified
- Whole tree compiled and linked clean at `/W4 /permissive- /utf-8` into a separate build directory
  (`tyrant_agent.dll` produced, exit 0); the only warnings are the two pre-existing `C4702` unreachable-code
  notes in `gui.cpp` around `captureMenuKey`/`captureModuleKey`, which are untouched by this round.
- The changed translation units (`agent/modules/modules.cpp`, `agent/gui/gui.cpp`) were also compiled on
  their own at `/W4` with no new warning.

## Limitations
- Nothing ran inside Minecraft. Freecam and Trap Save from the same request are **not** in this round: both
  need an engine hook the client does not have - Freecam has to move `Camera.pos`/its rotation after
  `Camera.update` and before the world draw, and Trap Save has to break a block the game's own raycast
  cannot reach - and the client only hooks `wglSwapBuffers`/GDI `SwapBuffers` and the game's WndProc today.
- The new modules are unobserved in a real world: Stun Web depends on the shield-stun plugin's own timing,
  and Logout Spots records a player the client no longer tracks, so a player simply leaving the client's
  entity range can read as a logout.

# Passive - the Xbow Macro's delay sliders are the delay again
"Xbow Macro feels too slow now. the sliders values arent changing even at 0 MS" - the round that saved the
flame put a 50 ms floor under every stage, so 0 ms, 10 ms and 50 ms all described the same wait, and each
stage cost its slider value **plus** that tick. The sliders are the timing again, and the one floor that is
left is the game's own tick after a hotbar swap, which is what actually keeps the rail, cart and flame from
being corrected away.

- The per-stage floor is gone (`agent/modules/xbow_timing.h`): `stageDelaySeconds` is exactly the slider
  value, so a Rail / Cart / Flint / Crossbow delay of 0 ms hands its interaction to the next frame. The
  600 ms ceiling is unchanged, and so is `slotSettled` - an interaction that follows a hotbar swap still
  waits a tick for the server to have seen the swap, whatever the slider says.
- Each stage now swaps its item into the hand the moment the stage opens (`modules.cpp`), instead of at the
  end of the slider's delay. The swap's tick and the slider therefore run *together*: a stage takes
  `max(slider, one tick)` rather than `slider + one tick`, so the number you type is the time it takes.
  Nothing about the gate moved - the interaction is still never in the same tick as its swap.
- A frame-accurate replay of the stage loop (rail, cart, flint, shot - the four stages that hold a delay;
  the fire's confirmation round trip is unchanged) puts the old defaults (100/110/120/100 ms) at ~650 ms of
  deliberate waiting at 60 fps and ~450 ms now. With every slider at 0 ms the four stages cost ~400 ms
  before and ~267 ms now: one swap tick each, which cannot be removed, instead of double-counting it.
- The stage-timing harness (`test/xbow_timing_check.cpp`, `test/build-xbow-timing.bat`) now asserts the
  slider as both bounds - 0 ms is due on the next frame and 10 ms, 25 ms and 50 ms are three different
  waits - and keeps the swap-gate cases, including one that no slider value can shorten it.

# Passive - Ghost Hand and AutoMace stop clicking for you, the Xbow flame stops vanishing, Shader ESP, Drag Click Friend, and a second interface
Five things in one round: two modules that used to act on their own now only ever help a click you are making,
the Xbow Macro's low delay values no longer lose the fire placement, two new modules (Shader ESP and Drag Click
Friend), and **Settings > General > Change Interface** which switches the whole menu between the Default layout
and a Dropdown layout. Shader ESP now rides Minecraft's own glowing outline around the rendered model - the
"outside their skin" glow - with the item in your own hand drawn from its texture, and keeps the drawn coloured
halo as its Custom style.

## Ghost Hand only ever helps your own click, and only through a web
- The report was "do not make it auto click randomly, let me hit players through webs while I am clicking". It
  used to swing at the nearest entity in a web whenever the attack button was held and the game's own pick was
  *any* block, which read as an autoclicker while you were mining. Now the game's own pick has to be a **cobweb**:
  the block under the crosshair is read (`mc::hit::blockPos` + `Client::isCobwebAt`) and anything else leaves the
  click completely alone, so a normal hit is never doubled and nothing is ever attacked out of the blue.
- The aim cone came in from ~52 degrees to ~34 degrees, so the target is the entity that web is hiding rather
  than anything standing in a web somewhere in front of you.
- It still never attacks on its own: the attack button has to be held down (that is your click), and it is silent
  while the menu is open or while another module already sent an attack this frame.

## AutoMace: one smash per fall
- "AutoMace still autoclicks" - a successful smash only started a wait timer, so with the target still in front of
  you and the fall still going the module could line up a second, third, ... mace. A fall is now spent by its
  smash: `g_maceSmashedThisFall` is set when the smash goes out and only cleared once the player is on the ground
  again (or the drop is below Minimum fall height). Retry Mace stays as the wait before the module may arm again,
  but only a new fall can now produce another hit.

## Xbow Macro: the fire no longer disappears at low delay values
- Root cause: a player's held item comes from the last **update-selected-slot** packet the server has processed.
  A placement that left the client in the same game tick as the swap was validated against the item that was
  still in hand, so the server rejected it and the block the client already showed - above all the flame - was
  corrected away a moment later. The lower the flint delay, the more often the swap and the placement shared a
  tick, which is exactly when the fire "disappeared".
- `agent/modules/xbow_timing.h` now holds the rule (pure, no JNI): every interaction that follows a hotbar
  swap waits for the swap's own tick (`slotSettled`). The Rail, Cart, Fire and Shoot steps call it, and
  `xbowSelectSlot` records when the hotbar last changed. (That rule first shipped with a 50 ms floor under
  every stage as well; see the entry above - the floor was what made the sliders stop changing anything.)
- The flame is also confirmed before the shot: the Fire step remembers its cell, and the shot only goes out once
  `blockIdAt` really reports fire there. If the server corrects it away, the step lights it once more (a fresh
  aim, a fresh placement) and only gives up with a "would not stay lit" toast after that.

## Shader ESP (Visual): Minecraft's own glowing outline
- The report was "I want the glow outside their skin like vanilla's, on the item textures, not on hitboxes". A
  drawn halo cannot do that, and nothing drawn by hand can: this build draws the world into an offscreen target
  and clears its depth buffer (`GameRenderer` -> `CommandEncoder.clearDepthTexture` -> the hand pass) before the
  frame ends, so at swap time there is no depth left to trace a silhouette from. Anything the overlay draws is a
  shape approximating a box.
- What Shader ESP does now is switch on **Minecraft's own glowing outline** for the players and dropped items in
  range. The client-side glow flag (`Entity.setGlowing`) makes the game itself draw the outline around the
  rendered model - skin, armour, the item in a player's hand, the item model of a dropped stack - which is
  exactly the white "outside their skin" effect the drawn version was imitating.
- The flag is never touched on an entity that already glows (a server effect, another mod), and every flag this
  module set is handed back when the module is switched off, when the style changes, when the entity walks out
  of Max distance or dies, and on client reset (`clearOwnedGlow`). The list is capped at 64 entities inside Max
  distance, because the game re-renders each glowing entity for its outline.
- **Glow Style**: `Vanilla (game outline)` - white, at Minecraft's own width, plus the hand glow below - or
  `Custom (drawn)`, the halo of `Shader Thickness` passes in ESP Color / Friend ESP Color at `Shader Opacity`
  (the previous behaviour, unchanged).
- The item in your own first-person hand is not an entity, so the game will never outline it. That one is drawn
  by Tyrant from the held item's own texture (its alpha is the item's shape) at the view-space pivot the game
  uses for the hand, projected with the frame's focal length so it follows the field of view. `Hand Glow`
  (on/off), `Hand Glow Size` (5-60% of a block), `Nudge X` / `Nudge Y` (-30..30% of the screen height) and
  `Hand Glow Rotation` (-90..90 degrees) are there because the item's in-hand scale differs per item and per
  hand setting - nudge it if it does not sit exactly on your hand.
- `agent/modules/shader_glow.h` holds the rules (pure, no JNI): which registry paths are glowable (`player`,
  `item`), when a flag is set or released, and where the hand glow lands. Saved as `shaderEsp`.

## Drag Click Friend (Misc)
- Hold attack and the click rate accelerates from **Minimum CPS** to **Maximum CPS**; **Acceleration Speed**
  (1-10) sets how fast the ramp is (1 = 4 s, 10 = 0.4 s) and **Drag Click Pause** (0-3 s) stops the clicking for
  that long once Maximum has been held for a second, then starts the ramp again.
- The clicks themselves are ordinary left clicks posted to the game window (`hooks::syntheticClick`), so the game
  runs its own attack, block break or item use - the module never fakes a swing. The button is released between
  clicks (a press and a release inside one tick would read as no click at all), it only runs while the left button
  is actually held, and it stays out of the way while a game screen or the menu is open.
- `agent/modules/click_pace.h` holds the pacing (pure, no JNI): the ramp, the pause cycle and a click scheduler
  that never queues a backlog after a long frame.

## Change Interface: Settings > General > a Default/Dropdown dropdown
- **Default** is the interface the client had: sidebar tabs, one category per page.
- **Dropdown** replaces the sidebar tabs with tabs you can **drag** onto each other to reorder (the order is
  saved), and each tab unfolds its category in the page as a dropdown section, so several categories can be open
  at once. Its cards, sliders and switches are drawn differently from the Default ones (flat square cards, thin
  bars and square switches instead of the Default rounded ones), which is what "customly made differently" comes
  down to in practice.
- Module Style and Module Shape belong to the Default interface, so the Appearance page hides both selectors while
  Dropdown is selected and says why - and a **Squared** shape left over from a preset no longer turns the Dropdown
  sections into three-wide grids, because the shape axis is only read in the Default layout.
- The choice and the tab order are saved with the presets (`ui.interface`, `ui.dropdownOrder`) and the layout is
  drawn through the same card/option code as the Default one, so every module option behaves identically in both.

## Checks
- `cmake --build build --config Release` - **green, exit 0, no warnings** at `/W4 /permissive- /utf-8`, with
  `modules.cpp`, `gui.cpp` and `agent.cpp` recompiled for that reading; `build\Release\tyrant_agent.dll` (11:37)
  is newer than every agent source (newest: `xbow_timing.h`), so the deployed binary is the final code.
- `test/build-detect.bat` - **65 checks, 0 failed**, exit 0 (player-detection rules, unchanged this round).
- `test/build-click-pace.bat` - **47 checks, 0 failed**, exit 0 (the drag-click ramp, pause cycle and click
  scheduler; it pins the two sliders crossing and the no-backlog rule).
- `test/build-xbow-timing.bat` - **38 checks, 0 failed**, exit 0: the one-tick floor, the swap gate, and the
  ordering case the fire bug was (a placement 49 ms after the swap is refused, the same placement a tick later
  goes out). Writing it found two real problems: the header leaned on `std::clamp`, which does not exist at the
  language level the harness compiles the header at, and a stage at exactly 100 ms / 600 ms landed a hair under
  its own delay from decimal rounding and would have waited an extra frame. The rule now has neither.
- `test/wglhost.exe` + `tyrant_injector.exe --pid` (the repository's own GL host harness) - inject OK, overlay
  handshake published, 3600 frames, host survived, both `SwapBuffers` and GDI hooks reported ok. JNI cannot attach
  in that host (there is no JVM), so it proves the new DLL loads and renders, not what the modules do.
- `node test/xbow_fireside_check.mjs` - **13/13**, exit 0: the fire-cell rule itself is untouched by the timing
  change, so the flame still lands on the shot line on the shooter's side.
- The interface and the two new modules are a live observation on top of that: nothing was run inside Minecraft
  here, so how the Dropdown layout feels, and how Shader ESP and Drag Click Friend look in a real world, are
  unchecked.

# Passive - player detection rebuilt around identity, and every targeting module now shares it
Built with MSVC (Release) at `/W4` with no warnings. The detection rules have their own harness
(`test/build-detect.bat`, 65 checks, no JVM needed) which is what caught the one real bug written into this
round; the in-game wiring is a live observation and is listed at the end as such.

## Why the modules were not finding players
Every module decided for itself who a player is, from the *display name*, and the Anti-Bot check read the UUID
badly:
- `Anti-Bot > Detect by UUID` called any UUID whose version nibble was not `4` a bot. Offline-mode (cracked or
  private) servers hand real players **version 3** UUIDs, so on those servers every single player counted as a
  bot and AimAssist, Triggerbot, Auto Shield Break, AutoMace and Ghost Hand found nobody at all. That is the
  loudest way "player detection is not accurate" shows up, and it was on by default.
- Friends and NPC markers were substring tests against the server-printed name. A name entered by hand
  (`Notch`) never matched `[VIP] Notch` or a colour-coded `\u00a7cNotch`, so friend protection stopped working the
  moment a server added a rank prefix - and NPC detection only knew four literal markers (`bot`, `npc`,
  `citizens`, `[shop]`), so a plugin NPC that simply *wears a real player's name* was invisible to it.
- Target tracking compared the raw UUID text with `==` and read the display name where a stable key was needed,
  and each module did its own `IsInstanceOf(entity, PlayerEntity)` test, so the modules could disagree about the
  same entity in the same frame.

## One service, and identity is the UUID
`agent/modules/player_detect.h` holds the rules (pure, no JNI); `modules.cpp` holds the JNI side and the caches;
every player-facing module goes through it.
- **Cleaning.** `§`/`&` codes (including the UTF-8 form), leading `[rank]` / `(team)` / `<tag>` blocks, trailing
  `:` `>` `|`, zero-width padding and space runs are removed, so the same player is one name however the server
  dresses them up.
- **Markers as whole tokens.** `NPC_Shop`, `[npc] Guard` and `Shopkeeper` match `npc`/`shop`; a real player called
  `npcguy`, `robot` or `Guardian` does not.
- **UUID read properly.** Version is taken from the canonical position, with or without dashes: `4` online
  account, `3` offline-mode account (a real player - this is the new default), `1`/`2`/`5` or an unreadable
  UUID = plugin-made.
- **Clone rule.** A second entity wearing a name another UUID already owns, or imitating the local player, is the
  plugin's copy. The *less account-like* of the two is flagged, and the most account-like keeps the name, so a
  real player is never dropped because an NPC spawned with their name first.
- **Friends.** Matching ignores decoration and accepts a UUID as the entry itself; a friend matched by name is
  then pinned to that UUID, so a rank change or nick plugin mid-session cannot silently un-friend them.
- **Caches hold strings only** - never a JNI reference, so they are safe between frames - forget identities no
  one has worn for 90 s, and are cleared whenever there is no world, so one server's names can never judge the
  next one's. With Anti-Bot off and no friends listed the UUID is not even read: the same cost as before.

## Wired through every module
ESP (including the friend colour), Nametags (names are shown cleaned), Triggerbot, AimAssist, AutoMace, Ghost
Hand, Auto Shield Break (both the crosshair path and the cone path, plus its mid-swap re-check), Auto Jump Reset,
Pearl Agro, the friend-join notifications, and `nearbyPlayerNames()` (cleaned, de-duplicated, NPCs skipped so the
friends page lists accounts rather than shop keepers). Target tracking now uses `trackedTargetKey()` - the UUID
when there is one, the cleaned name otherwise - and `findPlayerByUuid()` looks entities up by that key, so a
plugin renaming a target mid-sequence no longer loses it. The `PlayerEntity` test has a registry-type
(`minecraft:player`) fallback so a missing class handle cannot quietly turn player detection off.

## Settings
Anti-Bot gained **Allow offline UUIDs** and **Detect NPC clones** (both on by default) next to the existing two;
both are saved with the config and a defaults reset also clears the detection caches.

## Verified here
- `test\build-detect.bat` - **65 checks, 0 failed**: name cleaning (colour codes, tags, zero-width padding,
  separators, real names with underscores left alone), marker tokenisation, UUID version parsing with and
  without dashes, account/bot verdicts, and the clone decisions including the deliberate "a v4 duplicate is left
  alone" case. Writing it caught a real bug: `\u00a7cNotch` cleaned to `cNotch`, because only the `§` byte was stripped
  and not the code letter after it.
- `cmake --build build --config Release` - **exit 0, 0 warnings / 0 errors** at `/W4 /permissive- /EHsc /utf-8`
  (`modules.cpp`, `gui.cpp`, `agent.cpp` recompiled from scratch for that reading), and
  `build\Release\tyrant_agent.dll` is the newly linked binary.

## Still needs a live session
- The rules are unit-tested and the wiring compiles clean, but *targeting the right player on a real server with
  plugin NPCs* is a live observation and this machine has no second player. Restart or re-inject and the new
  detection is what the modules use; if a server still behaves oddly, the useful detail is whether its players
  get v4 or v3 UUIDs (offline mode) and whether NPCs share real players' names.

# Passive - the client is no longer ejected by a stray key or a stray click (random uninject)
Built with MSVC (Release) at `/W4` with no warnings. The unload path was exercised **live**: a message-pumping
GL 3.2 core host (the `test/wglhost_core.c` setup plus a key-posting variant) with the patched DLL injected into
it, and that process's module list read from outside while it ran - the observations are quoted per item below.

## What was actually happening
It was not a lost hook and not a crash: the client was **unloading itself**, and the source had exactly two ways
to ask for that. `g_requestUnload` is the only thing that starts the `FreeLibraryAndExitThread` path, and only
two call sites ever set it:
- **A bare `END` keypress.** `hkWndProc` unloaded the client on `vk == VK_END` with nothing else required, so a
  single stray press - or any module, macro or hotkey tool bound to END, and the keybind capture accepts END as
  a module binding - ejected the DLL from a perfectly healthy game. The client then has no menu and no modules
  because there is no DLL left to provide them, while Minecraft itself carries on: the "it randomly uninjects
  without me doing anything" report.
- **The SELF DESTRUCT button.** One click on Settings > General unloaded the client *and* reset every setting.
- Two more ways to look uninjected while the DLL was still loaded are fixed as well: `hooks::install()` parked
  the init thread on `Sleep(INFINITE)` when a just-injected DLL found the swap chain still settling (DLL loaded,
  overlay inert, client never came back), and nothing ever noticed a **lost render detour** - another overlay or
  the driver restoring the prologue of `wglSwapBuffers` stops our frames dead while the game keeps running.

## The emergency unload is a deliberate chord now, and it says so in the log
- `END` alone unloads nothing; the chord is **Ctrl+Shift+End**, checked against both the queue key state and the
  physical keyboard, so no single key, keybind or hotkey tool can fire it by accident. The Controls page text was
  updated to match.
- Every trigger names itself in the log (`unload requested: emergency chord Ctrl+Shift+End`,
  `unload requested: SELF DESTRUCT button (Settings > General)`), so the next report says *why* the client left.
- **SELF DESTRUCT is a two-step button**: the first click arms it (`CONFIRM UNLOAD`, logged, disarms itself after
  5 s), and only a second click unloads and resets the settings.

## The log was unreadable while the game ran - why the earlier rounds were flying blind
- `_wfopen_s(..., L"ab")` denies sharing, so nothing could read `tyrant_agent.log` while the client was up: on
  this machine today's live log was `Device or resource busy` to `cat`, `wc` and `grep` for exactly that reason.
- The log is opened with `_wfsopen(..., _SH_DENYNO)`; if the plain name is still held by an earlier session's
  leaked handle the client falls back to `tyrant_agent_N.log` instead of silently recording nothing, and the
  handle is closed on the way out of an unload.

## Recovery, instead of sitting there dead
- **Lost render detour:** the init thread now keeps a heartbeat. If `frameCount()` has not moved for 5 s while the
  window is up and visible it re-applies the detours (`MH_DisableHook` then `MH_EnableHook` - MinHook still
  believes they are enabled after an external unpatch, so enabling alone is a no-op) and re-hooks the WndProc.
  Bounded to 3 attempts per stall and ignored for a minimised/hidden window, so a window that is simply not
  rendering cannot churn the hooks.
- `ensureWndProcHooked()` can recover a WndProc chain that was restored behind our back: it re-chains only when
  the procedure it finds is the one it hooked, and leaves a third party's chain alone (recording that) rather
  than dropping the game's own handler out of the chain.
- `install()` is retried (2 s apart) instead of parking the thread for ever, and `hookOne()` accepts MinHook's
  ALREADY_CREATED / ALREADY_ENABLED from a retry or a rearm.
- `initGui` failures are logged, a throttled `heartbeat: <n> frames rendered, window <state>` line (once a
  minute) leaves a measurement where the log would otherwise just stop, and a throwing render callback is
  contained (the frame is closed cleanly, the next frame retries) instead of unwinding into the game.
- The unload worker's wait for the GL teardown is bounded (2 s) and it now detaches our own JNI threads and
  closes the log handle, so an unload cannot leave the client half-removed with the log locked.

## JNI: the cached env could be a different thread's env
- `jni::env()` returned one global `JNIEnv*` captured by whichever thread bootstrapped. Normally that is the
  render thread, but the 8-second no-frame fallback bootstraps on the init thread - after which every render
  frame handed the init thread's `JNIEnv` to JNI calls running on the render thread. Using another thread's
  `JNIEnv` is undefined behaviour and one of the ways a native call aborts the JVM or leaves the overlay dead.
  `env()` now resolves and caches the **calling** thread's env, and only detaches threads we attached ourselves.

## Verified
- Clean out-of-tree MSVC Release `/W4` build of `tyrant_agent.dll` and `tyrant_injector.exe`: **0 errors, 0
  compiler warnings** (the only messages were MSB8029, because the check build sat under %TEMP%).
  `build/Release/tyrant_agent.dll` is that same binary (md5 `5def06e78d9e9bfe60bf4115c6d39ebb`).
- `node test/xbow_fireside_check.mjs` -> **13/13 PASS**, exit 0.
- **Live, in a message-pumping GL 3.2 core host with the patched DLL injected** (its module list read from
  outside the process):
  - healthy startup: `install: wglSwapBuffers hook=ok, gdi SwapBuffers hook=ok`, `render frame 1`,
    `wndproc hooked (hwnd=... class=TyrantCoreGL)`, `agent ready`.
  - **regression:** a bare `END` posted into the window no longer unloads - the host's own procedure still
    received both messages (its counter went 137 -> 143 across the press, so they passed through our chain), the
    log held **0** `unload` lines, and `tyrant_agent.dll` stayed mapped.
  - **positive control:** Ctrl+Shift+End logged
    `unload requested: emergency chord Ctrl+Shift+End` -> `unload requested: tearing down` ->
    `teardown complete - releasing the log` in about 130 ms, and the DLL left the process (module list empty).
  - the log was readable **while the host ran** (read at two points mid-session), which it never was before.
  - the host survived the unload and kept rendering, so the teardown left nothing behind in the game's path.
- Not covered: a real Minecraft client was not available for this pass (the running instance still holds the
  previous DLL), so the chord has not been pressed by hand in-game - one live injection settles that.

# Passive - AutoMace Retry Mace, Breach Swap leads the swing, Cart Refill loop, fall-distance modules, Xbow shot arc, server-side silent aim, Pearl Agro, Nametags, Auto Pot
Built with MSVC (Release) at `/W4` with no warnings. Nothing here was run inside Minecraft; the checks that
were run are named per item below. The JNI layer was verified against the real 1.21.11 client jar by injecting
into `test/FullMc` (a JVM with the actual client classes on the classpath) - see the resolve log at the end.

## AutoMace: a Retry Mace wait timer
- A successful smash only set a hardcoded 0.25 s wait before AutoMace could arm again, so with the target still
  in front of it the module read as an autoclicker. There is now a **Retry Mace (ms)** slider (0-600, default
  250) used as the post-smash wait: the module cannot line up another mace until it has elapsed.
- Aborted swaps (target lost, landed early, timed out) keep their short waits, so a lost target never locks the
  module out. The field is saved and loaded with the rest of the config as `autoMace.retryDelayMs`, and the
  AutoMace card grew a row to hold the slider.

## Breach Swap: the swap now leads the swing, and is held long enough to be seen
- The module armed only on a **left-button tap seen in that one frame**. The overlay draws *after* the game has
  already run the tick's attack, and `gui::mouseTaps()` holds a tap only on the press frame - so a held attack,
  which is how a jump crit is normally made, produced no swap at all once the first frame had passed, and the
  mace never came out: "as i crit it doesn't swap to the mace".
- It now arms on the player's own attack being **held** as well as on a fresh tap. The mace is held while the crit
  it is covering can still land (attacking and still falling), never for less than 0.25 s so it always spans at
  least one game tick, and never longer than the Swap Back Delay. A swap that is undone before the game looks at
  the hotbar is a swap the game never saw.
- The default Swap Back Delay is 250 ms for the same reason, and configs saved with a smaller value are lifted to
  the floor by the code itself. The module still only ever swaps: it never attacks, so no critical hit is taken
  on its own.

## Cart Refill waits for the Xbow Macro to finish
- `updateCartRefill` runs on the same frame as `updateXbowMacro` (immediately after it), so the moment the macro
  placed its minecart a watched slot was empty and the refill fired **during** the sequence. With Silent Refill
  off, the inventory screen it opened then froze input and the macro's own guard ended the sequence before the
  shot - the cart was placed but never detonated.
- Cart Refill now holds every refill while `g_xbowStep` is not `Idle`, and the macro calls `cartRefillRequestNow`
  as it ends (`xbowMacroReset`), clearing the delay timer so the cart it used is restocked immediately after the
  action completes.

## Cart Refill no longer loops the inventory
- `updateCartRefill` used to call the screen helper **first** and only then look for a cart to move, so with a
  watched slot already stocked it opened the inventory, found nothing missing, closed it, and did it again for
  ever. It now works out which watched slot is missing a cart - and where the cart to move is - **before** the
  screen is touched, and returns without opening anything when there is nothing to do.
- The screen is also no longer closed the moment the swap packet is sent. It stays up until a pass finds every
  watched slot stocked: closing it straight away re-opened it on the next frame (the slot is not refilled until
  the server answers), which was the second half of the loop. Only the visible path opens anything at all;
  Silent Refill still moves the item with the ordinary slot packet and never shows a screen.

## Breach Swap and AutoMace: the fall was being read as zero ("does not swap", "does not switch when falling")
- Both modules arm on `!onGround() && fallDistance()`, and `fallDistance()` read the double field
  `Entity.field_6017` through a **`"D"`-only** lookup. `jni::field` answers null when the descriptor does not
  match, so on a build where that field is a float the getter returned 0 for ever and both modules silently
  never armed - which is exactly "it does not switch to the mace when I fall" and, because Spear Reach is armed
  by the same test, "Spear Reach does not work any more" as well.
- The lookup now tries the double descriptor and falls back to the float one, `fallDistance()` reads it with the
  matching accessor, and the resolve pass logs which one this build has: `Entity.fallDistance resolved as
  double`. Breach Swap needed no other change: the click is still what triggers it, and it only swaps.

## Xbow Macro: the arrow now lands in the minecart
- `solveThrowAngle` is a drag-free closed form, so with the crossbow's real 0.99 drag the arc it gives falls
  short and the arrow dropped into the flame instead of reaching the cart. A new `solveXbowShot` uses that closed
  form only as a starting guess and then searches the pitch against the **same per-tick simulation the flight
  uses** (3.15 b/t, 0.99 drag, 0.05 gravity, closest approach over 200 ticks).
- The fire step and the shot step both solve with it, and the shot step leaves the camera and the server on the
  solved rotation while the shot goes out, so the fire cell sits on the arrow's own line and the arrow passes
  through it into the cart.
- Checked with `test/xbow_fireside_check.mjs`: **13/13** approaches still place the flame on the shot line on
  the shooter's side, including the 1-3 block drops and both diagonals.

## Silent aim: the rotation packet is now resolved with everything else, and reported
- Silent aim is exactly one packet (`PlayerMoveC2SPacket.LookAndOnGround`): if that packet is not resolved,
  nothing ever reaches the server and the aim stays local - which is the report that made it a suspect.
- The nested class is pinned by number, so every candidate is tried and the accepted one is checked to really be
  a `PlayerMoveC2SPacket` and to really have the `(float, float, boolean, boolean)` constructor. Against this
  build's own client jar that is **`net.minecraft.class_2828$class_2831`**; `class_2830` is `Full` and
  `class_2829` is `PositionAndOnGround`, and their constructors do not match, so the search cannot pick them.
- The resolve pass now reports it, and the first packet sent (or the first failure) is logged once, so a live run
  says plainly whether the server was ever given the aim:
  `look packet: ready (net.minecraft.class_2828$class_2831)` and `ext: look packet sent (class ...)`.
  The behaviour is otherwise unchanged: the aimed rotation goes to the server, the camera and the previous-tick
  rotation are put straight back, and the hit is sent by the module while the attack button is held.

## Pearl Agro: the enemy's pearl is found by registry type, not by its English name
- The pearl was recognised by `displayName().find("Ender Pearl")`, which only ever matches on an English client -
  a localized game found no pearl at all, so the module never armed and never threw. It now compares the entity's
  **registry type path** (`Registries.ENTITY_TYPE.getId(type).getPath() == "ender_pearl"`), with the display name
  kept as a fallback; registry ids are the same text in every language.
- The search runs against the target **and** against the player (a pearl is normally thrown at you, and the
  thrower has often moved away from it), at 16 / 12 blocks, and a click that finds no pearl says so instead of
  failing silently.

## Nametags: item textures were upside down
- The item icons are decoded with the PNG rows already flipped for the GL upload (`toRgba(..., true)`) but were
  sampled with ImGui's top-first UVs, so every icon, trim overlay and enchant sheen drew mirrored. The three
  `AddImage` calls in `drawNametagItem` now sample V inverted (`uv0(0,1)`, `uv1(1,0)`).

## Auto Pot
- **It stopped repotting once an effect expired.** The module looked for the *first* missing effect and, if the
  player carried no potion for that one, it announced "no <effect> potion in the inventory" and gave up - so a
  missing effect the player has no potion for blocked every effect after it for ever. It now builds the list of
  everything that is missing and uses the **first one it can actually satisfy**, either from the pot slot or
  from the main inventory; only when none can be found does it say so. The inventory is also inspected before
  any screen is opened or clicked.
- **Health Pot (new).** With the option on, an instant health potion is drunk when the health is at or below
  the threshold (default 2.5 hearts, 0.5-10), and it is checked *before* the buffs so it takes priority. It
  matches either potion id (`minecraft:healing` and `minecraft:strong_healing`). The checkbox and slider are on
  the Auto Pot card; both settings save with the rest of the config.

## Verified
- `test/xbow_fireside_check.mjs` - **13/13**.
- Real-JVM check: the agent injected into `test/FullMc` (real 1.21.11 client classes on the classpath) reported
  `classloader acquired`, `JNI attached`, `Entity.fallDistance resolved as double`,
  `ext resolve: key=1 jump=1 sprint=1 net=1 send=1 look=1 (net.minecraft.class_2828$class_2831)`,
  `look packet: ready (net.minecraft.class_2828$class_2831)` and `resolve complete: 0 failed lookups (must be
  0)` - every pinned mapping the eight items depend on resolves against the shipping jar. Log: `test/fullmc-agent.log`.
- Overlay check: `test/wglhost_core.exe` + `build/Release/tyrant_injector.exe --pid <pid>` -> `INJECT-OK dll
  mapped; overlay handshake ok`, `HOST: frame 300 non-background pixels=32016 / 307200`.

# Passive - Xbow reach/ledge/aim, silent aim that lands, Pearl Agro, Breach Swap, inventory screens, tab counts
Built with MSVC (Release) at `/W4` with no warnings. Nothing here was run inside Minecraft; the checks that
were run are named per item below.

## Xbow Macro
- **It refuses to start unless the block it builds on is in block reach.** Every stage is built on the block
  under the crosshair (the rail goes on it, the cart on the rail), and a placement the server will not accept
  left the sequence half-finished behind. The bind now checks the crosshair really hit a block AND that its
  centre is inside `PlayerEntity.getBlockInteractionRange()`; otherwise it says so and does not start.
- **The flame is now placed from the shot itself, not from the cells around the cart.** Two things make a cell
  beside the cart useless: a crossbow arrow is only *sampled* once per tick and carries itself ~3.15 blocks per
  tick (the same step `drawTrajectoryPreview` walks), so the flight touches only two or three cells before the
  cart; and from a diagonal approach the arrow reaches the cart through the corner, so the cells beside it are
  clipped at a single point. The Fire step solves the shot, flies it per tick, and lights the nearest cell to
  the cart that an arrow sample is actually inside, never the cart's own cell and never anything past it.
- **Ledge xbowing.** When the shot comes from below, the arrow is under the flame's own level until it is
  almost at the cart, so there may be no sample at flame height at all. The step then falls back to the first
  cell on the shot **line**, which is the cell beside the cart - so the flame is still right next to the cart and
  the shot still passes through it. This is the case the previous build answered with "nowhere to light it".
- **The shot is now aimed, and the view shows it.** After the fire is lit, the crossbow step solves the
  projectile arc to the cart (gravity and drag included), writes that rotation and sends it to the server, then
  fires - and leaves the view on it, so the camera shows the shot that is actually being taken and the fire cell
  sits on that same line. It used to rotate for the shot and put the camera straight back, which read as "it
  shoots at the fire".
- Checked with `test/xbow_fireside_check.mjs`, which mirrors the shipped rule (solve, fly per tick, samples
  first, then the line) against vanilla's own step: **13/13** approaches place the flame on the shot line on the
  shooter's side, including 1-3 block drops (ledge xbowing) and both diagonals; 5 of them land in the cell right
  beside the cart.

## Silent aim now actually lands the hit
- The aimed rotation was already sent to the server with the camera left alone - but nothing else happened. The
  game only attacks what its own crosshair is on, and the crosshair never moves in silent mode, so a click went
  to whatever the player was facing and the silent aim had no effect on the fight at all.
- With Silent AimAssist on, the module now sends the hit itself while the attack button is held, with vanilla's
  own pacing (nothing before the attack is charged, nothing during the miss delay, one hit per frame at most).
  That is what makes it an aim: the target is hit while the view stays exactly where the player put it. It never
  attacks unless the attack button is down, so the module is not turned into an auto-clicker.

## Pearl Agro
- The enemy you attacked is remembered for **3 seconds** (was 6). Their pearl is now followed while it is in
  the air and its landing spot is re-solved from its **live** position and velocity every frame, so the throw is
  aimed at the spot the pearl is really going to land on rather than the first guess - you come out of the throw
  already next to the player you were fighting. It throws once the Agro Delay has passed and the estimate has
  settled (or the pearl is nearly down), with a hard cap so a pearl thrown straight up cannot stall it, and the
  search radius for the enemy's pearl went from 4 to 12 blocks so it is still found after it has travelled.

## Breach Swap
- It was attacking on its own whenever the conditions happened to hold, which is why critical hits kept firing
  by themselves. It is now triggered **only by the player's own click**, and it only ever **swaps**: the click is
  the attack, and with the mace already in hand when the game runs that click, the hit is the mace hit.
- The mace must have **Breach** on it. A plain mace no longer qualifies (it used to fall back to one), because
  the enchantment is the whole point of giving up the sword's hit.

## Inventory modules opened the wrong key
- Auto Totem, Cart Refill, Auto Clean and Auto Pot's non-silent path opened the inventory by synthesising the
  **E** key. The inventory key is rebindable, so on a setup where E is crouch the modules crouched and never
  opened anything.
- They now open and close the real screen through the game's own API - `MinecraftClient.setScreen(new
  InventoryScreen(player))` and `setScreen(null)` - which is exactly what the vanilla inventory key does, and the
  close goes through the screen's own `removed()` so the server still gets its close packet. New helpers
  `mc::Client::openInventoryScreen()` / `closeScreen()`; if the pinned class could not be resolved the open
  answers false (and logs once) instead of guessing a key.

## Interface
- Every sidebar tab now shows **how many modules it holds**, counted from the live module list each frame, under
  its icon (accent when the tab is selected, muted otherwise).

## What could not be checked here
- No Minecraft on this machine: everything above is compile- and harness-verified only. The two pinned mappings
  the inventory screen relies on (`class_490` = InventoryScreen, `method_1507` = setScreen) are guarded by a
  `IsAssignableFrom(Screen)` type check, so a wrong name answers false and logs once rather than installing
  something that is not a screen - the `resolveAll` "failed lookups" line is the first thing to read if the
  inventory stops opening.

# Passive - Xbow Macro fire side + true silent aim
Built with MSVC (Release) at `/W4` with no warnings. Nothing here was run inside Minecraft; the geometry check
and the injection harness in `test/` are the evidence.

## Xbow Macro: the fire was lit on the wrong side of the cart
- The fire cell was chosen from the four neighbours of the cart by comparing each one's horizontal offset against
  `toward` (the direction from the eye to the cart) and keeping the best dot product - except the comparison was
  inverted: `bestDot` started at `-2` and the loop kept the **largest** dot, which is the neighbour whose offset
  points *away* from the player, i.e. the far side of the cart. The fire was therefore lit **behind** the cart,
  where the arrow can never reach it, and the shot simply bounced off the cart's side.
- The direction is now the other way round, and the cell is no longer taken from the four cells beside the cart
  at all - see the next section, which is what makes the arrow actually catch fire.

## Xbow Macro: the flame is now a cell the arrow is really in
- Fixing the side was not enough. Arrows are set alight where the game actually **samples** them, and a crossbow
  arrow carries itself about **3.15 blocks every tick** - the same step `drawTrajectoryPreview` already walks - so
  the flight touches only two or three cells between the eye and the cart. A single fire block in the cell beside
  the cart is therefore normally stepped clean over, and the arrow arrives unlit and does not detonate anything.
  A 45 degree approach is worse still: the arrow reaches the cart through the corner, so the cells beside the cart
  are only clipped by its hitbox in that one spot.
- The Fire step now solves the shot it is about to take (`solveThrowAngle(3.15, 0.05, flat, dy)`), flies it one
  step per tick exactly like the trajectory preview, keeps the samples that are at the flame's own height and
  still short of the cart, and lights the **nearest one to the cart that can hold fire** (empty cell, something
  to burn on). It never picks the cart's own cell and never anything past the cart, so the flame is always on the
  shooter's side, on the line the arrow takes - and as close to the cart as a 3.15 block step allows, normally
  1-3 blocks.
- Two faults in the first attempt at this were caught by the check below before it shipped: the cell walk ran the
  wrong way along the line (which lit the far side again), and the samples were allowed to carry on past the cart
  - a step can jump clean over the cart's own cell - so a shot from close range would have lit the fire in the
  cart's own block.
- A shot from closer than about 4 blocks is now refused with "stand further back": the arrow reaches the cart
  before its first sample, so there is nowhere to put a flame it would pass.
- Checked for 15 approaches (every axis, all four diagonals, two off-axis angles, 4 to 20 blocks away) against a
  per-tick simulation of the shot: **15/15** place the flame in a cell an arrow sample lands in, every one on the
  shooter's side. The cell immediately beside the cart manages that in 2/15.

## Xbow Macro: the fire step now looks at the block it lights
- The interaction named the block and face explicitly, but the server validates a `PlayerInteractBlockC2SPacket`
  against **the rotation it holds for that player**, not against the coordinates in the packet. With the view
  left where it was, our raycast and the server's disagreed about what was being clicked - the desync.
- The fire step now solves the yaw/pitch to the point the flame is lit against (the **centre of the top face of
  the block under the fire cell**, which is both where the flame appears and what the server's own raycast
  resolves), writes it to the player and sends a look packet with it **before** the interaction, so client and
  server agree on both the rotation and the clicked face.

## Silent aim is now silent on every module that offers it
- "Silent" now means one thing everywhere: the player's rotation is turned towards the target and **told to the
  server**, while the on-screen camera keeps the player's own rotation. Nothing visible moves.
- `silentAimSend` writes the aim and restores the live rotation inside the same call, so the camera never leaves
  where the player put it, and it now also saves/sets/restores the **previous-tick rotation**
  (`Entity.lastYaw` / `lastPitch`, the value the renderer interpolates *from*). A game tick landing inside the
  spoof would otherwise stamp the aimed angle into that field: the renderer would then interpolate from the aim
  back towards the camera and visibly swing the view for a frame, and vanilla would send a rotation packet of its
  own on top of the aim. With the field pinned for the duration of the spoof and restored after it, neither
  happens. `Entity::setPrevYaw` / `setPrevPitch` were added for this (`agent/jni/mc.cpp`).
- Applied to every silent aim path: **Silent AimAssist** (via `silentAimSend`, every frame while a target is
  held), **Auto Pot** (the look-down to 89 degrees - pitch *and* previous pitch restored so the interpolation
  source cannot leak the spoof), **Pearl Agro** (the solved throw rotation), and the **Xbow Macro's** shot.
  When a module stops aiming, one more look packet hands the camera's real rotation back to the server, so the
  server is never left looking where the silent aim last pointed.
- The non-silent variants are unchanged: they rotate the real view, and the packet the game already sends every
  tick carries it.

## Still needs a live check
- Nothing was run inside Minecraft. The Xbow sequence (rail -> cart -> fire on your side -> shot) and the silent
  aim paths were compiled and exercised only against the GL hosts in `test/`; a live run is what will confirm the
  cart detonates instead of the arrow hitting its side, and that the camera does not twitch while AimAssist is
  silent.
- The check above trusts the trajectory preview's own model of the flight (one position per tick, 0.99 drag,
  0.05 gravity). If a live run shows the arrow still not catching, the most likely cause is that the game samples
  the arrow somewhere else than that model says, and the `tyrant_agent.log` line the macro already prints
  ("the fire could not be lit on the shot line" / "stand further back") says which way it went wrong.

# Passive - overlay window binding fix (interface going invisible and scrambled)
Built with MSVC (Release) at `/W4` with no warnings. Driven through the test hosts in `test/` - including a new
**core-profile** host that reproduces Minecraft's GL 3.2 CORE context, which the original compatibility-context
host never exercised.

## The bug
- The overlay's window handle was captured **once** and never updated: `install()` took whatever
  `FindWindowW(L"GLFW30", nullptr)` returned (a search of the *whole desktop*, so it can be another
  application's or another Minecraft instance's window), and the first swap only filled it in when it was still
  empty. Everything that sizes and routes the interface hung off that handle - `ImGui_ImplWin32_Init` (so
  `io.DisplaySize` comes from `GetClientRect` of that window), the `WndProc` chain, and `syntheticKey` /
  `syntheticClick`.
- So as soon as the handle no longer described the window actually being presented - Minecraft replacing or
  recreating its window after injection, a fullscreen or resolution change, or a second instance's window being
  picked up first - the interface was laid out against a stale or foreign client rect. An empty or wrong
  `DisplaySize` builds a degenerate frame: the panels vanish and whatever text still rasterises is projected
  into the wrong rectangle, which is exactly the "text and the UI goes invisible and obfuscated" report. The
  WndProc could also fail to hook (or hook the wrong window), and synthetic input went to the wrong place.

## The fix
- The window is now **re-derived from the swap chain's DC every frame** (`WindowFromDC`), which always belongs to
  this process, and re-adopted when it changes. Re-adoption undoes the old WndProc chain, re-points the ImGui
  Win32 backend at the new window (so `DisplaySize`, DPI and mouse routing follow it) and lets the WndProc hook
  re-attach to the real window on the same frame.
- `install()` no longer searches the whole desktop: the fallback lookup is **scoped to this process**
  (`EnumWindows` + `GetWindowThreadProcessId`), preferring a titled GLFW window, so a second Minecraft instance
  can never be selected.
- A window with an **empty client rect** (minimised, or between a destroy and a recreate) now skips the frame
  instead of building a degenerate one, and logs it once.
- **Scrambled text from the first frame**: the pixel-store state the overlay's own uploads depend on is now
  neutralised every frame. The font atlas and the item icons are uploaded from tightly packed CPU buffers, but
  ImGui's GL backend only zeroes `GL_UNPACK_ROW_LENGTH`; Minecraft uploads its atlases with `ROW_LENGTH` plus
  `SKIP_ROWS` / `SKIP_PIXELS` set and leaves whatever it used last behind, and an inherited skip offset shears a
  row-major upload into noise. The overlay now zeroes `UNPACK_ROW_LENGTH` / `UNPACK_SKIP_ROWS` /
  `UNPACK_SKIP_PIXELS` / `UNPACK_ALIGNMENT` before the frame and restores the game's values afterwards (they are
  part of the usual save/restore), so a texture it uploads can never inherit the game's state.
- `initGui` can no longer leave a half-built context behind: a failed Win32/GL backend init tears the context
  down before returning, so a retry cannot end up rendering against a stale font atlas.
- New one-shot diagnostic in `tyrant_agent.log`, re-printed whenever the client size changes:
  `render diag: window=TyrantCoreGL client=624x441 glViewport=0,0 624x441 fbo=0 profile=0x1 display=624x441 scale=1.00x1.00 fonts=3 atlas=512x256 fontTex=1 gameUnpack(row/skipRows/skipPixels)=0/0/0 gl=3.2.0 ...`.
  It carries the window class, the client size, the GL viewport, the framebuffer, the profile mask, ImGui's
  `DisplaySize` / `DisplayFramebufferScale`, how many fonts loaded and the atlas texture it built, and the
  game's own unpack row/skip state - so a broken run shows at once whether the interface is being laid out
  against a different rectangle than it is drawn into, whether the font atlas exists, and whether the game left
  an upload offset behind.

## Ruled out (so the next report can skip it)
- **GLSL version**: `ImGui_ImplOpenGL3_Init("#version 130")` was suspected, since Minecraft renders with a core
  profile context while `test/wglhost.exe` uses a legacy compatibility one. `test/wglhost_core.c` now creates the
  same **GL 3.2 CORE + forward-compatible** context as the game; the shaders compile with no error on stderr and
  the interface rasterises (verified by reading back the presented image, 32k non-background pixels). The GLSL
  version is therefore unchanged.
- **Textures and ImGui state**: the font atlas is uploaded once and never invalidated, every style/font/ID push
  is balanced, and the GL state the overlay restores already covers everything ImGui disturbs.

## Still needs a live check
- Nothing was run inside Minecraft. The diagnostics line above is the fastest way to confirm the fix: after a
  fullscreen toggle or after Minecraft recreates its window, the log should show a fresh
  `swap window adopted (hwnd=... )` followed by a `render diag:` line whose `client` and `glViewport` agree.

# Passive - round 7 (multi-instance injector, new modules, macro and nametag fixes)
Nothing here was run inside Minecraft; the whole solution was compiled with MSVC (Release) at `/W4` with no warnings.

## Injector
- **Multi Instance Injecting** is a second tab next to the one-button injector. It lists every running
  `javaw.exe` / `java.exe` process with the title of its visible window and a marker when Tyrant is already
  loaded, refreshes every two seconds (or on **Refresh**), and injects into the **selected** process instead
  of the first Java process found. The handshake, mapping and "did it stay loaded" checks are the same code
  path as the simple tab, so a multi-instance injection reports the same errors.

## New modules
- **Cart Refill** (Misc): keeps the checked hotbar slots stocked with a TNT minecart, moving a cart out of the
  main inventory with the ordinary slot packets. **Refill Slot 1-9**, **Refill Delay (0-600 ms)** and
  **Silent Refill** (off = the inventory is opened so the move is visible).
- **Auto Pot** (Misc): when an allowed effect is missing the potion is moved into the **Pot slot**, the player
  looks straight down and it is used. **Allow Strength / Speed / Fire Resistance / Weaving / Regeneration**,
  **Pot slot (1-9)**, **Inventory Delay (0-600 ms)**, **Silent Aim**, **Silent Inventory** and
  **Pot Refill After Use**.
- **Pearl Catch** (Mace): the bind throws an ender pearl, then a wind charge, each with its own 0-600 ms delay
  (**Pearl Delay**, **Wind Charge Delay**). Both items have to be in the hotbar.
- **Pearl Agro** (Combat): remembers the last entity you hit; when a pearl appears beside it the landing spot
  is solved from the pearl's real velocity, and after **Agro Delay (0-600 ms)** your own pearl is thrown to
  land on the same spot (**Silent Aim** aims the throw without turning the camera).
- **Breach Swap** (Mace): a critical hit while holding a sword swaps to a (breach) mace for that one click, so
  the hit lands mace damage with the sword's already-charged cooldown. **Swap Back**, **Swap Back Delay
  (0-600 ms)** and **Silent Swap**.

## Changed modules
- **AimAssist**: the silent aim now re-sends the aimed rotation every frame and, because the camera rotation is
  put straight back, the vanilla tick finds no rotation of its own to send and can no longer overwrite the aim
  on the server. Switching off (or losing the target) sends the camera's rotation once more, so the server is
  not left looking where the aim stopped. Added **No Glide**.
- **AutoMace**: added **Prefer Density** (use the Density mace for a smash from high ground, keep it holstered
  on level ground) and **No Glide** (stand down while an elytra is gliding).
- **Xbow Macro**: the rail step now accepts **rail, powered rail, activator rail and detector rail**. The fire
  is lit in the cart's horizontal neighbour that the arrow actually enters through (the side nearest the
  shooter, chosen from the geometry rather than the crosshair), and it is lit with an explicit block+face call
  so the camera is never rotated to place it. The shot aims, fires and puts the view straight back inside one
  frame, which removes the 180-degree turn the macro used to leave behind. **Recharge** now holds the vanilla
  use key so the game performs the whole 1.25 s reload itself and releases it the moment the crossbow is
  loaded (a single use call only started the charge and never finished it).
- **Auto Pressure Plate Web**: a plate the sequence already started on is kept for a 0.35 s grace window when
  the crosshair leaves it mid-swap, so a transient loss no longer drops the placement, and a rejected click is
  retried after 0.45 s instead of 0.8 s.
- **Auto Drain (Web mode)**: the check now runs on the cell the click would actually fill instead of the block
  under the crosshair, so looking at the **top face** of the block a water source sits on drains it instead of
  refusing (or air-placing) the web. The placement cell is re-checked on the frame of the click.
- **Nametags**: the durability bar reads `getDamage` / `getMaxDamage` and falls back to the item stack's own
  component text (`minecraft:damage=`, `minecraft:max_damage=`), which is what left the bar pinned at a full
  green bar. The armour trim overlay now only uses the 16x16 item-side trim texture — the model-side armour
  sheet that was used as a fallback is a full armour texture, and drawing it over an item icon is what
  scrambled the armour icons. The enchantment glint is also a soft sheen instead of a heavy purple wash.

# Passive - round 6 (interface polish, new macros and modules)
Nothing here was run inside Minecraft; the whole solution was compiled with MSVC (Release) only.

## Crash fix: enabling Nametags killed the game
Two hard faults in the new icon loader, both of which abort the JVM rather than raise an error:
- The loader cached class references in a static struct, but `jni::findClass` hands back a **local**
  reference, which is only valid until the frame that created it ends. The next frame reused a dangling
  reference and the JVM aborted. Class references are now promoted with `NewGlobalRef` (and the locals
  released), which is what the rest of the client already does.
- The Item Scroll screen check leaked one local reference per lookup: it released the screen's class and
  then dropped the superclass reference without releasing it, so the JVM's 512-entry local reference table
  would fill up in a few seconds and abort. Every reference is now released on every path.
The loader no longer touches AWT or ImageIO at all: the bytes come from `java.base`
(`ClassLoader.getSystemResourceAsStream` + `readAllBytes`) and are decompressed with the JVM's own
`java.util.zip.Inflater`, then decoded by our own reader in `agent/render/png_image.h` (1/2/4/8-bit
greyscale, RGB, palette, greyscale+alpha and RGBA, non-interlaced). Only classes that are always present
are loaded inside the game's JVM now.
- Nametags also gained an **Item icons** switch: turn it off to keep the board on plain text labels.
- `agent/render/png_image.h` is covered by `tools/png_decode_test.cpp`, which decodes PNGs produced by an
  independent implementation (`tools/png_testdata.ps1` uses .NET) and compares every pixel, including the
  unfiltering and the GL row flip. It is not part of the CMake build. Both cases pass against .NET's own
  decoder at `/W4` with no warnings: an RGBA8 image (7x5, varying alpha) and an 8-bit **palette** image
  (16x9, 64 real palette indices, written through `LockBits` because `SetPixel` does not work on indexed
  bitmaps). The expectation files are produced by re-decoding the saved PNGs, so they describe what an
  independent implementation actually reads out of the file.

## Interface
- **Sliders**: every slider (interface and module) is now one widget — a filled value bar instead of a
  track with a thumb. The fill animates toward the value, and clicking the value readout lets you type an
  exact number, which the bar then animates to. The `%` sliders keep their 0-1 storage.
- **Appearance**: the module **Style** and **Module Shape** selectors now live inside their own
  **Module Appearance** section instead of floating under the colour picker.
- **Squared shape**: square cards now keep their description. The configuration is no longer drawn inside the
  card — it is a **separate square panel attached directly under it**, animated open and closed, drawn after
  every card so it overlaps the modules below instead of being glitched by them, and **scrolling inside the
  square** (with a scrollbar indicator) when the option list is taller than the panel.
- **Crosshair**: the game hides its crosshair the moment any screen opens (inventory, Escape, settings), which
  is why it disappeared. Tyrant now draws its own crosshair while a Minecraft screen is open, under the panels,
  and still leaves the vanilla one alone in world.
- **Debug Mode** (Settings > General > Interface): modules now only report their actions while this is on, and
  those toasts carry a green **DEBUG MODE** chip. **Notifications** is a separate switch for "Friend <name>
  joined the server" and "Enabled/Disabled <module>".

## Changed modules
- **AimAssist**: the **Silent** option is back (the aimed rotation goes out as a look packet while the camera
  keeps the player's own rotation), plus an **Accuracy** dropdown — **Center** (middle of the body), **Head**
  (eye height) and **Loose** (a point inside the hitbox near where the crosshair already is).
- **Nametags**: armor and the held item are drawn as real icons instead of text — loaded from the game's own
  asset namespace, with the armor trim overlay tinted by the trim material, an enchantment glint on enchanted
  pieces, a Minecraft-coloured durability bar under every damageable piece (hue = remaining/total / 3), and the
  held item's stack count. A texture that cannot be loaded falls back to the text label, and **Item icons**
  turns the textures off entirely. See the crash fix above for how the textures are loaded.
- **StaticFov**: switching it off restores the field of view that was in place before the module applied its
  own value, so the FOV never stays stuck at the extended setting.
- **Auto Totem / AutoClean**: both gained a **Silent** switch. With it off the module really opens the player's
  inventory (a synthetic vanilla inventory key press), keeps it up long enough to be seen, does the work with
  the ordinary slot packets and closes it again — never leaving it open, and never taking over a screen the
  player already had open.
- **Auto Bucket Drain is now Auto Drain** with a **Mode** dropdown: **Bucket** keeps the original draining, and
  **Web** buries the water source in a cobweb instead. The two modes have their own option groups; selecting
  Web hides the bucket options and shows **Prefer Buckets If No Webs**, **Ignore Own Water**, **SwapBack**,
  **Use Offhand Web**, **Swap Delay (MS)** and **Silent Swap**. Web draining only ever clicks when the fluid
  pick is the water source *and* the cell the click would fill is that same source cell, so it can never air
  place, and it keeps ignoring water the player placed themselves.
- **Ghost Hand** is no longer an auto-attacker: it only assists a hit you are making. It needs the attack button
  held, and only steps in when the game's own pick lands on a block (the cobweb in the way) instead of the
  entity, so it cannot attack anything you did not click at.

## New modules
- **Cobweb Macro** (Macros): the bind swaps to a cobweb, places it under the crosshair and swaps back after
  45 ms — or places it in the same frame with **Silent Swap**, so the hotbar never visibly changes.
- **Xbow Macro** (Macros): rail -> TNT minecart -> flint and steel lighting a fire beside the cart -> crossbow
  shot through the flame, each stage with its own 0-600 ms delay. The shot is solved for the real projectile
  arc, so it works from an angle rather than only when the fire and cart share an axis. **Silent Fire** lights
  the fire without the first-person camera moving, **Recharge** reloads the crossbow after the shot, and
  **Swap back** returns to the slot you started on. Both the rail and the cart have to be in the hotbar.
- **Trajectory Preview** (Visual): while you hold an ender pearl, egg, snowball, wind charge, experience bottle,
  a potion or a loaded crossbow, the vanilla per-tick flight is simulated against the real world and drawn as a
  path with a landing marker.
- **Item Scroll** (Misc): hold Shift and the left button in a container screen and the click is repeated as the
  cursor passes over stacks, so items quick-move. Container screens are recognised by reflection (a field of
  the screen holds the `ScreenHandler`), so the pause menu, chat and the settings screens are never clicked in.

## Fixes / integration
- New synthetic input path: Tyrant can post a real key press or mouse click to the Minecraft window. The
  messages are tagged, so the game and GLFW see ordinary input while Tyrant's own keybinds and mouse taps
  ignore it completely — modules can open the inventory or repeat a click without triggering anything.
- JNI: `interactBlockAtFace` (place against an explicit face instead of always the top) and `objToString`
  (read an ItemStack's component text for trims, enchantments and loaded projectiles).
- New item-icon loader: icons come out of the game's own class loader and are decoded with the JDK image
  reader, then uploaded once per icon and cached — no Minecraft mappings and no extra native dependency. A
  missing icon degrades to the previous text label.

## Build
- The injector target's three long-standing warnings (an unused local, an unreferenced `wWinMain`
  parameter and an unused static helper) are gone, so a full solution build is warning-free.

## Not verified / known limits
- Nothing was run inside Minecraft, and there are no automated tests; every new behaviour is compile-verified
  only. Entity gating, the macros, the visible-inventory paths and the texture loader need a live check.
- **Silent** aim and silent swaps are approximations on the render thread: the rotation is set, sent as a look
  packet and restored in the same frame, so a live check should confirm the server sees the aimed rotation.
- Both macros require their items in the hotbar (no main-inventory shuffling), matching AutoMace and Auto Shield
  Break.
- In Squared shape the option widgets scrolled out of a panel can still answer hover (ImGui hit rects are not
  clipped by the clip rect), and during the closing animation a panel briefly shows only its frame.

# Passive - round 5 (interface + experimental modules)
Nothing here was run inside Minecraft; the whole solution was compiled with MSVC (Release) only.

## Interface
- Module **Style** moved from the header button into **Settings > Appearance**, next to a new independent
  **Module Shape** selector: **Rectangle** (default, the vertical list) and **Squared** (every module is a square
  card laid out three per row; the configuration is a square panel on the card). The shape is saved in presets.
- New **Settings > Friends** page: type a username or pick a player currently in the world to add them, and each
  friend row has a remove button. The list is saved with the module config and matched case-insensitively.
- The OS mouse cursor is no longer force-hidden while a Minecraft screen is open. It used to be hidden the moment
  the Tyrant menu closed, which also hid it inside the inventory, the Escape menu, and chest screens; it is now
  hidden only while the menu is closed *and* no Minecraft screen is open.

## New (experimental)
- **Anti-Bot** (Misc): keeps the entity modules off bots and NPCs. Two signals, both configurable: a display name
  carrying a bot/NPC marker, and a UUID that is not a normal (version 4) player UUID.
- **Nametags** (Visual): a billboard above each player showing name, health, distance, armor pieces with durability,
  and the held item with its stack count. The board scales with distance so it does not fill the screen up close.
  Item and armor "icons" are compact text labels rather than item textures (the overlay has no item texture atlas).
- **Ghost Hand** (Misc): attacks players or mobs that are standing inside a cobweb when they are in front and within
  interaction range, instead of letting the cobweb block pick win.
- **StaticFov** (Visual): a field of view above the vanilla 30-110 slider limit, applied by writing the FOV option's
  raw value field so the game's own validator no longer clamps it.
- **Auto Totem** (Crystal): when the off hand no longer holds a totem, swaps a hotbar totem into it (and, with
  "Totem Refill slot", keeps a chosen hotbar slot stocked) using the ordinary F key / number key slot packets. It
  only acts while no other screen is open, so the slot-click sync id always matches the server.
- **AutoClean** (Misc): drops junk stacks one at a time with the vanilla Q-key slot packet. **Silent Clean** at
  (default) drops with no inventory screen open; with it off, it only works while the player's own inventory is the
  screen that is open. **Scan Delay** (0-600 ms) paces the scan and a custom item-id list adds to the built-in junk set.

## Fixes / integration
- ESP: friends can be drawn in their own color (**Highlight friends** + friend color picker), and 3D ESP now has a
  health indicator like 2D.
- The friends list gates AutoMace, Triggerbot, Auto Shield Break, AimAssist and Ghost Hand; Anti-Bot gates the same
  modules for bots/NPCs. Auto Bucket Drain has no entity target, so there is nothing for it to skip.
- JNI: added the player's PlayerScreenHandler sync id, `clickSlot` (SWAP / THROW), `playerScreenHandler` /
  `currentScreenHandler`, `SlotActionType.THROW`, and a raw `SimpleOption.value` write used by StaticFov.

# Passive - round 4 (module fixes)
Nothing here was run inside Minecraft; the whole solution was compiled with MSVC (Release) only.

## Fixes
- AutoMace: "Swap delay" is now a real per-engagement reaction delay - it starts the frame falling + a valid
  target are first both true and resets whenever either is not, instead of being consumed once and then ignored.
  The target is tracked by UUID for the whole sequence, so swapping to the axe (Stun Slam) or mace no longer drops
  it. Spear Reach no longer silently swaps back in one frame: it now visibly swaps spear -> mace and smashes
  through the normal path, and still runs the axe step when Stun Slam is on.
- Lunge Macro: the return slot is remembered for the whole lunge session and cleared only when the macro stops, so a
  hold-to-loop can no longer leave the player stuck on the spear when the original slot was forgotten.
- Auto Bucket Drain: "Ignore Own Water" was checked before the render thread ran, by which point the water bucket had
  already been consumed ("still holding water_bucket" was always false), so every own placement was drained. It now
  detects a placement from the bucket leaving the hand / the raw right-click and rescans real water sources around the
  crosshair for a short window. Added off-hand bucket support (uses the off hand directly, nothing enters the hotbar)
  and the source bucket is chosen from the held slot first, so multiple buckets no longer jump between slots.
- Auto Pressure Plate Web: added off-hand cobweb support.
- Triggerbot: requires a fully charged swing (>= 0.99) under "Require full attack charge" instead of 0.90, so it no
  longer fires the half-charged sweep the old threshold produced.
- Arraylist: entries are now sorted A to Z (case-insensitive) instead of longest-first; each row's panel width still
  tracks its own text length.
- AimAssist: the server-side-only "silent" mode did nothing (the game's own movement packet overwrote the rotation
  each tick and the client raycast still used the real view). It now aims the real view, which is what actually lands.
- ESP: stopped after a while because local JNI references were never reclaimed on the attach/own-by-Minecraft render
  thread (not a native method, so nothing freed them) - one small leak per frame eventually exhausted them. The frame
  now runs inside PushLocalFrame/PopLocalFrame, and the per-frame leaks in getDoubleOption / textToString / gameModeId
  were fixed too.
- Auto Jump Reset: added a health-drop fallback next to the hurtTime edge and a longer key hold (spans two ticks),
  and it logs `auto jump reset: jumped on hit` so a miss is visible in tyrant_agent.log.
- JNI: `interactItem` / `interactBlockAt` accept a hand (off-hand support for the bucket and web modules).

# Passive - round 3 (fixes + experimental modules)
Nothing here was run inside Minecraft; sources were syntax-checked only (modules.cpp, mc.cpp).

## Fixes
- Triggerbot: now gated like a real click (no screen, not eating/using an item, shield not raised, vanilla miss delay)
  and attacks through `MinecraftClient.doAttack()` - one attack + one swing packet instead of a raw `attackEntity` + extra swing.
- Lunge Macro: swap only happens when it is meant to. Silent swap OFF = visible swap, wait for the spear's own attack
  charge, jab, swap back, repeat (hold-to-loop). Silent swap ON = swap/jab/swap back in one frame, paced by
  "Silent jab interval". The "did not accept the jab" toast only appears after ~0.8 s of real failure.
- Auto Bucket Drain: 1.5 s watchdog on pending state + automatic reset after 2 rejected pick-ups (what toggling did by hand).
- Auto Pressure Plate Web / Bucket: swap delay 0-1 ms now swaps and acts in the same frame; the use-cooldown gate no longer applies between swap and place.

## New (experimental)
AutoMace's Spear Reach is the attribute swap: with a spear in hand and the target picked at spear range, it swaps to the mace, sends the attack and swaps back inside one frame.
Fast Place, Sprint, Auto Jump Reset, AutoMace (+Stun Slam, Swap Back, Spear Reach), AimAssist (+Silent Assist).
UI: header "STYLE" button cycles Default / Rounded / Squared / Minimal / Outline for module cards and their controls (saved in presets).

# Tyrant — fix round (module audit)

Everything below was found by checking the code against the 1.21.11 mappings in `mappings/` and
`tools/mappings/` — not by guessing. A script cross-checked every method/field descriptor in
`agent/jni/mc.cpp` against the mappings: 0 mismatches remain (it flagged exactly one before the fix).
All sources were also syntax-checked (MinGW, C++20). **Nothing was run inside Minecraft.**

## Root causes fixed

| Module | Cause | Fix |
|---|---|---|
| AutoAnchor, Auto Bucket Drain, Auto Pressure Plate Web | `ClientPlayerInteractionManager.interactBlock` was resolved with the wrong descriptor (`PlayerEntity`/`HitResult` instead of `ClientPlayerEntity`/`BlockHitResult`), so the lookup failed and every block interaction silently returned false | descriptor corrected in `mc.cpp` |
| AutoAnchor, Bucket, Web | `kReg_BLOCK` pointed at `Registries.ENTITY_TYPE` (`field_41177`), so every block id came back as an entity id and `respawn_anchor` / `water` / `cobweb` / `*_pressure_plate` were never recognised | now `field_41175` (`Registries.BLOCK`) |
| ESP (2D + 3D) | camera axes were mislabeled: `getHorizontalPlane()` is the camera's *forward* axis and `getDiagonalPlane()` the sideways one, and the fallback "right" vector was actually the left vector (mirrored X) | basis rebuilt as forward / up from the camera, right = forward x up; fallback fixed too |
| LungeMacro | enchantment lookup used iterator symbols that were never assigned plus a cached `iterator()` jmethodID from one concrete Set class (invalid for other classes = JVM crash class); the attack went through `attackEntity`, not the spear's piercing jab | Lunge is now detected via `ItemStack.getEnchantments().toString()` (no iterator/holder JNI); the jab uses `MinecraftClient.doAttack()` (vanilla left click -> `attackWithPiercingWeapon`); DefaultedList helpers now clear pending Java exceptions |
| Auto Bucket Drain | looked for `water_bucket` (needs the empty `bucket`), used the crosshair (which ignores fluids) and `interactBlock` instead of Use Item | fluid-aware `Entity.raycast(range, delta, true)`, source check (`level=0` / waterlogged / kelp...), pick-up through vanilla `interactItem` |
| Auto Shield Break | required full attack charge *after* swapping (the swap resets it, so it could never fire) and used feet distance | charge check dropped after swap, range measured eye -> hitbox like vanilla, entity interaction range from the player |

## Behaviour notes

- Vanilla restrictions respected by all three new modules: no menu/inventory screen open, nothing while
  eating / using an item / holding your own shield up, hotbar-only axe / bucket / cobweb, vanilla right-click
  and attack delays (`itemUseCooldown`, `attackCooldown`), vanilla slot-sync packets only.
- AutoAnchor: an anchor that already has charge skips the glowstone step; hand swings on every successful use.
- Pressure Plate Web places against the plate's top face so the cobweb always lands above it.
- UI: expand chevrons are now drawn arrows; option rows were already vertical.
- CMake excludes the legacy `agent/modules/{macros,client,combat,movement,player,render,registry_builtins}.cpp`
  (not part of the live build; `macros.cpp` no longer compiles).

## Please check in-game / in `tyrant_agent.log`

- The log prints `resolve complete: N failed lookups (must be 0)` — send it if N is not 0.
- Lunge: the first spear scan logs `enchantment sample (for Lunge detection): ...` — send it if Lunge is not found.
- Not verified without the game: that the jab triggers Lunge when swapped-to and fired in one frame
  (spears may enforce a minimum attack charge), and ESP sway while walking with View Bobbing on
  (bobbing is applied by the game to the projection and is not reproduced).
