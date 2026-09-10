// storm_eagle_platform.cpp - R96 source-backed Storm Eagle platform motion.

#include "entities/storm_eagle_platform.h"

#include <array>

namespace mmx {

namespace {

struct SourceInitializer {
    std::uint8_t flags;
    std::uint16_t x;
    std::uint16_t y;
    std::uint16_t timer;
    std::uint8_t state;
    std::uint8_t substate;
};

// Source authority: knowledge_base/mmx1/stage_objects/
// storm_eagle_platform.json, generated from the accepted R64/R92 records.
// Unknown flag bytes deliberately return the zero state: no placement is
// invented for unmeasured records.
constexpr std::array<SourceInitializer, 10> kSourceInitializers = {{
    {137, 624, 1760, 176, 4, 8},
    {136, 640, 1600, 160, 4, 6},
    {135, 528, 1584, 48, 4, 2},
    {134, 448, 1760, 96, 4, 0},
    {5, 624, 1376, 128, 2, 4},
    {4, 624, 1184, 320, 2, 4},
    {3, 624, 992, 512, 2, 4},
    {2, 560, 1120, 128, 2, 0},
    {1, 560, 1312, 320, 2, 0},
    {0, 560, 1504, 512, 2, 0},
}};

void applyInitializer(StormEaglePlatformState& state,
                      const SourceInitializer& source) {
    state.x = source.x;
    state.y = source.y;
    state.previousX = 0;
    state.previousY = 0;
    state.timer = source.timer;
    state.flags = source.flags;
    state.state = source.state;
    state.substate = source.substate;
}

void moveState2(StormEaglePlatformState& state) {
    switch (state.substate) {
    case 0:
        --state.y;  // 83:F05F
        break;
    case 2:
        ++state.x;  // 83:F066
        break;
    case 4:
        ++state.y;  // 83:F06D
        break;
    case 6:
        --state.x;  // 83:F074
        break;
    default:
        // The source dispatch only reaches even substates 0,2,4,6. An
        // invalid raw byte is left unchanged rather than assigned a law.
        break;
    }
}

void moveState4(StormEaglePlatformState& state) {
    switch (state.substate) {
    case 0:
        --state.y;  // 83:F0A9
        break;
    case 2:
        ++state.x;  // 83:F0BD
        --state.y;
        break;
    case 4:
        ++state.x;  // 83:F0D6
        break;
    case 6:
        ++state.y;  // 83:F0EA
        break;
    case 8:
        --state.x;  // 83:F0FE
        break;
    default:
        // The source dispatch only reaches even substates 0,2,4,6,8.
        break;
    }
}

void transitionState2(StormEaglePlatformState& state) {
    state.substate = static_cast<std::uint8_t>((state.substate + 2) & 6);
    switch (state.substate) {
    case 0:
    case 4:
        state.timer = 512;  // 83:F07D, new substate 0/4
        break;
    case 2:
    case 6:
        state.timer = 64;  // 83:F07D, new substate 2/6
        break;
    default:
        state.timer = 0;
        break;
    }
}

void transitionState4(StormEaglePlatformState& state) {
    switch (state.substate) {
    case 0:
        state.substate = 2;
        state.timer = 128;  // 83:F0A9
        break;
    case 2:
        state.substate = 4;
        state.timer = 64;  // 83:F0BD
        break;
    case 4:
        state.substate = 6;
        state.timer = 224;  // 83:F0D6
        break;
    case 6:
        state.substate = 8;
        state.timer = 192;  // 83:F0EA
        break;
    case 8:
        state.substate = 0;
        state.timer = 96;  // 83:F0FE
        break;
    default:
        state.timer = 0;
        break;
    }
}

}  // namespace

StormEaglePlatform StormEaglePlatform::fromSourceFlag(std::uint8_t flags) {
    StormEaglePlatform platform;
    for (const auto& source : kSourceInitializers) {
        if (source.flags == flags) {
            applyInitializer(platform.state_, source);
            break;
        }
    }
    return platform;
}

void StormEaglePlatform::tickMotion() {
    // Source 83:F009's wrapper copies D+05/D+08 before the current substate
    // handler moves the words, then decrements the timer and transitions.
    state_.previousX = state_.x;
    state_.previousY = state_.y;

    if (state_.state == 2) {
        moveState2(state_);
    } else if (state_.state == 4) {
        moveState4(state_);
    } else {
        return;
    }

    if (state_.timer > 0) {
        --state_.timer;
    }
    if (state_.timer == 0) {
        if (state_.state == 2) {
            transitionState2(state_);
        } else {
            transitionState4(state_);
        }
    }
}

}  // namespace mmx
