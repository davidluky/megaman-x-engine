// map_editor_spawn_ops.h - spawn and resize mutation helpers.

#pragma once

#include "systems/tilemap.h"
#include "ui/map_editor_spawns.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mmx::map_editor {

struct StageResizeState {
    int* stageWidth = nullptr;
    int* stageHeight = nullptr;
    std::vector<int>* layerData = nullptr;
    std::vector<int>* layerDataBg = nullptr;
    bool* layerDataMainEditable = nullptr;
    bool* layerDataBgEditable = nullptr;
    std::vector<TileType>* collision = nullptr;
    std::unordered_map<int, std::pair<int, int>>* slopeEdits = nullptr;
};

struct ClampedSpawnPosition {
    float x = 0.0f;
    float y = 0.0f;
};

struct SpawnCatalogSelectionResult {
    bool hasItems = false;
    int selected = 0;
    std::string id;
};

enum class SpawnPlacementOutcome {
    StageInvalid,
    MissingId,
    Changed,
    Unchanged,
};

struct SpawnPlacementResult {
    SpawnPlacementOutcome outcome = SpawnPlacementOutcome::Unchanged;
    std::string id;
};

struct SpawnActionResult {
    bool handled = true;
    bool changed = false;
    std::string status;
};

inline int positiveWrappedIndex(int value, int count) {
    if (count <= 0) return 0;
    int result = value % count;
    return result < 0 ? result + count : result;
}

inline std::string selectedSpawnCatalogId(const std::vector<std::string>& ids,
                                          int selected) {
    if (ids.empty()) return {};
    const int index =
        std::clamp(selected, 0, static_cast<int>(ids.size()) - 1);
    return ids[index];
}

inline SpawnCatalogSelectionResult cycleSpawnCatalogSelection(
    const std::vector<std::string>& ids,
    int selected,
    int direction) {
    SpawnCatalogSelectionResult result;
    if (ids.empty()) return result;

    result.hasItems = true;
    result.selected =
        positiveWrappedIndex(selected + direction, static_cast<int>(ids.size()));
    result.id = ids[result.selected];
    return result;
}

inline std::string prefixedSpawnStatus(const std::string& prefix,
                                       const std::string& id) {
    if (id.empty()) return prefix;
    if (prefix.empty()) return id;
    return prefix + " " + id;
}

inline std::string selectSpawnCatalogWithStatus(
    const std::vector<std::string>& ids,
    int& selected,
    int direction,
    const std::string& noItemsStatus,
    const std::string& selectedStatusPrefix) {
    const SpawnCatalogSelectionResult result =
        cycleSpawnCatalogSelection(ids, selected, direction);
    selected = result.selected;
    if (!result.hasItems) return noItemsStatus;
    return prefixedSpawnStatus(selectedStatusPrefix, result.id);
}

inline ClampedSpawnPosition clampSpawnPosition(float worldX,
                                               float worldY,
                                               int stageWidth,
                                               int stageHeight,
                                               int tileSize) {
    const float maxWorldX =
        std::max(0.0f, static_cast<float>(stageWidth * tileSize - 1));
    const float maxWorldY =
        std::max(0.0f, static_cast<float>(stageHeight * tileSize - 1));
    return {
        std::clamp(worldX, 0.0f, maxWorldX),
        std::clamp(worldY, 0.0f, maxWorldY),
    };
}

inline int nearestSpawnIndexByType(const std::vector<EditorSpawn>& spawns,
                                   const std::string& type,
                                   float worldX,
                                   float worldY,
                                   float maxDistance) {
    if (maxDistance < 0.0f) return -1;

    const float maxDistanceSq = maxDistance * maxDistance;
    float bestDistanceSq = maxDistanceSq;
    int bestIndex = -1;

    for (int i = 0; i < static_cast<int>(spawns.size()); ++i) {
        const auto& spawn = spawns[i];
        if (spawn.type != type) continue;

        const float dx = spawn.x - worldX;
        const float dy = spawn.y - worldY;
        const float distanceSq = dx * dx + dy * dy;
        if (distanceSq <= bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestIndex = i;
        }
    }

    return bestIndex;
}

inline bool setPlayerSpawn(std::vector<EditorSpawn>& spawns,
                           float worldX,
                           float worldY,
                           int stageWidth,
                           int stageHeight,
                           int tileSize) {
    const auto position =
        clampSpawnPosition(worldX, worldY, stageWidth, stageHeight, tileSize);

    bool updated = false;
    std::vector<EditorSpawn> out;
    out.reserve(spawns.size() + 1);
    for (auto spawn : spawns) {
        if (spawn.type == "player_spawn") {
            if (updated) continue;
            spawn.x = position.x;
            spawn.y = position.y;
            updated = true;
        }
        out.push_back(std::move(spawn));
    }

    if (!updated) {
        EditorSpawn spawn;
        spawn.type = "player_spawn";
        spawn.x = position.x;
        spawn.y = position.y;
        spawn.raw = nlohmann::json::object({{"type", "player_spawn"}});
        out.insert(out.begin(), std::move(spawn));
    }

    const bool changed = out != spawns;
    spawns = std::move(out);
    return changed;
}

inline bool deleteSpawnsByType(std::vector<EditorSpawn>& spawns,
                               const std::string& type) {
    const auto oldSize = spawns.size();
    spawns.erase(
        std::remove_if(spawns.begin(), spawns.end(),
                       [&](const EditorSpawn& spawn) {
                           return spawn.type == type;
                       }),
        spawns.end());
    return spawns.size() != oldSize;
}

inline EditorSpawn makePlacedSpawn(const std::string& type,
                                   const std::string& id,
                                   bool writesId,
                                   float x,
                                   float y) {
    EditorSpawn spawn;
    spawn.type = type;
    spawn.id = writesId ? id : std::string();
    spawn.x = x;
    spawn.y = y;
    spawn.raw = writesId
        ? nlohmann::json::object({{"type", type}, {"id", id}})
        : nlohmann::json::object({{"type", type}});
    return spawn;
}

inline bool upsertSpawnNear(std::vector<EditorSpawn>& spawns,
                            const std::string& type,
                            const std::string& id,
                            bool writesId,
                            float worldX,
                            float worldY,
                            int stageWidth,
                            int stageHeight,
                            int tileSize,
                            float replaceDistance) {
    const auto position =
        clampSpawnPosition(worldX, worldY, stageWidth, stageHeight, tileSize);
    const int existingIndex =
        nearestSpawnIndexByType(spawns, type, worldX, worldY, replaceDistance);

    if (existingIndex >= 0) {
        auto& spawn = spawns[existingIndex];
        const EditorSpawn before = spawn;
        spawn.type = type;
        if (writesId) spawn.id = id;
        spawn.x = position.x;
        spawn.y = position.y;
        if (!spawn.raw.is_object()) {
            spawn.raw = nlohmann::json::object();
        }
        return !(spawn == before);
    }

    spawns.push_back(makePlacedSpawn(type, id, writesId, position.x, position.y));
    return true;
}

inline SpawnPlacementResult placeSpawnWithResult(std::vector<EditorSpawn>& spawns,
                                                 const std::string& type,
                                                 const std::string& id,
                                                 bool writesId,
                                                 bool requiresId,
                                                 float worldX,
                                                 float worldY,
                                                 int stageWidth,
                                                 int stageHeight,
                                                 int tileSize,
                                                 float replaceDistance) {
    SpawnPlacementResult result;
    result.id = id;
    if (stageWidth <= 0 || stageHeight <= 0) {
        result.outcome = SpawnPlacementOutcome::StageInvalid;
        return result;
    }
    if (requiresId && id.empty()) {
        result.outcome = SpawnPlacementOutcome::MissingId;
        return result;
    }
    result.outcome = upsertSpawnNear(spawns, type, id, writesId,
                                     worldX, worldY, stageWidth, stageHeight,
                                     tileSize, replaceDistance)
        ? SpawnPlacementOutcome::Changed
        : SpawnPlacementOutcome::Unchanged;
    return result;
}

inline bool deleteSpawnNear(std::vector<EditorSpawn>& spawns,
                            const std::string& type,
                            float worldX,
                            float worldY,
                            float maxDistance) {
    const int index = nearestSpawnIndexByType(spawns, type, worldX, worldY, maxDistance);
    if (index < 0) return false;
    spawns.erase(spawns.begin() + index);
    return true;
}

inline SpawnActionResult placePlayerSpawnWithStatus(
    std::vector<EditorSpawn>& spawns,
    float worldX,
    float worldY,
    int stageWidth,
    int stageHeight,
    int tileSize,
    const std::string& changedStatus,
    const std::string& unchangedStatus) {
    if (stageWidth <= 0 || stageHeight <= 0) return {false, false, {}};
    const bool changed =
        setPlayerSpawn(spawns, worldX, worldY, stageWidth, stageHeight, tileSize);
    return {true, changed, changed ? changedStatus : unchangedStatus};
}

inline SpawnActionResult deleteSpawnsByTypeWithStatus(
    std::vector<EditorSpawn>& spawns,
    const std::string& type,
    const std::string& deletedStatus,
    const std::string& missingStatus) {
    const bool deleted = deleteSpawnsByType(spawns, type);
    return {true, deleted, deleted ? deletedStatus : missingStatus};
}

inline SpawnActionResult placeCatalogSpawnWithStatus(
    std::vector<EditorSpawn>& spawns,
    const std::string& type,
    const std::string& id,
    float worldX,
    float worldY,
    int stageWidth,
    int stageHeight,
    int tileSize,
    float replaceDistance,
    const std::string& missingStatus,
    const std::string& changedStatusPrefix,
    const std::string& unchangedStatus) {
    const SpawnPlacementResult result =
        placeSpawnWithResult(spawns, type, id, true, true,
                             worldX, worldY, stageWidth, stageHeight,
                             tileSize, replaceDistance);
    if (result.outcome == SpawnPlacementOutcome::StageInvalid) {
        return {false, false, {}};
    }
    if (result.outcome == SpawnPlacementOutcome::MissingId) {
        return {true, false, missingStatus};
    }
    const bool changed = result.outcome == SpawnPlacementOutcome::Changed;
    return {true, changed,
            changed ? prefixedSpawnStatus(changedStatusPrefix, result.id)
                    : unchangedStatus};
}

inline SpawnActionResult placeFixedSpawnWithStatus(
    std::vector<EditorSpawn>& spawns,
    const std::string& type,
    float worldX,
    float worldY,
    int stageWidth,
    int stageHeight,
    int tileSize,
    float replaceDistance,
    const std::string& changedStatus,
    const std::string& unchangedStatus) {
    const SpawnPlacementResult result =
        placeSpawnWithResult(spawns, type, "", false, false,
                             worldX, worldY, stageWidth, stageHeight,
                             tileSize, replaceDistance);
    if (result.outcome == SpawnPlacementOutcome::StageInvalid) {
        return {false, false, {}};
    }
    const bool changed = result.outcome == SpawnPlacementOutcome::Changed;
    return {true, changed, changed ? changedStatus : unchangedStatus};
}

inline SpawnActionResult deleteSpawnNearWithStatus(
    std::vector<EditorSpawn>& spawns,
    const std::string& type,
    float worldX,
    float worldY,
    float maxDistance,
    const std::string& deletedStatus,
    const std::string& missingStatus) {
    const bool deleted = deleteSpawnNear(spawns, type, worldX, worldY, maxDistance);
    return {true, deleted, deleted ? deletedStatus : missingStatus};
}

inline void resizeIntLayer(std::vector<int>& data,
                           int oldWidth,
                           int oldHeight,
                           int newWidth,
                           int newHeight,
                           int fillValue) {
    std::vector<int> newData(newWidth * newHeight, fillValue);
    const int copyW = std::min(oldWidth, newWidth);
    const int copyH = std::min(oldHeight, newHeight);
    for (int y = 0; y < copyH; y++) {
        for (int x = 0; x < copyW; x++) {
            newData[y * newWidth + x] = data[y * oldWidth + x];
        }
    }
    data = std::move(newData);
}

inline void resizeCollisionLayer(std::vector<TileType>& collision,
                                 int oldWidth,
                                 int oldHeight,
                                 int newWidth,
                                 int newHeight) {
    std::vector<TileType> newCollision(newWidth * newHeight, TileType::None);
    const int copyW = std::min(oldWidth, newWidth);
    const int copyH = std::min(oldHeight, newHeight);
    for (int y = 0; y < copyH; y++) {
        for (int x = 0; x < copyW; x++) {
            newCollision[y * newWidth + x] = collision[y * oldWidth + x];
        }
    }
    collision = std::move(newCollision);
}

inline void resizeSlopeEdits(std::unordered_map<int, std::pair<int, int>>& slopeEdits,
                             int oldWidth,
                             int newWidth,
                             int newHeight) {
    std::unordered_map<int, std::pair<int, int>> newSlopeEdits;
    for (const auto& [oldIdx, heights] : slopeEdits) {
        if (oldIdx < 0) continue;
        const int x = oldIdx % oldWidth;
        const int y = oldIdx / oldWidth;
        if (x < newWidth && y < newHeight) {
            newSlopeEdits[y * newWidth + x] = heights;
        }
    }
    slopeEdits = std::move(newSlopeEdits);
}

inline bool resizeStageData(StageResizeState& state, int newWidth, int newHeight) {
    if (newWidth < 1 || newHeight < 1) return false;
    if (!state.stageWidth || !state.stageHeight) return false;
    if (newWidth == *state.stageWidth && newHeight == *state.stageHeight) return false;
    if (!state.layerData || !state.layerDataBg || !state.layerDataMainEditable ||
        !state.layerDataBgEditable || !state.collision || !state.slopeEdits) {
        return false;
    }

    const int oldWidth = *state.stageWidth;
    const int oldHeight = *state.stageHeight;
    resizeIntLayer(*state.layerData, oldWidth, oldHeight, newWidth, newHeight, -1);
    resizeIntLayer(*state.layerDataBg, oldWidth, oldHeight, newWidth, newHeight, -1);
    *state.layerDataMainEditable = true;
    *state.layerDataBgEditable = true;
    resizeCollisionLayer(*state.collision, oldWidth, oldHeight, newWidth, newHeight);
    resizeSlopeEdits(*state.slopeEdits, oldWidth, newWidth, newHeight);

    *state.stageWidth = newWidth;
    *state.stageHeight = newHeight;
    return true;
}

} // namespace mmx::map_editor
