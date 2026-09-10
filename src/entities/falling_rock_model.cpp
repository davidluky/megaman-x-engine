// falling_rock_model.cpp - advances the bounded Sting falling-rock core.

#include "entities/falling_rock_model.h"

namespace mmx {

void FallingRockModel::reset(int worldXQ8_8, int worldYQ8_8) {
    worldXQ8_8_ = worldXQ8_8;
    worldYQ8_8_ = worldYQ8_8;
    sourceRawYVelocityQ8_8_ = 0;
    phase_ = FallingRockPhase::Suspended;
}

bool FallingRockModel::beginFall() {
    if (phase_ != FallingRockPhase::Suspended) return false;
    sourceRawYVelocityQ8_8_ = 0;
    phase_ = FallingRockPhase::Falling;
    return true;
}

FallingRockFrameResult FallingRockModel::tick() {
    FallingRockFrameResult result;
    if (phase_ != FallingRockPhase::Falling) return result;

    // The source velocity uses negative-down while its captured world Y
    // advances positively. Apply the current magnitude, then accelerate.
    result.deltaYQ8_8 = -sourceRawYVelocityQ8_8_;
    worldYQ8_8_ += result.deltaYQ8_8;
    sourceRawYVelocityQ8_8_ += kSourceFallAccelerationQ8_8;
    return result;
}

bool FallingRockModel::markImpact() {
    if (phase_ != FallingRockPhase::Falling) return false;
    phase_ = FallingRockPhase::Impacted;
    sourceRawYVelocityQ8_8_ = 0;
    return true;
}

bool FallingRockModel::closeUnknown() {
    if (phase_ == FallingRockPhase::Closed) return false;
    phase_ = FallingRockPhase::Closed;
    sourceRawYVelocityQ8_8_ = 0;
    return true;
}

} // namespace mmx
