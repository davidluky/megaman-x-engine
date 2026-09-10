// moving_platform_model.h - bounded GC6 Storm moving-platform motion core.
// Scene adapters own path boundaries, collision, carry, culling, and art.

#pragma once

namespace mmx {

enum class MovingPlatformPhase {
    Dormant,
    Horizontal,
    Vertical,
    Closed
};

enum class MovingPlatformDirection {
    Left = -1,
    Right = 1
};

enum class MovingPlatformClosureCause {
    None,
    Offscreen,
    Unknown
};

struct MovingPlatformFrameResult {
    int deltaXQ8_8 = 0;
    int deltaYQ8_8 = 0;
};

class MovingPlatformModel {
public:
    static constexpr int kHorizontalSpeedQ8_8 = 384;
    static constexpr int kSourceVerticalVelocityRaw = -512;
    static constexpr int kVerticalPositionDeltaQ8_8 = 512;

    void reset(int worldXQ8_8, int worldYQ8_8);
    bool beginHorizontal(MovingPlatformDirection direction);
    bool reverseAtBoundary(MovingPlatformDirection direction);
    bool beginMeasuredVerticalPhase();
    MovingPlatformFrameResult tick();
    bool closeOffscreen();
    bool closeUnknown();

    MovingPlatformPhase phase() const { return phase_; }
    MovingPlatformClosureCause closureCause() const {
        return closureCause_;
    }
    int worldXQ8_8() const { return worldXQ8_8_; }
    int worldYQ8_8() const { return worldYQ8_8_; }
    int horizontalVelocityQ8_8() const {
        return horizontalVelocityQ8_8_;
    }
    bool active() const { return phase_ != MovingPlatformPhase::Closed; }

private:
    bool close(MovingPlatformClosureCause cause);

    MovingPlatformPhase phase_ = MovingPlatformPhase::Dormant;
    MovingPlatformClosureCause closureCause_ =
        MovingPlatformClosureCause::None;
    int worldXQ8_8_ = 0;
    int worldYQ8_8_ = 0;
    int horizontalVelocityQ8_8_ = 0;
};

} // namespace mmx
