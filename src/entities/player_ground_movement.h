#pragma once

#include "systems/tilemap.h"

#include <cmath>
#include <cstdint>
#include <optional>

namespace mmx::player_ground_movement {

// Called only by canonical CP's guarded sourceGroundSpeed owner. R175's
// secondary $849835 lookup keeps raw5 cached at its inclusive bottom edge.
inline int supportAttribute(const Tilemap& tilemap, float feetX, float feetY) {
    // $8490B3 looks up X before INC derives the local quarter edge.
    const float lookupX = feetX - 1.0f;
    const int rawAttr = tilemap.getRawAttrAtPixel(lookupX, feetY);
    if (tilemap.tileSize() != 16) return rawAttr;

    const int row = static_cast<int>(std::floor(feetY / 16.0f));
    if (feetY != static_cast<float>(row * 16)) return rawAttr;
    const int col = static_cast<int>(std::floor(lookupX / 16.0f));
    const auto slope = tilemap.getSlope(col, row - 1);
    if (tilemap.getRawAttr(col, row - 1) == 0x05 &&
        tilemap.getTileType(col, row - 1) == TileType::SlopeR &&
        slope.leftY == 16 && slope.rightY == 12) {
        return 0x05;
    }
    return rawAttr;
}

// ROM $849AAB/$849ACD and $86BA65/$86BB9A. This is a source-static
// translation; CP ascent, descent and restart controls verify observed groups,
// while remaining raw-byte groups preserve the ROM flags. Provenance lives in
// knowledge_base/mmx1/player/chill_penguin_ground_contact.json.
inline int terrainGroup(int rawAttr) {
    const auto value = static_cast<std::uint8_t>(rawAttr);
    if (value == 0) return 4;

    // CPX followed by BPL tests the 8-bit N flag of the subtraction result.
    // Do not turn high raw bytes into an unsigned >=13 bucket.
    const auto bplAfterCmp = [value](std::uint8_t immediate) {
        return static_cast<std::uint8_t>(value - immediate) < 0x80;
    };
    int group = 0;
    if (!bplAfterCmp(0x03)) return group;
    ++group;
    if (!bplAfterCmp(0x05)) return group;
    ++group;
    if (!bplAfterCmp(0x09)) return group;
    ++group;
    if (!bplAfterCmp(0x0D)) return group;
    return group + 1;
}

inline std::optional<float> speedMagnitudeForSupport(int rawAttr,
                                               float currentVelocityX) {
    // R129: selector direction comes from the pre-assignment velocity word,
    // not from the currently held input direction. The caller supplies action
    // speed for a moving restart; this helper does not model the zero branch.
    if (currentVelocityX == 0.0f) return std::nullopt;

    constexpr int kSpeedQ8_8[] = {456, 376, 408, 376, 376, 376};
    constexpr int kPositiveRemap[] = {1, 0, 3, 2, 4};
    const int group = terrainGroup(rawAttr);
    const int selector = currentVelocityX < 0.0f
        ? (group == 4 ? 5 : group)
        : kPositiveRemap[group];
    return static_cast<float>(kSpeedQ8_8[selector]) / 256.0f;
}

} // namespace mmx::player_ground_movement
