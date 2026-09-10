// gameplay_boss.h - coordinates boss activation, awards, and fight flow.
// Boundary: boss AI and health state stay owned by entities/boss modules.

#pragma once

#include "entities/boss.h"
#include "entities/player.h"
#include "entities/projectile.h"
#include "systems/audio.h"
#include "systems/camera.h"
#include "systems/tilemap.h"
#include "physics/collision.h"
#include "ui/hud.h"

#include <vector>

namespace mmx::gameplay_boss {

struct StageClearDecisionFlags {
    bool rescue = false;
    bool defeat = false;
};

inline bool ownsMeasuredCpFight(const Boss& boss) {
    if (!boss.active || !boss.cpMeasured()) return false;
    switch (boss.bossState) {
        case BossState::Idle:
        case BossState::Startup:
        case BossState::Active:
        case BossState::Recovery:
        case BossState::Stunned:
            return true;
        default:
            return false;
    }
}

inline bool shouldHoldCpDeath(const Boss& boss, const Player& player) {
    if (!ownsMeasuredCpFight(boss) || !player.isDead()) return false;
    if (player.deathTimer() < 1 || player.deathTimer() > 30) return false;
    return true;
}

// Own the single scene-side Boss update/physics step.  The CP native
// integration boundary is measured in
// knowledge_base/mmx1/bosses/chill-penguin/death_scheduler.json: the Boss
// interval is held while the Player death clock continues in GameplayScene;
// downstream drains and rewards remain in that scene.
inline void updateBossKinematicsForScene(Boss& boss,
                                         const Player& player,
                                         Tilemap& tilemap) {
    if (!boss.active) return;
    if (shouldHoldCpDeath(boss, player)) {
        boss.savePosition();
        return;
    }

    const bool cpFightOwner = ownsMeasuredCpFight(boss);
    const bool scriptedKinematics = boss.usesScriptedKinematics();
    if (scriptedKinematics) boss.savePosition();
    boss.update(0);
    if (cpFightOwner) {
        // The measured CP program owns its post-update position snapshot;
        // generic collision must not apply a second movement step.
        boss.savePosition();
    }
    if (!cpFightOwner && !scriptedKinematics) {
        physics::moveAndCollide(boss, tilemap);
    }
}

inline void drainPendingShots(Boss& boss, std::vector<Projectile>& projectiles) {
    for (const auto& shot : boss.pendingShots) {
        Projectile projectile;
        projectile.init(shot.x, shot.y, shot.vx, shot.vy, ProjectileType::Normal);
        projectile.isPlayerShot = false;
        projectile.applyEnemyVisual();
        projectile.damage = shot.damage;
        projectile.ballisticGravity = shot.gravity;
        if (shot.holdFirstFrame) projectile.holdFirstTick = true;
        if (shot.lifetime > 0) projectile.lifetime = shot.lifetime;
        if (!shot.sprite.empty()) {
            // Measured boss-shot art, centred on the authored spawn anchor --
            // the same mapping gameplay_enemies::drainPendingShots uses.
            projectile.visualSpritePath = shot.sprite;
            projectile.visualFrameWidth = static_cast<int>(shot.w);
            projectile.visualFrameHeight = static_cast<int>(shot.h);
            projectile.visualFrameCount = 1;
            projectile.visualScale = 1.0f;
            projectile.visualMirrorsWithFacing = false;
            projectile.hitboxSize = {shot.w, shot.h};
            projectile.position.x = shot.x - shot.w * 0.5f;
            projectile.position.y = shot.y - shot.h * 0.5f;
            projectile.prevPosition = projectile.position;
        }
        projectiles.push_back(projectile);
    }
    boss.pendingShots.clear();
}

inline StageClearDecisionFlags stageClearDecisionFlags(const Boss& boss) {
    return {
        boss.isRescued() && boss.rescueTimer() > 120,
        boss.isDead()
    };
}

// CP-B1C-H1: single owner of the boss-HP-bar decision.
// The source withholds the gauge until t313, so visibility is asked of the
// boss rather than assumed from activation. `immediate` is on only for the
// source-timed scripted intro, whose displayHealth already carries the ROM's
// exact +1-per-2-frames ladder and must not be re-eased by the HUD.
inline void syncBossHpBar(const Boss& boss, HUD& hud) {
    if (!boss.introHudVisible()) {
        hud.hideBossHP();
        return;
    }
    hud.showBossHP(boss.displayHealth(), boss.maxHealth,
                   boss.usesScriptedKinematics());
}

inline void activateBossEncounter(Boss& boss,
                                  bool& bossLocked,
                                  Camera& camera,
                                  HUD& hud,
                                  float roomX,
                                  float roomY,
                                  float roomW,
                                  float roomH) {
    bossLocked = true;
    boss.activate();
    camera.setRoom(roomX, roomY, roomW, roomH);
    syncBossHpBar(boss, hud);
    AudioManager::playSFX(SFX::BossIntro);
    AudioManager::playBGM("boss");
}

inline void activateBossRoom(Boss& boss,
                             bool& bossLocked,
                             Camera& camera,
                             HUD& hud,
                             const Room& room) {
    activateBossEncounter(
        boss, bossLocked, camera, hud, room.x, room.y, room.w, room.h);
}

inline void refreshBossHpBar(const Boss& boss, bool bossLocked, HUD& hud) {
    if (!bossLocked || !boss.active) return;
    if (!boss.introHudVisible()) {
        hud.hideBossHP();
        return;
    }
    if (boss.bossState != BossState::Dormant) syncBossHpBar(boss, hud);
}

inline void applyDeathCameraShake(const Boss& boss, Camera& camera) {
    if (!boss.isDying()) return;

    const int deathTimer = boss.deathTimer();
    if (deathTimer == 1) camera.shake(3.0f, 40);
    if (deathTimer == 45) camera.shake(4.0f, 35);
    if (deathTimer == 85) camera.shake(5.0f, 25);
    if (deathTimer == 115) camera.shake(8.0f, 10);
}

} // namespace mmx::gameplay_boss
