// projectile_collision.cpp - computes projectile terrain probes and wall snaps.
// Owns: projectile-specific tile ranges for terrain, support, and visuals.

#include "physics/projectile_collision.h"

#include <cmath>

namespace mmx::physics {

ProjectileTileRange projectileTerrainTileRange(
    const AABB& box, int tileSize, float vx, float vy, bool fullBody) {
    ProjectileTileRange range{
        static_cast<int>(std::floor(box.left() / tileSize)),
        static_cast<int>(std::floor(box.top() / tileSize)),
        static_cast<int>(std::floor((box.right() - 0.01f) / tileSize)),
        static_cast<int>(std::floor((box.bottom() - 0.01f) / tileSize)),
    };

    if (fullBody || (vx == 0.0f && vy == 0.0f)) {
        return range;
    }

    if (std::fabs(vx) >= std::fabs(vy)) {
        const float centerY = box.top() + box.h * 0.5f;
        range.startRow = static_cast<int>(std::floor(centerY / tileSize));
        range.endRow = range.startRow;
    } else {
        const float centerX = box.left() + box.w * 0.5f;
        range.startCol = static_cast<int>(std::floor(centerX / tileSize));
        range.endCol = range.startCol;
    }
    return range;
}

ProjectileTileRange projectileGroundSupportTileRange(
    const AABB& box, int tileSize, float probeDepthPx) {
    const int startRow = static_cast<int>(
        std::floor((box.bottom() + 1.0f) / tileSize));
    const int endRow = static_cast<int>(
        std::floor((box.bottom() + probeDepthPx) / tileSize));
    return {
        static_cast<int>(std::floor(box.left() / tileSize)),
        startRow,
        static_cast<int>(std::floor((box.right() - 0.01f) / tileSize)),
        endRow,
    };
}

ProjectileTileRange rollingShieldVisualWallTileRange(
    const AABB& box, int tileSize, float vx, float visualOffsetX,
    float visualFrameWidth, float visualScale) {
    ProjectileTileRange range{
        static_cast<int>(std::floor(box.left() / tileSize)),
        static_cast<int>(std::floor(box.top() / tileSize)),
        static_cast<int>(std::floor((box.right() - 0.01f) / tileSize)),
        static_cast<int>(std::floor((box.bottom() - 0.01f) / tileSize)),
    };

    if (vx == 0.0f) {
        return range;
    }

    const float visualHalfW = visualFrameWidth * visualScale * 0.5f;
    const float centerX = box.left() + box.w * 0.5f;
    const float leadX = vx > 0.0f
        ? centerX + visualOffsetX + visualHalfW
        : centerX - visualOffsetX - visualHalfW;
    const int leadCol = vx > 0.0f
        ? static_cast<int>(std::floor(leadX / tileSize))
        : static_cast<int>(std::floor((leadX - 0.01f) / tileSize));
    if (vx > 0.0f) {
        range.startCol = range.endCol;
        range.endCol = leadCol;
    } else {
        range.endCol = range.startCol;
        range.startCol = leadCol;
    }
    return range;
}

float rollingShieldWallSnapX(
    float previousX, float vx, float hitboxOffsetX, float hitboxWidth,
    float visualOffsetX, float visualFrameWidth, float visualScale) {
    (void)hitboxOffsetX;
    (void)hitboxWidth;
    (void)visualOffsetX;
    (void)visualFrameWidth;
    (void)visualScale;
    return previousX - vx;
}

} // namespace mmx::physics
