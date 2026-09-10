// gameplay_scene_boss.cpp - owns the per-tick boss orchestration lane.
// Boss AI remains in entities/boss; stage rewards remain in stage progress.

#include "gameplay/gameplay_scene.h"

#include "gameplay/gameplay_boss.h"
#include "gameplay/gameplay_stage_progress.h"
#include "physics/collision.h"
#include "systems/audio.h"

namespace mmx {

void GameplayScene::updateBoss() {
    if (!boss_.active) return;

    // Capture ownership before update: source t409 transitions Intro -> Idle,
    // but that final intro tick must still avoid an extra generic-physics move.
    // From the following tick onward the pre-existing CP fight physics resumes.
    gameplay_boss::updateBossKinematicsForScene(boss_, player_, tilemap_);

    // R5: Storm Eagle's measured hover flap wind
    // (knowledge_base/mmx1/bosses/storm-eagle/attack_kinematics.json
    // `hoverWind`, obs corpus): while the boss hovers, grounded X is pushed
    // away from its x column at exactly 2 px per frame -- the corpus reads
    // dx +136 fp on held-left hover frames, which is the walk's -376 plus the
    // wind's +512, and -376 on every pause frame. The push is resolved against
    // the tilemap the way the conveyor push is, and X's own velocity is left
    // alone so the walk still reads normally. Review row
    // P4-SE-HOVER-WIND-2026-09-06: the source's gate (the boss slot's +0x03
    // byte, ~45-frame pauses every ~500 frames) is approximate in the KB, so
    // the engine gates on the fight-loop state and the wind never pauses.
    if (const float wind = boss_.seHoverWindPushPx(); wind != 0.0f) {
        const float velocityX = player_.velocity.x;
        player_.velocity.x = wind;
        physics::moveAndResolveX(player_, tilemap_);
        player_.velocity.x = velocityX;
    }

    // R7.eggs: this used to be a second, drifting copy of
    // gameplay_boss::drainPendingShots that dropped every field the header's
    // version maps -- so the egg's ballistic term and its measured art would
    // have been silently lost on the real path while the contract passed
    // against the header. One owner now.
    gameplay_boss::drainPendingShots(boss_, projectiles_);

    // R7.eggs: enemies the boss's own measured program spawns (the four
    // eaglets the egg hatches into). Same shape as the shot drain.
    for (const auto& pending : boss_.pendingEnemies) {
        Enemy e;
        e.init(pending.id, pending.x, pending.y);
        if (!e.active) continue;          // no definition: init already said so
        e.cameraActivated = true;         // Boss-owned child: combat is immediate
        e.setTarget(&player_);
        e.setTilemap(&tilemap_);
        e.setEagletBurst(pending.vxFp, pending.vyFp, pending.lifeFrames);
        enemies_.push_back(std::move(e));
    }
    boss_.pendingEnemies.clear();

    if (boss_.isRescued() && boss_.rescueTimer() > 120) {
        stageClear_ = true;
        stageClearTimer_ = 0;
        hud_.hideBossHP();
    }

    if (boss_.bossState == BossState::Dying) {
        const int deathTimer = boss_.deathTimer();
        if (deathTimer == 1) camera_.shake(3.0f, 40);
        if (deathTimer == 45) camera_.shake(4.0f, 35);
        if (deathTimer == 85) camera_.shake(5.0f, 25);
        if (deathTimer == 115) camera_.shake(8.0f, 10);
    }

    if (boss_.isDead()) {
        stageClear_ = true;
        stageClearTimer_ = 0;
        hud_.hideBossHP();
        AudioManager::stopBGM();
        gameplay_stage_progress::applyBossClearRewards(
            player_, boss_, activeStageId_, stageTimer_, bossRushMode,
            suppressProgressionRewards, awardedWeapon_);
        // GC2.1b: when a weapon is actually awarded the sequence owns the audio
        // from tick 0 - its measured victory cue is $2D - so the generic
        // stage-clear jingle would be a second sound on top of it.
        beginWeaponGetSequence();
        if (!weaponGetSequenceActive()) {
            AudioManager::playSFX(SFX::StageClear);
        }
    }

    if (bossLocked_ && boss_.active &&
        boss_.bossState != BossState::Dormant) {
        gameplay_boss::syncBossHpBar(boss_, hud_);
    }
}

} // namespace mmx
