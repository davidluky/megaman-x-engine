// underwater_vertical_model.h - bounded GC6 Launch vertical-motion core.
// Scene adapters own water detection, collision, input policy, and presentation.

#pragma once

namespace mmx {

struct UnderwaterVerticalFrameResult {
    int deltaYQ8_8 = 0;
};

class UnderwaterVerticalModel {
public:
    static constexpr int kHeldJumpImpulseQ8_8 = -1330;
    static constexpr int kAccelerationQ8_8 = 33;
    static constexpr int kApexTransitionExtraQ8_8 = 8;
    static constexpr int kFallSpeedCapQ8_8 = 737;

    void reset(int worldYQ8_8);
    bool beginHeldJump();
    bool beginMeasuredDescentAtCap();
    bool applyObservedApexTransitionAdjustment();
    UnderwaterVerticalFrameResult tick();
    void settle();

    int worldYQ8_8() const { return worldYQ8_8_; }
    int verticalDeltaQ8_8() const { return verticalDeltaQ8_8_; }
    bool active() const { return active_; }
    bool apexTransitionAdjustmentApplied() const {
        return apexTransitionAdjustmentApplied_;
    }

private:
    int worldYQ8_8_ = 0;
    int verticalDeltaQ8_8_ = 0;
    int lastAppliedDeltaYQ8_8_ = 0;
    bool active_ = false;
    bool apexTransitionAdjustmentApplied_ = false;
};

} // namespace mmx
