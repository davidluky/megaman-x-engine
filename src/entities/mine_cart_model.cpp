// mine_cart_model.cpp - advances the bounded source-backed mine-cart model.

#include "entities/mine_cart_model.h"

#include <algorithm>
#include <cstdlib>

namespace mmx {

void MineCartModel::reset(int worldXQ8_8, int worldYQ8_8) {
    phase_ = MineCartPhase::Dormant;
    closureCause_ = MineCartClosureCause::None;
    worldXQ8_8_ = worldXQ8_8;
    worldYQ8_8_ = worldYQ8_8;
    horizontalVelocityQ8_8_ = 0;
}

bool MineCartModel::wake() {
    if (phase_ != MineCartPhase::Dormant) return false;
    phase_ = MineCartPhase::Awake;
    return true;
}

bool MineCartModel::beginMotion(MineCartDirection direction) {
    if (phase_ != MineCartPhase::Awake) return false;
    horizontalVelocityQ8_8_ =
        static_cast<int>(direction) * kInitialHorizontalVelocityQ8_8;
    phase_ = MineCartPhase::Moving;
    return true;
}

bool MineCartModel::turnAtTrackBoundary(MineCartDirection direction) {
    if (phase_ != MineCartPhase::Moving ||
        std::abs(horizontalVelocityQ8_8_) !=
            kHorizontalVelocityCeilingQ8_8) {
        return false;
    }
    horizontalVelocityQ8_8_ =
        static_cast<int>(direction) * kHorizontalVelocityCeilingQ8_8;
    return true;
}

MineCartFrameResult MineCartModel::tick(
    const MineCartRiderInput& rider,
    const MineCartTrackStep& track
) {
    MineCartFrameResult result;
    result.riderHorizontalIntent =
        (rider.rightHeld && !rider.leftHeld) ? 1 :
        (rider.leftHeld && !rider.rightHeld) ? -1 : 0;
    result.riderJumpRequested = rider.jumpPressed;

    if (phase_ != MineCartPhase::Moving) return result;

    result.cartDeltaXQ8_8 = horizontalVelocityQ8_8_;
    result.cartDeltaYQ8_8 = track.verticalDeltaQ8_8;
    worldXQ8_8_ += result.cartDeltaXQ8_8;
    worldYQ8_8_ += result.cartDeltaYQ8_8;

    if (track.advanceHorizontalAcceleration) {
        const int direction = horizontalVelocityQ8_8_ < 0 ? -1 : 1;
        const int nextMagnitude = std::min(
            std::abs(horizontalVelocityQ8_8_) +
                kHorizontalAccelerationQ8_8,
            kHorizontalVelocityCeilingQ8_8
        );
        horizontalVelocityQ8_8_ = direction * nextMagnitude;
    }
    return result;
}

bool MineCartModel::close(MineCartClosureCause cause) {
    if (phase_ == MineCartPhase::Closed ||
        cause == MineCartClosureCause::None) {
        return false;
    }
    phase_ = MineCartPhase::Closed;
    closureCause_ = cause;
    horizontalVelocityQ8_8_ = 0;
    return true;
}

bool MineCartModel::closeOffscreen() {
    return close(MineCartClosureCause::Offscreen);
}

bool MineCartModel::closeUnknown() {
    return close(MineCartClosureCause::Unknown);
}

} // namespace mmx
