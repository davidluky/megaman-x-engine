// falling_rock_model.h - bounded GC6 Sting falling-rock motion core.
// Scene adapters own trigger, collision, impact placement, and presentation.

#pragma once

namespace mmx {

enum class FallingRockPhase {
    Suspended,
    Falling,
    Impacted,
    Closed
};

struct FallingRockFrameResult {
    int deltaXQ8_8 = 0;
    int deltaYQ8_8 = 0;
};

class FallingRockModel {
public:
    static constexpr int kSourceFallAccelerationQ8_8 = -64;

    void reset(int worldXQ8_8, int worldYQ8_8);
    bool beginFall();
    FallingRockFrameResult tick();
    bool markImpact();
    bool closeUnknown();

    FallingRockPhase phase() const { return phase_; }
    int worldXQ8_8() const { return worldXQ8_8_; }
    int worldYQ8_8() const { return worldYQ8_8_; }
    int sourceRawYVelocityQ8_8() const {
        return sourceRawYVelocityQ8_8_;
    }
    bool active() const { return phase_ != FallingRockPhase::Closed; }

private:
    int worldXQ8_8_ = 0;
    int worldYQ8_8_ = 0;
    int sourceRawYVelocityQ8_8_ = 0;
    FallingRockPhase phase_ = FallingRockPhase::Suspended;
};

} // namespace mmx
