// Ground-jump callers, separated from the movement handlers in player.cpp.
#include "entities/player.h"

#include <cmath>

namespace mmx {

void Player::resetJumpState() {
    // R186: a lifecycle handoff cancels both an armed launch and a press
    // buffered before control returns; ordinary state updates keep the delay.
    coyoteTimer_ = 0;
    jumpBufferTimer_ = 0;
    dashBufferTimer_ = 0;  // also participates in tryDashJump()
    jumpLaunchDelay_ = 0;
    pendingDashJump_ = false;
    sourceJumpSpeed_.reset();
    wallJumpDashed_ = false;
    wallJumpWindup_ = 0;
    wallJumpLockout_ = 0;
    wallJumpDirection_ = 0;
    wallJumpResumePending_ = false;
    wallJumpResumeY_ = 0.0f;
}

bool Player::armGroundJump(bool dashJump) {
    coyoteTimer_ = 0;
    jumpBufferTimer_ = 0;
    jumpLaunchDelay_ = kJumpLaunchDelayFrames;
    pendingDashJump_ = dashJump;
    sourceJumpSpeed_.reset();
    velocity.x = 0;
    return true;
}

void Player::fireGroundJump() {
    velocity.y = jumpVelocity;
    if (pendingDashJump_) {
        velocity.x = facingRight ? dashSpeed : -dashSpeed;
        changeState(PlayerState::DashJump);
    } else {
        // update() skips the state handler when spending this delayed launch,
        // so horizontal input is selected here exactly once.
        // CP2016: the grounded walk-start zero must not consume the first ascent.
        onGround = false;
        applyHorizontalInput();
        changeState(PlayerState::Jump);
    }
    pendingDashJump_ = false;
    TraceLog(LOG_DEBUG, "Player jumped from pos (%.1f, %.1f)", position.x, position.y);
}

bool Player::tryJump() {
    if (jumpLaunchDelay_ > 0) return true;
    if (jumpBufferTimer_ <= 0) return false;

    if (onGround) {
        const auto profile = sourceContactProfile();
        // T1.7: the same-update launch is the ROM's, not the authored Chill
        // Penguin stage's, so sourceGroundMovementEnabled_ does not gate it.
        // Storm Eagle f1408 (b+left, X grounded on the deck platform) is
        // followed by f1409 already carrying the whole -1299/256 launch
        // displacement, with the walk's own -120/256 kept in x (capture
        // build/t17-land, harvest of the committed storm-eagle.mmo).
        const bool sourceWalk =
            state_ == PlayerState::Run &&
            profile && *profile == SourceContactProfile::NormalA552 &&
            !(walkStartRamping_ && walkStartTick_ < kWalkStartFrames);
        if (sourceWalk) {
            // ground_jump_launch.json: $818445 JSR initJump then $818448
            // JMP Ascending in this same update. updateRun already selected
            // horizontal speed; $81962E retains its magnitude in D+$5C.
            sourceJumpSpeed_ = std::abs(velocity.x);
            velocity.y = jumpVelocity;
            if (*sourceJumpSpeed_ == 456.0f / 256.0f) {
                velocity.y = -1569.0f / 256.0f;  // $81963F LDA #$0621
            } else if (*sourceJumpSpeed_ == 408.0f / 256.0f) {
                velocity.y = -1481.0f / 256.0f;  // $819644 LDA #$05C9
            }
            coyoteTimer_ = 0;
            jumpBufferTimer_ = 0;
            pendingDashJump_ = false;
            changeState(PlayerState::Jump);
            return true;
        }
        return armGroundJump(false);
    }
    if (canCoyoteJump()) {
        // Coyote time remains the existing engine extension.
        sourceJumpSpeed_.reset();
        velocity.y = jumpVelocity;
        coyoteTimer_ = 0;
        jumpBufferTimer_ = 0;
        changeState(PlayerState::Jump);
        return true;
    }
    return false;
}

} // namespace mmx
