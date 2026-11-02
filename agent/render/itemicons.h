// ============================================================================
//  Passive — item & armor icons for the HUD, Nametags and the ViewModel.
//
//  Every icon is a PNG inside the game's own asset namespace, loaded through the
//  game's resource pack - so a texture pack's items are drawn with the pack's
//  art, not the vanilla one. We ask the JVM for the raw bytes through the class
//  loader (pure JDK calls, no mappings involved), decode the PNG ourselves and
//  upload one GL texture per icon. Anything that cannot be loaded returns 0 so
//  the caller keeps drawing its text fallback — a missing icon is never a
//  failure that should break a frame.
// ============================================================================
#pragma once
#include <imgui.h>
#include <string>

namespace tyr {
namespace itemicons {

// GL texture for an item id path ("diamond_helmet", "cobweb", ...), or 0.
// Tries the item texture tree first and falls back to the block texture tree,
// so block items (a cobweb, a slab, a fluid) get their real icon too.
// Results (including failures) are cached for the session; call from the render
// thread with the GL context current.
ImTextureID get(const std::string& itemId);

// Armor trim overlay for a trim pattern id ("coast", "sentient", ...), or 0.
// Tries the item-side trim textures first, then the model-side masks.
ImTextureID getTrim(const std::string& patternId);

// ---- Potions -----------------------------------------------------------------
// A potion's inventory icon is not one image: the bottle (item/<base>.png) plus a
// liquid overlay (item/<base>_overlay.png) that the game tints with the potion's
// own colour. Both come from the resource pack, so a pack that retextures potions
// is honoured, and the tint is the vanilla potion colour (see potionTint).
struct PotionIcon {
    ImTextureID base = 0;      // the bottle silhouette
    ImTextureID overlay = 0;   // the liquid, drawn over the bottle with `tint`
    ImU32       tint = 0xFF9AA0B4u;   // 0xAARRGGBB; the caller uses the alpha too
};

// `itemId` is "potion" / "splash_potion" / "lingering_potion"; `potionId` is a
// potion registry path ("strength", "long_speed", "strong_healing"); `customTint`
// is the potion_contents custom colour when the stack has one (0 = none).
PotionIcon potion(const std::string& itemId, const std::string& potionId,
                  ImU32 customTint = 0);

// "minecraft:splash_potion" -> "splash_potion"; anything that is not a potion -> "".
inline std::string potionBaseId(const std::string& itemId) {
    const size_t colon = itemId.find(':');
    const std::string path = colon == std::string::npos ? itemId : itemId.substr(colon + 1);
    if (path == "potion" || path == "splash_potion" || path == "lingering_potion") return path;
    return {};
}

// The colour the game tints that overlay with. Pure (no JNI, no GL) so
// test/item_icons_check.cpp pins it: a potion the client cannot name keeps the
// neutral colour instead of turning into an invisible or black bottle.
inline ImU32 potionTint(const std::string& potionId, ImU32 customTint = 0) {
    // A custom colour (a potion_contents component with custom_color set) is what the
    // game itself uses, so it always wins over the registry colour.
    if ((customTint >> 24) != 0) return customTint;
    const size_t colon = potionId.find(':');
    std::string id = colon == std::string::npos ? potionId : potionId.substr(colon + 1);
    // The long_/strong_ variants of a potion are the same colour as the base one.
    if (id.rfind("long_", 0) == 0) id = id.substr(5);
    else if (id.rfind("strong_", 0) == 0) id = id.substr(7);

    struct Tint { const char* id; ImU32 color; };
    static const Tint tints[] = {
        { "speed",            0xFF7CAFC6u },
        { "slowness",         0xFF5A6C81u },
        { "strength",         0xFF932423u },
        { "healing",          0xFFF82423u },
        { "harming",          0xFF430A09u },
        { "poison",           0xFF4E9331u },
        { "regeneration",     0xFFCD5CABu },
        { "fire_resistance",  0xFFE49A3Au },
        { "water_breathing",  0xFF2E5299u },
        { "invisibility",     0xFF7F8392u },
        { "night_vision",     0xFF1F1FA1u },
        { "weakness",         0xFF484D48u },
        { "luck",             0xFF339900u },
        { "slow_falling",     0xFFFFEFD1u },
        { "weaving",          0xFF786297u },
        { "wind_charged",     0xFFBDC9FFu },
        { "oozing",           0xFF99FFA3u },
        { "infested",         0xFF8C9B8Bu },
        { "turtle_master",    0xFF7BA4A2u },
    };
    for (const Tint& t : tints)
        if (id == t.id) return t.color;
    // Water, mundane, thick, awkward and anything newer than this table: the game's own
    // fallback colour (a faint blue-grey) rather than a bottle with no liquid in it.
    return 0xFF9AA0B4u;
}

// Optional: release everything (session shutdown; GL context still current).
void shutdown();

} // namespace itemicons
} // namespace tyr
