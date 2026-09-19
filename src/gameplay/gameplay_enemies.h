// gameplay_enemies.h - coordinates enemy spawning, updates, and collisions.
// Boundary: enemy behavior definitions stay in entities and KB-backed data.

#pragma once

#include "entities/enemy.h"
#include "entities/projectile.h"
#include "systems/tilemap.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mmx::gameplay_enemies {

inline bool skipProvisionalForSetting(const char* setting) {
    // R282/R292 playable review: census markers are diagnostic actors.
    // MMX_SKIP_PROVISIONAL=0 explicitly enables that inspection view.
    return !setting || std::string(setting) != "0";
}

// Keep provisional census markers out of normal play and parity captures.
inline bool spawnAllowed(const SpawnPoint& sp, bool skipProvisional) {
    return !(skipProvisional && sp.provisional);
}

// Camera activation window (plan task R4.6, measured 2026-09-06).
//
// The source does not run a placed enemy until the camera brings it close to
// the screen. Taking the first frame each enemy instance occupies a slot in
// the two committed movie harvests, against the camera on that frame:
// Chill Penguin's 76 instances all appear between screen x -33 and +289, and
// so do the 82 Storm Eagle instances that are not OID 0x38 (that one type is
// emitted by something other than the camera window -- 21 of its instances
// appear from -1925 to +383). Nothing in either movie activates outside the
// 256 px screen widened by 33 px on each side.
inline constexpr float kEnemyActivationMarginPx = 33.0f;

inline bool withinCameraActivationWindow(float enemyX, float cameraX, float screenWidth) {
    const float relative = enemyX - cameraX;
    return relative >= -kEnemyActivationMarginPx &&
           relative <= screenWidth + kEnemyActivationMarginPx;
}

// oid_0x49/creation.json: ROM $80DC4B..8F scans one current X bucket when
// the camera changes buckets; $80DD13..1A uses an unsigned, half-open Y band.
// This opt-in covers the captured increasing-X path, without sweeping any
// skipped buckets or inferring reverse/initial-visible descriptor creation.
inline bool sourceRightwardCameraBucketReached(
    std::uint16_t previousCameraX, std::uint16_t cameraX,
    std::uint16_t cameraY, std::uint16_t sourceX, std::uint16_t sourceY) {
    const auto compared = static_cast<std::uint16_t>(previousCameraX - cameraX);
    if (previousCameraX >= cameraX || (compared & 0x8000u) == 0 ||
        (previousCameraX & 0xFFE0u) == (cameraX & 0xFFE0u)) {
        return false;
    }
    const auto scanX = static_cast<std::uint16_t>(cameraX + 0x0100u);
    const auto lowerY = static_cast<std::uint16_t>(cameraY - 0x0020u);
    const auto deltaY = static_cast<std::uint16_t>(sourceY - lowerY);
    return (scanX & 0xFFE0u) == (sourceX & 0xFFE0u) && deltaY < 0x0120u;
}

inline void drainPendingShots(Enemy& enemy, std::vector<Projectile>& projectiles) {
    for (const auto& shot : enemy.pendingShots) {
        Projectile projectile;
        projectile.init(shot.x, shot.y, shot.vx, shot.vy, ProjectileType::Normal);
        projectile.isPlayerShot = false;
        projectile.applyEnemyVisual();
        projectile.damage = shot.damage;
        projectile.sourceAxeMaxLog = shot.sourceAxeMaxLog;
        if (!shot.sprite.empty()) {
            // Measured enemy-shot art, such as Axe Max logs, is centered on
            // the authored shot anchor and mirrors by velocity.
            projectile.visualSpritePath = shot.sprite;
            projectile.visualFrameWidth = shot.visualWidth > 0
                ? shot.visualWidth : static_cast<int>(shot.w);
            projectile.visualFrameHeight = shot.visualHeight > 0
                ? shot.visualHeight : static_cast<int>(shot.h);
            projectile.visualFrameCount = shot.visualFrameCount;
            projectile.visualFrameTicks = shot.visualFrameTicks;
            projectile.visualScale = 1.0f;
            projectile.visualMirrorsWithFacing = shot.visualMirrorsWithFacing;
            projectile.facingRight = shot.vx > 0;
            if (shot.w > 0 && shot.h > 0) {
                projectile.hitboxSize = {shot.w, shot.h};
                projectile.position.x = shot.x - shot.w * 0.5f;
                projectile.position.y = shot.y - shot.h * 0.5f;
                projectile.prevPosition = projectile.position;
            } else {
                // Keep init()'s physical placement. The visual origin is the
                // authored shot anchor, independent of the muzzle hitbox.
                const float offset = shot.x - projectile.position.x
                    - projectile.hitboxSize.x * 0.5f;
                projectile.visualOffsetX = shot.visualMirrorsWithFacing
                    && !projectile.facingRight ? -offset : offset;
            }
        }
        if (shot.mine) {
            projectile.mineHazard = true;
            projectile.projGravity = 0.25f;
            projectile.lifetime = 100000;
        }
        projectiles.push_back(projectile);
    }
    enemy.pendingShots.clear();
}

} // namespace mmx::gameplay_enemies
