// options_scene.cpp - runs settings, controls, and input-capture menus.
// Owns: option rows, selection flow, binding capture, and settings writes.

#include "ui/options_scene.h"
#include "ui/title_scene.h"
#include "app/scene_manager.h"
#include "app/input.h"
#include "systems/audio.h"
#include "data/display_config.h"
#include "data/settings.h"
#include "data/difficulty.h"
#include <cmath>
#include <memory>
#include <algorithm>
#include <string>

namespace mmx {
namespace {

InputAction actionAtControlIndex(int index) {
    const auto& actions = allInputActions();
    if (index < 0 || index >= static_cast<int>(actions.size())) {
        return InputAction::Jump;
    }
    return actions[static_cast<size_t>(index)];
}

int nextWindowScalePreset(int current, int direction) {
    int scale = display_config::clampWindowScale(current);
    if (direction < 0) {
        return scale <= display_config::MinWindowScale ? display_config::MaxWindowScale : scale - 1;
    }
    if (direction > 0) {
        return scale >= display_config::MaxWindowScale ? display_config::MinWindowScale : scale + 1;
    }
    return scale;
}

} // namespace

const std::vector<OptionsScene::OptionRow>& OptionsScene::mainRows() {
    static const std::vector<OptionRow> rows = {
        {OptionRowId::MasterVolume, OptionRowType::Slider, "MASTER VOLUME", UiText::OptionMasterVolume},
        {OptionRowId::MusicVolume,  OptionRowType::Slider, "MUSIC VOLUME", UiText::OptionMusicVolume},
        {OptionRowId::SfxVolume,    OptionRowType::Slider, "SFX VOLUME", UiText::OptionSfxVolume},
        {OptionRowId::WindowScale,  OptionRowType::Stepper, "INTEGER SCALE", UiText::OptionIntegerScale},
        {OptionRowId::Fullscreen,   OptionRowType::Toggle, "FULLSCREEN", UiText::OptionFullscreen},
        {OptionRowId::BorderlessFullscreen, OptionRowType::Toggle, "BORDERLESS", UiText::OptionBorderless},
        {OptionRowId::Vsync,        OptionRowType::Toggle, "VSYNC", UiText::OptionVsync},
        {OptionRowId::Difficulty,   OptionRowType::Stepper, "DIFFICULTY", UiText::OptionDifficulty},
        {OptionRowId::ShowTimer,    OptionRowType::Toggle, "SHOW TIMER", UiText::OptionShowTimer},
        {OptionRowId::PracticeMode, OptionRowType::Toggle, "PRACTICE MODE", UiText::OptionPracticeMode},
        {OptionRowId::ScreenShake,  OptionRowType::Slider, "SCREEN SHAKE", UiText::OptionScreenShake},
        {OptionRowId::AspectRatio,  OptionRowType::Toggle, "ASPECT RATIO", UiText::OptionAspectRatio},
        {OptionRowId::Language,     OptionRowType::Stepper, "LANGUAGE", UiText::OptionLanguage},
        {OptionRowId::Controls,     OptionRowType::Submenu, "CONTROLS", UiText::OptionControls},
        {OptionRowId::Back,         OptionRowType::Back, "BACK", UiText::OptionBack},
    };
    return rows;
}

int OptionsScene::mainRowCount() const {
    return static_cast<int>(mainRows().size());
}

const OptionsScene::OptionRow& OptionsScene::selectedMainRow() const {
    const auto& rows = mainRows();
    int index = selection_;
    if (index < 0 || index >= static_cast<int>(rows.size())) index = 0;
    return rows[static_cast<size_t>(index)];
}

void OptionsScene::moveMainSelection(int direction) {
    const int count = mainRowCount();
    if (count <= 0) return;
    selection_ = (selection_ + direction + count) % count;
}

void OptionsScene::adjustMainRow(const OptionRow& row, int direction) {
    constexpr float volumeStep = 0.05f;
    switch (row.id) {
    case OptionRowId::MasterVolume:
        AudioManager::setMasterVolume(AudioManager::masterVolume() + volumeStep * direction);
        break;
    case OptionRowId::MusicVolume:
        AudioManager::setMusicVolume(AudioManager::musicVolume() + volumeStep * direction);
        break;
    case OptionRowId::SfxVolume:
        AudioManager::setSFXVolume(AudioManager::sfxVolume() + volumeStep * direction);
        AudioManager::playSFX(SFX::MenuMove);
        break;
    case OptionRowId::WindowScale:
        windowScale_ = nextWindowScalePreset(windowScale_, direction);
        if (!fullscreen_ && !borderlessFullscreen_) {
            SetWindowSize(display_config::windowWidth(windowScale_, Settings::aspect43),
                          display_config::windowHeight(windowScale_));
        }
        inputDelay_ = 10;
        break;
    case OptionRowId::Difficulty: {
        int d = DifficultySettings::toInt() + direction;
        if (d < 0) d = 2;
        if (d > 2) d = 0;
        DifficultySettings::fromInt(d);
        inputDelay_ = 10;
        AudioManager::playSFX(SFX::MenuMove);
        break;
    }
    case OptionRowId::ScreenShake:
        Settings::screenShake = std::clamp(Settings::screenShake + 0.1f * direction, 0.0f, 1.0f);
        inputDelay_ = 0;
        break;
    case OptionRowId::Language:
        Settings::language = nextLanguage(Settings::language, direction);
        inputDelay_ = 10;
        AudioManager::playSFX(SFX::MenuMove);
        break;
    default:
        break;
    }
}

void OptionsScene::activateMainRow(const OptionRow& row) {
    switch (row.id) {
    case OptionRowId::Fullscreen:
        if (borderlessFullscreen_) {
            ToggleBorderlessWindowed();
            borderlessFullscreen_ = false;
        }
        fullscreen_ = !fullscreen_;
        ToggleFullscreen();
        inputDelay_ = 15;
        AudioManager::playSFX(SFX::MenuSelect);
        break;
    case OptionRowId::BorderlessFullscreen:
        if (fullscreen_) {
            ToggleFullscreen();
            fullscreen_ = false;
        }
        borderlessFullscreen_ = !borderlessFullscreen_;
        ToggleBorderlessWindowed();
        inputDelay_ = 15;
        AudioManager::playSFX(SFX::MenuSelect);
        break;
    case OptionRowId::Vsync:
        Settings::vsync = !Settings::vsync;
        if (Settings::vsync) {
            SetWindowState(FLAG_VSYNC_HINT);
        } else {
            ClearWindowState(FLAG_VSYNC_HINT);
        }
        inputDelay_ = 15;
        AudioManager::playSFX(SFX::MenuSelect);
        break;
    case OptionRowId::ShowTimer:
        Settings::showTimer = !Settings::showTimer;
        inputDelay_ = 15;
        AudioManager::playSFX(SFX::MenuSelect);
        break;
    case OptionRowId::PracticeMode:
        Settings::practiceMode = !Settings::practiceMode;
        inputDelay_ = 15;
        AudioManager::playSFX(SFX::MenuSelect);
        break;
    case OptionRowId::AspectRatio:
        Settings::aspect43 = !Settings::aspect43;
        if (!IsWindowFullscreen() && !borderlessFullscreen_) {
            SetWindowSize(display_config::windowWidth(windowScale_, Settings::aspect43),
                          display_config::windowHeight(windowScale_));
        }
        inputDelay_ = 15;
        AudioManager::playSFX(SFX::MenuSelect);
        break;
    case OptionRowId::Controls:
        AudioManager::playSFX(SFX::MenuSelect);
        mode_ = Mode::Controls;
        controlSelection_ = 0;
        inputDelay_ = 0;
        bindMessageTimer_ = 0;
        break;
    case OptionRowId::Language:
        Settings::language = nextLanguage(Settings::language, 1);
        inputDelay_ = 15;
        AudioManager::playSFX(SFX::MenuSelect);
        break;
    case OptionRowId::Back:
        AudioManager::playSFX(SFX::MenuCancel);
        saveSettings();
        returnToTitle_ = true;
        transTimer_ = 0;
        break;
    default:
        break;
    }
}

void OptionsScene::onEnter() {
    mode_ = Mode::Main;
    bindingColumn_ = BindingColumn::Keyboard;
    selection_ = 0;
    controlSelection_ = 0;
    captureAction_ = InputAction::Jump;
    bindMessageTimer_ = 0;
    bindRejected_ = false;
    timer_ = 0;
    returnToTitle_ = false;
    transTimer_ = 0;
    fadeInTimer_ = FADE_IN_DURATION;
    windowScale_ = display_config::clampWindowScale(Settings::windowScale);
    fullscreen_ = IsWindowFullscreen();
    borderlessFullscreen_ = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
}

void OptionsScene::pollInput() {
    // Handled globally
}

void OptionsScene::handleInput() {
    if (returnToTitle_) return;
    if (mode_ == Mode::CaptureKey) {
        handleCaptureInput();
        return;
    }
    if (mode_ == Mode::CapturePad) {
        handleCaptureGamepadInput();
        return;
    }
    if (mode_ == Mode::Controls) {
        handleControlsInput();
        return;
    }
    handleMainInput();
}

void OptionsScene::handleMainInput() {
    if (inputDelay_ > 0) {
        inputDelay_--;
    } else {
        if (Input::isUpHeld()) {
            moveMainSelection(-1);
            inputDelay_ = 10;
            AudioManager::playSFX(SFX::MenuMove);
        }
        if (Input::isDownHeld()) {
            moveMainSelection(1);
            inputDelay_ = 10;
            AudioManager::playSFX(SFX::MenuMove);
        }
    }
    if (!Input::isUpHeld() && !Input::isDownHeld()) {
        inputDelay_ = 0;
    }

    const OptionRow& row = selectedMainRow();
    const bool leftHeld = Input::isLeftHeld();
    const bool rightHeld = Input::isRightHeld();
    bool activatedByHorizontal = false;
    if (row.type == OptionRowType::Toggle) {
        if ((leftHeld || rightHeld) && inputDelay_ == 0) {
            activateMainRow(row);
            activatedByHorizontal = true;
        }
    } else {
        if (leftHeld && inputDelay_ == 0) {
            adjustMainRow(row, -1);
        }
        if (rightHeld && inputDelay_ == 0) {
            adjustMainRow(row, 1);
        }
    }

    if (Input::isConfirmPressed() || Input::isJumpPressed()) {
        Input::consumeConfirmPress();
        Input::consumeJumpPress();
        if (!activatedByHorizontal) activateMainRow(row);
    }

    if (Input::isCancelPressed()) {
        Input::consumeCancelPress();
        AudioManager::playSFX(SFX::MenuCancel);
        saveSettings();
        returnToTitle_ = true;
        transTimer_ = 0;
    }
}

void OptionsScene::handleControlsInput() {
    if (inputDelay_ > 0) {
        inputDelay_--;
    } else {
        if (Input::isUpHeld()) {
            controlSelection_--;
            if (controlSelection_ < 0) controlSelection_ = CONTROL_ITEM_COUNT - 1;
            inputDelay_ = 10;
            AudioManager::playSFX(SFX::MenuMove);
        }
        if (Input::isDownHeld()) {
            controlSelection_++;
            if (controlSelection_ >= CONTROL_ITEM_COUNT) controlSelection_ = 0;
            inputDelay_ = 10;
            AudioManager::playSFX(SFX::MenuMove);
        }
    }
    if (!Input::isUpHeld() && !Input::isDownHeld()) {
        inputDelay_ = 0;
    }

    if (controlSelection_ < CONTROL_ACTION_COUNT && inputDelay_ == 0) {
        if (Input::isLeftHeld() && bindingColumn_ != BindingColumn::Keyboard) {
            bindingColumn_ = BindingColumn::Keyboard;
            inputDelay_ = 10;
            AudioManager::playSFX(SFX::MenuMove);
        } else if (Input::isRightHeld() && bindingColumn_ != BindingColumn::Gamepad) {
            bindingColumn_ = BindingColumn::Gamepad;
            inputDelay_ = 10;
            AudioManager::playSFX(SFX::MenuMove);
        }
    }

    if (Input::isCancelPressed()) {
        Input::consumeCancelPress();
        AudioManager::playSFX(SFX::MenuCancel);
        mode_ = Mode::Main;
        inputDelay_ = 0;
        return;
    }

    if (Input::isConfirmPressed() || Input::isJumpPressed()) {
        Input::consumeConfirmPress();
        Input::consumeJumpPress();
        if (controlSelection_ < CONTROL_ACTION_COUNT) {
            captureAction_ = actionAtControlIndex(controlSelection_);
            mode_ = bindingColumn_ == BindingColumn::Gamepad ? Mode::CapturePad : Mode::CaptureKey;
            bindMessageTimer_ = 0;
            bindRejected_ = false;
            AudioManager::playSFX(SFX::MenuSelect);
        } else if (controlSelection_ == CONTROL_ACTION_COUNT) {
            Settings::inputBindings = InputBindings::defaults();
            Input::setBindings(Settings::inputBindings);
            bindMessageTimer_ = 90;
            bindRejected_ = false;
            AudioManager::playSFX(SFX::MenuSelect);
        } else {
            AudioManager::playSFX(SFX::MenuCancel);
            mode_ = Mode::Main;
            inputDelay_ = 0;
        }
    }
}

void OptionsScene::handleCaptureInput() {
    KeyboardKey key = Input::consumeLastKeyboardKeyPressed();
    if (key != KeyboardKey::None) {
        InputBindings candidate = Settings::inputBindings;
        const bool rebound = candidate.rebindPrimary(captureAction_, key);
        if (rebound) {
            Settings::inputBindings = candidate;
            Input::setBindings(Settings::inputBindings);
            bindRejected_ = false;
            AudioManager::playSFX(SFX::MenuSelect);
        } else {
            bindRejected_ = true;
            AudioManager::playSFX(SFX::MenuCancel);
        }
        bindMessageTimer_ = 120;
        mode_ = Mode::Controls;
        inputDelay_ = 0;
        return;
    }

    if (Input::isCancelPressed()) {
        Input::consumeCancelPress();
        bindRejected_ = false;
        bindMessageTimer_ = 0;
        mode_ = Mode::Controls;
        inputDelay_ = 0;
        AudioManager::playSFX(SFX::MenuCancel);
    }
}

void OptionsScene::handleCaptureGamepadInput() {
    KeyboardKey key = Input::consumeLastKeyboardKeyPressed();
    if (key == KeyboardKey::Escape) {
        bindRejected_ = false;
        bindMessageTimer_ = 0;
        mode_ = Mode::Controls;
        inputDelay_ = 0;
        AudioManager::playSFX(SFX::MenuCancel);
        return;
    }

    GamepadButton button = Input::consumeLastGamepadButtonPressed();
    if (button != GamepadButton::None) {
        InputBindings candidate = Settings::inputBindings;
        const bool rebound = candidate.rebindGamepadPrimary(captureAction_, button);
        if (rebound) {
            Settings::inputBindings = candidate;
            Input::setBindings(Settings::inputBindings);
            bindRejected_ = false;
            AudioManager::playSFX(SFX::MenuSelect);
        } else {
            bindRejected_ = true;
            AudioManager::playSFX(SFX::MenuCancel);
        }
        bindMessageTimer_ = 120;
        mode_ = Mode::Controls;
        inputDelay_ = 0;
    }
}

void OptionsScene::update(float /*dt*/) {
    timer_++;
    if (fadeInTimer_ > 0) fadeInTimer_--;
    if (bindMessageTimer_ > 0) bindMessageTimer_--;

    if (returnToTitle_) {
        transTimer_++;
        if (transTimer_ > 10 && sceneManager_) {
            auto title = std::make_unique<TitleScene>();
            title->setSceneManager(sceneManager_);
            sceneManager_->changeScene(std::move(title));
        }
    }
}

void OptionsScene::renderMainRowValue(const OptionRow& row, int itemY, bool selected) const {
    auto valueColor = [selected]() {
        return selected ? Color{100, 200, 255, 255} : Color{130, 130, 150, 255};
    };

    auto sliderColor = [selected]() {
        return selected ? Color{80, 180, 255, 255} : Color{60, 120, 180, 255};
    };

    auto drawSlider = [&](float value, int sliderY, int sliderW, int sliderH, int pctFontSize) {
        const int sliderX = 36;
        DrawRectangle(sliderX, sliderY, sliderW, sliderH, {40, 40, 60, 255});
        DrawRectangle(sliderX, sliderY, static_cast<int>(value * sliderW), sliderH, sliderColor());
        DrawRectangleLines(sliderX, sliderY, sliderW, sliderH,
            selected ? Color{120, 200, 255, 255} : Color{80, 80, 100, 255});

        char pctBuf[8];
        snprintf(pctBuf, sizeof(pctBuf), "%d%%", static_cast<int>(value * 100));
        DrawText(pctBuf, sliderX + sliderW + 6, sliderY - (pctFontSize == 7 ? 2 : 0),
                 pctFontSize, selected ? WHITE : Color{130, 130, 150, 255});

        if (selected && pctFontSize == 8) {
            DrawText("<", sliderX - 10, sliderY - 1, 8, {100, 200, 255, 255});
            DrawText(">", sliderX + sliderW + 2, sliderY - 1, 8, {100, 200, 255, 255});
        }
    };

    switch (row.id) {
    case OptionRowId::MasterVolume:
        drawSlider(AudioManager::masterVolume(), itemY + 14, 140, 6, 8);
        break;
    case OptionRowId::MusicVolume:
        drawSlider(AudioManager::musicVolume(), itemY + 14, 140, 6, 8);
        break;
    case OptionRowId::SfxVolume:
        drawSlider(AudioManager::sfxVolume(), itemY + 14, 140, 6, 8);
        break;
    case OptionRowId::WindowScale: {
        char scaleBuf[16];
        snprintf(scaleBuf, sizeof(scaleBuf), "< %dx >", windowScale_);
        DrawText(scaleBuf, 36, itemY + 14, 8, valueColor());
        char resBuf[32];
        snprintf(resBuf, sizeof(resBuf), "(%dx%d)",
                 display_config::windowWidth(windowScale_, Settings::aspect43),
                 display_config::windowHeight(windowScale_));
        DrawText(resBuf, 86, itemY + 14, 8, {100, 100, 120, 255});
        break;
    }
    case OptionRowId::Fullscreen:
        DrawText(uiText(fullscreen_ ? UiText::ValueOn : UiText::ValueOff, Settings::language),
                 36, itemY + 14, 8, valueColor());
        break;
    case OptionRowId::BorderlessFullscreen:
        DrawText(uiText(borderlessFullscreen_ ? UiText::ValueOn : UiText::ValueOff, Settings::language),
                 36, itemY + 14, 8, valueColor());
        break;
    case OptionRowId::Vsync:
        DrawText(uiText(Settings::vsync ? UiText::ValueOn : UiText::ValueOff, Settings::language),
                 36, itemY + 14, 8, valueColor());
        break;
    case OptionRowId::Difficulty: {
        char diffBuf[32];
        snprintf(diffBuf, sizeof(diffBuf), "< %s >", DifficultySettings::name());
        Color diffColor = valueColor();
        if (DifficultySettings::current == Difficulty::Hard) {
            diffColor = selected ? Color{255, 120, 80, 255} : Color{180, 100, 80, 255};
        } else if (DifficultySettings::current == Difficulty::Easy) {
            diffColor = selected ? Color{100, 255, 150, 255} : Color{100, 180, 120, 255};
        }
        DrawText(diffBuf, 36, itemY + 14, 8, diffColor);
        break;
    }
    case OptionRowId::ShowTimer:
        DrawText(uiText(Settings::showTimer ? UiText::ValueOn : UiText::ValueOff, Settings::language),
                 36, itemY + 10, 8, valueColor());
        break;
    case OptionRowId::PracticeMode:
        DrawText(uiText(Settings::practiceMode ? UiText::ValueOn : UiText::ValueOff, Settings::language),
                 36, itemY + 10, 8, valueColor());
        break;
    case OptionRowId::ScreenShake:
        drawSlider(Settings::screenShake, itemY + 10, 100, 5, 7);
        break;
    case OptionRowId::AspectRatio: {
        char aspBuf[40];
        snprintf(aspBuf, sizeof(aspBuf), "< %s >",
                 Settings::aspect43
                     ? uiText(UiText::ValueClassic, Settings::language)
                     : uiText(UiText::ValuePixelPerfect, Settings::language));
        DrawText(aspBuf, 36, itemY + 10, 8, valueColor());
        break;
    }
    case OptionRowId::Language: {
        char langBuf[32];
        snprintf(langBuf, sizeof(langBuf), "< %s >", languageLabel(Settings::language));
        DrawText(langBuf, 36, itemY + 10, 8, valueColor());
        break;
    }
    default:
        break;
    }
}

void OptionsScene::render(float /*alpha*/) {
    ClearBackground({12, 8, 28, 255});

    // Title
    const char* title = uiText(UiText::OptionsTitle, Settings::language);
    int titleW = MeasureText(title, 14);
    DrawText(title, (INTERNAL_WIDTH - titleW) / 2, 20, 14, {200, 200, 230, 255});

    if (mode_ == Mode::Controls || mode_ == Mode::CaptureKey || mode_ == Mode::CapturePad) {
        renderControlsMenu();
    } else {
        int menuY = 34;
        int itemSpacing = 18;
        const auto& rows = mainRows();
        constexpr int visibleRows = 9;
        int firstRow = 0;
        if (selection_ >= visibleRows) {
            firstRow = selection_ - visibleRows + 1;
        }

        for (int i = firstRow; i < static_cast<int>(rows.size()) && i < firstRow + visibleRows; i++) {
            const OptionRow& row = rows[static_cast<size_t>(i)];
            int itemY = menuY + (i - firstRow) * itemSpacing;
            bool selected = (i == selection_);

            Color labelColor = selected ? WHITE : Color{150, 150, 170, 255};

            if (selected) {
                DrawText(">", 24, itemY, 10, {100, 200, 255, 255});
            }

            DrawText(uiText(row.text, Settings::language), 36, itemY, 10, labelColor);
            renderMainRowValue(row, itemY, selected);
        }

        std::string hint;
        if (Input::isGamepadConnected()) {
            hint = std::string("DPAD:") + uiText(UiText::PurposeNavigate, Settings::language) +
                   "  LEFT/RIGHT:" + uiText(UiText::PurposeAdjust, Settings::language) +
                   "  B:" + uiText(UiText::PurposeBack, Settings::language);
        } else {
            hint = keyboardDirectionalHint(Input::bindings(), uiText(UiText::PurposeNavigate, Settings::language)) + "  " +
                   keyboardHorizontalHint(Input::bindings(), uiText(UiText::PurposeAdjust, Settings::language)) + "  " +
                   keyboardActionHint(Input::bindings(), InputAction::Cancel, uiText(UiText::PurposeBack, Settings::language));
        }
        int hintW = MeasureText(hint.c_str(), 7);
        DrawText(hint.c_str(), (INTERNAL_WIDTH - hintW) / 2, INTERNAL_HEIGHT - 10, 7, {80, 80, 110, 255});
    }

    // Fade-in
    if (fadeInTimer_ > 0) {
        int a = 255 * fadeInTimer_ / FADE_IN_DURATION;
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
                      {0, 0, 0, static_cast<unsigned char>(a)});
    }

    // Fade transition
    if (returnToTitle_) {
        int fadeAlpha = std::min(255, transTimer_ * 25);
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
            {0, 0, 0, static_cast<unsigned char>(fadeAlpha)});
    }
}

void OptionsScene::renderControlsMenu() {
    const UiText subtitleText = mode_ == Mode::CaptureKey
        ? UiText::PressKey
        : (mode_ == Mode::CapturePad ? UiText::PressPad : UiText::ControlsTitle);
    const char* subtitle = uiText(subtitleText, Settings::language);
    int subW = MeasureText(subtitle, 10);
    DrawText(subtitle, (INTERNAL_WIDTH - subW) / 2, 38, 10, {160, 210, 255, 255});
    if (mode_ != Mode::CaptureKey) {
        const char* padStatus = uiText(Input::isGamepadConnected() ? UiText::PadConnected : UiText::NoPad,
                                       Settings::language);
        const int statusW = MeasureText(padStatus, 7);
        DrawText(padStatus, INTERNAL_WIDTH - statusW - 10, 39, 7,
                 Input::isGamepadConnected() ? Color{120, 230, 160, 255}
                                             : Color{120, 120, 150, 255});
    }

    if (mode_ == Mode::CaptureKey || mode_ == Mode::CapturePad) {
        const char* actionLabel = inputActionLabel(captureAction_);
        int actionW = MeasureText(actionLabel, 12);
        DrawText(actionLabel, (INTERNAL_WIDTH - actionW) / 2, 82, 12, WHITE);
        const char* hint = uiText(mode_ == Mode::CapturePad ? UiText::CapturePadHint
                                                            : UiText::CaptureKeyHint,
                                  Settings::language);
        int hintW = MeasureText(hint, 7);
        DrawText(hint, (INTERNAL_WIDTH - hintW) / 2, 104, 7, {120, 120, 150, 255});
        return;
    }

    constexpr int keyColumnX = 150;
    constexpr int padColumnRight = INTERNAL_WIDTH - 18;
    const char* keyColumn = uiText(UiText::ControlsKeyColumn, Settings::language);
    const char* padColumn = uiText(UiText::ControlsPadColumn, Settings::language);
    DrawText(keyColumn, keyColumnX - MeasureText(keyColumn, 7), 47, 7,
             bindingColumn_ == BindingColumn::Keyboard ? Color{100, 220, 255, 255}
                                                       : Color{90, 110, 140, 255});
    DrawText(padColumn, padColumnRight - MeasureText(padColumn, 7), 47, 7,
             bindingColumn_ == BindingColumn::Gamepad ? Color{120, 230, 160, 255}
                                                      : Color{90, 120, 100, 255});

    int menuY = 56;
    int itemSpacing = 9;
    for (int i = 0; i < CONTROL_ITEM_COUNT; ++i) {
        int itemY = menuY + i * itemSpacing;
        bool selected = i == controlSelection_;
        Color labelColor = selected ? WHITE : Color{150, 150, 170, 255};

        if (selected) {
            DrawText(">", 18, itemY, 9, {100, 200, 255, 255});
        }

        if (i < CONTROL_ACTION_COUNT) {
            InputAction action = actionAtControlIndex(i);
            DrawText(inputActionLabel(action), 30, itemY, 8, labelColor);

            std::string keyLabel = keyboardActionLabel(Settings::inputBindings, action);
            int keyW = MeasureText(keyLabel.c_str(), 8);
            const bool keySelected = selected && bindingColumn_ == BindingColumn::Keyboard;
            DrawText(keyLabel.c_str(), keyColumnX - keyW, itemY, 8,
                     keySelected ? Color{100, 220, 255, 255} : Color{120, 150, 170, 255});
            std::string padLabel = gamepadActionLabel(Settings::inputBindings, action);
            int padW = MeasureText(padLabel.c_str(), 8);
            const bool padSelected = selected && bindingColumn_ == BindingColumn::Gamepad;
            DrawText(padLabel.c_str(), padColumnRight - padW, itemY, 8,
                     padSelected ? Color{120, 230, 160, 255} : Color{120, 160, 140, 255});
        } else if (i == CONTROL_ACTION_COUNT) {
            DrawText(uiText(UiText::ResetDefaults, Settings::language), 30, itemY, 8, labelColor);
        } else {
            DrawText(uiText(UiText::OptionBack, Settings::language), 30, itemY, 8, labelColor);
        }
    }

    if (bindMessageTimer_ > 0) {
        const char* message = uiText(bindRejected_ ? UiText::KeyAlreadyUsed : UiText::Saved,
                                     Settings::language);
        Color color = bindRejected_ ? Color{255, 120, 80, 255} : Color{120, 230, 160, 255};
        int msgW = MeasureText(message, 8);
        DrawText(message, (INTERNAL_WIDTH - msgW) / 2, INTERNAL_HEIGHT - 22, 8, color);
    }

    std::string hint = Input::isGamepadConnected()
        ? std::string("DPAD:") + uiText(UiText::ControlsKeyPadColumns, Settings::language) + "  " +
          gamepadActionHint(Input::bindings(), InputAction::Confirm, uiText(UiText::PurposeRebind, Settings::language)) + "  " +
          gamepadActionHint(Input::bindings(), InputAction::Cancel, uiText(UiText::PurposeBack, Settings::language))
        : keyboardHorizontalHint(Input::bindings(), uiText(UiText::ControlsKeyPadColumns, Settings::language)) + "  " +
          keyboardActionHint(Input::bindings(), InputAction::Confirm, uiText(UiText::PurposeRebind, Settings::language)) + "  " +
          keyboardActionHint(Input::bindings(), InputAction::Cancel, uiText(UiText::PurposeBack, Settings::language));
    int hintW = MeasureText(hint.c_str(), 7);
    DrawText(hint.c_str(), (INTERNAL_WIDTH - hintW) / 2, INTERNAL_HEIGHT - 10, 7, {80, 80, 110, 255});
}

void OptionsScene::saveSettings() {
    Settings::masterVolume = AudioManager::masterVolume();
    Settings::musicVolume = AudioManager::musicVolume();
    Settings::sfxVolume = AudioManager::sfxVolume();
    Settings::windowScale = windowScale_;
    Settings::fullscreen = fullscreen_;
    Settings::borderlessFullscreen = borderlessFullscreen_;
    Settings::difficulty = DifficultySettings::toInt();
    Settings::inputBindings = Input::bindings();
    Settings::save();
}

} // namespace mmx
