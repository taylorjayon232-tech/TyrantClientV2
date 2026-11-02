#pragma once
// ============================================================================
//  Passive — pinned intermediary symbol IDs for Minecraft 1.21.11 (Fabric).
//  Every entry below was verified against yarn 1.21.11+build.6 tiny mappings.
//  Classes:  net/minecraft/<yarn_path>  ->  intermediary id
//  Fields:   intermediary name (official/obfuscated name is irrelevant:
//            we resolve via intermediary, which is stable for this version)
//  NOTE: Fabric maps intermediary -> runtime at class-load time; we resolve
//        through the Knot/Minecraft classloader using these intermediary ids.
// ============================================================================

// ---- class intermediary ids -----------------------------------------------
namespace mcids {
constexpr const char* kMinecraftClient   = "net.minecraft.class_310";   // MinecraftClient
constexpr const char* kClientPlayerEntity= "net.minecraft.class_746";   // ClientPlayerEntity
constexpr const char* kClientWorld       = "net.minecraft.class_638";   // ClientWorld
constexpr const char* kWorld             = "net.minecraft.class_1937";  // World
constexpr const char* kEntity            = "net.minecraft.class_1297";  // Entity
constexpr const char* kLivingEntity      = "net.minecraft.class_1309";  // LivingEntity
constexpr const char* kPlayerEntity      = "net.minecraft.class_1657";  // PlayerEntity
constexpr const char* kPlayerInventory   = "net.minecraft.class_1661";  // PlayerInventory
constexpr const char* kItemStack         = "net.minecraft.class_1799";  // ItemStack
constexpr const char* kItem              = "net.minecraft.class_1792";  // Item
constexpr const char* kBlockItem         = "net.minecraft.class_1747";  // BlockItem
constexpr const char* kBlock             = "net.minecraft.class_2248";  // Block
constexpr const char* kBlockState        = "net.minecraft.class_2680";  // BlockState
constexpr const char* kVec3d             = "net.minecraft.class_243";   // Vec3d
constexpr const char* kVec3i             = "net.minecraft.class_2382";  // Vec3i

// Vec3d (class_243) component fields (verified)
constexpr auto kVec_x = "field_1352";  // double x
constexpr auto kVec_y = "field_1351";  // double y
constexpr auto kVec_z = "field_1350";  // double z
constexpr const char* kBox               = "net.minecraft.class_238";   // Box
constexpr const char* kBlockPos          = "net.minecraft.class_2338";  // BlockPos
constexpr const char* kBlockHitResult    = "net.minecraft.class_3965";  // BlockHitResult
constexpr const char* kActionResult      = "net.minecraft.class_1269";  // ActionResult
constexpr const char* kCamera            = "net.minecraft.class_4184";  // Camera
constexpr const char* kRenderTickCounter = "net.minecraft.class_9779$class_9781"; // Dynamic render tick counter
constexpr const char* kGameOptions       = "net.minecraft.class_315";   // GameOptions
constexpr const char* kSimpleOption      = "net.minecraft.class_7172";  // SimpleOption
constexpr const char* kText              = "net.minecraft.class_2561";  // Text
constexpr const char* kTextContent       = "net.minecraft.class_7417";  // TextContent
constexpr const char* kIdentifier        = "net.minecraft.class_2960";  // Identifier
constexpr const char* kRegistries        = "net.minecraft.class_7923";  // Registries
constexpr const char* kRegistry          = "net.minecraft.class_2378";  // Registry
constexpr const char* kDefaultedList     = "net.minecraft.class_2371";  // DefaultedList
constexpr const char* kChatHud           = "net.minecraft.class_338";   // ChatHud
constexpr const char* kEquipmentSlot     = "net.minecraft.class_1304";  // EquipmentSlot
constexpr const char* kHand              = "net.minecraft.class_1268";  // Hand (MAIN_HAND/OFF_HAND)
constexpr const char* kPlayerAbilities   = "net.minecraft.class_1656";  // PlayerAbilities (verified 1.21.11)
constexpr const char* kSlotActionType    = "net.minecraft.class_1713";  // SlotActionType
constexpr const char* kScreenHandler     = "net.minecraft.class_1703";  // ScreenHandler
constexpr const char* kScreen            = "net.minecraft.class_437";   // Screen (the type of MinecraftClient.currentScreen)
constexpr const char* kInventoryScreen   = "net.minecraft.class_490";   // InventoryScreen (new InventoryScreen(player))
constexpr const char* kDirection         = "net.minecraft.class_2350";  // Direction
constexpr const char* kInteractionManager= "net.minecraft.class_636";   // ClientPlayerInteractionManager
constexpr const char* kSession           = "net.minecraft.class_320";   // Session
constexpr const char* kGameProfile       = "com.mojang.authlib.GameProfile";
constexpr const char* kItemCooldownMgr   = "net.minecraft.class_1796"; // ItemCooldownManager (verified 1.21.11)
constexpr const char* kHungerManager     = "net.minecraft.class_1702"; // HungerManager (food level / saturation)
constexpr const char* kItemEnchantments  = "net.minecraft.class_9304"; // ItemEnchantments component
constexpr const char* kRegistryEntry     = "net.minecraft.class_6880"; // RegistryEntry (holder)
constexpr const char* kIterable          = "java.lang.Iterable";
constexpr const char* kIterator          = "java.util.Iterator";
constexpr const char* kList              = "java.util.List";
constexpr const char* kObject            = "java.lang.Object";
constexpr const char* kDouble            = "java.lang.Double";
constexpr const char* kString            = "java.lang.String";
constexpr const char* kUuid              = "java.util.UUID";
constexpr const char* kSystem            = "java.lang.System";
} // namespace mcids

// ---- fields ----------------------------------------------------------------
namespace fld {
// MinecraftClient
constexpr auto kClient_player      = "field_1724";  // ClientPlayerEntity player
constexpr auto kClient_world       = "field_1687";  // ClientWorld world
constexpr auto kClient_options     = "field_1690";  // GameOptions options
constexpr auto kClient_interaction = "field_1761";  // ClientPlayerInteractionManager
constexpr auto kClient_session     = "field_1726";  // Session session (verified)
constexpr auto kClient_itemUseCd   = "field_1752";  // int itemUseCooldown
constexpr auto kClient_currentFps  = "field_1738";  // private STATIC int currentFps (1.21.11 — resolve via sfield)
constexpr auto kClient_mouse       = "field_1729";  // Mouse mouse
constexpr auto kClient_gameRenderer= "field_1773";  // GameRenderer gameRenderer
constexpr auto kClient_tickCounter = "field_52750"; // RenderTickCounter.Dynamic
constexpr auto kClient_crosshair   = "field_1765";  // HitResult crosshairTarget (game's own raycast)
constexpr auto kClient_currentScreen = "field_1755"; // Screen currently open, null while in-world
constexpr auto kClient_attackCooldown = "field_1771"; // int attackCooldown (vanilla "miss time")

// ClientPlayerEntity (class_746). The pair sendMovementPackets compares the current rotation with
// and stamps after a rotation packet: writing it after a manual look packet is what stops vanilla
// from sending the same rotation again on the next tick (AimDuplicateLook).
constexpr auto kCli_lastYaw   = "field_3941";  // lastYawClient
constexpr auto kCli_lastPitch = "field_3925";  // lastPitchClient
constexpr auto kReg_BLOCK          = "field_41175"; // Registries.BLOCK
constexpr auto kReg_ENTITY_TYPE    = "field_41177"; // Registries.ENTITY_TYPE (Registry<EntityType<?>>, checked in the client jar)

// Mouse (class_312): leftButtonClicked=field_1791, rightButtonClicked=field_1788.
// We read click state via WndProc instead of these - kept for reference only.

// Entity
constexpr auto kEnt_pos        = "field_22467"; // Vec3d pos
constexpr auto kEnt_lastX      = "field_6014";
constexpr auto kEnt_lastY      = "field_6036";
constexpr auto kEnt_lastZ      = "field_5969";
constexpr auto kEnt_prevX      = "field_6038"; // lastRenderX
constexpr auto kEnt_prevY      = "field_5971";
constexpr auto kEnt_prevZ      = "field_5989";
constexpr auto kEnt_yaw        = "field_6031";
constexpr auto kEnt_pitch      = "field_5965";
constexpr auto kEnt_lastYaw    = "field_5982";
constexpr auto kEnt_lastPitch  = "field_6004";
constexpr auto kEnt_fallDist   = "field_6017";
constexpr auto kEnt_onGround   = "field_5952";
constexpr auto kEnt_age        = "field_6012";
constexpr auto kEnt_id         = "field_5986";
constexpr auto kEnt_uuid       = "field_6021";
constexpr auto kEnt_uuidStr    = "field_5981";
constexpr auto kEnt_bbox       = "field_6005";
constexpr auto kEnt_velocity   = "field_18276";
// Box (class_238) bounds in 1.21.11.
constexpr auto kBox_minX       = "field_1323";
constexpr auto kBox_minY       = "field_1322";
constexpr auto kBox_minZ       = "field_1321";
constexpr auto kBox_maxX       = "field_1320";
constexpr auto kBox_maxY       = "field_1325";
constexpr auto kBox_maxZ       = "field_1324";

// LivingEntity
constexpr auto kLiv_hurtTime    = "field_6235";
constexpr auto kLiv_maxHurtTime = "field_6254";
constexpr auto kLiv_deathTime   = "field_6213";
constexpr auto kLiv_handSwing   = "field_6252";
constexpr auto kLiv_swingTicks  = "field_6279";
constexpr auto kLiv_activeStack = "field_6277";
constexpr auto kLiv_healthAttr  = "field_6247"; // HEALTH dataTracker entry
constexpr auto kLiv_bodyYaw     = "field_6283";   // bodyYaw
constexpr auto kLiv_headYaw     = "field_6241";   // headYaw
constexpr auto kLiv_lastBodyYaw = "field_6220";   // lastBodyYaw (the value the renderer blends FROM)
constexpr auto kLiv_lastHeadYaw = "field_6259";   // lastHeadYaw

// PlayerEntity
constexpr auto kPl_abilities = "field_7503"; // PlayerAbilities abilities
constexpr auto kPl_itemCooldownMgr = "field_7484"; // ItemCooldownManager itemCooldownManager (verified)

// PlayerEntity hunger / experience (read by the custom HUD bars).
constexpr auto kPl_hungerManager      = "field_7493";  // HungerManager hungerManager
constexpr auto kPl_experienceLevel    = "field_7520";  // int experienceLevel
constexpr auto kPl_experienceProgress = "field_7510";  // float experienceProgress (0..1)

// GameOptions
constexpr auto kOpt_gamma = "field_1840";
constexpr auto kOpt_fov   = "field_1826";
// GameOptions.hudHidden: true while the game draws none of its own HUD. The custom HUD
// module sets it so the vanilla hotbar / bars do not show under the drawn one.
constexpr auto kOpt_hudHidden = "field_1842";  // boolean hudHidden

// SimpleOption (class_7172): the raw, un-validated value field. Writing it
// directly is how a value outside the vanilla slider range can be applied.
constexpr auto kSO_value  = "field_37868";  // T value

// PlayerInventory
// NOTE (1.21.11): armor/offHand DefaultedList fields no longer exist — the
// inventory rework (1.21.5+) moved equipment into a single Equipment object.
// Armor/off-hand stacks must be read via LivingEntity.getEquippedStack.
constexpr auto kInv_selected   = "field_7545";
constexpr auto kInv_main       = "field_7547";  // DefaultedList main
constexpr auto kInv_player     = "field_7546";

// GameRenderer (class_757)
constexpr auto kGR_camera = "field_18765"; // Camera camera

// Camera (class_4184)
constexpr auto kCam_pos         = "field_18712"; // Vec3d pos
constexpr auto kCam_yaw         = "field_18718";
constexpr auto kCam_pitch       = "field_18717";
constexpr auto kCam_thirdPerson = "field_18719";

// Session (class_320)
constexpr auto kSes_username = "field_1982"; // String username
constexpr auto kSes_uuid     = "field_1985"; // UUID uuid

// Hand (class_1268) enum constants (verified)
constexpr auto kHand_MAIN = "field_5808";
constexpr auto kHand_OFF  = "field_5810";

// Registries (class_7923)
constexpr auto kReg_ITEM  = "field_41178";  // DefaultedRegistry<Item> (declared type class_7922 in 1.21.11)

// EquipmentSlot (class_1304) enum constants
constexpr auto kEq_MAINHAND = "field_6173";
constexpr auto kEq_OFFHAND  = "field_6171";
constexpr auto kEq_FEET     = "field_6166";
constexpr auto kEq_LEGS     = "field_6172";
constexpr auto kEq_CHEST    = "field_6174";
constexpr auto kEq_HEAD     = "field_6169";

// SlotActionType (class_1713) enum constants
constexpr auto kSAT_PICKUP     = "field_7790";
constexpr auto kSAT_QUICK_MOVE = "field_7794";
constexpr auto kSAT_SWAP       = "field_7791";
constexpr auto kSAT_THROW      = "field_7795";  // (1 = whole stack, 0 = one item)

// PlayerEntity screen handlers (class_1657) + ScreenHandler (class_1703)
constexpr auto kPl_playerScreenHandler  = "field_7498";  // PlayerScreenHandler playerScreenHandler
constexpr auto kPl_currentScreenHandler = "field_7512";  // ScreenHandler currentScreenHandler
constexpr auto kSH_syncId              = "field_7763";  // public final int ScreenHandler.syncId

// Direction (class_2350) enum constants
constexpr auto kDir_DOWN  = "field_11033";
constexpr auto kDir_UP    = "field_11036";
constexpr auto kDir_NORTH = "field_11043";
constexpr auto kDir_SOUTH = "field_11035";
constexpr auto kDir_WEST  = "field_11039";
constexpr auto kDir_EAST  = "field_11034";
} // namespace fld

// ---- methods ---------------------------------------------------------------
namespace mth {
// MinecraftClient
constexpr auto kClient_getInstance      = "method_1551";  // ()MinecraftClient (verified)

// HitResult (class_239) / EntityHitResult (class_3966) — crosshair targeting
constexpr auto kHit_getType      = "method_17783"; // ()Lftk$a; Type enum
constexpr auto kHit_getPos       = "method_17784"; // ()Lftm; Vec3d
constexpr auto kEntHit_getEntity = "method_17782"; // class_3966 -> Lcgk; Entity
constexpr auto kEnt_type         = "method_5864";  // Entity.getType() Lcgu;
constexpr auto kEnum_name        = "name";         // Enum.name() — stable Java
constexpr auto kClient_getWindow        = "method_22683"; // ()Window (class_1041 in 1.21.11)
constexpr auto kClient_getCameraEntity  = "method_1560";  // ()Entity
constexpr auto kClient_doAttack         = "method_1536";  // ()Z
constexpr auto kClient_handleInput      = "method_1508";  // ()V handleInputEvents
constexpr auto kClient_setScreen        = "method_1507";  // (Screen)V setScreen — the inventory key's own path
constexpr auto kEnt_getName             = "method_5477";  // ()Text display name (verified 1.21.11 jar)
// GameRenderer (class_757)
constexpr auto kGR_getCamera            = "method_19418"; // ()Camera (verified)
constexpr auto kGR_getFov               = "method_3196";  // (Camera,F,boolean)F getFov

// Entity
constexpr auto kEnt_getX         = "method_23317";
constexpr auto kEnt_getY         = "method_23318";
constexpr auto kEnt_getZ         = "method_23321";
constexpr auto kEnt_getPos       = "method_19538"; // ()Vec3d  (fallback: read pos field)
constexpr auto kEnt_getLerpedPos = "method_30950"; // (F)Vec3d, vanilla render interpolation
constexpr auto kEnt_getEyePos    = "method_33571";
constexpr auto kEnt_getYaw       = "method_36454";
constexpr auto kEnt_getPitch     = "method_36455";
constexpr auto kEnt_setYaw       = "method_36456";  // (F)V setYaw
constexpr auto kEnt_setPitch     = "method_36457";  // (F)V setPitch
constexpr auto kEnt_getVelocity  = "method_18798";
constexpr auto kEnt_setVelocity  = "method_18799";
constexpr auto kEnt_setSprinting = "method_5728";
constexpr auto kEnt_isSprinting  = "method_5624";  // ()Z (verified)
constexpr auto kEnt_isAlive      = "method_5805";
// Vanilla glowing outline. setGlowing writes the client entity flag; isGlowing
// reads it back (the client branch of isGlowing is the flag, not a team).
constexpr auto kEnt_setGlowing   = "method_5834";  // (Z)V setGlowing
constexpr auto kEnt_isGlowing    = "method_5851";  // ()Z isGlowing
constexpr auto kEnt_getWidth     = "method_17681";
constexpr auto kEnt_getHeight    = "method_17682";
// distance is computed in C++ from positions (no JNI needed)
constexpr auto kEnt_sqrDistTo    = "method_5858";  // (Entity)D
constexpr auto kEnt_getUuidStr   = "method_5845";
constexpr auto kEnt_getRotation  = "method_5720";  // ()Vec3d getRotationVector
constexpr auto kEnt_getStandEye  = "method_5751";  // ()F getStandingEyeHeight

// LivingEntity
constexpr auto kLiv_getHealth    = "method_6032";
constexpr auto kLiv_getMaxHealth = "method_6063";
constexpr auto kLiv_getAbsorption= "method_6067";  // ()F getAbsorptionAmount
constexpr auto kLiv_tryAttack    = "method_6121";  // 1.21.11: (ServerWorld,Entity)Z — UNUSED, client attack goes through interactionManager
constexpr auto kLiv_isBlocking   = "method_6039";
constexpr auto kLiv_isUsingItem  = "method_6115";
constexpr auto kLiv_getActiveItem= "method_6030";
constexpr auto kLiv_clearActive  = "method_6021";
constexpr auto kLiv_getMainHand  = "method_6047";
constexpr auto kLiv_getOffHand   = "method_6079";
constexpr auto kLiv_getEquip     = "method_6118";  // (EquipmentSlot)ItemStack
constexpr auto kLiv_isGliding    = "method_6128";
constexpr auto kLiv_getArmor     = "method_6096";  // ()I getArmor — total armour points from worn gear
// ()Ljava/util/Collection; getStatusEffects — read through Object.toString so no
// StatusEffectInstance / RegistryEntry accessors are needed.
constexpr auto kLiv_getStatusEffects = "method_6026";
constexpr auto kLiv_swingHand    = "method_6104";  // (Hand)V — Hand = class_1268
// (F)F lerpYaw — the render-facing rotation read for a given tick progress. Read back after our
// write: if it does not match the rotation we wrote, the model is not drawn from the fields we
// write and the writer has to target what backs this method instead.
constexpr auto kLiv_lerpYaw      = "method_60951";
constexpr auto kHand_MAIN_C      = "field_5808";   // Hand.MAIN_HAND
constexpr auto kHand_OFF_C       = "field_5810";   // Hand.OFF_HAND

// PlayerEntity / common
constexpr auto kPl_getInventory  = "method_31548";
constexpr auto kPl_getAttackCd   = "method_7261";  // (F)F getAttackCooldownProgress
constexpr auto kPl_getAbilities  = "method_31549";
constexpr auto kPl_getBlockRange  = "method_55754"; // ()D getBlockInteractionRange (verified)
constexpr auto kPl_getEntityRange = "method_55755"; // ()D getEntityInteractionRange (verified)
constexpr auto kEnt_raycast      = "method_5745";  // (DFZ)HitResult Entity.raycast(maxDistance, tickProgress, includeFluids) (verified)
constexpr auto kPl_isCreative    = "method_68878";
constexpr auto kPl_isSpectator   = "method_7340";  // NOTE: actually isMainPlayer on PlayerEntity — see resolver note
constexpr auto kPl_swingHand     = "method_6104";
constexpr auto kPl_getGameProfile= "method_7334";  // ()GameProfile
constexpr auto kPl_getScore      = "method_7272";
constexpr auto kPl_getHunger     = "method_7344";  // ()Lnet/minecraft/class_1702; getHungerManager
constexpr auto kHunger_getFoodLevel = "method_7586"; // ()I HungerManager.getFoodLevel (20 = full)

// PlayerInventory: main/armor/offHand stack access is done by reading the
// DefaultedList fields (field_7547/7548/7549) directly — safest across builds.
constexpr auto kInv_swapHotbar   = "method_7365";  // (I)V swapSlotWithHotbar
constexpr auto kInv_getSelected  = "method_7391";  // ()ItemStack getSelectedStack
constexpr auto kInv_setSelected  = "method_61496"; // (I)V setSelectedSlot — the real "number key" swap
constexpr auto kInv_getSlotM     = "method_67532"; // ()I getSelectedSlot
constexpr auto kCd_isCoolingDown = "method_7904"; // (ItemStack)Z ItemCooldownManager.isCoolingDown (verified on jar)
constexpr auto kInv_getMainStacks= "method_67533"; // ()DefaultedList getMainStacks
constexpr auto kInv_dropSel      = "method_37417"; // (Z)ItemStack dropSelectedItem

// ItemStack
constexpr auto kStk_isEmpty      = "method_7960";
constexpr auto kStk_getCount     = "method_7947";
constexpr auto kStk_getItem      = "method_7909";
constexpr auto kStk_getDamage    = "method_7919";
constexpr auto kStk_getMaxDamage = "method_7936";
constexpr auto kStk_isDamageable = "method_7963";
constexpr auto kStk_isOf         = "method_31574"; // (Item)Z
constexpr auto kStk_getName      = "method_7964";  // ()Text
constexpr auto kStk_getEnch      = "method_58657"; // ()ItemEnchantments (verified 1.21.11)
constexpr auto kIce_entrySet     = "method_57539"; // ()Set<Object2IntMap.Entry<RegistryEntry>> (verified)
constexpr auto kHolder_matchesId = "method_40226"; // (Identifier)Z RegistryEntry.matchesId (verified)
constexpr auto kStk_getHolder    = "method_27319";
constexpr auto kStk_getRegEntry  = "method_41409"; // ()RegistryEntry

// Text / profile
constexpr auto kTxt_getLiteralString = "method_54160";
constexpr auto kTxt_stringify        = "method_10858"; // asTruncatedString fallback
constexpr auto kTxt_content          = "method_10851"; // ()TextContent
constexpr auto kProfile_getName      = "getName";      // com.mojang.authlib (not mapped)

// Camera: pos read from Vec3d field (field_18712). Yaw/pitch via getters (verified).
constexpr auto kCam_getFocused   = "method_19331"; // ()Entity
constexpr auto kCam_getThirdPerson="method_19333";
constexpr auto kCam_getYaw       = "method_19330";
constexpr auto kCam_getPitch     = "method_19329";
constexpr auto kTickCounter_getProgress = "method_60637"; // (Z)F getTickProgress

// InteractionManager
constexpr auto kIm_attackEntity  = "method_2918";  // (PlayerEntity,Entity)V
constexpr auto kIm_attackBlock   = "method_2910";  // (BlockPos,Direction)Z
constexpr auto kIm_updateBreak   = "method_2902";  // (BlockPos,Direction)Z
constexpr auto kIm_cancelBreak   = "method_2925";  // ()V
constexpr auto kIm_interactBlock = "method_2896";  // (Player,Hand,BlockHitResult)ActionResult
constexpr auto kIm_interactItem  = "method_2919";  // (Player,Hand)ActionResult
constexpr auto kIm_swapHotbar    = "method_7365";  // via inventory
constexpr auto kIm_getGameMode   = "method_2920";  // ()GameMode
constexpr auto kIm_syncSelectedSlot = "method_2911"; // ()V syncSelectedSlot
constexpr auto kIm_clickSlot        = "method_2906"; // (IIILdhu;Lddm;)V clickSlot(syncId,slot,button,action,player)

// BlockPos / BlockHitResult geometry for ordinary client block interaction.
constexpr auto kBlockPos_getX    = "method_10263";
constexpr auto kBlockPos_getY    = "method_10264";
constexpr auto kBlockPos_getZ    = "method_10260";
constexpr auto kBlockHit_getBlockPos = "method_17777";  // BlockHitResult.getBlockPos
constexpr auto kBlockHit_getSide = "method_17780";  // BlockHitResult.getSide
constexpr auto kActionResult_isAccepted = "method_23665";

// SimpleOption / GameOptions
constexpr auto kOpt_getValue     = "method_41753";
constexpr auto kOpt_setValue     = "method_41748";

// ChatHud
constexpr auto kChat_addToHistory= "method_1803";  // (String)V

// World / entity iteration
constexpr auto kWorld_getEntities= "method_18112"; // ()Iterable  (ClientWorld override)
constexpr auto kWorld_getBlockState = "method_8320"; // (BlockPos)BlockState
constexpr auto kBlockState_getBlock = "method_26204"; // ()Block
constexpr auto kWorld_getRegMgr  = "method_56673"; // ()RegistryManager (Entity)
constexpr auto kIt_next          = "next";
constexpr auto kIt_hasNext       = "hasNext";

// Identifier
constexpr auto kIdentifier_getPath = "method_12832"; // ()String (verified)
constexpr auto kId_of              = "method_60655"; // static of(String,String)Identifier — the ONLY public way since 1.21.2 (ctor is private!)
constexpr auto kRegistry_getId     = "method_10221"; // (Object)Identifier (verified)

// DefaultedList
constexpr auto kDL_get           = "get";
constexpr auto kDL_size          = "size";

// Window (class_1041): vanillaGuiScale needs getScaledWidth()/getScaledHeight()/getScaleFactor()
// and the pinned yarn dump does not name them, so every ()I method the class declares is listed
// here and the probe picks the triple that satisfies scaledWidth * scale == framebufferWidth,
// scaledHeight * scale == framebufferHeight (the game's own invariant). All of these are plain
// getters; the probe clears any exception a call raises and simply moves on.
constexpr const char* kWindow_intMethods[] = {
    "method_4489", "method_4506", "method_4480", "method_4507",
    "method_4486", "method_4502", "method_4499", "method_4477", "method_4495",
};
constexpr int kWindow_intMethodCount = 9;
} // namespace mth

// Resolution notes (1.21.11 verified):
//  * getAttackCooldownProgress(F)F = method_7261 (PlayerEntity, inherited by ClientPlayerEntity).
//  * GameOptions.gamma field_1840, GameOptions.fov field_1826 are SimpleOption instances.
//  * SimpleOption value getter: method_41753 getValue / setter: method_41748 setValue.
//  * Entity position: read Vec3d pos field (field_22467) x/y/z doubles directly.
//  * Lerp positions: lastRenderX/Y/Z fields (field_6038/5971/5989) are prev-render pos.
//  * itemUseCooldown: field_1752 on MinecraftClient.  hurtTime: field_6235.
//  * Camera: obtained via gameRenderer.field_18765 (field) — no getter needed.
//  * Session username: Session.field_1982 (String) — display name fallback.
//  * EquipmentSlot enum constants MAINHAND/OFFHAND/FEET/LEGS/CHEST/HEAD (field_6173..6169).
//  * main list: PlayerInventory.field_7547 (DefaultedList). Armor/offHand go
//    through getEquippedStack (EquipmentSlot) — the 1.21.5+ equipment rework.
