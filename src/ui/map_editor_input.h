// map_editor_input.h - input helpers for map editor UI controls.

#pragma once

#include "app/screen_transform.h"
#include "raylib.h"

#include <algorithm>

namespace mmx::map_editor {

inline screen_transform::InternalPoint currentInternalMouse(bool aspect43) {
    const Vector2 rawMouse = GetMousePosition();
    return screen_transform::screenToInternal(
        rawMouse.x,
        rawMouse.y,
        static_cast<float>(GetScreenWidth()),
        static_cast<float>(GetScreenHeight()),
        aspect43);
}

template <typename SaveNameInputFn,
          typename MetadataInputFn,
          typename LoadBrowserInputFn,
          typename CommandMenuInputFn,
          typename OptionPickerInputFn,
          typename ExitConfirmInputFn>
inline bool dispatchCapturedInput(bool saveNamePromptOpen,
                                  bool metadataPromptOpen,
                                  bool loadBrowserOpen,
                                  bool commandMenuOpen,
                                  bool optionPickerOpen,
                                  bool confirmExitPromptOpen,
                                  SaveNameInputFn saveNameInput,
                                  MetadataInputFn metadataInput,
                                  LoadBrowserInputFn loadBrowserInput,
                                  CommandMenuInputFn commandMenuInput,
                                  OptionPickerInputFn optionPickerInput,
                                  ExitConfirmInputFn exitConfirmInput) {
    if (saveNamePromptOpen) {
        saveNameInput();
        return true;
    }
    if (metadataPromptOpen) {
        metadataInput();
        return true;
    }
    if (loadBrowserOpen) {
        loadBrowserInput();
        return true;
    }
    if (commandMenuOpen) {
        commandMenuInput();
        return true;
    }
    if (optionPickerOpen) {
        optionPickerInput();
        return true;
    }
    if (confirmExitPromptOpen) {
        exitConfirmInput();
        return true;
    }
    return false;
}

inline void clampCamera(float& camX,
                        float& camY,
                        int stageWidth,
                        int stageHeight,
                        int tileSize,
                        int stageAreaWidth,
                        int internalHeight) {
    const float maxX =
        std::max(0.0f, static_cast<float>(stageWidth * tileSize - stageAreaWidth));
    const float maxY =
        std::max(0.0f, static_cast<float>(stageHeight * tileSize - internalHeight + 10));
    camX = std::clamp(camX, 0.0f, maxX);
    camY = std::clamp(camY, 0.0f, maxY);
}

inline void focusCameraOnPoint(float& camX,
                               float& camY,
                               float worldX,
                               float worldY,
                               int stageWidth,
                               int stageHeight,
                               int tileSize,
                               int stageAreaWidth,
                               int internalHeight) {
    camX = worldX - static_cast<float>(stageAreaWidth) * 0.5f;
    camY = worldY - static_cast<float>(internalHeight) * 0.5f;
    clampCamera(camX, camY, stageWidth, stageHeight, tileSize,
                stageAreaWidth, internalHeight);
}

inline void scrollCamera(float& camX,
                         float& camY,
                         bool left,
                         bool right,
                         bool up,
                         bool down,
                         bool fast,
                         int stageWidth,
                         int stageHeight,
                         int tileSize,
                         int stageAreaWidth,
                         int internalHeight) {
    const float scrollSpeed = fast ? 6.0f : 3.0f;
    if (left) camX -= scrollSpeed;
    if (right) camX += scrollSpeed;
    if (up) camY -= scrollSpeed;
    if (down) camY += scrollSpeed;
    clampCamera(camX, camY, stageWidth, stageHeight, tileSize,
                stageAreaWidth, internalHeight);
}

} // namespace mmx::map_editor
