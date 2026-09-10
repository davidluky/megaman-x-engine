// mine_cart_model.h - bounded GC6 rideable mine-cart motion model.
// Scene adapters own contact, terrain selection, collision, culling, and art.

#pragma once

namespace mmx {

enum class MineCartPhase {
    Dormant,
    Awake,
    Moving,
    Closed
};

enum class MineCartDirection {
    Left = -1,
    Right = 1
};

enum class MineCartClosureCause {
    None,
    Offscreen,
    Unknown
};

struct MineCartRiderInput {
    bool leftHeld = false;
    bool rightHeld = false;
    bool jumpPressed = false;
};

// Terrain semantics for source action2 values 2/4/6 are not localized.
// The adapter therefore supplies its resolved vertical step and decides
// whether the measured horizontal acceleration advances on this tick.
struct MineCartTrackStep {
    int verticalDeltaQ8_8 = 0;
    bool advanceHorizontalAcceleration = true;
};

struct MineCartFrameResult {
    int cartDeltaXQ8_8 = 0;
    int cartDeltaYQ8_8 = 0;
    int riderHorizontalIntent = 0;
    bool riderJumpRequested = false;
    bool cartSteeredByRider = false;
};

class MineCartModel {
public:
    static constexpr int kInitialHorizontalVelocityQ8_8 = 256;
    static constexpr int kHorizontalAccelerationQ8_8 = 32;
    static constexpr int kHorizontalVelocityCeilingQ8_8 = 1280;
    static constexpr int kObservedCullRelativeXMin = 323;
    static constexpr int kObservedCullRelativeXMax = 325;

    void reset(int worldXQ8_8, int worldYQ8_8);
    bool wake();
    bool beginMotion(MineCartDirection direction);
    bool turnAtTrackBoundary(MineCartDirection direction);
    MineCartFrameResult tick(
        const MineCartRiderInput& rider,
        const MineCartTrackStep& track
    );
    bool closeOffscreen();
    bool closeUnknown();

    MineCartPhase phase() const { return phase_; }
    MineCartClosureCause closureCause() const { return closureCause_; }
    int worldXQ8_8() const { return worldXQ8_8_; }
    int worldYQ8_8() const { return worldYQ8_8_; }
    int horizontalVelocityQ8_8() const {
        return horizontalVelocityQ8_8_;
    }
    bool active() const { return phase_ != MineCartPhase::Closed; }

private:
    bool close(MineCartClosureCause cause);

    MineCartPhase phase_ = MineCartPhase::Dormant;
    MineCartClosureCause closureCause_ = MineCartClosureCause::None;
    int worldXQ8_8_ = 0;
    int worldYQ8_8_ = 0;
    int horizontalVelocityQ8_8_ = 0;
};

} // namespace mmx
