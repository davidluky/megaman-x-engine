// map_editor_spawns.h - spawn serialization and labels for the map editor.

#pragma once

#include "data/content_paths.h"
#include "data/x1_catalog.h"
#include "ui/map_editor_stage_json.h"
#include "raylib.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mmx::map_editor {

struct EditorSpawn {
    std::string type;
    std::string id;
    float x = 0.0f;
    float y = 0.0f;
    nlohmann::json raw = nlohmann::json::object();
};

struct EditorSpritePreviewSpec {
    std::string path;
    int frameWidth = 0;
    int frameHeight = 0;
};

inline bool operator==(const EditorSpawn& lhs, const EditorSpawn& rhs) {
    return lhs.type == rhs.type &&
           lhs.id == rhs.id &&
           lhs.x == rhs.x &&
           lhs.y == rhs.y &&
           lhs.raw == rhs.raw;
}

inline bool isEditorSafeId(const std::string& value) {
    if (value.empty() || value.size() > 64) return false;
    for (unsigned char ch : value) {
        if (std::isalnum(ch) || ch == '_' || ch == '-') continue;
        return false;
    }
    return true;
}

inline bool isEditorHiddenEnemySpawnAlias(const std::string& id) {
    // Legacy map IDs remain loadable, but the picker offers only the canonical
    // runtime definition for each shared source OID.
    return id == "bee_flyer" || id == "batton_bee";
}

inline std::string editorEnemyDisplayId(const std::string& id) {
    if (id == "bee_flyer") return "bat";
    if (id == "batton_bee") return "walker";
    return id;
}

inline std::vector<std::string> loadEnemySpawnIds() {
    std::vector<std::string> ids;
    const auto path = content_paths::resolveAssetPath("content/x1/enemies/enemy_defs.json");
    if (!path) return ids;

    nlohmann::json defs;
    if (!readJsonFile(*path, defs) || !defs.is_object()) return ids;

    for (auto it = defs.begin(); it != defs.end(); ++it) {
        if (!it.value().is_object()) continue;
        if (!isEditorSafeId(it.key())) continue;
        if (isEditorHiddenEnemySpawnAlias(it.key())) continue;
        ids.push_back(it.key());
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

inline std::unordered_map<std::string, EditorSpritePreviewSpec> loadEnemyPreviewSpecs() {
    std::unordered_map<std::string, EditorSpritePreviewSpec> specs;
    const auto path = content_paths::resolveAssetPath("content/x1/enemies/enemy_defs.json");
    if (!path) return specs;

    nlohmann::json defs;
    if (!readJsonFile(*path, defs) || !defs.is_object()) return specs;

    for (auto it = defs.begin(); it != defs.end(); ++it) {
        if (!it.value().is_object()) continue;
        if (!isEditorSafeId(it.key())) continue;
        const nlohmann::json& def = it.value();
        if (!def.contains("spritePath") || !def["spritePath"].is_string()) continue;

        EditorSpritePreviewSpec spec;
        spec.path = usableAssetPath(*path, def["spritePath"].get<std::string>());
        const int fallbackW = positiveIntMemberOrDefault(def, "hitboxWidth", 24);
        const int fallbackH = positiveIntMemberOrDefault(def, "hitboxHeight", 24);
        spec.frameWidth = positiveIntMemberOrDefault(def, "frameWidth", fallbackW);
        spec.frameHeight = positiveIntMemberOrDefault(def, "frameHeight", fallbackH);
        specs.emplace(it.key(), std::move(spec));
    }
    return specs;
}

inline const EditorSpritePreviewSpec* enemyPreviewSpecForId(const std::string& id) {
    static const std::unordered_map<std::string, EditorSpritePreviewSpec> specs =
        loadEnemyPreviewSpecs();
    const auto it = specs.find(editorEnemyDisplayId(id));
    return it == specs.end() ? nullptr : &it->second;
}

inline std::vector<std::string> loadPickupSpawnIds() {
    return {
        "heart-tank",
        "sub-tank",
    };
}

inline std::vector<std::string> loadBossSpawnIds() {
    std::vector<std::string> ids;
    for (const auto& boss : x1_catalog::maverickBosses()) {
        if (isEditorSafeId(boss.boss.str())) {
            ids.push_back(boss.boss.str());
        }
    }
    if (isEditorSafeId("sigma")) {
        ids.push_back("sigma");
    }
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

inline std::vector<EditorSpawn> loadEditorSpawns(const nlohmann::json& stage) {
    std::vector<EditorSpawn> spawns;
    if (!stage.contains("spawns") || !stage["spawns"].is_array()) {
        return spawns;
    }

    for (const auto& spawn : stage["spawns"]) {
        if (!spawn.is_object()) continue;
        if (!spawn.contains("type") || !spawn["type"].is_string()) continue;
        if (!spawn.contains("x") || !spawn["x"].is_number()) continue;
        if (!spawn.contains("y") || !spawn["y"].is_number()) continue;

        EditorSpawn editorSpawn;
        editorSpawn.type = spawn["type"].get<std::string>();
        if (spawn.contains("id") && spawn["id"].is_string()) {
            editorSpawn.id = spawn["id"].get<std::string>();
        }
        editorSpawn.x = spawn["x"].get<float>();
        editorSpawn.y = spawn["y"].get<float>();
        editorSpawn.raw = spawn;
        spawns.push_back(std::move(editorSpawn));
    }
    return spawns;
}

inline nlohmann::json editorSpawnsToJson(const std::vector<EditorSpawn>& spawns) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& spawn : spawns) {
        nlohmann::json item =
            spawn.raw.is_object() ? spawn.raw : nlohmann::json::object();
        item["type"] = spawn.type;
        if (!spawn.id.empty() || item.contains("id")) {
            item["id"] = spawn.id;
        }
        item["x"] = spawn.x;
        item["y"] = spawn.y;
        out.push_back(std::move(item));
    }
    return out;
}

inline Color editorSpawnMarkerColor(const std::string& type) {
    if (type == "player_spawn") return {80, 255, 120, 255};
    if (type == "enemy") return {255, 80, 80, 255};
    if (type == "pickup") return {255, 224, 80, 255};
    if (type == "checkpoint") return {80, 220, 255, 255};
    if (type == "boss") return {210, 120, 255, 255};
    if (type == "stage_object") return {255, 160, 80, 255};
    return {220, 220, 220, 255};
}

inline const char* editorSpawnMarkerLabel(const std::string& type) {
    if (type == "player_spawn") return "PL";
    if (type == "enemy") return "EN";
    if (type == "pickup") return "PK";
    if (type == "checkpoint") return "CP";
    if (type == "boss") return "BO";
    if (type == "stage_object") return "SO";
    return "OB";
}

inline std::string compactEditorObjectId(const std::string& id, std::size_t maxChars = 12) {
    if (id.empty()) return "-";
    if (id.size() <= maxChars) return id;
    if (maxChars <= 1) return id.substr(0, maxChars);
    return id.substr(0, maxChars - 1) + "~";
}

inline std::string editorSpawnMarkerLabel(const EditorSpawn& spawn) {
    if (spawn.type == "enemy" && !spawn.id.empty()) {
        return "EN:" + compactEditorObjectId(editorEnemyDisplayId(spawn.id), 12);
    }
    if (spawn.type == "pickup" && !spawn.id.empty()) {
        return "PK:" + compactEditorObjectId(spawn.id, 12);
    }
    if (spawn.type == "boss" && !spawn.id.empty()) {
        return "BO:" + compactEditorObjectId(spawn.id, 12);
    }
    return editorSpawnMarkerLabel(spawn.type);
}

} // namespace mmx::map_editor

namespace mmx {
using EditorSpawn = map_editor::EditorSpawn;
} // namespace mmx
