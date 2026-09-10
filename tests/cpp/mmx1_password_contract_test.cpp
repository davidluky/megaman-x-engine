#include "data/mmx1_password.h"

#include <algorithm>
#include <cassert>
#include <string>

using namespace mmx;

namespace {

bool hasString(const std::vector<std::string>& values, const std::string& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

bool hasWeapon(const SaveData& save, const std::string& id) {
    return std::find_if(
        save.weapons.begin(),
        save.weapons.end(),
        [&id](const SaveData::WeaponData& weapon) { return weapon.id == id; }
    ) != save.weapons.end();
}

} // namespace

int main() {
    namespace pw = mmx1_password;

    const auto empty = pw::decode("1764 1488 7748");
    assert(empty.has_value());
    assert(pw::formatDigits(empty->digits, true) == "1764 1488 7748");
    assert(pw::formatDigits(pw::generateDigits(pw::emptyFlags()), false) == "176414887748");
    assert(empty->saveData.maxHealth == 16);
    assert(!empty->saveData.armorBoots);
    assert(!empty->saveData.armorHelmet);
    assert(!empty->saveData.armorBuster);
    assert(!empty->saveData.armorBody);
    assert(empty->saveData.completedStages.empty());
    assert(empty->saveData.defeatedBosses.empty());
    assert(empty->saveData.weapons.size() == 1);
    assert(empty->saveData.weapons[0].id == "buster");
    assert(empty->saveData.ammo.size() == 1);
    assert(empty->saveData.ammo[0] == 0);

    const auto allDigits = pw::generateDigits(pw::allUnlocksFlags());
    assert(pw::formatDigits(allDigits, false) == "844121364421");
    assert(pw::formatDigits(allDigits, true) == "8441 2136 4421");
    const auto all = pw::decode("8441 2136 4421");
    assert(all.has_value());
    assert(all->saveData.maxHealth == 32);
    assert(all->saveData.armorBoots);
    assert(all->saveData.armorHelmet);
    assert(all->saveData.armorBuster);
    assert(all->saveData.armorBody);
    for (const auto& tank : all->saveData.subTanks) {
        assert(tank.collected);
        assert(tank.health == 0);
    }
    assert(all->saveData.completedStages.size() == 8);
    assert(all->saveData.defeatedBosses.size() == 8);
    assert(hasString(all->saveData.completedStages, "chill-penguin"));
    assert(hasString(all->saveData.completedStages, "storm-eagle"));
    assert(hasString(all->saveData.completedStages, "flame-mammoth"));
    assert(hasString(all->saveData.completedStages, "spark-mandrill"));
    assert(hasString(all->saveData.completedStages, "armored-armadillo"));
    assert(hasString(all->saveData.completedStages, "launch-octopus"));
    assert(hasString(all->saveData.completedStages, "boomer-kuwanger"));
    assert(hasString(all->saveData.completedStages, "sting-chameleon"));
    assert(all->saveData.weapons.size() == 9);
    assert(all->saveData.ammo.size() == 9);
    assert(all->saveData.ammo[0] == 0);
    for (std::size_t i = 1; i < all->saveData.ammo.size(); ++i) {
        assert(all->saveData.ammo[i] == 28);
    }
    assert(hasWeapon(all->saveData, "shotgun-ice"));
    assert(hasWeapon(all->saveData, "storm-tornado"));
    assert(hasWeapon(all->saveData, "fire-wave"));
    assert(hasWeapon(all->saveData, "electric-spark"));
    assert(hasWeapon(all->saveData, "rolling-shield"));
    assert(hasWeapon(all->saveData, "homing-torpedo"));
    assert(hasWeapon(all->saveData, "boomerang-cutter"));
    assert(hasWeapon(all->saveData, "chameleon-sting"));
    const auto exportedAllFlags = pw::flagsFromSaveData(all->saveData);
    assert(pw::formatDigits(pw::generateDigits(exportedAllFlags), false) == "844121364421");

    SaveData runtimeHeart;
    runtimeHeart.collectedPickups.push_back(
        "storm-eagle:pickup:0:heart-tank:600:32"
    );
    const auto runtimeHeartFlags = pw::flagsFromSaveData(runtimeHeart);
    assert(runtimeHeartFlags.heartStormEagle);
    assert(!runtimeHeartFlags.heartChillPenguin);

    mmx1_password::PasswordFlags bootsOnly;
    bootsOnly.armorBoots = true;
    const auto bootsDigits = pw::generateDigits(bootsOnly);
    assert(pw::formatDigits(bootsDigits, true) == "1724 1788 7748");
    const auto decodedBoots = pw::decode("1724 1788 7748");
    assert(decodedBoots.has_value());
    assert(decodedBoots->saveData.armorBoots);
    assert(!decodedBoots->saveData.armorHelmet);
    assert(decodedBoots->saveData.maxHealth == 16);
    assert(decodedBoots->saveData.weapons.size() == 1);

    const auto mixed = pw::decode("8824 1788 8576");
    assert(mixed.has_value());
    assert(mixed->flags.bossChillPenguin);
    assert(mixed->flags.heartChillPenguin);
    assert(mixed->flags.subTankChillPenguin);
    assert(mixed->flags.armorBoots);
    assert(mixed->flags.armorHelmet);
    assert(mixed->flags.armorBuster);
    assert(mixed->flags.armorBody);
    assert(mixed->saveData.maxHealth == 18);
    assert(mixed->saveData.completedStages.size() == 1);
    assert(mixed->saveData.weapons.size() == 2);
    assert(hasWeapon(mixed->saveData, "shotgun-ice"));

    assert(!pw::normalizeDigits("1764 1488 774").has_value());
    assert(!pw::normalizeDigits("1764 1488 77489").has_value());
    assert(!pw::normalizeDigits("1764 1488 7740").has_value());
    assert(!pw::normalizeDigits("1764 1488 7749").has_value());
    assert(!pw::normalizeDigits("1764 1488 774A").has_value());
    assert(!pw::decode("2764 1488 7748").has_value());

    return 0;
}
