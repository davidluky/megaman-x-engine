// mmx1_password.h - declares MMX1 password flags, digits, and decode helpers.
// Boundary: returns save-compatible data without mutating active save state.

#pragma once

#include "data/save_system.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace mmx::mmx1_password {

inline constexpr int kPasswordDigitCount = 12;
using PasswordDigits = std::array<int, kPasswordDigitCount>;

struct PasswordFlags {
    bool bossArmoredArmadillo = false;
    bool bossLaunchOctopus = false;
    bool bossChillPenguin = false;
    bool bossFlameMammoth = false;
    bool bossStormEagle = false;
    bool bossSparkMandrill = false;
    bool bossStingChameleon = false;
    bool bossBoomerKuwanger = false;

    bool heartLaunchOctopus = false;
    bool heartChillPenguin = false;
    bool heartFlameMammoth = false;
    bool heartBoomerKuwanger = false;
    bool heartStingChameleon = false;
    bool heartSparkMandrill = false;
    bool heartStormEagle = false;
    bool heartArmoredArmadillo = false;

    bool subTankChillPenguin = false;
    bool subTankStormEagle = false;
    bool subTankStingChameleon = false;
    bool subTankFlameMammoth = false;

    bool armorBoots = false;
    bool armorHelmet = false;
    bool armorBuster = false;
    bool armorBody = false;
};

struct DecodedPassword {
    PasswordDigits digits = {};
    PasswordFlags flags;
    SaveData saveData;
};

PasswordFlags emptyFlags();
PasswordFlags allUnlocksFlags();

std::optional<PasswordDigits> normalizeDigits(std::string_view text);
std::string formatDigits(const PasswordDigits& digits, bool grouped = true);

PasswordDigits generateDigits(const PasswordFlags& flags);
PasswordFlags flagsFromSaveData(const SaveData& save);
std::optional<PasswordFlags> decodeFlags(const PasswordDigits& digits);
std::optional<DecodedPassword> decode(std::string_view text);

SaveData toSaveData(const PasswordFlags& flags);

} // namespace mmx::mmx1_password
