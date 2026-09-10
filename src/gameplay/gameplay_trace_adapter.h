// gameplay_trace_adapter.h - adapts GameplayScene state into parity trace rows.
// Boundary: observation only; trace code must not change runtime behavior.

#pragma once

#include "gameplay/gameplay_trace_rows.h"
#include "gameplay/gameplay_trace_types.h"
#include "entities/boss.h"
#include "entities/enemy.h"
#include "entities/pickup.h"
#include "entities/player.h"
#include "entities/projectile.h"
#include "entities/stage_object.h"
#include "systems/audio.h"
#include "systems/camera.h"

#include <cstddef>
#include <cstdio>
#include <string>

namespace mmx::gameplay_trace {

template <typename Projectiles, typename Enemies, typename StageObjects, typename IceTrailBits>
inline void writeProjectileFrame(FILE* trace,
                                 long tick,
                                 const Camera& camera,
                                 const Player& player,
                                 const CpCapsuleTraceState& capsule,
                                 const Projectiles& projectiles,
                                 const Enemies& enemies,
                                 const StageObjects& stageObjects,
                                 bool bossActive,
                                 const Boss& boss,
                                 const IceTrailBits& iceTrailBits) {
    writeProjectileRow(trace, tick, "camera", 0, "-",
                       0, 0, camera.x(), camera.y(), 0.0f, 0.0f);
    writeProjectileRow(trace, tick, "player", 0, "-",
                       0, 0, player.position.x, player.position.y,
                       player.velocity.x, player.velocity.y);
    writeProjectileRow(trace, tick, "player_hp", 0, "-",
                       player.health, player.iframeTimer(),
                       player.position.x, player.position.y,
                       player.velocity.x, player.velocity.y);

    if (capsule.active || capsule.grantEvent || capsule.releaseEvent) {
        const char* state = capsule.releaseEvent
            ? "release"
            : (capsule.grantEvent ? "grant" : "active");
        const int eventCode = capsule.releaseEvent
            ? 2
            : (capsule.grantEvent ? 1 : 0);
        writeProjectileRow(trace, tick, "cp_capsule",
                           capsule.sourceFrame, state,
                           capsule.armorBoots ? 1 : 0, eventCode,
                           player.position.x, player.position.y,
                           player.velocity.x, player.velocity.y);
        writeProjectileRow(trace, tick, "cp_capsule_anim",
                           capsule.sourceFrame, "source_anim",
                           capsule.animByte, eventCode,
                           player.position.x, player.position.y,
                           player.velocity.x, player.velocity.y);
    }

    for (const auto& projectile : projectiles) {
        if (!projectile.active || !projectile.isPlayerShot) continue;
        if (projectile.dormantFrames > 0) continue;
        writeProjectileRow(trace, tick, "proj", projectile.serial,
                           projectile.weaponId.c_str(),
                           (projectile.type != ProjectileType::Normal) ? 1 : 0,
                           projectile.isShatterFragment ? 1 : 0,
                           projectile.position.x, projectile.position.y,
                           projectile.vx, projectile.vy);
    }

    for (std::size_t i = 0; i < enemies.size(); i++) {
        const auto& enemy = enemies[i];
        if (!enemy.active || enemy.enemyState == EnemyState::Dead || enemy.health <= 0) {
            continue;
        }
        const AABB hitbox = enemy.getHitbox();
        writeProjectileRow(trace, tick, "enemy",
                           static_cast<int>(i), enemy.type.c_str(),
                           enemy.health, 0,
                           hitbox.x + hitbox.w * 0.5f,
                           hitbox.y + hitbox.h * 0.5f,
                           0.0f, 0.0f);
    }

    for (std::size_t i = 0; i < stageObjects.size(); i++) {
        const auto& object = stageObjects[i];
        if (!object.active) continue;
        const AABB hitbox = object.getHitbox();
        writeProjectileRow(trace, tick, "stage_object",
                           static_cast<int>(i), object.id().c_str(),
                           object.contactDamage(),
                           object.canBeDamagedByPlayerShots() ? 1 : 0,
                           hitbox.x + hitbox.w * 0.5f,
                           hitbox.y + hitbox.h * 0.5f,
                           0.0f, 0.0f);
    }

    if (bossActive && boss.active && boss.bossState != BossState::Dormant &&
        boss.bossState != BossState::Dead && boss.health > 0) {
        const AABB hitbox = boss.getHitbox();
        writeProjectileRow(trace, tick, "boss", 0,
                           boss.type.c_str(), boss.health, 0,
                           hitbox.x + hitbox.w * 0.5f,
                           hitbox.y + hitbox.h * 0.5f,
                           0.0f, 0.0f);
    }

    for (const auto& bit : iceTrailBits) {
        writeProjectileRow(trace, tick,
                           bit.kind == 0 ? "trail" : "fx",
                           bit.serial, "-", 0, 0,
                           bit.x, bit.y, bit.vx, bit.vy);
    }
}

template <typename Projectiles,
          typename Enemies,
          typename StageObjects,
          typename Pickups,
          typename DeathOrbs,
          typename IceTrailBits,
          typename TorpedoPuffs>
inline void writeParityFrame(FILE* trace,
                             long tick,
                             const Camera& camera,
                             const ParityFrameState& state,
                             const Player& player,
                             const Projectiles& projectiles,
                             const Enemies& enemies,
                             bool bossActive,
                             const Boss& boss,
                             const StageObjects& stageObjects,
                             const Pickups& pickups,
                             const DeathOrbs& deathOrbs,
                             const IceTrailBits& iceTrailBits,
                             const TorpedoPuffs& torpedoPuffs,
                             std::size_t& apuLogIndex,
                             std::size_t& sfxLogIndex) {
    writeParityRow(
        trace, tick, "camera", 0,
        state.activeCameraSectionId.empty() ? "-" : state.activeCameraSectionId.c_str(),
        state.bossLocked ? 1 : 0, 0, camera.x(), camera.y(), 0.0f, 0.0f,
        state.stageClear ? 1 : 0, state.stageClearTimer,
        state.gameOver ? 1 : 0, state.paused ? 1 : 0);

    const std::string playerWeapon =
        player.weaponInventory.isBuster() ? "buster" : player.weaponInventory.current().id;
    writeParityRow(
        trace, tick, "player", 0, playerWeapon.c_str(),
        static_cast<int>(player.state()), player.health,
        player.position.x, player.position.y,
        player.velocity.x, player.velocity.y,
        player.progressState().maxHealth, player.iframeTimer(),
        player.facingRight ? 1 : 0, player.lives,
        player.renderFrameIndexForTest(), player.chargeLevel(),
        player.chargeTimer(), player.weaponInventory.currentIndex);
    writeParityRow(
        trace, tick, "player_hp", 0, "-", player.deathTimer(), player.health,
        player.position.x, player.position.y,
        player.velocity.x, player.velocity.y,
        player.progressState().maxHealth, player.iframeTimer(), player.isDead() ? 1 : 0, 0);

    const auto& apu = AudioManager::apuLog();
    while (apuLogIndex < apu.size()) {
        const auto& event = apu[apuLogIndex++];
        writeParityRow(
            trace, tick, "audio_apu", event.frame, "-", event.command, 0,
            player.position.x, player.position.y, 0.0f, 0.0f,
            event.command, 0, 0, 0);
    }

    const auto& sfx = AudioManager::sfxLog();
    while (sfxLogIndex < sfx.size()) {
        const auto& event = sfx[sfxLogIndex++];
        writeParityRow(
            trace, tick, "audio_sfx", event.frame, "-", event.id, 0,
            player.position.x, player.position.y, 0.0f, 0.0f,
            event.id, 0, 0, 0);
    }

    for (const auto& projectile : projectiles) {
        if (!projectile.active || projectile.dormantFrames > 0) continue;
        writeParityRow(
            trace, tick, "projectile", projectile.serial,
            projectile.weaponId.empty() ? "-" : projectile.weaponId.c_str(),
            static_cast<int>(projectile.type), projectile.damage,
            projectile.position.x, projectile.position.y,
            projectile.vx, projectile.vy, projectile.isPlayerShot ? 1 : 0,
            projectile.isShatterFragment ? 1 : 0,
            projectile.ageFrames, projectile.facingRight ? 1 : 0);
    }

    for (const auto& enemy : enemies) {
        const AABB hitbox = enemy.getHitbox();
        writeParityRow(
            trace, tick, "enemy", enemy.serial, enemy.type.c_str(),
            static_cast<int>(enemy.enemyState), enemy.health,
            hitbox.x + hitbox.w * 0.5f, hitbox.y + hitbox.h * 0.5f,
            enemy.velocity.x, enemy.velocity.y, enemy.active ? 1 : 0,
            enemy.alive ? 1 : 0, enemy.deathTimer, enemy.deathBurstNodeCount());
    }

    if (bossActive) {
        const AABB hitbox = boss.getHitbox();
        writeParityRow(
            trace, tick, "boss", 0, boss.type.c_str(),
            static_cast<int>(boss.bossState), boss.health,
            hitbox.x + hitbox.w * 0.5f, hitbox.y + hitbox.h * 0.5f,
            boss.velocity.x, boss.velocity.y, boss.active ? 1 : 0,
            boss.isDead() ? 1 : 0, boss.deathTimer(), state.bossLocked ? 1 : 0,
            boss.displayHealth(), boss.introVisible() ? 1 : 0,
            boss.introSourceTick(), static_cast<int>(boss.cpState()));
    }

    for (std::size_t i = 0; i < stageObjects.size(); ++i) {
        const auto& object = stageObjects[i];
        const AABB hitbox = object.getHitbox();
        writeParityRow(
            trace, tick, "stage_object", static_cast<int>(i), object.id().c_str(),
            object.animationFrameIndexForTest(), object.contactDamage(),
            hitbox.x + hitbox.w * 0.5f, hitbox.y + hitbox.h * 0.5f, 0.0f, 0.0f,
            object.active ? 1 : 0, object.canBeDamagedByPlayerShots() ? 1 : 0,
            object.sourceOid(), 0);
    }

    for (std::size_t i = 0; i < pickups.size(); ++i) {
        const auto& pickup = pickups[i];
        writeParityRow(
            trace, tick, "pickup", static_cast<int>(i),
            pickup.persistentId.empty() ? "-" : pickup.persistentId.c_str(),
            static_cast<int>(pickup.type), pickup.value,
            pickup.position.x, pickup.position.y,
            0.0f, pickup.vy, pickup.active ? 1 : 0,
            pickup.onGround ? 1 : 0, pickup.lifetime, 0);
    }

    for (const auto& orb : deathOrbs) {
        writeParityRow(
            trace, tick, "fx", 0, "death_orb", orb.animFrame, orb.lifetime,
            orb.x, orb.y, orb.vx, orb.vy, 0, 0, 0, 0);
    }

    for (const auto& bit : iceTrailBits) {
        writeParityRow(
            trace, tick, "fx", bit.serial, bit.kind == 0 ? "ice_trail" : "ice_debris",
            bit.age, bit.lifetime, bit.x, bit.y, bit.vx, bit.vy,
            bit.kind, bit.hflip ? 1 : 0, 0, 0);
    }

    for (const auto& puff : torpedoPuffs) {
        writeParityRow(
            trace, tick, "fx", puff.ownerSerial, "torpedo_puff", puff.age, 0,
            puff.x, puff.y, 0.0f, 0.0f, 0, 0, 0, 0);
    }

    const std::string stage = state.activeStageId.str();
    writeParityRow(
        trace, tick, "transition", 0, stage.c_str(), state.transitionState, 0,
        player.position.x, player.position.y, 0.0f, 0.0f,
        state.transitionTimer, state.bossLocked ? 1 : 0,
        state.returnToStageSelect ? 1 : 0, state.returnToTitle ? 1 : 0);
}

} // namespace mmx::gameplay_trace
