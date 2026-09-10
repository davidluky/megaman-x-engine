// gameplay_pause_menu.h - manages pause and weapon-select menu state.
// Boundary: presentation/control flow only; weapon data stays in systems/weapon.

#pragma once

namespace mmx::gameplay_pause_menu {

struct State {
    bool paused = false;
    int weaponCursor = 0;
    int inputDelay = 0;
};

inline bool weaponMenuActive(const State& state) {
    return state.paused;
}

} // namespace mmx::gameplay_pause_menu
