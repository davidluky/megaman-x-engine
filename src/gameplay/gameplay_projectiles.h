// gameplay_projectiles.h - declares GameplayScene projectile lane helpers.
// Boundary: scene orchestration only; projectile data stays in entities/projectile.

#pragma once

#include "entities/boss.h"
#include "entities/enemy.h"
#include "entities/enemy_damage.h"
#include "entities/player.h"
#include "entities/projectile.h"
#include "physics/projectile_collision.h"
#include "systems/tilemap.h"
#include "systems/weapon.h"

#include <cmath>
#include <vector>

namespace mmx::gameplay_projectiles {

struct TargetViewport {
    float left = 0.0f;
    float top = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct PlayerProjectileTarget {
    AABB hitbox{0.0f, 0.0f, 0.0f, 0.0f};
    float centerX = 0.0f;
    float centerY = 0.0f;
};

bool targetOnCamera(float x, float y, const TargetViewport& viewport);

void flushPendingSpawns(std::vector<Projectile>& projectiles,
                        std::vector<Projectile>& pending);

void removeInactive(std::vector<Projectile>& projectiles);

bool consumeAbsorbShield(std::vector<Projectile>& projectiles);

void anchorAbsorbShieldToHitbox(Projectile& projectile, const AABB& hitbox);

template <typename PlayApu>
inline void updateAbsorbShieldHum(const Projectile& projectile, PlayApu&& playApu) {
    // Rolling Shield charged hum: 0x64 first at release+47, then every 16f
    // while the shield lives. The CPU re-sends this one instead of relying on
    // SPC sustain.
    if (!projectile.absorbShield || !projectile.isPlayerShot ||
        projectile.sfxShieldHum < 0 || projectile.sfxShieldHumEvery <= 0 ||
        projectile.ageFrames < projectile.sfxShieldHumFirst) {
        return;
    }

    if ((projectile.ageFrames - projectile.sfxShieldHumFirst) %
            projectile.sfxShieldHumEvery == 0) {
        playApu(projectile.sfxShieldHum);
    }
}

bool updateBoomerangLiveTargetAndCatch(Projectile& projectile,
                                       float targetCenterX,
                                       float targetCenterY,
                                       const AABB& playerHitbox,
                                       WeaponInventory& inventory);

template <typename Projectiles>
inline void applyRideableCarry(Player& player,
                               const Projectiles& projectiles,
                               const Tilemap& tilemap) {
    // Charged Shotgun Ice ride: a moving sled scoops grounded X, then carries
    // him by the sled delta while clamping against solid wall tiles.
    if (player.isDead()) return;

    for (const auto& projectile : projectiles) {
        if (!projectile.active || !projectile.rideable || !projectile.isPlayerShot) {
            continue;
        }
        if (projectile.sledLaunchFrame > 0 &&
            projectile.ageFrames < projectile.sledLaunchFrame) {
            continue;
        }

        const AABB sledBox = projectile.getHitbox();
        const AABB feet = player.getHitbox();
        const bool overX = feet.right() > sledBox.left() &&
                           feet.left() < sledBox.right();
        const float feetY = feet.bottom();
        const bool onTopBand = feetY >= sledBox.top() - 3.0f &&
                               feetY <= sledBox.top() + 6.0f;
        const bool scoopBand = std::fabs(projectile.vx) > 0.01f &&
                               player.onGround &&
                               feetY > sledBox.top() + 6.0f &&
                               feetY <= sledBox.bottom() + 2.0f;
        if (!overX || player.velocity.y < 0.0f || (!onTopBand && !scoopBand)) {
            continue;
        }

        player.position.y =
            sledBox.top() - player.hitboxOffset.y - player.hitboxSize.y;
        player.position.x += projectile.vx;

        const int tileSize = tilemap.tileSize();
        if (tileSize > 0) {
            const AABB box = player.getHitbox();
            const int rowTop = static_cast<int>(box.top()) / tileSize;
            const int rowBottom =
                static_cast<int>(box.bottom() - 1.0f) / tileSize;
            if (projectile.vx > 0.0f) {
                const int col = static_cast<int>(box.right()) / tileSize;
                for (int row = rowTop; row <= rowBottom; ++row) {
                    if (tilemap.isSolid(col, row)) {
                        player.position.x -=
                            box.right() - static_cast<float>(col) * tileSize;
                        break;
                    }
                }
            } else if (projectile.vx < 0.0f) {
                const int col = static_cast<int>(box.left()) / tileSize;
                for (int row = rowTop; row <= rowBottom; ++row) {
                    if (tilemap.isSolid(col, row)) {
                        player.position.x +=
                            static_cast<float>(col + 1) * tileSize - box.left();
                        break;
                    }
                }
            }
        }

        player.velocity.y = 0.0f;
        player.onGround = true;
    }
}

void spawnShatterFragments(std::vector<Projectile>& pending,
                           const Projectile& source);

void spawnWallSplitProjectiles(std::vector<Projectile>& pending,
                               const Projectile& source);

void spawnStingFan(std::vector<Projectile>& pending,
                   const Projectile& muzzle);

template <typename SpawnStingFan, typename PlayApu>
inline void updateStingMuzzleFanSpawn(Projectile& projectile,
                                      SpawnStingFan&& spawnStingFan,
                                      PlayApu&& playApu) {
    // Chameleon Sting muzzle bolt: claim the three-dart fan at the measured
    // muzzle age, then let callers route pending-spawn and APU ownership.
    if (!projectile.stingMuzzle || projectile.stingFanSpawned ||
        projectile.ageFrames < projectile.stingFanTick) {
        return;
    }

    projectile.stingFanSpawned = true;
    playApu(projectile.sfxFanRelease);
    spawnStingFan(projectile);
}

bool spawnWaveSegment(std::vector<Projectile>& pending,
                      const Projectile& head,
                      const Tilemap& tilemap);

template <typename SpawnWaveSegment, typename PlayApu>
inline void updateFireWaveSegmentSpawn(Projectile& head,
                                       SpawnWaveSegment&& spawnWaveSegment,
                                       PlayApu&& playApu) {
    // Charged Fire Wave head drops terrain-snapped flames from its moving
    // previous-frame anchor. The head's margin/terrain death ends the chain;
    // waveMaxSegments is only the belt cap.
    if (!head.waveHead || head.waveSegmentsSpawned >= head.waveMaxSegments ||
        head.waveEveryFrames <= 0) {
        return;
    }

    // ageFrames hits 1 on the traced first tick, so +1 keeps the measured
    // head-to-first-segment gap.
    const int nextSegmentFrame =
        (head.waveSegmentsSpawned + 1) * head.waveEveryFrames + 1;
    if (head.ageFrames < nextSegmentFrame) {
        return;
    }

    if (spawnWaveSegment(head)) {
        // 0x61 is re-sent only when a flame is planted on real terrain.
        playApu(head.sfxSegmentPlant);
        head.waveSegmentsSpawned++;
    }
}

struct EnemyHitResult {
    bool handled = false;
    bool damagedEnemy = false;
    bool killedEnemy = false;
    bool busterNonlethalImpact = false;
    bool busterNormalContactPrelude = false;
    bool busterNormalLethalContactResidue = false;
    bool busterNormalSurvivorContactResidue = false;
    bool shatterImpact = false;
    float impactX = 0.0f;
    float impactY = 0.0f;
    int shatterSfx = -1;
};

struct BossHitResult {
    bool handled = false;
    bool damagedBoss = false;
};

enemy_damage::HitForm hitFormForEnemyDamage(const Projectile& projectile);

int damageToEnemy(const Projectile& projectile, const Enemy& enemy);

EnemyHitResult resolvePlayerShotEnemyHit(
    Projectile& projectile,
    Enemy& enemy,
    std::vector<Projectile>& pending);

template <typename Enemies, typename ApplyEnemyHit>
inline void resolvePlayerShotEnemyCollisions(std::vector<Projectile>& projectiles,
                                             Enemies& enemies,
                                             std::vector<Projectile>& pending,
                                             ApplyEnemyHit&& applyEnemyHit) {
    for (auto& projectile : projectiles) {
        if (!projectile.active || !projectile.isPlayerShot) continue;
        if (projectile.dormantFrames > 0) continue;

        for (auto& enemy : enemies) {
            if (!enemy.active || !enemy.cameraActivated ||
                enemy.enemyState == EnemyState::Dead) continue;

            const auto hit = resolvePlayerShotEnemyHit(projectile, enemy, pending);
            if (!hit.handled) continue;

            applyEnemyHit(enemy, hit);
            break;
        }
    }
}

bool bossCanBeHitByPlayerShot(const Boss& boss);

BossHitResult resolvePlayerShotBossHit(Projectile& projectile,
                                       Boss& boss);

template <typename ApplyBossHit>
inline void resolvePlayerShotBossCollisions(std::vector<Projectile>& projectiles,
                                            Boss& boss,
                                            ApplyBossHit&& applyBossHit) {
    for (auto& projectile : projectiles) {
        const auto hit = resolvePlayerShotBossHit(projectile, boss);
        if (!hit.handled) continue;

        applyBossHit(hit);
        break;
    }
}

void despawnOffCamera(Projectile& projectile,
                      const TargetViewport& viewport);

template <typename SpawnSledDebris, typename PlaySledBreakSfx>
inline void resolveTerrainCollision(Projectile& projectile,
                                    const Tilemap& tilemap,
                                    std::vector<Projectile>& pending,
                                    SpawnSledDebris&& spawnSledDebris,
                                    PlaySledBreakSfx&& playSledBreakSfx) {
    if (!projectile.active || projectile.ignoresTerrain) return;

    AABB box = projectile.getHitbox();
    const int tileSize = tilemap.tileSize();

    // Charged Fire Wave heads are floor-bound: near-foot support keeps the
    // head alive, while deeper floors past a gap must not rescue it.
    if (projectile.waveHead && projectile.groundFollow &&
        projectile.ageFrames > projectile.launchDelayFrames) {
        const auto supportRange = physics::projectileGroundSupportTileRange(
            box, tileSize, 8.0f);
        bool hasSupport = false;
        for (int row = supportRange.startRow;
             row <= supportRange.endRow && !hasSupport; ++row) {
            for (int col = supportRange.startCol; col <= supportRange.endCol; ++col) {
                if (supportsProjectile(tilemap.getTileType(col, row))) {
                    hasSupport = true;
                    break;
                }
            }
        }
        if (!hasSupport) {
            projectile.active = false;
            return;
        }
    }

    // Rolling Shield side bounces are detected at the rendered leading edge
    // and resolve as one opposite step from the previous frame.
    if (projectile.rolling && projectile.bouncesOffWalls &&
        projectile.wallBouncesLeft > 0 && projectile.vx != 0.0f &&
        projectile.visualFrameWidth > 0 &&
        projectile.ageFrames > projectile.launchDelayFrames) {
        const auto wallRange = physics::rollingShieldVisualWallTileRange(
            box, tileSize, projectile.vx, projectile.visualOffsetX,
            static_cast<float>(projectile.visualFrameWidth),
            projectile.visualScale);
        bool visualWallBounced = false;
        for (int row = wallRange.startRow; row <= wallRange.endRow; ++row) {
            for (int col = wallRange.startCol; col <= wallRange.endCol; ++col) {
                if (!tilemap.isSolid(col, row)) continue;
                const float tileTop = static_cast<float>(row * tileSize);
                const float tileBottom = static_cast<float>((row + 1) * tileSize);
                const float projBottom = box.bottom();
                if (projectile.vy > 0.0f && projBottom > tileTop &&
                    projBottom < tileBottom && !tilemap.isSolid(col, row - 1)) {
                    continue;
                }
                projectile.wallBouncesLeft--;
                projectile.position.x = physics::rollingShieldWallSnapX(
                    projectile.prevPosition.x, projectile.vx,
                    projectile.hitboxOffset.x, projectile.hitboxSize.x,
                    projectile.visualOffsetX,
                    static_cast<float>(projectile.visualFrameWidth),
                    projectile.visualScale);
                projectile.position.y = projectile.prevPosition.y;
                projectile.vx = -projectile.vx;
                projectile.facingRight = projectile.vx > 0.0f;
                visualWallBounced = true;
                break;
            }
            if (visualWallBounced) break;
        }
        if (visualWallBounced) return;
    }

    const bool fullBodyProbe =
        projectile.mineHazard || projectile.rolling || projectile.groundFollow;
    const auto tileRange = physics::projectileTerrainTileRange(
        box, tileSize, projectile.vx, projectile.vy, fullBodyProbe);

    bool rollingLanded = false;
    for (int row = tileRange.startRow;
         row <= tileRange.endRow && projectile.active && !rollingLanded; row++) {
        for (int col = tileRange.startCol; col <= tileRange.endCol && projectile.active; col++) {
            if (!tilemap.isSolid(col, row)) continue;

            // Bat mines stop on terrain instead of being destroyed.
            if (projectile.mineHazard) {
                const float tileTop = static_cast<float>(row * tileSize);
                if (projectile.vy > 0 && box.bottom() > tileTop) {
                    projectile.position.y = tileTop - projectile.hitboxSize.y;
                }
                projectile.vx = 0;
                projectile.vy = 0;
                rollingLanded = true;
                break;
            }

            // Rolling Shield settles on floors and spends its one wall bounce
            // before later side contacts destroy it.
            if (projectile.rolling) {
                const float tileTop = static_cast<float>(row * tileSize);
                const float tileBottom = static_cast<float>((row + 1) * tileSize);
                const float projBottom = box.bottom();

                if (projectile.vy > 0 && projBottom > tileTop &&
                    projBottom < tileBottom) {
                    projectile.position.y = tileTop - projectile.hitboxSize.y;
                    projectile.vy = 0;
                    rollingLanded = true;
                    break;
                }

                if (projectile.bouncesOffWalls && projectile.wallBouncesLeft > 0) {
                    projectile.wallBouncesLeft--;
                    projectile.position.x = physics::rollingShieldWallSnapX(
                        projectile.prevPosition.x, projectile.vx,
                        projectile.hitboxOffset.x, projectile.hitboxSize.x,
                        projectile.visualOffsetX,
                        static_cast<float>(projectile.visualFrameWidth),
                        projectile.visualScale);
                    projectile.position.y = projectile.prevPosition.y;
                    projectile.vx = -projectile.vx;
                    projectile.facingRight = projectile.vx > 0.0f;
                    rollingLanded = true;
                    break;
                }
            }

            // Ground-following sled/wave heads climb small steps; taller
            // walls destroy them without shatter children.
            if (projectile.groundFollow) {
                const float tileTop = static_cast<float>(row * tileSize);
                const float tileBottom = static_cast<float>((row + 1) * tileSize);
                const float projBottom = box.bottom();
                if (projectile.vy > 0 && projBottom > tileTop &&
                    projBottom < tileBottom) {
                    projectile.position.y = tileTop - projectile.hitboxSize.y;
                    projectile.vy = 0;
                    continue;
                }

                constexpr float kSledStepUpMaxPx = 6.0f;
                const float stepRise = projBottom - tileTop;
                if (stepRise <= kSledStepUpMaxPx && !tilemap.isSolid(col, row - 1)) {
                    projectile.position.y = tileTop - projectile.hitboxSize.y;
                    projectile.vy = 0;
                    continue;
                }

                if (projectile.sledDebrisOnWall) {
                    spawnSledDebris(projectile);
                    if (projectile.sfxSledBreak >= 0) {
                        playSledBreakSfx(projectile.sfxSledBreak);
                    }
                }
                projectile.active = false;
                continue;
            }

            // Plain terrain impacts can create Shotgun Ice fragments or
            // Electric Spark split children, but no Shotgun Ice burst effect.
            if (projectile.shattersOnWallHit && projectile.shatterCount > 0) {
                spawnShatterFragments(pending, projectile);
            }
            if (projectile.splitsOnWallHit && projectile.splitCount > 0) {
                spawnWallSplitProjectiles(pending, projectile);
            }
            projectile.active = false;
        }
    }
}

template <typename Enemies>
inline void updateTorpedoFanTarget(Projectile& projectile,
                                   const Enemies& enemies,
                                   bool bossActive,
                                   const Boss& boss,
                                   const TargetViewport& viewport) {
    if (!projectile.torpedoFanMember || !projectile.isPlayerShot) return;

    if (!projectile.fanAcquired) {
        const float px = projectile.position.x + projectile.hitboxSize.x * 0.5f;
        const float py = projectile.position.y + projectile.hitboxSize.y * 0.5f;
        float best = 1e9f;

        auto consider = [&](float cx, float cy, int serial, bool isBoss) {
            if (!targetOnCamera(cx, cy, viewport)) return;
            const float adx = std::fabs(cx - px);
            const float ady = std::fabs(cy - py);
            const float cheb = adx > ady ? adx : ady;
            if (cheb < best) {
                best = cheb;
                projectile.fanTargetSerial = serial;
                projectile.fanTargetIsBoss = isBoss;
            }
        };

        for (const auto& enemy : enemies) {
            if (!enemy.active || enemy.enemyState == EnemyState::Dead || enemy.health <= 0) {
                continue;
            }
            const AABB hitbox = enemy.getHitbox();
            consider(hitbox.x + hitbox.w * 0.5f,
                     hitbox.y + hitbox.h * 0.5f,
                     enemy.serial,
                     false);
        }

        if (bossActive && boss.active && boss.bossState != BossState::Dormant &&
            boss.bossState != BossState::Dead) {
            const AABB hitbox = boss.getHitbox();
            consider(hitbox.x + hitbox.w * 0.5f,
                     hitbox.y + hitbox.h * 0.5f,
                     -1,
                     true);
        }
        projectile.fanAcquired = true;
    }

    bool alive = false;
    if (projectile.fanTargetIsBoss) {
        if (bossActive && boss.active && boss.bossState != BossState::Dormant &&
            boss.bossState != BossState::Dead) {
            const AABB hitbox = boss.getHitbox();
            projectile.returnX = hitbox.x + hitbox.w * 0.5f;
            projectile.returnY = hitbox.y + hitbox.h * 0.5f;
            alive = true;
        }
    } else if (projectile.fanTargetSerial >= 0) {
        for (const auto& enemy : enemies) {
            if (enemy.serial != projectile.fanTargetSerial) continue;
            if (enemy.active && enemy.enemyState != EnemyState::Dead && enemy.health > 0) {
                const AABB hitbox = enemy.getHitbox();
                projectile.returnX = hitbox.x + hitbox.w * 0.5f;
                projectile.returnY = hitbox.y + hitbox.h * 0.5f;
                alive = true;
            }
            break;
        }
    }

    projectile.fanTargetAlive = alive;
    if (projectile.homing) {
        projectile.homingHasTarget = alive;
    }
}

template <typename Enemies>
inline void updateHomingTarget(Projectile& projectile,
                               const Enemies& enemies,
                               bool bossActive,
                               const Boss& boss,
                               const TargetViewport& viewport) {
    if (!projectile.homing || !projectile.isPlayerShot || projectile.torpedoFanMember) {
        return;
    }

    const float px = projectile.position.x + projectile.hitboxSize.x * 0.5f;
    const float py = projectile.position.y + projectile.hitboxSize.y * 0.5f;
    float best = 1e9f;
    float tx = 0.0f;
    float ty = 0.0f;
    bool found = false;

    auto consider = [&](float cx, float cy) {
        if (!targetOnCamera(cx, cy, viewport)) return;
        const float adx = std::fabs(cx - px);
        const float ady = std::fabs(cy - py);
        const float cheb = adx > ady ? adx : ady;
        if (cheb < best) {
            best = cheb;
            tx = cx;
            ty = cy;
            found = true;
        }
    };

    for (const auto& enemy : enemies) {
        if (!enemy.active || enemy.enemyState == EnemyState::Dead || enemy.health <= 0) {
            continue;
        }
        const AABB hitbox = enemy.getHitbox();
        consider(hitbox.x + hitbox.w * 0.5f, hitbox.y + hitbox.h * 0.5f);
    }

    if (bossActive && boss.active && boss.bossState != BossState::Dormant &&
        boss.bossState != BossState::Dead) {
        const AABB hitbox = boss.getHitbox();
        consider(hitbox.x + hitbox.w * 0.5f, hitbox.y + hitbox.h * 0.5f);
    }

    projectile.homingHasTarget = found;
    if (found) {
        projectile.returnX = tx;
        projectile.returnY = ty;
    }
}

template <typename Enemies,
          typename SpawnStingFan,
          typename SpawnWaveSegment,
          typename SpawnSledDebris,
          typename PlayApu>
inline void updateActiveProjectile(Projectile& projectile,
                                   const PlayerProjectileTarget& playerTarget,
                                   WeaponInventory& inventory,
                                   const Enemies& enemies,
                                   bool bossActive,
                                   const Boss& boss,
                                   const TargetViewport& targetViewport,
                                   const Tilemap& tilemap,
                                   std::vector<Projectile>& pending,
                                   SpawnStingFan&& spawnStingFan,
                                   SpawnWaveSegment&& spawnWaveSegment,
                                   SpawnSledDebris&& spawnSledDebris,
                                   PlayApu&& playApu) {
    if (!projectile.active) return;

    anchorAbsorbShieldToHitbox(projectile, playerTarget.hitbox);
    if (updateBoomerangLiveTargetAndCatch(
            projectile,
            playerTarget.centerX,
            playerTarget.centerY,
            playerTarget.hitbox,
            inventory)) {
        return;
    }

    updateTorpedoFanTarget(projectile, enemies, bossActive, boss, targetViewport);
    updateHomingTarget(projectile, enemies, bossActive, boss, targetViewport);

    projectile.update(0);

    updateStingMuzzleFanSpawn(projectile, spawnStingFan, playApu);
    updateAbsorbShieldHum(projectile, playApu);
    updateFireWaveSegmentSpawn(projectile, spawnWaveSegment, playApu);

    despawnOffCamera(projectile, targetViewport);
    if (!projectile.active) return;

    resolveTerrainCollision(projectile, tilemap, pending, spawnSledDebris, playApu);
}

} // namespace mmx::gameplay_projectiles
