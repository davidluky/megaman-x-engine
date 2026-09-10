// map_editor_stage_json.h - defensive JSON helpers for editor map files.

#pragma once

#include "data/json_io.h"
#include "systems/tilemap.h"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <ctime>
#include <exception>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace mmx::map_editor {

using AttrCollisionOverrides = std::unordered_map<int, TileType>;

inline bool readJsonFile(const std::filesystem::path& path, nlohmann::json& out) {
    const auto read = mmx::json_io::readJsonObjectFromFile(path);
    if (!read.ok) return false;
    out = read.value;
    return true;
}

inline std::string usableAssetPath(const std::filesystem::path& metadataPath,
                                   const std::string& rawPath) {
    std::filesystem::path candidate(rawPath);
    if (candidate.is_absolute() || std::filesystem::exists(candidate)) {
        return candidate.generic_string();
    }

    std::filesystem::path relativeToMetadata = metadataPath.parent_path() / candidate;
    if (std::filesystem::exists(relativeToMetadata)) {
        return relativeToMetadata.generic_string();
    }

    return candidate.generic_string();
}

inline std::string stringMemberOrDefault(const nlohmann::json& object,
                                         const char* key,
                                         const std::string& fallback) {
    if (!object.is_object() || !object.contains(key) || !object[key].is_string()) {
        return fallback;
    }
    return object[key].get<std::string>();
}

inline int intMemberOrDefault(const nlohmann::json& object, const char* key, int fallback) {
    if (!object.is_object() || !object.contains(key) || !object[key].is_number_integer()) {
        return fallback;
    }
    return object[key].get<int>();
}

inline int positiveIntMemberOrDefault(const nlohmann::json& object,
                                      const char* key,
                                      int fallback) {
    const int value = intMemberOrDefault(object, key, fallback);
    return value > 0 ? value : fallback;
}

inline float numberMemberOrDefault(const nlohmann::json& object,
                                   const char* key,
                                   float fallback) {
    if (!object.is_object() || !object.contains(key) || !object[key].is_number()) {
        return fallback;
    }
    return object[key].get<float>();
}

inline bool boolMemberOrDefault(const nlohmann::json& object,
                                const char* key,
                                bool fallback) {
    if (!object.is_object() || !object.contains(key) || !object[key].is_boolean()) {
        return fallback;
    }
    return object[key].get<bool>();
}

inline std::string todayIsoDate() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &now) != 0) return "unknown";
#else
    if (!localtime_r(&now, &local)) return "unknown";
#endif
    char buffer[11] = {};
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &local) == 0) {
        return "unknown";
    }
    return buffer;
}

inline nlohmann::json mapMetadataObject(const nlohmann::json& stage) {
    if (stage.contains("mapMetadata") && stage["mapMetadata"].is_object()) {
        return stage["mapMetadata"];
    }
    return nlohmann::json::object();
}

inline float clampPreviewParallax(float value) {
    return std::clamp(value, 0.0f, 4.0f);
}

inline nlohmann::json editableLayerJson(nlohmann::json layer,
                                        const char* name,
                                        double defaultParallaxX,
                                        double defaultParallaxY,
                                        const std::vector<int>& data,
                                        bool writeData) {
    if (!layer.is_object()) {
        layer = nlohmann::json::object();
    }
    layer["name"] = name;
    if (!layer.contains("parallaxX")) layer["parallaxX"] = defaultParallaxX;
    if (!layer.contains("parallaxY")) layer["parallaxY"] = defaultParallaxY;
    if (writeData) {
        layer["data"] = data;
    }
    return layer;
}

inline std::vector<int> intArrayOrFill(const nlohmann::json& value,
                                       size_t expectedSize,
                                       int fillValue) {
    std::vector<int> result(expectedSize, fillValue);
    if (!value.is_array()) {
        return result;
    }
    const size_t limit = std::min(expectedSize, value.size());
    for (size_t index = 0; index < limit; ++index) {
        if (value[index].is_number_integer()) {
            result[index] = value[index].get<int>();
        }
    }
    return result;
}

inline std::vector<int> intArrayValues(const nlohmann::json& value) {
    std::vector<int> result;
    if (!value.is_array()) {
        return result;
    }
    result.reserve(value.size());
    for (const auto& item : value) {
        if (item.is_number_integer()) {
            result.push_back(item.get<int>());
        }
    }
    return result;
}

inline AttrCollisionOverrides parseAttrCollisionOverrides(
    const nlohmann::json& stage) {
    AttrCollisionOverrides overrides;
    if (!stage.contains("attrMap") || !stage["attrMap"].is_object()) {
        return overrides;
    }
    for (const auto& [rawAttr, rawName] : stage["attrMap"].items()) {
        if (!rawName.is_string()) continue;
        const auto type = tileTypeForAttrMapName(rawName.get<std::string>());
        if (!type) continue;
        try {
            size_t consumed = 0;
            const int attr = std::stoi(rawAttr, &consumed, 16);
            if (consumed == rawAttr.size() && attr >= 0 && attr <= 0xFF) {
                overrides[attr] = *type;
            }
        } catch (const std::exception&) {
            continue;
        }
    }
    return overrides;
}

inline bool normalizeAttr10Collision(
    std::vector<TileType>& collision,
    const std::vector<int>& mainBlocks,
    const std::vector<int>& attrs,
    const AttrCollisionOverrides& overrides) {
    const auto override = overrides.find(0x10);
    const TileType expected = override != overrides.end() ? override->second
                                                          : TileType::None;
    const size_t limit = std::min(collision.size(), mainBlocks.size());
    bool changed = false;
    for (size_t idx = 0; idx < limit; ++idx) {
        const int blockId = mainBlocks[idx];
        if (blockId < 0 || blockId >= static_cast<int>(attrs.size()) ||
            attrs[blockId] != 0x10 || collision[idx] == expected) {
            continue;
        }
        collision[idx] = expected;
        changed = true;
    }
    return changed;
}

inline void applyCollisionPatches(std::vector<TileType>& collision,
                                  int width,
                                  int height,
                                  const nlohmann::json& stage) {
    if (width <= 0 || height <= 0) return;

    if (stage.contains("collisionPatches") && stage["collisionPatches"].is_array()) {
        for (const auto& patch : stage["collisionPatches"]) {
            if (!patch.is_array() || patch.size() < 2) continue;
            try {
                const int idx = patch[0].get<int>();
                const int type = patch[1].get<int>();
                if (idx >= 0 && idx < static_cast<int>(collision.size())) {
                    collision[idx] = static_cast<TileType>(type);
                }
            } catch (const nlohmann::json::exception&) {
                continue;
            }
        }
    }

    if (stage.contains("collisionPatchRuns") && stage["collisionPatchRuns"].is_array()) {
        for (const auto& run : stage["collisionPatchRuns"]) {
            if (!run.is_array() || run.size() < 4) continue;
            try {
                const int row = run[0].get<int>();
                int startCol = run[1].get<int>();
                int endCol = run[2].get<int>();
                const int type = run[3].get<int>();
                if (row < 0 || row >= height) continue;
                startCol = std::max(0, startCol);
                endCol = std::min(width - 1, endCol);
                if (startCol > endCol) continue;
                for (int col = startCol; col <= endCol; ++col) {
                    const int idx = row * width + col;
                    if (idx >= 0 && idx < static_cast<int>(collision.size())) {
                        collision[idx] = static_cast<TileType>(type);
                    }
                }
            } catch (const nlohmann::json::exception&) {
                continue;
            }
        }
    }
}

} // namespace mmx::map_editor
