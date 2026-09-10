// gameplay_player_damage.h - applies player damage, knockback, and death triggers.
// Boundary: damage values must remain evidence-backed and covered by tests.

#pragma once

#include "entities/boss.h"
#include "entities/enemy.h"
#include "entities/player.h"
#include "entities/projectile.h"
#include "entities/stage_object.h"
#include "systems/tilemap.h"

namespace mmx::gameplay_player_damage {

struct PlayerDamageResult {
    bool hit = false;
    int damage = 0;
    float direction = 0.0f;
    float shakeStrength = 0.0f;
    int shakeFrames = 0;
    bool consumeProjectile = false;
};

inline bool playerCanTakeDamage(const Player& player) {
    return !player.isInvulnerable() && !player.isDead();
}

template <typename OnDeath>
inline void applySpikeAndPitDeathTriggers(Player& player,
                                          const Tilemap& tilemap,
                                          OnDeath&& onDeath) {
    if (!player.isDead()) {
        const AABB hitbox = player.getHitbox();
        const int tileSize = tilemap.tileSize();
        const int startCol = static_cast<int>(hitbox.left()) / tileSize;
        const int startRow = static_cast<int>(hitbox.top()) / tileSize;
        const int endCol = static_cast<int>(hitbox.right() - 0.01f) / tileSize;
        const int endRow = static_cast<int>(hitbox.bottom() - 0.01f) / tileSize;
        for (int row = startRow; row <= endRow && !player.isDead(); ++row) {
            for (int col = startCol; col <= endCol && !player.isDead(); ++col) {
                if (isHazard(tilemap.getTileType(col, row))) {
                    player.forceDeath();
                    onDeath();
                }
            }
        }
    }

    if (player.position.y > tilemap.pixelHeight() + 32 && !player.isDead()) {
        player.forceDeath();
        onDeath();
    }
}

inline PlayerDamageResult resolveEnemyContact(const Player& player,
                                              const Enemy& enemy) {
    PlayerDamageResult result;
    if (!playerCanTakeDamage(player)) return result;
    if (!enemy.active || !enemy.cameraActivated ||
        enemy.enemyState == EnemyState::Dead) return result;

    const AABB playerBox = player.getHitbox();
    const AABB enemyBox = enemy.getHitbox();
    if (!playerBox.overlaps(enemyBox)) return result;

    result.hit = true;
    result.damage = enemy.contactDamage;
    result.direction = player.position.x < enemy.position.x ? -1.0f : 1.0f;
    result.shakeStrength = 3.0f;
    result.shakeFrames = 10;
    return result;
}

template <typename Enemies, typename ApplyDamage>
inline void resolveEnemyContacts(const Player& player,
                                 Enemies& enemies,
                                 ApplyDamage&& applyDamage) {
    for (auto& enemy : enemies) {
        const auto hit = resolveEnemyContact(player, enemy);
        if (!hit.hit) continue;

        if (applyDamage(hit)) {
            break;
        }
    }
}

inline PlayerDamageResult resolveStageObjectContact(const Player& player,
                                                    const StageObject& object) {
    PlayerDamageResult result;
    if (!playerCanTakeDamage(player)) return result;
    if (!object.active || object.contactDamage() <= 0) return result;

    const AABB playerBox = player.getHitbox();
    const AABB objectBox = object.getHitbox();
    if (!playerBox.overlaps(objectBox)) return result;

    const float playerCenter = playerBox.x + playerBox.w * 0.5f;
    const float objectCenter = objectBox.x + objectBox.w * 0.5f;
    result.hit = true;
    result.damage = object.contactDamage();
    result.direction = playerCenter < objectCenter ? -1.0f : 1.0f;
    result.shakeStrength = 3.0f;
    result.shakeFrames = 10;
    return result;
}

template <typename StageObjects, typename ApplyDamage>
inline void resolveStageObjectContacts(const Player& player,
                                       StageObjects& objects,
                                       ApplyDamage&& applyDamage) {
    for (auto& object : objects) {
        const auto hit = resolveStageObjectContact(player, object);
        if (!hit.hit) continue;

        if (applyDamage(hit)) {
            break;
        }
    }
}

inline PlayerDamageResult resolveEnemyShotContact(const Player& player,
                                                  const Projectile& projectile) {
    PlayerDamageResult result;
    if (!playerCanTakeDamage(player)) return result;
    if (!projectile.active || projectile.isPlayerShot) return result;

    const AABB playerBox = player.getHitbox();
    const AABB shotBox = projectile.getHitbox();
    if (!playerBox.overlaps(shotBox)) return result;

    result.hit = true;
    result.damage = projectile.damage;
    result.direction = player.position.x < projectile.position.x ? -1.0f : 1.0f;
    result.shakeStrength = 2.0f;
    result.shakeFrames = 8;
    result.consumeProjectile = true;
    return result;
}

template <typename Projectiles, typename ApplyProjectileDamage>
inline void resolveEnemyShotContacts(const Player& player,
                                     Projectiles& projectiles,
                                     ApplyProjectileDamage&& applyDamage) {
    for (auto& projectile : projectiles) {
        const auto hit = resolveEnemyShotContact(player, projectile);
        if (!hit.hit) continue;

        if (applyDamage(hit, projectile)) {
            break;
        }
    }
}

inline bool bossCanContactPlayer(const Boss& boss) {
    return boss.active &&
           boss.bossState != BossState::Rescued &&
           boss.bossState != BossState::Dying &&
           boss.bossState != BossState::Dead &&
           boss.bossState != BossState::Dormant &&
           boss.bossState != BossState::Intro;
}

inline PlayerDamageResult resolveBossContact(const Player& player,
                                             const Boss& boss) {
    PlayerDamageResult result;
    if (!playerCanTakeDamage(player)) return result;
    if (!bossCanContactPlayer(boss)) return result;

    const AABB playerBox = player.getHitbox();
    const AABB bossBox = boss.getHitbox();
    if (!playerBox.overlaps(bossBox)) return result;

    result.hit = true;
    result.damage = boss.contactDamage;
    result.direction = player.position.x < boss.position.x ? -1.0f : 1.0f;
    result.shakeStrength = 4.0f;
    result.shakeFrames = 12;
    return result;
}

template <typename ApplyDamage>
inline void resolveBossContactAndApply(const Player& player,
                                       const Boss& boss,
                                       ApplyDamage&& applyDamage) {
    const auto hit = resolveBossContact(player, boss);
    if (!hit.hit) return;

    applyDamage(hit);
}

} // namespace mmx::gameplay_player_damage
