// mmx1_password.cpp - encodes and decodes original MMX1 password digits.
// Owns: password parity rules and conversion into engine save data.

#include "data/mmx1_password.h"

#include <algorithm>
#include <cctype>

namespace mmx::mmx1_password {
namespace {

int bit(bool value) {
    return value ? 1 : 0;
}

bool odd(int value) {
    return (value & 1) != 0;
}

int bossCount(const PasswordFlags& f) {
    return bit(f.bossArmoredArmadillo) + bit(f.bossLaunchOctopus) +
           bit(f.bossChillPenguin) + bit(f.bossFlameMammoth) +
           bit(f.bossStormEagle) + bit(f.bossSparkMandrill) +
           bit(f.bossStingChameleon) + bit(f.bossBoomerKuwanger);
}

int heartCount(const PasswordFlags& f) {
    return bit(f.heartLaunchOctopus) + bit(f.heartChillPenguin) +
           bit(f.heartFlameMammoth) + bit(f.heartBoomerKuwanger) +
           bit(f.heartStingChameleon) + bit(f.heartSparkMandrill) +
           bit(f.heartStormEagle) + bit(f.heartArmoredArmadillo);
}

int subcapCount(const PasswordFlags& f) {
    return bit(f.subTankChillPenguin) + bit(f.subTankStormEagle) +
           bit(f.subTankStingChameleon) + bit(f.subTankFlameMammoth) +
           bit(f.armorBoots) + bit(f.armorHelmet) + bit(f.armorBuster) +
           bit(f.armorBody);
}

bool parity0(const PasswordFlags& f) {
    return odd(bit(f.bossChillPenguin) + bit(f.bossFlameMammoth) +
               bit(f.subTankStormEagle) + bit(f.subTankStingChameleon) +
               bit(f.subTankFlameMammoth) + bit(f.armorHelmet) +
               bit(f.armorBuster) + bit(f.heartLaunchOctopus) +
               bit(f.heartStingChameleon));
}

bool parity4(const PasswordFlags& f) {
    return odd(bit(f.bossChillPenguin) + bit(f.bossFlameMammoth) +
               bit(f.bossBoomerKuwanger) + bit(f.bossStormEagle) +
               bit(f.bossArmoredArmadillo) + bit(f.subTankFlameMammoth) +
               bit(f.armorBody) + bit(f.heartBoomerKuwanger) +
               bit(f.heartArmoredArmadillo));
}

bool parity6(const PasswordFlags& f) {
    return odd(bit(f.bossLaunchOctopus) + bit(f.bossStormEagle) +
               bit(f.subTankStormEagle) + bit(f.heartChillPenguin) +
               bit(f.heartFlameMammoth) + bit(f.armorBuster));
}

bool parity8(const PasswordFlags& f) {
    return odd(bit(f.bossBoomerKuwanger) + bit(f.bossArmoredArmadillo) +
               bit(f.subTankStingChameleon) + bit(f.armorHelmet) +
               bit(f.heartLaunchOctopus) + bit(f.heartChillPenguin) +
               bit(f.heartFlameMammoth) + bit(f.heartBoomerKuwanger) +
               bit(f.heartStingChameleon));
}

bool decodeP0(int digit, bool& parity, bool& armorBody, bool& heartChillPenguin) {
    switch (digit) {
        case 8: parity = true;  armorBody = true;  heartChillPenguin = true;  return true;
        case 6: parity = true;  armorBody = true;  heartChillPenguin = false; return true;
        case 2: parity = true;  armorBody = false; heartChillPenguin = true;  return true;
        case 4: parity = true;  armorBody = false; heartChillPenguin = false; return true;
        case 3: parity = false; armorBody = true;  heartChillPenguin = true;  return true;
        case 5: parity = false; armorBody = true;  heartChillPenguin = false; return true;
        case 7: parity = false; armorBody = false; heartChillPenguin = true;  return true;
        case 1: parity = false; armorBody = false; heartChillPenguin = false; return true;
        default: return false;
    }
}

bool decodeP1(int digit, bool& heartParity, bool& bossFlameMammoth, bool& subTankStormEagle) {
    switch (digit) {
        case 1: heartParity = true;  bossFlameMammoth = true;  subTankStormEagle = true;  return true;
        case 2: heartParity = true;  bossFlameMammoth = true;  subTankStormEagle = false; return true;
        case 5: heartParity = true;  bossFlameMammoth = false; subTankStormEagle = true;  return true;
        case 8: heartParity = true;  bossFlameMammoth = false; subTankStormEagle = false; return true;
        case 4: heartParity = false; bossFlameMammoth = true;  subTankStormEagle = true;  return true;
        case 3: heartParity = false; bossFlameMammoth = true;  subTankStormEagle = false; return true;
        case 6: heartParity = false; bossFlameMammoth = false; subTankStormEagle = true;  return true;
        case 7: heartParity = false; bossFlameMammoth = false; subTankStormEagle = false; return true;
        default: return false;
    }
}

bool decodeP2(int digit, bool& armorBoots, bool& heartFlameMammoth) {
    switch (digit) {
        case 4: armorBoots = true;  heartFlameMammoth = true;  return true;
        case 2: armorBoots = true;  heartFlameMammoth = false; return true;
        case 7: armorBoots = false; heartFlameMammoth = true;  return true;
        case 6: armorBoots = false; heartFlameMammoth = false; return true;
        default: return false;
    }
}

bool decodeP3(int digit, bool& bossStormEagle, bool& heartStormEagle) {
    switch (digit) {
        case 1: bossStormEagle = true;  heartStormEagle = true;  return true;
        case 7: bossStormEagle = true;  heartStormEagle = false; return true;
        case 8: bossStormEagle = false; heartStormEagle = true;  return true;
        case 4: bossStormEagle = false; heartStormEagle = false; return true;
        default: return false;
    }
}

bool decodeP4(int digit, bool& parity, bool& bossLaunchOctopus, bool& subTankStingChameleon) {
    switch (digit) {
        case 2: parity = true;  bossLaunchOctopus = true;  subTankStingChameleon = true;  return true;
        case 4: parity = true;  bossLaunchOctopus = true;  subTankStingChameleon = false; return true;
        case 7: parity = true;  bossLaunchOctopus = false; subTankStingChameleon = true;  return true;
        case 8: parity = true;  bossLaunchOctopus = false; subTankStingChameleon = false; return true;
        case 3: parity = false; bossLaunchOctopus = true;  subTankStingChameleon = true;  return true;
        case 6: parity = false; bossLaunchOctopus = true;  subTankStingChameleon = false; return true;
        case 5: parity = false; bossLaunchOctopus = false; subTankStingChameleon = true;  return true;
        case 1: parity = false; bossLaunchOctopus = false; subTankStingChameleon = false; return true;
        default: return false;
    }
}

bool decodeP5(int digit, bool& subcapParity, bool& bossBoomerKuwanger, bool& heartBoomerKuwanger) {
    switch (digit) {
        case 3: subcapParity = true;  bossBoomerKuwanger = true;  heartBoomerKuwanger = true;  return true;
        case 2: subcapParity = true;  bossBoomerKuwanger = true;  heartBoomerKuwanger = false; return true;
        case 5: subcapParity = true;  bossBoomerKuwanger = false; heartBoomerKuwanger = true;  return true;
        case 7: subcapParity = true;  bossBoomerKuwanger = false; heartBoomerKuwanger = false; return true;
        case 1: subcapParity = false; bossBoomerKuwanger = true;  heartBoomerKuwanger = true;  return true;
        case 8: subcapParity = false; bossBoomerKuwanger = true;  heartBoomerKuwanger = false; return true;
        case 6: subcapParity = false; bossBoomerKuwanger = false; heartBoomerKuwanger = true;  return true;
        case 4: subcapParity = false; bossBoomerKuwanger = false; heartBoomerKuwanger = false; return true;
        default: return false;
    }
}

bool decodeP6(int digit, bool& parity, bool& bossArmoredArmadillo, bool& subTankFlameMammoth) {
    switch (digit) {
        case 7: parity = true;  bossArmoredArmadillo = true;  subTankFlameMammoth = true;  return true;
        case 4: parity = true;  bossArmoredArmadillo = true;  subTankFlameMammoth = false; return true;
        case 2: parity = true;  bossArmoredArmadillo = false; subTankFlameMammoth = true;  return true;
        case 1: parity = true;  bossArmoredArmadillo = false; subTankFlameMammoth = false; return true;
        case 3: parity = false; bossArmoredArmadillo = true;  subTankFlameMammoth = true;  return true;
        case 5: parity = false; bossArmoredArmadillo = true;  subTankFlameMammoth = false; return true;
        case 6: parity = false; bossArmoredArmadillo = false; subTankFlameMammoth = true;  return true;
        case 8: parity = false; bossArmoredArmadillo = false; subTankFlameMammoth = false; return true;
        default: return false;
    }
}

bool decodeP7(int digit, bool& bossSparkMandrill, bool& heartStingChameleon) {
    switch (digit) {
        case 6: bossSparkMandrill = true;  heartStingChameleon = true;  return true;
        case 7: bossSparkMandrill = true;  heartStingChameleon = false; return true;
        case 2: bossSparkMandrill = false; heartStingChameleon = true;  return true;
        case 8: bossSparkMandrill = false; heartStingChameleon = false; return true;
        default: return false;
    }
}

bool decodeP8(int digit, bool& parity, bool& subTankChillPenguin, bool& heartArmoredArmadillo) {
    switch (digit) {
        case 4: parity = true;  subTankChillPenguin = true;  heartArmoredArmadillo = true;  return true;
        case 1: parity = true;  subTankChillPenguin = true;  heartArmoredArmadillo = false; return true;
        case 2: parity = true;  subTankChillPenguin = false; heartArmoredArmadillo = true;  return true;
        case 3: parity = true;  subTankChillPenguin = false; heartArmoredArmadillo = false; return true;
        case 6: parity = false; subTankChillPenguin = true;  heartArmoredArmadillo = true;  return true;
        case 8: parity = false; subTankChillPenguin = true;  heartArmoredArmadillo = false; return true;
        case 5: parity = false; subTankChillPenguin = false; heartArmoredArmadillo = true;  return true;
        case 7: parity = false; subTankChillPenguin = false; heartArmoredArmadillo = false; return true;
        default: return false;
    }
}

bool decodeP9(int digit, bool& bossStingChameleon, bool& armorBuster) {
    switch (digit) {
        case 4: bossStingChameleon = true;  armorBuster = true;  return true;
        case 6: bossStingChameleon = true;  armorBuster = false; return true;
        case 5: bossStingChameleon = false; armorBuster = true;  return true;
        case 7: bossStingChameleon = false; armorBuster = false; return true;
        default: return false;
    }
}

bool decodeP10(int digit, bool& bossParity, bool& heartLaunchOctopus, bool& armorHelmet) {
    switch (digit) {
        case 6: bossParity = true;  heartLaunchOctopus = true;  armorHelmet = true;  return true;
        case 3: bossParity = true;  heartLaunchOctopus = true;  armorHelmet = false; return true;
        case 7: bossParity = true;  heartLaunchOctopus = false; armorHelmet = true;  return true;
        case 1: bossParity = true;  heartLaunchOctopus = false; armorHelmet = false; return true;
        case 2: bossParity = false; heartLaunchOctopus = true;  armorHelmet = true;  return true;
        case 5: bossParity = false; heartLaunchOctopus = true;  armorHelmet = false; return true;
        case 8: bossParity = false; heartLaunchOctopus = false; armorHelmet = true;  return true;
        case 4: bossParity = false; heartLaunchOctopus = false; armorHelmet = false; return true;
        default: return false;
    }
}

bool decodeP11(int digit, bool& bossChillPenguin, bool& heartSparkMandrill) {
    switch (digit) {
        case 1: bossChillPenguin = true;  heartSparkMandrill = true;  return true;
        case 6: bossChillPenguin = true;  heartSparkMandrill = false; return true;
        case 4: bossChillPenguin = false; heartSparkMandrill = true;  return true;
        case 8: bossChillPenguin = false; heartSparkMandrill = false; return true;
        default: return false;
    }
}

void addWeapon(SaveData& save, const char* id) {
    save.weapons.push_back(SaveData::WeaponData{id});
    save.ammo.push_back(28);
}

void addCompletedBoss(SaveData& save, const char* stage, const char* boss, const char* weapon) {
    save.completedStages.push_back(stage);
    save.defeatedBosses.push_back(boss);
    addWeapon(save, weapon);
}

bool contains(const std::vector<std::string>& values, std::string_view target) {
    return std::find(values.begin(), values.end(), target) != values.end();
}

bool hasHeartMarker(const SaveData& save, std::string_view stage) {
    const std::string runtimePrefix = std::string(stage) + ":pickup:";
    const std::string importedMarker = "password:heart:" + std::string(stage);
    return std::any_of(
        save.collectedPickups.begin(),
        save.collectedPickups.end(),
        [&](const std::string& value) {
            return value == importedMarker ||
                   (value.rfind(runtimePrefix, 0) == 0 &&
                    value.find(":heart-tank:") != std::string::npos);
        }
    );
}

void addHeartMarker(SaveData& save, bool collected, const char* stage) {
    if (collected) {
        save.collectedPickups.push_back(std::string("password:heart:") + stage);
    }
}

} // namespace

PasswordFlags emptyFlags() {
    return {};
}

PasswordFlags allUnlocksFlags() {
    PasswordFlags flags;
    flags.bossArmoredArmadillo = true;
    flags.bossLaunchOctopus = true;
    flags.bossChillPenguin = true;
    flags.bossFlameMammoth = true;
    flags.bossStormEagle = true;
    flags.bossSparkMandrill = true;
    flags.bossStingChameleon = true;
    flags.bossBoomerKuwanger = true;

    flags.heartLaunchOctopus = true;
    flags.heartChillPenguin = true;
    flags.heartFlameMammoth = true;
    flags.heartBoomerKuwanger = true;
    flags.heartStingChameleon = true;
    flags.heartSparkMandrill = true;
    flags.heartStormEagle = true;
    flags.heartArmoredArmadillo = true;

    flags.subTankChillPenguin = true;
    flags.subTankStormEagle = true;
    flags.subTankStingChameleon = true;
    flags.subTankFlameMammoth = true;

    flags.armorBoots = true;
    flags.armorHelmet = true;
    flags.armorBuster = true;
    flags.armorBody = true;
    return flags;
}

std::optional<PasswordDigits> normalizeDigits(std::string_view text) {
    PasswordDigits digits = {};
    int count = 0;
    for (unsigned char c : text) {
        if (std::isspace(c)) {
            continue;
        }
        if (c < '1' || c > '8') {
            return std::nullopt;
        }
        if (count >= kPasswordDigitCount) {
            return std::nullopt;
        }
        digits[static_cast<std::size_t>(count)] = static_cast<int>(c - '0');
        ++count;
    }

    if (count != kPasswordDigitCount) {
        return std::nullopt;
    }
    return digits;
}

std::string formatDigits(const PasswordDigits& digits, bool grouped) {
    std::string out;
    out.reserve(grouped ? 14 : 12);
    for (int i = 0; i < kPasswordDigitCount; ++i) {
        if (grouped && i > 0 && (i % 4) == 0) {
            out.push_back(' ');
        }
        out.push_back(static_cast<char>('0' + digits[static_cast<std::size_t>(i)]));
    }
    return out;
}

PasswordDigits generateDigits(const PasswordFlags& f) {
    PasswordDigits o = {};

    if (parity0(f)) {
        if (f.armorBody) {
            o[0] = f.heartChillPenguin ? 8 : 6;
        } else {
            o[0] = f.heartChillPenguin ? 2 : 4;
        }
    } else {
        if (f.armorBody) {
            o[0] = f.heartChillPenguin ? 3 : 5;
        } else {
            o[0] = f.heartChillPenguin ? 7 : 1;
        }
    }

    if (odd(heartCount(f))) {
        if (f.bossFlameMammoth) {
            o[1] = f.subTankStormEagle ? 1 : 2;
        } else {
            o[1] = f.subTankStormEagle ? 5 : 8;
        }
    } else {
        if (f.bossFlameMammoth) {
            o[1] = f.subTankStormEagle ? 4 : 3;
        } else {
            o[1] = f.subTankStormEagle ? 6 : 7;
        }
    }

    o[2] = f.armorBoots ? (f.heartFlameMammoth ? 4 : 2)
                        : (f.heartFlameMammoth ? 7 : 6);
    o[3] = f.bossStormEagle ? (f.heartStormEagle ? 1 : 7)
                            : (f.heartStormEagle ? 8 : 4);

    if (parity4(f)) {
        if (f.bossLaunchOctopus) {
            o[4] = f.subTankStingChameleon ? 2 : 4;
        } else {
            o[4] = f.subTankStingChameleon ? 7 : 8;
        }
    } else {
        if (f.bossLaunchOctopus) {
            o[4] = f.subTankStingChameleon ? 3 : 6;
        } else {
            o[4] = f.subTankStingChameleon ? 5 : 1;
        }
    }

    if (odd(subcapCount(f))) {
        if (f.bossBoomerKuwanger) {
            o[5] = f.heartBoomerKuwanger ? 3 : 2;
        } else {
            o[5] = f.heartBoomerKuwanger ? 5 : 7;
        }
    } else {
        if (f.bossBoomerKuwanger) {
            o[5] = f.heartBoomerKuwanger ? 1 : 8;
        } else {
            o[5] = f.heartBoomerKuwanger ? 6 : 4;
        }
    }

    if (parity6(f)) {
        if (f.bossArmoredArmadillo) {
            o[6] = f.subTankFlameMammoth ? 7 : 4;
        } else {
            o[6] = f.subTankFlameMammoth ? 2 : 1;
        }
    } else {
        if (f.bossArmoredArmadillo) {
            o[6] = f.subTankFlameMammoth ? 3 : 5;
        } else {
            o[6] = f.subTankFlameMammoth ? 6 : 8;
        }
    }

    o[7] = f.bossSparkMandrill ? (f.heartStingChameleon ? 6 : 7)
                               : (f.heartStingChameleon ? 2 : 8);

    if (parity8(f)) {
        if (f.subTankChillPenguin) {
            o[8] = f.heartArmoredArmadillo ? 4 : 1;
        } else {
            o[8] = f.heartArmoredArmadillo ? 2 : 3;
        }
    } else {
        if (f.subTankChillPenguin) {
            o[8] = f.heartArmoredArmadillo ? 6 : 8;
        } else {
            o[8] = f.heartArmoredArmadillo ? 5 : 7;
        }
    }

    o[9] = f.bossStingChameleon ? (f.armorBuster ? 4 : 6)
                                : (f.armorBuster ? 5 : 7);

    if (odd(bossCount(f))) {
        if (f.heartLaunchOctopus) {
            o[10] = f.armorHelmet ? 6 : 3;
        } else {
            o[10] = f.armorHelmet ? 7 : 1;
        }
    } else {
        if (f.heartLaunchOctopus) {
            o[10] = f.armorHelmet ? 2 : 5;
        } else {
            o[10] = f.armorHelmet ? 8 : 4;
        }
    }

    o[11] = f.bossChillPenguin ? (f.heartSparkMandrill ? 1 : 6)
                               : (f.heartSparkMandrill ? 4 : 8);

    return o;
}

PasswordFlags flagsFromSaveData(const SaveData& save) {
    PasswordFlags flags;
    const auto hasBoss = [&](const char* id) {
        return contains(save.defeatedBosses, id) ||
               contains(save.completedStages, id);
    };

    flags.bossArmoredArmadillo = hasBoss("armored-armadillo");
    flags.bossLaunchOctopus = hasBoss("launch-octopus");
    flags.bossChillPenguin = hasBoss("chill-penguin");
    flags.bossFlameMammoth = hasBoss("flame-mammoth");
    flags.bossStormEagle = hasBoss("storm-eagle");
    flags.bossSparkMandrill = hasBoss("spark-mandrill");
    flags.bossStingChameleon = hasBoss("sting-chameleon");
    flags.bossBoomerKuwanger = hasBoss("boomer-kuwanger");

    flags.heartLaunchOctopus = hasHeartMarker(save, "launch-octopus");
    flags.heartChillPenguin = hasHeartMarker(save, "chill-penguin");
    flags.heartFlameMammoth = hasHeartMarker(save, "flame-mammoth");
    flags.heartBoomerKuwanger = hasHeartMarker(save, "boomer-kuwanger");
    flags.heartStingChameleon = hasHeartMarker(save, "sting-chameleon");
    flags.heartSparkMandrill = hasHeartMarker(save, "spark-mandrill");
    flags.heartStormEagle = hasHeartMarker(save, "storm-eagle");
    flags.heartArmoredArmadillo = hasHeartMarker(save, "armored-armadillo");

    flags.subTankChillPenguin = save.subTanks[0].collected;
    flags.subTankStormEagle = save.subTanks[1].collected;
    flags.subTankStingChameleon = save.subTanks[2].collected;
    flags.subTankFlameMammoth = save.subTanks[3].collected;
    flags.armorBoots = save.armorBoots;
    flags.armorHelmet = save.armorHelmet;
    flags.armorBuster = save.armorBuster;
    flags.armorBody = save.armorBody;
    return flags;
}

std::optional<PasswordFlags> decodeFlags(const PasswordDigits& digits) {
    PasswordFlags f;
    bool expectedHeartParity = false;
    bool expectedSubcapParity = false;
    bool expectedBossParity = false;
    bool expectedP0 = false;
    bool expectedP4 = false;
    bool expectedP6 = false;
    bool expectedP8 = false;

    if (!decodeP0(digits[0], expectedP0, f.armorBody, f.heartChillPenguin) ||
        !decodeP1(digits[1], expectedHeartParity, f.bossFlameMammoth, f.subTankStormEagle) ||
        !decodeP2(digits[2], f.armorBoots, f.heartFlameMammoth) ||
        !decodeP3(digits[3], f.bossStormEagle, f.heartStormEagle) ||
        !decodeP4(digits[4], expectedP4, f.bossLaunchOctopus, f.subTankStingChameleon) ||
        !decodeP5(digits[5], expectedSubcapParity, f.bossBoomerKuwanger, f.heartBoomerKuwanger) ||
        !decodeP6(digits[6], expectedP6, f.bossArmoredArmadillo, f.subTankFlameMammoth) ||
        !decodeP7(digits[7], f.bossSparkMandrill, f.heartStingChameleon) ||
        !decodeP8(digits[8], expectedP8, f.subTankChillPenguin, f.heartArmoredArmadillo) ||
        !decodeP9(digits[9], f.bossStingChameleon, f.armorBuster) ||
        !decodeP10(digits[10], expectedBossParity, f.heartLaunchOctopus, f.armorHelmet) ||
        !decodeP11(digits[11], f.bossChillPenguin, f.heartSparkMandrill)) {
        return std::nullopt;
    }

    if (expectedHeartParity != odd(heartCount(f)) ||
        expectedSubcapParity != odd(subcapCount(f)) ||
        expectedBossParity != odd(bossCount(f)) ||
        expectedP0 != parity0(f) ||
        expectedP4 != parity4(f) ||
        expectedP6 != parity6(f) ||
        expectedP8 != parity8(f)) {
        return std::nullopt;
    }

    return f;
}

std::optional<DecodedPassword> decode(std::string_view text) {
    const auto digits = normalizeDigits(text);
    if (!digits) {
        return std::nullopt;
    }
    const auto flags = decodeFlags(*digits);
    if (!flags) {
        return std::nullopt;
    }
    return DecodedPassword{*digits, *flags, toSaveData(*flags)};
}

SaveData toSaveData(const PasswordFlags& flags) {
    SaveData save;
    save.maxHealth = std::clamp(16 + heartCount(flags) * 2, 16, 32);
    save.armorBoots = flags.armorBoots;
    save.armorHelmet = flags.armorHelmet;
    save.armorBuster = flags.armorBuster;
    save.armorBody = flags.armorBody;

    save.subTanks[0] = {flags.subTankChillPenguin, 0};
    save.subTanks[1] = {flags.subTankStormEagle, 0};
    save.subTanks[2] = {flags.subTankStingChameleon, 0};
    save.subTanks[3] = {flags.subTankFlameMammoth, 0};

    addHeartMarker(save, flags.heartLaunchOctopus, "launch-octopus");
    addHeartMarker(save, flags.heartChillPenguin, "chill-penguin");
    addHeartMarker(save, flags.heartFlameMammoth, "flame-mammoth");
    addHeartMarker(save, flags.heartBoomerKuwanger, "boomer-kuwanger");
    addHeartMarker(save, flags.heartStingChameleon, "sting-chameleon");
    addHeartMarker(save, flags.heartSparkMandrill, "spark-mandrill");
    addHeartMarker(save, flags.heartStormEagle, "storm-eagle");
    addHeartMarker(save, flags.heartArmoredArmadillo, "armored-armadillo");

    save.weapons.push_back(SaveData::WeaponData{"buster"});
    save.ammo.push_back(0);

    if (flags.bossChillPenguin) {
        addCompletedBoss(save, "chill-penguin", "chill-penguin", "shotgun-ice");
    }
    if (flags.bossStormEagle) {
        addCompletedBoss(save, "storm-eagle", "storm-eagle", "storm-tornado");
    }
    if (flags.bossFlameMammoth) {
        addCompletedBoss(save, "flame-mammoth", "flame-mammoth", "fire-wave");
    }
    if (flags.bossSparkMandrill) {
        addCompletedBoss(save, "spark-mandrill", "spark-mandrill", "electric-spark");
    }
    if (flags.bossArmoredArmadillo) {
        addCompletedBoss(save, "armored-armadillo", "armored-armadillo", "rolling-shield");
    }
    if (flags.bossLaunchOctopus) {
        addCompletedBoss(save, "launch-octopus", "launch-octopus", "homing-torpedo");
    }
    if (flags.bossBoomerKuwanger) {
        addCompletedBoss(save, "boomer-kuwanger", "boomer-kuwanger", "boomerang-cutter");
    }
    if (flags.bossStingChameleon) {
        addCompletedBoss(save, "sting-chameleon", "sting-chameleon", "chameleon-sting");
    }

    return save;
}

} // namespace mmx::mmx1_password
