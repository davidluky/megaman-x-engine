#include "systems/randomizer.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <set>
#include <string>
#include <vector>

namespace {

bool sameFloat(float a, float b) {
    return std::fabs(a - b) < 0.001f;
}

bool sameSpawn(const mmx::SpawnShuffleEntry& lhs, const mmx::SpawnShuffleEntry& rhs) {
    return lhs.type == rhs.type
        && lhs.id == rhs.id
        && sameFloat(lhs.x, rhs.x)
        && sameFloat(lhs.y, rhs.y);
}

bool sameSpawns(const std::vector<mmx::SpawnShuffleEntry>& lhs,
                const std::vector<mmx::SpawnShuffleEntry>& rhs) {
    if (lhs.size() != rhs.size()) return false;
    for (size_t i = 0; i < lhs.size(); ++i) {
        if (!sameSpawn(lhs[i], rhs[i])) return false;
    }
    return true;
}

bool containsSameIds(std::vector<std::string> values, std::vector<std::string> expected) {
    std::sort(values.begin(), values.end());
    std::sort(expected.begin(), expected.end());
    return values == expected;
}

} // namespace

int main() {
    assert(mmx::Randomizer(0x123456789abcdef0ull).seedString() == "123456789ABCDEF0");

    const std::vector<mmx::SpawnShuffleEntry> original = {
        {"player_spawn", "", 16.0f, 32.0f},
        {"enemy", "met", 64.0f, 80.0f},
        {"enemy", "patrol", 160.0f, 96.0f},
        {"enemy", "batton", 240.0f, 48.0f},
        {"pickup", "health", 96.0f, 40.0f},
        {"pickup", "heart_tank", 220.0f, 120.0f},
        {"checkpoint", "", 300.0f, 64.0f},
        {"boss", "chill-penguin", 480.0f, 128.0f},
    };

    auto shuffled = original;
    mmx::Randomizer rng(0xc0ffeeull);
    mmx::RandomizerConfig runtimeConfig;
    runtimeConfig.enemyTypePool = {"met", "patrol", "batton", "axemax", "snowball"};
    rng.setConfig(runtimeConfig);
    rng.shuffleSpawns(shuffled, "chill-penguin");

    auto shuffledAgain = original;
    mmx::Randomizer sameRng(0xc0ffeeull);
    sameRng.setConfig(runtimeConfig);
    sameRng.shuffleSpawns(shuffledAgain, "chill-penguin");
    assert(sameSpawns(shuffled, shuffledAgain));

    const std::set<std::string> allowedEnemyTypes(runtimeConfig.enemyTypePool.begin(),
                                                  runtimeConfig.enemyTypePool.end());
    for (const auto& spawn : shuffled) {
        if (spawn.type == "enemy") {
            assert(allowedEnemyTypes.count(spawn.id) == 1);
        }
    }

    assert(sameSpawn(shuffled[0], original[0]));
    assert(sameSpawn(shuffled[6], original[6]));
    assert(sameSpawn(shuffled[7], original[7]));

    std::vector<std::string> bossOrder = rng.getShuffledBossOrder();
    assert(bossOrder.size() == 8);
    assert(containsSameIds(bossOrder, {
        "chill-penguin",
        "storm-eagle",
        "flame-mammoth",
        "spark-mandrill",
        "armored-armadillo",
        "launch-octopus",
        "boomer-kuwanger",
        "sting-chameleon",
    }));

    const auto assignments = rng.getBossAssignments();
    assert(assignments.size() == 8);
    std::set<std::string> assignedStages;
    std::set<std::string> assignedBosses;
    for (const auto& assignment : assignments) {
        assert(assignedStages.insert(assignment.stage.str()).second);
        assert(assignedBosses.insert(assignment.boss.str()).second);

        auto bossForStage = rng.bossForStage(assignment.stage);
        assert(bossForStage.has_value());
        assert(*bossForStage == assignment.boss);

        auto stageForBoss = rng.stageForBoss(assignment.boss);
        assert(stageForBoss.has_value());
        assert(*stageForBoss == assignment.stage);
    }

    auto reassigned = original;
    const mmx::StageId stageSlot = mmx::StageId::fromString("chill-penguin");
    const auto placedBoss = rng.bossForStage(stageSlot);
    assert(placedBoss.has_value());
    assert(rng.applyBossAssignment(reassigned, stageSlot));
    assert(reassigned[7].type == "boss");
    assert(reassigned[7].id == placedBoss->str());
    for (size_t i = 0; i + 1 < reassigned.size(); ++i) {
        assert(sameSpawn(reassigned[i], original[i]));
    }
    assert(!rng.applyBossAssignment(reassigned, mmx::StageId::fromString("intro-highway")));

    mmx::RandomizerConfig canonicalConfig;
    canonicalConfig.shuffleBossOrder = false;
    mmx::Randomizer canonical(0xc0ffeeull);
    canonical.setConfig(canonicalConfig);
    assert(canonical.getShuffledBossOrder() == std::vector<std::string>({
        "chill-penguin",
        "storm-eagle",
        "flame-mammoth",
        "spark-mandrill",
        "armored-armadillo",
        "launch-octopus",
        "boomer-kuwanger",
        "sting-chameleon",
    }));
    const auto canonicalAssignments = canonical.getBossAssignments();
    for (const auto& assignment : canonicalAssignments) {
        assert(assignment.stage.str() == assignment.boss.str());
    }

    return 0;
}
