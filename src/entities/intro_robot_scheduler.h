// Declares the evidence-bounded Intro Highway OID 0x29 scheduler kernel.
// Runtime activation, source-global RNG ownership, and constructor effects
// remain caller-owned integration work.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace mmx {

// Evidence-bounded scheduler kernel for Intro Highway OID 0x29. The caller
// supplies one post-step RNG low byte per source RNG call. This class owns no
// PRNG and deliberately does not choose activation, placement, constructor
// geometry, allocator policy, animation, collision, damage, or pixels.
class IntroRobotScheduler {
public:
    static constexpr std::uint8_t kIdleAction = 2;
    static constexpr std::uint8_t kAction4 = 4;
    static constexpr std::uint8_t kAction6 = 6;
    static constexpr std::uint16_t kIdleBlockUpdates = 60;
    static constexpr std::uint16_t kAction4Updates = 134;
    static constexpr std::uint16_t kAction6Updates = 91;

    using RngLowProvider = std::function<std::uint8_t()>;

    struct ConstructorCall {
        std::uint8_t action = 0;
        std::uint8_t variant = 0;
        std::uint8_t callIndex = 0;
    };

    struct StepResult {
        bool advanced = false;
        bool providerMissing = false;
        bool enteredAction = false;
        bool enteredIdle = false;
        std::uint16_t activeUpdate = 0;
        std::array<ConstructorCall, 2> calls{};
        std::size_t callCount = 0;
    };

    void reset();
    StepResult update(const RngLowProvider& rngLow);

    std::uint8_t action() const { return action_; }
    std::uint8_t variant() const { return variant_; }
    std::uint8_t helperState() const { return helperState_; }
    std::uint16_t waitTimer() const { return waitTimer_; }
    std::uint16_t actionAge() const { return actionAge_; }
    const StepResult& lastStep() const { return lastStep_; }

private:
    static std::uint8_t nextHelperState(std::uint8_t state,
                                        std::uint8_t postStepLow);
    void appendCall(std::uint8_t callIndex);
    void enterIdle();

    std::uint8_t action_ = kIdleAction;
    std::uint8_t variant_ = 0;
    std::uint8_t helperState_ = 0;
    std::uint16_t waitTimer_ = kIdleBlockUpdates;
    std::uint16_t actionAge_ = 0;
    StepResult lastStep_{};
};

} // namespace mmx
