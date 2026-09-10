// gameplay_sprite_test.h - provides focused sprite inspection hooks for gameplay.
// Boundary: test/diagnostic support only; runtime rendering stays in lanes.

#pragma once

namespace mmx::gameplay_sprite_test {

struct State {
    bool active = false;
    int frame = 0;
    int delay = 0;
};

inline void reset(State& state) {
    state.frame = 0;
    state.delay = 0;
}

inline void toggle(State& state) {
    state.active = !state.active;
    reset(state);
}

} // namespace mmx::gameplay_sprite_test
