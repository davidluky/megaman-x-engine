// enemy.cpp - runs enemy runtime behavior, rendering, and projectile emission.
// Owns: enemy timers, behavior state, serials, and spawn-derived runtime data.

#include "entities/enemy.h"

#include "entities/enemy_bat_drift_law.h"
#include "entities/enemy_visual_placement.h"
#include "entities/player_anchor.h"
#include "entities/player.h"
#include "gameplay/gameplay_enemy_death_presentation.h"
#include "systems/tilemap.h"
#include "systems/kb_fsm.h"
#include "systems/asset_cache.h"
#include "systems/audio.h"
#include "app/constants.h"
#include "data/kb_paths.h"
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace mmx {
namespace {

uint32_t stableEnemySeed(const std::string& type, float x, float y) {
    // FNV-1a over type + spawn tile-ish coordinates gives same-stage enemies
    // independent branch rolls without depending on platform std::hash salts.
    uint32_t hash = 2166136261u;
    auto mixByte = [&](uint8_t value) {
        hash ^= value;
        hash *= 16777619u;
    };
    for (unsigned char c : type) {
        mixByte(c);
    }
    const int ix = static_cast<int>(std::lround(x));
    const int iy = static_cast<int>(std::lround(y));
    for (int shift = 0; shift < 32; shift += 8) {
        mixByte(static_cast<uint8_t>((ix >> shift) & 0xff));
        mixByte(static_cast<uint8_t>((iy >> shift) & 0xff));
    }
    return hash;
}

std::unique_ptr<KBFSM> loadKBBehaviorFSM(const std::string& behaviorId,
                                         const std::string& seedType,
                                         float x,
                                         float y) {
    if (behaviorId.empty()) {
        return nullptr;
    }
    if (!kb_paths::isSafeIdSegment(behaviorId)) {
        TraceLog(LOG_ERROR, "Enemy: rejected unsafe runtime kbBehaviorId '%s'",
                 behaviorId.c_str());
        return nullptr;
    }

    auto kbBehaviorPath = kb_paths::resolveKBPath(
        "mmx1/enemies/" + behaviorId + "/behavior.json");
    if (!kbBehaviorPath) {
        return nullptr;
    }

    auto fsm = std::make_unique<KBFSM>();
    fsm->setSeed(stableEnemySeed(seedType, x, y));
    if (!fsm->loadFromJson(*kbBehaviorPath)) {
        return nullptr;
    }
    return fsm;
}

} // namespace

std::optional<EnemyBehavior> parseEnemyBehavior(const std::string& behavior) {
    if (behavior == "Patrol") return EnemyBehavior::Patrol;
    if (behavior == "HideAndShoot") return EnemyBehavior::HideAndShoot;
    if (behavior == "FlyPattern") return EnemyBehavior::FlyPattern;
    if (behavior == "Stationary") return EnemyBehavior::Stationary;
    if (behavior == "Drift") return EnemyBehavior::Drift;
    if (behavior == "Turret") return EnemyBehavior::Turret;
    if (behavior == "AxeMax") return EnemyBehavior::AxeMax;
    if (behavior == "Hover") return EnemyBehavior::Hover;
    if (behavior == "Walker") return EnemyBehavior::Walker;
    if (behavior == "Bat") return EnemyBehavior::Bat;
    if (behavior == "BatDrift") return EnemyBehavior::BatDrift;
    if (behavior == "EagletBurst") return EnemyBehavior::EagletBurst;
    if (behavior == "SnowballRoll") return EnemyBehavior::SnowballRoll;
    if (behavior == "Anchored") return EnemyBehavior::Anchored;
    if (behavior == "HoverPatrol") return EnemyBehavior::HoverPatrol;
    if (behavior == "StrideWalk") return EnemyBehavior::StrideWalk;
    return std::nullopt;
}

Enemy::Enemy() = default;
Enemy::~Enemy() = default;
Enemy::Enemy(Enemy&&) noexcept = default;
Enemy& Enemy::operator=(Enemy&&) noexcept = default;

namespace { int s_nextEnemySerial = 0; }

void Enemy::configureCpCameraReturn() {
    if (type != "axemax" || behavior != EnemyBehavior::AxeMax) return;
    if (!cpCameraReturn_) ++axeTimer_; // source claim -> init consumes one update
    cpCameraReturn_ = true;
    cameraReturnNativeOrigin_ = position;
    // camera_return.json: D080 saves the placement, then adds (+32,-14).
    cameraReturnSourceOrigin_ = {position.x - 32.0f, position.y + 14.0f};
}

bool Enemy::recycleForCameraExit(float cameraX, float cameraY) {
    // R72 source contract: only a live, already-activated canonical CP Axe
    // Max may use the source cull/re-entry seam.  Dead Axe Max remains the
    // existing render-only remnant, and every other enemy keeps legacy flow.
    if (!cpCameraReturn_ || !active || !cameraActivated || !alive || health <= 0) {
        return false;
    }

    // The source compares 16-bit integer words after flooring the retained
    // source origin and camera registers.  Keep the subtraction unsigned so
    // underflow follows the SNES predicate in camera_return.json exactly.
    const auto sourceWord = [](float value) {
        return static_cast<uint16_t>(static_cast<int64_t>(std::floor(value)));
    };
    const uint16_t sourceX = sourceWord(cameraReturnSourceOrigin_.x);
    const uint16_t sourceY = sourceWord(cameraReturnSourceOrigin_.y);
    const uint16_t cameraWordX = sourceWord(cameraX);
    const uint16_t cameraWordY = sourceWord(cameraY);
    const uint16_t xDelta = static_cast<uint16_t>(sourceX - cameraWordX + 0x0040u);
    const uint16_t yDelta = static_cast<uint16_t>(sourceY - cameraWordY + 0x0040u);
    const bool outsideSourceWindow = xDelta >= 0x0180u || yDelta >= 0x0160u;
    if (!outsideSourceWindow) {
        return false;
    }

    Player* target = target_;
    const Tilemap* tilemap = tilemap_;
    Enemy replacement;
    replacement.init(type, cameraReturnNativeOrigin_.x, cameraReturnNativeOrigin_.y);
    if (!replacement.active || !replacement.alive) {
        return false;
    }
    replacement.setTarget(target);
    replacement.setTilemap(tilemap);
    replacement.cameraActivated = false;
    replacement.configureCpCameraReturn();
    *this = std::move(replacement);
    return true;
}

void Enemy::init(const std::string& enemyType, float x, float y) {
    cpCameraReturn_ = false;
    hoverPatrolHorizontalEntered_ = false;
    cameraActivated = false;
    serial = ++s_nextEnemySerial;
    stretchBirdState_ = {};
    stretchBirdLaunch_ = false;
    deck_turret_parent::ParentFields turretFields;
    turretFields.parent_x = static_cast<std::uint16_t>(static_cast<std::int64_t>(std::floor(x)));
    deckTurretController_ = deck_turret_parent::ParentController(turretFields);
    deckTurretLaunch_ = false;
    madPeckerController_ = {};
    madPeckerLaunch_ = false;
    type = canonicalDefinitionId(enemyType);
    position = {x, y};
    prevPosition = position;
    active = true;
    alive = true;
    setSpriteSheetResource(nullptr);
    flashSheetResource_ = nullptr;
    hitFlashVisual = 0;
    frameWidth_ = 32;
    frameHeight_ = 32;
    frameCount_ = 0;
    bodyVisualOffsetY_ = 0.0f;
    sourceWalker_ = false;
    bodyMirrorsWithFacing_ = true;
    bodySourceFacesRight_ = true;
    placeholderLabel_.clear();
    std::string kbBehaviorId;

    auto it = definitions.find(type);
    if (it != definitions.end()) {
        const auto& def = it->second;
        kbBehaviorId = def.kbBehaviorId;
        health = def.hp;
        maxHealth = def.hp;
        contactDamage = def.contactDamage;
        hitboxSize = {def.hitboxWidth, def.hitboxHeight};
        hitboxOffset = {def.hitboxOffsetX, def.hitboxOffsetY};
        patrolSpeed = def.patrolSpeed;
        flySpeed = def.flySpeed;
        detectionRange = def.detectionRange;
        dropType = def.dropType;
        dropChancePct = def.dropChancePct;
        dropTypes = def.dropTypes;

        const auto parsedBehavior = parseEnemyBehavior(def.behavior);
        if (!parsedBehavior) {
            std::fprintf(stderr, "Enemy: unknown behavior '%s' for '%s'\n",
                         def.behavior.c_str(), type.c_str());
            active = false;
            alive = false;
            return;
        }
        behavior = *parsedBehavior;

        // Borrow the texture loaded by the definition (no per-instance loading).
        setSpriteSheetResource(&def.texture);
        flashSheetResource_ = &def.flashTexture;
        frameWidth_ = def.frameWidth;
        frameHeight_ = def.frameHeight;
        frameCount_ = def.frameCount;
        bodyVisualOffsetY_ = def.bodyVisualOffsetY;
        bodyMirrorsWithFacing_ = def.bodyMirrorsWithFacing;
        bodySourceFacesRight_ = def.bodySourceFacesRight;
    } else {
        std::fprintf(stderr, "Enemy: missing definition for '%s'\n", type.c_str());
        active = false;
        alive = false;
        return;
    }

    // Initial state
    switch (behavior) {
        case EnemyBehavior::Patrol:
            enemyState = EnemyState::Patrol;
            velocity.x = -patrolSpeed;
            gravity = 0.22f;
            break;
        case EnemyBehavior::HideAndShoot:
            enemyState = EnemyState::Hide;
            hideTimer = 90;
            gravity = 0.22f;
            invulnerable = true;
            break;
        case EnemyBehavior::FlyPattern:
            enemyState = EnemyState::Idle;
            gravity = 0.0f;
            break;
        case EnemyBehavior::SnowballRoll:
            // CP OID 0x54 (plan task R1.0x54): it starts at rest and only the
            // measured -12 fp per grounded frame moves it, so nothing is
            // chosen here. `gravity` comes from the def, where the measured
            // 0.25 (64 fp) is written down.
            enemyState = EnemyState::Idle;
            velocity = {0.0f, 0.0f};
            facingRight = false;
            // Measured, and it is not this file's usual 0.22: the airborne
            // frames of all six instances change vy by exactly -64 fp per
            // frame, which is 0.25 px/f^2.
            gravity = 0.25f;
            break;
        case EnemyBehavior::EagletBurst:
            // SE OID 0x56 (plan task R7.eggs): the hatch's diagonal. Gravity
            // free; the spawner supplies the measured fixed-point velocity
            // through setEagletBurst, so nothing is chosen here.
            enemyState = EnemyState::Idle;
            gravity = 0.0f;
            velocity = {0.0f, 0.0f};
            break;
        case EnemyBehavior::Drift:
            // OID 0x15: a spiked WHEEL that rolls along the ground drifting left
            // at a constant flySpeed (RAM-measured 1.5 px/f). Gravity keeps it on
            // the deck (measured y was constant = flat ground), not floating.
            enemyState = EnemyState::Idle;
            gravity = 0.22f;
            facingRight = false;            // starts rolling LEFT
            velocity = {-flySpeed, 0.0f};
            break;
        case EnemyBehavior::Turret:
            // OID 0x29: a stationary turret. Gravity settles it onto the deck so
            // it can't be left floating by an imperfect authored y.
            enemyState = EnemyState::Idle;
            gravity = 0.22f;
            velocity = {0.0f, 0.0f};
            break;
        case EnemyBehavior::AxeMax:
            // CP OID 0x0B lumberjack (oracle e1 runs): stationary; first
            // log fired ~64f after spawn (e1_cadence_long: spawn f596,
            // fire f663 minus the 24f sweep lead-in).
            enemyState = EnemyState::Idle;
            gravity = 0.22f;
            velocity = {0.0f, 0.0f};
            // First log 67f after spawn (e1_cadence_long f596 -> f663);
            // engine lead-in = 1 (sweep-start tick) + 15 (sweep lead).
            axeTimer_ = 51;
            axeShotInVolley_ = 0;
            axeSweep_ = -1;
            axeStack_ = 2;         // visible magazine: fire-line log + one above
            axeAlertLatched_ = false;
            remnant_ = false;
            facingRight = false;   // CP instance faces left (toward the path)
            break;
        case EnemyBehavior::Bat:
            // CP OID 0x19 (U19, _bat_runs/FINDINGS.md): fixed-height
            // patroller — gravity-free, flies its authored facing.
            enemyState = EnemyState::Idle;
            gravity = 0.0f;
            velocity = {0.0f, 0.0f};
            batTravelled_ = 0.0f;
            batHoverTimer_ = 0;
            batHoverDone_ = false;
            batMinesDropped_ = 0;
            facingRight = false;   // the CP instance patrols leftward
            break;
        case EnemyBehavior::BatDrift: {
            // CP OID 0x2D (plan R1.0x2D): a bat. It hangs motionless at its
            // spawn for a drawn 30-35 frames — the velocity field is non-zero
            // during it, so the hang is a state, not a zero speed — then flies
            // at exactly one pixel per frame in a drawn direction held for a
            // drawn number of frames, and repeats. Gravity-free.
            enemyState = EnemyState::Idle;
            gravity = 0.0f;
            velocity = {0.0f, 0.0f};
            batDriftXf_ = static_cast<int>(position.x * enemy_bat_drift_law::kFixedPointDivisor);
            batDriftYf_ = static_cast<int>(position.y * enemy_bat_drift_law::kFixedPointDivisor);
            batDriftChoices_ = {};
            batDriftHangDraws_ = 0;
            batDriftHoldDraws_ = 0;
            batDriftDirectionDraws_ = 0;
            // Seeded per instance so two bats on screen do not fly in lockstep;
            // a test replaces this with seedBatDriftRng or a choice sequence.
            batDriftRng_ = static_cast<uint32_t>(serial) * 2654435761u + 1u;
            // The spawn frame is the first of the drawn hang frames and no
            // update runs on it, so the counter holds the remaining ones.
            batDriftHang_ = enemy_bat_drift_law::pickWeightedValue(
                enemy_bat_drift_law::kHangFrames,
                enemy_bat_drift_law::kHangFramesCount,
                enemy_bat_drift_law::kHangFramesWeight,
                static_cast<int>(nextBatDriftRoll() & 0x7FFFFFFF)) - 1;
            batDriftHold_ = 0;
            batDriftVx_ = 0;
            batDriftVy_ = 0;
            break;
        }
        case EnemyBehavior::Anchored:
            // Gravity-free and completely still, which is why neither
            // Stationary (gravity 0.22 would drop these off their perches --
            // tools/audit_spawns.py names the positions) nor Hover (a 0.3 px
            // bob the movies do not show) fits either OID that uses it.
            enemyState = EnemyState::Idle;
            gravity = 0.0f;
            velocity = {0.0f, 0.0f};
            break;
        case EnemyBehavior::HoverPatrol:
            // SE OID 0x49: the source initializes its reference word and
            // patrol velocity in separate updates before horizontal motion.
            // The source action-2 trigger and its vertical cadence are kept
            // in updateHoverPatrol; shared physics remains the sole mover.
            enemyState = EnemyState::Fly;
            gravity = 0.0f;
            hoverPatrolSourcePhase_ = HoverPatrolSourcePhase::SetupReference;
            hoverPatrolSourceTimer_ = 0;
            hoverPatrolSourceVxFp_ = 0;
            hoverPatrolSourceVyFp_ = 0;
            hoverPatrolReferenceX_ = 0;
            velocity = {0.0f, 0.0f};
            break;
        case EnemyBehavior::StrideWalk:
            // SE OID 0x27 (plan R1.0x27): the direction is the side X is on at
            // the spawn -- 4 of 4 instances, two walking left with X 130 and
            // 131 px to the left and two right with X 154 px to the right --
            // and no instance ever turns. Gravity-free: y is constant on every
            // frame after the 4 px spawn hop.
            enemyState = EnemyState::Patrol;
            gravity = 0.0f;
            velocity = {0.0f, 0.0f};
            strideCell_ = 0;
            strideTimer_ = 0;
            strideSpawning_ = true;
            facingRight = target_ ? (target_->position.x >= position.x) : false;
            break;
        case EnemyBehavior::Walker:
            // CP OID 0x51 (U17, _walker_runs/FINDINGS.md): camera-entry then
            // a 27f activation stand before the first walk bout (obs f48
            // spawn -> f75 first motion, 3/3 runs).
            enemyState = EnemyState::Idle;
            gravity = 0.22f;
            velocity = {0.0f, 0.0f};
            walkerWalking_ = false;
            walkerTimer_ = 27;
            walkerStandLen_ = 27;
            walkerStandAge_ = 0;
            walkerStandFires_ = false;
            walkerStandIndex_ = 0;
            break;
        case EnemyBehavior::Hover:
            // OID 0x0F helicopter: flies at its authored height — no gravity.
            enemyState = EnemyState::Idle;
            gravity = 0.0f;
            velocity = {0.0f, 0.0f};
            break;
        default:
            enemyState = EnemyState::Idle;
            gravity = 0.22f;
            break;
    }

    maxFallSpeed = 5.0f;
    homePos_ = position;
    setupAnimations();

    // Captured KB FSMs are intentionally metadata-driven. This prevents a new
    // oracle folder from changing shipped enemy behavior until the content def
    // opts in and its sprite/hitbox contract is reviewed.
    kbFsm_ = loadKBBehaviorFSM(kbBehaviorId, type, x, y);
    if (kbFsm_) {
        applyKBState(kbFsm_->currentStateName(), true);
    }
}

void Enemy::setupAnimations() {
    anim_.addAnimation("idle",   {"idle",   {{0, 60}}, true});
    anim_.addAnimation("walk",   {"walk",   {{0, 10}, {1, 10}}, true});
    anim_.addAnimation("hide",   {"hide",   {{2, 60}}, true});
    anim_.addAnimation("peek",   {"peek",   {{3, 60}}, true});
    anim_.addAnimation("shoot",  {"shoot",  {{4, 60}}, true});
    anim_.addAnimation("hurt",   {"hurt",   {{5, 8}}, false});
    anim_.addAnimation("dead",   {"dead",   {{6, 60}}, false});

    // Apply per-def overrides. Each entry in def.animations replaces the
    // default registered above (addAnimation overwrites by name).
    auto it = definitions.find(type);
    if (it != definitions.end()) {
        for (const auto& [name, anim] : it->second.animations) {
            anim_.addAnimation(name, anim);
        }
    }

    switch (behavior) {
        case EnemyBehavior::Patrol:       anim_.play("walk"); break;
        case EnemyBehavior::HideAndShoot: anim_.play("hide"); break;
        case EnemyBehavior::FlyPattern:   anim_.play("idle"); break;
        case EnemyBehavior::Drift:        anim_.play("idle"); break;
        case EnemyBehavior::Turret:       anim_.play("idle"); break;
        case EnemyBehavior::AxeMax:       anim_.play("idle"); break;
        case EnemyBehavior::Hover:        anim_.play("idle"); break;
        case EnemyBehavior::Walker:       anim_.play("idle"); break;
        case EnemyBehavior::Bat:          anim_.play("fly"); break;
        // The spawn choreography (anim 0x80..0x84) runs once and lasts
        // exactly the hang; the flight ping-pong starts with it.
        case EnemyBehavior::BatDrift:     anim_.play("idle"); break;
        case EnemyBehavior::Anchored:    anim_.play("idle"); break;
        case EnemyBehavior::HoverPatrol:  anim_.play("fly"); break;
        default: anim_.play("idle"); break;
    }
}

void Enemy::update(float /*dt*/) {
    if (!active) return;
    if (enemyState == EnemyState::Dead) {
        updateDead();
        anim_.tick();
        return;
    }
    if (kbFsm_) {
        updateKBFSM();
    } else {
        switch (behavior) {
            case EnemyBehavior::Patrol:       updatePatrol(); break;
            case EnemyBehavior::HideAndShoot: updateHideAndShoot(); break;
            case EnemyBehavior::FlyPattern:   updateFlyPattern(); break;
            case EnemyBehavior::Drift:        updateDrift(); break;
            case EnemyBehavior::Turret:       updateTurret(); break;
            case EnemyBehavior::AxeMax:       updateAxeMax(); break;
            case EnemyBehavior::Hover:        updateHover(); break;
            case EnemyBehavior::Walker:       updateWalker(); break;
            case EnemyBehavior::Bat:          updateBat(); break;
            case EnemyBehavior::BatDrift:     updateBatDrift(); break;
            case EnemyBehavior::Anchored:    updateAnchored(); break;
            case EnemyBehavior::HoverPatrol:  updateHoverPatrol(); break;
            case EnemyBehavior::StrideWalk:   updateStrideWalk(); break;
            case EnemyBehavior::EagletBurst:  updateEagletBurst(); break;
            case EnemyBehavior::SnowballRoll: updateSnowballRoll(); break;
            default: break;
        }
    }
    if (hitFlash > 0) hitFlash--;
    if (hitFlashVisual > 0) hitFlashVisual--;
    anim_.tick();
}

void Enemy::setPlaceholder(const std::string& oid, int hp) {
    placeholderLabel_ = oid;
    if (hp > 0) {
        health = hp;
        maxHealth = hp;
    }
    // The census position is the measurement; a placeholder never falls or
    // drifts away from it (flyers spawn in the air, ground enemies on it).
    gravity = 0.0f;
    velocity = {0.0f, 0.0f};
}

void Enemy::render(float alpha) {
    render(alpha, {0.0f, 0.0f});
}

void Enemy::render(float alpha, Vector2 cameraOffset) {
    if (!active) return;
    if (sourceWalker_ && !sourceWalkerDrawReady_) return;
    // The first source hardware-OAM body follows both setup updates.
    // See render_phase.json; creation/contact and death have separate timing.
    if (enemyState != EnemyState::Dead &&
        behavior == EnemyBehavior::HoverPatrol && type == "se_drone" &&
        !hoverPatrolHorizontalEntered_) return;
    float drawX = prevPosition.x + (position.x - prevPosition.x) * alpha - cameraOffset.x;
    float drawY = prevPosition.y + (position.y - prevPosition.y) * alpha - cameraOffset.y;
    if (sourceWalker_ && enemyState != EnemyState::Dead) {
        drawX = std::floor(sourceWalkerDrawPosition_.x) - cameraOffset.x;
        drawY = std::floor(sourceWalkerDrawPosition_.y) - cameraOffset.y;
    }

    if (enemyState == EnemyState::Dead) {
        // AxeMax remnant: the explosion is over — draw only the frozen,
        // restocked log stack (the robot is gone; oracle e5_drop sf442).
        if (remnant_ && deathTimer >= DEATH_FLASH_FRAMES) {
            renderAxeStack(drawX, drawY);
            return;
        }
        const auto bodyDecision =
            gameplay_enemy_death_presentation::bodyDrawDecisionForAge(deathTimer);
        if (bodyDecision.drawBody) {
            renderBody(drawX, drawY, bodyDecision.drawHitFlashOverlay);
        }
        if (behavior == EnemyBehavior::AxeMax) renderAxeStack(drawX, drawY);
        // Source-exact generic enemy death presentation. Age zero retains the
        // last normal body pose with no dedicated effect material; ages 1..23
        // follow the ROM cadence and age 24 is first clear.
        const float cx = drawX + hitboxOffset.x + hitboxSize.x * 0.5f;
        const float cy = drawY + hitboxOffset.y + hitboxSize.y * 0.5f;
        const auto presentation =
            gameplay_enemy_death_presentation::drawForTargetCenter(
                deathTimer, cx, cy, 0.0f, 0.0f);
        const TextureResource* ex =
            AssetCache::loadTexture("content/x1/sprites/effects/enemy_death_explosion.png");
        if (presentation && ex && ex->valid()) {
            ex->setFilter(TEXTURE_FILTER_POINT);
            Rectangle src{static_cast<float>(presentation->sourceX),
                          static_cast<float>(presentation->sourceY),
                          static_cast<float>(presentation->sourceWidth),
                          static_cast<float>(presentation->sourceHeight)};
            Rectangle dst{presentation->topLeftX, presentation->topLeftY,
                          static_cast<float>(presentation->sourceWidth),
                          static_cast<float>(presentation->sourceHeight)};
            DrawTexturePro(ex->get(), src, dst, {0, 0}, 0.0f, WHITE);
        }
        return;
    }

    renderBody(drawX, drawY, true);

    if (behavior == EnemyBehavior::AxeMax) renderAxeStack(drawX, drawY);
}

void Enemy::renderBody(float drawX, float drawY, bool allowHitFlashOverlay) {
    // Bottom-center anchor: sprite's bottom-center aligns with hitbox
    // bottom-center. This lets any frame size render without repositioning
    // the entity. For pre-existing 32x32 sheets (Met, Batton) the math
    // reduces to the old top-left placement since hitboxOffset + hitbox
    // centers within a 32x32 cell. For larger sheets the sprite visually
    // overhangs the hitbox, which matches how MMX enemies are drawn.
    float hbCenterX = drawX + hitboxOffset.x + hitboxSize.x * 0.5f;
    float hbBottom  = drawY + hitboxOffset.y + hitboxSize.y;

    const TextureResource* sheet = spriteSheetResource();
    // FW7-REVIEW-C: a live enemy with a derived flash sheet swaps its whole
    // body draw to the exact palette-0 variant for the single flash frame
    // (source f447: identical geometry/tiles, OBJ palette 7 -> 0) and never
    // uses the legacy additive pass. Dead-body presentation keeps the
    // separately accepted death-flash overlay; enemies without a flash sheet
    // keep the legacy behavior unchanged.
    const bool exactFlashOwnsLiveBody = enemyState != EnemyState::Dead &&
        flashSheetResource_ && flashSheetResource_->valid();
    if (exactFlashOwnsLiveBody && allowHitFlashOverlay && hitFlashVisual > 0) {
        sheet = flashSheetResource_;
    }
    if (sheet && sheet->valid()) {
        const Texture2D& texture = sheet->get();
        int fw = frameWidth_  > 0 ? frameWidth_  : 32;
        int fh = frameHeight_ > 0 ? frameHeight_ : 32;
        int cols = texture.width / fw;
        if (cols < 1) cols = 1;
        // Clamp frame index to the sheet's capacity so enemies with shorter
        // strips (e.g. 6-frame rider) don't sample garbage beyond the
        // texture when the hardcoded animation indices (hurt=5, dead=6)
        // exceed frameCount.
        int maxFrames = frameCount_ > 0 ? frameCount_ : cols;
        int frameIdx = anim_.currentFrameIndex();
        if (sourceWalker_ && enemyState != EnemyState::Dead)
            frameIdx = sourceWalkerDrawCell_;
        const bool deckTurret = type == "se_turret" &&
            behavior == EnemyBehavior::Anchored && enemyState != EnemyState::Dead;
        if (deckTurret) frameIdx = deckTurretState().art_17 - 0x80;
        if (type == "mad_pecker" && behavior == EnemyBehavior::Anchored &&
            enemyState != EnemyState::Dead) frameIdx = madPeckerController_.animation.art17 - 0x80;
        if (type == "stretch_bird" && behavior == EnemyBehavior::Anchored &&
            enemyState != EnemyState::Dead) {
            const auto art = stretchBirdState_.animation.current().art;
            // Existing atlas 83..86,8B..96; source idle 87..8A appended at 16.
            if (art >= 0x83 && art <= 0x86) frameIdx = art - 0x83;
            else if (art >= 0x87 && art <= 0x8A) frameIdx = 16 + art - 0x87;
            else if (art >= 0x8B && art <= 0x96) frameIdx = 4 + art - 0x8B;
        }
        if (frameIdx >= maxFrames) frameIdx = maxFrames - 1;
        if (frameIdx < 0) frameIdx = 0;
        int tx = (frameIdx % cols) * fw;
        int ty = (frameIdx / cols) * fh;

        float dx = enemyBodyLeftX(hbCenterX, fw);
        // FE5.7A: definition-local BODY placement only. Axe Max opts into a
        // source-measured +4; renderAxeStack() deliberately continues using
        // the unmodified hitbox-derived fire/ground lines below.
        float dy = enemyBodyTopY(hbBottom, fh, bodyVisualOffsetY_);
        float srcW = enemyBodySourceWidth(
            fw, facingRight, bodyMirrorsWithFacing_, bodySourceFacesRight_);
        if (sourceWalker_ && enemyState != EnemyState::Dead) {
            // walker_contact_cycles_2026-09-17.json: 90 exact OAM matches.
            constexpr float ox[4] = {-20, -19, -19, -20};
            constexpr float oy[4] = {-39, -39, -35, -25};
            const int cell = sourceWalkerDrawCell_;
            dx = drawX + (sourceWalkerDrawFacing_ ? -40 - ox[cell] : ox[cell]);
            dy = drawY + oy[cell];
            srcW = sourceWalkerDrawFacing_ ? -static_cast<float>(fw) : static_cast<float>(fw);
        }
        if (deckTurret) {
            // OID 50 attack_parent_atlas.json: normalized six-cell source
            // envelope is -30..+23; reflection around the source pixel is
            // -23..+30. Sprite placement is independent of contact geometry.
            dx = drawX + (facingRight ? -23.0f : -30.0f);
            dy = drawY - 14.0f;
        }
        Rectangle srcRect  = { (float)tx, (float)ty, srcW, (float)fh };
        Rectangle destRect = { dx, dy, (float)fw, (float)fh };
        Color tint = WHITE;
        if (invulnerable) tint = { 150, 150, 180, 255 };
        DrawTexturePro(texture, srcRect, destRect, {0, 0}, 0.0f, tint);
        // White damage flash: the real game blanks the enemy to a white
        // silhouette for a few frames when hit. Additive white pass over the
        // same sprite pixels (alpha is preserved, so only the sprite flashes).
        if (allowHitFlashOverlay && hitFlash > 0 && !exactFlashOwnsLiveBody) {
            BeginBlendMode(BLEND_ADDITIVE);
            DrawTexturePro(texture, srcRect, destRect, {0, 0}, 0.0f, {255, 255, 255, 255});
            EndBlendMode();
        }
    } else if (!placeholderLabel_.empty()) {
        // Provisional census placeholder (waiver W2): no sprite exists for
        // this OID yet, so draw the hitbox outline with the OID above it.
        const int bx = static_cast<int>(drawX + hitboxOffset.x);
        const int by = static_cast<int>(drawY + hitboxOffset.y);
        DrawRectangleLines(bx, by, static_cast<int>(hitboxSize.x),
                           static_cast<int>(hitboxSize.y), MAGENTA);
        DrawText(placeholderLabel_.c_str(), bx, by - 10, 10, RAYWHITE);
    }
}

void Enemy::renderAxeStack(float drawX, float drawY) {
    // Standing log magazine at the launcher: logs stacked at the muzzle
    // point — 32px behind the body center, bottom log ON the fire line,
    // next 16px above (oracle: stack x=469 vs anchor 501, rest logs at
    // y 1144 + 1128, feed from 1160). Same art as the flying log.
    if (behavior != EnemyBehavior::AxeMax) return;
    const float cx = drawX + hitboxOffset.x + hitboxSize.x * 0.5f;
    const float fireY = drawY + hitboxOffset.y + 2.0f;   // PendingShot line
    // Axe Max's body is source-authored left-facing and never turns.  The
    // launcher/log composition follows that same fixed source side: it is
    // always 32px to the LEFT of the body anchor.  Using facingRight here
    // put the launcher on the body's right even though the body definition
    // deliberately disables facing mirroring.
    const float muzzleX = cx - 32.0f;
    const float groundY = drawY + hitboxOffset.y + hitboxSize.y;

    // The stationary launcher barrel under the stack — cropped from the
    // real game (e5_drop_26 sf444; world x 457-475 on the CP ground, the
    // structure the logs rest on). Persists with the remnant.
    const TextureResource* baseTex =
        AssetCache::loadTexture("content/x1/sprites/enemies/axemax_launcher.png");
    if (baseTex && baseTex->valid()) {
        baseTex->setFilter(TEXTURE_FILTER_POINT);
        const float bw = static_cast<float>(baseTex->get().width);
        const float bh = static_cast<float>(baseTex->get().height);
        Rectangle src{0.0f, 0.0f, bw, bh};
        Rectangle dst{muzzleX - bw * 0.5f, groundY - bh, bw, bh};
        DrawTexturePro(baseTex->get(), src, dst, {0, 0}, 0.0f, WHITE);
    }

    if (axeStack_ <= 0) return;
    const TextureResource* logTex =
        AssetCache::loadTexture("content/x1/sprites/enemies/axemax_log.png");
    if (!logTex || !logTex->valid()) return;
    logTex->setFilter(TEXTURE_FILTER_POINT);
    constexpr float LW = 32.0f, LH = 16.0f;
    Rectangle src{0.0f, 0.0f, LW, LH};
    for (int i = 0; i < axeStack_; ++i) {
        Rectangle dst{muzzleX - LW * 0.5f,
                      fireY - LH * 0.5f - 16.0f * static_cast<float>(i),
                      LW, LH};
        DrawTexturePro(logTex->get(), src, dst, {0, 0}, 0.0f, WHITE);
    }
}

AABB Enemy::solidAxeStackHitbox() const {
    if (behavior != EnemyBehavior::AxeMax || !active) {
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }

    // Keep this envelope tied to the exact draw math above.  The launcher is
    // always present; the logs extend the top of the blocker while stocked.
    // This makes the visual object and movement collision share one source of
    // truth instead of introducing a second hand-tuned rectangle.
    const float cx = position.x + hitboxOffset.x + hitboxSize.x * 0.5f;
    const float fireY = position.y + hitboxOffset.y + 2.0f;
    const float muzzleX = cx - 32.0f;
    const float groundY = position.y + hitboxOffset.y + hitboxSize.y;
    constexpr float kLauncherWidth = 27.0f;
    constexpr float kLauncherHeight = 36.0f;
    constexpr float kLogWidth = 32.0f;
    constexpr float kLogHeight = 16.0f;

    const float launcherLeft = muzzleX - kLauncherWidth * 0.5f;
    const float launcherRight = muzzleX + kLauncherWidth * 0.5f;
    float left = launcherLeft;
    float right = launcherRight;
    float top = groundY - kLauncherHeight;
    if (axeStack_ > 0) {
        left = std::min(left, muzzleX - kLogWidth * 0.5f);
        right = std::max(right, muzzleX + kLogWidth * 0.5f);
        const float logTop = fireY - kLogHeight * 0.5f -
            16.0f * static_cast<float>(axeStack_ - 1);
        top = std::min(top, logTop);
    }
    return {left, top, right - left, groundY - top};
}

std::vector<Enemy::AxeStackSourceBox> Enemy::axeStackSourceContactBoxes() const {
    // Source: knowledge_base/mmx1/enemies/oid_0x0B_axemax_stage_coverage/
    // launcher_solid_box_2026-09-15.json (descriptors, list order, records).
    const auto lowerLog = cpRemnantLogSourceAnchor();
    const auto upperLog = cpRemnantUpperLogSourceAnchor();
    if (behavior != EnemyBehavior::AxeMax || !lowerLog || !upperLog) return {};

    struct Descriptor {
        std::uint16_t address;
        int offsetX, offsetY, halfExtentX, halfExtentY;
    };
    constexpr Descriptor kList[2] = {
        {0xC0CE, 0, -4, 10, 7},  // $86:C0CE = 00 FC 0A 07
        {0xC0D2, 0, 0, 13, 7},   // $86:C0D2 = 00 00 0D 07
    };
    // The launcher record's position is the retained source origin
    // (camera_return.json D080 relation), which the T1.7g trap read live at
    // $142D/$1430 = (469, 1160).
    const Vector2 records[3] = {cameraReturnSourceOrigin_, *lowerLog, *upperLog};
    std::vector<AxeStackSourceBox> boxes;
    boxes.reserve(6);
    for (const auto& record : records) {
        const float x = std::floor(record.x);
        const float y = std::floor(record.y);
        for (const auto& d : kList) {
            boxes.push_back({d.address,
                             {x + static_cast<float>(d.offsetX - d.halfExtentX),
                              y + static_cast<float>(d.offsetY - d.halfExtentY),
                              static_cast<float>(2 * d.halfExtentX + 1),
                              static_cast<float>(2 * d.halfExtentY + 1)}});
        }
    }
    return boxes;
}

void Enemy::updatePatrol() {
    velocity.x = facingRight ? patrolSpeed : -patrolSpeed;
    if (touchingWallRight) { facingRight = false; velocity.x = -patrolSpeed; }
    else if (touchingWallLeft) { facingRight = true; velocity.x = patrolSpeed; }
    if (onGround && tilemap_) {
        AABB hb = getHitbox();
        float checkX = facingRight ? hb.right() + 1.0f : hb.left() - 1.0f;
        float checkY = hb.bottom() + 1.0f;
        int tx = static_cast<int>(checkX) / tilemap_->tileSize();
        int ty = static_cast<int>(checkY) / tilemap_->tileSize();
        if (tilemap_->getTileType(tx, ty) == TileType::None) {
            facingRight = !facingRight;
            velocity.x = facingRight ? patrolSpeed : -patrolSpeed;
        }
    }
}

void Enemy::updateHideAndShoot() {
    velocity.x = 0;
    switch (enemyState) {
        case EnemyState::Hide:
            invulnerable = true; anim_.play("hide");
            if (--hideTimer <= 0 && target_) {
                if (distToTarget() < detectionRange) {
                    enemyState = EnemyState::Peek; peekTimer = 20;
                    invulnerable = false; facingRight = (dirToTarget() > 0);
                } else hideTimer = 30;
            }
            break;
        case EnemyState::Peek:
            anim_.play("peek");
            if (--peekTimer <= 0) { enemyState = EnemyState::Shoot; shootTimer = 10; }
            break;
        case EnemyState::Shoot:
            anim_.play("shoot");
            if (--shootTimer == 5) {
                float sx = facingRight ? position.x + 24 : position.x;
                float sy = position.y + 12;
                PendingShot shot{};
                shot.x = sx;
                shot.y = sy;
                shot.vx = facingRight ? 2.5f : -2.5f;
                shot.vy = 0.0f;
                pendingShots.push_back(std::move(shot));
            }
            if (shootTimer <= 0) { enemyState = EnemyState::Hide; hideTimer = 60; invulnerable = true; }
            break;
        default: break;
    }
}

void Enemy::updateFlyPattern() {
    if (enemyState == EnemyState::Idle) {
        flyTimer_++;
        velocity.y = std::cos(flyTimer_ * 0.08f) * 0.16f;
        if (target_ && distToTarget() < detectionRange) {
            enemyState = EnemyState::Fly; swooping_ = true; flyTimer_ = 0;
            float dx = target_->position.x - position.x;
            float dy = target_->position.y - position.y;
            float d = std::sqrt(dx*dx + dy*dy);
            if (d > 0) { swoopVX_ = (dx/d)*flySpeed; swoopVY_ = (dy/d)*flySpeed; }
            facingRight = (dx > 0);
        }
    } else if (swooping_) {
        velocity.x = swoopVX_; velocity.y = swoopVY_;
        if (++flyTimer_ > 40) { swooping_ = false; flyTimer_ = 0; }
    } else {
        float dx = homePos_.x - position.x;
        float dy = homePos_.y - position.y;
        float d = std::sqrt(dx*dx + dy*dy);
        if (d < 2.0f) { position = homePos_; velocity = {0,0}; enemyState = EnemyState::Idle; }
        else { velocity.x = (dx/d)*0.8f; velocity.y = (dy/d)*0.8f; }
    }
}

void Enemy::updateDrift() {
    // Intro Highway OID 0x15. RAM-measured: drifts LEFT at a constant 1.5 px/f
    // (flySpeed) with y held constant — a pure straight-line float, no gravity,
    // no player tracking. Verified in behavior_trace_2026-06-03/behavior.csv
    // (xvel -384/256, y fixed). Keep it dead simple so the measured motion is
    // exactly reproduced; do NOT borrow Patrol (wall-turn) or FlyPattern (swoop).
    // Leave velocity.y to gravity/collision so the wheel rolls on the deck.
    // Wall bounce: David observed the wheel reverses when it hits a wall (the
    // RAM trace only saw a wall-free stretch, hence the earlier left-only note).
    if (touchingWallLeft) facingRight = true;
    else if (touchingWallRight) facingRight = false;
    velocity.x = facingRight ? flySpeed : -flySpeed;
    anim_.play("idle");
}

void Enemy::updateTurret() {
    // Intro Highway OID 0x29. FE5.1 source trace 2026-07-05:
    // action 6 holds for 91f, the enemy-shot slot appears at action6+80,
    // anchor offset is (0,+6), left shot speed is 4 px/f, base-X damage is 2.
    constexpr int kFireStateFrames = 91;
    constexpr int kShotLaunchDelayFrames = 80;
    constexpr int kShotLaunchTimerValue =
        kFireStateFrames - kShotLaunchDelayFrames + 1;
    constexpr float kShotSpeed = 4.0f;
    constexpr float kShotOffsetY = 6.0f;
    velocity.x = 0.0f; // stationary; gravity/collision handles vertical settling
    if (shootTimer > 0) {
        anim_.play("shoot");
        if (shootTimer == kShotLaunchTimerValue) {
            const float dir = facingRight ? 1.0f : -1.0f;
            PendingShot s;
            s.x = position.x;
            s.y = position.y + kShotOffsetY;
            s.vx = dir * kShotSpeed;
            s.vy = 0.0f;
            s.damage = 2;
            // R238: original OID0e has three 16x16 blue energy layouts.
            // These art bounds do not redefine the projectile's hitbox.
            s.sprite = "content/x1/sprites/enemies/intro_robot_shot.png";
            s.visualWidth = s.visualHeight = 16;
            s.visualFrameCount = 3;
            s.visualFrameTicks = 2;
            s.visualMirrorsWithFacing = false;
            pendingShots.push_back(s);
        }
        --shootTimer;
        return;
    }
    if (target_ && distToTarget() < detectionRange) {
        facingRight = (dirToTarget() > 0);
        shootTimer = kFireStateFrames;
        anim_.play("shoot");
    } else {
        anim_.play("idle");
    }
}

void Enemy::updateAxeMax() {
    // CP OID 0x0B lumberjack — oracle _snow_cannon_runs/FINDINGS.md
    // (e1_observe/park_close/cadence_long/ledge, 2026-06-10):
    //   - 2-log volleys: fire, +65f, fire, then REST 139f (period 204,
    //     zero variance over 12 fires at range);
    //   - CLOSE-RANGE ALERT: |dx| < ~110 -> rest stretches to ~220f and
    //     the idle plays the tracking cycle (sheet cells 1<->2, 6f/step);
    //   - log: 32x16, EXACTLY 3.0 px/f horizontal from the stack 32px
    //     behind-left of the anchor at the fixed fire line (anchor-2 px);
    //   - fires authored-left regardless of range/player side: 13/13 source
    //     launches stayed left with X on/past the anchor. FE5.7B separately
    //     proves the source-authored body art never turns or mirrors.
    //   - firing anim: cells 3..10 stepping 3f, the log releases on cell
    //     8 (spr 0x88), 15f into the sweep.
    constexpr int kVolleyGap   = 65;
    constexpr int kRestFar     = 139;
    constexpr int kRestAlert   = 220;
    constexpr float kAlertDx   = 110.0f;
    constexpr int kSweepLead   = 15;   // frames of sweep before the log launches
    constexpr float kLogSpeed  = 3.0f;
    velocity.x = 0.0f;

    const float cx = position.x + hitboxOffset.x + hitboxSize.x * 0.5f;
    const bool alert = target_ && distToTarget() < kAlertDx;

    if (axeSweep_ >= 0) {
        anim_.play("shoot");
        axeSweep_++;
        if (axeSweep_ == kSweepLead) {
            if (axeStack_ > 0) --axeStack_;   // the fire-line log launches
            PendingShot s;
            // Source-closed Axe Max orientation: the stack and every log
            // launch are authored left of the fixed body anchor.
            s.x = cx - 32.0f;           // the log stack sits 32px left
            s.y = position.y + hitboxOffset.y + 2.0f;  // fixed fire line
            s.vx = -kLogSpeed;
            s.vy = 0.0f;
            s.sprite = "content/x1/sprites/enemies/axemax_log.png";
            s.w = 32.0f;
            s.h = 16.0f;
            s.visualMirrorsWithFacing = false;
            if (cpCameraReturn_) {
                // second_creation_and_log_2026-09-17.json: C0D6 contact, separate art.
                s.x = std::floor(cameraReturnSourceOrigin_.x);
                s.y = std::floor(cameraReturnSourceOrigin_.y) - 32.0f;
                s.w = 26; s.h = 12;
                s.visualWidth = 32; s.visualHeight = 16;
                s.sourceAxeMaxLog = true;
            }
            s.damage = 2;   // Source trace 2026-07-05: clean unarmored
                            // log-hit candidates pin base damage 2.
            pendingShots.push_back(s);
        }
        if (axeSweep_ >= 24) {          // 8 cells x 3f
            axeSweep_ = -1;
            // Latch the alert decision for the WHOLE rest (anim + length):
            // re-evaluating per frame made the sprite flicker between the
            // idle and tracking cells whenever X hovered near the 110px
            // ring. The rest length was already latched here.
            axeAlertLatched_ = alert;
            // fire-to-fire = (24-15 sweep tail) + timer + 1 (sweep-start
            // tick) + 15 (next lead) = timer + 25.
            if (axeShotInVolley_ == 0) {
                axeShotInVolley_ = 1;
                axeTimer_ = kVolleyGap - 25;
            } else {
                axeShotInVolley_ = 0;
                axeTimer_ = (alert ? kRestAlert : kRestFar) - 25;
            }
        }
        return;
    }

    anim_.play(axeAlertLatched_ ? "alert" : "idle");
    if (axeTimer_ > 0) {
        // Restock during the long rest: a new log appears 70f then 54f
        // before the next volley's launch (oracle: restocks at f381/f397
        // vs next fire f451; f440/f456 vs f510 — both pairs exact).
        // Launch happens axeTimer_+16 ticks from here (countdown + sweep
        // start tick + 15-frame lead, fire anchored at tick 67/132/271),
        // so restock at timer 55 and 39 puts the stack change exactly 70
        // and 54 ticks before the launch tick. Gated on axeShotInVolley_
        // == 0 (the long rest after a completed volley) — the oracle shows
        // NO restock inside the 65f volley gap. Alert-rest timing
        // $provisional (same offsets assumed; only far-rest pairs
        // measured).
        if (axeShotInVolley_ == 0 && (axeTimer_ == 55 || axeTimer_ == 39))
            axeStack_ = std::min(2, axeStack_ + 1);
        --axeTimer_;
        return;
    }
    axeSweep_ = 0;
}

void Enemy::configureSourceWalker() {
    sourceWalker_ = true;
    sourceWalkerPhase_ = 0;
    sourceWalkerSpawnX_ = std::floor(position.x);
    sourceWalkerWait_ = 1;
    sourceWalkerCell_ = sourceWalkerDrawCell_ = 0;
    sourceWalkerCellTicks_ = 8;
    sourceWalkerDrawReady_ = false;
    sourceWalkerDrawPosition_ = position;
    gravity = 0;
    velocity = {0, 0};
    hitboxOffset = {-12, -12};
    hitboxSize = {24, 22};
}

void Enemy::pickSourceWalkerLaunch() {
    // walker_direction_gate_2026-09-17.json: init and stand timer 16.
    // walker_action_gates_2026-09-17.json: D791 forces inward at 128px.
    const float fromSpawn = std::floor(position.x) - sourceWalkerSpawnX_;
    if (fromSpawn >= 128) facingRight = false;
    else if (fromSpawn <= -128) facingRight = true;
    else if (target_) facingRight = player_anchor::sourceRamAnchorX(
        target_->position.x, target_->spriteWidth, target_->facingRight) >= position.x;
    sourceWalkerVx_ = facingRight ? 1.5f : -1.5f;
    sourceWalkerVy_ = -4;
}

void Enemy::updateSourceWalker() {
    // T1.7l first-hop and controlled repeat-hop witnesses. Firing branch
    // integration remains pending; the measured hop path is accepted.
    sourceWalkerDrawPosition_ = position;
    sourceWalkerDrawCell_ = sourceWalkerCell_;
    sourceWalkerDrawFacing_ = facingRight;
    sourceWalkerDrawReady_ = sourceWalkerPhase_ != 0;
    velocity = {0, 0};
    if (sourceWalkerPhase_ == 0) {
        pickSourceWalkerLaunch();
        sourceWalkerPhase_ = 1;
        return;
    }
    if (sourceWalkerPhase_ == 1) {
        if (sourceWalkerWait_ == 16) pickSourceWalkerLaunch();
        if (--sourceWalkerWait_ != 0) return;
        sourceWalkerCell_ = 0;
        sourceWalkerCellTicks_ = 8;
        sourceWalkerPhase_ = 2;
        return;
    }
    if (sourceWalkerPhase_ == 2) {
        if (sourceWalkerCell_ == 3) {
            sourceWalkerPhase_ = 3;
            return;
        }
        if (--sourceWalkerCellTicks_ == 0) {
            ++sourceWalkerCell_;
            sourceWalkerCellTicks_ = 8;
        }
        return;
    }
    sourceWalkerVy_ += 0.25f;
    velocity = {sourceWalkerVx_, sourceWalkerVy_};
    if (sourceWalkerVy_ > 0) sourceWalkerCell_ = 2;
}

void Enemy::finishSourceWalkerMotion(float attemptedY) {
    if (!sourceWalker_ || sourceWalkerPhase_ != 3) return;
    bool sourceFloor = false;
    // walker_slope_contact/fetch_2026-09-17.json: probe is box bottom+4,
    // tile coordinates use integer words and local nibble+1, not interpolation.
    if (tilemap_ && tilemap_->tileSize() == 16 && sourceWalkerVy_ > 0) {
        const int x = static_cast<int>(std::floor(position.x));
        const int probeY = static_cast<int>(std::floor(attemptedY)) + 14;
        const int col = x / 16, row = probeY / 16;
        const auto type = tilemap_->getTileType(col, row);
        const auto slope = tilemap_->getSlope(col, row);
        sourceFloor = type == TileType::Solid ||
            (type == TileType::SlopeL && slope.rightY == slope.leftY + 4);
        if (sourceFloor) {
            const int height = type == TileType::Solid ? 0 :
                slope.leftY + (((x & 15) + 1) >> 2);
            onGround = height < (probeY & 15) + 1;
            position.y = onGround ? row * 16 + height - 11 +
                attemptedY - std::floor(attemptedY) : attemptedY;
            if (!onGround) velocity.y = sourceWalkerVy_;
        }
    }
    if (!onGround) return;
    // Unmeasured terrain retains its existing collision behavior.
    if (!sourceFloor)
        position.y = std::floor(position.y) - 1 + attemptedY - std::floor(attemptedY);
    sourceWalkerPhase_ = 1;
    sourceWalkerWait_ = 30;
    sourceWalkerCell_ = 0;
    velocity = {0, 0};
}

void Enemy::updateWalker() {
    if (sourceWalker_) { updateSourceWalker(); return; }
    // CP OID 0x51 (U17, _walker_runs/FINDINGS.md, obs_p110/140/170):
    //   - pure chaser: EVERY bout toward X (12/12 incl. tracking X's
    //     knockback dance); facing follows X;
    //   - cycle: stand 55f -> walk 30f at EXACTLY 1.5 px/f (the SNES 2,1
    //     px alternation; float 1.5 = the same pixel coverage), after a
    //     27f activation stand;
    //   - firing stands stretch to 125f with the shot LAUNCHING at
    //     stand+48 (the table shows a 12f stationary warmup first — the
    //     engine spawns at the launch; warmup visual $open-minor): from
    //     anchor+18px toward X at 2.25 px/f, base damage 2, SFX 0x33;
    //   - fire gate $provisional: alternating stands while X is >= 24px
    //     away (exact range/cycle rule = FINDINGS $open 1; p170 fired 3x,
    //     p110 contact-range fired 0x).
    constexpr int kStand = 55;
    constexpr int kWalk = 30;
    constexpr int kStandFire = 125;
    constexpr int kFireAt = 48;
    constexpr float kWalkSpeed = 1.5f;
    constexpr float kShotSpeed = 2.25f;
    constexpr float kShotMuzzle = 18.0f;
    constexpr float kFireMinDx = 24.0f;

    if (target_) facingRight = (dirToTarget() > 0);

    if (walkerWalking_) {
        anim_.play("walk");
        velocity.x = (facingRight ? 1.0f : -1.0f) * kWalkSpeed;
        if (--walkerTimer_ <= 0) {
            walkerWalking_ = false;
            walkerStandIndex_++;
            const bool inRange = target_
                && std::fabs(target_->position.x - position.x) >= kFireMinDx;
            walkerStandFires_ = inRange && (walkerStandIndex_ % 2 == 0);
            walkerStandLen_ = walkerStandFires_ ? kStandFire : kStand;
            walkerTimer_ = walkerStandLen_;
            walkerStandAge_ = 0;
        }
        return;
    }

    velocity.x = 0.0f;
    anim_.play("idle");
    walkerStandAge_++;
    if (walkerStandFires_ && walkerStandAge_ == kFireAt) {
        const float dir = facingRight ? 1.0f : -1.0f;
        const float cx = position.x + hitboxOffset.x + hitboxSize.x * 0.5f;
        const float cy = position.y + hitboxOffset.y + hitboxSize.y * 0.5f;
        PendingShot s{};
        s.x = cx + dir * kShotMuzzle;
        s.y = cy;
        s.vx = dir * kShotSpeed;
        s.vy = 0.0f;
        s.sprite = "content/x1/sprites/enemies/walker_shot.png";
        s.w = 16.0f;
        s.h = 12.0f;
        s.damage = 2;   // base (armour rule halves on X: obs -1 ladders)
        pendingShots.push_back(s);
        AudioManager::playSFX(SFX::EnemyShoot);   // 0x33 at the launch
    }
    if (--walkerTimer_ <= 0) {
        walkerWalking_ = true;
        walkerTimer_ = kWalk;
    }
}

void Enemy::seedBatDriftRng(uint32_t seed) {
    batDriftRng_ = seed;
    batDriftHangDraws_ = 0;
    batDriftHoldDraws_ = 0;
    batDriftDirectionDraws_ = 0;
}

void Enemy::setBatDriftChoiceSequence(BatDriftChoices choices) {
    batDriftChoices_ = std::move(choices);
    batDriftHangDraws_ = 0;
    batDriftHoldDraws_ = 0;
    batDriftDirectionDraws_ = 0;
    if (!batDriftChoices_.hangFrames.empty()) {
        // Same convention as init: the spawn frame is the first hang frame.
        batDriftHang_ = batDriftChoices_.hangFrames[0] - 1;
        batDriftHangDraws_ = 1;
    }
}

uint32_t Enemy::nextBatDriftRoll() {
    // xorshift32: small, deterministic and seedable, which is all the draws
    // need. The SOURCE's own PRNG was tested against this enemy's choices and
    // does not explain them (direction_law.json, dependence.rngValue), so this
    // reproduces the measured distribution, not the source's sequence.
    batDriftRng_ ^= batDriftRng_ << 13;
    batDriftRng_ ^= batDriftRng_ >> 17;
    batDriftRng_ ^= batDriftRng_ << 5;
    return batDriftRng_;
}

void Enemy::drawBatDriftDirection() {
    using namespace enemy_bat_drift_law;
    if (batDriftDirectionDraws_ < static_cast<int>(batDriftChoices_.directions.size())) {
        const auto& d = batDriftChoices_.directions[batDriftDirectionDraws_++];
        batDriftVx_ = d.first;
        batDriftVy_ = d.second;
    } else {
        const int index = pickDirectionIndex(static_cast<int>(nextBatDriftRoll() & 0x7FFFFFFF));
        batDriftVx_ = kDirections[index].vx;
        batDriftVy_ = kDirections[index].vy;
        ++batDriftDirectionDraws_;
    }
    if (batDriftHoldDraws_ < static_cast<int>(batDriftChoices_.holdFrames.size())) {
        batDriftHold_ = batDriftChoices_.holdFrames[batDriftHoldDraws_++];
    } else {
        batDriftHold_ = pickWeightedValue(kHoldFrames, kHoldFramesCount, kHoldFramesWeight,
                                          static_cast<int>(nextBatDriftRoll() & 0x7FFFFFFF));
        ++batDriftHoldDraws_;
    }
    // The direction picks the facing: the art is mirrored left/right and the
    // movie's anim bytes move with the direction (step 1c).
    if (batDriftVx_ != 0) facingRight = batDriftVx_ > 0;
}

void Enemy::updateHoverPatrol() {
    // Source witness: knowledge_base/mmx1/enemies/oid_0x49/motion.json and
    // its R101 contract.  The source stores signed 1/256 velocities, while
    // the shared physics path consumes the applied Actor velocity exactly
    // once after this update.
    static constexpr int kSourceSpeedFp = 384;
    static constexpr int kAction2DownTimer = 21;
    static constexpr int kAction2HoldTimer = 30;
    static constexpr int kAction2UpTimer = 21;
    static constexpr std::uint16_t kBoundaryWords = 0x0040;

    auto sourceWord = [](float value) -> std::uint16_t {
        return static_cast<std::uint16_t>(static_cast<std::int64_t>(
            std::floor(value)));
    };
    auto sourceBoundaryMagnitude = [](std::uint16_t post,
                                      std::uint16_t reference) {
        std::uint16_t difference =
            static_cast<std::uint16_t>(post - reference);
        if ((difference & 0x8000u) != 0) {
            difference = static_cast<std::uint16_t>(0u - difference);
        }
        return difference;
    };
    auto sourcePlayerWord = [&]() -> std::optional<std::uint16_t> {
        if (!target_) return std::nullopt;
        const float anchor = player_anchor::sourceRamAnchorX(
            target_->position.x, target_->spriteWidth, target_->facingRight);
        return sourceWord(anchor);
    };
    auto sourceAligned = [&]() {
        const auto playerWord = sourcePlayerWord();
        if (!playerWord) return false;
        const std::uint16_t actorWord = sourceWord(position.x);
        const std::uint16_t probe = static_cast<std::uint16_t>(
            static_cast<std::uint32_t>(*playerWord) - actorWord + 4u);
        return probe < 8u;
    };
    auto applyHorizontal = [&]() {
        if (hoverPatrolSourceVxFp_ == 0) {
            velocity = {0.0f, 0.0f};
            return;
        }
        // 87:C6DD compares the next integer source word with the saved
        // reference. A boundary update cancels this frame's applied step and
        // retains the reversed source patrol velocity for the next update.
        const float projectedX = position.x +
            static_cast<float>(hoverPatrolSourceVxFp_) / 256.0f;
        const auto projectedWord = sourceWord(projectedX);
        if (sourceBoundaryMagnitude(projectedWord, hoverPatrolReferenceX_) >=
            kBoundaryWords) {
            hoverPatrolSourceVxFp_ = -hoverPatrolSourceVxFp_;
            facingRight = hoverPatrolSourceVxFp_ > 0;
            velocity = {0.0f, 0.0f};
            return;
        }
        facingRight = hoverPatrolSourceVxFp_ > 0;
        velocity = {static_cast<float>(hoverPatrolSourceVxFp_) / 256.0f,
                    0.0f};
    };
    auto applyVertical = [&]() {
        // The source vertical word is positive upward; native Actor Y is
        // positive downward. Keep the conversion at the physics boundary.
        velocity = {0.0f,
                    -static_cast<float>(hoverPatrolSourceVyFp_) / 256.0f};
    };

    enemyState = EnemyState::Fly;

    switch (hoverPatrolSourcePhase_) {
        case HoverPatrolSourcePhase::SetupReference:
            hoverPatrolReferenceX_ = sourceWord(position.x);
            hoverPatrolSourceVxFp_ = 0;
            hoverPatrolSourceVyFp_ = 0;
            hoverPatrolSourceTimer_ = 0;
            hoverPatrolSourcePhase_ = HoverPatrolSourcePhase::SetupVelocity;
            velocity = {0.0f, 0.0f};
            return;

        case HoverPatrolSourcePhase::SetupVelocity:
            if (const auto playerWord = sourcePlayerWord()) {
                const std::uint16_t actorWord = sourceWord(position.x);
                facingRight = static_cast<int>(*playerWord) >=
                              static_cast<int>(actorWord);
            }
            hoverPatrolSourceVxFp_ = facingRight ? kSourceSpeedFp
                                                  : -kSourceSpeedFp;
            hoverPatrolSourcePhase_ = HoverPatrolSourcePhase::Horizontal;
            velocity = {0.0f, 0.0f};
            return;

        case HoverPatrolSourcePhase::Horizontal:
            // Readiness follows dispatch, including a canceled boundary step.
            hoverPatrolHorizontalEntered_ = true;
            if (sourceAligned()) {
                hoverPatrolSourcePhase_ = HoverPatrolSourcePhase::Action2Down;
                hoverPatrolSourceTimer_ = 0;
            }
            applyHorizontal();
            return;

        case HoverPatrolSourcePhase::Action2Down:
            if (hoverPatrolSourceTimer_ == 0) {
                hoverPatrolSourceVyFp_ = -512;
                hoverPatrolSourceTimer_ = kAction2DownTimer;
                velocity = {0.0f, 0.0f};
                return;
            }
            if (hoverPatrolSourceTimer_ > 1) {
                --hoverPatrolSourceTimer_;
                applyVertical();
                return;
            }
            hoverPatrolSourceTimer_ = kAction2HoldTimer;
            hoverPatrolSourcePhase_ = HoverPatrolSourcePhase::Action2Hold;
            velocity = {0.0f, 0.0f};
            return;

        case HoverPatrolSourcePhase::Action2Hold:
            if (hoverPatrolSourceTimer_ > 1) {
                --hoverPatrolSourceTimer_;
                velocity = {0.0f, 0.0f};
                return;
            }
            hoverPatrolSourceTimer_ = kAction2UpTimer;
            hoverPatrolSourceVyFp_ = 512;
            hoverPatrolSourcePhase_ = HoverPatrolSourcePhase::Action2Up;
            velocity = {0.0f, 0.0f};
            return;

        case HoverPatrolSourcePhase::Action2Up:
            if (hoverPatrolSourceTimer_ > 0) {
                --hoverPatrolSourceTimer_;
                applyVertical();
                if (hoverPatrolSourceTimer_ == 0) {
                    hoverPatrolSourcePhase_ = HoverPatrolSourcePhase::SetupVelocity;
                }
                return;
            }
            hoverPatrolSourcePhase_ = HoverPatrolSourcePhase::SetupVelocity;
            velocity = {0.0f, 0.0f};
            return;
    }
}

void Enemy::updateStrideWalk() {
    // SE OID 0x27 (plan task R1.0x27), measured over all four instances of
    // David's Storm Eagle movie: this object has no speed. Its fixed-point x
    // holds still and then jumps once per animation cell, by a constant that
    // belongs to the cell -- 6, 9, 9, 6, 6, 11, 9, 7 px, no exception in 32
    // recorded steps, 63 px per eight-cell cycle. Two frames after the spawn
    // it hops 7 px forward and 4 px UP into the walk row (4 of 4 instances).
    const float dir = facingRight ? 1.0f : -1.0f;
    ++strideTimer_;
    if (strideSpawning_) {
        if (strideTimer_ >= kStrideSpawnFrames) {
            position.x += dir * float(kStrideSpawnHopX);
            position.y -= float(kStrideSpawnHopY);
            strideSpawning_ = false;
            strideCell_ = 0;
            strideTimer_ = 0;
        }
    } else if (strideTimer_ >= kStrideFrames) {
        position.x += dir * float(kStrideStep[strideCell_]);
        strideCell_ = (strideCell_ + 1) % kStrideCells;
        strideTimer_ = 0;
    }
    velocity = {0.0f, 0.0f};
    enemyState = EnemyState::Patrol;
}

void Enemy::updateSnowballRoll() {
    // CP OID 0x54, plan task R1.0x54. Measured over all six instances of
    // David's Chill Penguin movie (knowledge_base/_movies/chill-penguin/
    // enemies.csv, the OID 0x54 rows, frames 9047..10086):
    //
    //   * while it is AIRBORNE its vertical velocity changes by exactly -64 fp
    //     per frame -- 0.25 px/f^2, which is the def's gravity, NOT the 0.22
    //     the rest of this file uses -- and its horizontal velocity does not
    //     change at all;
    //   * while it is GROUNDED its horizontal velocity is re-written by
    //     exactly -12 fp every frame and its vertical one is zero.
    //
    // The planner's question was whether that -12 is the slope's share of
    // gravity rather than a constant. It is NOT: the ball keeps accelerating
    // on FLAT ground (collision class 0) in every instance -- 20 of instance
    // 0's 48 accelerating frames, 12 of instance 3's 35 -- and a slope-driven
    // term is zero there. So the measured constant ships, and it is a
    // constant.
    enemyState = EnemyState::Idle;
    if (onGround) {
        snowballVxFp_ += kSnowballAccelFp;
        velocity.y = 0.0f;
    }
    velocity.x = static_cast<float>(snowballVxFp_) / 256.0f;
    facingRight = snowballVxFp_ > 0;
}

void Enemy::setEagletBurst(int vxFp, int vyFp, int lifeFrames) {
    eagletXf_ = static_cast<int>(position.x * 256.0f);
    eagletYf_ = static_cast<int>(position.y * 256.0f);
    eagletVx_ = vxFp;
    eagletVy_ = vyFp;
    eagletLife_ = lifeFrames;
    facingRight = vxFp > 0;
}

void Enemy::updateEagletBurst() {
    // SE OID 0x56, plan task R7.eggs. Measured over the four instances the
    // movie hatches on frame 16738: each holds the diagonal it was given --
    // (-/+361, -/+361) fixed point, a 512 fp step split 45 degrees -- and the
    // two +x ones stop being logged after 6 frames while the two -x ones run
    // 57. The DIVE that the KB records after the 15-frame burst is not wired:
    // this movie only shows it for the two that survive.
    eagletXf_ += eagletVx_;
    eagletYf_ += eagletVy_;
    position.x = static_cast<float>(eagletXf_) / 256.0f;
    position.y = static_cast<float>(eagletYf_) / 256.0f;
    velocity = {0.0f, 0.0f};
    enemyState = EnemyState::Idle;
    if (eagletLife_ > 0 && --eagletLife_ == 0) {
        active = false;
        alive = false;
    }
}

void Enemy::updateAnchored() {
    // Source anchored actors remain motionless while their attack controllers run.
    velocity = {0.0f, 0.0f};
    enemyState = EnemyState::Idle;
    if (type == "mad_pecker") {
        const auto word = [](float value) {
            return static_cast<std::uint16_t>(static_cast<std::int64_t>(std::floor(value)));
        };
        const auto px = target_ ? word(player_anchor::sourceRamAnchorX(
            target_->position.x,target_->spriteWidth,target_->facingRight)) : word(position.x);
        const auto py = target_ ? word(target_->position.y+40.0f) : word(position.y);
        if (madPeckerController_.state01 == 0) {
            // Fresh initialization is its own source dispatch, without a live tick.
            // Native textures replace level-specific graphics/palette indices.
            madPeckerController_ = mad_pecker_parent_source::initializeState00_87_DEFF(
                word(position.x),word(position.y),px,{}).controller;
        } else {
            madPeckerLaunch_ = madPeckerController_.tick(px,py).requestChild23;
        }
        facingRight = (madPeckerController_.attr11 & 0x40) != 0;
        if (madPeckerController_.state01 == 4) enemyState = EnemyState::Shoot;
        return;
    }
    if (type == "se_turret") {
        deck_turret_parent::PlayerInput input{};
        if (target_) {
            input.player_x = static_cast<std::uint16_t>(static_cast<std::int64_t>(std::floor(
                player_anchor::sourceRamAnchorX(target_->position.x, target_->spriteWidth, target_->facingRight))));
            // Authenticated terrain ground bit BD3=04. Other source terrain bits
            // remain outside the current normal/active-dash contact mapping.
            input.player_bd3 = target_->onGround;
            input.helper_angle = deck_turret_parent::aim(
                static_cast<std::uint16_t>(deckTurretController_.fields().parent_x),
                static_cast<std::uint16_t>(static_cast<std::int64_t>(std::floor(position.y))),
                static_cast<std::uint16_t>(input.player_x),
                static_cast<std::uint16_t>(static_cast<std::int64_t>(std::floor(target_->position.y + 40.0f))));
        }
        deckTurretLaunch_ = deckTurretController_.tick(input).fired;
        const auto& state = deckTurretController_.fields();
        facingRight = (state.attr_11 & 0x40) != 0;
        if (state.flag_35 != 0 || state.state == 4) enemyState = EnemyState::Shoot;
        return;
    }
    if (type != "stretch_bird") return;
    stretch_bird_parent::Input input{};
    input.parentAttr11 = facingRight ? 0 : 0x40;
    if (target_) {
        const auto word = [](float value) {
            return static_cast<std::uint16_t>(static_cast<std::int64_t>(std::floor(value)));
        };
        input.playerX = word(player_anchor::sourceRamAnchorX(
            target_->position.x, target_->spriteWidth, target_->facingRight));
        input.playerY = word(target_->position.y + 40.0f);
        const auto profile = target_->sourceContactProfile();
        if (profile) input.playerDescriptor = *profile == SourceContactProfile::NormalA552
            ? stretch_bird_parent::PlayerDescriptor::NormalA552
            : stretch_bird_parent::PlayerDescriptor::ActiveDashBB38;
    }
    const auto result = stretch_bird_parent::step(stretchBirdState_, input,
        static_cast<std::uint16_t>(static_cast<std::int64_t>(std::floor(position.x))),
        static_cast<std::uint16_t>(static_cast<std::int64_t>(std::floor(position.y))));
    stretchBirdLaunch_ = result.spawnChild;
    if (result.action != 0) enemyState = EnemyState::Shoot;
}

void Enemy::updateBatDrift() {
    // CP OID 0x2D (plan task R1.0x2D, direction_law.json): hang motionless at
    // the spawn for the drawn number of frames, then integrate the source's
    // own fixed point exactly — xf += vx, yf -= vy at 1/256 px, measured on
    // 232/279 and 262/279 frame pairs of the longest instance, the remainder
    // being the hang — redrawing the direction when its hold runs out.
    using namespace enemy_bat_drift_law;
    if (batDriftHang_ > 0) {
        --batDriftHang_;
        velocity = {0.0f, 0.0f};
        enemyState = EnemyState::Idle;
        return;
    }
    if (batDriftHold_ <= 0) {
        if (batDriftVx_ == 0 && batDriftVy_ == 0) anim_.play("fly");
        drawBatDriftDirection();
    }

    batDriftXf_ += batDriftVx_;
    batDriftYf_ -= batDriftVy_;
    --batDriftHold_;

    const float previousX = position.x;
    const float previousY = position.y;
    position.x = static_cast<float>(batDriftXf_) / kFixedPointDivisor;
    position.y = static_cast<float>(batDriftYf_) / kFixedPointDivisor;
    velocity = {position.x - previousX, position.y - previousY};
    enemyState = EnemyState::Fly;
}

void Enemy::updateBat() {
    // CP OID 0x19 (U19, _bat_runs/FINDINGS.md, obs_p250/320/380):
    //   - fixed-height patrol (y constant 1091 across 2000+ rows — the
    //     "dive" is lore) at EXACTLY 1.25 px/f (the 4f dx cycle
    //     -1,-2,-1,-1 = fp 320; float 1.25 covers the same pixels);
    //   - one choreographed HOVER per pass after 113px of travel
    //     (570 -> 457 measured): 118f stationary, dropping a mine at
    //     hover+25 and hover+49 (bat-10px below, inheriting the flight
    //     drift), then the patrol resumes;
    //   - hover position is MAP-FIXED (three X positions tested — not
    //     X-coupled); off-camera the bat despawns/wraps (scene cull).
    //   - source trace 2026-07-05: body contact deals 2 base damage,
    //     landed mine touch deals 1 base damage.
    constexpr float kPatrolSpeed = 1.25f;
    constexpr float kHoverAfterPx = 113.0f;
    constexpr int kHoverFrames = 118;
    constexpr int kMineDropA = 25;
    constexpr int kMineDropB = 49;
    constexpr float kMineDownOffset = 10.0f;
    constexpr float kMineDriftVx = 1.25f;  // inherited flight momentum

    anim_.play("fly");
    velocity.y = 0.0f;

    if (batHoverTimer_ > 0) {
        velocity.x = 0.0f;
        const int age = kHoverFrames - batHoverTimer_;
        if (age == kMineDropA || age == kMineDropB) {
            const float dir = facingRight ? 1.0f : -1.0f;
            const float cx = position.x + hitboxOffset.x + hitboxSize.x * 0.5f;
            const float cy = position.y + hitboxOffset.y + hitboxSize.y * 0.5f;
            PendingShot m{};
            m.x = cx;
            m.y = cy + kMineDownOffset;
            m.vx = dir * kMineDriftVx;
            m.vy = 0.0f;
            m.mine = true;
            m.sprite = "content/x1/sprites/enemies/bat_mine.png";
            m.w = 14.0f;
            m.h = 10.0f;
            m.damage = 1;   // source trace 2026-07-05: landed mine touch
            pendingShots.push_back(m);
            batMinesDropped_++;
        }
        if (--batHoverTimer_ <= 0) {
            batHoverDone_ = true;
        }
        return;
    }

    const float dir = facingRight ? 1.0f : -1.0f;
    velocity.x = dir * kPatrolSpeed;
    batTravelled_ += kPatrolSpeed;
    if (!batHoverDone_ && batTravelled_ >= kHoverAfterPx) {
        batHoverTimer_ = kHoverFrames;
        velocity.x = 0.0f;
    }
}

void Enemy::updateHover() {
    // Intro Highway OID 0x0F helicopter: hovers at its authored height with a
    // gentle vertical bob (gravity-free). Patrol/bomb/dynamic-terrain fidelity:
    // docs/history/cockpit/BOARD.md H-P-hazard.
    flyTimer_++;
    velocity.x = 0.0f;
    velocity.y = std::sin(static_cast<float>(flyTimer_) * 0.1f) * 0.3f;
}

void Enemy::updateDead() {
    deathTimer++;
    velocity = {0, 0};
    if (deathTimer == 1) spawnDeathBurst();
    for (auto& n : deathNodes_) {
        n.x += n.vx;
        n.y += n.vy;
    }
    if (deathTimer >= DEATH_FLASH_FRAMES) {
        if (behavior == EnemyBehavior::AxeMax) {
            // Oracle e5: killing the lumberjack leaves the launcher — the
            // robot vanishes but the log stack restocks to full and FREEZES
            // as scenery (slot persists, aggro bit cleared, logs never
            // fire). Keep the entity active for the visible stack and its
            // movement contact; alive=false excludes normal body combat. Deviation
            // ($provisional): real stacked logs are still destructible by
            // player shots (0x23 in sfx_e6_kill); remnant here is inert.
            remnant_ = true;
            axeStack_ = 2;
        } else {
            active = false;
        }
    }
}

void Enemy::spawnDeathBurst() {
    // Small radial spark pop centered on the hitbox (matches X's death burst
    // style at enemy scale).
    deathNodes_.clear();
    const float cx = position.x + hitboxOffset.x + hitboxSize.x * 0.5f;
    const float cy = position.y + hitboxOffset.y + hitboxSize.y * 0.5f;
    constexpr float kTau = 6.2831853f;
    // Two rings — fast outer + slower offset inner — for a fuller burst.
    constexpr int kOuter = 8, kInner = 6;
    for (int i = 0; i < kOuter; ++i) {
        const float a = (kTau / kOuter) * static_cast<float>(i);
        deathNodes_.push_back({cx, cy, std::cos(a) * 2.7f, std::sin(a) * 2.7f});
    }
    for (int i = 0; i < kInner; ++i) {
        const float a = (kTau / kInner) * static_cast<float>(i) + 0.4f;
        deathNodes_.push_back({cx, cy, std::cos(a) * 1.4f, std::sin(a) * 1.4f});
    }
}

float Enemy::distToTarget() const {
    if (!target_) return 9999.0f;
    float dx = target_->position.x - position.x;
    float dy = target_->position.y - position.y;
    return std::sqrt(dx*dx + dy*dy);
}

float Enemy::dirToTarget() const {
    if (!target_) return 1.0f;
    return (target_->position.x > position.x) ? 1.0f : -1.0f;
}

void Enemy::updateKBFSM() {
    kbFsm_->tick();
    applyKBState(kbFsm_->currentStateName(), kbFsm_->transitioned());
}

void Enemy::applyKBState(const std::string& stateName, bool enteredState) {
    if (stateName == "idle" || stateName == "init" ||
        stateName == "alt" || stateName == "recover") {
        enemyState = EnemyState::Idle;
        velocity.x = 0;
        invulnerable = false;
        anim_.play("idle");
    } else if (stateName == "prep") {
        enemyState = EnemyState::Peek;
        velocity.x = 0;
        invulnerable = false;
        anim_.play("peek");
    } else if (stateName == "fire" || stateName == "shoot") {
        enemyState = EnemyState::Shoot;
        velocity.x = 0;
        anim_.play("shoot");
        if (enteredState && target_) {
            float dir = dirToTarget();
            facingRight = (dir > 0);
            PendingShot shot{};
            shot.x = position.x + dir * 12.0f;
            shot.y = position.y + 8.0f;
            shot.vx = dir * 2.5f;
            shot.vy = 0.0f;
            pendingShots.push_back(std::move(shot));
        }
    } else if (stateName == "patrol") {
        enemyState = EnemyState::Patrol;
        invulnerable = false;
        anim_.play("walk");
        updatePatrol();
    } else if (stateName == "fly") {
        anim_.play("idle");
        updateFlyPattern();
    } else if (stateName == "hide") {
        enemyState = EnemyState::Hide;
        velocity.x = 0;
        invulnerable = true;
        anim_.play("hide");
    } else if (stateName == "peek") {
        enemyState = EnemyState::Peek;
        velocity.x = 0;
        invulnerable = false;
        anim_.play("peek");
    }
}

} // namespace mmx
