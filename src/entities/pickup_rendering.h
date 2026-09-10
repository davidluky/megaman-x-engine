// pickup_rendering.h - source-backed atlas facts shared by pickup rendering.
//
// Placement is not part of this header. GC8.1's approved Storm Eagle centre
// anchor is bridged separately by storm_eagle_heart_tank_placement.h.

#pragma once

#include "raylib.h"

#include <array>
#include <cstddef>

namespace mmx::pickup_rendering {

inline constexpr const char* kItemsAtlasPath =
    "content/x1/sprites/misc/mmx1_items.png";

// The four 16x16 cells in the bottom row are the heart-tank animation. The
// visible pink silhouette is centred inside each cell by the source atlas.
inline constexpr std::array<Rectangle, 4> kHeartTankFrames = {{
    Rectangle{8.0f, 81.0f, 16.0f, 16.0f},
    Rectangle{24.0f, 81.0f, 16.0f, 16.0f},
    Rectangle{40.0f, 81.0f, 16.0f, 16.0f},
    Rectangle{56.0f, 81.0f, 16.0f, 16.0f},
}};

inline constexpr int kHeartTankTicksPerFrame = 8;
inline constexpr int kHeartTankAnimationPeriod =
    static_cast<int>(kHeartTankFrames.size()) * kHeartTankTicksPerFrame;

inline constexpr std::size_t heartTankFrameAtTick(int tick) {
    if (tick < 0) tick = 0;
    const int normalized = tick % kHeartTankAnimationPeriod;
    return static_cast<std::size_t>(normalized / kHeartTankTicksPerFrame);
}

} // namespace mmx::pickup_rendering
