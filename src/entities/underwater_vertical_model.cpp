// underwater_vertical_model.cpp - advances the bounded Launch vertical core.

#include "entities/underwater_vertical_model.h"

#include <algorithm>

namespace mmx {

void UnderwaterVerticalModel::reset(int worldYQ8_8) {
    worldYQ8_8_ = worldYQ8_8;
    verticalDeltaQ8_8_ = 0;
    lastAppliedDeltaYQ8_8_ = 0;
    active_ = false;
    apexTransitionAdjustmentApplied_ = false;
}

bool UnderwaterVerticalModel::beginHeldJump() {
    if (active_) return false;
    verticalDeltaQ8_8_ = kHeldJumpImpulseQ8_8;
    lastAppliedDeltaYQ8_8_ = 0;
    active_ = true;
    apexTransitionAdjustmentApplied_ = false;
    return true;
}

bool UnderwaterVerticalModel::beginMeasuredDescentAtCap() {
    if (active_) return false;
    verticalDeltaQ8_8_ = kFallSpeedCapQ8_8;
    lastAppliedDeltaYQ8_8_ = 0;
    active_ = true;
    apexTransitionAdjustmentApplied_ = false;
    return true;
}

bool UnderwaterVerticalModel::applyObservedApexTransitionAdjustment() {
    if (!active_ || lastAppliedDeltaYQ8_8_ <= 0 ||
        apexTransitionAdjustmentApplied_) {
        return false;
    }
    verticalDeltaQ8_8_ = std::min(
        verticalDeltaQ8_8_ + kApexTransitionExtraQ8_8,
        kFallSpeedCapQ8_8
    );
    apexTransitionAdjustmentApplied_ = true;
    return true;
}

UnderwaterVerticalFrameResult UnderwaterVerticalModel::tick() {
    UnderwaterVerticalFrameResult result;
    if (!active_) return result;

    result.deltaYQ8_8 = verticalDeltaQ8_8_;
    lastAppliedDeltaYQ8_8_ = result.deltaYQ8_8;
    worldYQ8_8_ += result.deltaYQ8_8;
    verticalDeltaQ8_8_ = std::min(
        verticalDeltaQ8_8_ + kAccelerationQ8_8,
        kFallSpeedCapQ8_8
    );
    return result;
}

void UnderwaterVerticalModel::settle() {
    verticalDeltaQ8_8_ = 0;
    lastAppliedDeltaYQ8_8_ = 0;
    active_ = false;
}

} // namespace mmx
