// player_support.cpp - scene-owned Storm Eagle support handoff.
//
// This seam stays out of player.cpp so existing Player-only targets do not
// acquire a gameplay-scene dependency. It transfers the previous per-record
// support latch around Player::update; contact geometry remains scene-owned.

#include "entities/player.h"

namespace mmx {

namespace {

bool isGroundSupportState(PlayerState state) {
    return state == PlayerState::Idle || state == PlayerState::Run ||
           state == PlayerState::Dash;
}

bool isLaunchOrUnsafeState(PlayerState state) {
    return state == PlayerState::Jump || state == PlayerState::DashJump ||
           state == PlayerState::WallJump || state == PlayerState::Ladder ||
           state == PlayerState::Hurt || state == PlayerState::Die;
}

}  // namespace

void Player::beginStormEagleSupportFrame(bool previousSupport) {
    stormEagleSupportLatched_ = previousSupport;
    stormEagleSupportFrameActive_ = true;

    if (!previousSupport) return;

    // Let the existing state machine observe previous support for this update.
    // consumeStormEagleSupportForPhysics clears this stale tile flag before
    // current contact physics runs.
    onGround = true;

    // Source idle initialization removes stale Y velocity after the landing
    // recovery pose has expired. During landing recovery (3..1), including
    // first DashJump contact, preserve the measured velocity.
    if (isGroundSupportState(state_) && landingAnimTimer_ == 0) {
        velocity.y = 0.0f;
    }
}

bool Player::consumeStormEagleSupportForPhysics() {
    if (!stormEagleSupportFrameActive_) return false;
    stormEagleSupportFrameActive_ = false;
    if (!stormEagleSupportLatched_) return false;

    // A launch, ladder transition, hurt, or death is never converted into a
    // platform hold. A pending ground-launch delay is the measured flat frame
    // and remains eligible for the old-support handoff.
    const bool pendingLaunch = jumpLaunchDelay_ > 0;
    const bool unsafe = isLaunchOrUnsafeState(state_);
    const bool hold = !unsafe &&
        (isGroundSupportState(state_) || pendingLaunch);

    // Clear only the old platform ground flag before either X-only or normal
    // current-contact physics. Ordinary tile frames retain normal ordering.
    onGround = false;
    onCeiling = false;
    return hold;
}

void Player::finishStormEagleSupport(bool currentSupport) {
    stormEagleSupportLatched_ = currentSupport;
    stormEagleSupportFrameActive_ = false;

    // False preserves onGround from ordinary tile physics. True establishes
    // current platform support only; it never writes Y speed or sub-speed.
    if (currentSupport) onGround = true;
}

void Player::clearStormEagleSupport() {
    stormEagleSupportLatched_ = false;
    stormEagleSupportFrameActive_ = false;
}

}  // namespace mmx
