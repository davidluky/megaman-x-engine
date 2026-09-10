// gameplay_projectiles.cpp - updates projectile spawning, collisions, and effects.
// Boundary: projectile entities own per-shot state; this lane coordinates scene use.

#include "gameplay/gameplay_projectiles.h"
#include "gameplay/gameplay_buster_impact.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace mmx::gameplay_projectiles {

bool targetOnCamera(float x, float y, const TargetViewport& viewport) {
    return x >= viewport.left && x <= viewport.left + viewport.width &&
           y >= viewport.top && y <= viewport.top + viewport.height;
}

void flushPendingSpawns(std::vector<Projectile>& projectiles,
                        std::vector<Projectile>& pending) {
    if (pending.empty()) return;

    for (auto& projectile : pending) {
        projectiles.push_back(std::move(projectile));
    }
    pending.clear();
}

void removeInactive(std::vector<Projectile>& projectiles) {
    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(),
                       [](const Projectile& projectile) { return !projectile.active; }),
        projectiles.end());
}

bool consumeAbsorbShield(std::vector<Projectile>& projectiles) {
    // Rolling Shield charged absorbs exactly one player hit, then the X-glued
    // shield object dies while the caller skips the player damage.
    for (auto& projectile : projectiles) {
        if (projectile.active && projectile.isPlayerShot && projectile.absorbShield) {
            projectile.active = false;
            return true;
        }
    }
    return false;
}

void anchorAbsorbShieldToHitbox(Projectile& projectile, const AABB& hitbox) {
    if (!projectile.absorbShield || !projectile.isPlayerShot) return;

    // Charged Rolling Shield is glued to X's anchor and re-fed every tick.
    projectile.prevPosition = projectile.position;
    projectile.position.x =
        (hitbox.x + hitbox.w * 0.5f) - projectile.hitboxSize.x * 0.5f;
    projectile.position.y =
        (hitbox.y + hitbox.h * 0.5f) - projectile.hitboxSize.y * 0.5f;
}

bool updateBoomerangLiveTargetAndCatch(Projectile& projectile,
                                       float targetCenterX,
                                       float targetCenterY,
                                       const AABB& playerHitbox,
                                       WeaponInventory& inventory) {
    if (!projectile.boomerang || !projectile.isPlayerShot) return false;

    projectile.returnX = targetCenterX;
    projectile.returnY = targetCenterY;
    if (!projectile.boomSteer ||
        projectile.boomerangTimer <= projectile.boomStraightFrames) {
        return false;
    }

    const AABB projectileHitbox = projectile.getHitbox();
    if (!playerHitbox.overlaps(projectileHitbox)) return false;

    projectile.active = false;
    if (projectile.boomCatchRefund) {
        for (size_t i = 0; i < inventory.weaponCount(); ++i) {
            if (inventory.weaponAt(i).id == projectile.weaponId) {
                inventory.refillAmmo(static_cast<int>(i), 1);
                break;
            }
        }
    }
    return true;
}

void spawnShatterFragments(std::vector<Projectile>& pending,
                           const Projectile& source) {
    // Shotgun Ice shatter: fragments spawn centered on the parent and fly
    // backward using the weapon-provided right-facing vector table.
    const float mirror = (source.vx >= 0) ? 1.0f : -1.0f;
    const float cx = source.position.x + source.hitboxSize.x / 2;
    const float cy = source.position.y + source.hitboxSize.y / 2;

    for (const auto& spec : source.shatterSpecs) {
        Projectile fragment;
        fragment.init(cx - 3.0f, cy - 3.0f,
                      mirror * spec.vx, spec.vy,
                      ProjectileType::Normal);
        fragment.isPlayerShot = source.isPlayerShot;
        fragment.damage = source.damage;
        fragment.projGravity = spec.gravity;
        fragment.isShatterFragment = true;
        fragment.lifetime = 120;
        fragment.hitboxSize = {6, 6};
        fragment.color = source.color;
        fragment.weaponId = source.weaponId;
        fragment.visualSpritePath =
            "content/x1/sprites/weapons/shotgun_ice_fragment.png";
        fragment.visualFrameWidth = 8;
        fragment.visualFrameHeight = 8;
        fragment.visualFrameCount = 1;
        fragment.shattersOnWallHit = false;
        fragment.shatterCount = 0;
        pending.push_back(fragment);
    }
}

void spawnWallSplitProjectiles(std::vector<Projectile>& pending,
                               const Projectile& source) {
    // Electric Spark terrain split: two children claim at the parent center,
    // move vertically, and may pierce terrain depending on the source shot.
    const float cx = source.position.x + source.hitboxSize.x / 2;
    const float cy = source.position.y + source.hitboxSize.y / 2;
    const float splitSpeed = source.splitSpeed > 0 ? source.splitSpeed : 6.0f;

    for (int i = 0; i < 2; i++) {
        Projectile split;
        const float svy = (i == 0) ? -splitSpeed : splitSpeed;
        split.init(0, 0, 0, svy, ProjectileType::Normal);
        split.isPlayerShot = source.isPlayerShot;
        split.damage = source.splitChildrenDamage >= 0
            ? source.splitChildrenDamage
            : source.damage;
        split.lifetime = 600;
        split.hitboxSize = source.hitboxSize;
        split.position.x = cx - split.hitboxSize.x * 0.5f;
        split.position.y = cy - split.hitboxSize.y * 0.5f;
        split.prevPosition = split.position;
        split.color = source.color;
        split.weaponId = source.weaponId;
        split.applyWeaponVisual(source.weaponId, false);
        split.splitsOnWallHit = false;
        split.ignoresTerrain = source.splitPierce;
        split.despawnMarginPx = source.despawnMarginPx;
        split.holdFirstTick = true;
        pending.push_back(split);
    }
}

void spawnStingFan(std::vector<Projectile>& pending,
                   const Projectile& muzzle) {
    // Chameleon Sting fan: three pre-spread darts claim around the muzzle and
    // keep fixed per-direction sprite cells.
    const float mx = muzzle.position.x + muzzle.hitboxSize.x / 2;
    const float my = muzzle.position.y + muzzle.hitboxSize.y / 2;

    for (const auto& dartSpec : muzzle.stingDartSpecs) {
        Projectile dart;
        dart.init(0, 0, dartSpec.vx, dartSpec.vy, ProjectileType::Normal);
        dart.isPlayerShot = muzzle.isPlayerShot;
        dart.facingRight = muzzle.facingRight;
        dart.damage = muzzle.stingDartDamage;
        dart.lifetime = 600;
        dart.hitboxSize = muzzle.hitboxSize;
        dart.position.x = mx + dartSpec.offX - dart.hitboxSize.x * 0.5f;
        dart.position.y = my + dartSpec.offY - dart.hitboxSize.y * 0.5f;
        dart.prevPosition = dart.position;
        dart.color = muzzle.color;
        dart.weaponId = muzzle.weaponId;
        dart.applyWeaponVisual(muzzle.weaponId, false);
        dart.visualFixedFrame = 12 + dartSpec.cell;
        dart.visualFrameCount = 15;
        dart.ignoresTerrain = true;
        dart.despawnMarginPx = muzzle.despawnMarginPx;
        dart.holdFirstTick = true;
        pending.push_back(dart);
    }
}

bool spawnWaveSegment(std::vector<Projectile>& pending,
                      const Projectile& head,
                      const Tilemap& tilemap) {
    // Fire Wave charged chain: wake flames drop from the head's previous
    // anchor and snap to the nearest terrain surface under that point.
    const float segX = head.prevPosition.x + head.hitboxSize.x * 0.5f;
    const float headY = head.position.y + head.hitboxSize.y * 0.5f;

    const int ts = tilemap.tileSize();
    if (ts <= 0) return false;

    float groundY = 0.0f;
    bool foundGround = false;
    for (float y = headY - 24.0f; y < headY + 96.0f; y += 4.0f) {
        const TileType tile = tilemap.getTileTypeAtPixel(segX, y);
        if (anchorsFireWaveSegment(tile)) {
            groundY = std::floor(y / static_cast<float>(ts)) * ts;
            foundGround = true;
            break;
        }
    }
    if (!foundGround) return false;

    Projectile segment;
    segment.init(0, 0, 0, 0, head.type);
    segment.isPlayerShot = head.isPlayerShot;
    segment.facingRight = head.waveFacingRight;
    segment.damage = head.waveSegDamage;
    segment.piercing = true;
    segment.ignoresTerrain = true;
    segment.waveSegment = true;
    segment.continuousDamage = true;
    segment.damageTickFrames = head.waveDamageTickFrames;
    segment.lifetime = head.waveSegLifetime;
    segment.despawnMarginPx = 0.0f;
    segment.hitboxSize = {16.0f, 24.0f};
    segment.position.x = segX - segment.hitboxSize.x * 0.5f;
    segment.position.y = groundY - segment.hitboxSize.y;
    segment.prevPosition = segment.position;
    segment.color = head.color;
    segment.weaponId = head.weaponId;
    segment.applyWeaponVisual(head.weaponId, true);
    pending.push_back(segment);
    return true;
}

enemy_damage::HitForm hitFormForEnemyDamage(const Projectile& projectile) {
    if (projectile.isShatterFragment) return enemy_damage::HitForm::Fragment;
    if (projectile.type != ProjectileType::Normal) return enemy_damage::HitForm::Charged;
    return enemy_damage::HitForm::Normal;
}

int damageToEnemy(const Projectile& projectile, const Enemy& enemy) {
    const auto defIt = Enemy::definitions.find(enemy.type);
    if (defIt == Enemy::definitions.end()) return projectile.damage;

    return enemy_damage::damageFor(
        defIt->second,
        projectile.weaponId,
        hitFormForEnemyDamage(projectile),
        projectile.damage);
}

EnemyHitResult resolvePlayerShotEnemyHit(
    Projectile& projectile,
    Enemy& enemy,
    std::vector<Projectile>& pending) {
    EnemyHitResult result;
    if (!projectile.active || !projectile.isPlayerShot ||
        projectile.dormantFrames > 0 ||
        !enemy.active || !enemy.cameraActivated ||
        enemy.enemyState == EnemyState::Dead) {
        return result;
    }

    const AABB projectileBox = projectile.getHitbox();
    const AABB enemyBox = enemy.getHitbox();
    if (!projectileBox.overlaps(enemyBox)) return result;
    if (projectile.usesOneHitPerTargetGate() &&
        projectile.hasHitEnemySerial(enemy.serial)) {
        return result;
    }

    const int damage = damageToEnemy(projectile, enemy);
    // Measured zero-damage pairings are full pass-throughs: no flash and no
    // projectile death.
    if (damage == 0) return result;

    if (projectile.continuousDamage &&
        enemy.hitFlash > 4 - projectile.damageTickFrames) {
        return result;
    }

    result.handled = true;
    if (!enemy.invulnerable) {
        if (projectile.usesOneHitPerTargetGate()) {
            projectile.markHitEnemySerial(enemy.serial);
        }

        enemy.health -= damage;
        enemy.hitFlash = 4;
        result.damagedEnemy = true;

        if (enemy.health <= 0) {
            enemy.enemyState = EnemyState::Dead;
            enemy.alive = false;
            enemy.deathTimer = 0;
            result.killedEnemy = true;
        } else {
            // FW7-REVIEW-C: one exact palette-0 flash frame on a surviving
            // hit (source f447 targetFlashProof).
            enemy.hitFlashVisual = 1;
        }
    }

    result.busterNonlethalImpact = gameplay_buster_impact::isEligible({
        projectile.type == ProjectileType::ChargeL1,
        projectile.weaponId,
        result.damagedEnemy,
        result.killedEnemy,
    });
    result.busterNormalContactPrelude = gameplay_buster_impact::isNormalBusterContactPreludeEligible({
        projectile.type == ProjectileType::Normal,
        projectile.weaponId,
        result.damagedEnemy,
        result.killedEnemy,
    });
    result.busterNormalLethalContactResidue = gameplay_buster_impact::isNormalBusterLethalContactResidueEligible({
        projectile.type == ProjectileType::Normal,
        projectile.weaponId,
        result.damagedEnemy,
        result.killedEnemy,
    });
    result.busterNormalSurvivorContactResidue = gameplay_buster_impact::isNormalBusterSurvivorContactResidueEligible({
        projectile.type == ProjectileType::Normal,
        projectile.weaponId,
        result.damagedEnemy,
        result.killedEnemy,
    });

    if (projectile.shouldConsumeAfterEnemyHit(result.killedEnemy)) {
        if (projectile.shattersOnWallHit && !projectile.shatterSpecs.empty()) {
            spawnShatterFragments(pending, projectile);
            result.shatterImpact = true;
            result.impactX = projectile.position.x + projectile.hitboxSize.x * 0.5f;
            result.impactY = projectile.position.y + projectile.hitboxSize.y * 0.5f;
            result.shatterSfx = projectile.sfxShatter;
        }
        projectile.active = false;
    }

    return result;
}

bool bossCanBeHitByPlayerShot(const Boss& boss) {
    return boss.active &&
           boss.bossState != BossState::Rescued &&
           boss.bossState != BossState::Dying &&
           boss.bossState != BossState::Dead &&
           boss.bossState != BossState::Dormant &&
           boss.bossState != BossState::Intro;
}

BossHitResult resolvePlayerShotBossHit(Projectile& projectile,
                                       Boss& boss) {
    BossHitResult result;
    if (!bossCanBeHitByPlayerShot(boss)) return result;
    if (!projectile.active || !projectile.isPlayerShot) return result;

    const AABB projectileBox = projectile.getHitbox();
    if (!boss.overlapsPlayerShot(projectileBox)) return result;
    if (projectile.usesOneHitPerTargetGate() && projectile.hitBossOnce) {
        return result;
    }

    result.handled = true;
    result.damagedBoss = boss.takeDamage(projectile.damageToBoss(), projectile.weaponId);
    if (result.damagedBoss && projectile.usesOneHitPerTargetGate()) {
        projectile.hitBossOnce = true;
    }

    if (!projectile.piercing) {
        projectile.active = false;
    }
    return result;
}

void despawnOffCamera(Projectile& projectile,
                      const TargetViewport& viewport) {
    // Steered boomerangs are allowed to leave camera and return; ordinary
    // player shots margin-die with weapon-specific oracle-measured bounds.
    if (!projectile.active || !projectile.isPlayerShot ||
        (projectile.boomerang && projectile.boomSteer)) {
        return;
    }

    if (projectile.type != ProjectileType::Normal &&
        projectile.type != ProjectileType::ChargeL1 &&
        projectile.type != ProjectileType::ChargeL2 &&
        projectile.type != ProjectileType::ChargeL3) {
        return;
    }

    const float margin = projectile.rideable ? 64.0f : projectile.despawnMarginPx;
    if (projectile.position.x < viewport.left - margin ||
        projectile.position.x > viewport.left + viewport.width + margin ||
        projectile.position.y < viewport.top - margin ||
        projectile.position.y > viewport.top + viewport.height + margin) {
        projectile.active = false;
    }
}

} // namespace mmx::gameplay_projectiles
