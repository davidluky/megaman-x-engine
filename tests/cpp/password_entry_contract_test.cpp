#include "ui/password_entry.h"

#include <algorithm>
#include <cassert>

using namespace mmx;

int main() {
    PasswordEntryState state;
    assert(std::all_of(
        state.digits.begin(), state.digits.end(),
        [](int digit) { return digit == 1; }
    ));
    assert(state.cursor == 0);
    assert(!state.invalid);

    movePasswordCursor(state, -1, 0);
    movePasswordCursor(state, 0, -1);
    assert(state.cursor == 0);

    movePasswordCursor(state, 1, 0);
    assert(state.cursor == 1);
    movePasswordCursor(state, 0, 1);
    assert(state.cursor == 5);
    movePasswordCursor(state, 8, 8);
    assert(state.cursor == 11);
    movePasswordCursor(state, 1, 1);
    assert(state.cursor == 11);

    state.invalid = true;
    cyclePasswordDigit(state, 1);
    assert(state.digits[11] == 2);
    assert(!state.invalid);
    for (int i = 0; i < 7; ++i) {
        cyclePasswordDigit(state, 1);
    }
    assert(state.digits[11] == 1);
    cyclePasswordDigit(state, -1);
    assert(state.digits[11] == 8);
    cyclePasswordDigit(state, 0);
    assert(state.digits[11] == 8);

    resetPasswordEntry(state);
    assert(state.cursor == 0);
    assert(state.digits[11] == 1);
    assert(!state.invalid);

    return 0;
}
