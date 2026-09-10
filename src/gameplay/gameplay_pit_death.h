// gameplay_pit_death.h - data-backed pit death threshold helpers.

#pragma once

#include "systems/tilemap.h"

namespace mmx::gameplay_pit_death {

inline float thresholdY(const Tilemap& tilemap) {
    if (auto stageThreshold = tilemap.pitDeathY()) {
        return *stageThreshold;
    }
    return static_cast<float>(tilemap.pixelHeight() + 32);
}

inline bool shouldKillAtY(float playerY, const Tilemap& tilemap) {
    return playerY > thresholdY(tilemap);
}

} // namespace mmx::gameplay_pit_death
