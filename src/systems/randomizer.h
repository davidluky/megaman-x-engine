// randomizer.h - declares randomizer configuration, assignments, and entries.
// Owns: seed state, boss-stage mapping, and spawn shuffle data contracts.

#pragma once

#include "data/game_ids.h"

#include <string>
#include <vector>
#include <random>
#include <cstdint>
#include <optional>

// ============================================================================
// randomizer.h — Seed-based shuffle system for randomizer mode
//
// Randomizes enemy placements, boss order, and item positions using a
// deterministic seed. The same seed always produces the same shuffle,
// enabling shareable runs and reproducible challenges.
//
// Usage flow:
//   1. Create Randomizer with a seed
//   2. Configure which aspects to shuffle via RandomizerConfig
//   3. After Tilemap::loadFromFile, convert SpawnPoints to SpawnShuffleEntries,
//      call shuffleSpawns(), then apply the results before spawning entities
//   4. Use bossForStage() to place shuffled bosses into fixed stage slots
//   5. Use getShuffledWeaknesses() to override boss damage tables
//
// Each stage gets a deterministic sub-RNG (seed XOR hash of stageId) so
// shuffling one stage never affects another — partial replays are stable.
// ============================================================================

namespace mmx {

struct RandomizerConfig {
    uint64_t seed = 0;
    bool shuffleEnemies = true;
    bool shuffleBossOrder = true;
    bool shufflePickups = true;
    bool shuffleWeaknesses = false;  // Advanced: randomize weakness chart
    std::vector<std::string> enemyTypePool;
};

struct BossAssignment {
    StageId stage;
    BossId boss;
};

class Randomizer {
public:
    explicit Randomizer(uint64_t seed);

    // Apply all enabled shuffles to stage data (modifies spawn list in-place)
    // Call after Tilemap::loadFromFile but before GameplayScene spawns entities
    void shuffleSpawns(std::vector<struct SpawnShuffleEntry>& spawns, const std::string& stageId);

    // Apply the boss-order assignment to a stage's boss spawn. Non-Maverick
    // stages return false and leave their boss spawns unchanged.
    bool applyBossAssignment(std::vector<struct SpawnShuffleEntry>& spawns, StageId stageId) const;

    // Get randomized boss assignments. Stage slots stay fixed; boss ids may move.
    std::vector<BossAssignment> getBossAssignments() const;

    // Lookup helpers for gameplay and UI. Return nullopt for non-Maverick stages.
    std::optional<BossId> bossForStage(StageId stageId) const;
    std::optional<StageId> stageForBoss(BossId bossId) const;

    // Legacy string view of the shuffled boss order, returned as boss ids.
    std::vector<std::string> getShuffledBossOrder() const;

    // Get randomized weakness chart (weaponId → targetBoss mapping)
    // Returns pairs of (weapon_id, effective_against_boss_id)
    std::vector<std::pair<std::string, std::string>> getShuffledWeaknesses() const;

    // Display seed as a readable string (hex format)
    std::string seedString() const;

    uint64_t seed() const { return seed_; }
    const RandomizerConfig& config() const { return config_; }
    void setConfig(const RandomizerConfig& cfg) { config_ = cfg; }

private:
    uint64_t seed_;
    RandomizerConfig config_;
    mutable std::mt19937_64 rng_;

    // Create a sub-RNG seeded with seed_ XOR hash(context) so each stage
    // or subsystem gets its own deterministic sequence
    std::mt19937_64 seededRng(const std::string& context) const;
};

// Lightweight struct for spawn shuffling (avoids pulling in tilemap.h)
struct SpawnShuffleEntry {
    std::string type;   // "enemy", "pickup", "boss", etc
    std::string id;     // Enemy type or pickup type
    float x, y;
};

} // namespace mmx
