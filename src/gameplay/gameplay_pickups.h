// gameplay_pickups.h - handles pickup spawning, collection, and reward flow.
// Boundary: placement truth stays source-backed; persistence stays in data/save.

#pragma once

#include "data/game_ids.h"
#include "entities/pickup.h"
#include "entities/player.h"
#include "systems/audio.h"
#include "systems/tilemap.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace mmx::gameplay_pickups {

constexpr int kHeartTankMaxHealth = 32; // 16 base HP + 8 heart tanks at +2.

struct CollectionResult {
    bool playSfx = false;
    SFX sfx = SFX::Pickup;
};

inline std::string persistentId(StageId stageId, size_t ordinal, const SpawnPoint& spawn) {
    return stageId.str() + ":pickup:" +
           std::to_string(ordinal) + ":" +
           spawn.id + ":" +
           std::to_string(static_cast<int>(std::lround(spawn.x))) + ":" +
           std::to_string(static_cast<int>(std::lround(spawn.y)));
}

inline std::string legacyPersistentId(std::string_view stagePath, const SpawnPoint& spawn) {
    return std::string(stagePath) + ":" +
           std::to_string(static_cast<int>(std::lround(spawn.x))) + ":" +
           std::to_string(static_cast<int>(std::lround(spawn.y)));
}

inline void spawnDrop(std::vector<Pickup>& pickups, float x, float y, int dropType) {
    if (dropType <= 0) return;

    Pickup pickup;
    pickup.init(x, y, static_cast<PickupType>(dropType));
    pickups.push_back(pickup);
}

inline bool spawnStagePickup(std::vector<Pickup>& pickups,
                             const SpawnPoint& spawn,
                             const std::string& persistentId,
                             bool alreadyCollected) {
    if (alreadyCollected) return true;

    Pickup pickup;
    if (spawn.id == "heart-tank") {
        pickup.init(spawn.x, spawn.y, PickupType::HeartTank, persistentId);
    } else if (spawn.id == "sub-tank") {
        pickup.init(spawn.x, spawn.y, PickupType::SubTank, persistentId);
    } else if (spawn.id == "capsule-boots" || spawn.id == "capsule-helmet" ||
               spawn.id == "capsule-body" || spawn.id == "capsule-buster") {
        pickup.init(spawn.x, spawn.y, PickupType::ArmorCapsule, persistentId);
        pickup.armorPart = spawn.id.substr(8);
    } else {
        return false;
    }

    pickups.push_back(pickup);
    return true;
}

inline void update(std::vector<Pickup>& pickups, const Tilemap& tilemap) {
    for (auto& pickup : pickups) {
        if (!pickup.active) continue;
        pickup.update(0, tilemap);
    }

    pickups.erase(
        std::remove_if(pickups.begin(), pickups.end(),
                       [](const Pickup& pickup) { return !pickup.active; }),
        pickups.end());
}

inline bool applyArmorCapsule(Player& player, const Pickup& pickup) {
    bool armorChanged = false;
    if (pickup.armorPart == "boots") {
        player.grantArmorBoots();
        armorChanged = true;
    } else if (pickup.armorPart == "helmet") {
        player.grantArmorHelmet();
        armorChanged = true;
    } else if (pickup.armorPart == "body") {
        player.grantArmorBody();
        armorChanged = true;
    } else if (pickup.armorPart == "buster") {
        player.grantArmorBuster();
        armorChanged = true;
    }
    return armorChanged;
}

inline CollectionResult collect(Player& player, const Pickup& pickup) {
    CollectionResult result;
    auto& progress = player.progressState();

    switch (pickup.type) {
        case PickupType::SmallHealth:
        case PickupType::LargeHealth:
            if (player.health < progress.maxHealth) {
                player.health = std::min(player.health + pickup.value, progress.maxHealth);
                result.playSfx = true;
                result.sfx = SFX::HealthRestore;
            } else {
                for (int i = 0; i < Player::MAX_SUB_TANKS; i++) {
                    if (progress.subTanks[i].collected &&
                        progress.subTanks[i].health < Player::SUB_TANK_CAPACITY) {
                        progress.subTanks[i].health = std::min(
                            progress.subTanks[i].health + pickup.value,
                            Player::SUB_TANK_CAPACITY);
                        break;
                    }
                }
            }
            break;

        case PickupType::SmallAmmo:
            break;

        case PickupType::ExtraLife:
            player.lives += pickup.value;
            result.playSfx = true;
            result.sfx = SFX::ExtraLife;
            break;

        case PickupType::HeartTank:
            if (progress.maxHealth < kHeartTankMaxHealth) {
                int gain = std::min(pickup.value, kHeartTankMaxHealth - progress.maxHealth);
                progress.maxHealth += gain;
                player.health += gain;
            }
            player.markPickupCollectedInProgress(pickup.persistentId);
            result.playSfx = true;
            result.sfx = SFX::HeartTank;
            break;

        case PickupType::SubTank:
            for (int i = 0; i < Player::MAX_SUB_TANKS; i++) {
                if (!progress.subTanks[i].collected) {
                    progress.subTanks[i].collected = true;
                    progress.subTanks[i].health = 0;
                    player.markPickupCollectedInProgress(pickup.persistentId);
                    result.playSfx = true;
                    result.sfx = SFX::SubTank;
                    break;
                }
            }
            break;

        case PickupType::ArmorCapsule:
            applyArmorCapsule(player, pickup);
            player.markPickupCollectedInProgress(pickup.persistentId);
            result.playSfx = true;
            result.sfx = SFX::HeartTank;
            break;
    }

    return result;
}

template <typename ShouldStartCpCapsule, typename StartCpCapsule>
inline void handlePlayerCollision(Player& player,
                                  std::vector<Pickup>& pickups,
                                  ShouldStartCpCapsule&& shouldStartCpCapsule,
                                  StartCpCapsule&& startCpCapsule) {
    if (player.isDead()) return;

    const AABB playerBox = player.getHitbox();
    for (auto& pickup : pickups) {
        if (!pickup.active) continue;

        const AABB pickupBox = pickup.getHitbox();
        if (!playerBox.overlaps(pickupBox)) continue;

        if (pickup.type == PickupType::ArmorCapsule &&
            shouldStartCpCapsule(pickup)) {
            startCpCapsule(pickup);
            continue;
        }

        const auto collection = collect(player, pickup);
        if (collection.playSfx) {
            AudioManager::playSFX(collection.sfx);
        }

        pickup.active = false;
    }
}

inline bool shouldStartCpCapsuleCutscene(bool cutsceneActive,
                                         const Pickup& pickup,
                                         const StageId& activeStageId,
                                         std::string_view stagePath) {
    if (cutsceneActive) return false;
    if (pickup.type != PickupType::ArmorCapsule || pickup.armorPart != "boots") {
        return false;
    }

    return activeStageId.str() == "chill-penguin" ||
           stagePath.find("chill-penguin") != std::string_view::npos;
}

} // namespace mmx::gameplay_pickups
