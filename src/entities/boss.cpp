// boss.cpp - runs boss fight state, attack selection, and rendering.
// Owns: boss timers, hit reactions, projectiles, and per-boss behavior lanes.

#include "entities/boss.h"
#include "entities/boss_death_timeline.h"
#include "entities/boss_cp_intro_timeline.h"
#include "entities/boss_se_intro_timeline.h"
#include "entities/boss_cp_tables.h"
#include "entities/boss_cp_barhang_y.h"
#include "entities/boss_cp_leap_short_y.h"
#include "entities/player.h"
#include "entities/player_anchor.h"
#include "app/constants.h"
#include "systems/raylib_resource.h"
#include "systems/audio.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <numeric>

namespace mmx {

using namespace boss_cp_tables;

// ============================================================================
// BossPhase — weighted random attack selection
// ============================================================================

const BossAttack* BossPhase::selectAttack() const {
    if (attacks.empty()) return nullptr;

    int totalWeight = 0;
    for (const auto& a : attacks) {
        if (a.weight > 0) totalWeight += a.weight;
    }
    if (totalWeight <= 0) return nullptr;

    int roll = std::rand() % totalWeight;
    int cumulative = 0;
    for (const auto& a : attacks) {
        if (a.weight <= 0) continue;
        cumulative += a.weight;
        if (roll < cumulative) return &a;
    }
    return nullptr;
}

namespace {

constexpr int kBossHitFlashFrames = 30;
constexpr int kCpBarTargetOffsetFp = 10 * 256;
constexpr float kCpBarCenterFromLeftWallPx = 98.0f;
constexpr int kCpHangMotionStartTimer = 32;
constexpr int kCpHangMotionEndTimer = 63;
constexpr int kCpSlideSourceRows = 148;

int ceilPixelsForFixedPointDelta(int deltaFp) {
    int pixels = deltaFp / 256;
    if (deltaFp > 0 && deltaFp % 256 != 0) ++pixels;
    return pixels;
}

int floorPixelsForFixedPoint(int valueFp) {
    int pixels = valueFp / 256;
    if (valueFp < 0 && valueFp % 256 != 0) --pixels;
    return pixels;
}

} // namespace

// ============================================================================
// Activation and damage
// ============================================================================

void Boss::activate() {
    dormant = false;
    bossState = BossState::Intro;
    introTimer_ = 0;
    stateTimer_ = 0;

    if (cpMeasured_) {
        // Arena geometry, spawn-relative (oracle: spawn x 7888, slide clamps
        // [7710, 7905], floor y 428 — attack_kinematics.json "arena" refs).
        if (!cpStageIntroConfigured_) cpFloorY_ = position.y;
        cpWallLeft_ = position.x - 178.0f;
        cpWallRight_ = position.x + 17.0f;
        cpBarCenterFp_ = static_cast<int>(std::lround(
            (cpWallLeft_ + kCpBarCenterFromLeftWallPx) * 256.0f));
    }
}

void Boss::configureCpStageIntro(float landingY) {
    if (!cpMeasured_) return;
    cpStageIntroConfigured_ = true;
    cpIntroCeilingY_ = position.y;
    cpFloorY_ = landingY;
    cpDisplayHealth_ = 0;
    cpHudVisible_ = false;
}

// ============================================================================
// U35 B5 — measured Chill Penguin FSM
// Spec: knowledge_base/mmx1/bosses/chill-penguin/{fsm_observations,
// attack_kinematics, damage_matrix}.json. Constants below carry their
// oracle $sources; the contract test (boss_cp_fsm_contract_test) pins them.
// ============================================================================
void Boss::cpEnter(CpState s) {
    cpState_ = s;
    cpTimer_ = 0;
    velocity = {0, 0};
    switch (s) {
        case CpState::IdleGate:
            bossState = BossState::Idle;
            cpIdleLen_ = cpFirstIdle_ ? kCpFirstIdleFrames : kCpIdleFrames;
            cpFirstIdle_ = false;
            cpIdleShotDone_ = false;
            anim_.play("idle");
            break;
        case CpState::Slide: {
            bossState = BossState::Active;
            // face/aim toward X at entry; the exact fp law runs in update.
            if (target_) facingRight = (dirToTarget() > 0);
            cpVelFp_ = 0; // loaded at timer 52, then decayed before the first move
            cpXFp_ = static_cast<int>(std::lround(position.x * 256.0f));
            anim_.play("slide");
            break;
        }
        case CpState::Volley:
            bossState = BossState::Active;
            if (target_) facingRight = (dirToTarget() > 0);
            anim_.play("shoot");
            break;
        case CpState::BarHang:
            bossState = BossState::Active;
            cpHangEntryYFp_ = static_cast<int>(std::lround(position.y * 256.0f));
            cpHangXFp_ = static_cast<int>(std::lround(position.x * 256.0f));
            cpHangVelFp_ = 0;
            cpHangTargetFp_ = cpBarCenterFp_;
            anim_.play("jump");
            break;
        case CpState::Leap:
            bossState = BossState::Active;
            cpLeapEntryYFp_ = static_cast<int>(std::lround(position.y * 256.0f));
            if (target_) {
                const float targetX = std::clamp(target_->position.x,
                                                 cpWallLeft_, cpWallRight_);
                facingRight = (targetX > position.x);
            }
            cpLeapShortXFp_ = static_cast<int>(std::lround(position.x * 256.0f));
            cpLeapShortVxFp_ = 0;
            cpLeapShortTargetSampled_ = false;
            anim_.play("jump");
            break;
        case CpState::Flinch:
            bossState = BossState::Stunned;
            // hop AWAY from the hit source (= away from X).
            if (target_) facingRight = (dirToTarget() > 0);
            anim_.play("stunned");
            break;
        case CpState::None:
            break;
    }
}

void Boss::cpSelectSource(int sourceSub) {
    // Round25 source contract: attack_selector.json, ROM $86:C45B..$86:C4AA;
    // the Flinch exit's row 0 is also pinned by flinch_selector.json.
    // Rows are keyed by the old source substate and contain the literal
    // 16-entry result bytes consumed after one C019 RNG low nibble.
    static constexpr int kSourceRows[5][16] = {
        {0, 2, 2, 4, 4, 4, 4, 4, 4, 4, 6, 6, 8, 8, 8, 8},
        {0, 0, 2, 4, 4, 6, 6, 6, 6, 6, 6, 6, 8, 8, 8, 8},
        {0, 0, 0, 0, 2, 2, 4, 6, 6, 8, 8, 8, 8, 8, 8, 8},
        {0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 4, 4, 6, 8, 8},
        {0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 4, 4, 6, 6, 8},
    };
    const int row = sourceSub / 2;
    const int sourceResult = kSourceRows[row][cpRoll(16)];
    static constexpr CpState kNext[5] = {
        CpState::IdleGate, CpState::BarHang, CpState::Slide,
        CpState::Leap, CpState::Volley,
    };
    // The five requested ROM rows contain only the verified even substates
    // 0, 2, 4, 6, and 8, so sourceResult / 2 is the direct state index.
    cpEnter(kNext[sourceResult / 2]);
}

void Boss::updateCpFsm() {
    cpTimer_++;
    switch (cpState_) {
        case CpState::IdleGate: {
            velocity = {0, 0};
            if (target_) facingRight = (dirToTarget() > 0);
            if (cpTimer_ == kCpIdleShotFrame && !cpIdleShotDone_ &&
                cpIdleLen_ >= 150) {
                cpIdleShotDone_ = true;
                float dir = facingRight ? 1.0f : -1.0f;
                pendingShots.push_back({position.x + kCpShotMouthDX * dir,
                                        cpFloorY_ - 2.0f,
                                        kCpIdleShotVx * dir, 0.0f,
                                        kCpIdleShotDmg});
            }
            if (cpTimer_ >= cpIdleLen_) {
                // Source C019 caller $81:B792: old substate 0.
                cpSelectSource(0);
            }
            break;
        }
        case CpState::Slide: {
            if (cpTimer_ < kCpSlideWindup) {
                velocity = {0, 0};
                break;
            }
            if (cpTimer_ >= kCpSlideSourceRows) {
                // Source row 147 holds zero in Slide; selection occurs at 148.
                // Source C019 caller $81:B937: old substate 4.
                cpSelectSource(4);
                break;
            }
            if (cpTimer_ == kCpSlideWindup) {
                int dir = (target_ && target_->position.x < position.x) ? -1 : 1;
                // The source loads +/-1536, then decays toward zero before
                // applying the first +/-1520 movement at timer 52.
                cpVelFp_ = (kCpSlideV0Fp + kCpSlideDecFp) * dir;
                cpXFp_ = static_cast<int>(std::lround(position.x * 256.0f));
            }
            if (cpVelFp_ > 0) cpVelFp_ -= kCpSlideDecFp;
            else if (cpVelFp_ < 0) cpVelFp_ += kCpSlideDecFp;

            // Source: knowledge_base/mmx1/bosses/chill-penguin/slide_motion_ghost.json.
            // Clamp only floor(candidate / 256) and retain its fp low byte;
            // arena bounds remain activation-relative under translation.
            const int candidate = cpXFp_ + cpVelFp_;
            const int candidatePixel = static_cast<int>(std::floor(
                static_cast<double>(candidate) / 256.0));
            const int wallLeftPixel = static_cast<int>(std::floor(cpWallLeft_));
            const int wallRightPixel = static_cast<int>(std::floor(cpWallRight_));
            const int clampedPixel = std::clamp(candidatePixel,
                                                wallLeftPixel, wallRightPixel);
            const int lowByte = ((candidate % 256) + 256) % 256;
            cpXFp_ = clampedPixel * 256 + lowByte;
            if (candidatePixel < wallLeftPixel || candidatePixel > wallRightPixel) {
                cpVelFp_ = -cpVelFp_;
            }
            facingRight = (cpVelFp_ > 0) || (cpVelFp_ == 0 && facingRight);
            position.x = static_cast<float>(cpXFp_) / 256.0f;
            velocity = {0, 0};
            break;
        }
        case CpState::Volley: {
            velocity = {0, 0};
            int sinceFirst = cpTimer_ - kCpVolleyFirstShot;
            if (sinceFirst >= 0 &&
                sinceFirst < kCpVolleyShotCount * kCpVolleyShotSpacing &&
                sinceFirst % kCpVolleyShotSpacing == 0) {
                float dir = facingRight ? 1.0f : -1.0f;
                pendingShots.push_back({position.x + kCpShotMouthDX * dir,
                                        cpFloorY_ - 2.0f,
                                        kCpVolleyShotVx * dir, 0.0f,
                                        kCpVolleyShotDmg});
            }
            if (cpTimer_ >= kCpVolleyFrames) {
                // Source C019 caller $81:BABB: old substate 8.
                cpSelectSource(8);
            }
            break;
        }
        case CpState::BarHang: {
            velocity = {0, 0};
            // Source row 0 is the entry sample before the first update. Once
            // cpTimer_ advances, its value is the canonical source row. Keep
            // the measured entry phase instead of rebuilding Y from the
            // integer floor; the fractional low byte is part of the contract.
            int i = cpTimer_;
            if (i >= 0 && i < boss_cp_barhang_y::kRowCount) {
                position.y = static_cast<float>(
                    cpHangEntryYFp_ + boss_cp_barhang_y::kRelativeYFp[i]) / 256.0f;
            }
            // Source contract: knowledge_base/mmx1/bosses/chill-penguin/
            // barhang_x_ghost.json. Global row 0 is entry; rows 0..31 hold x,
            // rows 32..63 apply 32 fixed-point steps toward the side target.
            if (cpTimer_ >= kCpHangMotionStartTimer &&
                cpTimer_ <= kCpHangMotionEndTimer) {
                if (cpTimer_ == kCpHangMotionStartTimer) {
                    // The source chooses the bar column from the source-side
                    // entry x. Exact center has no measured tie-break and
                    // therefore remains stationary.
                    if (cpHangXFp_ > cpBarCenterFp_) {
                        cpHangTargetFp_ = cpBarCenterFp_ + kCpBarTargetOffsetFp;
                    } else if (cpHangXFp_ < cpBarCenterFp_) {
                        cpHangTargetFp_ = cpBarCenterFp_ - kCpBarTargetOffsetFp;
                    } else {
                        cpHangTargetFp_ = cpBarCenterFp_;
                    }
                    const int targetDeltaFp = cpHangTargetFp_ - cpHangXFp_;
                    cpHangVelFp_ = targetDeltaFp == 0
                        ? 0
                        : ceilPixelsForFixedPointDelta(targetDeltaFp) * 8;
                }
                cpHangXFp_ += cpHangVelFp_;
                position.x = static_cast<float>(cpHangXFp_) / 256.0f;
            }
            if (cpTimer_ >= boss_cp_barhang_y::kRowCount) {
                // The source's AFTEREXIT row retains the final BarHang Y FP;
                // transition selection must not reset it to the integer floor.
                position.y = static_cast<float>(
                    cpHangEntryYFp_ + boss_cp_barhang_y::kRelativeYFp[
                        boss_cp_barhang_y::kRowCount - 1]) / 256.0f;
                // Source C019 caller $81:B88A: old substate 2. The source
                // row is the contract here.
                cpSelectSource(2);
            }
            break;
        }
        case CpState::Leap: {
            velocity = {0, 0};
            constexpr int leapFrames = boss_cp_leap_short_y::kRowCount;
            // Row 0 is the source entry sample before the first update. Y
            // therefore follows the active update count. The source
            // assignment and target anchor are recorded in
            // knowledge_base/mmx1/bosses/chill-penguin/leap_x_writer_observations.json;
            // world-update order remains an explicit follow-up.
            const int yIndex = cpTimer_;
            if (yIndex >= 0 && yIndex < leapFrames) {
                position.y = static_cast<float>(
                    cpLeapEntryYFp_ + boss_cp_leap_short_y::kRelativeYFp[yIndex])
                    / 256.0f;
                if (!cpLeapShortTargetSampled_) {
                    const int playerWord = target_
                        ? static_cast<int>(std::floor(
                            player_anchor::sourceRamAnchorX(
                                target_->position.x,
                                target_->spriteWidth,
                                target_->facingRight)))
                        : floorPixelsForFixedPoint(cpLeapShortXFp_);
                    const int bossWord = floorPixelsForFixedPoint(cpLeapShortXFp_);
                    const int delta = playerWord - bossWord;
                    const int wrapped = (4 * delta) & 0xFFFF;
                    cpLeapShortVxFp_ = wrapped >= 0x8000
                        ? wrapped - 0x10000 : wrapped;
                    cpLeapShortTargetSampled_ = true;
                }
                if (cpTimer_ >= 33 && cpTimer_ <= 100) {
                    const int candidate = cpLeapShortXFp_ + cpLeapShortVxFp_;
                    const int candidatePixel = floorPixelsForFixedPoint(candidate);
                    const int fraction = candidate - candidatePixel * 256;
                    const int leftWall = static_cast<int>(std::floor(cpWallLeft_));
                    const int rightWall = static_cast<int>(std::floor(cpWallRight_));
                    const int clampedPixel = std::max(
                        leftWall, std::min(rightWall, candidatePixel));
                    cpLeapShortXFp_ = clampedPixel * 256 + fraction;
                }
                position.x = static_cast<float>(cpLeapShortXFp_) / 256.0f;
            }
            if (cpTimer_ >= leapFrames) {
                position.x = static_cast<float>(cpLeapShortXFp_) / 256.0f;
                position.y = static_cast<float>(
                    cpLeapEntryYFp_ + boss_cp_leap_short_y::kRelativeYFp[
                        boss_cp_leap_short_y::kRowCount - 1]) / 256.0f;
                // Source C019 caller $81:B9F8: old substate 6.
                cpSelectSource(6);
            }
            break;
        }
        case CpState::Flinch: {
            velocity = {0, 0};
            int i = cpTimer_ - 1;
            if (i >= 0 && i < kCpFlinchFrames) {
                position.y = cpFloorY_ + static_cast<float>(kCpFlinchYOff[i]);
                // x +1 px/f AWAY from X on frames 2..18 (17 steps).
                if (i >= 2 && i <= 18) {
                    float away = (target_ && target_->position.x < position.x)
                                     ? 1.0f : -1.0f;
                    position.x += away;
                    if (position.x < cpWallLeft_) position.x = cpWallLeft_;
                    if (position.x > cpWallRight_) position.x = cpWallRight_;
                }
            }
            if (cpTimer_ >= kCpFlinchFrames) {
                position.y = cpFloorY_;
                // R25 source C019 row 0, measured at Flinch's 22-frame exit;
                // see knowledge_base/mmx1/bosses/chill-penguin/
                // flinch_selector.json. The hop timing and XY above remain
                // unchanged.
                cpSelectSource(0);
            }
            break;
        }
        case CpState::None:
            break;
    }
}

bool Boss::takeDamage(int amount, const std::string& weaponId) {
    if (bossState == BossState::Dormant || bossState == BossState::Intro ||
        bossState == BossState::Dead || bossState == BossState::Rescued) return false;
    if (hitFlash > 0) return false;

    // Invincible during specific attacks (e.g., CP belly slide)
    if ((bossState == BossState::Startup || bossState == BossState::Active) &&
        hasCurrentAttack_ && currentAttack_.invincibleDuring) return false;

    float mult = getWeaknessMult(weaponId);
    int finalDamage = static_cast<int>(amount * mult);
    if (finalDamage < 1) finalDamage = 1;

    health -= finalDamage;
    hitFlash = kBossHitFlashFrames;

    if (isScripted_ && health < 1) {
        health = 1;
    }

    if (health <= 0) {
        health = 0;
        bossState = BossState::Dying;
        deathTimer_ = 0;
        deathExplosions_ = 0;
        velocity = {0, 0};
        anim_.play("dying");
        if (deathTimeline_) {
            // Measured law (CP-B8, generalised by SE-B8): the killing hit
            // defeats SAME-FRAME with the $13 cry at death-local d0. The
            // generic dying SFX at d+1 stays unmeasured-boss only.
            deathPopIndex_ = 0;
            AudioManager::stopBGM();
            AudioManager::playApu(deathTimeline_->cryApuCommand);
            deathWhiteFlash_ = true;
        }
        return true;
    }

    // R6 2026-09-07: the impact goes out on a SURVIVING hit only. Neither
    // movie plays $11 on the frame a boss's HP reaches 0 -- both put only the
    // $13 cry there, which is the same command B8 already measured as the
    // death cry and which the branch above emits.
    AudioManager::playApu(0x11);   // measured hit impact (R6)

    if (cpMeasured_) {
        // Measured flinch rule (fsm_observations.json): every connecting hit
        // enters the 22f flinch hop EXCEPT during the volley, which absorbs
        // hits with no reaction (2/2 observed s8-window hits; the transition
        // table has zero s8->s10 edges).
        if (cpState_ != CpState::Volley && cpState_ != CpState::Flinch) {
            cpEnter(CpState::Flinch);
        }
        return true;
    }

    if (absorbsDamageInPlace_) {
        // Measured: no stun, no flinch, no phase change on any nonlethal hit
        // (fight_timeline.json, 23/23 absorbed). Returning here also skips
        // checkPhaseTransition(), whose phases are engine_extraction
        // inventions the corpus contradicts.
        return true;
    }

    // Brief stun on weakness hit
    if (mult > 1.0f && bossState != BossState::Stunned) {
        bossState = BossState::Stunned;
        stateTimer_ = 15; // Short stun
        velocity = {0, 0};
        anim_.play("stunned");
    }

    checkPhaseTransition();
    return true;
}

AABB Boss::getPlayerShotHitbox() const {
    const AABB body = getHitbox();
    if (type != "flame-mammoth") return body;

    // R368, bosses/flame-mammoth/projectile_hurtbox.json: ROM86C839
    // = (0,+3,19,29), so the shot body extends 58px up from the feet.
    // Keep the existing bottom-center render/terrain anchor: enlarging
    // Actor::hitboxSize would also move the sprite and its floor contact.
    return {body.x + body.w * 0.5f - 19.0f, body.bottom() - 58.0f,
            38.0f, 58.0f};
}

bool Boss::overlapsPlayerShot(const AABB& shot) const {
    const AABB body = getPlayerShotHitbox();
    if (type != "flame-mammoth") return shot.overlaps(body);
    // Source849C0E retains contact at equal summed half-extents.
    return shot.left() <= body.right() && shot.right() >= body.left() &&
           shot.top() <= body.bottom() && shot.bottom() >= body.top();
}

float Boss::getWeaknessMult(const std::string& weaponId) const {
    for (const auto& w : weaknesses) {
        if (w.first == weaponId) return w.second;
    }
    return 1.0f;
}

void Boss::checkPhaseTransition() {
    float hpPercent = static_cast<float>(health) / maxHealth;
    for (int i = static_cast<int>(phases.size()) - 1; i >= 0; i--) {
        if (hpPercent <= phases[i].healthThreshold && i > currentPhase) {
            currentPhase = i;
            TraceLog(LOG_INFO, "Boss '%s' entering phase %d: %s",
                     type.c_str(), i, phases[i].name.c_str());
            break;
        }
    }

    // Vile-specific: phase 2 = mech mode (sprite swap + bigger hitbox).
    if (type == "vile" && currentPhase == 1 && !useTextureAlt) {
        useTextureAlt = true;
        // Mech-Vile is bigger; widen + tall-en the hitbox. The bottom
        // stays anchored where it was so the boss doesn't pop down
        // through the floor.
        hitboxSize = {32, 40};
        // Re-anchor offset so the hitbox bottom matches where it was
        // (we grew the height by 10px).
        hitboxOffset.y = std::max(0.0f, hitboxOffset.y - 10.0f);
        // Brief invulnerability + visual flash (use Stunned for 30 frames
        // as the cheapest "transformation" beat).
        bossState = BossState::Stunned;
        stateTimer_ = 30;
        velocity = {0, 0};
        TraceLog(LOG_INFO, "Vile: transitioning to mech mode");
    }
}

// ============================================================================
// Update
// ============================================================================

void Boss::update(float /*dt*/) {
    if (!active) return;
    if (hitFlash > 0) hitFlash--;

    // U35 B5: the measured CP FSM owns the whole fight loop (Dormant/Intro/
    // Dying/Dead shells stay generic — intro choreography is B8's pin).
    if (cpMeasured_ &&
        (bossState == BossState::Idle || bossState == BossState::Startup ||
         bossState == BossState::Active || bossState == BossState::Recovery ||
         bossState == BossState::Stunned)) {
        if (cpState_ == CpState::None) cpEnter(CpState::IdleGate);
        updateCpFsm();
        fightTimer++;
        anim_.tick();
        return;
    }

    // SE-B5 E6: the measured Storm Eagle loop owns the same fight states.
    // Deliberately a separate gate rather than a shared "measured boss" path:
    // SE-B3F-C proved fight behaviour does NOT generalise between bosses, and
    // SE has no selector at all where CP is player-conditional.
    if (seMeasured_ &&
        (bossState == BossState::Idle || bossState == BossState::Startup ||
         bossState == BossState::Active || bossState == BossState::Recovery ||
         bossState == BossState::Stunned)) {
        updateSeFightLoop();
        fightTimer++;
        anim_.tick();
        return;
    }

    switch (bossState) {
        case BossState::Dormant:  updateDormant(); break;
        case BossState::Intro:    updateIntro(); break;
        case BossState::Idle:     updateIdle(); break;
        case BossState::Startup:  updateStartup(); break;
        case BossState::Active:   updateActive(); break;
        case BossState::Recovery: updateRecovery(); break;
        case BossState::Stunned:  updateStunned(); break;
        case BossState::Dying:    updateDying(); break;
        case BossState::Dead:     break; // No updates when dead
        case BossState::Rescued:  updateRescued(); break;
    }

    if (bossState != BossState::Dormant) {
        fightTimer++;
    }

    // Vile rescue timer
    if (isScripted_ && fightTimer >= rescueTime &&
        bossState != BossState::Rescued && bossState != BossState::Dormant) {
        bossState = BossState::Rescued;
        rescueTimer_ = 0;
        velocity = {0, 0};
        anim_.play("stunned");
    }

    anim_.tick();
}

void Boss::updateDormant() {
    // Waiting for activation — no behavior
    velocity = {0, 0};
}

void Boss::updateIntro() {
    velocity = {0, 0};
    if (target_) facingRight = (dirToTarget() > 0);  // face the player during the intro (was stuck on the spawn default = the wall)
    introTimer_++;

    if (cpStageIntroConfigured_) {
        const int sourceTick =
            boss_cp_intro_timeline::kSourceSpawnTick + introTimer_;
        const auto frame = boss_cp_intro_timeline::evaluate(
            sourceTick, cpIntroCeilingY_, cpFloorY_, maxHealth);
        position.y = frame.y;
        cpDisplayHealth_ = frame.displayHealth;
        cpHudVisible_ = frame.hudVisible;
        if (frame.emitFillApu) AudioManager::playApu(0x0C);
        if (frame.fightOn) cpEnter(CpState::IdleGate);
        return;
    }

    if (seMeasured_) {
        // C2: the measured Storm Eagle intro (boss_se_intro_timeline.h):
        // hidden pre-intro, the 150-tick entrance flight to the hover, the
        // 31-tick $0C gauge fill, fight on at tick 360 at (6192,196).
        const auto frame = boss_se_intro_timeline::frameAt(introSeTick());
        position.x = frame.x;
        position.y = frame.y;
        seIntroDisplayHealth_ = frame.displayHealth;
        seIntroVisible_ = frame.visible;
        if (frame.emitFillApu) AudioManager::playApu(boss_se_intro_timeline::kFillApuCommand);
        if (frame.fightOn) {
            bossState = BossState::Idle;
            stateTimer_ = 0;
        }
        return;
    }

    // Intro lasts ~120 frames (2 seconds) — health bar fills during this time.
    // Vile gets an extra 60-frame (1.0s) laugh tail per the UnityMegamanX
    // EnemyIntroStageVile profile, so the bar fills then he laughs before
    // becoming attackable.
    int introDuration = (type == "vile") ? 180 : 120;
    if (introTimer_ > introDuration) {
        bossState = BossState::Idle;
        stateTimer_ = 30; // Brief pause before first attack
    }
}

void Boss::updateIdle() {
    velocity.x = 0;

    // Face the player
    if (target_) {
        facingRight = (dirToTarget() > 0);
    }

    stateTimer_--;
    if (stateTimer_ <= 0) {
        // Pick an attack from the current phase
        if (currentPhase >= 0 && currentPhase < static_cast<int>(phases.size())) {
            if (const BossAttack* attack = phases[currentPhase].selectAttack()) {
                startAttack(*attack);
                return;
            }
        }
        stateTimer_ = 60; // Invalid or unavailable phase data: remain idle.
    }
}

void Boss::updateStartup() {
    velocity.x = 0;
    if (target_) facingRight = (dirToTarget() > 0);  // face the player during attack wind-up (so the slide/jump aims at X)
    attackTimer_--;

    if (attackTimer_ <= 0) {
        bossState = BossState::Active;
        attackTimer_ = hasCurrentAttack_ ? currentAttack_.active : 20;
        attackFired_ = false;

        if (hasCurrentAttack_) {
            switch (currentAttack_.type) {
                case BossAttackType::Charge:
                    anim_.play("charge");
                    break;
                case BossAttackType::ProjectileBurst:
                    anim_.play("shoot");
                    break;
                case BossAttackType::JumpAttack:
                    anim_.play("jump");
                    velocity.y = currentAttack_.jumpVelocity;
                    velocity.x = dirToTarget() * currentAttack_.jumpHSpeed;
                    break;
                case BossAttackType::Slide:
                    anim_.play("slide");
                    break;
                case BossAttackType::Special:
                    if (currentAttack_.name == "punch") anim_.play("charge");
                    else if (currentAttack_.name == "energy_ball") anim_.play("shoot");
                    else anim_.play("idle");
                    break;
                default:
                    anim_.play("charge");
                    break;
            }
        }
    }
}

void Boss::updateActive() {
    if (!hasCurrentAttack_) {
        bossState = BossState::Idle;
        stateTimer_ = 30;
        return;
    }

    attackTimer_--;

    switch (currentAttack_.type) {
        case BossAttackType::Charge:     executeCharge(); break;
        case BossAttackType::ProjectileBurst: executeProjectileBurst(); break;
        case BossAttackType::JumpAttack: executeJumpAttack(); break;
        case BossAttackType::Slide:      executeSlide(); break;
        case BossAttackType::Special: {
            // Vile-specific Special attacks dispatched by name.
            if (type == "vile" && currentAttack_.name == "punch") {
                // Step forward into the punch during the active window.
                // Damage applies via the existing boss-player contact check
                // (Active state already deals contactDamage on overlap).
                velocity.x = facingRight ? currentAttack_.moveSpeed
                                         : -currentAttack_.moveSpeed;
            } else if (type == "vile" && currentAttack_.name == "energy_ball") {
                // Fire one arc shot at the start of the active window.
                if (!attackFired_) {
                    float baseDir = facingRight ? 1.0f : -1.0f;
                    float spawnX = facingRight
                        ? position.x + hitboxOffset.x + hitboxSize.x + 4.0f
                        : position.x + hitboxOffset.x - 4.0f;
                    float spawnY = position.y + hitboxOffset.y + 6.0f;
                    pendingShots.push_back({
                        spawnX, spawnY,
                        baseDir * currentAttack_.projectileSpeed,
                        currentAttack_.projectileVY,
                        currentAttack_.projectileDamage
                    });
                    attackFired_ = true;
                }
                velocity.x = 0;
            }
            break;
        }
        default: break;
    }

    // Attack finished
    if (attackTimer_ <= 0) {
        velocity.x = 0;
        bossState = BossState::Recovery;
        attackTimer_ = currentAttack_.recovery;
        anim_.play("idle");
    }

    // Hit wall during charge/slide — bounce or end
    if ((currentAttack_.type == BossAttackType::Charge ||
         currentAttack_.type == BossAttackType::Slide) &&
        ((facingRight && touchingWallRight) || (!facingRight && touchingWallLeft))) {
        if (currentAttack_.wallBounce && currentAttack_.type == BossAttackType::Slide) {
            // Bounce: flip direction, reapply speed
            facingRight = !facingRight;
            float spd = currentAttack_.moveSpeed;
            if (!phases.empty() && currentPhase < static_cast<int>(phases.size()))
                spd *= phases[currentPhase].speedMultiplier;
            velocity.x = facingRight ? spd : -spd;
        } else {
            velocity.x = 0;
            bossState = BossState::Recovery;
            attackTimer_ = currentAttack_.recovery;
            anim_.play("idle");
        }
    }
}

void Boss::updateRecovery() {
    velocity.x = 0;
    if (target_) facingRight = (dirToTarget() > 0);  // face the player while recovering (turn back from a wall-bounced slide)
    attackTimer_--;

    if (attackTimer_ <= 0) {
        bossState = BossState::Idle;
        stateTimer_ = 20 + (rand() % 20); // 20-40 frame pause
        hasCurrentAttack_ = false;
        anim_.play("idle");
    }
}

void Boss::updateStunned() {
    velocity = {0, 0};
    stateTimer_--;

    if (stateTimer_ <= 0) {
        bossState = BossState::Idle;
        stateTimer_ = 15;
        anim_.play("idle");
    }
}

void Boss::updateDying() {
    velocity = {0, 0};
    deathTimer_++;

    if (deathTimer_ == 1 && !cpMeasured_) {
        AudioManager::stopBGM();
        AudioManager::playSFX(SFX::BossDeath);
        deathWhiteFlash_ = true;
    }

    if (deathTimer_ > 4) deathWhiteFlash_ = false;

    float cx = position.x + hitboxOffset.x + hitboxSize.x * 0.5f;
    float cy = position.y + hitboxOffset.y + hitboxSize.y * 0.5f;
    float hw = hitboxSize.x * 0.5f;
    float hh = hitboxSize.y * 0.5f;
    int hbW = std::max(1, static_cast<int>(hitboxSize.x));
    int hbH = std::max(1, static_cast<int>(hitboxSize.y));

    auto spawnBurst = [&](float ox, float oy, float maxR, int life) {
        DeathBurst b;
        b.x = cx + ox;
        b.y = cy + oy;
        b.radius = 2.0f;
        b.maxRadius = maxR;
        b.timer = 0;
        b.lifetime = life;
        deathBursts_.push_back(b);
    };

    if (deathTimeline_) {
        // Measured pop schedule at the exact observed death-local offsets
        // (CP: 78 over d+64..d+397; SE: 79 over d+63..d+406), same 4f/5f/6f
        // cadence grammar; variant uniform-rolled per pop through the cpRoll
        // seam. Burst VISUALS ride the measured schedule; the puff art itself
        // is still engine-invented (B8's art slice owns it).
        while (deathPopIndex_ < deathTimeline_->popCount &&
               deathTimer_ == deathTimeline_->popOffsets[deathPopIndex_]) {
            AudioManager::playApu(
                deathTimeline_->popApuCommandBase + cpRoll(4));
            float ox = static_cast<float>((deathTimer_ * 37 + 13) % hbW) - hw;
            float oy = static_cast<float>((deathTimer_ * 53 + 7) % hbH) - hh;
            spawnBurst(ox, oy, 14.0f, 16);
            deathPopIndex_++;
        }
        for (auto& b : deathBursts_) {
            b.timer++;
            float t = static_cast<float>(b.timer) / b.lifetime;
            b.radius = 2.0f + (b.maxRadius - 2.0f) * t;
        }
        deathBursts_.erase(
            std::remove_if(deathBursts_.begin(), deathBursts_.end(),
                           [](const DeathBurst& b) { return b.timer >= b.lifetime; }),
            deathBursts_.end()
        );
        // The defeated slot lingers through its measured tail (SE's value is
        // a floor, not a law - see boss_se_death_timeline.h).
        if (deathTimer_ > deathTimeline_->slotHoldTicks) {
            bossState = BossState::Dead;
            active = false;
            deathBursts_.clear();
        }
        return;
    }

    // Phase 1 (frames 5-40): rapid small bursts around the body
    if (deathTimer_ >= 5 && deathTimer_ <= 40 && deathTimer_ % 3 == 0) {
        float ox = static_cast<float>((deathTimer_ * 37 + 13) % hbW) - hw;
        float oy = static_cast<float>((deathTimer_ * 53 + 7) % hbH) - hh;
        spawnBurst(ox, oy, 8.0f, 12);
    }

    // Phase 2 (frames 45-80): medium bursts expanding outward
    if (deathTimer_ >= 45 && deathTimer_ <= 80 && deathTimer_ % 5 == 0) {
        float angle = (deathTimer_ * 47.0f) * 3.14159265f / 180.0f;
        float dist = hw * 0.8f;
        spawnBurst(std::cos(angle) * dist, std::sin(angle) * dist, 14.0f, 16);
    }

    // Phase 3 (frames 85-110): large bursts covering full body
    if (deathTimer_ >= 85 && deathTimer_ <= 110 && deathTimer_ % 7 == 0) {
        float ox = static_cast<float>((deathTimer_ * 23 + 41) % hbW) - hw;
        float oy = static_cast<float>((deathTimer_ * 31 + 19) % hbH) - hh;
        spawnBurst(ox, oy, 20.0f, 20);
    }

    // Final flash at frame 115
    if (deathTimer_ == 115) deathWhiteFlash_ = true;
    if (deathTimer_ == 118) deathWhiteFlash_ = false;

    // Tick existing bursts
    for (auto& b : deathBursts_) {
        b.timer++;
        float t = static_cast<float>(b.timer) / b.lifetime;
        b.radius = 2.0f + (b.maxRadius - 2.0f) * t;
    }
    deathBursts_.erase(
        std::remove_if(deathBursts_.begin(), deathBursts_.end(),
                       [](const DeathBurst& b) { return b.timer >= b.lifetime; }),
        deathBursts_.end()
    );

    if (deathTimer_ > 120) {
        bossState = BossState::Dead;
        active = false;
        deathBursts_.clear();
    }
}

void Boss::updateRescued() {
    velocity = {0, 0};
    rescueTimer_++;
    if (rescueTimer_ > 120) {
        active = false;
    }
}

// ============================================================================
// Attack execution
// ============================================================================

void Boss::startAttack(const BossAttack& attack) {
    currentAttack_ = attack; // Copy by value — safe across phase transitions
    hasCurrentAttack_ = true;
    bossState = BossState::Startup;
    attackTimer_ = attack.startup;
    attackFired_ = false;

    // Face target at attack start
    if (attack.faceTarget && target_) {
        facingRight = (dirToTarget() > 0);
    }

    anim_.play("idle"); // Startup animation
}

void Boss::executeCharge() {
    float speed = currentAttack_.moveSpeed;
    if (!phases.empty() && currentPhase < static_cast<int>(phases.size())) {
        speed *= phases[currentPhase].speedMultiplier;
    }
    velocity.x = facingRight ? speed : -speed;
    velocity.y = 0; // No gravity during charge
}

void Boss::executeProjectileBurst() {
    // Fire projectiles partway through the active phase
    int fireFrame = currentAttack_.active / 2;
    if (!attackFired_ && attackTimer_ <= fireFrame) {
        attackFired_ = true;

        float baseDir = facingRight ? 1.0f : -1.0f;
        int count = currentAttack_.projectileCount;
        float spread = currentAttack_.spreadAngle;
        float speed = currentAttack_.projectileSpeed;

        // Spawn position: front of boss at arm height
        float spawnX = facingRight
            ? position.x + hitboxSize.x + hitboxOffset.x + 4
            : position.x - 4;
        float spawnY = position.y + hitboxSize.y / 2 + hitboxOffset.y;

        if (count == 1) {
            pendingShots.push_back({spawnX, spawnY,
                                    speed * baseDir, currentAttack_.projectileVY,
                                    currentAttack_.projectileDamage});
        } else {
            // Spread pattern
            float halfSpread = spread / 2.0f;
            float step = (count > 1) ? spread / (count - 1) : 0;
            for (int i = 0; i < count; i++) {
                float angleDeg = -halfSpread + step * i;
                float angleRad = angleDeg * 3.14159f / 180.0f;
                float vx = speed * baseDir * std::cos(angleRad);
                float vy = speed * std::sin(angleRad) + currentAttack_.projectileVY;
                pendingShots.push_back({spawnX, spawnY, vx, vy,
                                        currentAttack_.projectileDamage});
            }
        }
    }

    velocity.x = 0;
}

void Boss::executeJumpAttack() {
    // Gravity handles the arc; we just set initial velocity in startAttack.
    // When landing (onGround), the attack ends.
    if (onGround && attackTimer_ < currentAttack_.active - 5) {
        // Landed — end attack early
        attackTimer_ = 0;
        velocity.x = 0;
    }
}

void Boss::executeSlide() {
    float speed = currentAttack_.moveSpeed;
    if (!phases.empty() && currentPhase < static_cast<int>(phases.size())) {
        speed *= phases[currentPhase].speedMultiplier;
    }
    velocity.x = facingRight ? speed : -speed;
}

// ============================================================================
// Render
// ============================================================================

void Boss::render(float alpha) {
    render(alpha, {0.0f, 0.0f});
}

void Boss::render(float alpha, Vector2 cameraOffset) {
    if (!active || !introVisible()) return;

    float drawX = prevPosition.x + (position.x - prevPosition.x) * alpha - cameraOffset.x;
    float drawY = prevPosition.y + (position.y - prevPosition.y) * alpha - cameraOffset.y;
    const float currentToInterpolatedX = position.x - (drawX + cameraOffset.x);
    const float currentToInterpolatedY = position.y - (drawY + cameraOffset.y);

    // Dying: staged explosion bursts
    if (bossState == BossState::Dying) {
        // White flash: entire boss area
        if (deathWhiteFlash_) {
            DrawRectangle(
                static_cast<int>(drawX + hitboxOffset.x - 4),
                static_cast<int>(drawY + hitboxOffset.y - 4),
                static_cast<int>(hitboxSize.x + 8),
                static_cast<int>(hitboxSize.y + 8),
                WHITE);
        }

        // Expanding explosion circles
        for (const auto& b : deathBursts_) {
            float bx = b.x - cameraOffset.x - currentToInterpolatedX;
            float by = b.y - cameraOffset.y - currentToInterpolatedY;
            float t = static_cast<float>(b.timer) / b.lifetime;

            unsigned char r = 255;
            unsigned char g = static_cast<unsigned char>(255 - t * 155);
            unsigned char bl = static_cast<unsigned char>(std::max(0.0f, 200 - t * 250));
            unsigned char a = static_cast<unsigned char>(255 * (1.0f - t * 0.6f));
            DrawCircle(static_cast<int>(bx), static_cast<int>(by),
                       b.radius, {r, g, bl, a});

            if (b.radius > 4.0f) {
                DrawCircle(static_cast<int>(bx), static_cast<int>(by),
                           b.radius * 0.4f, {255, 255, 220, a});
            }
        }

        // Flash boss sprite on/off between bursts
        if ((deathTimer_ / 2) % 2 == 0 && !deathWhiteFlash_) return;
    }

    // Rescued: flash
    if (bossState == BossState::Rescued) {
        if ((rescueTimer_ / 3) % 2 == 0) {
            DrawRectangle(static_cast<int>(drawX), static_cast<int>(drawY),
                          32, 32, {200, 200, 255, 255});
        } else {
            DrawRectangle(static_cast<int>(drawX), static_cast<int>(drawY),
                          32, 32, {100, 40, 140, 255});
        }
        return;
    }

    // U35 B7: measured-FSM render — oracle sheet cells carry the boss slot
    // anchor at in-cell (44,0) (poses face LEFT natively), so the cell
    // top-left lands at (slot_x - 44, slot_y); mirrored when facing right.
    // Frame choice follows the state map + the measured 2f art toggle.
    if (cpMeasured_ && texture && texture->valid() && frameWidth == 104) {
        if (cpStageIntroConfigured_) {
            drawY += static_cast<float>(
                boss_cp_intro_timeline::kBossRenderYBiasPx);
        }
        const int fr = cpRenderFrame();
        if (fr < 0) return;
        float left = drawX - (facingRight ? 60.0f : 44.0f);
        Rectangle src = { static_cast<float>(fr * 104), 0.0f,
                          (facingRight ? -104.0f : 104.0f), 72.0f };
        Rectangle dst = { left, drawY, 104.0f, 72.0f };
        Color tint = WHITE;
        if (cpState_ == CpState::Flinch && (cpTimer_ / 2) % 2 == 0) {
            tint = { 255, 255, 255, 200 };  // approximates the pal-blink ($open: real blink = palette swap, >60f tail)
        }
        DrawTexturePro(texture->get(), src, dst, {0, 0}, 0.0f, tint);
        return;
    }

    // Textured render path. When a sprite is loaded, draw the current
    // animation frame bottom-center anchored on the hitbox bottom-center.
    // Mirrors Enemy::render so bigger sprites overhang the hitbox the
    // same way they do for enemies.
    auto drawTexturedFrame = [&](const TextureResource* tex, int fw, int fh, int fc) {
        if (!tex || !tex->valid() || fw == 0 || fh == 0 || fc == 0) return false;
        int frame = anim_.currentFrameIndex();
        if (frame < 0) frame = 0;
        if (frame >= fc) frame = fc - 1;

        // Hitbox center / bottom in world space (interpolated for smooth render)
        float hbCenterX = drawX + hitboxOffset.x + hitboxSize.x * 0.5f;
        float hbBottom  = drawY + hitboxOffset.y + hitboxSize.y;
        float dx = hbCenterX - fw * 0.5f;
        float dy = hbBottom  - fh;

        Rectangle src = { static_cast<float>(frame * fw), 0.0f,
                          (facingRight ? 1.0f : -1.0f) * static_cast<float>(fw),
                          static_cast<float>(fh) };
        Rectangle dst = { dx, dy, static_cast<float>(fw), static_cast<float>(fh) };

        // Stunned: flash white via tint
        Color tint = WHITE;
        if (bossState == BossState::Stunned && (stateTimer_ / 2) % 2 == 0) {
            tint = { 255, 255, 255, 200 };
        }
        if (hitFlash > 0 && (hitFlash / 2) % 2 == 0) {
            if (type == "flame-mammoth" && tex == texture &&
                hitPaletteTexture && hitPaletteTexture->valid()) {
                // R279/R280: original OBJ palette 4 -> 0; preserve the pose,
                // anchor and existing blink clock. Source timing is separate.
                tex = hitPaletteTexture;
                tint = WHITE;
            } else {
                tint = {255, 255, 255, 160};
            }
        }
        DrawTexturePro(tex->get(), src, dst, {0, 0}, 0.0f, tint);
        return true;
    };

    if (useTextureAlt) {
        if (drawTexturedFrame(textureAlt, frameWidthAlt, frameHeightAlt, frameCountAlt)) return;
    } else {
        if (drawTexturedFrame(texture, frameWidth, frameHeight, frameCount)) return;
    }
    // No texture loaded — fall through to existing rectangle render below.

    // Color based on boss type and state
    Color color;
    if (type == "vile") {
        switch (bossState) {
            case BossState::Active:
                color = {160, 40, 200, 255}; // Bright purple when attacking
                break;
            case BossState::Stunned:
                color = {200, 200, 200, 255}; // White flash
                break;
            default:
                color = {100, 40, 140, 255}; // Dark purple
                break;
        }
    } else if (type == "chill-penguin") {
        switch (bossState) {
            case BossState::Active:
                color = {100, 200, 240, 255}; // Bright ice blue when attacking
                break;
            case BossState::Stunned:
                color = {255, 255, 255, 255}; // White flash
                break;
            case BossState::Intro:
                color = {60, 140, 200, 255};
                break;
            default:
                color = {80, 160, 220, 255}; // Ice blue
                break;
        }
    } else if (type == "storm-eagle") {
        switch (bossState) {
            case BossState::Active:
                color = {140, 220, 140, 255}; // Bright green when attacking
                break;
            case BossState::Stunned:
                color = {255, 255, 255, 255};
                break;
            case BossState::Intro:
                color = {80, 160, 80, 255};
                break;
            default:
                color = {100, 180, 100, 255}; // Green
                break;
        }
    } else if (type == "flame-mammoth") {
        switch (bossState) {
            case BossState::Active:  color = {255, 140, 40, 255}; break;
            case BossState::Stunned: color = {255, 255, 255, 255}; break;
            case BossState::Intro:   color = {180, 70, 20, 255}; break;
            default:                 color = {200, 90, 30, 255}; break;
        }
    } else if (type == "spark-mandrill") {
        switch (bossState) {
            case BossState::Active:  color = {255, 255, 100, 255}; break;
            case BossState::Stunned: color = {255, 255, 255, 255}; break;
            case BossState::Intro:   color = {180, 180, 40, 255}; break;
            default:                 color = {220, 200, 60, 255}; break;
        }
    } else if (type == "armored-armadillo") {
        switch (bossState) {
            case BossState::Active:  color = {120, 160, 255, 255}; break;
            case BossState::Stunned: color = {255, 255, 255, 255}; break;
            case BossState::Intro:   color = {60, 80, 140, 255}; break;
            default:                 color = {80, 120, 200, 255}; break;
        }
    } else if (type == "launch-octopus") {
        switch (bossState) {
            case BossState::Active:  color = {255, 120, 60, 255}; break;
            case BossState::Stunned: color = {255, 255, 255, 255}; break;
            case BossState::Intro:   color = {160, 50, 30, 255}; break;
            default:                 color = {200, 80, 50, 255}; break;
        }
    } else if (type == "boomer-kuwanger") {
        switch (bossState) {
            case BossState::Active:  color = {80, 200, 80, 255}; break;
            case BossState::Stunned: color = {255, 255, 255, 255}; break;
            case BossState::Intro:   color = {40, 120, 40, 255}; break;
            default:                 color = {60, 160, 60, 255}; break;
        }
    } else if (type == "sting-chameleon") {
        switch (bossState) {
            case BossState::Active:  color = {140, 240, 140, 255}; break;
            case BossState::Stunned: color = {255, 255, 255, 255}; break;
            case BossState::Intro:   color = {80, 160, 80, 255}; break;
            default:                 color = {100, 200, 100, 255}; break;
        }
    } else {
        color = {180, 60, 60, 255}; // Default red
    }
    if (hitFlash > 0 && (hitFlash / 2) % 2 == 0) {
        color = WHITE;
    }

    // Boss sprite (32x32)
    DrawRectangle(static_cast<int>(drawX), static_cast<int>(drawY), 32, 32, color);

    // Type-specific details
    if (type == "vile") {
        // "V" marking
        int cx = static_cast<int>(drawX + 16);
        int cy = static_cast<int>(drawY + 8);
        DrawLine(cx - 4, cy, cx, cy + 6, WHITE);
        DrawLine(cx, cy + 6, cx + 4, cy, WHITE);
        // Red eye
        int ex = static_cast<int>(drawX + (facingRight ? 22 : 6));
        int ey = static_cast<int>(drawY + 10);
        DrawRectangle(ex, ey, 4, 4, {255, 80, 80, 255});
    } else if (type == "chill-penguin") {
        // Penguin details: white belly, eyes, beak
        DrawRectangle(static_cast<int>(drawX + 8), static_cast<int>(drawY + 14),
                      16, 14, {220, 230, 240, 255}); // White belly
        // Eyes
        int eyeX = static_cast<int>(drawX + (facingRight ? 18 : 8));
        DrawRectangle(eyeX, static_cast<int>(drawY + 8), 4, 4, WHITE);
        DrawRectangle(eyeX + 1, static_cast<int>(drawY + 9), 2, 2, BLACK);
        // Beak
        int beakX = static_cast<int>(drawX + (facingRight ? 24 : 4));
        DrawRectangle(beakX, static_cast<int>(drawY + 12), 4, 3, {255, 180, 40, 255});
    } else if (type == "storm-eagle") {
        // Wings (triangles on sides)
        float wingX = facingRight ? drawX + 24 : drawX - 8;
        DrawTriangle(
            {wingX, drawY + 8}, {wingX, drawY + 20},
            {wingX + (facingRight ? 10.0f : -10.0f), drawY + 14},
            {60, 140, 60, 255}
        );
        // Eye
        int seEyeX = static_cast<int>(drawX + (facingRight ? 20 : 8));
        DrawRectangle(seEyeX, static_cast<int>(drawY + 8), 3, 3, WHITE);
        DrawRectangle(seEyeX, static_cast<int>(drawY + 9), 2, 2, {255, 200, 0, 255});
        // Beak
        int seBeakX = static_cast<int>(drawX + (facingRight ? 26 : 2));
        DrawRectangle(seBeakX, static_cast<int>(drawY + 11), 4, 2, {220, 180, 40, 255});
    } else if (type == "flame-mammoth") {
        // Tusks
        int tuskSide = facingRight ? 1 : -1;
        int tuskX = static_cast<int>(drawX + (facingRight ? 24 : 4));
        DrawRectangle(tuskX, static_cast<int>(drawY + 16), 4, 10, {240, 230, 200, 255});
        DrawRectangle(tuskX + tuskSide * 6, static_cast<int>(drawY + 16), 4, 10, {240, 230, 200, 255});
        // Trunk
        int trunkX = static_cast<int>(drawX + (facingRight ? 26 : 2));
        DrawRectangle(trunkX, static_cast<int>(drawY + 12), 4, 14, {160, 80, 30, 255});
        // Eyes
        int mEyeX = static_cast<int>(drawX + (facingRight ? 20 : 8));
        DrawRectangle(mEyeX, static_cast<int>(drawY + 8), 4, 3, WHITE);
        DrawRectangle(mEyeX + 1, static_cast<int>(drawY + 9), 2, 2, {200, 40, 20, 255});
        // Flame effect when attacking
        if (bossState == BossState::Active) {
            int fx = static_cast<int>(drawX + (facingRight ? 28 : 0));
            DrawRectangle(fx, static_cast<int>(drawY + 20), 4, 6, {255, 200, 50, 200});
            DrawRectangle(fx, static_cast<int>(drawY + 22), 3, 4, {255, 100, 30, 200});
        }
    } else if (type == "spark-mandrill") {
        // Muscular arms (wider body)
        DrawRectangle(static_cast<int>(drawX + 2), static_cast<int>(drawY + 10),
                      28, 16, color); // Wide torso
        // Fists
        int fistL = static_cast<int>(drawX);
        int fistR = static_cast<int>(drawX + 28);
        DrawRectangle(fistL, static_cast<int>(drawY + 18), 6, 8, {255, 240, 100, 255});
        DrawRectangle(fistR, static_cast<int>(drawY + 18), 6, 8, {255, 240, 100, 255});
        // Eyes (angry)
        int smEyeX = static_cast<int>(drawX + (facingRight ? 18 : 8));
        DrawRectangle(smEyeX, static_cast<int>(drawY + 6), 5, 4, WHITE);
        DrawRectangle(smEyeX + 1, static_cast<int>(drawY + 7), 3, 3, {255, 60, 60, 255});
        // Electric sparks when attacking
        if (bossState == BossState::Active && (fightTimer / 3) % 2 == 0) {
            DrawRectangle(static_cast<int>(drawX + 4), static_cast<int>(drawY + 2), 2, 6, {255, 255, 100, 200});
            DrawRectangle(static_cast<int>(drawX + 24), static_cast<int>(drawY + 4), 2, 6, {255, 255, 100, 200});
        }
    } else if (type == "armored-armadillo") {
        // Armor plates (layered rectangles for shell look)
        DrawRectangle(static_cast<int>(drawX + 4), static_cast<int>(drawY + 4),
                      24, 20, {100, 140, 220, 255}); // Shell
        DrawRectangle(static_cast<int>(drawX + 6), static_cast<int>(drawY + 6),
                      20, 8, {140, 170, 230, 255}); // Lighter shell band
        // Face
        int aaFaceX = static_cast<int>(drawX + (facingRight ? 20 : 4));
        DrawRectangle(aaFaceX, static_cast<int>(drawY + 10), 8, 10, {180, 180, 200, 255});
        // Eye
        DrawRectangle(aaFaceX + 2, static_cast<int>(drawY + 12), 3, 3, WHITE);
        DrawRectangle(aaFaceX + 3, static_cast<int>(drawY + 13), 2, 2, {60, 60, 200, 255});
        // Tail
        int tailX = static_cast<int>(drawX + (facingRight ? 0 : 26));
        DrawRectangle(tailX, static_cast<int>(drawY + 20), 6, 4, {60, 80, 160, 255});
    } else if (type == "launch-octopus") {
        // Tentacles (4 pairs, wavy)
        for (int i = 0; i < 4; i++) {
            int tx = static_cast<int>(drawX + 4 + i * 7);
            int ty = static_cast<int>(drawY + 24);
            int wave = ((fightTimer + i * 8) / 4) % 3 - 1;
            DrawRectangle(tx, ty, 3, 8, {180, 60, 40, 255});
            DrawRectangle(tx + wave, ty + 6, 3, 4, {160, 50, 30, 255});
        }
        // Head dome
        DrawRectangle(static_cast<int>(drawX + 6), static_cast<int>(drawY + 2),
                      20, 14, {220, 100, 60, 255});
        // Eyes
        int loEyeX1 = static_cast<int>(drawX + (facingRight ? 16 : 8));
        int loEyeX2 = loEyeX1 + (facingRight ? 6 : -6);
        DrawRectangle(loEyeX1, static_cast<int>(drawY + 6), 4, 4, WHITE);
        DrawRectangle(loEyeX2, static_cast<int>(drawY + 6), 4, 4, WHITE);
        DrawRectangle(loEyeX1 + 1, static_cast<int>(drawY + 7), 2, 2, BLACK);
        DrawRectangle(loEyeX2 + 1, static_cast<int>(drawY + 7), 2, 2, BLACK);
    } else if (type == "boomer-kuwanger") {
        // Tall thin body with horn
        DrawRectangle(static_cast<int>(drawX + 10), static_cast<int>(drawY),
                      12, 32, color); // Slim body
        // Horn (triangle on top)
        float hornX = drawX + 16;
        DrawTriangle(
            {hornX - 3, drawY + 2}, {hornX + 3, drawY + 2},
            {hornX, drawY - 8},
            {200, 220, 200, 255}
        );
        // Eyes
        int bkEyeX = static_cast<int>(drawX + (facingRight ? 18 : 8));
        DrawRectangle(bkEyeX, static_cast<int>(drawY + 8), 4, 4, WHITE);
        DrawRectangle(bkEyeX + 1, static_cast<int>(drawY + 9), 2, 2, {60, 200, 60, 255});
        // Mandibles
        int mandX = static_cast<int>(drawX + (facingRight ? 22 : 4));
        DrawRectangle(mandX, static_cast<int>(drawY + 14), 6, 2, {80, 180, 80, 255});
        DrawRectangle(mandX, static_cast<int>(drawY + 18), 6, 2, {80, 180, 80, 255});
    } else if (type == "sting-chameleon") {
        // Long tail
        int tailDir = facingRight ? -1 : 1;
        int tailStartX = static_cast<int>(drawX + (facingRight ? 2 : 28));
        DrawRectangle(tailStartX, static_cast<int>(drawY + 20), 6, 4, {80, 180, 80, 255});
        DrawRectangle(tailStartX + tailDir * 6, static_cast<int>(drawY + 22), 4, 3, {60, 160, 60, 255});
        DrawRectangle(tailStartX + tailDir * 10, static_cast<int>(drawY + 23), 3, 2, {50, 140, 50, 255});
        // Eyes (swiveling)
        int scEyeX = static_cast<int>(drawX + (facingRight ? 20 : 6));
        DrawRectangle(scEyeX, static_cast<int>(drawY + 6), 6, 6, {160, 220, 160, 255});
        int pupilOffset = (fightTimer / 10) % 3;
        DrawRectangle(scEyeX + pupilOffset, static_cast<int>(drawY + 8), 3, 3, BLACK);
        // Tongue when attacking
        if (bossState == BossState::Active) {
            int tongueX = static_cast<int>(drawX + (facingRight ? 28 : 0));
            int tongueDir = facingRight ? 1 : -1;
            DrawRectangle(tongueX, static_cast<int>(drawY + 14), 16 * tongueDir, 2, {255, 80, 120, 255});
        }
    }

    // Hitbox debug
    AABB hb = getHitbox();
    float hbX = hb.x - cameraOffset.x - currentToInterpolatedX;
    float hbY = hb.y - cameraOffset.y - currentToInterpolatedY;
    DrawRectangleLines(
        static_cast<int>(hbX), static_cast<int>(hbY),
        static_cast<int>(hb.w), static_cast<int>(hb.h),
        {255, 0, 0, 100}
    );

    // (No in-arena boss-name banner. The real game shows the Maverick name only in
    // the separate boss-intro CUTSCENE (boss_intro_scene), never during the gameplay
    // fight — David review pt2 caught the "CHILL PENGUIN" banner leaking into the arena.)
}

// ============================================================================
// Helpers
// ============================================================================

float Boss::dirToTarget() const {
    if (!target_) return 1.0f;
    return (target_->position.x > position.x) ? 1.0f : -1.0f;
}

float Boss::distToTarget() const {
    if (!target_) return 9999.0f;
    float dx = target_->position.x - position.x;
    float dy = target_->position.y - position.y;
    return std::sqrt(dx * dx + dy * dy);
}

} // namespace mmx
