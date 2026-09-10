// map_editor_user_maps.h - user-map discovery for the editor load browser.

#pragma once

#include "data/content_paths.h"
#include "data/json_io.h"
#include "ui/map_editor_paths.h"
#include "ui/map_editor_stage_json.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace mmx::map_editor {

inline constexpr int kUserMapSchemaVersion = 1;
inline constexpr int kUserMapMetadataFormatVersion = 1;

struct UserMapEntry {
    std::string name;
    std::filesystem::path path;
    std::filesystem::path thumbnailPath;
    std::uintmax_t sizeBytes = 0;
    std::string sizeLabel;
    std::string modifiedDate;
};

inline std::string mapSizeLabel(std::uintmax_t bytes) {
    if (bytes < 1024) {
        return std::to_string(bytes) + " B";
    }
    const std::uintmax_t kib = (bytes + 1023) / 1024;
    return std::to_string(kib) + " KB";
}

inline bool localTimeForUserMap(std::time_t rawTime, std::tm& out) {
#ifdef _WIN32
    return localtime_s(&out, &rawTime) == 0;
#else
    return localtime_r(&rawTime, &out) != nullptr;
#endif
}

inline std::string mapModifiedDateLabel(std::filesystem::file_time_type fileTime) {
    const auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        fileTime - std::filesystem::file_time_type::clock::now() +
        std::chrono::system_clock::now());
    const std::time_t rawTime = std::chrono::system_clock::to_time_t(systemTime);

    std::tm local{};
    if (!localTimeForUserMap(rawTime, local)) {
        return "unknown";
    }

    char buffer[11] = {};
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &local) == 0) {
        return "unknown";
    }
    return buffer;
}

inline std::string userMapDisplayName(const std::filesystem::path& path) {
    const auto read = json_io::readJsonObjectFromFile(path);
    if (read.ok && read.value.contains("name") && read.value["name"].is_string()) {
        return displayMapNameOrDefault(read.value["name"].get<std::string>());
    }
    return displayMapNameOrDefault(path.stem().string());
}

inline std::filesystem::path userMapThumbnailPath(
    const std::filesystem::path& mapPath) {
    std::filesystem::path thumbnail = mapPath;
    thumbnail.replace_extension(".thumbnail.png");
    return thumbnail;
}

inline std::string stringMemberOrEmpty(const nlohmann::json& object, const char* key) {
    if (!object.is_object() || !object.contains(key) || !object[key].is_string()) {
        return {};
    }
    return object[key].get<std::string>();
}

inline bool readSupportedVersionMember(const nlohmann::json& object,
                                       const char* key,
                                       int currentVersion,
                                       int& outVersion) {
    outVersion = 0;
    if (!object.is_object() || !object.contains(key)) {
        return true;
    }
    if (!object[key].is_number_integer()) {
        return false;
    }
    outVersion = object[key].get<int>();
    return outVersion >= 0 && outVersion <= currentVersion;
}

inline std::string userMapVersionError(const nlohmann::json& stage) {
    int schemaVersion = 0;
    if (!readSupportedVersionMember(stage, "schemaVersion",
                                    kUserMapSchemaVersion, schemaVersion)) {
        return "Unsupported map schema version";
    }

    const nlohmann::json metadata = mapMetadataObject(stage);
    int formatVersion = 0;
    if (!readSupportedVersionMember(metadata, "formatVersion",
                                    kUserMapMetadataFormatVersion, formatVersion)) {
        return "Unsupported map format version";
    }

    return {};
}

inline std::filesystem::path discoverUserMapThumbnail(
    const std::filesystem::path& mapPath,
    const nlohmann::json& stage) {
    namespace fs = std::filesystem;

    const nlohmann::json metadata = mapMetadataObject(stage);
    const std::string authored = stringMemberOrEmpty(metadata, "thumbnailPath");
    if (!authored.empty()) {
        fs::path authoredPath(authored);
        if (!authoredPath.is_absolute() && !authoredPath.has_parent_path() &&
            authoredPath.extension() == ".png") {
            const fs::path candidate = mapPath.parent_path() / authoredPath;
            std::error_code ec;
            if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
                return candidate;
            }
        }
    }

    const fs::path fallback = userMapThumbnailPath(mapPath);
    std::error_code ec;
    if (fs::exists(fallback, ec) && fs::is_regular_file(fallback, ec)) {
        return fallback;
    }
    return {};
}

inline bool portableDecodedStageAssetExists(const std::string& id,
                                            const char* suffix) {
    if (!content_paths::isSafeStem(id)) {
        return false;
    }
    const std::string authoredPath =
        std::string("content/x1/stages/tiles/") + id + suffix;
    auto resolvedPath = content_paths::resolveAssetPath(authoredPath);
    return resolvedPath.has_value() && std::filesystem::exists(*resolvedPath);
}

inline bool bg2LayerNeedsPortableBackground(const nlohmann::json& stage) {
    if (!stage.contains("layers") || !stage["layers"].is_array()) {
        return false;
    }
    for (const auto& layer : stage["layers"]) {
        if (!layer.is_object()) continue;
        if (stringMemberOrEmpty(layer, "name") != "bg2") continue;
        return stringMemberOrEmpty(layer, "previewPath").empty();
    }
    return false;
}

inline std::string userMapPlayabilityError(const std::filesystem::path& path) {
    const auto read = json_io::readJsonObjectFromFile(path);
    if (!read.ok) {
        return "Map JSON is invalid";
    }

    const nlohmann::json& stage = read.value;
    const std::string versionError = userMapVersionError(stage);
    if (!versionError.empty()) {
        return versionError;
    }

    const nlohmann::json metadata = mapMetadataObject(stage);
    const bool hasTilesetPath = !stringMemberOrEmpty(stage, "tilesetPath").empty();
    if (!hasTilesetPath) {
        const std::string tilesetId = stringMemberOrEmpty(metadata, "tilesetId");
        if (tilesetId.empty()) {
            return "Missing tileset id";
        }
        if (!portableDecodedStageAssetExists(tilesetId, "_tileset_full.png")) {
            return "Unknown tileset id: " + tilesetId;
        }
    }

    const std::string backgroundId = stringMemberOrEmpty(metadata, "backgroundId");
    if (!backgroundId.empty() && bg2LayerNeedsPortableBackground(stage) &&
        !portableDecodedStageAssetExists(backgroundId, "_bg2_full.png")) {
        return "Unknown background id: " + backgroundId;
    }

    return {};
}

inline std::vector<UserMapEntry> listUserMaps(
    const std::filesystem::path& root = userMapRoot()) {
    namespace fs = std::filesystem;

    std::vector<UserMapEntry> entries;
    std::error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        return entries;
    }

    for (const auto& item : fs::directory_iterator(root, ec)) {
        if (ec) break;
        if (!item.is_regular_file(ec) || item.path().extension() != ".json") {
            ec.clear();
            continue;
        }

        UserMapEntry entry;
        entry.path = item.path();
        entry.name = userMapDisplayName(entry.path);
        const auto read = json_io::readJsonObjectFromFile(entry.path);
        if (read.ok) {
            entry.thumbnailPath = discoverUserMapThumbnail(entry.path, read.value);
        }
        entry.sizeBytes = item.file_size(ec);
        if (ec) {
            entry.sizeBytes = 0;
            ec.clear();
        }
        entry.sizeLabel = mapSizeLabel(entry.sizeBytes);
        entry.modifiedDate = mapModifiedDateLabel(item.last_write_time(ec));
        if (ec) {
            entry.modifiedDate = "unknown";
            ec.clear();
        }
        entries.push_back(std::move(entry));
    }

    std::sort(entries.begin(), entries.end(), [](const UserMapEntry& a, const UserMapEntry& b) {
        return a.name < b.name;
    });
    return entries;
}

} // namespace mmx::map_editor
