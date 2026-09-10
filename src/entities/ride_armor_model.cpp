// ride_armor_model.cpp - advances the source-backed Ride Armor model.

#include "entities/ride_armor_model.h"

#include <algorithm>

namespace mmx {

void RideArmorModel::park(int worldXQ8_8, int worldYQ8_8) {
    phase_ = RideArmorPhase::Parked;
    health_ = kMaxHealth;
    invulnerabilityFrames_ = 0;
    mountFramesRemaining_ = 0;
    destructionFramesRemaining_ = 0;
    worldXQ8_8_ = worldXQ8_8;
    worldYQ8_8_ = worldYQ8_8;
    walkPhase_ = 0;
    dashPhase_ = 0;
    dashActive_ = false;
    facingRight_ = true;
    jumpPending_ = false;
    jumpProfileIndex_ = -1;
    airControlDelay_ = 0;
    fallVelocityQ8_8_ = 0;
    punchStartupFrames_ = 0;
    punchActiveFrames_ = 0;
    zeroHealthSettlePending_ = false;
}

void RideArmorModel::resolveWorldPosition(int worldXQ8_8,
                                          int worldYQ8_8) {
    worldXQ8_8_ = worldXQ8_8;
    worldYQ8_8_ = worldYQ8_8;
}

bool RideArmorModel::beginMount() {
    if (phase_ != RideArmorPhase::Parked || health_ <= 0) return false;
    phase_ = RideArmorPhase::Mounting;
    mountFramesRemaining_ = kMountFrames;
    return true;
}

bool RideArmorModel::mounted() const {
    return phase_ == RideArmorPhase::Mounting ||
           phase_ == RideArmorPhase::Ready ||
           phase_ == RideArmorPhase::Airborne ||
           phase_ == RideArmorPhase::Destroying;
}

bool RideArmorModel::controlsEnabled() const {
    return phase_ == RideArmorPhase::Ready ||
           phase_ == RideArmorPhase::Airborne;
}

int RideArmorModel::destructionCountdownSteps() const {
    if (phase_ != RideArmorPhase::Destroying) return 0;
    return (destructionFramesRemaining_ + kDestructionStepFrames - 1) /
           kDestructionStepFrames;
}

int RideArmorModel::horizontalDelta(const RideArmorInput& input) {
    const int direction =
        (input.rightHeld && !input.leftHeld) ? 1 :
        (input.leftHeld && !input.rightHeld) ? -1 : 0;
    if (direction == 0) {
        dashActive_ = false;
        dashPhase_ = 0;
        return 0;
    }

    facingRight_ = direction > 0;

    if (input.dashHeld) {
        if (!dashActive_) {
            // Source frame 7088 retains one walk pulse; 7089 enters dash.
            dashActive_ = true;
            dashPhase_ =
                static_cast<int>(kObservedDashCycleQ8_8.size());
            const int walk = walkPhase_ == 0 ? 256 : 512;
            walkPhase_ ^= 1;
            return direction * walk;
        }
        if (dashPhase_ ==
            static_cast<int>(kObservedDashCycleQ8_8.size())) {
            dashPhase_ = 0;
            return direction * 1024;
        }
        const int delta = kObservedDashCycleQ8_8[dashPhase_];
        dashPhase_ = (dashPhase_ + 1) %
                     static_cast<int>(kObservedDashCycleQ8_8.size());
        return direction * delta;
    }

    dashActive_ = false;
    dashPhase_ = 0;
    const int delta = walkPhase_ == 0 ? 256 : 512;
    walkPhase_ ^= 1;
    return direction * delta;
}

int RideArmorModel::verticalDelta(bool onGround) {
    if (jumpPending_) {
        jumpPending_ = false;
        jumpProfileIndex_ = 0;
        airControlDelay_ = kAirControlDelayFrames;
    }

    if (jumpProfileIndex_ >= 0 &&
        jumpProfileIndex_ < static_cast<int>(kObservedJumpRiseQ8_8.size())) {
        const int delta = kObservedJumpRiseQ8_8[jumpProfileIndex_++];
        if (jumpProfileIndex_ ==
            static_cast<int>(kObservedJumpRiseQ8_8.size())) {
            fallVelocityQ8_8_ = 0;
        }
        return delta;
    }

    if (onGround) {
        phase_ = RideArmorPhase::Ready;
        fallVelocityQ8_8_ = 0;
        jumpProfileIndex_ = -1;
        return 0;
    }

    // The observed post-apex deltas advance in quarter-pixel steps.
    fallVelocityQ8_8_ = std::min(fallVelocityQ8_8_ + 64, 1280);
    return fallVelocityQ8_8_;
}

void RideArmorModel::tickPunch(bool acceptedThisFrame) {
    if (acceptedThisFrame) return;
    if (punchStartupFrames_ > 0) {
        punchStartupFrames_--;
        if (punchStartupFrames_ == 0) {
            punchActiveFrames_ = kPunchActiveFrames;
        }
        return;
    }
    if (punchActiveFrames_ > 0) {
        punchActiveFrames_--;
    }
}

RideArmorFrameResult RideArmorModel::tick(const RideArmorInput& input,
                                          bool onGround) {
    RideArmorFrameResult result;

    if (zeroHealthSettlePending_) {
        zeroHealthSettlePending_ = false;
        invulnerabilityFrames_ = 0;
    } else if (invulnerabilityFrames_ > 0) {
        invulnerabilityFrames_--;
    }

    if (phase_ == RideArmorPhase::Removed ||
        phase_ == RideArmorPhase::Parked) {
        tickPunch(false);
        return result;
    }

    if (phase_ == RideArmorPhase::Destroying) {
        if (destructionFramesRemaining_ > 0) {
            destructionFramesRemaining_--;
        }
        if (destructionFramesRemaining_ == 0) {
            phase_ = RideArmorPhase::Removed;
            result.objectRemoved = true;
        }
        tickPunch(false);
        return result;
    }

    if (phase_ == RideArmorPhase::Mounting) {
        if (mountFramesRemaining_ > 0) {
            mountFramesRemaining_--;
        }
        if (mountFramesRemaining_ == 0) {
            phase_ = RideArmorPhase::Ready;
            result.mountCompleted = true;
        }
        tickPunch(false);
        return result;
    }

    if (input.upHeld && input.jumpPressed) {
        phase_ = RideArmorPhase::Parked;
        dashActive_ = false;
        jumpPending_ = false;
        jumpProfileIndex_ = -1;
        punchStartupFrames_ = 0;
        punchActiveFrames_ = 0;
        result.dismounted = true;
        return result;
    }

    bool acceptedPunch = false;
    if (input.punchPressed && !punchBusy()) {
        punchStartupFrames_ = kPunchStartupFrames;
        acceptedPunch = true;
        result.punchStarted = true;
    }
    tickPunch(acceptedPunch);

    bool jumpAcceptedThisFrame = false;
    if (phase_ == RideArmorPhase::Ready && input.jumpPressed) {
        phase_ = RideArmorPhase::Airborne;
        jumpPending_ = true;
        jumpAcceptedThisFrame = true;
    }

    if (phase_ == RideArmorPhase::Airborne) {
        if (!jumpAcceptedThisFrame) {
            result.deltaYQ8_8 = verticalDelta(onGround);
            if (airControlDelay_ > 0) {
                airControlDelay_--;
            } else {
                result.deltaXQ8_8 = horizontalDelta(input);
            }
        }
    } else {
        result.deltaXQ8_8 = horizontalDelta(input);
    }

    worldXQ8_8_ += result.deltaXQ8_8;
    worldYQ8_8_ += result.deltaYQ8_8;
    return result;
}

RideArmorDamageResult RideArmorModel::takeIncomingDamage(int amount) {
    RideArmorDamageResult result;
    if (amount <= 0 || phase_ == RideArmorPhase::Parked ||
        phase_ == RideArmorPhase::Mounting ||
        phase_ == RideArmorPhase::Destroying ||
        phase_ == RideArmorPhase::Removed) {
        return result;
    }
    if (invulnerabilityFrames_ > 0) return result;

    result.accepted = true;
    result.hurtFlash = true;

    if (health_ > 0) {
        health_ = std::max(0, health_ - amount);
        invulnerabilityFrames_ = kInvulnerabilityFrames;
        if (health_ == 0) {
            zeroHealthSettlePending_ = true;
        }
        return result;
    }

    phase_ = RideArmorPhase::Destroying;
    destructionFramesRemaining_ = kDestructionFrames;
    result.protectionLost = true;
    result.pilotEjected = true;
    result.playerDamage = 1;
    return result;
}

} // namespace mmx
