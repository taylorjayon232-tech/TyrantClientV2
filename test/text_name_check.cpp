// ============================================================================
//  Harness: the translatable item-name rule (agent/modules/text_name.h) - what
//  an item's display name reads as when the game hands back the text component's
//  own dump instead of a resolved name. This is the "translation{key=item..." the
//  HUD plaque used to print for a golden apple.
//
//  Build + run:  test\build-text-name.bat   (prints "PASS" or the failing cases)
// ============================================================================
#include "../agent/modules/text_name.h"

#include <cstdio>
#include <string>

using tyr::modules::names::nameFromDump;
using tyr::modules::names::prettifyKey;

static int g_checks = 0;
static int g_failed = 0;

static void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        ++g_failed;
        std::printf("FAIL  %s\n", what);
    }
}

int main() {
    // ---- the exact dump a golden apple comes back as ----
    check(nameFromDump("translation{key='item.minecraft.golden_apple', args=[]}") == "Golden Apple",
          "a plain item's dump reads as its name, not the component internals");

    // ---- the shapes the key is written in: single quotes, double quotes, bare ----
    check(nameFromDump("translation{key=\"item.minecraft.golden_apple\"}") == "Golden Apple",
          "a double-quoted key reads the same");
    check(nameFromDump("translation{key=item.minecraft.golden_apple}") == "Golden Apple",
          "a bare key reads the same");

    // ---- the name never swallows the dump's own tail ----
    check(nameFromDump("translation{key='item.minecraft.golden_apple', args=[]}") .find("args") == std::string::npos,
          "the args list after the key never becomes part of the name");
    check(nameFromDump("translation{key='item.minecraft.enchanted_golden_apple', args=[1, 2]}") ==
              "Enchanted Golden Apple",
          "a multi-arg dump is still just the name");

    // ---- prefixes: the namespace and the registry both go ----
    check(prettifyKey("item.minecraft.golden_apple") == "Golden Apple",
          "namespace prefix and underscores are handled");
    check(prettifyKey("block.minecraft.oak_planks") == "Oak Planks", "block keys work the same");
    check(prettifyKey("minecraft:diamond_sword") == "Diamond Sword", "a colon-namespaced key works");
    check(prettifyKey("golden_apple") == "Golden Apple", "a bare key with no prefix is prettified");
    check(prettifyKey("minecraft:trial_key") == "Trial Key", "the last separator wins");

    // ---- capitals inside a key are left alone, not re-cased ----
    check(prettifyKey("item.minecraft.tNT") == "TNT", "an already-capitalised word is untouched");
    check(prettifyKey("item.minecraft.axolotl_bucket") == "Axolotl Bucket",
          "only a word start is capitalised");

    // ---- nothing to read: empty, not a mangled string ----
    check(nameFromDump("") == "", "an empty dump gives nothing");
    check(nameFromDump("literal{Golden Apple}") == "", "a literal component's dump gives nothing");
    check(nameFromDump("translation{key='', args=[]}") == "", "an empty key gives nothing");
    check(nameFromDump("translation{args=[], key='item.minecraft.apple'}") == "Apple",
          "the key is found wherever it sits in the dump");

    // ---- the caller's own guard: a resolved name is never mistaken for a dump ----
    check(nameFromDump("Golden Apple") == "", "a name that already resolved has no key to read");

    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    std::printf("%s\n", g_failed == 0 ? "PASS" : "FAILED");
    return g_failed == 0 ? 0 : 1;
}
