#include "app/input.h"

#include <cstdlib>

namespace mmx {

// Static member initialization
bool Input::left_ = false;
bool Input::right_ = false;
bool Input::up_ = false;
bool Input::down_ = false;
bool Input::jumpHeld_ = false;
bool Input::dashHeld_ = false;
bool Input::shootHeld_ = false;
bool Input::shootPressed_ = false;
bool Input::jumpPressed_ = false;
bool Input::dashPressed_ = false;
bool Input::shootReleased_ = false;
bool Input::pausePressed_ = false;
bool Input::confirmPressed_ = false;
bool Input::cancelPressed_ = false;
bool Input::weaponNextPressed_ = false;
bool Input::weaponPrevPressed_ = false;
bool Input::subTankPressed_ = false;
bool Input::restartStagePressed_ = false;
bool Input::debug1Pressed_ = false;
bool Input::debug2Pressed_ = false;
bool Input::debug3Pressed_ = false;
bool Input::debug4Pressed_ = false;
bool Input::upgradeGrant1Pressed_ = false;
bool Input::upgradeGrant2Pressed_ = false;
bool Input::upgradeGrant3Pressed_ = false;
bool Input::upgradeGrant4Pressed_ = false;
KeyboardKey Input::lastKeyboardKeyPressed_ = KeyboardKey::None;
GamepadButton Input::lastGamepadButtonPressed_ = GamepadButton::None;
bool Input::prevShootHeld_ = false;
bool Input::gamepadConnected_ = false;
bool Input::scriptedRight_ = false;
bool Input::scriptedLeft_ = false;
bool Input::scriptedUp_ = false;
bool Input::scriptedDown_ = false;
bool Input::scriptedJumpHeld_ = false;
bool Input::scriptedDashHeld_ = false;
bool Input::scriptedShootHeld_ = false;
bool Input::scriptedJumpPress_ = false;
bool Input::scriptedDashPress_ = false;
bool Input::scriptedConfirmPress_ = false;
bool Input::scriptedCancelPress_ = false;
bool Input::scriptedPausePress_ = false;
bool Input::scriptedShootRelease_ = false;
bool Input::scriptedWeaponNextPress_ = false;
bool Input::scriptedWeaponPrevPress_ = false;
bool Input::scriptedWeaponNextForce_ = false;
bool Input::scriptedShootHeldPrev_ = false;
InputBindings Input::bindings_ = InputBindings::defaults();

namespace {

bool exclusiveAutotestInput() {
    const char* value = std::getenv("MMX_AUTOTEST_EXCLUSIVE_INPUT");
    return value && value[0] == '1' && value[1] == '\0';
}

bool isBoundKeyPressed(const KeyboardBinding& binding) {
    const int primary = raylibKeyCode(binding.primary);
    const int secondary = raylibKeyCode(binding.secondary);
    return (primary != 0 && IsKeyPressed(primary)) ||
           (secondary != 0 && IsKeyPressed(secondary));
}

bool isBoundKeyDown(const KeyboardBinding& binding) {
    const int primary = raylibKeyCode(binding.primary);
    const int secondary = raylibKeyCode(binding.secondary);
    return (primary != 0 && IsKeyDown(primary)) ||
           (secondary != 0 && IsKeyDown(secondary));
}

bool isBoundGamepadButtonPressed(const GamepadBinding& binding, int gamepad) {
    if (gamepad < 0) return false;
    const int primary = raylibGamepadButtonCode(binding.primary);
    const int secondary = raylibGamepadButtonCode(binding.secondary);
    return (primary >= 0 && IsGamepadButtonPressed(gamepad, primary)) ||
           (secondary >= 0 && IsGamepadButtonPressed(gamepad, secondary));
}

bool isBoundGamepadButtonDown(const GamepadBinding& binding, int gamepad) {
    if (gamepad < 0) return false;
    const int primary = raylibGamepadButtonCode(binding.primary);
    const int secondary = raylibGamepadButtonCode(binding.secondary);
    return (primary >= 0 && IsGamepadButtonDown(gamepad, primary)) ||
           (secondary >= 0 && IsGamepadButtonDown(gamepad, secondary));
}

bool isActionPressed(InputAction action, int gamepad) {
    return isBoundKeyPressed(Input::bindings().keyboardBinding(action)) ||
           isBoundGamepadButtonPressed(Input::bindings().gamepadBinding(action), gamepad);
}

bool isActionDown(InputAction action, int gamepad) {
    return isBoundKeyDown(Input::bindings().keyboardBinding(action)) ||
           isBoundGamepadButtonDown(Input::bindings().gamepadBinding(action), gamepad);
}

} // namespace

void Input::setBindings(const InputBindings& bindings) {
    if (bindings.hasSafeBindings()) {
        bindings_ = bindings;
    }
}

const InputBindings& Input::bindings() {
    return bindings_;
}

bool Input::rebindKeyboard(InputAction action, KeyboardKey key) {
    return bindings_.rebindPrimary(action, key);
}

void Input::resetBindingsToDefault() {
    bindings_ = InputBindings::defaults();
}

KeyboardKey Input::consumeLastKeyboardKeyPressed() {
    KeyboardKey key = lastKeyboardKeyPressed_;
    lastKeyboardKeyPressed_ = KeyboardKey::None;
    return key;
}

GamepadButton Input::consumeLastGamepadButtonPressed() {
    GamepadButton button = lastGamepadButtonPressed_;
    lastGamepadButtonPressed_ = GamepadButton::None;
    return button;
}

int Input::activeGamepad() {
    // raylib supports up to 4 gamepads (0-3). Use the first connected one.
    for (int i = 0; i < 4; i++) {
        if (IsGamepadAvailable(i)) return i;
    }
    return -1;
}

void Input::poll() {
    // Called once per render frame. Latches press events so they aren't lost
    // on high-refresh monitors where many frames have no physics tick.

    if (exclusiveAutotestInput()) {
        gamepadConnected_ = false;
        lastKeyboardKeyPressed_ = KeyboardKey::None;
        lastGamepadButtonPressed_ = GamepadButton::None;
        prevShootHeld_ = false;
        return;
    }

    int gp = activeGamepad();
    gamepadConnected_ = (gp >= 0);

    lastKeyboardKeyPressed_ = KeyboardKey::None;
    for (KeyboardKey key : allKeyboardKeys()) {
        const int code = raylibKeyCode(key);
        if (code != 0 && IsKeyPressed(code)) {
            lastKeyboardKeyPressed_ = key;
            break;
        }
    }

    lastGamepadButtonPressed_ = GamepadButton::None;
    if (gp >= 0) {
        for (GamepadButton button : allGamepadButtons()) {
            const int code = raylibGamepadButtonCode(button);
            if (code >= 0 && IsGamepadButtonPressed(gp, code)) {
                lastGamepadButtonPressed_ = button;
                break;
            }
        }
    }

    // Latch action presses using the current bindings (defaults live in InputBindings).
    if (isActionPressed(InputAction::Jump, gp)) {
        jumpPressed_ = true;
    }

    // Dash press
    if (isActionPressed(InputAction::Dash, gp)) {
        dashPressed_ = true;
    }

    // Shoot press/release edge detection
    bool shootNow = isActionDown(InputAction::Shoot, gp);
    if (!prevShootHeld_ && shootNow) {
        shootPressed_ = true;
    }
    if (prevShootHeld_ && !shootNow) {
        shootReleased_ = true;
    }
    prevShootHeld_ = shootNow;

    // Pause press
    if (isActionPressed(InputAction::Pause, gp)) {
        pausePressed_ = true;
    }

    // Confirm press
    if (isActionPressed(InputAction::Confirm, gp)) {
        confirmPressed_ = true;
    }

    // Cancel has its own validated binding to prevent gameplay presses from
    // leaking into scene transitions.
    if (isActionPressed(InputAction::Cancel, gp)) {
        cancelPressed_ = true;
    }

    // Weapon cycling actions
    if (isActionPressed(InputAction::WeaponPrev, gp)) {
        weaponPrevPressed_ = true;
    }
    if (isActionPressed(InputAction::WeaponNext, gp)) {
        weaponNextPressed_ = true;
    }

    // Sub-Tank action
    if (isActionPressed(InputAction::SubTank, gp)) {
        subTankPressed_ = true;
    }

    if (isActionPressed(InputAction::RestartStage, gp)) {
        restartStagePressed_ = true;
    }

    // Debug toggles
    if (IsKeyPressed(KEY_F1)) debug1Pressed_ = true;
    if (IsKeyPressed(KEY_F2)) debug2Pressed_ = true;
    if (IsKeyPressed(KEY_F3)) debug3Pressed_ = true;
    if (IsKeyPressed(KEY_F4)) debug4Pressed_ = true;

    // U66: capsule-grant debug keys (1=legs 2=body 3=helmet 4=buster),
    // accepting both the numpad and top-row keys.
    if (IsKeyPressed(KEY_KP_1) || IsKeyPressed(KEY_ONE)) upgradeGrant1Pressed_ = true;
    if (IsKeyPressed(KEY_KP_2) || IsKeyPressed(KEY_TWO)) upgradeGrant2Pressed_ = true;
    if (IsKeyPressed(KEY_KP_3) || IsKeyPressed(KEY_THREE)) upgradeGrant3Pressed_ = true;
    if (IsKeyPressed(KEY_KP_4) || IsKeyPressed(KEY_FOUR)) upgradeGrant4Pressed_ = true;
}

void Input::update() {
    // Called once per physics tick. Reads held state for continuous actions.

    const bool scriptedOnly = exclusiveAutotestInput();
    int gp = scriptedOnly ? -1 : activeGamepad();

    // Directional input: configured keyboard/gamepad bindings + left stick.
    left_  = !scriptedOnly && isActionDown(InputAction::Left, gp);
    right_ = !scriptedOnly && isActionDown(InputAction::Right, gp);
    up_    = !scriptedOnly && isActionDown(InputAction::Up, gp);
    down_  = !scriptedOnly && isActionDown(InputAction::Down, gp);

    if (gp >= 0) {
        // Left stick (deadzone 0.3)
        float axisX = GetGamepadAxisMovement(gp, GAMEPAD_AXIS_LEFT_X);
        float axisY = GetGamepadAxisMovement(gp, GAMEPAD_AXIS_LEFT_Y);
        if (axisX < -0.3f) left_  = true;
        if (axisX >  0.3f) right_ = true;
        if (axisY < -0.3f) up_    = true;
        if (axisY >  0.3f) down_  = true;
    }

    // Held buttons
    jumpHeld_  = !scriptedOnly && isActionDown(InputAction::Jump, gp);
    dashHeld_  = !scriptedOnly && isActionDown(InputAction::Dash, gp);
    shootHeld_ = !scriptedOnly && isActionDown(InputAction::Shoot, gp);

    // Scripted autotest input normally layers on live controls. Gates can set
    // MMX_AUTOTEST_EXCLUSIVE_INPUT=1 to make replay independent of peripherals.
    if (scriptedRight_)      right_     = true;
    if (scriptedLeft_)       left_      = true;
    if (scriptedUp_)         up_        = true;
    if (scriptedDown_)       down_      = true;
    if (scriptedJumpHeld_)   jumpHeld_  = true;
    if (scriptedDashHeld_)   dashHeld_  = true;
    if (scriptedShootHeld_)  shootHeld_ = true;

    // One-shot scripted presses: fire this tick, then self-clear.
    if (scriptedJumpPress_)  { jumpPressed_ = true; scriptedJumpPress_ = false; }
    if (scriptedDashPress_)  { dashPressed_ = true; scriptedDashPress_ = false; }
    if (scriptedConfirmPress_) { confirmPressed_ = true; scriptedConfirmPress_ = false; }
    if (scriptedCancelPress_)  { cancelPressed_ = true; scriptedCancelPress_ = false; }
    if (scriptedPausePress_)   { pausePressed_ = true; scriptedPausePress_ = false; }
    if (scriptedShootRelease_) { shootReleased_ = true; scriptedShootRelease_ = false; }
    if (scriptedWeaponNextPress_) { weaponNextPressed_ = true; scriptedWeaponNextPress_ = false; }
    if (scriptedWeaponPrevPress_) { weaponPrevPressed_ = true; scriptedWeaponPrevPress_ = false; }

    // Synthesize shoot press/release on scripted shoot edges, so autotest
    // drivers fire naturally without explicit press/release calls.
    if (!scriptedShootHeldPrev_ && scriptedShootHeld_) shootPressed_ = true;
    if (scriptedShootHeldPrev_ && !scriptedShootHeld_) shootReleased_ = true;
    scriptedShootHeldPrev_ = scriptedShootHeld_;
}

void Input::clear() {
    jumpPressed_ = false;
    dashPressed_ = false;
    shootPressed_ = false;
    shootReleased_ = false;
    pausePressed_ = false;
    confirmPressed_ = false;
    cancelPressed_ = false;
    weaponNextPressed_ = false;
    weaponPrevPressed_ = false;
    subTankPressed_ = false;
    restartStagePressed_ = false;
    debug1Pressed_ = false;
    debug2Pressed_ = false;
    debug3Pressed_ = false;
    debug4Pressed_ = false;
    upgradeGrant1Pressed_ = false;
    upgradeGrant2Pressed_ = false;
    upgradeGrant3Pressed_ = false;
    upgradeGrant4Pressed_ = false;
}

} // namespace mmx
