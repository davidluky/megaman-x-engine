// player.cpp - runs player simulation, combat state, and progress binding.
// Owns: player movement, damage, upgrades, weapons, and persistent progress.

#include "entities/player.h"
#include "entities/player_anchor.h"
#include "entities/player_ground_movement.h"
#include "app/constants.h"
#include "systems/audio.h"
#include "data/difficulty.h"
#include <algorithm>
#include <cmath>

namespace mmx {

namespace {

constexpr int kLandingRecoveryFrames = 3;

PlayerProgress& fallbackPlayerProgress() {
    static PlayerProgress progress;
    return progress;
}

PlayerProgress* activePlayerProgress = &fallbackPlayerProgress();

PlayerProgress& activeProgress() {
    return *activePlayerProgress;
}

} // namespace

Player::Player() {
    maxHealth = progress().maxHealth;
}

void Player::bindProgress(PlayerProgress& newProgress) {
    activePlayerProgress = &newProgress;
}

void Player::useFallbackProgress() {
    activePlayerProgress = &fallbackPlayerProgress();
}

PlayerProgress& Player::progress() {
    return activeProgress();
}

PlayerProgress& Player::progressState() {
    return Player::progress();
}

const PlayerProgress& Player::progressState() const {
    return Player::progress();
}
void Player::markPickupCollected(const std::string& id) {
    if (!isPickupCollected(id)) {
        progress().collectedPickups.push_back(id);
    }
}

bool Player::isPickupCollected(const std::string& id) {
    for (const auto& collectedId : progress().collectedPickups) {
        if (collectedId == id) return true;
    }
    return false;
}

PlayerProgress Player::captureProgress() {
    return progress();
}

void Player::applyProgress(const PlayerProgress& newProgress) {
    progress() = newProgress;
}
void Player::resetPersistentState() {
    Player::applyProgress(PlayerProgress{});
}

void Player::markPickupCollectedInProgress(const std::string& id) {
    markPickupCollected(id);
}

bool Player::isPickupCollectedInProgress(const std::string& id) const {
    return isPickupCollected(id);
}

void Player::setArmorProgress(bool boots, bool helmet, bool body, bool buster) {
    auto& state = progress();
    state.armorBoots = boots;
    state.armorHelmet = helmet;
    state.armorBody = body;
    state.armorBuster = buster;
    applyArmorState();
}

void Player::grantArmorBoots() {
    progress().armorBoots = true;
    applyArmorState();
}

void Player::grantArmorHelmet() {
    progress().armorHelmet = true;
    applyArmorState();
}

void Player::grantArmorBody() {
    progress().armorBody = true;
    applyArmorState();
}

void Player::grantArmorBuster() {
    progress().armorBuster = true;
    applyArmorState();
}

void Player::applyArmorState() {
    const auto& state = progress();
    hasBoots = cfgForceBoots_ || state.armorBoots;   // legs capsule = ground dash
    hasAirDash = state.armorBoots;                   // engine-extra (X1 has none)
    hasHelmet = state.armorHelmet;
    hasArmor = state.armorBody;
    hasBusterUpgrade = state.armorBuster;
    refreshArmorSheet();
}

void Player::pollInput() {
    // Input::poll() is called globally by the game loop. We just read the
    // buffered press states here and apply them to our internal timers.
    if (Input::isJumpPressed()) {
        jumpBufferTimer_ = jumpBufferFrames;
    }
    if (Input::isDashPressed()) {
        dashBufferTimer_ = jumpBufferFrames;
    }
    if (Input::isShootReleased()) {
        inputShootReleased_ = true;
    }
    processWeaponCycleInput();
}

void Player::processWeaponCycleInput() {
    // Weapon cycling uses Input actions; InputBindings owns the current keys/buttons.
    // As in the real game, you can't switch weapons while a special-weapon shot
    // is still on screen — the press is consumed but the swap is blocked.
    const bool forceNext = Input::isWeaponNextForced();
    const bool prev = Input::isWeaponPrevPressed();
    const bool next = Input::isWeaponNextPressed();
    if (!prev && !next && !forceNext) return;

    if (prev) Input::consumeWeaponPrevPress();
    if (next) Input::consumeWeaponNextPress();
    if (forceNext) Input::consumeWeaponNextForce();

    // With only the buster (no Maverick weapons acquired), L/R does nothing
    // in the real game — no switch, no sound.
    if (weaponInventory.weaponCount() <= 1) {
        return;
    }

    // forceNext bypasses active-shot check — used by the weapons autotest to
    // ensure cycling completes even when a previous shot is still on screen.
    if (!forceNext && hasActiveSpecialShot()) {
        return;  // swap silently blocked, as in the real game
    }

    // U68 MEASURED: switching away from Rolling Shield with the charged
    // shield live POPS the shield (real game allowed the switch and the
    // shield slot died — knowledge_base/_rolling_shield_runs/u68_switch).
    if (projectiles) {
        for (auto& p : *projectiles) {
            if (p.active && p.isPlayerShot && p.absorbShield) p.active = false;
        }
    }

    if (prev) weaponInventory.cyclePrev();
    else weaponInventory.cycleNext();
    AudioManager::playSFX(SFX::WeaponSwitch);
}

bool Player::hasActiveSpecialShot() const {
    if (!projectiles) return false;
    for (const auto& p : *projectiles) {
        if (p.active && p.isPlayerShot &&
            p.visualStyle != ProjectileVisualStyle::BusterSprite &&
            p.visualStyle != ProjectileVisualStyle::BusterCharge1 &&
            // U68 MEASURED (u68_switch verdict): the charged Rolling Shield
            // does NOT block the menu switch in the real game — switching is
            // allowed and POPS the shield (processWeaponCycleInput does the
            // popping).
            !p.absorbShield) {
            return true;
        }
    }
    return false;
}

void Player::handleInput() {
    // Read held state from the unified input system
    inputLeft_      = Input::isLeftHeld();
    inputRight_     = Input::isRightHeld();
    inputUp_        = Input::isUpHeld();
    inputDown_      = Input::isDownHeld();
    inputJumpHeld_  = Input::isJumpHeld();
    inputDashHeld_  = Input::isDashHeld();
    inputShootHeld_ = Input::isShootHeld();

    // Physics-tick pickup of presses/releases. pollInput() runs at render
    // rate and misses scripted autotest events because those are set AND
    // cleared entirely inside one physics tick. Latching the same flags here
    // catches both keyboard + scripted paths.
    if (Input::isJumpPressed()) jumpBufferTimer_ = jumpBufferFrames;
    if (Input::isDashPressed()) dashBufferTimer_ = jumpBufferFrames;
    if (Input::isShootPressed()) inputShootPressed_ = true;
    if (Input::isShootReleased()) inputShootReleased_ = true;
    processWeaponCycleInput();
}

void Player::update(float /*dt*/) {
    sourceObjectDepartureFrame_ = false;
    hurtEntryFrame_ = false;
    // Decrement timers
    if (coyoteTimer_ > 0) coyoteTimer_--;
    if (jumpBufferTimer_ > 0) jumpBufferTimer_--;
    if (dashBufferTimer_ > 0) dashBufferTimer_--;
    if (wallJumpLockout_ > 0) wallJumpLockout_--;
    if (wallDropPoseTimer_ > 0) wallDropPoseTimer_--;
    if (iframeTimer_ > 0) iframeTimer_--;
    if (landingAnimTimer_ > 0) landingAnimTimer_--;
    tickStingInvincibility();

    if (shotCooldown_ > 0) shotCooldown_--;
    if (shootAnimTimer_ > 0) shootAnimTimer_--;

    if (updateHurtTransition()) return;

    // CP-B1C-T1-M2: scripted boss-entry walk — the scene owns position.x;
    // only the run-animation clock advances here.
    if (scriptedEntryWalk_) {
        prevPosition = position;
        facingRight = true;
        velocity = {0.0f, 0.0f};
        anim_.tick();
        return;
    }

    if (wallJumpResumePending_) {
        // The preceding handoff held position through external gravity. Resume
        // its saved arc before ordinary Jump applies the current B input.
        if (!onGround && !onCeiling) velocity.y = wallJumpResumeY_;
        wallJumpResumePending_ = false;
    }

    // R4.4: spend the ground-launch delay. The launch frame does not also run
    // the new state's update -- exactly as the old immediate launch did not,
    // because it returned from the state it launched out of.
    bool launchedThisFrame = false;
    if (jumpLaunchDelay_ > 0 && --jumpLaunchDelay_ == 0) {
        fireGroundJump();
        launchedThisFrame = true;
    }

    if (!launchedThisFrame) {
        switch (state_) {
            case PlayerState::Idle:      updateIdle(); break;
            case PlayerState::Run:       updateRun(); break;
            case PlayerState::Jump:      updateJump(); break;
            case PlayerState::Fall:      updateFall(); break;
            case PlayerState::WallSlide: updateWallSlide(); break;
            case PlayerState::WallJump:  updateWallJump(); break;
            case PlayerState::Dash:      updateDash(); break;
            case PlayerState::DashJump:  updateDashJump(); break;
            case PlayerState::Ladder:    updateLadder(); break;
            case PlayerState::Hurt:      updateHurt(); break;
            case PlayerState::Die:       updateDie(); break;
        }
    }
    if (hurtEntryFrame_) return;
    sourceObjectDeparturePending_ = false;
#ifdef WALL_DEBUG_LOG
    if (state_ == PlayerState::Fall || state_ == PlayerState::WallSlide ||
        state_ == PlayerState::WallJump || state_ == PlayerState::Jump) {
        TraceLog(LOG_INFO, "WDBG st=%d face=%d vy=%.2f vx=%.2f tR=%d tL=%d wSide=%d",
                 (int)state_, facingRight ? 1 : 0, velocity.y, velocity.x,
                 touchingWallRight ? 1 : 0, touchingWallLeft ? 1 : 0, wallSide_);
    }
#endif

    // Combat runs in parallel with movement (can shoot in any state except hurt/die)
    if (state_ != PlayerState::Hurt && state_ != PlayerState::Die) {
        updateCombat();
    }

    // Shoot overlay: while shootAnimTimer_ is counting down, swap to the
    // `_shoot` variant of the current state anim so X visibly holds the
    // buster out. Each state maps to one; hurt/die keep their own anim.
    const char* wanted = nullptr;
    if (landingAnimTimer_ > 0 &&
        (state_ == PlayerState::Idle || state_ == PlayerState::Run)) {
        wanted = (shootAnimTimer_ > 0) ? "land_shoot" : "land";
    } else if (shootAnimTimer_ > 0) {
        switch (state_) {
            case PlayerState::Idle:      wanted = "shoot"; break;
            case PlayerState::Run:       wanted = "walk_shoot"; break;
            case PlayerState::WallJump:
                wanted = wallJumpWindup_ == 6 ? "wall_shoot" : "wall_kick_shoot";
                break;
            case PlayerState::Jump:
            case PlayerState::DashJump:  wanted = velocity.y >= 0.0f ? "fall_shoot" : "jump_shoot"; break;
            case PlayerState::Fall:      wanted = wallDropPoseTimer_ > 0 ? "wall_shoot" : "fall_shoot"; break;
            case PlayerState::WallSlide: wanted = "wall_shoot"; break;
            case PlayerState::Dash:      wanted = "dash_shoot"; break;
            case PlayerState::Ladder:    wanted = "wall_shoot"; break;
            default: break;
        }
    } else {
        // Return to the non-shoot anim when the timer expires.
        const bool lowHealth = health <= progressState().maxHealth / 4;  // heavy-breathing idle
        switch (state_) {
            case PlayerState::Idle:      wanted = lowHealth ? "idle_low" : "idle"; break;
            case PlayerState::Run:       wanted = "run"; break;
            case PlayerState::WallJump:
                wanted = wallJumpWindup_ == 6 ? "wall" : "wall_kick";
                break;
            case PlayerState::Jump:
            case PlayerState::DashJump:
                wanted = velocity.y >= 0.0f ? "fall" : "jump";
                break;
            case PlayerState::Fall:      wanted = wallDropPoseTimer_ > 0 ? "wall" : "fall"; break;
            case PlayerState::WallSlide: wanted = "wall"; break;
            case PlayerState::Dash:      wanted = "dash"; break;
            case PlayerState::Ladder:    wanted = "wall"; break;
            default: break;
        }
    }
    if (wanted && anim_.currentName() != wanted) {
        const std::string& cur = anim_.currentName();
        const std::string want = wanted;
        // A movement anim and its "_shoot" variant are the same body motion with
        // the buster out. Toggling shoot must keep the cycle phase, otherwise the
        // motion snaps back to its first frame on every shot (the run/dash jerk).
        auto baseOf = [](const std::string& n) -> std::string {
            if (n == "walk_shoot") return "run";
            if (n == "dash_shoot") return "dash";
            if (n == "jump_shoot") return "jump";
            if (n == "fall_shoot") return "fall";
            if (n == "wall_shoot") return "wall";
            if (n == "wall_kick_shoot") return "wall_kick";
            return n;
        };
        if (baseOf(cur) == baseOf(want)) {
            anim_.playSynced(want);
        } else {
            anim_.play(want);
        }
    }

    // Idle eye-blink cadence (visual only). Only ticks while standing in the
    // plain idle pose — not the low-health heavy-breathing idle, not while
    // moving/shooting. Mirrors the real game: shut ~7 frames every ~55-95
    // frames, ~25% of the time followed by a quick double-blink.
    if (anim_.currentName() == "idle") {
        if (blinkCloseTimer_ > 0) {
            if (--blinkCloseTimer_ == 0 && blinkPendingDouble_) {
                blinkPendingDouble_ = false;
                blinkCountdown_ = 8;  // quick second blink
            }
        } else if (--blinkCountdown_ <= 0) {
            blinkCloseTimer_ = 8;  // measured fully-closed window (frames 82-89)
            blinkRng_ = blinkRng_ * 1664525u + 1013904223u;
            blinkPendingDouble_ = ((blinkRng_ >> 24) & 3u) == 0u;     // ~25%
            blinkCountdown_ = 55 + static_cast<int>((blinkRng_ >> 8) % 41u);
        }
    } else {
        blinkCloseTimer_ = 0;
        blinkPendingDouble_ = false;
        if (blinkCountdown_ < 20) blinkCountdown_ = 40;  // re-arm on return to idle
    }

    anim_.tick();
}

void Player::beginScriptedEntryWalk() {
    resetJumpState();
    scriptedEntryWalk_ = true;
    facingRight = true;
    velocity = {0.0f, 0.0f};
    changeState(PlayerState::Run);
}

void Player::endScriptedEntryWalk() {
    if (!scriptedEntryWalk_) return;
    resetJumpState();
    scriptedEntryWalk_ = false;
    changeState(PlayerState::Idle);
}

void Player::forceIdlePose() {
    changeState(PlayerState::Idle);
    if (anim_.currentName() != "idle") {
        anim_.play("idle");
    }
}

std::optional<SourceContactProfile> Player::sourceContactProfile() const {
    // These engine paths have no measured X1 selector. The actual air-dash
    // flag survives state changes until the existing landing/reset logic.
    if (scriptedEntryWalk_ || usedAirDash_) return std::nullopt;

    switch (state_) {
        case PlayerState::Dash:
            // Source switches BB38 to A552 before the ground jump rises;
            // this armed native frame still uses the Dash movement state.
            return jumpLaunchDelay_ > 0 ? SourceContactProfile::NormalA552
                                        : SourceContactProfile::ActiveDashBB38;
        case PlayerState::Idle:
        case PlayerState::Run:
        case PlayerState::Jump:
        case PlayerState::Fall:
        case PlayerState::WallSlide:
        case PlayerState::WallJump:
        case PlayerState::DashJump:
        case PlayerState::Ladder:
        case PlayerState::Hurt:
        case PlayerState::Die:
            return SourceContactProfile::NormalA552;
    }
    return std::nullopt;
}

std::optional<float> Player::sourceGroundSpeed() const {
    if (!sourceGroundMovementEnabled_ || !tilemap_ || !onGround ||
        (state_ != PlayerState::Idle && state_ != PlayerState::Run)) {
        return std::nullopt;
    }
    const auto profile = sourceContactProfile();
    if (!profile || *profile != SourceContactProfile::NormalA552) {
        return std::nullopt;
    }

    const AABB box = getHitbox();
    const float feetX = box.left() + box.w * 0.5f;
    const float feetY = box.bottom();
    const int rawAttr = player_ground_movement::supportAttribute(*tilemap_, feetX, feetY);
    // The first moving restart reaches $81996A after $819934 has seeded
    // signed walk speed. R137 observes 0 -> -376 -> -408FP on raw 0x08.
    // applyHorizontalInput still owns the preceding stationary/ramp frames.
    const float selectorVelocity = velocity.x != 0.0f
        ? velocity.x : (facingRight ? runSpeed : -runSpeed);
    return player_ground_movement::speedMagnitudeForSupport(rawAttr, selectorVelocity);
}

void Player::changeState(PlayerState newState) {
    if (state_ == newState) return;
    const PlayerState oldState = state_;
    if (newState != PlayerState::Hurt) {
        hurtInitPending_ = false;
        hurtRecoveryPending_ = false;
    }
    if (newState != PlayerState::Jump && newState != PlayerState::Fall) {
        sourceJumpSpeed_.reset();
    }
    wallJumpResumePending_ = false;
    wallJumpWindup_ = 0;
    wallJumpDashed_ = false;
    if (newState == PlayerState::Hurt || newState == PlayerState::Die) {
        // A collision after the press frame overrides its queued launch.
        jumpLaunchDelay_ = 0;
        pendingDashJump_ = false;
        wallJumpLockout_ = 0;
    }
    state_ = newState;
    if (newState != PlayerState::Fall) {
        wallDropPoseTimer_ = 0;
    }

    switch (newState) {
        case PlayerState::Idle:
            if (oldState == PlayerState::Fall || oldState == PlayerState::Jump ||
                oldState == PlayerState::WallJump || oldState == PlayerState::DashJump) {
                AudioManager::playApu(0x07);
                landingAnimTimer_ = kLandingRecoveryFrames;
            }
            anim_.play("idle");
            break;
        case PlayerState::Run:
            if (oldState == PlayerState::Fall || oldState == PlayerState::Jump ||
                oldState == PlayerState::WallJump || oldState == PlayerState::DashJump) {
                AudioManager::playApu(0x07);
                landingAnimTimer_ = kLandingRecoveryFrames;
                anim_.play("run");                       // landing w/ momentum: straight to stride
            } else {
                anim_.play("run");
            }
            break;
        case PlayerState::Jump:
            AudioManager::playSFX(SFX::Jump);
            anim_.play("jump");
            break;
        case PlayerState::Fall:      anim_.play("fall"); break;
        case PlayerState::WallSlide:
            AudioManager::playSFX(SFX::WallSlide);
            if (touchingWallRight) wallSide_ = 1;
            else if (touchingWallLeft) wallSide_ = -1;
            facingRight = (wallSide_ < 0);
            anim_.play("wall");
            break;
        case PlayerState::WallJump:
            AudioManager::playSFX(SFX::Jump);
            // Keep the B-press row's wall pose; the shared selector starts
            // the authored kick on the next preparation update (R299).
            break;
        case PlayerState::Dash:
            AudioManager::playSFX(SFX::Dash);
            anim_.play("dash");
            break;
        case PlayerState::DashJump:
            // T1.7 (Storm Eagle f2112): the rise-end zero fires once per
            // dash-jump. fireGroundJump writes the launch speed before this
            // call, so a launch arms it; an air dash entered while already
            // falling has no rise left to end.
            dashJumpRiseEnded_ = velocity.y > 0.0f;
            // SE f2207/f7373/f7760 (2026-09-16): a dash-jump ends a running walk start,
            // so X lands walking 376/256; a closed one (-1) is unmeasured, left alone.
            if (walkStartTick_ >= 0) { walkStartTick_ = kWalkStartFrames; walkStartRamping_ = false; }
            AudioManager::playSFX(SFX::Jump);
            anim_.play("jump");
            break;
        case PlayerState::Ladder:
            velocity = {0.0f, 0.0f};
            anim_.play("wall");
            break;
        case PlayerState::Hurt:
            if (!hurtInitPending_) anim_.play("hurt");
            break;
        case PlayerState::Die:
            AudioManager::playSFX(SFX::PlayerDeath);
            spawnDeathBurst();
            anim_.play("die");
            break;
    }
}

// Chameleon-sting charged-state visual schedule (U03b, oracle s7b_cgram:
// per-frame CGRAM row 9 + per-frame OAM across the whole 480f state).
// d = frames since release. Hidden chain measured d4 → d481 (184 frames):
// 40 every-3rd, 120 every-2nd (peak flicker), 24 every-5th (wind-down).
// ============================================================================
// Helpers
// ============================================================================

void Player::applyHorizontalInput() {
    const bool right = inputRight_ && !inputLeft_;
    const bool left = inputLeft_ && !inputRight_;

    // Source recovery: two frames after release, one after the held cap. The
    // walk that follows does NOT ramp -- either hold is under R4.1's
    // measured threshold of ten -- so the walk start is left closed.
    if (dashRecoveryTimer_ > 0) {
        if (onGround) {
            --dashRecoveryTimer_;
            walkStillFrames_ = 0;
            walkStartTick_ = kWalkStartFrames;
            walkStartRamping_ = false;
            if (right) facingRight = true;
            else if (left) facingRight = false;
            velocity.x = 0;
            return;
        }
        dashRecoveryTimer_ = 0;  // airborne recovery is not measured
    }

    if (!right && !left) {
        walkStartTick_ = -1;
        walkStartRamping_ = false;
        walkStillFrames_ = onGround ? walkStillFrames_ + 1 : 0;
        velocity.x = 0;
        return;
    }

    facingRight = right;

    // R4.1 idle starts ramp; landing dispatch bypasses idle (CP2047).
    // Provenance: player/landing_walk_dispatch_2026-09-17.json.
    if (walkStartTick_ < 0) {
        const bool landingWalk = onGround && landingAnimTimer_ > 0;
        walkStartTick_ = landingWalk ? kWalkStartFrames : 0;
        walkStartRamping_ = !landingWalk && onGround && walkStillFrames_ >= kWalkStartRampStill;
    }
    walkStillFrames_ = 0;

    // T1.7: the ramp is a GROUND law. R4.1 measured it on Chill Penguin's
    // floor and the first-held-frame zero above is already gated on onGround;
    // Storm Eagle source frame 1383 (knowledge_base/_movies/storm-eagle,
    // player.csv + _movie/Input.txt) has X airborne on the ramp's fifth frame,
    // right held, advancing 2 px while the engine held him at 0.
    const bool walkStartRampActive =
        onGround && walkStartRamping_ && walkStartTick_ < kWalkStartFrames;
    float speed = runSpeed;
    if (onGround && walkStartTick_ == 0) {
        speed = 0.0f;  // measured: the first held frame does not move
    } else if (walkStartRampActive) {
        speed = (walkStartTick_ == kWalkStartFrames - 1) ? 0.0f : kWalkStartRampSpeed;
    }
    if (walkStartTick_ < kWalkStartFrames) ++walkStartTick_;

    // Preserve every measured walk-start zero/ramp frame. Source support
    // selection begins only after that policy is closed. Existing movement
    // uses its previous sign; a moving restart first seeds action speed.
    if (speed != 0.0f && !walkStartRampActive) {
        if (const auto sourceSpeed = sourceGroundSpeed()) {
            speed = *sourceSpeed;
        }
    }
    // $819982 copies the retained Walk-jump magnitude with the current side
    // input sign. The neutral branch above clears effective velocity only.
    // T1.7: a retained magnitude of ZERO is not a magnitude the source copies.
    // Storm Eagle f1477 launches with the direction released on the press
    // frame, and the capture build/t17-air (f1477..f1505) has X holding x for
    // thirteen airborne frames and then stepping a flat -376/256 -- the walk
    // speed, no ramp -- from f1491, the first frame that reads `left`.
    if (!onGround && sourceJumpSpeed_ && *sourceJumpSpeed_ != 0.0f &&
        (state_ == PlayerState::Jump || state_ == PlayerState::Fall)) {
        speed = *sourceJumpSpeed_;
    }
    velocity.x = right ? speed : -speed;
}

void Player::applyHorizontalInputWithLockout() {
    // During wall-jump lockout, the player can't move back toward the wall
    // they kicked off from. This prevents instant re-grab and forces
    // commitment to the jump direction.
    if (wallJumpLockout_ > 0) {
        const float speed = wallJumpDashed_ ? dashSpeed : wallJumpVelocityX;
        const float kickSpeed = wallJumpDirection_ * speed;

        // Allow input AWAY from the wall (same direction as kick or neutral)
        // Block input TOWARD the wall (opposite to kick direction)
        bool inputTowardWall = (wallJumpDirection_ > 0 && inputLeft_) ||
                               (wallJumpDirection_ < 0 && inputRight_);

        if (inputTowardWall) {
            // Ignore it — maintain kick-off momentum
            velocity.x = kickSpeed;
        } else if ((wallJumpDirection_ > 0 && inputRight_) ||
                   (wallJumpDirection_ < 0 && inputLeft_)) {
            // R304 away-input control retains all seven fast steps too.
            if (wallJumpDashed_) velocity.x = kickSpeed;
            else applyHorizontalInput();
        } else {
            // No input — maintain kick-off momentum
            velocity.x = kickSpeed;
        }
    } else {
        applyHorizontalInput();
    }
}

bool Player::canCoyoteJump() const {
    return coyoteTimer_ > 0;
}

bool Player::isTouchingWallToward() const {
    return (touchingWallRight && inputRight_) || (touchingWallLeft && inputLeft_);
}

bool Player::tryDash() {
    if (!hasBoots) return false;
    if (dashBufferTimer_ <= 0) return false;

    // Air dash requires the boots capsule upgrade
    if (!onGround && !hasAirDash) return false;

    // Only one air dash per jump — reset when landing
    if (!onGround && usedAirDash_) return false;

    dashBufferTimer_ = 0;
    dashTimer_ = dashDuration;
    dashReleased_ = false;
    dashWallLimited_ = false;
    dashRecoveryTimer_ = 0;
    velocity.x = facingRight ? dashSpeed : -dashSpeed;

    if (onGround) {
        velocity.y = 0;
        changeState(PlayerState::Dash);
    } else {
        // Air dash: maintain current vertical velocity (no vertical reset)
        usedAirDash_ = true;
        changeState(PlayerState::DashJump);
    }
    return true;
}

bool Player::tryDashJump() {
    // Simultaneous dash+jump input → go directly to DashJump.
    // Without this, pressing Z slightly before X gives a normal jump
    // because tryJump runs before tryDash on the next frame.
    if (!hasBoots) return false;
    if (!onGround) return false;
    if (dashBufferTimer_ <= 0 && !inputDashHeld_) return false;
    if (jumpBufferTimer_ <= 0 && !inputJumpHeld_) return false;

    dashBufferTimer_ = 0;
    return armGroundJump(true);   // R4.4
}

bool Player::isTouchingLadder() const {
    if (!tilemap_) return false;
    const int ts = tilemap_->tileSize();
    if (ts <= 0) return false;

    const AABB hb = getHitbox();
    const float centerX = hb.left() + hb.w * 0.5f;
    const int centerCol = static_cast<int>(std::floor(centerX / static_cast<float>(ts)));
    const int topRow = static_cast<int>(std::floor((hb.top() + 2.0f) / static_cast<float>(ts)));
    const int bottomRow = static_cast<int>(std::floor((hb.bottom() - 2.0f) / static_cast<float>(ts)));

    for (int row = topRow; row <= bottomRow; ++row) {
        if (tilemap_->getTileType(centerCol, row) == TileType::Ladder) {
            return true;
        }
    }
    return false;
}

bool Player::tryLadder() {
    if (!inputUp_ && !inputDown_) return false;
    if (!isTouchingLadder()) return false;

    dashBufferTimer_ = 0;
    jumpBufferTimer_ = 0;
    velocity = {0.0f, 0.0f};
    onGround = false;
    changeState(PlayerState::Ladder);
    return true;
}

void Player::updateLadder() {
    if (!isTouchingLadder()) {
        changeState(onGround ? PlayerState::Idle : PlayerState::Fall);
        return;
    }

    velocity.x = 0.0f;
    float desiredY = 0.0f;
    if (inputUp_ && !inputDown_) {
        desiredY = -ladderClimbSpeed;
    } else if (inputDown_ && !inputUp_) {
        desiredY = ladderClimbSpeed;
    }
    velocity.y = desiredY - gravity;

    if (jumpBufferTimer_ > 0) {
        jumpBufferTimer_ = 0;
        velocity.y = jumpVelocity;
        changeState(PlayerState::Jump);
    }
}

// ============================================================================
// State implementations
// ============================================================================

void Player::updateIdle() {
    applyHorizontalInput();

    if (tryLadder()) return;

    // If both dash and jump are buffered simultaneously, go directly to dash-jump.
    // This catches the case where the player presses both keys at once or in
    // rapid succession — without this, the ordering (dash checked before jump)
    // means pressing Z slightly before X gives a normal jump instead.
    if (tryDashJump()) return;
    if (tryDash()) return;
    if (velocity.x != 0 || walkStartActive()) { changeState(PlayerState::Run); return; }
    if (tryJump()) return;

    if (!onGround) {
        coyoteTimer_ = coyoteFrames;
        changeState(PlayerState::Fall);
    }
}

void Player::updateRun() {
    const bool objectDeparture = sourceObjectDeparturePending_ && !onGround;
    if (!objectDeparture) applyHorizontalInput();

    if (tryLadder()) return;

    if (tryDashJump()) return;
    if (tryDash()) return;
    // SE f1477 (build/t17-idle): releasing right with B launches before Idle.
    if (tryJump()) return;
    if (objectDeparture) {
        changeState(PlayerState::Fall);
        sourceObjectDepartureFrame_ = true;
        return;
    }
    if (velocity.x == 0 && !walkStartActive()) { changeState(PlayerState::Idle); return; }

    if (!onGround) {
        coyoteTimer_ = coyoteFrames;
        changeState(PlayerState::Fall);
    }
}

void Player::updateJump() {
    applyHorizontalInputWithLockout();

    if (tryLadder()) return;

    // R36 source KB: ground_jump_release.json measures a rising B release as
    // zero Y velocity before moveAndCollide applies gravity. T1.7 (Chill
    // Penguin f1551 -> f1552, build/t17-cp2) measures the same zero when the
    // rise has already turned over: the release is read at +45/256 and the
    // source's next displacement is +64/256, the bare gravity step, not
    // +109/256. The sign is not part of the law; the Fall transition below
    // keeps the zero from firing more than once per jump.
    if (!inputJumpHeld_) {
        velocity.y = 0.0f;
    }

    // Air dash during jump
    // R287: a successful transition owns this tick; the wall check below
    // otherwise overwrites DashJump before it can update.
    if (hasAirDash && tryDash()) return;

    // Wall slide: touching wall + holding toward it + falling or ascending slowly
    if (isTouchingWallToward() && wallJumpLockout_ <= 0) {
        changeState(PlayerState::WallSlide);
        return;
    }

    if (velocity.y >= 0) { changeState(PlayerState::Fall); return; }
    if (onCeiling) { velocity.y = 0; changeState(PlayerState::Fall); return; }
}

void Player::updateFall() {
    applyHorizontalInputWithLockout();

    if (tryLadder()) return;

    // Air dash — check before landing so input isn't consumed by ground dash
    if (!onGround && hasAirDash && tryDash()) return;

    // Land
    if (onGround) {
        usedAirDash_ = false; // Reset air dash on landing
        if (tryJump()) return;
        if (tryDash()) return;
        changeState((velocity.x != 0 || walkStartActive()) ? PlayerState::Run
                                                             : PlayerState::Idle);
        return;
    }

    // Coyote jump
    if (tryJump()) return;

    // Wall slide: touching wall + holding toward it
    if (isTouchingWallToward() && wallJumpLockout_ <= 0) {
        changeState(PlayerState::WallSlide);
        return;
    }
}

void Player::updateDash() {
    if (tryLadder()) return;

    velocity.x = facingRight ? dashSpeed : -dashSpeed;
    velocity.y = 0; // No gravity during dash

    dashTimer_--;

    // R4.3 measured early release; R111's uninterrupted 40-frame source
    // capture also proves a cap. Source integrates for timer $20 through $00
    // before DEC. tryDash already integrates the launch frame, so the native
    // duration is 33 under this decrement-before-movement update order.
    if (onGround && !inputDashHeld_ && dashTimer_ > 0) {
        dashTimer_ = 0;
        dashReleased_ = true;
    }

    // Dash-jump: jump during dash preserves dash speed.
    // Accept BOTH the jump buffer AND the held jump key as triggers.
    // The held-key fallback catches cases where the buffer was consumed
    // or where keyboard ghosting prevented IsKeyPressed from firing.
    if (jumpBufferTimer_ > 0 || inputJumpHeld_) {
        armGroundJump(true);   // R4.4: fires on the next frame
        return;
    }

    // Dash ended
    if (dashTimer_ <= 0) {
        if (dashReleased_) {
            dashRecoveryTimer_ = kDashReleaseRecoveryFrames;
            dashReleased_ = false;
        } else if (onGround && inputDashHeld_ && !dashWallLimited_) {
            dashRecoveryTimer_ = kDashCapRecoveryFrames;
        }
        applyHorizontalInput();
        changeState((velocity.x != 0 || walkStartActive()) ? PlayerState::Run
                                                             : PlayerState::Idle);
        return;
    }

    // Hit a wall — don't cancel immediately. Cap remaining dash time so the
    // player still has a few frames to press jump for a dash-jump. Without this,
    // dashing into a wall cancels after 1 frame (too fast to react). BUG-005.
    if ((facingRight && touchingWallRight) || (!facingRight && touchingWallLeft)) {
        if (dashTimer_ > 4) {
            dashTimer_ = 4;
            dashWallLimited_ = true;
        }
        // Don't exit dash — let timer/jump checks handle it naturally
    }

    // Ran off edge during dash
    if (!onGround) {
        changeState(PlayerState::Fall);
        return;
    }
}

void Player::updateDashJump() {
    if (tryLadder()) return;

    // Directional input uses dash speed throughout the dash-jump arc.
    if (inputRight_) {
        velocity.x = dashSpeed;
        facingRight = true;
    } else if (inputLeft_) {
        velocity.x = -dashSpeed;
        facingRight = false;
    } else {
        // dashjump_release.json: without direction, X stops even while A,
        // B or both are held; measured during ascent and descent.
        velocity.x = 0.0f;
    }

    // R32 source KB: dashjump_release.json measures a rising B release as
    // zero Y velocity before moveAndCollide applies gravity. T1.7 (Storm
    // Eagle f2111 -> f2112, capture build/t17-se3) measures the other way a
    // dash-jump's rise can end: with A, B and left held for the whole arc,
    // the source's +45/256 is followed by +64/256 -- one bare gravity step
    // from zero, not +109/256 -- and $7E0BBC turns 196 -> 209 on that frame.
    // Either way the rise ends ONCE: from f2112 the steps are 64/256 again
    // (+128, +192, +256), so the zero must not fire on every falling frame.
    const bool riseTurnedOver = velocity.y > 0.0f;
    const bool riseReleased = !inputJumpHeld_ && velocity.y < 0.0f;
    if (!dashJumpRiseEnded_ && (riseTurnedOver || riseReleased)) {
        velocity.y = 0.0f;
        dashJumpRiseEnded_ = true;
    }

    // Wall slide — only grab walls when falling, not during ascent.
    // Without this, dash-jumping near a wall instantly grabs it before
    // the player even leaves the ground. BUG-005.
    if (velocity.y >= 0 && isTouchingWallToward()) {
        changeState(PlayerState::WallSlide);
        return;
    }

    // Landed — dash-jump ends, return to normal speed
    if (onGround) {
        applyHorizontalInput(); // Snap back to run speed
        if (tryDash()) return;
        changeState((velocity.x != 0 || walkStartActive()) ? PlayerState::Run
                                                             : PlayerState::Idle);
        return;
    }

    // The shared animation selector owns ascent/descent, including shooting.
    // Replaying fall here every tick would keep restarting its first pose.
    if (onCeiling) velocity.y = 0;

}

// ============================================================================
// Combat system — runs parallel to movement states
// ============================================================================

void Player::updateCombat() {
    if (progressState().armorBuster && !hasBusterUpgrade) hasBusterUpgrade = true;
    const bool busterUpgradeActive = hasBusterUpgrade || progressState().armorBuster;
    const bool specialChargeActive = busterUpgradeActive || debugSpecialChargeUnlocked_;

    // Charge system: hold shoot to build charge, release to fire.
    // This runs every tick regardless of movement state.
    if (inputShootHeld_) {
        chargeTimer_++;
#ifdef WALL_DEBUG_LOG
        TraceLog(LOG_INFO, "CHG timer=%d level=%d held=%d rel=%d", chargeTimer_, chargeLevel_, inputShootHeld_?1:0, inputShootReleased_?1:0);
#endif
        int prevLevel = chargeLevel_;
        // Special weapons cannot charge at all without the arm upgrade
        // (real-game rule) — no aura, no level, release fires nothing extra.
        const bool canCharge = weaponInventory.isBuster() || specialChargeActive;
        if (canCharge) {
            if ((weaponInventory.isBuster() ? busterUpgradeActive : specialChargeActive)
                && chargeTimer_ >= chargeTime3) {
                chargeLevel_ = 3;
            } else if (chargeTimer_ >= chargeTime2) {
                chargeLevel_ = 2;
            } else if (chargeTimer_ >= chargeTime1) {
                chargeLevel_ = 1;
            }
        }
        (void)prevLevel;
    }

    // Charge whirr loops while charging and stops the moment the shot is
    // released (driven every frame so it never lingers past release).
    AudioManager::setChargeLoop(inputShootHeld_ && chargeLevel_ >= 1);

    // Real-game fire model (oracle shotgun-ice campaign 2026-06-09):
    //  - Normal shot fires ON PRESS (real pellet appears 1 frame after Y down).
    //  - Release fires the charged form at full charge.
    //  - An intermediate charge releases another NORMAL shot (measured: 120f
    //    hold released a 1-cost pellet; 240f hold released the sled).
    //  - Special weapons can only charge with the arm/buster upgrade, and
    //    their full-charge threshold (chargeTimeSpecial) is longer than the
    //    buster's.
    if (inputShootPressed_) {
        inputShootPressed_ = false;
        fireShot(ProjectileType::Normal);
    }

    // Fire Wave held stream (oracle hold60/s3_long 2026-06-11): one segment
    // per streamCadenceFrames while Y stays held — INCLUDING through a
    // charge hold. Re-emissions spend streamSubPerSegment sub-units; an
    // empty sub-counter buys streamSubUnits more with 1 whole unit.
    {
        const Weapon& cw = weaponInventory.current();
        if (!weaponInventory.isBuster() && cw.streamCadenceFrames > 0) {
            // Held-stream flame sound (U41): 0x61 at press+12 then every 16f
            // while held; a tap never reaches the first tick (measured
            // silent). Independent of the segment cadence.
            if (inputShootHeld_ && cw.sfxStreamHold >= 0
                && cw.sfxStreamHoldEvery > 0) {
                streamHoldSfxTimer_++;
                const int t = streamHoldSfxTimer_;
                if (t == cw.sfxStreamHoldFirst
                    || (t > cw.sfxStreamHoldFirst
                        && (t - cw.sfxStreamHoldFirst) % cw.sfxStreamHoldEvery == 0)) {
                    AudioManager::playApu(cw.sfxStreamHold);
                }
            } else if (!inputShootHeld_) {
                streamHoldSfxTimer_ = 0;
            }
            if (inputShootHeld_) {
                if (streamCooldown_ > 0) streamCooldown_--;
                if (streamCooldown_ <= 0) {
                    if (streamSubAmmo_ < cw.streamSubPerSegment
                        && weaponInventory.useAmmo(1)) {
                        streamSubAmmo_ += cw.streamSubUnits;
                    }
                    if (streamSubAmmo_ >= cw.streamSubPerSegment) {
                        streamSubAmmo_ -= cw.streamSubPerSegment;
                        streamReemit_ = true;
                        fireShot(ProjectileType::Normal);
                        streamReemit_ = false;
                        streamCooldown_ = cw.streamCadenceFrames;
                    }
                }
            } else {
                streamCooldown_ = 0;   // next press restarts the cadence
            }
        }
    }
    if (inputShootReleased_) {
#ifdef WALL_DEBUG_LOG
        TraceLog(LOG_INFO, "FIRE chargeLevel=%d chargeTimer=%d (t1=%d t2=%d)",
                 chargeLevel_, chargeTimer_, chargeTime1, chargeTime2);
#endif
        if (weaponInventory.isBuster()) {
            if (chargeLevel_ >= 3) {
                // Buster upgrade: spiral charge shot — unique projectile type
                fireShot(ProjectileType::ChargeL3);
            } else if (chargeLevel_ >= 2) {
                fireShot(ProjectileType::ChargeL2);
            } else if (chargeLevel_ >= 1) {
                fireShot(ProjectileType::ChargeL1);
            }
        } else if (specialChargeActive) {
            if (chargeTimer_ >= chargeTimeSpecial) {
                fireShot(ProjectileType::ChargeL2);
            } else if (chargeLevel_ >= 1) {
                // Sub-full release: a normal volley with the 0x17+fire-id
                // release pair (measured all 8 weapons, U41).
                subThresholdRelease_ = true;
                fireShot(ProjectileType::Normal);
                subThresholdRelease_ = false;
            }
        }
        // Specials without the arm upgrade: release fires nothing (the press
        // already fired the normal shot; no charge accumulates in real game).
        chargeTimer_ = 0;
        chargeLevel_ = 0;
        inputShootReleased_ = false;
    }

    // Quick tap detection: if shoot was pressed for just 1 frame (not held),
    // fire immediately. pollInput sets inputShootReleased_ on release.
}

// ============================================================================
// Damage system
// ============================================================================

void Player::respawnAt(Vector2 spawn, int invulnerableFrames, bool grounded) {
    resetJumpState();
    position = spawn;
    prevPosition = spawn;
    velocity = {0, 0};
    health = progressState().maxHealth;
    alive = true;

    inputLeft_ = false;
    inputRight_ = false;
    inputUp_ = false;
    inputDown_ = false;
    inputJumpHeld_ = false;
    inputDashHeld_ = false;
    inputShootHeld_ = false;
    inputShootReleased_ = false;

    dashTimer_ = 0;
    dashRecoveryTimer_ = 0;
    dashReleased_ = false;
    dashWallLimited_ = false;
    wallJumpLockout_ = 0;
    wallJumpDirection_ = 0;
    wallJumpWindup_ = 0;
    wallJumpResumePending_ = false;
    wallJumpResumeY_ = 0.0f;
    wallSide_ = 0;
    usedAirDash_ = false;
    landingAnimTimer_ = 0;

    chargeTimer_ = 0;
    chargeLevel_ = 0;
    shotCooldown_ = 0;
    shootAnimTimer_ = 0;
    hurtTimer_ = 0;
    hurtInitPending_ = false;
    hurtRecoveryPending_ = false;
    hurtEntryFrame_ = false;
    hurtKnockbackDirection_ = -1.0f;
    deathTimer_ = 0;
    deathNodes_.clear();
    iframeTimer_ = std::max(0, invulnerableFrames);

    onGround = grounded;
    wasOnGround = grounded;
    onCeiling = false;
    touchingWallLeft = false;
    touchingWallRight = false;
    if (grounded) {
        state_ = PlayerState::Idle;
        anim_.play("idle");
    } else {
        changeState(PlayerState::Fall);
    }
}

void Player::updateDie() {
    deathTimer_++;
    velocity = {0, 0};
    // Advance the radial death-burst nodes outward each frame. The gameplay
    // scene handles respawn/game-over logic by checking isDead() + deathTimer_.
    for (auto& n : deathNodes_) {
        n.x += n.vx;
        n.y += n.vy;
    }
}

void Player::spawnDeathBurst() {
    // Classic MMX death: X explodes into a ring of small light nodes that travel
    // outward at a constant speed in evenly spaced directions.
    deathNodes_.clear();
    const float cx = position.x + spriteWidth * 0.5f;
    const float cy = position.y + spriteHeight * 0.5f;
    constexpr int kNodeCount = 8;       // 8-way radial burst
    constexpr float kNodeSpeed = 2.4f;  // pixels/frame outward
    constexpr float kTau = 6.2831853f;  // 2*pi
    for (int i = 0; i < kNodeCount; ++i) {
        const float angle = (kTau / kNodeCount) * static_cast<float>(i);
        deathNodes_.push_back(
            {cx, cy, std::cos(angle) * kNodeSpeed, std::sin(angle) * kNodeSpeed});
    }
}

void Player::fireShot(ProjectileType type) {
    if (!projectiles) return;

    const Weapon& weapon = weaponInventory.current();
    bool isCharged = (type == ProjectileType::ChargeL1 || type == ProjectileType::ChargeL2 || type == ProjectileType::ChargeL3);
    // Fire Wave stream segments run their own cadence/caps (oracle: one per
    // 2f, up to 6 live — the 3-shot cap and press cooldown don't apply).
    const bool isStream = !isCharged && !weaponInventory.isBuster()
                          && weapon.streamCadenceFrames > 0;

    // Normal shots: max 3 on screen + cooldown
    if (type == ProjectileType::Normal) {
        if (isStream) {
            int live = 0;
            for (const auto& q : *projectiles)
                if (q.active && q.isPlayerShot && q.weaponId == weapon.id) live++;
            if (live >= 6) return;   // hold60 slot round-robin 0-5
        } else {
            if (countActiveShots() >= MAX_PLAYER_SHOTS) return;
            if (shotCooldown_ > 0) return;
            shotCooldown_ = SHOT_COOLDOWN;
        }
    }

    // Fire gate (Chameleon Sting s1_taps + Storm Tornado s2_wall, oracle
    // 2026-06-11): a NEW volley is BLOCKED — costing no ammo — while ANY
    // object of the previous one lives (sting taps every 40f fired only
    // every 80f; tornado taps every 40f fired only every ~120f, dry taps
    // left the ammo untouched). Strict any-object rule.
    // Dry-tap SFX unmeasured (S8) — gate before the fire sound.
    if (!isCharged && weapon.stingFireGate) {
        for (const auto& q : *projectiles) {
            if (q.active && q.isPlayerShot && q.weaponId == weapon.id) return;
        }
    }

    shootAnimTimer_ = 14;

    // Check ammo
    int ammoCost = isCharged ? weapon.chargedAmmoCost : weapon.ammoCost;
    // Homing-torpedo half-unit scheme ($7E1F87 bit7): normal shots alternate
    // cost 1, 0, 1, 0 — a set flag makes THIS shot free and clears.
    const bool halfScheme = !isCharged && weapon.ammoHalfAlternating;
    if (halfScheme && weaponInventory.halfFlagSet()) ammoCost = 0;
    // Fire Wave stream re-emissions already paid sub-units in updateCombat.
    if (isStream && streamReemit_) ammoCost = 0;
    if (!weaponInventory.useAmmo(ammoCost)) return;
    if (halfScheme) weaponInventory.toggleHalfFlag();

    // Measured APU SFX (U41 audit 2026-06-11, weapon.json "sfx" blocks +
    // _buster_runs/FINDINGS.md), played only once the shot actually spends
    // (the oracle measured shot-limit/ammo-blocked fires as SILENT). Normal
    // fire = the weapon's own command id (-1 = measured silent: fire-wave
    // segments; its held-stream flame cadence lives in updateCombat).
    // Charged release = the shared 0x17 at the release frame + a followup
    // ONE frame later: buster's is charge-LEVEL-specific (0x04/0x02/0x05),
    // specials use their full-charge followup; a sub-full release follows
    // 0x17 with the weapon's NORMAL FIRE id (measured for ALL 8 weapons,
    // _buster_runs/subthresh_cur*).
    if (isCharged) {
        AudioManager::playApu(weapon.sfxChargedRelease);
        int followup = weapon.sfxChargedFollowup;
        if (weaponInventory.isBuster()) {
            if (type == ProjectileType::ChargeL3)      followup = weapon.sfxChargedFollowupL3;
            else if (type == ProjectileType::ChargeL2) followup = weapon.sfxChargedFollowupL2;
            else                                       followup = weapon.sfxChargedFollowupL1;
        }
        AudioManager::playApuDelayed(followup, 1);
    } else if (subThresholdRelease_) {
        AudioManager::playApu(weapon.sfxChargedRelease);
        AudioManager::playApuDelayed(weapon.sfxFire, 1);
    } else {
        AudioManager::playApu(weapon.sfxFire);
    }
    if (isStream && !streamReemit_) {
        // The press's whole unit buys the sub-counter ($7E1F8D 0->240).
        // First re-emission lands one frame later than the steady cadence
        // (oracle hold60: press f32 -> f35 -> 2f steps). The emitter
        // decrements then fires at zero in the same tick, so a countdown of
        // N emits N-1 ticks after the press: cadence+2 -> the measured +3.
        streamSubAmmo_ = weapon.streamSubUnits;
        streamCooldown_ = weapon.streamCadenceFrames + 2;
    }

    // Chameleon Sting charged (oracle s3_* 2026-06-11): NO projectile — the
    // release applies the X invincibility player-state (cost -4 consumed
    // above; the real game parks an OID-0x11 bookkeeping object instead).
    if (isCharged && weapon.chargedType == WeaponShotType::PlayerState) {
        beginStingInvincibility(weapon.chargedInvincibilityFrames);
        return;
    }

    float speed = isCharged ? weapon.chargedSpeed : weapon.normalSpeed;
    float vx = facingRight ? speed : -speed;

    Projectile p;
    p.init(0, 0, vx, 0, type);
    p.isPlayerShot = true;
    p.weaponId = weapon.id;

    // Override velocity with weapon-specific speed (init sets vx from parameter,
    // but we also want to ensure the speed field matches for consistency)
    p.vx = vx;
    p.speed = speed;

    // normal_motion.json: the claimed normal Buster slot stays at its
    // spawn position for this update; its mover starts on the next one.
    if (!isCharged && weaponInventory.isBuster() && type == ProjectileType::Normal) {
        p.holdFirstTick = true;
    }

    // U71: the BUSTER max-charge shot is held at the muzzle for 6 frames
    // before moving (measured: build/u71/l2_shot oid=3, x static f152-158).
    // Buster-only — special weapons fire as ChargeL2 too with their own laws.
    if (isCharged && weaponInventory.isBuster() &&
        (type == ProjectileType::ChargeL2 || type == ProjectileType::ChargeL3)) {
        p.launchDelayFrames = 6;
    }

    // U45/U219: the ARM L3 helix (build/u45/l3_shot) is THREE objects at
    // weapon.armL3Speed. The source table handles y; phase 0 remains the
    // main-shot sentinel for the one-time release flash.
    if (isCharged && weaponInventory.isBuster() &&
        type == ProjectileType::ChargeL3) {
        p.vx = (facingRight ? weapon.armL3Speed : -weapon.armL3Speed);
        p.speed = weapon.armL3Speed;
        p.sineAmplitude = 13.0f;
        p.sinePeriodFrames = 16;
        p.sinePhaseFrames = 0;
        p.l3HelixVariant = 0;
        p.visualSpritePath = "content/x1/sprites/weapons/buster_l3_orbs.png";
        p.visualFrameWidth = 56;
        p.visualFrameHeight = 48;
        p.visualFrameStart = 0;
        p.visualFrameCount = 1;
        p.visualFrameTicks = 2;
        p.renderSuppressFrames = 12;
    }

    // Apply weapon-specific properties. Buster charge levels keep their own
    // DD-derived damage/collision profiles from Projectile::init(); special
    // weapons use their per-weapon charged definitions.
    if (isCharged && !weaponInventory.isBuster()) {
        p.damage = weapon.chargedDamage;
        p.piercing = weapon.chargedPiercing;
        p.hitboxSize = {weapon.chargedWidth, weapon.chargedHeight};
        p.despawnMarginPx = weapon.chargedDespawnMarginFwd;
    } else if (!weaponInventory.isBuster()) {
        p.damage = weapon.normalDamage;
        p.hitboxSize = {weapon.normalWidth, weapon.normalHeight};
        p.despawnMarginPx = weapon.normalDespawnMargin;
    }

    p.color = weapon.shotColor;
    p.applyWeaponVisual(weapon.id, isCharged);
    p.sfxShatter = weapon.sfxShatter;
    p.shattersOnWallHit = weapon.shattersOnWallHit;
    p.shatterCount = weapon.shatterCount;
    for (const auto& s : weapon.shatterFragments) {
        p.shatterSpecs.push_back({s.vx, s.vy, s.gravity});
    }
    if (!isCharged) {
        // Cosmetic falling ice trail (normal pellet only; oracle s1_air OAM).
        p.trailEveryFrames = weapon.trailEveryFrames;
        p.trailRiseVy = weapon.trailRiseVy;
        p.trailGravity = weapon.trailGravity;
    }

    // Fire Wave stream segment (oracle s1/hold60/s6 2026-06-11): 10f life,
    // pierces terrain AND enemies (lifetime-only despawn), 1 damage per
    // overlapped frame per enemy.
    if (isStream) {
        p.lifetime = weapon.streamSegmentLifetime;
        p.piercing = true;
        p.ignoresTerrain = true;
        p.continuousDamage = true;
        p.damageTickFrames = 1;
    }

    // Storm Tornado column (oracle 2026-06-11): grows WORLD-FIXED for 65f
    // (launchDelayFrames reuses the e-spark stationary-window machinery),
    // then rushes at normalSpeed until the 32px margin kills it. Pierces
    // enemies; 1 damage per overlapped frame.
    if (!isCharged && weapon.tornadoStationaryFrames > 0) {
        p.launchDelayFrames = weapon.tornadoStationaryFrames;
        p.lifetime = 600;             // margin-killed, no expiry observed
        p.piercing = true;
        p.ignoresTerrain = true;      // oracle S1/S2: terrain never consumed it
        p.continuousDamage = true;
        p.damageTickFrames = 1;
    }

    // Fire Wave charged ground wave (U29 re-measure, cp_wave_vram/
    // sm_wave_wall/s6_wave): the release spawns a MOVING head — held 1
    // frame then exactly 1.5 px/f — that ground-follows (climbs the CP
    // mound; a tall face kills it: SM x283) and dies at the +32 margin
    // (cp x321 = edge+32). It drops a stationary wake flame at its
    // previous-frame anchor every 10f (scene updateWaveHeads). The head
    // itself ships harmless — its own contact damage stays unmeasured
    // (wake segments arrive at its trail within 10f anyway; KB $open).
    if (isCharged && weapon.chargedGroundWave) {
        p.vx = (facingRight ? 1.0f : -1.0f) * weapon.waveHeadSpeed;
        p.vy = 0;
        p.launchDelayFrames = 2;         // claim row + 1 held row (f272/f273
                                         // both unmoved; fp +384 from f274)
        p.groundFollow = true;           // sled climb semantics (<=6px steps)
        p.projGravity = 0.25f;           // settles to the ground line (SM +9)
        p.waveHead = true;
        p.sfxSegmentPlant = weapon.sfxStreamHold;   // 0x61 per planted flame
        p.waveEveryFrames = weapon.waveSegmentEveryFrames;
        p.waveStepX = weapon.waveSegmentStepX;
        p.waveSegLifetime = weapon.waveSegmentLifetime;
        p.waveMaxSegments = weapon.waveMaxSegments;
        p.waveFacingRight = facingRight;
        p.waveSegDamage = weapon.chargedDamage;
        p.waveDamageTickFrames = weapon.waveDamageTickFrames;
        p.damage = 0;
        p.ignoresTerrain = false;
        p.despawnMarginPx = 32.0f;       // head margins out at edge+32
        p.lifetime = 100000;             // no intrinsic expiry
        // Head art (cp_wave_vram slot0 spr timeline): 129 x4f, 130 x4f,
        // then 147 looping — its own 3-cell sheet, intro-then-loop.
        p.visualSpritePath = "content/x1/sprites/weapons/fire_wave_head.png";
        p.visualFrameWidth = 24;
        p.visualFrameHeight = 48;
        p.visualFrameCount = 3;
        p.visualFrameTicks = 4;
        p.visualIntroCells = 2;
        p.visualFrameStart = 0;
        p.visualFixedFrame = -1;
        p.visualOffsetX = 0.0f;
        p.visualOffsetY = -7.0f;         // U43: anchor rides floor-11 (677
                                         // vs 688 measured); +4 sank the art
    }

    // Charged Shotgun Ice sled (oracle 2026-06-09): spawns STATIONARY in a
    // ~90-frame formation, then accelerates; ground-follows; never shatters;
    // breaks into cosmetic debris on true wall faces, silently.
    if (isCharged && weapon.chargedRideable) {
        p.rideable = true;
        p.groundFollow = weapon.chargedGroundFollow;
        p.sledDebrisOnWall = true;   // ice-only cosmetic break-up (U77)
        p.sfxSledBreak = -1;         // s8_sfx_s6_ride_wide: wall-break is silent
        p.sledLaunchFrame = weapon.sledLaunchFrame;
        p.sledAccel = weapon.sledAccel;
        p.sledMaxSpeed = weapon.sledMaxSpeed;
        p.sledFacingRight = facingRight;
        p.vx = 0;
        p.projGravity = 0.25f;   // measured settle ramp (vy steps of 0.25/f)
        p.lifetime = 600;        // long-lived; real sled despawns off-screen
        p.shattersOnWallHit = false;
        p.shatterSpecs.clear();
        p.piercing = true;
    }

    const float visualWidth = p.visualFrameWidth > 0
        ? static_cast<float>(p.visualFrameWidth) * p.visualScale
        : p.hitboxSize.x * p.visualScale;
    const Vector2 muzzleOffset = busterMuzzleOffsetForState();
    const float muzzleX = position.x + (facingRight
        ? muzzleOffset.x
        : spriteWidth - muzzleOffset.x);
    // KB-2026-08-06k: keep grounded X poses and their muzzle aligned with the
    // measured snow occlusion. Airborne/wall poses keep their own anchors.
    const float groundedVisualOffset = visualGroundingApplies()
        ? visualGroundingOffsetY
        : 0.0f;
    const float muzzleY = position.y + muzzleOffset.y + groundedVisualOffset;
    p.position.x = facingRight
        ? muzzleX + busterMuzzleGap + visualWidth * 0.5f - p.hitboxSize.x * 0.5f
        : muzzleX - busterMuzzleGap - visualWidth * 0.5f - p.hitboxSize.x * 0.5f;
    p.position.y = muzzleY - p.hitboxSize.y * 0.5f;
    p.prevPosition = p.position;

    // WP-C anchor_idle/anchor_offset.json maps the idle source anchor;
    // projCenterCorr maps projectile RAM to its rendered hitbox center.
    constexpr float kAnchorDy = 37.0f;
    const auto sourceRamAnchorX = [&]() noexcept {
        return player_anchor::sourceRamAnchorX(
            position.x, spriteWidth, facingRight);
    };
    if (weaponInventory.isBuster() && type == ProjectileType::ChargeL1) {
        if (onGround && (state_ == PlayerState::Idle || state_ == PlayerState::Run))
            p.configureSourceChargeL1(sourceRamAnchorX(), position.y + 40.0f);
        // charge_l1_air_birth_and_walker_deaths_2026-09-17.json: CP f1807.
        else if (!onGround && facingRight && state_ == PlayerState::Fall)
            p.configureSourceChargeL1(sourceRamAnchorX(), position.y + 40.0f, {25.0f, -8.0f});
    }
    if (weaponInventory.isBuster() && type == ProjectileType::Normal &&
        state_ == PlayerState::Idle && facingRight && onGround) {
        // knowledge_base/mmx1/weapons/buster/idle_spawn_x.json: the source
        // idle/right normal shot center is the player RAM anchor plus 16px.
        constexpr float kIdleBusterSourceShotOffsetX = 16.0f;
        const float centerX = sourceRamAnchorX() + kIdleBusterSourceShotOffsetX;
        p.position.x = centerX - p.hitboxSize.x * 0.5f;
        p.prevPosition.x = p.position.x;
        // normal_motion.json birth (16,-3) joined to
        // chill_penguin_ground_contact.json source-to-native Y=-40.
        // The legacy muzzle is an art center; retain it and tag the RAM center.
        p.sourceCollisionCenterOffset = Vector2{
            centerX - p.position.x, position.y + 40.0f - 3.0f - p.position.y};
    }
    if (weapon.hasMeasuredSpawn && !isCharged) {
        // KB spawn_offset is SNES-anchor -> projectile-RAM-anchor (mirrored
        // for left). The charged sled keeps the muzzle system: its tile-anchor
        // mapping is unmeasured and its formation spawn is WP-A-verified.
        const float anchorX = sourceRamAnchorX();
        const float centerX = anchorX + (facingRight
            ? weapon.spawnOffsetX + weapon.projCenterCorrX
            : -(weapon.spawnOffsetX + weapon.projCenterCorrX));
        const float centerY = position.y + kAnchorDy + weapon.spawnOffsetY
                              + weapon.projCenterCorrY;
        p.position.x = centerX - p.hitboxSize.x * 0.5f;
        p.position.y = centerY - p.hitboxSize.y * 0.5f;
        p.prevPosition = p.position;
        // SNES: the pellet's first visible frame is the UNMOVED spawn
        // position (s1_air bullets.csv f32 = anchor+16, +8 only next frame);
        // the engine updates projectiles after fireShot within the same
        // tick, so hold the first movement step.
        p.holdFirstTick = true;
    }
    if (weapon.hasMeasuredChargedSpawn && isCharged) {
        // Charged sled leaves the muzzle system (tile-anchor measured
        // 2026-06-11, cos_s3 + cos_sled_anchor_left): RAM spawn = player
        // anchor + (chargedSpawnOffset, x mirrored); visual center = RAM
        // anchor + the PER-FACING correction (left is not a mirror).
        const float anchorX = sourceRamAnchorX();
        const float centerX = anchorX + (facingRight
            ? weapon.chargedSpawnOffsetX + weapon.chargedCenterCorrXRight
            : -weapon.chargedSpawnOffsetX + weapon.chargedCenterCorrXLeft);
        const float centerY = position.y + kAnchorDy + weapon.chargedSpawnOffsetY
                              + weapon.chargedCenterCorrY;
        p.position.x = centerX - p.hitboxSize.x * 0.5f;
        p.position.y = centerY - p.hitboxSize.y * 0.5f;
        p.prevPosition = p.position;
    }

    // Storm Tornado charged halves (oracle s3_charged/thresh179): TWO
    // stationary terrain-ignoring columns at X-anchor +-(0, 96), 55f each
    // (the lower spawns below the floor in the real game). The rising
    // tornado is the anim; halves carry the normal column's damage (own
    // value unmeasured, KB $open).
    if (isCharged && weapon.chargedTornadoHalves) {
        const float anchorXc = sourceRamAnchorX();
        const float anchorYc = position.y + kAnchorDy;
        p.vx = 0;
        p.vy = 0;
        p.lifetime = weapon.tornadoHalfLifetime;
        p.piercing = true;
        p.ignoresTerrain = true;
        p.continuousDamage = true;
        p.damageTickFrames = 1;
        p.despawnMarginPx = std::max(
            p.despawnMarginPx, weapon.tornadoHalfOffsetY + weapon.chargedHeight);
        p.position.x = anchorXc - p.hitboxSize.x * 0.5f;
        p.position.y = (anchorYc - weapon.tornadoHalfOffsetY) - p.hitboxSize.y * 0.5f;
        p.prevPosition = p.position;
        // SNES claim-row convention (same as the measured-spawn path):
        // counting the spawn row makes the trace lifetime exactly 55.
        p.holdFirstTick = true;
        // U44 (supersedes the U30 halves rendering; build/u44/tornado OAM):
        // the real charged tornado renders as ONE continuous column at X's
        // x — 42 sprites spanning the FULL screen height (y 0-224), 16px-
        // periodic, flickering 1-on/1-off. The two RAM objects are damage
        // bookkeeping, not two visuals. The UPPER object draws the column
        // (temporal sheet: grow-from-center intro, 4 loop cycles, fade
        // outro — 29 cells x 2f = the 55f life), centered on X's anchor;
        // the LOWER draws nothing.
        p.visualSpritePath =
            "content/x1/sprites/weapons/storm_tornado_charged_column.png";
        p.visualFrameWidth = 64;
        p.visualFrameHeight = 224;
        p.visualFrameCount = 29;
        p.visualFrameTicks = 2;
        p.visualIntroCells = 0;
        p.visualOffsetX = 0.0f;
        p.visualTileYCopies = 1;
        // upper hitbox center sits at anchorY-96; the column cell centers
        // on X's anchor row.
        p.visualOffsetY = 96.0f;
        p.flickerAlternate = true;

        Projectile lower = p;
        lower.assignFreshSerial();
        lower.position.y = (anchorYc + weapon.tornadoHalfOffsetY) - lower.hitboxSize.y * 0.5f;
        lower.prevPosition = lower.position;
        lower.visualSpritePath.clear();   // the upper draws the whole column
        projectiles->push_back(lower);
    }

    // Apply weapon shot type behaviors
    WeaponShotType shotType = isCharged ? weapon.chargedType : weapon.normalType;

    if (shotType == WeaponShotType::StingFan) {
        // Chameleon Sting (oracle 2026-06-11): a STATIONARY muzzle bolt at
        // the measured spawn point (placed by the WP-C bridge above), 23f
        // life; the scene spawns the 3-dart fan at stingFanTick of its life
        // (GameplayScene::spawnStingFan). Bolt contact damage UNMEASURED —
        // ships harmless (KB $open).
        p.vx = 0;
        p.vy = 0;
        p.stingMuzzle = true;
        p.sfxFanRelease = weapon.sfxFanRelease;   // 0x67 at the fan spawn
        p.stingFanTick = weapon.stingFanSpawnTick;
        p.lifetime = weapon.stingMuzzleLifetimeFrames;
        p.damage = 0;
        p.stingDartDamage = weapon.normalDamage;
        p.ignoresTerrain = true;
        p.despawnMarginPx = weapon.normalDespawnMargin;
        const float mirror = facingRight ? 1.0f : -1.0f;
        for (const auto& d : weapon.stingDarts) {
            Projectile::StingDartSpec s;
            // anchor-relative (KB, as measured) -> muzzle-relative: the
            // muzzle is world-fixed, so a moving X can't drag the fan.
            s.offX = (d.offX - weapon.spawnOffsetX) * mirror;
            s.offY = d.offY - weapon.spawnOffsetY;
            s.vx = d.vx * mirror;
            s.vy = d.vy;
            s.cell = d.cell;
            p.stingDartSpecs.push_back(s);
        }
        projectiles->push_back(p);
        return;
    }

    if (shotType == WeaponShotType::Spread3) {
        // 3-way spread: fire forward, up-diagonal, and down-diagonal
        p.vy = 0; // Center shot
        projectiles->push_back(p);

        // Up-diagonal. A plain copy keeps the center shot's serial — trace
        // tracks would merge (same latent bug the e-spark backward twin had).
        Projectile pUp = p;
        pUp.assignFreshSerial();
        pUp.vy = -2.0f;
        pUp.position.y -= 2;
        pUp.prevPosition = pUp.position;
        projectiles->push_back(pUp);

        // Down-diagonal
        Projectile pDown = p;
        pDown.assignFreshSerial();
        pDown.vy = 2.0f;
        pDown.position.y += 2;
        pDown.prevPosition = pDown.position;
        projectiles->push_back(pDown);
        return; // Already pushed all 3
    }

    if (shotType == WeaponShotType::Homing && weapon.homingLaunchFrames > 0) {
        const float dir = facingRight ? 1.0f : -1.0f;
        if (isCharged && weapon.chargedTorpedoFan) {
            // FIVE-torpedo fan (OID 16, oracle s3_charged): scripted members
            // through the 15f countdown; at expiry each member steers iff
            // its release-acquired target is still alive (w70iso_REPORT —
            // the scene acquires/tracks, Projectile::update switches). Each
            // spawns at its measured CLAIM-row offset from the player
            // anchor with the claim velocity pre-advanced one accel1 tick
            // (the claim row is logged post-move; see KB $anchor_convention)
            // and holds its first engine tick.
            const float fAnchorX = sourceRamAnchorX();
            const float fAnchorY = position.y + kAnchorDy;
            bool first = true;
            int memberIdx = 0;
            for (const auto& m : weapon.torpedoFan) {
                Projectile g = p;
                if (!first) g.assignFreshSerial();
                first = false;
                g.homing = false;
                g.torpedoFanMember = true;
                // S7: one measured fish cell per member. Strip cols are in
                // ORACLE SLOT order (E,S,N,NE,SE); the engine fan list is
                // E,N,S,NE,SE — so N takes col 2 and S col 1.
                static const int kFishCell[5] = {0, 2, 1, 3, 4};
                g.visualFixedFrame = kFishCell[memberIdx % 5];
                memberIdx++;
                // Fan members PIERCE terrain (oracle s3_charged: the S/SE
                // members crossed the CP floor to y=1261/1231, 48-78px into
                // solid ground, dying only at the screen margin). Enemy
                // contact unmeasured — they die on enemies like the normal
                // shot (engine approximation, KB $open).
                g.ignoresTerrain = true;
                // countdown-2: claim row + the pre-advanced tick (phase 2
                // first lands on the 15th post-claim frame, s3 f287)
                g.fanCountdown = weapon.torpedoFanCountdownFrames - 2;
                g.fanA1x = m.a1x * static_cast<int>(dir);
                g.fanA1y = m.a1y;
                g.fanA2x = m.a2x * static_cast<int>(dir);
                g.fanA2y = m.a2y;
                g.fanCapX = m.capX;
                g.fanCapY = m.capY;
                // Steering start heading if the homing gate opens (measured
                // right-release byte; left = engine mirror, unmeasured).
                g.fanHeading = facingRight ? m.headingAtExpiry
                                           : (32 - m.headingAtExpiry) % 32;
                g.homingAccelRaw = weapon.homingAccelRaw;
                g.homingStraightCapRaw = weapon.homingStraightCapRaw;
                for (int i = 0; i < 9; i++) {
                    g.homingTableQRaw[i] = weapon.homingTableQRaw[i];
                    g.homingXcapRaw[i] = weapon.homingXcapRaw[i];
                }
                g.homingCcwZero = weapon.homingCcwZero;
                g.vx = (m.v0x + m.a1x) * dir / 256.0f;
                g.vy = (m.v0y + m.a1y) / 256.0f;
                const float cX = fAnchorX
                    + (facingRight ? m.offX + weapon.projCenterCorrX
                                   : -(m.offX + weapon.projCenterCorrX));
                const float cY = fAnchorY + m.offY + weapon.projCenterCorrY;
                g.position.x = cX - g.hitboxSize.x * 0.5f;
                g.position.y = cY - g.hitboxSize.y * 0.5f;
                g.prevPosition = g.position;
                g.holdFirstTick = true;
                projectiles->push_back(g);
            }
            return;  // all five pushed
        }
        // Normal torpedo: claim-row spawn (already placed by the WP-C bridge
        // above via spawnOffset 17.125 = muzzle 16 + one v0 move); velocity
        // pre-advanced one accel tick, countdown = launch_frames - 2 (claim
        // row + the pre-advanced tick), so steering first applies on the
        // 7th post-claim frame like the oracle.
        p.homing = true;
        p.vx = (weapon.homingLaunchV0Raw + weapon.homingAccelRaw) * dir / 256.0f;
        p.vy = 0;
        p.homingCountdown = weapon.homingLaunchFrames - 2;
        p.homingHeading = facingRight ? 8 : 24;
        p.homingAccelRaw = weapon.homingAccelRaw;
        p.homingStraightCapRaw = weapon.homingStraightCapRaw;
        for (int i = 0; i < 9; i++) {
            p.homingTableQRaw[i] = weapon.homingTableQRaw[i];
            p.homingXcapRaw[i] = weapon.homingXcapRaw[i];
        }
        p.homingCcwZero = weapon.homingCcwZero;
        p.lifetime = 600;   // no timer in the real game — margins/terrain kill
    } else if (shotType == WeaponShotType::Homing) {
        p.homing = true;    // legacy KB without a measured model
    }

    if (shotType == WeaponShotType::Rolling) {
        // Rolling Shield ball (oracle 2026-06-11): 7f unmoved at the muzzle
        // (launchDelay reuse), then the roll under the measured 0.25 px/f^2
        // gravity — it settles +2px onto the ground and follows slopes.
        // No spawn arc (the oracle rows show none).
        p.rolling = true;
        p.bouncesOffWalls = weapon.rollWallBounce;
        p.wallBouncesLeft = p.bouncesOffWalls ? 1 : 0;   // U76: one bounce,
                                                         // second wall kills
        p.projGravity = weapon.rollGravity > 0 ? weapon.rollGravity : 0.18f;
        p.launchDelayFrames = weapon.rollSpawnPauseFrames;
        p.vy = 0.0f;
        if (onGround) {
            // U86: the SNES ball art starts on X's floor line when fired
            // grounded. The engine stores a compact 14px hitbox top-left,
            // so bridge the measured anchor spawn to the resolved foot line
            // instead of letting gravity visibly drop the ball there.
            const float floorY = position.y + hitboxOffset.y + hitboxSize.y;
            p.position.y = floorY - p.hitboxOffset.y - p.hitboxSize.y;
            p.prevPosition = p.position;
        }
    }

    // Rolling Shield charged (oracle 2026-06-11): ONE object GLUED to X's
    // anchor (the scene re-feeds the anchor every tick); absorbs exactly
    // one hit — the scene's player-damage paths consume it instead of
    // hurting X. Persists until absorbed (no lifetime).
    if (isCharged && weapon.chargedAbsorbShield) {
        p.absorbShield = true;
        p.sfxShieldHum = weapon.sfxShieldHum;          // 0x64 cadence (scene)
        p.sfxShieldHumFirst = weapon.sfxShieldHumFirst;
        p.sfxShieldHumEvery = weapon.sfxShieldHumEvery;
        p.vx = 0;
        p.vy = 0;
        p.lifetime = 1 << 24;     // until absorbed
        p.ignoresTerrain = true;
        p.piercing = true;
        // Contact damage TO enemies is unmeasured (KB $open) — a glued
        // piercing body must not melt overlapped enemies per-frame, so the
        // continuous-damage window caps it (tick cadence provisional).
        p.continuousDamage = true;
        p.damageTickFrames = 4;
        const AABB playerBox = getHitbox();
        p.position.x = playerBox.x + (playerBox.w - p.hitboxSize.x) * 0.5f;
        p.position.y = playerBox.y + (playerBox.h - p.hitboxSize.y) * 0.5f;
        p.prevPosition = p.position;
    }

    if (shotType == WeaponShotType::Boomerang) {
        // Boomerang Cutter (oracle 2026-06-11): slope-table flight; returnX/Y
        // are re-aimed at the LIVE player center every frame by the scene.
        p.boomerang = true;
        p.boomerangTimer = 0;
        p.returnX = position.x + spriteWidth / 2;
        p.returnY = position.y + spriteHeight / 2;
        // Steered cutter: NO lifetime expiry and NO screen-margin despawn —
        // a missed catch loops indefinitely (oracle s1_left: ~5 full orbits,
        // 390f+, alive at log end with no decay) and offscreen excursions
        // survive and RETURN (s9_offscreen). U46 (build/u46/cutter_wall2 +
        // charged, FM crate wall): the cutter AND the charged giants fly
        // THROUGH solid blocks (39px+ inside the crate rows, alive 350f+)
        // — the old terrain-death claim is SUPERSEDED (David was right:
        // "they should pass walls"). Dies on enemies and the catch only.
        p.ignoresTerrain = true;
        p.lifetime = 1 << 20;
        if (isCharged && weapon.chargedBoomerangFourWay) {
            // FOUR giants at the anchor: +x, up, -x, down (idx 0/24/16/8),
            // 11 straight frames, one table step per frame, monotonic
            // clockwise-screen rotation, no steering, no catch.
            p.boomMagRaw = static_cast<int>(weapon.chargedSpeed * 256.0f);
            p.boomStraightFrames = weapon.chargedBoomStraightFrames;
            p.boomTurnEvery = weapon.chargedBoomTurnEveryFrames;
            p.boomFirstStep = 1;
            p.boomSteer = false;
            p.boomRotSense = 1;
            p.boomCatchRefund = false;
            // Giants die at the ~60px screen margin (oracle: 55-65px past
            // the edges); the lifetime is only a safety net.
            p.lifetime = 600;
            // All four spawn AT the player anchor (oracle: (0,0) like the
            // normal cutter; the giants' own center correction awaits S7 —
            // anchor used as the visual/hitbox center directly).
            const float gAnchorX = sourceRamAnchorX();
            const float gAnchorY = position.y + kAnchorDy;
            static constexpr int kFourWayIdx[4] = {0, 24, 16, 8};
            for (int i = 0; i < 4; i++) {
                Projectile g = p;
                if (i > 0) g.assignFreshSerial();
                g.position.x = gAnchorX - g.hitboxSize.x * 0.5f;
                g.position.y = gAnchorY - g.hitboxSize.y * 0.5f;
                g.prevPosition = g.position;
                // Giants PIERCE terrain (oracle s3_charged: the down/left
                // giants crossed the floor to y1307/1308 and died at the
                // BOTTOM margin) — screen-bound only.
                g.ignoresTerrain = true;
                g.boomAngleIdx = facingRight ? kFourWayIdx[i]
                                             : ((kFourWayIdx[i] + 16) % 32);
                // initial velocity = the table entry (straight phase flies it)
                const float s = weapon.chargedSpeed;
                switch (g.boomAngleIdx) {
                    case 0:  g.vx =  s; g.vy = 0;  break;
                    case 8:  g.vx = 0;  g.vy =  s; break;
                    case 16: g.vx = -s; g.vy = 0;  break;
                    default: g.vx = 0;  g.vy = -s; break;
                }
                projectiles->push_back(g);
            }
            return;  // all four pushed
        }
        p.boomMagRaw = static_cast<int>(weapon.normalSpeed * 256.0f);
        p.boomStraightFrames = weapon.boomerangStraightFrames;
        p.boomTurnEvery = weapon.boomerangTurnEveryFrames;
        p.boomFirstStep = weapon.boomerangFirstTurnStep;
        // First-turn route fields — Projectile::update decides UP vs DOWN
        // at the decision tick against the live returnY (s5_route_REPORT).
        p.boomDecisionTick = weapon.boomerangRouteDecisionTick;
        p.boomDownStraightFrames = weapon.boomerangDownStraightFrames;
        p.boomDownFirstStep = weapon.boomerangDownFirstTurnStep;
        p.boomSteer = true;
        p.boomAngleIdx = facingRight ? 0 : 16;
        // Initial turn lifts UPWARD both facings: CCW (-1) for right, CW (+1)
        // for left in clockwise-screen index terms.
        p.boomRotSense = facingRight ? -1 : 1;
        p.boomCatchRefund = weapon.boomerangCatchRefund;
    }

    if (shotType == WeaponShotType::WallSplit) {
        p.splitsOnWallHit = true;
        p.splitCount = 2;
        p.splitSpeed = weapon.wallSplitSpeed;
        p.splitPierce = weapon.splitChildrenPierceTerrain;
        p.splitChildrenDamage = weapon.splitChildrenDamage;
    }

    // Electric Spark charged twin (oracle s3_charged 2026-06-10): the forward
    // giant spawns at release and sits stationary 12 frames; a BACKWARD twin's
    // bullet slot is claimed at release+10 at the SAME spawn point; both first
    // move at release+12 at +-8.0 px/f. Spawn at the SNES anchor + KB offset
    // (RAM-exact; the charged sprite's own visual-center correction awaits S7).
    if (isCharged && weapon.chargedTwinBackward) {
        const float twinAnchorX = sourceRamAnchorX();
        const float twinCx = twinAnchorX + (facingRight ? weapon.spawnOffsetX
                                                        : -weapon.spawnOffsetX);
        const float twinCy = position.y + kAnchorDy + weapon.spawnOffsetY;
        p.position.x = twinCx - p.hitboxSize.x * 0.5f;
        p.position.y = twinCy - p.hitboxSize.y * 0.5f;
        p.prevPosition = p.position;
        p.launchDelayFrames = weapon.chargedLaunchFrame;

        Projectile back = p;
        back.assignFreshSerial();
        back.vx = -p.vx;
        // dormant for claim+1 update ticks -> first traced/visible row lands
        // exactly on the SNES slot-claim frame (release+10), then 1 more
        // stationary tick before motion at release+12.
        back.dormantFrames = weapon.chargedBackwardClaimFrame + 1;
        back.launchDelayFrames = 1;
        back.despawnMarginPx = weapon.chargedDespawnMarginBack;
        projectiles->push_back(back);
    }

    projectiles->push_back(p);

    // U45/U219: trailers claim +5f after the main, then move at
    // main-claim+10 and +13 using table variants 1 and 2.
    if (isCharged && weaponInventory.isBuster() &&
        type == ProjectileType::ChargeL3) {
        // Helix baseline = the spawn fire line (main's y at rest).
        for (int i = 0; i < 2; ++i) {
            Projectile trail = p;
            trail.assignFreshSerial();
            trail.dormantFrames = 5;
            trail.launchDelayFrames = (i == 0) ? 4 : 7;
            trail.sinePhaseFrames = (i == 0) ? 4 : 8;
            trail.l3HelixVariant = (i == 0) ? 1 : 2;
            projectiles->push_back(trail);
        }
        // Anchor every helix table to the fire line AFTER the copies.
        const size_t n = projectiles->size();
        for (size_t k = n - 3; k < n; ++k) {
            (*projectiles)[k].sineBaseY = (*projectiles)[k].position.y;
        }
    }
}

Vector2 Player::busterMuzzleOffsetForState() const {
    switch (state_) {
        case PlayerState::Run:
            return busterMuzzleRun;
        case PlayerState::Dash:
            return busterMuzzleDash;
        case PlayerState::Jump:
        case PlayerState::Fall:
        case PlayerState::WallJump:
        case PlayerState::DashJump:
        case PlayerState::Ladder:
            return busterMuzzleJump;
        case PlayerState::WallSlide:
            return busterMuzzleWall;
        case PlayerState::Idle:
        case PlayerState::Hurt:
        case PlayerState::Die:
        default:
            return busterMuzzleIdle;
    }
}

bool Player::visualGroundingApplies() const {
    // Grounded neutral/walk cells and the source's grounded hurt recoil share
    // the same snow occlusion layer.  Hurt still has its measured physical
    // hop; this offset only places the sprite canvas on the snow, preventing
    // the body from floating when the hit pose changes height.
    return state_ == PlayerState::Idle || state_ == PlayerState::Run ||
           state_ == PlayerState::Hurt;
}

int Player::countActiveShots() const {
    if (!projectiles) return 0;
    int count = 0;
    for (const auto& p : *projectiles) {
        if (p.active && p.isPlayerShot && p.type == ProjectileType::Normal) {
            count++;
        }
    }
    return count;
}

} // namespace mmx
