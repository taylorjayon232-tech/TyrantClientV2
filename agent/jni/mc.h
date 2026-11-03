// ============================================================================
//  Passive — Java object wrappers around Minecraft classes (RAII local refs).
//  Every call is exception-guarded; failures return safe defaults.
// ============================================================================
#pragma once
#include "jvm.h"
#include "ids.h"
#include "../render/math.h"

#include <jni.h>
#include <string>
#include <vector>
#include <array>

namespace tyr {
namespace mc {

// ---------------------------------------------------------------------------
// Cached global class/jmethodID handles, resolved once after JVM attach.
// ---------------------------------------------------------------------------
struct Symbols {
    bool ok = false;

    // classes (global refs)
    jclass minecraft{}, entity{}, living{}, player{}, clientPlayer{}, world{},
           clientWorld{}, inventory{}, itemStack{}, item{}, blockItem{}, block{},
           vec3d{}, camera{}, renderTickCounter{}, blockState{}, gameOptions{}, simpleOption{}, text{}, textContent{},
           identifier{}, registries{}, defaultedList{}, chatHud{}, equipmentSlot{},
           slotActionType{}, direction{}, hand{}, interactionManager{}, session{}, screenHandler{},
           iterable{}, iterator{}, list{}, objectCls{}, doubleCls{}, gameProfile{}, entityHit{}, box{},
           blockPos{}, blockHitResult{}, actionResult{},
           itemCooldownMgr{}, itemEnchantments{}, registryEntry{}, hitType{},
           screen{}, inventoryScreen{}, hungerManager{};

    // methods
    jmethodID              mc_getInstance{},      mc_doAttack{},        mc_handleInput{}, mc_setScreen{},
              invScreen_ctor{},
              mc_getCameraEntity{},  mc_getWindow{},
              gr_getCamera{}, gr_getFov{}, tickCounter_getProgress{},
              ent_getX{}, ent_getY{}, ent_getZ{}, ent_getLerpedPos{}, ent_getEyePos{}, ent_getYaw{},
              ent_getPitch{}, ent_setYaw{}, ent_setPitch{}, ent_isAlive{}, ent_isSprinting{}, ent_setSprinting{},
              ent_isGlowing{}, ent_setGlowing{},
              ent_getWidth{}, ent_getHeight{}, ent_getUuidStr{}, ent_getVelocity{},
              ent_setVelocity{}, ent_getStandEye{}, ent_getName{},
              liv_getHealth{}, liv_getMaxHealth{}, liv_getAbsorption{}, liv_tryAttack{}, liv_isBlocking{}, liv_getArmor{},
              liv_isUsingItem{}, liv_getActiveItem{}, liv_getMainHand{},               liv_getOffHand{}, liv_getEquip{}, liv_isGliding{}, liv_getStatusEffects{}, liv_swingHand{},
               liv_lerpYaw{},                            // (F)F — the render-facing rotation read
              pl_getInventory{}, pl_getAttackCd{}, pl_getGameProfile{}, pl_isCreative{},
              pl_getCooldownMgr{}, pl_getHunger{}, hunger_getFoodLevel{},
              inv_swapHotbar{}, inv_getSelected{}, inv_getMainStacks{}, inv_setSelected{},
              stk_isEmpty{}, stk_getCount{}, stk_getItem{}, stk_getDamage{},
              stk_getMaxDamage{}, stk_isDamageable{}, stk_isOf{}, stk_getName{},
              stk_getEnch{}, ice_entrySet{}, holder_matchesId{}, num_intValue{},
              txt_getLiteralString{}, txt_content{},
              cam_getFocused{}, cam_getThirdPerson{}, cam_getYaw{}, cam_getPitch{},
              cam_getHorizontalPlane{}, cam_getVerticalPlane{}, cam_getDiagonalPlane{},
              opt_getValue{}, opt_setValue{},
              win_intMethod[9]{},
              im_attackEntity{}, im_interactBlock{}, im_interactItem{}, im_getGameMode{}, im_swapHotbar{}, im_syncSelectedSlot{},
              im_clickSlot{},
              chat_addToHistory{},
              world_getEntities{}, world_getBlockState{}, blockState_getBlock{}, it_next{}, it_hasNext{}, cd_isCoolingDown{},
              id_getPath{}, id_of{}, reg_getId{},
              dl_get{}, dl_size{},
              iter_next{}, iter_hasNext{}, hit_getType{}, hit_getPos{}, blockHit_getBlockPos{}, blockHit_getSide{},
              blockPos_getX{}, blockPos_getY{}, blockPos_getZ{}, vec3d_ctor{}, blockPos_ctor{}, blockHit_ctor{},
              actionResult_isAccepted{},
              entHit_getEntity{}, ent_type{}, enum_name{},
              pl_getBlockRange{}, pl_getEntityRange{}, ent_raycast{}, obj_toString{};

    // fields
    jfieldID mc_player{}, mc_world{}, mc_options{}, mc_interaction{}, mc_session{}, mc_currentScreen{},
             pl_itemCooldownMgr{},
             mc_itemUseCd{}, mc_currentFps{}, mc_mouse{}, mc_gameRenderer{}, mc_tickCounter{},
             mc_attackCooldown{},
             cli_lastYaw{}, cli_lastPitch{},
             ent_pos{}, ent_prevX{}, ent_prevY{}, ent_prevZ{}, ent_yaw{}, ent_pitch{},
             ent_lastYaw{}, ent_lastPitch{}, ent_age{}, ent_fallDist{}, ent_onGround{}, ent_bbox{},
             box_minX{}, box_minY{}, box_minZ{}, box_maxX{}, box_maxY{}, box_maxZ{},
             liv_hurtTime{}, liv_maxHurtTime{}, liv_deathTime{}, liv_swinging{},
             liv_bodyYaw{}, liv_headYaw{}, liv_lastBodyYaw{}, liv_lastHeadYaw{},
             pl_abilities{}, pl_playerScreenHandler{}, pl_currentScreenHandler{}, sh_syncId{},
             vec_x{}, vec_y{}, vec_z{},   // Vec3d component fields
             inv_selectedSlot{}, inv_main{}, inv_offHand{},
             opt_gamma{}, opt_fov{}, opt_hudHidden{}, so_value{},
             pl_hungerManager{}, pl_experienceLevel{}, pl_experienceProgress{},
             cam_pos{}, cam_thirdPerson{},
             ses_username{}, ses_uuid{};

    // Entity.fallDistance is a double in this tree and a float in older ones, and the descriptor
    // that resolved decides which accessor reads it (see resolveAll).
    bool ent_fallDistIsFloat = false;

    // enum constants (static fields, global refs after resolve)
    jobject eqMainhand{}, eqOffhand{}, eqFeet{}, eqLegs{}, eqChest{}, eqHead{};
    jobject satSwap{}, satPickup{}, satThrow{};
    jobject dirDown{}, dirUp{}, dirNorth{}, dirSouth{}, dirWest{}, dirEast{};
};

Symbols& sym();          // singleton access
bool     resolveAll();   // one-time resolve; returns false on critical miss

// Per-frame context: grab MinecraftClient + friends as global refs, release at
// frame end (call from the SwapBuffers hook around ImGui render).
namespace frame {
void beginFrame();
void endFrame();
bool valid();
jobject client();
bool resolved();  // full symbol resolve succeeded (modules may run)
} // namespace frame

// ---------------------------------------------------------------------------
// LocalRef: RAII guard for JNI local references inside a frame.
// ---------------------------------------------------------------------------
class LocalRef {
public:
    LocalRef() = default;
    explicit LocalRef(jobject o) : obj_(o) {}
    ~LocalRef() { reset(); }
    LocalRef(const LocalRef&) = delete;
    LocalRef& operator=(const LocalRef&) = delete;
    LocalRef(LocalRef&& o) noexcept : obj_(o.obj_) { o.obj_ = nullptr; }
    LocalRef& operator=(LocalRef&& o) noexcept { reset(); obj_ = o.obj_; o.obj_ = nullptr; return *this; }
    void reset() {
        if (obj_ && jni::env()) jni::env()->DeleteLocalRef(obj_);
        obj_ = nullptr;
    }
    jobject get() const { return obj_; }
    operator bool() const { return obj_ != nullptr; }
private:
    jobject obj_ = nullptr;
};

// ---------------------------------------------------------------------------
// Vec3 mirror of net.minecraft.class_243 (read-only usage).
// ---------------------------------------------------------------------------
struct JVec3 {
    double x = 0, y = 0, z = 0;
    Vec3 toF() const { return { (float)x, (float)y, (float)z }; }
};

// ---------------------------------------------------------------------------
// Game context — the root accessor. Obtained once per frame.
// ---------------------------------------------------------------------------
class Client {
public:
    static bool acquire();            // fetch MinecraftClient.getInstance()
    static jobject handle();          // raw jobject (valid until next frame end)

    static jobject player();          // ClientPlayerEntity or null
    static jobject world();           // ClientWorld or null
    static jobject options();         // GameOptions
    static jobject interaction();     // ClientPlayerInteractionManager or null
    static jobject session();         // Session
    static jobject camera();          // Camera (via gameRenderer)
    static jobject mouse();
    static bool screenOpen();          // true while any Minecraft screen is open
    // Open / close the player's own inventory exactly like the vanilla inventory key does
    // (MinecraftClient.setScreen(new InventoryScreen(player)) / setScreen(null)). Used by the
    // inventory modules so they never synthesise somebody else's keyboard key.
    static bool openInventoryScreen();   // false when the game classes could not be resolved
    static bool closeScreen();           // closes whatever screen is open, sending the close packet

    static int  itemUseCooldown();
    static void setItemUseCooldown(int v);
    static int  currentFps();
    static float renderTickDelta();  // vanilla partial tick used for this rendered frame
    // Camera FOV. `changedFov` is what the world is drawn with (sprint, zoom and
    // item effects included); pass false for the FOV the first-person hand uses,
    // which the game keeps at the base value so the hand does not zoom.
    static float renderFov(float tickDelta, bool changedFov = true);

    static jobject crosshair();       // game's own crosshairTarget (HitResult)
    // Point the game's crosshairTarget at `entity` (null = miss). The HUD's attack indicator and the
    // block outline both read this one field, so a silent aim can show the indicator and drop the
    // block outline without the camera moving. The game recomputes the pick each frame.
    static bool setCrosshairTargetEntity(jobject entity);
    // Fluid-aware pick from the camera entity (Entity.raycast with includeFluids=true).
    // The game's own crosshairTarget ignores fluids, so water is never "hit" by it.
    static jobject raycastWithFluids(double range, float tickDelta);   // local ref or null
    static std::string blockStateString(int x, int y, int z);          // e.g. "Block{minecraft:water}[level=0]"
    static int  attackCooldown();     // MinecraftClient.attackCooldown (vanilla miss delay, ticks)

    static bool doAttack();           // MinecraftClient.doAttack()
    static void handleInputEvents();  // MinecraftClient.handleInputEvents()
    static bool interactAtCrosshair(); // interactionManager.interactBlock(player, mainHand, crosshairTarget)
    static bool interactItem(bool offhand = false);        // vanilla interactItem(player, hand)
    static bool interactBlockAt(int blockX, int blockY, int blockZ,
                                double hitX, double hitY, double hitZ,
                                bool offhand = false); // ordinary vanilla block interaction
    // Same, but with an explicit hit face instead of the hard-coded UP face:
    // 0=DOWN 1=UP 2=NORTH 3=SOUTH 4=WEST 5=EAST. hitX/hitY/hitZ are the
    // absolute world hit position (pass mc::hit::pos(hit)). Needed where the
    // face decides the placement cell — e.g. placing a cobweb on a water source.
    static bool interactBlockAtFace(int blockX, int blockY, int blockZ,
                                    double hitX, double hitY, double hitZ,
                                    int face, bool offhand = false);
    // Object.toString() of any Java object (guarded, empty on failure). Used to
    // read an ItemStack's components (enchants, trims, count, damage) without
    // mapping every component accessor.
    static std::string objToString(jobject obj);
    static bool attackEntity(jobject target); // interactionManager.attackEntity(player, target)
    static void syncSelectedSlot(); // sends the game's ordinary selected-hotbar-slot update

    // ---- Player inventory screen-handler slot clicks (vanilla slot-click path) ----
    // syncId of the player's own PlayerScreenHandler (field_7763 on class_1703).
    static int  playerScreenHandlerSyncId();
    // True when the player's own inventory handler is the screen the server expects
    // (no screen open, or their inventory screen is the one open).
    static bool playerScreenIsCurrent();
    // Screen-handler slot for a PlayerInventory main-list index (0-35):
    //   0-8 hotbar -> 36..44, 9-35 main -> 9..35.  Off-hand is slot 45.
    static int  playerScreenSlotForMainIndex(int index);
    // clickSlot(syncId, slot, 40, SWAP, player): swaps `slot` with the off hand.
    static bool swapOffhand(int screenSlot);
    // clickSlot(syncId, slot, hotbarIndex, SWAP, player): swaps `slot` with hotbar 0-8.
    static bool swapHotbarSlot(int screenSlot, int hotbarIndex);
    // clickSlot(syncId, slot, wholeStack?1:0, THROW, player): drops the slot's stack.
    static bool throwSlot(int screenSlot, bool wholeStack);
    // clickSlot(syncId, slot, 0, PICKUP, player): the ordinary left click on one slot. Three of
    // them move a stack between the inventory and an armour slot (pick it up, click the armour
    // slot, click the inventory slot again), which is how AutoMace changes a chestplate.
    static bool pickupSlot(int screenSlot);
    static bool isRespawnAnchorAt(int x, int y, int z);
    static bool isWaterAt(int x, int y, int z);
    static bool isPressurePlateAt(int x, int y, int z);
    static bool isCobwebAt(int x, int y, int z);
    static std::string blockIdAt(int x, int y, int z);

    // Resolved jmethodIDs for combat modules that must call attacks on their
    // own thread (Lunge Macro scheduled logic). Null until resolveAll ran.
    static jmethodID doAttackSym();
    static jmethodID attackEntitySym();

    static std::string username();
};

// HitResult helpers over the game's own crosshairTarget.
namespace hit {
int     typeOf(jobject hit);   // 0=miss 1=block 2=entity, -1 fail
Vec3    pos(jobject hit);      // block hit pos or entity pos
jobject entity(jobject hit);   // EntityHitResult.getEntity() local ref
bool    isUpFace(jobject hit);
bool    sideOffset(jobject hit, int& dx, int& dy, int& dz);
bool    blockPos(jobject hit, int& x, int& y, int& z);
} // namespace hit

// ---------------------------------------------------------------------------
// Entity / LivingEntity / Player views (all null-tolerant).
// ---------------------------------------------------------------------------
class Entity {
public:
    explicit Entity(jobject o) : obj_(o) {}
    explicit operator bool() const { return obj_ != nullptr; }
    jobject handle() const { return obj_; }

    Vec3  pos() const;                 // feet-center
    Vec3  prevPos() const;             // last-render pos (for lerp)
    Vec3  eyePos() const;
    float yaw() const;                 // degrees
    float pitch() const;
    void  setYaw(float yaw) const;     // degrees
    void  setPitch(float pitch) const; // degrees
    float prevYaw() const;             // previous-tick rotation (render interpolation source)
    float prevPitch() const;
    int   age() const;                 // Entity.age (ticks this entity has existed; -1 if unresolved)
    void  setPrevYaw(float yaw) const;   // written by silent aim: see modules.cpp Silent rotation
    void  setPrevPitch(float pitch) const;
    float width() const;
    float height() const;
    float eyeHeight() const;
    bool  alive() const;
    bool  sprinting() const;
    void  setSprinting(bool on) const;
    // Minecraft's own glowing outline is a client-side entity flag. Shader ESP sets
    // it on the players and dropped items it wants outlined and clears it again on
    // its own entries; an entity that already glows is never touched.
    bool  glowing() const;
    void  setGlowing(bool on) const;
    Vec3  velocity() const;
    void  setVelocity(const Vec3& v) const;
    double fallDistance() const;
    bool  onGround() const;
    std::string uuid() const;
    std::string displayName();   // Entity.getName() Text -> string (never throws)

    double distanceTo(const Entity& o) const;
    Vec3   lerpPos(float tickDelta) const;   // prev->cur interpolation
    struct Bounds { Vec3 min{}, max{}; bool valid = false; };
    Bounds interpolatedBounds(float tickDelta) const;

protected:
    jobject obj_;
};

class LivingEntity : public Entity {
public:
    explicit LivingEntity(jobject o) : Entity(o) {}
    float health() const;
    float maxHealth() const;
    float absorptionAmount() const;            // LivingEntity.getAbsorptionAmount() (0 when none)
    int   hurtTime() const;
    bool  blocking() const;                    // shield up
    bool  usingItem() const;
    jobject activeItem() const;                // ItemStack (local ref)
    jobject mainHandStack() const;
    jobject offHandStack() const;
    jobject equippedStack(int slotIndex) const; // 0=MAINHAND..5=HEAD (EquipmentSlot ordinal order: MAINHAND,OFFHAND,FEET,LEGS,CHEST,HEAD)
    bool  gliding() const;                     // elytra
    // Active status effects as Component.toString() text ("minecraft:speed ...").
    std::string statusEffectsText() const;
    void  swingHand(bool offhand=false) const;
    // Body/head rotation the third-person model is drawn with (not the same as Entity yaw).
    // setBodyPose writes the "last" values as well, so the renderer has nothing to blend from:
    // writing only the current pair left it interpolating from the camera's rotation every tick.
    void  setBodyPose(float yaw) const;
    // Head rotation only (current + "last"). Leaves the body alone so movement keeps following the camera.
    void  setHeadPose(float yaw) const;
    // NOTE: tryAttack removed — 1.21.11's LivingEntity.tryAttack needs a
    // ServerWorld and is server-authoritative. Use mc::Client::attackEntity.
};

class Player : public LivingEntity {
public:
    explicit Player(jobject o) : LivingEntity(o) {}
    jobject inventory() const;
    float attackCooldownProgress(float tickDelta) const; // 0..1
    jobject gameProfile() const;
    bool  creative() const;
    // ---- custom HUD bar reads (all null-tolerant, safe defaults) ----
    int   armorPoints() const;          // LivingEntity.getArmor() — points from worn gear
    int   foodLevel() const;            // HungerManager.getFoodLevel() (0..20; 20 = full)
    int   experienceLevel() const;      // PlayerEntity.experienceLevel
    float experienceProgress() const;   // PlayerEntity.experienceProgress (0..1 towards the next level)
    double blockInteractionRange() const;   // PlayerEntity.getBlockInteractionRange (4.5 survival)
    double entityInteractionRange() const;  // PlayerEntity.getEntityInteractionRange (3.0 survival)
};

// ---------------------------------------------------------------------------
// PlayerInventory helpers (work on the DefaultedList fields directly).
// ---------------------------------------------------------------------------
namespace inv {
    jobject mainList(jobject inventoryObj);   // DefaultedList (36)
    // NOTE: armorList/offHandList removed — read equipment via
    // LivingEntity::equippedStack (1.21.11 inventory rework).
int    selectedSlot(jobject inventoryObj);
void   setSelectedSlot(jobject inventoryObj, int slot);
// True "press number key" swap: server-acknowledged, hand ends on `slot`
// (swapSlotWithHotbar shuffles stacks around and leaves a ghost hand).
void   setSelectedSlotMethod(jobject inventoryObj, int slot);
    jobject getFromList(jobject list, int index);      // ItemStack or null
    int    listSize(jobject list);
    void   swapWithHotbar(jobject inventoryObj, int slot);
    bool   isCoolingDown(jobject stack);       // ItemCooldownManager.isCoolingDown
    jobject selectedStack(jobject inventoryObj);
}

// ---------------------------------------------------------------------------
// ItemStack helpers.
// ---------------------------------------------------------------------------
namespace stk {
    bool  isEmpty(jobject stack);
    int   count(jobject stack);
    jobject item(jobject stack);              // Item or null
    int   damage(jobject stack);
    int   maxDamage(jobject stack);
    bool  damageable(jobject stack);
    bool  isOf(jobject stack, jobject itemObj);
    std::string name(jobject stack);          // best-effort text extraction
    // Item id path like "diamond_sword" via Registries.ITEM.getId(item).getPath
    std::string itemId(jobject stack);
    // Level of the "lunge" enchantment on this stack (0 = none). Detects the
    // enchantment through its registry holder id, no Enchantment class needed.
    int lungeLevel(jobject stack);
}

// ---------------------------------------------------------------------------
// Options (gamma/fov SimpleOption access).
// ---------------------------------------------------------------------------
namespace opts {
    jobject simpleOption(jobject gameOptions, jfieldID optionField);
    double  getDoubleOption(jobject gameOptions, jfieldID optionField, double fallback);
    void    setDoubleOption(jobject gameOptions, jfieldID optionField, double value);
    // Writes the option's raw value field, bypassing the slider/validator clamp.
    // Used by StaticFov to hold a field of view above the vanilla maximum.
    void    setIntOptionRaw(jobject gameOptions, jfieldID optionField, int value);
    // Plain boolean fields on GameOptions (e.g. hudHidden), read/written directly so the
    // custom HUD can hide the game's own HUD while it is drawing its own.
    bool    getBoolOption(jobject gameOptions, jfieldID optionField, bool fallback);
    void    setBoolOption(jobject gameOptions, jfieldID optionField, bool value);
}

// ---------------------------------------------------------------------------
// The game's own HUD geometry.
// ---------------------------------------------------------------------------
namespace hud {
    // The GUI scale the game is drawing its HUD with (1..6), or 0 when it could not be resolved.
    // The two arguments are the framebuffer size the overlay itself draws into (ImGui's
    // DisplaySize), which is the space the scale maps the vanilla GUI coordinates onto. The
    // answer is cached; a failed probe is retried at most once a second.
    int vanillaGuiScale(float framebufferWidth, float framebufferHeight);
}

// ---------------------------------------------------------------------------
// Camera.
// ---------------------------------------------------------------------------
namespace cam {
    Vec3  pos(jobject camera);
    float yaw(jobject camera);
    float pitch(jobject camera);
    bool  thirdPerson(jobject camera);
    jobject focused(jobject camera);
    bool  basis(jobject camera, Vec3& right, Vec3& up, Vec3& forward);
}

// ---------------------------------------------------------------------------
// World entity iteration (ClientWorld.getEntities() -> Iterable<Entity>).
// ---------------------------------------------------------------------------
std::vector<jobject> collectEntities(jobject world, int maxCount);

// ---------------------------------------------------------------------------
// InteractionManager.
// ---------------------------------------------------------------------------
namespace im {
    void attackEntity(jobject interactionManager, jobject player, jobject target);
    int  gameModeId(jobject interactionManager); // GameMode ordinal-ish (best-effort)
}

// ---------------------------------------------------------------------------
// Chat HUD.
// ---------------------------------------------------------------------------
void chatMessage(const std::string& text); // adds to chat history/hud best-effort

// ---------------------------------------------------------------------------
// Text helpers (extract readable string from a Text).
// ---------------------------------------------------------------------------
std::string textToString(jobject textObj);

// ---------------------------------------------------------------------------
// Registry id for an item instance.
// ---------------------------------------------------------------------------
std::string registryItemPath(jobject itemObj);

// Registry path of an entity's type, e.g. "ender_pearl". Registry ids are the same text in every
// language, unlike an entity's display name, which is translated - which is why matching a pearl
// by its name only ever worked on an English client.
std::string entityTypePath(jobject entity);


// Extended helpers: vanilla key presses, vanilla look packet, block-item test.
namespace ext {
enum class Key { Jump, Sprint, Use };
bool setKey(Key which, bool pressed);                     // KeyBinding.setPressed
// Silent aim's three packets, all real PlayerMoveC2SPacket subclasses and all sent from this DLL -
// nothing about the aim needs a Java payload. Each carries the fields it actually has in 1.21:
//   LookAndOnGround      yaw, pitch            (onGround, horizontalCollision) - rotation only,
//                                              which is what the aim repeats every frame
//   PositionAndOnGround  x, y, z              (onGround, horizontalCollision) - position only; it
//                                              carries NO rotation, so it never aims anything alone
//   Full                 x, y, z, yaw, pitch  (onGround, horizontalCollision) - both in one packet,
//                                              which is what makes the server track the aim as the
//                                              player's own movement
bool sendLookPacket(float yaw, float pitch, bool onGround);   // PlayerMoveC2SPacket.LookAndOnGround
// The player's own camera rotation, sent from outside the per-frame JNI scope (the input hook,
// before the game's next tick sends a block place / break). The client and player are resolved
// here because the frame globals are released at the end of every frame. Writes the last-sent
// rotation back to the player, exactly like sendLookPacket. `yawOut`/`pitchOut` receive what was
// sent; either may be null.
bool sendCameraLookLive(float* yawOut, float* pitchOut);
bool sendPositionPacket(bool onGround);                       // PlayerMoveC2SPacket.PositionAndOnGround
bool sendFullPacket(float yaw, float pitch, bool onGround);   // PlayerMoveC2SPacket.Full
// Resolve and report all three; true when every one can be sent. The resolve pass logs which of
// them were found, because a missing one leaves silent aim with nothing to send.
bool probeMovePackets();
const char* movePacketsStatus();   // "ready (look=... position=... full=...)"
bool isBlockItemStack(jobject stack);
} // namespace ext

} // namespace mc
} // namespace tyr
