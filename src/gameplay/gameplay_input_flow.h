// gameplay_input_flow.h - routes player input through gameplay pause and control modes.
// Boundary: consumes app input facade; does not own key binding policy.

#pragma once

#include "gameplay/gameplay_escape_confirm.h"
#include "gameplay/gameplay_navigation.h"
#include "gameplay/gameplay_pause_menu.h"
#include "gameplay/gameplay_stage_clear.h"
#include "entities/player.h"
#include "app/input.h"
#include "systems/audio.h"
#include "ui/menu_flow.h"

#include <algorithm>

namespace mmx::gameplay_input_flow {

inline bool isStageClearInputActive(bool bossRushMode,
                                    const gameplay_stage_clear::State& state) {
    return !bossRushMode && gameplay_stage_clear::inputReady(state);
}

inline void handleStageClearConfirm(gameplay_navigation::State& navigation) {
    if (Input::isConfirmPressed()) {
        Input::consumeConfirmPress();
        gameplay_navigation::requestStageSelect(navigation);
    }
}

inline void handleStageClearCancel(gameplay_navigation::State& navigation) {
    if (Input::isCancelPressed()) {
        Input::consumeCancelPress();
        gameplay_navigation::requestTitle(navigation);
    }
}

inline bool handleEscapeConfirmDismiss(gameplay_escape_confirm::State& state,
                                       bool& paused) {
    if (!Input::isCancelPressed() && !Input::isPausePressed()) return false;

    Input::consumeCancelPress();
    Input::consumePausePress();
    closeEscapeConfirm(state.dialog);
    state.inputDelay = 0;
    paused = false;
    AudioManager::resumeBGM();
    AudioManager::playSFX(SFX::MenuCancel);
    return true;
}

inline int escapeConfirmNavigationDirection() {
    if (Input::isLeftHeld() || Input::isUpHeld()) return -1;
    if (Input::isRightHeld() || Input::isDownHeld()) return 1;
    return 0;
}

inline void updateEscapeConfirmNavigation(gameplay_escape_confirm::State& state) {
    if (state.inputDelay > 0) {
        state.inputDelay--;
    } else {
        const int direction = escapeConfirmNavigationDirection();
        if (direction != 0) {
            moveEscapeConfirm(state.dialog, direction);
            state.inputDelay = 8;
            AudioManager::playSFX(SFX::MenuMove);
        }
    }
    if (!Input::isLeftHeld() && !Input::isRightHeld() &&
        !Input::isUpHeld() && !Input::isDownHeld()) {
        state.inputDelay = 0;
    }
}

inline bool handleEscapeConfirmSelection(gameplay_escape_confirm::State& state,
                                         bool& paused,
                                         gameplay_navigation::State& navigation) {
    if (!Input::isConfirmPressed() && !Input::isJumpPressed()) return false;

    Input::consumeConfirmPress();
    Input::consumeJumpPress();
    const bool wantsTitle = escapeConfirmWantsTitle(state.dialog);
    closeEscapeConfirm(state.dialog);
    state.inputDelay = 0;
    paused = false;
    AudioManager::playSFX(wantsTitle ? SFX::MenuSelect : SFX::MenuCancel);
    if (wantsTitle) {
        gameplay_navigation::requestTitle(navigation);
    } else {
        AudioManager::resumeBGM();
    }
    return true;
}

inline bool openEscapeConfirmIfRequested(gameplay_escape_confirm::State& state,
                                         bool& paused,
                                         bool gameOver,
                                         bool stageClear) {
    if (paused || gameOver || stageClear || !Input::isCancelPressed()) return false;

    Input::consumeCancelPress();
    openEscapeConfirm(state.dialog);
    state.inputDelay = 0;
    paused = true;
    AudioManager::playSFX(SFX::Pause);
    AudioManager::pauseBGM();
    return true;
}

inline void handlePauseToggleInput(gameplay_pause_menu::State& state,
                                   bool gameOver,
                                   bool stageClear,
                                   const Player& player) {
    if (Input::isPausePressed()) {
        Input::consumePausePress();
        if (!gameOver && !stageClear) {
            state.paused = !state.paused;
            AudioManager::playSFX(SFX::Pause);
            if (state.paused) {
                AudioManager::pauseBGM();
                state.weaponCursor = player.weaponInventory.currentIndex;
                state.inputDelay = 0;
            } else {
                AudioManager::resumeBGM();
            }
        }
    }
}

inline void handlePauseCancelInput(gameplay_pause_menu::State& state) {
    if (state.paused && Input::isCancelPressed()) {
        Input::consumeCancelPress();
        state.paused = false;
        AudioManager::resumeBGM();
    }
}

inline bool isPauseWeaponMenuActive(const gameplay_pause_menu::State& state,
                                    const gameplay_escape_confirm::State& escapeConfirm) {
    return gameplay_pause_menu::weaponMenuActive(state) &&
           !gameplay_escape_confirm::isOpen(escapeConfirm);
}

inline void updatePauseWeaponCursor(gameplay_pause_menu::State& state, int weaponCount) {
    if (state.inputDelay > 0) {
        state.inputDelay--;
    } else {
        if (Input::isUpHeld()) {
            state.weaponCursor--;
            if (state.weaponCursor < 0) state.weaponCursor = weaponCount - 1;
            state.inputDelay = 8;
            AudioManager::playSFX(SFX::MenuMove);
        }
        if (Input::isDownHeld()) {
            state.weaponCursor++;
            if (state.weaponCursor >= weaponCount) state.weaponCursor = 0;
            state.inputDelay = 8;
            AudioManager::playSFX(SFX::MenuMove);
        }
    }
    if (!Input::isUpHeld() && !Input::isDownHeld()) state.inputDelay = 0;
}

inline void confirmPauseWeaponSelection(Player& player, gameplay_pause_menu::State& state) {
    if (!Input::isConfirmPressed() && !Input::isJumpPressed()) return;

    Input::consumeConfirmPress();
    Input::consumeJumpPress();
    player.weaponInventory.currentIndex = state.weaponCursor;
    AudioManager::playSFX(SFX::WeaponSwitch);
    state.paused = false;
    AudioManager::resumeBGM();
}

inline bool canUseSubTank(const Player& player) {
    return player.health < player.progressState().maxHealth;
}

inline int firstUsableSubTankIndex(const Player& player) {
    const auto& progress = player.progressState();
    for (int i = 0; i < Player::MAX_SUB_TANKS; i++) {
        if (progress.subTanks[i].collected && progress.subTanks[i].health > 0) {
            return i;
        }
    }
    return -1;
}

inline void transferSubTankHealth(Player& player, int tankIndex) {
    auto& progress = player.progressState();
    const int amount =
        std::min(progress.maxHealth - player.health, progress.subTanks[tankIndex].health);
    player.health += amount;
    progress.subTanks[tankIndex].health -= amount;
}

} // namespace mmx::gameplay_input_flow
