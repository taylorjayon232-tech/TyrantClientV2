// ============================================================================
//  Harness: the pure player-detection rules (agent/modules/player_detect.h).
// ----------------------------------------------------------------------------
//  The rules decide who every combat module may act on, and the inputs they see
//  in game are what plugins paint on names and UUIDs, so the cases below are the
//  real shapes: rank prefixes, colour codes, zero-width padding, NPC markers,
//  online (v4) and offline (v3) accounts, and plugin-generated UUIDs.
//
//  Build + run:  test\build-detect.bat   (prints "PASS" or the failing cases)
// ============================================================================
#include "../agent/modules/player_detect.h"

#include <cstdio>
#include <string>

using namespace tyr::modules;

static int gChecks = 0;
static int gFails = 0;

static void check(bool ok, const char* what) {
    ++gChecks;
    if (!ok) { ++gFails; std::printf("FAIL  %s\n", what); }
}

static void checkEq(const std::string& got, const std::string& want, const char* what) {
    ++gChecks;
    if (got != want) {
        ++gFails;
        std::printf("FAIL  %s\n        got  \"%s\"\n        want \"%s\"\n", what, got.c_str(), want.c_str());
    }
}

int main() {
    const std::string sect = "\xC2\xA7";        // § as the JVM hands a name over (UTF-8)
    const std::string zwsp = "\xE2\x80\x8B";    // zero-width space: NPC name padding

    // ---- names: what a module should compare and display ---------------------
    checkEq(detect::cleanPlayerName("Notch"), "Notch", "plain name unchanged");
    checkEq(detect::cleanPlayerName(sect + "cNotch"), "Notch", "section colour code stripped");
    checkEq(detect::cleanPlayerName("&aNotch"), "Notch", "ampersand colour code stripped");
    checkEq(detect::cleanPlayerName(sect + "l" + sect + "6Notch"), "Notch", "two codes stripped");
    checkEq(detect::cleanPlayerName("[VIP] Notch"), "Notch", "rank prefix stripped");
    checkEq(detect::cleanPlayerName(sect + "c[NPC] " + sect + "fShop"), "Shop", "colour + NPC tag stripped");
    checkEq(detect::cleanPlayerName("(Team) Jayon"), "Jayon", "team tag stripped");
    checkEq(detect::cleanPlayerName("<T> Jayon"), "Jayon", "angle tag stripped");
    checkEq(detect::cleanPlayerName("{g}Jayon"), "Jayon", "brace tag stripped");
    checkEq(detect::cleanPlayerName("Notch:"), "Notch", "trailing colon stripped");
    checkEq(detect::cleanPlayerName("Shopkeeper (NPC)"), "Shopkeeper (NPC)", "tag is only stripped at the front");
    checkEq(detect::cleanPlayerName("Jo" + zwsp + "hn"), "John", "zero-width padding stripped");
    checkEq(detect::cleanPlayerName("  Jayon  "), "Jayon", "surrounding spaces trimmed");
    checkEq(detect::cleanPlayerName("Mega   Knight"), "Mega Knight", "space runs collapsed");
    checkEq(detect::cleanPlayerName("xX_Killer_Xx"), "xX_Killer_Xx", "underscores in a real name kept");
    checkEq(detect::cleanPlayerName("[VIP]xX_Killer_Xx"), "xX_Killer_Xx", "tag without space stripped");
    checkEq(detect::cleanPlayerName("Notch | Shop"), "Notch | Shop", "inner separator kept");
    checkEq(detect::cleanPlayerName("[VIP]"), "", "a name that was only a tag");
    checkEq(detect::cleanPlayerName(sect + "c"), "", "a name that was only a code");
    checkEq(detect::cleanPlayerName(""), "", "empty stays empty");

    // ---- names: bot / NPC markers (whole tokens only) ------------------------
    check(detect::isBotNameMarker("npc"), "marker: npc");
    check(detect::isBotNameMarker("npc_shop"), "marker: npc_shop");
    check(detect::isBotNameMarker(detect::lowerCopy(detect::cleanPlayerName("[NPC] Shop"))), "marker: [NPC] Shop");
    check(detect::isBotNameMarker("shopkeeper"), "marker: shopkeeper");
    check(detect::isBotNameMarker("bot"), "marker: bot");
    check(detect::isBotNameMarker("guard"), "marker: guard");
    check(!detect::isBotNameMarker("npcguy"), "not a marker: npcguy (no token break)");
    check(!detect::isBotNameMarker("robot"), "not a marker: robot");
    check(!detect::isBotNameMarker("guardian"), "not a marker: guardian");
    check(!detect::isBotNameMarker("notch"), "not a marker: notch");
    check(!detect::isBotNameMarker("jayon"), "not a marker: jayon");
    check(!detect::isBotNameMarker(""), "not a marker: empty");

    // ---- UUIDs ---------------------------------------------------------------
    const std::string online = "069a79f4-44e9-4726-a5be-fca90e38aaf5";       // a real v4 account
    const std::string offline = "b50ad385-829d-3141-a216-7e7d7539ba7f";      // offline-mode v3
    const std::string pluginV2 = "a1b2c3d4-e5f6-2a3b-8c9d-0e1f2a3b4c5d";     // plugin-generated
    check(detect::looksLikeUuid(online), "uuid: dashed v4 reads as a UUID");
    check(detect::looksLikeUuid(detect::uuidKey(online)), "uuid: dashless 32 hex reads as a UUID");
    check(!detect::looksLikeUuid(""), "uuid: empty is not a UUID");
    check(!detect::looksLikeUuid("Shop"), "uuid: a name is not a UUID");
    check(!detect::looksLikeUuid("069a79f4-44e9-4726-a5be-fca90e38aaf"), "uuid: 31 hex is not a UUID");
    check(detect::uuidVersion(online) == '4', "uuid: v4 detected");
    check(detect::uuidVersion(offline) == '3', "uuid: v3 detected");
    check(detect::uuidVersion(pluginV2) == '2', "uuid: v2 detected");
    check(detect::uuidVersion(detect::uuidKey(online)) == '4', "uuid: version read without dashes");
    check(detect::uuidVersion("") == 0, "uuid: empty has no version");
    check(detect::uuidVersion("npc-shop") == 0, "uuid: junk has no version");
    check(detect::uuidKey(online) == "069a79f444e94726a5befca90e38aaf5", "uuid: key is dashless and lower-case");
    check(detect::uuidKey("069A79F4-44E9-4726-A5BE-FCA90E38AAF5") == detect::uuidKey(online),
          "uuid: key is case-insensitive");

    // ---- UUID verdicts -------------------------------------------------------
    check(detect::isAccountUuid('4', false), "account: v4 online");
    check(detect::isAccountUuid('3', true), "account: v3 counts while offline UUIDs are allowed");
    check(!detect::isAccountUuid('3', false), "account: v3 rejected in strict mode");
    check(!detect::isAccountUuid('2', true), "account: v2 is never an account");
    check(!detect::isAccountUuid(0, true), "account: unreadable UUID is not an account");
    check(!detect::uuidIsBotShape('4', true), "bot shape: v4 is fine");
    check(!detect::uuidIsBotShape('3', true), "bot shape: v3 is fine by default");
    check(detect::uuidIsBotShape('3', false), "bot shape: v3 flagged in strict mode");
    check(detect::uuidIsBotShape('2', true), "bot shape: v2 flagged");
    check(detect::uuidIsBotShape('5', true), "bot shape: v5 flagged");
    check(!detect::uuidIsBotShape(0, true), "bot shape: unreadable UUID is not a verdict");

    // ---- clone detection -----------------------------------------------------
    const std::string selfKey = detect::uuidKey(online);
    const std::string weakKey = detect::uuidKey(pluginV2);
    check(detect::isBotClone("jayon", weakKey, '2', selfKey, true, "jayon", selfKey),
          "clone: an entity wearing the local player's name");
    check(!detect::isBotClone("jayon", selfKey, '4', selfKey, true, "jayon", selfKey),
          "clone: the local player is not a clone of themselves");
    check(detect::isBotClone("notch", weakKey, '2', selfKey, true, "jayon", selfKey),
          "clone: a weak UUID wearing a name another account owns");
    check(!detect::isBotClone("notch", detect::uuidKey(offline), '3', selfKey, true, "jayon", selfKey),
          "clone: a v3 (offline) duplicate is not flagged by default");
    check(detect::isBotClone("notch", detect::uuidKey(offline), '3', selfKey, false, "jayon", selfKey),
          "clone: a v3 duplicate is flagged in strict mode");
    const std::string otherV4 = "11111111111111111111111111111111";
    check(!detect::isBotClone("notch", otherV4, '4', selfKey, true, "jayon", selfKey),
          "clone: a v4 duplicate is left alone (no way to tell)");
    check(!detect::isBotClone("notch", weakKey, '2', "", true, "jayon", selfKey),
          "clone: no owner means no verdict");
    check(!detect::isBotClone("notch", weakKey, '2', weakKey, true, "jayon", selfKey),
          "clone: the owner is not a clone of itself");
    check(!detect::isBotClone("", weakKey, '2', selfKey, true, "jayon", selfKey),
          "clone: an empty name has no verdict");

    std::printf("%d checks, %d failed\n", gChecks, gFails);
    std::printf("%s\n", gFails == 0 ? "PASS" : "FAIL");
    return gFails == 0 ? 0 : 1;
}
