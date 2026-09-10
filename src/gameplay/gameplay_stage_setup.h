// gameplay_stage_setup.h - prepares stage data, player spawn, and scene fixtures.
// Boundary: setup assembles existing data; it does not invent stage content.

#pragma once

#include "data/game_ids.h"
#include "entities/enemy.h"
#include "systems/randomizer.h"
#include "systems/tilemap.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mmx::gameplay_stage_setup {

inline RandomizerConfig completeRandomizerEnemyPool(RandomizerConfig config) {
    if (!config.enemyTypePool.empty()) return config;

    config.enemyTypePool.reserve(Enemy::definitions.size());
    for (const auto& [enemyType, _] : Enemy::definitions) {
        config.enemyTypePool.push_back(enemyType);
    }
    std::sort(config.enemyTypePool.begin(), config.enemyTypePool.end());
    return config;
}

inline std::vector<SpawnShuffleEntry> toShuffleEntries(
    const std::vector<SpawnPoint>& spawns) {
    std::vector<SpawnShuffleEntry> entries;
    entries.reserve(spawns.size());
    for (const auto& spawn : spawns) {
        entries.push_back({spawn.type, spawn.id, spawn.x, spawn.y});
    }
    return entries;
}

inline void applyShuffleEntries(std::vector<SpawnPoint>& spawns,
                                const std::vector<SpawnShuffleEntry>& entries) {
    for (std::size_t i = 0; i < entries.size() && i < spawns.size(); ++i) {
        spawns[i].type = entries[i].type;
        spawns[i].id = entries[i].id;
        spawns[i].x = entries[i].x;
        spawns[i].y = entries[i].y;
    }
}

inline void applyRandomizerToSpawns(std::vector<SpawnPoint>& spawns,
                                    uint64_t randomizerSeed,
                                    const RandomizerConfig& randomizerConfig,
                                    StageId activeStageId) {
    Randomizer randomizer(randomizerSeed);
    randomizer.setConfig(completeRandomizerEnemyPool(randomizerConfig));

    std::vector<SpawnShuffleEntry> entries = toShuffleEntries(spawns);
    randomizer.shuffleSpawns(entries, activeStageId.str());
    randomizer.applyBossAssignment(entries, activeStageId);
    applyShuffleEntries(spawns, entries);
}

} // namespace mmx::gameplay_stage_setup
