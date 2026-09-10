#pragma once

#include "app/input_bindings.h"
#include "raylib.h"

// ============================================================================
// input.h — Unified input abstraction
//
// Maps keyboard and gamepad inputs to game actions. All game code queries
// actions (isJumpPressed, isDashHeld) instead of raw keys. This means:
//   - Adding gamepad support doesn't touch gameplay code
//   - Key remapping is centralized
//   - Input buffering lives in one place
//
// Design: singleton-style static class. Call Input::poll() once per render
// frame (for press detection) and Input::update() once per physics tick
// (for held state). Then query actions anywhere.
//
// Gamepad mapping is provided by InputBindings, persisted beside keyboard
// bindings in config.json, with the left stick always acting as movement.
// ============================================================================

namespace mmx {

class Input {
public:
    // Call once per render frame — captures IsKeyPressed/IsGamepadButtonPressed
    // events that would be lost between physics ticks on high-refresh monitors.
    static void poll();

    // Call once per physics tick — reads held-state for continuous actions.
    static void update();

    // Call at the end of every physics frame (once per update loop)
    // to clear press/release states so they don't persist.
    static void clear();

    // --- Action queries (use these in gameplay code) ---

    // Movement
    static bool isLeftHeld()  { return left_; }
    static bool isRightHeld() { return right_; }
    static bool isUpHeld()    { return up_; }
    static bool isDownHeld()  { return down_; }

    // Jump: pressed = buffered one-shot, held = sustain (variable jump height)
    static bool isJumpPressed() { return jumpPressed_; }
    static bool isJumpHeld()    { return jumpHeld_; }
    static void consumeJumpPress() { jumpPressed_ = false; }

    // Dash: pressed = buffered one-shot, held = sustain
    static bool isDashPressed() { return dashPressed_; }
    static bool isDashHeld()    { return dashHeld_; }
    static void consumeDashPress() { dashPressed_ = false; }

    // Shoot: pressed = fire normal shot (real MMX fires on press — oracle
    // 2026-06-09), held = charging, released = fire charged shot
    static bool isShootPressed()  { return shootPressed_; }
    static bool isShootHeld()     { return shootHeld_; }
    static bool isShootReleased() { return shootReleased_; }
    static void consumeShootPress()   { shootPressed_ = false; }
    static void consumeShootRelease() { shootReleased_ = false; }

    // Pause
    static bool isPausePressed() { return pausePressed_; }
    static void consumePausePress() { pausePressed_ = false; }

    // Confirm/Cancel (menus)
    static bool isConfirmPressed() { return confirmPressed_; }
    static bool isCancelPressed()  { return cancelPressed_; }
    static void consumeConfirmPress() { confirmPressed_ = false; }
    static void consumeCancelPress()  { cancelPressed_ = false; }

    // Weapon cycling
    static bool isWeaponNextPressed() { return weaponNextPressed_; }
    static bool isWeaponPrevPressed() { return weaponPrevPressed_; }
    static bool isSubTankPressed()   { return subTankPressed_; }
    static bool isRestartStagePressed() { return restartStagePressed_; }
    static void consumeWeaponNextPress() { weaponNextPressed_ = false; }
    static void consumeWeaponPrevPress() { weaponPrevPressed_ = false; }
    static void consumeSubTankPress()    { subTankPressed_ = false; }
    static void consumeRestartStagePress() { restartStagePressed_ = false; }

    // Debug
    static bool isDebug1Pressed() { return debug1Pressed_; }
    static bool isDebug2Pressed() { return debug2Pressed_; }
    static bool isDebug3Pressed() { return debug3Pressed_; }
    static bool isDebug4Pressed() { return debug4Pressed_; }

    // U66 numpad capsule-grant debug keys (KP1-4)
    static bool isUpgradeGrant1Pressed() { return upgradeGrant1Pressed_; }
    static bool isUpgradeGrant2Pressed() { return upgradeGrant2Pressed_; }
    static bool isUpgradeGrant3Pressed() { return upgradeGrant3Pressed_; }
    static bool isUpgradeGrant4Pressed() { return upgradeGrant4Pressed_; }
    static void consumeUpgradeGrant1Press() { upgradeGrant1Pressed_ = false; }
    static void consumeUpgradeGrant2Press() { upgradeGrant2Pressed_ = false; }
    static void consumeUpgradeGrant3Press() { upgradeGrant3Pressed_ = false; }
    static void consumeUpgradeGrant4Press() { upgradeGrant4Pressed_ = false; }
    static void consumeDebug1Press() { debug1Pressed_ = false; }
    static void consumeDebug2Press() { debug2Pressed_ = false; }
    static void consumeDebug3Press() { debug3Pressed_ = false; }
    static void consumeDebug4Press() { debug4Pressed_ = false; }

    // Gamepad detection
    static bool isGamepadConnected() { return gamepadConnected_; }

    // Runtime keyboard/gamepad bindings. Settings owns persistence; Input owns the
    // active table consumed by gameplay and menus.
    static void setBindings(const InputBindings& bindings);
    static const InputBindings& bindings();
    static bool rebindKeyboard(InputAction action, KeyboardKey key);
    static void resetBindingsToDefault();
    static KeyboardKey consumeLastKeyboardKeyPressed();
    static GamepadButton consumeLastGamepadButtonPressed();

    // Scripted input for --autotest mode. Setters override the real input
    // state only when true — a "false" does NOT suppress a real key, so you
    // can mix scripted events with a human watching. clearScripted() resets
    // everything at once. Held setters persist across frames until cleared;
    // Pressed setters auto-clear after one tick (like real presses).
    static void setScriptedRightHeld(bool v) { scriptedRight_ = v; }
    static void setScriptedLeftHeld(bool v)  { scriptedLeft_  = v; }
    static void setScriptedUpHeld(bool v)    { scriptedUp_    = v; }
    static void setScriptedDownHeld(bool v)  { scriptedDown_  = v; }
    static void setScriptedJumpHeld(bool v)  { scriptedJumpHeld_ = v; }
    static void setScriptedDashHeld(bool v)  { scriptedDashHeld_ = v; }
    static void setScriptedShootHeld(bool v) { scriptedShootHeld_ = v; }
    static void scriptedJumpPress()  { scriptedJumpPress_ = true; }
    static void scriptedDashPress()  { scriptedDashPress_ = true; }
    static void scriptedConfirmPress() { scriptedConfirmPress_ = true; }
    static void scriptedCancelPress()  { scriptedCancelPress_ = true; }
    static void scriptedPausePress()   { scriptedPausePress_ = true; }
    static void scriptedShootRelease() { scriptedShootRelease_ = true; }
    static void scriptedWeaponNextPress() { scriptedWeaponNextPress_ = true; }
    static void scriptedWeaponPrevPress() { scriptedWeaponPrevPress_ = true; }
    // Force-switch: bypasses hasActiveSpecialShot() — for autotest weapon cycling only.
    static void scriptedWeaponNextForce() { scriptedWeaponNextForce_ = true; }
    static bool isWeaponNextForced() { return scriptedWeaponNextForce_; }
    static void consumeWeaponNextForce() { scriptedWeaponNextForce_ = false; }
    // Clears held-state flags only. One-shot press flags are left alone
    // because Input::update self-clears them on consumption — wiping them
    // here would drop a scheduled press if the render rate runs faster than
    // the physics rate (no physics tick between the autotest setting the
    // flag and the next autotest call).
    static void clearScripted() {
        scriptedRight_ = scriptedLeft_ = scriptedUp_ = scriptedDown_ = false;
        scriptedJumpHeld_ = scriptedDashHeld_ = scriptedShootHeld_ = false;
    }

private:
    // Helper: checks if any gamepad is available
    static int activeGamepad();

    // Held state (updated every physics tick)
    static bool left_, right_, up_, down_;
    static bool jumpHeld_, dashHeld_, shootHeld_;

    // Press state (latched at render-frame rate, consumed by game logic)
    static bool jumpPressed_, dashPressed_;
    static bool shootPressed_;
    static bool shootReleased_;
    static bool pausePressed_;
    static bool confirmPressed_, cancelPressed_;
    static bool weaponNextPressed_, weaponPrevPressed_;
    static bool subTankPressed_;
    static bool restartStagePressed_;
    static bool debug1Pressed_, debug2Pressed_, debug3Pressed_, debug4Pressed_;
    static bool upgradeGrant1Pressed_, upgradeGrant2Pressed_,
                upgradeGrant3Pressed_, upgradeGrant4Pressed_;
    static KeyboardKey lastKeyboardKeyPressed_;
    static GamepadButton lastGamepadButtonPressed_;

    // Shoot release tracking
    static bool prevShootHeld_;

    static bool gamepadConnected_;

    // --autotest scripted overrides. "Held" persist until cleared; "Press"
    // flags are one-shot — set them with scriptedJumpPress() etc. and they
    // fire for exactly one physics tick before auto-clearing.
    static bool scriptedRight_;
    static bool scriptedLeft_;
    static bool scriptedUp_;
    static bool scriptedDown_;
    static bool scriptedJumpHeld_;
    static bool scriptedDashHeld_;
    static bool scriptedShootHeld_;
    static bool scriptedJumpPress_;
    static bool scriptedDashPress_;
    static bool scriptedConfirmPress_;
    static bool scriptedCancelPress_;
    static bool scriptedPausePress_;
    static bool scriptedShootRelease_;
    static bool scriptedWeaponNextPress_;
    static bool scriptedWeaponPrevPress_;
    static bool scriptedWeaponNextForce_;
    static bool scriptedShootHeldPrev_;  // for synthesizing shootReleased on scripted drop
    static InputBindings bindings_;
};

} // namespace mmx
