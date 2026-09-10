#pragma once

#include <array>
#include <optional>
#include <string>

namespace mmx {

enum class InputAction {
    Left = 0,
    Right,
    Up,
    Down,
    Jump,
    Dash,
    Shoot,
    Pause,
    Confirm,
    Cancel,
    WeaponPrev,
    WeaponNext,
    SubTank,
    RestartStage,
    Count
};

enum class KeyboardKey {
    None = 0,
    Left,
    Right,
    Up,
    Down,
    Z,
    Space,
    X,
    LeftShift,
    C,
    LeftAlt,
    P,
    Enter,
    Escape,
    Q,
    E,
    R,
    Tab,
    A,
    D,
    W,
    S
};

enum class GamepadButton {
    None = 0,
    DpadLeft,
    DpadRight,
    DpadUp,
    DpadDown,
    FaceDown,
    FaceRight,
    FaceLeft,
    FaceUp,
    LeftShoulder,
    RightShoulder,
    LeftTrigger,
    RightTrigger,
    Start,
    Select
};

struct KeyboardBinding {
    KeyboardKey primary = KeyboardKey::None;
    KeyboardKey secondary = KeyboardKey::None;
};

struct GamepadBinding {
    GamepadButton primary = GamepadButton::None;
    GamepadButton secondary = GamepadButton::None;
};

class InputBindings {
public:
    static InputBindings defaults();

    const KeyboardBinding& keyboardBinding(InputAction action) const;
    KeyboardBinding& keyboardBinding(InputAction action);
    const GamepadBinding& gamepadBinding(InputAction action) const;
    GamepadBinding& gamepadBinding(InputAction action);
    bool rebindPrimary(InputAction action, KeyboardKey key);
    bool rebindGamepadPrimary(InputAction action, GamepadButton button);
    bool hasSafeKeyboardBindings() const;
    bool hasSafeGamepadBindings() const;
    bool hasSafeBindings() const;

private:
    std::array<KeyboardBinding, static_cast<size_t>(InputAction::Count)> keyboard_ = {};
    std::array<GamepadBinding, static_cast<size_t>(InputAction::Count)> gamepad_ = {};
};

const std::array<InputAction, static_cast<size_t>(InputAction::Count)>& allInputActions();
const std::array<KeyboardKey, 21>& allKeyboardKeys();
const std::array<GamepadButton, 14>& allGamepadButtons();

const char* inputActionName(InputAction action);
const char* inputActionLabel(InputAction action);
std::optional<InputAction> inputActionFromName(const std::string& name);

const char* keyboardKeyName(KeyboardKey key);
const char* keyboardKeyLabel(KeyboardKey key);
std::string keyboardActionLabel(const InputBindings& bindings, InputAction action);
std::string keyboardActionHint(const InputBindings& bindings, InputAction action, const char* purpose);
std::string keyboardHorizontalHint(const InputBindings& bindings, const char* purpose);
std::string keyboardVerticalHint(const InputBindings& bindings, const char* purpose);
std::string keyboardDirectionalHint(const InputBindings& bindings, const char* purpose);
std::optional<KeyboardKey> keyboardKeyFromName(const std::string& name);
std::optional<KeyboardKey> keyboardKeyFromRaylib(int raylibKey);
int raylibKeyCode(KeyboardKey key);

const char* gamepadButtonName(GamepadButton button);
const char* gamepadButtonLabel(GamepadButton button);
std::string gamepadActionLabel(const InputBindings& bindings, InputAction action);
std::string gamepadActionHint(const InputBindings& bindings, InputAction action, const char* purpose);
std::optional<GamepadButton> gamepadButtonFromName(const std::string& name);
int raylibGamepadButtonCode(GamepadButton button);

} // namespace mmx
