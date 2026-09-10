// map_editor_save_load.h - user-map save/load JSON helpers.

#pragma once

#include "data/content_paths.h"
#include "systems/tilemap.h"
#include "ui/map_editor_backgrounds.h"
#include "ui/map_editor_spawns.h"
#include "ui/map_editor_stage_json.h"
#include "ui/map_editor_user_maps.h"

#include <nlohmann/json.hpp>
#include "raylib.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mmx::map_editor {

struct SavePreviewLayer {
    std::string path;
    float parallaxX = 1.0f;
    float parallaxY = 1.0f;
    int offsetX = 0;
    int offsetY = 0;
    bool repeatX = false;
    bool repeatY = false;
};

template <typename PreviewLayer>
inline SavePreviewLayer savePreviewLayerFrom(const PreviewLayer& preview) {
    SavePreviewLayer state;
    state.path = preview.path;
    state.parallaxX = preview.parallaxX;
    state.parallaxY = preview.parallaxY;
    state.offsetX = preview.offsetX;
    state.offsetY = preview.offsetY;
    state.repeatX = preview.repeatX;
    state.repeatY = preview.repeatY;
    return state;
}

template <typename PreviewLayer>
inline void applyLoadedPreviewLayer(PreviewLayer& preview,
                                    const SavePreviewLayer& loaded) {
    preview = {};
    preview.path = loaded.path;
    preview.parallaxX = loaded.parallaxX;
    preview.parallaxY = loaded.parallaxY;
    preview.offsetX = loaded.offsetX;
    preview.offsetY = loaded.offsetY;
    preview.repeatX = loaded.repeatX;
    preview.repeatY = loaded.repeatY;
    if (preview.path.empty()) return;

    auto resolvedPreview = content_paths::resolveAssetPath(preview.path);
    if (resolvedPreview && preview.texture.load(*resolvedPreview)) {
        preview.texture.setFilter(TEXTURE_FILTER_POINT);
    }
}

struct MapSaveJsonState {
    std::string mapName;
    std::string mapAuthor;
    std::string mapDescription;
    std::string tilesetPath;
    std::string tilesetId;
    std::string thumbnailFilename;
    int stageWidth = 0;
    int stageHeight = 0;
    int tileSize = 16;
    int tilesetCols = 0;
    int selectedBackground = -1;
    const std::vector<int>* paletteExclude = nullptr;
    const std::vector<int>* layerData = nullptr;
    const std::vector<int>* layerDataBg = nullptr;
    bool layerDataMainEditable = true;
    bool layerDataBgEditable = true;
    const std::vector<TileType>* collision = nullptr;
    const std::unordered_map<int, std::pair<int, int>>* slopeEdits = nullptr;
    const std::vector<EditorSpawn>* editorSpawns = nullptr;
    bool spawnsDirty = false;
    const std::vector<BackgroundEntry>* availableBackgrounds = nullptr;
    SavePreviewLayer previewMain;
    SavePreviewLayer previewBg;
};

struct LoadedMapIdentity {
    bool hasNamedSavePath = false;
    std::string savePath;
    std::string mapName;
    std::string mapAuthor;
    std::string mapDescription;
    std::string tilesetId;
    std::string backgroundId;
};

struct LoadedMapLayers {
    std::vector<int> main;
    std::vector<int> bg;
    bool mainEditable = false;
    bool bgEditable = false;
    SavePreviewLayer previewMain;
    SavePreviewLayer previewBg;
};

inline void writeSavePreviewMetadata(nlohmann::json& layer,
                                     const SavePreviewLayer& preview) {
    if (preview.path.empty()) return;
    layer["previewPath"] = preview.path;
    layer["parallaxX"] = preview.parallaxX;
    layer["parallaxY"] = preview.parallaxY;
    layer["previewOffsetX"] = preview.offsetX;
    layer["previewOffsetY"] = preview.offsetY;
    layer["repeatPreviewX"] = preview.repeatX;
    layer["repeatPreviewY"] = preview.repeatY;
}

inline std::string saveSelectedBackgroundId(const MapSaveJsonState& state) {
    if (!state.availableBackgrounds) return {};
    const auto& backgrounds = *state.availableBackgrounds;
    if (state.selectedBackground >= 0 &&
        state.selectedBackground < static_cast<int>(backgrounds.size())) {
        return backgrounds[state.selectedBackground].id;
    }
    if (!state.previewBg.path.empty()) {
        for (const auto& entry : backgrounds) {
            if (entry.path == state.previewBg.path) return entry.id;
        }
    }
    return {};
}

inline void writeMapSaveMetadata(nlohmann::json& j, const MapSaveJsonState& state) {
    nlohmann::json metadata = mapMetadataObject(j);
    const std::string today = todayIsoDate();
    metadata["formatVersion"] = kUserMapMetadataFormatVersion;
    metadata["author"] = state.mapAuthor;
    metadata["description"] = state.mapDescription;
    if (!metadata.contains("createdDate") || !metadata["createdDate"].is_string()) {
        metadata["createdDate"] = today;
    }
    metadata["modifiedDate"] = today;
    if (!state.tilesetId.empty()) {
        metadata["tilesetId"] = state.tilesetId;
    } else {
        metadata.erase("tilesetId");
    }

    const std::string backgroundId = saveSelectedBackgroundId(state);
    if (!backgroundId.empty()) {
        metadata["backgroundId"] = backgroundId;
    } else {
        metadata.erase("backgroundId");
    }
    metadata["thumbnailPath"] = state.thumbnailFilename;
    j["mapMetadata"] = metadata;
}

inline void writeMapSaveLayers(nlohmann::json& j, const MapSaveJsonState& state) {
    nlohmann::json layers = nlohmann::json::array();
    bool wroteBg2 = false;
    bool wroteMain = false;

    if (j.contains("layers") && j["layers"].is_array()) {
        for (const auto& existingLayer : j["layers"]) {
            if (!existingLayer.is_object()) continue;
            const std::string name = existingLayer.value("name", std::string{});
            if (name == "bg2") {
                auto layer = editableLayerJson(
                    existingLayer, "bg2", 0.5, 0.5, *state.layerDataBg,
                    state.layerDataBgEditable);
                writeSavePreviewMetadata(layer, state.previewBg);
                layers.push_back(std::move(layer));
                wroteBg2 = true;
            } else if (name == "main") {
                auto layer = editableLayerJson(
                    existingLayer, "main", 1.0, 1.0, *state.layerData,
                    state.layerDataMainEditable);
                writeSavePreviewMetadata(layer, state.previewMain);
                layers.push_back(std::move(layer));
                wroteMain = true;
            } else {
                layers.push_back(existingLayer);
            }
        }
    }

    if (!wroteBg2) {
        auto layer = editableLayerJson(
            nlohmann::json::object(), "bg2", 0.5, 0.5, *state.layerDataBg, true);
        writeSavePreviewMetadata(layer, state.previewBg);
        layers.push_back(std::move(layer));
    }
    if (!wroteMain) {
        auto layer = editableLayerJson(
            nlohmann::json::object(), "main", 1.0, 1.0, *state.layerData, true);
        writeSavePreviewMetadata(layer, state.previewMain);
        layers.push_back(std::move(layer));
    }
    j["layers"] = layers;
}

inline void writeMapSaveCollision(nlohmann::json& j, const MapSaveJsonState& state) {
    std::vector<int> collision;
    collision.reserve(state.collision ? state.collision->size() : 0);
    if (state.collision) {
        for (auto tile : *state.collision) {
            collision.push_back(static_cast<int>(tile));
        }
    }
    j["collision"] = collision;
    j.erase("collisionPatches");
    j.erase("collisionPatchRuns");
}

inline void writeMapSaveSlopes(nlohmann::json& j, const MapSaveJsonState& state) {
    if (!state.slopeEdits || state.slopeEdits->empty()) return;

    nlohmann::json slopes = nlohmann::json::array();
    if (j.contains("slopes") && j["slopes"].is_array()) {
        for (const auto& slope : j["slopes"]) {
            if (slope.is_array() && slope.size() >= 3 &&
                state.slopeEdits->count(slope[0].get<int>()) == 0) {
                slopes.push_back(slope);
            }
        }
    }
    for (const auto& kv : *state.slopeEdits) {
        slopes.push_back({kv.first, kv.second.first, kv.second.second});
    }
    j["slopes"] = slopes;
}

inline void writeMapSaveSpawns(nlohmann::json& j, const MapSaveJsonState& state) {
    if (state.spawnsDirty && state.editorSpawns) {
        j["spawns"] = editorSpawnsToJson(*state.editorSpawns);
    } else if (!j.contains("spawns") || !j["spawns"].is_array() || j["spawns"].empty()) {
        j["spawns"] = nlohmann::json::array({
            {{"type", "player_spawn"},
             {"id", ""},
             {"x", 32.0},
             {"y", static_cast<double>(state.stageHeight * state.tileSize - 48)}}
        });
    }
}

inline nlohmann::json buildUserMapSaveJson(nlohmann::json j,
                                           const MapSaveJsonState& state) {
    if (!j.is_object()) {
        j = nlohmann::json::object();
    }

    j["name"] = displayMapNameOrDefault(state.mapName);
    j["schemaVersion"] = kUserMapSchemaVersion;
    if (!j.contains("source")) j["source"] = "map-editor";
    j["width"] = state.stageWidth;
    j["height"] = state.stageHeight;
    j["tileSize"] = state.tileSize;
    if (!j.contains("backgroundColor")) j["backgroundColor"] = {0, 0, 0};
    j["tilesetPath"] = state.tilesetPath;
    j["tilesetCols"] = state.tilesetCols;
    if (state.paletteExclude && !state.paletteExclude->empty()) {
        j["paletteExclude"] = *state.paletteExclude;
    } else {
        j.erase("paletteExclude");
    }

    writeMapSaveMetadata(j, state);
    writeMapSaveLayers(j, state);
    writeMapSaveCollision(j, state);
    writeMapSaveSlopes(j, state);
    writeMapSaveSpawns(j, state);
    return j;
}

inline LoadedMapIdentity loadMapIdentity(const nlohmann::json& j,
                                         const std::filesystem::path& stagePath) {
    const std::filesystem::path normalizedStagePath = stagePath.lexically_normal();
    const std::filesystem::path normalizedUserMapRoot = userMapRoot().lexically_normal();
    const nlohmann::json metadata = mapMetadataObject(j);

    LoadedMapIdentity out;
    out.hasNamedSavePath = normalizedStagePath.parent_path().generic_string() ==
                           normalizedUserMapRoot.generic_string();
    out.savePath = out.hasNamedSavePath
        ? normalizedStagePath.generic_string()
        : defaultUserMapPath().generic_string();
    out.mapName = displayMapNameOrDefault(
        stringMemberOrDefault(j, "name", stagePath.stem().string()));
    out.mapAuthor = stringMemberOrDefault(metadata, "author", "");
    out.mapDescription = stringMemberOrDefault(metadata, "description", "");
    out.tilesetId = stringMemberOrDefault(metadata, "tilesetId", "");
    out.backgroundId = stringMemberOrDefault(metadata, "backgroundId", "");
    return out;
}

inline SavePreviewLayer previewLayerFromJson(const nlohmann::json& layer) {
    SavePreviewLayer preview;
    preview.parallaxX = numberMemberOrDefault(layer, "parallaxX", preview.parallaxX);
    preview.parallaxY = numberMemberOrDefault(layer, "parallaxY", preview.parallaxY);
    preview.offsetX = intMemberOrDefault(layer, "previewOffsetX", 0);
    preview.offsetY = intMemberOrDefault(layer, "previewOffsetY", 0);
    preview.repeatX = boolMemberOrDefault(layer, "repeatPreviewX", false);
    preview.repeatY = boolMemberOrDefault(layer, "repeatPreviewY", false);
    preview.path = stringMemberOrDefault(layer, "previewPath", "");
    return preview;
}

inline LoadedMapLayers loadMapLayers(const nlohmann::json& j,
                                     int stageWidth,
                                     int stageHeight) {
    LoadedMapLayers out;
    const size_t expectedSize = static_cast<size_t>(stageWidth * stageHeight);
    out.main.assign(expectedSize, -1);
    out.bg.assign(expectedSize, -1);

    if (!j.contains("layers") || !j["layers"].is_array()) {
        return out;
    }

    for (const auto& layer : j["layers"]) {
        if (!layer.is_object()) continue;
        const std::string name = stringMemberOrDefault(layer, "name", "");
        if (name == "main") {
            out.previewMain = previewLayerFromJson(layer);
        } else if (name == "bg2") {
            out.previewBg = previewLayerFromJson(layer);
        }

        if (!layer.contains("data") || !layer["data"].is_array()) continue;
        if (name == "bg2") {
            out.bg = intArrayOrFill(layer["data"], expectedSize, -1);
            out.bgEditable = true;
        } else {
            out.main = intArrayOrFill(layer["data"], expectedSize, -1);
            out.mainEditable = true;
        }
    }
    return out;
}

inline std::vector<TileType> loadMapCollision(const nlohmann::json& j,
                                              int stageWidth,
                                              int stageHeight) {
    std::vector<TileType> collision(
        static_cast<size_t>(stageWidth * stageHeight), TileType::None);
    if (j.contains("collision") && j["collision"].is_array()) {
        const auto values = intArrayOrFill(
            j["collision"], static_cast<size_t>(stageWidth * stageHeight), 0);
        for (int i = 0; i < static_cast<int>(values.size()); i++) {
            collision[i] = static_cast<TileType>(values[i]);
        }
    }
    applyCollisionPatches(collision, stageWidth, stageHeight, j);
    return collision;
}

} // namespace mmx::map_editor
