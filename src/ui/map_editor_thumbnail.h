// map_editor_thumbnail.h - saved-map thumbnail generation for map browsers.

#pragma once

#include "systems/raylib_resource.h"
#include "systems/tilemap.h"
#include "ui/map_editor_spawns.h"
#include "ui/map_editor_user_maps.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <vector>

namespace mmx::map_editor {

inline constexpr int kUserMapThumbnailWidth = 120;
inline constexpr int kUserMapThumbnailHeight = 56;

inline Color blendThumbnailColor(Color base, Color overlay, int overlayAlpha) {
    const int alpha = std::clamp(overlayAlpha, 0, 255);
    auto blend = [&](unsigned char a, unsigned char b) -> unsigned char {
        return static_cast<unsigned char>((static_cast<int>(a) * (255 - alpha) +
                                           static_cast<int>(b) * alpha) / 255);
    };
    return {
        blend(base.r, overlay.r),
        blend(base.g, overlay.g),
        blend(base.b, overlay.b),
        255,
    };
}

inline Color thumbnailTileColor(int tileId, int bgTileId, TileType collision) {
    Color color = {62, 126, 184, 255};
    if (bgTileId >= 0) {
        const unsigned int h = static_cast<unsigned int>(bgTileId * 1103515245u + 12345u);
        color = {
            static_cast<unsigned char>(50 + (h & 0x2f)),
            static_cast<unsigned char>(82 + ((h >> 5) & 0x4f)),
            static_cast<unsigned char>(110 + ((h >> 11) & 0x4f)),
            255,
        };
    }
    if (tileId >= 0) {
        const unsigned int h = static_cast<unsigned int>(tileId * 2654435761u);
        color = {
            static_cast<unsigned char>(84 + (h & 0x5f)),
            static_cast<unsigned char>(88 + ((h >> 7) & 0x5f)),
            static_cast<unsigned char>(96 + ((h >> 15) & 0x5f)),
            255,
        };
    }

    switch (collision) {
        case TileType::Solid:
            return blendThumbnailColor(color, {128, 160, 198, 255}, 82);
        case TileType::OneWay:
            return blendThumbnailColor(color, {228, 220, 84, 255}, 96);
        case TileType::Spike:
            return blendThumbnailColor(color, {240, 70, 74, 255}, 128);
        case TileType::Breakable:
            return blendThumbnailColor(color, {188, 126, 72, 255}, 112);
        case TileType::Ladder:
            return blendThumbnailColor(color, {66, 210, 156, 255}, 110);
        default:
            return color;
    }
}

inline bool writeUserMapThumbnail(const std::filesystem::path& mapPath,
                                  int stageWidth,
                                  int stageHeight,
                                  const std::vector<int>& layerData,
                                  const std::vector<int>& layerDataBG,
                                  const std::vector<TileType>& collision,
                                  const std::vector<EditorSpawn>& editorSpawns,
                                  int tileSize) {
    if (stageWidth <= 0 || stageHeight <= 0 || tileSize <= 0) return false;

    std::vector<Color> pixels(
        static_cast<size_t>(kUserMapThumbnailWidth) *
        static_cast<size_t>(kUserMapThumbnailHeight),
        Color{12, 16, 34, 255});

    auto setPixel = [&](int x, int y, Color color) {
        if (x < 0 || y < 0 ||
            x >= kUserMapThumbnailWidth || y >= kUserMapThumbnailHeight) {
            return;
        }
        pixels[static_cast<size_t>(y) * kUserMapThumbnailWidth +
               static_cast<size_t>(x)] = color;
    };

    const int tileCount = stageWidth * stageHeight;
    int minTileX = stageWidth;
    int minTileY = stageHeight;
    int maxTileX = -1;
    int maxTileY = -1;
    auto includeTile = [&](int tx, int ty) {
        if (tx < 0 || ty < 0 || tx >= stageWidth || ty >= stageHeight) return;
        minTileX = std::min(minTileX, tx);
        minTileY = std::min(minTileY, ty);
        maxTileX = std::max(maxTileX, tx);
        maxTileY = std::max(maxTileY, ty);
    };
    for (int ty = 0; ty < stageHeight; ++ty) {
        for (int tx = 0; tx < stageWidth; ++tx) {
            const int idx = ty * stageWidth + tx;
            const bool hasMainTile =
                idx >= 0 && idx < static_cast<int>(layerData.size()) && layerData[idx] >= 0;
            const bool hasBgTile =
                idx >= 0 && idx < static_cast<int>(layerDataBG.size()) && layerDataBG[idx] >= 0;
            const bool hasCollision =
                idx >= 0 && idx < tileCount && idx < static_cast<int>(collision.size()) &&
                collision[idx] != TileType::None;
            if (hasMainTile || hasBgTile || hasCollision) includeTile(tx, ty);
        }
    }
    for (const auto& spawn : editorSpawns) {
        includeTile(static_cast<int>(spawn.x) / tileSize,
                    static_cast<int>(spawn.y) / tileSize);
    }
    if (maxTileX < minTileX || maxTileY < minTileY) {
        minTileX = 0;
        minTileY = 0;
        maxTileX = stageWidth - 1;
        maxTileY = stageHeight - 1;
    } else {
        constexpr int kThumbnailMarginTiles = 2;
        minTileX = std::max(0, minTileX - kThumbnailMarginTiles);
        minTileY = std::max(0, minTileY - kThumbnailMarginTiles);
        maxTileX = std::min(stageWidth - 1, maxTileX + kThumbnailMarginTiles);
        maxTileY = std::min(stageHeight - 1, maxTileY + kThumbnailMarginTiles);
    }
    auto expandSpan = [](int& minValue, int& maxValue, int limit, int minSpan) {
        int span = maxValue - minValue + 1;
        while (span < minSpan && (minValue > 0 || maxValue < limit - 1)) {
            bool expanded = false;
            if (minValue > 0) {
                --minValue;
                expanded = true;
            }
            span = maxValue - minValue + 1;
            if (span >= minSpan) break;
            if (maxValue < limit - 1) {
                ++maxValue;
                expanded = true;
            }
            if (!expanded) break;
            span = maxValue - minValue + 1;
        }
    };
    expandSpan(minTileX, maxTileX, stageWidth, std::min(stageWidth, 16));
    expandSpan(minTileY, maxTileY, stageHeight, std::min(stageHeight, 10));
    const int sampleWidth = std::max(1, maxTileX - minTileX + 1);
    const int sampleHeight = std::max(1, maxTileY - minTileY + 1);

    for (int py = 0; py < kUserMapThumbnailHeight; ++py) {
        const int ty = minTileY + std::min(sampleHeight - 1,
                                           py * sampleHeight / kUserMapThumbnailHeight);
        for (int px = 0; px < kUserMapThumbnailWidth; ++px) {
            const int tx = minTileX + std::min(sampleWidth - 1,
                                               px * sampleWidth / kUserMapThumbnailWidth);
            const int idx = ty * stageWidth + tx;
            const int mainTile =
                (idx >= 0 && idx < static_cast<int>(layerData.size())) ? layerData[idx] : -1;
            const int bgTile =
                (idx >= 0 && idx < static_cast<int>(layerDataBG.size())) ? layerDataBG[idx] : -1;
            const TileType collisionType =
                (idx >= 0 && idx < tileCount && idx < static_cast<int>(collision.size()))
                    ? collision[idx]
                    : TileType::None;
            setPixel(px, py, thumbnailTileColor(mainTile, bgTile, collisionType));
        }
    }

    const float viewWorldX = static_cast<float>(minTileX * tileSize);
    const float viewWorldY = static_cast<float>(minTileY * tileSize);
    const float viewWorldW = std::max(1.0f, static_cast<float>(sampleWidth * tileSize));
    const float viewWorldH = std::max(1.0f, static_cast<float>(sampleHeight * tileSize));
    for (const auto& spawn : editorSpawns) {
        const int sx = std::clamp(
            static_cast<int>(std::round(
                (spawn.x - viewWorldX) / viewWorldW * (kUserMapThumbnailWidth - 1))),
            0, kUserMapThumbnailWidth - 1);
        const int sy = std::clamp(
            static_cast<int>(std::round(
                (spawn.y - viewWorldY) / viewWorldH * (kUserMapThumbnailHeight - 1))),
            0, kUserMapThumbnailHeight - 1);
        const Color color = editorSpawnMarkerColor(spawn.type);
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                setPixel(sx + dx, sy + dy, color);
            }
        }
    }

    ImageResource thumbnail;
    if (!thumbnail.loadFromPixels(
            pixels.data(), kUserMapThumbnailWidth, kUserMapThumbnailHeight)) {
        return false;
    }

    const std::filesystem::path thumbnailPath = userMapThumbnailPath(mapPath);
    return thumbnail.exportTo(thumbnailPath.string().c_str());
}

} // namespace mmx::map_editor
