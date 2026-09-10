#pragma once

#include "systems/raylib_resource.h"
#include "ui/map_editor_spawns.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace mmx::map_editor {

inline bool drawEditorObjectTexturePreview(const TextureResource* texture,
                                           Rectangle src,
                                           float maxW,
                                           float maxH,
                                           Color tint,
                                           int screenX,
                                           int screenY) {
    if (!texture || !texture->valid() || src.width <= 0.0f || src.height <= 0.0f) {
        return false;
    }
    src.width = std::min(src.width, static_cast<float>(texture->width()) - src.x);
    src.height = std::min(src.height, static_cast<float>(texture->height()) - src.y);
    if (src.width <= 0.0f || src.height <= 0.0f) return false;

    const float scale = std::min(maxW / src.width, maxH / src.height);
    const Rectangle dst = {
        static_cast<float>(screenX) - src.width * scale * 0.5f,
        static_cast<float>(screenY) - src.height * scale * 0.5f,
        src.width * scale,
        src.height * scale,
    };
    DrawTexturePro(texture->get(), src, dst, {0.0f, 0.0f}, 0.0f, tint);
    return true;
}

template <typename TextureLookup>
inline void renderObjectPreview(const EditorSpawn& spawn,
                                int screenX,
                                int screenY,
                                Color markerColor,
                                TextureLookup&& textureForPath) {
    if (spawn.type == "player_spawn") {
        if (drawEditorObjectTexturePreview(
                textureForPath("content/x1/sprites/x_spritesheet.png"),
                {0.0f, 0.0f, 70.0f, 70.0f},
                28.0f,
                28.0f,
                {255, 255, 255, 150},
                screenX,
                screenY)) {
            DrawRectangleLines(screenX - 9, screenY - 12, 18, 24, markerColor);
            return;
        }
    }

    if (spawn.type == "enemy") {
        if (const EditorSpritePreviewSpec* spec = enemyPreviewSpecForId(spawn.id)) {
            const TextureResource* texture = textureForPath(spec->path);
            if (texture && texture->valid()) {
                const float frameW = static_cast<float>(
                    std::max(1, std::min(spec->frameWidth, texture->width())));
                const float frameH = static_cast<float>(
                    std::max(1, std::min(spec->frameHeight, texture->height())));
                if (drawEditorObjectTexturePreview(texture,
                                                   {0.0f, 0.0f, frameW, frameH},
                                                   30.0f,
                                                   26.0f,
                                                   {255, 255, 255, 145},
                                                   screenX,
                                                   screenY)) {
                    DrawRectangleLines(screenX - 11, screenY - 10, 22, 20, markerColor);
                    return;
                }
            }
        }
    }

    if (spawn.type == "pickup") {
        if (spawn.id == "heart-tank") {
            const Color fill = {255, 72, 100, 150};
            DrawCircle(screenX - 4, screenY - 4, 5.0f, fill);
            DrawCircle(screenX + 4, screenY - 4, 5.0f, fill);
            DrawTriangle({static_cast<float>(screenX - 10), static_cast<float>(screenY - 1)},
                         {static_cast<float>(screenX + 10), static_cast<float>(screenY - 1)},
                         {static_cast<float>(screenX), static_cast<float>(screenY + 11)},
                         fill);
            DrawRectangleLines(screenX - 9, screenY - 10, 18, 20, markerColor);
            return;
        }

        if (spawn.id == "sub-tank" || spawn.id == "energy-tank") {
            DrawRectangle(screenX - 5, screenY - 11, 10, 22, {64, 226, 220, 145});
            DrawRectangle(screenX - 3, screenY - 8, 6, 16, {30, 86, 170, 120});
            DrawRectangleLines(screenX - 5, screenY - 11, 10, 22, markerColor);
            DrawLine(screenX - 5, screenY - 4, screenX + 5, screenY - 4, markerColor);
            DrawLine(screenX - 5, screenY + 4, screenX + 5, screenY + 4, markerColor);
            return;
        }
    }

    if (spawn.type == "checkpoint") {
        DrawLine(screenX - 4, screenY - 12, screenX - 4, screenY + 11,
                 {80, 220, 255, 180});
        DrawTriangle({static_cast<float>(screenX - 4), static_cast<float>(screenY - 12)},
                     {static_cast<float>(screenX + 10), static_cast<float>(screenY - 8)},
                     {static_cast<float>(screenX - 4), static_cast<float>(screenY - 4)},
                     {80, 220, 255, 125});
        DrawRectangleLines(screenX - 8, screenY - 12, 20, 24, markerColor);
        return;
    }

    if (spawn.type == "boss") {
        DrawCircle(screenX, screenY, 11.0f, {210, 120, 255, 110});
        DrawRectangleLines(screenX - 11, screenY - 11, 22, 22, markerColor);
        return;
    }

    DrawRectangle(screenX - 5, screenY - 5, 11, 11, {0, 0, 0, 160});
    DrawRectangleLines(screenX - 5, screenY - 5, 11, 11, markerColor);
}

template <typename TextureLookup>
inline void renderObjectOverlay(const std::vector<EditorSpawn>& spawns,
                                int stageAreaWidth,
                                int internalHeight,
                                float camX,
                                float camY,
                                TextureLookup&& textureForPath) {
    if (spawns.empty()) return;

    BeginScissorMode(0, 0, stageAreaWidth, internalHeight);
    for (const auto& spawn : spawns) {
        const int sx = static_cast<int>(std::round(spawn.x - camX));
        const int sy = static_cast<int>(std::round(spawn.y - camY));
        if (sx < -12 || sx > stageAreaWidth + 12 ||
            sy < -12 || sy > internalHeight + 12) {
            continue;
        }

        const Color color = editorSpawnMarkerColor(spawn.type);
        const std::string label = editorSpawnMarkerLabel(spawn);
        renderObjectPreview(spawn, sx, sy, color, textureForPath);
        DrawLine(sx - 7, sy, sx + 7, sy, color);
        DrawLine(sx, sy - 7, sx, sy + 7, color);
        DrawText(label.c_str(), sx + 8, sy - 4, 7, {0, 0, 0, 180});
        DrawText(label.c_str(), sx + 7, sy - 5, 7, color);
    }
    EndScissorMode();
}

} // namespace mmx::map_editor
