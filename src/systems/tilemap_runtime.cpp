// tilemap_runtime.cpp - guarded live mutations of decoded stage terrain.
// Owns runtime-only tile/collision transactions; loading remains in its module.

#include "systems/tilemap.h"
#include "systems/tile_attr_collision.h"

#include <limits>

namespace mmx {

int Tilemap::mainTileId(int tileX, int tileY) const {
    if (tileX < 0 || tileX >= width_ || tileY < 0 || tileY >= height_) {
        return -1;
    }
    const TileLayer* main = nullptr;
    for (const auto& layer : layers_) {
        if (layer.name != "main") continue;
        if (main != nullptr) return -1;
        main = &layer;
    }
    if (main == nullptr) return -1;
    const size_t index = static_cast<size_t>(tileY) * static_cast<size_t>(width_) +
                         static_cast<size_t>(tileX);
    if (index >= main->data.size()) return -1;
    return main->data[index];
}

bool Tilemap::replaceDirectMainTile(int tileX, int tileY,
                                    int expectedBlockId, int replacementBlockId,
                                    TileType replacementCollision) {
    if (tileX < 0 || tileX >= width_ || tileY < 0 || tileY >= height_) {
        return false;
    }

    TileLayer* main = nullptr;
    for (auto& layer : layers_) {
        if (layer.name != "main") continue;
        if (main != nullptr) return false;
        main = &layer;
    }
    if (main == nullptr) return false;

    const size_t index = static_cast<size_t>(tileY) * static_cast<size_t>(width_) +
                         static_cast<size_t>(tileX);
    if (index >= main->data.size() || index >= collision_.size()) return false;
    if (main->data[index] != expectedBlockId) return false;

    main->data[index] = replacementBlockId;
    collision_[index] = replacementCollision;
    slopeData_.erase(static_cast<int>(index));
    main->previewTex.reset();
    return true;
}

bool Tilemap::replaceDecodedMainBlock(int tileX, int tileY,
                                      int expectedBlockId, int replacementBlockId) {
    return replaceDecodedMainBlocks({{tileX, tileY, expectedBlockId, replacementBlockId}});
}

bool Tilemap::replaceDecodedMainBlocks(
    const std::vector<DecodedMainBlockReplacement>& replacements) {
    if (replacements.empty() || width_ <= 0 || height_ <= 0) {
        return false;
    }

    TileLayer* main = nullptr;
    for (auto& layer : layers_) {
        if (layer.name != "main") continue;
        if (main != nullptr) return false;
        main = &layer;
    }
    if (main == nullptr) return false;

    const size_t tileCount = static_cast<size_t>(width_) *
                             static_cast<size_t>(height_);
    if (tileCount > static_cast<size_t>(std::numeric_limits<int>::max()) ||
        replacements.size() > tileCount || main->data.size() != tileCount ||
        collision_.size() != tileCount) {
        return false;
    }
    struct PreparedReplacement {
        int index;
        int blockId;
        tile_attr_collision::Expectation collision;
    };
    std::vector<PreparedReplacement> prepared;
    prepared.reserve(replacements.size());
    const auto isUnshapedSlope = [](const tile_attr_collision::Expectation& value) {
        return isSlope(value.type) && !value.hasSlope;
    };
    for (const auto& edit : replacements) {
        if (edit.tileX < 0 || edit.tileX >= width_ ||
            edit.tileY < 0 || edit.tileY >= height_ ||
            edit.expectedBlockId < 0 ||
            static_cast<size_t>(edit.expectedBlockId) >= attrs_.size() ||
            edit.replacementBlockId < 0 ||
            static_cast<size_t>(edit.replacementBlockId) >= attrs_.size()) {
            return false;
        }
        const int index = edit.tileY * width_ + edit.tileX;
        if (main->data[index] != edit.expectedBlockId) return false;
        for (const auto& prior : prepared) {
            if (prior.index == index) return false;
        }

        // Match loading precedence for both baked and derived collision stages.
        auto current = tile_attr_collision::derive(
            attrs_[edit.expectedBlockId], attrOverrides_, attrOverridesFirst_);
        const auto replacement = tile_attr_collision::derive(
            attrs_[edit.replacementBlockId], attrOverrides_, attrOverridesFirst_);
        if (edit.expectedBakedCollision.has_value()) {
            // The loader leaves attr0's baked collision unconstrained. Require
            // an explicit expected type for that case, never a global bypass.
            if (attrs_[edit.expectedBlockId] != 0) return false;
            current.type = *edit.expectedBakedCollision;
        }
        if (isUnshapedSlope(current) || isUnshapedSlope(replacement) ||
            collision_[index] != current.type) {
            return false;
        }
        const auto currentSlope = slopeData_.find(index);
        if ((current.hasSlope &&
             (currentSlope == slopeData_.end() ||
              currentSlope->second.leftY != current.slope.leftY ||
              currentSlope->second.rightY != current.slope.rightY)) ||
            (!current.hasSlope && currentSlope != slopeData_.end())) {
            return false;
        }
        prepared.push_back({index, edit.replacementBlockId, replacement});
    }

    // Stage allocating slope operations before touching live data. R365's six
    // gate cells must commit together even when a later cell fails preflight.
    auto nextSlopes = slopeData_;
    for (const auto& edit : prepared) {
        nextSlopes.erase(edit.index);
        if (edit.collision.hasSlope) nextSlopes[edit.index] = edit.collision.slope;
    }
    for (const auto& edit : prepared) {
        main->data[edit.index] = edit.blockId;
        collision_[edit.index] = edit.collision.type;
    }
    slopeData_.swap(nextSlopes);
    main->previewTex.reset();
    return true;
}

} // namespace mmx
