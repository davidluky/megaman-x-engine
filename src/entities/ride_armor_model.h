// ride_armor_model.h - source-backed pilotable Ride Armor state machine.
// Boundary: pure fixed-tick model; scene adapters own collision and rendering.

#pragma once

#include <array>

namespace mmx {

enum class RideArmorPhase {
    Parked,
    Mounting,
    Ready,
    Airborne,
    Destroying,
    Removed
};

struct RideArmorInput {
    bool leftHeld = false;
    bool rightHeld = false;
    bool upHeld = false;
    bool jumpPressed = false;
    bool dashHeld = false;
    bool punchPressed = false;
};

struct RideArmorFrameResult {
    int deltaXQ8_8 = 0;
    int deltaYQ8_8 = 0;
    bool mountCompleted = false;
    bool dismounted = false;
    bool punchStarted = false;
    bool objectRemoved = false;
};

struct RideArmorDamageResult {
    bool accepted = false;
    bool hurtFlash = false;
    bool protectionLost = false;
    bool pilotEjected = false;
    int playerDamage = 0;
};

class RideArmorModel {
public:
    static constexpr int kMaxHealth = 16;
    static constexpr int kMountFrames = 41;
    static constexpr int kInvulnerabilityFrames = 121;
    static constexpr int kPunchStartupFrames = 2;
    static constexpr int kPunchActiveFrames = 40;
    static constexpr int kPunchDamage = 5;
    static constexpr int kDestructionSteps = 30;
    static constexpr int kDestructionStepFrames = 2;
    static constexpr int kDestructionFrames =
        kDestructionSteps * kDestructionStepFrames;
    static constexpr int kAirControlDelayFrames = 6;

    void park(int worldXQ8_8, int worldYQ8_8);
    // Scene collision owns geometry. Feed its resolved anchor back after a
    // requested model step instead of baking an unmeasured universal hull
    // into this source-timing model.
    void resolveWorldPosition(int worldXQ8_8, int worldYQ8_8);
    bool beginMount();
    RideArmorFrameResult tick(const RideArmorInput& input, bool onGround);
    RideArmorDamageResult takeIncomingDamage(int amount);

    RideArmorPhase phase() const { return phase_; }
    bool active() const { return phase_ != RideArmorPhase::Removed; }
    bool mounted() const;
    bool controlsEnabled() const;
    int health() const { return health_; }
    int invulnerabilityFrames() const { return invulnerabilityFrames_; }
    int destructionFramesRemaining() const {
        return destructionFramesRemaining_;
    }
    int destructionCountdownSteps() const;
    int worldXQ8_8() const { return worldXQ8_8_; }
    int worldYQ8_8() const { return worldYQ8_8_; }
    bool facingRight() const { return facingRight_; }
    bool punchBusy() const {
        return punchStartupFrames_ > 0 || punchActiveFrames_ > 0;
    }
    bool punchActive() const { return punchActiveFrames_ > 0; }
    int punchActiveFramesRemaining() const { return punchActiveFrames_; }

private:
    static constexpr std::array<int, 21> kObservedJumpRiseQ8_8 = {
        -1280, -1024, -1280, -1024, -1024, -1024, -1024,
        -768, -768, -768, -768, -512, -512, -512,
        -256, -512, -256, -256, 0, -256, 0
    };
    static constexpr std::array<int, 5> kObservedDashCycleQ8_8 = {
        0, 1024, 1024, 1024, 1024
    };

    int horizontalDelta(const RideArmorInput& input);
    int verticalDelta(bool onGround);
    void tickPunch(bool acceptedThisFrame);

    RideArmorPhase phase_ = RideArmorPhase::Parked;
    int health_ = kMaxHealth;
    int invulnerabilityFrames_ = 0;
    int mountFramesRemaining_ = 0;
    int destructionFramesRemaining_ = 0;
    int worldXQ8_8_ = 0;
    int worldYQ8_8_ = 0;
    int walkPhase_ = 0;
    int dashPhase_ = 0;
    bool dashActive_ = false;
    bool facingRight_ = true;
    bool jumpPending_ = false;
    int jumpProfileIndex_ = -1;
    int airControlDelay_ = 0;
    int fallVelocityQ8_8_ = 0;
    int punchStartupFrames_ = 0;
    int punchActiveFrames_ = 0;
    bool zeroHealthSettlePending_ = false;
};

} // namespace mmx
