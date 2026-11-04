#include "modules.h"
#include "java_modules.h"
#include "player_detect.h"
#include "click_pace.h"
#include "triggerbot_rules.h"
#include "xbow_timing.h"
#include "placement_aim.h"
#include "drain_anchor.h"
#include "head_web.h"
#include "pot_slot_rules.h"
#include "wind_charge.h"
#include "shader_glow.h"
#include "../jni/mc.h"
#include "../jni/silent_rotation.h"
#include "../gui/gui.h"
#include "../gui/theme.h"
#include "../render/itemicons.h"
#include "../render/motionblur.h"
#include "../core/agent.h"
#include "../core/hooks.h"
#include <imgui.h>
#include <json.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <windows.h>

namespace tyr { namespace modules {
namespace {
bool g_arraylist = false;
bool g_esp = false;
bool g_autoAnchor = false;
bool g_autoShieldBreak = false;
bool g_autoBucketDrain = false;
bool g_autoPressurePlateWeb = false;
bool g_fastPlace = false;
bool g_sprint = false;
bool g_autoJumpReset = false;
bool g_aimAssist = false;
int g_arraylistKey = 0;
int g_espKey = 0;
int g_autoAnchorKey = 0;
int g_autoShieldBreakKey = 0;
int g_autoBucketDrainKey = 0;
int g_autoPressurePlateWebKey = 0;
int g_fastPlaceKey = 0;
int g_sprintKey = 0;
int g_autoJumpResetKey = 0;
int g_aimAssistKey = 0;
EspSettings g_espSettings{};
AutoAnchorSettings g_autoAnchorSettings{};
AutoShieldBreakSettings g_autoShieldBreakSettings{};
AutoBucketDrainSettings g_autoBucketDrainSettings{};
AutoPressurePlateWebSettings g_autoPressurePlateWebSettings{};
FastPlaceSettings g_fastPlaceSettings{};
AutoJumpResetSettings g_autoJumpResetSettings{};
AimAssistSettings g_aimAssistSettings{};
bool g_silentAimModule = false;
int g_silentAimModuleKey = 0;
SilentAimSettings g_silentAimModuleSettings{};
bool g_triggerbot = false;
int g_triggerbotKey = 0;
TriggerbotSettings g_triggerbotSettings{};
bool g_autoMace = false;
int g_autoMaceKey = 0;
AutoMaceSettings g_autoMaceSettings{};

// ---- round 5 (experimental) modules ----
bool g_antiBot = false;
bool g_nametags = false;
bool g_staticFov = false;
bool g_autoTotem = false;
bool g_autoClean = false;
int g_antiBotKey = 0;
int g_nametagsKey = 0;
int g_staticFovKey = 0;
int g_autoTotemKey = 0;
int g_autoCleanKey = 0;
AntiBotSettings g_antiBotSettings{};
NametagsSettings g_nametagsSettings{};
StaticFovSettings g_staticFovSettings{};
AutoTotemSettings g_autoTotemSettings{};
AutoCleanSettings g_autoCleanSettings{};

// ---- round 6 modules ----
bool g_trajPreview = false;
bool g_itemScroll = false;
int g_trajPreviewKey = 0;
int g_itemScrollKey = 0;
int g_instantCartKey = 0;
InstantCartMacroSettings g_instantCartSettings{};
int g_pearlCatchKey = 0;
PearlCatchSettings g_pearlCatchSettings{};

// ---- round 7 modules ----
bool g_cartRefill = false;
bool g_pearlAgro = false;
int g_cartRefillKey = 0;
int g_pearlAgroKey = 0;
CartRefillSettings g_cartRefillSettings{};
PearlAgroSettings g_pearlAgroSettings{};

// ---- round 8 modules ----
bool g_shaderEsp = false;
bool g_dragClick = false;
int g_shaderEspKey = 0;
int g_dragClickKey = 0;
ShaderEspSettings g_shaderEspSettings{};
DragClickSettings g_dragClickSettings{};

// ---- round 9 modules ----
bool g_stunWeb = false;
bool g_logoutSpots = false;
int g_stunWebKey = 0;
int g_logoutSpotsKey = 0;
StunWebSettings g_stunWebSettings{};
LogoutSpotsSettings g_logoutSpotsSettings{};

// ---- Triggerbot V2 / AutoWeb ----
// Triggerbot V2 is a Combat card beside Triggerbot with the same switches plus OnHit Allowance,
// and AutoWeb is the Stun Web experiment plus the head web. Each keeps its own settings, so the
// cards they sit beside are untouched.

// ---- round 10 modules ----

// ---- round 12 modules: Motion Blur (Visual) and Auto Shield (Combat) ----
bool g_motionBlur = false;
int g_motionBlurKey = 0;
MotionBlurSettings g_motionBlurSettings{};

// ---- round 11 modules: Spear ----

// ---- Walk Trail (Visual) ----
// One grey line along the ground the player has walked over, fading out behind them.
bool g_walkTrail = false;
int g_walkTrailKey = 0;
WalkTrailSettings g_walkTrailSettings{};

// ---- HUD (Visual) ----
// Passive's own HUD: a rounded, animated hotbar, separated health/armour and food/XP panels, a
// held-item name plaque, an attack speed bar and a custom crosshair. See drawHud() below.
bool g_hud = false;
int g_hudKey = 0;
HudSettings g_hudSettings{};

// ---- Clarity cosmetics ----
// Five Visual modules with no native renderer of their own: each one just asks the Clarity
// Java mod to switch that cosmetic on. See updateClarityCosmetics below.
bool g_playerModel = false;
bool g_wings = false;
bool g_headwear = false;
bool g_capes = false;
bool g_viewModel = false;
int g_playerModelKey = 0;
int g_wingsKey = 0;
int g_headwearKey = 0;
int g_capesKey = 0;
int g_viewModelKey = 0;
PlayerModelSettings g_playerModelSettings{};
WingsSettings g_wingsSettings{};
HeadwearSettings g_headwearSettings{};
CapesSettings g_capesSettings{};
ViewModelSettings g_viewModelSettings{};

// ---- round 13 modules: Elytra (Macros), Auto Mace V2 (Mace), Knockback Displacement (Combat),
// Name Hider (Visual) ----

// Debug Mode / Notifications (Settings > General > Interface).
bool g_debugMode = false;
bool g_notifications = true;

// Friends list: display names, stored exactly as typed (matched case-insensitively).
std::vector<std::string> g_friends;

const std::vector<Entry> g_builtinEntries = {
    {"Arraylist", "Animated list of enabled modules", Category::Visual, &g_arraylist, &g_arraylistKey, false, Tag::Safe},
    {"HUD", "Custom hotbar, health/armour and food/XP panels, item plaque, attack bar and crosshair", Category::Visual, &g_hud, &g_hudKey, false, Tag::Safe},
    {"Auto Drain", "Collect looked-at water sources with a bucket or a cobweb", Category::Misc, &g_autoBucketDrain, &g_autoBucketDrainKey, false, Tag::Unsafe},
    {"AutoAnchor", "Charge a respawn anchor and activate it with a totem", Category::Crystal, &g_autoAnchor, &g_autoAnchorKey, false, Tag::Unsafe},
    {"Auto Pressure Plate Web", "Place a cobweb above a looked-at pressure plate", Category::Misc, &g_autoPressurePlateWeb, &g_autoPressurePlateWebKey, false, Tag::Unsafe},
    {"Auto Shield Break", "Use a hotbar axe against a player holding up a shield", Category::Combat, &g_autoShieldBreak, &g_autoShieldBreakKey, false, Tag::Unsafe},
    {"Triggerbot", "Automatically hits the entity under the crosshair for you", Category::Combat, &g_triggerbot, &g_triggerbotKey, false, Tag::Blatant},
    {"AutoMace", "Swap to the mace for a falling smash, with an optional axe-first StunSlam", Category::Mace, &g_autoMace, &g_autoMaceKey, false, Tag::Unsafe},
    {"ESP", "Show player and living entity bounds through walls", Category::Visual, &g_esp, &g_espKey, false, Tag::Blatant},
    {"Fast Place", "Experimental: place blocks / use throwables faster while holding Use Item", Category::Misc, &g_fastPlace, &g_fastPlaceKey, false, Tag::Unsafe},
    {"Sprint", "Experimental: keep sprinting without holding the sprint key", Category::Movement, &g_sprint, &g_sprintKey, false, Tag::Experimental},
    {"Auto Jump Reset", "Experimental: jump the moment another player damages you", Category::Combat, &g_autoJumpReset, &g_autoJumpResetKey, false, Tag::Experimental},
    {"AimAssist", "Experimental: smoothly rotate the real view onto players or entities", Category::Combat, &g_aimAssist, &g_aimAssistKey, false, Tag::Blatant},
    {"Silent Aim", "Experimental: sweep the silent aim onto players or entities, camera untouched", Category::Combat, &g_silentAimModule, &g_silentAimModuleKey, false, Tag::Experimental},
    {"Anti-Bot", "Experimental: keep the entity modules off bots and NPCs", Category::Misc, &g_antiBot, &g_antiBotKey, false, Tag::Experimental},
    {"Nametags", "Experimental: info board above players (gear, held item, health, distance)", Category::Visual, &g_nametags, &g_nametagsKey, false, Tag::Experimental},
    {"StaticFov", "Experimental: field of view above the vanilla slider limit", Category::Visual, &g_staticFov, &g_staticFovKey, false, Tag::Experimental},
    {"Auto Totem", "Experimental: refill the off hand (and a hotbar slot) with a totem", Category::Crystal, &g_autoTotem, &g_autoTotemKey, false, Tag::Experimental},
    {"AutoClean", "Experimental: drop unnecessary items without opening the inventory", Category::Misc, &g_autoClean, &g_autoCleanKey, false, Tag::Experimental},
    {"Trajectory Preview", "Experimental: draw where the held projectile will land", Category::Visual, &g_trajPreview, &g_trajPreviewKey, false, Tag::Experimental},
    {"Item Scroll", "Experimental: hold Shift to auto-click hovered inventory slots", Category::Misc, &g_itemScroll, &g_itemScrollKey, false, Tag::Experimental},
    {"Instant Cart Macro", "Fire a partial bow, then place a rail and a TNT minecart on the looked-at block", Category::Macros, nullptr, &g_instantCartKey, true, Tag::Unsafe},
    {"Pearl Catch", "Throw an ender pearl, then a wind charge, from the hotbar", Category::Mace, nullptr, &g_pearlCatchKey, true, Tag::Safe},
    {"Cart Refill", "Keep chosen hotbar slots stocked with a TNT minecart", Category::Misc, &g_cartRefill, &g_cartRefillKey, false, Tag::Experimental},
    {"Pearl Agro", "Answer a target's ender pearl with one of your own", Category::Combat, &g_pearlAgro, &g_pearlAgroKey, false, Tag::Unsafe},
    {"Drag Click Friend", "Experimental: hold attack and the clicks accelerate from Minimum to Maximum CPS", Category::Misc, &g_dragClick, &g_dragClickKey, false, Tag::Experimental},
    {"Shader ESP", "Minecraft's own glowing outline around players and dropped items, plus the item in hand", Category::Visual, &g_shaderEsp, &g_shaderEspKey, false, Tag::Blatant},
    {"Stun Web", "Experimental: place a cobweb under a player whose shield Auto Shield Break just stunned", Category::Combat, &g_stunWeb, &g_stunWebKey, false, Tag::Experimental},
    {"Logout Spots", "Mark where players stood when they left the world", Category::Visual, &g_logoutSpots, &g_logoutSpotsKey, false, Tag::Safe},
    {"Walk Trail", "A grey trail follows you as you walk and fades out behind you", Category::Visual, &g_walkTrail, &g_walkTrailKey, false, Tag::Safe},
    {"Player Model", "Clarity: render a custom 3D player model", Category::Visual, &g_playerModel, &g_playerModelKey, false, Tag::Safe},
    {"Wings", "Clarity: animated 3D wings (Angel, Dragon, Demon, Butterfly, Crystal, Feathered)", Category::Visual, &g_wings, &g_wingsKey, false, Tag::Safe},
    {"Headwear", "Clarity: 3D headwear cosmetics (crowns, horns, antlers, halos, ears)", Category::Visual, &g_headwear, &g_headwearKey, false, Tag::Safe},
    {"Capes", "Clarity: a custom cape with its own physics", Category::Visual, &g_capes, &g_capesKey, false, Tag::Safe},
    {"ViewModel", "Clarity: reposition and rescale the first-person item and hand", Category::Visual, &g_viewModel, &g_viewModelKey, false, Tag::Safe},
    {"Motion Blur", "Natural Motion Blur's look: velocity trail, frame blending or hybrid, natively", Category::Visual, &g_motionBlur, &g_motionBlurKey, false, Tag::Safe},
};

// Cards the interface draws. Starts as the built-in list; the Java module host swaps the C++ card
// it ported for a Java one (see java_modules.cpp), so every module has exactly one card.
std::vector<Entry> g_entries = g_builtinEntries;

struct Fade { float value = 0.f; float width = 0.f; };
std::unordered_map<std::string, Fade> g_arraylistFades;
// OnHit Allowance: how long a module swing stays fresh for the follow-ups that answer the player's
// own click. Auto Shield Break consumes such a swing exactly once, so one swing raises the shield
// once, whatever frame it lands on.
constexpr double kOnHitAllowanceWindow = 0.15;
uint64_t g_shieldConsumedSwingSeq = 0;
double g_lungeLastAt = 0.0;
double g_shieldNextAt = 0.0;
double g_bucketNextAt = 0.0;
double g_webNextAt = 0.0;
std::string g_anchorLastNotice;
double g_anchorNoticeAt = 0.0;

enum class AnchorPhase { Idle, WaitItems, WaitGlowstone, WaitTotem, Done };
AnchorPhase g_anchorPhase = AnchorPhase::Idle;
bool g_anchorTargetValid = false;
bool g_anchorSlotChanged = false;
int g_anchorX = 0, g_anchorY = 0, g_anchorZ = 0;
int g_anchorOriginalSlot = -1;
int g_anchorGlowstoneSlot = -1;
double g_anchorStageAt = 0.0;

struct BlockTarget { int x=0,y=0,z=0; bool valid=false; };
BlockTarget g_bucketTarget{}, g_webTarget{};
int g_bucketSlot=-1, g_bucketOriginalSlot=-1;
int g_webSlot=-1, g_webOriginalSlot=-1;
double g_bucketStageAt=0.0, g_webStageAt=0.0, g_webSwapAt=-1.0;
bool g_bucketWaiting=false, g_webWaiting=false;
std::unordered_set<std::string> g_ownWaterPositions;
std::string g_shieldTargetUuid;
int g_shieldOriginalSlot=-1, g_shieldAxeSlot=-1;
double g_shieldStageAt=0.0;
bool g_shieldWaiting=false;
bool g_attackSentThisFrame=false;
// A swing any module sent, announced so the follow-ups that answer the player's own click (Auto
// Shield Break's OnHit mode today, Auto Shield's swing trigger before it) react to it too. The
// sequence makes one swing raise one shield, whatever frame the follow-up reads it on.
uint64_t g_moduleSwingSeq = 0;
double g_moduleSwingAt = -1000.0;
void noteModuleSwing(double now) {
    g_attackSentThisFrame = true;
    g_moduleSwingAt = now;
    ++g_moduleSwingSeq;
}

std::string blockKey(int x,int y,int z) {
    return std::to_string(x)+","+std::to_string(y)+","+std::to_string(z);
}

// The depth every player-facing module reads the world's entity list with. `ClientWorld.getEntities()`
// hands the list back in hash order, and that order shifts as entities come and go - so with a cap of
// 256 in a world holding more than that, the SAME player could be inside the window on one frame and
// outside it on the next. That is the reported "it finds the players, then loses them again": NewLocalRef
// capacity is reserved for 4096 in the per-frame JNI scope (see agent.cpp), so the scan is deep enough
// that a tracked player stays in it.
constexpr int kEntityScan = 1024;

// A fresh left-button PRESS this frame (the rising edge), never a button that is merely held. Every
// module that must answer the player's own click and never click by itself asks this: Breach Swap's
// swap, Lag Reach's reach, Ghost Hand's assist and AutoMace's arming all hang off it, so one physical
// click is one action and a held button can never turn any of them into an autoclicker.
bool playerAttackTapped() {
    const std::vector<int>& taps = gui::mouseTaps();
    return std::find(taps.begin(), taps.end(), VK_LBUTTON) != taps.end();
}

ImU32 packed(ImVec4 c, float extraAlpha = 1.f) {
    return IM_COL32((int)(std::clamp(c.x, 0.f, 1.f) * 255.f),
                    (int)(std::clamp(c.y, 0.f, 1.f) * 255.f),
                    (int)(std::clamp(c.z, 0.f, 1.f) * 255.f),
                    (int)(std::clamp(c.w * extraAlpha, 0.f, 1.f) * 255.f));
}

// Case-insensitive alphabetical order (A to Z).
bool nameAlphaLess(const std::string& a, const std::string& b) {
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        const unsigned char ca = (unsigned char)std::tolower((unsigned char)a[i]);
        const unsigned char cb = (unsigned char)std::tolower((unsigned char)b[i]);
        if (ca != cb) return ca < cb;
    }
    return a.size() < b.size();
}

// Arraylist order: longest name first, ties broken alphabetically.
bool arraylistLess(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return a.size() > b.size();
    return nameAlphaLess(a, b);
}

std::string lowerCopy(const std::string& value) {
    std::string out = value;
    for (char& c : out) c = (char)std::tolower((unsigned char)c);
    return out;
}

namespace aim = ::tyr::modules::aim;

// ---- view aiming for a placement ------------------------------------------------------------
// Degrees helpers that treat an angle as a position on a circle, so a turn from 179 to -179 is the
// short 2 degree way round and never the long way back.
// Shortest signed turn from `from` to `to`, in (-180, 180]. (The AimAssist smoothing above has its
// own `wrapDeg`; this one is only for the placement aim and never touches it.)
float angleDeltaDeg(float from, float to) {
    float d = std::fmod(to - from + 540.f, 360.f);
    if (d < 0.f) d += 360.f;
    return d - 180.f;
}
float approachDeg(float from, float to, float t) { return from + angleDeltaDeg(from, to) * std::clamp(t, 0.f, 1.f); }

// Turn the player's view onto `wantYaw`/`wantPitch` for a placement. Instant snaps there and returns
// true at once; Smooth walks it there (the way AimAssist does) and returns true only once the view is
// within `tolerance` degrees, so the caller returns and comes back on a later frame while it turns.
// `sensitivity` is the Camera Sensitivity slider (0-10): it is the speed the Smooth walk uses, where 5
// is the pace these placements always had, 0 is deliberately slow and 10 is nearly a snap.
// `silent` ("Silent Place") never moves the camera at all: the rotation is put on the player, sent to
// the server, and put straight back, so the block is placed against the rotation the server holds
// while the view stays exactly where the player left it.
// The look is always sent to the server: a placement is validated against the rotation it holds.
bool aimViewForPlacement(mc::Player& player, float wantYaw, float wantPitch,
                         WebLookMode mode, float tolerance = 1.5f,
                         float sensitivity = 5.f, bool silent = false) {
    if (silent) {
        const float realYaw = player.yaw(), realPitch = player.pitch();
        const float realLastYaw = player.prevYaw(), realLastPitch = player.prevPitch();
        player.setYaw(wantYaw);
        player.setPitch(wantPitch);
        player.setPrevYaw(wantYaw);
        player.setPrevPitch(wantPitch);
        mc::ext::sendLookPacket(wantYaw, wantPitch, player.onGround());
        player.setYaw(realYaw);
        player.setPitch(realPitch);
        player.setPrevYaw(realLastYaw);
        player.setPrevPitch(realLastPitch);
        return true;
    }
    if (mode == WebLookMode::Instant) {
        player.setYaw(wantYaw);
        player.setPitch(wantPitch);
        mc::ext::sendLookPacket(wantYaw, wantPitch, player.onGround());
        return true;
    }
    // Camera Sensitivity 0-10 -> 4..20 per second, with the middle of the slider (5) being the 12/s
    // these placements always walked at, so nothing changes for an untouched config. The rule
    // itself lives in placement_aim.h, where test/placement_aim_check.cpp checks it.
    const float t = aim::approachStep(sensitivity, ImGui::GetIO().DeltaTime);
    const float curYaw = player.yaw();
    const float curPitch = player.pitch();
    const bool onTarget = std::abs(angleDeltaDeg(curYaw, wantYaw)) <= tolerance &&
                          std::abs(wantPitch - curPitch) <= tolerance;
    const float newYaw = onTarget ? wantYaw : approachDeg(curYaw, wantYaw, t);
    const float newPitch = onTarget ? wantPitch : (curPitch + (wantPitch - curPitch) * t);
    player.setYaw(newYaw);
    player.setPitch(newPitch);
    mc::ext::sendLookPacket(newYaw, newPitch, player.onGround());
    return onTarget;
}

// ---- Smooth Look Back -------------------------------------------------------------------------
// "Look Back" is one rule for every module that turns the camera away to do its work (Stun Web,
// the Xbow Macro's fire step, Auto Pot's look-down): the view the player held before the action is
// walked back to at the Camera Sensitivity pace instead of being snapped back in a single frame,
// which was the jerk Look Back was reported as. One state block, so two modules can never run two
// competing walks, and the player's own mouse input always wins: if the view is more than a couple
// of degrees away from where the walk left it, the walk is dropped and the player keeps their aim.
bool  g_lookBackActive = false;
float g_lookBackYaw = 0.f, g_lookBackPitch = 0.f, g_lookBackSensitivity = 5.f;
float g_lookBackAppliedYaw = 0.f, g_lookBackAppliedPitch = 0.f;

void startLookBack(float yaw, float pitch, float sensitivity) {
    g_lookBackYaw = yaw;
    g_lookBackPitch = pitch;
    g_lookBackSensitivity = sensitivity;
    // Seed the guard with the view as it is RIGHT NOW, not with the destination. The guard drops the
    // walk when the view is more than a couple of degrees from the last rotation the walk wrote; a
    // module arms Look Back right after turning the camera onto the cell it placed against, which is
    // many degrees from the destination, so seeding it with the destination made the guard fire on
    // the very first step - the walk was dropped before it moved a single frame, which is the
    // reported "Look Back does not look back".
    g_lookBackAppliedYaw = yaw;
    g_lookBackAppliedPitch = pitch;
    if (jobject self = mc::Client::player()) {
        mc::Player me(self);
        g_lookBackAppliedYaw = me.yaw();
        g_lookBackAppliedPitch = me.pitch();
    }
    g_lookBackActive = true;
}

void cancelLookBack() { g_lookBackActive = false; }

void stepLookBack(mc::Player& player) {
    if (!g_lookBackActive) return;
    const float curYaw = player.yaw(), curPitch = player.pitch();
    if (std::abs(angleDeltaDeg(g_lookBackAppliedYaw, curYaw)) > 2.f ||
        std::abs(curPitch - g_lookBackAppliedPitch) > 2.f) {
        g_lookBackActive = false;      // the player moved the view: their aim wins
        return;
    }
    const float t = aim::approachStep(g_lookBackSensitivity, ImGui::GetIO().DeltaTime);
    const float newYaw = approachDeg(curYaw, g_lookBackYaw, t);
    const float newPitch = curPitch + (g_lookBackPitch - curPitch) * t;
    const bool arrived = std::abs(angleDeltaDeg(newYaw, g_lookBackYaw)) <= 0.4f &&
                         std::abs(newPitch - g_lookBackPitch) <= 0.4f;
    const float yaw = arrived ? g_lookBackYaw : newYaw;
    const float pitch = arrived ? g_lookBackPitch : newPitch;
    player.setYaw(yaw);
    player.setPitch(pitch);
    player.setPrevYaw(yaw);      // the renderer interpolates from these, so both are pinned and
    player.setPrevPitch(pitch);  // the turn back is on screen the frame it happens
    mc::ext::sendLookPacket(yaw, pitch, player.onGround());
    g_lookBackAppliedYaw = yaw;
    g_lookBackAppliedPitch = pitch;
    if (arrived) g_lookBackActive = false;
}

namespace det = ::tyr::modules::detect;
namespace xbow = ::tyr::modules::xbow;

// Defined with the other silent-aim one-shots further down; the drain and Stun Web placements call
// it before that point in the file.
bool silentPlaceBlock(mc::Player& player, int blockX, int blockY, int blockZ, int face,
                      const Vec3& facePoint, bool offhand);

// ---- Player detection service ----------------------------------------------------------------
// The one place every player-facing module asks "what is this entity?". AimAssist, Triggerbot,
// Auto Shield Break, AutoMace, Ghost Hand, ESP, Nametags, Pearl Agro and the friends list all go
// through it, so they agree on who counts as a player and who is a bot or an NPC dressed up as
// one. The rules themselves live in agent/modules/player_detect.h (pure and unit-tested in
// test/detect_check.cpp); this is the JNI side plus the caches around them.
//
// Identity is the UUID, never the name. Plugins rewrite display names - rank prefixes, colour
// codes, padding, and whole NPC copies of real players - but they cannot change an account's
// UUID. So names are cleaned before they are compared or shown, a friend confirmed once is pinned
// to their UUID, and an NPC copy of a name is spotted because it is the less account-like of the
// two entities wearing it.

// What the service decided about one entity, for one frame.
struct TargetInfo {
    bool isPlayer = false;     // PlayerEntity subclass (other players, never the local player)
    bool isFriend = false;     // friends list, by UUID or by cleaned name
    bool isBot = false;        // Anti-Bot says this is a bot / NPC
    bool blocked = false;      // isBot || isFriend - what the targeting modules must honour
    char uuidVersion = 0;
    std::string name;          // cleaned display name
    std::string rawName;       // exactly as the server prints it
    std::string uuid;          // exactly as the game reports it
    std::string uuidKey;       // comparison form (lower case, no dashes)
};

// What an entity last looked like, keyed by uuidKey. Strings only: no JNI reference is ever held
// between frames, so these caches are safe to keep.
struct PlayerIdentity {
    std::string name;
    char version = 0;
    bool accountLike = false;
    bool botMarker = false;
    bool botShape = false;
    double lastSeen = 0.0;
};
std::unordered_map<std::string, PlayerIdentity> g_identities;
// cleaned lower-case name -> uuidKey of the most account-like entity seen wearing it
std::unordered_map<std::string, std::string> g_nameOwners;
// uuidKey -> the friends-list entry that already matched this entity
std::unordered_map<std::string, std::string> g_friendPins;
std::string g_selfUuidKey, g_selfNameLower;
double g_detectionRefreshedAt = -1.0;
constexpr double kIdentityTtlSeconds = 90.0;

bool isFriendEntry(const std::string& entry) {
    return std::find(g_friends.begin(), g_friends.end(), entry) != g_friends.end();
}

// Friends are compared decoration-insensitively, so a name entered by hand ("Notch") still matches
// the same player once a plugin dresses them up ("[VIP] Notch" with a colour code in front). A
// friend entry that is itself a UUID is not a name: those match by identity in friendMatch().
bool isFriendName(const std::string& name) {
    if (name.empty() || g_friends.empty()) return false;
    const std::string lower = lowerCopy(name);
    const std::string clean = det::lowerCopy(det::cleanPlayerName(name));
    for (const std::string& entry : g_friends) {
        if (det::looksLikeUuid(entry)) continue;
        if (lowerCopy(entry) == lower) return true;
        const std::string entryClean = det::lowerCopy(det::cleanPlayerName(entry));
        if (!entryClean.empty() && entryClean == clean) return true;
    }
    return false;
}

// The friends check for a live entity. A UUID entry matches by identity; a name entry matches the
// cleaned name and is then pinned to the UUID, so a rename (rank change, nick plugin) can no longer
// silently turn a friend into a target mid-session.
bool friendMatch(const std::string& entityUuidKey, const std::string& cleanName) {
    if (g_friends.empty()) return false;
    if (!entityUuidKey.empty()) {
        for (const std::string& entry : g_friends)
            if (det::looksLikeUuid(entry) && det::uuidKey(entry) == entityUuidKey) return true;
        const auto pin = g_friendPins.find(entityUuidKey);
        if (pin != g_friendPins.end() && isFriendEntry(pin->second)) return true;
    }
    if (cleanName.empty()) return false;
    const std::string cleanLower = det::lowerCopy(cleanName);
    for (const std::string& entry : g_friends) {
        if (det::looksLikeUuid(entry)) continue;
        const std::string entryClean = det::lowerCopy(det::cleanPlayerName(entry));
        if (!entryClean.empty() && entryClean == cleanLower) {
            if (!entityUuidKey.empty()) g_friendPins[entityUuidKey] = entry;
            return true;
        }
    }
    return false;
}

bool identityAccountLike(const std::string& entityUuidKey) {
    const auto it = g_identities.find(entityUuidKey);
    return it != g_identities.end() && it->second.accountLike;
}

// PlayerEntity subclass test. If the class handle is ever missing, the registry type
// ("minecraft:player") answers instead of the modules quietly finding no players at all.
bool isPlayerEntity(JNIEnv* env, jobject entity) {
    if (!entity) return false;
    if (env && mc::sym().player) {
        const jboolean v = env->IsInstanceOf(entity, mc::sym().player);
        if (!env->ExceptionCheck()) return v == JNI_TRUE;
        env->ExceptionClear();
    }
    return mc::entityTypePath(entity) == "player";
}

// Once per displayed frame: forget identities nobody has worn for a while, and re-read the local
// player (an entity wearing the local player's own name is an NPC imitating them). Everything is
// cleared the moment there is no world, so one server's names can never judge the next one's.
void refreshPlayerDetection() {
    const double now = ImGui::GetTime();
    if (now == g_detectionRefreshedAt) return;
    g_detectionRefreshedAt = now;
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved() || !mc::Client::world()) {
        g_identities.clear();
        g_nameOwners.clear();
        g_friendPins.clear();
        g_selfUuidKey.clear();
        g_selfNameLower.clear();
        return;
    }
    for (auto it = g_identities.begin(); it != g_identities.end();) {
        if (now - it->second.lastSeen > kIdentityTtlSeconds) it = g_identities.erase(it);
        else ++it;
    }
    for (auto it = g_nameOwners.begin(); it != g_nameOwners.end();) {
        if (!g_identities.contains(it->second)) it = g_nameOwners.erase(it);
        else ++it;
    }
    jobject self = mc::Client::player();
    if (!self) return;
    g_selfUuidKey = det::uuidKey(mc::Entity(self).uuid());
    g_selfNameLower = det::lowerCopy(det::cleanPlayerName(mc::Entity(self).displayName()));
    if (!g_selfUuidKey.empty() && !g_selfNameLower.empty()) g_nameOwners[g_selfNameLower] = g_selfUuidKey;
}

// Everything the targeting modules need about one entity. `isPlayer` is the caller's own class
// test, which every module already has from its world loop.
bool detectTarget(jobject entity, bool isPlayer, TargetInfo& out) {
    out = TargetInfo{};
    if (!entity) return false;
    out.isPlayer = isPlayer;
    if (!isPlayer) return true;                       // mobs are never friends or bots
    refreshPlayerDetection();
    out.rawName = mc::Entity(entity).displayName();
    out.name = det::cleanPlayerName(out.rawName);
    // With Anti-Bot off and no friend listed there is nothing left to decide, and the UUID is not
    // even read: the same cost as before this service existed.
    if (!g_antiBot && g_friends.empty()) return true;
    out.uuid = mc::Entity(entity).uuid();
    out.uuidKey = det::uuidKey(out.uuid);
    out.uuidVersion = det::uuidVersion(out.uuid);
    out.isFriend = friendMatch(out.uuidKey, out.name);
    if (g_antiBot) {
        const AntiBotSettings& s = g_antiBotSettings;
        const std::string lower = det::lowerCopy(out.name);
        const bool marker = s.checkNames && det::isBotNameMarker(lower);
        // A UUID that is present but not a UUID at all counts as plugin-made too.
        const bool shape = s.checkUuid && (det::uuidIsBotShape(out.uuidVersion, s.allowOfflineUuid) ||
                                           (!out.uuid.empty() && !det::looksLikeUuid(out.uuid)));
        bool clone = false;
        if (s.checkClones) {
            const auto owner = g_nameOwners.find(lower);
            clone = det::isBotClone(lower, out.uuidKey, out.uuidVersion,
                                    owner != g_nameOwners.end() ? owner->second : std::string{},
                                    s.allowOfflineUuid, g_selfNameLower, g_selfUuidKey);
        }
        out.isBot = marker || shape || clone;
        if (!out.uuidKey.empty()) {
            PlayerIdentity& id = g_identities[out.uuidKey];
            id.name = out.name;
            id.version = out.uuidVersion;
            id.accountLike = det::isAccountUuid(out.uuidVersion, s.allowOfflineUuid);
            id.botMarker = marker;
            id.botShape = shape;
            id.lastSeen = ImGui::GetTime();
            // The most account-like entity keeps the name, so an NPC that spawned first cannot make
            // the real player look like the copy.
            if (!lower.empty()) {
                const auto owner = g_nameOwners.find(lower);
                if (owner == g_nameOwners.end() || owner->second == out.uuidKey ||
                    (id.accountLike && !identityAccountLike(owner->second)))
                    g_nameOwners[lower] = out.uuidKey;
            }
        }
    }
    out.blocked = out.isBot || out.isFriend;
    return true;
}

// Cleaned display name (what a module shows and compares). Never throws.
std::string entityName(jobject entity) {
    if (!entity) return {};
    return det::cleanPlayerName(mc::Entity(entity).displayName());
}

bool entityIsFriend(jobject entity) {
    TargetInfo info;
    detectTarget(entity, true, info);
    return info.isFriend;
}

// True when an entity-targeting module must leave this entity alone: a friend, or a bot / NPC that
// the Anti-Bot module is asked to keep the entity modules off.
bool entityBlocked(jobject entity, bool isPlayer) {
    if (!entity) return false;
    TargetInfo info;
    detectTarget(entity, isPlayer, info);
    return info.blocked;
}

// A stable key for a target we mean to keep acting on: the UUID when the game gives us one (plugins
// can rename a player, but they cannot change an account's UUID), the cleaned name otherwise so
// tracking still works when no UUID is readable.
std::string trackedTargetKey(jobject entity) {
    if (!entity) return {};
    const std::string uuid = mc::Entity(entity).uuid();
    if (det::looksLikeUuid(uuid)) return det::uuidKey(uuid);
    const std::string name = det::lowerCopy(det::cleanPlayerName(mc::Entity(entity).displayName()));
    return name.empty() ? std::string{} : ("name:" + name);
}

void resetPlayerDetection() {
    g_identities.clear();
    g_nameOwners.clear();
    g_friendPins.clear();
    g_selfUuidKey.clear();
    g_selfNameLower.clear();
    g_detectionRefreshedAt = -1.0;
}

bool project(const Vec3& p, const Vec3& camera, const Vec3& right,
             const Vec3& forward, const Vec3& up, float focal,
             const ImVec2& display, ImVec2& out) {
    const Vec3 rel = p - camera;
    const float z = rel.dot(forward);
    if (!std::isfinite(z) || z <= 0.05f || z > 512.f) return false;
    const float sx = display.x * 0.5f + rel.dot(right) * focal / z;
    const float sy = display.y * 0.5f - rel.dot(up) * focal / z;
    if (!std::isfinite(sx) || !std::isfinite(sy)) return false;
    out = ImVec2(sx, sy);
    return true;
}

void drawEsp(ImDrawList* dl, float tickDelta) {
    if (!g_esp || !jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return;
    jobject world = mc::Client::world();
    jobject player = mc::Client::player();
    jobject camera = mc::Client::camera();
    if (!world || !player || !camera) return;

    JNIEnv* env = jni::env();
    if (!env) return;
    const mc::Symbols& symbols = mc::sym();
    if (!symbols.living) return;

    tickDelta = std::clamp(tickDelta, 0.f, 1.f);
    const Vec3 camPos = mc::cam::pos(camera);
    Vec3 right,up,forward;
    if (!mc::cam::basis(camera,right,up,forward)) {
        // The rotation-vector fallback still uses the exact interpolated
        // Camera yaw/pitch, never the player or entity yaw.
        const float yaw=mc::cam::yaw(camera)*0.017453292519943295f;
        const float pitch=mc::cam::pitch(camera)*0.017453292519943295f;
        forward=Vec3(-std::sin(yaw)*std::cos(pitch),-std::sin(pitch),
                     std::cos(yaw)*std::cos(pitch));
        right=Vec3(-std::cos(yaw),0.f,-std::sin(yaw));     // facing -Z gives +X
        up=right.cross(forward).normalized();
    }
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    float fov = mc::Client::renderFov(tickDelta);
    if (fov < 1.f || fov > 179.f)
        fov = (float)mc::opts::getDoubleOption(mc::Client::options(), symbols.opt_fov, 70.0);
    if (display.x <= 1.f || display.y <= 1.f || fov < 1.f || fov > 179.f) return;
    const float focal = (display.y * 0.5f) / std::tan(fov * 0.008726646259971648f);

    const std::vector<jobject> entities = mc::collectEntities(world, kEntityScan);
    const ImU32 color = packed(g_espSettings.color);
    const ImU32 fill = packed(g_espSettings.color, 0.18f);
    const ImU32 friendCol = packed(g_espSettings.friendColor);
    const ImU32 friendFill = packed(g_espSettings.friendColor, 0.18f);
    const ImU32 healthBg = IM_COL32(15, 12, 16, 210);
    for (jobject entity : entities) {
        if (!entity) continue;
        const bool isPlayer = isPlayerEntity(env, entity);
        const bool isLiving = env->IsInstanceOf(entity, symbols.living) == JNI_TRUE;
        if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(entity); continue; }
        if (env->IsSameObject(entity,player)==JNI_TRUE || !isLiving || (isPlayer && !g_espSettings.allowPlayers) ||
            (!isPlayer && !g_espSettings.allowEntities)) {
            env->DeleteLocalRef(entity);
            continue;
        }

        mc::LivingEntity living(entity);
        if (!living.alive()) { env->DeleteLocalRef(entity); continue; }
        // Friends get their own colour so they stand out from other players.
        const bool isFriendTarget = isPlayer && g_espSettings.highlightFriends && entityIsFriend(entity);
        const ImU32 entityColor = isFriendTarget ? friendCol : color;
        const ImU32 entityFill = isFriendTarget ? friendFill : fill;
        const mc::Entity::Bounds bounds = living.interpolatedBounds(tickDelta);
        if (!bounds.valid) {
            env->DeleteLocalRef(entity);
            continue;
        }

        const Vec3 corners[8] = {
            {bounds.min.x,bounds.min.y,bounds.min.z}, {bounds.max.x,bounds.min.y,bounds.min.z},
            {bounds.max.x,bounds.min.y,bounds.max.z}, {bounds.min.x,bounds.min.y,bounds.max.z},
            {bounds.min.x,bounds.max.y,bounds.min.z}, {bounds.max.x,bounds.max.y,bounds.min.z},
            {bounds.max.x,bounds.max.y,bounds.max.z}, {bounds.min.x,bounds.max.y,bounds.max.z}
        };
        ImVec2 screen[8];
        bool projected = true;
        for (int i=0; i<8; ++i)
            projected = project(corners[i], camPos, right, forward, up, focal, display, screen[i]) && projected;
        if (!projected) { env->DeleteLocalRef(entity); continue; }

        if (g_espSettings.mode == EspMode::TwoD) {
            float minX=screen[0].x, maxX=minX, minY=screen[0].y, maxY=minY;
            for (int i=1; i<8; ++i) {
                minX=std::min(minX,screen[i].x); maxX=std::max(maxX,screen[i].x);
                minY=std::min(minY,screen[i].y); maxY=std::max(maxY,screen[i].y);
            }
            if (maxX-minX > 1.f && maxY-minY > 1.f) {
                dl->AddRect(ImVec2(minX,minY), ImVec2(maxX,maxY), entityColor, 0.f, 0, 1.5f);
                const float health = std::clamp(living.health() / std::max(1.f,living.maxHealth()),0.f,1.f);
                const float barY = std::max(2.f, minY - 5.f);
                dl->AddRectFilled(ImVec2(minX,barY), ImVec2(maxX,barY+3.f), healthBg);
                dl->AddRectFilled(ImVec2(minX,barY), ImVec2(minX+(maxX-minX)*health,barY+3.f),
                                  IM_COL32((int)(235.f*(1.f-health)+58.f*health), (int)(70.f+175.f*health), 76, 255));
            }
        } else {
            static constexpr int edges[][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
            if (!g_espSettings.onlyOutline) {
                for (const auto& face : std::array<std::array<int,4>,6>{{{{0,1,2,3}},{{4,7,6,5}},{{0,4,5,1}},{{1,5,6,2}},{{2,6,7,3}},{{3,7,4,0}}}}) {
                    dl->AddQuadFilled(screen[face[0]],screen[face[1]],screen[face[2]],screen[face[3]],entityFill);
                }
            }
            for (const auto& edge : edges) dl->AddLine(screen[edge[0]],screen[edge[1]],entityColor,1.5f);
            // 3D health indicator (2D already had a health bar).
            if (g_espSettings.showHealth3d) {
                float barMinX=screen[0].x, barMaxX=screen[0].x, barMinY=screen[0].y;
                for (int i=1;i<8;++i) {
                    barMinX=std::min(barMinX,screen[i].x); barMaxX=std::max(barMaxX,screen[i].x);
                    barMinY=std::min(barMinY,screen[i].y);
                }
                if (barMaxX-barMinX > 1.f) {
                    const float health=std::clamp(living.health()/std::max(1.f,living.maxHealth()),0.f,1.f);
                    const float barY=std::max(2.f,barMinY-6.f);
                    dl->AddRectFilled(ImVec2(barMinX,barY),ImVec2(barMaxX,barY+3.f),healthBg);
                    dl->AddRectFilled(ImVec2(barMinX,barY),ImVec2(barMinX+(barMaxX-barMinX)*health,barY+3.f),
                                      IM_COL32((int)(235.f*(1.f-health)+58.f*health),(int)(70.f+175.f*health),76,255));
                }
            }
        }
        env->DeleteLocalRef(entity);
    }
}

// ---- Shader ESP -----------------------------------------------------------------------------
// A glowing outline around every player and every dropped item: a soft halo of `thickness`
// passes under a solid rim, all at the opacity the card asks for. Friends take the friend colour,
// so a friend reads the same here as in the flat ESP.
void drawShaderEsp(ImDrawList* dl, float tickDelta) {
    // The drawn glow is the Custom style. Vanilla lets the game outline the model
    // itself (see updateShaderEspGlow), which is what the card opens on.
    if (g_shaderEspSettings.style != ShaderGlowStyle::Custom) return;
    if (!g_shaderEsp || !jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return;
    jobject world = mc::Client::world();
    jobject player = mc::Client::player();
    jobject camera = mc::Client::camera();
    if (!world || !player || !camera) return;
    JNIEnv* env = jni::env();
    if (!env) return;
    const float opacity = std::clamp(g_shaderEspSettings.opacity, 0.f, 1.f);
    if (opacity <= 0.002f) return;
    const int thickness = std::clamp(g_shaderEspSettings.thickness, 1, 5);
    const float maxDistance = std::max(8.f, g_shaderEspSettings.maxDistance);

    tickDelta = std::clamp(tickDelta, 0.f, 1.f);
    const Vec3 camPos = mc::cam::pos(camera);
    Vec3 right, up, forward;
    if (!mc::cam::basis(camera, right, up, forward)) {
        const float yaw = mc::cam::yaw(camera) * 0.017453292519943295f;
        const float pitch = mc::cam::pitch(camera) * 0.017453292519943295f;
        forward = Vec3(-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch));
        right = Vec3(-std::cos(yaw), 0.f, -std::sin(yaw));
        up = right.cross(forward).normalized();
    }
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    float fov = mc::Client::renderFov(tickDelta);
    if (fov < 1.f || fov > 179.f)
        fov = (float)mc::opts::getDoubleOption(mc::Client::options(), mc::sym().opt_fov, 70.0);
    if (display.x <= 1.f || display.y <= 1.f || fov < 1.f || fov > 179.f) return;
    const float focal = (display.y * 0.5f) / std::tan(fov * 0.008726646259971648f);

    for (jobject entity : mc::collectEntities(world, kEntityScan)) {
        if (!entity) continue;
        if (env->IsSameObject(entity, player) == JNI_TRUE) { env->DeleteLocalRef(entity); continue; }
        const bool isPlayer = isPlayerEntity(env, entity);
        if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(entity); continue; }
        // Players, and dropped items: an item entity's registry path is "item" in every language.
        const bool isItem = !isPlayer && mc::entityTypePath(entity) == "item";
        if (!isPlayer && !isItem) { env->DeleteLocalRef(entity); continue; }
        mc::Entity view(entity);
        const mc::Entity::Bounds bounds = view.interpolatedBounds(tickDelta);
        if (!bounds.valid) { env->DeleteLocalRef(entity); continue; }
        const Vec3 centre((bounds.min.x + bounds.max.x) * 0.5f,
                          (bounds.min.y + bounds.max.y) * 0.5f,
                          (bounds.min.z + bounds.max.z) * 0.5f);
        if ((centre - camPos).len() > maxDistance) { env->DeleteLocalRef(entity); continue; }

        const Vec3 corners[8] = {
            {bounds.min.x,bounds.min.y,bounds.min.z}, {bounds.max.x,bounds.min.y,bounds.min.z},
            {bounds.max.x,bounds.min.y,bounds.max.z}, {bounds.min.x,bounds.min.y,bounds.max.z},
            {bounds.min.x,bounds.max.y,bounds.min.z}, {bounds.max.x,bounds.max.y,bounds.min.z},
            {bounds.max.x,bounds.max.y,bounds.max.z}, {bounds.min.x,bounds.max.y,bounds.max.z}
        };
        ImVec2 screen[8];
        bool projected = true;
        for (int i = 0; i < 8; ++i)
            projected = project(corners[i], camPos, right, forward, up, focal, display, screen[i]) && projected;
        if (!projected) { env->DeleteLocalRef(entity); continue; }
        float minX = screen[0].x, maxX = minX, minY = screen[0].y, maxY = minY;
        for (int i = 1; i < 8; ++i) {
            minX = std::min(minX, screen[i].x); maxX = std::max(maxX, screen[i].x);
            minY = std::min(minY, screen[i].y); maxY = std::max(maxY, screen[i].y);
        }
        if (maxX - minX < 1.f || maxY - minY < 1.f) { env->DeleteLocalRef(entity); continue; }

        const bool friendEntity = isPlayer && entityIsFriend(entity);
        const ImVec4& tint = friendEntity ? g_shaderEspSettings.friendColor : g_shaderEspSettings.color;
        const float radius = 3.f;
        // Halo: widest and faintest pass first, so the rim reads as glowing rather than boxed.
        for (int ring = thickness; ring >= 1; --ring) {
            const float spread = (float)ring * 1.7f;
            const float ringAlpha = opacity * (1.f - (float)ring / (float)(thickness + 1)) * 0.6f;
            dl->AddRect(ImVec2(minX - spread, minY - spread), ImVec2(maxX + spread, maxY + spread),
                        packed(tint, ringAlpha), radius + spread * 0.5f, 0, 1.2f + spread * 0.5f);
        }
        // A faint body glow, so the shape itself is shaded and not only outlined.
        dl->AddRectFilled(ImVec2(minX, minY), ImVec2(maxX, maxY), packed(tint, opacity * 0.10f), radius);
        dl->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), packed(tint, opacity), radius, 0, 1.6f);
        env->DeleteLocalRef(entity);
    }
}

// ---- Shader ESP: Minecraft's own glow --------------------------------------
// The vanilla style draws nothing itself: it asks the game for its own glowing
// outline by switching on the client-side glow flag of the players and dropped
// items it wants outlined. What the game then draws wraps the rendered model -
// the skin, the armour, the item in a player's hand, the item model of a dropped
// stack - which is the outline the drawn halo could only approximate (the frame's
// depth is cleared before it ends, so a silhouette cannot be traced by hand).
//
// Flags are handed back when the module is switched off, when the style changes,
// when an entity walks out of Max distance or dies, and when the client resets.
// An entity that already glows (a server effect, another mod) is never touched,
// so this module can never switch off something it did not switch on.
std::unordered_map<std::string,double> g_glowOwned;
const double kGlowOwnedTtl   = 2.0;   // our glow, still wanted: how long one frame may miss an entity
const double kGlowReleaseTtl = 8.0;   // handing a flag back: keep looking for a while
const int    kGlowScan      = 256;    // entities considered while the glow is on
const int    kGlowScanRelease = 4096; // ...and while the flags are being handed back

void clearOwnedGlow() {
    if (g_glowOwned.empty()) return;
    if (jni::ready() && mc::frame::valid() && mc::frame::resolved()) {
        jobject world = mc::Client::world();
        JNIEnv* env = jni::env();
        if (world && env) {
            for (jobject entity : mc::collectEntities(world, kGlowScanRelease)) {
                if (!entity) continue;
                mc::Entity view(entity);
                const std::string key = view.uuid();
                if (!key.empty() && g_glowOwned.contains(key)) view.setGlowing(false);
                env->DeleteLocalRef(entity);
            }
        }
    }
    g_glowOwned.clear();
}

void updateShaderEspGlow() {
    const bool want = g_shaderEsp && g_shaderEspSettings.style == ShaderGlowStyle::Vanilla;
    if (!want && g_glowOwned.empty()) return;
    const double now = ImGui::GetTime();
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) { g_glowOwned.clear(); return; }
    jobject world = mc::Client::world();
    jobject camera = mc::Client::camera();
    jobject self = mc::Client::player();
    JNIEnv* env = jni::env();
    if (!world || !camera || !env) { g_glowOwned.clear(); return; }
    const Vec3 camPos = mc::cam::pos(camera);
    // A maxDistance of 0 means "glow nothing", which is how every flagged entity is
    // released again when the module or the vanilla style goes away.
    const float maxDistance = want ? std::max(8.f, g_shaderEspSettings.maxDistance) : 0.f;
    // Releasing must not be limited to the window the glow itself scans: an entity we
    // switched on can be outside it (that window is the nearest 256), and leaving one
    // flag behind is exactly the failure this loop exists to prevent. So while
    // releasing, the world list is read much deeper and only the entities we own are
    // looked at - one id read each, no type or health work.
    const int scanLimit = want ? kGlowScan : kGlowScanRelease;
    for (jobject entity : mc::collectEntities(world, scanLimit)) {
        if (!entity) continue;
        if (self && env->IsSameObject(entity, self) == JNI_TRUE) { env->DeleteLocalRef(entity); continue; }
        if (!want) {
            mc::Entity view(entity);
            const std::string key = view.uuid();
            if (!key.empty() && g_glowOwned.contains(key)) {
                view.setGlowing(false);
                g_glowOwned.erase(key);
            }
            env->DeleteLocalRef(entity);
            continue;
        }
        const bool isPlayer = isPlayerEntity(env, entity);
        if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(entity); continue; }
        const std::string typePath = isPlayer ? std::string("player") : mc::entityTypePath(entity);
        const glow::Kind kind = glow::kindFor(typePath.c_str());
        if (kind == glow::Kind::Ignored) { env->DeleteLocalRef(entity); continue; }
        mc::Entity view(entity);
        if (!view.alive()) { env->DeleteLocalRef(entity); continue; }
        const float distance = (float)(view.pos() - camPos).len();
        const std::string key = view.uuid();
        const bool ours = !key.empty() && g_glowOwned.contains(key);
        const glow::Decision decision = glow::decide(kind, false, distance, maxDistance,
                                                     ours, view.glowing());
        if (decision.clear) {
            view.setGlowing(false);
            g_glowOwned.erase(key);
        } else if (decision.flag) {
            if (ours || g_glowOwned.size() < (size_t)glow::kMaxGlowing) {
                view.setGlowing(true);
                if (!key.empty()) g_glowOwned[key] = now;
            }
        } else if (ours) {
            g_glowOwned[key] = now;   // still ours and still wanted: keep it alive
        }
        env->DeleteLocalRef(entity);
    }
    // Entities that left the world keep nothing to release: their flag died with them.
    const double ttl = want ? kGlowOwnedTtl : kGlowReleaseTtl;
    for (auto it = g_glowOwned.begin(); it != g_glowOwned.end();) {
        if (now - it->second > ttl) it = g_glowOwned.erase(it);
        else ++it;
    }
}

// ---- Shader ESP: the item in your own hand ---------------------------------
// The game never outlines the first-person item - it is not an entity - so this
// is drawn here. The held item's own texture supplies the shape, at the position
// Minecraft puts the hand, so it is the item that glows and not a box around it.
void addRotatedIcon(ImDrawList* dl, ImTextureID texture, const ImVec2& centre, float half,
                    float cosR, float sinR, ImU32 col) {
    const float corners[4][2] = { {-half,-half}, {half,-half}, {half,half}, {-half,half} };
    ImVec2 p[4];
    for (int i = 0; i < 4; ++i) {
        const float x = corners[i][0] * cosR - corners[i][1] * sinR;
        const float y = corners[i][0] * sinR + corners[i][1] * cosR;
        p[i] = ImVec2(centre.x + x, centre.y + y);
    }
    dl->AddImageQuad(texture, p[0], p[1], p[2], p[3],
                     ImVec2(0,0), ImVec2(1,0), ImVec2(1,1), ImVec2(0,1), col);
}

void drawHandGlow(ImDrawList* dl, float tickDelta) {
    if (!g_shaderEsp || g_shaderEspSettings.style != ShaderGlowStyle::Vanilla) return;
    if (!g_shaderEspSettings.handGlow) return;
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return;
    // The game does not render the hand behind one of its own screens (inventory, chat,
    // pause), so a glow there would sit on nothing.
    if (mc::Client::screenOpen()) return;
    jobject player = mc::Client::player();
    jobject camera = mc::Client::camera();
    if (!player || !camera) return;
    mc::LocalRef stack(mc::LivingEntity(player).mainHandStack());
    if (!stack || mc::stk::isEmpty(stack.get())) return;
    const ImTextureID texture = itemicons::get(mc::stk::itemId(stack.get()));
    if (!texture) return;
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    // The hand is drawn with the base FOV, not the sprint/zoom one the world uses.
    float fov = mc::Client::renderFov(tickDelta, false);
    if (fov < 1.f || fov > 179.f)
        fov = (float)mc::opts::getDoubleOption(mc::Client::options(), mc::sym().opt_fov, 70.0);
    const glow::HandGlow anchor = glow::handGlow(fov, display.x, display.y,
                                                 (float)g_shaderEspSettings.handSize,
                                                 (float)g_shaderEspSettings.handOffsetX,
                                                 (float)g_shaderEspSettings.handOffsetY);
    if (!anchor.valid) return;
    const float radians = (float)g_shaderEspSettings.handRotation * 0.017453292519943295f;
    const float cosR = std::cos(radians), sinR = std::sin(radians);
    const ImVec2 centre(anchor.x, anchor.y);
    // White, like the outline the game draws on everything else: a soft halo, then
    // the item's own silhouette at full strength under it.
    const float rings = 3.f;
    for (int ring = (int)rings; ring >= 1; --ring) {
        const float half = anchor.size * 0.5f * (1.f + (float)ring * 0.22f);
        const float alpha = 0.5f * (1.f - (float)ring / (rings + 1.f));
        addRotatedIcon(dl, texture, centre, half, cosR, sinR,
                       IM_COL32(255, 255, 255, (int)(alpha * 255.f)));
    }
    addRotatedIcon(dl, texture, centre, anchor.size * 0.5f, cosR, sinR, IM_COL32(255, 255, 255, 235));
}

void processModuleKeybinds() {
    // Chat and any other screen swallow the keys, so a letter typed into chat can never
    // toggle a module bound to that letter.
    if (gui::inputFrozen() || mc::Client::screenOpen()) return;
    const std::vector<int>& taps = gui::keyTaps();
    for (const Entry& entry : g_entries) {
        if (!entry.enabled || !entry.keybind || entry.macro || *entry.keybind <= 0) continue;
        if (std::find(taps.begin(), taps.end(), *entry.keybind) != taps.end()) {
            *entry.enabled = !*entry.enabled;
            // Notifications (Settings > General > Interface) announces the toggle.
            notifyModuleToggled(entry.name, *entry.enabled);
        }
    }
}

// ---- Friend join notifications ------------------------------------------------------------
// "Friend <name> joined the server": every name that appears while a world is
// loaded is compared against the friends list. The first scan after a join only
// seeds the set, so friends already present never spam a notification.
std::unordered_set<std::string> g_seenPlayers;
bool g_seenPlayersSeeded = false;

void updateFriendJoinNotifications() {
    const bool ready = jni::ready() && mc::frame::valid() && mc::frame::resolved() && mc::Client::world();
    if (!ready) { g_seenPlayers.clear(); g_seenPlayersSeeded = false; return; }
    const std::vector<std::string> names = nearbyPlayerNames();
    std::unordered_set<std::string> current;
    for (const std::string& name : names) current.insert(lowerCopy(name));
    if (!g_seenPlayersSeeded) { g_seenPlayers = current; g_seenPlayersSeeded = true; return; }
    for (const std::string& name : names) {
        const std::string lower = lowerCopy(name);
        if (g_seenPlayers.contains(lower)) continue;
        if (!g_notifications || !isFriendName(name)) continue;
        char buf[256];
        snprintf(buf, sizeof(buf), "Friend %s joined the server", name.c_str());
        gui::notify(buf);
    }
    g_seenPlayers = std::move(current);
}

// ---- Logout Spots --------------------------------------------------------------------------
// A spot is saved when a player who was in the world is no longer in it: their name and the last
// position the client saw them at. A marker is drawn around that standing box (the colour from the
// card, black by default) with their name over it, and only while the camera is within range.
struct LogoutSpot { std::string key; std::string name; Vec3 pos; };
std::unordered_map<std::string, LogoutSpot> g_lastSeenPlayers;   // uuidKey -> last seen this frame
std::vector<LogoutSpot> g_logoutSpotList;

void updateLogoutSpots() {
    if (!g_logoutSpots) { g_lastSeenPlayers.clear(); return; }
    const bool ready = jni::ready() && mc::frame::valid() && mc::frame::resolved() && mc::Client::world();
    if (!ready) { g_lastSeenPlayers.clear(); return; }
    jobject world = mc::Client::world();
    jobject self = mc::Client::player();
    JNIEnv* env = jni::env();
    if (!world || !env) return;
    std::unordered_map<std::string, LogoutSpot> current;
    for (jobject entity : mc::collectEntities(world, 1024)) {
        if (!entity) continue;
        const bool isPlayer = isPlayerEntity(env, entity);
        if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(entity); continue; }
        if (!isPlayer || !g_logoutSpotsSettings.onlyPlayers || (self && env->IsSameObject(entity, self) == JNI_TRUE)) {
            env->DeleteLocalRef(entity);
            continue;
        }
        mc::Entity view(entity);
        const std::string key = view.uuid();
        if (!key.empty()) {
            LogoutSpot spot;
            spot.key = key;
            spot.pos = view.pos();
            spot.name = det::cleanPlayerName(view.displayName());
            current[key] = spot;
        }
        env->DeleteLocalRef(entity);
    }
    for (const auto& entry : g_lastSeenPlayers) {
        const std::string& key = entry.first;
        if (current.contains(key)) continue;
        // Gone from the world: that is a logout. One marker per player, refreshed to where they
        // were last seen; the list is bounded so a long session cannot grow it without limit.
        bool updated = false;
        for (LogoutSpot& spot : g_logoutSpotList)
            if (spot.key == key) { spot = entry.second; updated = true; break; }
        if (!updated && g_logoutSpotList.size() < 256) g_logoutSpotList.push_back(entry.second);
    }
    g_lastSeenPlayers = std::move(current);
}

void drawLogoutSpots(ImDrawList* dl, float tickDelta) {
    if (!g_logoutSpots || g_logoutSpotList.empty()) return;
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return;
    jobject camera = mc::Client::camera();
    if (!camera) return;
    tickDelta = std::clamp(tickDelta, 0.f, 1.f);
    const Vec3 camPos = mc::cam::pos(camera);
    Vec3 right, up, forward;
    if (!mc::cam::basis(camera, right, up, forward)) {
        const float yaw = mc::cam::yaw(camera) * 0.017453292519943295f;
        const float pitch = mc::cam::pitch(camera) * 0.017453292519943295f;
        forward = Vec3(-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch));
        right = Vec3(-std::cos(yaw), 0.f, -std::sin(yaw));
        up = right.cross(forward).normalized();
    }
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    float fov = mc::Client::renderFov(tickDelta);
    if (fov < 1.f || fov > 179.f)
        fov = (float)mc::opts::getDoubleOption(mc::Client::options(), mc::sym().opt_fov, 70.0);
    if (display.x <= 1.f || display.y <= 1.f || fov < 1.f || fov > 179.f) return;
    const float focal = (display.y * 0.5f) / std::tan(fov * 0.008726646259971648f);
    const ImU32 color = packed(g_logoutSpotsSettings.color);
    const float maxDistance = std::max(4.f, g_logoutSpotsSettings.maxDistance);
    static constexpr int edges[][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
    for (const LogoutSpot& spot : g_logoutSpotList) {
        if ((spot.pos - camPos).len() > maxDistance) continue;
        const Vec3 corners[8] = {
            {spot.pos.x-0.3f, spot.pos.y,      spot.pos.z-0.3f},
            {spot.pos.x+0.3f, spot.pos.y,      spot.pos.z-0.3f},
            {spot.pos.x+0.3f, spot.pos.y,      spot.pos.z+0.3f},
            {spot.pos.x-0.3f, spot.pos.y,      spot.pos.z+0.3f},
            {spot.pos.x-0.3f, spot.pos.y+1.8f, spot.pos.z-0.3f},
            {spot.pos.x+0.3f, spot.pos.y+1.8f, spot.pos.z-0.3f},
            {spot.pos.x+0.3f, spot.pos.y+1.8f, spot.pos.z+0.3f},
            {spot.pos.x-0.3f, spot.pos.y+1.8f, spot.pos.z+0.3f},
        };
        ImVec2 screen[8];
        bool projected = true;
        for (int i = 0; i < 8; ++i)
            projected = project(corners[i], camPos, right, forward, up, focal, display, screen[i]) && projected;
        if (!projected) continue;
        for (const auto& edge : edges) dl->AddLine(screen[edge[0]], screen[edge[1]], color, 2.0f);
        if (!spot.name.empty()) {
            float cx = 0.f, topY = screen[0].y;
            for (int i = 0; i < 8; ++i) { cx += screen[i].x; topY = std::min(topY, screen[i].y); }
            cx /= 8.f;
            const ImVec2 size = ImGui::CalcTextSize(spot.name.c_str());
            dl->AddText(ImVec2(cx - size.x * 0.5f, topY - size.y - 3.f),
                        IM_COL32(255, 255, 255, 235), spot.name.c_str());
        }
    }
}

void drawArraylist(ImDrawList* dl) {
    std::vector<std::string> active;
    if (g_arraylist)
        for (const Entry& entry : g_entries) if (entry.enabled && *entry.enabled) active.emplace_back(entry.name);
    std::sort(active.begin(), active.end(), arraylistLess);
    std::unordered_map<std::string, bool> present;
    for (const auto& name : active) present[name] = true;
    for (const auto& name : active) g_arraylistFades[name].value = std::min(1.f, g_arraylistFades[name].value + ImGui::GetIO().DeltaTime * 10.f);
    for (auto& pair : g_arraylistFades) if (!present[pair.first]) pair.second.value = std::max(0.f, pair.second.value - ImGui::GetIO().DeltaTime * 10.f);
    if (active.empty() && g_arraylistFades.empty()) return;

    std::vector<std::string> displayed=active;
    for (const auto& pair:g_arraylistFades)
        if (!present[pair.first]&&pair.second.value>0.f) displayed.push_back(pair.first);
    std::sort(displayed.begin(), displayed.end(), arraylistLess);

    float y = 18.f;
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const ImU32 text = IM_COL32(245,240,240,255), panel = IM_COL32(26,22,29,230);
    const ImVec4 themeAccent=gui::accentColor();
    for (const auto& name : displayed) {
        Fade& fade = g_arraylistFades[name];
        const ImVec2 size = ImGui::CalcTextSize(name.c_str());
        fade.width = fade.width + (size.x + 26.f - fade.width) * (1.f - std::exp(-12.f*ImGui::GetIO().DeltaTime));
        const float w = std::max(4.f, fade.width) * fade.value;
        const float h = (size.y + 12.f) * fade.value;
        const ImVec2 p0(display.x - w - 14.f, y), p1(display.x - 8.f, y+h);
        dl->AddRectFilled(p0,p1,panel,5.f);
        dl->AddRectFilled(ImVec2(p0.x,p0.y),ImVec2(p0.x+2.f,p1.y),packed(themeAccent,fade.value));
        dl->AddText(ImVec2(p0.x+12.f,p0.y+6.f),
                    (text & ~IM_COL32_A_MASK) | ((ImU32)(255.f*fade.value)<<IM_COL32_A_SHIFT),name.c_str());
        y += (size.y + 14.f) * fade.value;
    }
    for (auto it=g_arraylistFades.begin(); it!=g_arraylistFades.end();) {
        if (!present[it->first] && it->second.value <= 0.f) it=g_arraylistFades.erase(it); else ++it;
    }
}

bool isWeaponId(const std::string& id) {
    return id.find("_sword") != std::string::npos || id.find("_axe") != std::string::npos ||
           id.find("_spear") != std::string::npos || id == "mace" || id == "trident" ||
           id == "bow" || id == "crossbow";
}

bool playerCanUseModule(jobject playerObj);   // defined below
bool playerWouldCrit(mc::Player& player);     // defined below

// One Triggerbot attempt. `second` selects the Triggerbot V2 card, so the two share every rule
// and differ only in their settings and in their own pacing state.
//
// V2 posts the game's own Attack key rather than calling doAttack(). A click the game is meant to
// read has to be a real button transition, so the press is held for a beat and released on a later
// frame instead of pressing and releasing inside one frame - the same way the key-driven placements
// elsewhere in this client hold the Use key across a tick.
constexpr double kTriggerbotV2ClickHold = 0.04;



// Would the next hit from this player be critical? Vanilla also excludes water, ladders, blindness,
// vehicles and sprinting; those are left to the server, which still decides the hit's damage. The
// airborne + falling signal is the part the module can act on before the swing goes out.
bool playerWouldCrit(mc::Player& player) {
    return !player.onGround() && !player.gliding() && player.fallDistance() > 0.f;
}

void selectSlot(jobject inventory, int slot) {
    if (!inventory || slot < 0 || slot > 8) return;
    mc::inv::setSelectedSlotMethod(inventory, slot);
    mc::Client::syncSelectedSlot();
}

void restoreAnchorSlot(bool sync) {
    if (!g_anchorSlotChanged || g_anchorOriginalSlot < 0 || g_anchorOriginalSlot > 8) return;
    mc::LocalRef inventory(mc::Player(mc::Client::player()).inventory());
    if (inventory) {
        if (sync) selectSlot(inventory.get(), g_anchorOriginalSlot);
        else mc::inv::setSelectedSlot(inventory.get(), g_anchorOriginalSlot);
    }
    g_anchorSlotChanged = false;
}

void cancelAnchor(bool restore) {
    if (restore) restoreAnchorSlot(true);
    g_anchorPhase = AnchorPhase::Idle;
    g_anchorTargetValid = false;
    g_anchorOriginalSlot = -1;
    g_anchorGlowstoneSlot = -1;
}

void notifyAnchor(const char* message) {
    const double now=ImGui::GetTime();
    const std::string text=message?message:"AutoAnchor failed";
    if (text!=g_anchorLastNotice || now-g_anchorNoticeAt>2.8) {
        notifyAction(text);
        g_anchorLastNotice=text;
        g_anchorNoticeAt=now;
    }
}

std::string hotbarItemId(jobject list, int slot) {
    mc::LocalRef stack(mc::inv::getFromList(list, slot));
    return stack ? mc::stk::itemId(stack.get()) : std::string{};
}

// ---- block-state text helpers (BlockState.toString() -> "Block{minecraft:x}[a=1,b=true]") ----
int stateInt(const std::string& state, const char* key, int fallback) {
    const std::string needle = std::string(key) + "=";
    size_t at = state.find(needle);
    while (at != std::string::npos) {
        if (at == 0 || state[at - 1] == '[' || state[at - 1] == ',') {
            size_t i = at + needle.size();
            int value = 0; bool any = false;
            while (i < state.size() && state[i] >= '0' && state[i] <= '9') { value = value * 10 + (state[i] - '0'); ++i; any = true; }
            return any ? value : fallback;
        }
        at = state.find(needle, at + 1);
    }
    return fallback;
}

bool isAirId(const std::string& id) { return id == "air" || id == "cave_air" || id == "void_air"; }

// A block a vanilla empty bucket can scoop water out of: a still water block (level=0),
// a waterlogged block, or one of the always-wet plants / bubble columns.
bool isWaterSourceAt(int x, int y, int z) {
    const std::string state = mc::Client::blockStateString(x, y, z);
    if (state.empty()) return false;
    if (state.find("minecraft:water}") != std::string::npos) return stateInt(state, "level", -1) == 0;
    if (state.find("waterlogged=true") != std::string::npos) return true;
    static const char* const wet[] = {"minecraft:kelp}", "minecraft:kelp_plant}", "minecraft:seagrass}",
                                      "minecraft:tall_seagrass}", "minecraft:bubble_column}"};
    for (const char* id : wet) if (state.find(id) != std::string::npos) return true;
    return false;
}

bool anchorItemsReady(jobject playerObj, bool needGlowstone) {
    mc::LocalRef inventory(mc::Player(playerObj).inventory());
    mc::LocalRef list(inventory ? mc::inv::mainList(inventory.get()) : nullptr);
    if (!list) return false;
    const int size=std::min(9,mc::inv::listSize(list.get()));
    const int totemSlot=std::clamp(g_autoAnchorSettings.totemSlot,1,9)-1;
    if (totemSlot>=size || hotbarItemId(list.get(),totemSlot)!="totem_of_undying") return false;
    if (!needGlowstone) return true;
    for (int i=0;i<size;++i)
        if (hotbarItemId(list.get(),i)=="glowstone") return true;
    return false;
}

void updateAutoAnchor() {
    if (!g_autoAnchor || gui::inputFrozen()) {
        // Do not leave the player on the temporary Glowstone/Totem slot while
        // the menu is open or after this module is switched off.
        if (g_anchorPhase != AnchorPhase::Idle) cancelAnchor(true);
        return;
    }
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) {
        return;
    }
    jobject playerObj = mc::Client::player();
    jobject world = mc::Client::world();
    if (!playerObj || !world) { cancelAnchor(true); return; }
    mc::LivingEntity playerLiving(playerObj);
    if (mc::Client::screenOpen() || playerLiving.usingItem() || playerLiving.blocking()) {
        if (g_anchorPhase != AnchorPhase::Idle) cancelAnchor(true);
        return;
    }

    mc::LocalRef hit(mc::Client::crosshair());
    int x=0, y=0, z=0;
    if (!hit || mc::hit::typeOf(hit.get()) != 1 || !mc::hit::blockPos(hit.get(),x,y,z) ||
        !mc::Client::isRespawnAnchorAt(x,y,z)) {
        if (g_anchorPhase != AnchorPhase::Idle || g_anchorTargetValid) cancelAnchor(true);
        return;
    }

    mc::Player localPlayer(playerObj);
    const float reach = (float)localPlayer.blockInteractionRange() + 0.75f;
    const Vec3 center((float)x+0.5f,(float)y+0.5f,(float)z+0.5f);
    const Vec3 delta = center - localPlayer.eyePos();
    if (delta.len2() > reach * reach) {
        if (g_anchorPhase != AnchorPhase::Idle || g_anchorTargetValid) cancelAnchor(true);
        return;
    }

    // An anchor that already holds charge only needs the Totem click.
    const bool charged = stateInt(mc::Client::blockStateString(x,y,z), "charges", 0) > 0;
    const bool needGlowstone = !charged;

    const bool sameAnchor = g_anchorTargetValid && x==g_anchorX && y==g_anchorY && z==g_anchorZ;
    if (sameAnchor && g_anchorPhase==AnchorPhase::WaitItems) {
        if (anchorItemsReady(playerObj, needGlowstone)) {
            g_anchorPhase=AnchorPhase::Idle;
            g_anchorTargetValid=false;
            g_anchorLastNotice.clear();
        }
        return;
    }
    if (!(sameAnchor && (g_anchorPhase==AnchorPhase::WaitGlowstone ||
                         g_anchorPhase==AnchorPhase::WaitTotem ||
                         g_anchorPhase==AnchorPhase::Done))) {
        if (g_anchorPhase != AnchorPhase::Idle) cancelAnchor(true);
        g_anchorTargetValid = true;
        g_anchorX=x; g_anchorY=y; g_anchorZ=z;
        mc::LocalRef inventory(mc::Player(playerObj).inventory());
        mc::LocalRef list(inventory ? mc::inv::mainList(inventory.get()) : nullptr);
        if (!inventory || !list) {
            notifyAnchor("AutoAnchor: inventory is unavailable");
            g_anchorPhase=AnchorPhase::WaitItems;
            return;
        }
        const int size=std::min(9,mc::inv::listSize(list.get()));
        const int requestedTotem=std::clamp(g_autoAnchorSettings.totemSlot,1,9)-1;
        if (requestedTotem>=size || hotbarItemId(list.get(),requestedTotem)!="totem_of_undying") {
            notifyAnchor("AutoAnchor: put a Totem in the configured hotbar slot");
            g_anchorPhase=AnchorPhase::WaitItems;
            return;
        }
        g_anchorGlowstoneSlot=-1;
        if (needGlowstone) {
            for (int i=0;i<size;++i) if (hotbarItemId(list.get(),i)=="glowstone") {g_anchorGlowstoneSlot=i;break;}
            if (g_anchorGlowstoneSlot<0) {
                notifyAnchor("AutoAnchor: Glowstone is missing from the hotbar");
                g_anchorPhase=AnchorPhase::WaitItems;
                return;
            }
        }
        g_anchorOriginalSlot=mc::inv::selectedSlot(inventory.get());
        if (needGlowstone) {
            g_anchorSlotChanged=g_anchorOriginalSlot!=g_anchorGlowstoneSlot;
            selectSlot(inventory.get(),g_anchorGlowstoneSlot);
            if (!mc::Client::interactAtCrosshair()) {
                notifyAnchor("AutoAnchor: Glowstone interaction was rejected");
                restoreAnchorSlot(true);
                g_anchorPhase=AnchorPhase::Idle;
                g_anchorTargetValid=false;
                return;
            }
            localPlayer.swingHand();
            notifyAnchor("AutoAnchor: Glowstone used; preparing the Totem");
        } else {
            g_anchorSlotChanged=false;
            notifyAnchor("AutoAnchor: anchor is already charged; preparing the Totem");
        }
        g_anchorPhase=AnchorPhase::WaitGlowstone;
        g_anchorStageAt=ImGui::GetTime();
        return;
    }

    const double now=ImGui::GetTime();
    mc::LocalRef inventory(mc::Player(playerObj).inventory());
    if (!inventory) { cancelAnchor(true); return; }
    if (g_anchorPhase==AnchorPhase::WaitGlowstone &&
        now-g_anchorStageAt >= std::clamp(g_autoAnchorSettings.glowstoneDelayMs,0,600)/1000.0) {
        const int totemSlot=std::clamp(g_autoAnchorSettings.totemSlot,1,9)-1;
        mc::LocalRef list(mc::inv::mainList(inventory.get()));
        if (!list || hotbarItemId(list.get(),totemSlot)!="totem_of_undying") {
            notifyAnchor("AutoAnchor: Totem is no longer in the configured slot");
            cancelAnchor(true);
            return;
        }
        if (!g_anchorSlotChanged) g_anchorOriginalSlot=mc::inv::selectedSlot(inventory.get());
        g_anchorSlotChanged = g_anchorSlotChanged || g_anchorOriginalSlot!=totemSlot;
        selectSlot(inventory.get(),totemSlot);
        g_anchorPhase=AnchorPhase::WaitTotem;
        g_anchorStageAt=now;
    } else if (g_anchorPhase==AnchorPhase::WaitTotem &&
        now-g_anchorStageAt >= std::clamp(g_autoAnchorSettings.totemDelayMs,0,600)/1000.0) {
        const bool activated=mc::Client::interactAtCrosshair();
        if (activated) localPlayer.swingHand();
        notifyAnchor(activated ? "AutoAnchor: Totem interaction sent" : "AutoAnchor: Totem interaction was rejected");
        restoreAnchorSlot(true);
        g_anchorPhase=AnchorPhase::Done;
    }
}

bool playerCanUseModule(jobject playerObj) {
    if (!playerObj || mc::Client::screenOpen()) return false;
    mc::LivingEntity player(playerObj);
    // Vanilla refuses to attack / place / drain while a hand is busy (eating, drawing a bow,
    // holding a shield up) or while any screen such as the inventory is open.
    return !player.usingItem() && !player.blocking();
}

int findHotbarItem(jobject list,const std::string& id) {
    if (!list) return -1;
    const int count=std::min(9,mc::inv::listSize(list));
    for (int i=0;i<count;++i) if (hotbarItemId(list,i)==id) return i;
    return -1;
}

// Module action toasts. These are quiet unless Debug Mode is on (see
// notifyAction in modules.h), so a module never chatters on its own.
void rateLimitedNotify(double& lastAt,const char* message,double seconds) {
    const double now=ImGui::GetTime();
    if (now-lastAt<seconds) return;
    lastAt=now;
    notifyAction(message?message:"");
}

std::string offhandItemId(jobject playerObj) {
    if (!playerObj) return {};
    mc::LocalRef stack(mc::LivingEntity(playerObj).offHandStack());
    return stack ? mc::stk::itemId(stack.get()) : std::string{};
}

// Remember the water source(s) the player just placed. Called from updateAutoBucketDrain
// whenever a right-click happens while a water bucket is/was in a hand, or the water
// bucket disappears from a hand (the click was already consumed). The vanilla bucket is
// consumed in the same tick as the click, so the old check ("still holding water_bucket")
// ran too late on the render thread and every own placement got drained afterwards.
void recordOwnWaterAroundCrosshair(jobject playerObj) {
    if (!playerObj || mc::Client::screenOpen()) return;
    mc::LocalRef hit(mc::Client::crosshair());
    int x=0,y=0,z=0;
    if (!hit || mc::hit::typeOf(hit.get())!=1 || !mc::hit::blockPos(hit.get(),x,y,z)) return;
    // Scan the clicked block and its immediate surroundings for the freshly placed water
    // (it lands in the clicked block, the adjacent block, or a waterloggable one). Only
    // real water sources are recorded, so unrelated blocks are never marked as own water.
    for (int ox=-1;ox<=1;++ox) for (int oy=0;oy<=1;++oy) for (int oz=-1;oz<=1;++oz) {
        const int bx=x+ox, by=y+oy, bz=z+oz;
        if (isWaterSourceAt(bx,by,bz)) g_ownWaterPositions.insert(blockKey(bx,by,bz));
    }
    if (g_ownWaterPositions.size()>512) g_ownWaterPositions.clear();
}

void restoreTemporarySlot(jobject inventory,int originalSlot,bool swapBack) {
    if (swapBack && inventory && originalSlot>=0 && originalSlot<9)
        selectSlot(inventory,originalSlot);
}

bool g_bucketSwapped=false, g_webSwapped=false;
bool g_bucketUseOffhand=false, g_webUseOffhand=false;
bool g_prevMainWater=false, g_prevOffWater=false;
double g_ownWaterScanUntil=0.0;
int g_bucketFails=0;
double g_bucketNoticeAt=-100.0, g_webNoticeAt=-100.0, g_shieldNoticeAt=-100.0;

void abortBucket(jobject inventory,bool restore) {
    if (g_bucketWaiting && g_bucketSwapped && restore) restoreTemporarySlot(inventory,g_bucketOriginalSlot,true);
    g_bucketWaiting=false; g_bucketSwapped=false; g_bucketUseOffhand=false; g_bucketTarget={}; g_bucketSlot=-1;
}

// ---- Auto Drain: web draining --------------------------------------------------------------
// Instead of scooping the water up, a cobweb is placed on the source so the water is buried.
// The rule (drain_anchor.h) accepts only a TOP face: either the ray stops in the water itself
// (the click goes against the block under it) or the ray stops on the top face of a block with
// water in the cell above. Any other face would put the web beside the water, which the server
// never sees. Silent Place sends the top-face rotation with the interaction and puts the real
// view back in the same frame; the camera itself never turns.
bool g_drainWebWaiting=false, g_drainWebSwapped=false, g_drainWebUseOffhand=false;
bool g_drainWebSilentPlace=false;
int g_drainWebSlot=-1, g_drainWebOriginalSlot=-1;
double g_drainWebStageAt=0.0, g_drainWebNextAt=0.0, g_drainWebNoticeAt=-100.0;
BlockTarget g_drainWebTarget{};

void abortDrainWeb(jobject inventory,bool restore) {
    if (g_drainWebWaiting && g_drainWebSwapped && restore) restoreTemporarySlot(inventory,g_drainWebOriginalSlot,true);
    g_drainWebWaiting=false; g_drainWebSwapped=false; g_drainWebUseOffhand=false;
    g_drainWebSilentPlace=false;
    g_drainWebTarget={}; g_drainWebSlot=-1;
}

void updateWebDrain(jobject playerObj,jobject inventory,jobject list,int selected,
                    const std::string& mainId,const std::string& offId) {
    AutoBucketDrainSettings& st=g_autoBucketDrainSettings;
    mc::Player player(playerObj);
    float tickDelta=mc::Client::renderTickDelta();
    if (tickDelta<0.f) tickDelta=1.f;
    mc::LocalRef hit(mc::Client::raycastWithFluids(player.blockInteractionRange(),tickDelta));
    int x=0,y=0,z=0;
    if (!hit || mc::hit::typeOf(hit.get())!=1 || !mc::hit::blockPos(hit.get(),x,y,z)) {
        abortDrainWeb(inventory,true);
        return;
    }
    int ndx=0,ndy=0,ndz=0;
    const bool hitIsWater=isWaterSourceAt(x,y,z);
    const bool faceVisible=mc::hit::sideOffset(hit.get(),ndx,ndy,ndz);
    const int face=ndy>0?1:ndy<0?0:ndz<0?2:ndz>0?3:ndx<0?4:5;
    const bool waterInCell=hitIsWater || isWaterSourceAt(x+ndx,y+ndy,z+ndz);
    const bool solidBelow=!isAirId(mc::Client::blockIdAt(x,y-1,z)) && !isWaterSourceAt(x,y-1,z);
    const drain::Anchor anchor=drain::resolve(hitIsWater,faceVisible,ndy,face,waterInCell,solidBelow);
    if (!anchor.valid) { abortDrainWeb(inventory,true); return; }
    const int placeX=x+anchor.placeDx, placeY=y+anchor.placeDy, placeZ=z+anchor.placeDz;
    const int clickX=x+anchor.clickDx, clickY=y+anchor.clickDy, clickZ=z+anchor.clickDz;
    if (st.webIgnoreOwnWater && g_ownWaterPositions.contains(blockKey(placeX,placeY,placeZ))) {
        abortDrainWeb(inventory,true);
        return;
    }

    const double now=ImGui::GetTime();
    if (g_drainWebWaiting && now-g_drainWebStageAt>1.5) { abortDrainWeb(inventory,true); return; }   // watchdog
    if (!g_drainWebWaiting && (now<g_drainWebNextAt || mc::Client::itemUseCooldown()>0)) return;

    if (!g_drainWebWaiting) {
        int sourceSlot=-1;
        bool useOffhand=false;
        if (mainId=="cobweb") sourceSlot=selected;
        else if (st.webUseOffhand && offId=="cobweb") useOffhand=true;
        else sourceSlot=findHotbarItem(list,"cobweb");
        if (!useOffhand && sourceSlot<0) {
            rateLimitedNotify(g_drainWebNoticeAt,"Auto Drain needs a cobweb in the hotbar or off hand",6.0);
            return;
        }
        g_drainWebOriginalSlot=selected;
        g_drainWebSlot=sourceSlot;
        g_drainWebUseOffhand=useOffhand;
        g_drainWebSilentPlace=st.webSilentPlace;
        g_drainWebTarget={placeX,placeY,placeZ,true};
        g_drainWebStageAt=now;
        g_drainWebWaiting=true;
        g_drainWebSwapped=false;
        if (!useOffhand && !st.webSilentSwap && sourceSlot!=selected) {
            selectSlot(inventory,sourceSlot);
            g_drainWebSwapped=true;
        }
        if (st.webSwapDelayMs>1) return;
    }
    if (!g_drainWebTarget.valid || placeX!=g_drainWebTarget.x || placeY!=g_drainWebTarget.y || placeZ!=g_drainWebTarget.z) {
        abortDrainWeb(inventory,true);
        return;
    }
    // Re-validate the cell the click will fill on the frame the click goes out: the ray (or the
    // world) can have moved while the swap was settling.
    if (!isWaterSourceAt(placeX,placeY,placeZ)) { abortDrainWeb(inventory,true); return; }
    const double delay=st.webSwapDelayMs<=1?0.0:st.webSwapDelayMs/1000.0;
    if (delay>0.0 && now-g_drainWebStageAt<delay) return;

    if (!g_drainWebUseOffhand && st.webSilentSwap && g_drainWebSlot!=selected) {
        selectSlot(inventory,g_drainWebSlot);
        g_drainWebSwapped=true;
    }
    const Vec3 facePoint((float)clickX+0.5f,(float)clickY+1.0f,(float)clickZ+0.5f);
    const int clickFace=anchor.face;
    bool placed=false;
    if (g_drainWebUseOffhand) {
        if (offhandItemId(playerObj)=="cobweb") {
            if (g_drainWebSilentPlace)
                placed=silentPlaceBlock(player,clickX,clickY,clickZ,clickFace,facePoint,true);
            else {
                mc::Client::setItemUseCooldown(4);
                placed=mc::Client::interactBlockAtFace(clickX,clickY,clickZ,
                    facePoint.x,facePoint.y,facePoint.z,clickFace,true);
            }
            if (placed) player.swingHand(true);
        }
    } else {
        const int nowSelected=std::clamp(mc::inv::selectedSlot(inventory),0,8);
        if (hotbarItemId(list,nowSelected)=="cobweb") {
            if (g_drainWebSilentPlace)
                placed=silentPlaceBlock(player,clickX,clickY,clickZ,clickFace,facePoint,false);
            else {
                mc::Client::setItemUseCooldown(4);
                placed=mc::Client::interactBlockAtFace(clickX,clickY,clickZ,
                    facePoint.x,facePoint.y,facePoint.z,clickFace,false);
            }
            if (placed) player.swingHand(false);
        }
    }
    if (g_drainWebSwapped && (st.webSwapBack || st.webSilentSwap || !placed))
        restoreTemporarySlot(inventory,g_drainWebOriginalSlot,true);
    g_drainWebWaiting=false; g_drainWebSwapped=false; g_drainWebUseOffhand=false;
    g_drainWebSilentPlace=false;
    g_drainWebTarget={}; g_drainWebSlot=-1;
    g_drainWebNextAt=now+(placed?0.35:0.5);
    if (!placed) notifyAction("Auto Drain: the cobweb placement was rejected by Minecraft");
}

// Auto Bucket Drain: vanilla pick-up is "use an empty bucket while looking at a water source".
// The game's own crosshair ignores fluids, so the target is found with a fluid-aware pick, and
// the pick-up itself goes through the normal Use Item path (the bucket does its own fluid ray).
void updateAutoBucketDrain() {
    if (!g_autoBucketDrain) {
        if (g_bucketWaiting) {
            mc::LocalRef inventory(mc::Player(mc::Client::player()).inventory());
            abortBucket(inventory.get(),true);
        }
        g_prevMainWater=g_prevOffWater=false; g_ownWaterScanUntil=0.0;
        return;
    }
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return;
    jobject playerObj=mc::Client::player();
    mc::LocalRef inventory(mc::Player(playerObj).inventory());
    if (gui::inputFrozen() || !playerObj || !playerCanUseModule(playerObj)) {
        abortBucket(inventory.get(),true);
        return;
    }
    mc::LocalRef list(inventory ? mc::inv::mainList(inventory.get()) : nullptr);
    if (!inventory || !list) return;
    const int selected=std::clamp(mc::inv::selectedSlot(inventory.get()),0,8);
    const std::string mainId=hotbarItemId(list.get(),selected);
    const std::string offId=offhandItemId(playerObj);
    const bool mainWater=mainId=="water_bucket";
    const bool offWater=offId=="water_bucket";
    const bool rmbTap=std::find(gui::mouseTaps().begin(),gui::mouseTaps().end(),VK_RBUTTON)!=gui::mouseTaps().end();
    // Detect a water placement either from the water bucket vanishing from a hand (the
    // click already ran this tick) or from the raw right-click while a water bucket is/was
    // held (the click has not been processed yet). Covers both frame orderings, and the
    // short scan window absorbs the client/server block-update delay before the water exists.
    const double frameNow=ImGui::GetTime();
    const bool placementHappened=(g_prevMainWater && !mainWater) || (g_prevOffWater && !offWater);
    const bool placementStarting=rmbTap && (mainWater || offWater || g_prevMainWater || g_prevOffWater);
    if (placementHappened || placementStarting) g_ownWaterScanUntil=frameNow+0.4;
    if (frameNow<g_ownWaterScanUntil) recordOwnWaterAroundCrosshair(playerObj);
    g_prevMainWater=mainWater;
    g_prevOffWater=offWater;

    // ---- Auto Drain: web mode ---------------------------------------------------------------
    if (g_autoBucketDrainSettings.mode==DrainMode::Web) {
        const bool haveWeb = mainId=="cobweb" ||
            (g_autoBucketDrainSettings.webUseOffhand && offId=="cobweb") ||
            findHotbarItem(list.get(),"cobweb")>=0;
        if (haveWeb) {
            updateWebDrain(playerObj,inventory.get(),list.get(),selected,mainId,offId);
            return;
        }
        abortDrainWeb(inventory.get(),true);
        // "Prefer Buckets If No Webs": with no cobweb anywhere, fall through to the
        // bucket draining below (which uses the bucket configuration group).
        if (!g_autoBucketDrainSettings.webPreferBuckets) return;
    }

    mc::Player player(playerObj);
    float tickDelta=mc::Client::renderTickDelta();
    if (tickDelta<0.f) tickDelta=1.f;
    mc::LocalRef hit(mc::Client::raycastWithFluids(player.blockInteractionRange(),tickDelta));
    int x=0,y=0,z=0;
    bool targetIsSource=hit && mc::hit::typeOf(hit.get())==1 && mc::hit::blockPos(hit.get(),x,y,z) &&
                        isWaterSourceAt(x,y,z);
    if (targetIsSource && g_autoBucketDrainSettings.ignoreOwnWater &&
        g_ownWaterPositions.contains(blockKey(x,y,z))) targetIsSource=false;
    if (!targetIsSource) { abortBucket(inventory.get(),true); return; }

    const double now=ImGui::GetTime();
    if (g_bucketWaiting && now-g_bucketStageAt>1.5) { abortBucket(inventory.get(),true); return; }   // watchdog
    if (!g_bucketWaiting && (now<g_bucketNextAt || mc::Client::itemUseCooldown()>0)) return;   // vanilla right-click pacing

    // Pick the source: the held slot first, then a supported off hand, then the first
    // empty bucket in the hotbar (so multiple buckets no longer make the module jump
    // between slots).
    int sourceSlot=-1;
    bool useOffhand=false;
    if (mainId=="bucket") sourceSlot=selected;
    else if (g_autoBucketDrainSettings.useOffhand && offId=="bucket") useOffhand=true;
    else sourceSlot=findHotbarItem(list.get(),"bucket");      // EMPTY bucket picks water up
    if (!useOffhand && sourceSlot<0) {
        rateLimitedNotify(g_bucketNoticeAt,"Auto Drain needs an empty bucket in the hotbar or off hand",6.0);
        return;
    }

    if (!g_bucketWaiting) {
        g_bucketOriginalSlot=selected;
        g_bucketSlot=sourceSlot;
        g_bucketUseOffhand=useOffhand;
        g_bucketTarget={x,y,z,true};
        g_bucketStageAt=now;
        g_bucketWaiting=true;
        g_bucketSwapped=false;
        if (!useOffhand && !g_autoBucketDrainSettings.silentSwap && sourceSlot!=selected) {
            selectSlot(inventory.get(),sourceSlot);
            g_bucketSwapped=true;
        }
        if (g_autoBucketDrainSettings.swapDelayMs>1) return;
    }
    if (!g_bucketTarget.valid || x!=g_bucketTarget.x || y!=g_bucketTarget.y || z!=g_bucketTarget.z) {
        abortBucket(inventory.get(),true);
        return;
    }
    const double delay=g_autoBucketDrainSettings.swapDelayMs<=1?0.0:g_autoBucketDrainSettings.swapDelayMs/1000.0;
    if (delay>0.0 && now-g_bucketStageAt<delay) return;

    // Silent swap: switch, use and switch back inside this single frame, so the hand never
    // visibly leaves the original item. Only vanilla slot-sync packets are involved.
    if (!g_bucketUseOffhand && g_autoBucketDrainSettings.silentSwap && g_bucketSlot!=selected) {
        selectSlot(inventory.get(),g_bucketSlot);
        g_bucketSwapped=true;
    }
    bool collected=false;
    if (g_bucketUseOffhand) {
        // Use the off hand directly: nothing is swapped and nothing enters the hotbar.
        if (offhandItemId(playerObj)=="bucket") {
            mc::Client::setItemUseCooldown(4);
            collected=mc::Client::interactItem(true);
            if (collected) player.swingHand(true);
        }
    } else {
        const int nowSelected=std::clamp(mc::inv::selectedSlot(inventory.get()),0,8);
        if (hotbarItemId(list.get(),nowSelected)=="bucket") {
            mc::Client::setItemUseCooldown(4);
            collected=mc::Client::interactItem(false);
            if (collected) player.swingHand(false);
        }
    }
    if (g_bucketSwapped && (g_autoBucketDrainSettings.swapBack || g_autoBucketDrainSettings.silentSwap || !collected))
        restoreTemporarySlot(inventory.get(),g_bucketOriginalSlot,true);
    g_bucketWaiting=false; g_bucketSwapped=false; g_bucketUseOffhand=false; g_bucketTarget={}; g_bucketSlot=-1;
    g_bucketNextAt=now+(collected?0.35:0.8);
    if (collected) g_bucketFails=0;
    else if (++g_bucketFails>=2) {
        // Same recovery as switching the module off and on: forget all pending state so a stuck
        // attempt cannot block the next one.
        g_bucketFails=0; g_bucketNextAt=0.0; g_ownWaterPositions.clear();
        abortBucket(inventory.get(),true);
        notifyAction("Auto Drain: reset after repeated rejections");
    }
}

void abortWeb(jobject inventory,bool restore) {
    if (g_webWaiting && g_webSwapped && restore) restoreTemporarySlot(inventory,g_webOriginalSlot,true);
    g_webWaiting=false; g_webSwapped=false; g_webUseOffhand=false; g_webTarget={}; g_webSlot=-1;
    g_webSwapAt=-1.0;
}

// ---- Pressure Plate Assist ------------------------------------------------------------------
// Aim assistance aimed at pressure plates. The plate is found by scanning the cells around the eyes
// (a plate is a block, so there is no entity list to read), the scan runs five times a second rather
// than every frame, and the view is walked onto the best plate the same way AimAssist walks onto a
// target - one shared approach step, so both features feel identical.
double g_plateScanAt = 0.0;
bool   g_plateHave = false;
int    g_plateX = 0, g_plateY = 0, g_plateZ = 0;

bool plateBlockId(const std::string& id) { return id.find("pressure_plate") != std::string::npos; }

// The plate inside the cone and range that is closest to the crosshair, or false. "Only If NO Web"
// skips a plate that already has a web above it: another web there is a plate that is already doing
// its job, and pulling the view onto it is what made the assist fight the player.
bool findPlateForAssist(mc::Player& player, const AutoPressurePlateWebSettings& st) {
    const Vec3 eye = player.eyePos();
    const float range = std::clamp(st.assistRange, 1.f, 6.f);
    const float halfFov = std::clamp(st.assistFov, 10.f, 360.f) * 0.5f;
    const float yawRad = player.yaw() * 0.017453292519943295f;
    const float pitchRad = player.pitch() * 0.017453292519943295f;
    const Vec3 look(-std::sin(yawRad) * std::cos(pitchRad), -std::sin(pitchRad),
                    std::cos(yawRad) * std::cos(pitchRad));
    const int r = (int)std::ceil(range);
    const int ex = (int)std::floor(eye.x), ey = (int)std::floor(eye.y), ez = (int)std::floor(eye.z);
    bool found = false;
    float bestDot = -1.f;
    for (int dx = -r; dx <= r; ++dx)
    for (int dy = -2; dy <= 2; ++dy)
    for (int dz = -r; dz <= r; ++dz) {
        const int bx = ex + dx, by = ey + dy, bz = ez + dz;
        if (!plateBlockId(mc::Client::blockIdAt(bx, by, bz))) continue;
        if (st.onlyIfNoWeb && mc::Client::blockIdAt(bx, by + 1, bz) == "cobweb") continue;
        const Vec3 centre((float)bx + 0.5f, (float)by + 0.0625f, (float)bz + 0.5f);
        const Vec3 d = centre - eye;
        const float len = d.len();
        if (len < 0.4f || len > range) continue;
        const float dot = d.dot(look) / len;
        if (dot <= 0.f) continue;
        if (std::acos(std::clamp(dot, -1.f, 1.f)) * 57.29577951f > halfFov) continue;
        if (dot <= bestDot) continue;
        bestDot = dot;
        g_plateX = bx; g_plateY = by; g_plateZ = bz;
        found = true;
    }
    return found;
}

void updatePlateAssist() {
    const AutoPressurePlateWebSettings& st = g_autoPressurePlateWebSettings;
    const bool ready = jni::ready() && mc::frame::valid() && mc::frame::resolved();
    jobject playerObj = ready ? mc::Client::player() : nullptr;
    // It is a setting of Auto Pressure Plate Web, so it only runs with that card on.
    if (!g_autoPressurePlateWeb || !st.plateAssist || !ready || !playerObj ||
        gui::inputFrozen() || mc::Client::screenOpen()) {
        g_plateHave = false;
        return;
    }
    const double now = ImGui::GetTime();
    mc::Player player(playerObj);
    // The scan costs one block lookup per cell, so it is refreshed five times a second and the last
    // plate is kept between scans. Moving out of its range invalidates it immediately.
    if (!g_plateHave || now - g_plateScanAt >= 0.2) {
        g_plateScanAt = now;
        g_plateHave = findPlateForAssist(player, st);
    }
    if (g_plateHave) {
        const Vec3 centre((float)g_plateX + 0.5f, (float)g_plateY + 0.0625f, (float)g_plateZ + 0.5f);
        if ((centre - player.eyePos()).len() > std::clamp(st.assistRange, 1.f, 6.f)) g_plateHave = false;
        else if (!plateBlockId(mc::Client::blockIdAt(g_plateX, g_plateY, g_plateZ))) g_plateHave = false;
    }
    if (!g_plateHave) return;
    const Vec3 centre((float)g_plateX + 0.5f, (float)g_plateY + 0.0625f, (float)g_plateZ + 0.5f);
    const Vec3 d = centre - player.eyePos();
    const float flat = std::sqrt(d.x * d.x + d.z * d.z);
    const float wantYaw = std::atan2(-d.x, d.z) * 57.29577951f;
    const float wantPitch = st.assistVertical ? -std::atan2(d.y, std::max(flat, 0.001f)) * 57.29577951f
                                             : player.pitch();
    // Speed 1-30 maps onto the same 0-10 sensitivity AimAssist uses, so the two cards pace the turn
    // identically and neither needs its own rule for how fast is fast.
    const float t = aim::approachStep(std::clamp(st.assistSpeed * 0.33f, 0.f, 10.f), ImGui::GetIO().DeltaTime);
    player.setYaw(approachDeg(player.yaw(), wantYaw, t));
    player.setPitch(player.pitch() + (wantPitch - player.pitch()) * t);
    mc::ext::sendLookPacket(player.yaw(), player.pitch(), player.onGround());
}

void updatePressurePlateWeb() {
    if (!g_autoPressurePlateWeb) {
        if (g_webWaiting) {
            mc::LocalRef inventory(mc::Player(mc::Client::player()).inventory());
            abortWeb(inventory.get(),true);
        }
        return;
    }
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return;
    jobject playerObj=mc::Client::player();
    mc::LocalRef inventory(mc::Player(playerObj).inventory());
    if (gui::inputFrozen() || !playerObj || !playerCanUseModule(playerObj)) {
        abortWeb(inventory.get(),true);
        return;
    }
    mc::LocalRef list(inventory ? mc::inv::mainList(inventory.get()) : nullptr);
    if (!inventory || !list) return;

    mc::LocalRef hit(mc::Client::crosshair());     // block outline pick: pressure plates are hit
    int x=0,y=0,z=0;
    bool targetIsPlate=hit && mc::hit::typeOf(hit.get())==1 &&
        mc::hit::blockPos(hit.get(),x,y,z) && mc::Client::isPressurePlateAt(x,y,z);
    if (targetIsPlate) {
        const std::string above=mc::Client::blockIdAt(x,y+1,z);
        targetIsPlate = isAirId(above) || above=="water";      // somewhere a cobweb can actually go
    }
    if (!targetIsPlate && g_webWaiting && g_webTarget.valid) {
        // The crosshair can leave the plate for a frame while the module swaps slots or
        // finishes the click, and the old code dropped the placement right there (the
        // "sometimes it just stops" report). Hold on to the plate we started on for a
        // short grace window instead, then keep placing at those coordinates.
        const double now=ImGui::GetTime();
        // Kept short: a long grace window made the web wait out the plate on high ping.
        if (now-g_webStageAt<=0.25) { x=g_webTarget.x; y=g_webTarget.y; z=g_webTarget.z; targetIsPlate=true; }
    }
    if (!targetIsPlate) { abortWeb(inventory.get(),true); return; }

    const double now=ImGui::GetTime();
    if (!g_webWaiting && (now<g_webNextAt || mc::Client::itemUseCooldown()>0)) return;
    if (g_webWaiting && now-g_webStageAt>2.5) { abortWeb(inventory.get(),true); return; }   // watchdog (high ping safe)
    const int selected=std::clamp(mc::inv::selectedSlot(inventory.get()),0,8);
    const std::string mainId=hotbarItemId(list.get(),selected);
    const std::string offId=offhandItemId(playerObj);
    if (!g_webWaiting) {
        int sourceSlot=-1;
        bool useOffhand=false;
        if (mainId=="cobweb") sourceSlot=selected;
        else if (g_autoPressurePlateWebSettings.useOffhand && offId=="cobweb") useOffhand=true;
        else sourceSlot=findHotbarItem(list.get(),"cobweb");
        if (!useOffhand && sourceSlot<0) {
            rateLimitedNotify(g_webNoticeAt,"Auto Pressure Plate Web needs a cobweb in the hotbar or off hand",6.0);
            return;
        }
        g_webOriginalSlot=selected;
        g_webSlot=sourceSlot;
        g_webUseOffhand=useOffhand;
        g_webTarget={x,y,z,true};
        g_webStageAt=now;
        g_webWaiting=true;
        g_webSwapped=false;
        if (!useOffhand && !g_autoPressurePlateWebSettings.silentSwap && sourceSlot!=selected) {
            selectSlot(inventory.get(),sourceSlot);
            g_webSwapped=true;
            g_webSwapAt=now;
        }
        // A delay of 0-1 ms places in this same frame instead of waiting for the next one.
        if (std::min(g_autoPressurePlateWebSettings.swapDelayMs,50)>1) return;
    }
    if (!g_webTarget.valid || x!=g_webTarget.x || y!=g_webTarget.y || z!=g_webTarget.z) {
        abortWeb(inventory.get(),true);
        return;
    }
    // Capped at 50 ms whatever the saved slider says: an older config kept a 70 ms swap delay, which
    // is the wait the high-ping report describes. Placement must follow the plate quickly.
    const int webDelayMs=std::min(g_autoPressurePlateWebSettings.swapDelayMs,50);
    const double delay=webDelayMs<=1?0.0:webDelayMs/1000.0;
    if (delay>0.0 && now-g_webStageAt<delay) return;
    if (!g_webUseOffhand && g_autoPressurePlateWebSettings.silentSwap && g_webSlot!=selected) {
        selectSlot(inventory.get(),g_webSlot);
        g_webSwapped=true;
        g_webSwapAt=now;
    }
    // Wait for the hotbar swap's own tick before the interaction. Doing both in one frame sends the
    // cobweb with the old slot still selected on the server, so the placement was rejected and the
    // player had to wait out the retry - the reported high-ping delay.
    if (!xbow::slotSettled(now,g_webSwapAt)) return;
    bool placed=false;
    // Ordinary "use item on block" against the plate's TOP face, so the cobweb is placed
    // in the block above it no matter which side of the plate the crosshair is on.
    const double faceX=(double)x+0.5, faceY=(double)y+0.0625, faceZ=(double)z+0.5;
    if (g_webUseOffhand) {
        if (offhandItemId(playerObj)=="cobweb") {
            mc::Client::setItemUseCooldown(4);
            placed=mc::Client::interactBlockAt(x,y,z,faceX,faceY,faceZ,true);
            if (placed) mc::Player(playerObj).swingHand(true);
        }
    } else {
        const int nowSelected=std::clamp(mc::inv::selectedSlot(inventory.get()),0,8);
        if (hotbarItemId(list.get(),nowSelected)=="cobweb") {
            mc::Client::setItemUseCooldown(4);
            placed=mc::Client::interactBlockAt(x,y,z,faceX,faceY,faceZ,false);
            if (placed) mc::Player(playerObj).swingHand(false);
        }
    }
    if (g_webSwapped && (g_autoPressurePlateWebSettings.swapBack || g_autoPressurePlateWebSettings.silentSwap || !placed))
        restoreTemporarySlot(inventory.get(),g_webOriginalSlot,true);
    g_webWaiting=false; g_webSwapped=false; g_webUseOffhand=false; g_webTarget={}; g_webSlot=-1; g_webSwapAt=-1.0;
    // Vanilla's own use-item cooldown is four ticks (0.2 s); waiting longer than that after a
    // successful placement is what made the module feel slower than placing by hand on a high-ping
    // server, where the block update already arrives late. The throttle is now the game's own 0.2 s;
    // a rejection is retried at 0.35 s so a refused click cannot become a packet loop.
    g_webNextAt=now+(placed?0.20:0.35);
    if (!placed) notifyAction("Auto Pressure Plate Web: placement was rejected by Minecraft");
}

// Distance from `eye` to the closest point of the target's (interpolated) hitbox. This is how
// vanilla measures entity interaction range, so it matches what the server will accept.
float distanceToHitbox(const Vec3& eye,const mc::Entity& target,float tickDelta) {
    const mc::Entity::Bounds b=target.interpolatedBounds(tickDelta);
    if (!b.valid) return 1e9f;
    const float dx=std::max({b.min.x-eye.x,0.f,eye.x-b.max.x});
    const float dy=std::max({b.min.y-eye.y,0.f,eye.y-b.max.y});
    const float dz=std::max({b.min.z-eye.z,0.f,eye.z-b.max.z});
    return std::sqrt(dx*dx+dy*dy+dz*dz);
}

mc::LocalRef targetPlayerForShield(jobject playerObj,float tickDelta) {
    JNIEnv* env=jni::env();
    if (!env||!playerObj) return {};
    mc::Player player(playerObj);
    const float range=(float)player.entityInteractionRange();
    const Vec3 eye=player.eyePos();
    if (g_autoShieldBreakSettings.onlyWhenLooking) {
        // Crosshair + look vector: the game's own entity pick already honours the interaction range.
        mc::LocalRef hit(mc::Client::crosshair());
        if (!hit||mc::hit::typeOf(hit.get())!=2) return {};
        mc::LocalRef target(mc::hit::entity(hit.get()));
        if (!target||!isPlayerEntity(env,target.get())) return {};
        mc::Entity candidate(target.get());
        if (!candidate.alive()||distanceToHitbox(eye,candidate,tickDelta)>range+0.05f) return {};
        if (entityBlocked(target.get(),true)) return {};   // friends / Anti-Bot
        return target;
    }

    // Otherwise: the closest player inside a 90 degree view cone and interaction range.
    jobject world=mc::Client::world();
    if (!world) return {};
    const float yaw=mc::cam::yaw(mc::Client::camera())*0.017453292519943295f;
    const float pitch=mc::cam::pitch(mc::Client::camera())*0.017453292519943295f;
    const Vec3 look(-std::sin(yaw)*std::cos(pitch),-std::sin(pitch),std::cos(yaw)*std::cos(pitch));
    constexpr float minDot=0.70710678f;
    float nearest=range+0.05f;
    mc::LocalRef best;
    for (jobject entity:mc::collectEntities(world,256)) {
        if (!entity) continue;
        const bool isPlayer=isPlayerEntity(env,entity);
        if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(entity); continue; }
        if (!isPlayer||env->IsSameObject(entity,playerObj)==JNI_TRUE) { env->DeleteLocalRef(entity); continue; }
        if (entityBlocked(entity,true)) { env->DeleteLocalRef(entity); continue; }   // friends / Anti-Bot
        mc::LivingEntity target(entity);
        const Vec3 center=target.pos()+Vec3(0.f,target.height()*0.5f,0.f);
        const Vec3 delta=center-eye;
        const float centerDistance=delta.len();
        const float boxDistance=distanceToHitbox(eye,target,tickDelta);
        if (!target.alive()||centerDistance<=0.01f||boxDistance>nearest||
            delta.dot(look)/centerDistance<minDot) { env->DeleteLocalRef(entity); continue; }
        best=mc::LocalRef(env->NewLocalRef(entity));
        nearest=boxDistance;
        env->DeleteLocalRef(entity);
    }
    return best;
}

// Find the entity a module captured earlier (Auto Shield Break, AutoMace and Pearl Agro all keep a
// trackedTargetKey()). A UUID key matches by identity, so a plugin renaming the target mid-sequence
// cannot make the module lose it; a "name:" key is the fallback used when no UUID was readable at
// capture time.
mc::LocalRef findPlayerByUuid(const std::string& targetKey) {
    if (targetKey.empty()||!jni::env()||!mc::Client::world()) return {};
    JNIEnv* env=jni::env();
    const bool byName=targetKey.rfind("name:",0)==0;
    const std::string want=byName?targetKey.substr(5):det::uuidKey(targetKey);
    if (want.empty()) return {};
    mc::LocalRef result;
    for (jobject entity:mc::collectEntities(mc::Client::world(),kEntityScan)) {
        if (!entity) continue;
        const bool match=byName
            ? det::lowerCopy(det::cleanPlayerName(mc::Entity(entity).displayName()))==want
            : det::uuidKey(mc::Entity(entity).uuid())==want;
        if (match&&!result) result=mc::LocalRef(env->NewLocalRef(entity));
        env->DeleteLocalRef(entity);
    }
    return result;
}

// ---- Stun Web ------------------------------------------------------------------------------
// A follow-up to Auto Shield Break. The moment its axe hit lands on a raised shield the target is
// remembered; a short beat later a second click goes out (the "double click" the shield-stun
// window wants) and then the view is turned onto the cell under the target's feet, a cobweb is
// swapped into the hand and dropped there - so the upward motion the stun gives them is caught in
// a web instead of a launch. Nothing is placed unless the cell is inside block reach.
std::string g_stunWebTarget;
double g_stunWebArmedAt = -100.0;
double g_stunWebSwapAt = -1.0;
int g_stunWebPhase = 0;        // 0 = idle, 1 = second click, 2 = the cobweb place delay, 3 = place the web
int g_stunWebOrigSlot = -1;
bool g_stunWebSwapped = false;
double g_stunWebPhaseAt = 0.0;  // when the place-delay phase began
// Silent Place is a network action, so a rejected click is retried on a later frame instead of a
// second identical click in the same frame (which the server would reject the same way).
double g_stunWebRetryAt = -100.0;
int    g_stunWebAttempts = 0;
// Look Back: the direction the player was facing the moment the stun landed, so the view can be
// handed back to it once the web is down.
float g_stunWebBackYaw = 0.f, g_stunWebBackPitch = 0.f;
bool g_stunWebHaveBack = false;
// When the attempt began. The stall watchdog runs off g_stunWebArmedAt, which every step of real
// progress refreshes (the swap, the turn onto the cell); this is the hard deadline for the whole
// attempt, so a Smooth walk that never ends still gives the hotbar slot back.
double g_stunWebStartedAt = -100.0;
// The axe target Auto Shield Break last hit, waiting for its shield to actually drop. The web
// only arms on a real stun: the target's own blocking state going false after the axe hit. A
// swing the server shrugged off never gets a web.
std::string g_stunWebPendingTarget;
double g_stunWebPendingAt = -100.0;
constexpr double kStunWebStunWindow = 1.5;   // seconds to wait for the shield to drop

// Stun Web and AutoWeb share one follow-up; the two cards differ only in where their settings
// live. A sequence belongs to the card that armed it (AutoWeb wins when both are on, since it is
// the newer experiment), so the two never fight over the hotbar.
bool g_stunWebOwnerAuto = false;

void stunWebReset(jobject inventory, bool restore) {
    if (restore && g_stunWebSwapped && inventory && g_stunWebOrigSlot >= 0 && g_stunWebOrigSlot <= 8)
        restoreTemporarySlot(inventory, g_stunWebOrigSlot, true);
    g_stunWebTarget.clear();
    g_stunWebPhase = 0;
    g_stunWebArmedAt = -100.0;
    g_stunWebSwapAt = -1.0;
    g_stunWebOrigSlot = -1;
    g_stunWebSwapped = false;
    g_stunWebHaveBack = false;
    g_stunWebStartedAt = -100.0;
    g_stunWebPhaseAt = 0.0;
    g_stunWebRetryAt = -100.0;
    g_stunWebAttempts = 0;
    g_stunWebOwnerAuto = false;
}

void armStunWeb(const std::string& targetKey, double now) {
    if (targetKey.empty()) return;
    g_stunWebTarget = targetKey;
    g_stunWebPhase = 1;
    g_stunWebArmedAt = now;
    g_stunWebStartedAt = now;
    g_stunWebRetryAt = -100.0;
    g_stunWebAttempts = 0;
    // Remember where the view was pointing before any of this: Look Back returns to it.
    jobject self = mc::Client::player();
    if (self) {
        mc::Player me(self);
        g_stunWebBackYaw = me.yaw();
        g_stunWebBackPitch = me.pitch();
        g_stunWebHaveBack = true;
    } else {
        g_stunWebHaveBack = false;
    }
}

// The first open cell at or under feetY in this column that has a real block straight under it -
// the web is lit on that block's top face, so the web lands in the open cell above it. False when
// the column has nothing to place against: the cell is taken, or it is already a web (which means
// the catch has already happened there).
bool stunWebGroundInColumn(int bx, int bz, int feetY, int& webY, int& groundY) {
    webY = feetY;
    groundY = feetY - 1;
    for (int step = 0; step < 5; ++step, --webY) {
        if (mc::Client::isCobwebAt(bx, webY, bz)) return false;          // already webbed
        const std::string cellId = mc::Client::blockIdAt(bx, webY, bz);
        if (!isAirId(cellId)) return false;                              // the cell is taken
        const std::string underId = mc::Client::blockIdAt(bx, webY - 1, bz);
        if (!isAirId(underId) && underId != "water") { groundY = webY - 1; return true; }
    }
    return false;
}

void updateStunWeb() {
    // A real stun only: the target Auto Shield Break hit must stop blocking - that is the
    // server telling us the shield was disabled. Only then does the web arm; a target still
    // holding the shield when the window expires is left alone.
    if (!g_stunWebPendingTarget.empty()) {
        const double now = ImGui::GetTime();
        if (!g_stunWeb || now - g_stunWebPendingAt > kStunWebStunWindow) {
            g_stunWebPendingTarget.clear();
        } else {
            mc::LocalRef watched = findPlayerByUuid(g_stunWebPendingTarget);
            if (!watched || !mc::LivingEntity(watched.get()).alive()) {
                g_stunWebPendingTarget.clear();
            } else if (!mc::LivingEntity(watched.get()).blocking()) {
                armStunWeb(g_stunWebPendingTarget, now);
                g_stunWebPendingTarget.clear();
            }
        }
    }
    if (!g_stunWeb || g_stunWebPhase == 0) {
        if (g_stunWebPhase != 0) {
            mc::LocalRef inventory(mc::Player(mc::Client::player()).inventory());
            stunWebReset(inventory.get(), true);
        }
        return;
    }
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) { stunWebReset(nullptr, false); return; }
    jobject playerObj = mc::Client::player();
    mc::LocalRef inventory(playerObj ? mc::Player(playerObj).inventory() : nullptr);
    mc::LocalRef list(inventory ? mc::inv::mainList(inventory.get()) : nullptr);
    if (!playerObj || !inventory || !list) { stunWebReset(nullptr, false); return; }
    if (gui::inputFrozen() || !playerCanUseModule(playerObj)) { stunWebReset(inventory.get(), true); return; }
    const double now = ImGui::GetTime();
    if (now - g_stunWebArmedAt > 1.4 || now - g_stunWebStartedAt > 8.0) {
        stunWebReset(inventory.get(), true); return;   // stalled (or still turning for too long)
    }
    mc::LocalRef target = findPlayerByUuid(g_stunWebTarget);
    if (!target) { stunWebReset(inventory.get(), true); return; }
    mc::LivingEntity living(target.get());
    if (!living.alive()) { stunWebReset(inventory.get(), true); return; }
    mc::Player player(playerObj);
    // The armed card's own Stun Web section drives the placement settings.
    StunWebSettings& web = g_stunWebSettings;

    const Vec3 feet = mc::Entity(target.get()).pos();
    const int bx = (int)std::floor(feet.x), bz = (int)std::floor(feet.z);
    const int feetY = (int)std::floor(feet.y);
    // Stun Web only runs while the target is not inside a cobweb already. A web around the body (the
    // cell their feet are in, or the one their head is in) means the catch has happened: another web
    // changes nothing, so the whole sequence stands down instead of burning a cobweb.
    if (mc::Client::isCobwebAt(bx, feetY, bz) ||
        mc::Client::isCobwebAt(bx, feetY + 1, bz)) {
        stunWebReset(inventory.get(), true);
        return;
    }
    // The same goes for the player running it: standing in a web yourself, the follow-up has nothing
    // to add, and the web that would go down is the one you are standing in.
    const Vec3 selfPos = player.pos();
    if (mc::Client::isCobwebAt((int)std::floor(selfPos.x), (int)std::floor(selfPos.y), (int)std::floor(selfPos.z))) {
        stunWebReset(inventory.get(), true);
        return;
    }

    if (g_stunWebPhase == 1) {
        if (now - g_stunWebArmedAt < 0.06) return;
        // The double click the shield-stun window wants, sent on the beat. It used to wait for the
        // vanilla miss time first, which is always still running 0.06 s after the first hit - so the
        // promised second click was dropped every time and the stun never got its double tap. The
        // click is only skipped when the target has moved out of entity reach, and the web still
        // goes down either way.
        const float tickDelta = mc::Client::renderTickDelta();
        if (distanceToHitbox(player.eyePos(), mc::Entity(target.get()), tickDelta) <=
            (float)player.entityInteractionRange() + 0.05f) {
            if (mc::Client::attackEntity(target.get())) {
                player.swingHand();
                g_attackSentThisFrame = true;
            }
        }
        g_stunWebPhase = 2;
        g_stunWebPhaseAt = now;
        return;
    }

    // Phase 2: Cobweb Place Delay - a straight wait between the stun and the placement, so the
    // web can be timed to go down as the knockback carries the target.
    if (g_stunWebPhase == 2) {
        if (now - g_stunWebPhaseAt < std::clamp(web.placeDelayMs, 0, 600) / 1000.0) {
            g_stunWebArmedAt = now;                      // waiting out the delay is progress
            return;
        }
        g_stunWebPhase = 3;
        return;
    }

    // Phase 3: the cell under their feet. The cell under their feet is air while
    // they are moving - which is the moment this module runs - and a click on air is a placement
    // the server refuses, so the first open cell with a real block straight under it is what gets
    // lit. When that column has nothing to place against (a web is already there, or the cell is
    // taken) the attempt stands down rather than burning a cobweb.
    const int placeX = bx, placeZ = bz, placeFeetY = feetY;
    int webY = 0, groundY = 0;
    if (!stunWebGroundInColumn(placeX, placeZ, placeFeetY, webY, groundY)) {
        stunWebReset(inventory.get(), true);
        return;
    }
    // Never web the player's own cell. At shield-stun range the target's column can be the one
    // the player is standing in, and a web in it catches the player instead of the target - the
    // attempt stands down and waits for a real angle on the next stun. selfPos was already read
    // above for the "standing in a web" stand-down.
    const int selfX = (int)std::floor(selfPos.x), selfZ = (int)std::floor(selfPos.z);
    const int selfFeetY = (int)std::floor(selfPos.y);
    if (placeX == selfX && placeZ == selfZ && (webY == selfFeetY || webY == selfFeetY + 1)) {
        stunWebReset(inventory.get(), true);
        return;
    }
    // Phase 3 continues: only from block reach, and then the view must be on the block before the
    // web goes out.
    const Vec3 aimPoint((float)placeX + 0.5f, (float)webY, (float)placeZ + 0.5f);
    if ((aimPoint - player.eyePos()).len() > (float)player.blockInteractionRange() + 0.05f) {
        stunWebReset(inventory.get(), true);
        return;
    }
    const int selected = std::clamp(mc::inv::selectedSlot(inventory.get()), 0, 8);
    if (g_stunWebOrigSlot < 0) g_stunWebOrigSlot = selected;
    const int slot = hotbarItemId(list.get(), selected) == "cobweb" ? selected : findHotbarItem(list.get(), "cobweb");
    if (slot < 0) {
        notifyAction("Stun Web needs a cobweb in the hotbar");
        stunWebReset(inventory.get(), true);
        return;
    }
    if (selected != slot) {
        selectSlot(inventory.get(), slot);
        g_stunWebSwapped = true;
        g_stunWebSwapAt = now;
        g_stunWebArmedAt = now;          // the watchdog keeps running through the swap
        return;
    }
    if (!xbow::slotSettled(now, g_stunWebSwapAt)) { g_stunWebArmedAt = now; return; }
    bool placed = false;
    if (web.silentPlace) {
        // Silent Place rides the same one-shot Silent Aim pipeline as the silent throws: aim at the
        // ground block's TOP face (the cell the web goes into), send the aim ahead of the interaction
        // and restore the real view in the same frame. It deliberately does not depend on where the
        // camera is pointing, so a player looking straight ahead - not down - still gets the web.
        if (now < g_stunWebRetryAt) return;
        if (g_stunWebAttempts >= 2) {
            notifyAction("Stun Web: the cobweb placement was rejected");
            stunWebReset(inventory.get(), true);
            return;
        }
        placed = silentPlaceBlock(player, placeX, groundY, placeZ, 1,
                                  Vec3((float)placeX + 0.5f, (float)webY, (float)placeZ + 0.5f), false);
        if (!placed) {
            // The world (or the server's slot state) can still be settling on the frame of the swap.
            // Retry on a later frame, after the ordinary use-item cooldown, rather than sending the
            // same click twice in this one - a same-frame retry is rejected for the same reason.
            ++g_stunWebAttempts;
            g_stunWebRetryAt = now + 0.1;
            g_stunWebArmedAt = now;
            return;
        }
    } else {
        const Vec3 toAim = aimPoint - player.eyePos();
        const float flat = std::sqrt(toAim.x * toAim.x + toAim.z * toAim.z);
        const float aimYaw = std::atan2(-toAim.x, toAim.z) * 57.29577951f;
        const float aimPitch = -std::atan2(toAim.y, flat) * 57.29577951f;
        if (!aimViewForPlacement(player, aimYaw, aimPitch, web.look, 1.5f,
                                 web.cameraSensitivity, false)) {
            // Still turning onto the cell: real progress, so the stall watchdog waits for it and only
            // the attempt's own hard deadline can end the sequence mid-walk.
            g_stunWebArmedAt = now;
            return;
        }
        mc::Client::setItemUseCooldown(4);
        placed = mc::Client::interactBlockAtFace(placeX, groundY, placeZ,
                                                 (double)placeX + 0.5, (double)webY, (double)placeZ + 0.5, 1, false);
    }
    if (placed) player.swingHand();
    else notifyAction("Stun Web: the cobweb placement was rejected");
    // Look Back: the web is down, so the view the player held before the stun is walked back to -
    // the shared Smooth Look Back, not a snap. Silent Place never moved the camera, so there is
    // nothing to put back.
    if (web.lookBack && !web.silentPlace && g_stunWebHaveBack)
        startLookBack(g_stunWebBackYaw, g_stunWebBackPitch, web.cameraSensitivity);
    g_stunWebHaveBack = false;
    stunWebReset(inventory.get(), true);
}

// AutoWeb's own follow-up, driven by a hit of the player's own - a left click, or a swing another
// module sent this frame - on a player standing right against a solid block. A cobweb then goes
// into the two cells against that block: the cell at the block's own level, which is the cell the
// target stands in, and the cell one above it. It needs a cobweb in the hotbar and block reach,
// exactly like the stun web, and stands down while a stun-web attempt is using the hotbar. The
// geometry itself lives in head_web.h, where test/auto_web_check.mjs pins it.
int g_headWebWallX = 0, g_headWebWallY = 0, g_headWebWallZ = 0, g_headWebFace = 1;
int g_headWebLowX = 0, g_headWebLowY = 0, g_headWebLowZ = 0;
int g_headWebHighX = 0, g_headWebHighY = 0, g_headWebHighZ = 0;
bool g_headWebHighOnWall = false;
int g_headWebPhase = 0;        // 0 = idle, 1 = hold the cobweb, 2 = place the low web, 3 = place the high web
int g_headWebOrigSlot = -1;
bool g_headWebSwapped = false;
double g_headWebStartedAt = -100.0;
double g_headWebSwapAt = -1.0;
double g_headWebStepAt = -1.0;
double g_headWebNextAt = 0.0;
double g_headWebNoticeAt = -100.0;

void headWebReset(jobject inventory, bool restore) {
    if (restore && g_headWebSwapped && inventory && g_headWebOrigSlot >= 0 && g_headWebOrigSlot <= 8)
        restoreTemporarySlot(inventory, g_headWebOrigSlot, true);
    g_headWebPhase = 0;
    g_headWebOrigSlot = -1;
    g_headWebSwapped = false;
    g_headWebSwapAt = -1.0;
    g_headWebStepAt = -1.0;
    g_headWebHighOnWall = false;
}


void clearShieldPending(jobject inventory,bool restore) {
    if (restore && g_shieldWaiting) restoreTemporarySlot(inventory,g_shieldOriginalSlot,true);
    g_shieldWaiting=false;
    g_shieldTargetUuid.clear();
    g_shieldOriginalSlot=-1;
    g_shieldAxeSlot=-1;
}


bool isLungeSpear(jobject stack) {
    if (!stack) return false;
    const std::string id=mc::stk::itemId(stack);
    return id.find("spear")!=std::string::npos && mc::stk::lungeLevel(stack)>0;
}

// Silent swap OFF : visibly select the spear, wait until the spear's own attack charge (its attack
//                   speed) is full, jab, then swap back. Hold-to-loop repeats that cycle.
// Silent swap ON  : select, jab and restore inside one frame so the hand never visibly moves.
enum class LungePhase { Idle, Charging, Back };
LungePhase g_lungePhase=LungePhase::Idle;
bool g_lungeWant=false;
int g_lungeOrigSlot=-1, g_lungeSpearSlot=-1;
double g_lungeStageAt=0.0, g_lungeNextAt=0.0, g_lungeWantAt=0.0, g_lungeBackAt=0.0, g_lungeSwapAt=0.0;

// One game tick between the slot change and the jab (xbow::slotSettled, the same tested rule the
// other swap-then-act modules use). Frames run faster than ticks, so "wait for the next frame"
// could still put the slot change and the attack in the same tick - and a jab sent in the same tick
// as the swap is the ghost lunge: the server still has the old item in hand, the lunge never happens
// on its side and the swing comes back as a phantom. A full tick between them makes the server
// resolve the jab with the spear actually equipped.

// A lunge that cannot go out (busy hand, the game's own miss delay never clearing) must not leave the
// player holding the spear. Past this wait the original item is handed straight back - the reported
// "it stays on the spear, then lunges and swaps back".
constexpr double kLungeTimeout = 0.75;

// Swap Back Delay: the slot is restored this long AFTER the jab instead of in the same frame, so the
// attack the server received was still made with the spear in hand. A same-frame restore is the swap
// the server never saw, which is why the lunge did not go out at all.

// Restore the exact slot the lunge session swapped FROM. g_lungeOrigSlot is kept
// for the whole session (cleared only when the macro stops) so a hold-to-loop can
// never leave the player stuck on the spear when the original slot was forgotten.
void lungeFinish(jobject inventory,bool restore) {
    if (restore && inventory && g_lungeOrigSlot>=0 && g_lungeOrigSlot!=g_lungeSpearSlot)
        selectSlot(inventory,g_lungeOrigSlot);
    g_lungePhase=LungePhase::Idle;
    g_lungeSpearSlot=-1;
}



// ---- Fast Place ---------------------------------------------------------------------------
bool isThrowableId(const std::string& id) {
    static const char* const throwables[]={"experience_bottle","splash_potion","lingering_potion","snowball",
        "egg","ender_pearl","wind_charge","blue_egg","brown_egg"};
    for (const char* t:throwables) if (id==t) return true;
    return false;
}

void updateAutoShieldBreak() {
    if (!g_autoShieldBreak) {
        if (g_shieldWaiting) {
            mc::LocalRef inventory(mc::Player(mc::Client::player()).inventory());
            clearShieldPending(inventory.get(),true);
        }
        return;
    }
    if (g_attackSentThisFrame) return;
    if (!jni::ready()||!mc::frame::valid()||!mc::frame::resolved()) return;
    jobject playerObj=mc::Client::player();
    mc::LocalRef inventory(mc::Player(playerObj).inventory());
    // Vanilla restrictions are respected: no menu / inventory screen, nothing while eating or
    // otherwise using an item, and nothing while this player's own shield is up.
    if (gui::inputFrozen()||!playerObj||!playerCanUseModule(playerObj)) {
        clearShieldPending(inventory.get(),true);
        return;
    }
    mc::LocalRef list(inventory?mc::inv::mainList(inventory.get()):nullptr);
    if (!inventory||!list) return;
    mc::Player local(playerObj);
    float tickDelta=mc::Client::renderTickDelta();
    if (tickDelta<0.f) tickDelta=1.f;

    if (g_shieldWaiting) {
        const double now=ImGui::GetTime();
        if (now-g_shieldStageAt<std::clamp(g_autoShieldBreakSettings.swapDelayMs,1,600)/1000.0) return;
        mc::LocalRef target=findPlayerByUuid(g_shieldTargetUuid);
        const bool stillValid=target && mc::LivingEntity(target.get()).alive() &&
            !entityBlocked(target.get(),true) && mc::LivingEntity(target.get()).blocking() &&
            distanceToHitbox(local.eyePos(),mc::Entity(target.get()),tickDelta)<=(float)local.entityInteractionRange()+0.05f &&
            mc::Client::attackCooldown()<=0;
        if (!stillValid) {
            clearShieldPending(inventory.get(),true);
            g_shieldNextAt=now+std::clamp(g_autoShieldBreakSettings.retryDelayMs,1,600)/1000.0;
            return;
        }
        // Switching items resets the attack charge, but vanilla still lets an axe hit disable a
        // raised shield at partial charge, so the swap delay alone paces the attack.
        const bool attacked=mc::Client::attackEntity(target.get());
        if (attacked) {
            local.swingHand();
            g_attackSentThisFrame=true;
            // Stun Web: remember the axe target, but do not arm yet - the swing alone is not
            // a stun. The web waits for the shield to actually drop (see updateStunWeb).
            // Stun Web / AutoWeb: remember the axe target, but do not arm yet - the swing alone
            // is not a stun. The web waits for the shield to actually drop (see updateStunWeb),
            // and the follow-up belongs to whichever of the two cards is driving it.
            if (g_stunWeb) {
                StunWebSettings& web = g_stunWebSettings;
                if (web.allowPlayers) {
                    g_stunWebPendingTarget=trackedTargetKey(target.get());
                    g_stunWebPendingAt=now;
            }
            }
        }
        clearShieldPending(inventory.get(),g_autoShieldBreakSettings.swapBack||!attacked);
        g_shieldNextAt=now+std::clamp(g_autoShieldBreakSettings.retryDelayMs,1,600)/1000.0;
        return;
    }

    const double now=ImGui::GetTime();
    if (now<g_shieldNextAt) return;
    if (mc::Client::attackCooldown()>0) return;                        // vanilla "miss" delay
    if (g_autoShieldBreakSettings.mode==ShieldBreakMode::OnHit) {
        const std::vector<int>& taps=gui::mouseTaps();
        bool clicked=std::find(taps.begin(),taps.end(),VK_LBUTTON)!=taps.end();
        // A swing another module sent counts as the click too: Triggerbot and AutoMace announce
        // theirs, so OnHit follows the attack that is already going out. Every announced swing is
        // consumed exactly once, and only while it is still fresh.
        if (!clicked && g_moduleSwingSeq!=g_shieldConsumedSwingSeq &&
            now-g_moduleSwingAt<=kOnHitAllowanceWindow) {
            clicked=true;
            g_shieldConsumedSwingSeq=g_moduleSwingSeq;
        }
        if (!clicked) return;
    }
    const int currentSlot=std::clamp(mc::inv::selectedSlot(inventory.get()),0,8);
    if (g_autoShieldBreakSettings.swordOnly&&
        hotbarItemId(list.get(),currentSlot).find("_sword")==std::string::npos) return;
    mc::LocalRef target=targetPlayerForShield(playerObj,tickDelta);
    if (!target||!mc::LivingEntity(target.get()).blocking()) return;
    int foundAxe=-1;
    const int hotbarSize=std::min(9,mc::inv::listSize(list.get()));
    for (int i=0;i<hotbarSize;++i) {
        const std::string id=hotbarItemId(list.get(),i);
        if (id.size()>=4&&id.compare(id.size()-4,4,"_axe")==0) { foundAxe=i; break; }
    }
    if (foundAxe<0) {                                                  // an axe in the inventory does not count
        rateLimitedNotify(g_shieldNoticeAt,"Auto Shield Break needs an axe in the hotbar",4.0);
        g_shieldNextAt=now+0.75;
        return;
    }
    g_shieldOriginalSlot=currentSlot;
    g_shieldAxeSlot=foundAxe;
    g_shieldTargetUuid=trackedTargetKey(target.get());
    if (g_shieldTargetUuid.empty()) { clearShieldPending(inventory.get(),false); return; }
    if (foundAxe!=currentSlot) selectSlot(inventory.get(),foundAxe);
    g_shieldStageAt=now;
    g_shieldWaiting=true;
}

// ---- the real click (Attack And Destroy) ---------------------------------------------------
// Triggerbot and AutoMace click the way a player does: the left button is posted to the game window,
// so the game handles it through its own Attack And Destroy binding - its crosshair pick, its
// cooldown, its hand swing and the click a CPS HUD counts. Nothing is sent at an entity from here.
// The press is held for a beat and released by clickAttackTick, because a press and a release inside
// one frame read as no click at all (the same mechanism Drag Click and Triggerbot already use).
bool   g_clickAttackDown = false;
double g_clickAttackUpAt = 0.0;
constexpr double kClickAttackHold = 0.025;

void clickAttackRelease() {
    if (!g_clickAttackDown) return;
    hooks::syntheticClick(false);
    g_clickAttackDown = false;
}
void clickAttackTick(double now) {
    if (g_clickAttackDown && now >= g_clickAttackUpAt) clickAttackRelease();
}
bool clickAttackPress(double now) {
    if (g_clickAttackDown) return false;          // the previous click is still held
    hooks::syntheticClick(true);
    g_clickAttackDown = true;
    g_clickAttackUpAt = now + kClickAttackHold;
    return true;
}

// ---- Triggerbot ---------------------------------------------------------------------------
// A port of Argon's Trigger Bot: the card owns pacing and the target rules, and the swing itself is
// the game's own attack (interactionManager.attackEntity - or a left click posted to the game window
// with Click Simulation, the mechanism Drag Click already uses). Reach, the attack cooldown and a
// raised shield therefore stay vanilla's decision; the module only decides when to click.
std::string g_triggerLastTarget;       // Same Player: the target the last swing went to
double g_triggerLastSwingAt = 0.0;
int    g_triggerDelayMs = 0;           // the delay rolled for the current swing
int    g_triggerDelayWeapon = -1;      // which weapon window g_triggerDelayMs came from


// The rules themselves live in triggerbot_rules.h, where the harness can exercise them.
int triggerRollDelay(trigger::Weapon weapon) {
    const TriggerbotSettings& st = g_triggerbotSettings;
    const int lo = weapon == trigger::Weapon::Axe ? st.axeDelayMinMs : st.swordDelayMinMs;
    const int hi = weapon == trigger::Weapon::Axe ? st.axeDelayMaxMs : st.swordDelayMaxMs;
    return trigger::rollDelay(lo, hi, (unsigned)std::rand());
}

void updateTriggerbot() {
    const double now = ImGui::GetTime();
    const bool ready = jni::ready() && mc::frame::valid() && mc::frame::resolved() && mc::Client::player();
    if (!g_triggerbot || !ready || gui::inputFrozen()) { clickAttackRelease(); return; }
    clickAttackTick(now);

    const TriggerbotSettings& st = g_triggerbotSettings;
    if (g_attackSentThisFrame) return;                     // another module already swung this frame
    if (mc::Client::screenOpen()) return;                  // nothing clicks through an open screen

    jobject playerObj = mc::Client::player();
    mc::Player local(playerObj);

    mc::LocalRef hand(local.mainHandStack());
    if (!hand) return;
    trigger::Weapon weapon = trigger::weaponOf(mc::stk::itemId(hand.get()));
    if (weapon == trigger::Weapon::None) {
        if (!st.allItems) return;
        weapon = trigger::Weapon::Sword;                   // All Items keeps Argon's sword-delay pacing
    }

    mc::LocalRef crosshair(mc::Client::crosshair());
    if (!crosshair || mc::hit::typeOf(crosshair.get()) != 2) return;
    mc::LocalRef target(mc::hit::entity(crosshair.get()));
    if (!target || target.get() == playerObj) return;
    mc::LivingEntity living(target.get());
    if (!living.alive()) return;

    const bool playerTarget = isPlayerEntity(jni::env(), target.get());
    if (!playerTarget && !st.allEntities &&
        !(st.strayBypass && trigger::isZombieType(mc::entityTypePath(target.get())))) return;
    if (entityBlocked(target.get(), playerTarget)) return;  // friends list and Anti-Bot
    if (st.checkShield && playerTarget && living.blocking()) {
        const Vec3 toSelf = local.pos() - mc::Entity(target.get()).pos();
        if (trigger::shieldAngle(mc::Entity(target.get()).yaw(), toSelf.x, toSelf.z) < 90.f) return;
    }

    const std::string targetKey = trackedTargetKey(target.get());
    if (st.samePlayer && !g_triggerLastTarget.empty() && !targetKey.empty() &&
        targetKey != g_triggerLastTarget) return;

    // Only Crit: nothing but a critical hit is allowed out of the card.
    const bool crit = playerWouldCrit(local);
    if (st.criticals == CritMode::OnlyCrit && !crit) return;

    // The first roll also starts the clock, so turning the card on never fires an instant hit - one
    // random window passes first, as with Argon's timer. A weapon change rolls its own window at
    // once without restarting the clock, so an axe swing never waits on the sword's pending delay.
    if (g_triggerDelayMs <= 0) {
        g_triggerDelayMs = triggerRollDelay(weapon);
        g_triggerLastSwingAt = now;
    } else if (g_triggerDelayWeapon != (int)weapon) {
        g_triggerDelayMs = triggerRollDelay(weapon);
    }
    g_triggerDelayWeapon = (int)weapon;
    if (now - g_triggerLastSwingAt < g_triggerDelayMs / 1000.0) return;

    // Prio Crit: hold the due swing while no critical hit is on offer, then take the plain hit once
    // one more delay window has gone by without a fall. The card keeps swinging instead of waiting
    // forever on a target that never leaves the ground.
    if (st.criticals == CritMode::PrioCrit && !crit &&
        now - g_triggerLastSwingAt < (g_triggerDelayMs / 1000.0) * 2.0) return;

    // The swing is a real left click posted to the game window, so the game makes the attack itself
    // through its own Attack And Destroy binding. The millisecond window paces the card; the swing
    // also waits for the weapon's own attack charge, because a click on a weapon that has not
    // recovered lands no damage. Both are gates: whichever comes last is when the hit goes out.
    if (local.attackCooldownProgress(1.f) < 0.99f) return;
    if (!clickAttackPress(now)) return;

    // The swing is announced, so Auto Shield Break's OnHit mode answers it exactly as it answers a
    // real click (and Auto Shield's swing trigger can do the same).
    noteModuleSwing(now);
    g_triggerLastTarget = targetKey;
    g_triggerLastSwingAt = now;
    g_triggerDelayMs = triggerRollDelay(weapon);
    g_triggerDelayWeapon = (int)weapon;
}

// ---- AutoMace -----------------------------------------------------------------------------
// A smash, not a combo. The sequence only arms while the player is falling at least Minimum Fall
// Height - the mace's damage is a function of the fall - then takes the mace out of the hotbar and
// clicks once at the configured Hit delay.
//
// Every click is a real left click posted to the game window, so the game handles it through its own
// Attack And Destroy binding: its crosshair pick, its hand swing and the click a CPS HUD counts.
// Nothing here sends an attack at the entity, and nothing waits for the attack charge - the delays on
// the card are the whole timing, so a hit lands when the card says it does.
//
// StunSlam puts the hotbar axe in front of the same smash: a raised shield is hit with the axe,
// then, after the configured Axe delay (the time the shield needs to come down), the mace goes in
// and the click goes out at the Hit delay.
enum class MacePhase { Idle, AxeSwing, AxeWait, ChestOff, ChestEquip, MaceSelect, MaceSwing, Restore, Cooldown };

MacePhase g_macePhase = MacePhase::Idle;
double g_maceStageAt = 0.0;       // when the current step may continue
double g_maceNextAt = 0.0;        // when the next sequence may arm
std::string g_maceTarget;         // UUID of the entity the sequence was armed on
int  g_maceOrigSlot = -1;         // hotbar slot to give back afterwards
bool g_maceChestSwapped = false;  // the chestplate went on, so the elytra has to come back
// How many axe swings a StunSlam has already spent on a shield that is still up. A shield only
// drops when the server's tick applies the axe disable, so a swing that lands a frame early leaves
// the target blocking; the sequence swings again instead of spending the mace on a raised shield.
constexpr int kMaceAxeTries = 3;
int g_maceAxeTries = 0;
int  g_maceHitDelayMs = 0;        // the hit delay rolled for this smash
// The axe is swapped in and then clicked. The selection is synced at once, but the server resolves
// the attack against the item it has when the packet lands there, so a click sent in the same breath
// as the swap is still read as the mace (or whatever was in hand) and the shield is never disabled.
// A short beat between the two makes the stun land.
constexpr double kStunAxeSwapDwell = 0.025;   // 25 ms from the axe in hand to the click
double g_maceAxeSwapAt = 0.0;                 // when the axe was last selected

int maceRandMs(int lo, int hi) {
    lo = std::clamp(lo, 0, 5000); hi = std::clamp(hi, 0, 5000);
    if (hi < lo) std::swap(lo, hi);
    if (hi <= lo) return lo;
    return lo + (int)(std::rand() % (hi - lo + 1));
}

double maceSeconds(int ms) { return std::max(0, ms) / 1000.0; }

std::string maceItemId(jobject list, int index) {
    mc::LocalRef stack(mc::inv::getFromList(list, index));
    return stack ? mc::stk::itemId(stack.get()) : std::string{};
}

int maceFindHotbar(jobject list, const char* id) {
    const int count = std::min(9, mc::inv::listSize(list));
    for (int i = 0; i < count; ++i) if (maceItemId(list, i) == id) return i;
    return -1;
}

// The first hotbar slot whose item id ends in "suffix" ("_axe", "_chestplate").
int maceFindHotbarEnding(jobject list, const char* suffix) {
    const std::string want(suffix);
    const int count = std::min(9, mc::inv::listSize(list));
    for (int i = 0; i < count; ++i) {
        const std::string id = maceItemId(list, i);
        if (id.size() >= want.size() && id.compare(id.size() - want.size(), want.size(), want) == 0)
            return i;
    }
    return -1;
}
int maceFindHotbarAxe(jobject list)        { return maceFindHotbarEnding(list, "_axe"); }
int maceFindHotbarChestplate(jobject list) { return maceFindHotbarEnding(list, "_chestplate"); }

bool maceHolding(jobject playerObj, const char* id) {
    if (!playerObj) return false;
    mc::LocalRef hand(mc::LivingEntity(playerObj).mainHandStack());
    return hand && mc::stk::itemId(hand.get()) == id;
}

// Is the main hand the item that sits in `slot` right now? A hotbar selection that did not take is
// retried rather than clicking with the item it was meant to replace (the axe, after a stun slam).
bool maceHoldingSlotItem(jobject list, jobject playerObj, int slot) {
    const std::string want = maceItemId(list, slot);
    return !want.empty() && maceHolding(playerObj, want.c_str());
}

bool maceWearing(jobject playerObj, const char* id) {
    if (!playerObj) return false;
    mc::LocalRef chest(mc::LivingEntity(playerObj).equippedStack(4));   // 4 = CHEST
    return chest && mc::stk::itemId(chest.get()) == id;
}

// Put the stack in the hand on the way a right click does. For a chestplate that is the whole Elytra
// Swap: the vanilla call takes the elytra off as it puts the plate on - no screen, no menu clicks.
bool maceEquipHeld(jobject playerObj, const char* id) {
    if (!maceHolding(playerObj, id)) return false;
    mc::Client::interactItem(false);
    return maceWearing(playerObj, id);
}

void maceRestoreSlot() {
    const int slot = g_maceOrigSlot;
    g_maceOrigSlot = -1;
    if (!g_autoMaceSettings.swapBack || slot < 0 || slot > 8) return;
    jobject playerObj = mc::Client::player();
    if (!playerObj) return;
    mc::LocalRef inventory(mc::Player(playerObj).inventory());
    if (!inventory) return;
    const int current = std::clamp(mc::inv::selectedSlot(inventory.get()), 0, 8);
    if (current != slot) selectSlot(inventory.get(), slot);
}

// Select the axe and remember when: the swing waits out kStunAxeSwapDwell from this moment.
void maceSelectAxe(jobject inventory, int slot, double now) {
    selectSlot(inventory, slot);
    g_maceAxeSwapAt = now;
}

void maceResetState(bool restore) {
    if (restore) {
        maceRestoreSlot();
        if (g_maceChestSwapped) {
            jobject playerObj = mc::Client::player();
            mc::LocalRef inventory(playerObj ? mc::Player(playerObj).inventory() : nullptr);
            mc::LocalRef list(inventory ? mc::inv::mainList(inventory.get()) : nullptr);
            const int elytra = list ? maceFindHotbar(list.get(), "elytra") : -1;
            // The elytra went into the inventory when the plate went on. Put it back on if it landed
            // in the hotbar; otherwise the plate stays on rather than a screen opening mid fight.
            if (elytra >= 0 && playerObj) {
                selectSlot(inventory.get(), elytra);
                maceEquipHeld(playerObj, "elytra");
            }
            g_maceChestSwapped = false;
        }
    }
    g_macePhase = MacePhase::Idle;
    g_maceStageAt = 0.0;
    g_maceTarget.clear();
    g_maceHitDelayMs = 0;
    g_maceAxeSwapAt = 0.0;
    g_maceAxeTries = 0;
    g_maceOrigSlot = -1;
    g_maceChestSwapped = false;
}

void updateAutoMace() {
    const double now = ImGui::GetTime();
    clickAttackTick(now);
    const bool ready = jni::ready() && mc::frame::valid() && mc::frame::resolved() && mc::Client::player();
    if (!g_autoMace || !ready || gui::inputFrozen()) {
        if (g_macePhase != MacePhase::Idle) maceResetState(true);
        clickAttackRelease();
        return;
    }
    // A screen pauses the sequence where it is; the card does not cancel a smash for it.
    if (mc::Client::screenOpen()) return;

    const AutoMaceSettings& st = g_autoMaceSettings;
    jobject playerObj = mc::Client::player();
    mc::Player local(playerObj);
    mc::LocalRef inventory(local.inventory());
    if (!inventory) return;
    mc::LocalRef list(mc::inv::mainList(inventory.get()));
    if (!list) return;

    const auto targetAlive = [&](float extra) -> mc::LocalRef {
        if (g_maceTarget.empty()) return {};
        mc::LocalRef target = findPlayerByUuid(g_maceTarget);
        if (!target || !mc::LivingEntity(target.get()).alive()) return {};
        if ((mc::Entity(target.get()).pos() - local.pos()).len() > st.range + extra) return {};
        return target;
    };

    switch (g_macePhase) {
        case MacePhase::Idle: {
            if (now < g_maceNextAt) return;
            // Minimum Fall Height: the mace's damage is a function of the fall, so the sequence does
            // not arm until the player is falling that far. 0 keeps the ground-level use.
            // No Glide: a smash out of an elytra glide does not connect, so the sequence does not
            // arm at all while the elytra is carrying the player.
            if (st.noGlide && local.gliding()) return;
            if (st.minFallHeight > 0.01f &&
                (local.onGround() || local.fallDistance() < st.minFallHeight)) return;
            mc::LocalRef crosshair(mc::Client::crosshair());
            if (!crosshair || mc::hit::typeOf(crosshair.get()) != 2) return;
            mc::LocalRef target(mc::hit::entity(crosshair.get()));
            if (!target || target.get() == playerObj) return;
            if (!isPlayerEntity(jni::env(), target.get())) return;
            if (entityBlocked(target.get(), true)) return;
            if ((mc::Entity(target.get()).pos() - local.pos()).len() > st.range) return;
            const int mace = maceFindHotbar(list.get(), "mace");
            if (mace < 0) return;
            const std::string key = trackedTargetKey(target.get());
            if (key.empty()) return;
            g_maceTarget = key;
            g_maceOrigSlot = std::clamp(mc::inv::selectedSlot(inventory.get()), 0, 8);
            g_maceChestSwapped = false;
            g_maceHitDelayMs = 0;
            const int axe = maceFindHotbarAxe(list.get());
            if (st.stunSlam && axe >= 0 && mc::LivingEntity(target.get()).blocking()) {
                maceSelectAxe(inventory.get(), axe, now);
                g_macePhase = MacePhase::AxeSwing;
                g_maceStageAt = now + maceSeconds(st.swapDelayMs);
                return;
            }
            g_macePhase = MacePhase::ChestOff;
            g_maceStageAt = now;
            return;
        }
        case MacePhase::AxeSwing: {
            if (now < g_maceStageAt) return;
            mc::LocalRef target = targetAlive(0.5f);
            if (!target) { maceResetState(true); return; }
            const int axe = maceFindHotbarAxe(list.get());
            if (axe < 0) { maceResetState(true); return; }
            if (!maceHoldingSlotItem(list.get(), playerObj, axe)) {
                maceSelectAxe(inventory.get(), axe, now);  // the selection did not take: retry it
                g_maceStageAt = now + maceSeconds(st.swapDelayMs);
                return;
            }
            // The axe is in hand on the client; give the slot update the beat it needs to be on the
            // wire before the click, or the server still has the old item and the shield stays up.
            if (now - g_maceAxeSwapAt < kStunAxeSwapDwell) return;
            if (!clickAttackPress(now)) return;
            noteModuleSwing(now);
            ++g_maceAxeTries;
            g_macePhase = MacePhase::AxeWait;
            g_maceStageAt = now + maceSeconds(maceRandMs(st.axeDelayMinMs, st.axeDelayMaxMs));
            return;
        }
        case MacePhase::AxeWait: {
            if (now < g_maceStageAt) return;
            mc::LocalRef target = targetAlive(0.5f);
            if (!target) { maceResetState(true); return; }
            // The shield is the whole point of the axe: a target still blocking here means the swing
            // did not stun (it landed before the server had the shield up, or the shield went back
            // up). Swing the axe again instead of hammering a raised shield with the mace; a target
            // that never drops it ends the sequence and re-arms on the next fall.
            if (mc::LivingEntity(target.get()).blocking()) {
                if (g_maceAxeTries >= kMaceAxeTries) { maceResetState(true); return; }
                const int axe = maceFindHotbarAxe(list.get());
                if (axe < 0) { maceResetState(true); return; }
                if (!maceHoldingSlotItem(list.get(), playerObj, axe)) maceSelectAxe(inventory.get(), axe, now);
                g_macePhase = MacePhase::AxeSwing;
                g_maceStageAt = now + maceSeconds(maceRandMs(st.axeDelayMinMs, st.axeDelayMaxMs));
                return;
            }
            g_macePhase = MacePhase::ChestOff;
            g_maceStageAt = now;
            return;
        }
        case MacePhase::ChestOff: {
            if (now < g_maceStageAt) return;
            if (st.elytraSwap && maceWearing(playerObj, "elytra")) {
                const int plate = maceFindHotbarChestplate(list.get());
                if (plate >= 0) {
                    selectSlot(inventory.get(), plate);
                    g_macePhase = MacePhase::ChestEquip;
                    g_maceStageAt = now + maceSeconds(st.swapDelayMs);
                    return;
                }
            }
            const int mace = maceFindHotbar(list.get(), "mace");
            if (mace < 0) { maceResetState(true); return; }
            selectSlot(inventory.get(), mace);
            g_macePhase = MacePhase::MaceSelect;
            g_maceStageAt = now + maceSeconds(st.swapDelayMs);
            return;
        }
        case MacePhase::ChestEquip: {
            if (now < g_maceStageAt) return;
            const int plate = maceFindHotbarChestplate(list.get());
            if (plate >= 0 && maceHoldingSlotItem(list.get(), playerObj, plate)) {
                const std::string id = maceItemId(list.get(), plate);
                if (!id.empty() && maceEquipHeld(playerObj, id.c_str())) g_maceChestSwapped = true;
            }
            const int mace = maceFindHotbar(list.get(), "mace");
            if (mace < 0) { maceResetState(true); return; }
            selectSlot(inventory.get(), mace);
            g_macePhase = MacePhase::MaceSelect;
            g_maceStageAt = now + maceSeconds(st.swapDelayMs);
            return;
        }
        case MacePhase::MaceSelect: {
            if (now < g_maceStageAt) return;
            const int mace = maceFindHotbar(list.get(), "mace");
            if (mace < 0) { maceResetState(true); return; }
            if (!maceHoldingSlotItem(list.get(), playerObj, mace)) {
                selectSlot(inventory.get(), mace);
                g_maceStageAt = now + maceSeconds(st.swapDelayMs);
                return;
            }
            g_maceHitDelayMs = maceRandMs(st.hitDelayMinMs, st.hitDelayMaxMs);
            g_macePhase = MacePhase::MaceSwing;
            g_maceStageAt = now + maceSeconds(g_maceHitDelayMs);
            return;
        }
        case MacePhase::MaceSwing: {
            if (now < g_maceStageAt) return;
            mc::LocalRef target = targetAlive(0.5f);
            if (!target) { maceResetState(true); return; }
            // A shield that came back up between the stun and the smash would eat the mace hit: put
            // the axe back in and stun it again rather than wasting the smash.
            if (st.stunSlam && mc::LivingEntity(target.get()).blocking() && g_maceAxeTries < kMaceAxeTries) {
                const int axe = maceFindHotbarAxe(list.get());
                if (axe >= 0) {
                    maceSelectAxe(inventory.get(), axe, now);
                    g_macePhase = MacePhase::AxeSwing;
                    g_maceStageAt = now + maceSeconds(st.swapDelayMs);
                    return;
                }
            }
            const int mace = maceFindHotbar(list.get(), "mace");
            if (mace < 0) { maceResetState(true); return; }
            if (!maceHoldingSlotItem(list.get(), playerObj, mace)) {
                g_macePhase = MacePhase::MaceSelect;     // the mace is not in hand: go and get it
                g_maceStageAt = now + maceSeconds(st.swapDelayMs);
                return;
            }
            if (!clickAttackPress(now)) return;
            noteModuleSwing(now);
            g_macePhase = MacePhase::Restore;
            g_maceStageAt = now + maceSeconds(maceRandMs(st.reswapMinMs, st.reswapMaxMs));
            return;
        }
        case MacePhase::Restore: {
            if (now < g_maceStageAt) return;
            maceResetState(true);
            g_macePhase = MacePhase::Cooldown;
            g_maceNextAt = now + maceSeconds(maceRandMs(st.cooldownMinMs, st.cooldownMaxMs));
            return;
        }
        case MacePhase::Cooldown: {
            if (now < g_maceNextAt) return;
            maceResetState(false);
            return;
        }
    }
}

void updateFastPlace() {
    if (!g_fastPlace || gui::inputFrozen() || !jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return;
    if (!gui::mouseDown(VK_RBUTTON) || mc::Client::screenOpen()) return;
    if (mc::Client::itemUseCooldown()<=0) return;
    jobject playerObj=mc::Client::player();
    if (!playerObj) return;
    mc::LocalRef hand(mc::Player(playerObj).mainHandStack());
    if (!hand) return;
    const std::string id=mc::stk::itemId(hand.get());
    if (id.empty()) return;
    bool allowed=false;
    if (g_fastPlaceSettings.onlyXp) allowed=id=="experience_bottle";
    else {
        if (g_fastPlaceSettings.allowItems && isThrowableId(id)) allowed=true;
        if (g_fastPlaceSettings.allowBlocks && mc::ext::isBlockItemStack(hand.get())) allowed=true;
    }
    if (allowed) mc::Client::setItemUseCooldown(0);   // vanilla Use Item then fires on the next tick
}

// ---- Sprint -------------------------------------------------------------------------------
bool g_sprintApplied=false;
void updateSprint() {
    const bool ready=jni::ready() && mc::frame::valid() && mc::frame::resolved();
    if (!g_sprint || !ready || gui::inputFrozen() || mc::Client::screenOpen()) {
        if (g_sprintApplied && ready) { mc::ext::setKey(mc::ext::Key::Sprint,false); g_sprintApplied=false; }
        return;
    }
    // Hold the vanilla sprint key; the game still enforces hunger, sneaking, blindness and collisions.
    mc::ext::setKey(mc::ext::Key::Sprint,true);
    g_sprintApplied=true;
}

// ---- Auto Jump Reset ----------------------------------------------------------------------
int g_prevHurtTime=0;
float g_prevHealth=-1.f;
double g_jumpReleaseAt=0.0;
bool g_jumpHeld=false;
void updateAutoJumpReset() {
    const bool ready=jni::ready() && mc::frame::valid() && mc::frame::resolved();
    const double now=ImGui::GetTime();
    if (g_jumpHeld && (now>=g_jumpReleaseAt || !g_autoJumpReset) && ready) {
        mc::ext::setKey(mc::ext::Key::Jump,false);
        g_jumpHeld=false;
    }
    if (!g_autoJumpReset || !ready || gui::inputFrozen()) { g_prevHurtTime=0; g_prevHealth=-1.f; return; }
    jobject playerObj=mc::Client::player();
    jobject world=mc::Client::world();
    if (!playerObj || !world) { g_prevHurtTime=0; g_prevHealth=-1.f; return; }
    mc::LivingEntity self(playerObj);
    // Two signals for "just took a hit": hurtTime's rising edge (the exact hurt
    // tick) and a health drop (a safety net if the edge is missed between ticks).
    const int hurt=self.hurtTime();
    const float health=self.health();
    const bool hurtEdge=hurt>0 && hurt>g_prevHurtTime;
    const bool healthEdge=g_prevHealth>0.f && health>0.f && health<g_prevHealth-0.01f;
    const bool freshHit=hurtEdge || healthEdge;
    g_prevHurtTime=hurt;
    if (health>0.f) g_prevHealth=health;
    if (!freshHit || mc::Client::screenOpen() || !self.onGround() || g_jumpHeld) return;

    // Only damage that plausibly came from another player: one must be within melee-ish range.
    JNIEnv* env=jni::env();
    if (!env) return;
    const Vec3 me=self.pos();
    bool playerNearby=false;
    for (jobject entity:mc::collectEntities(world,256)) {
        if (!entity) continue;
        if (!playerNearby && isPlayerEntity(env,entity) &&
            env->IsSameObject(entity,playerObj)!=JNI_TRUE) {
            mc::LivingEntity other(entity);
            if (other.alive() && (other.pos()-me).len2()<=8.f*8.f) playerNearby=true;
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(entity);
    }
    if (!playerNearby) return;
    if ((int)(std::rand()%100) >= std::clamp(g_autoJumpResetSettings.accuracy,0,100)) return;
    // Jump on the hit tick itself. Waiting for the next input tick is a tick too late: knockback is
    // applied by the server on the damage tick, so the jump has to be in the player's own movement
    // for that tick. The upward velocity is written straight into the entity (exactly the 0.42 a
    // vanilla ground jump produces), and the vanilla jump key is held as well so the game's own tick
    // keeps the jump if it processes input before the next frame. Hunger, sneaking and vehicles are
    // still the game's rules, because the key is a real key press and the velocity is a real jump.
    const Vec3 velocity = self.velocity();
    if (self.onGround() && velocity.y <= 0.001f) {
        self.setVelocity(Vec3(velocity.x, 0.42f, velocity.z));
    }
    if (mc::ext::setKey(mc::ext::Key::Jump,true)) {
        g_jumpHeld=true;
        g_jumpReleaseAt=now+0.12;
        trace::log("auto jump reset: jumped on hit (hurt=%d)",hurt);
    } else {
        g_jumpHeld=true;
        g_jumpReleaseAt=now+0.12;
    }
}

// The player's own attack input: the left button held down, or a fresh press this frame. Every
// module that must never click by itself asks this - AutoMace's Only On Your Own Click and
// Breach Swap both do, so there is one rule for "the player asked for this swing".
bool playerAttackInputHeld() {
    if (gui::mouseDown(VK_LBUTTON)) return true;
    const std::vector<int>& taps=gui::mouseTaps();
    return std::find(taps.begin(),taps.end(),VK_LBUTTON)!=taps.end();
}



// The target captured when the sequence started is tracked by UUID (see targetAlive inside
// updateAutoMace), so swapping to a shorter-reach item - which changes the crosshair pick - can no
// longer make the module drop the target mid stun-slam / smash. The swing itself is clickAttackPress.


// ---- AimAssist ----------------------------------------------------------------------------
double g_aimLast=0.0;

// The point of the hitbox the current look ray runs into. Only used by the "Loose"
// accuracy, where the aim settles around whatever part of the body is under the
// crosshair instead of snapping to the centre or the head.
Vec3 rayNearBox(const Vec3& origin,const Vec3& dir,const mc::Entity::Bounds& b) {
    float tmin=0.f, tmax=1e9f;
    const float lo[3]={b.min.x,b.min.y,b.min.z}, hi[3]={b.max.x,b.max.y,b.max.z};
    const float o[3]={origin.x,origin.y,origin.z}, d[3]={dir.x,dir.y,dir.z};
    for (int i=0;i<3;++i) {
        if (std::fabs(d[i])<1e-6f) { if (o[i]<lo[i]||o[i]>hi[i]) return Vec3((lo[0]+hi[0])*0.5f,(lo[1]+hi[1])*0.5f,(lo[2]+hi[2])*0.5f); continue; }
        float t1=(lo[i]-o[i])/d[i], t2=(hi[i]-o[i])/d[i];
        if (t1>t2) std::swap(t1,t2);
        tmin=std::max(tmin,t1); tmax=std::min(tmax,t2);
    }
    const float t=tmin>0.f?tmin:std::max(0.f,tmax);
    return origin+dir*t;
}

// Accuracy target: Centre = middle of the hitbox, Head = the head's centre, Loose =
// a smoothly wandering point inside the hitbox near where the crosshair already is
// (always inside the box, so the hit still lands).
Vec3 aimPointFor(const AimAccuracy accuracy,const mc::Entity::Bounds& b,const Vec3& eye,const Vec3& look,double now,
                 const Vec3& headPoint) {
    const float cx=(b.min.x+b.max.x)*0.5f, cz=(b.min.z+b.max.z)*0.5f;
    if (accuracy==AimAccuracy::Center) return Vec3(cx,(b.min.y+b.max.y)*0.5f,cz);
    // Head: the target's own eye position, clamped inside the hitbox. The old top-of-box estimate
    // put the pitch on the crown of tall entities and above the head of short ones, which is why
    // the aim looked level while the body clearly was not. The eye is the point the head hitbox is
    // built around, so it is the one point that is inside the skull for every mob size.
    if (accuracy == AimAccuracy::Head) {
        const float y = std::clamp(headPoint.y, b.min.y + 0.02f, b.max.y - 0.02f);
        return Vec3(cx, y, cz);
    }
    // NOTE: not named `near` — windef.h defines that as an empty macro.
    const Vec3 hitPoint=rayNearBox(eye,look,b);
    const float jx=std::sin((float)now*3.1f)*0.35f, jy=std::sin((float)now*2.3f+1.7f)*0.30f, jz=std::sin((float)now*1.7f+0.9f)*0.35f;
    const float hx=(b.max.x-b.min.x)*0.5f, hy=(b.max.y-b.min.y)*0.5f, hz=(b.max.z-b.min.z)*0.5f;
    return Vec3(std::clamp(hitPoint.x+hx*jx,b.min.x,b.max.x),
                std::clamp(hitPoint.y+hy*jy,b.min.y,b.max.y),
                std::clamp(hitPoint.z+hz*jz,b.min.z,b.max.z));
}

float wrapDeg(float d) { while (d>180.f) d-=360.f; while (d<-180.f) d+=360.f; return d; }

// ---- Silent rotation -------------------------------------------------------------------------
// "Silent" means exactly one thing everywhere in this client: the player's own view is turned
// towards the target and told to the server, while the on-screen camera keeps the player's
// rotation. The aim therefore lands server-side and what you see never moves.
//
// The rotation is written on the render thread while the game thread runs the vanilla tick, so
// the previous-tick rotation (Entity.lastYaw/lastPitch, the source the renderer interpolates
// from) is put back along with the live rotation. A tick landing inside the spoof would
// otherwise stamp lastYaw = the aimed angle; the renderer would then interpolate from the aim
// towards the camera's own rotation and visibly swing the view, and vanilla would send a
// rotation packet of its own that overwrites the aim. Vanilla only sends its own rotation when
// the player's yaw actually changed, and here the camera's rotation is restored, so it stays
// quiet and the aim holds.
// When a module stops aiming, one more packet hands the camera's rotation back to the server, so
// the server is never left looking where the silent aim last pointed.
bool g_silentAimActive = false;
// True while the body/head are held on the server aim (so release restores them exactly once).
bool g_silentBodyActive = false;
void applySilentBodyPoseImpl(bool fromPreRender = false);   // single writer of body/head rotation
bool silentAimHoldingPoseImpl();                            // true while the aim (not a handoff) owns it

// One-shot silent actions (throws, placements) hold the pose for a moment so the model turns for the
// action and then returns, without a second writer fighting the aim.
double g_silentPoseUntil = 0.0;
float  g_silentPoseYaw = 0.f;
// Whether a silent model pose needs to be restored when the hold ends.
bool   g_headSmoothValid = false;
// Last rotation actually written to the model, and the camera it was compared against. Shown on
// screen by the Debug Mode diagnostics so a broken writer is visible without reading a log.
float  g_silentBodyWrittenYaw = 0.f, g_silentCameraYawNow = 0.f;
// Read-back of the four rotation fields right after writing them (-9999 = the field never resolved).
// If the "last" fields read back differently from the current ones, the renderer is still blending
// from camera-derived values and the field ids are the problem, not the timing.
float  g_dbgBody = -9999.f, g_dbgLastBody = -9999.f;
float  g_dbgHead = -9999.f, g_dbgLastHead = -9999.f;
// End-of-frame read of bodyYaw (taken in drawOverlay, before this frame's own module writes): if it
// differs from what the pre-render hook wrote, something between the write and the end of the frame
// put a different rotation back - the counter accumulates how often.
float  g_dbgEofBody = -9999.f;
int    g_dbgClobbers = 0;
// LivingEntity.lerpYaw(tickProgress) - the game's own render-facing rotation read. If this does not
// match what we wrote, the model is not drawn from the fields we write, and no amount of field
// pinning will ever change what is on screen.
float  g_dbgRenderYaw = -9999.f;
// Aim sessions that ended while the module was on and tracking. Should stay 0 now that every
// release is debounced; anything higher means one of the pre-checks is still flickering.
int    g_dbgReleases = 0;

// The pose has TWO writers: the pre-render hook (start of the frame) and the module's own update
// (end of frame, AFTER the world has already been drawn). Only the write that lands last before
// the draw can be visible; a second write in the same frame just stamps values for no benefit and
// overwrites the read-back state. This counter keeps them in step: whoever writes first claims the
// frame, and any second write in that frame is skipped.
uintptr_t g_poseFrame = ~uintptr_t(0);

// When the aim last had a qualifying target. Target checks (FOV cone, range, interpolated bounds)
// drop in and out frame to frame, and releasing on a single dropped frame snapped the body back to
// the camera and sent another movement packet - that is the twitch and the flagged packets.
double g_silentTargetSeenAt = -1.0;
// The rotation the server is being told (silent mode only). It eases toward the target frame by
// frame and is kept apart from the camera, so the camera never moves and the aim still converges.
float g_silentServerYaw = 0.f, g_silentServerPitch = 0.f;

// True silent aim: the aimed rotation is written into a look packet and sent, and the player's own
// rotation is never touched. That is the whole separation - the server is aimed at the target
// while the visible view stays exactly where the player put it. It matters most in F5: writing
// yaw/pitch onto the entity (even for a single call) leaks the aim into the third-person model,
// which is what used to visibly lock the character onto the target. Vanilla only sends a rotation
// packet of its own when the player's rotation changed, and this does not change it, so nothing
// overwrites the aim on the server either.
// Last rotation the silent aim put in a look packet. Only used by the fallback path (a build where
// the movement-packet transform could not install), and it stops a repeated rotation being sent.
bool  g_silentSentValid = false;
float g_silentSentYaw = 0.f, g_silentSentPitch = 0.f;
// True while the silent aim is riding the game's own movement packets (the normal path) instead of
// sending extra look packets of its own. The transform patches Entity.getYaw/getPitch inside
// ClientPlayerEntity.sendMovementPackets, so every rotation the server receives is the one packet
// vanilla was going to send anyway - on the game's tick, in the game's order, with the position the
// client really has. Extra look packets are what produced the Grim "Simulation" / duplicate-look /
// timer flags: they arrive between ticks, and a rotation with no matching movement is a mismatch.
bool  g_silentUsingPackets = true;
// The aim is a continuous session: while a target is in the cone the movement-packet transform
// carries the eased rotation on every tick, so the server tracks the target even between clicks.
// The camera is never written, and no extra look packet is sent for the tracking itself.
constexpr double kSilentClickBuffer = 0.85;
// A target that leaves the cone for a frame or two must not end the session (and send the camera
// rotation back); the same goes for a one-frame input/screen check. These are the grace windows.
constexpr double kSilentTargetGrace = 0.25;
constexpr double kSilentBlockGrace = 0.15;
double g_silentClickUntil = -1.0;

// A click is answered with the rotation the last movement packet already told the server, never
// with a fresh look packet. When the aim moved after that packet, the hit waits for the next tick
// instead of sending a second rotation: an extra look packet with the same angle is Grim's
// AimDuplicateLook, and any extra flying packet is what its timer counts. The wait is short (a
// client tick) and is dropped if the target leaves or the window closes.
//
// The wait is measured in game ticks, not wall time: Entity.age increments once per tick, and the
// movement packet is sent inside that tick. Seeing a different age therefore means "the packet that
// carries the pinned aim has gone out", which is the only thing the hit needs. Nothing gates the
// hit on where the camera is looking - the server's rotation is the silent aim, not the view.
bool g_silentAttackWaiting = false;
double g_silentAttackWaitUntil = -1.0;
double g_silentAttackArmAt = -1.0;
int    g_silentAttackAge = -1;
float g_silentAttackYaw = 0.f, g_silentAttackPitch = 0.f;
constexpr double kSilentAttackWait = 0.2;

// ---- world-action handoff ----------------------------------------------------------------------
// Silent aim owns the rotation the server is told, but a block place, a block break or an item use
// is the player's own camera action: the action's packet is validated against the view the player
// actually has, and rotating them away from the block is how a placement gets refused or a break
// never starts. So while one of those actions runs, the aim stands down for its duration and the
// server is handed the camera instead - one look packet at the press (the same rotation vanilla's
// next movement packet would carry), then nothing at all, because the packet transform is
// suspended. The aim keeps solving in the background and takes the rotation back the moment the
// action ends, so the tracking is never more than one action away.
//
// This is what makes the two coexist: the player's own actions are camera actions, exactly like
// vanilla, while combat clicks stay silent-aim clicks (a left click that is not breaking a block
// ends the handoff immediately and is answered by the aim).
constexpr double kActionLease = 0.20;   // how long a press keeps the camera on the server
double g_actionHoldUntil = -1.0;
bool   g_cameraOnBlock = false;         // the last frame's crosshair pick, for the input hook
bool   g_cameraOnEntity = false;        // the camera is already on something vanilla will hit
bool   g_actionLogged = false;

// A monotonic clock, not ImGui's frame time: this is also read from the input-message hook, which
// runs between frames and must not depend on a started ImGui frame.
double actionClock() {
    using namespace std::chrono;
    static const steady_clock::time_point start = steady_clock::now();
    return duration<double>(steady_clock::now() - start).count();
}

// True while the server is deliberately holding the player's camera instead of the silent aim.
// The lease is cleared with the session, so this can never keep a released aim standing down.
bool actionHolding() { return g_actionHoldUntil > actionClock(); }

// Hand the server the camera for one world action. `mining` only colours the log line: what the
// handoff does is the same for a place, a break or a use. Safe to call every frame while the
// button is held; only the first call of a handoff touches the packets.
void silentAimActionHold(bool mining, double holdSeconds) {
    if (!g_silentAimActive) return;
    const double now = actionClock();
    const bool first = g_actionHoldUntil <= now;
    g_actionHoldUntil = now + holdSeconds;
    if (!first) return;
    // The action owns this tick. A click that was waiting for its aim packet cannot be carried
    // while the server is being handed the camera, so it is dropped instead of firing late.
    g_silentAttackWaiting = false;
    g_silentAttackWaitUntil = -1.0;
    g_silentAttackArmAt = -1.0;
    g_silentAttackAge = -1;
    // The body and head follow the camera while the player acts (the watchdog and the pose writer
    // both check actionHolding()), so the model matches the action instead of staring at the target.
    g_headSmoothValid = false;
    if (g_silentUsingPackets) jni::silentRotationSuspend();
    float cameraYaw = 0.f, cameraPitch = 0.f;
    if (!mc::ext::sendCameraLookLive(&cameraYaw, &cameraPitch)) {
        if (!g_actionLogged) trace::log("silent aim: action handoff could not hand the camera over");
        g_actionLogged = true;
        return;
    }
    // The fallback (no packet transform) sends its own look packets; record the camera as the last
    // sent rotation so the aim is not deduplicated away when it resumes.
    if (!g_silentUsingPackets) {
        g_silentSentValid = true;
        g_silentSentYaw = cameraYaw;
        g_silentSentPitch = cameraPitch;
    }
    if (!g_actionLogged) {
        g_actionLogged = true;
        trace::log("silent aim: %s action handoff - server on the camera, aim resumes after",
                   mining ? "break" : "use");
    }
}

void silentAimSend(jobject playerObj, float yaw, float pitch) {
    if (!playerObj) return;
    const double now = ImGui::GetTime();
    if (!g_silentAimActive) {
        // Preferred path: the movement-packet transform carries the aim, and no extra packet is sent.
        if (jni::silentRotationStart(yaw, pitch)) {
            g_silentUsingPackets = true;
        } else {
            // Fallback for a game where the transform could not be installed: one real look packet,
            // throttled to slightly more than one game tick so it can never arrive twice per tick.
            static double lastSendAt = -1.0;
            if (lastSendAt >= 0.0 && now - lastSendAt < 0.051) return;
            if (!mc::ext::sendLookPacket(yaw, pitch, mc::Player(playerObj).onGround())) return;
            lastSendAt = now;
            g_silentUsingPackets = false;
            g_silentSentValid = true;
            g_silentSentYaw = yaw;
            g_silentSentPitch = pitch;
        }
    } else if (g_silentUsingPackets) {
        jni::silentRotationUpdate(yaw, pitch);
    } else {
        static double lastSendAt = -1.0;
        if (lastSendAt >= 0.0 && now - lastSendAt < 0.051) return;
        if (g_silentSentValid &&
            std::abs(angleDeltaDeg(g_silentSentYaw, yaw)) < 0.01f &&
            std::abs(pitch - g_silentSentPitch) < 0.01f) return;
        if (!mc::ext::sendLookPacket(yaw, pitch, mc::Player(playerObj).onGround())) return;
        lastSendAt = now;
        g_silentSentValid = true;
        g_silentSentYaw = yaw;
        g_silentSentPitch = pitch;
    }
    g_silentAimActive = true;
    // The model's rotation is normally written by applySilentBodyPose() from the pre-render hook,
    // which beats the game's own tick. This write at end of frame is the fallback for a build where
    // that hook cannot install: it writes the same rotation, so the two never disagree - the only
    // cost without the pre-render hook is that a tick-rewritten frame can still show for one frame.
    g_silentBodyActive = true;
    applySilentBodyPoseImpl();
}

// Silent Aim solver: the rotation that points the player at `target` from `origin`, in Minecraft's
// yaw/pitch convention (degrees). It only computes the angles; the caller decides what is sent.
// The camera is never written here, so every Silent Aim module shares this one solution.
struct SilentAngles { float yaw; float pitch; };
SilentAngles silentAimAngles(const Vec3& origin, const Vec3& target) {
    const Vec3 delta = target - origin;
    const float horizontal = std::sqrt(delta.x * delta.x + delta.z * delta.z);
    return { std::atan2(-delta.x, delta.z) * 57.29577951f,
             -std::atan2(delta.y, horizontal) * 57.29577951f };
}

// Does a ray from `origin` at yaw/pitch enter `bounds` within maxDistance? This is how the silent
// attack decides whether the rotation the server is already holding is on the target; if it is,
// the hit needs no look packet at all.
bool silentRayHitsBounds(const Vec3& origin, float yawDeg, float pitchDeg,
                         const mc::Entity::Bounds& bounds, float maxDistance) {
    if (!bounds.valid) return false;
    const float yaw = yawDeg * 0.017453292f;
    const float pitch = pitchDeg * 0.017453292f;
    const Vec3 dir(-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch));
    const float lo[3] = { bounds.min.x, bounds.min.y, bounds.min.z };
    const float hi[3] = { bounds.max.x, bounds.max.y, bounds.max.z };
    const float o[3] = { origin.x, origin.y, origin.z };
    const float d[3] = { dir.x, dir.y, dir.z };
    float tmin = 0.f, tmax = maxDistance;
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(d[i]) < 1e-6f) {
            if (o[i] < lo[i] || o[i] > hi[i]) return false;
            continue;
        }
        const float t1 = (lo[i] - o[i]) / d[i];
        const float t2 = (hi[i] - o[i]) / d[i];
        tmin = std::max(tmin, std::min(t1, t2));
        tmax = std::min(tmax, std::max(t1, t2));
        if (tmin > tmax) return false;
    }
    return tmax >= tmin;
}

// One-shot silent aim for the modules whose action packet (use item / interact) carries the
// player's own rotation: the aim is written for the single call and put straight back, so the
// action is aimed server-side while nothing visible turns. A look packet is sent afterwards so the
// server is left holding the player's real view (the caller can skip it with `restoreServer`).
// Returns the action's own result.
template <typename Fn>
bool silentActionAim(mc::Player& player, float yaw, float pitch, bool restoreServer, Fn&& action) {
    const float realYaw = player.yaw(), realPitch = player.pitch();
    const float realLastYaw = player.prevYaw(), realLastPitch = player.prevPitch();
    player.setYaw(yaw);
    player.setPitch(pitch);
    player.setPrevYaw(yaw);
    player.setPrevPitch(pitch);
    // A one-shot action is validated against whatever rotation the server holds when the action
    // arrives, so the aim goes out in the same tick as the action. One look packet only: vanilla
    // sends a single movement packet per tick, and any extra position-carrying packet reads to the
    // server as an invalid move.
    mc::ext::sendLookPacket(yaw, pitch, player.onGround());
    // The model follows the aim for the action through the pre-render writer, then returns.
    g_silentPoseYaw = yaw;
    g_silentPoseUntil = ImGui::GetTime() + 0.18;
    const bool result = action();
    player.setYaw(realYaw);
    player.setPitch(realPitch);
    player.setPrevYaw(realLastYaw);
    player.setPrevPitch(realLastPitch);
    if (restoreServer) mc::ext::sendLookPacket(realYaw, realPitch, player.onGround());
    return result;
}

// One silent block placement: the rotation is put on the player for the single interaction and sent
// ahead of it, the interaction is the game's own interactBlock, and the real view (and the server's
// rotation) is put back immediately afterwards. This is the same single-shot path the silent throws
// use, not the continuous aim session, so it can never leave the player on the placement angle or
// flood the server with rotation packets. The camera never moves: what the player sees is untouched
// from the first frame to the last.
bool silentPlaceBlock(mc::Player& player, int blockX, int blockY, int blockZ, int face,
                      const Vec3& facePoint, bool offhand) {
    Vec3 eye = player.eyePos();
    Vec3 to = facePoint - eye;
    if (to.len() < 0.01f) return false;
    const float flat = std::sqrt(to.x * to.x + to.z * to.z);
    const float yaw = std::atan2(-to.x, to.z) * 57.29577951f;
    const float pitch = -std::atan2(to.y, std::max(flat, 0.001f)) * 57.29577951f;
    return silentActionAim(player, yaw, pitch, true, [&]() {
        mc::Client::setItemUseCooldown(4);
        return mc::Client::interactBlockAtFace(blockX, blockY, blockZ,
                                               facePoint.x, facePoint.y, facePoint.z,
                                               face, offhand);
    });
}

void silentAimRelease(jobject playerObj) {
    if (!g_silentAimActive) {
        g_silentAttackWaiting = false;
        g_silentAttackWaitUntil = -1.0;
        g_silentAttackArmAt = -1.0;
        g_silentAttackAge = -1;
        g_actionHoldUntil = -1.0;
        g_actionLogged = false;
        return;
    }
    ++g_dbgReleases;   // diagnostic: only real ends of a session should ever land here
    const bool usedPackets = g_silentUsingPackets;
    const bool sentValid = g_silentSentValid;
    const float sentYaw = g_silentSentYaw, sentPitch = g_silentSentPitch;
    g_silentAimActive = false;
    jni::silentRotationStop();
    g_silentBodyActive = false;
    g_silentPoseUntil = 0.0;     // the watchdog must not write the old action pose after release
    g_silentTargetSeenAt = -1.0;
    g_silentSentValid = false;
    g_silentClickUntil = -1.0;
    g_silentAttackWaiting = false;
    g_silentAttackWaitUntil = -1.0;
    g_silentAttackArmAt = -1.0;
    g_silentAttackAge = -1;
    g_actionHoldUntil = -1.0;      // a handoff never outlives its session
    g_actionLogged = false;
    trace::log("silent aim: session ended (drops=%d)", g_dbgReleases);
    if (!playerObj || !jni::ready()) return;
    mc::Player self(playerObj);
    // The movement-packet transform already leaves lastYaw/lastPitch on the last silent aim, so the
    // next ordinary tick sends the camera back with no extra packet. Only the fallback (no
    // transform) has no such packet, and it hands the view back explicitly.
    if (!usedPackets) {
        const bool changed = !sentValid ||
            std::abs(angleDeltaDeg(sentYaw, self.yaw())) > 0.001f ||
            std::abs(sentPitch - self.pitch()) > 0.001f;
        if (changed) mc::ext::sendLookPacket(self.yaw(), self.pitch(), self.onGround());
    }
    // Give vanilla its pose back once; do not keep forcing it on later frames.
    mc::LivingEntity(playerObj).setBodyPose(self.yaw());
    g_headSmoothValid = false;
}

// The render hooks can run outside the per-frame JNI scope. Acquire the player directly here
// and hold both current and previous model rotations while the silent pose is active.
void applySilentBodyPoseImpl(bool fromPreRender) {
    if (!jni::ready() || !mc::frame::resolved() || !ImGui::GetCurrentContext()) return;
    const bool holding = silentAimHoldingPoseImpl();
    if (!holding && !g_headSmoothValid) return;
    JNIEnv* env = jni::env();
    if (!env || !mc::sym().minecraft || !mc::sym().mc_getInstance) return;
    mc::LocalRef client(env->CallStaticObjectMethod(mc::sym().minecraft, mc::sym().mc_getInstance));
    if (env->ExceptionCheck()) { jni::clearException(env, "pose getInstance"); return; }
    if (!client || !mc::sym().mc_player) return;
    mc::LocalRef player(env->GetObjectField(client.get(), mc::sym().mc_player));
    if (env->ExceptionCheck()) { jni::clearException(env, "pose player"); return; }
    jobject playerObj = player.get();
    if (!playerObj) return;
    // One write per frame: the pre-render hook claims the frame it runs in; the end-of-frame
    // fallback only writes on frames where the hook did not run at all (so it cannot overwrite the
    // hook's read-back state or fight it for the same frame).
    const uintptr_t frame = hooks::frameCount();
    if (fromPreRender) g_poseFrame = frame;
    else if (g_poseFrame == frame) return;
    if (holding) {
        // Match the rotation the server is being told, without touching the camera yaw or pitch.
        // Silent aim keeps the camera still, but the character's body and head still turn.
        const float target = g_silentAimActive ? g_silentServerYaw : g_silentPoseYaw;
        g_headSmoothValid = true;
        mc::LivingEntity(playerObj).setBodyPose(target);
        g_silentBodyWrittenYaw = target;
        g_silentCameraYawNow = mc::Player(playerObj).yaw();
        return;
    }
    g_headSmoothValid = false;
    mc::LivingEntity living(playerObj);
    const float yaw = mc::Player(playerObj).yaw();
    living.setBodyPose(yaw);
    g_silentBodyWrittenYaw = yaw;
    g_silentCameraYawNow = mc::Player(playerObj).yaw();
    // Prove the write stuck: read all four fields back. A -9999 here means the field id is null,
    // which would leave the previous value (camera-derived) doing the blending.
    if (JNIEnv* e = jni::env()) {
        const auto read = [&](jfieldID f) {
            if (!f) return -9999.f;
            const float v = e->GetFloatField(playerObj, f);
            if (e->ExceptionCheck()) { jni::clearException(e, "bodyPose read"); return -9999.f; }
            return v;
        };
        g_dbgBody     = read(mc::sym().liv_bodyYaw);
        g_dbgLastBody = read(mc::sym().liv_lastBodyYaw);
        g_dbgHead     = read(mc::sym().liv_headYaw);
        g_dbgLastHead = read(mc::sym().liv_lastHeadYaw);
    }
}

// ---- pose watchdog -----------------------------------------------------------------------------
// The game's tick can put the camera's rotation back on the body/head at any point between the
// frame hooks and the moment the model is built - and the model then interpolates between our aim
// and that value, which is the visible twitch. No GL hook can land inside that gap, so a dedicated
// thread keeps the pose corrected continuously while the silent aim owns it: anything the game
// writes is undone within ~100 us instead of surviving a whole frame.
//
// It resolves the player itself (getInstance -> player field) rather than using the per-frame cached
// global references: the render thread frees those every frame, and touching a freed global
// reference from another thread would corrupt the JVM.
int g_wdWrites = 0;        // watchdog writes so far
int g_wdCorrections = 0;   // writes that found the field off the aim (the game had rewritten it)

// The silent aim now holds the body/head on the aim the server is being told (only the camera is
// left alone), so the watchdog keeps the game's tick from putting the camera rotation back.
bool silentAimHoldingPoseImpl() {
    // A world-action handoff gives the model back to the game: while the player places or breaks a
    // block their body and head must follow the camera like vanilla, not stay on the silent aim.
    return (g_silentAimActive && !actionHolding()) || g_silentPoseUntil > ImGui::GetTime();
}

void silentPoseWatchdogWriteImpl() {
    if (!silentAimHoldingPoseImpl()) return;
    JNIEnv* e = jni::env();
    if (!e || !mc::sym().minecraft || !mc::sym().mc_getInstance || !mc::sym().mc_player) return;
    jobject client = e->CallStaticObjectMethod(mc::sym().minecraft, mc::sym().mc_getInstance);
    jni::clearException(e, "watchdog getInstance");
    if (!client) return;
    jobject player = e->GetObjectField(client, mc::sym().mc_player);
    e->DeleteLocalRef(client);
    jni::clearException(e, "watchdog player");
    if (!player) return;
    const float yaw = g_silentAimActive ? g_silentServerYaw : g_silentPoseYaw;
    if (mc::sym().liv_headYaw) {
        const float cur = e->GetFloatField(player, mc::sym().liv_headYaw);
        if (e->ExceptionCheck()) jni::clearException(e, "watchdog read");
        else if (std::abs(cur - yaw) > 1.f) ++g_wdCorrections;
    }
    mc::LivingEntity(player).setBodyPose(yaw);
    ++g_wdWrites;
    e->DeleteLocalRef(player);
}

// ---- Silent Aim (Combat, experimental) ---------------------------------------------------------
// A port of frost.rotation.mouse.SilentRotator. The Java class owns currentYaw/currentPitch and
// walks them toward the target in update() through applyMouseDelta (a fixed 0.15 of the pending
// delta), clamps the pitch to +-90 and reports completion inside one degree. Its relativeMode
// exists because Frost turns the camera onto the server rotation; this client never turns the
// camera, so the aim here is absolute. The 0.15 becomes the frame's alpha, which keeps the same
// ease at any frame rate, and the sweep starts from the rotation the server already holds - so a
// new target is acquired over several updates instead of snapping onto it.
struct SilentRotator {
    float currentYaw = 0.f;
    float currentPitch = 0.f;
    bool started = false;
    bool complete = false;

    static float clampPitch(float pitch) { return std::clamp(pitch, -90.f, 90.f); }

    void reset(float yaw, float pitch) {
        currentYaw = wrapDeg(yaw);
        currentPitch = clampPitch(pitch);
        started = true;
        complete = false;
    }

    // Java update(): the yaw/pitch deltas, then the rotator's own step. `alpha` is that step's 0.15.
    void update(float targetYaw, float targetPitch, float alpha) {
        if (!started) { reset(targetYaw, targetPitch); return; }
        applyMouseDelta(wrapDeg(targetYaw - currentYaw) * alpha,
                        (targetPitch - currentPitch) * alpha);
        complete = std::abs(wrapDeg(targetYaw - currentYaw)) < 1.f &&
                   std::abs(targetPitch - currentPitch) < 1.f;
    }

    // Java applyMouseDelta().
    void applyMouseDelta(float yawDelta, float pitchDelta) {
        currentYaw = wrapDeg(currentYaw + yawDelta);
        currentPitch = clampPitch(currentPitch + pitchDelta);
    }

    // Java getRenderedYaw()/getRenderedPitch().
    float getRenderedYaw() const { return currentYaw; }
    float getRenderedPitch() const { return clampPitch(currentPitch); }
    bool isComplete() const { return complete; }
};

SilentRotator g_silentRotator;
// The session this card started. AimAssist's Silent switch drives the same session, and only the
// card that started it may end it.
bool g_silentRotatorOwned = false;
double g_silentRotatorSeenAt = -1.0;
double g_silentRotatorLast = 0.0;

void updateSilentAimModule() {
    const bool ready = jni::ready() && mc::frame::valid() && mc::frame::resolved();
    jobject playerObj = ready ? mc::Client::player() : nullptr;
    jobject world = ready ? mc::Client::world() : nullptr;
    // updateAimAssist runs first, so a session AimAssist's Silent switch is driving this frame
    // already exists by the time this card sees it: it must not be stopped here, or the two cards
    // would end each other's session every time both are on.
    const bool assistDriving = g_aimAssist && g_aimAssistSettings.silent;
    const auto forget = [&]() {
        g_silentRotatorOwned = false;
        g_silentRotator = SilentRotator{};
        g_silentRotatorSeenAt = -1.0;
    };
    const auto release = [&]() {
        if (g_silentRotatorOwned && !assistDriving) silentAimRelease(playerObj);
        forget();
    };
    if (!g_silentAimModule) { release(); return; }
    // AimAssist's own Silent switch drives the same session; one card at a time.
    if (assistDriving) { forget(); return; }
    const SilentAimSettings& st = g_silentAimModuleSettings;
    const double now = ImGui::GetTime();
    const bool blocked = !ready || !playerObj || !world || gui::inputFrozen() ||
                         mc::Client::screenOpen() ||
                         (st.onlyWhileAttacking && !gui::mouseDown(VK_LBUTTON)) ||
                         (st.noGlide && mc::Player(playerObj).gliding());
    if (blocked) {
        // The same grace the silent AimAssist uses: one bad frame must not drop the aim.
        if (g_silentRotatorOwned && g_silentRotatorSeenAt >= 0.0 &&
            now - g_silentRotatorSeenAt <= kSilentTargetGrace) return;
        release();
        return;
    }
    JNIEnv* env = jni::env();
    if (!env || !mc::sym().living) return;

    mc::Player self(playerObj);
    const Vec3 eye = self.eyePos();
    const float curYaw = self.yaw(), curPitch = self.pitch();
    const float lookYaw = curYaw * 0.017453292f, lookPitch = curPitch * 0.017453292f;
    const Vec3 cameraLook(-std::sin(lookYaw) * std::cos(lookPitch), -std::sin(lookPitch),
                          std::cos(lookYaw) * std::cos(lookPitch));
    const float halfFov = std::clamp(st.fov, 5.f, 360.f) * 0.5f;

    float bestAngle = 1e9f, wantYaw = 0.f, wantPitch = 0.f;
    bool found = false;
    jobject bestEntity = nullptr;   // the aim's target, for the game's own crosshair pick
    for (int pass = 0; pass < 2 && !found; ++pass) {
        if (pass == 1 && !g_silentAimActive) break;
        Vec3 look = cameraLook;
        if (pass == 1) {
            // The aim the server already holds is the second reference: a camera turned away must
            // not drop the target the rotator is sweeping onto.
            const float sy = g_silentServerYaw * 0.017453292f, sp = g_silentServerPitch * 0.017453292f;
            look = Vec3(-std::sin(sy) * std::cos(sp), -std::sin(sp), std::cos(sy) * std::cos(sp));
        }
        for (jobject entity : mc::collectEntities(world, 256)) {
            if (!entity) continue;
            const bool isLiving = env->IsInstanceOf(entity, mc::sym().living) == JNI_TRUE;
            const bool isPlayer = isPlayerEntity(env, entity);
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (isLiving && env->IsSameObject(entity, playerObj) != JNI_TRUE &&
                ((isPlayer && st.allowPlayers) || (!isPlayer && st.allowEntities)) &&
                !entityBlocked(entity, isPlayer)) {
                mc::LivingEntity t(entity);
                const mc::Entity::Bounds b = t.interpolatedBounds(1.f);
                if (t.alive() && b.valid && distanceToHitbox(eye, t, 1.f) <= st.range) {
                    const Vec3 aim = aimPointFor(st.accuracy, b, eye, look, now, t.eyePos());
                    const Vec3 d = aim - eye;
                    const float dist = d.len();
                    if (dist > 0.01f) {
                        const float angle = std::acos(std::clamp(d.dot(look) / dist, -1.f, 1.f)) * 57.29578f;
                        if (angle <= halfFov && angle < bestAngle) {
                            bestAngle = angle;
                            found = true;
                            if (bestEntity) env->DeleteLocalRef(bestEntity);
                            bestEntity = env->NewLocalRef(entity);
                            const SilentAngles solved = silentAimAngles(eye, aim);
                            wantYaw = solved.yaw;
                            wantPitch = solved.pitch;
                        }
                    }
                }
            }
            env->DeleteLocalRef(entity);
        }
    }
    if (!found) {
        if (g_silentRotatorOwned && g_silentRotatorSeenAt >= 0.0 &&
            now - g_silentRotatorSeenAt <= kSilentTargetGrace) return;
        release();
        return;
    }
    g_silentRotatorSeenAt = now;
    if (!g_silentRotator.started) {
        // Begin where the server already is - the live aim, otherwise the camera - so the first
        // lock is a sweep from the player's own view, never an instant snap.
        g_silentRotator.reset(g_silentAimActive ? g_silentServerYaw : curYaw,
                              g_silentAimActive ? g_silentServerPitch : curPitch);
        g_silentRotatorLast = now;
    }
    const float dt = (float)std::clamp(now - g_silentRotatorLast, 0.001, 0.1);
    g_silentRotatorLast = now;
    const float alpha = st.smooth ? 1.f - std::exp(-std::clamp(st.speed, 1.f, 40.f) * dt) : 1.f;
    g_silentRotator.update(wantYaw, st.vertical ? wantPitch : g_silentRotator.currentPitch, alpha);
    const float aimYaw = g_silentRotator.getRenderedYaw();
    const float aimPitch = st.vertical ? g_silentRotator.getRenderedPitch() : curPitch;
    // The body follows the aim; the camera is never written. The movement-packet transform carries
    // the rotation, and the game's own crosshair pick is computed from it.
    g_silentPoseYaw = aimYaw;
    const bool wasActive = g_silentAimActive;
    silentAimSend(playerObj, aimYaw, aimPitch);
    // The game's own crosshair pick follows the aim: the block under the camera stops being outlined
    // and the attack indicator shows the target, whether or not the pick-level transform installed.
    mc::Client::setCrosshairTargetEntity(bestEntity);
    if (bestEntity) env->DeleteLocalRef(bestEntity);
    // Only a session this card started is ended by it; one a silent throw or another card owns is
    // left to its owner.
    if (!wasActive && g_silentAimActive) g_silentRotatorOwned = true;
}

void updateAimAssist() {
    const bool ready=jni::ready() && mc::frame::valid() && mc::frame::resolved();
    jobject playerObj=ready?mc::Client::player():nullptr;
    jobject world=ready?mc::Client::world():nullptr;
    // The charge the player's click was made at. The game processes a click in its own input
    // handler on the tick, before this frame runs, and that swing spends the attack charge even
    // when it hit only air - so reading the cooldown here refused every silent hit whose camera was
    // not already on the target, which is exactly the "I still have to look at them" case. The
    // value from the previous frame is the charge the click actually had.
    bool chargeReadyNow = false;
    if (ready && playerObj) {
        mc::Player self(playerObj);
        chargeReadyNow = mc::Client::attackCooldown() <= 0 && self.attackCooldownProgress(1.f) >= 0.99f;
    }
    static bool chargeReadyPrev = false;
    const bool chargeAtClick = chargeReadyNow || chargeReadyPrev;
    chargeReadyPrev = chargeReadyNow;
    // The camera's own pick for this frame. A left click on a block is a block break, not an
    // attack; the input hook reads this flag (one frame later) to decide what a press starts.
    g_cameraOnBlock = false;
    g_cameraOnEntity = false;
    // With pick-level targeting the game's own crosshair is computed from the aim, so the field no
    // longer says where the camera looks. The camera's pick is kept for the two questions that are
    // still about the view: is this press a block action, and is vanilla already on an entity.
    const bool pickAim = jni::silentAimPickActive();
    if (ready && playerObj) {
        jobject cameraPick = pickAim ? jni::silentAimCameraPick() : nullptr;
        mc::LocalRef cameraOwned(cameraPick ? nullptr : mc::Client::crosshair());
        if (!cameraPick) cameraPick = cameraOwned.get();
        const int hitType = cameraPick ? mc::hit::typeOf(cameraPick) : 0;
        g_cameraOnBlock = hitType == 1;
        g_cameraOnEntity = hitType == 2;
        if (cameraPick && !cameraOwned) {
            if (JNIEnv* pe = jni::env()) pe->DeleteLocalRef(cameraPick);
        }
    }
    if (!g_aimAssist) { silentAimRelease(playerObj); return; }   // module off: end immediately
    const AimAssistSettings& st=g_aimAssistSettings;
    // Everything else that would end the aim is debounced. A single frame where one of these checks
    // fails (a hand-stack read, the screen poll, the attack button) used to end the session on the
    // spot: the body snapped back to the camera for a frame and a Full packet went out with the
    // camera's rotation - one visible twitch plus a stray packet every time it happened, which is
    // also the most likely source of the flagged "invalid" packets. The session now ends only when
    // the condition persists; in between, the last aim is held exactly like a dropped target frame.
    const double now0=ImGui::GetTime();
    static double blockedSince=-1.0;
    // A click that is waiting for its movement packet is still the player's attack input for this
    // short window, even if the button was released immediately (Only While Holding Attack).
    const bool pendingClick = g_silentAttackWaiting && now0 <= g_silentAttackWaitUntil;
    // A world action in progress is part of the fight: do not end the session just because the
    // attack button came up to place a block or eat. The aim is standing down for that action and
    // picks up as soon as it ends, instead of the target being dropped and re-acquired.
    bool blocked=!ready || !playerObj || !world || gui::inputFrozen() || mc::Client::screenOpen() ||
                 (st.onlyWhileAttacking && !gui::mouseDown(VK_LBUTTON) && !pendingClick && !actionHolding());
    if (!blocked && st.weaponOnly) {
        mc::LocalRef hand(mc::Player(playerObj).mainHandStack());
        if (!hand || !isWeaponId(mc::stk::itemId(hand.get()))) blocked=true;
    }
    // "No Glide": an elytra glide is not a fight, so the aim stands down while flying.
    if (!blocked && st.noGlide && mc::Player(playerObj).gliding()) blocked=true;
    if (blocked) {
        if (blockedSince<0.0) blockedSince=now0;
        if (now0-blockedSince>=kSilentBlockGrace) {
            silentAimRelease(playerObj);
            blockedSince=-1.0;
        }
        return;
    }
    blockedSince=-1.0;
    JNIEnv* env=jni::env();
    if (!env || !mc::sym().living) return;
    const double now=ImGui::GetTime();
    const float dt=(float)std::clamp(now-g_aimLast,0.001,0.1);
    g_aimLast=now;

    mc::Player self(playerObj);
    const Vec3 eye=self.eyePos();
    const float curYaw=self.yaw(), curPitch=self.pitch();
    const float lookYaw=curYaw*0.017453292f, lookPitch=curPitch*0.017453292f;
    const Vec3 cameraLook(-std::sin(lookYaw)*std::cos(lookPitch),-std::sin(lookPitch),std::cos(lookYaw)*std::cos(lookPitch));
    const float halfFov=std::clamp(st.fov,5.f,360.f)*0.5f;
    // A live silent session has a second reference direction: the aim the server is holding. The
    // camera search runs first, so looking at another player still switches to that one; only when
    // the camera holds nothing (it is on a block, or turned away) does the tracked target win. That
    // is what lets a click land on the silent target while the view is somewhere else entirely -
    // the hit is not decided by the camera ray.
    Vec3 silentLook=cameraLook;
    const bool silentRef=st.silent && g_silentAimActive;
    if (silentRef) {
        const float sy=g_silentServerYaw*0.017453292f, sp=g_silentServerPitch*0.017453292f;
        silentLook=Vec3(-std::sin(sy)*std::cos(sp),-std::sin(sp),std::cos(sy)*std::cos(sp));
    }

    float bestAngle=1e9f, wantYaw=0.f, wantPitch=0.f;
    bool found=false;
    Vec3 bestAim{};            // the exact point the click's one-shot solution uses
    mc::LocalRef bestTarget;   // kept for the silent aim's own attack below
    for (int pass=0; pass<2 && !found; ++pass) {
        if (pass==1 && !silentRef) break;
        const Vec3 look = pass==0 ? cameraLook : silentLook;
        for (jobject entity:mc::collectEntities(world,256)) {
            if (!entity) continue;
            const bool isLiving=env->IsInstanceOf(entity,mc::sym().living)==JNI_TRUE;
            const bool isPlayer=isPlayerEntity(env,entity);
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (isLiving && env->IsSameObject(entity,playerObj)!=JNI_TRUE &&
                ((isPlayer&&st.allowPlayers)||(!isPlayer&&st.allowEntities)) &&
                !entityBlocked(entity,isPlayer)) {   // friends / Anti-Bot
                mc::LivingEntity t(entity);
                const mc::Entity::Bounds b=t.interpolatedBounds(1.f);
                if (t.alive() && b.valid && distanceToHitbox(eye,t,1.f)<=st.range) {
                    const Vec3 aim=aimPointFor(st.accuracy,b,eye,look,now,t.eyePos());
                    const Vec3 d=aim-eye;
                    const float dist=d.len();
                    if (dist>0.01f) {
                        const float angle=std::acos(std::clamp(d.dot(look)/dist,-1.f,1.f))*57.29578f;
                        if (angle<=halfFov && angle<bestAngle) {
                            bestAngle=angle; found=true;
                            const SilentAngles solved=silentAimAngles(eye,aim);
                            wantYaw=solved.yaw;
                            wantPitch=solved.pitch;
                            bestAim=aim;
                            bestTarget = mc::LocalRef(env->NewLocalRef(entity));
                        }
                    }
                }
            }
            env->DeleteLocalRef(entity);
        }
    }
    if (!found) {
        // Hold the last aim through a brief target loss instead of releasing on one dropped frame.
        if (g_silentAimActive && g_silentTargetSeenAt >= 0.0 &&
            now-g_silentTargetSeenAt <= kSilentTargetGrace) return;
        silentAimRelease(playerObj);
        return;
    }
    g_silentTargetSeenAt = now;

    const float k=1.f-std::exp(-std::clamp(st.speed,1.f,40.f)*dt);
    if (st.silent) {
        // Continuous silent tracking. The movement-packet transform carries the aim on every tick
        // while a target is in the cone, so the server follows the target between clicks rather than
        // only at the moment of an attack. The camera is left untouched, and no extra look packet is
        // sent for the tracking itself - the rotation rides the packets vanilla already sends.
        //
        // A world action is the player's own camera action and owns the rotation while it lasts.
        // The input hook starts the handoff before the game's tick sends the place / break packet;
        // these calls renew it while the button is held and are the fallback when that hook is not
        // available. A left click anywhere but a block is a combat click: it ends the handoff at
        // once, so the aim is back on the server for the swing this press arms.
        // Pick-level aim: the game resolves every click against the aim itself, which is the whole
        // point - a block action is not started while the crosshair is on the target. The camera
        // handoff exists for the field-write fallback, so it is not used here.
        const bool leftHeld = !pickAim && gui::mouseDown(VK_LBUTTON);
        if (leftHeld) {
            if (g_cameraOnBlock) silentAimActionHold(true, kActionLease);
            else g_actionHoldUntil = -1.0;
        } else if (!pickAim && gui::mouseDown(VK_RBUTTON)) {
            silentAimActionHold(false, kActionLease);
        }
        const bool acting = actionHolding();
        if (playerAttackTapped()) {
            // The camera already on an entity means the game's own attack handles this click: it
            // hits that entity in this same tick (or did, just before the frame), so adding the
            // silent swing would send two attacks for one press.
            if (chargeAtClick && bestTarget && !g_attackSentThisFrame && !acting &&
                !g_cameraOnEntity && !pickAim) {
                // One press, one hit. The exact solved angle is pinned, the game's own movement
                // packet carries it on the next tick, and the hit goes out after that packet. No
                // look packet is sent for the hit, so a duplicate rotation can never be the price
                // of a click - and nothing here asks where the camera points: the server's
                // rotation is the silent aim, which is why a hit no longer needs the real view on
                // the target.
                const SilentAngles angles = silentAimAngles(eye, bestAim);
                g_silentAttackWaiting = true;
                g_silentAttackWaitUntil = now + kSilentAttackWait;
                g_silentAttackArmAt = now;
                g_silentAttackAge = self.age();
                g_silentAttackYaw = angles.yaw;
                g_silentAttackPitch = angles.pitch;
                g_silentServerYaw = angles.yaw;
                g_silentServerPitch = angles.pitch;
            }
        }

        // While a click is waiting for its packet, hold the exact aim instead of easing away from
        // it: otherwise the tick that is supposed to carry the shot would send a different angle.
        const bool waiting = g_silentAttackWaiting;
        const float aimYawFrom = waiting ? g_silentAttackYaw : (g_silentAimActive ? g_silentServerYaw : curYaw);
        const float aimPitchFrom = waiting ? g_silentAttackPitch : (g_silentAimActive ? g_silentServerPitch : curPitch);
        const float aimYaw = waiting ? g_silentAttackYaw : aimYawFrom + wrapDeg(wantYaw - aimYawFrom) * k;
        const float aimPitch = waiting ? g_silentAttackPitch : (st.vertical
            ? std::clamp(aimPitchFrom + (wantPitch - aimPitchFrom) * k, -90.f, 90.f)
            : curPitch);
        g_silentServerYaw = aimYaw;
        g_silentServerPitch = aimPitch;
        // The body and head follow the aim; only the camera stays where the player put it.
        g_silentPoseYaw = aimYaw;
        // While a world action runs the aim is solved but not told to the server: the camera is what
        // the action is being validated against, and the next update after the handoff hands the
        // aim back through one ordinary movement packet.
        if (!acting) silentAimSend(playerObj, aimYaw, aimPitch);
        // The crosshair field is set whether or not the pick-level transform is installed: the HUD
        // attack indicator and the block outline read that one field, and the transform's recompute
        // can legitimately come back with the camera's block pick (the aim's target outside the
        // game's own entity range, a ray that only grazes the hitbox). The write keeps the game's own
        // pick when it already holds this entity, so the transform's exact hit vector is preserved
        // when it did its job - and when it did not, this is what removes the outline of the block
        // under the camera and lights the attack indicator on the aim instead.
        mc::Client::setCrosshairTargetEntity(bestTarget.get());

        // The tick that carries the pinned aim is the gate for the hit, never the camera.
        // Entity.age changes once per game tick, and sendMovementPackets runs inside that tick with
        // either a Look or a Full packet holding the pinned angle (or no packet at all when the
        // server already had it). Once the age has changed, the server is holding the aim this hit
        // is validated against. The fallback (no transform) instead waits until its own look packet
        // with the pinned angle has actually gone out.
        if (g_silentAttackWaiting && bestTarget && !g_attackSentThisFrame && !acting && !pickAim) {
            const int age = self.age();
            const bool carried = g_silentUsingPackets
                ? (age >= 0 ? age != g_silentAttackAge
                            : now - g_silentAttackArmAt >= 0.02)
                : (g_silentSentValid &&
                   std::abs(angleDeltaDeg(g_silentSentYaw, g_silentAttackYaw)) <= 0.01f &&
                   std::abs(g_silentSentPitch - g_silentAttackPitch) <= 0.01f);
            const mc::Entity::Bounds bounds = mc::LivingEntity(bestTarget.get()).interpolatedBounds(1.f);
            if (carried &&
                silentRayHitsBounds(eye, g_silentAttackYaw, g_silentAttackPitch, bounds, st.range + 0.5f) &&
                mc::Client::attackEntity(bestTarget.get())) {
                self.swingHand();
                g_attackSentThisFrame = true;
                // attackEntity resets the game's attack cooldown, but this frame's snapshot was
                // taken before the hit. Without this the next frame would still read the pre-hit
                // charge and a second click there could swing through the cooldown.
                chargeReadyPrev = false;
                g_silentAttackWaiting = false;
                g_silentAttackWaitUntil = -1.0;
                g_silentAttackArmAt = -1.0;
                g_silentAttackAge = -1;
            }
        }
        // A click that could not be carried (the target left the reach, the world paused, a very
        // slow frame) is dropped here instead of being kept for a later, unrelated swing. This runs
        // after the attempt, so a frame that is longer than the window still gets its hit.
        if (g_silentAttackWaiting && now > g_silentAttackWaitUntil) {
            g_silentAttackWaiting = false;
            g_silentAttackWaitUntil = -1.0;
            g_silentAttackArmAt = -1.0;
            g_silentAttackAge = -1;
        }
        return;
    }
    // Switching out of silent mode must release its pose and packet state immediately.
    silentAimRelease(playerObj);
    // Normal AimAssist: rotate the real view. Minecraft's own movement packet carries
    // this rotation to the server every tick and the client's attack ray uses it too.
    const float newYaw = curYaw + wrapDeg(wantYaw - curYaw) * k;
    const float newPitch = st.vertical ? std::clamp(curPitch + (wantPitch - curPitch) * k, -90.f, 90.f) : curPitch;
    self.setYaw(newYaw);
    if (st.vertical) self.setPitch(newPitch);
}

// ---- StaticFov ----------------------------------------------------------------------------
// The vanilla FOV slider is validated/clamped to 30..110. Writing the option's raw
// value field bypasses that clamp so a larger field of view actually takes effect.
// Switching the module off restores the value that was in place before we applied
// ours, so the field of view never stays stuck at an extended value.
int g_fovBeforeStatic = -1;
int g_fovAppliedValue = -1;
bool g_fovApplied = false;

void updateStaticFov() {
    const bool ready = jni::ready() && mc::frame::valid() && mc::frame::resolved();
    if (!g_staticFov) {
        if (g_fovApplied && ready) {
            jobject options = mc::Client::options();
            if (options && mc::sym().opt_fov && g_fovBeforeStatic >= 0) {
                const int current = (int)std::lround(
                    mc::opts::getDoubleOption(options, mc::sym().opt_fov, (double)g_fovBeforeStatic));
                // Only put it back while our own value is still the one in place, so a
                // field of view changed by hand in the video settings is never clobbered.
                if (current == g_fovAppliedValue)
                    mc::opts::setIntOptionRaw(options, mc::sym().opt_fov, g_fovBeforeStatic);
            }
            g_fovApplied = false;
            g_fovAppliedValue = -1;
        }
        return;
    }
    if (!ready) return;
    jobject options = mc::Client::options();
    if (!options || !mc::sym().opt_fov) return;
    if (!g_fovApplied) {
        g_fovBeforeStatic = (int)std::lround(mc::opts::getDoubleOption(options, mc::sym().opt_fov, 70.0));
        g_fovApplied = true;
    }
    const int want = std::clamp((int)std::lround(g_staticFovSettings.fov), 30, 179);
    mc::opts::setIntOptionRaw(options, mc::sym().opt_fov, want);
    g_fovAppliedValue = want;
}

// Vanilla refuses your own attack when a cobweb block sits between you and the target,
// because the block pick wins over the entity pick. Ghost Hand only helps a hit YOU are
// making — it never attacks on its own (that is what Triggerbot is for). The click has to
// land on the cobweb itself: while the attack button is held and the game's own pick is a
// COBWEB, the closest entity standing inside a web on that line takes the hit instead.
double g_ghostNextAt = 0.0;
// One assist per physical press: set on the press, cleared only when the button comes back up.
bool g_ghostLatched = false;

// ---- Drag Click Friend ----------------------------------------------------------------------
// Hold attack and the click rate ramps from Minimum CPS to Maximum CPS; with a Drag Click Pause
// set it holds Maximum for a second, stops for that long and then starts the ramp again. Every
// click is posted to the game window as an ordinary left click, so the game performs its own
// attack, block break or item use — the module never fakes a swing of its own.
// The pacing itself (ramp, pause cycle, click schedule) lives in click_pace.h, where
// test/click_pace_check.cpp exercises it without a game.
pace::Timer g_dragTimer;
double g_dragHeldAt = 0.0;
bool g_dragHeld = false;
bool g_dragButtonDown = false;
double g_dragReleaseAt = 0.0;
constexpr int kDragSlotsPerFrame = 6;   // a skipped frame owes clicks, not an endless burst

void updateDragClick() {
    const bool usable = g_dragClick && !gui::inputFrozen() && jni::ready() &&
                        mc::frame::valid() && mc::frame::resolved();
    const double now = ImGui::GetTime();
    // Only the player's own hold starts a drag click, and never while a game screen is open.
    const bool holding = usable && gui::mouseDown(VK_LBUTTON) && !mc::Client::screenOpen();
    if (!holding) {
        if (g_dragButtonDown) { hooks::syntheticClick(false); g_dragButtonDown = false; }
        g_dragHeld = false;
        g_dragTimer.reset();
        return;
    }
    if (!g_dragHeld) { g_dragHeld = true; g_dragHeldAt = now; g_dragTimer.reset(); }
    // Let go of the previous click first: a press and a release inside one game tick are read as
    // no click at all, so the button is held for part of the interval and released in between.
    if (g_dragButtonDown) { hooks::syntheticClick(false); g_dragButtonDown = false; }
    const DragClickSettings& st = g_dragClickSettings;
    const pace::Cycle cycle = pace::cycleAt(st.minCps, st.maxCps, st.acceleration, st.pauseSeconds,
                                            now - g_dragHeldAt);
    if (!cycle.clicking || cycle.cps <= 0.0) return;
    const double interval = 1.0 / cycle.cps;
    // Several slots can come due in one frame, and every one of them is owed a click. A scheduler
    // that can only answer yes/no per frame throws away every slot a slow frame skipped, which caps
    // the module at the frame rate - that is why a card set to 40 CPS topped out around 8. Each due
    // slot is a complete click here, bounded so one hitch cannot queue an endless burst.
    const int slots = pace::slotsDue(g_dragTimer, now, interval, kDragSlotsPerFrame);
    if (slots <= 0) return;
    for (int i = 0; i < slots; ++i) {
        hooks::syntheticClick(true);
        hooks::syntheticClick(false);
    }
    g_dragReleaseAt = now;
}

// Misc: attack whatever the crosshair ray reaches from a range vanilla never allows. Vanilla's
// own pick stops at the entity interaction range (~3 blocks), so the target is found here
// instead: the nearest living entity the look ray passes through. Every swing rolls a fresh
// reach between 3 blocks and Max Reach, so the packet range never sits on one number. The
// attack still only goes out while the player holds the attack key; the server decides what
// it accepts.


// Distance from the eye along the look ray to where it first touches the entity's hitbox,
// or -1 when the ray misses the box. This is the crosshair test vanilla's own entity pick
// performs, with the range cap lifted to the rolled reach.
float rayHitDistance(const Vec3& eye, const Vec3& look, const mc::Entity::Bounds& b) {
    if (!b.valid) return -1.f;
    float tmin = 0.f, tmax = 1e9f;
    const float o[3] = { eye.x, eye.y, eye.z };
    const float d[3] = { look.x, look.y, look.z };
    const float lo[3] = { b.min.x, b.min.y, b.min.z };
    const float hi[3] = { b.max.x, b.max.y, b.max.z };
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(d[i]) < 1e-8f) { if (o[i] < lo[i] || o[i] > hi[i]) return -1.f; continue; }
        float t1 = (lo[i] - o[i]) / d[i], t2 = (hi[i] - o[i]) / d[i];
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1); tmax = std::min(tmax, t2);
        if (tmin > tmax) return -1.f;
    }
    return tmin;
}

// Is the straight line from the eye to `to` clear of solid blocks? The reach is longer than
// vanilla's, but the hit still may not travel through a wall: the old code attacked whatever the
// look ray passed through, so a player standing behind a block was hit through it. The march is
// coarse (a step every 0.2 blocks) because it only has to tell "clear" from "a wall in the way".
bool rayReachesThroughAir(const Vec3& eye, const Vec3& to) {
    const Vec3 delta = to - eye;
    const float dist = delta.len();
    if (!(dist > 0.2f)) return true;
    const Vec3 dir = delta * (1.f / dist);
    for (float t = 0.2f; t < dist - 0.05f; t += 0.2f) {
        const Vec3 p = eye + dir * t;
        if (!isAirId(mc::Client::blockIdAt((int)std::floor(p.x), (int)std::floor(p.y), (int)std::floor(p.z))))
            return false;
    }
    return true;
}


// ---- Macros: keybind plumbing --------------------------------------------------------------
// Macro entries carry no enabled flag: the bind fires the sequence once, exactly like the
// Lunge Macro. They only ever touch the hotbar, so nothing has to be shuffled around.
bool macroTapped(int key) {
    // While a screen (chat included) is open, letters are typing, not binds: no macro fires.
    if (key <= 0 || gui::inputFrozen() || mc::Client::screenOpen()) return false;
    const std::vector<int>& taps = gui::keyTaps();
    return std::find(taps.begin(), taps.end(), key) != taps.end();
}

bool isAirAt(int x, int y, int z) { return isAirId(mc::Client::blockIdAt(x, y, z)); }

// Ordinary "use the held block on the block under the crosshair" (vanilla interaction).
bool placeBlockAtCrosshair(mc::Player& player) {
    mc::LocalRef hit(mc::Client::crosshair());
    if (!hit || mc::hit::typeOf(hit.get()) != 1) return false;
    mc::Client::setItemUseCooldown(4);
    const bool placed = mc::Client::interactAtCrosshair();
    if (placed) player.swingHand();
    return placed;
}

// Bind -> swap to a cobweb and place it on the block under the crosshair.
double g_cobwebStageAt = 0.0, g_cobwebNextAt = 0.0;
bool g_cobwebActive = false;
int g_cobwebOrigSlot = -1;



// Rail -> TNT minecart -> fire beside the cart -> crossbow shot through the flame.
// Every stage is a timed step with its own delay; the shot uses a real projectile solution,
// so it works from any angle instead of only along one axis.
enum class XbowStep { Idle, Rail, Cart, Fire, Shoot, Recharge, Done };
XbowStep g_xbowStep = XbowStep::Idle;
double g_xbowStageAt = 0.0, g_xbowNextAt = 0.0;
int g_xbowOrigSlot = -1;
int g_xbowRailX = 0, g_xbowRailY = 0, g_xbowRailZ = 0;
bool g_xbowRailValid = false;

// The stage delay (the slider) and the one-tick gate that follows a hotbar swap live in
// xbow_timing.h, where test/xbow_timing_check.cpp checks them. The gate is the one floor the
// sequence has, and it is the game's own tick: without it the rail, cart and flame are
// corrected away again. It costs nothing extra, because each stage swaps its item into the hand
// as soon as the stage opens, so that tick passes while the slider's own delay runs.
double g_xbowSwapAt = -1.0;                  // when the hotbar was last changed (-1 = never)

// The flame's own cell, remembered so the Fire step can check the world really kept it.
int g_xbowFireX = 0, g_xbowFireY = 0, g_xbowFireZ = 0;
bool g_xbowFirePending = false;              // lit, waiting for the world to show the block
bool g_xbowFireRetried = false;
bool g_xbowFireKeyDown = false;                        // Fire Place holds the Use Item / Place Block key
double g_xbowFireKeyAt = 0.0;
// The view the player held before the macro began, handed back the moment the shot goes out
// (Look Back): the shot is built on the solved rotation, the camera is not left on it.
bool g_xbowHaveBack = false;
float g_xbowBackYaw = 0.f, g_xbowBackPitch = 0.f;

// A swap the server can follow: the interaction that uses the new item goes out only once the
// slot update has had its own tick.
void xbowSelectSlot(jobject inventory, int slot, double now) {
    selectSlot(inventory, slot);
    g_xbowSwapAt = now;
}

// Defined with the Cart Refill module below. Ending the macro asks it to restock the slot the
// minecart came from straight away, instead of waiting out its refill delay.
void cartRefillRequestNow();

void xbowMacroReset() {
    g_xbowStep = XbowStep::Idle;
    g_xbowOrigSlot = -1;
    g_xbowRailValid = false;
    g_xbowNextAt = ImGui::GetTime() + 0.1;
    g_xbowSwapAt = -1.0;
    if (g_xbowFireKeyDown) { mc::ext::setKey(mc::ext::Key::Use, false); g_xbowFireKeyDown = false; }
    g_xbowFirePending = false;
    g_xbowFireRetried = false;
    g_xbowFireY = 0;
    g_xbowHaveBack = false;
    // The Reload step holds the vanilla use key; whatever ends the sequence must let it go or
    // the game would keep right-clicking with an already-loaded crossbow and fire it.
    mc::ext::setKey(mc::ext::Key::Use, false);
    // The macro is done: let Cart Refill restock the minecart it just used.
    cartRefillRequestNow();
}


// The rail step accepts every rail variant, not only the plain one.
int findAnyRail(jobject list) {
    static const char* const rails[] = { "rail", "powered_rail", "activator_rail", "detector_rail" };
    for (const char* id : rails) {
        const int slot = findHotbarItem(list, id);
        if (slot >= 0) return slot;
    }
    return -1;
}

// Low-arc launch angle that carries a projectile of `speed` (blocks/tick) under `gravity`
// (blocks/tick^2) over `horizontal` blocks and `vertical` blocks of height difference.
bool solveThrowAngle(float speed, float gravity, float horizontal, float vertical, float& pitchDeg) {
    if (horizontal < 0.05f || speed < 0.05f || gravity <= 0.f) return false;
    const float v2 = speed * speed;
    const float disc = v2 * v2 - gravity * (gravity * horizontal * horizontal + 2.f * vertical * v2);
    if (disc < 0.f) return false;                       // out of range for this speed
    const float root = std::sqrt(disc);
    const float tan = (v2 - root) / (gravity * horizontal);   // flat (low) trajectory
    pitchDeg = -std::atan(tan) * 57.29577951f;
    return true;
}

// The launch rotation for the cart, refined against the real flight. `solveThrowAngle` is a
// drag-free closed form, so the arc it gives lands short of the target once drag is applied - which
// is what made the shot fall into the flame instead of reaching the minecart. The closed form is
// only the starting guess here; the pitch is then searched against the same per-tick simulation the
// flight (and the Fire step's cell choice) uses, so the arrow really arrives at the cart.
bool solveXbowShot(const Vec3& eye, const Vec3& cart, float& yawDeg, float& pitchDeg) {
    const Vec3 d = cart - eye;
    const float horizontal = std::sqrt(d.x * d.x + d.z * d.z);
    if (horizontal < 0.05f) return false;
    float pitch = 0.f;
    if (!solveThrowAngle(3.15f, 0.05f, horizontal, d.y, pitch)) return false;
    yawDeg = std::atan2(-d.x, d.z) * 57.29577951f;
    const float yawRad = yawDeg * 0.017453292519943295f;
    auto missDistance = [&](float pitchDegrees) {
        const float p = pitchDegrees * 0.017453292519943295f;
        Vec3 pos = eye;
        Vec3 vel(-std::sin(yawRad) * std::cos(p) * 3.15f,
                 -std::sin(p) * 3.15f,
                 std::cos(yawRad) * std::cos(p) * 3.15f);
        float best = (pos - cart).len();
        for (int tick = 0; tick < 200; ++tick) {
            pos = pos + vel;
            vel = vel * 0.99f;
            vel.y -= 0.05f;
            const float dist = (pos - cart).len();
            if (dist < best) best = dist;
            if (pos.y < cart.y - 2.f) break;
        }
        return best;
    };
    float bestPitch = pitch, bestMiss = missDistance(pitch);
    for (float candidate = pitch - 6.f; candidate <= pitch + 6.f; candidate += 0.25f) {
        const float miss = missDistance(candidate);
        if (miss < bestMiss) { bestMiss = miss; bestPitch = candidate; }
    }
    pitchDeg = bestPitch;
    return true;
}

// True when the crossbow in hand already has a loaded projectile. The component text is the
// safest read: an empty charged_projectiles list means it is not charged.
bool crossbowLoaded(jobject stack) {
    if (!stack) return false;
    const std::string info = mc::Client::objToString(stack);
    if (info.find("charged_projectiles=[]") != std::string::npos) return false;
    return info.find("charged_projectiles=[") != std::string::npos;
}

// A cell a fire can be lit in, at its own row: an empty cell with something to burn on
// underneath. Both halves are the server's own rules (FireBlock.canPlaceAt), so water in
// either half is a placement both sides refuse - the old allowance for a waterlogged cell
// picked flames the light was rejected on.
bool xbowCanHoldFireAt(int cx, int y, int cz) {
    const std::string cell = mc::Client::blockIdAt(cx, y, cz);
    const std::string below = mc::Client::blockIdAt(cx, y - 1, cz);
    return isAirId(cell) && !isAirId(below) && below != "water";
}

// The server resolves the click with its own ray from the eye, so a flame cell is only a cell
// the light can really use when that ray reaches it: inside block interaction range, and with
// nothing solid between the eye and the top face the flame is lit on. At longer range a hill or
// an edge block somewhere along the line is exactly what turned the flame into a client-only
// ghost - the click was refused while the client kept the block it had predicted.
bool xbowFireVisible(const Vec3& eye, const Vec3& hit, float maxDist) {
    const Vec3 d = hit - eye;
    const float len = d.len();
    if (len > maxDist) return false;
    const int steps = (int)(len / 0.3f) + 1;
    for (int i = 1; i < steps; ++i) {
        const Vec3 p = eye + d * ((float)i / (float)steps);
        const std::string id = mc::Client::blockIdAt((int)std::floor(p.x), (int)std::floor(p.y), (int)std::floor(p.z));
        if (!isAirId(id) && id != "water" && id != "lava") return false;   // something in the way
    }
    return true;
}

// The block a flame cell is lit against, the face of it the click lands on (vanilla Direction ids:
// 1 = UP, 2 = NORTH, 3 = SOUTH, 4 = WEST, 5 = EAST), the centre of that face, and its outward normal
// on x/z. A flame above the eye can only be lit from a side face, since the top face is never seen.
struct FireSupport { int x, y, z, face; Vec3 hit; int nx, nz; };
FireSupport fireSupportFor(int cx, int cy, int cz, int face) {
    switch (face) {
        case 2: return { cx, cy, cz + 1, 2, Vec3((float)cx + 0.5f, (float)cy + 0.5f, (float)cz + 1.f), 0, -1 };
        case 3: return { cx, cy, cz - 1, 3, Vec3((float)cx + 0.5f, (float)cy + 0.5f, (float)cz), 0, 1 };
        case 4: return { cx + 1, cy, cz, 4, Vec3((float)cx + 1.f, (float)cy + 0.5f, (float)cz + 0.5f), -1, 0 };
        case 5: return { cx - 1, cy, cz, 5, Vec3((float)cx, (float)cy + 0.5f, (float)cz + 0.5f), 1, 0 };
        default: return { cx, cy - 1, cz, 1, Vec3((float)cx + 0.5f, (float)cy, (float)cz + 0.5f), 0, 0 };
    }
}

// True when the block the player is looking at is really inside the vanilla block interaction
// range. Every stage of the macro is built on that one block, and a placement the server refuses
// leaves the sequence half-finished behind, so the macro refuses to start unless the block it is
// about to build on is actually in reach.
bool xbowBlockInReach(mc::Player& player) {
    mc::LocalRef hit(mc::Client::crosshair());
    if (!hit || mc::hit::typeOf(hit.get()) != 1) return false;
    int bx = 0, by = 0, bz = 0;
    if (!mc::hit::blockPos(hit.get(), bx, by, bz)) return false;
    const Vec3 centre((float)bx + 0.5f, (float)by + 0.5f, (float)bz + 0.5f);
    return (centre - player.eyePos()).len() <= (float)player.blockInteractionRange() + 0.05f;
}


// ---- Trajectory Preview --------------------------------------------------------------------
// Vanilla per-tick defaults for the throwables and for a fully charged crossbow arrow.
struct ProjectilePhysics { float speed = 0.f; float gravity = 0.03f; float drag = 0.99f; };

bool projectilePhysics(const std::string& id, ProjectilePhysics& out) {
    if (id == "ender_pearl")   { out = {1.5f, 0.03f, 0.99f}; return true; }
    if (id == "egg")           { out = {1.5f, 0.03f, 0.99f}; return true; }
    if (id == "snowball")      { out = {1.5f, 0.03f, 0.99f}; return true; }
    if (id == "wind_charge")   { out = {1.5f, 0.00f, 0.99f}; return true; }
    if (id == "experience_bottle") { out = {0.7f, 0.07f, 0.99f}; return true; }
    if (id == "splash_potion" || id == "lingering_potion") { out = {0.5f, 0.05f, 0.99f}; return true; }
    if (id == "crossbow")      { out = {3.15f, 0.05f, 0.99f}; return true; }
    return false;
}

// ---- Trajectory Preview (Visual) ------------------------------------------------------------
// The held projectile's flight, drawn as a ribbon built in the world: a quad between every step of
// the arc, given a real width in blocks, so it reads as a 3D path that thickens and thins with
// distance instead of a flat screen line. The shot is flown from the player's own look direction
// (what the game really launches it on, not the camera's), and the block it lands on is outlined in
// yellow so the landing spot is unmistakable.
struct ScreenFrame {
    Vec3 eye, right, up, forward;
    float focal = 0.f;
    ImVec2 display{};
};
bool screenFrame(float tickDelta, jobject camera, ScreenFrame& out);
bool projectIn(const ScreenFrame& f, const Vec3& p, ImVec2& out);
void drawWorldQuad(ImDrawList* dl, const ScreenFrame& f, const Vec3& a, const Vec3& b,
                   const Vec3& c, const Vec3& d, ImU32 fill, ImU32 edge);
void drawWorldBox(ImDrawList* dl, const ScreenFrame& f, const Vec3& centre,
                  float halfX, float halfY, float halfZ, float yawDeg, ImU32 fill, ImU32 edge);

void drawTrajectoryPreview(ImDrawList* dl, float tickDelta) {
    if (!g_trajPreview || !jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return;
    jobject playerObj = mc::Client::player();
    jobject camera = mc::Client::camera();
    if (!playerObj || !camera) return;
    mc::Player player(playerObj);
    mc::LocalRef hand(player.mainHandStack());
    if (!hand || mc::stk::isEmpty(hand.get())) return;
    const std::string id = mc::stk::itemId(hand.get());
    ProjectilePhysics physics;
    if (!projectilePhysics(id, physics)) return;
    if (id == "crossbow" && !crossbowLoaded(hand.get())) return;   // nothing to shoot yet

    ScreenFrame f;
    if (!screenFrame(tickDelta, camera, f)) return;

    // Vanilla spawns the projectile just in front of the eye along the player's look, at the item's
    // own speed; the camera's direction is only the same thing in first person, which is why the old
    // camera-based arc could sit off the real flight in third person.
    const float yawRad = player.yaw() * 0.017453292519943295f;
    const float pitchRad = player.pitch() * 0.017453292519943295f;
    const Vec3 look(-std::sin(yawRad) * std::cos(pitchRad), -std::sin(pitchRad),
                    std::cos(yawRad) * std::cos(pitchRad));
    const Vec3 eye = player.eyePos();
    Vec3 pos = eye + look * 0.1f;
    Vec3 vel = look * physics.speed;

    std::vector<Vec3> points;
    points.reserve(128);
    points.push_back(pos);
    int landX = 0, landY = 0, landZ = 0;
    bool landed = false;
    for (int tick = 0; tick < 120; ++tick) {
        pos = pos + vel;
        vel = vel * physics.drag;
        vel.y -= physics.gravity;
        if (pos.y < -80.f) break;
        points.push_back(pos);
        const int bx = (int)std::floor(pos.x), by = (int)std::floor(pos.y), bz = (int)std::floor(pos.z);
        if (!isAirId(mc::Client::blockIdAt(bx, by, bz))) { landX = bx; landY = by; landZ = bz; landed = true; break; }
    }

    const ImU32 pathColor = packed(gui::accentColor());
    const ImU32 fillColor = packed(gui::accentColor(), 0.22f);

    // The ribbon. Each segment is a quad whose two edges are offset along the horizontal right of
    // the segment's own direction, so the arc has real thickness in the world; the width tapers, so
    // near steps read thicker than far ones.
    for (size_t i = 1; i < points.size(); ++i) {
        const Vec3 a = points[i - 1], b = points[i];
        Vec3 dir = b - a;
        const float len = dir.len();
        if (len < 1e-4f) continue;
        dir = dir * (1.f / len);
        Vec3 side = dir.cross(Vec3(0.f, 1.f, 0.f));
        const float sideLen = side.len();
        if (sideLen < 1e-4f) side = Vec3(1.f, 0.f, 0.f);
        else side = side * (1.f / sideLen);
        const float t = (float)i / (float)std::max<size_t>(1, points.size());
        const float halfW = 0.05f * (1.f - 0.4f * t);
        drawWorldQuad(dl, f, a + side * halfW, b + side * halfW, b - side * halfW, a - side * halfW,
                      fillColor, pathColor);
    }

    if (landed) {
        // The landing block, outlined in yellow, plus a marker on the path where the shot stops.
        const ImU32 yellow = IM_COL32(255, 214, 60, 235);
        const ImU32 yellowFill = IM_COL32(255, 214, 60, 58);
        drawWorldBox(dl, f, Vec3((float)landX + 0.5f, (float)landY + 0.5f, (float)landZ + 0.5f),
                     0.5f, 0.5f, 0.5f, player.yaw(), yellowFill, yellow);
        ImVec2 mark{};
        if (projectIn(f, points.back(), mark)) {
            dl->AddCircleFilled(mark, 5.f, yellow);
            dl->AddCircle(mark, 8.5f, yellow, 0, 2.f);
        }
    } else if (!points.empty()) {
        ImVec2 mark{};
        if (projectIn(f, points.back(), mark)) dl->AddCircle(mark, 6.f, pathColor, 0, 1.6f);
    }
}

// ---- Walk Trail (Visual) -------------------------------------------------------------------
// The ground the player has actually walked over, as one grey line that fades out behind them. A
// point is sampled from the player's own feet a few times a second and kept for as long as the
// Trail Length says; newest is at the player, oldest is the tail. The world has no depth test in
// this overlay, so "Through Walls" off means: the point was in sight when it was recorded (a short
// block march from the eye at that moment), and points that were behind a wall are dropped.
struct TrailPoint { Vec3 pos; double at = 0.0; bool visible = true; };
std::vector<TrailPoint> g_trailPoints;
double g_trailNextSampleAt = 0.0;

bool trailPointVisible(const Vec3& eye, const Vec3& at) {
    const Vec3 d = at - eye;
    const float dist = d.len();
    if (dist < 0.6f) return true;
    const Vec3 dir = d * (1.f / dist);
    for (float t = 0.6f; t < dist - 0.25f; t += 0.9f) {
        const Vec3 p = eye + dir * t;
        if (!isAirId(mc::Client::blockIdAt((int)std::floor(p.x), (int)std::floor(p.y), (int)std::floor(p.z))))
            return false;
    }
    return true;
}

void updateWalkTrail() {
    if (!g_walkTrail) {
        if (!g_trailPoints.empty()) g_trailPoints.clear();
        return;
    }
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) { g_trailPoints.clear(); return; }
    jobject playerObj = mc::Client::player();
    if (!playerObj) { g_trailPoints.clear(); return; }

    const WalkTrailSettings& st = g_walkTrailSettings;
    const double ttl = std::clamp((double)st.lifetime, 0.5, 10.0);
    const double now = ImGui::GetTime();
    for (auto it = g_trailPoints.begin(); it != g_trailPoints.end();) {
        if (now - it->at > ttl) it = g_trailPoints.erase(it);
        else ++it;
    }
    if (now < g_trailNextSampleAt) return;
    g_trailNextSampleAt = now + 0.05;                       // 20 samples a second follows a sprint
    mc::Entity self(playerObj);
    const Vec3 feet = self.pos();
    if (!g_trailPoints.empty() && (g_trailPoints.back().pos - feet).len() < 0.08f) return;
    TrailPoint point;
    point.pos = feet;
    point.at = now;
    point.visible = trailPointVisible(self.eyePos(), feet);
    g_trailPoints.push_back(point);
    const int cap = std::clamp(st.maxPoints, 20, 600);
    while ((int)g_trailPoints.size() > cap) g_trailPoints.erase(g_trailPoints.begin());
}

void drawWalkTrail(ImDrawList* dl, float tickDelta) {
    if (!g_walkTrail || g_trailPoints.size() < 2) return;
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return;
    jobject camera = mc::Client::camera();
    if (!camera) return;
    tickDelta = std::clamp(tickDelta, 0.f, 1.f);
    const Vec3 camPos = mc::cam::pos(camera);
    Vec3 right, up, forward;
    if (!mc::cam::basis(camera, right, up, forward)) {
        const float yaw = mc::cam::yaw(camera) * 0.017453292519943295f;
        const float pitch = mc::cam::pitch(camera) * 0.017453292519943295f;
        forward = Vec3(-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch));
        right = Vec3(-std::cos(yaw), 0.f, -std::sin(yaw));
        up = right.cross(forward).normalized();
    }
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    float fov = mc::Client::renderFov(tickDelta);
    if (fov < 1.f || fov > 179.f)
        fov = (float)mc::opts::getDoubleOption(mc::Client::options(), mc::sym().opt_fov, 70.0);
    if (display.x <= 1.f || display.y <= 1.f || fov < 1.f || fov > 179.f) return;
    const float focal = (display.y * 0.5f) / std::tan(fov * 0.008726646259971648f);

    const WalkTrailSettings& st = g_walkTrailSettings;
    const double ttl = std::clamp((double)st.lifetime, 0.5, 10.0);
    const double now = ImGui::GetTime();
    const float width = std::clamp(st.width, 1.f, 8.f);
    const float fade = std::clamp(st.fade, 0.f, 1.f);
    // A soft pulse travels along the trail by itself, so the line keeps moving even when you stop.
    const double pulseAge = st.animate ? std::fmod(now * 0.6, 1.0) * ttl : -1.0;
    const auto colour = [&](float alpha) {
        const float a = std::clamp(st.color.w, 0.f, 1.f) * std::clamp(alpha, 0.f, 1.f);
        return IM_COL32((int)(std::clamp(st.color.x, 0.f, 1.f) * 255.f),
                        (int)(std::clamp(st.color.y, 0.f, 1.f) * 255.f),
                        (int)(std::clamp(st.color.z, 0.f, 1.f) * 255.f),
                        (int)(a * 255.f));
    };

    ImVec2 previous{};
    bool havePrevious = false;
    for (size_t i = 0; i < g_trailPoints.size(); ++i) {
        const TrailPoint& point = g_trailPoints[i];
        if (!st.throughWalls && !point.visible) { havePrevious = false; continue; }
        ImVec2 screen{};
        if (!project(point.pos, camPos, right, forward, up, focal, display, screen)) {
            havePrevious = false;
            continue;
        }
        if (havePrevious) {
            const float age = (float)std::clamp((now - point.at) / ttl, 0.0, 1.0);
            float alpha = fade > 0.f ? 1.f - age * fade : 1.f;
            float lineWidth = width;
            if (pulseAge >= 0.0) {
                const double fromPulse = std::abs((now - point.at) - pulseAge);
                if (fromPulse < 0.35) {
                    const float boost = 1.f - (float)(fromPulse / 0.35);
                    alpha = std::min(1.f, alpha + 0.55f * boost);
                    lineWidth += 1.6f * boost;
                }
            }
            dl->AddLine(previous, screen, colour(alpha), lineWidth);
        }
        previous = screen;
        havePrevious = true;
    }
}

// ---- Native cosmetic renderer (Player Model, Wings, Headwear, Capes) ------------------------
// These four used to be nothing but switches on an external Java mod. They are drawn here
// instead: every shape is built in the world out of the player's own position and facing and
// projected onto the screen, exactly the way ESP and the trajectory preview are. Nothing depends
// on the jar, and the game's own models are never touched - these are the client's own shapes.
bool screenFrame(float tickDelta, jobject camera, ScreenFrame& out) {
    if (!camera) return false;
    out.eye = mc::cam::pos(camera);
    if (!mc::cam::basis(camera, out.right, out.up, out.forward)) {
        const float yaw = mc::cam::yaw(camera) * 0.017453292519943295f;
        const float pitch = mc::cam::pitch(camera) * 0.017453292519943295f;
        out.forward = Vec3(-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch));
        out.right = Vec3(-std::cos(yaw), 0.f, -std::sin(yaw));
        out.up = out.right.cross(out.forward).normalized();
    }
    tickDelta = std::clamp(tickDelta, 0.f, 1.f);
    out.display = ImGui::GetIO().DisplaySize;
    float fov = mc::Client::renderFov(tickDelta);
    if (fov < 1.f || fov > 179.f)
        fov = (float)mc::opts::getDoubleOption(mc::Client::options(), mc::sym().opt_fov, 70.0);
    if (out.display.x <= 1.f || out.display.y <= 1.f || fov < 1.f || fov > 179.f) return false;
    out.focal = (out.display.y * 0.5f) / std::tan(fov * 0.008726646259971648f);
    return true;
}

bool projectIn(const ScreenFrame& f, const Vec3& p, ImVec2& out) {
    return project(p, f.eye, f.right, f.forward, f.up, f.focal, f.display, out);
}

void drawWorldQuad(ImDrawList* dl, const ScreenFrame& f, const Vec3& a, const Vec3& b,
                   const Vec3& c, const Vec3& d, ImU32 fill, ImU32 edge) {
    ImVec2 p[4];
    if (!projectIn(f, a, p[0]) || !projectIn(f, b, p[1]) ||
        !projectIn(f, c, p[2]) || !projectIn(f, d, p[3])) return;
    if ((fill & IM_COL32_A_MASK) != 0) dl->AddConvexPolyFilled(p, 4, fill);
    if ((edge & IM_COL32_A_MASK) != 0) dl->AddPolyline(p, 4, edge, ImDrawFlags_Closed, 1.2f);
}

// A box in the player's own frame: `centre` is a world point, the half extents are along the
// player's right / up / forward axes, so the box turns with them.
void drawWorldBox(ImDrawList* dl, const ScreenFrame& f, const Vec3& centre,
                  float halfX, float halfY, float halfZ, float yawDeg, ImU32 fill, ImU32 edge) {
    const float r = yawDeg * 0.017453292519943295f;
    const Vec3 right(-std::cos(r), 0.f, -std::sin(r));
    const Vec3 fwd(-std::sin(r), 0.f, std::cos(r));
    const Vec3 upAxis(0.f, 1.f, 0.f);
    Vec3 corner[8];
    for (int i = 0; i < 8; ++i) {
        const float sx = (i & 1) ? halfX : -halfX;
        const float sy = (i & 2) ? halfY : -halfY;
        const float sz = (i & 4) ? halfZ : -halfZ;
        corner[i] = centre + right * sx + upAxis * sy + fwd * sz;
    }
    const int faces[6][4] = {
        {0, 1, 3, 2}, {4, 5, 7, 6},
        {0, 1, 5, 4}, {2, 3, 7, 6},
        {0, 2, 6, 4}, {1, 3, 7, 5},
    };
    for (const auto& face : faces)
        drawWorldQuad(dl, f, corner[face[0]], corner[face[1]], corner[face[2]], corner[face[3]], fill, edge);
}

// One line of 15 colours: the styles are a look, not a texture atlas, so each style carries its
// own colour and the shapes below vary with it.
ImU32 cosmeticColour(int index, float alpha) {
    static const int table[][3] = {
        {255, 176,  64}, {150, 220, 255}, {255, 214, 170}, {170, 235, 255}, {226, 255, 190},
        {255, 120, 200}, {230, 150, 110}, {255, 226, 140}, {180, 140, 255}, {255,  92,  92},
        {166, 122,  82}, {255, 150, 190}, {255, 244, 210}, {255, 236, 250}, {190, 210, 255},
    };
    const int i = std::clamp(index, 0, 14);
    const float a = std::clamp(alpha, 0.f, 1.f);
    return IM_COL32(table[i][0], table[i][1], table[i][2], (int)(a * 255.f));
}

// A blocky body, the client's own "player model": two legs, a torso and a head, drawn from world
// quads that turn with the player. The game draws the real model itself and it cannot be
// re-textured from the overlay, so this is the shape the Player Model module owns.
void drawBodyModel(ImDrawList* dl, const ScreenFrame& f, const Vec3& feet, float yawDeg, float scale,
                   ImU32 fill, ImU32 edge) {
    const float s = std::clamp(scale, 0.5f, 2.f);
    const auto anchor = [&](float localY) { return feet + Vec3(0.f, localY * s, 0.f); };
    drawWorldBox(dl, f, anchor(0.38f), 0.28f * s, 0.38f * s, 0.14f * s, yawDeg, fill, edge);
    drawWorldBox(dl, f, anchor(1.05f), 0.28f * s, 0.30f * s, 0.15f * s, yawDeg, fill, edge);
    drawWorldBox(dl, f, anchor(1.55f), 0.16f * s, 0.20f * s, 0.16f * s, yawDeg, fill, edge);
}

void drawWingsFor(ImDrawList* dl, const ScreenFrame& f, const Vec3& feet, float yawDeg, double now,
                  bool animate, float flapSpeed, float yOffset, const WingsSettings& st) {
    const float r = yawDeg * 0.017453292519943295f;
    const Vec3 right(-std::cos(r), 0.f, -std::sin(r));
    const Vec3 fwd(-std::sin(r), 0.f, std::cos(r));
    const float s = std::clamp(st.scale, 0.5f, 2.f);
    const float alpha = std::clamp(st.opacity, 0.1f, 1.f);
    const float flap = animate ? std::sin((float)now * std::clamp(flapSpeed, 0.5f, 3.f) * 2.6f) * 0.16f : 0.f;
    // Each style is a span/height pair on the same wing outline: broad and round, long and
    // pointed, small and quick.
    float span = 0.78f, drop = 0.52f, rise = 0.30f;
    switch (st.style) {
        case WingStyle::Angel:     span = 0.82f; drop = 0.55f; rise = 0.34f; break;
        case WingStyle::Dragon:    span = 0.95f; drop = 0.34f; rise = 0.24f; break;
        case WingStyle::Demon:     span = 0.88f; drop = 0.42f; rise = 0.20f; break;
        case WingStyle::Butterfly: span = 0.72f; drop = 0.66f; rise = 0.44f; break;
        case WingStyle::Crystal:   span = 0.76f; drop = 0.50f; rise = 0.38f; break;
        case WingStyle::Feathered: span = 0.86f; drop = 0.60f; rise = 0.30f; break;
    }
    const Vec3 root = feet + Vec3(0.f, (1.28f + std::clamp(yOffset, -1.f, 1.f) * 0.3f) * s, 0.f) - fwd * 0.10f;
    const ImU32 body = cosmeticColour((int)st.style, alpha * 0.75f);
    const ImU32 edge = cosmeticColour((int)st.style, alpha);
    for (int side = 0; side < 2; ++side) {
        const float dir = side == 0 ? -1.f : 1.f;      // -1 = the player's right in camera terms
        const Vec3 sroot = root + right * (0.16f * s * dir);
        const Vec3 tip  = sroot + right * (span * s * dir) + Vec3(0.f, (rise + flap) * s, 0.f);
        const Vec3 mid  = sroot + right * (span * 0.62f * s * dir) - Vec3(0.f, (drop * 0.45f) * s, 0.f);
        const Vec3 low  = sroot + right * (span * 0.24f * s * dir) - Vec3(0.f, drop * s, 0.f);
        drawWorldQuad(dl, f, sroot, tip, mid, low, body, edge);
        // A second, shorter feather layer gives the wing some depth instead of one flat card.
        drawWorldQuad(dl, f, sroot - fwd * 0.06f, tip - fwd * 0.06f,
                      mid - fwd * 0.06f, low - fwd * 0.06f,
                      cosmeticColour((int)st.style, alpha * 0.35f), 0);
    }
}

void drawCapeFor(ImDrawList* dl, const ScreenFrame& f, const Vec3& feet, float yawDeg, double now,
                 const Vec3& velocity, const CapesSettings& st) {
    const float r = yawDeg * 0.017453292519943295f;
    const Vec3 right(-std::cos(r), 0.f, -std::sin(r));
    const Vec3 fwd(-std::sin(r), 0.f, std::cos(r));
    const Vec3 shoulders = feet + Vec3(0.f, 1.42f, 0.f);
    const float speed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
    const float wave = st.movementWave ? std::min(0.28f, speed * 0.09f) : 0.f;
    const float sway = st.physics ? std::sin((float)now * 2.2f) * 0.05f : 0.f;
    const ImU32 body = cosmeticColour((int)st.style, 0.80f);
    const ImU32 edge = cosmeticColour((int)st.style, 1.f);
    const Vec3 left  = shoulders + right * 0.30f - fwd * 0.13f;
    const Vec3 rightP= shoulders - right * 0.30f - fwd * 0.13f;
    const Vec3 bottomL = left  - Vec3(0.f, 0.88f, 0.f) - fwd * (0.10f + wave) + right * sway;
    const Vec3 bottomR = rightP - Vec3(0.f, 0.88f, 0.f) - fwd * (0.10f + wave) - right * sway;
    drawWorldQuad(dl, f, left, rightP, bottomR, bottomL, body, edge);
    // The trim line along the bottom hem, so the cape reads as cloth rather than a flat card.
    const Vec3 hemL = bottomL + Vec3(0.f, 0.07f, 0.f);
    const Vec3 hemR = bottomR + Vec3(0.f, 0.07f, 0.f);
    drawWorldQuad(dl, f, bottomL, bottomR, hemR, hemL, cosmeticColour((int)st.style, 0.95f), 0);
}

void drawHeadwearFor(ImDrawList* dl, const ScreenFrame& f, const Vec3& feet, float yawDeg, double now,
                     const HeadwearSettings& st) {
    static const int family[15] = { 2, 0, 3, 2, 0, 4, 3, 2, 0, 2, 3, 0, 0, 3, 3 };
    // 0 = flat ring, 2 = crown of spikes, 3 = horns / antlers / ears, 4 = orbiting orbs
    const int style = std::clamp((int)st.style, 0, 14);
    const int kind = family[style];
    const float s = std::clamp(st.scale, 0.5f, 2.f);
    const float spin = (float)now * std::clamp(st.rotationSpeed, 0.5f, 3.f);
    const float yaw = yawDeg * 0.017453292519943295f + spin * 0.35f;
    const Vec3 right(-std::cos(yaw), 0.f, -std::sin(yaw));
    const Vec3 fwd(-std::sin(yaw), 0.f, std::cos(yaw));
    const Vec3 anchor = feet + Vec3(0.f, (1.82f + std::clamp(st.yOffset, -0.5f, 0.5f)) * s, 0.f);
    const ImU32 edge = cosmeticColour(style, 0.95f);
    const ImU32 fill = cosmeticColour(style, 0.55f);
    if (kind == 4) {
        for (int i = 0; i < 3; ++i) {
            const float a = spin * 1.5f + (float)i * 2.0943951f;
            const Vec3 orb = anchor + right * (std::cos(a) * 0.34f * s) + fwd * (std::sin(a) * 0.34f * s)
                                    + Vec3(0.f, std::sin(a * 2.f) * 0.07f, 0.f);
            drawWorldBox(dl, f, orb, 0.07f * s, 0.07f * s, 0.07f * s, yawDeg, fill, edge);
        }
        return;
    }
    if (kind == 3) {
        for (int side = 0; side < 2; ++side) {
            const float dir = side == 0 ? -1.f : 1.f;
            const Vec3 base = anchor - Vec3(0.f, 0.10f, 0.f) + right * (0.15f * s * dir);
            const Vec3 up   = base + Vec3(0.f, 0.30f * s, 0.f) + right * (0.06f * s * dir);
            const Vec3 out  = up + right * (0.14f * s * dir);
            const Vec3 down = base + right * (0.10f * s * dir);
            drawWorldQuad(dl, f, base, up, out, down, fill, edge);
        }
        return;
    }
    // Ring (0) and crown (2) are the same band of points; the crown alternates the radius and the
    // height so it comes out as spikes, the ring stays level and wide.
    const int points = 10;
    Vec3 ring[points];
    for (int i = 0; i < points; ++i) {
        const float a = (float)i / (float)points * 6.2831853f;
        const float spike = (kind == 2 && (i % 2) == 1) ? 0.16f : 0.f;
        const float radius = (kind == 2 ? 0.20f : 0.34f) * s + spike * s;
        ring[i] = anchor + right * (std::cos(a) * radius) + fwd * (std::sin(a) * radius)
                         + Vec3(0.f, spike * s - (kind == 0 ? 0.02f : 0.f), 0.f);
    }
    ImVec2 screen[points];
    bool ok = true;
    for (int i = 0; i < points && ok; ++i) ok = projectIn(f, ring[i], screen[i]);
    if (ok) {
        dl->AddConvexPolyFilled(screen, points, fill);
        dl->AddPolyline(screen, points, edge, ImDrawFlags_Closed, 1.4f);
    }
}

void drawNativeCosmetics(ImDrawList* dl, float tickDelta) {
    const bool wantBody = g_playerModel;
    const bool wantWings = g_wings;
    const bool wantHeadwear = g_headwear;
    const bool wantCapes = g_capes;
    if (!wantBody && !wantWings && !wantHeadwear && !wantCapes) return;
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return;
    jobject world = mc::Client::world();
    jobject self = mc::Client::player();
    jobject camera = mc::Client::camera();
    if (!world || !self || !camera) return;
    ScreenFrame frame;
    if (!screenFrame(tickDelta, camera, frame)) return;

    JNIEnv* env = jni::env();
    if (!env) return;
    const double now = ImGui::GetTime();
    for (jobject entity : mc::collectEntities(world, kEntityScan)) {
        if (!entity) continue;
        const bool isSelf = env->IsSameObject(entity, self) == JNI_TRUE;
        const bool isPlayer = isPlayerEntity(env, entity);
        if (!isPlayer || isSelf) { env->DeleteLocalRef(entity); continue; }
        mc::Entity other(entity);
        const Vec3 feet = other.lerpPos(std::clamp(tickDelta, 0.f, 1.f));
        const float yawDeg = other.yaw();
        const float maxDistance = 96.f;
        if ((feet - frame.eye).len() > maxDistance) { env->DeleteLocalRef(entity); continue; }
        if (wantBody) {
            const float scale = std::clamp(g_playerModelSettings.scale, 0.5f, 2.f);
            drawBodyModel(dl, frame, feet, yawDeg, scale,
                          IM_COL32(120, 150, 255, g_playerModelSettings.throughWalls ? 70 : 100),
                          IM_COL32(190, 215, 255, 200));
            if (g_playerModelSettings.showTiming) {
                ImVec2 label{};
                if (projectIn(frame, feet + Vec3(0.f, 2.1f, 0.f), label))
                    dl->AddText(ImVec2(label.x + 6.f, label.y), IM_COL32(190, 215, 255, 220),
                                ("model x" + std::to_string(scale).substr(0, 4)).c_str());
            }
        }
        if (wantWings) drawWingsFor(dl, frame, feet, yawDeg, now, g_wingsSettings.animate,
                                    g_wingsSettings.flapSpeed, g_wingsSettings.yOffset, g_wingsSettings);
        if (wantCapes) drawCapeFor(dl, frame, feet, yawDeg, now, other.velocity(), g_capesSettings);
        if (wantHeadwear) drawHeadwearFor(dl, frame, feet, yawDeg, now, g_headwearSettings);
        env->DeleteLocalRef(entity);
    }
}

// ---- ViewModel: the first-person item, drawn by Passive --------------------------------------
// The game draws the real hand and it cannot be moved from outside the render path, so the
// configured offsets drive the item Passive draws in the hand's own place: the item's texture at
// the position, rotation and scale set on the card. Offsets of 0 leave it exactly where the game
// puts the hand; the off-hand item is the same shape mirrored to the other side.
void drawNativeViewModel(ImDrawList* dl, float tickDelta) {
    if (!g_viewModel) return;
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return;
    if (mc::Client::screenOpen()) return;      // the game hides the hand behind its own screens
    jobject player = mc::Client::player();
    if (!player) return;
    const ViewModelSettings& st = g_viewModelSettings;
    // An untouched card is the vanilla hand: with 0 offsets and scale 1 there is nothing to add,
    // so the drawn item only appears once the card actually changes something (otherwise the
    // game's own hand and ours would sit on top of each other, drawing the item twice).
    const bool untouched = std::abs(st.posX) < 0.001f && std::abs(st.posY) < 0.001f &&
                           std::abs(st.posZ) < 0.001f && std::abs(st.rotX) < 0.001f &&
                           std::abs(st.rotY) < 0.001f && std::abs(st.rotZ) < 0.001f &&
                           std::abs(st.scale - 1.f) < 0.001f;
    if (untouched) return;

    float fov = mc::Client::renderFov(tickDelta, false);
    if (fov < 1.f || fov > 179.f)
        fov = (float)mc::opts::getDoubleOption(mc::Client::options(), mc::sym().opt_fov, 70.0);
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    if (display.x <= 1.f || display.y <= 1.f) return;
    const glow::HandGlow anchor = glow::handGlow(fov, display.x, display.y,
                                                 (float)g_shaderEspSettings.handSize,
                                                 (float)g_shaderEspSettings.handOffsetX,
                                                 (float)g_shaderEspSettings.handOffsetY);
    if (!anchor.valid) return;

    const float scale = std::clamp(st.scale, 0.1f, 2.f);
    const float alpha = std::clamp(st.opacity, 0.1f, 1.f);
    // Pos X/Y/Z are view-space offsets: X across the screen, Y up it, Z towards the camera (which
    // reads as size here, the way a hand pushed towards the lens grows).
    const float dx = st.posX * display.y * 0.22f;
    const float dy = -st.posY * display.y * 0.22f;
    const float size = anchor.size * scale * (1.f + std::clamp(st.posZ, -1.5f, 1.5f) * 0.18f);
    // Rot X/Y squash the flat card the way a turn away from the camera would; Rot Z turns it in
    // the plane of the screen, which is the one a first-person item visibly does.
    const float squashX = std::max(0.12f, std::abs(std::cos(st.rotY * 0.017453292519943295f)));
    const float squashY = std::max(0.12f, std::abs(std::cos(st.rotX * 0.017453292519943295f)));
    const float radians = st.rotZ * 0.017453292519943295f;
    const float cosR = std::cos(radians), sinR = std::sin(radians);

    const auto drawHand = [&](const std::string& itemId, float centreX) {
        if (itemId.empty()) return;
        const ImTextureID texture = itemicons::get(itemId);
        if (!texture) return;
        const float halfX = size * 0.5f * squashX;
        const float halfY = size * 0.5f * squashY;
        const float corners[4][2] = { {-halfX,-halfY}, {halfX,-halfY}, {halfX,halfY}, {-halfX,halfY} };
        ImVec2 p[4];
        for (int i = 0; i < 4; ++i) {
            const float x = corners[i][0] * cosR - corners[i][1] * sinR;
            const float y = corners[i][0] * sinR + corners[i][1] * cosR;
            p[i] = ImVec2(centreX + dx + x, anchor.y + dy + y);
        }
        dl->AddImageQuad(texture, p[0], p[1], p[2], p[3],
                         ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1),
                         IM_COL32(255, 255, 255, (int)(alpha * 255.f)));
    };
    mc::LivingEntity living(player);
    if (st.offHand) {
        mc::LocalRef off(living.offHandStack());
        if (off && !mc::stk::isEmpty(off.get()))
            drawHand(mc::stk::itemId(off.get()), display.x - anchor.x);      // the other hand
    }
    mc::LocalRef main(living.mainHandStack());
    if (main && !mc::stk::isEmpty(main.get())) drawHand(mc::stk::itemId(main.get()), anchor.x);
}

// ---- Item Scroll ---------------------------------------------------------------------------
// While Shift and the left button are held over a container screen, the click is repeated so
// stacks move as the cursor sweeps over them (vanilla quick-move). Input is posted to the
// game window, so the game performs its own slot handling. Container screens are detected by
// reflection (a field of the screen holds the ScreenHandler); chat, the pause menu and the
// settings screens have no such field and are never clicked in.
double g_scrollNextAt = 0.0;
std::unordered_map<std::string, bool> g_handledScreenCache;
jmethodID g_refl_getName = nullptr, g_refl_getDeclaredFields = nullptr, g_refl_getType = nullptr;

bool resolveReflectionIds(JNIEnv* e) {
    if (g_refl_getName && g_refl_getDeclaredFields && g_refl_getType) return true;
    jclass classCls = e->FindClass("java/lang/Class");
    jclass fieldCls = e->FindClass("java/lang/reflect/Field");
    if (!classCls || !fieldCls) {
        jni::clearException(e, "reflection classes");
        if (classCls) e->DeleteLocalRef(classCls);
        if (fieldCls) e->DeleteLocalRef(fieldCls);
        return false;
    }
    g_refl_getName = e->GetMethodID(classCls, "getName", "()Ljava/lang/String;");
    g_refl_getDeclaredFields = e->GetMethodID(classCls, "getDeclaredFields", "()[Ljava/lang/reflect/Field;");
    g_refl_getType = e->GetMethodID(fieldCls, "getType", "()Ljava/lang/Class;");
    jni::clearException(e, "reflection method ids");
    e->DeleteLocalRef(classCls);
    e->DeleteLocalRef(fieldCls);
    return g_refl_getName && g_refl_getDeclaredFields && g_refl_getType;
}

bool isHandledScreen(jobject screen) {
    JNIEnv* e = jni::env();
    if (!e || !screen || !mc::sym().screenHandler || !resolveReflectionIds(e)) return false;
    jclass cls = e->GetObjectClass(screen);
    jni::clearException(e, "screen class");
    if (!cls) return false;
    jstring name = (jstring)e->CallObjectMethod(cls, g_refl_getName);
    jni::clearException(e, "screen class name");
    const std::string clsName = jni::fromJString(e, name);
    if (name) e->DeleteLocalRef(name);
    const auto cached = g_handledScreenCache.find(clsName);
    if (cached != g_handledScreenCache.end()) { e->DeleteLocalRef(cls); return cached->second; }

    bool handled = false;
    for (jclass current = cls; current && !handled;) {
        jobjectArray fields = (jobjectArray)e->CallObjectMethod(current, g_refl_getDeclaredFields);
        jni::clearException(e, "declared fields");
        if (fields) {
            const jsize count = e->GetArrayLength(fields);
            for (jsize i = 0; i < count && !handled; ++i) {
                jobject field = e->GetObjectArrayElement(fields, i);
                if (!field) continue;
                jobject type = e->CallObjectMethod(field, g_refl_getType);
                jni::clearException(e, "field type");
                if (type) {
                    // The field's declared type must BE the handler (or a subclass of it).
                    // Asking it the other way round would also match a field declared as
                    // Object, which would make the pause menu look clickable.
                    if (e->IsAssignableFrom(mc::sym().screenHandler, (jclass)type) == JNI_TRUE) handled = true;
                    jni::clearException(e, "assignable");
                    e->DeleteLocalRef(type);
                }
                e->DeleteLocalRef(field);
            }
            e->DeleteLocalRef(fields);
        }
        jclass parent = e->GetSuperclass(current);
        jni::clearException(e, "super class");
        e->DeleteLocalRef(current);
        // Every local reference must be released: this runs inside the render
        // frame, and leaking one per frame would overflow the JVM's local
        // reference table (512 entries) in a matter of seconds.
        if (handled || !parent) {
            if (parent) e->DeleteLocalRef(parent);
            current = nullptr;
        } else {
            current = parent;
        }
    }
    if (g_handledScreenCache.size() < 64) g_handledScreenCache[clsName] = handled;
    return handled;
}

void updateItemScroll() {
    if (!g_itemScroll || gui::inputFrozen()) return;
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return;
    if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == 0) return;
    if (!gui::mouseDown(VK_LBUTTON)) return;                 // the player's own click starts it
    if (!mc::Client::screenOpen()) return;
    JNIEnv* e = jni::env();
    jobject client = mc::frame::client();
    if (!e || !client || !mc::sym().mc_currentScreen) return;
    jobject screen = e->GetObjectField(client, mc::sym().mc_currentScreen);
    jni::clearException(e, "currentScreen for Item Scroll");
    if (!screen) return;
    const bool handled = isHandledScreen(screen);
    e->DeleteLocalRef(screen);
    if (!handled) return;                                    // never click blindly in a menu
    const double now = ImGui::GetTime();
    if (now < g_scrollNextAt) return;
    g_scrollNextAt = now + 0.1;
    hooks::syntheticClick(true);
    hooks::syntheticClick(false);
}

// ---- Nametags -----------------------------------------------------------------------------
std::string shortItemId(const std::string& id) {
    std::string out = id;
    for (char& c : out) if (c == '_') c = ' ';
    return out;
}

// Vanilla durability-bar colour: the hue is (fraction remaining / 3), saturation and value 1.
void hsvToRgb(float h, float s, float v, float& r, float& g, float& b) {
    const int i = (int)std::floor(h * 6.f);
    const float f = h * 6.f - (float)i;
    const float p = v * (1.f - s), q = v * (1.f - f * s), t = v * (1.f - (1.f - f) * s);
    switch (((i % 6) + 6) % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
}

// Reads one value out of the `trim={...}` section of an ItemStack's component text, e.g.
// `trim={material:"minecraft:iron",pattern:"minecraft:coast"}` -> "coast".
std::string trimField(const std::string& info, const char* key) {
    const size_t at = info.find("trim=");
    if (at == std::string::npos) return {};
    const size_t k = info.find(key, at);
    if (k == std::string::npos) return {};
    size_t i = k + std::string(key).size();
    while (i < info.size() && (info[i] == '=' || info[i] == ':' || info[i] == '"' || info[i] == ' ')) ++i;
    const size_t start = i;
    while (i < info.size() && (std::isalnum((unsigned char)info[i]) || info[i] == '_' || info[i] == ':' || info[i] == '.')) ++i;
    std::string value = info.substr(start, i - start);
    const size_t colon = value.rfind(':');
    return colon == std::string::npos ? value : value.substr(colon + 1);
}

// Reads an integer component out of an ItemStack's toString text ("minecraft:damage=12").
// The key must start a component, so "damage" never matches the tail of "max_damage".
bool componentInt(const std::string& info, const char* key, int& out) {
    const size_t keyLen = std::string(key).size();
    size_t at = info.find(key);
    while (at != std::string::npos) {
        // The key must not be the tail of a longer identifier ("damage" inside
        // "max_damage"); a ':' from "minecraft:damage" is a valid separator.
        const char before = at == 0 ? '[' : info[at - 1];
        if (!(std::isalnum((unsigned char)before) || before == '_')) {
            size_t i = at + keyLen;
            if (i < info.size() && info[i] == '=') {
                ++i;
                long value = 0; bool any = false;
                while (i < info.size() && std::isdigit((unsigned char)info[i])) { value = value * 10 + (info[i] - '0'); ++i; any = true; }
                if (any) { out = (int)value; return true; }
            }
        }
        at = info.find(key, at + 1);
    }
    return false;
}

// Trim material colours, used to tint the trim overlay the way the game does.
ImU32 trimTintFrom(const std::string& material) {
    static const struct { const char* id; int r, g, b; } materials[] = {
        {"iron", 216, 216, 216}, {"gold", 255, 240, 74}, {"diamond", 74, 237, 217},
        {"netherite", 75, 65, 61}, {"emerald", 80, 239, 114}, {"copper", 240, 165, 101},
        {"amethyst", 160, 139, 209}, {"quartz", 235, 229, 222}, {"redstone", 170, 30, 30},
        {"lapis", 38, 97, 156}, {"resin", 240, 150, 90}
    };
    for (const auto& material2 : materials)
        if (material == material2.id) return IM_COL32(material2.r, material2.g, material2.b, 255);
    return IM_COL32(255, 255, 255, 255);
}

// The potion id and any custom colour out of a stack's component text. The game dumps a potion as
// `potion_contents={potion:"minecraft:strong_strength", custom_color:16711680}`, which is what the
// icon tint is read from - a potion with no custom colour gets the vanilla colour of its effect.
void potionParts(const std::string& info, std::string& potionId, ImU32& customTint) {
    const size_t at = info.find("potion_contents=");
    if (at == std::string::npos) return;
    const size_t end = info.find('}', at);
    const std::string section = info.substr(at, end == std::string::npos ? std::string::npos : end - at);
    const size_t key = section.find("potion:");
    if (key != std::string::npos) {
        const size_t open = section.find('"', key);
        const size_t close = open == std::string::npos ? std::string::npos : section.find('"', open + 1);
        if (close != std::string::npos) potionId = section.substr(open + 1, close - open - 1);
    }
    const size_t colourKey = section.find("custom_color:");
    if (colourKey != std::string::npos) {
        size_t i = colourKey + 13;
        long value = 0;
        bool any = false;
        while (i < section.size() && std::isdigit((unsigned char)section[i])) {
            value = value * 10 + (section[i] - '0');
            ++i;
            any = true;
        }
        if (any) customTint = 0xFF000000u | (ImU32)(value & 0xFFFFFF);
    }
}

// One item icon with its Minecraft durability bar and, for enchanted gear, a pulsing glint.
// Trims get their pattern overlay tinted with the trim material. A texture that could not be
// loaded falls back to the compact text name, so a slot is never silently hidden - and the
// durability bar draws in both cases, because the wear is read from the stack itself.
void drawNametagItem(ImDrawList* dl, jobject stack, float x, float y, float size,
                     float scale, ImFont* font, float fontSize, const char* fallbackLabel,
                     bool useIcons) {
    if (!stack) return;
    const std::string id = mc::stk::itemId(stack);
    const std::string info = mc::Client::objToString(stack);
    // Potions: the bottle the pack draws plus the tinted liquid overlay the game itself uses, so a
    // splash Strength potion shows the Strength colour instead of one generic bottle. Both textures
    // come from the resource pack, so texture packs are honoured, and the tint is the potion's own.
    itemicons::PotionIcon potion{};
    if (useIcons && !itemicons::potionBaseId(id).empty()) {
        std::string potionId;
        ImU32 customTint = 0;
        potionParts(info, potionId, customTint);
        potion = itemicons::potion(id, potionId, customTint);
    }
    const ImTextureID texture = potion.base ? potion.base : (useIcons ? itemicons::get(id) : 0);
    const ImVec2 p0(x, y), p1(x + size, y + size);
    if (texture) {
        // The icon textures are uploaded with the PNG's rows already flipped for GL, so sampling them
        // with ImGui's top-first UVs drew every icon upside down. The V axis is inverted here.
        const ImVec2 uv0(0.f, 1.f), uv1(1.f, 0.f);
        dl->AddImage(texture, p0, p1, uv0, uv1, IM_COL32(255, 255, 255, 255));
        if (potion.overlay) dl->AddImage(potion.overlay, p0, p1, uv0, uv1, potion.tint);
        if (info.find("trim=") != std::string::npos) {
            const std::string pattern = trimField(info, "pattern");
            const ImTextureID trimTexture = pattern.empty() ? 0 : itemicons::getTrim(pattern);
            if (trimTexture) {
                const std::string material = trimField(info, "material");
                dl->AddImage(trimTexture, p0, p1, uv0, uv1, trimTintFrom(material));
            }
        }
        const bool enchanted = info.find("enchantments=") != std::string::npos ||
                               info.find("enchantment_glint_override=true") != std::string::npos;
        if (enchanted) {
            // A soft sheen rather than a heavy purple wash, so the icon stays readable.
            const float pulse = 0.08f + 0.06f * std::sin((float)ImGui::GetTime() * 2.2f);
            const int alpha = (int)(std::clamp(pulse, 0.f, 0.20f) * 255.f);
            dl->AddImage(texture, p0, p1, uv0, uv1, IM_COL32(163, 103, 255, alpha));
        }
    } else {
        // No icon texture (a plugin's custom item, a name nothing matches, or icons off): the slot
        // still shows the item's name, so the piece of gear is never silently hidden.
        const std::string label = fallbackLabel && *fallbackLabel ? std::string(fallbackLabel) : shortItemId(id);
        if (!label.empty() && font) {
            const float tw = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, label.c_str()).x;
            const float tx = x + std::max(0.f, (size - tw) * 0.5f);
            const float ty = y + std::max(0.f, (size - fontSize) * 0.5f);
            dl->AddText(font, fontSize, ImVec2(tx, ty), IM_COL32(190, 183, 192, 255), label.c_str());
        }
    }
    // Durability bar, whichever way the slot was drawn. It used to live only behind the icon, so
    // an item whose texture could not be read - a plugin's renamed gear, exactly the gear that
    // most needs reading - showed no wear at all.
    int maxDamage = mc::stk::maxDamage(stack);
    int damage = mc::stk::damage(stack);
    if (maxDamage <= 0) componentInt(info, "max_damage", maxDamage);
    if (damage <= 0) componentInt(info, "damage", damage);
    if (maxDamage > 0) {
        if (damage < 0) damage = 0;
        if (damage > maxDamage) damage = maxDamage;
        const float left = std::clamp((float)(maxDamage - damage) / (float)maxDamage, 0.f, 1.f);
        const float barH = std::max(1.f, 2.f * scale);
        const ImVec2 b0(x, y + size + 1.f), b1(x + size, y + size + 1.f + barH);
        float r = 0.f, g = 0.f, b = 0.f;
        hsvToRgb(left / 3.f, 1.f, 1.f, r, g, b);
        dl->AddRectFilled(b0, b1, IM_COL32(0, 0, 0, 220));
        dl->AddRectFilled(b0, ImVec2(b0.x + size * left, b1.y), IM_COL32((int)(r * 255.f), (int)(g * 255.f), (int)(b * 255.f), 255));
    }
}

// A billboard above each player's head. Item/armor "icons" are drawn as compact
// text labels (with durability) because the overlay has no item texture atlas.
void drawNametags(ImDrawList* dl, float tickDelta) {
    if (!g_nametags || !jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return;
    jobject world = mc::Client::world();
    jobject player = mc::Client::player();
    jobject camera = mc::Client::camera();
    if (!world || !player || !camera) return;
    JNIEnv* env = jni::env();
    if (!env) return;
    tickDelta = std::clamp(tickDelta, 0.f, 1.f);
    const Vec3 camPos = mc::cam::pos(camera);
    Vec3 right, up, forward;
    if (!mc::cam::basis(camera, right, up, forward)) {
        const float yaw = mc::cam::yaw(camera) * 0.017453292519943295f;
        const float pitch = mc::cam::pitch(camera) * 0.017453292519943295f;
        forward = Vec3(-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch));
        right = Vec3(-std::cos(yaw), 0.f, -std::sin(yaw));
        up = right.cross(forward).normalized();
    }
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    float fov = mc::Client::renderFov(tickDelta);
    if (fov < 1.f || fov > 179.f)
        fov = (float)mc::opts::getDoubleOption(mc::Client::options(), mc::sym().opt_fov, 70.0);
    if (display.x <= 1.f || display.y <= 1.f || fov < 1.f || fov > 179.f) return;
    const float focal = (display.y * 0.5f) / std::tan(fov * 0.008726646259971648f);
    const NametagsSettings& st = g_nametagsSettings;
    ImFont* font = gui::fontRegular();
    if (!font) font = ImGui::GetFont();
    ImFont* bold = gui::fontSemibold();
    if (!bold) bold = font;

    // Single Show: only the player or entity the crosshair is actually on gets a board. The
    // game's own crosshairTarget is the exact pick vanilla uses, so the one board that is drawn
    // matches what the player is aiming at instead of every player in view.
    mc::LocalRef singleHit(st.singleShow ? mc::Client::crosshair() : nullptr);
    mc::LocalRef singleTarget;
    if (st.singleShow && singleHit && mc::hit::typeOf(singleHit.get()) == 2)
        singleTarget = mc::LocalRef(mc::hit::entity(singleHit.get()));

    for (jobject entity : mc::collectEntities(world, kEntityScan)) {
        if (!entity) continue;
        const bool isPlayer = isPlayerEntity(env, entity);
        if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(entity); continue; }
        if (!isPlayer || env->IsSameObject(entity, player) == JNI_TRUE) { env->DeleteLocalRef(entity); continue; }
        if (st.singleShow && (!singleTarget || env->IsSameObject(entity, singleTarget.get()) != JNI_TRUE)) {
            env->DeleteLocalRef(entity);
            continue;
        }
        mc::LivingEntity living(entity);
        if (!living.alive()) { env->DeleteLocalRef(entity); continue; }
        const mc::Entity::Bounds bounds = living.interpolatedBounds(tickDelta);
        if (!bounds.valid) { env->DeleteLocalRef(entity); continue; }
        const Vec3 centerPos((bounds.min.x + bounds.max.x) * 0.5f,
                             (bounds.min.y + bounds.max.y) * 0.5f,
                             (bounds.min.z + bounds.max.z) * 0.5f);
        const float distance = (centerPos - camPos).len();
        if (distance > st.maxDistance) { env->DeleteLocalRef(entity); continue; }
        ImVec2 anchor;
        if (!project(Vec3(centerPos.x, bounds.max.y + 0.5f, centerPos.z),
                     camPos, right, forward, up, focal, display, anchor)) {
            env->DeleteLocalRef(entity); continue;
        }

        // The board scales with distance so it does not fill the screen up close.
        const float textScale = std::clamp(st.scale * (16.f / std::max(4.f, distance)), 0.45f, 1.4f);
        const float fontSize = 12.f * textScale;
        const float lineH = fontSize + 3.f;

        // ---- board contents: text lines, then real armour and held-item icons ----
        std::vector<std::string> lines;
        if (st.showName) lines.push_back(entityName(entity));
        {
            char buf[64];
            buf[0] = '\0';
            if (st.showHealth && st.showDistance)
                snprintf(buf, sizeof(buf), "HP %.1f / %.0f   %.1f m", living.health(), living.maxHealth(), distance);
            else if (st.showHealth)
                snprintf(buf, sizeof(buf), "HP %.1f / %.0f", living.health(), living.maxHealth());
            else if (st.showDistance)
                snprintf(buf, sizeof(buf), "%.1f m", distance);
            if (buf[0]) lines.push_back(buf);
        }
        // The four equipped armour pieces, in the order a player reads them: helmet first.
        std::vector<mc::LocalRef> armour;
        if (st.showArmor) {
            const int slots[4] = { 5, 4, 3, 2 };   // HEAD, CHEST, LEGS, FEET
            for (int i = 0; i < 4; ++i) {
                mc::LocalRef stack(living.equippedStack(slots[i]));
                if (stack && !mc::stk::isEmpty(stack.get())) armour.push_back(std::move(stack));
            }
        }
        mc::LocalRef held;
        if (st.showHeldItem) {
            mc::LocalRef hand(living.mainHandStack());
            if (hand && !mc::stk::isEmpty(hand.get())) held = std::move(hand);
        }
        if (lines.empty() && armour.empty() && !held) { env->DeleteLocalRef(entity); continue; }

        const auto textWidth = [&](const char* value) {
            return font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, value).x
                        : ImGui::CalcTextSize(value).x;
        };
        const float padX = 6.f * textScale, padY = 4.f * textScale;
        const float rowGap = 3.f * textScale;
        const float iconSize = 16.f * textScale;
        const float iconGap = 3.f * textScale;
        float maxWidth = 0.f;
        for (const std::string& line : lines) maxWidth = std::max(maxWidth, textWidth(line.c_str()));
        if (!armour.empty())
            maxWidth = std::max(maxWidth, armour.size() * (iconSize + iconGap) - iconGap);
        std::string heldLabel;
        if (held) {
            heldLabel = shortItemId(mc::stk::itemId(held.get()));
            const int count = mc::stk::count(held.get());
            if (count > 1) heldLabel += " x" + std::to_string(count);
            maxWidth = std::max(maxWidth, iconSize + iconGap + textWidth(heldLabel.c_str()));
        }
        float contentH = lines.size() * lineH;
        if (!armour.empty()) contentH += iconSize + rowGap;
        if (held) contentH += std::max(iconSize, fontSize) + rowGap;
        const float boxW = maxWidth + padX * 2.f;
        const float boxH = contentH + padY * 2.f;
        const ImVec2 p0(anchor.x - boxW * 0.5f, anchor.y - boxH);
        const ImVec2 p1(anchor.x + boxW * 0.5f, anchor.y);
        const bool isFriendTarget = !g_friends.empty() && entityIsFriend(entity);
        const ImU32 accent = isFriendTarget ? packed(g_espSettings.friendColor) : IM_COL32(255, 120, 96, 255);
        dl->AddRectFilled(p0, p1, IM_COL32(12, 10, 14, 190), 4.f * textScale);
        dl->AddRect(p0, p1, accent, 4.f * textScale, 0, 1.f);
        float ty = p0.y + padY;
        for (size_t i = 0; i < lines.size(); ++i) {
            const float tw = textWidth(lines[i].c_str());
            const ImU32 tint = i == 0 ? IM_COL32(245, 240, 240, 255) : IM_COL32(190, 183, 192, 255);
            ImFont* useFont = (i == 0 && bold) ? bold : font;
            if (useFont) dl->AddText(useFont, fontSize, ImVec2(anchor.x - tw * 0.5f, ty), tint, lines[i].c_str());
            else dl->AddText(ImVec2(anchor.x - tw * 0.5f, ty), tint, lines[i].c_str());
            ty += lineH;
        }
        if (!armour.empty()) {
            float ix = anchor.x - (armour.size() * (iconSize + iconGap) - iconGap) * 0.5f;
            for (const mc::LocalRef& stack : armour) {
                drawNametagItem(dl, stack.get(), ix, ty, iconSize, textScale, font, fontSize, nullptr,
                                g_nametagsSettings.icons);
                ix += iconSize + iconGap;
            }
            ty += iconSize + rowGap;
        }
        if (held) {
            // The held row always keeps its slot and its label (item name and stack count): both
            // come from the stack itself, so neither a plugin hiding the player's name nor a
            // texture that will not load can take the row down. The durability bar draws under
            // the slot whichever way it was filled.
            const float labelW = textWidth(heldLabel.c_str());
            const float rowW = iconSize + iconGap + labelW;
            float ix = anchor.x - rowW * 0.5f;
            drawNametagItem(dl, held.get(), ix, ty, iconSize, textScale, font, fontSize, "",
                            g_nametagsSettings.icons);
            ix += iconSize + iconGap;
            const float textY = ty + (std::max(iconSize, fontSize) - fontSize) * 0.5f;
            if (font) dl->AddText(font, fontSize, ImVec2(ix, textY), IM_COL32(190, 183, 192, 255), heldLabel.c_str());
            else dl->AddText(ImVec2(ix, textY), IM_COL32(190, 183, 192, 255), heldLabel.c_str());
        }
        env->DeleteLocalRef(entity);
    }
}

// ---- HUD (Visual) ---------------------------------------------------------------------------
// Passive's own HUD. Everything here is drawn into the overlay's background draw list, so it sits
// above the frame the game already rendered. The game hides its whole HUD as one switch - its own
// hotbar and bars, but the first-person hand, the chat, the Tab list and the scoreboard with them
// - so this module never touches GameOptions.hudHidden. "Cover Vanilla" draws an opaque plate over
// the game's own hotbar and bar rows instead, and everything else the game draws stays exactly as
// it is.
double g_hudFade = 0.0;               // 0..1 master fade (fades the whole HUD in on enable)
double g_hudItemNameUntil = 0.0;      // held-item plaque timer
std::string g_hudItemNameId;          // last held stack key (change detection)
float g_hudSlotPop[9] = {};           // per-slot selection pop animation
int g_hudPrevSelected = -1;
// Animated bar values, one per row (health, armour, food, XP): the value the row is drawn at,
// sliding towards the real one so losing a heart drains the row instead of snapping it. A
// negative entry is "nothing drawn yet" and seeds itself on the first frame the row is drawn.
float  g_hudBarShown[4] = { -1.f, -1.f, -1.f, -1.f };
double g_hudHeartPulse = 0.0;         // low-health heart beat phase (seconds)

// A rounded filled rectangle by default; the border is skipped entirely when the caller passes a
// fully transparent colour, which keeps the hotbar's hot path branch-light.
void hudRect(ImDrawList* dl, ImVec2 p0, ImVec2 p1, ImU32 fill, float rounding,
             ImU32 border = 0, float borderW = 0.f) {
    dl->AddRectFilled(p0, p1, fill, rounding);
    if (border && (border & IM_COL32_A_MASK)) dl->AddRect(p0, p1, border, rounding, 0, borderW);
}

// ---- HUD glyphs -----------------------------------------------------------------------------
// Small vector glyphs so the bars read at a glance without an icon font. Each is centred on `c`
// with a half-extent of `r` pixels.
void hudGlyphHeart(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircleFilled(ImVec2(c.x - r * 0.42f, c.y - r * 0.28f), r * 0.55f, col, 12);
    dl->AddCircleFilled(ImVec2(c.x + r * 0.42f, c.y - r * 0.28f), r * 0.55f, col, 12);
    dl->AddTriangleFilled(ImVec2(c.x - r * 1.02f, c.y - r * 0.08f), ImVec2(c.x + r * 1.02f, c.y - r * 0.08f),
                          ImVec2(c.x, c.y + r), col);
}
void hudGlyphShield(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddQuadFilled(ImVec2(c.x - r, c.y - r * 0.9f), ImVec2(c.x + r, c.y - r * 0.9f),
                      ImVec2(c.x + r * 0.62f, c.y + r * 0.35f), ImVec2(c.x - r * 0.62f, c.y + r * 0.35f), col);
    dl->AddTriangleFilled(ImVec2(c.x - r * 0.62f, c.y + r * 0.2f), ImVec2(c.x + r * 0.62f, c.y + r * 0.2f),
                          ImVec2(c.x, c.y + r), col);
}
void hudGlyphFood(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircleFilled(ImVec2(c.x - r * 0.3f, c.y - r * 0.3f), r * 0.72f, col, 16);
    dl->AddRectFilled(ImVec2(c.x + r * 0.02f, c.y - r * 0.14f), ImVec2(c.x + r, c.y + r * 0.5f), col, r * 0.3f);
    dl->AddCircleFilled(ImVec2(c.x + r * 0.86f, c.y + r * 0.5f), r * 0.42f, col, 14);
}
void hudGlyphOrb(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircleFilled(c, r * 0.78f, col, 18);
    dl->AddCircle(c, r, col, 18, std::max(1.f, r * 0.28f));
}

// A track + fill bar with an optional value string to its right. `fraction` is clamped, and the
// fill only collapses to a single rounded rect when it is nearly empty so the rounding never
// inverts on a tiny fill.
void hudBar(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float fraction, ImU32 fill, float rounding,
            const char* value, ImFont* font, float fontSize, ImU32 textColor, float fade) {
    fraction = std::clamp(fraction, 0.f, 1.f);
    dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, (int)(120.f * fade)), rounding);
    const float w = p1.x - p0.x;
    if (w * fraction >= rounding * 1.2f)
        dl->AddRectFilled(p0, ImVec2(p0.x + w * fraction, p1.y), fill, rounding);
    if (value && *value && font)
        dl->AddText(font, fontSize, ImVec2(p1.x + 4.f, p0.y + (p1.y - p0.y - fontSize) * 0.5f), textColor, value);
}

// ---- Custom crosshair -----------------------------------------------------------------------
// Preset shapes plus the Draw Board. `extra` opens the whole cross up while a swing charges (the
// Dynamic option), and the whole thing rotates about the screen centre.
void hudDrawCrosshair(ImDrawList* dl, const HudSettings& st, ImVec2 c, float scale, float extra) {
    const float size = std::clamp(st.crosshairSize, 0.4f, 3.f);
    const float gap = std::clamp(st.crosshairGap, 0.f, 12.f) * scale + extra;
    const float arm = 6.f * scale * size;
    const float th = std::max(0.5f, std::clamp(st.crosshairThickness, 0.5f, 5.f) * scale);
    const float dot = std::max(0.5f, std::clamp(st.crosshairDot, 0.5f, 6.f) * scale);
    const ImU32 col = packed(st.crosshairColor);
    const ImU32 outline = st.crosshairOutline ? IM_COL32(0, 0, 0, 150) : 0;
    const float rot = st.crosshairRotation * 0.017453292519943295f;
    const float cs = std::cos(rot), sn = std::sin(rot);
    const auto P = [&](float x, float y) { return ImVec2(c.x + x * cs - y * sn, c.y + x * sn + y * cs); };
    const auto line = [&](float x0, float y0, float x1, float y1) {
        if (outline) dl->AddLine(P(x0, y0), P(x1, y1), outline, th + 2.f * scale);
        dl->AddLine(P(x0, y0), P(x1, y1), col, th);
    };
    const auto dotAt = [&](float x, float y, float r) {
        if (outline) dl->AddCircleFilled(P(x, y), r + scale, outline, 16);
        dl->AddCircleFilled(P(x, y), r, col, 16);
    };
    const auto cross = [&](bool withDot) {
        line(-gap - arm, 0.f, -gap, 0.f);
        line(gap, 0.f, gap + arm, 0.f);
        line(0.f, -gap - arm, 0.f, -gap);
        line(0.f, gap, 0.f, gap + arm);
        if (withDot) dotAt(0.f, 0.f, dot);
    };
    switch (st.crosshairStyle) {
        case CrosshairStyle::Cross: cross(false); break;
        case CrosshairStyle::Dot: dotAt(0.f, 0.f, dot); break;
        case CrosshairStyle::CrossDot: cross(true); break;
        case CrosshairStyle::TShape:
            line(-gap - arm, 0.f, -gap, 0.f);
            line(gap, 0.f, gap + arm, 0.f);
            line(0.f, gap, 0.f, gap + arm);
            break;
        case CrosshairStyle::Circle:
            if (outline) dl->AddCircle(c, arm + gap, outline, 32, th + 2.f * scale);
            dl->AddCircle(c, arm + gap, col, 32, th);
            break;
        case CrosshairStyle::CircleDot:
            if (outline) dl->AddCircle(c, arm + gap, outline, 32, th + 2.f * scale);
            dl->AddCircle(c, arm + gap, col, 32, th);
            dotAt(0.f, 0.f, dot);
            break;
        case CrosshairStyle::Chevron:
            line(-arm, arm * 0.7f, 0.f, -arm * 0.7f);
            line(0.f, -arm * 0.7f, arm, arm * 0.7f);
            break;
        case CrosshairStyle::Arrows:
            line(-gap - arm, 0.f, -gap, 0.f);
            line(gap, 0.f, gap + arm, 0.f);
            line(0.f, -gap - arm, 0.f, -gap);
            line(0.f, gap, 0.f, gap + arm);
            line(-gap - arm, 0.f, -gap - arm * 0.6f, -arm * 0.28f);
            line(-gap - arm, 0.f, -gap - arm * 0.6f, arm * 0.28f);
            line(gap + arm, 0.f, gap + arm * 0.6f, -arm * 0.28f);
            line(gap + arm, 0.f, gap + arm * 0.6f, arm * 0.28f);
            break;
        case CrosshairStyle::CustomDraw: {
            const float px = std::max(1.f, std::clamp(st.drawBoardPixel, 1.f, 6.f) * scale);
            const float halfX = 4.5f * px, halfY = 4.5f * px;
            for (int row = 0; row < 9; ++row)
                for (int gx = 0; gx < 9; ++gx)
                    if (st.drawBoardPixels[row * 9 + gx]) {
                        const ImVec2 pp(c.x - halfX + gx * px, c.y - halfY + row * px);
                        if (outline)
                            dl->AddRectFilled(ImVec2(pp.x - 1.f, pp.y - 1.f),
                                              ImVec2(pp.x + px + 1.f, pp.y + px + 1.f), outline);
                        dl->AddRectFilled(pp, ImVec2(pp.x + px, pp.y + px), col);
                    }
            break;
        }
        default: cross(true); break;
    }
}

// The held item the plaque names: the main-hand stack with its item id and stack count.
struct HudHeldItem { mc::LocalRef stack; std::string id; int count = 0; bool valid = false; };

HudHeldItem hudHeldItem(jobject playerObj) {
    HudHeldItem out;
    if (!playerObj) return out;
    out.stack = mc::LocalRef(mc::LivingEntity(playerObj).mainHandStack());
    if (!out.stack || mc::stk::isEmpty(out.stack.get())) return out;
    out.id = mc::stk::itemId(out.stack.get());
    out.count = mc::stk::count(out.stack.get());
    out.valid = !out.id.empty();
    return out;
}

std::string hudItemLabel(jobject stack) {
    if (!stack) return {};
    std::string name = mc::stk::name(stack);            // the game's own display name
    // A translatable component whose key could not be resolved comes back as the component's own
    // dump ("translation{key='item.minecraft.golden_apple', ...}"), which is what the plaque used
    // to print for a golden apple. Anything that still looks like one falls back to the item id,
    // so the plaque shows "Golden Apple" rather than the component's internals.
    if (name.find("key=") != std::string::npos || name.find("translation") != std::string::npos)
        name.clear();
    if (name.empty()) name = shortItemId(mc::stk::itemId(stack));
    return name;
}

void drawHud(ImDrawList* dl, float tickDelta) {
    (void)tickDelta;   // the HUD is drawn in screen space; nothing here needs the frame's tick delta
    if (!g_hud) { g_hudFade = 0.0; return; }
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return;
    const ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 1.f || io.DisplaySize.y <= 1.f) return;
    const double now = ImGui::GetTime();
    const float dt = std::clamp((float)io.DeltaTime, 0.001f, 0.1f);
    g_hudFade += (1.0 - g_hudFade) * (1.0 - std::exp(-12.f * dt));
    const float fade = (float)std::clamp(g_hudFade, 0.0, 1.0);
    if (fade <= 0.01f) return;

    const HudSettings& st = g_hudSettings;
    // Nothing to draw a HUD over until there is a player (the title screen, a loading frame, or
    // the moment between worlds): the fade still runs so it comes back smoothly.
    jobject playerObj = mc::Client::player();
    if (!playerObj) return;
    jobject inventory = mc::Player(playerObj).inventory();
    mc::LocalRef list(inventory ? mc::inv::mainList(inventory) : nullptr);
    const int selected = inventory ? std::clamp(mc::inv::selectedSlot(inventory), 0, 8) : 0;
    mc::Player player(playerObj);
    ImFont* font = gui::fontRegular(); if (!font) font = ImGui::GetFont();
    ImFont* bold = gui::fontSemibold(); if (!bold) bold = font;

    // One UI scale for the whole HUD so it stays proportionate on any resolution.
    const float ui = std::max(1.f, io.DisplaySize.y / 720.f);
    const float s = ui * std::clamp(st.hotbarScale, 0.6f, 1.6f);
    const float slot = 26.f * s;
    const float gap = 2.5f * s;
    const float rounding = st.hotbarRounded ? std::clamp(st.hotbarRounding, 0.f, 16.f) * s : 0.f;
    const float barW = slot * 9.f + gap * 8.f + 8.f * s;
    const float barH = slot + 8.f * s;
    const float barX = (io.DisplaySize.x - barW) * 0.5f;
    const float barY = io.DisplaySize.y - 6.f * s - barH;

    // Shared stack geometry. The two panels share the hotbar's own width exactly (with a gap
    // between them), so they can never overlap whatever the scale is; the held-item plaque then
    // sits above the top of the whole stack, so the three never fight for the same rows.
    const float ps = ui * std::clamp(st.panelScale, 0.6f, 1.6f);
    const float pr = std::clamp(st.panelRounding, 0.f, 16.f) * ps;
    const float panelGap = 8.f * ps;
    const float panelW = (barW - panelGap) * 0.5f;
    const float rowH = 7.f * ps;
    const float rowGap = 9.f * ps;
    const float glyph = std::min(11.f * ps, panelW * 0.22f);
    const float padX = std::min(8.f * ps, panelW * 0.12f);
    const float padY = std::min(8.f * ps, panelW * 0.12f);
    const float innerW = panelW - padX * 2.f - (st.panelIcons ? glyph + 5.f * ps : 0.f);
    const float panelH = padY * 2.f + rowH * 2.f + rowGap;
    const float panelY = barY - 8.f * ps - panelH - std::clamp((float)st.panelOffsetY, 0.f, 120.f) * ps;
    const float hudTop = st.panels ? panelY : barY;

    // ---- plate over the game's own hotbar and bars -------------------------------------------
    // The game draws its own hotbar and bar rows in exactly this place, and it hides its whole
    // HUD - the first-person hand, the chat, the Tab list and the scoreboard included - as a
    // single switch, which is why this module never touches that switch. An opaque plate is drawn
    // over the game's block instead: the block is 182 gui units wide and about 52 tall, centred
    // and bottom-anchored, so it is placed at the game's own GUI scale (read from the window) and
    // unioned with this HUD, which keeps the plate behind both whatever scale either one uses.
    // Without a resolvable scale the plate simply backs this HUD alone.
    // Drawn whenever Cover Vanilla is on, even with the custom hotbar and panels off, so the game's
    // own hotbar and bars are always hidden.
    if (st.coverVanilla) {
        const float pad = std::clamp(st.platePadding, 0.f, 24.f) * ui;
        float vW = 0.f, vH = 0.f;
        int gs = mc::hud::vanillaGuiScale(io.DisplaySize.x, io.DisplaySize.y);
        // Without a resolved scale the plate would only back this HUD and the game's own hotbar
        // would show around it, so the scale is estimated from the framebuffer height instead
        // (Minecraft picks an integer scale for that height, and the estimate deliberately rounds
        // up: a plate a little too large hides the vanilla bar, one a little too small does not).
        if (gs <= 0) gs = (int)std::ceil(io.DisplaySize.y / 300.f);
        if (gs < 1) gs = 1;
        vW = 184.f * gs; vH = 54.f * gs;
        const float plateX0 = std::min(barX, (io.DisplaySize.x - vW) * 0.5f) - pad;
        const float plateX1 = std::max(barX + barW, (io.DisplaySize.x + vW) * 0.5f) + pad;
        const float plateY0 = std::min(hudTop, io.DisplaySize.y - vH) - pad;
        const float plateY1 = io.DisplaySize.y - pad * 0.5f;
        // Opaque by default: the game draws its own hotbar and bar rows in exactly this place, and
        // anything translucent left them showing through the drawn HUD.
        hudRect(dl, ImVec2(plateX0, plateY0), ImVec2(plateX1, plateY1),
                packed(st.plateColor, fade * std::clamp(st.plateOpacity, 0.f, 1.f)), 10.f * ui);
    }

    // ---- hotbar ----------------------------------------------------------------------------
    if (st.customHotbar) {
        // Selection pop: the newly selected slot spikes and settles, and the whole bar fades in.
        if (selected != g_hudPrevSelected) {
            if (selected >= 0 && selected < 9) g_hudSlotPop[selected] = 1.f;
            g_hudPrevSelected = selected;
        }
        for (int i = 0; i < 9; ++i) {
            if (!st.hotbarAnimations) { g_hudSlotPop[i] = 0.f; continue; }
            const float speed = std::clamp(st.hotbarAnimSpeed, 4.f, 30.f);
            g_hudSlotPop[i] = std::max(0.f, g_hudSlotPop[i] - dt * speed * 0.18f);
        }
        hudRect(dl, ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
                packed(st.hotbarBackground, fade), rounding, packed(st.hotbarAccent, fade * 0.35f), 1.f * s);
        for (int i = 0; i < 9; ++i) {
            const float pop = st.hotbarAnimations ? g_hudSlotPop[i] : 0.f;
            const float grow = pop * 2.5f * s;
            const ImVec2 p0(barX + 4.f * s + i * (slot + gap) - grow * 0.5f, barY + 4.f * s - grow * 0.5f);
            const ImVec2 p1(p0.x + slot + grow, p0.y + slot + grow);
            const bool isSel = (i == selected);
            ImU32 slotFill = packed(st.slotBackground, fade);
            if (isSel) {
                // Blend the slot towards the accent by hand (the gui helper is file-local).
                const ImVec4& a = st.hotbarAccent;
                const ImVec4& b = st.slotBackground;
                const float t = std::clamp(0.30f + 0.15f * pop, 0.f, 1.f);
                slotFill = packed(ImVec4(b.x + (a.x - b.x) * t, b.y + (a.y - b.y) * t,
                                         b.z + (a.z - b.z) * t, b.w + (a.w - b.w) * t), fade);
            }
            hudRect(dl, p0, p1, slotFill, rounding * 0.7f,
                    isSel ? packed(st.hotbarAccent, fade) : IM_COL32(90, 80, 92, (int)(140.f * fade)),
                    isSel ? 2.f * s : 1.f * s);
            mc::LocalRef stack(list ? mc::inv::getFromList(list.get(), i) : nullptr);
            if (stack && !mc::stk::isEmpty(stack.get())) {
                drawNametagItem(dl, stack.get(), p0.x + 3.f * s, p0.y + 3.f * s, slot - 6.f * s,
                                s, font, 9.f * s, "", st.hotbarItemCount);
                if (st.hotbarItemCount) {
                    const int count = mc::stk::count(stack.get());
                    if (count > 1) {
                        char buf[12];
                        snprintf(buf, sizeof(buf), "%d", count);
                        const float fs = 9.f * s;
                        const float tw = font ? font->CalcTextSizeA(fs, FLT_MAX, 0.f, buf).x : ImGui::CalcTextSize(buf).x;
                        if (font) dl->AddText(font, fs, ImVec2(p1.x - tw - 3.f * s, p1.y - fs - 2.f * s),
                                              IM_COL32(255, 255, 255, (int)(235.f * fade)), buf);
                    }
                }
            }
            if (st.hotbarNumbers) {
                char num[4];
                snprintf(num, sizeof(num), "%d", i + 1);
                const float fs = 8.f * s;
                if (font) dl->AddText(font, fs, ImVec2(p0.x + 3.f * s, p0.y + 2.f * s),
                                      isSel ? packed(st.hotbarAccent, fade) : IM_COL32(210, 202, 212, (int)(200.f * fade)), num);
            }
        }
    }

    // ---- held-item plaque ------------------------------------------------------------------
    if (st.itemNamePopup) {
        const HudHeldItem held = hudHeldItem(playerObj);
        const std::string key = held.valid ? (held.id + "#" + std::to_string(held.count)) : std::string{};
        if (key != g_hudItemNameId) {
            g_hudItemNameId = key;
            if (!key.empty())
                g_hudItemNameUntil = now + std::clamp(st.itemNameHoldMs, 300.f, 5000.f) / 1000.0;
        }
        if (held.valid && now < g_hudItemNameUntil) {
            const float ns = ui * std::clamp(st.itemNameScale, 0.6f, 1.6f);
            const std::string label = hudItemLabel(held.stack.get());
            const float fsz = 11.f * ns;
            const float tw = font ? font->CalcTextSizeA(fsz, FLT_MAX, 0.f, label.c_str()).x
                                  : ImGui::CalcTextSize(label.c_str()).x;
            const float icon = 14.f * ns;
            const float pad = 7.f * ns;
            const float pw = pad + (st.itemNameIcons ? icon + 6.f * ns : 0.f) + tw + pad;
            const float ph = std::max(icon, fsz) + pad * 1.4f;
            // Fade the plaque out over its last third of a second rather than snapping off.
            const float life = std::clamp((float)((g_hudItemNameUntil - now) / 0.35), 0.f, 1.f);
            const float a = fade * life;
            const ImVec2 p0((io.DisplaySize.x - pw) * 0.5f, hudTop - 10.f * ns - ph);
            const ImVec2 p1(p0.x + pw, p0.y + ph);
            hudRect(dl, p0, p1, packed(st.itemNameBackground, a), 5.f * ns,
                    packed(st.hotbarAccent, a), 1.f * ns);
            float cx = p0.x + pad;
            if (st.itemNameIcons)
                drawNametagItem(dl, held.stack.get(), cx, p0.y + (ph - icon) * 0.5f, icon, ns,
                                font, fsz, "", true);
            if (st.itemNameIcons) cx += icon + 6.f * ns;
            const float ty = p0.y + (ph - fsz) * 0.5f;
            if (font) dl->AddText(font, fsz, ImVec2(cx, ty), packed(st.itemNameText, a), label.c_str());
            else dl->AddText(ImVec2(cx, ty), packed(st.itemNameText, a), label.c_str());
        }
    }

    // ---- health / armour and food / XP panels ----------------------------------------------
    if (st.panels) {
        const ImU32 textColor = IM_COL32(245, 240, 240, (int)(235.f * fade));
        const float valueFont = 8.f * ps;
        const auto panelAt = [&](float px, const char* title, float f0, float f1, ImU32 c0, ImU32 c1,
                                 const char* v0, const char* v1, int g0, int g1, float beat) {
            hudRect(dl, ImVec2(px, panelY), ImVec2(px + panelW, panelY + panelH),
                    packed(st.panelBackground, fade), pr, IM_COL32(255, 255, 255, (int)(26.f * fade)), 1.f * ps);
            if (title && *title && font)
                dl->AddText(font, 7.f * ps, ImVec2(px + padX, panelY + 1.f * ps), IM_COL32(150, 142, 154, (int)(210.f * fade)), title);
            // The rows are measured from the panel's own top edge: without this they were drawn
            // at the top of the screen while the panel box stayed down by the hotbar.
            const float rowY0 = panelY + padY + 5.f * ps;
            const float rowY1 = rowY0 + rowH + rowGap;
            const float bar0X = px + padX + (st.panelIcons ? glyph + 5.f * ps : 0.f);
            if (st.panelIcons) {
                // 1 = heart, 2 = shield, 3 = drumstick, 4 = orb. The row's own bar colour tints
                // its glyph, so a health heart is red and an XP orb is green by default.
                const float gx = px + padX + glyph * 0.5f;
                // The heart swells with the low-health beat; the other glyphs never pulse.
                const auto drawGlyph = [&](int g, float cy, float pulse) {
                    if (g == 1) hudGlyphHeart(dl, ImVec2(gx, cy), glyph * 0.55f * (1.f + 0.22f * pulse), c0);
                    else if (g == 2) hudGlyphShield(dl, ImVec2(gx, cy), glyph * 0.55f, c1);
                    else if (g == 3) hudGlyphFood(dl, ImVec2(gx, cy), glyph * 0.55f, c0);
                    else if (g == 4) hudGlyphOrb(dl, ImVec2(gx, cy), glyph * 0.55f, c1);
                };
                drawGlyph(g0, rowY0 + rowH * 0.5f, beat);
                drawGlyph(g1, rowY1 + rowH * 0.5f, 0.f);
            }
            hudBar(dl, ImVec2(bar0X, rowY0), ImVec2(bar0X + innerW, rowY0 + rowH), f0, c0,
                   rowH * 0.5f, st.panelNumbers ? v0 : nullptr, font, valueFont, textColor, fade);
            hudBar(dl, ImVec2(bar0X, rowY1), ImVec2(bar0X + innerW, rowY1 + rowH), f1, c1,
                   rowH * 0.5f, st.panelNumbers ? v1 : nullptr, font, valueFont, textColor, fade);
        };
        char hBuf[24], aBuf[24], fBuf[24], xBuf[24];
        const float health = player.health();
        const float maxHealth = player.maxHealth() > 0.f ? player.maxHealth() : 20.f;
        // Absorption: while absorption is being held the health row is yellow and its maximum is
        // the health plus the absorption held right now, so the row covers the shielding too. The
        // moment the absorption is gone the row is the plain red health bar again.
        const float absorb = st.absorbHealth ? std::clamp(player.absorptionAmount(), 0.f, 40.f) : 0.f;
        const bool absorbing = absorb > 0.f;
        const float shownMax = absorbing ? health + absorb : maxHealth;
        const float hp = shownMax > 0.f ? std::clamp(health / shownMax, 0.f, 1.f) : 0.f;
        const int armor = player.armorPoints();
        const int food = player.foodLevel();
        const int xpLevel = player.experienceLevel();
        const float xpProg = std::clamp(player.experienceProgress(), 0.f, 1.f);
        snprintf(hBuf, sizeof(hBuf), "%.0f/%.0f", health, shownMax);
        snprintf(aBuf, sizeof(aBuf), "%d/20", armor);
        snprintf(fBuf, sizeof(fBuf), "%d/20", food);
        snprintf(xBuf, sizeof(xBuf), "%d", xpLevel);
        // Bar animation: each row slides towards its real value instead of snapping to it, so
        // losing health drains the row rather than dropping it a chunk at a time.
        const float barTargets[4] = { hp, std::clamp(armor / 20.f, 0.f, 1.f),
                                      std::clamp(food / 20.f, 0.f, 1.f), xpProg };
        const float barSpeed = std::clamp(st.barAnimSpeed, 3.f, 30.f);
        for (int i = 0; i < 4; ++i) {
            if (!st.barAnimations || g_hudBarShown[i] < 0.f) { g_hudBarShown[i] = barTargets[i]; continue; }
            g_hudBarShown[i] += (barTargets[i] - g_hudBarShown[i]) * (1.f - std::exp(-barSpeed * dt));
        }
        // Below two hearts the heart glyph beats, and it beats faster the lower the health is.
        float beat = 0.f;
        if (st.heartBeat && health < 4.f) {
            g_hudHeartPulse += dt * (2.4 + (4.f - std::max(health, 0.f)) * 0.9);
            beat = 0.5f + 0.5f * std::sin((float)g_hudHeartPulse * 6.2831853f);
        } else {
            g_hudHeartPulse = 0.0;
        }
        panelAt(barX, "VITALS", g_hudBarShown[0], g_hudBarShown[1],
                packed(absorbing ? st.absorbColor : st.healthColor, fade), packed(st.armorColor, fade),
                hBuf, aBuf, 1, 2, beat);
        panelAt(barX + panelW + panelGap, "SUBSTANCE", g_hudBarShown[2], g_hudBarShown[3],
                packed(st.foodColor, fade), packed(st.xpColor, fade), fBuf, xBuf, 3, 4, 0.f);
    }

    // ---- attack speed bar -------------------------------------------------------------------
    if (st.attackSpeedBar) {
        const float progress = std::clamp(player.attackCooldownProgress(1.f), 0.f, 1.f);
        const bool attacking = gui::mouseDown(VK_LBUTTON) || mc::Client::attackCooldown() > 0 || progress < 0.999f;
        if (attacking) {
            const float as = ui * std::clamp(st.attackSpeedScale, 0.6f, 1.6f);
            const float w = 90.f * as, h = 4.f * as;
            const ImVec2 c(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
            const ImVec2 p0(c.x - w * 0.5f, c.y + 26.f * ui);
            const ImVec2 p1(p0.x + w, p0.y + h);
            dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, (int)(130.f * fade)), h * 0.5f);
            if (w * progress >= h * 0.6f)
                dl->AddRectFilled(p0, ImVec2(p0.x + w * progress, p1.y), packed(st.attackSpeedColor, fade), h * 0.5f);
        }
    }

    // ---- crosshair --------------------------------------------------------------------------
    // The game draws its own crosshair, and this module no longer hides the game's HUD, so a
    // custom crosshair is drawn only when the card asks for one. The indicator is separate: while
    // the crosshair ray is on a living entity inside the range, the crosshair is drawn in the
    // indicator colour, a little larger than the game's own so the game's stays visible as its
    // core. That is what the crosshair-indicator mod does, and it needs nothing hidden.
    if (!mc::Client::screenOpen()) {
        const ImVec2 c(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
        bool indicator = false;
        if (st.crosshairIndicator) {
            mc::LocalRef hit(mc::Client::crosshair());
            if (hit && mc::hit::typeOf(hit.get()) == 2) {
                mc::LocalRef target(mc::hit::entity(hit.get()));
                JNIEnv* env = jni::env();
                if (target && env && mc::sym().living &&
                    env->IsInstanceOf(target.get(), mc::sym().living) == JNI_TRUE &&
                    mc::LivingEntity(target.get()).alive()) {
                    const float range = std::clamp(st.crosshairIndicatorRange, 1.f, 64.f);
                    indicator = (float)mc::Entity(target.get()).distanceTo(player) <= range;
                }
                if (env) jni::clearException(env, "crosshair indicator");
            }
        }
        float extra = 0.f;
        if (st.crosshairDynamic)
            extra = (1.f - std::clamp(player.attackCooldownProgress(1.f), 0.f, 1.f)) * 6.f * ui;
        if (st.customCrosshair || indicator) {
            HudSettings shown = st;
            if (indicator) {
                if (!st.customCrosshair) shown.crosshairStyle = CrosshairStyle::CrossDot;
                shown.crosshairColor = st.crosshairIndicatorColor;
                if (!st.customCrosshair) shown.crosshairSize = st.crosshairSize * 1.35f;
            }
            hudDrawCrosshair(dl, shown, c, ui, extra);
        }
    }
}

// ---- Non-silent inventory helpers ---------------------------------------------------------
// Auto Totem and AutoClean do their slot work through the player's own
// PlayerScreenHandler. Silent mode never opens anything; with the Silent option off the real
// inventory screen is opened first so the work is actually visible, then closed again.
// Opening and closing use the ordinary E / Escape keys the player would press (synthetic
// Win32 input through hooks::), so nothing but vanilla interaction happens.
enum class ScreenPhase { Idle, Opening, Working, Closing };

struct InvScreenState {
    ScreenPhase phase = ScreenPhase::Idle;
    double stageAt = 0.0;
};

InvScreenState g_totemScreen, g_cleanScreen;

void screenPhaseReset(InvScreenState& s) { s.phase = ScreenPhase::Idle; s.stageAt = 0.0; }

// Ask the game to close the inventory we opened (never a screen the player opened).
void inventoryWorkDone(InvScreenState& s, bool silent) {
    if (!silent && s.phase == ScreenPhase::Working) {
        // Closed through the game's own API. A synthesised ESC also works, but it arms the pause menu
        // whenever the inventory is already gone by the time the key lands.
        mc::Client::closeScreen();
        s.phase = ScreenPhase::Closing;
        s.stageAt = ImGui::GetTime();
        return;
    }
    if (silent) return;
    screenPhaseReset(s);
}

// True while the caller may touch the slot packets. Silent mode is ready straight away;
// the visible path opens the inventory and only reports ready once it really appeared.
bool inventoryWorkReady(InvScreenState& s, bool silent, double minVisibleSeconds = 0.25) {
    const bool screenOpen = mc::Client::screenOpen();
    if (silent) return !screenOpen;
    const double now = ImGui::GetTime();
    switch (s.phase) {
        case ScreenPhase::Idle:
            // Never take over a screen the player already has open.
            if (screenOpen) return false;
            // Open the inventory the way the game's own inventory key does. A synthesised key press
            // went to whatever that key is bound to for this player (E, which on this setup is
            // crouch), so the inventory never opened at all.
            if (!mc::Client::openInventoryScreen()) return false;   // not resolvable: retry next frame
            s.phase = ScreenPhase::Opening;
            s.stageAt = now;
            return false;
        case ScreenPhase::Opening:
            if (screenOpen) { s.phase = ScreenPhase::Working; s.stageAt = now; }
            else if (now - s.stageAt > 0.6) screenPhaseReset(s);   // never appeared: give up quietly
            return false;
        case ScreenPhase::Working:
            if (!screenOpen) { screenPhaseReset(s); return false; }   // the player closed it again
            if (now - s.stageAt > 4.0) { inventoryWorkDone(s, false); return false; }   // safety
            // Let the inventory stay up long enough to actually be seen instead of flashing open
            // and shut around a single slot click. The caller can shorten this: Auto Pot passes its
            // Inventory Delay, so a 2 ms setting really is 2 ms rather than a fixed 250 ms.
            if (now - s.stageAt < minVisibleSeconds) return false;
            return true;
        case ScreenPhase::Closing:
            if (!screenOpen || now - s.stageAt > 0.6) screenPhaseReset(s);
            return false;
    }
    return false;
}

// ---- Auto Totem ---------------------------------------------------------------------------
double g_totemNextAt = 0.0;

bool isTotemAt(jobject list, int index) {
    mc::LocalRef stack(mc::inv::getFromList(list, index));
    return stack && !mc::stk::isEmpty(stack.get()) && mc::stk::itemId(stack.get()) == "totem_of_undying";
}

bool isItemAt(jobject list, int index, const char* id) {
    if (!list || index < 0) return false;
    mc::LocalRef stack(mc::inv::getFromList(list, index));
    return stack && !mc::stk::isEmpty(stack.get()) && mc::stk::itemId(stack.get()) == id;
}

// Off-hand refill (and optional hotbar refill) with the F-key / number-key slot
// packets: clickSlot(syncId, slot, 40|hotbar, SWAP, player). Nothing is opened.
void updateAutoTotem() {
    if (!g_autoTotem || gui::inputFrozen()) {
        if (g_totemScreen.phase == ScreenPhase::Working && !gui::inputFrozen())
            inventoryWorkDone(g_totemScreen, false);
        return;
    }
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return;
    const AutoTotemSettings& st = g_autoTotemSettings;
    jobject playerObj = mc::Client::player();
    if (!playerObj) return;
    mc::LivingEntity self(playerObj);
    if (!self.alive() || self.health() <= 0.f) { inventoryWorkDone(g_totemScreen, st.silent); return; }  // no refill while dead
    mc::LocalRef inventory(mc::Player(playerObj).inventory());
    if (!inventory) return;
    mc::LocalRef list(mc::inv::mainList(inventory.get()));
    if (!list) return;
    const int size = std::min(36, mc::inv::listSize(list.get()));
    const int selectedNow = std::clamp(mc::inv::selectedSlot(inventory.get()), 0, 8);
    const double delay = std::clamp(st.delayMs, 0, 600) / 1000.0;

    // ---- decide what has to move BEFORE anything is opened -----------------------------------
    // The visible path used to open the inventory first and only then look for work, so with no
    // totem anywhere it opened, found nothing, closed and opened again on the very next pass - the
    // reported endless inventory open/close. Everything below is read from the inventory that is
    // always available, so "nothing to do" means the screen is never opened at all.
    mc::LocalRef offhandNow(self.offHandStack());
    const std::string offNow = offhandNow ? mc::stk::itemId(offhandNow.get()) : std::string{};
    int hotbarTarget = -1, hotbarSource = -1, offSource = -1, smpSource = -1;
    bool smpLower = false;
    if (st.type == TotemType::SMP) {
        const bool wantTotem = self.health() > 0.f && self.health() <= std::clamp(st.smpHealthHearts, 0.5f, 10.f) * 2.f;
        if (wantTotem && offNow != "totem_of_undying") {
            for (int i = 0; i < size; ++i) if (isTotemAt(list.get(), i)) { smpSource = i; break; }
        } else if (!wantTotem && offNow == "totem_of_undying") {
            smpLower = true;
        }
    } else {
        if (st.refillHotbarSlot) {
            const int hotbar = std::clamp(st.hotbarSlot, 1, 9) - 1;
            if (hotbar < size && !isTotemAt(list.get(), hotbar)) {
                for (int i = 9; i < size; ++i) if (isTotemAt(list.get(), i)) { hotbarTarget = hotbar; hotbarSource = i; break; }
            }
        }
        if (st.refillOffhand && offNow != "totem_of_undying") {
            if (selectedNow < size && isTotemAt(list.get(), selectedNow)) offSource = selectedNow;
            else for (int i = 0; i < std::min(9, size); ++i) if (isTotemAt(list.get(), i)) { offSource = i; break; }
        }
    }
    if (hotbarTarget < 0 && offSource < 0 && !smpLower && smpSource < 0) {
        // Nothing to refill (or no totem left to refill from): the inventory is closed if it was
        // open and never opened again, so an empty inventory no longer spins open/closed for ever.
        if (g_totemScreen.phase != ScreenPhase::Idle) inventoryWorkDone(g_totemScreen, st.silent);
        return;
    }

    // Silent: work in the background. Otherwise the inventory is opened now, and only now - there
    // is a refill waiting, so the screen never opens for a pass with nothing to do.
    if (!inventoryWorkReady(g_totemScreen, st.silent)) return;
    // The clickSlot syncId must match the handler the server has open.
    if (!mc::Client::playerScreenIsCurrent()) { inventoryWorkDone(g_totemScreen, st.silent); return; }
    const double now = ImGui::GetTime();
    if (now < g_totemNextAt) return;

    // ---- Type: SMP ----
    // Hold the totem only while the health is at or below the threshold; above it the shield goes
    // back into the off hand. Both swaps ride on the game's ordinary F-key slot packet, and the
    // swap delay paces them the same way Vanilla's Delay paces its refills.
    if (st.type == TotemType::SMP) {
        const double smpDelay = std::clamp(st.smpSwapDelayMs, 0, 600) / 1000.0;
        if (smpSource >= 0) {
            const int screenSlot = mc::Client::playerScreenSlotForMainIndex(smpSource);
            if (screenSlot >= 0 && mc::Client::swapOffhand(screenSlot)) g_totemNextAt = now + smpDelay;
        } else if (smpLower) {
            // The configured shield slot is tried first, then the whole inventory.
            int shield = std::clamp(st.smpShieldSlot, 1, 9) - 1;
            if (shield >= size || !isItemAt(list.get(), shield, "shield")) {
                shield = -1;
                for (int i = 0; i < size; ++i) if (isItemAt(list.get(), i, "shield")) { shield = i; break; }
            }
            if (shield >= 0) {
                const int screenSlot = mc::Client::playerScreenSlotForMainIndex(shield);
                if (screenSlot >= 0 && mc::Client::swapOffhand(screenSlot)) g_totemNextAt = now + smpDelay;
            }
        }
        inventoryWorkDone(g_totemScreen, st.silent);
        return;
    }

    // 1) "Totem Refill slot": keep the chosen hotbar slot stocked with a totem.
    if (hotbarTarget >= 0 && hotbarSource >= 0) {
        const int screenSlot = mc::Client::playerScreenSlotForMainIndex(hotbarSource);
        if (screenSlot >= 0 && mc::Client::swapHotbarSlot(screenSlot, hotbarTarget)) {
            g_totemNextAt = now + delay;
            // Close behind the move and let the work check above decide whether another pass opens
            // it again: it only does while a totem is really missing, so an empty inventory can no
            // longer spin open and shut.
            inventoryWorkDone(g_totemScreen, st.silent);
            return;
        }
    }

    // 2) Off hand: swap a hotbar totem into the off hand.
    if (offSource >= 0) {
        const int screenSlot = mc::Client::playerScreenSlotForMainIndex(offSource);
        if (screenSlot >= 0 && mc::Client::swapOffhand(screenSlot)) {
            g_totemNextAt = now + delay;
            inventoryWorkDone(g_totemScreen, st.silent);
            return;
        }
    }
    // Nothing could be moved this pass: the inventory we opened is closed again.
    inventoryWorkDone(g_totemScreen, st.silent);
}

// ---- AutoClean ----------------------------------------------------------------------------
double g_cleanNextAt = 0.0;

bool isCleanTarget(const std::string& id) {
    if (id.empty()) return false;
    for (const std::string& custom : g_autoCleanSettings.customItems)
        if (!custom.empty() && id == lowerCopy(custom)) return true;
    static const char* const junk[] = {
        "rotten_flesh", "spider_eye", "bone", "string", "gunpowder", "flint", "feather",
        "leather", "wheat_seeds", "beetroot_seeds", "melon_seeds", "pumpkin_seeds",
        "poisonous_potato", "stick", "bowl", "zombie_head"
    };
    for (const char* item : junk) if (id == item) return true;
    return false;
}

// Drops one junk stack per scan using the vanilla THROW slot packet (Q key path).
void updateAutoClean() {
    if (!g_autoClean || gui::inputFrozen()) {
        if (g_cleanScreen.phase == ScreenPhase::Working && !gui::inputFrozen())
            inventoryWorkDone(g_cleanScreen, false);
        return;
    }
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return;
    const bool silent = g_autoCleanSettings.silentClean;
    // Silent Clean works straight from the inventory packets; with it off the inventory is
    // opened first so the drops are visible, and closed again when there is nothing left.
    if (!inventoryWorkReady(g_cleanScreen, silent)) return;
    const bool playerHandler = mc::Client::playerScreenIsCurrent();
    if (!playerHandler) { inventoryWorkDone(g_cleanScreen, silent); return; }   // never desync it
    jobject playerObj = mc::Client::player();
    if (!playerObj) return;
    const double now = ImGui::GetTime();
    if (now < g_cleanNextAt) return;
    mc::Player self(playerObj);
    if (!self.alive()) return;
    mc::LocalRef inventory(self.inventory());
    if (!inventory) return;
    mc::LocalRef list(mc::inv::mainList(inventory.get()));
    if (!list) return;
    const int size = std::min(36, mc::inv::listSize(list.get()));
    g_cleanNextAt = now + std::clamp(g_autoCleanSettings.scanDelayMs, 0, 600) / 1000.0;
    bool dropped = false;
    for (int i = 0; i < size; ++i) {
        mc::LocalRef stack(mc::inv::getFromList(list.get(), i));
        if (!stack || mc::stk::isEmpty(stack.get())) continue;
        if (!isCleanTarget(mc::stk::itemId(stack.get()))) continue;
        const int screenSlot = mc::Client::playerScreenSlotForMainIndex(i);
        if (screenSlot < 0) continue;
        mc::Client::throwSlot(screenSlot, true);                  // whole stack
        dropped = true;
        break;                                                    // one drop per scan
    }
    // Clean finished: the inventory we opened is closed again.
    if (!dropped) inventoryWorkDone(g_cleanScreen, silent);
}

// ---- Cart Refill --------------------------------------------------------------------------
// Keeps the watched hotbar slots stocked with a TNT minecart, moving carts out of the main
// inventory with the ordinary slot packets (silently), or with the inventory opened first so
// the move is visible. One refill per delay window.
double g_cartRefillNextAt = 0.0;
InvScreenState g_cartScreen;

// Called when the Xbow Macro finishes so the cart it placed is refilled right away, with no
// remaining delay window from an earlier refill.
void cartRefillRequestNow() { g_cartRefillNextAt = 0.0; }

bool isCartAt(jobject list, int index) {
    mc::LocalRef stack(mc::inv::getFromList(list, index));
    return stack && !mc::stk::isEmpty(stack.get()) && mc::stk::itemId(stack.get()) == "tnt_minecart";
}

void updateCartRefill() {
    const CartRefillSettings& st = g_cartRefillSettings;
    if (!g_cartRefill || gui::inputFrozen()) {
        if (g_cartScreen.phase == ScreenPhase::Working && !gui::inputFrozen())
            inventoryWorkDone(g_cartScreen, false);
        return;
    }
    // Hold every refill until the Xbow Macro has finished its whole sequence. Refilling in the
    // middle of the macro - and, with Silent Refill off, the inventory screen it opens - froze
    // input and aborted the shot before it went out. The macro requests a refill the moment it
    // ends (xbowMacroReset -> cartRefillRequestNow), so the cart it used is restocked then.
    if (g_xbowStep != XbowStep::Idle) {
        if (g_cartScreen.phase == ScreenPhase::Working && !gui::inputFrozen())
            inventoryWorkDone(g_cartScreen, st.silent);
        return;
    }
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return;
    const double now = ImGui::GetTime();
    jobject playerObj = mc::Client::player();
    if (!playerObj) return;
    mc::LivingEntity self(playerObj);
    if (!self.alive()) { inventoryWorkDone(g_cartScreen, st.silent); return; }
    mc::LocalRef inventory(mc::Player(playerObj).inventory());
    if (!inventory) return;
    mc::LocalRef list(mc::inv::mainList(inventory.get()));
    if (!list) return;
    const int size = std::min(36, mc::inv::listSize(list.get()));

    // Decide whether anything is actually missing BEFORE the inventory is touched. This is what
    // stops the visible path looping: it used to open the inventory first, find every watched slot
    // already stocked, close it, and open it again on the next pass for ever.
    int wantHotbar = -1, source = -1;
    for (int hotbar = 0; hotbar < 9 && hotbar < size && wantHotbar < 0; ++hotbar) {
        if (!st.slots[hotbar]) continue;
        if (isCartAt(list.get(), hotbar)) continue;          // this slot already has its cart
        for (int i = 9; i < size; ++i) {
            if (!isCartAt(list.get(), i)) continue;
            wantHotbar = hotbar;
            source = i;
            break;
        }
    }
    if (wantHotbar < 0) {                                    // nothing to do: the inventory stays shut
        if (g_cartScreen.phase != ScreenPhase::Idle) inventoryWorkDone(g_cartScreen, st.silent);
        return;
    }
    if (now < g_cartRefillNextAt) return;
    // Only now, with a refill actually waiting, is the inventory opened (and then only when the
    // Silent option is off).
    if (!inventoryWorkReady(g_cartScreen, st.silent)) return;
    if (!mc::Client::playerScreenIsCurrent()) { inventoryWorkDone(g_cartScreen, st.silent); return; }
    const int screenSlot = mc::Client::playerScreenSlotForMainIndex(source);
    if (screenSlot < 0 || !mc::Client::swapHotbarSlot(screenSlot, wantHotbar)) {
        g_cartRefillNextAt = now + 0.2;
        return;
    }
    // The move is done, and the screen stays up until the swap has really landed: closing it here
    // re-opened it on the very next pass (the slot is not refilled until the server answers), which
    // is exactly the open/close loop this module used to spin in. The pass that finds every watched
    // slot already stocked is the one that closes it.
    g_cartRefillNextAt = now + std::clamp(st.delayMs, 0, 600) / 1000.0;
}

// Uses a buff potion when its effect is missing: the potion is moved into the chosen hotbar
// slot (silently through slot packets, or with the inventory opened first), the player looks
// straight down and the potion is used.
double g_potNextAt = 0.0, g_potNoticeAt = -100.0;
InvScreenState g_potScreen;
bool g_potEverUsed = false;
// The pitch that counts as looking down for a throw. It used to be 80 degrees, which refused a
// view that was plainly down; 65 is the same gate the throw reports use now.
constexpr float kPotLookDownPitch = 65.f;
constexpr float kPotLookDownTarget = 89.f;   // Self Aim off: the camera walks down toward this pitch
// The throw holds the game's own "Use Item / Place Block" key down for a few frames. The game reads
// that key on its own tick, so the pitch has to stay down (and the key held) until the tick has
// actually used the potion - restoring both in the frame the key went down is why the potion was
// reported as never being thrown.
bool g_potThrowing = false;
double g_potThrowAt = 0.0;
// When the look-down finished for the current throw (-1 = not looking down yet). Pot Use Delay counts from here.
double g_potLookDownAt = -1.0;
// Look Back for the throw: the view held before the aim started. The shared Smooth Look Back walk
// is armed with these once the potion has actually left the hand.
float g_potBackYaw = 0.f, g_potBackPitch = 0.f;
bool  g_potHaveBack = false;
// 180 Pot: the yaw the horizontal turn is walking towards, and whether this throw armed its view
// state yet (the back view above is captured once, before the turn moves anything).
float g_potAimYaw = 0.f;
bool  g_potAimArmed = false;
// The last refill move that was sent: the inventory list only reflects it after the server answers.
int    g_potMoveTarget = -1, g_potMoveSource = -1;
double g_potMoveAt = 0.0;

// A toggle, not a bind. Every ender pearl seen leaving the player is followed as a live entity,
// and the wind charge's flight is solved against the pearl's own sampled one: nothing here waits
// for a fixed delay, so a pearl that inherits the player's speed (a sprinting or gliding throw) is
// met on the arc it really flies. The charge is thrown on the first tick its own travel reaches the
// pearl, which is the boost that carries the pearl - and the player who teleports to it - onto the
// high ground.
struct WindPearl {
    Vec3 pos{};              // last position read from the live entity
    Vec3 vel{};              // its velocity on that read
    double firstAt = 0.0;    // when this module first saw it (the throw)
    double seenAt = 0.0;     // when its live state was last read
    bool   used = false;     // a charge has already been thrown at it
};
std::unordered_map<std::string, WindPearl> g_windPearls;
double g_windNextAt = 0.0, g_windNoticeAt = -100.0, g_windSwapAt = -1.0;

// The solver itself (the charge's per-tick travel, the ping offset and the first reachable meeting
// tick) lives in wind_charge.h, where test/wind_charge_check.cpp exercises exactly this code.

void autoWindChargeReset() {
    g_windPearls.clear();
    g_windNextAt = ImGui::GetTime() + 0.05;
}


// ---- Instant Cart Macro --------------------------------------------------------------------
// Bind -> fire a partially charged bow, then place a rail and a TNT minecart on the block under the
// crosshair. The bind itself no longer needs a block in view (that used to refuse the whole
// sequence before the bow was even drawn); the block only has to be there for the two placements,
// which are made with the game's own "Use Item / Place Block" key rather than an interactBlock call
// built here, so the game's placement rule (and the cell the block really lands in) is the game's.
enum class CartMacroStep { Idle, BowDraw, BowRelease, Rail, RailPlace, Cart, CartPlace, Done };
CartMacroStep g_cartStep = CartMacroStep::Idle;
double g_cartStageAt = 0.0, g_cartNextAt = 0.0;
double g_cartUseReleaseAt = -1.0;      // when the held use key for a placement is let go
int g_cartOrigSlot = -1;
int g_cartRailX = 0, g_cartRailY = 0, g_cartRailZ = 0;
bool g_cartRailValid = false;
bool g_cartBowDrawn = false;

void instantCartReset() {
    if (g_cartStep != CartMacroStep::Idle) mc::ext::setKey(mc::ext::Key::Use, false);
    g_cartStep = CartMacroStep::Idle;
    g_cartRailValid = false;
    g_cartBowDrawn = false;
    g_cartUseReleaseAt = -1.0;
    g_cartNextAt = ImGui::GetTime() + 0.1;
}

void instantCartFinish(jobject inventory) {
    if (g_cartStep != CartMacroStep::Idle) mc::ext::setKey(mc::ext::Key::Use, false);
    if (g_instantCartSettings.swapBack && inventory && g_cartOrigSlot >= 0 && g_cartOrigSlot <= 8)
        restoreTemporarySlot(inventory, g_cartOrigSlot, true);
    g_cartOrigSlot = -1;
    g_cartRailValid = false;
    g_cartBowDrawn = false;
    g_cartUseReleaseAt = -1.0;
    g_cartStep = CartMacroStep::Idle;
    g_cartNextAt = ImGui::GetTime() + 0.1;
}

void updateInstantCartMacro() {
    if (macroTapped(g_instantCartKey) && g_cartStep == CartMacroStep::Idle && ImGui::GetTime() >= g_cartNextAt) {
        const bool ready = jni::ready() && mc::frame::valid() && mc::frame::resolved();
        jobject tapPlayer = ready ? mc::Client::player() : nullptr;
        if (!tapPlayer) return;
        // The bind starts the bow straight away. The block under the crosshair is only needed by
        // the rail placement, which is checked when it actually happens, so looking away while the
        // bow is drawn (or binding before turning onto a block) no longer refuses the sequence.
        g_cartStep = CartMacroStep::BowDraw;
        g_cartStageAt = ImGui::GetTime();
    }
    if (g_cartStep == CartMacroStep::Idle) return;
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) { instantCartReset(); return; }
    jobject playerObj = mc::Client::player();
    mc::LocalRef inventory(playerObj ? mc::Player(playerObj).inventory() : nullptr);
    mc::LocalRef list(inventory ? mc::inv::mainList(inventory.get()) : nullptr);
    if (!playerObj || !inventory || !list) { instantCartReset(); return; }
    // The bow steps and the two key-driven placements deliberately hold the use key, which makes
    // playerCanUseModule report a busy hand; the check must not end the sequence while the key is
    // held for the game to act on.
    const bool drawing = g_cartStep == CartMacroStep::BowDraw || g_cartStep == CartMacroStep::BowRelease ||
                         g_cartStep == CartMacroStep::RailPlace || g_cartStep == CartMacroStep::CartPlace;
    if (gui::inputFrozen() || (!drawing && !playerCanUseModule(playerObj))) {
        instantCartFinish(inventory.get());
        return;
    }
    mc::Player player(playerObj);
    const double now = ImGui::GetTime();
    if (now - g_cartStageAt > 5.0) { instantCartFinish(inventory.get()); return; }   // watchdog
    const int selected = std::clamp(mc::inv::selectedSlot(inventory.get()), 0, 8);
    if (g_cartOrigSlot < 0) g_cartOrigSlot = selected;
    const InstantCartMacroSettings& st = g_instantCartSettings;
    switch (g_cartStep) {
        case CartMacroStep::BowDraw: {
            const int slot = findHotbarItem(list.get(), "bow");
            if (slot < 0) { notifyAction("Instant Cart Macro needs a bow in the hotbar"); instantCartFinish(inventory.get()); return; }
            if (selected != slot) { selectSlot(inventory.get(), slot); return; }   // swap, draw next frame
            if (!g_cartBowDrawn) {
                // Start the draw directly, then keep the use key held so the game's own tick does not
                // release the bow the next frame (a bow only charges while the use key is down).
                mc::Client::setItemUseCooldown(4);
                mc::Client::interactItem(false);
                mc::ext::setKey(mc::ext::Key::Use, true);
                g_cartBowDrawn = true;
                g_cartStageAt = now;
                return;
            }
            if (now - g_cartStageAt >= std::clamp(st.bowChargeMs, 50, 900) / 1000.0) {
                g_cartStep = CartMacroStep::BowRelease;
                g_cartStageAt = now;
            }
            return;
        }
        case CartMacroStep::BowRelease: {
            // Release the use key: the game fires the bow at whatever charge it reached (never a
            // full draw, since Bow Draw Time is bounded below a full charge).
            mc::ext::setKey(mc::ext::Key::Use, false);
            g_cartBowDrawn = false;
            if (now - g_cartStageAt < 0.08) return;               // let the shot go out
            g_cartStep = CartMacroStep::Rail;
            g_cartStageAt = now;
            return;
        }
        case CartMacroStep::Rail: {
            if (now - g_cartStageAt < std::clamp(st.railDelayMs, 0, 600) / 1000.0) return;
            const int slot = findAnyRail(list.get());
            if (slot < 0) { notifyAction("Instant Cart Macro needs a rail in the hotbar"); instantCartFinish(inventory.get()); return; }
            if (selected != slot) { selectSlot(inventory.get(), slot); return; }
            mc::LocalRef hit(mc::Client::crosshair());
            int x = 0, y = 0, z = 0;
            if (!hit || mc::hit::typeOf(hit.get()) != 1 || !mc::hit::blockPos(hit.get(), x, y, z)) {
                notifyAction("Instant Cart Macro: look at the block the rail should sit on");
                instantCartFinish(inventory.get());
                return;
            }
            // The block has to be inside block reach, or the placement the key sends is one the
            // server refuses.
            const float reach = (float)player.blockInteractionRange() + 0.05f;
            if ((Vec3((float)x + 0.5f, (float)y + 0.5f, (float)z + 0.5f) - player.eyePos()).len() > reach) {
                notifyAction("Instant Cart Macro: look at a block within block range");
                instantCartFinish(inventory.get());
                return;
            }
            // The cell the rail lands in is the cell the click would fill, read off the face the
            // crosshair is on - the game's own placement rule, not an assumed top face.
            int fx = 0, fy = 0, fz = 0;
            if (!mc::hit::sideOffset(hit.get(), fx, fy, fz)) { fy = 1; }
            g_cartRailX = x + fx;
            g_cartRailY = y + fy;
            g_cartRailZ = z + fz;
            g_cartRailValid = true;
            mc::Client::setItemUseCooldown(0);
            mc::ext::setKey(mc::ext::Key::Use, true);      // the game's own "Use Item / Place Block"
            g_cartUseReleaseAt = now + 0.06;               // held a beat so the game's tick reads it
            g_cartStep = CartMacroStep::RailPlace;
            g_cartStageAt = now;
            return;
        }
        case CartMacroStep::RailPlace: {
            if (now < g_cartUseReleaseAt) return;
            mc::ext::setKey(mc::ext::Key::Use, false);
            g_cartStep = CartMacroStep::Cart;
            g_cartStageAt = now;
            return;
        }
        case CartMacroStep::Cart: {
            if (!g_cartRailValid) { notifyAction("Instant Cart Macro: the rail position was lost"); instantCartFinish(inventory.get()); return; }
            if (now - g_cartStageAt < std::clamp(st.cartDelayMs, 0, 600) / 1000.0) return;
            const int slot = findHotbarItem(list.get(), "tnt_minecart");
            if (slot < 0) { notifyAction("Instant Cart Macro needs a TNT minecart in the hotbar"); instantCartFinish(inventory.get()); return; }
            if (selected != slot) { selectSlot(inventory.get(), slot); return; }
            // The minecart is placed with the same key while the crosshair is still on the block the
            // rail sits on, exactly as a player would: the rail is in the cell above it, so the
            // minecart goes onto the rail.
            mc::Client::setItemUseCooldown(0);
            mc::ext::setKey(mc::ext::Key::Use, true);
            g_cartUseReleaseAt = now + 0.06;
            g_cartStep = CartMacroStep::CartPlace;
            g_cartStageAt = now;
            return;
        }
        case CartMacroStep::CartPlace: {
            if (now < g_cartUseReleaseAt) return;
            mc::ext::setKey(mc::ext::Key::Use, false);
            g_cartStep = CartMacroStep::Done;
            g_cartStageAt = now;
            return;
        }
        default:
            instantCartFinish(inventory.get());
            return;
    }
}

// ---- Pearl Catch --------------------------------------------------------------------------
// Mace: one bind throws an ender pearl and then a wind charge, both from the hotbar. The pearl is
// what travels; the charge is the catch - it pops the pearl's flight up so the throw reaches the
// ledge it was aimed at instead of falling short. Every step is the game's own use-item call and
// its own slot packets, so the server sees an ordinary player doing both.
enum class PearlCatchStep { Idle, PearlSelect, PearlThrow, WindSelect, WindThrow, Restore };
PearlCatchStep g_pearlCatchStep = PearlCatchStep::Idle;
double g_pearlCatchStageAt = 0.0, g_pearlCatchNextAt = 0.0, g_pearlCatchSwapAt = -1.0;
int g_pearlCatchOrigSlot = -1;

void pearlCatchReset(jobject inventory, bool restore) {
    if (restore && g_pearlCatchSettings.swapBack && inventory &&
        g_pearlCatchOrigSlot >= 0 && g_pearlCatchOrigSlot <= 8)
        restoreTemporarySlot(inventory, g_pearlCatchOrigSlot, true);
    g_pearlCatchStep = PearlCatchStep::Idle;
    g_pearlCatchOrigSlot = -1;
    g_pearlCatchSwapAt = -1.0;
    g_pearlCatchNextAt = ImGui::GetTime() + 0.1;
}

void updatePearlCatch() {
    if (macroTapped(g_pearlCatchKey) && g_pearlCatchStep == PearlCatchStep::Idle &&
        ImGui::GetTime() >= g_pearlCatchNextAt) {
        const bool ready = jni::ready() && mc::frame::valid() && mc::frame::resolved();
        jobject playerObj = ready ? mc::Client::player() : nullptr;
        if (playerObj) {
            mc::LocalRef inventory(mc::Player(playerObj).inventory());
            g_pearlCatchOrigSlot = inventory
                ? std::clamp(mc::inv::selectedSlot(inventory.get()), 0, 8) : -1;
            g_pearlCatchStep = PearlCatchStep::PearlSelect;
            g_pearlCatchStageAt = ImGui::GetTime();
            g_pearlCatchSwapAt = -1.0;
        }
    }
    if (g_pearlCatchStep == PearlCatchStep::Idle) return;
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) {
        pearlCatchReset(nullptr, false);
        return;
    }
    jobject playerObj = mc::Client::player();
    mc::LocalRef inventory(playerObj ? mc::Player(playerObj).inventory() : nullptr);
    mc::LocalRef list(inventory ? mc::inv::mainList(inventory.get()) : nullptr);
    if (!playerObj || !inventory || !list) { pearlCatchReset(nullptr, false); return; }
    // The throw itself leaves the hand briefly busy; the steps after it must not be mistaken for
    // "a player using an item" and cancel the sequence.
    const bool throwing = g_pearlCatchStep == PearlCatchStep::PearlThrow ||
                          g_pearlCatchStep == PearlCatchStep::WindThrow;
    if (gui::inputFrozen() || (!throwing && !playerCanUseModule(playerObj))) {
        pearlCatchReset(inventory.get(), true);
        return;
    }
    mc::Player player(playerObj);
    const double now = ImGui::GetTime();
    if (now - g_pearlCatchStageAt > 4.0) { pearlCatchReset(inventory.get(), true); return; }  // watchdog
    const PearlCatchSettings& st = g_pearlCatchSettings;
    const int selected = std::clamp(mc::inv::selectedSlot(inventory.get()), 0, 8);

    const auto useHeld = [&](const char* itemName) -> bool {
        mc::Client::setItemUseCooldown(4);
        const bool used = mc::Client::interactItem(false);
        if (used) player.swingHand();
        else notifyAction((std::string("Pearl Catch: the game refused the ") + itemName + " use").c_str());
        return used;
    };
    const auto selectFor = [&](const char* itemName) -> int {
        const int slot = findHotbarItem(list.get(), itemName);
        if (slot < 0) {
            notifyAction((std::string("Pearl Catch needs a ") + itemName + " in the hotbar").c_str());
            pearlCatchReset(inventory.get(), true);
            return -1;
        }
        if (selected != slot) {
            selectSlot(inventory.get(), slot);
            g_pearlCatchSwapAt = now;
            g_pearlCatchStageAt = now;      // the watchdog restarts on real progress
            return -2;                       // swapped: act on a later frame
        }
        return slot;
    };

    switch (g_pearlCatchStep) {
        case PearlCatchStep::PearlSelect: {
            if (now - g_pearlCatchStageAt < std::clamp(st.pearlDelayMs, 0, 600) / 1000.0) return;
            if (selectFor("ender_pearl") == -2) return;
            if (g_pearlCatchStep != PearlCatchStep::PearlSelect) return;   // missing item: reset above
            // A slot change needs its own server tick before the use is validated against it.
            if (!xbow::slotSettled(now, g_pearlCatchSwapAt)) return;
            if (selected == std::clamp(mc::inv::selectedSlot(inventory.get()), 0, 8) &&
                hotbarItemId(list.get(), selected) == "ender_pearl")
                useHeld("ender pearl");
            g_pearlCatchStep = PearlCatchStep::PearlThrow;
            g_pearlCatchStageAt = now;
            return;
        }
        case PearlCatchStep::PearlThrow: {
            if (now - g_pearlCatchStageAt < std::clamp(st.windChargeDelayMs, 0, 600) / 1000.0) return;
            g_pearlCatchStep = PearlCatchStep::WindSelect;
            g_pearlCatchStageAt = now;
            return;
        }
        case PearlCatchStep::WindSelect: {
            if (selectFor("wind_charge") == -2) return;
            if (g_pearlCatchStep != PearlCatchStep::WindSelect) return;
            if (!xbow::slotSettled(now, g_pearlCatchSwapAt)) return;
            if (hotbarItemId(list.get(), std::clamp(mc::inv::selectedSlot(inventory.get()), 0, 8)) == "wind_charge")
                useHeld("wind charge");
            g_pearlCatchStep = PearlCatchStep::WindThrow;
            g_pearlCatchStageAt = now;
            return;
        }
        case PearlCatchStep::WindThrow: {
            if (now - g_pearlCatchStageAt < 0.08) return;
            g_pearlCatchStep = PearlCatchStep::Restore;
            return;
        }
        case PearlCatchStep::Restore:
            pearlCatchReset(inventory.get(), true);
            return;
        default:
            pearlCatchReset(inventory.get(), true);
            return;
    }
}

// While a spear is held and a valid target is in front of it, the spear is used (right-clicked) on
// a cadence so its poke lands without the player clicking. Configurations: players / entities,
// range, use delay, Only When Looking and Silent Aim.


// The spear's damage scales with the user's velocity (falling and movement). On the frame the
// spear is used, this writes a large client-side velocity to the player so the use is read at that
// speed. Silent restores the real velocity the same frame, so the movement the server is told about
// is left alone - the speed exists only for the instant of the hit.
bool g_instantSpearHeld = false;
Vec3 g_instantSpearRealVel{};
double g_instantSpearUntil = 0.0, g_instantSpearNextAt = 0.0;


// ---- Pearl Agro ----------------------------------------------------------------------------
// Remembers the last entity you attacked. When a pearl appears next to it, the throw is
// solved so your own pearl lands on the same spot, then it is thrown — after the Agro Delay.
// The enemy you attacked is remembered for PEARL_AGRO_WINDOW seconds. Their pearl is followed
// while it is in the air and its landing spot is re-solved from its live position and velocity, so
// our own pearl is aimed at the spot it will really land on - you come out of the throw already
// next to the player you were fighting.
constexpr double PEARL_AGRO_WINDOW = 3.0;
std::string g_pearlAgroTargetUuid;
double g_pearlAgroTargetAt = 0.0, g_pearlAgroReadyAt = 0.0, g_pearlAgroNoticeAt = -100.0;
double g_pearlAgroLandingAt = 0.0;
bool g_pearlAgroArmed = false;
Vec3 g_pearlAgroLanding{};

// Vanilla ender pearl flight: 1.5 blocks/tick launch, gravity 0.03, drag 0.99 per tick.
Vec3 simulatePearlLanding(const Vec3& pos, const Vec3& vel) {
    Vec3 p = pos, v = vel;
    for (int tick = 0; tick < 120; ++tick) {
        p = p + v;
        v = v * 0.99f;
        v.y -= 0.03f;
        if (p.y < -70.f) break;
        const int bx = (int)std::floor(p.x), by = (int)std::floor(p.y), bz = (int)std::floor(p.z);
        if (!isAirId(mc::Client::blockIdAt(bx, by, bz))) break;
    }
    return p;
}

// Distance between where a pearl launched from `eye` at `yawDeg`/`pitchDeg` lands and `want`.
float pearlLandingError(const Vec3& eye, float yawDeg, float pitchDeg, const Vec3& want) {
    const float yaw = yawDeg * 0.017453292f, pitch = pitchDeg * 0.017453292f;
    Vec3 v(-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch));
    v = v * 1.5f;
    return (simulatePearlLanding(eye, v) - want).len();
}

// An ender pearl, recognised by its registry type instead of its display name: the name is
// translated ("Ender Pearl" exists only on an English client), so a localized game never matched
// one and the module silently found nothing to answer. The name is kept as a fallback.
bool isPearlEntity(jobject entity) {
    const std::string type = mc::entityTypePath(entity);
    if (!type.empty()) return type == "ender_pearl";
    return mc::Entity(entity).displayName().find("Ender Pearl") != std::string::npos;
}

// The nearest ender pearl entity around a position (the target's fresh pearl).
mc::LocalRef findPearlNear(jobject world, const Vec3& around, float radius) {
    JNIEnv* env = jni::env();
    if (!env) return {};
    mc::LocalRef best;
    float bestDist = radius;
    for (jobject entity : mc::collectEntities(world, kEntityScan)) {
        if (!entity) continue;
        if (isPearlEntity(entity)) {
            const float d = (mc::Entity(entity).pos() - around).len();
            if (d < bestDist) { bestDist = d; best = mc::LocalRef(env->NewLocalRef(entity)); }
        }
        env->DeleteLocalRef(entity);
    }
    return best;
}

// The UUID key of the player closest to a position inside `radius`, or empty. A pearl found near the
// player is only the last-hit player's when that player is the one it belongs to, which is what stops
// Pearl Agro answering somebody else's pearl.
std::string nearestPlayerKeyNear(const Vec3& at, float radius) {
    JNIEnv* env = jni::env();
    if (!env) return {};
    jobject world = mc::Client::world();
    if (!world) return {};
    std::string best;
    float nearest = radius;
    for (jobject entity : mc::collectEntities(world, kEntityScan)) {
        if (!entity) continue;
        const bool player = isPlayerEntity(env, entity);
        if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(entity); continue; }
        // The local player is never an answer: a pearl flying towards us is closest to US, and that
        // would reject the only pearl the tracked target ever threw.
        const bool self = env->IsSameObject(entity, mc::Client::player()) == JNI_TRUE;
        if (!player || self) { env->DeleteLocalRef(entity); continue; }
        const float d = (mc::Entity(entity).pos() - at).len();
        if (d <= nearest) { nearest = d; best = trackedTargetKey(entity); }
        env->DeleteLocalRef(entity);
    }
    return best;
}

void trackAgroTarget() {
    // Only a swing the player really made counts as "the last hit": a fresh click of their own, or
    // a swing another module sent this frame. Reading the entity under the crosshair on any click was
    // too loose - a click at the ground or at a pearl still recorded whatever happened to be behind it.
    if (!playerAttackTapped() && !g_attackSentThisFrame) return;
    mc::LocalRef hit(mc::Client::crosshair());
    if (!hit || mc::hit::typeOf(hit.get()) != 2) return;
    mc::LocalRef entity(mc::hit::entity(hit.get()));
    if (!entity) return;
    JNIEnv* env = jni::env();
    if (!env || !mc::sym().living) return;
    if (env->IsInstanceOf(entity.get(), mc::sym().living) != JNI_TRUE) { env->ExceptionClear(); return; }
    if (!mc::LivingEntity(entity.get()).alive()) return;
    if (env->IsSameObject(entity.get(), mc::Client::player()) == JNI_TRUE) return;
    const std::string uuid = trackedTargetKey(entity.get());
    if (uuid.empty()) return;
    if (uuid != g_pearlAgroTargetUuid) g_pearlAgroArmed = false;   // a new last hit restarts the answer
    g_pearlAgroTargetUuid = uuid;
    g_pearlAgroTargetAt = ImGui::GetTime();
}

void updatePearlAgro() {
    const PearlAgroSettings& st = g_pearlAgroSettings;
    if (!g_pearlAgro) { g_pearlAgroArmed = false; g_pearlAgroTargetUuid.clear(); return; }
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return;
    if (gui::inputFrozen() || mc::Client::screenOpen()) return;
    trackAgroTarget();
    const double now = ImGui::GetTime();
    if (g_pearlAgroTargetUuid.empty() || now - g_pearlAgroTargetAt > PEARL_AGRO_WINDOW) { g_pearlAgroArmed = false; return; }
    jobject playerObj = mc::Client::player();
    jobject world = mc::Client::world();
    if (!playerObj || !world) return;
    mc::LocalRef inventory(mc::Player(playerObj).inventory());
    if (!inventory) return;
    mc::LocalRef list(mc::inv::mainList(inventory.get()));
    if (!list) return;
    const int pearlSlot = findHotbarItem(list.get(), "ender_pearl");
    if (pearlSlot < 0) { rateLimitedNotify(g_pearlAgroNoticeAt, "Pearl Agro needs an ender pearl in the hotbar", 6.0); return; }
    mc::LocalRef target = findPlayerByUuid(g_pearlAgroTargetUuid);
    if (!target) { g_pearlAgroArmed = false; return; }
    // FOV: the player you last hit has to be inside this cone in front of the view before the pearl
    // is thrown. Agro answers somebody you are actually facing, instead of spinning round to pearl
    // at a target who is behind you.
    {
        mc::Player player(playerObj);
        const mc::LivingEntity tg(target.get());
        const Vec3 eye = player.eyePos();
        const Vec3 delta = tg.pos() + Vec3(0.f, tg.height() * 0.5f, 0.f) - eye;
        const float len = delta.len();
        if (len > 0.01f) {
            const float yawRad = player.yaw() * 0.017453292519943295f;
            const float pitchRad = player.pitch() * 0.017453292519943295f;
            const Vec3 look(-std::sin(yawRad) * std::cos(pitchRad), -std::sin(pitchRad),
                            std::cos(yawRad) * std::cos(pitchRad));
            const float dot = std::clamp(look.x * delta.x / len + look.y * delta.y / len +
                                         look.z * delta.z / len, -1.f, 1.f);
            const float angle = std::acos(dot) * 57.29577951f;
            if (angle > std::clamp(st.fov, 10.f, 360.f) * 0.5f) {
                g_pearlAgroArmed = false;   // out of the cone: nothing is thrown at them
                return;
            }
        }
    }
    // Wide enough that a pearl the enemy threw a moment ago is still found after it has travelled
    // away from them, and re-checked every frame while it is in the air. The pearl is usually
    // thrown *at* the player, so the player is the second centre when the thrower has already
    // moved away from it.
    mc::LocalRef pearl = findPearlNear(world, mc::Entity(target.get()).pos(), 16.f);
    if (!pearl) {
        // A pearl already on its way towards us is only the tracked player's when they are the
        // closest player to it. Without that test the player-centred search answered any pearl in
        // the area - a teammate's, or one lying on the ground - which is the "wrong person" report.
        mc::LocalRef nearPearl = findPearlNear(world, mc::Entity(playerObj).pos(), 12.f);
        if (nearPearl && nearestPlayerKeyNear(mc::Entity(nearPearl.get()).pos(), 16.f) == g_pearlAgroTargetUuid)
            pearl = mc::LocalRef(jni::env()->NewLocalRef(nearPearl.get()));
    }
    if (!pearl) {
        g_pearlAgroArmed = false;
        rateLimitedNotify(g_pearlAgroNoticeAt, "Pearl Agro: no enemy pearl in sight", 6.0);
        return;
    }

    // The landing spot is re-solved from the pearl's live position and velocity every frame while it
    // flies, so the throw answers where it is really going to land rather than the first guess. It
    // is thrown once the Agro Delay has passed and the estimate has settled (or the pearl is nearly
    // down), with a hard cap so a pearl thrown straight up can never stall the module.
    const mc::Entity pearlEntity(pearl.get());
    const Vec3 landingNow = simulatePearlLanding(pearlEntity.pos(), pearlEntity.velocity());
    if (!g_pearlAgroArmed) {
        g_pearlAgroLanding = landingNow;
        g_pearlAgroLandingAt = now;
        g_pearlAgroReadyAt = now + std::clamp(st.agroDelayMs, 0, 600) / 1000.0;
        g_pearlAgroArmed = true;
        return;
    }
    const float drift = (landingNow - g_pearlAgroLanding).len();
    g_pearlAgroLanding = landingNow;
    if (now < g_pearlAgroReadyAt) return;
    const bool settled = drift < 0.35f || pearlEntity.velocity().len() < 0.35f;
    if (!settled && now - g_pearlAgroLandingAt < 1.5) return;   // still flying: keep tracking it

    mc::Player player(playerObj);
    const Vec3 eye = player.eyePos();
    const Vec3 d = g_pearlAgroLanding - eye;
    if (d.len() < 1.0f) { g_pearlAgroArmed = false; return; }
    const float yaw = std::atan2(-d.x, d.z) * 57.29577951f;
    // Start from the closed-form low arc, then refine with the drag-aware simulation over a
    // narrow window (each trial costs real world lookups, so the search stays small).
    const float horizontal = std::sqrt(d.x * d.x + d.z * d.z);
    float guess = 0.f;
    if (!solveThrowAngle(1.5f, 0.03f, horizontal, d.y, guess)) guess = 0.f;
    float bestPitch = guess, bestErr = pearlLandingError(eye, yaw, guess, g_pearlAgroLanding);
    for (float p = guess - 8.f; p <= guess + 8.f; p += 0.5f) {
        const float e = pearlLandingError(eye, yaw, p, g_pearlAgroLanding);
        if (e < bestErr) { bestErr = e; bestPitch = p; }
    }
    const int selected = std::clamp(mc::inv::selectedSlot(inventory.get()), 0, 8);
    if (selected != pearlSlot) { selectSlot(inventory.get(), pearlSlot); return; }
    // Silent Aim: the throw is made with the solved rotation for that one use, then the camera -
    // and the previous-tick rotation it interpolates from - are put straight back and the server
    // is told the camera's own view, so the aim lands server-side while nothing visible turns.
    const bool silent = st.silentAim;
    const auto throwPearl = [&]() {
        mc::Client::setItemUseCooldown(4);
        return mc::Client::interactItem(false);
    };
    bool thrown = false;
    if (silent) {
        thrown = silentActionAim(player, yaw, bestPitch, true, throwPearl);
    } else if (st.look == WebLookMode::Smooth) {
        // Smooth: the pearl goes out on the solved rotation in this very frame - exactly the throw
        // Silent Aim makes - and only then is the camera walked onto that same rotation at Camera
        // Sensitivity. The throw is never delayed waiting for the turn, so Smooth is as fast as
        // Instant and differs only in what the player sees.
        thrown = silentActionAim(player, yaw, bestPitch, true, throwPearl);
        startLookBack(yaw, bestPitch, st.cameraSensitivity);
    } else {
        player.setYaw(yaw);
        player.setPitch(bestPitch);
        mc::ext::sendLookPacket(yaw, bestPitch, player.onGround());
        thrown = throwPearl();
    }
    if (thrown) player.swingHand();
    else notifyAction("Pearl Agro: the ender pearl was not thrown");
    g_pearlAgroArmed = false;
    g_pearlAgroTargetUuid.clear();
}

// A critical hit while holding a sword swaps to a mace for that one click (the mace's damage
// with the sword's already-charged cooldown), then optionally swaps back.
enum class BreachPhase { Idle, Swapped };
BreachPhase g_breachPhase = BreachPhase::Idle;
double g_breachStageAt = 0.0, g_breachNextAt = 0.0, g_breachNoticeAt = -100.0;
int g_breachOrigSlot = -1;
// One swap per press: the press arms the swap, and only a release can arm another. A held button
// must not re-arm it on every frame (that is the "it clicks for me" it was reported for).
bool g_breachClickLatched = false;

// The player's own attack: the button held down, or a fresh press this frame. The overlay draws
// after the game has already run this tick's attack, so a one-frame tap can never put the mace in
// hand for the click it belongs to: the swap has to lead the swing. A held button does exactly
// that - the mace goes in and the game's own next attack is the one that uses it. This never
// attacks; it only decides when the mace should be the held item.
// One rule for "the player asked for this swing", shared with AutoMace's Only On Your Own Click:
// the button held, or a fresh press this frame. A fresh press is what Auto Shield Break's OnHit
// mode reacts to, and this module answers the same click.
bool breachAttackHeld() {
    if (playerAttackInputHeld()) return true;
    // A swing another module sent this frame is the player's click too.
    return g_attackSentThisFrame;
}

// A swap that is undone before the next game tick is a swap the game never saw, so the mace is
// always held for at least this long no matter how small the configured delay is.
constexpr double kBreachHoldFloor = 0.25;

// The hotbar slot holding a mace with Breach on it, or -1. Only a Breach mace qualifies: the
// enchantment is the entire point of the swap, and swapping to a plain mace gives up the sword's
// hit for nothing.
int breachMaceSlot(jobject list) {
    const int count = std::min(9, mc::inv::listSize(list));
    for (int i = 0; i < count; ++i) {
        mc::LocalRef stack(mc::inv::getFromList(list, i));
        if (!stack || mc::stk::isEmpty(stack.get()) || mc::stk::itemId(stack.get()) != "mace") continue;
        if (mc::Client::objToString(stack.get()).find("breach") != std::string::npos) return i;
    }
    return -1;
}


// Triggerbot V2 only. A swing from that card is the trigger (the same tick stamp the card leaves for
// Auto Shield Break's OnHit mode, so the two follow-ups can never disagree about when it swung), and
// after Shield Delay the off-hand shield goes up through the game's own "Use Item / Place Block"
// key. It is held for a random time between the minimum and maximum hold and then released, so the
// shield comes up for the hit and goes away again instead of sitting raised.
enum class AutoShieldPhase { Idle, Waiting, Holding };


// ---- Mace Protect ----
// The closest player who is falling and holding a mace inside the threshold, or nullptr. "Falling"
// is read from the entity's own vertical velocity and its ground flag, so somebody standing on a
// ledge with a mace does not trip it; only a real incoming smash does.
mc::LocalRef fallingMacePlayer(jobject playerObj, float threshold) {
    JNIEnv* env = jni::env();
    jobject world = mc::Client::world();
    if (!env || !world || !playerObj) return {};
    const Vec3 me = mc::Entity(playerObj).pos();
    mc::LocalRef best;
    float nearest = threshold;
    for (jobject entity : mc::collectEntities(world, kEntityScan)) {
        if (!entity) continue;
        const bool player = isPlayerEntity(env, entity);
        if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(entity); continue; }
        const bool self = env->IsSameObject(entity, playerObj) == JNI_TRUE;
        if (!player || self) { env->DeleteLocalRef(entity); continue; }
        mc::LivingEntity other(entity);
        if (!other.alive() || other.onGround() || other.velocity().y > -0.02f) { env->DeleteLocalRef(entity); continue; }
        const float d = (other.pos() - me).len();
        if (d > nearest) { env->DeleteLocalRef(entity); continue; }
        mc::LocalRef hand(other.mainHandStack());
        if (!hand || mc::stk::itemId(hand.get()) != "mace") { env->DeleteLocalRef(entity); continue; }
        best = mc::LocalRef(env->NewLocalRef(entity));
        nearest = d;
        env->DeleteLocalRef(entity);
    }
    return best;
}


// One bind, two presses. The first press uses the elytra sitting in the hotbar, the second uses the
// chestplate again. Both are the vanilla "use item" on the stack in that slot - exactly what a right
// click does - so the game performs the armour swap itself and nothing is written to the inventory.
enum class ElytraPhase { Idle, Swap, Use };
ElytraPhase g_elytraPhase = ElytraPhase::Idle;
double g_elytraStageAt = 0.0, g_elytraNextAt = 0.0;

int elytraSlotFor(jobject list) {
    const int count = std::min(9, mc::inv::listSize(list));
    for (int i = 0; i < count; ++i) if (hotbarItemId(list, i) == "elytra") return i;
    return -1;
}

int chestplateSlotFor(jobject list) {
    const int count = std::min(9, mc::inv::listSize(list));
    for (int i = 0; i < count; ++i) {
        const std::string id = hotbarItemId(list, i);
        if (id.size() >= 10 && id.compare(id.size() - 10, 10, "_chestplate") == 0) return i;
    }
    return -1;
}

void elytraReset() {
    g_elytraPhase = ElytraPhase::Idle;
    g_elytraNextAt = ImGui::GetTime() + 0.15;
}


// Every hit of the player's own is answered by turning the view Horizontal Turn degrees and putting
// it straight back on the next frame, so the swing carries the turn while the aim does not stay on
// it. It never attacks: the swing was already the player's.
enum class KnockPhase { Idle, Turned };


// A different engine from AutoMace, not a copy of it. Three parts:
//
//   scan    - one pass over the entities around the player scores every living target by how far
//             it is falling, how close it is and how near the view it sits, and takes the best one.
//             The crosshair is not the gate, which is what "I am falling, looking at a player, and
//             nothing happens" needed: a target in reach a few degrees off the crosshair is found.
//   plan    - the smash is a short list of timed steps, built once and then consumed exactly once
//             each, so a step the game rejects can never turn into a stream of clicks.
//   execute - a swing only goes out while the game's own crosshair pick is the target and the
//             attack cooldown is full. The click is MinecraftClient.doAttack(), the very call the
//             physical Attack And Destroy key makes, so nothing is sent that a real click would not
//             send and the swing that lands is the one the player sees.
enum class MaceV2Step { Aim, EquipAxe, SwingAxe, EquipMace, SwingMace };
enum class MaceV2Outcome { Done, Wait, Abort };
struct MaceV2Action { MaceV2Step step = MaceV2Step::Aim; double at = 0.0; };


// MMC Stun Slam Type: the same sequence with every timing floored, because MMC refuses a swap and
// click that is too fast to have come from a real tick.
int maceV2Delay(int configuredMs, bool mmc) {
    const int value = std::clamp(configuredMs, 0, 600);
    return mmc ? std::max(value, 100) : value;
}

// The mace this smash should use. Smart Breach: an entity above or level with the player while in
// the air takes the Breach mace; below them Density wins, because Density pays per block fallen and
// a target underneath is exactly the fall it pays for.

int maceV2AxeSlot(jobject list) {
    const int count = std::min(9, mc::inv::listSize(list));
    for (int i = 0; i < count; ++i) {
        const std::string id = hotbarItemId(list, i);
        if (id.size() >= 4 && id.compare(id.size() - 4, 4, "_axe") == 0) return i;
    }
    return -1;
}

// Turn the view onto the target's hitbox centre and tell the server the rotation as movement, so
// the swing is validated against the view the player is looking through.
bool maceV2AimAt(mc::Player& player, jobject targetObj) {
    if (!targetObj) return false;
    const mc::LivingEntity target(targetObj);
    const Vec3 centre = target.pos() + Vec3(0.f, target.height() * 0.5f, 0.f);
    const SilentAngles angles = silentAimAngles(player.eyePos(), centre);
    player.setYaw(angles.yaw);
    player.setPitch(angles.pitch);
    player.setPrevYaw(angles.yaw);
    player.setPrevPitch(angles.pitch);
    mc::ext::sendLookPacket(angles.yaw, angles.pitch, player.onGround());
    return true;
}

// Is the game's own crosshair pick the entity the plan is aimed at? A swing is never sent while
// this is false, because doAttack() would then hit whatever else the crosshair points at - the
// "it clicks but the mace does nothing" report.
// Geometric crosshair test. The game's pick result (mc::Client::crosshair()) is only refreshed on the
// game's own tick, so asking it on the frame we aim still returns the pre-aim hit - the AutoMaceV2
// swing then "missed" six frames in a row and aborted as "the mace swing was rejected". This tests
// the player's current yaw/pitch straight against the target's box, so it is correct immediately.
bool maceV2CrosshairOn(jobject targetObj, const mc::Player& player, float maxDistance) {
    if (!targetObj) return false;
    const mc::Entity::Bounds b = mc::LivingEntity(targetObj).interpolatedBounds(1.f);
    if (!b.valid) return false;
    const Vec3 origin = player.eyePos();
    const float yaw = player.yaw() * 0.017453292f;
    const float pitch = player.pitch() * 0.017453292f;
    const Vec3 dir(-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch));
    float tmin = 0.f, tmax = maxDistance;
    const float lo[3] = { b.min.x, b.min.y, b.min.z };
    const float hi[3] = { b.max.x, b.max.y, b.max.z };
    const float o[3] = { origin.x, origin.y, origin.z };
    const float d[3] = { dir.x, dir.y, dir.z };
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(d[i]) < 1e-6f) {
            if (o[i] < lo[i] || o[i] > hi[i]) return false;
            continue;
        }
        float t1 = (lo[i] - o[i]) / d[i], t2 = (hi[i] - o[i]) / d[i];
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return false;
    }
    return true;
}

bool maceV2CrosshairIs(jobject targetObj) {
    if (!targetObj) return false;
    JNIEnv* env = jni::env();
    if (!env) return false;
    mc::LocalRef hit(mc::Client::crosshair());
    if (!hit || mc::hit::typeOf(hit.get()) != 2) return false;
    mc::LocalRef picked(mc::hit::entity(hit.get()));
    if (!picked) return false;
    const bool same = env->IsSameObject(picked.get(), targetObj) == JNI_TRUE;
    jni::clearException(env, "mace v2 crosshair compare");
    return same;
}

// The scan: the best living target inside Attack Range. Score is fall distance first (the smash is
// worth more the further they have fallen), then closeness, then how near the view the target is -
// so the player's aim breaks a tie without ever being required.

// Silent Aim's aim point (SilentAim.java): the closest of the body points - Head, Neck, Body, Arms,
// Thighs, Legs, Feet, as fractions of the hitbox height - measured from the eye. The horizontal part
// is the nearest point of the hitbox to the eye, so the chosen point is always on the target.
Vec3 maceV2BodyPoint(const Vec3& eye, const mc::Entity::Bounds& b) {
    static constexpr float kParts[] = {0.95f, 0.85f, 0.7f, 0.65f, 0.35f, 0.2f, 0.05f};
    const Vec3 centre((b.min.x + b.max.x) * 0.5f, (b.min.y + b.max.y) * 0.5f, (b.min.z + b.max.z) * 0.5f);
    const float height = b.max.y - b.min.y;
    if (height <= 0.f) return centre;
    const float cx = std::clamp(eye.x, b.min.x, b.max.x);
    const float cz = std::clamp(eye.z, b.min.z, b.max.z);
    Vec3 best = centre;
    float bestScore = 1e30f;
    for (float f : kParts) {
        const float y = b.min.y + f * height;
        const float dx = cx - eye.x, dz = cz - eye.z, dy = y - eye.y;
        const float score = dx * dx + dz * dz + dy * dy;
        if (score < bestScore) { bestScore = score; best = Vec3(cx, y, cz); }
    }
    return best;
}

// The Silent Aim swing: the attack is sent with the aimed rotation for that one call only, the
// camera is restored right after, and the server is handed the real view back (silentActionAim).
bool maceV2SilentSwing(mc::Player& player, jobject targetObj) {
    if (!targetObj) return false;
    const mc::LivingEntity target(targetObj);
    const mc::Entity::Bounds b = target.interpolatedBounds(1.f);
    if (!b.valid) return false;
    const Vec3 eye = player.eyePos();
    const SilentAngles angles = silentAimAngles(eye, maceV2BodyPoint(eye, b));
    const bool attacked = silentActionAim(player, angles.yaw, angles.pitch, true,
                                          [&]() { return mc::Client::attackEntity(targetObj); });
    if (attacked) player.swingHand();
    return attacked;
}

// One plan step. Done consumes it, Wait keeps it for the next frame, Abort ends the whole smash.


// ---- Motion Blur (Visual) -------------------------------------------------------------------
// The frame the game has just drawn is folded into the render module's history ring and the older
// frames are mixed back over it along the direction the camera turned. Everything is driven by the
// turn: standing still draws nothing, and turning fast smears a couple of frames the way a real
// head-turn does. The whole thing is one GPU blit plus at most two quads, so nothing is taken off
// the frame rate.
double g_blurPrevYaw = 0.0, g_blurPrevPitch = 0.0;
bool g_blurHavePrev = false;

void updateMotionBlur() {
    const bool ready = jni::ready() && mc::frame::valid() && mc::frame::resolved();
    jobject camera = ready ? mc::Client::camera() : nullptr;
    // Only the world is blurred: while a Minecraft screen (inventory, chat, pause) or the Passive
    // menu is up, the trail is dropped so the UI is never smeared.
    const bool worldOnly = !mc::Client::screenOpen() && !gui::menuOpen();
    if (!g_motionBlur || !g_motionBlurSettings.enabled || !camera || !worldOnly) {
        motionblur::release();
        g_blurHavePrev = false;
        return;
    }
    const double yaw = mc::cam::yaw(camera), pitch = mc::cam::pitch(camera);
    float turnYaw = 0.f, turnPitch = 0.f;
    if (g_blurHavePrev) {
        turnYaw = angleDeltaDeg((float)g_blurPrevYaw, (float)yaw);
        turnPitch = (float)(pitch - g_blurPrevPitch);
    }
    g_blurPrevYaw = yaw;
    g_blurPrevPitch = pitch;
    g_blurHavePrev = true;
    motionblur::Params params;
    params.method = (int)g_motionBlurSettings.method;
    params.strength = g_motionBlurSettings.strength;
    params.accumulationMax = g_motionBlurSettings.accumulationMax;
    params.accumulationMix = g_motionBlurSettings.accumulationMix;
    params.weighted = g_motionBlurSettings.profile == BlurProfile::Weighted;
    params.refreshScaling = g_motionBlurSettings.refreshScaling;
    params.sensitivity = g_motionBlurSettings.sensitivity;
    params.turnYaw = turnYaw;
    params.turnPitch = turnPitch;
    params.deltaTime = std::clamp(ImGui::GetIO().DeltaTime, 0.0005f, 0.25f);
    motionblur::compose(params);
}

// The five Clarity cosmetics share one Java bridge, so one call drives all of them. Calling it
// per module would re-resolve the same five classes and their base class every frame.
void updateClarityCosmetics(float tickDelta) {
    if (!g_playerModel && !g_wings && !g_headwear && !g_capes && !g_viewModel) return;
    if (!jni::ready() || !jni::clarityAvailable()) return;
    jni::ClarityToggles toggles;
    toggles.playerModel = g_playerModel;
    toggles.wings       = g_wings;
    toggles.headwear    = g_headwear;
    toggles.capes       = g_capes;
    toggles.viewModel   = g_viewModel;
    jni::updateClarityVisuals(tickDelta, toggles);
}

} // namespace

// Public entry point for hooks::setPreRenderCallback (defined above, in the file-local namespace).
// fromPreRender=true: this writer claims the frame, so the end-of-frame fallback cannot double-write.
void applySilentBodyPose() { applySilentBodyPoseImpl(true); }

// Public entry points for the watchdog thread (declared in modules.h, defined in the file-local
// namespace above - these wrappers give them external linkage).
bool silentAimHoldingPose()  { return silentAimHoldingPoseImpl(); }
void silentPoseWatchdogWrite() { silentPoseWatchdogWriteImpl(); }

// The WndProc hook runs between frames: it is the last moment before the game's tick processes a
// mouse press. A place / use / break packet is sent inside that tick, ahead of the tick's own
// movement packet, so the camera has to be on the server *now* - waiting for the next frame (and
// the next tick) is a tick too late for the action this press is making. This never consumes the
// message; the game must still receive every click.
bool onInputMessage(void* hwnd, uint32_t msg, uint64_t wParam, int64_t lParam) {
    (void)hwnd; (void)wParam; (void)lParam;
    if (!g_silentAimActive || gui::inputFrozen() || !jni::ready() || !mc::frame::resolved()) return false;
    // Pick-level aim: the game itself answers the click against the aim, so there is nothing to hand
    // over - and a world action must not be started while the crosshair is on the target.
    if (jni::silentAimPickActive()) return false;
    if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) {
        silentAimActionHold(false, kActionLease);   // place / eat / shield / use item
    } else if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) {
        if (g_cameraOnBlock) silentAimActionHold(true, kActionLease);   // break
        else g_actionHoldUntil = -1.0;                                   // attack: aim takes it back
    }
    return false;
}

const std::vector<Entry>& entries() { return g_entries; }

void registerJavaEntries(const std::vector<Entry>& cards) {
    g_entries = g_builtinEntries;
    for (const Entry& card : cards) {
        if (!card.name) continue;
        const std::string name = card.name;
        g_entries.erase(std::remove_if(g_entries.begin(), g_entries.end(),
            [&](const Entry& entry) { return entry.name && name == entry.name; }),
            g_entries.end());
    }
    g_entries.insert(g_entries.end(), cards.begin(), cards.end());
}
EspSettings& espSettings() { return g_espSettings; }
AutoAnchorSettings& autoAnchorSettings() { return g_autoAnchorSettings; }
AutoShieldBreakSettings& autoShieldBreakSettings() { return g_autoShieldBreakSettings; }
AutoBucketDrainSettings& autoBucketDrainSettings() { return g_autoBucketDrainSettings; }
AutoPressurePlateWebSettings& autoPressurePlateWebSettings() { return g_autoPressurePlateWebSettings; }
FastPlaceSettings& fastPlaceSettings() { return g_fastPlaceSettings; }
AutoJumpResetSettings& autoJumpResetSettings() { return g_autoJumpResetSettings; }
AimAssistSettings& aimAssistSettings() { return g_aimAssistSettings; }
SilentAimSettings& silentAimModuleSettings() { return g_silentAimModuleSettings; }
TriggerbotSettings& triggerbotSettings() { return g_triggerbotSettings; }
AutoMaceSettings& autoMaceSettings() { return g_autoMaceSettings; }
AntiBotSettings& antiBotSettings() { return g_antiBotSettings; }
MotionBlurSettings& motionBlurSettings() { return g_motionBlurSettings; }
NametagsSettings& nametagsSettings() { return g_nametagsSettings; }
HudSettings& hudSettings() { return g_hudSettings; }
StaticFovSettings& staticFovSettings() { return g_staticFovSettings; }
AutoTotemSettings& autoTotemSettings() { return g_autoTotemSettings; }
AutoCleanSettings& autoCleanSettings() { return g_autoCleanSettings; }
InstantCartMacroSettings& instantCartMacroSettings() { return g_instantCartSettings; }
PearlCatchSettings& pearlCatchSettings() { return g_pearlCatchSettings; }
CartRefillSettings& cartRefillSettings() { return g_cartRefillSettings; }
PearlAgroSettings& pearlAgroSettings() { return g_pearlAgroSettings; }
ShaderEspSettings& shaderEspSettings() { return g_shaderEspSettings; }
DragClickSettings& dragClickSettings() { return g_dragClickSettings; }
PlayerModelSettings& playerModelSettings() { return g_playerModelSettings; }
WingsSettings& wingsSettings() { return g_wingsSettings; }
HeadwearSettings& headwearSettings() { return g_headwearSettings; }
CapesSettings& capesSettings() { return g_capesSettings; }
ViewModelSettings& viewModelSettings() { return g_viewModelSettings; }

bool isPlayerModelEnabled() { return g_playerModel; }
bool isWingsEnabled() { return g_wings; }
bool isHeadwearEnabled() { return g_headwear; }
bool isCapesEnabled() { return g_capes; }
bool isViewModelEnabled() { return g_viewModel; }
StunWebSettings& stunWebSettings() { return g_stunWebSettings; }
LogoutSpotsSettings& logoutSpotsSettings() { return g_logoutSpotsSettings; }
WalkTrailSettings& walkTrailSettings() { return g_walkTrailSettings; }
bool isWalkTrailEnabled() { return g_walkTrail; }

// ---- Debug Mode / Notifications -------------------------------------------------------------
// Debug Mode gates every module action toast; Notifications covers the "Enabled <module>"
// and "Friend <name> joined" messages, which are useful without the action spam.
bool& debugMode() { return g_debugMode; }

bool& notificationsEnabled() { return g_notifications; }

void notifyAction(const std::string& text) {
    if (!g_debugMode || text.empty()) return;
    gui::notifyTagged(text.c_str());
}

void notifyModuleToggled(const std::string& name, bool enabled) {
    if (!g_notifications || name.empty()) return;
    const std::string message = (enabled ? "Enabled " : "Disabled ") + name;
    gui::notify(message.c_str());
}

// ---- Friends list -------------------------------------------------------------------------
bool isFriend(const std::string& name) { return isFriendName(name); }

std::vector<std::string> friends() { return g_friends; }

bool addFriend(const std::string& name) {
    // Trim surrounding whitespace; reject blanks and duplicates.
    size_t begin = 0, end = name.size();
    while (begin < end && std::isspace((unsigned char)name[begin])) ++begin;
    while (end > begin && std::isspace((unsigned char)name[end - 1])) --end;
    const std::string trimmed = name.substr(begin, end - begin);
    if (trimmed.empty()) return false;
    if (isFriendName(trimmed)) return false;
    g_friends.push_back(trimmed);
    std::sort(g_friends.begin(), g_friends.end(), nameAlphaLess);
    return true;
}

bool removeFriend(const std::string& name) {
    const std::string lower = lowerCopy(name);
    for (auto it = g_friends.begin(); it != g_friends.end(); ++it) {
        if (lowerCopy(*it) == lower) { g_friends.erase(it); return true; }
    }
    return false;
}

// The players currently in the world, named the way a human reads them: cleaned of rank prefixes
// and colour codes, one row per player, and NPCs skipped while Anti-Bot is on so the friends page
// lists accounts rather than shop keepers.
std::vector<std::string> nearbyPlayerNames() {
    std::vector<std::string> out;
    if (!jni::ready() || !mc::frame::valid() || !mc::frame::resolved()) return out;
    jobject world = mc::Client::world();
    jobject self = mc::Client::player();
    JNIEnv* env = jni::env();
    if (!world || !env) return out;
    std::unordered_set<std::string> seen;
    for (jobject entity : mc::collectEntities(world, kEntityScan)) {
        if (!entity) continue;
        if (!isPlayerEntity(env, entity) || env->IsSameObject(entity, self) == JNI_TRUE) {
            env->DeleteLocalRef(entity);
            continue;
        }
        TargetInfo info;
        detectTarget(entity, true, info);
        env->DeleteLocalRef(entity);
        if (info.isBot || info.name.empty()) continue;
        if (!seen.insert(det::lowerCopy(info.name)).second) continue;
        out.push_back(info.name);
    }
    std::sort(out.begin(), out.end(), nameAlphaLess);
    return out;
}

void drawOverlay(float tickDelta) {
    ImDrawList* dl=ImGui::GetBackgroundDrawList();
    // Sampled EVERY frame, independent of Debug Mode: the periodic log line below is the evidence
    // trail for a visible twitch, so it has to exist even with the overlay switched off.
    if (JNIEnv* e=jni::env()) {
        jobject p=mc::Client::player();
        // End-of-frame state of the body rotation, read before this frame's module writes run: a
        // mismatch against `wrote` means something overwrote the pose after the pre-render write.
        if (p && mc::sym().liv_bodyYaw) {
            const float eof=e->GetFloatField(p, mc::sym().liv_bodyYaw);
            if (e->ExceptionCheck()) jni::clearException(e, "eof bodyYaw");
            else {
                g_dbgEofBody=eof;
                if (g_silentAimActive && std::abs(eof-g_silentBodyWrittenYaw)>1.f) ++g_dbgClobbers;
            }
        }
        // The game's own render-facing rotation read, straight from the horse's mouth: what the
        // model's rotation source currently reports. == wrote: our fields drive the model.
        // ~ cam instead: the model is drawn from different state and the writer must target that.
        if (p && mc::sym().liv_lerpYaw) {
            jvalue jv{}; jv.f = 0.5f;   // array form: no float-through-varargs ambiguity
            const float ry=e->CallFloatMethodA(p, mc::sym().liv_lerpYaw, &jv);
            if (e->ExceptionCheck()) jni::clearException(e, "lerpYaw read");
            else g_dbgRenderYaw=ry;
        }
        // Periodic state dump into passive_agent.log (%TEMP%\passive_agent.log).
        static int s_stateCountdown = 0;
        if (--s_stateCountdown <= 0) {
            s_stateCountdown = 45;   // ~every 1.5 s of gameplay
            float ly = -9999.f, lp = -9999.f;
            if (p && mc::sym().ent_lastYaw && mc::sym().ent_lastPitch) {
                ly = e->GetFloatField(p, mc::sym().ent_lastYaw);
                lp = e->GetFloatField(p, mc::sym().ent_lastPitch);
                if (e->ExceptionCheck()) { jni::clearException(e, "gameLastYaw read"); ly = lp = -9999.f; }
            }
            trace::log("silent state: active=%d draws %d wd %d fixes %d wrote %+.1f body %+.1f lastBody %+.1f render %+.1f eof %+.1f cam %+.1f aim %+.1f/%+.1f gameLastYaw %+.1f gameLastPitch %+.1f clobbers %d drops %d",
                       g_silentAimActive ? 1 : 0, (int)hooks::drawWrites(),
                       g_wdWrites, g_wdCorrections,
                       (double)g_silentBodyWrittenYaw, (double)g_dbgBody, (double)g_dbgLastBody,
                       (double)g_dbgRenderYaw, (double)g_dbgEofBody, (double)g_silentCameraYawNow,
                       (double)g_silentServerYaw, (double)g_silentServerPitch,
                       (double)ly, (double)lp, g_dbgClobbers, g_dbgReleases);
        }
    }
    // Debug Mode: the same numbers on screen, so the state can be read without the log.
    if (debugMode()) {
        char dbg[352];
        snprintf(dbg, sizeof(dbg),
                 "pre-render %s | wrote %+.0f -> body %+.0f lastBody %+.0f head %+.0f lastHead %+.0f | cam %+.0f | eof %+.0f render %+.0f clobbers %d drops %d",
                 hooks::preRenderCalls() ? "LIVE" : "DEAD",
                 (double)g_silentBodyWrittenYaw, (double)g_dbgBody, (double)g_dbgLastBody,
                 (double)g_dbgHead, (double)g_dbgLastHead, (double)g_silentCameraYawNow,
                 (double)g_dbgEofBody, (double)g_dbgRenderYaw, g_dbgClobbers, g_dbgReleases);
        dl->AddText(ImVec2(12.f, 140.f), IM_COL32(255, 92, 92, 255), dbg);
        // Which pick the game is using: the aim's own raycast when the transform installed, the
        // range-check fallback, or the camera's pick (only the field write then).
        const int pickMode = jni::silentAimPickMode();
        snprintf(dbg, sizeof(dbg), "pick %s | camera look: %s",
                 pickMode == 1 ? "aim (full)" : pickMode == 2 ? "aim (range)" : "camera",
                 g_cameraOnEntity ? "entity" : (g_cameraOnBlock ? "block" : "nothing"));
        dl->AddText(ImVec2(12.f, 172.f), IM_COL32(150, 210, 255, 255), dbg);
        // -9999 anywhere in that line means the field id never resolved.
        if (g_dbgLastBody < -999.f || g_dbgLastHead < -999.f ||
            std::abs(g_dbgBody - g_silentBodyWrittenYaw) > 1.f)
            dl->AddText(ImVec2(12.f, 156.f), IM_COL32(255, 200, 0, 255),
                        "FIELD PROBLEM: a rotation field did not resolve or the write did not stick");
    }
    g_attackSentThisFrame=false;
    // One player-detection pass for the whole frame: the identity caches and the local
    // player's own name/uuid are refreshed before any module asks who a target is.
    refreshPlayerDetection();
    // Motion Blur captures and composites first: its mixback has to be the bottom-most thing in the
    // overlay's background draw list, or it would smear the interface drawn over it.
    updateMotionBlur();
    // The shared Smooth Look Back walk runs before the modules: a module that arms it this frame
    // hands the view over on the next one, and no module ever steps the walk itself.
    {
        const bool ready=jni::ready() && mc::frame::valid() && mc::frame::resolved();
        jobject backPlayer=ready?mc::Client::player():nullptr;
        if (backPlayer) { mc::Player backView(backPlayer); stepLookBack(backView); }
    }
    // The Java module host loads on the first frame that can see a world, swaps the C++ cards it
    // ported, and then runs with the rest of the modules. It never blocks the native path: a client
    // without the Java payload just keeps the C++ modules.
    java::init();
    processModuleKeybinds();
    java::tick(tickDelta);
    updateAimAssist();
    updateSilentAimModule();
    updateTriggerbot();
    updateAutoMace();
    updateAutoJumpReset();
    updateSprint();
    updateFastPlace();
    updateAutoAnchor();
    updateAutoShieldBreak();
    updateStunWeb();
    updateAutoBucketDrain();
    updatePlateAssist();
    updatePressurePlateWeb();
    updateDragClick();
    updateShaderEspGlow();
    updateStaticFov();
    updateAutoTotem();
    updateAutoClean();
    updateInstantCartMacro();
    updatePearlCatch();
    updateCartRefill();
    updatePearlAgro();
    updateItemScroll();
    updateFriendJoinNotifications();
    updateLogoutSpots();
    updateWalkTrail();
    updateClarityCosmetics(tickDelta);
    drawLogoutSpots(dl,tickDelta);
    drawEsp(dl,tickDelta);
    drawShaderEsp(dl,tickDelta);
    drawWalkTrail(dl,tickDelta);
    drawNativeCosmetics(dl,tickDelta);
    drawHandGlow(dl,tickDelta);
    drawNativeViewModel(dl,tickDelta);
    drawNametags(dl,tickDelta);
    drawTrajectoryPreview(dl,tickDelta);
    drawHud(dl,tickDelta);
    drawArraylist(dl);
}

void resetDefaults() {
    java::disableAll();   // let Java modules release any key they hold before their state goes
    clearOwnedGlow();
    cancelAnchor(true);
    mc::LocalRef inventory(mc::Player(mc::Client::player()).inventory());
    clearShieldPending(inventory.get(),true);
    if (g_bucketWaiting) restoreTemporarySlot(inventory.get(),g_bucketOriginalSlot,true);
    if (g_webWaiting) restoreTemporarySlot(inventory.get(),g_webOriginalSlot,true);
    g_plateHave=false;
    if (g_lungePhase!=LungePhase::Idle) lungeFinish(inventory.get(),true);
    g_lungeOrigSlot=-1;
    g_lungeWant=false;
    if (g_sprintApplied) { mc::ext::setKey(mc::ext::Key::Sprint,false); g_sprintApplied=false; }
    if (g_jumpHeld) { mc::ext::setKey(mc::ext::Key::Jump,false); g_jumpHeld=false; }
    g_fastPlaceSettings=FastPlaceSettings{}; g_autoJumpResetSettings=AutoJumpResetSettings{};
    g_autoShieldBreak=false; g_autoBucketDrain=false; g_autoPressurePlateWeb=false;
    g_triggerbot=false; g_triggerbotKey=0; g_triggerbotSettings=TriggerbotSettings{};
    g_autoMace=false; g_autoMaceKey=0; g_autoMaceSettings=AutoMaceSettings{};
    g_macePhase=MacePhase::Idle; g_maceStageAt=0.0; g_maceNextAt=0.0; g_maceTarget.clear();
    g_maceOrigSlot=-1; g_maceChestSwapped=false; g_maceHitDelayMs=0;
    clickAttackRelease();
    g_triggerLastTarget.clear(); g_triggerLastSwingAt=0.0; g_triggerDelayMs=0; g_triggerDelayWeapon=-1;
    clickAttackRelease();   // never leave the Attack key held when the cards are reset
    g_moduleSwingSeq=0; g_shieldConsumedSwingSeq=0; g_moduleSwingAt=-1000.0;
    g_autoShieldBreakKey=0; g_autoBucketDrainKey=0; g_autoPressurePlateWebKey=0;
    g_antiBotSettings=AntiBotSettings{}; g_nametagsSettings=NametagsSettings{};
    g_autoTotemSettings=AutoTotemSettings{}; g_autoCleanSettings=AutoCleanSettings{};
    g_friends.clear();
    resetPlayerDetection();
    g_totemNextAt=0.0; g_cleanNextAt=0.0; g_ghostNextAt=0.0;
    g_espSettings=EspSettings{};
    g_autoAnchorSettings=AutoAnchorSettings{};
    g_autoShieldBreakSettings=AutoShieldBreakSettings{};
    g_autoBucketDrainSettings=AutoBucketDrainSettings{};
    g_autoPressurePlateWebSettings=AutoPressurePlateWebSettings{};
    g_bucketWaiting=false; g_webWaiting=false; g_shieldWaiting=false;
    g_bucketTarget={}; g_webTarget={}; g_ownWaterPositions.clear();
    g_bucketUseOffhand=g_webUseOffhand=false; g_prevMainWater=g_prevOffWater=false;
    g_ownWaterScanUntil=0.0;
    g_bucketSlot=g_bucketOriginalSlot=g_webSlot=g_webOriginalSlot=-1;
    g_shieldTargetUuid.clear(); g_shieldOriginalSlot=g_shieldAxeSlot=-1;

    // ---- round 6 ----
    g_trajPreview=g_itemScroll=false;
    g_instantCartSettings=InstantCartMacroSettings{};
    g_cobwebActive=false; g_cobwebOrigSlot=-1; g_cobwebNextAt=0.0;
    g_xbowStep=XbowStep::Idle; g_xbowOrigSlot=-1; g_xbowRailValid=false; g_xbowNextAt=0.0;
    g_xbowSwapAt=-1.0; g_xbowFirePending=false; g_xbowFireRetried=false; g_xbowFireY=0;
    instantCartReset(); g_cartOrigSlot=-1;

    // ---- round 7 ----

    // ---- round 8 ----
    g_shaderEsp=g_dragClick=false;
    g_shaderEspKey=g_dragClickKey=0;
    g_shaderEspSettings=ShaderEspSettings{}; g_dragClickSettings=DragClickSettings{};

    // ---- round 9 ----
    g_stunWeb=g_logoutSpots=false;
    g_stunWebKey=g_logoutSpotsKey=0;
    g_stunWebSettings=StunWebSettings{}; g_logoutSpotsSettings=LogoutSpotsSettings{};
    stunWebReset(nullptr,false);
    g_stunWebPendingTarget.clear();

    // ---- Triggerbot V2 / AutoWeb ----
    headWebReset(nullptr,false);
    g_headWebNextAt=0.0;

    // ---- round 10 ----
    g_lastSeenPlayers.clear(); g_logoutSpotList.clear();

    // ---- round 12: Motion Blur / Auto Shield ----
    motionblur::release(); g_blurHavePrev=false; g_blurPrevYaw=g_blurPrevPitch=0.0;
    g_cartUseReleaseAt=-1.0;
    // Never leave the game's Attack key held by a V2 click that was in flight at unload.

    // ---- round 11 (Spear) ----
    g_instantSpearHeld=false;

    // ---- Clarity cosmetics ----
    g_playerModel=g_wings=g_headwear=g_capes=g_viewModel=false;
    g_playerModelKey=g_wingsKey=g_headwearKey=g_capesKey=g_viewModelKey=0;
    g_playerModelSettings=PlayerModelSettings{}; g_wingsSettings=WingsSettings{};
    g_headwearSettings=HeadwearSettings{}; g_capesSettings=CapesSettings{};
    g_viewModelSettings=ViewModelSettings{};

    // ---- Walk Trail ----
    g_walkTrail=false; g_walkTrailKey=0;
    g_walkTrailSettings=WalkTrailSettings{};
    g_trailPoints.clear(); g_trailNextSampleAt=0.0;

    // ---- HUD ----
    g_hud=false; g_hudKey=0;
    g_hudSettings=HudSettings{};
    g_hudFade=0.f; g_hudPrevSelected=-1; g_hudItemNameUntil=0.0; g_hudItemNameId.clear();
    for (int i=0;i<9;++i) g_hudSlotPop[i]=0.f;
    for (int i=0;i<4;++i) g_hudBarShown[i]=-1.f;
    g_hudHeartPulse=0.0;

    g_dragHeld=false; g_dragButtonDown=false;
    g_dragHeldAt=0.0; g_dragReleaseAt=0.0;
    g_dragTimer.reset();
    g_cartRefillNextAt=0.0;
    g_potNextAt=0.0; g_potEverUsed=false;
    g_potLookDownAt=-1.0;     // no look-down is in flight
    g_potAimArmed=false; g_potHaveBack=false; g_potAimYaw=0.f;   // and no throw aim is armed
    g_potMoveTarget=-1; g_potMoveSource=-1; g_potMoveAt=0.0;
    cancelLookBack();         // and no shared Look Back walk is either
    g_pearlAgroArmed=false; g_pearlAgroTargetUuid.clear(); g_pearlAgroReadyAt=0.0; g_pearlAgroLandingAt=0.0;
    g_silentAimActive=false;
    jni::silentRotationStop();
    g_actionHoldUntil=-1.0; g_actionLogged=false; g_cameraOnBlock=false; g_cameraOnEntity=false;
    mc::ext::setKey(mc::ext::Key::Use,false);
    // Close an inventory we opened for the visible inventory modules, through the game's own API.
    if (g_cartScreen.phase==ScreenPhase::Working || g_potScreen.phase==ScreenPhase::Working ||
        g_totemScreen.phase==ScreenPhase::Working || g_cleanScreen.phase==ScreenPhase::Working)
        mc::Client::closeScreen();
    screenPhaseReset(g_cartScreen);
    screenPhaseReset(g_potScreen);
    g_scrollNextAt=0.0;
    g_handledScreenCache.clear();
    g_seenPlayers.clear(); g_seenPlayersSeeded=false;
    // Put the field of view back before the module state is cleared.
    screenPhaseReset(g_totemScreen);
    screenPhaseReset(g_cleanScreen);
    if (g_fovApplied && jni::ready()) {
        jobject options=mc::Client::options();
        if (options && mc::sym().opt_fov && g_fovBeforeStatic>=0)
            mc::opts::setIntOptionRaw(options, mc::sym().opt_fov, g_fovBeforeStatic);
    }
    g_fovApplied=false; g_fovAppliedValue=-1; g_fovBeforeStatic=-1;
}

std::string serialize() {
    nlohmann::json j;
    for (const Entry& entry : g_entries) {
        if (entry.macro) continue;
        j["enabled"][entry.name] = entry.enabled && *entry.enabled;
    }
    for (const Entry& entry : g_entries)
        if (entry.keybind) j["keybinds"][entry.name] = *entry.keybind;
    j["esp"] = {{"mode",(int)g_espSettings.mode},{"onlyOutline",g_espSettings.onlyOutline},
        {"allowEntities",g_espSettings.allowEntities},{"allowPlayers",g_espSettings.allowPlayers},
        {"highlightFriends",g_espSettings.highlightFriends},{"showHealth3d",g_espSettings.showHealth3d},
        {"color",{g_espSettings.color.x,g_espSettings.color.y,g_espSettings.color.z,g_espSettings.color.w}},
        {"friendColor",{g_espSettings.friendColor.x,g_espSettings.friendColor.y,g_espSettings.friendColor.z,g_espSettings.friendColor.w}}};
    j["autoAnchor"] = {{"totemSlot",g_autoAnchorSettings.totemSlot},
        {"glowstoneDelayMs",g_autoAnchorSettings.glowstoneDelayMs},
        {"totemDelayMs",g_autoAnchorSettings.totemDelayMs}};
    j["autoShieldBreak"]={{"onlyWhenLooking",g_autoShieldBreakSettings.onlyWhenLooking},
        {"mode",(int)g_autoShieldBreakSettings.mode},{"swapBack",g_autoShieldBreakSettings.swapBack},
        {"swordOnly",g_autoShieldBreakSettings.swordOnly},{"swapDelayMs",g_autoShieldBreakSettings.swapDelayMs},
        {"retryDelayMs",g_autoShieldBreakSettings.retryDelayMs}};
    j["autoBucketDrain"]={{"mode",(int)g_autoBucketDrainSettings.mode},
        {"ignoreOwnWater",g_autoBucketDrainSettings.ignoreOwnWater},
        {"swapBack",g_autoBucketDrainSettings.swapBack},{"swapDelayMs",g_autoBucketDrainSettings.swapDelayMs},
        {"silentSwap",g_autoBucketDrainSettings.silentSwap},{"useOffhand",g_autoBucketDrainSettings.useOffhand},
        {"webPreferBuckets",g_autoBucketDrainSettings.webPreferBuckets},
        {"webIgnoreOwnWater",g_autoBucketDrainSettings.webIgnoreOwnWater},
        {"webSwapBack",g_autoBucketDrainSettings.webSwapBack},
        {"webUseOffhand",g_autoBucketDrainSettings.webUseOffhand},
        {"webSwapDelayMs",g_autoBucketDrainSettings.webSwapDelayMs},
        {"webSilentSwap",g_autoBucketDrainSettings.webSilentSwap},
        {"webSilentPlace",g_autoBucketDrainSettings.webSilentPlace}};
    j["autoPressurePlateWeb"]={{"swapBack",g_autoPressurePlateWebSettings.swapBack},
        {"swapDelayMs",g_autoPressurePlateWebSettings.swapDelayMs},
        {"silentSwap",g_autoPressurePlateWebSettings.silentSwap},
        {"useOffhand",g_autoPressurePlateWebSettings.useOffhand},
        {"plateAssist",g_autoPressurePlateWebSettings.plateAssist},
        {"assistRange",g_autoPressurePlateWebSettings.assistRange},
        {"assistFov",g_autoPressurePlateWebSettings.assistFov},
        {"assistSpeed",g_autoPressurePlateWebSettings.assistSpeed},
        {"assistVertical",g_autoPressurePlateWebSettings.assistVertical},
        {"onlyIfNoWeb",g_autoPressurePlateWebSettings.onlyIfNoWeb}};
    j["fastPlace"]={{"onlyXp",g_fastPlaceSettings.onlyXp},{"allowBlocks",g_fastPlaceSettings.allowBlocks},
        {"allowItems",g_fastPlaceSettings.allowItems}};
    j["autoJumpReset"]={{"accuracy",g_autoJumpResetSettings.accuracy}};
    j["triggerbot"]={{"allItems",g_triggerbotSettings.allItems},
        {"swordDelayMinMs",g_triggerbotSettings.swordDelayMinMs},
        {"swordDelayMaxMs",g_triggerbotSettings.swordDelayMaxMs},
        {"axeDelayMinMs",g_triggerbotSettings.axeDelayMinMs},
        {"axeDelayMaxMs",g_triggerbotSettings.axeDelayMaxMs},
        {"checkShield",g_triggerbotSettings.checkShield},
        {"strayBypass",g_triggerbotSettings.strayBypass},
        {"allEntities",g_triggerbotSettings.allEntities},
        {"samePlayer",g_triggerbotSettings.samePlayer},
        {"criticals",(int)g_triggerbotSettings.criticals}};
    j["autoMace"]={{"stunSlam",g_autoMaceSettings.stunSlam},
        {"elytraSwap",g_autoMaceSettings.elytraSwap},
        {"swapBack",g_autoMaceSettings.swapBack},{"noGlide",g_autoMaceSettings.noGlide},{"range",g_autoMaceSettings.range},
        {"minFallHeight",g_autoMaceSettings.minFallHeight},
        {"hitDelayMinMs",g_autoMaceSettings.hitDelayMinMs},
        {"hitDelayMaxMs",g_autoMaceSettings.hitDelayMaxMs},
        {"cooldownMinMs",g_autoMaceSettings.cooldownMinMs},
        {"cooldownMaxMs",g_autoMaceSettings.cooldownMaxMs},
        {"reswapMinMs",g_autoMaceSettings.reswapMinMs},
        {"reswapMaxMs",g_autoMaceSettings.reswapMaxMs},
        {"axeDelayMinMs",g_autoMaceSettings.axeDelayMinMs},
        {"axeDelayMaxMs",g_autoMaceSettings.axeDelayMaxMs},
        {"swapDelayMs",g_autoMaceSettings.swapDelayMs}};
    j["aimAssist"]={{"allowPlayers",g_aimAssistSettings.allowPlayers},{"allowEntities",g_aimAssistSettings.allowEntities},
        {"range",g_aimAssistSettings.range},{"fov",g_aimAssistSettings.fov},{"speed",g_aimAssistSettings.speed},
        {"vertical",g_aimAssistSettings.vertical},{"onlyWhileAttacking",g_aimAssistSettings.onlyWhileAttacking},
        {"weaponOnly",g_aimAssistSettings.weaponOnly},
        {"silent",g_aimAssistSettings.silent},{"accuracy",(int)g_aimAssistSettings.accuracy},
        {"noGlide",g_aimAssistSettings.noGlide}};
    j["silentAimModule"]={{"allowPlayers",g_silentAimModuleSettings.allowPlayers},
        {"allowEntities",g_silentAimModuleSettings.allowEntities},
        {"range",g_silentAimModuleSettings.range},{"fov",g_silentAimModuleSettings.fov},
        {"speed",g_silentAimModuleSettings.speed},{"smooth",g_silentAimModuleSettings.smooth},
        {"vertical",g_silentAimModuleSettings.vertical},
        {"onlyWhileAttacking",g_silentAimModuleSettings.onlyWhileAttacking},
        {"accuracy",(int)g_silentAimModuleSettings.accuracy},
        {"noGlide",g_silentAimModuleSettings.noGlide}};
    j["antiBot"]={{"checkNames",g_antiBotSettings.checkNames},{"checkUuid",g_antiBotSettings.checkUuid},
        {"allowOfflineUuid",g_antiBotSettings.allowOfflineUuid},{"checkClones",g_antiBotSettings.checkClones}};
    j["nametags"]={{"showName",g_nametagsSettings.showName},{"showHealth",g_nametagsSettings.showHealth},
        {"icons",g_nametagsSettings.icons},
        {"showDistance",g_nametagsSettings.showDistance},{"showArmor",g_nametagsSettings.showArmor},
        {"showHeldItem",g_nametagsSettings.showHeldItem},{"singleShow",g_nametagsSettings.singleShow},
        {"maxDistance",g_nametagsSettings.maxDistance},
        {"scale",g_nametagsSettings.scale}};
    j["staticFov"]={{"fov",g_staticFovSettings.fov}};
    j["autoTotem"]={{"type",(int)g_autoTotemSettings.type},
        {"refillOffhand",g_autoTotemSettings.refillOffhand},
        {"refillHotbarSlot",g_autoTotemSettings.refillHotbarSlot},{"hotbarSlot",g_autoTotemSettings.hotbarSlot},
        {"delayMs",g_autoTotemSettings.delayMs},{"silent",g_autoTotemSettings.silent},
        {"smpHealthHearts",g_autoTotemSettings.smpHealthHearts},
        {"smpSwapDelayMs",g_autoTotemSettings.smpSwapDelayMs},
        {"smpShieldSlot",g_autoTotemSettings.smpShieldSlot}};
    j["autoClean"]={{"silentClean",g_autoCleanSettings.silentClean},{"scanDelayMs",g_autoCleanSettings.scanDelayMs},
        {"items",g_autoCleanSettings.customItems}};
    nlohmann::json cartSlots=nlohmann::json::array();
    for (int i=0;i<9;++i) cartSlots.push_back(g_cartRefillSettings.slots[i]);
    j["cartRefill"]={{"slots",cartSlots},{"delayMs",g_cartRefillSettings.delayMs},{"silent",g_cartRefillSettings.silent}};
    j["instantCartMacro"]={{"bowChargeMs",g_instantCartSettings.bowChargeMs},
        {"railDelayMs",g_instantCartSettings.railDelayMs},
        {"cartDelayMs",g_instantCartSettings.cartDelayMs},
        {"swapBack",g_instantCartSettings.swapBack}};
    j["pearlCatch"]={{"pearlDelayMs",g_pearlCatchSettings.pearlDelayMs},
        {"windChargeDelayMs",g_pearlCatchSettings.windChargeDelayMs},
        {"swapBack",g_pearlCatchSettings.swapBack}};
    j["pearlAgro"]={{"agroDelayMs",g_pearlAgroSettings.agroDelayMs},{"fov",g_pearlAgroSettings.fov},
        {"silentAim",g_pearlAgroSettings.silentAim},
        {"look",(int)g_pearlAgroSettings.look},{"cameraSensitivity",g_pearlAgroSettings.cameraSensitivity}};
    // The Custom pot-slot table is written flat (6 potions x 9 slots) because the JSON helper only
    // carries one-dimensional bool vectors.
    j["shaderEsp"]={{"style",(int)g_shaderEspSettings.style},{"handGlow",g_shaderEspSettings.handGlow},
        {"handSize",g_shaderEspSettings.handSize},{"handOffsetX",g_shaderEspSettings.handOffsetX},
        {"handOffsetY",g_shaderEspSettings.handOffsetY},{"handRotation",g_shaderEspSettings.handRotation},
        {"thickness",g_shaderEspSettings.thickness},{"opacity",g_shaderEspSettings.opacity},
        {"maxDistance",g_shaderEspSettings.maxDistance},
        {"color",{g_shaderEspSettings.color.x,g_shaderEspSettings.color.y,g_shaderEspSettings.color.z,g_shaderEspSettings.color.w}},
        {"friendColor",{g_shaderEspSettings.friendColor.x,g_shaderEspSettings.friendColor.y,g_shaderEspSettings.friendColor.z,g_shaderEspSettings.friendColor.w}}};
    j["dragClick"]={{"minCps",g_dragClickSettings.minCps},{"maxCps",g_dragClickSettings.maxCps},
        {"acceleration",g_dragClickSettings.acceleration},{"pauseSeconds",g_dragClickSettings.pauseSeconds}};
    j["playerModel"]={{"scale",g_playerModelSettings.scale},{"throughWalls",g_playerModelSettings.throughWalls},
        {"showTiming",g_playerModelSettings.showTiming}};
    j["wings"]={{"style",(int)g_wingsSettings.style},{"scale",g_wingsSettings.scale},
        {"opacity",g_wingsSettings.opacity},{"animate",g_wingsSettings.animate},
        {"flapSpeed",g_wingsSettings.flapSpeed},{"yOffset",g_wingsSettings.yOffset},
        {"throughWalls",g_wingsSettings.throughWalls}};
    j["headwear"]={{"style",(int)g_headwearSettings.style},{"scale",g_headwearSettings.scale},
        {"yOffset",g_headwearSettings.yOffset},{"rotationSpeed",g_headwearSettings.rotationSpeed},
        {"throughWalls",g_headwearSettings.throughWalls}};
    j["capes"]={{"style",(int)g_capesSettings.style},{"physics",g_capesSettings.physics},
        {"movementWave",g_capesSettings.movementWave}};
    j["viewModel"]={{"posX",g_viewModelSettings.posX},{"posY",g_viewModelSettings.posY},
        {"posZ",g_viewModelSettings.posZ},{"rotX",g_viewModelSettings.rotX},
        {"rotY",g_viewModelSettings.rotY},{"rotZ",g_viewModelSettings.rotZ},
        {"scale",g_viewModelSettings.scale},{"offHand",g_viewModelSettings.offHand},
        {"opacity",g_viewModelSettings.opacity}};
    j["stunWeb"]={{"look",(int)g_stunWebSettings.look},{"allowPlayers",g_stunWebSettings.allowPlayers},
        {"cameraSensitivity",g_stunWebSettings.cameraSensitivity},{"silentPlace",g_stunWebSettings.silentPlace},
        {"lookBack",g_stunWebSettings.lookBack},{"placeDelayMs",g_stunWebSettings.placeDelayMs}};
    j["walkTrail"]={{"color",{g_walkTrailSettings.color.x,g_walkTrailSettings.color.y,g_walkTrailSettings.color.z,g_walkTrailSettings.color.w}},
        {"lifetime",g_walkTrailSettings.lifetime},{"width",g_walkTrailSettings.width},
        {"maxPoints",g_walkTrailSettings.maxPoints},{"fade",g_walkTrailSettings.fade},
        {"animate",g_walkTrailSettings.animate},{"throughWalls",g_walkTrailSettings.throughWalls}};
    j["logoutSpots"]={{"maxDistance",g_logoutSpotsSettings.maxDistance},
        {"onlyPlayers",g_logoutSpotsSettings.onlyPlayers},
        {"color",{g_logoutSpotsSettings.color.x,g_logoutSpotsSettings.color.y,g_logoutSpotsSettings.color.z,g_logoutSpotsSettings.color.w}}};
    j["motionBlur"]={{"enabled",g_motionBlurSettings.enabled},{"strength",g_motionBlurSettings.strength},
        {"method",(int)g_motionBlurSettings.method},
        {"refreshScaling",g_motionBlurSettings.refreshScaling},
        {"accumulationMax",g_motionBlurSettings.accumulationMax},
        {"accumulationMix",g_motionBlurSettings.accumulationMix},
        {"profile",(int)g_motionBlurSettings.profile},
        {"sensitivity",g_motionBlurSettings.sensitivity}};
    nlohmann::json hudPixels=nlohmann::json::array();
    for (int i=0;i<81;++i) hudPixels.push_back(g_hudSettings.drawBoardPixels[i]);
    j["hud"]={{"customHotbar",g_hudSettings.customHotbar},
        {"hotbarAnimations",g_hudSettings.hotbarAnimations},{"hotbarRounded",g_hudSettings.hotbarRounded},
        {"hotbarNumbers",g_hudSettings.hotbarNumbers},{"hotbarItemCount",g_hudSettings.hotbarItemCount},
        {"itemNamePopup",g_hudSettings.itemNamePopup},{"panels",g_hudSettings.panels},
        {"attackSpeedBar",g_hudSettings.attackSpeedBar},{"customCrosshair",g_hudSettings.customCrosshair},
        {"coverVanilla",g_hudSettings.coverVanilla},{"platePadding",g_hudSettings.platePadding},
        {"plateOpacity",g_hudSettings.plateOpacity},
        {"plateColor",{g_hudSettings.plateColor.x,g_hudSettings.plateColor.y,g_hudSettings.plateColor.z,g_hudSettings.plateColor.w}},
        {"barAnimations",g_hudSettings.barAnimations},{"barAnimSpeed",g_hudSettings.barAnimSpeed},
        {"heartBeat",g_hudSettings.heartBeat},{"absorbHealth",g_hudSettings.absorbHealth},
        {"absorbColor",{g_hudSettings.absorbColor.x,g_hudSettings.absorbColor.y,g_hudSettings.absorbColor.z,g_hudSettings.absorbColor.w}},
        {"crosshairIndicator",g_hudSettings.crosshairIndicator},
        {"crosshairIndicatorRange",g_hudSettings.crosshairIndicatorRange},
        {"crosshairIndicatorColor",{g_hudSettings.crosshairIndicatorColor.x,g_hudSettings.crosshairIndicatorColor.y,g_hudSettings.crosshairIndicatorColor.z,g_hudSettings.crosshairIndicatorColor.w}},
        {"hotbarBackground",{g_hudSettings.hotbarBackground.x,g_hudSettings.hotbarBackground.y,g_hudSettings.hotbarBackground.z,g_hudSettings.hotbarBackground.w}},
        {"slotBackground",{g_hudSettings.slotBackground.x,g_hudSettings.slotBackground.y,g_hudSettings.slotBackground.z,g_hudSettings.slotBackground.w}},
        {"hotbarAccent",{g_hudSettings.hotbarAccent.x,g_hudSettings.hotbarAccent.y,g_hudSettings.hotbarAccent.z,g_hudSettings.hotbarAccent.w}},
        {"hotbarScale",g_hudSettings.hotbarScale},{"hotbarRounding",g_hudSettings.hotbarRounding},
        {"hotbarAnimSpeed",g_hudSettings.hotbarAnimSpeed},
        {"panelBackground",{g_hudSettings.panelBackground.x,g_hudSettings.panelBackground.y,g_hudSettings.panelBackground.z,g_hudSettings.panelBackground.w}},
        {"healthColor",{g_hudSettings.healthColor.x,g_hudSettings.healthColor.y,g_hudSettings.healthColor.z,g_hudSettings.healthColor.w}},
        {"armorColor",{g_hudSettings.armorColor.x,g_hudSettings.armorColor.y,g_hudSettings.armorColor.z,g_hudSettings.armorColor.w}},
        {"foodColor",{g_hudSettings.foodColor.x,g_hudSettings.foodColor.y,g_hudSettings.foodColor.z,g_hudSettings.foodColor.w}},
        {"xpColor",{g_hudSettings.xpColor.x,g_hudSettings.xpColor.y,g_hudSettings.xpColor.z,g_hudSettings.xpColor.w}},
        {"panelScale",g_hudSettings.panelScale},{"panelRounding",g_hudSettings.panelRounding},
        {"panelIcons",g_hudSettings.panelIcons},{"panelNumbers",g_hudSettings.panelNumbers},
        {"panelOffsetY",g_hudSettings.panelOffsetY},
        {"itemNameBackground",{g_hudSettings.itemNameBackground.x,g_hudSettings.itemNameBackground.y,g_hudSettings.itemNameBackground.z,g_hudSettings.itemNameBackground.w}},
        {"itemNameText",{g_hudSettings.itemNameText.x,g_hudSettings.itemNameText.y,g_hudSettings.itemNameText.z,g_hudSettings.itemNameText.w}},
        {"itemNameScale",g_hudSettings.itemNameScale},{"itemNameHoldMs",g_hudSettings.itemNameHoldMs},
        {"itemNameIcons",g_hudSettings.itemNameIcons},
        {"attackSpeedColor",{g_hudSettings.attackSpeedColor.x,g_hudSettings.attackSpeedColor.y,g_hudSettings.attackSpeedColor.z,g_hudSettings.attackSpeedColor.w}},
        {"attackSpeedScale",g_hudSettings.attackSpeedScale},
        {"crosshairStyle",(int)g_hudSettings.crosshairStyle},
        {"crosshairColor",{g_hudSettings.crosshairColor.x,g_hudSettings.crosshairColor.y,g_hudSettings.crosshairColor.z,g_hudSettings.crosshairColor.w}},
        {"crosshairOutline",g_hudSettings.crosshairOutline},{"crosshairDynamic",g_hudSettings.crosshairDynamic},
        {"crosshairSize",g_hudSettings.crosshairSize},{"crosshairGap",g_hudSettings.crosshairGap},
        {"crosshairThickness",g_hudSettings.crosshairThickness},{"crosshairDot",g_hudSettings.crosshairDot},
        {"crosshairRotation",g_hudSettings.crosshairRotation},
        {"drawBoardPixels",hudPixels},{"drawBoardPixel",g_hudSettings.drawBoardPixel}};
    j["javaModules"]=java::settingsJson();
    j["debug"]=g_debugMode;
    j["notifications"]=g_notifications;
    j["friends"]=g_friends;
    return j.dump(2);
}

bool deserialize(const std::string& jsonText) {
    try {
        const auto j=nlohmann::json::parse(jsonText);
        if (!j.is_object()) return false;
        const auto enabled=j.value("enabled",nlohmann::json::object());
        for (const Entry& entry : g_entries)
            if (entry.enabled && !entry.macro) *entry.enabled=enabled.value(entry.name,false);
        java::loadSettings(j.value("javaModules",nlohmann::json::object()));
        const auto keybinds=j.value("keybinds",nlohmann::json::object());
        for (const Entry& entry : g_entries)
            if (entry.keybind) *entry.keybind=std::clamp(keybinds.value(entry.name,0),0,255);
        const auto e=j.value("esp",nlohmann::json::object());
        g_espSettings.mode=e.value("mode",0)==1?EspMode::ThreeD:EspMode::TwoD;
        g_espSettings.onlyOutline=e.value("onlyOutline",false);
        g_espSettings.allowEntities=e.value("allowEntities",true);
        g_espSettings.allowPlayers=e.value("allowPlayers",true);
        g_espSettings.highlightFriends=e.value("highlightFriends",true);
        g_espSettings.showHealth3d=e.value("showHealth3d",true);
        const auto c=e.value("color",nlohmann::json::array({0.94,0.38,0.30,1.0}));
        if (c.is_array() && c.size()>=4) g_espSettings.color=ImVec4(std::clamp(c[0].get<float>(),0.f,1.f),std::clamp(c[1].get<float>(),0.f,1.f),std::clamp(c[2].get<float>(),0.f,1.f),std::clamp(c[3].get<float>(),0.f,1.f));
        const auto fc=e.value("friendColor",nlohmann::json::array({0.36,0.76,1.0,1.0}));
        if (fc.is_array() && fc.size()>=4) g_espSettings.friendColor=ImVec4(std::clamp(fc[0].get<float>(),0.f,1.f),std::clamp(fc[1].get<float>(),0.f,1.f),std::clamp(fc[2].get<float>(),0.f,1.f),std::clamp(fc[3].get<float>(),0.f,1.f));
        const auto a=j.value("autoAnchor",nlohmann::json::object());
        g_autoAnchorSettings.totemSlot=std::clamp(a.value("totemSlot",1),1,9);
        g_autoAnchorSettings.glowstoneDelayMs=std::clamp(a.value("glowstoneDelayMs",100),0,600);
        g_autoAnchorSettings.totemDelayMs=std::clamp(a.value("totemDelayMs",100),0,600);
        const auto s=j.value("autoShieldBreak",nlohmann::json::object());
        g_autoShieldBreakSettings.onlyWhenLooking=s.value("onlyWhenLooking",true);
        g_autoShieldBreakSettings.mode=s.value("mode",0)==1?ShieldBreakMode::OnHit:ShieldBreakMode::Auto;
        g_autoShieldBreakSettings.swapBack=s.value("swapBack",true);
        g_autoShieldBreakSettings.swordOnly=s.value("swordOnly",false);
        g_autoShieldBreakSettings.swapDelayMs=std::clamp(s.value("swapDelayMs",70),1,600);
        g_autoShieldBreakSettings.retryDelayMs=std::clamp(s.value("retryDelayMs",300),1,600);
        const auto tb=j.value("triggerbot",nlohmann::json::object());
        g_triggerbotSettings.allItems=tb.value("allItems",false);
        g_triggerbotSettings.swordDelayMinMs=std::clamp(tb.value("swordDelayMinMs",540),0,1000);
        g_triggerbotSettings.swordDelayMaxMs=std::clamp(tb.value("swordDelayMaxMs",550),0,1000);
        g_triggerbotSettings.axeDelayMinMs=std::clamp(tb.value("axeDelayMinMs",780),0,1000);
        g_triggerbotSettings.axeDelayMaxMs=std::clamp(tb.value("axeDelayMaxMs",800),0,1000);
        g_triggerbotSettings.checkShield=tb.value("checkShield",false);
        g_triggerbotSettings.strayBypass=tb.value("strayBypass",false);
        g_triggerbotSettings.allEntities=tb.value("allEntities",false);
        g_triggerbotSettings.samePlayer=tb.value("samePlayer",false);
        g_triggerbotSettings.criticals=(CritMode)std::clamp(tb.value("criticals",0),0,2);
        const auto am=j.value("autoMace",nlohmann::json::object());
        g_autoMaceSettings.stunSlam=am.value("stunSlam",false);
        g_autoMaceSettings.elytraSwap=am.value("elytraSwap",true);
        g_autoMaceSettings.swapBack=am.value("swapBack",true);
        g_autoMaceSettings.noGlide=am.value("noGlide",false);
        g_autoMaceSettings.range=std::clamp(am.value("range",3.f),1.f,6.f);
        g_autoMaceSettings.minFallHeight=std::clamp(am.value("minFallHeight",2.5f),0.f,20.f);
        g_autoMaceSettings.hitDelayMinMs=std::clamp(am.value("hitDelayMinMs",0),0,500);
        g_autoMaceSettings.hitDelayMaxMs=std::clamp(am.value("hitDelayMaxMs",60),0,500);
        g_autoMaceSettings.cooldownMinMs=std::clamp(am.value("cooldownMinMs",120),0,1000);
        g_autoMaceSettings.cooldownMaxMs=std::clamp(am.value("cooldownMaxMs",260),0,1000);
        g_autoMaceSettings.reswapMinMs=std::clamp(am.value("reswapMinMs",80),0,1000);
        g_autoMaceSettings.reswapMaxMs=std::clamp(am.value("reswapMaxMs",180),0,1000);
        g_autoMaceSettings.axeDelayMinMs=std::clamp(am.value("axeDelayMinMs",100),0,500);
        g_autoMaceSettings.axeDelayMaxMs=std::clamp(am.value("axeDelayMaxMs",200),0,500);
        g_autoMaceSettings.swapDelayMs=std::clamp(am.value("swapDelayMs",50),0,300);
        const auto b=j.value("autoBucketDrain",nlohmann::json::object());
        g_autoBucketDrainSettings.ignoreOwnWater=b.value("ignoreOwnWater",true);
        g_autoBucketDrainSettings.swapBack=b.value("swapBack",true);
        g_autoBucketDrainSettings.swapDelayMs=std::clamp(b.value("swapDelayMs",70),0,600);
        g_autoBucketDrainSettings.silentSwap=b.value("silentSwap",false);
        g_autoBucketDrainSettings.useOffhand=b.value("useOffhand",true);
        g_autoBucketDrainSettings.mode=b.value("mode",0)==1?DrainMode::Web:DrainMode::Bucket;
        g_autoBucketDrainSettings.webPreferBuckets=b.value("webPreferBuckets",true);
        g_autoBucketDrainSettings.webIgnoreOwnWater=b.value("webIgnoreOwnWater",true);
        g_autoBucketDrainSettings.webSwapBack=b.value("webSwapBack",true);
        g_autoBucketDrainSettings.webUseOffhand=b.value("webUseOffhand",true);
        g_autoBucketDrainSettings.webSwapDelayMs=std::clamp(b.value("webSwapDelayMs",70),0,600);
        g_autoBucketDrainSettings.webSilentSwap=b.value("webSilentSwap",true);
        g_autoBucketDrainSettings.webSilentPlace=b.value("webSilentPlace",false);
        const auto w=j.value("autoPressurePlateWeb",nlohmann::json::object());
        g_autoPressurePlateWebSettings.swapBack=w.value("swapBack",true);
        g_autoPressurePlateWebSettings.swapDelayMs=std::clamp(w.value("swapDelayMs",0),0,600);
        g_autoPressurePlateWebSettings.silentSwap=w.value("silentSwap",false);
        g_autoPressurePlateWebSettings.useOffhand=w.value("useOffhand",true);
        g_autoPressurePlateWebSettings.plateAssist=w.value("plateAssist",false);
        g_autoPressurePlateWebSettings.assistRange=std::clamp(w.value("assistRange",4.5f),1.f,6.f);
        g_autoPressurePlateWebSettings.assistFov=std::clamp(w.value("assistFov",120.f),10.f,360.f);
        g_autoPressurePlateWebSettings.assistSpeed=std::clamp(w.value("assistSpeed",8.f),1.f,30.f);
        g_autoPressurePlateWebSettings.assistVertical=w.value("assistVertical",true);
        g_autoPressurePlateWebSettings.onlyIfNoWeb=w.value("onlyIfNoWeb",false);
        const auto fp=j.value("fastPlace",nlohmann::json::object());
        g_fastPlaceSettings.onlyXp=fp.value("onlyXp",false);
        g_fastPlaceSettings.allowBlocks=fp.value("allowBlocks",true);
        g_fastPlaceSettings.allowItems=fp.value("allowItems",true);
        g_autoJumpResetSettings.accuracy=std::clamp(j.value("autoJumpReset",nlohmann::json::object()).value("accuracy",100),0,100);
        const auto aa=j.value("aimAssist",nlohmann::json::object());
        g_aimAssistSettings.allowPlayers=aa.value("allowPlayers",true);
        g_aimAssistSettings.allowEntities=aa.value("allowEntities",false);
        g_aimAssistSettings.range=std::clamp(aa.value("range",4.5f),1.f,8.f);
        g_aimAssistSettings.fov=std::clamp(aa.value("fov",90.f),10.f,360.f);
        g_aimAssistSettings.speed=std::clamp(aa.value("speed",8.f),1.f,40.f);
        g_aimAssistSettings.vertical=aa.value("vertical",true);
        g_aimAssistSettings.onlyWhileAttacking=aa.value("onlyWhileAttacking",true);
        g_aimAssistSettings.weaponOnly=aa.value("weaponOnly",false);
        g_aimAssistSettings.silent=aa.value("silent",false);
        g_aimAssistSettings.accuracy=aa.value("accuracy",0)==1?AimAccuracy::Head:
            (aa.value("accuracy",0)==2?AimAccuracy::Loose:AimAccuracy::Center);
        g_aimAssistSettings.noGlide=aa.value("noGlide",false);
        const auto sa=j.value("silentAimModule",nlohmann::json::object());
        g_silentAimModuleSettings.allowPlayers=sa.value("allowPlayers",true);
        g_silentAimModuleSettings.allowEntities=sa.value("allowEntities",true);
        g_silentAimModuleSettings.range=std::clamp(sa.value("range",6.f),1.f,12.f);
        g_silentAimModuleSettings.fov=std::clamp(sa.value("fov",180.f),10.f,360.f);
        g_silentAimModuleSettings.speed=std::clamp(sa.value("speed",6.f),1.f,40.f);
        g_silentAimModuleSettings.smooth=sa.value("smooth",true);
        g_silentAimModuleSettings.vertical=sa.value("vertical",true);
        g_silentAimModuleSettings.onlyWhileAttacking=sa.value("onlyWhileAttacking",false);
        g_silentAimModuleSettings.accuracy=sa.value("accuracy",0)==1?AimAccuracy::Head:
            (sa.value("accuracy",0)==2?AimAccuracy::Loose:AimAccuracy::Center);
        g_silentAimModuleSettings.noGlide=sa.value("noGlide",false);
        const auto ab=j.value("antiBot",nlohmann::json::object());
        g_antiBotSettings.checkNames=ab.value("checkNames",true);
        g_antiBotSettings.checkUuid=ab.value("checkUuid",true);
        g_antiBotSettings.allowOfflineUuid=ab.value("allowOfflineUuid",true);
        g_antiBotSettings.checkClones=ab.value("checkClones",true);
        const auto nt=j.value("nametags",nlohmann::json::object());
        g_nametagsSettings.showName=nt.value("showName",true);
        g_nametagsSettings.showHealth=nt.value("showHealth",true);
        g_nametagsSettings.showDistance=nt.value("showDistance",true);
        g_nametagsSettings.showArmor=nt.value("showArmor",true);
        g_nametagsSettings.showHeldItem=nt.value("showHeldItem",true);
        g_nametagsSettings.icons=nt.value("icons",true);
        g_nametagsSettings.maxDistance=std::clamp(nt.value("maxDistance",48.f),4.f,256.f);
        g_nametagsSettings.scale=std::clamp(nt.value("scale",1.f),0.4f,3.f);
        g_nametagsSettings.singleShow=nt.value("singleShow",false);
        const auto hj=j.value("hud",nlohmann::json::object());
        {
            HudSettings& h=g_hudSettings;
            h.customHotbar=hj.value("customHotbar",true);
            h.hotbarAnimations=hj.value("hotbarAnimations",true);
            h.hotbarRounded=hj.value("hotbarRounded",true);
            h.hotbarNumbers=hj.value("hotbarNumbers",true);
            h.hotbarItemCount=hj.value("hotbarItemCount",true);
            h.itemNamePopup=hj.value("itemNamePopup",true);
            h.panels=hj.value("panels",true);
            h.attackSpeedBar=hj.value("attackSpeedBar",true);
            h.customCrosshair=hj.value("customCrosshair",false);
            h.coverVanilla=hj.value("coverVanilla",true);
            h.platePadding=std::clamp(hj.value("platePadding",5.f),0.f,24.f);
            h.plateOpacity=std::clamp(hj.value("plateOpacity",1.f),0.3f,1.f);
            h.barAnimations=hj.value("barAnimations",true);
            h.barAnimSpeed=std::clamp(hj.value("barAnimSpeed",9.f),3.f,30.f);
            h.heartBeat=hj.value("heartBeat",true);
            h.absorbHealth=hj.value("absorbHealth",true);
            h.crosshairIndicator=hj.value("crosshairIndicator",false);
            h.crosshairIndicatorRange=std::clamp(hj.value("crosshairIndicatorRange",12.f),1.f,64.f);
            h.hotbarScale=std::clamp(hj.value("hotbarScale",1.f),0.6f,1.6f);
            h.hotbarRounding=std::clamp(hj.value("hotbarRounding",8.f),0.f,16.f);
            h.hotbarAnimSpeed=std::clamp(hj.value("hotbarAnimSpeed",14.f),4.f,30.f);
            h.panelScale=std::clamp(hj.value("panelScale",1.f),0.6f,1.6f);
            h.panelRounding=std::clamp(hj.value("panelRounding",8.f),0.f,16.f);
            h.panelIcons=hj.value("panelIcons",true);
            h.panelNumbers=hj.value("panelNumbers",true);
            h.panelOffsetY=std::clamp(hj.value("panelOffsetY",6),0,120);
            h.itemNameScale=std::clamp(hj.value("itemNameScale",1.f),0.6f,1.6f);
            h.itemNameHoldMs=std::clamp(hj.value("itemNameHoldMs",1600.f),300.f,5000.f);
            h.itemNameIcons=hj.value("itemNameIcons",true);
            h.attackSpeedScale=std::clamp(hj.value("attackSpeedScale",1.f),0.6f,1.6f);
            h.crosshairStyle=(CrosshairStyle)std::clamp(hj.value("crosshairStyle",2),0,8);
            h.crosshairOutline=hj.value("crosshairOutline",true);
            h.crosshairDynamic=hj.value("crosshairDynamic",false);
            h.crosshairSize=std::clamp(hj.value("crosshairSize",1.f),0.4f,3.f);
            h.crosshairGap=std::clamp(hj.value("crosshairGap",2.f),0.f,12.f);
            h.crosshairThickness=std::clamp(hj.value("crosshairThickness",1.5f),0.5f,5.f);
            h.crosshairDot=std::clamp(hj.value("crosshairDot",1.5f),0.5f,6.f);
            h.crosshairRotation=std::clamp(hj.value("crosshairRotation",0.f),-180.f,180.f);
            h.drawBoardPixel=std::clamp(hj.value("drawBoardPixel",2.f),1.f,6.f);
            const auto readColor=[&](const char* key, const ImVec4& fallback){
                const auto arr=hj.value(key,nlohmann::json::array({fallback.x,fallback.y,fallback.z,fallback.w}));
                if (!arr.is_array()||arr.size()<4) return fallback;
                return ImVec4(std::clamp(arr[0].get<float>(),0.f,1.f),std::clamp(arr[1].get<float>(),0.f,1.f),
                              std::clamp(arr[2].get<float>(),0.f,1.f),std::clamp(arr[3].get<float>(),0.f,1.f));
            };
            h.hotbarBackground=readColor("hotbarBackground",h.hotbarBackground);
            h.slotBackground=readColor("slotBackground",h.slotBackground);
            h.hotbarAccent=readColor("hotbarAccent",h.hotbarAccent);
            h.panelBackground=readColor("panelBackground",h.panelBackground);
            h.healthColor=readColor("healthColor",h.healthColor);
            h.absorbColor=readColor("absorbColor",h.absorbColor);
            h.plateColor=readColor("plateColor",h.plateColor);
            h.crosshairIndicatorColor=readColor("crosshairIndicatorColor",h.crosshairIndicatorColor);
            h.armorColor=readColor("armorColor",h.armorColor);
            h.foodColor=readColor("foodColor",h.foodColor);
            h.xpColor=readColor("xpColor",h.xpColor);
            h.itemNameBackground=readColor("itemNameBackground",h.itemNameBackground);
            h.itemNameText=readColor("itemNameText",h.itemNameText);
            h.attackSpeedColor=readColor("attackSpeedColor",h.attackSpeedColor);
            h.crosshairColor=readColor("crosshairColor",h.crosshairColor);
            const auto px=hj.value("drawBoardPixels",nlohmann::json::array());
            for (int i=0;i<81;++i)
                h.drawBoardPixels[i]=i<(int)px.size()&&px[i].is_boolean()?px[i].get<bool>():(i<(int)px.size()&&px[i].is_number()?px[i].get<int>()!=0:false);
        }
        const auto at=j.value("autoTotem",nlohmann::json::object());
        g_autoTotemSettings.refillOffhand=at.value("refillOffhand",true);
        g_autoTotemSettings.refillHotbarSlot=at.value("refillHotbarSlot",false);
        g_autoTotemSettings.hotbarSlot=std::clamp(at.value("hotbarSlot",1),1,9);
        g_autoTotemSettings.delayMs=std::clamp(at.value("delayMs",80),0,600);
        g_autoTotemSettings.silent=at.value("silent",true);
        g_autoTotemSettings.type=at.value("type",0)==1?TotemType::SMP:TotemType::Vanilla;
        g_autoTotemSettings.smpHealthHearts=std::clamp(at.value("smpHealthHearts",3.f),0.5f,10.f);
        g_autoTotemSettings.smpSwapDelayMs=std::clamp(at.value("smpSwapDelayMs",150),0,600);
        g_autoTotemSettings.smpShieldSlot=std::clamp(at.value("smpShieldSlot",1),1,9);
        const auto ac=j.value("autoClean",nlohmann::json::object());
        g_autoCleanSettings.silentClean=ac.value("silentClean",true);
        g_autoCleanSettings.scanDelayMs=std::clamp(ac.value("scanDelayMs",200),0,600);
        g_autoCleanSettings.customItems.clear();
        const auto items=ac.value("items",nlohmann::json::array());
        if (items.is_array())
            for (const auto& item : items)
                if (item.is_string() && !item.get<std::string>().empty() && g_autoCleanSettings.customItems.size()<64)
                    g_autoCleanSettings.customItems.push_back(item.get<std::string>());
        const auto cr=j.value("cartRefill",nlohmann::json::object());
        const auto crSlots=cr.value("slots",nlohmann::json::array());
        for (int i=0;i<9;++i)
            g_cartRefillSettings.slots[i]=(i<(int)crSlots.size()&&crSlots[i].is_boolean())?crSlots[i].get<bool>():false;
        g_cartRefillSettings.delayMs=std::clamp(cr.value("delayMs",150),0,600);
        g_cartRefillSettings.silent=cr.value("silent",true);
        const auto pcm=j.value("pearlCatch",nlohmann::json::object());
        g_pearlCatchSettings.pearlDelayMs=std::clamp(pcm.value("pearlDelayMs",100),0,600);
        g_pearlCatchSettings.windChargeDelayMs=std::clamp(pcm.value("windChargeDelayMs",120),0,600);
        g_pearlCatchSettings.swapBack=pcm.value("swapBack",true);
        const auto icm=j.value("instantCartMacro",nlohmann::json::object());
        g_instantCartSettings.bowChargeMs=std::clamp(icm.value("bowChargeMs",200),50,900);
        g_instantCartSettings.railDelayMs=std::clamp(icm.value("railDelayMs",100),0,600);
        g_instantCartSettings.cartDelayMs=std::clamp(icm.value("cartDelayMs",110),0,600);
        g_instantCartSettings.swapBack=icm.value("swapBack",true);
        const auto pa=j.value("pearlAgro",nlohmann::json::object());
        g_pearlAgroSettings.agroDelayMs=std::clamp(pa.value("agroDelayMs",150),0,600);
        g_pearlAgroSettings.fov=std::clamp(pa.value("fov",180.f),10.f,360.f);
        g_pearlAgroSettings.silentAim=pa.value("silentAim",true);
        g_pearlAgroSettings.look=pa.value("look",0)==1?WebLookMode::Smooth:WebLookMode::Instant;
        g_pearlAgroSettings.cameraSensitivity=std::clamp(pa.value("cameraSensitivity",10.f),0.f,50.f);
        const auto se=j.value("shaderEsp",nlohmann::json::object());
        g_shaderEspSettings.style=se.value("style",0)==1?ShaderGlowStyle::Custom:ShaderGlowStyle::Vanilla;
        g_shaderEspSettings.handGlow=se.value("handGlow",true);
        g_shaderEspSettings.handSize=std::clamp(se.value("handSize",25),5,60);
        g_shaderEspSettings.handOffsetX=std::clamp(se.value("handOffsetX",0),-30,30);
        g_shaderEspSettings.handOffsetY=std::clamp(se.value("handOffsetY",0),-30,30);
        g_shaderEspSettings.handRotation=std::clamp(se.value("handRotation",25),-90,90);
        g_shaderEspSettings.thickness=std::clamp(se.value("thickness",3),1,5);
        g_shaderEspSettings.opacity=std::clamp(se.value("opacity",0.85f),0.f,1.f);
        g_shaderEspSettings.maxDistance=std::clamp(se.value("maxDistance",64.f),8.f,256.f);
        const auto sec=se.value("color",nlohmann::json::array({0.94,0.38,0.30,1.0}));
        if (sec.is_array() && sec.size()>=4) g_shaderEspSettings.color=ImVec4(std::clamp(sec[0].get<float>(),0.f,1.f),std::clamp(sec[1].get<float>(),0.f,1.f),std::clamp(sec[2].get<float>(),0.f,1.f),std::clamp(sec[3].get<float>(),0.f,1.f));
        const auto sed=se.value("friendColor",nlohmann::json::array({0.36,0.76,1.0,1.0}));
        if (sed.is_array() && sed.size()>=4) g_shaderEspSettings.friendColor=ImVec4(std::clamp(sed[0].get<float>(),0.f,1.f),std::clamp(sed[1].get<float>(),0.f,1.f),std::clamp(sed[2].get<float>(),0.f,1.f),std::clamp(sed[3].get<float>(),0.f,1.f));
        const auto dc=j.value("dragClick",nlohmann::json::object());
        g_dragClickSettings.minCps=std::clamp(dc.value("minCps",8),1,40);
        g_dragClickSettings.maxCps=std::clamp(dc.value("maxCps",20),1,40);
        g_dragClickSettings.acceleration=std::clamp(dc.value("acceleration",5),1,10);
        g_dragClickSettings.pauseSeconds=std::clamp(dc.value("pauseSeconds",0.f),0.f,3.f);
        const auto pm=j.value("playerModel",nlohmann::json::object());
        g_playerModelSettings.scale=std::clamp(pm.value("scale",1.0f),0.5f,2.0f);
        g_playerModelSettings.throughWalls=pm.value("throughWalls",false);
        g_playerModelSettings.showTiming=pm.value("showTiming",false);
        const auto wi=j.value("wings",nlohmann::json::object());
        g_wingsSettings.style=(WingStyle)std::clamp(wi.value("style",0),0,5);
        g_wingsSettings.scale=std::clamp(wi.value("scale",1.0f),0.5f,2.0f);
        g_wingsSettings.opacity=std::clamp(wi.value("opacity",0.85f),0.1f,1.f);
        g_wingsSettings.animate=wi.value("animate",true);
        g_wingsSettings.flapSpeed=std::clamp(wi.value("flapSpeed",1.0f),0.5f,3.0f);
        g_wingsSettings.yOffset=std::clamp(wi.value("yOffset",0.0f),-1.0f,1.0f);
        g_wingsSettings.throughWalls=wi.value("throughWalls",false);
        const auto hw=j.value("headwear",nlohmann::json::object());
        g_headwearSettings.style=(HeadwearStyle)std::clamp(hw.value("style",7),0,14);
        g_headwearSettings.scale=std::clamp(hw.value("scale",1.0f),0.5f,2.0f);
        g_headwearSettings.yOffset=std::clamp(hw.value("yOffset",0.0f),-0.5f,0.5f);
        g_headwearSettings.rotationSpeed=std::clamp(hw.value("rotationSpeed",1.0f),0.5f,3.0f);
        g_headwearSettings.throughWalls=hw.value("throughWalls",false);
        const auto cp=j.value("capes",nlohmann::json::object());
        g_capesSettings.style=(CapeStyle)std::clamp(cp.value("style",0),0,14);
        g_capesSettings.physics=cp.value("physics",true);
        g_capesSettings.movementWave=cp.value("movementWave",true);
        const auto vm=j.value("viewModel",nlohmann::json::object());
        g_viewModelSettings.posX=std::clamp(vm.value("posX",0.0f),-1.5f,1.5f);
        g_viewModelSettings.posY=std::clamp(vm.value("posY",0.0f),-1.5f,1.5f);
        g_viewModelSettings.posZ=std::clamp(vm.value("posZ",0.0f),-1.5f,1.5f);
        g_viewModelSettings.rotX=std::clamp(vm.value("rotX",0.0f),-180.f,180.f);
        g_viewModelSettings.rotY=std::clamp(vm.value("rotY",0.0f),-180.f,180.f);
        g_viewModelSettings.rotZ=std::clamp(vm.value("rotZ",0.0f),-180.f,180.f);
        g_viewModelSettings.scale=std::clamp(vm.value("scale",1.0f),0.1f,2.0f);
        g_viewModelSettings.offHand=vm.value("offHand",true);
        g_viewModelSettings.opacity=std::clamp(vm.value("opacity",1.0f),0.1f,1.0f);
        const auto sw=j.value("stunWeb",nlohmann::json::object());
        g_stunWebSettings.look=sw.value("look",0)==1?WebLookMode::Smooth:WebLookMode::Instant;
        g_stunWebSettings.allowPlayers=sw.value("allowPlayers",true);
        g_stunWebSettings.cameraSensitivity=std::clamp(sw.value("cameraSensitivity",5.0f),0.f,50.f);
        g_stunWebSettings.silentPlace=sw.value("silentPlace",false);
        g_stunWebSettings.lookBack=sw.value("lookBack",false);
        g_stunWebSettings.placeDelayMs=std::clamp(sw.value("placeDelayMs",0),0,600);
        const auto mb=j.value("motionBlur",nlohmann::json::object());
        g_motionBlurSettings.enabled=mb.value("enabled",true);
        g_motionBlurSettings.strength=std::clamp(mb.value("strength",1.0f),0.1f,3.f);
        g_motionBlurSettings.method=(BlurMethod)std::clamp(mb.value("method",0),0,2);
        g_motionBlurSettings.refreshScaling=mb.value("refreshScaling",true);
        g_motionBlurSettings.accumulationMax=std::clamp(mb.value("accumulationMax",3),1,8);
        g_motionBlurSettings.accumulationMix=std::clamp(mb.value("accumulationMix",0.6f),0.f,1.f);
        g_motionBlurSettings.profile=mb.value("profile",1)==0?BlurProfile::Uniform:BlurProfile::Weighted;
        g_motionBlurSettings.sensitivity=std::clamp(mb.value("sensitivity",1.0f),0.2f,3.f);
        const auto wt=j.value("walkTrail",nlohmann::json::object());
        const auto wtc=wt.value("color",nlohmann::json::array({0.62,0.63,0.68,1.0}));
        if (wtc.is_array() && wtc.size()>=4) g_walkTrailSettings.color=ImVec4(std::clamp(wtc[0].get<float>(),0.f,1.f),std::clamp(wtc[1].get<float>(),0.f,1.f),std::clamp(wtc[2].get<float>(),0.f,1.f),std::clamp(wtc[3].get<float>(),0.f,1.f));
        g_walkTrailSettings.lifetime=std::clamp(wt.value("lifetime",3.0f),0.5f,10.f);
        g_walkTrailSettings.width=std::clamp(wt.value("width",2.5f),1.f,8.f);
        g_walkTrailSettings.maxPoints=std::clamp(wt.value("maxPoints",240),20,600);
        g_walkTrailSettings.fade=std::clamp(wt.value("fade",1.0f),0.f,1.f);
        g_walkTrailSettings.animate=wt.value("animate",true);
        g_walkTrailSettings.throughWalls=wt.value("throughWalls",false);
        const auto ls=j.value("logoutSpots",nlohmann::json::object());
        g_logoutSpotsSettings.maxDistance=std::clamp(ls.value("maxDistance",128.f),8.f,1024.f);
        g_logoutSpotsSettings.onlyPlayers=ls.value("onlyPlayers",true);
        const auto lsc=ls.value("color",nlohmann::json::array({0.0,0.0,0.0,1.0}));
        if (lsc.is_array() && lsc.size()>=4) g_logoutSpotsSettings.color=ImVec4(std::clamp(lsc[0].get<float>(),0.f,1.f),std::clamp(lsc[1].get<float>(),0.f,1.f),std::clamp(lsc[2].get<float>(),0.f,1.f),std::clamp(lsc[3].get<float>(),0.f,1.f));
        g_debugMode=j.value("debug",false);
        g_notifications=j.value("notifications",true);
        // Friends ride along with the rest of the configuration. A config that predates the list
        // (no "friends" key at all) leaves the current friends alone instead of wiping them, so
        // loading an older preset can never silently empty the list.
        if (j.contains("friends")) {
            const auto fr=j.value("friends",nlohmann::json::array());
            g_friends.clear();
            if (fr.is_array())
                for (const auto& entry : fr)
                    if (entry.is_string() && !entry.get<std::string>().empty() && g_friends.size()<256)
                        addFriend(entry.get<std::string>());
        }
        return true;
    } catch (...) { return false; }
}

} }
