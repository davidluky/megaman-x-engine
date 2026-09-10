// autotest_profiles.cpp - defines finite scripted autotest profile metadata.
// Owns: profile ids, frame caps, weapon-grant flags, and setup modes.

#include "autotest_profiles.h"

namespace mmx::autotest_profiles {
namespace {

constexpr int kUseParityReplayFrameCap = -1;

constexpr Profile kProfiles[] = {
    {"walk", 420, false, Setup::Gameplay},
    {"shoot", 600, false, Setup::Gameplay},
    {"combat", 900, false, Setup::Gameplay},
    {"weapons", 760, true, Setup::Gameplay},
    {"weapon-idle", 120, true, Setup::Gameplay},
    {"slopes", 8000, false, Setup::Gameplay},
    // 520 also covers password reveal/dismiss/destination when the caller sets
    // MMX_AUTOTEST_PLAYER_LIVES=0.
    {"die", 520, false, Setup::DeathPitSpawn},
    {"boss", 1200, false, Setup::BossRoomSpawn},
    {"boss-entry", 340, false, Setup::BossRoomSpawn},
    {"boss-l3", 620, false, Setup::Gameplay},
    {"wallslide", 160, false, Setup::Gameplay},
    {"ladder", 200, false, Setup::Gameplay},
    {"parity-probe", 8, false, Setup::Gameplay},
    {"parity-replay", kUseParityReplayFrameCap, true, Setup::Gameplay},
    {"buster-replay", kUseParityReplayFrameCap, false, Setup::Gameplay},
    {"charge2", 420, false, Setup::Gameplay},
    {"warp", 420, false, Setup::Gameplay},
    {"intro", 620, false, Setup::BossIntroScene},
    {"capsule", 2780, false, Setup::Gameplay},
    {"helmet-capsule", 1200, false, Setup::Gameplay},
    {"body-capsule", 1531, false, Setup::Gameplay},
    {"ice", 1800, true, Setup::Gameplay},
    {"ghost", 2600, true, Setup::Gameplay},
    {"ghost-espark", 900, true, Setup::Gameplay},
    {"ghost-cutter", 800, true, Setup::Gameplay},
    {"ghost-torpedo", 980, true, Setup::Gameplay},
    {"ghost-sting", 900, true, Setup::Gameplay},
    {"ghost-firewave", 800, true, Setup::Gameplay},
    {"ghost-stormtornado", 640, true, Setup::Gameplay},
    {"ghost-rollingshield", 760, true, Setup::Gameplay},
    // 1160 frames covers both recorded source-backed password-grid handoffs:
    // Chill Penguin through tick 1131 and Storm Eagle through tick 1146.
    {"weapon-get", 1160, true, Setup::WeaponGetSequence},
};

const Profile* find(std::string_view profile) {
    for (const auto& candidate : kProfiles) {
        if (profile == candidate.id) return &candidate;
    }
    return nullptr;
}

} // namespace

const Profile* all() {
    return kProfiles;
}

std::size_t count() {
    return sizeof(kProfiles) / sizeof(kProfiles[0]);
}

bool isKnown(std::string_view profile) {
    return find(profile) != nullptr;
}

bool grantsAllWeapons(std::string_view profile) {
    if (const Profile* found = find(profile)) {
        return found->grantAllWeapons;
    }
    return false;
}

Setup setup(std::string_view profile) {
    if (const Profile* found = find(profile)) {
        return found->setup;
    }
    return Setup::Gameplay;
}

bool usesDeathPitSpawn(std::string_view profile) {
    return setup(profile) == Setup::DeathPitSpawn;
}

bool usesBossRoomSpawn(std::string_view profile) {
    return setup(profile) == Setup::BossRoomSpawn;
}

bool usesBossIntroScene(std::string_view profile) {
    return setup(profile) == Setup::BossIntroScene;
}

bool usesWeaponGetSequence(std::string_view profile) {
    return setup(profile) == Setup::WeaponGetSequence;
}

int totalFrames(std::string_view profile, int parityReplayFrames) {
    if (const Profile* found = find(profile)) {
        if (found->defaultTotalFrames == kUseParityReplayFrameCap) {
            return parityReplayFrames;
        }
        return found->defaultTotalFrames;
    }
    return 420;
}

std::string helpText() {
    std::string text;
    for (const auto& profile : kProfiles) {
        if (!text.empty()) text += ", ";
        text += profile.id;
    }
    return text;
}

} // namespace mmx::autotest_profiles
