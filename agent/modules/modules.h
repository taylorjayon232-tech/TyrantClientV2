// Small module registry used by the in-game category pages and overlay renderers.
#pragma once

#include <imgui.h>
#include <cstdint>
#include <string>
#include <vector>

namespace tyr { namespace modules {

enum class Category { Combat, Mace, Misc, Movement, Spear, Visual, Crystal, Macros };
enum class EspMode { TwoD, ThreeD };
enum class ShieldBreakMode { Auto, OnHit };
// "Web Look" / "Fire placement type": how a module turns the view onto the block it
// places on. Instant snaps the view there in one frame; Smooth walks it there the way
// AimAssist does, so the camera is not yanked.
enum class WebLookMode { Instant, Smooth };
// Auto Totem "Type": Vanilla keeps the off hand stocked with a totem as it is today;
// SMP only holds the totem while the health is at or below a threshold (a shield lives
// in the off hand the rest of the time).
enum class TotemType { Vanilla, SMP };
// Shader ESP: Minecraft's own glowing outline (exact, around the rendered model)
// or Passive's drawn glow, which is the only one that can carry a colour.
enum class ShaderGlowStyle { Vanilla, Custom };

// AimAssist "Accuracy": where the rotation settles on the target.
enum class AimAccuracy { Center, Head, Loose };

// HUD "Crosshair" preset. CustomDraw paints the user's own 9x9 Draw Board instead.
enum class CrosshairStyle { Cross, Dot, CrossDot, Circle, CircleDot, TShape, Chevron, Arrows, CustomDraw };

// Triggerbot "Criticals": how the module treats critical hits.
//  Default  — no crit logic; every valid swing goes out, with 0 stalls.
//  PrioCrit — hold the swing while a critical hit is on offer (a fall, or the short airborne
//             window a knockback gives) and only fall back to a plain hit once it has waited
//             past Prio Crit Wait.
//  OnlyCrit — only ever swing while the hit would land critical.
enum class CritMode { Default, PrioCrit, OnlyCrit };
// Auto Drain "Mode": collect the water source with a bucket, or bury it in a cobweb.
enum class DrainMode { Bucket, Web };

// Auto Pot "Pot Slot Type": how the checked Pot Slot hotbar slots are used.
//  EachDifferent - every checked slot holds one different potion; a slot is refilled with
//                  whichever potion is missing once the one it held has been used.
//  SinglePot     - one potion only: the same potion is kept in the checked slots instead of
//                  a different effect in each.
//  Custom        - every potion names its own slots in the Pot Slot dropdowns.
enum class PotSlotType { EachDifferent, SinglePot, Custom };
// The potions Auto Pot carries, in the order the Custom slot dropdowns are drawn.
enum class PotKind { Strength, Speed, FireResistance, Weaving, Regeneration, Health, Count };
constexpr int kPotKindCount = (int)PotKind::Count;

// The state a module is in, shown as a chip on its card. Safe: very safe on servers,
// unlikely to be detected. Experimental: a beta feature that will still change.
// Unsafe: can lead to a ban on most anticheats. Blatant: very obvious, detectable,
// and can lead to a manual ban.
enum class Tag { Safe, Experimental, Unsafe, Blatant };

struct Entry {
    const char* name;
    const char* description;
    Category category;
    bool* enabled;
    int* keybind = nullptr;
    bool macro = false;
    Tag tag = Tag::Safe;
};

struct EspSettings {
    EspMode mode = EspMode::TwoD;
    bool onlyOutline = false;
    bool allowEntities = true;
    bool allowPlayers = true;
    bool highlightFriends = true;                 // draw friends in their own colour
    ImVec4 color{0.94f, 0.38f, 0.30f, 1.f};
    ImVec4 friendColor{0.36f, 0.76f, 1.f, 1.f};
    bool showHealth3d = true;                     // 3D ESP health indicator
};


// Triggerbot V2: a second Combat card beside Triggerbot with the same switches plus one the
// original does not have. OnHit Allowance hands Auto Shield Break's OnHit mode the left click it
// is waiting for whenever this card swings, so the two work together instead of the shield break
// needing a physical click on top of the triggerbot's own attack.

struct AutoAnchorSettings {
    int totemSlot = 1; // one-based hotbar slot
    int glowstoneDelayMs = 100;
    int totemDelayMs = 100;
};


struct AutoShieldBreakSettings {
    bool onlyWhenLooking = true;
    ShieldBreakMode mode = ShieldBreakMode::Auto;
    bool swapBack = true;
    bool swordOnly = false;
    int swapDelayMs = 70;
    int retryDelayMs = 300;
};

// "Auto Drain" — bucket draining (original behaviour) or web draining
// (place a cobweb on the water source instead of picking it up).
struct AutoBucketDrainSettings {
    DrainMode mode = DrainMode::Bucket;
    // ---- bucket draining ----
    bool ignoreOwnWater = true;
    bool swapBack = true;
    int swapDelayMs = 70;
    bool silentSwap = false;
    bool useOffhand = true;        // drain with an empty bucket held in the off hand
    // ---- web draining ----
    bool webPreferBuckets = true; // no cobweb in the hotbar -> fall back to bucket draining
    bool webIgnoreOwnWater = true; // do not bury water the player just placed
    bool webSwapBack = true;
    bool webUseOffhand = true;     // cobweb held in the off hand counts too
    int  webSwapDelayMs = 70;
    bool webSilentSwap = true;
    // Silent Place: the placement's rotation is put on the player and sent to the server, then the
    // camera is put straight back. The crosshair still has to be on the top face - the client-side
    // hit is what the placement is built from.
    bool webSilentPlace = false;
};

struct AutoPressurePlateWebSettings {
    bool swapBack = true;
    int swapDelayMs = 0;           // 0 places in the same frame the plate is seen (high ping safe)
    bool silentSwap = false;
    bool useOffhand = true;        // place with a cobweb held in the off hand
    // ---- Pressure Plate Assist ----
    // Aim assistance aimed at pressure plates instead of players: a plate inside the cone and the
    // range pulls the view onto it the way AimAssist pulls onto a target, so the web (and the trap)
    // lands on the plate the player meant instead of whatever the crosshair drifted onto.
    bool  plateAssist = false;
    float assistRange = 4.5f;      // 1-6 blocks (eye -> plate centre)
    float assistFov = 120.f;       // 10-360 degrees, total cone
    float assistSpeed = 8.f;       // 1-30, higher = snappier
    bool  assistVertical = true;
    // Only If NO Web: leave a plate alone while a cobweb already sits in the cell above it (the web
    // this module (or the player) placed there is still catching them, and placing another is noise).
    bool onlyIfNoWeb = false;
};

struct FastPlaceSettings {
    bool onlyXp = false;
    bool allowBlocks = true;
    bool allowItems = true;
};

struct AutoJumpResetSettings {
    int accuracy = 100;   // percent chance a hit triggers the jump
};


// ---- Motion Blur (Visual) ------------------------------------------------------------------
// A short trail behind everything the camera does: turning fast smears the frame the way a real
// head-turn blurs what you see, and standing still costs nothing. The smear is one GPU pass per
// frame over the previous frame's capture, so it does not touch the frame rate.
// Natural Motion Blur's feature set, implemented natively (a C++ client cannot host a Fabric mod
// inside its own overlay, so the mod's look is reproduced here instead of bundled).
enum class BlurMethod { Velocity, FrameBlending, Hybrid };
enum class BlurProfile { Uniform, Weighted };
struct MotionBlurSettings {
    bool  enabled = true;         // the module's own switch sits on the card, this is the effect gate
    BlurMethod method = BlurMethod::Velocity;   // Velocity Based / Frame Blending / Hybrid
    float strength = 1.0f;        // 0.1 - 3: overall how visible the effect is (the mod's default 1)
    bool  refreshScaling = true;  // scale by the frame rate, so 60 and 240 fps smear the same
    int   accumulationMax = 3;    // 1 - 8: how many earlier frames the trail folds in
    float accumulationMix = 0.6f; // 0 - 1: how strongly the accumulated frames are mixed back
    BlurProfile profile = BlurProfile::Weighted;  // Weighted / Uniform frame weights
    float sensitivity = 1.0f;     // 0.2 - 3: how strongly turning feeds the blur
};

// ---- Auto Shield (Combat, experimental) ----------------------------------------------------
// Works with Triggerbot V2 only. A swing from that card makes the off-hand shield come up: after
// Shield Delay the use key is held (the vanilla "Use Item / Place Block" press raises a shield held
// in the off hand), for a random time between the minimum and maximum hold, and then released
// again. The player must be holding a weapon for it to fire, so the shield never interrupts a
// shovel, a potion or an empty hand.

// Triggerbot (Combat): the port of Argon's Trigger Bot. While the game's own crosshair is on an
// acceptable entity it swings at it on a randomised delay for the held weapon. The module only
// decides when to click - reach, the attack cooldown and shield handling stay the vanilla game's
// decision, because the swing is the game's own attack. Every swing is announced to the rest of the
// client too, so Auto Shield Break's OnHit mode reacts to it exactly as it reacts to a real click.
struct TriggerbotSettings {
    bool allItems = false;         // any held item is paced by the sword delay
    int swordDelayMinMs = 540;     // random delay window for swords
    int swordDelayMaxMs = 550;
    int axeDelayMinMs = 780;       // random delay window for axes
    int axeDelayMaxMs = 800;
    bool checkShield = false;      // skip a target whose raised shield faces us
    bool clickSimulation = false;  // post the click to the game window instead of attacking directly
    bool strayBypass = false;      // treat zombies as targets too
    bool allEntities = false;      // hit any living entity under the crosshair
    bool samePlayer = false;       // stay on the target the last swing went to
    // Criticals: Default swings on the delay whatever the state of the player; PrioCrit waits for a
    // critical hit while one is on offer and takes a plain hit if it never comes; OnlyCrit never
    // swings without one.
    CritMode criticals = CritMode::Default;
};

// AutoMace (Combat): crosshair on a player within reach, swap the elytra for a carried chestplate,
// let the mace take the swing on a random delay and put the elytra back afterwards. StunSlam opens
// the same swing with the hotbar axe - the raised shield comes down, the mace goes in and the smash
// lands on the exposed hit.
struct AutoMaceSettings {
    bool stunSlam = false;         // axe first, then the mace, against a raised shield
    bool elytraSwap = true;        // elytra off for a carried chestplate before the smash
    bool swapBack = true;          // original hotbar slot back after the sequence
    float range = 3.f;             // blocks, as in Argon's REACH
    // Minimum Fall Height: a smash only arms while the player has fallen at least this many blocks,
    // because the mace's bonus damage is a function of the fall. 0 allows a smash from a standstill.
    float minFallHeight = 2.5f;    // blocks (0-20)
    int hitDelayMinTicks = 0;      // random delay before the swing
    int hitDelayMaxTicks = 2;
    int cooldownMinTicks = 3;      // pause between sequences
    int cooldownMaxTicks = 6;
    int reswapMinTicks = 2;        // pause before the elytra goes back on
    int reswapMaxTicks = 4;
    int axeTicks = 2;              // StunSlam: spacer between the axe and the mace
    int maceTicks = 2;             // ticks between selecting the mace and the smash
    int swapTicks = 1;             // ticks a hotbar / chestplate swap is given
};

struct AimAssistSettings {
    bool allowPlayers = true;
    bool allowEntities = false;
    float range = 4.5f;            // blocks (eye -> hitbox)
    float fov = 90.f;              // degrees, total cone
    float speed = 8.f;             // smoothing speed (higher = snappier)
    bool vertical = true;
    bool onlyWhileAttacking = true;
    bool weaponOnly = false;       // Weapon Only: track only while a weapon is held in the main hand
    bool silent = false;           // Silent Aim: turn the player's view on the server only; camera never moves
    AimAccuracy accuracy = AimAccuracy::Center;  // Center / Head / Loose
    bool noGlide = false;          // do nothing while an elytra is gliding
};

// ---- Round 5 (experimental) modules ---------------------------------------

// Misc: stop the entity-targeting modules from acting on bots / NPCs. The verdict comes from the
// shared player-detection service (agent/modules/player_detect.h), so every module agrees on it.
struct AntiBotSettings {
    bool checkNames = true;        // the cleaned display name carries a bot/NPC marker
    bool checkUuid = true;         // the UUID is not one a real account is given
    bool allowOfflineUuid = true;  // v3 (offline-mode) UUIDs are real players, not bots
    bool checkClones = true;       // a second entity wearing a name another UUID owns
};

// Visual: billboard above players showing gear, held item, health and distance.
struct NametagsSettings {
    bool showName = true;
    bool showHealth = true;
    bool showDistance = true;
    bool showArmor = true;
    bool showHeldItem = true;
    bool icons = true;             // draw real item/armor textures; off = text labels
    // Single Show: only the player or entity the crosshair is actually on gets a board, so a
    // crowded fight shows one nametag instead of a wall of them.
    bool singleShow = false;
    float maxDistance = 48.f;      // blocks
    float scale = 1.f;             // user size multiplier
};

// Visual: Passive's own HUD in place of (or on top of) the game's. A rounded, animated hotbar,
// separated health/armour and food/XP panels, a held-item name plaque, an attack speed bar, and
// a custom crosshair chosen from presets or painted on a Draw Board.
struct HudSettings {
    // ---- what the HUD draws ----
    bool customHotbar = true;
    bool hotbarAnimations = true;     // fade the bar in and pop the selected slot
    bool hotbarRounded = true;
    bool hotbarNumbers = true;        // slot numbers 1..9
    bool hotbarItemCount = true;      // stack counts on the slots
    bool itemNamePopup = true;        // held-item plaque above the hotbar
    bool panels = true;               // health/armour + food/XP panels
    bool attackSpeedBar = true;       // shown while a swing is charging
    bool customCrosshair = false;
    // The game hides its whole HUD as one switch: GameOptions.hudHidden takes its own hotbar and
    // bars, but it takes the first-person hand, the chat, the Tab list and the scoreboard with
    // them. This client therefore never touches that flag. Cover Vanilla draws an opaque plate
    // over the game's own hotbar and bar rows instead, so those are hidden while everything else
    // the game draws (hand, chat, Tab, scoreboard, boss bar) is left exactly as it is.
    bool   coverVanilla = true;
    ImVec4 plateColor{0.06f, 0.05f, 0.08f, 1.f};
    float  platePadding = 5.f;        // 0 - 24 px of plate around the drawn HUD
    // How solid that plate is. It defaults to fully opaque: the game's own hotbar is drawn
    // underneath it, and anything below 100% let the vanilla bar show through the drawn one.
    float  plateOpacity = 1.f;        // 0.3 - 1

    // ---- bar animation ----
    bool   barAnimations = true;      // bars slide to a new value instead of snapping to it
    float  barAnimSpeed = 9.f;        // 3 - 30
    bool   heartBeat = true;          // pulse the heart glyph at or below two hearts
    bool   absorbHealth = true;       // absorption: yellow health bar, max = health + absorption

    // ---- hotbar look ----
    ImVec4 hotbarBackground{0.09f, 0.07f, 0.11f, 0.78f};
    ImVec4 slotBackground{0.16f, 0.13f, 0.19f, 0.70f};
    ImVec4 hotbarAccent{0.94f, 0.38f, 0.30f, 1.f};
    float  hotbarScale = 1.f;         // 0.6 - 1.6
    float  hotbarRounding = 8.f;      // 0 - 16
    float  hotbarAnimSpeed = 14.f;    // 4 - 30

    // ---- bar panels ----
    ImVec4 panelBackground{0.09f, 0.07f, 0.11f, 0.74f};
    ImVec4 healthColor{0.84f, 0.19f, 0.24f, 1.f};
    ImVec4 absorbColor{0.95f, 0.83f, 0.25f, 1.f};   // yellow, while absorption is being held
    ImVec4 armorColor{0.80f, 0.85f, 0.92f, 1.f};
    ImVec4 foodColor{0.86f, 0.55f, 0.18f, 1.f};
    ImVec4 xpColor{0.58f, 0.90f, 0.36f, 1.f};
    float  panelScale = 1.f;          // 0.6 - 1.6
    float  panelRounding = 8.f;       // 0 - 16
    bool   panelIcons = true;         // heart / shield / drumstick / orb glyphs
    bool   panelNumbers = true;       // numeric values beside each bar
    int    panelOffsetY = 6;          // 0 - 120 px above the hotbar

    // ---- held item plaque ----
    ImVec4 itemNameBackground{0.07f, 0.06f, 0.09f, 0.80f};
    ImVec4 itemNameText{0.98f, 0.95f, 0.93f, 1.f};
    float  itemNameScale = 1.f;       // 0.6 - 1.6
    float  itemNameHoldMs = 1600.f;   // 300 - 5000 ms the plaque stays up
    bool   itemNameIcons = true;

    // ---- attack speed bar ----
    ImVec4 attackSpeedColor{0.94f, 0.38f, 0.30f, 1.f};
    float  attackSpeedScale = 1.f;    // 0.6 - 1.6

    // ---- crosshair ----
    CrosshairStyle crosshairStyle = CrosshairStyle::CrossDot;
    ImVec4 crosshairColor{1.f, 1.f, 1.f, 1.f};
    bool   crosshairOutline = true;   // dark backing so the cross reads on any background
    bool   crosshairDynamic = false;  // opens up while a swing is charging
    float  crosshairSize = 1.f;       // 0.4 - 3
    float  crosshairGap = 2.f;        // 0 - 12 px
    float  crosshairThickness = 1.5f; // 0.5 - 5 px
    float  crosshairDot = 1.5f;       // 0.5 - 6 px
    float  crosshairRotation = 0.f;   // -180 - 180 degrees
    // Draw board: a 9x9 grid the user paints (row-major, 1 = a pixel at that cell). The
    // Crosshair style must be CustomDraw for it to be drawn.
    bool   drawBoardPixels[81] = {};
    float  drawBoardPixel = 2.f;      // 1 - 6 px per painted cell
    // Crosshair indicator: while the crosshair ray is on a living entity inside the range, the
    // crosshair is drawn in the indicator colour instead (friends get their own colour), so a
    // target is unmistakable without changing what the game aims at.
    bool   crosshairIndicator = false;
    ImVec4 crosshairIndicatorColor{1.f, 0.30f, 0.30f, 1.f};
    float  crosshairIndicatorRange = 12.f;   // 1 - 64 blocks (eye -> hitbox)
};

// Misc: hit a player or mob that is standing inside a cobweb — only while the
// player is attacking themselves (Ghost Hand is an assist, never an auto-hit).

// Visual: allow a field of view above the vanilla slider's maximum.
struct StaticFovSettings {
    float fov = 130.f;             // 30..179
};

// Crystal: refill the off hand (and optionally a hotbar slot) with a totem.
struct AutoTotemSettings {
    TotemType type = TotemType::Vanilla;   // Vanilla / Crystal (always stocked) or SMP (health threshold)
    bool refillOffhand = true;
    bool refillHotbarSlot = false;
    int  hotbarSlot = 1;           // one-based hotbar slot used by "Totem Refill slot"
    int  delayMs = 80;
    bool silent = true;            // silent: never shows the inventory screen
    // ---- SMP type only ----
    float smpHealthHearts = 3.f;   // 0.5-10 hearts: hold the totem at or below this health
    int   smpSwapDelayMs = 150;    // 0-600 ms between the totem and the shield swaps
    int   smpShieldSlot = 1;       // one-based hotbar slot the shield is kept in
};

// Misc: drop junk from the inventory (optionally without opening the inventory).
struct AutoCleanSettings {
    bool silentClean = true;       // drop straight from the inventory, no screen
    int  scanDelayMs = 200;        // 0-600 ms between scans
    std::vector<std::string> customItems;  // extra item ids (path form) to drop
};

// ---- Round 6 modules -------------------------------------------------------

// Macros: swap to a cobweb and place it on the block under the crosshair.

// Misc: keep chosen hotbar slots stocked with a TNT minecart.
struct CartRefillSettings {
    bool slots[9] = { true, false, false, false, false, false, false, false, false };
    int  delayMs = 150;            // 0-600 ms between refills
    bool silent = true;            // refill without opening the inventory
};

// Mace: throw an ender pearl, then a wind charge (tap the bind to run it).
// Mace: Auto Windcharge. A toggle (not a bind): every pearl seen leaving the player is followed in
// real time and a wind charge is thrown onto the first tick its own flight passes through the
// pearl, so the pearl is boosted onto the high ground instead of dropping back down.

// Mace: Pearl Catch. Tap the bind to throw an ender pearl, then a wind charge, both from the
// hotbar. The charge is what "catches" the pearl: it pops the pearl's flight up so it lands where
// the throw was aimed instead of dropping short.
struct PearlCatchSettings {
    int  pearlDelayMs = 100;        // 0-600 ms after the bind before the pearl is thrown
    int  windChargeDelayMs = 120;   // 0-600 ms after the pearl before the wind charge goes out
    bool swapBack = true;           // return to the slot held before the bind
};

// Macros: fire a partially charged bow, then place a rail and a TNT minecart on the block under
// the crosshair. Both placements require a block under the crosshair (never a bare cell).
struct InstantCartMacroSettings {
    int  bowChargeMs = 200;        // 50-900 ms the bow is drawn before it is released (never a full draw)
    int  railDelayMs = 100;        // 0-600 ms before the rail is placed
    int  cartDelayMs = 110;        // 0-600 ms before the minecart is placed
    bool swapBack = true;          // return to the original slot when the sequence ends
};

// Spear: use the held spear at whatever is in front of it, so its poke lands without the player
// right-clicking themselves.

// Spear: the spear's damage scales with the user's velocity, so on the frame the spear is used
// the module writes a large client-side velocity to the player. Silent leaves the server movement
// alone by restoring the real velocity the same frame.

// Combat: answer a target's ender pearl with one of our own, aimed at its landing spot.
struct PearlAgroSettings {
    int  agroDelayMs = 150;        // 0-600 ms of warning before we pearl after them
    // FOV: the last-hit player has to be inside this cone in front of the view before the pearl is
    // thrown, so agro only answers somebody the player is actually facing (10-360 degrees).
    float fov = 180.f;
    bool silentAim = true;         // Silent Aim: the throw's rotation goes to the server only; camera never moves
    // Smooth turns the camera onto the solved throw instead of leaving it where it was. The throw
    // itself is still the instant one (the rotation the server is given is solved before the pearl
    // leaves), so Smooth is only about what the player sees, never about the pearl arriving late.
    WebLookMode look = WebLookMode::Instant;   // Instant / Smooth
    float cameraSensitivity = 10.f;            // 0-50; how fast a Smooth turn walks the view
};

// Macros: equip the elytra in the hotbar, and equip the chestplate again on the next press.
// Pressing the bind uses the vanilla "use item" path on the elytra, which is exactly how the game
// equips it from the hotbar - the chestplate it replaces goes back into that slot, so the second
// press puts the chestplate on again with the same code path.

// Mace: the revamped AutoMace. A falling target, one smash - with shield stun slams, Density /
// Breach selection, elytra handling and a click gate of its own.
enum class StunSlamType { Custom, MMC };

// Combat (blatant): a 180 on the hit. The view is turned by Horizontal Turn for one game tick and
// then put straight back, so the swing carries the turn without the camera staying on it.

// Misc: automatically use buff potions when their effect is missing.

// Mace: swap to a (breach) mace for a critical hit, so a sword's cooldown deals mace damage.

// Macros: rail -> TNT minecart -> flint & steel -> crossbow shot through the fire.

// ---- Round 8 modules -------------------------------------------------------

// Visual: Minecraft's own glowing outline around every player and every dropped
// item, or Passive's drawn glow with its own colour, thickness and opacity.
struct ShaderEspSettings {
    ShaderGlowStyle style = ShaderGlowStyle::Vanilla;
    // ---- Vanilla: the game outlines the model itself (skin, armour, held item,
    //      dropped item model). White, at Minecraft's own width.
    bool handGlow = true;         // the item in your own first-person hand
    int  handSize = 25;           // 5-60, the glow's width as a % of one block at the hand
    int  handOffsetX = 0;         // -30..30, nudge as a % of the screen height
    int  handOffsetY = 0;
    int  handRotation = 25;       // -90..90 degrees, how the item sits in hand
    // ---- Custom: Passive draws it, so colour, thickness and opacity apply.
    ImVec4 color{0.94f, 0.38f, 0.30f, 1.f};
    ImVec4 friendColor{0.36f, 0.76f, 1.f, 1.f};
    int   thickness = 3;          // 1-5 halo passes under the rim
    float opacity = 0.85f;        // 0-1 (the card shows it as 0-100%)
    float maxDistance = 64.f;     // blocks
};

// Combat (experimental): a follow-up to Auto Shield Break. Once the shield stun lands, a
// cobweb is placed in the cell under the target so the upward motion the stun gives them is
// turned into a web catch. Web Look chooses how the view is turned onto that cell.
struct StunWebSettings {
    WebLookMode look = WebLookMode::Instant;   // Instant / Smooth
    float cameraSensitivity = 5.f;             // 0-50; how fast a Smooth look walks the camera down to the cell
    bool  silentPlace = false;                 // Silent Place: the web goes down server-side; the camera never moves
    bool  lookBack = false;                    // Look Back: once the web is down, turn back where the view was pre-stun
    bool  allowPlayers = true;                 // only stun-web other players
    int   placeDelayMs = 0;                    // 0-600 ms between the shield stun and the web placement
};

// Combat: the experiment that follows Stun Web. AutoWeb carries the whole shield-stun web
// sequence in a Stun Web section of its own, and adds a head web on top of it: when you hit a
// player standing right against a solid block, a cobweb goes into the cell against that block and
// the cell one above it, so their head is webbed. The placement itself uses the Stun Web
// section's Web Look, Camera Sensitivity and Silent Place.

// Visual: remember where a player was standing when they left the world and draw a box around
// the spot. The box is drawn in the chosen colour (black by default).
struct LogoutSpotsSettings {
    ImVec4 color{0.f, 0.f, 0.f, 1.f};          // logout colour
    float maxDistance = 128.f;                 // blocks
    bool onlyPlayers = true;
};

// Misc: attack an entity the crosshair ray reaches from a range vanilla never allows.
// Every swing rolls a fresh reach between 3 blocks and Max Reach, so the packet range
// never sits on one number. The server still decides what it accepts.

// Misc: hold attack and the click rate accelerates from Minimum CPS to Maximum CPS.
struct DragClickSettings {
    int   minCps = 8;             // 1-40
    int   maxCps = 20;            // 1-40
    int   acceleration = 5;       // 1 (slowest ramp) - 10 (fastest)
    float pauseSeconds = 0.f;     // 0-3 s of no clicking, then the ramp starts again
};

// ---- Clarity cosmetics --------------------------------------------------------------------
// These five Visual modules have no native renderer. They drive the optional Clarity Java mod,
// which is loaded at boot from clarity-1.2.0.jar next to the client (see jni::injectClarityJar).
// With the jar missing the cards still work and still save; they simply have nothing to draw.
enum class WingStyle { Angel, Dragon, Demon, Butterfly, Crystal, Feathered };
enum class HeadwearStyle { FlameCrown, FloatingCrown, FoxEars, IceCrown, LaurelWreath, OrbitingOrbs, ReindeerAntlers, RoyalCrown, RunicHalo, SpikedCrown, StagAntlers, ThornHalo, Tiara, UnicornHorn, VikingHorns };
enum class CapeStyle { ClarityAurora, ClarityCircuit, ClarityFlow, ClarityNebula, ClarityPrism, Anniversary15, CherryBlossom, Cobalt, Migrator, Minecon2011, Minecon2016, Mojang, Prismarine, PurpleHeart, Twisted };

struct PlayerModelSettings {
    float scale = 1.0f;          // 0.5-2
    bool  throughWalls = false;
    bool  showTiming = false;
};

struct WingsSettings {
    WingStyle style = WingStyle::Angel;
    float scale = 1.0f;          // 0.5-2
    float opacity = 0.85f;       // 0.1-1
    bool  animate = true;
    float flapSpeed = 1.0f;      // 0.5-3
    float yOffset = 0.0f;        // -1-1
    bool  throughWalls = false;
};

struct HeadwearSettings {
    HeadwearStyle style = HeadwearStyle::RoyalCrown;
    float scale = 1.0f;          // 0.5-2
    float yOffset = 0.0f;        // -0.5-0.5
    float rotationSpeed = 1.0f;  // 0.5-3
    bool  throughWalls = false;
};

struct CapesSettings {
    CapeStyle style = CapeStyle::ClarityAurora;
    bool physics = true;
    bool movementWave = true;
};

// First-person item and hand positioning. Values are offsets, not absolutes: 0 is vanilla.
struct ViewModelSettings {
    float posX = 0.0f;   // -1.5-1.5
    float posY = 0.0f;
    float posZ = 0.0f;
    float rotX = 0.0f;   // -180-180 degrees
    float rotY = 0.0f;
    float rotZ = 0.0f;
    float scale = 1.0f;  // 0.1-2
    bool  offHand = true; // draw the off-hand item too (the left side of the view)
    float opacity = 1.0f; // 0.1-1, how solid the drawn view model is
};

// ---- Walk Trail (Visual) ------------------------------------------------------------------
// A trail of the ground the player has walked over: one grey line along the path, fading out
// behind them. The points are sampled from the player's own feet, in the world, and projected
// onto the screen every frame, so it follows the terrain the player actually crossed.
struct WalkTrailSettings {
    ImVec4 color{0.62f, 0.63f, 0.68f, 1.f};   // grey
    float lifetime = 3.0f;    // 0.5-10 s a point takes to disappear
    float width = 2.5f;       // 1-8 px line width
    int   maxPoints = 240;    // 20-600 points kept (the length of the trail)
    float fade = 1.0f;        // 0-1 how much the tail fades out; 0 keeps it solid
    bool  animate = true;     // a soft pulse travels along the trail
    bool  throughWalls = false;
};

const std::vector<Entry>& entries();

// Java module host: swaps the C++ cards named by `cards` for the Java ones and appends them. Called
// once, when the Java descriptors arrive; entries() answers with the merged list from then on.
void registerJavaEntries(const std::vector<Entry>& cards);
EspSettings& espSettings();
AutoAnchorSettings& autoAnchorSettings();
AutoShieldBreakSettings& autoShieldBreakSettings();
AutoBucketDrainSettings& autoBucketDrainSettings();
AutoPressurePlateWebSettings& autoPressurePlateWebSettings();
FastPlaceSettings& fastPlaceSettings();
AutoJumpResetSettings& autoJumpResetSettings();
AimAssistSettings& aimAssistSettings();
TriggerbotSettings& triggerbotSettings();
AutoMaceSettings& autoMaceSettings();
AntiBotSettings& antiBotSettings();
NametagsSettings& nametagsSettings();
HudSettings& hudSettings();
StaticFovSettings& staticFovSettings();
AutoTotemSettings& autoTotemSettings();
StunWebSettings& stunWebSettings();
LogoutSpotsSettings& logoutSpotsSettings();
WalkTrailSettings& walkTrailSettings();
AutoCleanSettings& autoCleanSettings();
MotionBlurSettings& motionBlurSettings();
InstantCartMacroSettings& instantCartMacroSettings();
PearlCatchSettings& pearlCatchSettings();
CartRefillSettings& cartRefillSettings();
PearlAgroSettings& pearlAgroSettings();
ShaderEspSettings& shaderEspSettings();
DragClickSettings& dragClickSettings();
PlayerModelSettings& playerModelSettings();
WingsSettings& wingsSettings();
HeadwearSettings& headwearSettings();
CapesSettings& capesSettings();
ViewModelSettings& viewModelSettings();

// Per-module state the Clarity bridge asks about once per frame.
bool isPlayerModelEnabled();
bool isWingsEnabled();
bool isHeadwearEnabled();
bool isCapesEnabled();
bool isViewModelEnabled();
// True while the natives of the five Clarity cosmetics are on and drawing.
bool isWalkTrailEnabled();

// ---- Debug Mode / Notifications (Settings > General > Interface) ----------
// Debug Mode: modules only raise their action toasts while this is on, and the
// toast carries the green "DEBUG MODE" tag. Notifications: separate switch for
// "Enabled <module>" and "Friend <name> joined" toasts.
bool& debugMode();
bool& notificationsEnabled();
// Module action toast ("Auto Totem: ..."). Quiet unless Debug Mode is on.
void notifyAction(const std::string& text);
// "Enabled X" / "Disabled X" toast. Quiet unless Notifications is on.
void notifyModuleToggled(const std::string& name, bool enabled);

// ---- Friends list ---------------------------------------------------------
// Friends are matched case-insensitively and ignore what plugins paint on a name
// (rank prefixes, colour codes, padding), so an entry typed by hand keeps matching
// the same player. An entry that is itself a UUID matches by identity, which cannot
// be renamed at all. They are saved with the rest of the modules config and used to
// keep the entity modules off them.
bool isFriend(const std::string& name);
std::vector<std::string> friends();
bool addFriend(const std::string& name);
bool removeFriend(const std::string& name);
std::vector<std::string> nearbyPlayerNames();   // players currently in the world

void drawOverlay(float tickDelta);

// WndProc message hook (hooks::setWndProcCallback). It never consumes input: it only notices the
// player's own world-action presses early enough to hand the server the camera rotation before the
// game's next tick sends the block place / break / use packet.
bool onInputMessage(void* hwnd, uint32_t msg, uint64_t wParam, int64_t lParam);

// Body/head rotation writer, driven by the pre-render/draw hooks (see hooks::setPreRenderCallback).
void applySilentBodyPose();
// True while the silent aim (or a one-shot silent action) owns the body pose.
bool silentAimHoldingPose();
// Lean pose write from the watchdog thread: resolves the player itself, so it never touches the
// per-frame cached references the render thread frees.
void silentPoseWatchdogWrite();
void resetDefaults();
std::string serialize();
bool deserialize(const std::string& jsonText);

} }
