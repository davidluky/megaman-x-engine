// gameplay_collision_effects.h - emits collision-driven hit and death effects.
// Boundary: visual/audio reactions only; damage rules stay in combat lanes.

#pragma once

#include "gameplay/gameplay_pickups.h"
#include "gameplay/gameplay_player_damage.h"
#include "gameplay/gameplay_projectiles.h"
#include "data/save_system.h"
#include "entities/enemy.h"
#include "entities/pickup.h"
#include "entities/player.h"
#include "entities/projectile.h"
#include "systems/audio.h"
#include "systems/camera.h"
#include "systems/tilemap.h"

#include <cstdlib>
#include <vector>

namespace mmx::gameplay_collision_effects {

inline void applyPlayerDamage(Player& player,
                              Camera& camera,
                              const gameplay_player_damage::PlayerDamageResult& hit) {
    if (!hit.hit) return;

    player.takeDamage(hit.damage, hit.direction);
    camera.shake(hit.shakeStrength, hit.shakeFrames);
}

inline bool applyShieldedPlayerDamage(
    Player& player,
    Camera& camera,
    std::vector<Projectile>& projectiles,
    const gameplay_player_damage::PlayerDamageResult& hit,
    Projectile* consumedProjectile = nullptr) {
    if (!hit.hit) return false;

    if (gameplay_projectiles::consumeAbsorbShield(projectiles)) {
        if (consumedProjectile && hit.consumeProjectile) {
            consumedProjectile->active = false;
        }
        return true;
    }

    applyPlayerDamage(player, camera, hit);
    if (consumedProjectile && hit.consumeProjectile) {
        consumedProjectile->active = false;
    }
    return true;
}

inline void applyEnemyHit(Enemy& enemy,
                          std::vector<Pickup>& pickups,
                          const gameplay_projectiles::EnemyHitResult& hit) {
    if (!hit.handled) return;

    if (hit.killedEnemy) {
        AudioManager::playSFX(SFX::EnemyDeath);
        SaveSystem::incrementEnemiesDefeated();
        // RNG-gated like the real game (enemy_defs dropChancePct/dropTypes;
        // axemax E5 sample).
        const int dropType = enemy.rollDropType(std::rand() % 100, std::rand());
        if (dropType > 0) {
            gameplay_pickups::spawnDrop(
                pickups, enemy.position.x, enemy.position.y, dropType);
        }
        enemy.dropType = 0;
        enemy.dropTypes.clear();
    } else if (hit.damagedEnemy) {
        AudioManager::playApu(0x11);   // measured hit impact (R6)
    }

    // T1.2a.3 (2026-09-15): the shatter on an enemy hit is audible and spawns
    // fragments, but draws no burst — the source's only palette-2 effect at
    // both measured Shotgun Ice boss hits is the 16 x 16 contact square.
    if (hit.shatteredOnEnemyHit) {
        AudioManager::playApu(hit.shatterSfx);
    }
}

inline void applyBossHit(const gameplay_projectiles::BossHitResult& hit) {
    if (hit.damagedBoss) {
        AudioManager::playSFX(SFX::Hit);
    }
}

inline void applyHelmetHeadbutt(Player& player,
                                Tilemap& tilemap,
                                std::vector<Projectile>& projectiles,
                                Camera& camera) {
    if (!player.hasHelmet) return;
    if (!player.onCeiling) return;
    if (player.isDead()) return;

    const AABB hitbox = player.getHitbox();
    const int tileSize = tilemap.tileSize();
    const int startCol = static_cast<int>(hitbox.left()) / tileSize;
    const int endCol = static_cast<int>((hitbox.right() - 0.01f)) / tileSize;
    const int row = static_cast<int>((hitbox.top() - 1.0f)) / tileSize;

    bool brokeAny = false;
    for (int col = startCol; col <= endCol; col++) {
        if (tilemap.getTileType(col, row) != TileType::Breakable) continue;

        tilemap.breakTile(col, row);
        brokeAny = true;

        const float tx = static_cast<float>(col * tileSize + tileSize / 2);
        const float ty = static_cast<float>(row * tileSize + tileSize / 2);
        const float speeds[] = {-1.5f, -0.5f, 0.5f, 1.5f};
        for (int i = 0; i < 4; i++) {
            Projectile debris;
            debris.init(tx, ty, speeds[i], -2.0f + (i % 2) * 0.5f,
                        ProjectileType::Normal);
            debris.isPlayerShot = false;
            debris.damage = 0;
            debris.lifetime = 20;
            debris.hitboxSize = {4, 4};
            debris.color = {180, 140, 80, 255};
            debris.piercing = true;
            projectiles.push_back(debris);
        }
    }

    if (brokeAny) {
        AudioManager::playSFX(SFX::EnemyDeath);
        camera.shake(2.0f, 6);
    }
}

} // namespace mmx::gameplay_collision_effects
