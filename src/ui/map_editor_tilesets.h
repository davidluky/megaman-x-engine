// map_editor_tilesets.h - tileset metadata discovery for the map editor.

#pragma once

#include "ui/map_editor_palette.h"
#include "ui/map_editor_stage_json.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace mmx::map_editor {

struct DiscoveredTileset {
    std::string name;
    std::string id;
    std::string path;
    int cols = 32;
    std::string stageJson;
    std::vector<int> paletteExclude;
};

enum class LoadedMapTilesetWarning {
    None,
    MissingTilesetId,
};

struct LoadedMapTilesetChoice {
    bool hasTileset = false;
    int selectedTileset = -1;
    std::string missingTilesetId;
    LoadedMapTilesetWarning warning = LoadedMapTilesetWarning::None;
    std::string path;
    int cols = 32;
    std::string stageJson;
    std::vector<int> paletteExclude;
    std::string id;
};

struct LoadedTilesetStageMetadata {
    std::vector<int> attrs;
    AttrCollisionOverrides attrOverrides;
    std::vector<int> stagePaletteTileIds;
};

template <typename Tilesets>
inline int tilesetIndexForId(const Tilesets& tilesets, const std::string& id) {
    if (id.empty()) return -1;
    for (int i = 0; i < static_cast<int>(tilesets.size()); ++i) {
        if (tilesets[i].id == id) return i;
    }
    return -1;
}

template <typename TilesetEntry>
inline void loadChoiceFromTilesetEntry(LoadedMapTilesetChoice& choice,
                                       const TilesetEntry& entry) {
    choice.hasTileset = true;
    choice.path = entry.path;
    choice.cols = entry.cols;
    choice.stageJson = entry.stageJson;
    choice.paletteExclude = entry.paletteExclude;
    choice.id = entry.id;
}

inline void loadChoiceFromDiscoveredTileset(LoadedMapTilesetChoice& choice,
                                            const DiscoveredTileset& entry) {
    choice.hasTileset = true;
    choice.path = entry.path;
    choice.cols = entry.cols;
    choice.stageJson = entry.stageJson;
    choice.paletteExclude = entry.paletteExclude;
    choice.id = entry.id;
}

inline LoadedTilesetStageMetadata loadTilesetStageMetadata(
    const std::string& stageJson,
    int rawTileCount,
    const std::vector<int>& paletteExclude) {
    LoadedTilesetStageMetadata out;
    nlohmann::json stageMetadata;
    const bool hasStageMetadata =
        !stageJson.empty() && readJsonFile(stageJson, stageMetadata);
    if (!hasStageMetadata) return out;

    if (stageMetadata.contains("attrs") && stageMetadata["attrs"].is_array()) {
        out.attrs = intArrayValues(stageMetadata["attrs"]);
    }
    out.attrOverrides = parseAttrCollisionOverrides(stageMetadata);
    if (rawTileCount > 0) {
        out.stagePaletteTileIds =
            buildStagePaletteTileIds(stageMetadata, rawTileCount, paletteExclude);
    }
    return out;
}

inline bool readTilesetMetadata(const std::filesystem::path& metadataPath,
                                std::string& tilesetPath,
                                int& tilesetCols,
                                std::vector<int>& paletteExclude,
                                bool* hasAttrs = nullptr) {
    nlohmann::json metadata;
    if (!readJsonFile(metadataPath, metadata)) return false;
    if (hasAttrs) {
        *hasAttrs = metadata.contains("attrs") && metadata["attrs"].is_array();
    }

    std::string pathValue;
    if (metadata.contains("tilesetPath") && metadata["tilesetPath"].is_string()) {
        pathValue = metadata["tilesetPath"].get<std::string>();
    } else if (metadata.contains("tileset_path") && metadata["tileset_path"].is_string()) {
        pathValue = metadata["tileset_path"].get<std::string>();
    }

    if (!pathValue.empty()) {
        tilesetPath = usableAssetPath(metadataPath, pathValue);
    }

    if (metadata.contains("tilesetCols") && metadata["tilesetCols"].is_number_integer()) {
        tilesetCols = metadata["tilesetCols"].get<int>();
    } else if (metadata.contains("tileset_cols") && metadata["tileset_cols"].is_number_integer()) {
        tilesetCols = metadata["tileset_cols"].get<int>();
    }

    paletteExclude = readPaletteExclude(metadata);
    return !tilesetPath.empty();
}

inline bool discoverTilesetForStage(const std::string& stageId,
                                    const std::string& stageName,
                                    const std::filesystem::path& stageConfigPath,
                                    DiscoveredTileset& out) {
    std::string tilesetPath;
    int tilesetCols = 32;
    std::vector<int> paletteExclude;
    bool stageConfigHasAttrs = false;
    const bool stageConfigHasTileset =
        readTilesetMetadata(stageConfigPath, tilesetPath, tilesetCols,
                            paletteExclude, &stageConfigHasAttrs);
    std::string stageJson;
    if (stageConfigHasTileset && stageConfigHasAttrs) {
        stageJson = stageConfigPath.generic_string();
    }

    const std::filesystem::path stageDir = stageConfigPath.parent_path();
    if (tilesetPath.empty()) {
        readTilesetMetadata(stageDir / "stage_webp.json", tilesetPath, tilesetCols,
                            paletteExclude);
    }
    if (tilesetPath.empty()) {
        readTilesetMetadata(stageDir / "tilemap_webp.json", tilesetPath, tilesetCols,
                            paletteExclude);
    }
    if (tilesetPath.empty()) {
        std::filesystem::path fallback = stageDir / "tileset_webp.png";
        if (std::filesystem::exists(fallback)) {
            tilesetPath = fallback.generic_string();
        }
    }

    if (tilesetPath.empty() || !std::filesystem::exists(std::filesystem::path(tilesetPath))) {
        return false;
    }

    if (stageJson.empty()) {
        nlohmann::json stageMetadata;
        if (readJsonFile(stageConfigPath, stageMetadata) &&
            stageMetadata.contains("layers") &&
            stageMetadata["layers"].is_array()) {
            stageJson = stageConfigPath.generic_string();
        }
    }

    out.name = stageName.empty() ? stageId : stageName;
    out.id = stageId;
    out.path = tilesetPath;
    out.cols = std::max(1, tilesetCols);
    out.stageJson = stageJson;
    out.paletteExclude = std::move(paletteExclude);
    return true;
}

template <typename Tilesets>
inline LoadedMapTilesetChoice resolveLoadedMapTileset(
    const nlohmann::json& stage,
    const std::filesystem::path& stageConfigPath,
    const Tilesets& availableTilesets,
    const std::string& tilesetId) {
    LoadedMapTilesetChoice choice;
    if (!tilesetId.empty()) {
        const int tilesetIndex = tilesetIndexForId(availableTilesets, tilesetId);
        if (tilesetIndex >= 0) {
            choice.selectedTileset = tilesetIndex;
            loadChoiceFromTilesetEntry(choice, availableTilesets[tilesetIndex]);
            return choice;
        }
        choice.warning = LoadedMapTilesetWarning::MissingTilesetId;
        choice.missingTilesetId = tilesetId;
    }

    if (stage.contains("tilesetPath") && stage["tilesetPath"].is_string()) {
        choice.hasTileset = true;
        choice.path = usableAssetPath(stageConfigPath,
                                      stage["tilesetPath"].get<std::string>());
        choice.cols = positiveIntMemberOrDefault(stage, "tilesetCols", 32);
        choice.stageJson = (stage.contains("attrs") && stage["attrs"].is_array())
            ? stageConfigPath.generic_string()
            : std::string{};
        choice.paletteExclude = readPaletteExclude(stage);
        return choice;
    }

    DiscoveredTileset fallbackTileset;
    const std::string stageId = stageConfigPath.parent_path().filename().generic_string();
    if (discoverTilesetForStage(stageId, stageId, stageConfigPath, fallbackTileset)) {
        loadChoiceFromDiscoveredTileset(choice, fallbackTileset);
    }
    return choice;
}

inline std::vector<DiscoveredTileset> discoverTilesetsFromManifest(
    const std::filesystem::path& manifestPath) {
    std::vector<DiscoveredTileset> result;

    nlohmann::json manifest;
    if (!readJsonFile(manifestPath, manifest)) return result;

    const std::filesystem::path manifestDir = manifestPath.parent_path();
    for (const auto& stage : manifest.value("stages", nlohmann::json::array())) {
        if (!stage.is_object()) continue;
        if (!stage.contains("config") || !stage["config"].is_string()) continue;

        std::string stageId = stage.value("id", std::string{});
        if (stageId.empty()) continue;
        const std::string stageName = stage.value("name", stageId);

        std::filesystem::path configPath = manifestDir / stage["config"].get<std::string>();
        DiscoveredTileset entry;
        if (!discoverTilesetForStage(stageId, stageName, configPath, entry)) continue;

        auto duplicate = std::find_if(
            result.begin(), result.end(),
            [&](const DiscoveredTileset& existing) { return existing.path == entry.path; });
        if (duplicate == result.end()) {
            result.push_back(std::move(entry));
        }
    }

    return result;
}

inline std::vector<DiscoveredTileset> discoverTilesetsFromRippedDirectory(
    const std::filesystem::path& rippedBase) {
    std::vector<DiscoveredTileset> result;
    if (!std::filesystem::exists(rippedBase)) return result;

    for (const auto& entry : std::filesystem::directory_iterator(rippedBase)) {
        if (!entry.is_directory()) continue;
        DiscoveredTileset tileset;
        if (discoverTilesetForStage(entry.path().filename().generic_string(),
                                    entry.path().filename().generic_string(),
                                    entry.path() / "stage_final.json",
                                    tileset)) {
            result.push_back(std::move(tileset));
        }
    }

    std::sort(result.begin(), result.end(),
              [](const DiscoveredTileset& a, const DiscoveredTileset& b) {
                  return a.name < b.name;
              });
    return result;
}

inline void appendDiscoveredTileset(std::vector<DiscoveredTileset>& tilesets,
                                    DiscoveredTileset entry) {
    auto duplicate = std::find_if(
        tilesets.begin(), tilesets.end(),
        [&](const DiscoveredTileset& existing) { return existing.path == entry.path; });
    if (duplicate != tilesets.end()) {
        if (duplicate->stageJson.empty() && !entry.stageJson.empty()) {
            duplicate->stageJson = std::move(entry.stageJson);
        }
        if (duplicate->paletteExclude.empty() && !entry.paletteExclude.empty()) {
            duplicate->paletteExclude = std::move(entry.paletteExclude);
        }
        if (duplicate->id.empty() && !entry.id.empty()) {
            duplicate->id = std::move(entry.id);
        }
        return;
    }
    tilesets.push_back(std::move(entry));
}

inline std::vector<DiscoveredTileset> discoverDecodedStageTilesets(
    const std::filesystem::path& tilesDir = std::filesystem::path("content") / "x1" /
                                            "stages" / "tiles") {
    std::vector<DiscoveredTileset> result;
    std::error_code ec;
    if (!std::filesystem::exists(tilesDir, ec) ||
        !std::filesystem::is_directory(tilesDir, ec)) {
        return result;
    }

    for (const auto& entry : std::filesystem::directory_iterator(tilesDir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) {
            ec.clear();
            continue;
        }
        const std::string filename = entry.path().filename().generic_string();
        const std::string suffix = "_tileset_full.png";
        if (filename.size() <= suffix.size() ||
            filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) != 0) {
            continue;
        }

        const std::string stageId = filename.substr(0, filename.size() - suffix.size());
        const std::filesystem::path stageJson = tilesDir / (stageId + "_full.json");
        if (!std::filesystem::exists(stageJson, ec)) {
            ec.clear();
            continue;
        }

        DiscoveredTileset tileset;
        tileset.name = stageId + " (decoded)";
        tileset.id = stageId;
        tileset.path = entry.path().generic_string();
        tileset.cols = 16;
        tileset.stageJson = stageJson.generic_string();
        result.push_back(std::move(tileset));
    }

    return result;
}

inline std::vector<DiscoveredTileset> discoverAvailableTilesets(
    const std::filesystem::path& manifestPath = std::filesystem::path("content") / "x1" /
                                                "manifest.json",
    const std::filesystem::path& rippedBase = std::filesystem::path("content") / "x1" /
                                              "stages" / "ripped",
    const std::filesystem::path& decodedTilesDir = std::filesystem::path("content") / "x1" /
                                                   "stages" / "tiles") {
    std::vector<DiscoveredTileset> result;
    auto discovered = discoverTilesetsFromManifest(manifestPath);
    if (discovered.empty()) {
        discovered = discoverTilesetsFromRippedDirectory(rippedBase);
    }
    for (auto& tileset : discovered) {
        appendDiscoveredTileset(result, std::move(tileset));
    }
    for (auto& tileset : discoverDecodedStageTilesets(decodedTilesDir)) {
        appendDiscoveredTileset(result, std::move(tileset));
    }
    return result;
}

} // namespace mmx::map_editor
