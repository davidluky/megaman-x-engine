// gameplay_stage_progress.h - translates gameplay events into progress updates.
// Boundary: writes through runtime/save APIs; source progression rules stay data-backed.

#pragma once

#include "data/game_ids.h"
#include "data/save_system.h"
#include "entities/boss.h"
#include "entities/player.h"
#include "systems/tilemap.h"
#include "systems/weapon.h"

#include <cmath>
#include <optional>

namespace mmx::gameplay_stage_progress {

template <typename Checkpoints, typename RespawnPoint>
inline void updateCheckpointTriggers(Checkpoints& checkpoints,
                                     float playerX,
                                     RespawnPoint& currentCheckpoint) {
    for (auto& checkpoint : checkpoints) {
        if (!checkpoint.triggered && playerX >= checkpoint.triggerX) {
            checkpoint.triggered = true;
            currentCheckpoint = checkpoint.respawn;
        }
    }
}

inline bool isRespawnPointSafe(const Player& player,
                               const Tilemap& tilemap,
                               Vector2 pos) {
    const int tileSize = tilemap.tileSize();
    if (tileSize <= 0) return false;

    AABB hitbox = {
        pos.x + player.hitboxOffset.x,
        pos.y + player.hitboxOffset.y,
        player.hitboxSize.x,
        player.hitboxSize.y
    };
    if (hitbox.left() < 0.0f ||
        hitbox.right() > static_cast<float>(tilemap.pixelWidth()) ||
        hitbox.top() < 0.0f ||
        hitbox.bottom() > static_cast<float>(tilemap.pixelHeight())) {
        return false;
    }

    const int startCol = static_cast<int>(std::floor(hitbox.left() / tileSize));
    const int endCol =
        static_cast<int>(std::floor((hitbox.right() - 0.01f) / tileSize));
    const int startRow = static_cast<int>(std::floor(hitbox.top() / tileSize));
    const int endRow =
        static_cast<int>(std::floor((hitbox.bottom() - 0.01f) / tileSize));
    for (int row = startRow; row <= endRow; ++row) {
        for (int col = startCol; col <= endCol; ++col) {
            const TileType type = tilemap.getTileType(col, row);
            if (isFullTileBlock(type)) {
                return false;
            }
        }
    }

    const int feetRow =
        static_cast<int>(std::floor((hitbox.bottom() + 1.0f) / tileSize));
    const int feetStartCol =
        static_cast<int>(std::floor((hitbox.left() + 1.0f) / tileSize));
    const int feetEndCol =
        static_cast<int>(std::floor((hitbox.right() - 1.0f) / tileSize));
    bool hasFloor = false;
    for (int col = feetStartCol; col <= feetEndCol; ++col) {
        const TileType type = tilemap.getTileType(col, feetRow);
        if (isHazard(type)) {
            return false;
        }
        if (isStandable(type)) {
            hasFloor = true;
        }
    }
    return hasFloor;
}

inline std::optional<Vector2> snapRespawnToGround(const Player& player,
                                                  const Tilemap& tilemap,
                                                  Vector2 pos) {
    constexpr int kSearchPx = 160;
    constexpr int kStepPx = 2;
    for (int distance = 0; distance <= kSearchPx; distance += kStepPx) {
        const Vector2 down = {pos.x, pos.y + static_cast<float>(distance)};
        if (isRespawnPointSafe(player, tilemap, down)) return down;
        if (distance > 0) {
            const Vector2 up = {pos.x, pos.y - static_cast<float>(distance)};
            if (isRespawnPointSafe(player, tilemap, up)) return up;
        }
    }
    return std::nullopt;
}

inline Vector2 resolveRespawnPoint(const Player& player,
                                   const Tilemap& tilemap,
                                   Vector2 pos,
                                   Vector2 defaultSpawn) {
    if (auto snapped = snapRespawnToGround(player, tilemap, pos)) {
        return *snapped;
    }
    if (auto fallback = snapRespawnToGround(player, tilemap, defaultSpawn)) {
        return *fallback;
    }
    return pos;
}

inline void applyBossClearRewards(Player& player,
                                  const Boss& boss,
                                  const StageId& activeStageId,
                                  int stageTimer,
                                  bool bossRushMode,
                                  bool suppressProgressionRewards,
                                  std::optional<Weapon>& awardedWeapon) {
    if (suppressProgressionRewards) return;

    const BossId bossId = BossId::fromString(boss.type);
    awardedWeapon = weapons::awardForBoss(bossId);
    if (awardedWeapon.has_value()) {
        player.weaponInventory.addWeapon(*awardedWeapon);
    }

    SaveSystem::incrementBossesDefeated();
    if (boss.type == "sigma") {
        SaveSystem::setSigmaDefeated(true);
    }

    if (!bossRushMode) {
        SaveSystem::markBossDefeated(bossId);
        SaveSystem::markStageCompleted(activeStageId);
        SaveSystem::recordTime(activeStageId, stageTimer);
        SaveSystem::save();
    }
}

} // namespace mmx::gameplay_stage_progress
