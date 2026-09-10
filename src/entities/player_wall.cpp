// player_wall.cpp - owns wall-slide, wall-drop and wall-jump movement.
#include "entities/player.h"

namespace mmx {
namespace {
constexpr int kWallDropPoseFrames = 6;
}

void Player::beginWallDropFall() {
    velocity.x = 0.0f;
    wallDropPoseTimer_ = kWallDropPoseFrames;
    wallDropVisualFacingRight_ = (wallSide_ > 0);
    changeState(PlayerState::Fall);
}

void Player::updateWallSlide() {
    // R282/R287: physics can land a sliding X before this update. Resolve
    // that contact before direction release opens an airborne wall-drop pose
    // or the wall-jump branch consumes a grounded jump press.
    if (onGround) {
        velocity = {0.0f, 0.0f};
        changeState(PlayerState::Idle);
        updateIdle();
        return;
    }

    // Push gently into the wall each frame. Without this, collision resolution
    // zeroes velocity.x on contact, so next frame there's no movement, no
    // collision detected, touchingWall goes false, and the state flickers
    // between WallSlide and Fall every other frame. (flight-recorder BUG-003)
    velocity.x = (wallSide_ > 0) ? 0.5f : -0.5f;

    // Slow descent — this is the core wall-slide feel. moveAndCollide applies
    // gravity AFTER this update, so cap to (wallSlideSpeed - gravity) here; after
    // that gravity tick the net descent is exactly wallSlideSpeed (RAM-measured
    // 2.0 px/f). Capping to wallSlideSpeed directly gave 2.25 (12.5% too fast).
    float wallSlideCap = wallSlideSpeed - gravity;
    if (velocity.y > wallSlideCap) {
        velocity.y = wallSlideCap;
    }

    // Player released the direction toward the wall — let go
    bool holdingToward = (wallSide_ > 0 && inputRight_) || (wallSide_ < 0 && inputLeft_);
    if (!holdingToward) {
        beginWallDropFall();
        return;
    }

    // wall_jump_release.json: the accepted B press starts six stationary
    // updates. The impulse follows this preparation, even if B is released.
    if (jumpBufferTimer_ > 0) {
        const bool dashSeen = hasBoots && (inputDashHeld_ || dashBufferTimer_ > 0);
        jumpBufferTimer_ = 0;
        wallJumpDirection_ = (wallSide_ > 0) ? -1 : 1; // Direction of kick
        facingRight = (wallSide_ > 0) ? false : true;   // Face kick direction
        changeState(PlayerState::WallJump);
        wallJumpDashed_ = dashSeen;
        wallJumpWindup_ = 6;
        wallJumpLockout_ = 0;
        velocity = {0.0f, -gravity};
        return;
    }

    // Slid past the bottom of the wall (no more tiles to grab)
    bool stillOnWall = (wallSide_ > 0) ? touchingWallRight : touchingWallLeft;
    if (!stillOnWall) {
        beginWallDropFall();
        return;
    }
}

void Player::updateWallJump() {
    if (onCeiling) {
        velocity.y = 0;
        wallJumpLockout_ = 0;
        changeState(PlayerState::Fall);
        return;
    }

    if (onGround) {
        wallJumpLockout_ = 0;
        changeState((velocity.x != 0 || walkStartActive()) ? PlayerState::Run
                                                        : PlayerState::Idle);
        return;
    }

    if (wallJumpWindup_ > 0) {
        // R304 held/tap/late controls share the same fast impulse. A may
        // arrive during preparation, and releasing it does not cancel it.
        if (hasBoots && (inputDashHeld_ || dashBufferTimer_ > 0)) wallJumpDashed_ = true;
        if (--wallJumpWindup_ > 0) {
            velocity = {0.0f, -gravity};
        } else {
            const float speed = wallJumpDashed_ ? dashSpeed : wallJumpVelocityX;
            velocity = {wallJumpDirection_ * speed, wallJumpVelocityY};
            if (wallJumpDashed_) dashBufferTimer_ = 0;
            wallJumpLockout_ = wallJumpLockoutFrames;
        }
        return;
    }

    // Source controls with B held or released share all seven moving steps.
    // Preserve one stationary handoff before ordinary air control and release.
    if (wallJumpLockout_ <= 0) {
        const float resumeY = velocity.y;
        const bool resumeDash = wallJumpDashed_;
        changeState(velocity.y < 0 ? PlayerState::Jump : PlayerState::Fall);
        if (resumeDash) sourceJumpSpeed_ = dashSpeed;
        wallJumpResumeY_ = resumeY;
        wallJumpResumePending_ = true;
        velocity = {0.0f, -gravity};
        return;
    }

    applyHorizontalInputWithLockout();

    // During lockout, still transition to Fall on apex (but keep lockout running)
    if (velocity.y >= 0) {
        changeState(PlayerState::Fall);
        return;
    }

}

} // namespace mmx
