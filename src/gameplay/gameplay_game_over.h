// gameplay_game_over.h - source-backed final-stock transition timing.
// Boundary: destination routing stays with GameplayScene.

#pragma once

#include <array>
#include <functional>

namespace mmx::gameplay_game_over {

inline constexpr int kPasswordRevealFrames = 60;
inline constexpr int kHelmetAnimationFrameTicks = 5;
inline constexpr int kHelmetAnimationFrames = 7;
inline constexpr int kHelmetAnimationTicks =
    kHelmetAnimationFrameTicks * kHelmetAnimationFrames;
inline constexpr int kHelmetMinimumWaitFrames = 45;

struct State {
    bool active = false;
    int timer = 0;
};

inline void reset(State& state) {
    state.active = false;
    state.timer = 0;
}

inline void begin(State& state) {
    state.active = true;
    state.timer = 0;
}

inline bool advance(State& state) {
    if (!state.active) return false;
    state.timer++;
    return true;
}

inline bool passwordVisible(const State& state) {
    return state.active && state.timer > kPasswordRevealFrames;
}

inline bool passwordVisible(bool active, int timer) {
    return active && timer > kPasswordRevealFrames;
}

inline std::array<int, 12> makeDecorativeGridDigits(
    const std::function<int()>& rollDigit
) {
    std::array<int, 12> digits = {};
    for (int& digit : digits) {
        digit = rollDigit();
        if (digit < 1) digit = 1;
        if (digit > 8) digit = 8;
    }
    return digits;
}

inline void initializeDecorativeHelmetTicks(
    std::array<int, 12>& ticks,
    const std::function<int()>& rollDelay
) {
    for (int& tick : ticks) {
        int delay = rollDelay();
        if (delay < 0) delay = 0;
        if (delay > 240) delay = 240;
        tick = -(kPasswordRevealFrames + delay);
    }
}

inline void advanceDecorativeHelmetTicks(
    std::array<int, 12>& ticks,
    const std::function<int()>& rollDelay
) {
    for (int& tick : ticks) {
        tick++;
        if (tick < kHelmetAnimationTicks) continue;

        int delay = rollDelay();
        if (delay < 0) delay = 0;
        if (delay > 179) delay = 179;
        tick = -(kHelmetMinimumWaitFrames + delay);
    }
}

inline int decorativeHelmetFrame(int tick) {
    if (tick < 0) return 0;
    const int frame = 1 + tick / kHelmetAnimationFrameTicks;
    return frame > kHelmetAnimationFrames ? kHelmetAnimationFrames : frame;
}

} // namespace mmx::gameplay_game_over
