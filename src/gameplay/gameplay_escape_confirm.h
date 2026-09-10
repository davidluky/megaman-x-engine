// gameplay_escape_confirm.h - manages the in-game escape confirmation overlay.
// Boundary: scene-exit UX only; progression and save state remain elsewhere.

#pragma once

#include "ui/menu_flow.h"

namespace mmx::gameplay_escape_confirm {

struct State {
    EscapeConfirmState dialog;
    int inputDelay = 0;
};

inline bool isOpen(const State& state) {
    return state.dialog.open;
}

inline bool yesSelected(const State& state) {
    return state.dialog.choice == EscapeConfirmChoice::Yes;
}

} // namespace mmx::gameplay_escape_confirm
