// moving_platform_model.cpp - advances the bounded Storm platform core.

#include "entities/moving_platform_model.h"

namespace mmx {

void MovingPlatformModel::reset(int worldXQ8_8, int worldYQ8_8) {
    phase_ = MovingPlatformPhase::Dormant;
    closureCause_ = MovingPlatformClosureCause::None;
    worldXQ8_8_ = worldXQ8_8;
    worldYQ8_8_ = worldYQ8_8;
    horizontalVelocityQ8_8_ = 0;
}

bool MovingPlatformModel::beginHorizontal(
    MovingPlatformDirection direction
) {
    if (phase_ != MovingPlatformPhase::Dormant) return false;
    horizontalVelocityQ8_8_ =
        static_cast<int>(direction) * kHorizontalSpeedQ8_8;
    phase_ = MovingPlatformPhase::Horizontal;
    return true;
}

bool MovingPlatformModel::reverseAtBoundary(
    MovingPlatformDirection direction
) {
    if (phase_ != MovingPlatformPhase::Horizontal) return false;
    horizontalVelocityQ8_8_ =
        static_cast<int>(direction) * kHorizontalSpeedQ8_8;
    return true;
}

bool MovingPlatformModel::beginMeasuredVerticalPhase() {
    if (phase_ != MovingPlatformPhase::Horizontal) return false;
    // The source retains raw xVelocity=-384 while X stops changing.  Keep
    // that evidence value observable but do not apply it during this phase.
    phase_ = MovingPlatformPhase::Vertical;
    return true;
}

MovingPlatformFrameResult MovingPlatformModel::tick() {
    MovingPlatformFrameResult result;
    if (phase_ == MovingPlatformPhase::Horizontal) {
        result.deltaXQ8_8 = horizontalVelocityQ8_8_;
    } else if (phase_ == MovingPlatformPhase::Vertical) {
        // MMX's raw vertical velocity uses positive-up sign convention while
        // the captured position field advances by +512 Q8.8 per frame.
        result.deltaYQ8_8 = kVerticalPositionDeltaQ8_8;
    } else {
        return result;
    }
    worldXQ8_8_ += result.deltaXQ8_8;
    worldYQ8_8_ += result.deltaYQ8_8;
    return result;
}

bool MovingPlatformModel::close(MovingPlatformClosureCause cause) {
    if (phase_ == MovingPlatformPhase::Closed ||
        cause == MovingPlatformClosureCause::None) {
        return false;
    }
    phase_ = MovingPlatformPhase::Closed;
    closureCause_ = cause;
    horizontalVelocityQ8_8_ = 0;
    return true;
}

bool MovingPlatformModel::closeOffscreen() {
    return close(MovingPlatformClosureCause::Offscreen);
}

bool MovingPlatformModel::closeUnknown() {
    return close(MovingPlatformClosureCause::Unknown);
}

} // namespace mmx
