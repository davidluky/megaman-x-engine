// map_editor_canvas.h - canvas layer/grid/collision rendering helpers.

#pragma once

#include "data/localization.h"
#include "systems/raylib_resource.h"
#include "systems/tilemap.h"
#include "raylib.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace mmx::map_editor {

struct PreviewLayerRenderState {
    const TextureResource* texture = nullptr;
    float parallaxX = 1.0f;
    float parallaxY = 1.0f;
    int offsetX = 0;
    int offsetY = 0;
    bool repeatX = false;
    bool repeatY = false;
    int stageAreaWidth = 0;
    int internalHeight = 0;
    float camX = 0.0f;
    float camY = 0.0f;
};

template <typename PreviewLayer>
inline PreviewLayerRenderState previewLayerRenderState(const PreviewLayer& preview,
                                                       int stageAreaWidth,
                                                       int internalHeight,
                                                       float camX,
                                                       float camY) {
    PreviewLayerRenderState state;
    state.texture = &preview.texture;
    state.parallaxX = preview.parallaxX;
    state.parallaxY = preview.parallaxY;
    state.offsetX = preview.offsetX;
    state.offsetY = preview.offsetY;
    state.repeatX = preview.repeatX;
    state.repeatY = preview.repeatY;
    state.stageAreaWidth = stageAreaWidth;
    state.internalHeight = internalHeight;
    state.camX = camX;
    state.camY = camY;
    return state;
}

inline int previewPositiveModulo(int value, int divisor) {
    if (divisor <= 0) return 0;
    int result = value % divisor;
    return result < 0 ? result + divisor : result;
}

inline void renderPreviewLayer(const PreviewLayerRenderState& state, Color tint) {
    if (!state.texture || !state.texture->valid()) return;

    const int texW = state.texture->width();
    const int texH = state.texture->height();
    if (texW <= 0 || texH <= 0) return;

    const int scrollX = static_cast<int>(state.camX * state.parallaxX) + state.offsetX;
    const int scrollY = static_cast<int>(state.camY * state.parallaxY) + state.offsetY;
    const int firstSrcX = state.repeatX
        ? previewPositiveModulo(scrollX, texW)
        : std::clamp(scrollX, 0, std::max(0, texW - 1));
    const int firstSrcY = state.repeatY
        ? previewPositiveModulo(scrollY, texH)
        : std::clamp(scrollY, 0, std::max(0, texH - 1));
    const int firstDstX = (!state.repeatX && scrollX < 0) ? -scrollX : 0;
    const int firstDstY = (!state.repeatY && scrollY < 0) ? -scrollY : 0;

    for (int dstY = firstDstY, srcY = firstSrcY; dstY < state.internalHeight;) {
        const int drawH = std::min(texH - srcY, state.internalHeight - dstY);
        if (drawH <= 0) break;

        for (int dstX = firstDstX, srcX = firstSrcX; dstX < state.stageAreaWidth;) {
            const int drawW = std::min(texW - srcX, state.stageAreaWidth - dstX);
            if (drawW <= 0) break;

            Rectangle src = {
                static_cast<float>(srcX),
                static_cast<float>(srcY),
                static_cast<float>(drawW),
                static_cast<float>(drawH),
            };
            Rectangle dst = {
                static_cast<float>(dstX),
                static_cast<float>(dstY),
                static_cast<float>(drawW),
                static_cast<float>(drawH),
            };
            DrawTexturePro(state.texture->get(), src, dst, {0, 0}, 0.0f, tint);

            if (!state.repeatX) break;
            dstX += drawW;
            srcX = 0;
        }

        if (!state.repeatY) break;
        dstY += drawH;
        srcY = 0;
    }
}

inline void renderCursor(const TextureResource& tileset,
                         int tilesetCols,
                         int selectedTile,
                         int cursorX,
                         int cursorY,
                         float camX,
                         float camY,
                         int stageAreaWidth,
                         int internalHeight,
                         int tileSize,
                         bool editCollision) {
    const int sx = cursorX * tileSize - static_cast<int>(camX);
    const int sy = cursorY * tileSize - static_cast<int>(camY);

    if (sx < 0 || sx >= stageAreaWidth || sy < 0 || sy >= internalHeight) return;

    if (!editCollision && tileset.valid() && selectedTile >= 0 && tilesetCols > 0) {
        const int tx = (selectedTile % tilesetCols) * tileSize;
        const int ty = (selectedTile / tilesetCols) * tileSize;
        Rectangle src = {static_cast<float>(tx), static_cast<float>(ty),
                         static_cast<float>(tileSize), static_cast<float>(tileSize)};
        Rectangle dst = {static_cast<float>(sx), static_cast<float>(sy),
                         static_cast<float>(tileSize), static_cast<float>(tileSize)};
        DrawTexturePro(tileset.get(), src, dst, {0, 0}, 0.0f, {255, 255, 255, 128});
    }

    Color cursorColor = editCollision ? Color{255, 100, 100, 200}
                                      : Color{100, 200, 255, 200};
    DrawRectangleLines(sx, sy, tileSize, tileSize, cursorColor);
}

inline std::string cursorStatusText(int cursorX,
                                    int cursorY,
                                    int stageWidth,
                                    int stageHeight,
                                    int activeLayer,
                                    const std::vector<int>& layerData,
                                    const std::vector<int>& layerDataBg,
                                    Language language) {
    const char* tilePrefix = uiText(UiText::MapEditorToolbarTilePrefix, language);
    if (cursorX < 0 || cursorX >= stageWidth ||
        cursorY < 0 || cursorY >= stageHeight) {
        char out[32];
        snprintf(out, sizeof(out), "%d,%d %s:%s", cursorX, cursorY,
                 tilePrefix,
                 uiText(UiText::MapEditorToolbarOutOfBounds, language));
        return out;
    }

    const auto& layer = (activeLayer == 0) ? layerData : layerDataBg;
    const int idx = cursorY * stageWidth + cursorX;
    const int tileId =
        (idx >= 0 && idx < static_cast<int>(layer.size())) ? layer[idx] : -1;

    char out[40];
    if (tileId >= 0) {
        snprintf(out, sizeof(out), "%d,%d %s:%d", cursorX, cursorY,
                 tilePrefix, tileId);
    } else {
        snprintf(out, sizeof(out), "%d,%d %s:-", cursorX, cursorY, tilePrefix);
    }
    return out;
}

inline void renderStatus(const std::string& statusMsg,
                         int statusTimer,
                         int stageAreaWidth) {
    if (statusTimer <= 0 || statusMsg.empty()) return;

    const int alpha = std::min(255, statusTimer * 4);
    const int w = MeasureText(statusMsg.c_str(), 8);
    const int x = (stageAreaWidth - w) / 2;
    DrawRectangle(x - 2, 1, w + 4, 10,
                  {0, 0, 0, static_cast<unsigned char>(alpha * 3 / 4)});
    DrawText(statusMsg.c_str(), x, 2, 8,
             {255, 255, 100, static_cast<unsigned char>(alpha)});
}

inline void renderTileLayer(const TextureResource& tileset,
                            const std::vector<int>& layer,
                            int tilesetCols,
                            int stageWidth,
                            int stageHeight,
                            int areaWidth,
                            float camX,
                            float camY,
                            int tileSize,
                            int internalHeight,
                            Color tint) {
    if (!tileset.valid() || tilesetCols <= 0) return;

    const int startCol = std::max(0, static_cast<int>(camX) / tileSize);
    const int startRow = std::max(0, static_cast<int>(camY) / tileSize);
    const int endCol = std::min(stageWidth, startCol + areaWidth / tileSize + 2);
    const int endRow = std::min(stageHeight, startRow + internalHeight / tileSize + 2);

    for (int row = startRow; row < endRow; row++) {
        for (int col = startCol; col < endCol; col++) {
            int idx = row * stageWidth + col;
            if (idx >= static_cast<int>(layer.size())) continue;
            int tid = layer[idx];
            if (tid < 0) continue;

            int tx = (tid % tilesetCols) * tileSize;
            int ty = (tid / tilesetCols) * tileSize;
            int sx = col * tileSize - static_cast<int>(camX);
            int sy = row * tileSize - static_cast<int>(camY);

            Rectangle src = {
                static_cast<float>(tx),
                static_cast<float>(ty),
                static_cast<float>(tileSize),
                static_cast<float>(tileSize),
            };
            Rectangle dst = {
                static_cast<float>(sx),
                static_cast<float>(sy),
                static_cast<float>(tileSize),
                static_cast<float>(tileSize),
            };
            DrawTexturePro(tileset.get(), src, dst, {0, 0}, 0.0f, tint);
        }
    }
}

inline void renderGrid(int stageWidth,
                       int stageHeight,
                       int areaWidth,
                       float camX,
                       float camY,
                       int tileSize,
                       int internalHeight,
                       int screenTileWidth,
                       int screenTileHeight,
                       Language language) {
    Color gridColor = {60, 60, 80, 80};
    Color screenColor = {255, 236, 120, 150};

    int startCol = static_cast<int>(camX) / tileSize;
    int startRow = static_cast<int>(camY) / tileSize;

    for (int col = startCol; col <= stageWidth; col++) {
        int x = col * tileSize - static_cast<int>(camX);
        if (x >= 0 && x < areaWidth) {
            DrawLine(x, 0, x, internalHeight, gridColor);
        }
    }
    for (int row = startRow; row <= stageHeight; row++) {
        int y = row * tileSize - static_cast<int>(camY);
        if (y >= 0 && y < internalHeight) {
            DrawLine(0, y, areaWidth, y, gridColor);
        }
    }

    int rightEdge = stageWidth * tileSize - static_cast<int>(camX);
    int bottomEdge = stageHeight * tileSize - static_cast<int>(camY);
    if (rightEdge >= 0 && rightEdge < areaWidth) {
        DrawLine(rightEdge, 0, rightEdge, internalHeight, {255, 100, 100, 180});
    }
    if (bottomEdge >= 0 && bottomEdge < internalHeight) {
        DrawLine(0, bottomEdge, areaWidth, bottomEdge, {255, 100, 100, 180});
    }

    const int firstScreenCol = (startCol / screenTileWidth) * screenTileWidth;
    for (int col = firstScreenCol; col <= stageWidth; col += screenTileWidth) {
        const int x = col * tileSize - static_cast<int>(camX);
        if (x < 0 || x >= areaWidth) continue;
        DrawLine(x, 0, x, internalHeight, screenColor);
        if (x + 1 < areaWidth) {
            DrawLine(x + 1, 0, x + 1, internalHeight, screenColor);
        }
        char label[16];
        snprintf(label, sizeof(label), "%s%d",
                 uiText(UiText::MapEditorScreenPrefix, language),
                 col / screenTileWidth + 1);
        DrawText(label, x + 4, 2, 8, {0, 0, 0, 190});
        DrawText(label, x + 3, 1, 8, screenColor);
    }

    const int firstScreenRow = (startRow / screenTileHeight) * screenTileHeight;
    for (int row = firstScreenRow; row <= stageHeight; row += screenTileHeight) {
        const int y = row * tileSize - static_cast<int>(camY);
        if (y < 0 || y >= internalHeight) continue;
        DrawLine(0, y, areaWidth, y, screenColor);
        if (y + 1 < internalHeight) {
            DrawLine(0, y + 1, areaWidth, y + 1, screenColor);
        }
    }
}

inline void renderCollisionOverlay(const std::vector<TileType>& collision,
                                   int stageWidth,
                                   int stageHeight,
                                   int areaWidth,
                                   float camX,
                                   float camY,
                                   int tileSize,
                                   int internalHeight) {
    int startCol = std::max(0, static_cast<int>(camX) / tileSize);
    int startRow = std::max(0, static_cast<int>(camY) / tileSize);
    int endCol = std::min(stageWidth, startCol + areaWidth / tileSize + 2);
    int endRow = std::min(stageHeight, startRow + internalHeight / tileSize + 2);

    for (int row = startRow; row < endRow; row++) {
        for (int col = startCol; col < endCol; col++) {
            int idx = row * stageWidth + col;
            if (idx >= static_cast<int>(collision.size())) continue;
            TileType t = collision[idx];
            if (t == TileType::None) continue;

            int sx = col * tileSize - static_cast<int>(camX);
            int sy = row * tileSize - static_cast<int>(camY);

            Color overlay = {0, 0, 0, 0};
            switch (t) {
                case TileType::Solid:     overlay = {255, 60, 60, 80};   break;
                case TileType::OneWay:    overlay = {255, 255, 0, 80};   break;
                case TileType::Spike:     overlay = {255, 0, 0, 100};    break;
                case TileType::Breakable: overlay = {180, 120, 60, 80};  break;
                case TileType::Ladder:    overlay = {0, 200, 100, 80};   break;
                default:                  overlay = {100, 100, 200, 60}; break;
            }
            DrawRectangle(sx, sy, tileSize, tileSize, overlay);
        }
    }
}

} // namespace mmx::map_editor
