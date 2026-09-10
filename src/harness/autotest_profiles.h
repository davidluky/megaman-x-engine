// autotest_profiles.h - declares named autotest profiles and setup queries.
// Boundary: profile metadata only; frame-by-frame inputs live in scripts.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace mmx::autotest_profiles {

enum class Setup {
    Gameplay,
    DeathPitSpawn,
    BossRoomSpawn,
    BossIntroScene,
    // GC2.1b. Boots straight into the post-boss weapon-get sequence, the same
    // way BossIntroScene boots the intro cutscene: the measured sequence is
    // just over 1100 ticks with the recorded handoff tail, and no scripted
    // fight reliably kills a Maverick, so capturing it needs a direct entry
    // point rather than a lucky playthrough.
    WeaponGetSequence,
};

struct Profile {
    const char* id;
    int defaultTotalFrames;
    bool grantAllWeapons;
    Setup setup;
};

const Profile* all();
std::size_t count();
bool isKnown(std::string_view profile);
bool grantsAllWeapons(std::string_view profile);
Setup setup(std::string_view profile);
bool usesDeathPitSpawn(std::string_view profile);
bool usesBossRoomSpawn(std::string_view profile);
bool usesBossIntroScene(std::string_view profile);
bool usesWeaponGetSequence(std::string_view profile);
int totalFrames(std::string_view profile, int parityReplayFrames);
std::string helpText();

} // namespace mmx::autotest_profiles
