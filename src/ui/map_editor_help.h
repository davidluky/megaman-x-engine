// map_editor_help.h - localized help overlay rendering for the map editor.

#pragma once

#include "data/localization.h"
#include "raylib.h"
#include <string>

namespace mmx::map_editor {

inline constexpr UiText kMapEditorHelpLines[] = {
    UiText::MapEditorHelpScroll,
    UiText::MapEditorHelpPaint,
    UiText::MapEditorHelpErase,
    UiText::MapEditorHelpPick,
    UiText::MapEditorHelpWheel,
    UiText::MapEditorHelpTools,
    UiText::MapEditorHelpPalette,
    UiText::MapEditorHelpToolbar,
    UiText::MapEditorHelpGrid,
    UiText::MapEditorHelpCollisionMode,
    UiText::MapEditorHelpCollisionView,
    UiText::MapEditorHelpLayer,
    UiText::MapEditorHelpBackground,
    UiText::MapEditorHelpParallax,
    UiText::MapEditorHelpRepeat,
    UiText::MapEditorHelpResize,
    UiText::MapEditorHelpUndo,
    UiText::MapEditorHelpRedo,
    UiText::MapEditorHelpSave,
    UiText::MapEditorHelpSaveAs,
    UiText::MapEditorHelpMapInfo,
    UiText::MapEditorHelpBrowse,
    UiText::MapEditorHelpPlayerSpawn,
    UiText::MapEditorHelpEnemyId,
    UiText::MapEditorHelpEnemySpawn,
    UiText::MapEditorHelpPickupId,
    UiText::MapEditorHelpPickupSpawn,
    UiText::MapEditorHelpCheckpointSpawn,
    UiText::MapEditorHelpBossId,
    UiText::MapEditorHelpBossSpawn,
    UiText::MapEditorHelpOverlay,
    UiText::MapEditorHelpTestPlay,
    UiText::MapEditorHelpEscape,
};

inline constexpr int kMapEditorHelpLineCount =
    static_cast<int>(sizeof(kMapEditorHelpLines) / sizeof(kMapEditorHelpLines[0]));

inline void renderHelpOverlay(int internalWidth, int internalHeight, Language language) {
    const int panelW = internalWidth - 12;
    const int panelH = 164;
    const int panelX = (internalWidth - panelW) / 2;
    const int panelY = (internalHeight - panelH) / 2;
    DrawRectangle(0, 0, internalWidth, internalHeight, {0, 0, 0, 136});
    DrawRectangle(panelX, panelY, panelW, panelH, {0, 12, 48, 244});
    DrawRectangleLines(panelX, panelY, panelW, panelH, {210, 235, 255, 255});
    DrawRectangleLines(panelX + 2, panelY + 2, panelW - 4, panelH - 4,
                       {74, 148, 236, 255});

    DrawText(uiText(UiText::MapEditorHelpTitle, language),
             panelX + 10, panelY + 8, 8, {255, 242, 116, 255});
    const char* closeHint = uiText(UiText::MapEditorHelpCloseHint, language);
    DrawText(closeHint, panelX + panelW - MeasureText(closeHint, 6) - 10,
             panelY + 8, 6, {188, 212, 244, 255});

    const int rows = (kMapEditorHelpLineCount + 1) / 2;
    const int colW = (panelW - 26) / 2;
    const int firstY = panelY + 25;
    for (int i = 0; i < kMapEditorHelpLineCount; ++i) {
        const int col = i / rows;
        const int row = i % rows;
        const int x = panelX + 10 + col * (colW + 8);
        const int y = firstY + row * 8;
        std::string line = uiText(kMapEditorHelpLines[i], language);
        while (MeasureText(line.c_str(), 6) > colW && line.size() > 1) {
            line.pop_back();
        }
        DrawText(line.c_str(), x, y, 6, {230, 238, 255, 255});
    }
}

} // namespace mmx::map_editor
