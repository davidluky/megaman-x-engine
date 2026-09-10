// map_editor_palette.h - pure tile-palette helpers for the map editor.

#pragma once

#include "data/localization.h"
#include "systems/raylib_resource.h"
#include "raylib.h"
#include <algorithm>
#include <cstdio>
#include <nlohmann/json.hpp>
#include <unordered_set>
#include <vector>

namespace mmx::map_editor {

inline std::vector<int> buildPaletteTileIds(int totalTiles,
                                            const std::vector<int>& excludedTiles) {
    if (totalTiles <= 0) return {};

    std::unordered_set<int> excluded;
    excluded.reserve(excludedTiles.size());
    for (const int tileId : excludedTiles) {
        if (tileId >= 0 && tileId < totalTiles) {
            excluded.insert(tileId);
        }
    }

    std::vector<int> tileIds;
    tileIds.reserve(static_cast<size_t>(totalTiles) - excluded.size());
    for (int tileId = 0; tileId < totalTiles; ++tileId) {
        if (excluded.count(tileId) == 0) {
            tileIds.push_back(tileId);
        }
    }
    return tileIds;
}

inline std::vector<int> readPaletteExclude(const nlohmann::json& metadata) {
    std::vector<int> excluded;
    if (!metadata.contains("paletteExclude") || !metadata["paletteExclude"].is_array()) {
        return excluded;
    }

    auto addTile = [&](int tileId) {
        if (tileId >= 0) excluded.push_back(tileId);
    };
    auto addRange = [&](int start, int count) {
        if (start < 0 || count <= 0) return;
        for (int offset = 0; offset < count; ++offset) {
            excluded.push_back(start + offset);
        }
    };

    for (const auto& item : metadata["paletteExclude"]) {
        try {
            if (item.is_number_integer()) {
                addTile(item.get<int>());
            } else if (item.is_object()) {
                const int start = item.value("start", -1);
                if (item.contains("count") && item["count"].is_number_integer()) {
                    addRange(start, item["count"].get<int>());
                } else if (item.contains("end") && item["end"].is_number_integer()) {
                    const int end = item["end"].get<int>();
                    addRange(start, end - start + 1);
                }
            }
        } catch (const nlohmann::json::exception&) {
            continue;
        }
    }

    std::sort(excluded.begin(), excluded.end());
    excluded.erase(std::unique(excluded.begin(), excluded.end()), excluded.end());
    return excluded;
}

inline std::vector<int> buildStagePaletteTileIds(const nlohmann::json& stage,
                                                 int rawTileCount,
                                                 const std::vector<int>& excludedTiles) {
    if (rawTileCount <= 0 || !stage.contains("layers") || !stage["layers"].is_array()) {
        return {};
    }

    std::unordered_set<int> excluded;
    excluded.reserve(excludedTiles.size());
    for (const int tileId : excludedTiles) {
        if (tileId >= 0 && tileId < rawTileCount) {
            excluded.insert(tileId);
        }
    }

    std::unordered_set<int> seen;
    std::vector<int> tileIds;
    auto addTile = [&](int tileId) {
        if (tileId < 0 || tileId >= rawTileCount || excluded.count(tileId) != 0) {
            return;
        }
        if (seen.insert(tileId).second) {
            tileIds.push_back(tileId);
        }
    };

    for (const auto& layer : stage["layers"]) {
        if (!layer.is_object() || !layer.contains("data") || !layer["data"].is_array()) {
            continue;
        }
        for (const auto& value : layer["data"]) {
            try {
                if (value.is_number_integer()) {
                    addTile(value.get<int>());
                }
            } catch (const nlohmann::json::exception&) {
                continue;
            }
        }
    }

    std::sort(tileIds.begin(), tileIds.end());
    return tileIds;
}

inline int paletteVisibleRows(int internalHeight,
                              int headerHeight,
                              int pagerHeight,
                              int tilePx) {
    return std::max(1, (internalHeight - headerHeight - pagerHeight) / tilePx);
}

inline int paletteMaxScroll(int totalTiles, int paletteCols, int visibleRows) {
    return std::max(0, (totalTiles + paletteCols - 1) / paletteCols - visibleRows);
}

inline int clampedPaletteScroll(int paletteScroll, int maxScroll) {
    return std::clamp(paletteScroll, 0, maxScroll);
}

struct PalettePageResult {
    int scroll = 0;
    int currentPage = 1;
    int totalPages = 1;
};

inline PalettePageResult pagePaletteScroll(int paletteScroll,
                                           int direction,
                                           int maxScroll,
                                           int visibleRows) {
    PalettePageResult result;
    const int step = std::max(1, visibleRows - 1);
    if (maxScroll <= 0) {
        result.scroll = 0;
        return result;
    }

    if (direction < 0) {
        result.scroll = paletteScroll <= 0 ? maxScroll : paletteScroll - step;
    } else {
        result.scroll = paletteScroll >= maxScroll ? 0 : paletteScroll + step;
    }
    result.scroll = clampedPaletteScroll(result.scroll, maxScroll);
    result.totalPages = ((maxScroll + step - 1) / step) + 1;
    result.currentPage =
        std::min(result.totalPages, ((result.scroll + step - 1) / step) + 1);
    return result;
}

struct PaletteMouseInputState {
    bool paletteVisible = false;
    float internalX = 0.0f;
    float internalY = 0.0f;
    int stageAreaWidth = 0;
    int internalHeight = 0;
    int paletteScroll = 0;
    int totalTiles = 0;
    int visibleRows = 0;
    int paletteCols = 0;
    int paletteTilePx = 0;
    int headerHeight = 0;
    int paletteWidth = 0;
    float wheel = 0.0f;
    bool leftPressed = false;
    bool rightPressed = false;
    bool leftDown = false;
};

inline PaletteMouseInputState paletteMouseInputState(bool paletteVisible,
                                                     float internalX,
                                                     float internalY,
                                                     int stageAreaWidth,
                                                     int internalHeight,
                                                     int paletteScroll,
                                                     int totalTiles,
                                                     int visibleRows,
                                                     int paletteCols,
                                                     int paletteTilePx,
                                                     int headerHeight,
                                                     int paletteWidth) {
    PaletteMouseInputState state;
    state.paletteVisible = paletteVisible;
    state.internalX = internalX;
    state.internalY = internalY;
    state.stageAreaWidth = stageAreaWidth;
    state.internalHeight = internalHeight;
    state.paletteScroll = paletteScroll;
    state.totalTiles = totalTiles;
    state.visibleRows = visibleRows;
    state.paletteCols = paletteCols;
    state.paletteTilePx = paletteTilePx;
    state.headerHeight = headerHeight;
    state.paletteWidth = paletteWidth;
    state.wheel = GetMouseWheelMove();
    state.leftPressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    state.rightPressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    state.leftDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    return state;
}

struct PaletteMouseInputResult {
    bool overPalette = false;
    int pageDirection = 0;
    int selectedPaletteIndex = -1;
    int scrollDelta = 0;
};

inline int paletteIndexAt(float internalX,
                          float internalY,
                          int stageAreaWidth,
                          int paletteScroll,
                          int totalTiles,
                          int paletteCols,
                          int paletteTilePx,
                          int headerHeightPx,
                          int paletteWidthPx) {
    if (totalTiles <= 0 || paletteCols <= 0 || paletteTilePx <= 0 ||
        paletteWidthPx <= 0) {
        return -1;
    }
    if (internalX < static_cast<float>(stageAreaWidth) ||
        internalX >= static_cast<float>(stageAreaWidth + paletteWidthPx)) {
        return -1;
    }

    const float localX = internalX - static_cast<float>(stageAreaWidth);
    const float localY = internalY - static_cast<float>(headerHeightPx);
    if (localY < 0.0f) {
        return -1;
    }

    const int col = static_cast<int>(localX) / paletteTilePx;
    if (col < 0 || col >= paletteCols) {
        return -1;
    }

    const int visibleRow = static_cast<int>(localY) / paletteTilePx;
    const int row = visibleRow + paletteScroll;
    if (row < 0) {
        return -1;
    }

    const int index = row * paletteCols + col;
    return index >= 0 && index < totalTiles ? index : -1;
}

inline PaletteMouseInputResult handlePaletteMouseInput(
    const PaletteMouseInputState& state) {
    PaletteMouseInputResult result;
    result.overPalette =
        state.paletteVisible &&
        state.internalX >= static_cast<float>(state.stageAreaWidth);
    if (!result.overPalette) return result;

    const int footerY = state.headerHeight + state.visibleRows * state.paletteTilePx;
    if (state.internalY >= static_cast<float>(footerY) &&
        state.internalY < static_cast<float>(state.internalHeight)) {
        if (state.leftPressed || state.rightPressed) {
            const float localX = state.internalX - static_cast<float>(state.stageAreaWidth);
            result.pageDirection = localX < state.paletteWidth * 0.5f ? -1 : 1;
        }
        return result;
    }

    result.selectedPaletteIndex = paletteIndexAt(
        state.internalX,
        state.internalY,
        state.stageAreaWidth,
        state.paletteScroll,
        state.totalTiles,
        state.paletteCols,
        state.paletteTilePx,
        state.headerHeight,
        state.paletteWidth);
    if (!state.leftDown) {
        result.selectedPaletteIndex = -1;
    }
    if (state.wheel != 0.0f) {
        result.scrollDelta = -static_cast<int>(state.wheel * 2.0f);
    }
    return result;
}

struct PaletteRenderState {
    const TextureResource* tileset = nullptr;
    const std::vector<int>* paletteTileIds = nullptr;
    int paletteX = 0;
    int paletteWidth = 0;
    int internalHeight = 0;
    int headerHeight = 0;
    int paletteCols = 0;
    int paletteTilePx = 0;
    int tileSize = 0;
    int tilesetCols = 0;
    int totalTiles = 0;
    int selectedTile = 0;
    int paletteScroll = 0;
    int visibleRows = 0;
    int maxScroll = 0;
    bool showAllPaletteTiles = false;
    Language language = Language::English;
};

inline void renderPalette(const PaletteRenderState& state) {
    DrawRectangle(state.paletteX, 0, state.paletteWidth, state.internalHeight,
                  {20, 20, 40, 240});
    DrawLine(state.paletteX, 0, state.paletteX, state.internalHeight,
             {80, 80, 120, 255});

    const char* paletteModeLabel = state.showAllPaletteTiles
        ? uiText(UiText::MapEditorToolbarPaletteAll, state.language)
        : uiText(UiText::MapEditorPaletteTitle, state.language);
    DrawText(paletteModeLabel,
             state.paletteX + 2, 1, 8,
             state.showAllPaletteTiles ? Color{255, 220, 100, 255}
                                       : Color{150, 210, 255, 255});

    char paletteCountText[12];
    snprintf(paletteCountText, sizeof(paletteCountText), "%d", state.totalTiles);
    const int countW = MeasureText(paletteCountText, 6);
    DrawText(paletteCountText,
             state.paletteX + state.paletteWidth - countW - 3,
             2,
             6,
             {180, 180, 220, 255});

    if (state.tileset && state.tileset->valid() &&
        state.tilesetCols > 0 && state.paletteTileIds) {
        for (int vr = 0; vr < state.visibleRows; vr++) {
            const int row = vr + state.paletteScroll;
            for (int col = 0; col < state.paletteCols; col++) {
                const int paletteIndex = row * state.paletteCols + col;
                if (paletteIndex >= state.totalTiles) break;
                const int tid = (*state.paletteTileIds)[paletteIndex];

                const int atlasCol = tid % state.tilesetCols;
                const int atlasRow = tid / state.tilesetCols;
                const int tx = atlasCol * state.tileSize;
                const int ty = atlasRow * state.tileSize;

                const int dx = state.paletteX + col * state.paletteTilePx;
                const int dy = state.headerHeight + vr * state.paletteTilePx;

                const Rectangle src = {
                    static_cast<float>(tx),
                    static_cast<float>(ty),
                    static_cast<float>(state.tileSize),
                    static_cast<float>(state.tileSize),
                };
                const Rectangle dst = {
                    static_cast<float>(dx),
                    static_cast<float>(dy),
                    static_cast<float>(state.paletteTilePx),
                    static_cast<float>(state.paletteTilePx),
                };
                DrawTexturePro(state.tileset->get(), src, dst, {0, 0}, 0.0f, WHITE);

                if (tid == state.selectedTile) {
                    DrawRectangleLines(dx, dy, state.paletteTilePx, state.paletteTilePx,
                                       {255, 255, 100, 255});
                }
            }
        }
    }

    if (state.totalTiles > 0) {
        const int totalRows = (state.totalTiles + state.paletteCols - 1) /
            state.paletteCols;
        if (totalRows > state.visibleRows) {
            const float scrollRatio =
                static_cast<float>(state.paletteScroll) /
                (totalRows - state.visibleRows);
            const int scrollTrackH = state.visibleRows * state.paletteTilePx;
            const int barH = std::max(4, scrollTrackH * state.visibleRows / totalRows);
            const int barY = state.headerHeight +
                static_cast<int>(scrollRatio * (scrollTrackH - barH));
            DrawRectangle(state.paletteX + state.paletteWidth - 2, barY, 2, barH,
                          {100, 100, 200, 180});
        }
    }

    const int footerY = state.headerHeight + state.visibleRows * state.paletteTilePx;
    DrawLine(state.paletteX, footerY, state.paletteX + state.paletteWidth, footerY,
             {80, 80, 120, 255});
    DrawText("<", state.paletteX + 4, footerY + 2, 8, {230, 238, 255, 255});
    DrawText(">", state.paletteX + state.paletteWidth - 9, footerY + 2, 8,
             {230, 238, 255, 255});

    const int step = std::max(1, state.visibleRows - 1);
    const int totalPages =
        state.maxScroll <= 0 ? 1 : ((state.maxScroll + step - 1) / step) + 1;
    const int currentPage = state.maxScroll <= 0
        ? 1
        : std::min(totalPages, ((state.paletteScroll + step - 1) / step) + 1);
    char pageText[16];
    snprintf(pageText, sizeof(pageText), "%d/%d", currentPage, totalPages);
    const int pageW = MeasureText(pageText, 7);
    DrawText(pageText,
             state.paletteX + (state.paletteWidth - pageW) / 2,
             footerY + 3,
             7,
             {255, 242, 116, 255});
}

} // namespace mmx::map_editor
