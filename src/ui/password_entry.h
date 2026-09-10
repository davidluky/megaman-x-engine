// password_entry.h - pure state transitions for the MMX1 password grid.
// Source contract: 4 columns x 3 rows, digits 1..8, Y +1, B -1.

#pragma once

#include "data/mmx1_password.h"

#include <algorithm>
#include <cstddef>

namespace mmx {

inline constexpr int kPasswordGridColumns = 4;
inline constexpr int kPasswordGridRows = 3;

struct PasswordEntryState {
    mmx1_password::PasswordDigits digits = {
        1, 1, 1, 1,
        1, 1, 1, 1,
        1, 1, 1, 1,
    };
    int cursor = 0;
    bool invalid = false;
};

inline void resetPasswordEntry(PasswordEntryState& state) {
    state = PasswordEntryState{};
}

inline void movePasswordCursor(PasswordEntryState& state, int columnDelta, int rowDelta) {
    const int column = state.cursor % kPasswordGridColumns;
    const int row = state.cursor / kPasswordGridColumns;
    const int nextColumn = std::clamp(column + columnDelta, 0, kPasswordGridColumns - 1);
    const int nextRow = std::clamp(row + rowDelta, 0, kPasswordGridRows - 1);
    state.cursor = nextRow * kPasswordGridColumns + nextColumn;
}

inline void cyclePasswordDigit(PasswordEntryState& state, int direction) {
    if (direction == 0) {
        return;
    }
    int& digit = state.digits[static_cast<std::size_t>(state.cursor)];
    digit += direction > 0 ? 1 : -1;
    if (digit > 8) {
        digit = 1;
    } else if (digit < 1) {
        digit = 8;
    }
    state.invalid = false;
}

} // namespace mmx
