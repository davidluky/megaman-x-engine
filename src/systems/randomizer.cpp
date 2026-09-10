// randomizer.cpp - applies deterministic spawn, boss, and weakness shuffles.
// Owns: seed hashing, shuffled boss assignments, and spawn replacement rules.

#include "systems/randomizer.h"
#include "data/x1_catalog.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace mmx {

static uint64_t stableStringHash(const std::string& text) {
    // FNV-1a keeps shared randomizer seeds reproducible across standard
    // library implementations; std::hash deliberately makes no such promise.
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char ch : text) {
        hash ^= static_cast<uint64_t>(ch);
        hash *= 1099511628211ull;
    }
    return hash;
}

// ============================================================================
// Construction
// ============================================================================

Randomizer::Randomizer(uint64_t seed)
    : seed_(seed)
    , rng_(seed)
{
    config_.seed = seed;
}

// ============================================================================
// seededRng — deterministic sub-RNG per context
//
// XORs the master seed with a stable hash of the context string so each stage
// (or other subsystem) gets its own independent, reproducible sequence.
// Two stages with the same seed will always produce the same layout, and
// shuffling stage A never changes stage B's results.
// ============================================================================

std::mt19937_64 Randomizer::seededRng(const std::string& context) const {
    uint64_t h = stableStringHash(context);
    return std::mt19937_64(seed_ ^ h);
}

static std::vector<std::string> enemyTypePoolFromSpawns(const std::vector<SpawnShuffleEntry>& spawns) {
    std::vector<std::string> pool;
    for (const auto& spawn : spawns) {
        if (spawn.type != "enemy" || spawn.id.empty()) continue;
        if (std::find(pool.begin(), pool.end(), spawn.id) == pool.end()) {
            pool.push_back(spawn.id);
        }
    }
    return pool;
}

// ============================================================================
// shuffleSpawns — randomize enemy and pickup positions within a stage
//
// Enemies: positions are shuffled among all enemy spawns, and each enemy
// has a 20% chance to swap to a different valid type (keeps the same total
// count, just repositioned and occasionally retyped).
//
// Pickups: positions are shuffled among pickup spawns (heart tank stays a
// heart tank, sub tank stays a sub tank — only locations change).
//
// Untouched spawn types: "player_spawn", "boss", "checkpoint" — these are
// structural and randomizing them would break level flow.
// ============================================================================

void Randomizer::shuffleSpawns(std::vector<SpawnShuffleEntry>& spawns, const std::string& stageId) {
    auto rng = seededRng(stageId);

    // --- Enemy shuffle ---
    if (config_.shuffleEnemies) {
        // Collect indices of all enemy spawns
        std::vector<size_t> enemyIndices;
        for (size_t i = 0; i < spawns.size(); ++i) {
            if (spawns[i].type == "enemy") {
                enemyIndices.push_back(i);
            }
        }

        if (enemyIndices.size() > 1) {
            const std::vector<std::string> enemyTypePool = config_.enemyTypePool.empty()
                ? enemyTypePoolFromSpawns(spawns)
                : config_.enemyTypePool;

            // Collect enemy positions, shuffle them, reassign
            std::vector<std::pair<float, float>> positions;
            positions.reserve(enemyIndices.size());
            for (size_t idx : enemyIndices) {
                positions.push_back({spawns[idx].x, spawns[idx].y});
            }
            std::shuffle(positions.begin(), positions.end(), rng);

            for (size_t i = 0; i < enemyIndices.size(); ++i) {
                size_t idx = enemyIndices[i];
                spawns[idx].x = positions[i].first;
                spawns[idx].y = positions[i].second;

                // 20% chance to swap enemy type with a random valid type
                std::uniform_int_distribution<int> chanceDist(0, 4);
                if (!enemyTypePool.empty() && chanceDist(rng) == 0) {
                    std::uniform_int_distribution<size_t> typeDist(0, enemyTypePool.size() - 1);
                    spawns[idx].id = enemyTypePool[typeDist(rng)];
                }
            }
        }
    }

    // --- Pickup shuffle ---
    if (config_.shufflePickups) {
        // Collect indices of all pickup spawns
        std::vector<size_t> pickupIndices;
        for (size_t i = 0; i < spawns.size(); ++i) {
            if (spawns[i].type == "pickup") {
                pickupIndices.push_back(i);
            }
        }

        if (pickupIndices.size() > 1) {
            // Shuffle positions only — types stay the same (heart tank stays
            // heart tank, sub tank stays sub tank, just at different locations)
            std::vector<std::pair<float, float>> positions;
            positions.reserve(pickupIndices.size());
            for (size_t idx : pickupIndices) {
                positions.push_back({spawns[idx].x, spawns[idx].y});
            }
            std::shuffle(positions.begin(), positions.end(), rng);

            for (size_t i = 0; i < pickupIndices.size(); ++i) {
                size_t idx = pickupIndices[i];
                spawns[idx].x = positions[i].first;
                spawns[idx].y = positions[i].second;
            }
        }
    }
}

// ============================================================================
// Boss assignment - deterministic boss placement shuffle
//
// Stage slots stay fixed so completion, BGM, variants, and best times keep
// using StageId. Boss ids move among those slots when boss-order shuffle is
// enabled, which gives gameplay and stage select one shared placement contract.
// ============================================================================

bool Randomizer::applyBossAssignment(std::vector<SpawnShuffleEntry>& spawns, StageId stageId) const {
    const auto assignedBoss = bossForStage(stageId);
    if (!assignedBoss.has_value()) return false;

    bool applied = false;
    for (auto& spawn : spawns) {
        if (spawn.type != "boss") continue;
        spawn.id = assignedBoss->str();
        applied = true;
    }
    return applied;
}

std::vector<BossAssignment> Randomizer::getBossAssignments() const {
    std::vector<StageId> stages;
    std::vector<BossId> bosses;
    for (const auto& boss : x1_catalog::maverickBosses()) {
        stages.push_back(boss.stage);
        bosses.push_back(boss.boss);
    }

    if (config_.shuffleBossOrder) {
        auto rng = seededRng("boss-order");
        std::shuffle(bosses.begin(), bosses.end(), rng);
    }

    std::vector<BossAssignment> assignments;
    assignments.reserve(stages.size());
    for (size_t i = 0; i < stages.size(); ++i) {
        assignments.push_back({stages[i], bosses[i]});
    }
    return assignments;
}

std::optional<BossId> Randomizer::bossForStage(StageId stageId) const {
    for (const auto& assignment : getBossAssignments()) {
        if (assignment.stage == stageId) return assignment.boss;
    }
    return std::nullopt;
}

std::optional<StageId> Randomizer::stageForBoss(BossId bossId) const {
    for (const auto& assignment : getBossAssignments()) {
        if (assignment.boss == bossId) return assignment.stage;
    }
    return std::nullopt;
}

std::vector<std::string> Randomizer::getShuffledBossOrder() const {
    std::vector<std::string> order;
    for (const auto& assignment : getBossAssignments()) {
        order.push_back(assignment.boss.str());
    }
    return order;
}

// ============================================================================
// getShuffledWeaknesses — randomize the weakness chart
//
// When enabled, shuffles which weapon is super-effective against which boss.
// The weapon list and boss list are each independently shuffled, then
// zipped back together. This guarantees every boss still has exactly one
// weakness and every weapon is still effective against exactly one boss —
// just not the canonical pairings.
//
// When disabled (default), returns the canonical MMX1 weakness chart.
// ============================================================================

std::vector<std::pair<std::string, std::string>> Randomizer::getShuffledWeaknesses() const {
    std::vector<std::pair<std::string, std::string>> canonicalWeaknesses;
    for (const auto& boss : x1_catalog::maverickBosses()) {
        canonicalWeaknesses.push_back({boss.weaknessWeapon.str(), boss.boss.str()});
    }

    if (!config_.shuffleWeaknesses) {
        return canonicalWeaknesses;
    }

    // Extract weapons and bosses separately
    std::vector<std::string> weapons;
    std::vector<std::string> bosses;
    weapons.reserve(canonicalWeaknesses.size());
    bosses.reserve(canonicalWeaknesses.size());
    for (const auto& pair : canonicalWeaknesses) {
        weapons.push_back(pair.first);
        bosses.push_back(pair.second);
    }

    // Shuffle the boss targets — each weapon now hits a random boss
    auto rng = seededRng("weaknesses");
    std::shuffle(bosses.begin(), bosses.end(), rng);

    // Zip back into pairs
    std::vector<std::pair<std::string, std::string>> result;
    result.reserve(weapons.size());
    for (size_t i = 0; i < weapons.size(); ++i) {
        result.push_back({weapons[i], bosses[i]});
    }
    return result;
}

// ============================================================================
// seedString — human-readable seed display
//
// Formats the 64-bit seed as uppercase hex for display in menus and
// seed-sharing (e.g. "A1B2C3D4").
// ============================================================================

std::string Randomizer::seedString() const {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << seed_;
    return oss.str();
}

} // namespace mmx
