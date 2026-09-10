#include "app/input_bindings.h"

#include <cassert>
#include <string>

int main() {
    mmx::InputBindings defaults = mmx::InputBindings::defaults();
    assert(defaults.hasSafeKeyboardBindings());
    assert(defaults.hasSafeGamepadBindings());
    assert(defaults.hasSafeBindings());

    assert(mmx::inputActionFromName("jump") == mmx::InputAction::Jump);
    assert(mmx::inputActionFromName("weapon_next") == mmx::InputAction::WeaponNext);
    assert(mmx::inputActionFromName("restart_stage") == mmx::InputAction::RestartStage);
    assert(!mmx::inputActionFromName("not_an_action").has_value());

    assert(mmx::keyboardKeyFromName("LEFT_SHIFT") == mmx::KeyboardKey::LeftShift);
    assert(mmx::keyboardKeyFromName("ESCAPE") == mmx::KeyboardKey::Escape);
    assert(mmx::keyboardKeyFromName("R") == mmx::KeyboardKey::R);
    assert(!mmx::keyboardKeyFromName("CTRL_ALT_DELETE").has_value());
    assert(mmx::keyboardKeyName(mmx::KeyboardKey::Space) == std::string("SPACE"));
    assert(mmx::keyboardKeyLabel(mmx::KeyboardKey::LeftAlt) == std::string("ALT"));
    assert(mmx::keyboardActionLabel(defaults, mmx::InputAction::Jump) == std::string("S"));
    assert(mmx::keyboardActionHint(defaults, mmx::InputAction::Confirm, "SELECT") ==
           std::string("Z:SELECT"));
    assert(mmx::keyboardHorizontalHint(defaults, "ADJUST") == std::string("LEFT/RIGHT:ADJUST"));
    assert(mmx::keyboardVerticalHint(defaults, "SELECT") == std::string("UP/DOWN:SELECT"));
    assert(mmx::keyboardDirectionalHint(defaults, "MOVE") == std::string("ARROWS:MOVE"));
    assert(mmx::gamepadButtonFromName("FACE_DOWN") == mmx::GamepadButton::FaceDown);
    assert(mmx::gamepadButtonFromName("RIGHT_TRIGGER") == mmx::GamepadButton::RightTrigger);
    assert(!mmx::gamepadButtonFromName("GUIDE").has_value());
    assert(mmx::gamepadButtonName(mmx::GamepadButton::LeftShoulder) == std::string("LEFT_SHOULDER"));
    assert(mmx::gamepadButtonLabel(mmx::GamepadButton::FaceDown) == std::string("A"));
    assert(mmx::allGamepadButtons().size() == 14);
    assert(defaults.gamepadBinding(mmx::InputAction::Jump).primary == mmx::GamepadButton::FaceDown);
    assert(mmx::gamepadActionLabel(defaults, mmx::InputAction::Jump) == std::string("A"));
    assert(mmx::gamepadActionHint(defaults, mmx::InputAction::Cancel, "BACK") ==
           std::string("B:BACK"));
    assert(defaults.gamepadBinding(mmx::InputAction::Dash).secondary == mmx::GamepadButton::FaceLeft);
    assert(defaults.gamepadBinding(mmx::InputAction::Shoot).secondary == mmx::GamepadButton::RightTrigger);
    assert(defaults.keyboardBinding(mmx::InputAction::RestartStage).primary == mmx::KeyboardKey::R);
    assert(defaults.gamepadBinding(mmx::InputAction::RestartStage).primary == mmx::GamepadButton::None);
    assert(mmx::keyboardActionHint(defaults, mmx::InputAction::RestartStage, "RESTART") ==
           std::string("R:RESTART"));

    mmx::InputBindings rebound = defaults;
    // C is unused by the defaults (jump=S, dash=A, shoot=X/Z), so this succeeds.
    assert(rebound.rebindPrimary(mmx::InputAction::Jump, mmx::KeyboardKey::C));
    assert(rebound.keyboardBinding(mmx::InputAction::Jump).primary == mmx::KeyboardKey::C);
    assert(rebound.keyboardBinding(mmx::InputAction::Jump).secondary == mmx::KeyboardKey::None);
    assert(rebound.hasSafeKeyboardBindings());
    assert(mmx::keyboardActionHint(rebound, mmx::InputAction::Jump, "JUMP") ==
           std::string("C:JUMP"));

    mmx::InputBindings customMove = defaults;
    assert(customMove.rebindPrimary(mmx::InputAction::Left, mmx::KeyboardKey::E));
    assert(mmx::keyboardDirectionalHint(customMove, "NAVIGATE") ==
           std::string("E/RIGHT/UP/DOWN:NAVIGATE"));

    mmx::InputBindings duplicateGameplay = defaults;
    // X is Shoot's primary, so rebinding Dash onto it must be rejected.
    assert(!duplicateGameplay.rebindPrimary(mmx::InputAction::Dash, mmx::KeyboardKey::X));
    assert(duplicateGameplay.keyboardBinding(mmx::InputAction::Dash).primary == mmx::KeyboardKey::A);

    mmx::InputBindings duplicateMenu = defaults;
    assert(!duplicateMenu.rebindPrimary(mmx::InputAction::Cancel, mmx::KeyboardKey::Enter));
    assert(duplicateMenu.keyboardBinding(mmx::InputAction::Cancel).primary == mmx::KeyboardKey::Escape);

    mmx::InputBindings ghostDashRegression = defaults;
    assert(!ghostDashRegression.rebindPrimary(mmx::InputAction::Cancel, mmx::KeyboardKey::X));
    assert(ghostDashRegression.keyboardBinding(mmx::InputAction::Cancel).primary == mmx::KeyboardKey::Escape);

    mmx::InputBindings duplicateRestart = defaults;
    assert(!duplicateRestart.rebindPrimary(mmx::InputAction::RestartStage, mmx::KeyboardKey::X));
    assert(duplicateRestart.keyboardBinding(mmx::InputAction::RestartStage).primary == mmx::KeyboardKey::R);

    mmx::InputBindings padRebound = defaults;
    assert(padRebound.rebindGamepadPrimary(mmx::InputAction::Jump, mmx::GamepadButton::LeftTrigger));
    assert(padRebound.gamepadBinding(mmx::InputAction::Jump).primary == mmx::GamepadButton::LeftTrigger);
    assert(padRebound.gamepadBinding(mmx::InputAction::Jump).secondary == mmx::GamepadButton::None);
    assert(padRebound.hasSafeGamepadBindings());

    mmx::InputBindings duplicateGamepadGameplay = defaults;
    assert(!duplicateGamepadGameplay.rebindGamepadPrimary(
        mmx::InputAction::Dash, mmx::GamepadButton::FaceUp));
    assert(duplicateGamepadGameplay.gamepadBinding(mmx::InputAction::Dash).primary ==
           mmx::GamepadButton::FaceRight);

    mmx::InputBindings duplicateGamepadMenu = defaults;
    assert(!duplicateGamepadMenu.rebindGamepadPrimary(
        mmx::InputAction::Cancel, mmx::GamepadButton::FaceDown));
    assert(duplicateGamepadMenu.gamepadBinding(mmx::InputAction::Cancel).primary ==
           mmx::GamepadButton::FaceRight);

    mmx::InputBindings padRestart = defaults;
    assert(padRestart.rebindGamepadPrimary(mmx::InputAction::RestartStage,
                                           mmx::GamepadButton::LeftTrigger));
    assert(padRestart.gamepadBinding(mmx::InputAction::RestartStage).primary ==
           mmx::GamepadButton::LeftTrigger);
    assert(padRestart.hasSafeGamepadBindings());

    mmx::InputBindings duplicateGamepadRestart = defaults;
    assert(!duplicateGamepadRestart.rebindGamepadPrimary(
        mmx::InputAction::RestartStage, mmx::GamepadButton::FaceUp));
    assert(duplicateGamepadRestart.gamepadBinding(mmx::InputAction::RestartStage).primary ==
           mmx::GamepadButton::None);

    return 0;
}
