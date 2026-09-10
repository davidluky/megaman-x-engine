// storm_eagle_platform.h - measured Storm Eagle platform source-motion seam.
//
// This source-motion seam is intentionally independent of raylib, Player,
// scene event clocks, rendering, and culling. The source observation and its
// 770-row contract live in knowledge_base/mmx1/stage_objects/
// storm_eagle_platform.json; native scene integration remains a later step.

#pragma once

#include <cstdint>

namespace mmx {

struct StormEaglePlatformState {
    std::uint16_t x = 0;
    std::uint16_t y = 0;
    std::uint16_t previousX = 0;
    std::uint16_t previousY = 0;
    std::uint16_t timer = 0;
    std::uint8_t flags = 0;
    std::uint8_t state = 0;
    std::uint8_t substate = 0;
};

class StormEaglePlatform {
public:
    StormEaglePlatform() = default;

    static StormEaglePlatform fromSourceFlag(std::uint8_t flags);

    void tickMotion();

    const StormEaglePlatformState& state() const { return state_; }

private:
    StormEaglePlatformState state_{};
};

}  // namespace mmx
