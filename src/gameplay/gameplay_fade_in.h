// gameplay_fade_in.h - manages gameplay entry fade timing and drawing.
// Boundary: presentation only; spawn and control state are owned by scene setup.

#pragma once

namespace mmx::gameplay_fade_in {

inline constexpr int kDuration = 30;

struct State {
    int timer = 0;
    int duration = kDuration;
};

inline void setActive(State& state, bool active) {
    state.timer = active ? state.duration : 0;
}

inline void tick(State& state) {
    if (state.timer > 0) state.timer--;
}

} // namespace mmx::gameplay_fade_in
