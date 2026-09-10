// boss_se_fight.cpp - Storm Eagle's measured fight lane (SE-B5 E6 + E7a).
// Owns: the fixed fight-loop cursor and the measured hold motion.
//
// Split out of boss.cpp on 2026-09-01. That file already carried the Chill
// Penguin FSM lane and was at its QC8 ceiling for the third time; the note left
// at the previous raise said to split the SE lane rather than raise a fourth
// time, so this is that split. The boss_cp_* files are the precedent.

#include "entities/boss.h"
#include "entities/boss_se_fight_loop.h"
#include "entities/boss_se_hover_wind_schedule.h"
#include "entities/boss_se_chain_motion.h"
#include "entities/boss_se_egg_drop.h"
#include "entities/boss_se_hold_motion.h"
#include "entities/boss_se_recover_motion.h"
#include "entities/player.h"

namespace mmx {

// SE-B5 E6: walk the measured fixed loop.
//
// Unlike Chill Penguin there is no selector: SE-B2 observed the SAME order in
// 7/7 live-player windows, with only durations modulated by the player. So the
// whole FSM is a cursor over boss_se_fight_loop::kLoop.
//
// The wrap goes to kLoopRestartIndex, not 0. Step 0 is the one-time fight-on
// entry hold; re-entering it every cycle would put two Hover holds back to
// back, which no live window shows.
//
// NONCLAIM: per-step durations are one observation (the header says so). The
// engine needs a concrete number, so it uses that one; a second captured fight
// can refute it and the table is regenerated, not edited.
float Boss::seHoverWindPushPx() const {
    // Measured (attack_kinematics.json `hoverWind`, obs corpus): held-left
    // frames inside the hover read dx +136 fp, which is the walk's -376 plus
    // the wind's +512, and every pause frame reads -376; the whole gust window
    // is pure -376, so the Gust step is windless. The law is stated for
    // GROUNDED X, so an airborne X feels nothing here.
    if (!seMeasured_ || !active) return 0.0f;
    // Only while the fight loop is actually running: the same states
    // Boss::update() gates updateSeFightLoop() on. A Storm Eagle that exists
    // in the campaign data but has not been activated does not blow -- caught
    // by the storm-eagle:spawn pixel gate, which had X 60 px left of his
    // spawn, exactly 2 px/frame times that anchor's 30 frames.
    if (!(bossState == BossState::Idle || bossState == BossState::Startup ||
          bossState == BossState::Active || bossState == BossState::Recovery ||
          bossState == BossState::Stunned))
        return 0.0f;
    if (!target_ || !target_->onGround) return 0.0f;
    if (seStepIndex_ < 0 || seStepIndex_ >= boss_se_fight_loop::kStepCount)
        return 0.0f;
    if (boss_se_fight_loop::kLoop[seStepIndex_].state !=
        boss_se_fight_loop::SeLoopState::Hover)
        return 0.0f;
    // R5.2: the wind pauses. The gate byte's schedule is measured per Hover
    // step (boss_se_hover_wind_schedule.h): 120, 240 or 360 frames of blowing
    // and then exactly 40 of pause, identical in every observation run that
    // reaches the step. updateSeFightLoop() has already incremented seTimer_
    // for this frame by the time the scene reads the wind, so the step's
    // 0-based frame is seTimer_ - 1.
    if (!boss_se_hover_wind::blowing(seStepIndex_, seTimer_ - 1)) return 0.0f;
    // Away from the boss's x column. A player exactly on it is pushed the way
    // he faces nothing: the sign of zero is right of the column.
    return target_->position.x < position.x ? -kSeHoverWindPushPx
                                            : +kSeHoverWindPushPx;
}

void Boss::updateSeFightLoop() {
    using namespace boss_se_fight_loop;

    velocity = {0, 0};
    if (target_) facingRight = (dirToTarget() > 0);

    // E7a: replay the measured vertical law for this step, in the same 1/256 px
    // fixed point the source series is recorded in, so the dy sequence is exact
    // rather than accumulated float error. x is fixed in every hold ("xFixed").
    //
    // All eight steps have a measured law since plan task R5: five holds and
    // the Gust from boss_se_hold_motion.h, the two Chain steps from
    // boss_se_chain_motion.h, and the s4 Recover from
    // boss_se_recover_motion.h. Each table returns 0 for the steps it does not
    // cover.
    if (!seYFpInit_) {
        seYFp_ = static_cast<int>(position.y * 256.0f);
        seXFp_ = static_cast<int>(position.x * 256.0f);
        seYFpInit_ = true;
    }
    if (kLoop[seStepIndex_].state == SeLoopState::Chain) {
        // E7b: the Chain steps replay the measured climb / top-teleport /
        // swoop / descent program (boss_se_chain_motion.h, generated from
        // attack_kinematics.json). SET segments are the source's top-teleports
        // in absolute source coordinates, which the shipped (6192,196) spawn
        // shares. Past the program the step holds (the hit-pause frames).
        const auto d = boss_se_chain_motion::at(seStepIndex_, seTimer_);
        if (d.set) {
            seXFp_ = d.a;
            seYFp_ = d.b;
        } else {
            seXFp_ += d.a;
            seYFp_ += d.b;
        }
        position.x = static_cast<float>(seXFp_) / 256.0f;
    } else {
        // R5: the s4 Recover step has a measured law now
        // (recover_kinematics.json -> boss_se_recover_motion.h): one flat
        // frame, 85 frames of exactly +1 px in screen y, then 189 flat frames,
        // which is the descent back to the hover row the preceding Chain step
        // left it above. Both tables return 0 for a step they do not cover, so
        // the two add without a branch.
        seYFp_ += boss_se_hold_motion::dyFpAt(seStepIndex_, seTimer_);
        seYFp_ += boss_se_recover_motion::dyFpAt(seStepIndex_, seTimer_);
    }
    position.y = static_cast<float>(seYFp_) / 256.0f;

    // R7.eggs: the egg drop. The whole program is deterministic and measured
    // (boss_se_egg_drop.h), so the boss pushes the egg itself on the Gust
    // step's frame 169 -- 5/5 live gusts in the observation corpus and the
    // movie's f16538 + 169 = f16707 -- at the measured offset, with the
    // measured launch velocity and the uncapped ballistic term.
    if (kLoop[seStepIndex_].state == SeLoopState::Gust &&
        seTimer_ == boss_se_egg::kSpawnFrameInGust) {
        seEggSpawnX_ = position.x + boss_se_egg::kSpawnOffsetX;
        seEggSpawnY_ = position.y + boss_se_egg::kSpawnOffsetY;
        // +1 because the hatch countdown below spends a frame on this one too.
        seHatchIn_ = boss_se_egg::kHatchDelayFromSpawn + 1;
        pendingShots.push_back(PendingShot{
            position.x + boss_se_egg::kSpawnOffsetX,
            position.y + boss_se_egg::kSpawnOffsetY,
            boss_se_egg::kVelocityX,
            boss_se_egg::kVelocityY,
            boss_se_egg::kProvisionalDamage,
            boss_se_egg::kGravity,
            /*holdFirstFrame=*/true,
            boss_se_egg::kLifetimeFrames,
            boss_se_egg::kSpritePath,
            boss_se_egg::kSpriteWidth,
            boss_se_egg::kSpriteHeight});
    }

    // R7.eggs: the hatch. All four eaglets appear on the frame after the
    // egg's last row, at the egg's spawn plus the measured (+60, +115), each
    // on one of the four measured diagonals with its measured lifetime.
    if (seHatchIn_ > 0 && --seHatchIn_ == 0) {
        for (const auto& e : boss_se_egg::kEaglets) {
            pendingEnemies.push_back(PendingEnemy{
                boss_se_egg::kEagletEnemyId,
                seEggSpawnX_ + boss_se_egg::kHatchOffsetXFromEgg,
                seEggSpawnY_ + boss_se_egg::kHatchOffsetYFromEgg,
                e.vxFp, e.vyFp, e.lifeFrames});
        }
    }

    seTimer_++;
    if (seTimer_ >= kLoop[seStepIndex_].observedFrames) {
        seTimer_ = 0;
        seStepIndex_ = (seStepIndex_ + 1 >= kStepCount) ? kLoopRestartIndex
                                                        : seStepIndex_ + 1;
    }
}

} // namespace mmx
