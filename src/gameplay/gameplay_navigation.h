// gameplay_navigation.h - handles gameplay scene transitions and navigation exits.
// Boundary: chooses scene handoffs; UI scenes own their own presentation.

#pragma once

namespace mmx::gameplay_navigation {

struct State {
    bool restartRequested = false;
    bool returnToTitle = false;
    bool returnToStageSelect = false;
};

inline void requestRestart(State& state) {
    state.restartRequested = true;
}

inline void clearRestart(State& state) {
    state.restartRequested = false;
}

inline void requestTitle(State& state) {
    state.returnToTitle = true;
}

inline void requestStageSelect(State& state) {
    state.returnToStageSelect = true;
}

inline bool wantsRestart(const State& state) {
    return state.restartRequested;
}

inline bool wantsTitle(const State& state) {
    return state.returnToTitle;
}

inline bool wantsStageSelect(const State& state) {
    return state.returnToStageSelect;
}

} // namespace mmx::gameplay_navigation
