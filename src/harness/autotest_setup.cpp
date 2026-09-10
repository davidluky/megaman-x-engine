// autotest_setup.cpp - prepares player inventory and upgrades for test profiles.
// Boundary: test-only setup does not replace normal progression grants.

#include "harness/autotest_setup.h"

#include "entities/player.h"
#include "systems/weapon.h"

#include <raylib.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>

namespace mmx::autotest_setup {
namespace {

constexpr const char* kReferenceWeaponOrder[] = {
    "homing-torpedo",
    "chameleon-sting",
    "rolling-shield",
    "fire-wave",
    "storm-tornado",
    "electric-spark",
    "boomerang-cutter",
    "shotgun-ice",
};

} // namespace

void grantAllWeapons(Player& player, bool grantBusterArmor) {
    // Keep screenshot order aligned with weapon_palette_rows.json cursor order.
    player.weaponInventory.init();
    for (const char* weaponId : kReferenceWeaponOrder) {
        auto weapon = weapons::makeById(WeaponId::fromString(weaponId));
        if (!weapon) {
            TraceLog(LOG_WARNING, "Autotest: missing weapon id %s",
                     weaponId);
            continue;
        }
        player.weaponInventory.addWeapon(*weapon);
    }
    for (std::size_t i = 0; i < player.weaponInventory.weaponCount(); ++i) {
        player.weaponInventory.setAmmo(i, player.weaponInventory.weaponAt(i).maxAmmo);
    }
    player.weaponInventory.currentIndex = 0;
    if (const char* slot = std::getenv("MMX_AUTOTEST_WEAPON_SLOT")) {
        if (*slot) {
            char* end = nullptr;
            const long parsed = std::strtol(slot, &end, 10);
            if (end != slot && *end == '\0') {
                const int maxIndex = static_cast<int>(player.weaponInventory.weaponCount()) - 1;
                player.weaponInventory.currentIndex =
                    static_cast<int>(std::clamp<long>(parsed, 0, maxIndex));
            }
        }
    }

    // Autotest profiles match the all-weapons fixture and need the real arm
    // upgrade. Manual F4 review must not mutate armor sprites, so it uses a
    // scene-local special-charge override instead.
    if (grantBusterArmor) {
        player.grantArmorBuster();
        player.setDebugSpecialChargeUnlocked(false);
    } else {
        player.setDebugSpecialChargeUnlocked(true);
    }
    TraceLog(LOG_INFO, "Autotest: granted %zu weapons for weapon-cycle profile",
             player.weaponInventory.weaponCount());
}

} // namespace mmx::autotest_setup
