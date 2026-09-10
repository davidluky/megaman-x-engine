// tile_attr_collision.h - shared decoded raw-attribute collision semantics.
// Owns the one mapping used by both stage loading and live block replacement.

#pragma once

#include "systems/tilemap.h"

#include <tuple>
#include <unordered_map>

namespace mmx::tile_attr_collision {

struct Expectation {
    TileType type = TileType::None;
    bool hasSlope = false;
    SlopeHeight slope{};
};

inline const std::unordered_map<int, std::tuple<TileType, uint8_t, uint8_t>>&
slopeMap() {
    static const std::unordered_map<int, std::tuple<TileType, uint8_t, uint8_t>> slopes = {
        // U61 low family (the common encoding) + 1:2 pairs.
        {0x01, {TileType::SlopeR, 16, 8}},  {0x02, {TileType::SlopeR, 8, 0}},
        {0x04, {TileType::SlopeL, 0, 8}},   {0x03, {TileType::SlopeL, 8, 16}},
        {0x05, {TileType::SlopeR, 16, 12}}, {0x06, {TileType::SlopeR, 12, 8}},
        {0x07, {TileType::SlopeR, 8, 4}},   {0x08, {TileType::SlopeR, 4, 0}},
        {0x09, {TileType::SlopeL, 12, 16}}, {0x0A, {TileType::SlopeL, 8, 12}},
        {0x0B, {TileType::SlopeL, 4, 8}},   {0x0C, {TileType::SlopeL, 0, 4}},
        // Flame Mammoth high copy.
        {0x45, {TileType::SlopeR, 16, 12}}, {0x46, {TileType::SlopeR, 12, 8}},
        {0x47, {TileType::SlopeR, 8, 4}},   {0x48, {TileType::SlopeR, 4, 0}},
        {0x4C, {TileType::SlopeL, 0, 4}},   {0x4B, {TileType::SlopeL, 4, 8}},
        {0x4A, {TileType::SlopeL, 8, 12}},  {0x49, {TileType::SlopeL, 12, 16}},
    };
    return slopes;
}

inline Expectation derive(
    int attr,
    const std::unordered_map<int, TileType>& attrOverrides,
    bool attrOverridesFirst) {
    Expectation out;
    if (attr == 0) return out;

    const auto overrideIt = attrOverrides.find(attr);
    if (attrOverridesFirst && overrideIt != attrOverrides.end()) {
        out.type = overrideIt->second;
        return out;
    }

    const auto slopeIt = slopeMap().find(attr);
    if (slopeIt != slopeMap().end()) {
        const auto [type, leftY, rightY] = slopeIt->second;
        out.type = type;
        out.hasSlope = true;
        out.slope = {leftY, rightY};
        return out;
    }

    out.type = (overrideIt != attrOverrides.end())
                   ? overrideIt->second
                   : (attr == 0x10 ? TileType::None : TileType::Solid);
    return out;
}

} // namespace mmx::tile_attr_collision
