#include "app/input_bindings.h"

#include "raylib.h"

#include <algorithm>
#include <iterator>

namespace mmx {
namespace {

constexpr size_t actionIndex(InputAction action) {
    return static_cast<size_t>(action);
}

constexpr std::array<InputAction, static_cast<size_t>(InputAction::Count)> kActions = {
    InputAction::Left,
    InputAction::Right,
    InputAction::Up,
    InputAction::Down,
    InputAction::Jump,
    InputAction::Dash,
    InputAction::Shoot,
    InputAction::Pause,
    InputAction::Confirm,
    InputAction::Cancel,
    InputAction::WeaponPrev,
    InputAction::WeaponNext,
    InputAction::SubTank,
    InputAction::RestartStage,
};

constexpr std::array<KeyboardKey, 21> kKeys = {
    KeyboardKey::Left,
    KeyboardKey::Right,
    KeyboardKey::Up,
    KeyboardKey::Down,
    KeyboardKey::Z,
    KeyboardKey::Space,
    KeyboardKey::X,
    KeyboardKey::LeftShift,
    KeyboardKey::C,
    KeyboardKey::LeftAlt,
    KeyboardKey::P,
    KeyboardKey::Enter,
    KeyboardKey::Escape,
    KeyboardKey::Q,
    KeyboardKey::E,
    KeyboardKey::R,
    KeyboardKey::Tab,
    KeyboardKey::A,
    KeyboardKey::D,
    KeyboardKey::W,
    KeyboardKey::S,
};

constexpr std::array<GamepadButton, 14> kGamepadButtons = {
    GamepadButton::DpadLeft,
    GamepadButton::DpadRight,
    GamepadButton::DpadUp,
    GamepadButton::DpadDown,
    GamepadButton::FaceDown,
    GamepadButton::FaceRight,
    GamepadButton::FaceLeft,
    GamepadButton::FaceUp,
    GamepadButton::LeftShoulder,
    GamepadButton::RightShoulder,
    GamepadButton::LeftTrigger,
    GamepadButton::RightTrigger,
    GamepadButton::Start,
    GamepadButton::Select,
};

bool bindingContains(const KeyboardBinding& binding, KeyboardKey key) {
    return key != KeyboardKey::None && (binding.primary == key || binding.secondary == key);
}

bool bindingContains(const GamepadBinding& binding, GamepadButton button) {
    return button != GamepadButton::None &&
           (binding.primary == button || binding.secondary == button);
}

bool actionsOverlap(const InputBindings& bindings, InputAction a, InputAction b) {
    const auto& left = bindings.keyboardBinding(a);
    const auto& right = bindings.keyboardBinding(b);
    return bindingContains(right, left.primary) ||
           bindingContains(right, left.secondary);
}

bool actionGroupHasDuplicates(const InputBindings& bindings, const InputAction* actions, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        const auto& binding = bindings.keyboardBinding(actions[i]);
        for (KeyboardKey key : {binding.primary, binding.secondary}) {
            if (key == KeyboardKey::None) {
                continue;
            }
            for (size_t j = i + 1; j < count; ++j) {
                if (bindingContains(bindings.keyboardBinding(actions[j]), key)) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool gamepadActionGroupHasDuplicates(const InputBindings& bindings, const InputAction* actions, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        const auto& binding = bindings.gamepadBinding(actions[i]);
        for (GamepadButton button : {binding.primary, binding.secondary}) {
            if (button == GamepadButton::None) {
                continue;
            }
            for (size_t j = i + 1; j < count; ++j) {
                if (bindingContains(bindings.gamepadBinding(actions[j]), button)) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace

InputBindings InputBindings::defaults() {
    InputBindings bindings;
    bindings.keyboardBinding(InputAction::Left) = {KeyboardKey::Left, KeyboardKey::None};
    bindings.keyboardBinding(InputAction::Right) = {KeyboardKey::Right, KeyboardKey::None};
    bindings.keyboardBinding(InputAction::Up) = {KeyboardKey::Up, KeyboardKey::None};
    bindings.keyboardBinding(InputAction::Down) = {KeyboardKey::Down, KeyboardKey::None};
    // SNES face buttons mapped to keyboard: B(jump)=S, A(dash)=A, X/Y(shoot)=X/Z.
    bindings.keyboardBinding(InputAction::Jump) = {KeyboardKey::S, KeyboardKey::Space};
    bindings.keyboardBinding(InputAction::Dash) = {KeyboardKey::A, KeyboardKey::LeftShift};
    bindings.keyboardBinding(InputAction::Shoot) = {KeyboardKey::X, KeyboardKey::Z};
    bindings.keyboardBinding(InputAction::Pause) = {KeyboardKey::P, KeyboardKey::Enter};
    bindings.keyboardBinding(InputAction::Confirm) = {KeyboardKey::Z, KeyboardKey::Enter};
    bindings.keyboardBinding(InputAction::Cancel) = {KeyboardKey::Escape, KeyboardKey::None};
    // Q / W mirror the original game's L / R weapon shoulder buttons.
    bindings.keyboardBinding(InputAction::WeaponPrev) = {KeyboardKey::Q, KeyboardKey::None};
    bindings.keyboardBinding(InputAction::WeaponNext) = {KeyboardKey::W, KeyboardKey::None};
    bindings.keyboardBinding(InputAction::SubTank) = {KeyboardKey::Tab, KeyboardKey::None};
    bindings.keyboardBinding(InputAction::RestartStage) = {KeyboardKey::R, KeyboardKey::None};

    bindings.gamepadBinding(InputAction::Left) = {GamepadButton::DpadLeft, GamepadButton::None};
    bindings.gamepadBinding(InputAction::Right) = {GamepadButton::DpadRight, GamepadButton::None};
    bindings.gamepadBinding(InputAction::Up) = {GamepadButton::DpadUp, GamepadButton::None};
    bindings.gamepadBinding(InputAction::Down) = {GamepadButton::DpadDown, GamepadButton::None};
    // SNES-style pad defaults on an Xbox-style controller:
    // B(jump)=A, A(dash)=B, X/Y(shoot)=X/Y.
    bindings.gamepadBinding(InputAction::Jump) = {GamepadButton::FaceDown, GamepadButton::None};
    bindings.gamepadBinding(InputAction::Dash) = {GamepadButton::FaceRight, GamepadButton::FaceLeft};
    bindings.gamepadBinding(InputAction::Shoot) = {GamepadButton::FaceUp, GamepadButton::RightTrigger};
    bindings.gamepadBinding(InputAction::Pause) = {GamepadButton::Start, GamepadButton::None};
    bindings.gamepadBinding(InputAction::Confirm) = {GamepadButton::FaceDown, GamepadButton::None};
    bindings.gamepadBinding(InputAction::Cancel) = {GamepadButton::FaceRight, GamepadButton::None};
    bindings.gamepadBinding(InputAction::WeaponPrev) = {GamepadButton::LeftShoulder, GamepadButton::None};
    bindings.gamepadBinding(InputAction::WeaponNext) = {GamepadButton::RightShoulder, GamepadButton::None};
    bindings.gamepadBinding(InputAction::SubTank) = {GamepadButton::Select, GamepadButton::None};
    bindings.gamepadBinding(InputAction::RestartStage) = {GamepadButton::None, GamepadButton::None};
    return bindings;
}

const KeyboardBinding& InputBindings::keyboardBinding(InputAction action) const {
    return keyboard_[actionIndex(action)];
}

KeyboardBinding& InputBindings::keyboardBinding(InputAction action) {
    return keyboard_[actionIndex(action)];
}

const GamepadBinding& InputBindings::gamepadBinding(InputAction action) const {
    return gamepad_[actionIndex(action)];
}

GamepadBinding& InputBindings::gamepadBinding(InputAction action) {
    return gamepad_[actionIndex(action)];
}

bool InputBindings::rebindPrimary(InputAction action, KeyboardKey key) {
    if (key == KeyboardKey::None) {
        return false;
    }
    InputBindings candidate = *this;
    candidate.keyboardBinding(action).primary = key;
    candidate.keyboardBinding(action).secondary = KeyboardKey::None;
    if (!candidate.hasSafeKeyboardBindings()) {
        return false;
    }
    *this = candidate;
    return true;
}

bool InputBindings::rebindGamepadPrimary(InputAction action, GamepadButton button) {
    if (button == GamepadButton::None) {
        return false;
    }
    InputBindings candidate = *this;
    candidate.gamepadBinding(action).primary = button;
    candidate.gamepadBinding(action).secondary = GamepadButton::None;
    if (!candidate.hasSafeGamepadBindings()) {
        return false;
    }
    *this = candidate;
    return true;
}

bool InputBindings::hasSafeKeyboardBindings() const {
    constexpr InputAction gameplayActions[] = {
        InputAction::Left,
        InputAction::Right,
        InputAction::Up,
        InputAction::Down,
        InputAction::Jump,
        InputAction::Dash,
        InputAction::Shoot,
        InputAction::Pause,
        InputAction::WeaponPrev,
        InputAction::WeaponNext,
        InputAction::SubTank,
        InputAction::RestartStage,
    };
    constexpr InputAction menuActions[] = {
        InputAction::Left,
        InputAction::Right,
        InputAction::Up,
        InputAction::Down,
        InputAction::Confirm,
        InputAction::Cancel,
    };

    for (InputAction action : kActions) {
        const auto& binding = keyboardBinding(action);
        if (binding.primary == KeyboardKey::None) {
            return false;
        }
        if (binding.primary == binding.secondary) {
            return false;
        }
    }

    if (actionGroupHasDuplicates(*this, gameplayActions, std::size(gameplayActions))) {
        return false;
    }
    if (actionGroupHasDuplicates(*this, menuActions, std::size(menuActions))) {
        return false;
    }

    for (InputAction action : kActions) {
        if (action != InputAction::Cancel && actionsOverlap(*this, InputAction::Cancel, action)) {
            return false;
        }
    }
    return true;
}

bool InputBindings::hasSafeGamepadBindings() const {
    constexpr InputAction gameplayActions[] = {
        InputAction::Left,
        InputAction::Right,
        InputAction::Up,
        InputAction::Down,
        InputAction::Jump,
        InputAction::Dash,
        InputAction::Shoot,
        InputAction::Pause,
        InputAction::WeaponPrev,
        InputAction::WeaponNext,
        InputAction::SubTank,
        InputAction::RestartStage,
    };
    constexpr InputAction menuActions[] = {
        InputAction::Left,
        InputAction::Right,
        InputAction::Up,
        InputAction::Down,
        InputAction::Confirm,
        InputAction::Cancel,
    };

    for (InputAction action : kActions) {
        const auto& binding = gamepadBinding(action);
        if (binding.primary == GamepadButton::None) {
            if (action == InputAction::RestartStage && binding.secondary == GamepadButton::None) {
                continue;
            }
            return false;
        }
        if (binding.primary == binding.secondary) {
            return false;
        }
    }

    if (gamepadActionGroupHasDuplicates(*this, gameplayActions, std::size(gameplayActions))) {
        return false;
    }
    if (gamepadActionGroupHasDuplicates(*this, menuActions, std::size(menuActions))) {
        return false;
    }
    return true;
}

bool InputBindings::hasSafeBindings() const {
    return hasSafeKeyboardBindings() && hasSafeGamepadBindings();
}

const std::array<InputAction, static_cast<size_t>(InputAction::Count)>& allInputActions() {
    return kActions;
}

const std::array<KeyboardKey, 21>& allKeyboardKeys() {
    return kKeys;
}

const std::array<GamepadButton, 14>& allGamepadButtons() {
    return kGamepadButtons;
}

const char* inputActionName(InputAction action) {
    switch (action) {
        case InputAction::Left: return "left";
        case InputAction::Right: return "right";
        case InputAction::Up: return "up";
        case InputAction::Down: return "down";
        case InputAction::Jump: return "jump";
        case InputAction::Dash: return "dash";
        case InputAction::Shoot: return "shoot";
        case InputAction::Pause: return "pause";
        case InputAction::Confirm: return "confirm";
        case InputAction::Cancel: return "cancel";
        case InputAction::WeaponPrev: return "weapon_prev";
        case InputAction::WeaponNext: return "weapon_next";
        case InputAction::SubTank: return "sub_tank";
        case InputAction::RestartStage: return "restart_stage";
        case InputAction::Count: break;
    }
    return "";
}

const char* inputActionLabel(InputAction action) {
    switch (action) {
        case InputAction::Left: return "MOVE LEFT";
        case InputAction::Right: return "MOVE RIGHT";
        case InputAction::Up: return "MOVE UP";
        case InputAction::Down: return "MOVE DOWN";
        case InputAction::Jump: return "JUMP";
        case InputAction::Dash: return "DASH";
        case InputAction::Shoot: return "SHOOT";
        case InputAction::Pause: return "PAUSE";
        case InputAction::Confirm: return "CONFIRM";
        case InputAction::Cancel: return "CANCEL";
        case InputAction::WeaponPrev: return "WEAPON PREV";
        case InputAction::WeaponNext: return "WEAPON NEXT";
        case InputAction::SubTank: return "SUB-TANK";
        case InputAction::RestartStage: return "RESTART STAGE";
        case InputAction::Count: break;
    }
    return "";
}

std::optional<InputAction> inputActionFromName(const std::string& name) {
    for (InputAction action : kActions) {
        if (name == inputActionName(action)) {
            return action;
        }
    }
    return std::nullopt;
}

const char* keyboardKeyName(KeyboardKey key) {
    switch (key) {
        case KeyboardKey::None: return "NONE";
        case KeyboardKey::Left: return "LEFT";
        case KeyboardKey::Right: return "RIGHT";
        case KeyboardKey::Up: return "UP";
        case KeyboardKey::Down: return "DOWN";
        case KeyboardKey::Z: return "Z";
        case KeyboardKey::Space: return "SPACE";
        case KeyboardKey::X: return "X";
        case KeyboardKey::LeftShift: return "LEFT_SHIFT";
        case KeyboardKey::C: return "C";
        case KeyboardKey::LeftAlt: return "LEFT_ALT";
        case KeyboardKey::P: return "P";
        case KeyboardKey::Enter: return "ENTER";
        case KeyboardKey::Escape: return "ESCAPE";
        case KeyboardKey::Q: return "Q";
        case KeyboardKey::E: return "E";
        case KeyboardKey::R: return "R";
        case KeyboardKey::Tab: return "TAB";
        case KeyboardKey::A: return "A";
        case KeyboardKey::D: return "D";
        case KeyboardKey::W: return "W";
        case KeyboardKey::S: return "S";
    }
    return "NONE";
}

const char* keyboardKeyLabel(KeyboardKey key) {
    switch (key) {
        case KeyboardKey::None: return "-";
        case KeyboardKey::Left: return "LEFT";
        case KeyboardKey::Right: return "RIGHT";
        case KeyboardKey::Up: return "UP";
        case KeyboardKey::Down: return "DOWN";
        case KeyboardKey::Z: return "Z";
        case KeyboardKey::Space: return "SPACE";
        case KeyboardKey::X: return "X";
        case KeyboardKey::LeftShift: return "SHIFT";
        case KeyboardKey::C: return "C";
        case KeyboardKey::LeftAlt: return "ALT";
        case KeyboardKey::P: return "P";
        case KeyboardKey::Enter: return "ENTER";
        case KeyboardKey::Escape: return "ESC";
        case KeyboardKey::Q: return "Q";
        case KeyboardKey::E: return "E";
        case KeyboardKey::R: return "R";
        case KeyboardKey::Tab: return "TAB";
        case KeyboardKey::A: return "A";
        case KeyboardKey::D: return "D";
        case KeyboardKey::W: return "W";
        case KeyboardKey::S: return "S";
    }
    return "-";
}

std::string keyboardActionLabel(const InputBindings& bindings, InputAction action) {
    return keyboardKeyLabel(bindings.keyboardBinding(action).primary);
}

std::string keyboardActionHint(const InputBindings& bindings, InputAction action, const char* purpose) {
    return keyboardActionLabel(bindings, action) + ":" + purpose;
}

std::string keyboardHorizontalHint(const InputBindings& bindings, const char* purpose) {
    return keyboardActionLabel(bindings, InputAction::Left) + "/" +
           keyboardActionLabel(bindings, InputAction::Right) + ":" + purpose;
}

std::string keyboardVerticalHint(const InputBindings& bindings, const char* purpose) {
    return keyboardActionLabel(bindings, InputAction::Up) + "/" +
           keyboardActionLabel(bindings, InputAction::Down) + ":" + purpose;
}

std::string keyboardDirectionalHint(const InputBindings& bindings, const char* purpose) {
    const auto& left = bindings.keyboardBinding(InputAction::Left);
    const auto& right = bindings.keyboardBinding(InputAction::Right);
    const auto& up = bindings.keyboardBinding(InputAction::Up);
    const auto& down = bindings.keyboardBinding(InputAction::Down);

    std::string label;
    if (left.primary == KeyboardKey::Left &&
        right.primary == KeyboardKey::Right &&
        up.primary == KeyboardKey::Up &&
        down.primary == KeyboardKey::Down) {
        label = "ARROWS";
    } else if (left.primary == KeyboardKey::A &&
               right.primary == KeyboardKey::D &&
               up.primary == KeyboardKey::W &&
               down.primary == KeyboardKey::S) {
        label = "WASD";
    } else {
        label = keyboardActionLabel(bindings, InputAction::Left) + "/" +
                keyboardActionLabel(bindings, InputAction::Right) + "/" +
                keyboardActionLabel(bindings, InputAction::Up) + "/" +
                keyboardActionLabel(bindings, InputAction::Down);
    }
    return label + ":" + purpose;
}

std::optional<KeyboardKey> keyboardKeyFromName(const std::string& name) {
    for (KeyboardKey key : kKeys) {
        if (name == keyboardKeyName(key)) {
            return key;
        }
    }
    return std::nullopt;
}

std::optional<KeyboardKey> keyboardKeyFromRaylib(int raylibKey) {
    for (KeyboardKey key : kKeys) {
        if (raylibKeyCode(key) == raylibKey) {
            return key;
        }
    }
    return std::nullopt;
}

int raylibKeyCode(KeyboardKey key) {
    switch (key) {
        case KeyboardKey::None: return 0;
        case KeyboardKey::Left: return KEY_LEFT;
        case KeyboardKey::Right: return KEY_RIGHT;
        case KeyboardKey::Up: return KEY_UP;
        case KeyboardKey::Down: return KEY_DOWN;
        case KeyboardKey::Z: return KEY_Z;
        case KeyboardKey::Space: return KEY_SPACE;
        case KeyboardKey::X: return KEY_X;
        case KeyboardKey::LeftShift: return KEY_LEFT_SHIFT;
        case KeyboardKey::C: return KEY_C;
        case KeyboardKey::LeftAlt: return KEY_LEFT_ALT;
        case KeyboardKey::P: return KEY_P;
        case KeyboardKey::Enter: return KEY_ENTER;
        case KeyboardKey::Escape: return KEY_ESCAPE;
        case KeyboardKey::Q: return KEY_Q;
        case KeyboardKey::E: return KEY_E;
        case KeyboardKey::R: return KEY_R;
        case KeyboardKey::Tab: return KEY_TAB;
        case KeyboardKey::A: return KEY_A;
        case KeyboardKey::D: return KEY_D;
        case KeyboardKey::W: return KEY_W;
        case KeyboardKey::S: return KEY_S;
    }
    return 0;
}

const char* gamepadButtonName(GamepadButton button) {
    switch (button) {
        case GamepadButton::None: return "NONE";
        case GamepadButton::DpadLeft: return "DPAD_LEFT";
        case GamepadButton::DpadRight: return "DPAD_RIGHT";
        case GamepadButton::DpadUp: return "DPAD_UP";
        case GamepadButton::DpadDown: return "DPAD_DOWN";
        case GamepadButton::FaceDown: return "FACE_DOWN";
        case GamepadButton::FaceRight: return "FACE_RIGHT";
        case GamepadButton::FaceLeft: return "FACE_LEFT";
        case GamepadButton::FaceUp: return "FACE_UP";
        case GamepadButton::LeftShoulder: return "LEFT_SHOULDER";
        case GamepadButton::RightShoulder: return "RIGHT_SHOULDER";
        case GamepadButton::LeftTrigger: return "LEFT_TRIGGER";
        case GamepadButton::RightTrigger: return "RIGHT_TRIGGER";
        case GamepadButton::Start: return "START";
        case GamepadButton::Select: return "SELECT";
    }
    return "NONE";
}

const char* gamepadButtonLabel(GamepadButton button) {
    switch (button) {
        case GamepadButton::None: return "-";
        case GamepadButton::DpadLeft: return "D-LEFT";
        case GamepadButton::DpadRight: return "D-RIGHT";
        case GamepadButton::DpadUp: return "D-UP";
        case GamepadButton::DpadDown: return "D-DOWN";
        case GamepadButton::FaceDown: return "A";
        case GamepadButton::FaceRight: return "B";
        case GamepadButton::FaceLeft: return "X";
        case GamepadButton::FaceUp: return "Y";
        case GamepadButton::LeftShoulder: return "LB";
        case GamepadButton::RightShoulder: return "RB";
        case GamepadButton::LeftTrigger: return "LT";
        case GamepadButton::RightTrigger: return "RT";
        case GamepadButton::Start: return "START";
        case GamepadButton::Select: return "BACK";
    }
    return "-";
}

std::string gamepadActionLabel(const InputBindings& bindings, InputAction action) {
    return gamepadButtonLabel(bindings.gamepadBinding(action).primary);
}

std::string gamepadActionHint(const InputBindings& bindings, InputAction action, const char* purpose) {
    return gamepadActionLabel(bindings, action) + ":" + purpose;
}

std::optional<GamepadButton> gamepadButtonFromName(const std::string& name) {
    for (GamepadButton button : kGamepadButtons) {
        if (name == gamepadButtonName(button)) {
            return button;
        }
    }
    return std::nullopt;
}

int raylibGamepadButtonCode(GamepadButton button) {
    switch (button) {
        case GamepadButton::None: return -1;
        case GamepadButton::DpadLeft: return GAMEPAD_BUTTON_LEFT_FACE_LEFT;
        case GamepadButton::DpadRight: return GAMEPAD_BUTTON_LEFT_FACE_RIGHT;
        case GamepadButton::DpadUp: return GAMEPAD_BUTTON_LEFT_FACE_UP;
        case GamepadButton::DpadDown: return GAMEPAD_BUTTON_LEFT_FACE_DOWN;
        case GamepadButton::FaceDown: return GAMEPAD_BUTTON_RIGHT_FACE_DOWN;
        case GamepadButton::FaceRight: return GAMEPAD_BUTTON_RIGHT_FACE_RIGHT;
        case GamepadButton::FaceLeft: return GAMEPAD_BUTTON_RIGHT_FACE_LEFT;
        case GamepadButton::FaceUp: return GAMEPAD_BUTTON_RIGHT_FACE_UP;
        case GamepadButton::LeftShoulder: return GAMEPAD_BUTTON_LEFT_TRIGGER_1;
        case GamepadButton::RightShoulder: return GAMEPAD_BUTTON_RIGHT_TRIGGER_1;
        case GamepadButton::LeftTrigger: return GAMEPAD_BUTTON_LEFT_TRIGGER_2;
        case GamepadButton::RightTrigger: return GAMEPAD_BUTTON_RIGHT_TRIGGER_2;
        case GamepadButton::Start: return GAMEPAD_BUTTON_MIDDLE_RIGHT;
        case GamepadButton::Select: return GAMEPAD_BUTTON_MIDDLE_LEFT;
    }
    return -1;
}

} // namespace mmx
