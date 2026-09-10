// map_editor_toolbar.h - toolbar and picker layout helpers for the map editor.

#pragma once

#include "data/localization.h"
#include "systems/tilemap.h"
#include "raylib.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace mmx::map_editor {

inline constexpr int kToolbarRowHeight = 10;
inline constexpr int kToolbarRows = 4;
inline constexpr int kOptionPickerVisibleRows = 9;
inline constexpr int kOptionPickerPanelWidth = 218;
inline constexpr int kOptionPickerPanelHeight = 132;
inline constexpr int kCommandMenuItemCount = 5;
inline constexpr int kCommandMenuPanelWidth = 124;
inline constexpr int kCommandMenuPanelHeight = 98;

inline const char* mapEditorToolBadge(int toolIndex, Language language) {
    constexpr UiText kToolBadges[] = {
        UiText::MapEditorToolbarToolPaint,
        UiText::MapEditorToolbarToolErase,
        UiText::MapEditorToolbarToolFill,
        UiText::MapEditorToolbarToolPick,
    };
    const int index = std::clamp(toolIndex, 0, 3);
    return uiText(kToolBadges[index], language);
}

inline const char* mapEditorLayerBadge(int layerIndex, Language language) {
    return uiText(layerIndex == 0 ? UiText::MapEditorToolbarLayerMain
                                  : UiText::MapEditorToolbarLayerBack,
                  language);
}

inline const char* mapEditorCollisionBrushBadge(TileType brush, Language language) {
    constexpr UiText kBrushBadges[] = {
        UiText::MapEditorBrushNone,
        UiText::MapEditorBrushSolid,
        UiText::MapEditorBrushOneway,
        UiText::MapEditorBrushSlopeLeft,
        UiText::MapEditorBrushSlopeRight,
        UiText::MapEditorBrushSpike,
        UiText::MapEditorBrushLadder,
        UiText::MapEditorBrushBreakable,
    };
    const int index = std::clamp(static_cast<int>(brush), 0, 7);
    return uiText(kBrushBadges[index], language);
}

inline const char* mapEditorCommandLabel(int commandIndex, Language language) {
    constexpr UiText kCommandLabels[] = {
        UiText::MapEditorCommandSave,
        UiText::MapEditorCommandLoad,
        UiText::MapEditorCommandPlayerSpawn,
        UiText::MapEditorCommandPlay,
        UiText::MapEditorCommandExtras,
    };
    const int index = std::clamp(commandIndex, 0, kCommandMenuItemCount - 1);
    return uiText(kCommandLabels[index], language);
}

inline int toolbarTop(int internalHeight) {
    return internalHeight - kToolbarRowHeight * kToolbarRows;
}

inline int toolbarButtonWidth(const std::string& label) {
    return MeasureText(label.c_str(), 7) + 6;
}

inline bool toolbarButtonHit(int& controlX,
                             int y,
                             const std::string& label,
                             int stageAreaWidth,
                             Vector2 point) {
    const int w = toolbarButtonWidth(label);
    const bool fits = controlX + w <= stageAreaWidth - 2;
    const Rectangle rect = {
        static_cast<float>(controlX),
        static_cast<float>(y),
        static_cast<float>(w),
        static_cast<float>(kToolbarRowHeight),
    };
    const bool hit = fits && CheckCollisionPointRec(point, rect);
    if (fits) controlX += w + 3;
    return hit;
}

inline void drawToolbarButton(int& controlX,
                              int y,
                              const std::string& label,
                              Color fill,
                              int stageAreaWidth) {
    const int w = toolbarButtonWidth(label);
    if (controlX + w > stageAreaWidth - 2) return;
    DrawRectangle(controlX, y, w, kToolbarRowHeight, fill);
    DrawRectangleLines(controlX, y, w, kToolbarRowHeight, {96, 174, 236, 210});
    DrawText(label.c_str(), controlX + 3, y + 2, 7, {230, 238, 255, 255});
    controlX += w + 3;
}

inline std::string toolbarInfoText(int toolIndex,
                                   int layerIndex,
                                   int selectedTile,
                                   bool editCollision,
                                   TileType collisionBrush,
                                   bool hasObjectTool,
                                   UiText objectBadge,
                                   Language language) {
    char info[128];
    if (hasObjectTool) {
        snprintf(info, sizeof(info), "%s %s %s%d %s",
                 mapEditorToolBadge(toolIndex, language),
                 mapEditorLayerBadge(layerIndex, language),
                 uiText(UiText::MapEditorToolbarTilePrefix, language),
                 selectedTile,
                 uiText(objectBadge, language));
    } else if (editCollision) {
        snprintf(info, sizeof(info), "%s %s %s%d %s:%s",
                 mapEditorToolBadge(toolIndex, language),
                 mapEditorLayerBadge(layerIndex, language),
                 uiText(UiText::MapEditorToolbarTilePrefix, language),
                 selectedTile,
                 uiText(UiText::MapEditorCollisionBadge, language),
                 mapEditorCollisionBrushBadge(collisionBrush, language));
    } else {
        snprintf(info, sizeof(info), "%s %s %s%d",
                 mapEditorToolBadge(toolIndex, language),
                 mapEditorLayerBadge(layerIndex, language),
                 uiText(UiText::MapEditorToolbarTilePrefix, language),
                 selectedTile);
    }
    return info;
}

struct ToolbarRenderState {
    int internalHeight = 0;
    int stageAreaWidth = 0;
    std::string infoText;
    std::string cursorText;
    bool paletteVisible = true;
    bool showAllPaletteTiles = false;
    bool editCollision = false;
    TileType collisionBrush = TileType::Solid;
    bool artActive = true;
    bool commandMenuOpen = false;
    bool hasTilesets = false;
    int selectedTileset = 0;
    int tilesetCount = 0;
    bool hasBackgrounds = false;
    int selectedBackground = -1;
    int backgroundCount = 0;
    bool playerObjectActive = false;
    bool enemyObjectActive = false;
    bool pickupObjectActive = false;
    bool bossObjectActive = false;
    bool hasEnemies = false;
    std::string enemyLabel;
    bool hasPickups = false;
    int selectedPickup = 0;
    int pickupCount = 0;
    bool hasBosses = false;
    int selectedBoss = 0;
    int bossCount = 0;
    Language language = Language::English;
};

struct ToolbarSceneRenderInput {
    int internalHeight = 0;
    int stageAreaWidth = 0;
    int toolIndex = 0;
    int activeLayer = 0;
    int selectedTile = 0;
    bool editCollision = false;
    TileType collisionBrush = TileType::Solid;
    int objectToolIndex = 0;
    std::string cursorText;
    bool paletteVisible = true;
    bool showAllPaletteTiles = false;
    bool commandMenuOpen = false;
    int selectedTileset = 0;
    int tilesetCount = 0;
    int selectedBackground = -1;
    int backgroundCount = 0;
    std::string enemyLabel;
    int enemyCount = 0;
    int selectedPickup = 0;
    int pickupCount = 0;
    int selectedBoss = 0;
    int bossCount = 0;
    Language language = Language::English;
};

inline UiText toolbarObjectBadge(int objectToolIndex) {
    switch (objectToolIndex) {
        case 1:
            return UiText::MapEditorToolbarPlayerPrefix;
        case 2:
            return UiText::MapEditorToolbarEnemyPrefix;
        case 3:
            return UiText::MapEditorToolbarPickupPrefix;
        case 4:
            return UiText::MapEditorToolbarBossPrefix;
        default:
            return UiText::MapEditorToolbarPlayerPrefix;
    }
}

inline ToolbarRenderState buildToolbarRenderState(
    const ToolbarSceneRenderInput& input) {
    ToolbarRenderState state;
    state.internalHeight = input.internalHeight;
    state.stageAreaWidth = input.stageAreaWidth;
    state.infoText = toolbarInfoText(
        input.toolIndex,
        input.activeLayer,
        input.selectedTile,
        input.editCollision,
        input.collisionBrush,
        input.objectToolIndex != 0,
        toolbarObjectBadge(input.objectToolIndex),
        input.language);
    state.cursorText = input.cursorText;
    state.paletteVisible = input.paletteVisible;
    state.showAllPaletteTiles = input.showAllPaletteTiles;
    state.editCollision = input.editCollision;
    state.collisionBrush = input.collisionBrush;
    state.artActive = !input.editCollision && input.objectToolIndex == 0;
    state.commandMenuOpen = input.commandMenuOpen;
    state.hasTilesets = input.tilesetCount > 0;
    state.selectedTileset = input.selectedTileset;
    state.tilesetCount = input.tilesetCount;
    state.hasBackgrounds = input.backgroundCount > 0;
    state.selectedBackground = input.selectedBackground;
    state.backgroundCount = input.backgroundCount;
    state.playerObjectActive = input.objectToolIndex == 1;
    state.enemyObjectActive = input.objectToolIndex == 2;
    state.pickupObjectActive = input.objectToolIndex == 3;
    state.bossObjectActive = input.objectToolIndex == 4;
    state.hasEnemies = input.enemyCount > 0;
    state.enemyLabel = input.enemyLabel;
    state.hasPickups = input.pickupCount > 0;
    state.selectedPickup = input.selectedPickup;
    state.pickupCount = input.pickupCount;
    state.hasBosses = input.bossCount > 0;
    state.selectedBoss = input.selectedBoss;
    state.bossCount = input.bossCount;
    state.language = input.language;
    return state;
}

inline std::string toolbarPaletteLabel(const ToolbarRenderState& state) {
    return uiText(UiText::MapEditorToolbarPalette, state.language);
}

inline std::string toolbarPaletteModeLabel(const ToolbarRenderState& state) {
    return uiText(state.showAllPaletteTiles
                      ? UiText::MapEditorToolbarPaletteUsed
                      : UiText::MapEditorToolbarPaletteAll,
                  state.language);
}

inline std::string toolbarTilesetLabel(const ToolbarRenderState& state) {
    char label[32];
    snprintf(label, sizeof(label), "%s:%d/%d",
             uiText(UiText::MapEditorToolbarTilesetPrefix, state.language),
             state.selectedTileset + 1,
             state.tilesetCount);
    return label;
}

inline std::string toolbarBackgroundLabel(const ToolbarRenderState& state) {
    char label[32];
    if (state.selectedBackground >= 0) {
        snprintf(label, sizeof(label), "%s:%d/%d",
                 uiText(UiText::MapEditorToolbarBackgroundPrefix, state.language),
                 state.selectedBackground + 1,
                 state.backgroundCount);
    } else {
        snprintf(label, sizeof(label), "%s:-/%d",
                 uiText(UiText::MapEditorToolbarBackgroundPrefix, state.language),
                 state.backgroundCount);
    }
    return label;
}

inline std::string toolbarEnemyLabel(const ToolbarRenderState& state) {
    char label[32];
    snprintf(label, sizeof(label), "%s:%s",
             uiText(UiText::MapEditorToolbarEnemyPrefix, state.language),
             state.enemyLabel.c_str());
    return label;
}

inline std::string toolbarPickupLabel(const ToolbarRenderState& state) {
    char label[32];
    snprintf(label, sizeof(label), "%s:%d/%d",
             uiText(UiText::MapEditorToolbarPickupPrefix, state.language),
             state.selectedPickup + 1,
             state.pickupCount);
    return label;
}

inline std::string toolbarBossLabel(const ToolbarRenderState& state) {
    char label[32];
    snprintf(label, sizeof(label), "%s:%d/%d",
             uiText(UiText::MapEditorToolbarBossPrefix, state.language),
             state.selectedBoss + 1,
             state.bossCount);
    return label;
}

enum class ToolbarMouseAction {
    None,
    TogglePalette,
    TogglePaletteMode,
    EnterArtMode,
    SetPassableBrush,
    SetSolidBrush,
    OpenTilesetPicker,
    SelectNextTileset,
    OpenBackgroundPicker,
    SelectNextBackground,
    OpenCommandMenu,
    SelectPlayerObject,
    SelectEnemyObject,
    SelectPreviousEnemy,
    SelectNextEnemy,
    SelectPickupObject,
    SelectPreviousPickup,
    SelectNextPickup,
    SelectBossObject,
    SelectPreviousBoss,
    SelectNextBoss,
};

struct ToolbarMouseResult {
    bool consumed = false;
    ToolbarMouseAction action = ToolbarMouseAction::None;
};

inline ToolbarMouseResult toolbarMouseAction(const ToolbarRenderState& state,
                                             float internalX,
                                             float internalY,
                                             bool leftPressed,
                                             bool rightPressed,
                                             bool leftDown,
                                             bool rightDown) {
    const int toolbarY = toolbarTop(state.internalHeight);
    if (state.paletteVisible && internalX >= state.stageAreaWidth) return {};
    if (internalY < toolbarY || internalY >= state.internalHeight) return {};
    if (!leftPressed && !rightPressed) {
        return {leftDown || rightDown, ToolbarMouseAction::None};
    }

    const int mapRowY = toolbarY;
    const int stageRowY = toolbarY + kToolbarRowHeight;
    const int objectRowY = toolbarY + kToolbarRowHeight * 2;
    auto hitButton = [&](int& controlX, int y, const std::string& label) {
        return toolbarButtonHit(controlX, y, label, state.stageAreaWidth,
                                {internalX, internalY});
    };
    auto action = [](ToolbarMouseAction value) {
        return ToolbarMouseResult{true, value};
    };

    if (internalY >= mapRowY && internalY < mapRowY + kToolbarRowHeight) {
        int x = 2;
        if (hitButton(x, mapRowY, toolbarPaletteLabel(state))) {
            return action(ToolbarMouseAction::TogglePalette);
        }
        if (hitButton(x, mapRowY, toolbarPaletteModeLabel(state))) {
            return action(ToolbarMouseAction::TogglePaletteMode);
        }
        if (hitButton(x, mapRowY,
                      uiText(UiText::MapEditorToolbarArt, state.language))) {
            return action(ToolbarMouseAction::EnterArtMode);
        }
        if (hitButton(x, mapRowY,
                      uiText(UiText::MapEditorToolbarPassable, state.language))) {
            return action(ToolbarMouseAction::SetPassableBrush);
        }
        if (hitButton(x, mapRowY,
                      uiText(UiText::MapEditorToolbarSolid, state.language))) {
            return action(ToolbarMouseAction::SetSolidBrush);
        }
        return {true, ToolbarMouseAction::None};
    }

    if (internalY >= stageRowY && internalY < stageRowY + kToolbarRowHeight) {
        int x = 2;
        if (state.hasTilesets && hitButton(x, stageRowY, toolbarTilesetLabel(state))) {
            return action(rightPressed ? ToolbarMouseAction::SelectNextTileset
                                       : ToolbarMouseAction::OpenTilesetPicker);
        }
        if (state.hasBackgrounds &&
            hitButton(x, stageRowY, toolbarBackgroundLabel(state))) {
            return action(rightPressed ? ToolbarMouseAction::SelectNextBackground
                                       : ToolbarMouseAction::OpenBackgroundPicker);
        }
        if (hitButton(x, stageRowY,
                      uiText(UiText::MapEditorToolbarCommands, state.language))) {
            return action(ToolbarMouseAction::OpenCommandMenu);
        }
        return {true, ToolbarMouseAction::None};
    }

    if (internalY >= objectRowY && internalY < objectRowY + kToolbarRowHeight) {
        int x = 2;
        if (hitButton(x, objectRowY,
                      uiText(UiText::MapEditorToolbarPlayerPrefix, state.language))) {
            return action(ToolbarMouseAction::SelectPlayerObject);
        }
        if (state.hasEnemies && hitButton(x, objectRowY, toolbarEnemyLabel(state))) {
            if (rightPressed) return action(ToolbarMouseAction::SelectPreviousEnemy);
            return action(state.enemyObjectActive
                              ? ToolbarMouseAction::SelectNextEnemy
                              : ToolbarMouseAction::SelectEnemyObject);
        }
        if (state.hasPickups && hitButton(x, objectRowY, toolbarPickupLabel(state))) {
            if (rightPressed) return action(ToolbarMouseAction::SelectPreviousPickup);
            return action(state.pickupObjectActive
                              ? ToolbarMouseAction::SelectNextPickup
                              : ToolbarMouseAction::SelectPickupObject);
        }
        if (state.hasBosses && hitButton(x, objectRowY, toolbarBossLabel(state))) {
            if (rightPressed) return action(ToolbarMouseAction::SelectPreviousBoss);
            return action(state.bossObjectActive
                              ? ToolbarMouseAction::SelectNextBoss
                              : ToolbarMouseAction::SelectBossObject);
        }
        return {true, ToolbarMouseAction::None};
    }

    return {true, ToolbarMouseAction::None};
}

inline void renderToolbar(const ToolbarRenderState& state) {
    const int toolbarY = toolbarTop(state.internalHeight);
    const int mapRowY = toolbarY;
    const int stageRowY = toolbarY + kToolbarRowHeight;
    const int objectRowY = toolbarY + kToolbarRowHeight * 2;
    const int infoRowY = toolbarY + kToolbarRowHeight * 3;
    DrawRectangle(0, toolbarY, state.stageAreaWidth,
                  kToolbarRowHeight * kToolbarRows, {0, 0, 0, 180});
    DrawText(state.infoText.c_str(), 2, infoRowY + 1, 8, {200, 200, 230, 255});

    auto drawButton = [&](int& controlX, int y, const std::string& label, Color fill) {
        drawToolbarButton(controlX, y, label, fill, state.stageAreaWidth);
    };

    int mapX = 2;
    drawButton(mapX, mapRowY, toolbarPaletteLabel(state),
               state.paletteVisible ? Color{28, 78, 110, 220}
                                    : Color{50, 50, 70, 220});
    drawButton(mapX, mapRowY, toolbarPaletteModeLabel(state),
               state.showAllPaletteTiles ? Color{120, 96, 34, 235}
                                         : Color{70, 62, 44, 210});
    drawButton(mapX, mapRowY, uiText(UiText::MapEditorToolbarArt, state.language),
               state.artActive ? Color{34, 88, 132, 230}
                               : Color{42, 54, 72, 210});
    drawButton(mapX, mapRowY,
               uiText(UiText::MapEditorToolbarPassable, state.language),
               (state.editCollision && state.collisionBrush == TileType::None)
                   ? Color{30, 112, 78, 230}
                   : Color{44, 74, 62, 210});
    drawButton(mapX, mapRowY, uiText(UiText::MapEditorToolbarSolid, state.language),
               (state.editCollision && state.collisionBrush == TileType::Solid)
                   ? Color{150, 58, 48, 230}
                   : Color{74, 54, 52, 210});

    int stageX = 2;
    if (state.hasTilesets) {
        drawButton(stageX, stageRowY, toolbarTilesetLabel(state), {28, 62, 108, 220});
    }
    if (state.hasBackgrounds) {
        drawButton(stageX, stageRowY, toolbarBackgroundLabel(state),
                   {34, 66, 94, 220});
    }
    drawButton(stageX, stageRowY,
               uiText(UiText::MapEditorToolbarCommands, state.language),
               state.commandMenuOpen ? Color{110, 82, 130, 235}
                                     : Color{62, 56, 92, 220});

    int objectX = 2;
    drawButton(objectX, objectRowY,
               uiText(UiText::MapEditorToolbarPlayerPrefix, state.language),
               state.playerObjectActive ? Color{72, 136, 190, 235}
                                        : Color{42, 68, 94, 220});
    if (state.hasEnemies) {
        drawButton(objectX, objectRowY, toolbarEnemyLabel(state),
                   state.enemyObjectActive ? Color{150, 54, 62, 235}
                                           : Color{84, 44, 50, 220});
    }
    if (state.hasPickups) {
        drawButton(objectX, objectRowY, toolbarPickupLabel(state),
                   state.pickupObjectActive ? Color{154, 118, 42, 235}
                                            : Color{84, 70, 34, 220});
    }
    if (state.hasBosses) {
        drawButton(objectX, objectRowY, toolbarBossLabel(state),
                   state.bossObjectActive ? Color{128, 72, 156, 235}
                                          : Color{76, 50, 92, 220});
    }

    const int posW = MeasureText(state.cursorText.c_str(), 8);
    DrawText(state.cursorText.c_str(), state.stageAreaWidth - posW - 2,
             infoRowY + 1, 8, {150, 150, 180, 255});
}

inline int optionPickerFirstVisibleIndex(int selection, int count) {
    int first = 0;
    if (selection >= kOptionPickerVisibleRows) {
        first = selection - kOptionPickerVisibleRows + 1;
    }
    return std::max(0, std::min(first, std::max(0, count - kOptionPickerVisibleRows)));
}

inline Rectangle optionPickerPanelRect(int internalWidth, int internalHeight) {
    return {
        static_cast<float>((internalWidth - kOptionPickerPanelWidth) / 2),
        static_cast<float>((internalHeight - kOptionPickerPanelHeight) / 2),
        static_cast<float>(kOptionPickerPanelWidth),
        static_cast<float>(kOptionPickerPanelHeight),
    };
}

inline Rectangle optionPickerRowRect(int panelX, int y) {
    return {
        static_cast<float>(panelX + 8),
        static_cast<float>(y - 2),
        static_cast<float>(kOptionPickerPanelWidth - 16),
        11.0f,
    };
}

enum class OptionPickerInputAction {
    None,
    Cancel,
    Apply,
};

struct OptionPickerInputState {
    int internalWidth = 0;
    int internalHeight = 0;
    int selection = 0;
    int count = 0;
    bool escapePressed = false;
    bool confirmPressed = false;
    bool upPressed = false;
    bool downPressed = false;
    bool pageUpPressed = false;
    bool pageDownPressed = false;
    bool mouseInside = false;
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    bool leftMousePressed = false;
    bool rightMousePressed = false;
    float mouseWheel = 0.0f;
};

struct OptionPickerInputResult {
    int selection = 0;
    OptionPickerInputAction action = OptionPickerInputAction::None;
    bool tickStatusTimer = true;
};

inline int optionPickerWrapSelection(int value, int count) {
    if (count <= 0) return 0;
    int wrapped = value % count;
    if (wrapped < 0) wrapped += count;
    return wrapped;
}

inline OptionPickerInputResult handleOptionPickerInput(
    const OptionPickerInputState& state) {
    OptionPickerInputResult result;
    result.selection = optionPickerWrapSelection(state.selection, state.count);

    auto finish = [&](OptionPickerInputAction action) {
        result.action = action;
        result.tickStatusTimer = false;
        return result;
    };

    if (state.count <= 0 || state.escapePressed) {
        return finish(OptionPickerInputAction::Cancel);
    }
    if (state.confirmPressed) {
        return finish(OptionPickerInputAction::Apply);
    }
    if (state.upPressed) {
        result.selection = optionPickerWrapSelection(result.selection - 1, state.count);
    }
    if (state.downPressed) {
        result.selection = optionPickerWrapSelection(result.selection + 1, state.count);
    }
    if (state.pageUpPressed) {
        result.selection = optionPickerWrapSelection(
            result.selection - kOptionPickerVisibleRows, state.count);
    }
    if (state.pageDownPressed) {
        result.selection = optionPickerWrapSelection(
            result.selection + kOptionPickerVisibleRows, state.count);
    }

    if (state.mouseInside) {
        const Rectangle panelRect =
            optionPickerPanelRect(state.internalWidth, state.internalHeight);
        const int panelX = static_cast<int>(panelRect.x);
        const int panelY = static_cast<int>(panelRect.y);
        const int first = optionPickerFirstVisibleIndex(result.selection, state.count);
        const Vector2 mouse = {state.mouseX, state.mouseY};
        if ((state.leftMousePressed || state.rightMousePressed) &&
            !CheckCollisionPointRec(mouse, panelRect)) {
            return finish(OptionPickerInputAction::Cancel);
        }

        if (state.mouseWheel != 0.0f) {
            result.selection = optionPickerWrapSelection(
                result.selection + (state.mouseWheel < 0.0f ? 1 : -1),
                state.count);
        }

        for (int row = 0;
             row < kOptionPickerVisibleRows && first + row < state.count;
             ++row) {
            const int y = panelY + 24 + row * 11;
            if (CheckCollisionPointRec(mouse, optionPickerRowRect(panelX, y))) {
                result.selection = first + row;
                if (state.leftMousePressed) {
                    return finish(OptionPickerInputAction::Apply);
                }
            }
        }
    }

    return result;
}

struct OptionPickerRenderItem {
    std::string name;
    std::string id;
    bool current = false;
};

struct OptionPickerRenderState {
    UiText title = UiText::MapEditorTilesetPickerTitle;
    std::vector<OptionPickerRenderItem> items;
};

template <typename Entries>
inline std::vector<OptionPickerRenderItem> buildOptionPickerItems(
    const Entries& entries,
    int selectedIndex) {
    std::vector<OptionPickerRenderItem> items;
    items.reserve(entries.size());
    for (int index = 0; index < static_cast<int>(entries.size()); ++index) {
        const auto& entry = entries[static_cast<size_t>(index)];
        items.push_back({entry.name, entry.id, index == selectedIndex});
    }
    return items;
}

template <typename Tilesets, typename Backgrounds>
inline OptionPickerRenderState buildOptionPickerRenderState(
    bool pickingTileset,
    const Tilesets& tilesets,
    int selectedTileset,
    const Backgrounds& backgrounds,
    int selectedBackground) {
    OptionPickerRenderState state;
    state.title = pickingTileset ? UiText::MapEditorTilesetPickerTitle
                                 : UiText::MapEditorBackgroundPickerTitle;
    state.items = pickingTileset
        ? buildOptionPickerItems(tilesets, selectedTileset)
        : buildOptionPickerItems(backgrounds, selectedBackground);
    return state;
}

inline void renderOptionPicker(int internalWidth,
                               int internalHeight,
                               UiText title,
                               const std::vector<OptionPickerRenderItem>& items,
                               int selection,
                               Language language) {
    if (items.empty()) return;

    const Rectangle panelRect = optionPickerPanelRect(internalWidth, internalHeight);
    const int panelX = static_cast<int>(panelRect.x);
    const int panelY = static_cast<int>(panelRect.y);
    DrawRectangle(0, 0, internalWidth, internalHeight, {0, 0, 0, 96});
    DrawRectangle(panelX, panelY, kOptionPickerPanelWidth, kOptionPickerPanelHeight,
                  {0, 12, 48, 242});
    DrawRectangleLines(panelX, panelY, kOptionPickerPanelWidth, kOptionPickerPanelHeight,
                       {210, 235, 255, 255});
    DrawRectangleLines(panelX + 2, panelY + 2, kOptionPickerPanelWidth - 4,
                       kOptionPickerPanelHeight - 4, {74, 148, 236, 255});

    DrawText(uiText(title, language), panelX + 10, panelY + 8, 8,
             {255, 242, 116, 255});

    const int count = static_cast<int>(items.size());
    const int first = optionPickerFirstVisibleIndex(selection, count);
    for (int row = 0; row < kOptionPickerVisibleRows && first + row < count; ++row) {
        const int index = first + row;
        const OptionPickerRenderItem& item = items[index];
        const bool selected = index == selection;
        const int y = panelY + 24 + row * 11;
        const Color rowColor = selected ? Color{255, 230, 90, 255}
                                        : Color{220, 220, 230, 255};
        if (selected) {
            DrawText(">", panelX + 8, y, 7, rowColor);
        }
        if (item.current) {
            DrawText("*", panelX + 17, y, 7, {96, 220, 160, 255});
        }

        std::string name = item.name.empty() ? item.id : item.name;
        while (MeasureText(name.c_str(), 7) > 132 && name.size() > 1) {
            name.pop_back();
        }
        DrawText(name.c_str(), panelX + 28, y, 7, rowColor);

        std::string id = item.id;
        while (MeasureText(id.c_str(), 6) > 46 && id.size() > 1) {
            id.pop_back();
        }
        DrawText(id.c_str(), panelX + 162, y, 6,
                 selected ? Color{255, 242, 116, 255}
                          : Color{150, 170, 200, 255});
    }
}

inline Rectangle commandMenuPanelRect(int internalWidth, int internalHeight) {
    return {
        static_cast<float>((internalWidth - kCommandMenuPanelWidth) / 2),
        static_cast<float>((internalHeight - kCommandMenuPanelHeight) / 2),
        static_cast<float>(kCommandMenuPanelWidth),
        static_cast<float>(kCommandMenuPanelHeight),
    };
}

inline Rectangle commandMenuRowRect(int panelX, int y) {
    return {
        static_cast<float>(panelX + 8),
        static_cast<float>(y - 2),
        static_cast<float>(kCommandMenuPanelWidth - 16),
        12.0f,
    };
}

enum class CommandMenuInputAction {
    None,
    Cancel,
    Activate,
};

struct CommandMenuInputState {
    int internalWidth = 0;
    int internalHeight = 0;
    int selection = 0;
    bool escapePressed = false;
    bool upPressed = false;
    bool downPressed = false;
    bool confirmPressed = false;
    bool mouseInside = false;
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    bool leftMousePressed = false;
    bool rightMousePressed = false;
};

struct CommandMenuInputResult {
    int selection = 0;
    CommandMenuInputAction action = CommandMenuInputAction::None;
    bool tickStatusTimer = true;
};

inline int commandMenuWrapSelection(int value) {
    int wrapped = value % kCommandMenuItemCount;
    if (wrapped < 0) wrapped += kCommandMenuItemCount;
    return wrapped;
}

inline CommandMenuInputResult handleCommandMenuInput(
    const CommandMenuInputState& state) {
    CommandMenuInputResult result;
    result.selection = std::clamp(state.selection, 0, kCommandMenuItemCount - 1);

    auto finish = [&](CommandMenuInputAction action) {
        result.action = action;
        result.tickStatusTimer = false;
        return result;
    };

    if (state.escapePressed) {
        return finish(CommandMenuInputAction::Cancel);
    }
    if (state.upPressed) {
        result.selection = commandMenuWrapSelection(result.selection - 1);
    }
    if (state.downPressed) {
        result.selection = commandMenuWrapSelection(result.selection + 1);
    }
    if (state.confirmPressed) {
        return finish(CommandMenuInputAction::Activate);
    }

    if (state.mouseInside) {
        const Rectangle panelRect =
            commandMenuPanelRect(state.internalWidth, state.internalHeight);
        const int panelX = static_cast<int>(panelRect.x);
        const int panelY = static_cast<int>(panelRect.y);
        const Vector2 mouse = {state.mouseX, state.mouseY};
        if ((state.leftMousePressed || state.rightMousePressed) &&
            !CheckCollisionPointRec(mouse, panelRect)) {
            return finish(CommandMenuInputAction::Cancel);
        }

        for (int row = 0; row < kCommandMenuItemCount; ++row) {
            const int y = panelY + 22 + row * 13;
            if (CheckCollisionPointRec(mouse, commandMenuRowRect(panelX, y))) {
                result.selection = row;
                if (state.leftMousePressed) {
                    return finish(CommandMenuInputAction::Activate);
                }
            }
        }
    }

    return result;
}

inline void renderCommandMenu(int internalWidth,
                              int internalHeight,
                              int selection,
                              Language language) {
    const Rectangle panelRect = commandMenuPanelRect(internalWidth, internalHeight);
    const int panelX = static_cast<int>(panelRect.x);
    const int panelY = static_cast<int>(panelRect.y);
    DrawRectangle(0, 0, internalWidth, internalHeight, {0, 0, 0, 96});
    DrawRectangle(panelX, panelY, kCommandMenuPanelWidth, kCommandMenuPanelHeight,
                  {0, 12, 48, 242});
    DrawRectangleLines(panelX, panelY, kCommandMenuPanelWidth, kCommandMenuPanelHeight,
                       {210, 235, 255, 255});
    DrawRectangleLines(panelX + 2, panelY + 2, kCommandMenuPanelWidth - 4,
                       kCommandMenuPanelHeight - 4, {74, 148, 236, 255});

    DrawText(uiText(UiText::MapEditorCommandMenuTitle, language),
             panelX + 10, panelY + 8, 8, {255, 242, 116, 255});

    for (int row = 0; row < kCommandMenuItemCount; ++row) {
        const bool selected = row == selection;
        const int y = panelY + 22 + row * 13;
        const Color rowColor = selected ? Color{255, 230, 90, 255}
                                        : Color{220, 220, 230, 255};
        if (selected) {
            DrawText(">", panelX + 10, y, 7, rowColor);
        }
        DrawText(mapEditorCommandLabel(row, language), panelX + 24, y, 7, rowColor);
    }

    DrawText(uiText(UiText::MapEditorEnterCommandCancelHint, language),
             panelX + 10, panelY + kCommandMenuPanelHeight - 12, 6,
             {188, 212, 244, 255});
}

} // namespace mmx::map_editor
