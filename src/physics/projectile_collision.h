// projectile_collision.h - declares projectile terrain range and snap helpers.
// Boundary: returns geometry probes; projectile behavior stays in gameplay.

#pragma once

#include "entities/entity.h"

namespace mmx::physics {

struct ProjectileTileRange {
    int startCol = 0;
    int startRow = 0;
    int endCol = 0;
    int endRow = 0;
};

ProjectileTileRange projectileTerrainTileRange(
    const AABB& box, int tileSize, float vx, float vy, bool fullBody);

ProjectileTileRange projectileGroundSupportTileRange(
    const AABB& box, int tileSize, float probeDepthPx = 1.0f);

ProjectileTileRange rollingShieldVisualWallTileRange(
    const AABB& box, int tileSize, float vx, float visualOffsetX,
    float visualFrameWidth, float visualScale);

float rollingShieldWallSnapX(
    float previousX, float vx, float hitboxOffsetX, float hitboxWidth,
    float visualOffsetX, float visualFrameWidth, float visualScale);

} // namespace mmx::physics
