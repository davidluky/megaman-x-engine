// gameplay_debug_overlay.h - draws gameplay diagnostics and trace overlays.
// Boundary: diagnostic display only; gameplay state remains source-backed.

#pragma once

namespace mmx::gameplay_debug_overlay {

struct State {
    bool showCollision = false;
    bool showHud = false;
};

inline void toggleCollision(State& state) {
    state.showCollision = !state.showCollision;
}

inline void toggleHud(State& state) {
    state.showHud = !state.showHud;
}

} // namespace mmx::gameplay_debug_overlay
