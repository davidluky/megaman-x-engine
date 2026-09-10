// options_scene.h - declares the options and controls scene.
// Owns: options mode, selected rows, pending capture, and row definitions.

#pragma once

#include "app/scene.h"
#include "app/constants.h"
#include "app/input_bindings.h"
#include "data/localization.h"
#include <vector>

// ============================================================================
// options_scene.h — Options menu
//
// Volume/video/gameplay settings plus a controls submenu that captures keyboard
// rebinds and displays keyboard/gamepad bindings through InputBindings. Row metadata in
// options_scene.cpp owns labels, row type, and behavior.
// ============================================================================

namespace mmx {

class SceneManager;

class OptionsScene : public Scene {
public:
    void setSceneManager(SceneManager* mgr) { sceneManager_ = mgr; }

    void onEnter() override;
    void pollInput() override;
    void handleInput() override;
    void update(float dt) override;
    void render(float alpha) override;

private:
    SceneManager* sceneManager_ = nullptr;

    enum class Mode {
        Main,
        Controls,
        CaptureKey,
        CapturePad
    };

    enum class BindingColumn {
        Keyboard,
        Gamepad
    };

    Mode mode_ = Mode::Main;
    BindingColumn bindingColumn_ = BindingColumn::Keyboard;
    int selection_ = 0;
    int controlSelection_ = 0;
    static constexpr int CONTROL_ACTION_COUNT = static_cast<int>(InputAction::Count);
    static constexpr int CONTROL_ITEM_COUNT = CONTROL_ACTION_COUNT + 2;
    InputAction captureAction_ = InputAction::Jump;
    int bindMessageTimer_ = 0;
    bool bindRejected_ = false;
    int inputDelay_ = 0;
    int timer_ = 0;
    bool returnToTitle_ = false;
    int transTimer_ = 0;
    int windowScale_ = 4;
    bool fullscreen_ = false;
    bool borderlessFullscreen_ = false;
    int fadeInTimer_ = 0;
    static constexpr int FADE_IN_DURATION = 30;

    enum class OptionRowId {
        MasterVolume,
        MusicVolume,
        SfxVolume,
        WindowScale,
        Fullscreen,
        BorderlessFullscreen,
        Vsync,
        Difficulty,
        ShowTimer,
        PracticeMode,
        ScreenShake,
        AspectRatio,
        Language,
        Controls,
        Back
    };

    enum class OptionRowType {
        Slider,
        Stepper,
        Toggle,
        Submenu,
        Back
    };

    struct OptionRow {
        OptionRowId id;
        OptionRowType type;
        const char* label;
        UiText text;
    };

    void saveSettings();
    static const std::vector<OptionRow>& mainRows();
    int mainRowCount() const;
    const OptionRow& selectedMainRow() const;
    void moveMainSelection(int direction);
    void adjustMainRow(const OptionRow& row, int direction);
    void activateMainRow(const OptionRow& row);
    void renderMainRowValue(const OptionRow& row, int itemY, bool selected) const;
    void handleMainInput();
    void handleControlsInput();
    void handleCaptureInput();
    void handleCaptureGamepadInput();
    void renderControlsMenu();
};

} // namespace mmx
