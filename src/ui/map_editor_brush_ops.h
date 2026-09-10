// map_editor_brush_ops.h - tile/collision brush mutation helpers.

#pragma once

#include "systems/tilemap.h"
#include "ui/map_editor_fill.h"

#include <unordered_map>
#include <utility>
#include <vector>

namespace mmx::map_editor {

struct BrushEditState {
    int stageWidth = 0;
    int stageHeight = 0;
    int activeLayer = 0;
    int selectedTile = 0;
    bool editCollision = false;
    TileType collisionBrush = TileType::Solid;
    std::vector<int>* layerData = nullptr;
    std::vector<int>* layerDataBg = nullptr;
    bool* layerDataMainEditable = nullptr;
    bool* layerDataBgEditable = nullptr;
    std::vector<TileType>* collision = nullptr;
    const std::vector<int>* editorAttrs = nullptr;
    const std::unordered_map<int, TileType>* editorAttrOverrides = nullptr;
    std::unordered_map<int, std::pair<int, int>>* slopeEdits = nullptr;
};

struct BrushSlopeAttr {
    TileType type = TileType::None;
    int left = 0;
    int right = 0;
};

struct BrushFillResult {
    bool inBounds = false;
    bool noChange = false;
    bool changed = false;
    int tileCount = 0;
};

inline bool brushTileInBounds(const BrushEditState& state, int tx, int ty) {
    return tx >= 0 && tx < state.stageWidth && ty >= 0 && ty < state.stageHeight;
}

inline int brushTileIndex(const BrushEditState& state, int tx, int ty) {
    return ty * state.stageWidth + tx;
}

inline std::vector<int>& activeBrushLayer(BrushEditState& state) {
    return (state.activeLayer == 0) ? *state.layerData : *state.layerDataBg;
}

inline const std::vector<int>& activeBrushLayer(const BrushEditState& state) {
    return (state.activeLayer == 0) ? *state.layerData : *state.layerDataBg;
}

inline bool& activeBrushLayerEditable(BrushEditState& state) {
    return (state.activeLayer == 0) ? *state.layerDataMainEditable
                                    : *state.layerDataBgEditable;
}

inline bool brushSlopeForAttr(int attr, BrushSlopeAttr& out) {
    switch (attr) {
        case 0x01: out = {TileType::SlopeR, 16, 8}; return true;
        case 0x02: out = {TileType::SlopeR, 8, 0}; return true;
        case 0x04: out = {TileType::SlopeL, 0, 8}; return true;
        case 0x03: out = {TileType::SlopeL, 8, 16}; return true;
        case 0x05: out = {TileType::SlopeR, 16, 12}; return true;
        case 0x06: out = {TileType::SlopeR, 12, 8}; return true;
        case 0x07: out = {TileType::SlopeR, 8, 4}; return true;
        case 0x08: out = {TileType::SlopeR, 4, 0}; return true;
        case 0x09: out = {TileType::SlopeL, 12, 16}; return true;
        case 0x0A: out = {TileType::SlopeL, 8, 12}; return true;
        case 0x0B: out = {TileType::SlopeL, 4, 8}; return true;
        case 0x0C: out = {TileType::SlopeL, 0, 4}; return true;
        case 0x45: out = {TileType::SlopeR, 16, 12}; return true;
        case 0x46: out = {TileType::SlopeR, 12, 8}; return true;
        case 0x47: out = {TileType::SlopeR, 8, 4}; return true;
        case 0x48: out = {TileType::SlopeR, 4, 0}; return true;
        case 0x4C: out = {TileType::SlopeL, 0, 4}; return true;
        case 0x4B: out = {TileType::SlopeL, 4, 8}; return true;
        case 0x4A: out = {TileType::SlopeL, 8, 12}; return true;
        case 0x49: out = {TileType::SlopeL, 12, 16}; return true;
        default: return false;
    }
}

inline bool paintTileNoHistory(BrushEditState& state, int tx, int ty) {
    if (!brushTileInBounds(state, tx, ty)) return false;
    const int idx = brushTileIndex(state, tx, ty);

    if (state.editCollision) {
        if (!state.collision || idx < 0 ||
            idx >= static_cast<int>(state.collision->size())) {
            return false;
        }
        const TileType oldCollision = (*state.collision)[idx];
        (*state.collision)[idx] = state.collisionBrush;
        return (*state.collision)[idx] != oldCollision;
    }

    auto& layer = activeBrushLayer(state);
    if (idx < 0 || idx >= static_cast<int>(layer.size())) return false;

    const int oldTile = layer[idx];
    const bool oldEditable = activeBrushLayerEditable(state);
    const TileType oldCollision =
        (state.collision && idx < static_cast<int>(state.collision->size()))
            ? (*state.collision)[idx]
            : TileType::None;
    const auto oldSlopeIt = state.slopeEdits ? state.slopeEdits->find(idx)
                                             : decltype(state.slopeEdits->end()){};
    const bool hadOldSlope = state.slopeEdits && oldSlopeIt != state.slopeEdits->end();
    const std::pair<int, int> oldSlope =
        hadOldSlope ? oldSlopeIt->second : std::pair<int, int>{0, 0};

    activeBrushLayerEditable(state) = true;
    layer[idx] = state.selectedTile;

    if (state.activeLayer == 0 && state.editorAttrs && !state.editorAttrs->empty() &&
        state.selectedTile >= 0 &&
        state.selectedTile < static_cast<int>(state.editorAttrs->size()) &&
        state.collision && idx < static_cast<int>(state.collision->size()) &&
        state.slopeEdits) {
        const int attr = (*state.editorAttrs)[state.selectedTile];
        state.slopeEdits->erase(idx);
        const TileType* overrideType = nullptr;
        if (state.editorAttrOverrides) {
            const auto override = state.editorAttrOverrides->find(attr);
            if (override != state.editorAttrOverrides->end()) {
                overrideType = &override->second;
            }
        }
        BrushSlopeAttr slope;
        if (attr == 0) {
            (*state.collision)[idx] = TileType::None;
        } else if (overrideType) {
            (*state.collision)[idx] = *overrideType;
        } else if (brushSlopeForAttr(attr, slope)) {
            (*state.collision)[idx] = slope.type;
            (*state.slopeEdits)[idx] = {slope.left, slope.right};
        } else if (attr == 0x0D || attr == 0x0E || attr == 0x10) {
            (*state.collision)[idx] = TileType::None;
        } else if (attr == 0x37 || attr == 0x38) {
            (*state.collision)[idx] = TileType::Conveyor;
        } else {
            (*state.collision)[idx] = TileType::Solid;
        }
    }

    const bool newEditable = activeBrushLayerEditable(state);
    const auto newSlopeIt = state.slopeEdits ? state.slopeEdits->find(idx)
                                             : decltype(state.slopeEdits->end()){};
    const bool hasNewSlope = state.slopeEdits && newSlopeIt != state.slopeEdits->end();
    const std::pair<int, int> newSlope =
        hasNewSlope ? newSlopeIt->second : std::pair<int, int>{0, 0};
    const bool collisionChanged =
        state.collision && idx < static_cast<int>(state.collision->size()) &&
        (*state.collision)[idx] != oldCollision;
    return layer[idx] != oldTile ||
           newEditable != oldEditable ||
           collisionChanged ||
           hasNewSlope != hadOldSlope ||
           (hasNewSlope && newSlope != oldSlope);
}

inline bool eraseTileNoHistory(BrushEditState& state, int tx, int ty) {
    if (!brushTileInBounds(state, tx, ty)) return false;
    const int idx = brushTileIndex(state, tx, ty);

    if (state.editCollision) {
        if (!state.collision || idx < 0 ||
            idx >= static_cast<int>(state.collision->size())) {
            return false;
        }
        const TileType oldCollision = (*state.collision)[idx];
        const bool hadSlope =
            state.slopeEdits && state.slopeEdits->find(idx) != state.slopeEdits->end();
        (*state.collision)[idx] = TileType::None;
        if (state.slopeEdits) state.slopeEdits->erase(idx);
        return oldCollision != (*state.collision)[idx] || hadSlope;
    }

    auto& layer = activeBrushLayer(state);
    if (idx < 0 || idx >= static_cast<int>(layer.size())) return false;
    const int oldTile = layer[idx];
    const bool oldEditable = activeBrushLayerEditable(state);
    activeBrushLayerEditable(state) = true;
    layer[idx] = -1;
    return layer[idx] != oldTile || activeBrushLayerEditable(state) != oldEditable;
}

inline BrushFillResult fillTileNoHistory(BrushEditState& state, int tx, int ty) {
    BrushFillResult result;
    if (!brushTileInBounds(state, tx, ty)) return result;
    const int startIdx = brushTileIndex(state, tx, ty);

    if (state.editCollision) {
        if (!state.collision || startIdx < 0 ||
            startIdx >= static_cast<int>(state.collision->size())) {
            return result;
        }
        result.inBounds = true;
        if ((*state.collision)[startIdx] == state.collisionBrush) {
            result.noChange = true;
            return result;
        }
        const auto region = collectConnectedRegion(
            *state.collision, state.stageWidth, state.stageHeight, startIdx);
        result.tileCount = static_cast<int>(region.size());
        for (const int idx : region) {
            result.changed = paintTileNoHistory(
                state, idx % state.stageWidth, idx / state.stageWidth) ||
                result.changed;
        }
        return result;
    }

    auto& layer = activeBrushLayer(state);
    if (startIdx < 0 || startIdx >= static_cast<int>(layer.size())) return result;
    result.inBounds = true;
    if (layer[startIdx] == state.selectedTile) {
        result.noChange = true;
        return result;
    }

    const auto region = collectConnectedRegion(
        layer, state.stageWidth, state.stageHeight, startIdx);
    result.tileCount = static_cast<int>(region.size());
    for (const int idx : region) {
        result.changed = paintTileNoHistory(
            state, idx % state.stageWidth, idx / state.stageWidth) ||
            result.changed;
    }
    return result;
}

inline TileType nextCollisionBrush(TileType brush) {
    return static_cast<TileType>((static_cast<int>(brush) + 1) % 8);
}

inline int pickTileId(const BrushEditState& state, int tx, int ty) {
    if (!brushTileInBounds(state, tx, ty)) return -1;
    const int idx = brushTileIndex(state, tx, ty);
    const auto& layer = activeBrushLayer(state);
    if (idx < 0 || idx >= static_cast<int>(layer.size())) return -1;
    return layer[idx];
}

} // namespace mmx::map_editor
