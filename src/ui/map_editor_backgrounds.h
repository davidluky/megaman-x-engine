// map_editor_backgrounds.h - metadata-backed background choices for the editor.

#pragma once

#include "data/json_io.h"
#include "ui/map_editor_stage_json.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace mmx::map_editor {

struct BackgroundEntry {
    std::string id;
    std::string name;
    std::string path;
    float parallaxX = 1.0f;
    float parallaxY = 1.0f;
    int offsetX = 0;
    int offsetY = 0;
    bool repeatX = false;
    bool repeatY = false;
};

inline int nextBackgroundSelection(int current, int direction, int count) {
    if (count <= 0) return -1;
    if (current < 0) return direction < 0 ? count - 1 : 0;
    const int wrapped = (current + direction) % count;
    return wrapped < 0 ? wrapped + count : wrapped;
}

inline int backgroundIndexForPreviewPath(const std::vector<BackgroundEntry>& backgrounds,
                                         const std::string& previewPath) {
    if (previewPath.empty()) return -1;
    for (int i = 0; i < static_cast<int>(backgrounds.size()); ++i) {
        if (backgrounds[i].path == previewPath) return i;
    }
    return -1;
}

inline int backgroundIndexForId(const std::vector<BackgroundEntry>& backgrounds,
                                const std::string& id) {
    if (id.empty()) return -1;
    for (int i = 0; i < static_cast<int>(backgrounds.size()); ++i) {
        if (backgrounds[i].id == id) return i;
    }
    return -1;
}

inline void applyBackgroundEntryToPreview(const BackgroundEntry& entry,
                                          std::string& path,
                                          float& parallaxX,
                                          float& parallaxY,
                                          int& offsetX,
                                          int& offsetY,
                                          bool& repeatX,
                                          bool& repeatY) {
    path = entry.path;
    parallaxX = entry.parallaxX;
    parallaxY = entry.parallaxY;
    offsetX = entry.offsetX;
    offsetY = entry.offsetY;
    repeatX = entry.repeatX;
    repeatY = entry.repeatY;
}

inline void adjustBackgroundParallax(float& parallaxX,
                                     float& parallaxY,
                                     bool vertical,
                                     bool increase) {
    const float delta = increase ? 0.05f : -0.05f;
    if (vertical) {
        parallaxY = clampPreviewParallax(parallaxY + delta);
    } else {
        parallaxX = clampPreviewParallax(parallaxX + delta);
    }
}

inline void toggleBackgroundRepeat(bool& repeatX, bool& repeatY, bool vertical) {
    if (vertical) {
        repeatY = !repeatY;
    } else {
        repeatX = !repeatX;
    }
}

inline bool backgroundEntryFromStageJson(const std::filesystem::path& stageJson,
                                         BackgroundEntry& out) {
    namespace fs = std::filesystem;

    const auto read = json_io::readJsonObjectFromFile(stageJson);
    if (!read.ok || !read.value.contains("layers") || !read.value["layers"].is_array()) {
        return false;
    }

    for (const auto& layer : read.value["layers"]) {
        if (!layer.is_object() ||
            stringMemberOrDefault(layer, "name", "") != "bg2" ||
            !layer.contains("previewPath") ||
            !layer["previewPath"].is_string() ||
            !layer.contains("parallaxX") ||
            !layer["parallaxX"].is_number() ||
            !layer.contains("parallaxY") ||
            !layer["parallaxY"].is_number()) {
            continue;
        }

        const std::string previewPath = layer["previewPath"].get<std::string>();
        if (!fs::exists(fs::path(previewPath))) {
            return false;
        }

        std::string filename = stageJson.filename().generic_string();
        const std::string suffix = "_full.json";
        if (filename.size() > suffix.size() &&
            filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) == 0) {
            out.id = filename.substr(0, filename.size() - suffix.size());
        } else {
            out.id = stageJson.stem().string();
        }
        out.name = stringMemberOrDefault(read.value, "name", stageJson.stem().string());
        out.path = previewPath;
        out.parallaxX = layer["parallaxX"].get<float>();
        out.parallaxY = layer["parallaxY"].get<float>();
        out.offsetX = intMemberOrDefault(layer, "previewOffsetX", 0);
        out.offsetY = intMemberOrDefault(layer, "previewOffsetY", 0);
        out.repeatX = boolMemberOrDefault(layer, "repeatPreviewX", false);
        out.repeatY = boolMemberOrDefault(layer, "repeatPreviewY", false);
        return true;
    }

    return false;
}

inline std::vector<BackgroundEntry> discoverBackgroundsFromDecodedStages(
    const std::filesystem::path& tilesDir = std::filesystem::path("content") / "x1" /
                                            "stages" / "tiles") {
    namespace fs = std::filesystem;

    std::vector<BackgroundEntry> result;
    std::error_code ec;
    if (!fs::exists(tilesDir, ec) || !fs::is_directory(tilesDir, ec)) {
        return result;
    }

    for (const auto& item : fs::directory_iterator(tilesDir, ec)) {
        if (ec) break;
        if (!item.is_regular_file(ec) || item.path().extension() != ".json") {
            ec.clear();
            continue;
        }
        const std::string filename = item.path().filename().generic_string();
        const std::string suffix = "_full.json";
        if (filename.size() <= suffix.size() ||
            filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) != 0) {
            continue;
        }

        BackgroundEntry entry;
        if (backgroundEntryFromStageJson(item.path(), entry)) {
            result.push_back(std::move(entry));
        }
    }

    std::sort(result.begin(), result.end(),
              [](const BackgroundEntry& a, const BackgroundEntry& b) {
                  return a.name < b.name;
              });
    return result;
}

} // namespace mmx::map_editor
