// ============================================================================
//  Passive — Minecraft JNI wrappers (implementation).
//  Contract: every function is null-safe and exception-guarded. If the JVM
//  call fails, the game must never see a pending exception and Passive must
//  degrade to "module inactive" instead of crashing.
// ============================================================================
#include "mc.h"
#include "jvm.h"
#include "ids.h"
#include "../core/agent.h"   // trace::log (resolve summary)
#include "../modules/text_name.h"   // the pure translatable-name rule

#include <jni.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cmath>

namespace tyr {
namespace mc {

// ---------------------------------------------------------------------------
// Symbol singleton
// ---------------------------------------------------------------------------
static Symbols g_sym;
static bool    g_resolved = false;

Symbols& sym() { return g_sym; }

namespace {

jclass makeGlobal(JNIEnv* e, jclass local) {
    if (!local) return nullptr;
    jclass g = static_cast<jclass>(e->NewGlobalRef(local));
    e->DeleteLocalRef(local);
    return g;
}

jobject makeGlobalObj(JNIEnv* e, jobject local) {
    if (!local) return nullptr;
    jobject g = e->NewGlobalRef(local);
    e->DeleteLocalRef(local);
    return g;
}

jobject enumStatic(JNIEnv* e, jclass cls, const char* name, const char* sig) {
    if (!cls) return nullptr;
    const jfieldID fid = e->GetStaticFieldID(cls, name, sig);
    if (!fid) { jni::clearException(e, "enumConst"); return nullptr; }
    return e->GetStaticObjectField(cls, fid);
}

// Cached Hand enum constants (MAIN_HAND / OFF_HAND). Used by every vanilla
// interaction path so the caller can act from either hand.
jobject handEnum(JNIEnv* e, bool offhand) {
    static jobject mainHand = nullptr, offHandEnum = nullptr;
    if (!mainHand || !offHandEnum) {
        const jfieldID mf = e->GetStaticFieldID(g_sym.hand, fld::kHand_MAIN, "Lnet/minecraft/class_1268;");
        const jfieldID of = e->GetStaticFieldID(g_sym.hand, fld::kHand_OFF,  "Lnet/minecraft/class_1268;");
        if (mf && !mainHand)     mainHand     = e->NewGlobalRef(e->GetStaticObjectField(g_sym.hand, mf));
        if (of && !offHandEnum)  offHandEnum  = e->NewGlobalRef(e->GetStaticObjectField(g_sym.hand, of));
        jni::clearException(e, "hand constants");
    }
    return offhand ? offHandEnum : mainHand;
}

// Read a Vec3d's components via its double fields (field_1352/1351/1350).
Vec3 readVec3(jobject vec) {
    JNIEnv* e = jni::env();
    if (!e || !vec) return {};
    const jdouble x = jni::getDouble(e, vec, g_sym.vec_x);
    const jdouble y = jni::getDouble(e, vec, g_sym.vec_y);
    const jdouble z = jni::getDouble(e, vec, g_sym.vec_z);
    return { (float)x, (float)y, (float)z };
}

} // namespace

bool resolveAll() {
    JNIEnv* e = jni::env();
    if (!e) { jni::pushError("resolveAll: no env"); return false; }
    Symbols& s = g_sym;

    // ---- classes ----------------------------------------------------------
    s.minecraft           = makeGlobal(e, jni::findClass(e, mcids::kMinecraftClient));
    s.entity              = makeGlobal(e, jni::findClass(e, mcids::kEntity));
    s.living              = makeGlobal(e, jni::findClass(e, mcids::kLivingEntity));
    s.player              = makeGlobal(e, jni::findClass(e, mcids::kPlayerEntity));
    s.clientPlayer        = makeGlobal(e, jni::findClass(e, mcids::kClientPlayerEntity));
    s.world               = makeGlobal(e, jni::findClass(e, mcids::kWorld));
    s.clientWorld         = makeGlobal(e, jni::findClass(e, mcids::kClientWorld));
    s.inventory           = makeGlobal(e, jni::findClass(e, mcids::kPlayerInventory));
    s.itemStack           = makeGlobal(e, jni::findClass(e, mcids::kItemStack));
    s.item                = makeGlobal(e, jni::findClass(e, mcids::kItem));
    s.blockItem           = makeGlobal(e, jni::findClass(e, mcids::kBlockItem));
    s.block               = makeGlobal(e, jni::findClass(e, mcids::kBlock));
    s.blockState          = makeGlobal(e, jni::findClass(e, mcids::kBlockState));
    s.vec3d               = makeGlobal(e, jni::findClass(e, mcids::kVec3d));
    s.blockPos            = makeGlobal(e, jni::findClass(e, mcids::kBlockPos));
    s.blockHitResult      = makeGlobal(e, jni::findClass(e, mcids::kBlockHitResult));
    s.actionResult        = makeGlobal(e, jni::findClass(e, mcids::kActionResult));
    s.box                 = makeGlobal(e, jni::findClass(e, mcids::kBox));
    s.vec_x              = jni::field(e, s.vec3d, mcids::kVec_x, "D");
    s.vec_y              = jni::field(e, s.vec3d, mcids::kVec_y, "D");
    s.vec_z              = jni::field(e, s.vec3d, mcids::kVec_z, "D");
    s.camera              = makeGlobal(e, jni::findClass(e, mcids::kCamera));
    s.renderTickCounter   = makeGlobal(e, jni::findClass(e, mcids::kRenderTickCounter));
    s.gameOptions         = makeGlobal(e, jni::findClass(e, mcids::kGameOptions));
    s.simpleOption        = makeGlobal(e, jni::findClass(e, mcids::kSimpleOption));
    s.text                = makeGlobal(e, jni::findClass(e, mcids::kText));
    s.textContent         = makeGlobal(e, jni::findClass(e, mcids::kTextContent));
    s.identifier          = makeGlobal(e, jni::findClass(e, mcids::kIdentifier));
    s.registries          = makeGlobal(e, jni::findClass(e, mcids::kRegistries));
    s.defaultedList       = makeGlobal(e, jni::findClass(e, mcids::kDefaultedList));
    s.chatHud             = makeGlobal(e, jni::findClass(e, mcids::kChatHud));
    s.equipmentSlot       = makeGlobal(e, jni::findClass(e, mcids::kEquipmentSlot));
    s.slotActionType      = makeGlobal(e, jni::findClass(e, mcids::kSlotActionType));
    s.screenHandler       = makeGlobal(e, jni::findClass(e, mcids::kScreenHandler));
    s.direction          = makeGlobal(e, jni::findClass(e, mcids::kDirection));
    s.hand               = makeGlobal(e, jni::findClass(e, mcids::kHand));
    s.interactionManager = makeGlobal(e, jni::findClass(e, mcids::kInteractionManager));
    s.session             = makeGlobal(e, jni::findClass(e, mcids::kSession));
    s.iterable            = makeGlobal(e, jni::findClass(e, mcids::kIterable));
    s.iterator            = makeGlobal(e, jni::findClass(e, mcids::kIterator));
    s.gameProfile         = makeGlobal(e, jni::findClass(e, mcids::kGameProfile));
    s.itemCooldownMgr     = makeGlobal(e, jni::findClass(e, mcids::kItemCooldownMgr));
    s.itemEnchantments    = makeGlobal(e, jni::findClass(e, mcids::kItemEnchantments));
    s.registryEntry       = makeGlobal(e, jni::findClass(e, mcids::kRegistryEntry));
    if (s.registryEntry) s.holder_matchesId = e->GetMethodID(s.registryEntry, mth::kHolder_matchesId, "(Lnet/minecraft/class_2960;)Z");
    if (jclass numCls = e->FindClass("java/lang/Number")) {
        s.num_intValue = e->GetMethodID(numCls, "intValue", "()I");
        e->DeleteLocalRef(numCls);
    }
    if (!s.minecraft || !s.entity || !s.living || !s.itemStack || !s.inventory) {
        jni::pushError("resolveAll: core class missing");
        return false;
    }

    // ---- MinecraftClient methods/fields ------------------------------------
    // Methods resolved by PINNED INTERMEDIARY ID (mth namespace, verified
    // against yarn 1.21.11+build.6) so a production install works; the yarn
    // name is kept in a comment for each.
    s.mc_getInstance  = e->GetStaticMethodID(s.minecraft, mth::kClient_getInstance, "()Lnet/minecraft/class_310;");
    s.mc_player       = jni::field(e, s.minecraft, fld::kClient_player,   "Lnet/minecraft/class_746;");
    s.mc_world        = jni::field(e, s.minecraft, fld::kClient_world,    "Lnet/minecraft/class_638;");
    s.mc_options      = jni::field(e, s.minecraft, fld::kClient_options,  "Lnet/minecraft/class_315;");
    s.mc_interaction  = jni::field(e, s.minecraft, fld::kClient_interaction, "Lnet/minecraft/class_636;");
    s.mc_session      = jni::field(e, s.minecraft, fld::kClient_session,  "Lnet/minecraft/class_320;");
    s.mc_currentScreen = jni::field(e, s.minecraft, fld::kClient_currentScreen, "Lnet/minecraft/class_437;");
    s.mc_itemUseCd    = jni::field(e, s.minecraft, fld::kClient_itemUseCd, "I");
    s.mc_currentFps   = jni::sfield(e, s.minecraft, fld::kClient_currentFps, "I");   // 1.21.11: private STATIC field
    s.mc_mouse        = jni::field(e, s.minecraft, fld::kClient_mouse,    "Lnet/minecraft/class_312;");
    s.mc_gameRenderer = jni::field(e, s.minecraft, fld::kClient_gameRenderer, "Lnet/minecraft/class_757;");
    s.mc_tickCounter  = jni::field(e, s.minecraft, fld::kClient_tickCounter,
                                   "Lnet/minecraft/class_9779$class_9781;");
    if (s.renderTickCounter)
        s.tickCounter_getProgress = jni::method(e, s.renderTickCounter,
            mth::kTickCounter_getProgress, "(Z)F");
    s.mc_doAttack     = jni::method(e, s.minecraft, mth::kClient_doAttack,  "()Z");   // doAttack
    s.mc_handleInput  = jni::method(e, s.minecraft, mth::kClient_handleInput, "()V");   // handleInputEvents

    // ---- Player inventory screen (what the inventory key does) -------------------
    // The inventory modules open the player's inventory through the game API instead of
    // synthesising a keyboard key: the inventory key is rebindable, so a synthetic key fires
    // whatever the player actually bound to it (one report: their E is crouch).
    s.screen          = makeGlobal(e, jni::findClass(e, mcids::kScreen));
    s.inventoryScreen = makeGlobal(e, jni::findClass(e, mcids::kInventoryScreen));
    s.mc_setScreen    = jni::method(e, s.minecraft, mth::kClient_setScreen,
                                    "(Lnet/minecraft/class_437;)V");                 // setScreen(Screen)
    if (s.inventoryScreen)
        s.invScreen_ctor = jni::method(e, s.inventoryScreen, "<init>",
                                       "(Lnet/minecraft/class_1657;)V");             // InventoryScreen(PlayerEntity)
    if (!s.inventoryScreen || !s.mc_setScreen)
        trace::log("inventory screen unavailable - the visible inventory modules will not open it");
    s.mc_getCameraEntity = jni::method(e, s.minecraft, mth::kClient_getCameraEntity, "()Lnet/minecraft/class_1297;"); // getCameraEntity
    s.mc_getWindow       = jni::method(e, s.minecraft, mth::kClient_getWindow,       "()Lnet/minecraft/class_1041;"); // getWindow -> Window (1.21.11)

    // Crosshair hit path (class_239 family)
    if (jclass hitLocal = jni::findClass(e, "net.minecraft.class_239")) {
        s.hit_getType = e->GetMethodID(hitLocal, mth::kHit_getType, "()Lnet/minecraft/class_239$class_240;");
        s.hit_getPos  = e->GetMethodID(hitLocal, mth::kHit_getPos,  "()Lnet/minecraft/class_243;");
        s.hitType     = jni::findClass(e, "net.minecraft.class_239$class_240");
        e->DeleteLocalRef(hitLocal);
    }
    if (s.blockPos) {
        s.blockPos_getX = jni::method(e, s.blockPos, mth::kBlockPos_getX, "()I");
        s.blockPos_getY = jni::method(e, s.blockPos, mth::kBlockPos_getY, "()I");
        s.blockPos_getZ = jni::method(e, s.blockPos, mth::kBlockPos_getZ, "()I");
        s.blockPos_ctor = jni::method(e, s.blockPos, "<init>", "(III)V");
    }
    if (s.vec3d) s.vec3d_ctor = jni::method(e, s.vec3d, "<init>", "(DDD)V");
    if (s.blockHitResult) {
        s.blockHit_getBlockPos = jni::method(e, s.blockHitResult, mth::kBlockHit_getBlockPos,
                                             "()Lnet/minecraft/class_2338;");
        s.blockHit_getSide = jni::method(e, s.blockHitResult, mth::kBlockHit_getSide,
                                         "()Lnet/minecraft/class_2350;");
        s.blockHit_ctor = jni::method(e, s.blockHitResult, "<init>",
            "(Lnet/minecraft/class_243;Lnet/minecraft/class_2350;Lnet/minecraft/class_2338;Z)V");
    }
    if (s.actionResult)
        s.actionResult_isAccepted = jni::method(e, s.actionResult, mth::kActionResult_isAccepted, "()Z");
    jni::clearException(e, "hitresult");
    if (jclass entHitLocal = jni::findClass(e, "net.minecraft.class_3966")) {
        s.entHit_getEntity = e->GetMethodID(entHitLocal, mth::kEntHit_getEntity, "()Lnet/minecraft/class_1297;");
        s.entityHit = makeGlobal(e, entHitLocal);
    }
    jni::clearException(e, "entityhitresult");
    if (s.entity) s.ent_type = jni::method(e, s.entity, mth::kEnt_type, "()Lnet/minecraft/class_1299;");
    // Enum.name() is final on java.lang.Enum — resolve against its class.
    if (jclass enumLocal = jni::findClass(e, "java.lang.Enum")) {
        s.enum_name = e->GetMethodID(enumLocal, mth::kEnum_name, "()Ljava/lang/String;");
        e->DeleteLocalRef(enumLocal);
    }
    jni::clearException(e, "enum name");

    // GameRenderer -> Camera (pinned intermediary id, verified)
    if (jclass grLocal = jni::findClass(e, "net.minecraft.class_757")) {
        s.gr_getCamera = e->GetMethodID(grLocal, mth::kGR_getCamera, "()Lnet/minecraft/class_4184;");
        s.gr_getFov = e->GetMethodID(grLocal, mth::kGR_getFov,
            "(Lnet/minecraft/class_4184;FZ)F");
        e->DeleteLocalRef(grLocal);
    }
    jni::clearException(e, "gameRenderer");

    // ---- Entity -------------------------------------------------------------
    s.ent_getX        = jni::method(e, s.entity, mth::kEnt_getX,        "()D");                    // getX
    s.ent_getY        = jni::method(e, s.entity, mth::kEnt_getY,        "()D");                    // getY
    s.ent_getZ        = jni::method(e, s.entity, mth::kEnt_getZ,        "()D");                    // getZ
    s.ent_getLerpedPos= jni::method(e, s.entity, mth::kEnt_getLerpedPos,"(F)Lnet/minecraft/class_243;");
    s.ent_getEyePos   = jni::method(e, s.entity, mth::kEnt_getEyePos,   "()Lnet/minecraft/class_243;"); // getEyePos
    s.ent_getYaw      = jni::method(e, s.entity, mth::kEnt_getYaw,      "()F");                    // getYaw
    s.ent_getPitch    = jni::method(e, s.entity, mth::kEnt_getPitch,    "()F");                    // getPitch
    s.ent_setYaw      = jni::method(e, s.entity, mth::kEnt_setYaw,      "(F)V");                   // setYaw
    s.ent_setPitch    = jni::method(e, s.entity, mth::kEnt_setPitch,    "(F)V");                   // setPitch
    s.ent_isAlive     = jni::method(e, s.entity, mth::kEnt_isAlive,     "()Z");                    // isAlive
    s.ent_isSprinting = jni::method(e, s.entity, mth::kEnt_isSprinting, "()Z");                    // isSprinting
    s.ent_setSprinting= jni::method(e, s.entity, mth::kEnt_setSprinting, "(Z)V");                   // setSprinting
    s.ent_getWidth    = jni::method(e, s.entity, mth::kEnt_getWidth,    "()F");                    // getWidth
    s.ent_getHeight   = jni::method(e, s.entity, mth::kEnt_getHeight,   "()F");                    // getHeight
    s.ent_getUuidStr  = jni::method(e, s.entity, mth::kEnt_getUuidStr,  "()Ljava/lang/String;");   // getUuidAsString
    s.ent_getVelocity = jni::method(e, s.entity, mth::kEnt_getVelocity, "()Lnet/minecraft/class_243;"); // getVelocity
    s.ent_setVelocity = jni::method(e, s.entity, mth::kEnt_setVelocity, "(Lnet/minecraft/class_243;)V"); // setVelocity
    s.ent_getStandEye = jni::method(e, s.entity, mth::kEnt_getStandEye, "()F");                    // getStandingEyeHeight
    s.ent_pos         = jni::field(e, s.entity, fld::kEnt_pos,      "Lnet/minecraft/class_243;");
    s.ent_prevX       = jni::field(e, s.entity, fld::kEnt_prevX,    "D");
    s.ent_prevY       = jni::field(e, s.entity, fld::kEnt_prevY,    "D");
    s.ent_prevZ       = jni::field(e, s.entity, fld::kEnt_prevZ,    "D");
    s.ent_yaw         = jni::field(e, s.entity, fld::kEnt_yaw,      "F");
    s.ent_pitch       = jni::field(e, s.entity, fld::kEnt_pitch,    "F");
    s.ent_lastYaw     = jni::field(e, s.entity, fld::kEnt_lastYaw,  "F");
    s.ent_lastPitch   = jni::field(e, s.entity, fld::kEnt_lastPitch,"F");
    s.ent_age         = jni::field(e, s.entity, fld::kEnt_age,      "I");
    // The rotation vanilla last put in a movement packet. Manual look packets write this too (see
    // ext::sendMove) so the game never repeats the same rotation on the tick after one.
    if (s.clientPlayer) {
        s.cli_lastYaw   = jni::field(e, s.clientPlayer, fld::kCli_lastYaw,   "F");
        s.cli_lastPitch = jni::field(e, s.clientPlayer, fld::kCli_lastPitch, "F");
    }
    // Entity.fallDistance is a double in this tree and a float in older ones, and `jni::field`
    // answers null when the descriptor does not match - which used to leave this at null and make
    // fallDistance() read 0 forever. That silently disabled every fall-based module: AutoMace never
    // armed and Breach Swap never swapped. Whichever descriptor resolves is the one the game has,
    // and it is read with the matching accessor.
    s.ent_fallDist    = jni::field(e, s.entity, fld::kEnt_fallDist, "D");
    if (!s.ent_fallDist) {
        s.ent_fallDist = jni::field(e, s.entity, fld::kEnt_fallDist, "F");
        s.ent_fallDistIsFloat = (s.ent_fallDist != nullptr);
    }
    trace::log("Entity.fallDistance resolved as %s",
               s.ent_fallDist ? (s.ent_fallDistIsFloat ? "float" : "double") : "MISSING");
    s.ent_onGround    = jni::field(e, s.entity, fld::kEnt_onGround, "Z");
    s.ent_getName     = jni::method(e, s.entity, mth::kEnt_getName, "()Lnet/minecraft/class_2561;"); // displayName
    // Shader ESP drives the game's own glowing outline through this flag.
    s.ent_setGlowing  = jni::method(e, s.entity, mth::kEnt_setGlowing, "(Z)V");   // setGlowing
    s.ent_isGlowing   = jni::method(e, s.entity, mth::kEnt_isGlowing,  "()Z");    // isGlowing (client flag)
    trace::log("Entity glow resolved: setGlowing=%s isGlowing=%s",
               s.ent_setGlowing ? "ok" : "MISSING", s.ent_isGlowing ? "ok" : "MISSING");
    s.ent_bbox        = jni::field(e, s.entity, fld::kEnt_bbox,     "Lnet/minecraft/class_238;");
    s.box_minX        = jni::field(e, s.box, fld::kBox_minX, "D");
    s.box_minY        = jni::field(e, s.box, fld::kBox_minY, "D");
    s.box_minZ        = jni::field(e, s.box, fld::kBox_minZ, "D");
    s.box_maxX        = jni::field(e, s.box, fld::kBox_maxX, "D");
    s.box_maxY        = jni::field(e, s.box, fld::kBox_maxY, "D");
    s.box_maxZ        = jni::field(e, s.box, fld::kBox_maxZ, "D");

    // ---- LivingEntity --------------------------------------------------------
    s.liv_getHealth    = jni::method(e, s.living, mth::kLiv_getHealth,    "()F");                  // getHealth
    s.liv_getMaxHealth = jni::method(e, s.living, mth::kLiv_getMaxHealth, "()F");                  // getMaxHealth
    s.liv_getAbsorption = jni::method(e, s.living, mth::kLiv_getAbsorption, "()F");                // getAbsorptionAmount
    // NOTE: LivingEntity.tryAttack no longer used — the client attack path is
    // interactionManager.attackEntity(player, target) (see Client::attackEntity).
    s.liv_isBlocking   = jni::method(e, s.living, mth::kLiv_isBlocking,   "()Z");                  // isBlocking
    s.liv_isUsingItem  = jni::method(e, s.living, mth::kLiv_isUsingItem,  "()Z");                  // isUsingItem
    s.liv_getActiveItem= jni::method(e, s.living, mth::kLiv_getActiveItem,"()Lnet/minecraft/class_1799;"); // getActiveItem
    s.liv_getMainHand  = jni::method(e, s.living, mth::kLiv_getMainHand,  "()Lnet/minecraft/class_1799;"); // getMainHandStack
    s.liv_getOffHand   = jni::method(e, s.living, mth::kLiv_getOffHand,   "()Lnet/minecraft/class_1799;"); // getOffHandStack
    s.liv_getEquip     = jni::method(e, s.living, mth::kLiv_getEquip,     "(Lnet/minecraft/class_1304;)Lnet/minecraft/class_1799;"); // getEquippedStack
    s.liv_isGliding    = jni::method(e, s.living, mth::kLiv_isGliding,    "()Z");                  // isGliding
    s.liv_lerpYaw      = jni::method(e, s.living, mth::kLiv_lerpYaw,      "(F)F");                 // lerpYaw (render-facing read)
    s.liv_getStatusEffects = jni::method(e, s.living, mth::kLiv_getStatusEffects,
                                         "()Ljava/util/Collection;");                            // getStatusEffects
    s.liv_getArmor     = jni::method(e, s.living, mth::kLiv_getArmor,     "()I");                    // getArmor (armour points)
    s.liv_swingHand    = jni::method(e, s.living, mth::kLiv_swingHand,    "(Lnet/minecraft/class_1268;)V"); // swingHand(Hand)
    s.liv_hurtTime     = jni::field(e, s.living, fld::kLiv_hurtTime,    "I");
    s.liv_maxHurtTime  = jni::field(e, s.living, fld::kLiv_maxHurtTime, "I");
    s.liv_deathTime    = jni::field(e, s.living, fld::kLiv_deathTime,   "I");
    s.liv_swinging     = jni::field(e, s.living, fld::kLiv_handSwing,   "Z");
    s.liv_bodyYaw      = jni::field(e, s.living, fld::kLiv_bodyYaw,     "F");
    s.liv_headYaw      = jni::field(e, s.living, fld::kLiv_headYaw,     "F");
    s.liv_lastBodyYaw  = jni::field(e, s.living, fld::kLiv_lastBodyYaw, "F");
    s.liv_lastHeadYaw  = jni::field(e, s.living, fld::kLiv_lastHeadYaw, "F");

    // ---- Player ---------------------------------------------------------------
    s.pl_getInventory  = jni::method(e, s.player, mth::kPl_getInventory,   "()Lnet/minecraft/class_1661;");       // getInventory
    s.pl_getAttackCd   = jni::method(e, s.player, mth::kPl_getAttackCd,    "(F)F");                               // getAttackCooldownProgress
    // NOTE: pl_getGameProfile intentionally NOT resolved — authlib 7.x turned
    // GameProfile into a record and our old getName() call on it was the
    // mob-death crash. Names now come from Entity.getName() (displayName()).
    s.pl_isCreative    = jni::method(e, s.player, mth::kPl_isCreative,     "()Z");                                // isCreative
    s.pl_abilities     = jni::field(e, s.player, fld::kPl_abilities,       "Lnet/minecraft/class_1656;");  // PlayerAbilities (verified)
    s.pl_playerScreenHandler = jni::field(e, s.player, fld::kPl_playerScreenHandler,
                                          "Lnet/minecraft/class_1723;");           // playerScreenHandler (PlayerScreenHandler)
    s.pl_currentScreenHandler = jni::field(e, s.player, fld::kPl_currentScreenHandler,
                                           "Lnet/minecraft/class_1703;");          // currentScreenHandler
    s.sh_syncId        = jni::field(e, s.screenHandler, fld::kSH_syncId, "I");       // ScreenHandler.syncId
    s.pl_itemCooldownMgr = jni::field(e, s.player, fld::kPl_itemCooldownMgr, "Lnet/minecraft/class_1796;"); // itemCooldownManager (intermediary desc!)
    // ---- hunger / experience (custom HUD bars) ----
    s.pl_getHunger        = jni::method(e, s.player, mth::kPl_getHunger, "()Lnet/minecraft/class_1702;"); // getHungerManager
    s.pl_hungerManager    = jni::field(e, s.player, fld::kPl_hungerManager, "Lnet/minecraft/class_1702;"); // hungerManager
    s.pl_experienceLevel  = jni::field(e, s.player, fld::kPl_experienceLevel, "I");                       // experienceLevel
    s.pl_experienceProgress = jni::field(e, s.player, fld::kPl_experienceProgress, "F");                   // experienceProgress
    s.hungerManager       = makeGlobal(e, jni::findClass(e, mcids::kHungerManager));
    if (s.hungerManager)
        s.hunger_getFoodLevel = jni::method(e, s.hungerManager, mth::kHunger_getFoodLevel, "()I");        // getFoodLevel
    s.cd_isCoolingDown = jni::method(e, s.itemCooldownMgr, mth::kCd_isCoolingDown, "(Lnet/minecraft/class_1799;)Z"); // isCoolingDown(stack)

    // ---- PlayerInventory -------------------------------------------------------
    s.inv_selectedSlot  = jni::field(e, s.inventory, fld::kInv_selected,    "I");
    s.inv_main          = jni::field(e, s.inventory, fld::kInv_main,       "Lnet/minecraft/class_2371;");
    s.inv_swapHotbar    = jni::method(e, s.inventory, mth::kInv_swapHotbar,    "(I)V");                   // swapSlotWithHotbar
    s.inv_getSelected   = jni::method(e, s.inventory, mth::kInv_getSelected,   "()Lnet/minecraft/class_1799;"); // getSelectedStack
    s.inv_getMainStacks = jni::method(e, s.inventory, mth::kInv_getMainStacks, "()Lnet/minecraft/class_2371;"); // getMainStacks
    s.inv_setSelected   = jni::method(e, s.inventory, mth::kInv_setSelected,   "(I)V");                   // setSelectedSlot (number-key swap)

    // ---- ItemStack ---------------------------------------------------------------
    s.stk_isEmpty      = jni::method(e, s.itemStack, mth::kStk_isEmpty,      "()Z");                          // isEmpty
    s.stk_getCount     = jni::method(e, s.itemStack, mth::kStk_getCount,     "()I");                          // getCount
    s.stk_getItem      = jni::method(e, s.itemStack, mth::kStk_getItem,      "()Lnet/minecraft/class_1792;"); // getItem
    s.stk_getDamage    = jni::method(e, s.itemStack, mth::kStk_getDamage,    "()I");                          // getDamage
    s.stk_getMaxDamage = jni::method(e, s.itemStack, mth::kStk_getMaxDamage, "()I");                          // getMaxDamage
    s.stk_isDamageable = jni::method(e, s.itemStack, mth::kStk_isDamageable, "()Z");                          // isDamageable
    s.stk_isOf         = jni::method(e, s.itemStack, mth::kStk_isOf,         "(Lnet/minecraft/class_1792;)Z"); // isOf
    s.stk_getName      = jni::method(e, s.itemStack, mth::kStk_getName,      "()Lnet/minecraft/class_2561;"); // getName
    s.stk_getEnch      = jni::method(e, s.itemStack, mth::kStk_getEnch,      "()Lnet/minecraft/class_9304;"); // getEnchantments
    s.ice_entrySet     = jni::method(e, s.itemEnchantments, mth::kIce_entrySet, "()Ljava/util/Set;");            // entrySet
    s.holder_matchesId = jni::method(e, s.registryEntry, mth::kHolder_matchesId, "(Lnet/minecraft/class_2960;)Z"); // matchesId

    // ---- Text -----------------------------------------------------------------
    s.txt_getLiteralString = jni::method(e, s.text, mth::kTxt_getLiteralString, "()Ljava/lang/String;"); // getLiteralString
    s.txt_content          = jni::method(e, s.text, mth::kTxt_content,          "()Lnet/minecraft/class_7417;"); // getContent

    // ---- Camera ------------------------------------------------------------------
    s.cam_getFocused    = jni::method(e, s.camera, mth::kCam_getFocused,    "()Lnet/minecraft/class_1297;"); // getFocusedEntity
    s.cam_getThirdPerson= jni::method(e, s.camera, mth::kCam_getThirdPerson, "()Z");                          // isThirdPerson
    s.cam_getYaw        = jni::method(e, s.camera, mth::kCam_getYaw,        "()F");                          // getYaw
    s.cam_getHorizontalPlane = jni::method(e, s.camera, "method_19335", "()Lorg/joml/Vector3fc;");
    s.cam_getVerticalPlane   = jni::method(e, s.camera, "method_19336", "()Lorg/joml/Vector3fc;");
    s.cam_getDiagonalPlane   = jni::method(e, s.camera, "method_35689", "()Lorg/joml/Vector3fc;");
    s.cam_getPitch      = jni::method(e, s.camera, mth::kCam_getPitch,      "()F");                          // getPitch
    s.cam_pos           = jni::field(e, s.camera, fld::kCam_pos,       "Lnet/minecraft/class_243;");
    s.cam_thirdPerson   = jni::field(e, s.camera, fld::kCam_thirdPerson, "Z");

    // ---- Options --------------------------------------------------------------------
    s.opt_gamma = jni::field(e, s.gameOptions, fld::kOpt_gamma, "Lnet/minecraft/class_7172;");
    s.opt_fov   = jni::field(e, s.gameOptions, fld::kOpt_fov,   "Lnet/minecraft/class_7172;");
    s.opt_hudHidden = jni::field(e, s.gameOptions, fld::kOpt_hudHidden, "Z"); // GameOptions.hudHidden
    s.so_value  = jni::field(e, s.simpleOption, fld::kSO_value, "Ljava/lang/Object;"); // SimpleOption.value
    s.opt_getValue = jni::method(e, s.simpleOption, mth::kOpt_getValue, "()Ljava/lang/Object;"); // getValue (verified)
    s.opt_setValue = jni::method(e, s.simpleOption, mth::kOpt_setValue, "(Ljava/lang/Object;)V"); // setValue

    // ---- InteractionManager ------------------------------------------------------------
    s.im_attackEntity = jni::method(e, s.interactionManager, mth::kIm_attackEntity, "(Lnet/minecraft/class_1657;Lnet/minecraft/class_1297;)V"); // attackEntity
    s.im_interactBlock = jni::method(e, s.interactionManager, mth::kIm_interactBlock, "(Lnet/minecraft/class_746;Lnet/minecraft/class_1268;Lnet/minecraft/class_3965;)Lnet/minecraft/class_1269;"); // interactBlock (ClientPlayerEntity, Hand, BlockHitResult) - verified
    s.im_interactItem = jni::method(e, s.interactionManager, mth::kIm_interactItem, "(Lnet/minecraft/class_1657;Lnet/minecraft/class_1268;)Lnet/minecraft/class_1269;"); // interactItem
    s.im_getGameMode  = jni::method(e, s.interactionManager, mth::kIm_getGameMode,  "()Lnet/minecraft/class_1934;");                          // getCurrentGameMode
    s.im_syncSelectedSlot = jni::method(e, s.interactionManager,
        mth::kIm_syncSelectedSlot, "()V");
    s.im_clickSlot = jni::method(e, s.interactionManager, mth::kIm_clickSlot,
        "(IIILnet/minecraft/class_1713;Lnet/minecraft/class_1657;)V"); // clickSlot

    // ---- ranges / fluid raycast / misc (all verified against the 1.21.11 mappings) ----
    s.pl_getBlockRange  = jni::method(e, s.player, mth::kPl_getBlockRange,  "()D");
    s.pl_getEntityRange = jni::method(e, s.player, mth::kPl_getEntityRange, "()D");
    s.ent_raycast       = jni::method(e, s.entity, mth::kEnt_raycast,       "(DFZ)Lnet/minecraft/class_239;");
    s.mc_attackCooldown = jni::field(e, s.minecraft, fld::kClient_attackCooldown, "I");
    if (jclass objLocal = e->FindClass("java/lang/Object")) {
        s.obj_toString = e->GetMethodID(objLocal, "toString", "()Ljava/lang/String;");
        e->DeleteLocalRef(objLocal);
    }
    jni::clearException(e, "misc ids");

    // ---- World / iteration ---------------------------------------------------------------
    s.world_getEntities = jni::method(e, s.clientWorld, mth::kWorld_getEntities, "()Ljava/lang/Iterable;"); // getEntities (verified)
    s.world_getBlockState = jni::method(e, s.world, mth::kWorld_getBlockState,
        "(Lnet/minecraft/class_2338;)Lnet/minecraft/class_2680;");
    s.blockState_getBlock = jni::method(e, s.blockState, mth::kBlockState_getBlock,
        "()Lnet/minecraft/class_2248;");
    jclass itLocal = jni::findClass(e, mcids::kIterator);
    if (itLocal) {
        s.it_hasNext = jni::method(e, itLocal, "hasNext", "()Z");
        s.it_next    = jni::method(e, itLocal, "next",    "()Ljava/lang/Object;");
        e->DeleteLocalRef(itLocal);
    }

    // ---- misc -----------------------------------------------------------------
    s.id_getPath       = jni::method(e, s.identifier, mth::kIdentifier_getPath, "()Ljava/lang/String;"); // getPath (verified)
    s.id_of            = jni::smethod(e, s.identifier, mth::kId_of, "(Ljava/lang/String;Ljava/lang/String;)Lnet/minecraft/class_2960;"); // static Identifier.of (1.21.2+; ctor private!)
    if (!s.id_of) jni::clearException(e, "id_of.resolve");
    if (s.registries) {
        if (jfieldID itemRegFid = jni::sfield(e, s.registries, fld::kReg_ITEM, "Lnet/minecraft/class_7922;")) {  // 1.21.11 declared type: DefaultedRegistry
            LocalRef itemReg(jni::sgetObj(e, s.registries, itemRegFid));
            if (itemReg) {
                s.reg_getId = e->GetMethodID(e->GetObjectClass(itemReg.get()), mth::kRegistry_getId,
                                             "(Ljava/lang/Object;)Lnet/minecraft/class_2960;");
            }
        }
        jni::clearException(e, "registries.ITEM");
    }
    s.dl_get           = jni::method(e, s.defaultedList, "get",  "(I)Ljava/lang/Object;");
    s.dl_size          = jni::method(e, s.defaultedList, "size", "()I");
    s.chat_addToHistory= jni::method(e, s.chatHud, mth::kChat_addToHistory, "(Ljava/lang/String;)V"); // addToMessageHistory (verified)
    s.ses_username     = jni::field(e, s.session, fld::kSes_username, "Ljava/lang/String;");

    // ---- enum constants -----------------------------------------------------------
    const char* eqSig = "Lnet/minecraft/class_1304;";
    s.eqMainhand = makeGlobalObj(e, enumStatic(e, s.equipmentSlot, fld::kEq_MAINHAND, eqSig));
    s.eqOffhand  = makeGlobalObj(e, enumStatic(e, s.equipmentSlot, fld::kEq_OFFHAND,  eqSig));
    s.eqFeet     = makeGlobalObj(e, enumStatic(e, s.equipmentSlot, fld::kEq_FEET,     eqSig));
    s.eqLegs     = makeGlobalObj(e, enumStatic(e, s.equipmentSlot, fld::kEq_LEGS,     eqSig));
    s.eqChest    = makeGlobalObj(e, enumStatic(e, s.equipmentSlot, fld::kEq_CHEST,    eqSig));
    s.eqHead     = makeGlobalObj(e, enumStatic(e, s.equipmentSlot, fld::kEq_HEAD,     eqSig));

    const char* satSig = "Lnet/minecraft/class_1713;";
    s.satSwap   = makeGlobalObj(e, enumStatic(e, s.slotActionType, fld::kSAT_SWAP,   satSig));
    s.satPickup = makeGlobalObj(e, enumStatic(e, s.slotActionType, fld::kSAT_PICKUP, satSig));
    s.satThrow  = makeGlobalObj(e, enumStatic(e, s.slotActionType, fld::kSAT_THROW,  satSig));

    const char* dirSig = "Lnet/minecraft/class_2350;";
    s.dirDown  = makeGlobalObj(e, enumStatic(e, s.direction, fld::kDir_DOWN,  dirSig));
    s.dirUp    = makeGlobalObj(e, enumStatic(e, s.direction, fld::kDir_UP,    dirSig));
    s.dirNorth = makeGlobalObj(e, enumStatic(e, s.direction, fld::kDir_NORTH, dirSig));
    s.dirSouth = makeGlobalObj(e, enumStatic(e, s.direction, fld::kDir_SOUTH, dirSig));
    s.dirWest  = makeGlobalObj(e, enumStatic(e, s.direction, fld::kDir_WEST,  dirSig));
    s.dirEast  = makeGlobalObj(e, enumStatic(e, s.direction, fld::kDir_EAST,  dirSig));

    // Silent aim is exactly one packet, so the rotation packet is resolved together with everything
    // else and reported here: "MISSING" means nothing ever reaches the server and the aim stays
    // local. It uses the raw JNI calls, so it cannot disturb the failed-lookup count below.
    // The probe is run first and stored: the two calls cannot be arguments of the same call, or
    // MSVC's right-to-left evaluation would read the class name before it is resolved.
    ext::probeMovePackets();
    trace::log("move packets: %s", ext::movePacketsStatus());

    jni::clearException(e, "resolveAll");
    g_resolved = true;
    s.ok = true;
    trace::log("resolve complete: %d failed lookups (must be 0)", jni::failedLookups());
    return true;
}

// ---------------------------------------------------------------------------
// Client
// ---------------------------------------------------------------------------
namespace {
jobject g_client = nullptr;   // per-frame global ref (deleted in endFrame)
jobject g_player = nullptr;
jobject g_world  = nullptr;
jobject g_opts   = nullptr;
jobject g_inter  = nullptr;
jobject g_sess   = nullptr;
jobject g_cam    = nullptr;
jobject g_mouse  = nullptr;
}

namespace frame {
void beginFrame() {
    JNIEnv* e = jni::env();
    if (!e || !g_resolved) return;
    Symbols& s = g_sym;

    if (s.mc_getInstance) {
        jobject c = e->CallStaticObjectMethod(s.minecraft, s.mc_getInstance);
        jni::clearException(e, "getInstance");
        if (g_client) e->DeleteGlobalRef(g_client);
        g_client = c ? e->NewGlobalRef(c) : nullptr;
        if (c) e->DeleteLocalRef(c);
    }
    if (!g_client) return;

    auto grab = [&](jfieldID fid) -> jobject {
        if (!fid) return nullptr;
        jobject l = e->GetObjectField(g_client, fid);
        jobject g = l ? e->NewGlobalRef(l) : nullptr;
        if (l) e->DeleteLocalRef(l);
        return g;
    };
    auto rel = [&e](jobject& g) { if (g && e) { e->DeleteGlobalRef(g); g = nullptr; } };

    rel(g_player); g_player = grab(s.mc_player);
    rel(g_world);  g_world  = grab(s.mc_world);
    rel(g_opts);   g_opts   = grab(s.mc_options);
    rel(g_inter);  g_inter  = grab(s.mc_interaction);
    rel(g_sess);   g_sess   = grab(s.mc_session);
    rel(g_mouse);  g_mouse  = grab(s.mc_mouse);
    rel(g_cam);
    if (s.mc_gameRenderer) {
        jobject gr = grab(s.mc_gameRenderer);
        if (gr && s.gr_getCamera) {
            jobject cl = e->CallObjectMethod(gr, s.gr_getCamera);
            jni::clearException(e, "getCamera");
            g_cam = cl ? e->NewGlobalRef(cl) : nullptr;
            if (cl) e->DeleteLocalRef(cl);
        }
        if (gr) e->DeleteGlobalRef(gr);
    }
}
void endFrame() {
    JNIEnv* e = jni::env();
    if (!e) return;
    auto rel = [&](jobject& g) { if (g) { e->DeleteGlobalRef(g); g = nullptr; } };
    rel(g_cam); rel(g_mouse); rel(g_sess); rel(g_inter); rel(g_opts); rel(g_world); rel(g_player); rel(g_client);
}
bool valid() { return g_client != nullptr; }
jobject client() { return g_client; }
bool resolved() { return g_resolved; }
} // namespace frame

bool Client::acquire()            { return frame::valid(); }
jobject Client::handle()          { return frame::client(); }
jobject Client::player()          { return g_player; }
jobject Client::world()           { return g_world; }
jobject Client::options()         { return g_opts; }
jobject Client::interaction()     { return g_inter; }
jobject Client::session()         { return g_sess; }
jobject Client::camera()          { return g_cam; }
jobject Client::mouse()           { return g_mouse; }
bool Client::screenOpen() {
    JNIEnv* e = jni::env();
    if (!e || !g_client || !g_sym.mc_currentScreen) return false;
    jobject screen = e->GetObjectField(g_client, g_sym.mc_currentScreen);
    jni::clearException(e, "currentScreen");
    if (!screen) return false;
    e->DeleteLocalRef(screen);
    return true;
}

// The vanilla inventory key's own path, without touching the keyboard: the game builds the
// screen and installs it. Rebind-safe, and it cannot fire somebody else's binding.
bool Client::openInventoryScreen() {
    JNIEnv* e = jni::env();
    if (!e || !g_client || !g_player) return false;
    if (screenOpen()) return true;          // never take over a screen the player opened
    if (!g_sym.mc_setScreen || !g_sym.inventoryScreen || !g_sym.invScreen_ctor) return false;
    // The class name is a pinned mapping guess, so check it really is a Screen before using it:
    // a wrong name then answers false instead of installing something that is not a screen.
    if (g_sym.screen &&
        e->IsAssignableFrom(g_sym.inventoryScreen, g_sym.screen) != JNI_TRUE) {
        jni::clearException(e, "inventory screen type check");
        static bool warned = false;
        if (!warned) {
            warned = true;
            trace::log("inventory screen class did not check out - the inventory will not open");
        }
        return false;
    }
    jobject screen = e->NewObject(g_sym.inventoryScreen, g_sym.invScreen_ctor, g_player);
    if (e->ExceptionCheck()) { jni::clearException(e, "InventoryScreen ctor"); return false; }
    if (!screen) return false;
    e->CallVoidMethod(g_client, g_sym.mc_setScreen, screen);
    const bool ok = !e->ExceptionCheck();
    jni::clearException(e, "setScreen(inventory)");
    e->DeleteLocalRef(screen);
    return ok;
}

bool Client::closeScreen() {
    JNIEnv* e = jni::env();
    if (!e || !g_client || !g_sym.mc_setScreen) return false;
    if (!screenOpen()) return true;
    // setScreen(null) is the vanilla close path - the screen's removed() hook sends the
    // close-screen packet for us, so nothing is sent from here.
    e->CallVoidMethod(g_client, g_sym.mc_setScreen, (jobject)nullptr);
    const bool ok = !e->ExceptionCheck();
    jni::clearException(e, "setScreen(null)");
    return ok;
}

int Client::itemUseCooldown() {
    JNIEnv* e = jni::env();
    if (!e || !g_client || !g_sym.mc_itemUseCd) return 0;
    return e->GetIntField(g_client, g_sym.mc_itemUseCd);
}
void Client::setItemUseCooldown(int v) {
    JNIEnv* e = jni::env();
    if (!e || !g_client || !g_sym.mc_itemUseCd) return;
    e->SetIntField(g_client, g_sym.mc_itemUseCd, (jint)v);
}
int Client::currentFps() {
    JNIEnv* e = jni::env();
    if (!e || !g_client || !g_sym.mc_currentFps) return 0;
    return e->GetStaticIntField(g_sym.minecraft, g_sym.mc_currentFps); // static field
}
float Client::renderTickDelta() {
    JNIEnv* e = jni::env();
    if (!e || !g_client || !g_sym.mc_tickCounter || !g_sym.tickCounter_getProgress)
        return -1.f;
    LocalRef counter(e->GetObjectField(g_client, g_sym.mc_tickCounter));
    jni::clearException(e, "renderTickCounter");
    if (!counter) return -1.f;
    const jfloat delta = e->CallFloatMethod(counter.get(), g_sym.tickCounter_getProgress,
                                             JNI_FALSE);
    if (e->ExceptionCheck()) { jni::clearException(e, "getTickProgress"); return -1.f; }
    return std::isfinite(delta) ? std::clamp((float)delta, 0.f, 1.f) : -1.f;
}
float Client::renderFov(float tickDelta, bool changedFov) {
    JNIEnv* e = jni::env();
    if (!e || !g_client || !g_cam || !g_sym.mc_gameRenderer || !g_sym.gr_getFov)
        return -1.f;
    LocalRef renderer(e->GetObjectField(g_client, g_sym.mc_gameRenderer));
    jni::clearException(e, "gameRenderer for FOV");
    if (!renderer) return -1.f;
    const jfloat fov=e->CallFloatMethod(renderer.get(),g_sym.gr_getFov,g_cam,
                                         (jfloat)tickDelta, changedFov ? JNI_TRUE : JNI_FALSE);
    if (e->ExceptionCheck()) { jni::clearException(e,"GameRenderer.getFov"); return -1.f; }
    return std::isfinite(fov) && fov>1.f && fov<179.f ? (float)fov : -1.f;
}

static bool actionAccepted(JNIEnv* e, jobject result, const char* context) {
    if (!e) return false;
    if (!result || !g_sym.actionResult_isAccepted) {
        jni::clearException(e, context);
        return false;
    }
    const jboolean accepted = e->CallBooleanMethod(result, g_sym.actionResult_isAccepted);
    const bool ok = !e->ExceptionCheck() && accepted == JNI_TRUE;
    jni::clearException(e, context);
    return ok;
}

bool Client::interactAtCrosshair() {
    JNIEnv* e = jni::env();
    if (!e || !g_inter || !g_player || !g_sym.hand || !g_sym.im_interactBlock) return false;
    LocalRef hit(crosshair());
    if (!hit) return false;

    // Only interact when the current crosshair is actually a block hit. Passing
    // an entity hit or a stale/invalid hit result into the BlockHitResult-based
    // interaction path causes the Java-side placement flow to dereference the wrong
    // object graph and can trigger the crash signature seen in the report.
    if (mc::hit::typeOf(hit.get()) != 1) return false;

    jobject handMain = handEnum(e, false);
    if (!handMain) return false;

    jobject result = e->CallObjectMethod(g_inter, g_sym.im_interactBlock, g_player, handMain, hit.get());
    const bool accepted = actionAccepted(e, result, "interactAtCrosshair");
    if (result) e->DeleteLocalRef(result);
    return accepted;
}

bool Client::interactBlockAt(int blockX, int blockY, int blockZ,
                             double hitX, double hitY, double hitZ, bool offhand) {
    return interactBlockAtFace(blockX, blockY, blockZ, hitX, hitY, hitZ, 1, offhand);
}

bool Client::interactBlockAtFace(int blockX, int blockY, int blockZ,
                                 double hitX, double hitY, double hitZ,
                                 int face, bool offhand) {
    JNIEnv* e = jni::env();
    if (!e || !g_inter || !g_player || !g_sym.hand || !g_sym.im_interactBlock ||
        !g_sym.blockPos || !g_sym.blockHitResult || !g_sym.vec3d || !g_sym.dirUp ||
        !g_sym.blockPos_ctor || !g_sym.vec3d_ctor || !g_sym.blockHit_ctor)
        return false;

    // face: 0=DOWN 1=UP 2=NORTH 3=SOUTH 4=WEST 5=EAST (Direction ordinal order)
    jobject direction = nullptr;
    switch (face < 0 || face > 5 ? 1 : face) {
        case 0:  direction = g_sym.dirDown;  break;
        case 1:  direction = g_sym.dirUp;    break;
        case 2:  direction = g_sym.dirNorth; break;
        case 3:  direction = g_sym.dirSouth; break;
        case 4:  direction = g_sym.dirWest;  break;
        default: direction = g_sym.dirEast;  break;
    }
    if (!direction) return false;

    LocalRef blockPos(e->NewObject(g_sym.blockPos, g_sym.blockPos_ctor,
                                   (jint)blockX, (jint)blockY, (jint)blockZ));
    jni::clearException(e, "BlockPos construction for interactBlockAtFace");
    if (!blockPos) return false;

    LocalRef hitPos(e->NewObject(g_sym.vec3d, g_sym.vec3d_ctor,
                                 (jdouble)hitX, (jdouble)hitY, (jdouble)hitZ));
    jni::clearException(e, "Vec3d construction for interactBlockAtFace");
    if (!hitPos) return false;

    LocalRef hit(e->NewObject(g_sym.blockHitResult, g_sym.blockHit_ctor,
                              hitPos.get(), direction, blockPos.get(), JNI_FALSE));
    jni::clearException(e, "BlockHitResult construction for interactBlockAtFace");
    if (!hit) return false;

    jobject hand = handEnum(e, offhand);
    if (!hand) return false;

    jobject result = e->CallObjectMethod(g_inter, g_sym.im_interactBlock,
                                         g_player, hand, hit.get());
    const bool ok = actionAccepted(e, result, "interactBlockAtFace");
    if (result) e->DeleteLocalRef(result);
    return ok;
}

std::string Client::objToString(jobject obj) {
    JNIEnv* e = jni::env();
    if (!e || !obj || !g_sym.obj_toString) return {};
    jstring text = (jstring)e->CallObjectMethod(obj, g_sym.obj_toString);
    jni::clearException(e, "objToString");
    if (!text) return {};
    const std::string out = jni::fromJString(e, text);
    e->DeleteLocalRef(text);
    return out;
}

bool Client::interactItem(bool offhand) {
    JNIEnv* e = jni::env();
    if (!e || !g_inter || !g_player || !g_sym.hand || !g_sym.im_interactItem) return false;

    jobject hand = handEnum(e, offhand);
    if (!hand) return false;

    jobject result = e->CallObjectMethod(g_inter, g_sym.im_interactItem, g_player, hand);
    const bool ok = actionAccepted(e, result, "interactItem");
    if (result) e->DeleteLocalRef(result);
    return ok;
}

bool Client::attackEntity(jobject target) {
    // Canonical client-side attack: ClientPlayerInteractionManager.attackEntity(player, target).
    // (LivingEntity.tryAttack gained a ServerWorld parameter in 1.21.x and is
    // server-authoritative anyway — this is the correct path on the client.)
    JNIEnv* e = jni::env();
    if (!e || !target || !g_sym.im_attackEntity) return false;
    if (!g_inter || !g_player) return false;
    e->CallVoidMethod(g_inter, g_sym.im_attackEntity, g_player, target);
    if (e->ExceptionCheck()) {
        jni::clearException(e, "attackEntity");
        return false;
    }
    jni::clearException(e, "attackEntity");
    return true;
}
void Client::syncSelectedSlot() {
    JNIEnv* e = jni::env();
    if (!e || !g_inter || !g_sym.im_syncSelectedSlot) return;
    e->CallVoidMethod(g_inter, g_sym.im_syncSelectedSlot);
    jni::clearException(e, "syncSelectedSlot");
}

// ---- Player inventory screen-handler clicks ---------------------------------
// Vanilla slot movement (number key / F key / Q key) all go through
// ClientPlayerInteractionManager.clickSlot(syncId, slot, button, action, player).
// Doing the same here keeps the server in sync instead of poking stacks directly.
int Client::playerScreenHandlerSyncId() {
    JNIEnv* e = jni::env();
    if (!e || !g_player || !g_sym.pl_playerScreenHandler || !g_sym.sh_syncId) return -1;
    LocalRef handler(e->GetObjectField(g_player, g_sym.pl_playerScreenHandler));
    jni::clearException(e, "playerScreenHandler");
    if (!handler) return -1;
    const jint syncId = e->GetIntField(handler.get(), g_sym.sh_syncId);
    jni::clearException(e, "syncId");
    return (int)syncId;
}

bool Client::playerScreenIsCurrent() {
    JNIEnv* e = jni::env();
    if (!e || !g_player || !g_sym.pl_playerScreenHandler || !g_sym.pl_currentScreenHandler) return false;
    LocalRef playerHandler(e->GetObjectField(g_player, g_sym.pl_playerScreenHandler));
    LocalRef current(e->GetObjectField(g_player, g_sym.pl_currentScreenHandler));
    jni::clearException(e, "screenHandlers");
    if (!playerHandler || !current) return false;
    const bool same = e->IsSameObject(playerHandler.get(), current.get()) == JNI_TRUE;
    jni::clearException(e, "screenHandlers.compare");
    return same;
}

int Client::playerScreenSlotForMainIndex(int index) {
    if (index < 0 || index > 35) return -1;
    return index < 9 ? 36 + index : index;
}

static bool clickPlayerSlot(int screenSlot, int button, jobject action) {
    JNIEnv* e = jni::env();
    if (!e || !g_inter || !g_player || !g_sym.im_clickSlot || !action) return false;
    const int syncId = Client::playerScreenHandlerSyncId();
    if (syncId < 0) return false;
    e->CallVoidMethod(g_inter, g_sym.im_clickSlot, (jint)syncId, (jint)screenSlot,
                      (jint)button, action, g_player);
    jni::clearException(e, "clickSlot");
    return true;
}

bool Client::swapOffhand(int screenSlot) {
    // button 40 selects the off-hand as the SWAP target (the F key's own packet).
    return clickPlayerSlot(screenSlot, 40, g_sym.satSwap);
}

bool Client::swapHotbarSlot(int screenSlot, int hotbarIndex) {
    if (hotbarIndex < 0 || hotbarIndex > 8) return false;
    return clickPlayerSlot(screenSlot, hotbarIndex, g_sym.satSwap);
}

bool Client::throwSlot(int screenSlot, bool wholeStack) {
    return clickPlayerSlot(screenSlot, wholeStack ? 1 : 0, g_sym.satThrow);
}

bool Client::pickupSlot(int screenSlot) {
    if (screenSlot < 0) return false;
    return clickPlayerSlot(screenSlot, 0, g_sym.satPickup);
}
std::string Client::blockIdAt(int x, int y, int z) {
    JNIEnv* e = jni::env();
    if (!e || !g_world || !g_sym.world_getBlockState || !g_sym.blockState_getBlock ||
        !g_sym.blockPos || !g_sym.blockPos_ctor || !g_sym.registries || !g_sym.identifier ||
        !g_sym.id_getPath)
        return {};

    LocalRef pos(e->NewObject(g_sym.blockPos, g_sym.blockPos_ctor,
                              (jint)x, (jint)y, (jint)z));
    jni::clearException(e, "respawn-anchor BlockPos");
    if (!pos) return {};
    LocalRef state(e->CallObjectMethod(g_world, g_sym.world_getBlockState, pos.get()));
    jni::clearException(e, "getBlockState");
    if (!state) return {};
    LocalRef block(e->CallObjectMethod(state.get(), g_sym.blockState_getBlock));
    jni::clearException(e, "BlockState.getBlock");
    if (!block) return {};

    static jfieldID blockRegistryField = nullptr;
    if (!blockRegistryField)
        blockRegistryField = jni::sfield(e, g_sym.registries, fld::kReg_BLOCK,
                                         "Lnet/minecraft/class_7922;");
    if (!blockRegistryField) return {};
    LocalRef registry(e->GetStaticObjectField(g_sym.registries, blockRegistryField));
    jni::clearException(e, "Registries.BLOCK");
    if (!registry) return {};
    jmethodID registryGetId = g_sym.reg_getId;
    if (!registryGetId) {
        jclass registryClass = e->GetObjectClass(registry.get());
        if (registryClass) {
            registryGetId = e->GetMethodID(registryClass, mth::kRegistry_getId,
                "(Ljava/lang/Object;)Lnet/minecraft/class_2960;");
            e->DeleteLocalRef(registryClass);
        }
        jni::clearException(e, "block registry getId resolve");
    }
    if (!registryGetId) return {};
    LocalRef id(e->CallObjectMethod(registry.get(), registryGetId, block.get()));
    jni::clearException(e, "block registry getId");
    if (!id) return {};
    jstring pathValue = (jstring)e->CallObjectMethod(id.get(), g_sym.id_getPath);
    jni::clearException(e, "block identifier getPath");
    const std::string path = jni::fromJString(e, pathValue);
    if (pathValue) e->DeleteLocalRef(pathValue);
    return path;
}
bool Client::isRespawnAnchorAt(int x, int y, int z) { return blockIdAt(x,y,z) == "respawn_anchor"; }
bool Client::isWaterAt(int x, int y, int z) { return blockIdAt(x,y,z) == "water"; }
bool Client::isPressurePlateAt(int x, int y, int z) {
    const std::string id=blockIdAt(x,y,z);
    return id.size()>=14 && id.compare(id.size()-14,14,"pressure_plate")==0;
}
bool Client::isCobwebAt(int x, int y, int z) { return blockIdAt(x,y,z) == "cobweb"; }
bool Client::setCrosshairTargetEntity(jobject entity) {
    JNIEnv* e = jni::env();
    if (!e || !g_resolved || !g_client) return false;
    static jfieldID crossField = nullptr;
    static bool probed = false;
    if (!probed) {
        probed = true;
        jclass clientClass = e->GetObjectClass(g_client);
        crossField = e->GetFieldID(clientClass, fld::kClient_crosshair, "Lnet/minecraft/class_239;");
        jni::clearException(e, "crosshairTarget (write)");
        e->DeleteLocalRef(clientClass);
    }
    if (!crossField) return false;
    // Keep the pick the game already made for this same entity: with the pick-level transform that
    // is the aim's own hit vector, and rebuilding it here would replace it with the entity's feet.
    // When the field does not hold this entity (the transform is unavailable, or its recompute came
    // back with the camera's block pick) the write below is what puts the target there, so the HUD
    // attack indicator draws and the block under the camera stops being outlined.
    if (entity) {
        LocalRef current(e->GetObjectField(g_client, crossField));
        jni::clearException(e, "crosshairTarget (read for write)");
        if (current && hit::typeOf(current.get()) == 2) {
            LocalRef held(hit::entity(current.get()));
            if (held && e->IsSameObject(held.get(), entity) == JNI_TRUE) return true;
        }
    }
    LocalRef built;
    if (entity) {
        jclass hitCls = jni::findClass(e, "net/minecraft/class_3966");   // EntityHitResult
        if (!hitCls) return false;
        const jmethodID ctor = e->GetMethodID(hitCls, "<init>",
            "(Lnet/minecraft/class_1297;Lnet/minecraft/class_243;)V");
        jni::clearException(e, "EntityHitResult ctor");
        if (ctor) {
            const Vec3 p = Entity(entity).pos();
            LocalRef vec(e->NewObject(g_sym.vec3d, g_sym.vec3d_ctor,
                                      (jdouble)p.x, (jdouble)p.y, (jdouble)p.z));
            jni::clearException(e, "EntityHitResult vec");
            if (vec) built = LocalRef(e->NewObject(hitCls, ctor, entity, vec.get()));
            jni::clearException(e, "EntityHitResult construction");
        }
        e->DeleteLocalRef(hitCls);
    }
    e->SetObjectField(g_client, crossField, built ? built.get() : nullptr);
    jni::clearException(e, "crosshairTarget set");
    // The HUD reads the target state from `targetedEntity`, which the game fills from the pick at the
    // end of its render. Writing it here as well means the indicator is on the aim's target in the
    // same frame the field is set, instead of one frame behind it.
    static jfieldID targetField = nullptr;
    static bool targetProbed = false;
    if (!targetProbed) {
        targetProbed = true;
        jclass clientClass = e->GetObjectClass(g_client);
        targetField = e->GetFieldID(clientClass, fld::kClient_targetedEntity,
                                   "Lnet/minecraft/class_1297;");
        jni::clearException(e, "targetedEntity (write)");
        e->DeleteLocalRef(clientClass);
    }
    if (targetField) {
        e->SetObjectField(g_client, targetField, entity);
        jni::clearException(e, "targetedEntity set");
    }
    return true;
}

jobject Client::crosshair() {
    JNIEnv* e = jni::env();
    if (!e || !g_resolved || !g_client) return nullptr;
    static jfieldID crossField = nullptr;
    static bool probed = false;
    if (!probed) {
        probed = true;
        jclass clientClass = e->GetObjectClass(g_client);
        crossField = e->GetFieldID(clientClass, fld::kClient_crosshair, "Lnet/minecraft/class_239;"); // base HitResult
        jni::clearException(e, "crosshairTarget");
        if (!crossField) trace::log("resolve warn: crosshairTarget field failed");
    }
    if (!crossField) return nullptr;
    jobject h = e->GetObjectField(g_client, crossField);
    jni::clearException(e, "get crosshairTarget");
    return h;   // local ref, valid for this frame's call
}

jobject Client::raycastWithFluids(double range, float tickDelta) {
    JNIEnv* e = jni::env();
    jobject cameraEntity = g_player;      // the local player is the camera entity in first person
    if (!e || !g_resolved || !cameraEntity || !g_sym.ent_raycast || !(range > 0.0)) return nullptr;
    jobject hit = e->CallObjectMethod(cameraEntity, g_sym.ent_raycast, (jdouble)range,
                                      (jfloat)std::clamp(tickDelta, 0.f, 1.f), JNI_TRUE);
    if (e->ExceptionCheck()) { jni::clearException(e, "Entity.raycast(fluids)"); return nullptr; }
    return hit;
}
std::string Client::blockStateString(int x, int y, int z) {
    JNIEnv* e = jni::env();
    if (!e || !g_world || !g_sym.world_getBlockState || !g_sym.blockPos || !g_sym.blockPos_ctor ||
        !g_sym.obj_toString) return {};
    LocalRef pos(e->NewObject(g_sym.blockPos, g_sym.blockPos_ctor, (jint)x, (jint)y, (jint)z));
    jni::clearException(e, "blockStateString BlockPos");
    if (!pos) return {};
    LocalRef state(e->CallObjectMethod(g_world, g_sym.world_getBlockState, pos.get()));
    jni::clearException(e, "blockStateString getBlockState");
    if (!state) return {};
    jstring text = (jstring)e->CallObjectMethod(state.get(), g_sym.obj_toString);
    jni::clearException(e, "blockStateString toString");
    const std::string out = jni::fromJString(e, text);
    if (text) e->DeleteLocalRef(text);
    return out;
}
int Client::attackCooldown() {
    JNIEnv* e = jni::env();
    if (!e || !g_client || !g_sym.mc_attackCooldown) return 0;
    return e->GetIntField(g_client, g_sym.mc_attackCooldown);
}

bool Client::doAttack()           {
    JNIEnv* e = jni::env();
    if (!e || !g_client || !g_sym.mc_doAttack) return false;
    const jboolean r = e->CallBooleanMethod(g_client, g_sym.mc_doAttack);
    jni::clearException(e, "doAttack");
    return r == JNI_TRUE;
}
jmethodID Client::doAttackSym()      { return g_sym.mc_doAttack; }
jmethodID Client::attackEntitySym()  { return g_sym.im_attackEntity; }
void Client::handleInputEvents() {
    JNIEnv* e = jni::env();
    if (!e || !g_client || !g_sym.mc_handleInput) return;
    e->CallVoidMethod(g_client, g_sym.mc_handleInput);
    jni::clearException(e, "handleInputEvents");
}
std::string Client::username() {
    JNIEnv* e = jni::env();
    if (!e || !g_sess || !g_sym.ses_username) return "player";
    jstring js = (jstring)e->GetObjectField(g_sess, g_sym.ses_username);
    std::string out = jni::fromJString(e, js);
    if (js) e->DeleteLocalRef(js);
    return out;
}

// ---------------------------------------------------------------------------
// Entity
// ---------------------------------------------------------------------------
Vec3 Entity::pos() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_getX) return {};
    // Prefer getters (handle any custom positions); fall back to field via X getter anyway.
    const jdouble x = e->CallDoubleMethod(obj_, g_sym.ent_getX);
    const jdouble y = e->CallDoubleMethod(obj_, g_sym.ent_getY);
    const jdouble z = e->CallDoubleMethod(obj_, g_sym.ent_getZ);
    jni::clearException(e, "Entity.pos");
    return { (float)x, (float)y, (float)z };
}
Vec3 Entity::prevPos() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_) return {};
    const double x = jni::getDouble(e, obj_, g_sym.ent_prevX);
    const double y = jni::getDouble(e, obj_, g_sym.ent_prevY);
    const double z = jni::getDouble(e, obj_, g_sym.ent_prevZ);
    return { (float)x, (float)y, (float)z };
}
Vec3 Entity::eyePos() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_getEyePos) { Vec3 p = pos(); p.y += eyeHeight(); return p; }
    LocalRef v(e->CallObjectMethod(obj_, g_sym.ent_getEyePos));
    jni::clearException(e, "eyePos");
    if (!v) { Vec3 p = pos(); p.y += eyeHeight(); return p; }
    return readVec3(v.get());
}
float Entity::yaw() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_getYaw) return jni::getFloat(e, obj_, g_sym.ent_yaw);
    const jfloat v = e->CallFloatMethod(obj_, g_sym.ent_getYaw);
    jni::clearException(e, "yaw");
    return v;
}
float Entity::pitch() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_getPitch) return jni::getFloat(e, obj_, g_sym.ent_pitch);
    const jfloat v = e->CallFloatMethod(obj_, g_sym.ent_getPitch);
    jni::clearException(e, "pitch");
    return v;
}
void Entity::setYaw(float yaw) const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_setYaw) return;
    e->CallVoidMethod(obj_, g_sym.ent_setYaw, (jfloat)yaw);
    jni::clearException(e, "setYaw");
}
void Entity::setPitch(float pitch) const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_setPitch) return;
    e->CallVoidMethod(obj_, g_sym.ent_setPitch, (jfloat)pitch);
    jni::clearException(e, "setPitch");
}
float Entity::prevYaw() const   { return jni::getFloat(jni::env(), obj_, g_sym.ent_lastYaw); }
float Entity::prevPitch() const { return jni::getFloat(jni::env(), obj_, g_sym.ent_lastPitch); }
int Entity::age() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_age) return -1;
    const jint v = e->GetIntField(obj_, g_sym.ent_age);
    if (e->ExceptionCheck()) { jni::clearException(e, "age"); return -1; }
    return (int)v;
}
// The renderer interpolates the camera from lastYaw/lastPitch towards yaw/pitch, and the vanilla
// tick stamps lastYaw = yaw. Silent aim spoofs yaw/pitch for one call, so if a tick lands inside
// that window it would leave the aimed angle behind as the interpolation source and the camera
// would visibly swing. These put that source back afterwards.
void Entity::setPrevYaw(float yaw) const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_lastYaw) return;
    e->SetFloatField(obj_, g_sym.ent_lastYaw, (jfloat)yaw);
    jni::clearException(e, "setPrevYaw");
}
void Entity::setPrevPitch(float pitch) const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_lastPitch) return;
    e->SetFloatField(obj_, g_sym.ent_lastPitch, (jfloat)pitch);
    jni::clearException(e, "setPrevPitch");
}
float Entity::width() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_getWidth) return 0.6f;
    const jfloat v = e->CallFloatMethod(obj_, g_sym.ent_getWidth);
    jni::clearException(e, "getWidth");
    return v;
}
float Entity::height() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_getHeight) return 1.8f;
    const jfloat v = e->CallFloatMethod(obj_, g_sym.ent_getHeight);
    jni::clearException(e, "getHeight");
    return v;
}
float Entity::eyeHeight() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_getStandEye) return height() * 0.85f;
    return e->CallFloatMethod(obj_, g_sym.ent_getStandEye);
}
bool Entity::alive() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_isAlive) return false;
    const jboolean v = e->CallBooleanMethod(obj_, g_sym.ent_isAlive);
    jni::clearException(e, "isAlive");
    return v == JNI_TRUE;
}
bool Entity::sprinting() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_isSprinting) return false;
    const jboolean v = e->CallBooleanMethod(obj_, g_sym.ent_isSprinting);
    jni::clearException(e, "isSprinting");
    return v == JNI_TRUE;
}
void Entity::setSprinting(bool on) const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_setSprinting) return;
    e->CallVoidMethod(obj_, g_sym.ent_setSprinting, (jboolean)on);
    jni::clearException(e, "setSprinting");
}
bool Entity::glowing() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_isGlowing) return false;
    const jboolean v = e->CallBooleanMethod(obj_, g_sym.ent_isGlowing);
    jni::clearException(e, "isGlowing");
    return v == JNI_TRUE;
}
void Entity::setGlowing(bool on) const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_setGlowing) return;
    e->CallVoidMethod(obj_, g_sym.ent_setGlowing, (jboolean)(on ? JNI_TRUE : JNI_FALSE));
    jni::clearException(e, "setGlowing");
}
Vec3 Entity::velocity() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_getVelocity) return {};
    LocalRef v(e->CallObjectMethod(obj_, g_sym.ent_getVelocity));
    jni::clearException(e, "getVelocity");
    if (!v) return {};
    return readVec3(v.get());
}
void Entity::setVelocity(const Vec3& v) const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_setVelocity || !g_sym.vec3d) return;
    const jmethodID ctor = e->GetMethodID(g_sym.vec3d, "<init>", "(DDD)V");
    if (!ctor) return;
    LocalRef vec(e->NewObject(g_sym.vec3d, ctor, (jdouble)v.x, (jdouble)v.y, (jdouble)v.z));
    if (!vec) return;
    e->CallVoidMethod(obj_, g_sym.ent_setVelocity, vec.get());
    jni::clearException(e, "setVelocity");
}
double Entity::fallDistance() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_fallDist) return 0.0;
    return g_sym.ent_fallDistIsFloat ? (double)e->GetFloatField(obj_, g_sym.ent_fallDist)
                                     : e->GetDoubleField(obj_, g_sym.ent_fallDist);
}
bool   Entity::onGround() const     { return jni::getBool(jni::env(), obj_, g_sym.ent_onGround) == JNI_TRUE; }
std::string Entity::displayName() {
    // Entity.getName() -> Text -> getString. Replaces the old GameProfile
    // lookup: authlib 7.x made GameProfile a record and getName() no longer
    // exists there (NoSuchMethodError = the mob-death crash).
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_getName || !g_sym.txt_getLiteralString) return {};
    LocalRef txt(e->CallObjectMethod(obj_, g_sym.ent_getName));
    jni::clearException(e, "entity.getName");
    if (!txt) return {};
    // Text.getString == getLiteralString (same intermediary method_54160,
    // verified in the 1.21.11 jar) — reuse the already-resolved symbol.
    jstring js = (jstring)e->CallObjectMethod(txt.get(), g_sym.txt_getLiteralString);
    jni::clearException(e, "text.getString");
    std::string out = jni::fromJString(e, js);
    if (js) e->DeleteLocalRef(js);
    return out;
}
std::string Entity::uuid() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_getUuidStr) return {};
    jstring js = (jstring)e->CallObjectMethod(obj_, g_sym.ent_getUuidStr);
    std::string out = jni::fromJString(e, js);
    if (js) e->DeleteLocalRef(js);
    return out;
}
double Entity::distanceTo(const Entity& o) const {
    const Vec3 a = pos(), b = o.pos();
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}
Vec3 Entity::lerpPos(float t) const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.ent_getLerpedPos) return pos();
    LocalRef value(e->CallObjectMethod(obj_, g_sym.ent_getLerpedPos,
                                       (jfloat)std::clamp(t, 0.f, 1.f)));
    jni::clearException(e, "Entity.getLerpedPos");
    return value ? readVec3(value.get()) : pos();
}
Entity::Bounds Entity::interpolatedBounds(float tickDelta) const {
    Bounds out;
    JNIEnv* e = jni::env();
    if (!e || !obj_) return out;
    if (g_sym.ent_bbox && g_sym.box_minX && g_sym.box_minY && g_sym.box_minZ &&
        g_sym.box_maxX && g_sym.box_maxY && g_sym.box_maxZ && g_sym.ent_getLerpedPos) {
        LocalRef box(e->GetObjectField(obj_, g_sym.ent_bbox));
        jni::clearException(e, "Entity.boundingBox");
        if (box) {
            const double minX=jni::getDouble(e,box.get(),g_sym.box_minX);
            const double minY=jni::getDouble(e,box.get(),g_sym.box_minY);
            const double minZ=jni::getDouble(e,box.get(),g_sym.box_minZ);
            const double maxX=jni::getDouble(e,box.get(),g_sym.box_maxX);
            const double maxY=jni::getDouble(e,box.get(),g_sym.box_maxY);
            const double maxZ=jni::getDouble(e,box.get(),g_sym.box_maxZ);
            const Vec3 current=pos(), rendered=lerpPos(tickDelta);
            const Vec3 delta=rendered-current;
            out.min={ (float)minX+delta.x,(float)minY+delta.y,(float)minZ+delta.z };
            out.max={ (float)maxX+delta.x,(float)maxY+delta.y,(float)maxZ+delta.z };
            out.valid=std::isfinite(out.min.x)&&std::isfinite(out.min.y)&&std::isfinite(out.min.z)&&
                      std::isfinite(out.max.x)&&std::isfinite(out.max.y)&&std::isfinite(out.max.z)&&
                      out.max.x>out.min.x&&out.max.y>out.min.y&&out.max.z>out.min.z;
            if (out.valid) return out;
        }
    }

    const Vec3 feet=lerpPos(tickDelta);
    const float w=width(),h=height();
    if (w>0.05f&&w<4.f&&h>0.2f&&h<8.f) {
        const float half=w*0.5f;
        out.min={feet.x-half,feet.y,feet.z-half};
        out.max={feet.x+half,feet.y+h,feet.z+half};
        out.valid=true;
    }
    return out;
}

// ---------------------------------------------------------------------------
// LivingEntity
// ---------------------------------------------------------------------------
float LivingEntity::health() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.liv_getHealth) return 0.f;
    const jfloat v = e->CallFloatMethod(obj_, g_sym.liv_getHealth);
    jni::clearException(e, "getHealth");
    return v;
}
float LivingEntity::maxHealth() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.liv_getMaxHealth) return 20.f;
    const jfloat v = e->CallFloatMethod(obj_, g_sym.liv_getMaxHealth);
    jni::clearException(e, "getMaxHealth");
    return v;
}
float LivingEntity::absorptionAmount() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.liv_getAbsorption) return 0.f;
    const jfloat v = e->CallFloatMethod(obj_, g_sym.liv_getAbsorption);
    jni::clearException(e, "getAbsorptionAmount");
    return v > 0.f ? v : 0.f;
}
int LivingEntity::hurtTime() const { return jni::getInt(jni::env(), obj_, g_sym.liv_hurtTime); }
bool LivingEntity::blocking() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.liv_isBlocking) return false;
    const jboolean v = e->CallBooleanMethod(obj_, g_sym.liv_isBlocking);
    jni::clearException(e, "isBlocking");
    return v == JNI_TRUE;
}
bool LivingEntity::usingItem() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.liv_isUsingItem) return false;
    const jboolean v = e->CallBooleanMethod(obj_, g_sym.liv_isUsingItem);
    jni::clearException(e, "isUsingItem");
    return v == JNI_TRUE;
}
jobject LivingEntity::activeItem() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.liv_getActiveItem) return nullptr;
    return e->CallObjectMethod(obj_, g_sym.liv_getActiveItem);
}
jobject LivingEntity::mainHandStack() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.liv_getMainHand) return nullptr;
    return e->CallObjectMethod(obj_, g_sym.liv_getMainHand);
}
jobject LivingEntity::offHandStack() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.liv_getOffHand) return nullptr;
    return e->CallObjectMethod(obj_, g_sym.liv_getOffHand);
}
jobject LivingEntity::equippedStack(int slotIndex) const {
    // ordinal order: MAINHAND(0) OFFHAND(1) FEET(2) LEGS(3) CHEST(4) HEAD(5)
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.liv_getEquip) return nullptr;
    jobject slot = nullptr;
    Symbols& s = g_sym;
    switch (slotIndex) {
        case 0: slot = s.eqMainhand; break;
        case 1: slot = s.eqOffhand;  break;
        case 2: slot = s.eqFeet;     break;
        case 3: slot = s.eqLegs;     break;
        case 4: slot = s.eqChest;    break;
        case 5: slot = s.eqHead;     break;
        default: return nullptr;
    }
    if (!slot) return nullptr;
    return e->CallObjectMethod(obj_, g_sym.liv_getEquip, slot);
}
bool LivingEntity::gliding() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.liv_isGliding) return false;
    const jboolean v = e->CallBooleanMethod(obj_, g_sym.liv_isGliding);
    jni::clearException(e, "isGliding");
    return v == JNI_TRUE;
}
std::string LivingEntity::statusEffectsText() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.liv_getStatusEffects) return {};
    // The collection's own toString already lists every StatusEffectInstance, whose
    // text carries the effect's registry id ("minecraft:strength"). One call, no
    // StatusEffectInstance accessors to map.
    LocalRef effects(e->CallObjectMethod(obj_, g_sym.liv_getStatusEffects));
    jni::clearException(e, "getStatusEffects");
    if (!effects) return {};
    return Client::objToString(effects.get());
}
// Writes both the current and the "last" body/head rotation. The renderer draws the model with
// lerp(last, current, partialTick), so setting only `current` left every tick blending the aim back
// toward the camera-derived value - the constant tweaking. With both pinned to the same angle the
// model is stable while the camera moves completely independently.
void LivingEntity::setBodyPose(float yaw) const {
    JNIEnv* e = jni::env();
    if (!e || !obj_) return;
    // All four fields, direct stores. (Entity declares setBodyYaw/setHeadYaw in this version, but
    // LivingEntity does not override them, so calling them would run Entity's default - which can
    // write the entity's own yaw, i.e. the camera. Never call those on the local player.)
    if (g_sym.liv_bodyYaw)     e->SetFloatField(obj_, g_sym.liv_bodyYaw,     (jfloat)yaw);
    if (g_sym.liv_lastBodyYaw) e->SetFloatField(obj_, g_sym.liv_lastBodyYaw, (jfloat)yaw);
    if (g_sym.liv_headYaw)     e->SetFloatField(obj_, g_sym.liv_headYaw,     (jfloat)yaw);
    if (g_sym.liv_lastHeadYaw) e->SetFloatField(obj_, g_sym.liv_lastHeadYaw, (jfloat)yaw);
    jni::clearException(e, "setBodyPose");
}

void LivingEntity::setHeadPose(float yaw) const {
    JNIEnv* e = jni::env();
    if (!e || !obj_) return;
    if (g_sym.liv_headYaw)     e->SetFloatField(obj_, g_sym.liv_headYaw,     (jfloat)yaw);
    if (g_sym.liv_lastHeadYaw) e->SetFloatField(obj_, g_sym.liv_lastHeadYaw, (jfloat)yaw);
    jni::clearException(e, "setHeadPose");
}

void LivingEntity::swingHand(bool offhand) const {
    // Hand enum (class_1268): MAIN_HAND = field_5808, OFF_HAND = field_5810 (verified).
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.liv_swingHand || !g_sym.hand) return;
    static jobject handMain = nullptr, handOff = nullptr;
    if (!handMain || !handOff) {
        const jfieldID mf = e->GetStaticFieldID(g_sym.hand, fld::kHand_MAIN, "Lnet/minecraft/class_1268;");
        const jfieldID of = e->GetStaticFieldID(g_sym.hand, fld::kHand_OFF,  "Lnet/minecraft/class_1268;");
        if (mf && !handMain) handMain = e->NewGlobalRef(e->GetStaticObjectField(g_sym.hand, mf));
        if (of && !handOff) handOff = e->NewGlobalRef(e->GetStaticObjectField(g_sym.hand, of));
        jni::clearException(e, "hand constants");
    }
    jobject hand = offhand ? handOff : handMain;
    if (!hand) return;
    e->CallVoidMethod(obj_, g_sym.liv_swingHand, hand);
    jni::clearException(e, "swingHand.call");
}

// ---------------------------------------------------------------------------
// Player
// ---------------------------------------------------------------------------
jobject Player::inventory() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.pl_getInventory) return nullptr;
    return e->CallObjectMethod(obj_, g_sym.pl_getInventory);
}
float Player::attackCooldownProgress(float tickDelta) const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.pl_getAttackCd) return 1.f;
    const jfloat v = e->CallFloatMethod(obj_, g_sym.pl_getAttackCd, (jfloat)tickDelta);
    jni::clearException(e, "attackCooldown");
    return v;
}
jobject Player::gameProfile() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.pl_getGameProfile) return nullptr;
    jobject r = e->CallObjectMethod(obj_, g_sym.pl_getGameProfile);
    jni::clearException(e, "getGameProfile");
    return r;
}
double Player::blockInteractionRange() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.pl_getBlockRange) return creative() ? 5.0 : 4.5;
    const jdouble v = e->CallDoubleMethod(obj_, g_sym.pl_getBlockRange);
    if (e->ExceptionCheck()) { jni::clearException(e, "getBlockInteractionRange"); return 4.5; }
    return std::isfinite(v) && v > 0.0 ? v : 4.5;
}
double Player::entityInteractionRange() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.pl_getEntityRange) return creative() ? 5.0 : 3.0;
    const jdouble v = e->CallDoubleMethod(obj_, g_sym.pl_getEntityRange);
    if (e->ExceptionCheck()) { jni::clearException(e, "getEntityInteractionRange"); return 3.0; }
    return std::isfinite(v) && v > 0.0 ? v : 3.0;
}
bool Player::creative() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.pl_isCreative) return false;
    const jboolean v = e->CallBooleanMethod(obj_, g_sym.pl_isCreative);
    jni::clearException(e, "isCreative");
    return v == JNI_TRUE;
}
// ---- custom HUD reads -------------------------------------------------------------------
// Armour points, food level and experience are read straight off the game objects so the
// drawn bars show what vanilla would have shown. Every accessor is null-tolerant: a missing
// symbol (a re-mapped build) returns a safe default instead of throwing out of the overlay.
int Player::armorPoints() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.liv_getArmor) return 0;
    const jint v = e->CallIntMethod(obj_, g_sym.liv_getArmor);
    jni::clearException(e, "getArmor");
    return v > 0 ? (int)v : 0;
}
int Player::foodLevel() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_ || !g_sym.hunger_getFoodLevel) return 0;
    jobject hunger = g_sym.pl_getHunger ? e->CallObjectMethod(obj_, g_sym.pl_getHunger)
                                        : jni::getObj(e, obj_, g_sym.pl_hungerManager);
    if (!hunger) { jni::clearException(e, "getHungerManager"); return 0; }
    const jint v = e->CallIntMethod(hunger, g_sym.hunger_getFoodLevel);
    jni::clearException(e, "getFoodLevel");
    e->DeleteLocalRef(hunger);
    return v > 0 ? (int)v : 0;
}
int Player::experienceLevel() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_) return 0;
    return (int)jni::getInt(e, obj_, g_sym.pl_experienceLevel, 0);
}
float Player::experienceProgress() const {
    JNIEnv* e = jni::env();
    if (!e || !obj_) return 0.f;
    return (float)jni::getFloat(e, obj_, g_sym.pl_experienceProgress, 0.f);
}

// ---------------------------------------------------------------------------
// PlayerInventory
// ---------------------------------------------------------------------------
namespace inv {
jobject mainList(jobject inventoryObj) {
    JNIEnv* e = jni::env();
    if (!e || !inventoryObj) return nullptr;
    return jni::getObj(e, inventoryObj, g_sym.inv_main);
}
// NOTE: armor/offHand list accessors were removed — 1.21.11 has no such
// PlayerInventory fields. Use LivingEntity::equippedStack instead.
int selectedSlot(jobject inventoryObj) {
    JNIEnv* e = jni::env();
    if (!e || !inventoryObj) return 0;
    return jni::getInt(e, inventoryObj, g_sym.inv_selectedSlot);
}
void setSelectedSlot(jobject inventoryObj, int slot) {
    JNIEnv* e = jni::env();
    if (!e || !inventoryObj || !g_sym.inv_selectedSlot) return;
    e->SetIntField(inventoryObj, g_sym.inv_selectedSlot, (jint)slot);
}
void setSelectedSlotMethod(jobject inventoryObj, int slot) {
    // PlayerInventory.setSelectedSlot(I)V — the method the game calls when you
    // press 1-9. Server-acknowledged, screen handled; the hand truly ends on
    // `slot` (unlike swapSlotWithHotbar, which shuffles stacks and can leave
    // a ghost hand or swap the wrong slot while an inventory screen is open).
    JNIEnv* e = jni::env();
    if (!e || !inventoryObj || !g_sym.inv_setSelected) return;
    e->CallVoidMethod(inventoryObj, g_sym.inv_setSelected, (jint)slot);
    jni::clearException(e, "setSelectedSlot");
}
jobject getFromList(jobject list, int index) {
    JNIEnv* e = jni::env();
    if (!e || !list || !g_sym.dl_get || index < 0) return nullptr;
    jobject value = e->CallObjectMethod(list, g_sym.dl_get, (jint)index);
    if (e->ExceptionCheck()) { jni::clearException(e, "DefaultedList.get"); return nullptr; }
    return value;
}
int listSize(jobject list) {
    JNIEnv* e = jni::env();
    if (!e || !list || !g_sym.dl_size) return 0;
    const jint size = e->CallIntMethod(list, g_sym.dl_size);
    if (e->ExceptionCheck()) { jni::clearException(e, "DefaultedList.size"); return 0; }
    return size;
}
void swapWithHotbar(jobject inventoryObj, int slot) {
    JNIEnv* e = jni::env();
    if (!e || !inventoryObj || !g_sym.inv_swapHotbar) return;
    e->CallVoidMethod(inventoryObj, g_sym.inv_swapHotbar, (jint)slot);
    jni::clearException(e, "swapWithHotbar");
}
bool isCoolingDown(jobject stack) {
    // player.getItemCooldownManager().isCoolingDown(stack)
    JNIEnv* e = jni::env();
    jobject p = Client::player();
    if (!e || !stack || !p || !g_sym.pl_itemCooldownMgr || !g_sym.cd_isCoolingDown) return false;
    LocalRef mgr(e->GetObjectField(p, g_sym.pl_itemCooldownMgr));
    if (!mgr) { jni::clearException(e, "cooldown.mgr"); return false; }
    const jboolean r = e->CallBooleanMethod(mgr.get(), g_sym.cd_isCoolingDown, stack);
    jni::clearException(e, "isCoolingDown");
    return r == JNI_TRUE;
}
jobject selectedStack(jobject inventoryObj) {
    JNIEnv* e = jni::env();
    if (!e || !inventoryObj || !g_sym.inv_getSelected) return nullptr;
    return e->CallObjectMethod(inventoryObj, g_sym.inv_getSelected);
}
} // namespace inv

// ---------------------------------------------------------------------------
// ItemStack
// ---------------------------------------------------------------------------
namespace stk {
bool isEmpty(jobject stack) {
    JNIEnv* e = jni::env();
    if (!e || !stack || !g_sym.stk_isEmpty) return true;
    return e->CallBooleanMethod(stack, g_sym.stk_isEmpty) == JNI_TRUE;
}
int count(jobject stack) {
    JNIEnv* e = jni::env();
    if (!e || !stack || !g_sym.stk_getCount) return 0;
    return e->CallIntMethod(stack, g_sym.stk_getCount);
}
jobject item(jobject stack) {
    JNIEnv* e = jni::env();
    if (!e || !stack || !g_sym.stk_getItem) return nullptr;
    return e->CallObjectMethod(stack, g_sym.stk_getItem);
}
int damage(jobject stack) {
    JNIEnv* e = jni::env();
    if (!e || !stack || !g_sym.stk_getDamage) return 0;
    return e->CallIntMethod(stack, g_sym.stk_getDamage);
}
int maxDamage(jobject stack) {
    JNIEnv* e = jni::env();
    if (!e || !stack || !g_sym.stk_getMaxDamage) return 0;
    return e->CallIntMethod(stack, g_sym.stk_getMaxDamage);
}
bool damageable(jobject stack) {
    JNIEnv* e = jni::env();
    if (!e || !stack || !g_sym.stk_isDamageable) return false;
    return e->CallBooleanMethod(stack, g_sym.stk_isDamageable) == JNI_TRUE;
}
bool isOf(jobject stack, jobject itemObj) {
    JNIEnv* e = jni::env();
    if (!e || !stack || !itemObj || !g_sym.stk_isOf) return false;
    return e->CallBooleanMethod(stack, g_sym.stk_isOf, itemObj) == JNI_TRUE;
}
std::string name(jobject stack) {
    JNIEnv* e = jni::env();
    if (!e || !stack || !g_sym.stk_getName) return {};
    LocalRef txt(e->CallObjectMethod(stack, g_sym.stk_getName));
    jni::clearException(e, "stack.getName");
    return textToString(txt.get());
}
std::string itemId(jobject stack) {
    JNIEnv* e = jni::env();
    if (!e || !stack) return {};
    LocalRef itemObj(item(stack));
    return itemObj ? registryItemPath(itemObj.get()) : std::string{};
}

int lungeLevel(jobject stack) {
    // Detect the Lunge enchantment through ItemStack.getEnchantments().toString().
    // The component prints its registry keys (e.g. "...minecraft:lunge]=...}=3"), so no
    // iterator / registry-holder / boxed-Integer JNI calls are needed - only
    // java.lang.Object.toString, which cannot be mis-resolved.
    JNIEnv* e = jni::env();
    if (!e || !stack || !g_sym.stk_getEnch || !g_sym.obj_toString) return 0;
    LocalRef ench(e->CallObjectMethod(stack, g_sym.stk_getEnch));
    if (e->ExceptionCheck()) { jni::clearException(e, "stack.getEnchantments"); return 0; }
    if (!ench) return 0;
    jstring text = (jstring)e->CallObjectMethod(ench.get(), g_sym.obj_toString);
    if (e->ExceptionCheck()) { jni::clearException(e, "enchantments.toString"); return 0; }
    std::string repr = jni::fromJString(e, text);
    if (text) e->DeleteLocalRef(text);
    if (repr.empty()) return 0;

    static bool loggedSample = false;
    if (!loggedSample && repr.find("minecraft:") != std::string::npos) {
        loggedSample = true;
        trace::log("enchantment sample (for Lunge detection): %.200s", repr.c_str());
    }
    std::string lower(repr);
    for (char& c : lower) c = (char)std::tolower((unsigned char)c);
    const size_t at = lower.find("minecraft:lunge");
    if (at == std::string::npos) return 0;
    // The level follows the entry's closing brace: "...minecraft:lunge]=<holder>}=<level>".
    const size_t eq = lower.find("}=", at);
    if (eq != std::string::npos) {
        int level = 0;
        for (size_t i = eq + 2; i < lower.size() && lower[i] >= '0' && lower[i] <= '9'; ++i)
            level = level * 10 + (lower[i] - '0');
        if (level > 0) return level;
    }
    return 1;   // present, level text not parseable - still a Lunge item
}
} // namespace stk

// ---------------------------------------------------------------------------
// Text -> string (best-effort: literal content first, else the translated name)
// ---------------------------------------------------------------------------
// Every plain item name takes this path: "item.minecraft.golden_apple" has no literal content, so
// getLiteralString below answers nothing and the component's own toString dump is all there is. The
// dump -> readable name rule lives in agent/modules/text_name.h (test/text_name_check.cpp pins it),
// because the HUD plaque is not the only place that has to read an item's real name.
std::string textToString(jobject textObj) {
    JNIEnv* e = jni::env();
    if (!e || !textObj) return {};
    if (g_sym.txt_getLiteralString) {
        jstring js = (jstring)e->CallObjectMethod(textObj, g_sym.txt_getLiteralString);
        jni::clearException(e, "getLiteralString");
        if (js) {
            std::string out = jni::fromJString(e, js);
            e->DeleteLocalRef(js);
            if (!out.empty()) return out;
        }
    }
    // Fallback: TextContent visit is too complex from C++; use toString().
    jclass cls = e->GetObjectClass(textObj);
    if (!cls) { jni::clearException(e, "text class"); return {}; }
    const jmethodID toStr = e->GetMethodID(cls, "toString", "()Ljava/lang/String;");
    jstring js = toStr ? (jstring)e->CallObjectMethod(textObj, toStr) : nullptr;
    e->DeleteLocalRef(cls);
    jni::clearException(e, "text.toString");
    std::string out = jni::fromJString(e, js);
    if (js) e->DeleteLocalRef(js);
    // A translatable component prints its own fields ("translation{key='item.minecraft.
    // golden_apple', args=[]}"), which is not a name. The key is turned into one instead.
    if (out.find("key=") != std::string::npos) {
        const std::string named = modules::names::nameFromDump(out);
        if (!named.empty()) return named;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Registries.ITEM.getId(item).getPath()
// ---------------------------------------------------------------------------
std::string registryItemPath(jobject itemObj) {
    JNIEnv* e = jni::env();
    if (!e || !itemObj) return {};
    if (!g_sym.reg_getId) {
        // One-time: fetch Registries.ITEM and resolve getId on it.
        if (!g_sym.registries) return {};
        if (jfieldID itemRegFid = jni::sfield(e, g_sym.registries, fld::kReg_ITEM, "Lnet/minecraft/class_7922;")) {
            LocalRef itemReg(jni::sgetObj(e, g_sym.registries, itemRegFid));
            if (itemReg) {
                g_sym.reg_getId = e->GetMethodID(e->GetObjectClass(itemReg.get()), mth::kRegistry_getId,
                                                 "(Ljava/lang/Object;)Lnet/minecraft/class_2960;");
                jni::clearException(e, "registry.getId.resolve");
            }
        }
        if (!g_sym.reg_getId) return {};
    }
    if (jfieldID itemRegFid = jni::sfield(e, g_sym.registries, fld::kReg_ITEM, "Lnet/minecraft/class_7922;")) {
        LocalRef itemReg(jni::sgetObj(e, g_sym.registries, itemRegFid));
        if (!itemReg) return {};
        LocalRef id(e->CallObjectMethod(itemReg.get(), g_sym.reg_getId, itemObj));
        jni::clearException(e, "registry.getId");
        if (!id || !g_sym.id_getPath) return {};
        jstring js = (jstring)e->CallObjectMethod(id.get(), g_sym.id_getPath);
        std::string out = jni::fromJString(e, js);
        if (js) e->DeleteLocalRef(js);
        return out;
    }
    return {};
}

// ---------------------------------------------------------------------------
// Registries.ENTITY_TYPE.getId(type).getPath()
// ---------------------------------------------------------------------------
std::string entityTypePath(jobject entity) {
    JNIEnv* e = jni::env();
    if (!e || !entity || !g_sym.ent_type || !g_sym.registries || !g_sym.id_getPath) return {};
    LocalRef type(e->CallObjectMethod(entity, g_sym.ent_type));
    jni::clearException(e, "Entity.getType");
    if (!type) return {};
    static jfieldID entityTypeRegFid = nullptr;
    if (!entityTypeRegFid)
        entityTypeRegFid = jni::sfield(e, g_sym.registries, fld::kReg_ENTITY_TYPE,
                                       "Lnet/minecraft/class_7922;");
    if (!entityTypeRegFid) return {};
    LocalRef registry(e->GetStaticObjectField(g_sym.registries, entityTypeRegFid));
    jni::clearException(e, "Registries.ENTITY_TYPE");
    if (!registry) return {};
    jmethodID registryGetId = g_sym.reg_getId;
    if (!registryGetId) {
        jclass registryClass = e->GetObjectClass(registry.get());
        if (registryClass) {
            registryGetId = e->GetMethodID(registryClass, mth::kRegistry_getId,
                                           "(Ljava/lang/Object;)Lnet/minecraft/class_2960;");
            e->DeleteLocalRef(registryClass);
        }
        jni::clearException(e, "entity type registry getId resolve");
    }
    if (!registryGetId) return {};
    LocalRef id(e->CallObjectMethod(registry.get(), registryGetId, type.get()));
    jni::clearException(e, "entity type registry getId");
    if (!id) return {};
    jstring js = (jstring)e->CallObjectMethod(id.get(), g_sym.id_getPath);
    const std::string path = jni::fromJString(e, js);
    if (js) e->DeleteLocalRef(js);
    return path;
}

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------
namespace opts {
jobject simpleOption(jobject gameOptions, jfieldID optionField) {
    JNIEnv* e = jni::env();
    if (!e || !gameOptions || !optionField) return nullptr;
    return jni::getObj(e, gameOptions, optionField);
}
double getDoubleOption(jobject gameOptions, jfieldID optionField, double fallback) {
    JNIEnv* e = jni::env();
    if (!e || !gameOptions || !optionField || !g_sym.opt_getValue) return fallback;
    LocalRef opt(simpleOption(gameOptions, optionField));
    if (!opt) return fallback;
    LocalRef val(e->CallObjectMethod(opt.get(), g_sym.opt_getValue));
    jni::clearException(e, "opt.getValue");
    if (!val) return fallback;
    jclass dblCls = e->GetObjectClass(val.get());
    if (!dblCls) { jni::clearException(e, "opt.getValue class"); return fallback; }
    const jmethodID dv = e->GetMethodID(dblCls, "doubleValue", "()D");
    const double out = dv ? (double)e->CallDoubleMethod(val.get(), dv) : fallback;
    e->DeleteLocalRef(dblCls);
    jni::clearException(e, "opt.getValue doubleValue");
    return out;
}
void setDoubleOption(jobject gameOptions, jfieldID optionField, double value) {
    JNIEnv* e = jni::env();
    if (!e || !gameOptions || !optionField || !g_sym.opt_setValue) return;
    LocalRef opt(simpleOption(gameOptions, optionField));
    if (!opt) return;
    jclass dblCls = jni::findClass(e, mcids::kDouble);
    if (!dblCls) return;
    const jmethodID ctor = e->GetMethodID(dblCls, "<init>", "(D)V");
    if (!ctor) { e->DeleteLocalRef(dblCls); return; }
    LocalRef boxed(e->NewObject(dblCls, ctor, (jdouble)value));
    e->DeleteLocalRef(dblCls);
    if (!boxed) return;
    e->CallVoidMethod(opt.get(), g_sym.opt_setValue, boxed.get());
    jni::clearException(e, "opt.setValue");
}
void setIntOptionRaw(jobject gameOptions, jfieldID optionField, int value) {
    // Bypasses the option's own validator by writing its value field directly, so
    // values outside the vanilla slider range (e.g. FOV above 110) actually stick.
    JNIEnv* e = jni::env();
    if (!e || !gameOptions || !optionField || !g_sym.so_value) return;
    LocalRef opt(simpleOption(gameOptions, optionField));
    if (!opt) return;
    jclass intCls = e->FindClass("java/lang/Integer");
    if (!intCls) { jni::clearException(e, "Integer class"); return; }
    const jmethodID ctor = e->GetMethodID(intCls, "<init>", "(I)V");
    if (!ctor) { e->DeleteLocalRef(intCls); return; }
    LocalRef boxed(e->NewObject(intCls, ctor, (jint)value));
    e->DeleteLocalRef(intCls);
    if (!boxed) return;
    e->SetObjectField(opt.get(), g_sym.so_value, boxed.get());
    jni::clearException(e, "opt.value(SetObjectField)");
}
// Plain boolean fields on GameOptions (hudHidden today). The value is read/written directly,
// without going through a SimpleOption, because these are the raw option flags the game reads
// while it builds its HUD.
bool getBoolOption(jobject gameOptions, jfieldID optionField, bool fallback) {
    JNIEnv* e = jni::env();
    if (!e || !gameOptions || !optionField) return fallback;
    const jboolean v = e->GetBooleanField(gameOptions, optionField);
    if (e->ExceptionCheck()) { jni::clearException(e, "opt.bool(get)"); return fallback; }
    return v == JNI_TRUE;
}
void setBoolOption(jobject gameOptions, jfieldID optionField, bool value) {
    JNIEnv* e = jni::env();
    if (!e || !gameOptions || !optionField) return;
    e->SetBooleanField(gameOptions, optionField, (jboolean)value);
    jni::clearException(e, "opt.bool(set)");
}
} // namespace opts

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------
namespace cam {
Vec3 pos(jobject camera) {
    JNIEnv* e = jni::env();
    if (!e || !camera) return {};
    LocalRef v(jni::getObj(e, camera, g_sym.cam_pos));
    if (!v) return {};
    return readVec3(v.get());
}
float yaw(jobject camera) {
    JNIEnv* e = jni::env();
    if (!e || !camera || !g_sym.cam_getYaw) return 0.f;
    return e->CallFloatMethod(camera, g_sym.cam_getYaw);
}
float pitch(jobject camera) {
    JNIEnv* e = jni::env();
    if (!e || !camera || !g_sym.cam_getPitch) return 0.f;
    return e->CallFloatMethod(camera, g_sym.cam_getPitch);
}
bool thirdPerson(jobject camera) {
    JNIEnv* e = jni::env();
    if (!e || !camera || !g_sym.cam_getThirdPerson) return false;
    return e->CallBooleanMethod(camera, g_sym.cam_getThirdPerson) == JNI_TRUE;
}
jobject focused(jobject camera) {
    JNIEnv* e = jni::env();
    if (!e || !camera || !g_sym.cam_getFocused) return nullptr;
    return e->CallObjectMethod(camera, g_sym.cam_getFocused);
}

bool basis(jobject camera, Vec3& right, Vec3& up, Vec3& forward) {
    JNIEnv* e=jni::env();
    if (!e||!camera||!g_sym.cam_getHorizontalPlane||!g_sym.cam_getVerticalPlane) return false;
    auto readVector=[&](jobject vector,Vec3& out)->bool {
        if (!vector) return false;
        jclass cls=e->GetObjectClass(vector);
        if (!cls) { jni::clearException(e,"camera basis class"); return false; }
        jmethodID mx=e->GetMethodID(cls,"x","()F");
        jmethodID my=e->GetMethodID(cls,"y","()F");
        jmethodID mz=e->GetMethodID(cls,"z","()F");
        e->DeleteLocalRef(cls);
        if (!mx||!my||!mz) { jni::clearException(e,"camera basis vector methods"); return false; }
        out={e->CallFloatMethod(vector,mx),e->CallFloatMethod(vector,my),e->CallFloatMethod(vector,mz)};
        if (e->ExceptionCheck()) { jni::clearException(e,"camera basis components"); return false; }
        return std::isfinite(out.x)&&std::isfinite(out.y)&&std::isfinite(out.z)&&out.len2()>0.25f;
    };
    // Camera.getHorizontalPlane() is the camera's FORWARD axis, getVerticalPlane() is UP
    // (vanilla Camera.getProjection scales them by the near plane / half-height). The
    // third vector (getDiagonalPlane) is the sideways axis; we derive the screen-right
    // axis from forward x up instead so its sign can never be mirrored.
    LocalRef forwardVec(e->CallObjectMethod(camera,g_sym.cam_getHorizontalPlane));
    LocalRef upVec(e->CallObjectMethod(camera,g_sym.cam_getVerticalPlane));
    if (e->ExceptionCheck()) { jni::clearException(e,"camera basis vectors"); return false; }
    if (!readVector(forwardVec.get(),forward)||!readVector(upVec.get(),up)) return false;

    constexpr float degToRad=0.017453292519943295f;
    const float yawRad=yaw(camera)*degToRad, pitchRad=pitch(camera)*degToRad;
    const Vec3 expectedForward(-std::sin(yawRad)*std::cos(pitchRad),
                               -std::sin(pitchRad),
                               std::cos(yawRad)*std::cos(pitchRad));
    // Orient the forward axis against Minecraft's documented yaw/pitch direction in
    // case the renderer stores it mirrored; the up axis must point toward world-up.
    if (forward.dot(expectedForward)<0.f) forward=forward*-1.f;
    forward=forward.normalized();
    up=(up-forward*up.dot(forward)).normalized();          // Gram-Schmidt against forward
    if (up.y<-0.05f && std::abs(pitchRad)<1.5f) up=up*-1.f;
    right=forward.cross(up).normalized();                  // right-handed: facing -Z gives +X
    return std::isfinite(right.x)&&std::isfinite(right.y)&&std::isfinite(right.z)&&
           forward.dot(expectedForward)>0.95f && right.len2()>0.5f;
}
} // namespace cam

// ---------------------------------------------------------------------------
// Entity collection
// ---------------------------------------------------------------------------
std::vector<jobject> collectEntities(jobject world, int maxCount) {
    std::vector<jobject> out;
    JNIEnv* e = jni::env();
    if (!e || !world || !g_sym.world_getEntities) return out;
    LocalRef iterable(e->CallObjectMethod(world, g_sym.world_getEntities));
    jni::clearException(e, "world.getEntities");
    if (!iterable) return out;

    static jmethodID itMid = nullptr;
    if (!itMid && g_sym.iterable) itMid = e->GetMethodID(g_sym.iterable, "iterator", "()Ljava/util/Iterator;");
    if (e->ExceptionCheck()) { jni::clearException(e, "Iterable.iterator"); return out; }
    if (!itMid || !g_sym.it_hasNext || !g_sym.it_next) return out;
    LocalRef it(e->CallObjectMethod(iterable.get(), itMid));
    if (!it) return out;

    out.reserve(64);
    while (e->CallBooleanMethod(it.get(), g_sym.it_hasNext) == JNI_TRUE) {
        LocalRef ent(e->CallObjectMethod(it.get(), g_sym.it_next));
        jni::clearException(e, "iterator.next");
        if (ent) {
            out.push_back(e->NewLocalRef(ent.get()));
            if ((int)out.size() >= maxCount) break;
        }
    }
    jni::clearException(e, "collectEntities");
    return out;
}

// ---------------------------------------------------------------------------
// InteractionManager
// ---------------------------------------------------------------------------
namespace im {
void attackEntity(jobject interactionManager, jobject player, jobject target) {
    JNIEnv* e = jni::env();
    if (!e || !interactionManager || !player || !target || !g_sym.im_attackEntity) return;
    e->CallVoidMethod(interactionManager, g_sym.im_attackEntity, player, target);
    jni::clearException(e, "attackEntity");
}
int gameModeId(jobject interactionManager) {
    JNIEnv* e = jni::env();
    if (!e || !interactionManager || !g_sym.im_getGameMode) return -1;
    LocalRef gm(e->CallObjectMethod(interactionManager, g_sym.im_getGameMode));
    jni::clearException(e, "gameMode");
    if (!gm) return -1;
    // GameMode is an enum; ordinal() is a stable Java Enum method.
    jclass gmCls = e->GetObjectClass(gm.get());
    if (!gmCls) { jni::clearException(e, "gameMode class"); return -1; }
    const jmethodID ord = e->GetMethodID(gmCls, "ordinal", "()I");
    e->DeleteLocalRef(gmCls);
    return ord ? e->CallIntMethod(gm.get(), ord) : -1;
}
} // namespace im

// ---------------------------------------------------------------------------
// Chat message (best-effort — only when a ChatHud can be reached)
// ---------------------------------------------------------------------------
void chatMessage(const std::string& text) {
    JNIEnv* e = jni::env();
    if (!e || !g_sym.chat_addToHistory) return;
    // We don't reach InGameHud's ChatHud without another hop; history-only is
    // still useful for scripts. (Full HUD chat is drawn by Passive itself.)
    jstring js = jni::toJString(e, text.c_str());
    if (!js) return;
    // NOTE: without a ChatHud instance this is a no-op wrapper kept for parity.
    e->DeleteLocalRef(js);
}

// ---------------------------------------------------------------------------
// Hit helpers — used by combat targeting (game's own crosshair result).
// ---------------------------------------------------------------------------
namespace hit {

// Returns 0=MISS 1=BLOCK 2=ENTITY, or -1 on any failure.
int typeOf(jobject hit) {
    JNIEnv* e = jni::env();
    if (!e || !hit) return -1;
    Symbols& s = g_sym;
    if (s.entityHit && e->IsInstanceOf(hit, s.entityHit) == JNI_TRUE) return 2;
    if (e->ExceptionCheck()) { jni::clearException(e, "HitResult subtype"); return -1; }
    if (s.blockHitResult && e->IsInstanceOf(hit, s.blockHitResult) == JNI_TRUE) {
        // The game returns a BlockHitResult with type MISS when the crosshair ray
        // reached no block (looking at the sky). Instanceof alone reads that as a
        // block hit, so every placement built on it became an air place. The hit's
        // own HitResult.Type decides; when the enum cannot be read the old
        // instanceof rule stands, so a version without getType keeps working.
        if (s.hit_getType && s.enum_name) {
            LocalRef typeObj(e->CallObjectMethod(hit, s.hit_getType));
            jni::clearException(e, "HitResult.getType");
            if (typeObj) {
                LocalRef name(e->CallObjectMethod(typeObj.get(), s.enum_name));
                jni::clearException(e, "HitResult.Type.name");
                if (name) {
                    const char* chars = e->GetStringUTFChars((jstring)name.get(), nullptr);
                    if (chars) {
                        const bool miss = std::strcmp(chars, "MISS") == 0;
                        e->ReleaseStringUTFChars((jstring)name.get(), chars);
                        if (miss) return 0;
                    }
                }
            }
        }
        return 1;
    }
    if (e->ExceptionCheck()) { jni::clearException(e, "BlockHitResult subtype"); return -1; }
    return 0;
}

Vec3 pos(jobject hit) {
    JNIEnv* e = jni::env();
    if (!e || !hit) return {};

    if (typeOf(hit) == 2) {
        jobject ent = entity(hit);
        if (!ent) return {};
        const Entity entityRef(ent);
        return entityRef.pos();
    }

    if (!g_sym.hit_getPos) return {};
    LocalRef posObj(e->CallObjectMethod(hit, g_sym.hit_getPos));
    jni::clearException(e, "hit.getPos");
    if (!posObj) return {};
    return readVec3(posObj.get());
}

// EntityHitResult.getEntity() — local ref or null.
jobject entity(jobject hit) {
    JNIEnv* e = jni::env();
    if (!e || !hit) return nullptr;
    Symbols& s = g_sym;
    if (!s.entHit_getEntity) return nullptr;
    jobject en = e->CallObjectMethod(hit, s.entHit_getEntity);
    jni::clearException(e, "hit getEntity");
    return en;
}

bool isUpFace(jobject hit) {
    JNIEnv* e = jni::env();
    if (!e || !hit || !g_sym.blockHitResult || !g_sym.blockHit_getSide || !g_sym.dirUp ||
        e->IsInstanceOf(hit, g_sym.blockHitResult) != JNI_TRUE)
        return false;
    LocalRef side(e->CallObjectMethod(hit, g_sym.blockHit_getSide));
    jni::clearException(e, "BlockHitResult.getSide");
    return side && e->IsSameObject(side.get(), g_sym.dirUp) == JNI_TRUE;
}

bool sideOffset(jobject hit, int& dx, int& dy, int& dz) {
    JNIEnv* e=jni::env();
    if (!e||!hit||!g_sym.blockHitResult||!g_sym.blockHit_getSide||
        e->IsInstanceOf(hit,g_sym.blockHitResult)!=JNI_TRUE) return false;
    LocalRef side(e->CallObjectMethod(hit,g_sym.blockHit_getSide));
    jni::clearException(e,"BlockHitResult.getSide");
    if (!side) return false;
    struct DirOffset { jobject direction; int x,y,z; };
    const DirOffset dirs[]={{g_sym.dirDown,0,-1,0},{g_sym.dirUp,0,1,0},
        {g_sym.dirNorth,0,0,-1},{g_sym.dirSouth,0,0,1},
        {g_sym.dirWest,-1,0,0},{g_sym.dirEast,1,0,0}};
    for (const auto& dir:dirs) {
        if (dir.direction && e->IsSameObject(side.get(),dir.direction)==JNI_TRUE) {
            dx=dir.x; dy=dir.y; dz=dir.z;
            return true;
        }
    }
    return false;
}

bool blockPos(jobject hit, int& x, int& y, int& z) {
    JNIEnv* e = jni::env();
    if (!e || !hit || !g_sym.blockHitResult || !g_sym.blockHit_getBlockPos ||
        !g_sym.blockPos_getX || !g_sym.blockPos_getY || !g_sym.blockPos_getZ ||
        e->IsInstanceOf(hit, g_sym.blockHitResult) != JNI_TRUE)
        return false;

    LocalRef pos(e->CallObjectMethod(hit, g_sym.blockHit_getBlockPos));
    jni::clearException(e, "BlockHitResult.getBlockPos");
    if (!pos) return false;
    x = e->CallIntMethod(pos.get(), g_sym.blockPos_getX);
    y = e->CallIntMethod(pos.get(), g_sym.blockPos_getY);
    z = e->CallIntMethod(pos.get(), g_sym.blockPos_getZ);
    if (e->ExceptionCheck()) {
        jni::clearException(e, "BlockPos coordinates");
        return false;
    }
    return true;
}

} // namespace hit


// ---------------------------------------------------------------------------
// Extended helpers (lazy-resolved, never counted as resolve failures).
// Key presses go through vanilla KeyBinding.setPressed so every vanilla check
// (hunger, sneaking, using an item...) still applies; rotation packets are the
// stock PlayerMoveC2SPacket.LookAndOnGround.
// ---------------------------------------------------------------------------
namespace ext {
namespace {
bool g_tried = false;
jclass g_keyCls = nullptr;
jmethodID g_setPressed = nullptr, g_getNet = nullptr, g_sendPacket = nullptr;
jfieldID g_jumpKey = nullptr, g_sprintKey = nullptr, g_useKey = nullptr;

// Silent aim's three real PlayerMoveC2SPacket subclasses, resolved once and sent from here - the
// DLL owns all of them, so nothing about the aim depends on a Java payload. `wide` records whether
// the constructor takes the trailing horizontalCollision boolean that 1.21.2+ added.
enum MoveKind { MoveLook = 0, MovePosition = 1, MoveFull = 2, MoveKindCount = 3 };
jclass    g_movePkt[MoveKindCount] = {};
jmethodID g_moveCtor[MoveKindCount] = {};
bool      g_moveWide[MoveKindCount] = {};
const char* const g_moveName[MoveKindCount] = {
    "net.minecraft.class_2828$class_2831",   // LookAndOnGround
    "net.minecraft.class_2828$class_2829",   // PositionAndOnGround
    "net.minecraft.class_2828$class_2830",   // Full
};

void resolveExt(JNIEnv* e) {
    if (g_tried) return;
    g_tried = true;
    auto glob = [&](jclass c) { return c ? (jclass)e->NewGlobalRef(c) : nullptr; };
    jclass key = e->FindClass("net/minecraft/class_304");
    if (!key) { e->ExceptionClear(); key = jni::findClass(e, "net.minecraft.class_304"); }
    g_keyCls = glob(key);
    if (g_keyCls) g_setPressed = e->GetMethodID(g_keyCls, "method_23481", "(Z)V");
    e->ExceptionClear();
    if (g_sym.gameOptions) {
        g_jumpKey   = e->GetFieldID(g_sym.gameOptions, "field_1903", "Lnet/minecraft/class_304;");
        e->ExceptionClear();
        g_sprintKey = e->GetFieldID(g_sym.gameOptions, "field_1867", "Lnet/minecraft/class_304;");
        e->ExceptionClear();
        // GameOptions.useKey (field_1904) — the item-use button, used to hold a
        // crossbow while it reloads.
        g_useKey    = e->GetFieldID(g_sym.gameOptions, "field_1904", "Lnet/minecraft/class_304;");
        e->ExceptionClear();
    }
    if (g_sym.minecraft) {
        g_getNet = e->GetMethodID(g_sym.minecraft, "method_1562", "()Lnet/minecraft/class_634;");
        e->ExceptionClear();
    }
    jclass handler = jni::findClass(e, "net.minecraft.class_634");
    if (handler) {
        g_sendPacket = e->GetMethodID(handler, "method_52787", "(Lnet/minecraft/class_2596;)V");
        e->ExceptionClear();
        e->DeleteLocalRef(handler);
    }
    // Silent aim is carried by three real subclasses of PlayerMoveC2SPacket, each pinned by name
    // against this build's client jar (mappings/mappings.tiny, agent/jni/ids.h):
    //   class_2828$class_2831  LookAndOnGround      (float yaw, float pitch, boolean onGround
    //                                               [, boolean horizontalCollision])
    //   class_2829             PositionAndOnGround  (double x, double y, double z, boolean onGround
    //                                               [, boolean horizontalCollision])
    //   class_2828$class_2830  Full                 (double x, double y, double z, float yaw,
    //                                               float pitch, boolean onGround
    //                                               [, boolean horizontalCollision])
    // PositionAndOnGround and Full carry no rotation and both carry position respectively, which is
    // why the aim picks between them by what an update needs to say. Every class must really be
    // assignable to class_2828 before it is used, and the resolved set is logged: a build where a
    // name moved leaves silent aim with nothing to send, which reads in game as "the aim did
    // nothing".
    static const char* const kWideSig[MoveKindCount] = { "(FFZZ)V", "(DDDZZ)V", "(DDDFFZZ)V" };
    static const char* const kNarrowSig[MoveKindCount] = { "(FFZ)V", "(DDDZ)V", "(DDDFFZ)V" };
    jclass moveBase = jni::findClass(e, "net.minecraft.class_2828");
    for (int kind = 0; kind < MoveKindCount; ++kind) {
        jclass cls = jni::findClass(e, g_moveName[kind]);
        if (!cls) continue;
        if (moveBase && e->IsAssignableFrom(cls, moveBase) != JNI_TRUE) {
            e->ExceptionClear();
            e->DeleteLocalRef(cls);
            continue;
        }
        jmethodID wide = e->GetMethodID(cls, "<init>", kWideSig[kind]);
        e->ExceptionClear();
        jmethodID narrow = wide ? nullptr : e->GetMethodID(cls, "<init>", kNarrowSig[kind]);
        e->ExceptionClear();
        if (!wide && !narrow) { e->DeleteLocalRef(cls); continue; }
        g_movePkt[kind] = glob(cls);
        g_moveCtor[kind] = wide ? wide : narrow;
        g_moveWide[kind] = (wide != nullptr);
        e->DeleteLocalRef(cls);
    }
    if (moveBase) e->DeleteLocalRef(moveBase);
    trace::log("ext resolve: key=%d jump=%d sprint=%d net=%d send=%d look=%d pos=%d full=%d",
               g_setPressed != nullptr, g_jumpKey != nullptr, g_sprintKey != nullptr,
               g_getNet != nullptr, g_sendPacket != nullptr,
               g_moveCtor[MoveLook] != nullptr, g_moveCtor[MovePosition] != nullptr,
               g_moveCtor[MoveFull] != nullptr);
}
} // namespace

// Resolve once and report silent aim's three packets. Called from the resolve pass so a build where
// a name moved is visible in the log instead of silently leaving the aim with nothing to send.
bool probeMovePackets() {
    JNIEnv* e = jni::env();
    if (!e) return false;
    resolveExt(e);
    for (int kind = 0; kind < MoveKindCount; ++kind)
        if (!g_moveCtor[kind]) return false;
    return true;
}
bool movePacketReady(MoveKind kind) { return kind >= 0 && kind < MoveKindCount && g_moveCtor[kind] != nullptr; }

// The one line the resolve pass prints: which of the three silent aim can actually send. Built once,
// after the lookup, because that is the only time it can change.
const char* movePacketsStatus() {
    static char status[192];
    if (!status[0]) {
        const bool look = movePacketReady(MoveLook);
        const bool pos  = movePacketReady(MovePosition);
        const bool full = movePacketReady(MoveFull);
        std::snprintf(status, sizeof(status), "%s (look=%s position=%s full=%s)",
                      (look && pos && full) ? "ready" : "SOME MISSING",
                      look ? "LookAndOnGround" : "missing",
                      pos ? "PositionAndOnGround" : "missing",
                      full ? "Full" : "missing");
    }
    return status;
}

bool setKey(Key which, bool pressed) {
    JNIEnv* e = jni::env();
    if (!e || !g_opts) return false;
    resolveExt(e);
    jfieldID f = which == Key::Jump ? g_jumpKey : (which == Key::Sprint ? g_sprintKey : g_useKey);
    if (!f || !g_setPressed) return false;
    jobject kb = e->GetObjectField(g_opts, f);
    if (jni::env()->ExceptionCheck()) { e->ExceptionClear(); return false; }
    if (!kb) return false;
    e->CallVoidMethod(kb, g_setPressed, (jboolean)pressed);
    jni::clearException(e, "KeyBinding.setPressed");
    e->DeleteLocalRef(kb);
    return true;
}

// One sender for all three PlayerMoveC2SPacket subclasses. What each one says:
//   LookAndOnGround      rotation only          - safe to repeat every frame
//   PositionAndOnGround  position only          - moves nothing the aim owns, carries no rotation
//   Full                 position and rotation  - the whole state in one packet
// The position is read at the moment the packet is built, from the client itself, so it can never
// disagree with the movement the game is about to send.
bool sendMove(MoveKind kind, float yaw, float pitch, bool onGround) {
    JNIEnv* e = jni::env();
    if (!e || !g_client) return false;
    resolveExt(e);
    if (!g_getNet || !g_sendPacket || !g_movePkt[kind] || !g_moveCtor[kind]) return false;
    Vec3 p{};
    if (kind != MoveLook) {
        if (!g_player) return false;
        p = Player(g_player).pos();
    }
    jobject net = e->CallObjectMethod(g_client, g_getNet);
    if (e->ExceptionCheck()) { e->ExceptionClear(); return false; }
    if (!net) return false;
    jobject pkt = nullptr;
    if (kind == MoveLook) {
        pkt = g_moveWide[kind]
            ? e->NewObject(g_movePkt[kind], g_moveCtor[kind],
                           (jfloat)yaw, (jfloat)pitch, (jboolean)onGround, (jboolean)false)
            : e->NewObject(g_movePkt[kind], g_moveCtor[kind],
                           (jfloat)yaw, (jfloat)pitch, (jboolean)onGround);
    } else if (kind == MovePosition) {
        pkt = g_moveWide[kind]
            ? e->NewObject(g_movePkt[kind], g_moveCtor[kind],
                           (jdouble)p.x, (jdouble)p.y, (jdouble)p.z, (jboolean)onGround, (jboolean)false)
            : e->NewObject(g_movePkt[kind], g_moveCtor[kind],
                           (jdouble)p.x, (jdouble)p.y, (jdouble)p.z, (jboolean)onGround);
    } else {
        pkt = g_moveWide[kind]
            ? e->NewObject(g_movePkt[kind], g_moveCtor[kind],
                           (jdouble)p.x, (jdouble)p.y, (jdouble)p.z, (jfloat)yaw, (jfloat)pitch,
                           (jboolean)onGround, (jboolean)false)
            : e->NewObject(g_movePkt[kind], g_moveCtor[kind],
                           (jdouble)p.x, (jdouble)p.y, (jdouble)p.z, (jfloat)yaw, (jfloat)pitch,
                           (jboolean)onGround);
    }
    if (e->ExceptionCheck()) { e->ExceptionClear(); pkt = nullptr; }
    bool ok = false;
    if (pkt) {
        e->CallVoidMethod(net, g_sendPacket, pkt);
        ok = !e->ExceptionCheck();
        jni::clearException(e, "sendPacket(move)");
        e->DeleteLocalRef(pkt);
    }
    e->DeleteLocalRef(net);
    // Vanilla's sendMovementPackets compares the current rotation against lastYawClient /
    // lastPitchClient and stamps them whenever it sends a rotation. A rotation sent from here is
    // that rotation as far as the server is concerned, so stamp the pair too: otherwise the next
    // tick sees a "changed" rotation and sends the exact same angle again, which Grim reports as
    // AimDuplicateLook ("duplicate rotation update without changing look direction").
    if (ok && (kind == MoveLook || kind == MoveFull) && g_player) {
        if (g_sym.cli_lastYaw)   e->SetFloatField(g_player, g_sym.cli_lastYaw,   (jfloat)yaw);
        if (g_sym.cli_lastPitch) e->SetFloatField(g_player, g_sym.cli_lastPitch, (jfloat)pitch);
        jni::clearException(e, "last sent rotation");
    }
    // One line each way per packet kind: silent aim is only "silent" if these really go out, and a
    // resolved class name in the log is the difference between "the server turned" and "nothing was
    // sent".
    static int reported[MoveKindCount] = {};   // 0 = nothing yet, 1 = first success, -1 = first failure
    if (reported[kind] == 0) {
        reported[kind] = ok ? 1 : -1;
        trace::log("ext: %s packet %s (class %s)", kind == MoveLook ? "look" : kind == MovePosition ? "position" : "full",
                   ok ? "sent" : "FAILED", g_moveName[kind]);
    }
    return ok;
}

bool sendLookPacket(float yaw, float pitch, bool onGround) {
    return sendMove(MoveLook, yaw, pitch, onGround);
}

bool sendCameraLookLive(float* yawOut, float* pitchOut) {
    // Same packet as sendLookPacket, but resolved from the JVM directly: this runs from the input
    // hook, where the per-frame client/player globals have already been released. It hands the
    // server the view the player actually has, one tick before vanilla's own movement packet would,
    // so the place / break packet the next tick sends is validated against the real camera instead
    // of the silent aim.
    JNIEnv* e = jni::env();
    if (!e || !g_sym.minecraft || !g_sym.mc_getInstance || !g_sym.mc_player ||
        !g_sym.ent_yaw || !g_sym.ent_pitch || !g_sym.ent_onGround) return false;
    resolveExt(e);
    if (!g_getNet || !g_sendPacket || !g_movePkt[MoveLook] || !g_moveCtor[MoveLook]) return false;
    LocalRef client(e->CallStaticObjectMethod(g_sym.minecraft, g_sym.mc_getInstance));
    if (e->ExceptionCheck()) { jni::clearException(e, "live look getInstance"); return false; }
    if (!client) return false;
    LocalRef player(e->GetObjectField(client.get(), g_sym.mc_player));
    if (e->ExceptionCheck()) { jni::clearException(e, "live look player"); return false; }
    if (!player) return false;
    const jfloat yaw = e->GetFloatField(player.get(), g_sym.ent_yaw);
    const jfloat pitch = e->GetFloatField(player.get(), g_sym.ent_pitch);
    const jboolean onGround = e->GetBooleanField(player.get(), g_sym.ent_onGround);
    if (e->ExceptionCheck()) { jni::clearException(e, "live look rotation"); return false; }
    // lastYawClient/lastPitchClient are the rotation the server already holds (vanilla stamps
    // them after every movement packet; so does the silent transform). When the camera is already
    // that rotation there is nothing to hand over, and repeating it would be AimDuplicateLook.
    if (g_sym.cli_lastYaw && g_sym.cli_lastPitch) {
        const jfloat lastYaw = e->GetFloatField(player.get(), g_sym.cli_lastYaw);
        const jfloat lastPitch = e->GetFloatField(player.get(), g_sym.cli_lastPitch);
        if (!e->ExceptionCheck()) {
            float delta = std::fmod((float)yaw - (float)lastYaw + 540.f, 360.f);
            if (delta < 0.f) delta += 360.f;
            delta -= 180.f;
            if (std::abs(delta) < 0.01f && std::abs((float)pitch - (float)lastPitch) < 0.01f) {
                if (yawOut) *yawOut = (float)yaw;
                if (pitchOut) *pitchOut = (float)pitch;
                return true;   // the server is already on the camera
            }
        } else {
            jni::clearException(e, "live look last rotation read");
        }
    }
    LocalRef net(e->CallObjectMethod(client.get(), g_getNet));
    if (e->ExceptionCheck()) { jni::clearException(e, "live look handler"); return false; }
    if (!net) return false;
    LocalRef pkt(g_moveWide[MoveLook]
        ? e->NewObject(g_movePkt[MoveLook], g_moveCtor[MoveLook], yaw, pitch, onGround, JNI_FALSE)
        : e->NewObject(g_movePkt[MoveLook], g_moveCtor[MoveLook], yaw, pitch, onGround));
    if (e->ExceptionCheck()) { jni::clearException(e, "live look packet"); return false; }
    if (!pkt) return false;
    e->CallVoidMethod(net.get(), g_sendPacket, pkt.get());
    const bool ok = !e->ExceptionCheck();
    jni::clearException(e, "live look send");
    if (!ok) return false;
    // Exactly what vanilla stamps after a movement packet: without it the game sees a changed
    // rotation on the next tick and sends the identical angle again (AimDuplicateLook).
    if (g_sym.cli_lastYaw)   e->SetFloatField(player.get(), g_sym.cli_lastYaw, yaw);
    if (g_sym.cli_lastPitch) e->SetFloatField(player.get(), g_sym.cli_lastPitch, pitch);
    jni::clearException(e, "live look last rotation");
    if (yawOut) *yawOut = (float)yaw;
    if (pitchOut) *pitchOut = (float)pitch;
    return true;
}

// PositionAndOnGround carries no rotation in 1.21 - it is (x, y, z, onGround, horizontalCollision) -
// so on its own it never aims anything. Silent aim pairs it with a look packet when the aim has to
// arrive with a fresh position.
bool sendPositionPacket(bool onGround) {
    return sendMove(MovePosition, 0.f, 0.f, onGround);
}

// Full carries position and rotation in one packet, which is what makes the server track the aim as
// the player's own movement rather than as a rotation that belongs to nothing.
bool sendFullPacket(float yaw, float pitch, bool onGround) {
    return sendMove(MoveFull, yaw, pitch, onGround);
}

bool isBlockItemStack(jobject stack) {
    JNIEnv* e = jni::env();
    if (!e || !stack || !g_sym.blockItem) return false;
    jobject item = stk::item(stack);
    if (!item) return false;
    const bool r = e->IsInstanceOf(item, g_sym.blockItem) == JNI_TRUE;
    e->DeleteLocalRef(item);
    return r;
}
} // namespace ext

// ---------------------------------------------------------------------------
// hud::vanillaGuiScale
// ---------------------------------------------------------------------------
// The game draws its own HUD block 182 gui units wide, centred and bottom-anchored, and covering
// that block means knowing how many framebuffer pixels one gui unit is. That number is the
// window's own scale, but the pinned yarn dump does not name the three window methods that
// expose it (getScaledWidth / getScaledHeight / getScaleFactor), so every int-returning method
// the class declares is tried and the only triple that satisfies the game's own invariant
// (scaledWidth * scale == framebufferWidth, scaledHeight * scale == framebufferHeight, scale an
// integer from 1 to 6) is the answer. All of it is best effort: a probe that cannot find the
// scale answers 0 and the caller leaves the game's own HUD exactly as it is.
namespace hud {
namespace {
int  g_guiScale    = 0;      // the resolved scale, 0 until it is found
bool g_guiResolved = false;  // the nine window method ids have been looked up
int  g_guiSkip     = 0;      // frames to wait before probing again
}

int vanillaGuiScale(float framebufferWidth, float framebufferHeight) {
    if (g_guiScale > 0) return g_guiScale;
    const int fbW = (int)(framebufferWidth + 0.5f);
    const int fbH = (int)(framebufferHeight + 0.5f);
    if (fbW <= 0 || fbH <= 0) return 0;
    if (g_guiSkip > 0) { --g_guiSkip; return 0; }              // at most one probe a second
    if (!jni::ready() || !frame::valid()) return 0;
    JNIEnv* e = jni::env();
    jobject client = frame::client();
    if (!e || !client || !g_sym.mc_getWindow) return 0;
    g_guiSkip = 60;
    jobject win = e->CallObjectMethod(client, g_sym.mc_getWindow);
    jni::clearException(e, "getWindow");
    if (!win) return 0;
    if (!g_guiResolved) {
        jclass cls = e->GetObjectClass(win);
        if (cls) {
            bool any = false;
            for (int i = 0; i < mth::kWindow_intMethodCount; ++i) {
                g_sym.win_intMethod[i] = e->GetMethodID(cls, mth::kWindow_intMethods[i], "()I");
                if (e->ExceptionCheck()) { jni::clearException(e, "Window probe"); g_sym.win_intMethod[i] = nullptr; }
                if (g_sym.win_intMethod[i]) any = true;
            }
            e->DeleteLocalRef(cls);
            g_guiResolved = any;
        }
    }
    int values[mth::kWindow_intMethodCount];
    int count = 0;
    for (int i = 0; i < mth::kWindow_intMethodCount; ++i) {
        if (!g_sym.win_intMethod[i]) continue;
        const jint v = e->CallIntMethod(win, g_sym.win_intMethod[i]);
        if (e->ExceptionCheck()) { jni::clearException(e, "Window probe"); continue; }
        if (v > 0) values[count++] = (int)v;
    }
    e->DeleteLocalRef(win);
    for (int scale = 1; scale <= 6; ++scale) {
        if ((fbW % scale) != 0 || (fbH % scale) != 0) continue;
        bool haveW = false, haveH = false;
        for (int i = 0; i < count; ++i) {
            if (values[i] == fbW / scale) haveW = true;
            if (values[i] == fbH / scale) haveH = true;
        }
        // The pair only holds together for the real scale: the framebuffer is the scaled size
        // times the scale, and no other candidate divides both sides exactly.
        if (haveW && haveH) { g_guiScale = scale; return scale; }
    }
    return 0;
}
} // namespace hud

} // namespace mc
} // namespace tyr
