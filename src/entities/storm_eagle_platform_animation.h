// storm_eagle_platform_animation.h - measured Storm Eagle pose timer seam.
//
// This header-only source seam owns only the eight-pose timer. Culling,
// rendering, motion, and scene clocks remain adapter responsibilities.

#pragma once

#include <cstdint>

namespace mmx {

class StormEaglePlatformAnimation {
public:
    static constexpr std::uint8_t kPoseCount = 8;
    static constexpr std::uint8_t kRecordDuration = 4;

    constexpr StormEaglePlatformAnimation() = default;

    static constexpr StormEaglePlatformAnimation fromSource() noexcept {
        return StormEaglePlatformAnimation{};
    }

    constexpr std::uint8_t pose() const noexcept { return pose_; }
    constexpr std::uint8_t timer() const noexcept { return timer_; }

    void tick() noexcept {
        if (timer_ > 1) {
            --timer_;
            return;
        }
        timer_ = kRecordDuration;
        pose_ = static_cast<std::uint8_t>((pose_ + 1) % kPoseCount);
    }

private:
    std::uint8_t pose_ = 0;
    std::uint8_t timer_ = kRecordDuration;
};

}  // namespace mmx
