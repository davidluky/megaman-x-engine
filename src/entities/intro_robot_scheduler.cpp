// Implements the source-measured idle/helper/action cadence and constructor
// call events without binding the kernel to live Enemy behavior.

#include "entities/intro_robot_scheduler.h"

namespace mmx {

void IntroRobotScheduler::reset() {
    action_ = kIdleAction;
    variant_ = 0;
    helperState_ = 0;
    waitTimer_ = kIdleBlockUpdates;
    actionAge_ = 0;
    lastStep_ = {};
}

std::uint8_t IntroRobotScheduler::nextHelperState(
    std::uint8_t state, std::uint8_t postStepLow) {
    if ((postStepLow & 1U) != 0) {
        const auto candidate = static_cast<std::uint16_t>(
            ((state & 0x80U) != 0 ? 0U : state) + 1U);
        return candidate >= 3U ? 0x81U
                               : static_cast<std::uint8_t>(candidate);
    }

    const auto base = static_cast<std::uint8_t>(
        (state & 0x80U) != 0 ? state : 0x80U);
    const auto candidate = static_cast<std::uint8_t>(base + 1U);
    return (candidate & 0x7FU) >= 3U ? 1U : candidate;
}

void IntroRobotScheduler::appendCall(std::uint8_t callIndex) {
    if (lastStep_.callCount >= lastStep_.calls.size()) {
        return;
    }
    lastStep_.calls[lastStep_.callCount++] = {action_, variant_, callIndex};
}

void IntroRobotScheduler::enterIdle() {
    action_ = kIdleAction;
    waitTimer_ = kIdleBlockUpdates;
    actionAge_ = 0;
    lastStep_.enteredIdle = true;
}

IntroRobotScheduler::StepResult IntroRobotScheduler::update(
    const RngLowProvider& rngLow) {
    lastStep_ = {};
    if (!rngLow) {
        lastStep_.providerMissing = true;
        return lastStep_;
    }
    lastStep_.advanced = true;

    if (action_ == kIdleAction) {
        if (waitTimer_ > 1) {
            --waitTimer_;
            return lastStep_;
        }

        waitTimer_ = 0;
        helperState_ = helperState_ == 0
                           ? 0x81U
                           : nextHelperState(helperState_, rngLow());
        if ((helperState_ & 0x80U) == 0) {
            waitTimer_ = kIdleBlockUpdates;
            return lastStep_;
        }

        // Action and variant are two distinct subsequent source RNG calls.
        const auto actionLow = rngLow();
        const auto variantLow = rngLow();
        action_ = static_cast<std::uint8_t>(4U + 2U * (actionLow & 1U));
        variant_ = static_cast<std::uint8_t>(variantLow & 7U);
        actionAge_ = 0;
        lastStep_.enteredAction = true;
        return lastStep_;
    }

    ++actionAge_;
    lastStep_.activeUpdate = actionAge_;

    if (action_ == kAction4) {
        if (actionAge_ == 80) {
            appendCall(1);
        } else if (actionAge_ == 123) {
            appendCall(1);
        }
        if (actionAge_ >= kAction4Updates) {
            enterIdle();
        }
    } else if (action_ == kAction6) {
        if (actionAge_ == 80) {
            appendCall(1);
            appendCall(2);
        }
        if (actionAge_ >= kAction6Updates) {
            enterIdle();
        }
    }

    return lastStep_;
}

} // namespace mmx
