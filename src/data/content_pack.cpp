// content_pack.cpp - loads content pack manifests into runtime catalog data.
// Owns: manifest path normalization and pack-local content root boundaries.

#include "data/content_pack.h"
#include "data/path_utils.h"
#include "raylib.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;
using mmx::path_utils::normalizedPath;
using mmx::path_utils::pathIsWithin;
using mmx::path_utils::withForwardSlashes;

namespace mmx {

bool ContentPack::loadFromFile(const std::string& manifestPath) {
    std::ifstream file(manifestPath);
    if (!file.is_open()) {
        TraceLog(LOG_ERROR, "Failed to open manifest: %s", manifestPath.c_str());
        return false;
    }

    json j;
    try {
        file >> j;
    } catch (const json::parse_error& e) {
        TraceLog(LOG_ERROR, "JSON parse error in %s: %s", manifestPath.c_str(), e.what());
        return false;
    }

    const fs::path manifestDir = fs::path(manifestPath).parent_path().empty()
        ? fs::path(".")
        : fs::path(manifestPath).parent_path();
    const fs::path normalizedBase = normalizedPath(manifestDir);
    const fs::path normalizedRoot = normalizedBase.parent_path().empty()
        ? normalizedBase
        : normalizedBase.parent_path();

    basePath = withForwardSlashes(normalizedBase);
    contentRoot = withForwardSlashes(normalizedRoot);
    if (!basePath.empty() && basePath.back() != '/') {
        basePath += '/';
    }

    try {
        id = j.value("id", "unknown");
        name = j.value("name", "Unknown Pack");
        version = j.value("version", "0.0.0");
        description = j.value("description", "");

        // Load character definitions
        characters.clear();
        if (j.contains("characters")) {
            for (auto& jc : j["characters"]) {
                CharacterInfo ci;
                ci.id = jc.value("id", "");
                ci.name = jc.value("name", "");
                ci.config = jc.value("config", "");
                characters.push_back(std::move(ci));
            }
        }

        // Load stage definitions
        stages.clear();
        if (j.contains("stages")) {
            for (auto& js : j["stages"]) {
                StageInfo si;
                si.id = js.value("id", "");
                si.name = js.value("name", "");
                si.order = js.value("order", 0);

                // Config path (null in JSON = empty string = not yet built)
                if (js.contains("config") && !js["config"].is_null()) {
                    si.config = js["config"].get<std::string>();
                    // Check if the file actually exists
                    std::string fullPath = resolvePath(si.config);
                    si.available = fs::exists(fullPath);
                }

                if (js.contains("boss") && !js["boss"].is_null()) {
                    si.boss = js["boss"].get<std::string>();
                }
                if (js.contains("prerequisites") && js["prerequisites"].is_array()) {
                    for (auto& p : js["prerequisites"]) {
                        si.prerequisites.push_back(p.get<std::string>());
                    }
                }
                si.source = js.value("source", "");
                si.origin = js.value("origin", "");

                stages.push_back(std::move(si));
            }
        }

        TraceLog(LOG_INFO, "Loaded content pack '%s' v%s: %d characters, %d stages",
                 name.c_str(), version.c_str(),
                 (int)characters.size(), (int)stages.size());
        return true;
    } catch (const json::exception& e) {
        id = "unknown";
        name = "Unknown Pack";
        version = "0.0.0";
        description.clear();
        characters.clear();
        stages.clear();
        TraceLog(LOG_ERROR, "Invalid manifest schema in %s: %s", manifestPath.c_str(), e.what());
        return false;
    }
}

std::string ContentPack::resolvePath(const std::string& relativePath) const {
    if (relativePath.empty() || basePath.empty() || contentRoot.empty()) {
        return {};
    }

    const fs::path relative(relativePath);
    if (relative.is_absolute() || relative.has_root_name() || relative.has_root_directory()) {
        TraceLog(LOG_WARNING, "ContentPack: rejecting absolute content path: %s",
                 relativePath.c_str());
        return {};
    }

    const fs::path root(contentRoot);
    const fs::path candidate = normalizedPath(fs::path(basePath) / relative);
    if (!pathIsWithin(root, candidate)) {
        TraceLog(LOG_WARNING, "ContentPack: rejecting path outside content root: %s",
                 relativePath.c_str());
        return {};
    }

    return withForwardSlashes(candidate);
}

const StageInfo* ContentPack::findStage(const std::string& stageId) const {
    for (const auto& s : stages) {
        if (s.id == stageId) return &s;
    }
    return nullptr;
}

const CharacterInfo* ContentPack::findCharacter(const std::string& charId) const {
    for (const auto& c : characters) {
        if (c.id == charId) return &c;
    }
    return nullptr;
}

} // namespace mmx
