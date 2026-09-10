// tilemap_loading.cpp - loads, saves, and variants tilemap JSON data.
// Owns: stage JSON parsing, collision attributes, visual sections, and export.

#include "systems/tilemap.h"
#include "systems/tile_attr_collision.h"

#include "data/content_paths.h"
#include "data/json_io.h"
#include "data/save_system.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <unordered_map>
#include <utility>

using json = nlohmann::json;

namespace mmx {
namespace fs = std::filesystem;

struct PortableAssetPath {
    std::string authoredPath;
    std::string resolvedPath;
};

static std::string stringMemberOrEmpty(const json& object, const char* key) {
    if (!object.is_object() || !object.contains(key) || !object[key].is_string()) {
        return {};
    }
    return object[key].get<std::string>();
}

static json mapMetadataObject(const json& stage) {
    if (stage.contains("mapMetadata") && stage["mapMetadata"].is_object()) {
        return stage["mapMetadata"];
    }
    return json::object();
}

static std::optional<PortableAssetPath> resolvePortableDecodedStageAsset(
    const std::string& id,
    const char* suffix) {
    if (!content_paths::isSafeStem(id)) {
        return std::nullopt;
    }

    const std::string authoredPath =
        std::string("content/x1/stages/tiles/") + id + suffix;
    auto resolvedPath = content_paths::resolveAssetPath(authoredPath);
    if (!resolvedPath || !fs::exists(*resolvedPath)) {
        return std::nullopt;
    }
    return PortableAssetPath{authoredPath, *resolvedPath};
}

static bool parseCameraSectionRect(const json& value, CameraSectionRect& out) {
    if (!value.is_array() || value.size() != 4) return false;
    for (const auto& v : value) {
        if (!v.is_number()) return false;
    }
    out.x = value[0].get<float>();
    out.y = value[1].get<float>();
    out.w = value[2].get<float>();
    out.h = value[3].get<float>();
    return out.w > 0.0f && out.h > 0.0f;
}

static std::vector<CameraSection> parseCameraSections(const json& value) {
    std::vector<CameraSection> out;
    if (!value.is_object()) return out;
    const auto it = value.find("sections");
    if (it == value.end() || !it->is_array()) return out;

    for (const auto& item : *it) {
        if (!item.is_object()) continue;
        CameraSection section;
        section.id = item.value("id", std::string{});
        if (!item.contains("rect") || !parseCameraSectionRect(item["rect"], section.rect)) {
            continue;
        }

        const std::string mode = item.value("mode", std::string{"lock"});
        if (mode == "lock") {
            if (!item.contains("lock_cam_y") || !item["lock_cam_y"].is_number()) {
                continue;
            }
            section.mode = CameraSectionMode::Lock;
            section.lockCamY = item["lock_cam_y"].get<float>();
        } else if (mode == "follow") {
            if (!item.contains("deadzone") || !item["deadzone"].is_array() ||
                item["deadzone"].size() != 2 ||
                !item["deadzone"][0].is_number() ||
                !item["deadzone"][1].is_number()) {
                continue;
            }
            section.mode = CameraSectionMode::Follow;
            section.deadzoneTop = item["deadzone"][0].get<float>();
            section.deadzoneBottom = item["deadzone"][1].get<float>();
            if (section.deadzoneBottom < section.deadzoneTop) {
                std::swap(section.deadzoneTop, section.deadzoneBottom);
            }
            section.lagPxPerFrame = item.value("lag_px_per_f", 0.0f);
        } else {
            continue;
        }
        out.push_back(section);
    }
    return out;
}

struct ParsedVisualSections {
    std::string defaultId;
    std::string bossLockId;
    std::vector<VisualSection> sections;
    VisualSectionSelectionMode selectionMode = VisualSectionSelectionMode::FirstOverlap;
};

static ParsedVisualSections parseVisualSections(const json& value) {
    ParsedVisualSections out;
    if (!value.is_object()) return out;
    if (value.contains("default") && value["default"].is_string()) {
        out.defaultId = value["default"].get<std::string>();
    }
    if (value.contains("boss_lock") && value["boss_lock"].is_string()) {
        out.bossLockId = value["boss_lock"].get<std::string>();
    }
    if (value.contains("selection") && value["selection"].is_string()) {
        const std::string selection = value["selection"].get<std::string>();
        if (selection == "smallest_rect_nearest_center") {
            out.selectionMode = VisualSectionSelectionMode::SmallestRectNearestCenter;
        }
    }

    const auto it = value.find("sections");
    if (it == value.end() || !it->is_array()) return out;
    for (const auto& item : *it) {
        if (!item.is_object()) continue;
        VisualSection section;
        section.id = item.value("id", std::string{});
        if (section.id.empty()) continue;
        if (!item.contains("rect") || !parseCameraSectionRect(item["rect"], section.rect)) {
            continue;
        }
        if (item.contains("tilesetPath") && item["tilesetPath"].is_string()) {
            section.tilesetPath = item["tilesetPath"].get<std::string>();
        }
        out.sections.push_back(section);
    }
    return out;
}

static std::unordered_map<int, TileType> parseAttrOverrides(const json& stageJson) {
    std::unordered_map<int, TileType> attrOverride;
    if (!stageJson.contains("attrMap") || !stageJson["attrMap"].is_object()) {
        return attrOverride;
    }
    for (auto& [k, v] : stageJson["attrMap"].items()) {
        if (!v.is_string()) continue;
        const auto type = tileTypeForAttrMapName(v.get<std::string>());
        if (type) {
            attrOverride[std::stoi(k, nullptr, 16)] = *type;
        }
    }
    return attrOverride;
}

void Tilemap::clearLoadedData() {
    tileset_.reset();

    stageName_.clear();
    stageSource_.clear();
    width_ = 0;
    height_ = 0;
    tileSize_ = TILE_SIZE;
    pitDeathY_.reset();
    bgColor_ = BLACK;
    useDirectTiles_ = false;
    tilesetCols_ = 0;
    tilesetPath_.clear();
    metatiles_.clear();
    layers_.clear();
    collision_.clear();
    slopeData_.clear();
    spawns_.clear();
    rooms_.clear();
    cameraSections_.clear();
    visualSections_.clear();
    sectionTilesets_.clear();
    defaultVisualSectionId_.clear();
    bossLockVisualSectionId_.clear();
    visualSectionSelectionMode_ = VisualSectionSelectionMode::FirstOverlap;
    overlays_.clear();
    regionOverlays_.clear();
    waterEnabled_ = false;
    waterLevel_ = 0.0f;
    waterColor_ = {30, 80, 180, 90};
    collisionPatchesJsonDump_.clear();
    collisionPatchRunsJsonDump_.clear();
    variantsJsonDump_.clear();
    attrs_.clear();
    attrOverrides_.clear();
    attrOverridesFirst_ = false;
    attrMapJsonDump_.clear();
    tilesetFrames_.clear();
    tilesetFramePaths_.clear();
    tilesetFramePeriod_ = 0;
    animationsJsonDump_.clear();
    cameraSectionsJsonDump_.clear();
    visualSectionsJsonDump_.clear();
}

void Tilemap::commitLoadedDataFrom(Tilemap& loaded) {
    clearLoadedData();

    stageName_ = std::move(loaded.stageName_);
    stageSource_ = std::move(loaded.stageSource_);
    width_ = loaded.width_;
    height_ = loaded.height_;
    tileSize_ = loaded.tileSize_;
    pitDeathY_ = loaded.pitDeathY_;
    bgColor_ = loaded.bgColor_;
    useDirectTiles_ = loaded.useDirectTiles_;
    tilesetCols_ = loaded.tilesetCols_;
    tilesetPath_ = std::move(loaded.tilesetPath_);
    tileset_ = std::move(loaded.tileset_);
    metatiles_ = std::move(loaded.metatiles_);
    layers_ = std::move(loaded.layers_);
    tilePriority_ = std::move(loaded.tilePriority_);
    collision_ = std::move(loaded.collision_);
    slopeData_ = std::move(loaded.slopeData_);
    spawns_ = std::move(loaded.spawns_);
    rooms_ = std::move(loaded.rooms_);
    cameraSections_ = std::move(loaded.cameraSections_);
    visualSections_ = std::move(loaded.visualSections_);
    sectionTilesets_ = std::move(loaded.sectionTilesets_);
    defaultVisualSectionId_ = std::move(loaded.defaultVisualSectionId_);
    bossLockVisualSectionId_ = std::move(loaded.bossLockVisualSectionId_);
    visualSectionSelectionMode_ = loaded.visualSectionSelectionMode_;
    overlays_ = std::move(loaded.overlays_);
    regionOverlays_ = std::move(loaded.regionOverlays_);
    waterEnabled_ = loaded.waterEnabled_;
    waterLevel_ = loaded.waterLevel_;
    waterColor_ = loaded.waterColor_;
    collisionPatchesJsonDump_ = std::move(loaded.collisionPatchesJsonDump_);
    collisionPatchRunsJsonDump_ = std::move(loaded.collisionPatchRunsJsonDump_);
    variantsJsonDump_ = std::move(loaded.variantsJsonDump_);
    attrs_ = std::move(loaded.attrs_);
    attrOverrides_ = std::move(loaded.attrOverrides_);
    attrOverridesFirst_ = loaded.attrOverridesFirst_;
    attrMapJsonDump_ = std::move(loaded.attrMapJsonDump_);
    tilesetFrames_ = std::move(loaded.tilesetFrames_);
    tilesetFramePaths_ = std::move(loaded.tilesetFramePaths_);
    tilesetFramePeriod_ = loaded.tilesetFramePeriod_;
    animationsJsonDump_ = std::move(loaded.animationsJsonDump_);
    cameraSectionsJsonDump_ = std::move(loaded.cameraSectionsJsonDump_);
    visualSectionsJsonDump_ = std::move(loaded.visualSectionsJsonDump_);
}

bool Tilemap::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        TraceLog(LOG_ERROR, "Failed to open stage file: %s", path.c_str());
        return false;
    }

    json j;
    try {
        file >> j;
    } catch (const json::parse_error& e) {
        TraceLog(LOG_ERROR, "JSON parse error in %s: %s", path.c_str(), e.what());
        return false;
    }

    try {
        Tilemap loaded;
        loaded.stageName_ = j.value("name", std::string{"unnamed"});
        loaded.stageSource_ = j.value("source", std::string{});
        loaded.width_ = j.value("width", 0);
        loaded.height_ = j.value("height", 0);
        loaded.tileSize_ = j.value("tileSize", TILE_SIZE);
        if (j.contains("pitDeathY")) {
            loaded.pitDeathY_ = j["pitDeathY"].get<float>();
        }

        const json mapMetadata = mapMetadataObject(j);
        const std::string portableTilesetId =
            stringMemberOrEmpty(mapMetadata, "tilesetId");
        const std::string portableBackgroundId =
            stringMemberOrEmpty(mapMetadata, "backgroundId");

        if (j.contains("tilesetPath")) {
            std::string tsPath = j["tilesetPath"];
            loaded.tilesetPath_ = tsPath;
            auto resolvedTilesetPath = content_paths::resolveAssetPath(tsPath);
            if (!resolvedTilesetPath) {
                TraceLog(LOG_ERROR, "Rejected unsafe tileset path in %s: %s",
                         path.c_str(), tsPath.c_str());
                return false;
            }
            loaded.tileset_.load(*resolvedTilesetPath);
            loaded.tileset_.setFilter(TEXTURE_FILTER_POINT);
        } else if (!portableTilesetId.empty()) {
            const auto portableTileset =
                resolvePortableDecodedStageAsset(portableTilesetId, "_tileset_full.png");
            if (!portableTileset) {
                TraceLog(LOG_ERROR, "Unknown mapMetadata.tilesetId in %s: %s",
                         path.c_str(), portableTilesetId.c_str());
                return false;
            }
            loaded.tilesetPath_ = portableTileset->authoredPath;
            loaded.tileset_.load(portableTileset->resolvedPath);
            loaded.tileset_.setFilter(TEXTURE_FILTER_POINT);
            TraceLog(LOG_INFO, "Resolved portable tileset id '%s' to %s",
                     portableTilesetId.c_str(), loaded.tilesetPath_.c_str());
        }

        // U63 palette-cycle animation: per-phase tileset frames + period.
        // Measured (FM belts): 4 CGRAM phases, 10f each, period 40 — the
        // renderer swaps whole atlas textures on that cadence.
        if (j.contains("tilesetFrames") && j["tilesetFrames"].is_array()) {
            for (auto& fp : j["tilesetFrames"]) {
                std::string p = fp.get<std::string>();
                auto resolved = content_paths::resolveAssetPath(p);
                if (!resolved) {
                    TraceLog(LOG_ERROR, "Rejected unsafe tileset frame in %s: %s",
                             path.c_str(), p.c_str());
                    return false;
                }
                loaded.tilesetFramePaths_.push_back(p);
                loaded.tilesetFrames_.emplace_back();
                loaded.tilesetFrames_.back().load(*resolved);
                loaded.tilesetFrames_.back().setFilter(TEXTURE_FILTER_POINT);
            }
            loaded.tilesetFramePeriod_ = j.value("tilesetFramePeriod", 0);
        }

        // (Per-layer preview textures are loaded below inside the layers loop.)

        // Detect direct-tile mode (webp-decomposed stages): has tilesetCols, no metatiles
        loaded.useDirectTiles_ = false;
        loaded.tilesetCols_ = 0;
        if (j.contains("tilesetCols") && !j.contains("metatiles")) {
            loaded.useDirectTiles_ = true;
            loaded.tilesetCols_ = j["tilesetCols"].get<int>();
            TraceLog(LOG_INFO, "Direct-tile mode: %d columns in tileset", loaded.tilesetCols_);
        }

        if (j.contains("metatiles")) {
            for (auto& jm : j["metatiles"]) {
                Metatile m;
                auto& jt = jm["tiles"];
                for (int i = 0; i < 4; i++) {
                    m.tiles[i] = jt[i].get<int>();
                }
                loaded.metatiles_.push_back(m);
            }
        }

        if (loaded.width_ <= 0 || loaded.height_ <= 0) {
            TraceLog(LOG_ERROR, "Invalid stage dimensions: %dx%d", loaded.width_, loaded.height_);
            return false;
        }

        // Background color
        if (j.contains("backgroundColor")) {
            auto& bg = j["backgroundColor"];
            loaded.bgColor_ = {
                static_cast<unsigned char>(bg[0].get<int>()),
                static_cast<unsigned char>(bg[1].get<int>()),
                static_cast<unsigned char>(bg[2].get<int>()),
                255
            };
        }

        // Load visual layers into a temporary map. The old live map is not
        // touched until every required field parses and validates.
        if (j.contains("layers")) {
            for (auto& jl : j["layers"]) {
                TileLayer layer;
                layer.name = jl.value("name", "unnamed");
                layer.parallaxX = jl.value("parallaxX", 1.0f);
                layer.parallaxY = jl.value("parallaxY", 1.0f);
                layer.previewOffsetX = jl.value("previewOffsetX", 0);
                layer.previewOffsetY = jl.value("previewOffsetY", 0);
                layer.repeatPreviewX = jl.value("repeatPreviewX", false);
                layer.repeatPreviewY = jl.value("repeatPreviewY", false);
                layer.drawAfterEntities = jl.value("drawAfterEntities", false);
                if (jl.contains("cameraSectionIds") && jl["cameraSectionIds"].is_array()) {
                    for (const auto& id : jl["cameraSectionIds"]) {
                        if (id.is_string()) layer.cameraSectionIds.push_back(id.get<std::string>());
                    }
                }
                if (jl.contains("visualSectionIds") && jl["visualSectionIds"].is_array()) {
                    for (const auto& id : jl["visualSectionIds"]) {
                        if (id.is_string()) layer.visualSectionIds.push_back(id.get<std::string>());
                    }
                }
                layer.visualPhasePeriod = jl.value("visualPhasePeriod", 0);
                if (jl.contains("visualPhaseIds") && jl["visualPhaseIds"].is_array()) {
                    for (const auto& id : jl["visualPhaseIds"]) {
                        if (id.is_number_integer()) layer.visualPhaseIds.push_back(id.get<int>());
                    }
                }

                auto loadLayerPreview = [&](const std::string& authoredPreviewPath) {
                    layer.previewPath = authoredPreviewPath;
                    auto resolvedPreviewPath = content_paths::resolveAssetPath(layer.previewPath);
                    if (!resolvedPreviewPath) {
                        TraceLog(LOG_ERROR, "Rejected unsafe layer preview path in %s: %s",
                                 path.c_str(), layer.previewPath.c_str());
                        return false;
                    }
                    layer.previewTex.load(*resolvedPreviewPath);
                    if (layer.previewTex.valid()) {
                        layer.previewTex.setFilter(TEXTURE_FILTER_POINT);
                    } else {
                        TraceLog(LOG_WARNING, "Failed to load layer preview: %s",
                                 layer.previewPath.c_str());
                    }
                    return true;
                };

                if (jl.contains("previewPath")) {
                    if (!loadLayerPreview(jl["previewPath"].get<std::string>())) {
                        return false;
                    }
                } else if (layer.name == "bg2" && !portableBackgroundId.empty()) {
                    const auto portableBackground =
                        resolvePortableDecodedStageAsset(portableBackgroundId, "_bg2_full.png");
                    if (!portableBackground) {
                        TraceLog(LOG_ERROR, "Unknown mapMetadata.backgroundId in %s: %s",
                                 path.c_str(), portableBackgroundId.c_str());
                        return false;
                    }
                    if (!loadLayerPreview(portableBackground->authoredPath)) {
                        return false;
                    }
                    TraceLog(LOG_INFO, "Resolved portable background id '%s' to %s",
                             portableBackgroundId.c_str(), layer.previewPath.c_str());
                }

                if (jl.contains("data")) {
                    auto& data = jl["data"];
                    layer.data.reserve(data.size());
                    for (auto& val : data) {
                        layer.data.push_back(val.get<int>());
                    }
                    // Validate size only when data is provided (preview-only layers skip it).
                    if (!layer.data.empty() &&
                        static_cast<int>(layer.data.size()) != loaded.width_ * loaded.height_) {
                        TraceLog(LOG_WARNING, "Layer '%s' has %d tiles, expected %d",
                                 layer.name.c_str(), (int)layer.data.size(), loaded.width_ * loaded.height_);
                    }
                }

                loaded.layers_.push_back(std::move(layer));
            }
        }

        // Load collision layer
        if (j.contains("collision")) {
            auto& coll = j["collision"];
            loaded.collision_.reserve(coll.size());
            for (auto& val : coll) {
                loaded.collision_.push_back(static_cast<TileType>(val.get<int>()));
            }
        }
        std::vector<int> attrOverrideNonSlopeIndices;
        std::unordered_map<int, TileType> attrOverride;
        const TileLayer* main = nullptr;

        // U59: raw per-blockId attribute bytes (decoded stages). When the
        // stage carries attrs and NO collision array, derive collision +
        // slopes from the "main" layer's blockId grid: 0 = air, the measured
        // slope family 0x45-0x4C = the 1:4 profiles, attrMap name overrides
        // (e.g. {"0x37": "conveyor"}); 0x10 = pass-through; other
        // unclassified nonzero attrs = Solid.
        // B10: the SNES BG tile priority nibbles, parallel to the main layer's
        // data (see the stage's own $priority note). Absent = every cell behind.
        if (j.contains("priority") && j["priority"].is_array()) {
            loaded.tilePriority_.reserve(j["priority"].size());
            for (auto& v : j["priority"])
                loaded.tilePriority_.push_back(static_cast<uint8_t>(v.get<int>() & 0x0F));
        }
        if (j.contains("attrs") && j["attrs"].is_array()) {
            loaded.attrs_.reserve(j["attrs"].size());
            for (auto& v : j["attrs"]) loaded.attrs_.push_back(v.get<int>());
            if (j.contains("attrMap") && j["attrMap"].is_object()) {
                loaded.attrMapJsonDump_ = j["attrMap"].dump();
            }
            attrOverride = parseAttrOverrides(j);
            loaded.attrOverrides_ = attrOverride;
            loaded.attrOverridesFirst_ = j.contains("collision");
            for (const auto& l : loaded.layers_) {
                if (l.name == "main") main = &l;
            }
            if (!main && !loaded.layers_.empty()) main = &loaded.layers_.back();

            // Decoded stages may carry a baked collision array from an earlier
            // attr pass. Keep attrMap authoritative so newly classified raw
            // attrs (ladders, decorative no-collide tiles, conveyors) do not
            // require rewriting huge generated collision arrays.
            if (main && !attrOverride.empty() &&
                loaded.collision_.size() == main->data.size()) {
                for (size_t i = 0; i < main->data.size(); ++i) {
                    int bid = main->data[i];
                    int attr = (bid >= 0 && bid < static_cast<int>(loaded.attrs_.size()))
                                   ? loaded.attrs_[bid] : 0;
                    auto ov = attrOverride.find(attr);
                    if (ov == attrOverride.end()) continue;
                    loaded.collision_[i] = ov->second;
                    if (ov->second != TileType::SlopeL &&
                        ov->second != TileType::SlopeR) {
                        const int idx = static_cast<int>(i);
                        loaded.slopeData_.erase(idx);
                        attrOverrideNonSlopeIndices.push_back(idx);
                    }
                }
            }
            if (!j.contains("collision") && !loaded.layers_.empty()) {
                if (!main) main = &loaded.layers_.back();
                loaded.collision_.resize(main->data.size(), TileType::None);
                for (size_t i = 0; i < main->data.size(); ++i) {
                    int bid = main->data[i];
                    int attr = (bid >= 0 && bid < static_cast<int>(loaded.attrs_.size()))
                                   ? loaded.attrs_[bid] : 0;
                    const auto expected = tile_attr_collision::derive(
                        attr, attrOverride, false);
                    loaded.collision_[i] = expected.type;
                    if (expected.hasSlope) {
                        loaded.slopeData_[static_cast<int>(i)] = expected.slope;
                    } else if (expected.type != TileType::SlopeL &&
                               expected.type != TileType::SlopeR) {
                        attrOverrideNonSlopeIndices.push_back(static_cast<int>(i));
                    }
                }
            }
        }

        // Load slope surface heights. Format: array of [tileIdx, leftY, rightY] triples.
        // tileIdx = row * width + col. leftY/rightY are pixel offsets from tile top (0..tileSize).
        if (j.contains("slopes")) {
            for (auto& s : j["slopes"]) {
                if (!s.is_array() || s.size() < 3) continue;
                int idx = s[0].get<int>();
                SlopeHeight h;
                h.leftY  = static_cast<uint8_t>(s[1].get<int>());
                h.rightY = static_cast<uint8_t>(s[2].get<int>());
                loaded.slopeData_[idx] = h;
            }
        }
        for (int idx : attrOverrideNonSlopeIndices) {
            loaded.slopeData_.erase(idx);
        }

        if (j.contains("collisionPatches")) {
            loaded.collisionPatchesJsonDump_ = j["collisionPatches"].dump();
            for (auto& p : j["collisionPatches"]) {
                if (!p.is_array() || p.size() < 2) continue;
                int idx = p[0].get<int>();
                int type = p[1].get<int>();
                if (idx >= 0 && idx < static_cast<int>(loaded.collision_.size())) {
                    loaded.collision_[idx] = static_cast<TileType>(type);
                    if (type != static_cast<int>(TileType::SlopeL) &&
                        type != static_cast<int>(TileType::SlopeR)) {
                        loaded.slopeData_.erase(idx);
                    }
                }
            }
        }

        if (j.contains("collisionPatchRuns")) {
            loaded.collisionPatchRunsJsonDump_ = j["collisionPatchRuns"].dump();
            for (auto& run : j["collisionPatchRuns"]) {
                if (!run.is_array() || run.size() < 4) continue;
                int row = run[0].get<int>();
                int startCol = run[1].get<int>();
                int endCol = run[2].get<int>();
                int type = run[3].get<int>();
                if (row < 0 || row >= loaded.height_) continue;
                startCol = std::max(0, startCol);
                endCol = std::min(loaded.width_ - 1, endCol);
                if (startCol > endCol) continue;
                for (int col = startCol; col <= endCol; ++col) {
                    const int idx = row * loaded.width_ + col;
                    if (idx < 0 || idx >= static_cast<int>(loaded.collision_.size())) continue;
                    loaded.collision_[idx] = static_cast<TileType>(type);
                    if (type != static_cast<int>(TileType::SlopeL) &&
                        type != static_cast<int>(TileType::SlopeR)) {
                        loaded.slopeData_.erase(idx);
                    }
                }
            }
        }

        if (!loaded.attrs_.empty() && main &&
            loaded.collision_.size() == main->data.size()) {
            const bool attrOverrideFirst = j.contains("collision");
            for (size_t i = 0; i < main->data.size(); ++i) {
                int bid = main->data[i];
                int attr = (bid >= 0 && bid < static_cast<int>(loaded.attrs_.size()))
                               ? loaded.attrs_[bid] : 0;
                if (attr == 0) continue;

                const auto expected = tile_attr_collision::derive(
                    attr, attrOverride, attrOverrideFirst);
                const TileType actual = loaded.collision_[i];
                if (actual != expected.type) {
                    TraceLog(LOG_ERROR,
                             "Rejected decoded attr/collision mismatch in %s at tile %d: attr=0x%02X expected=%d actual=%d",
                             path.c_str(), static_cast<int>(i), attr,
                             static_cast<int>(expected.type), static_cast<int>(actual));
                    return false;
                }

                auto slopeIt = loaded.slopeData_.find(static_cast<int>(i));
                if (expected.hasSlope) {
                    if (slopeIt == loaded.slopeData_.end() ||
                        slopeIt->second.leftY != expected.slope.leftY ||
                        slopeIt->second.rightY != expected.slope.rightY) {
                        TraceLog(LOG_ERROR,
                                 "Rejected decoded attr/slope mismatch in %s at tile %d: attr=0x%02X expected=%u,%u",
                                 path.c_str(), static_cast<int>(i), attr,
                                 expected.slope.leftY, expected.slope.rightY);
                        return false;
                    }
                } else if (expected.type != TileType::SlopeL &&
                           expected.type != TileType::SlopeR &&
                           slopeIt != loaded.slopeData_.end()) {
                    TraceLog(LOG_ERROR,
                             "Rejected stale slope data in %s at non-slope tile %d: attr=0x%02X",
                             path.c_str(), static_cast<int>(i), attr);
                    return false;
                }
            }
        }
        // Load spawn points
        if (j.contains("spawns")) {
            for (auto& js : j["spawns"]) {
                SpawnPoint sp;
                sp.type = js.value("type", "unknown");
                sp.id = js.value("id", "");
                sp.x = js.value("x", 0.0f);
                sp.y = js.value("y", 0.0f);
                if (js.contains("activation")) {
                    if (!js["activation"].is_string()) {
                        TraceLog(LOG_ERROR,
                                 "Rejected non-string spawn activation in %s",
                                 path.c_str());
                        return false;
                    }
                    const std::string activation = js["activation"].get<std::string>();
                    if (activation == "source-horizontal-camera-bucket") {
                        sp.activation = SpawnActivation::SourceHorizontalCameraBucket;
                    } else if (activation != "camera-window") {
                        TraceLog(LOG_ERROR,
                                 "Rejected unknown spawn activation in %s: %s",
                                 path.c_str(), activation.c_str());
                        return false;
                    }
                }
                sp.oid = js.value("oid", "");
                sp.hp = js.value("hp", 0);
                sp.provisional = js.value("provenance", "") == "provisional";
                loaded.spawns_.push_back(sp);
            }
        }

        // Load rooms
        if (j.contains("rooms")) {
            for (auto& jr : j["rooms"]) {
                Room room;
                room.name = jr.value("name", "");
                room.x = jr.value("x", 0.0f);
                room.y = jr.value("y", 0.0f);
                room.w = jr.value("w", 0.0f);
                room.h = jr.value("h", 0.0f);
                if (jr.contains("trigger") && jr["trigger"].is_object()) {
                    const auto& jt = jr["trigger"];
                    room.hasTrigger = true;
                    room.tx = jt.value("x", 0.0f);
                    room.ty = jt.value("y", 0.0f);
                    room.tw = jt.value("w", 0.0f);
                    room.th = jt.value("h", 0.0f);
                }
                loaded.rooms_.push_back(room);
            }
        }

        if (j.contains("variants")) {
            loaded.variantsJsonDump_ = j["variants"].dump();
        }

        // U59 passthrough: not-yet-editable keys preserved verbatim so editor
        // saves can't strip them (animations = U63; camera_sections = U39/M5).
        if (j.contains("animations")) {
            loaded.animationsJsonDump_ = j["animations"].dump();
        }
        if (j.contains("camera_sections")) {
            loaded.cameraSectionsJsonDump_ = j["camera_sections"].dump();
            loaded.cameraSections_ = parseCameraSections(j["camera_sections"]);
        }
        if (j.contains("visual_sections")) {
            loaded.visualSectionsJsonDump_ = j["visual_sections"].dump();
            const auto parsed = parseVisualSections(j["visual_sections"]);
            loaded.defaultVisualSectionId_ = parsed.defaultId;
            loaded.bossLockVisualSectionId_ = parsed.bossLockId;
            loaded.visualSections_ = parsed.sections;
            loaded.visualSectionSelectionMode_ = parsed.selectionMode;
            // A section may carry its own block atlas (plan task
            // R3.storm-eagle.sections): the SNES reloads CGRAM per region, so
            // a stage decoded from one capture wears one region's colours
            // everywhere.
            for (const auto& section : loaded.visualSections_) {
                if (section.tilesetPath.empty()) continue;
                auto resolvedSectionTileset =
                    content_paths::resolveAssetPath(section.tilesetPath);
                if (!resolvedSectionTileset) {
                    TraceLog(LOG_ERROR,
                             "Rejected unsafe visual-section tileset in %s: %s",
                             path.c_str(), section.tilesetPath.c_str());
                    return false;
                }
                auto& tex = loaded.sectionTilesets_[section.id];
                tex.load(*resolvedSectionTileset);
                tex.setFilter(TEXTURE_FILTER_POINT);
            }
        }

        const std::string stageName = loaded.stageName_;
        const int loadedLayerCount = static_cast<int>(loaded.layers_.size());
        const int loadedSpawnCount = static_cast<int>(loaded.spawns_.size());
        const int loadedMetatileCount = static_cast<int>(loaded.metatiles_.size());
        const int loadedTilesetWidth = loaded.tileset_.width();
        const int loadedTilesetHeight = loaded.tileset_.height();

        commitLoadedDataFrom(loaded);

        TraceLog(LOG_INFO, "Loaded stage '%s': %dx%d tiles, %d layers, %d spawns, %d metatiles, tileset %dx%d",
                 stageName.c_str(), width_, height_,
                 loadedLayerCount, loadedSpawnCount,
                 loadedMetatileCount, loadedTilesetWidth, loadedTilesetHeight);
        return true;
    } catch (const json::exception& e) {
        TraceLog(LOG_ERROR, "Invalid stage schema in %s: %s", path.c_str(), e.what());
        return false;
    }
}

void Tilemap::applyVariants(const std::string& path) {
    const auto read = json_io::readJsonObjectFromFile(path);
    if (!read.ok) {
        if (read.error != json_io::ReadError::Open) {
            TraceLog(LOG_WARNING, "Stage variants: %s", read.message.c_str());
        }
        return;
    }
    const json& j = read.value;
    if (!j.contains("variants")) return;

    int totalPatches = 0;
    for (auto& var : j["variants"]) {
        if (!var.contains("condition")) continue;
        auto& cond = var["condition"];

        bool hasCondition = false;
        bool met = true;
        if (cond.contains("stage_completed")) {
            hasCondition = true;
            met = met && SaveSystem::isStageCompleted(StageId::fromString(cond["stage_completed"].get<std::string>()));
        }
        if (cond.contains("boss_defeated")) {
            hasCondition = true;
            met = met && SaveSystem::isBossDefeated(BossId::fromString(cond["boss_defeated"].get<std::string>()));
        }
        if (!hasCondition) met = false;
        if (!met) continue;

        std::string label = var.value("label", "unnamed");
        int patches = 0;

        if (var.contains("collision_patches")) {
            for (auto& p : var["collision_patches"]) {
                if (!p.is_array() || p.size() < 2) continue;
                int idx = p[0].get<int>();
                int type = p[1].get<int>();
                if (idx >= 0 && idx < static_cast<int>(collision_.size())) {
                    collision_[idx] = static_cast<TileType>(type);
                    patches++;
                }
            }
        }

        if (var.contains("slope_patches")) {
            for (auto& s : var["slope_patches"]) {
                if (!s.is_array() || s.size() < 3) continue;
                int idx = s[0].get<int>();
                SlopeHeight h;
                h.leftY = static_cast<uint8_t>(s[1].get<int>());
                h.rightY = static_cast<uint8_t>(s[2].get<int>());
                slopeData_[idx] = h;
                patches++;
            }
        }

        if (var.contains("slope_removals")) {
            for (auto& idx : var["slope_removals"]) {
                slopeData_.erase(idx.get<int>());
                patches++;
            }
        }

        if (var.contains("tile_overlays")) {
            for (auto& o : var["tile_overlays"]) {
                if (!o.is_array() || o.size() < 5) continue;
                int idx = o[0].get<int>();
                if (idx >= 0 && idx < static_cast<int>(collision_.size())) {
                    Color c;
                    c.r = static_cast<unsigned char>(o[1].get<int>());
                    c.g = static_cast<unsigned char>(o[2].get<int>());
                    c.b = static_cast<unsigned char>(o[3].get<int>());
                    c.a = static_cast<unsigned char>(o[4].get<int>());
                    overlays_[idx] = c;
                    patches++;
                }
            }
        }

        if (var.contains("region_overlays")) {
            for (auto& o : var["region_overlays"]) {
                if (!o.is_object() || !o.contains("rect") || !o.contains("color")) continue;
                const auto& rect = o["rect"];
                const auto& color = o["color"];
                if (!rect.is_array() || rect.size() < 4 ||
                    !color.is_array() || color.size() < 4) {
                    continue;
                }
                RegionOverlay overlay;
                overlay.x = rect[0].get<float>();
                overlay.y = rect[1].get<float>();
                overlay.w = rect[2].get<float>();
                overlay.h = rect[3].get<float>();
                overlay.color.r = static_cast<unsigned char>(color[0].get<int>());
                overlay.color.g = static_cast<unsigned char>(color[1].get<int>());
                overlay.color.b = static_cast<unsigned char>(color[2].get<int>());
                overlay.color.a = static_cast<unsigned char>(color[3].get<int>());
                if (o.contains("visualSectionIds") && o["visualSectionIds"].is_array()) {
                    for (const auto& id : o["visualSectionIds"]) {
                        if (id.is_string()) overlay.visualSectionIds.push_back(id.get<std::string>());
                    }
                }
                if (overlay.w <= 0.0f || overlay.h <= 0.0f || overlay.color.a == 0) continue;
                regionOverlays_.push_back(overlay);
                patches++;
            }
        }

        if (var.contains("water")) {
            auto& w = var["water"];
            waterEnabled_ = true;
            waterLevel_ = w.value("level", 0.0f);
            if (w.contains("color") && w["color"].is_array() && w["color"].size() >= 4) {
                waterColor_.r = static_cast<unsigned char>(w["color"][0].get<int>());
                waterColor_.g = static_cast<unsigned char>(w["color"][1].get<int>());
                waterColor_.b = static_cast<unsigned char>(w["color"][2].get<int>());
                waterColor_.a = static_cast<unsigned char>(w["color"][3].get<int>());
            }
            TraceLog(LOG_INFO, "Water enabled at Y=%.0f", waterLevel_);
            patches++;
        }

        if (var.contains("visual") && var["visual"].is_object()) {
            auto& visual = var["visual"];
            auto findLayer = [this](const json& spec) -> TileLayer* {
                if (spec.contains("layer") && spec["layer"].is_string()) {
                    const std::string name = spec["layer"].get<std::string>();
                    auto it = std::find_if(layers_.begin(), layers_.end(),
                                           [&name](const TileLayer& layer) {
                                               return layer.name == name;
                                           });
                    return it == layers_.end() ? nullptr : &(*it);
                }
                if (spec.contains("layerIndex") && spec["layerIndex"].is_number_integer()) {
                    const int index = spec["layerIndex"].get<int>();
                    if (index >= 0 && index < static_cast<int>(layers_.size())) {
                        return &layers_[index];
                    }
                }
                return nullptr;
            };

            if (visual.contains("tilesetPath") && visual["tilesetPath"].is_string()) {
                const std::string authored = visual["tilesetPath"].get<std::string>();
                auto resolved = content_paths::resolveAssetPath(authored);
                if (resolved) {
                    tilesetPath_ = authored;
                    tileset_.reset();
                    if (IsWindowReady()) {
                        tileset_.load(*resolved);
                        tileset_.setFilter(TEXTURE_FILTER_POINT);
                    }
                    patches++;
                }
            }

            if (visual.contains("tilesetFrames") && visual["tilesetFrames"].is_array()) {
                std::vector<std::string> framePaths;
                std::vector<TextureResource> frames;
                bool safe = true;
                for (const auto& frame : visual["tilesetFrames"]) {
                    if (!frame.is_string()) {
                        safe = false;
                        break;
                    }
                    const std::string authored = frame.get<std::string>();
                    auto resolved = content_paths::resolveAssetPath(authored);
                    if (!resolved) {
                        safe = false;
                        break;
                    }
                    framePaths.push_back(authored);
                    frames.emplace_back();
                    if (IsWindowReady()) {
                        frames.back().load(*resolved);
                        frames.back().setFilter(TEXTURE_FILTER_POINT);
                    }
                }
                if (safe) {
                    tilesetFramePaths_ = std::move(framePaths);
                    tilesetFrames_ = std::move(frames);
                    patches++;
                }
            }
            if (visual.contains("tilesetFramePeriod") &&
                visual["tilesetFramePeriod"].is_number_integer()) {
                tilesetFramePeriod_ = visual["tilesetFramePeriod"].get<int>();
                patches++;
            }

            if (visual.contains("layer_patches") && visual["layer_patches"].is_array()) {
                for (const auto& layerPatch : visual["layer_patches"]) {
                    if (!layerPatch.is_object()) continue;
                    TileLayer* layer = findLayer(layerPatch);
                    if (!layer || !layerPatch.contains("tiles") ||
                        !layerPatch["tiles"].is_array()) {
                        continue;
                    }
                    for (const auto& tilePatch : layerPatch["tiles"]) {
                        if (!tilePatch.is_array() || tilePatch.size() < 2) continue;
                        const int idx = tilePatch[0].get<int>();
                        const int tileId = tilePatch[1].get<int>();
                        if (idx >= 0 && idx < static_cast<int>(layer->data.size())) {
                            layer->data[idx] = tileId;
                            patches++;
                        }
                    }
                }
            }

            if (visual.contains("layer_previews") && visual["layer_previews"].is_array()) {
                for (const auto& previewPatch : visual["layer_previews"]) {
                    if (!previewPatch.is_object()) continue;
                    TileLayer* layer = findLayer(previewPatch);
                    if (!layer) continue;
                    bool changed = false;
                    if (previewPatch.contains("previewPath") &&
                        previewPatch["previewPath"].is_string()) {
                        const std::string authored = previewPatch["previewPath"].get<std::string>();
                        auto resolved = content_paths::resolveAssetPath(authored);
                        if (!resolved) continue;
                        layer->previewPath = authored;
                        layer->previewTex.reset();
                        if (IsWindowReady()) {
                            layer->previewTex.load(*resolved);
                            layer->previewTex.setFilter(TEXTURE_FILTER_POINT);
                        }
                        changed = true;
                    }
                    if (previewPatch.contains("previewOffsetX")) {
                        layer->previewOffsetX = previewPatch["previewOffsetX"].get<int>();
                        changed = true;
                    }
                    if (previewPatch.contains("previewOffsetY")) {
                        layer->previewOffsetY = previewPatch["previewOffsetY"].get<int>();
                        changed = true;
                    }
                    if (previewPatch.contains("repeatPreviewX")) {
                        layer->repeatPreviewX = previewPatch["repeatPreviewX"].get<bool>();
                        changed = true;
                    }
                    if (previewPatch.contains("repeatPreviewY")) {
                        layer->repeatPreviewY = previewPatch["repeatPreviewY"].get<bool>();
                        changed = true;
                    }
                    if (changed) patches++;
                }
            }
        }

        TraceLog(LOG_INFO, "Applied variant '%s': %d patches", label.c_str(), patches);
        totalPatches += patches;
    }

    if (totalPatches > 0) {
        TraceLog(LOG_INFO, "Stage variants: %d total patches applied", totalPatches);
    }
}

void Tilemap::saveToFile(const std::string& path) const {
    json j;
    j["name"] = stageName_.empty() ? "Custom Stage" : stageName_;
    j["source"] = stageSource_.empty() ? "map-editor" : stageSource_;
    j["width"] = width_;
    j["height"] = height_;
    j["tileSize"] = tileSize_;
    if (pitDeathY_.has_value()) j["pitDeathY"] = *pitDeathY_;
    j["backgroundColor"] = {bgColor_.r, bgColor_.g, bgColor_.b};

    if (!tilesetPath_.empty()) {
        j["tilesetPath"] = tilesetPath_;
    }
    if (useDirectTiles_ || tilesetCols_ > 0) {
        j["tilesetCols"] = tilesetCols_ > 0 ? tilesetCols_ : 32;
    }

    if (!metatiles_.empty()) {
        json jMetatiles = json::array();
        for (const auto& metatile : metatiles_) {
            jMetatiles.push_back({
                {"tiles", {metatile.tiles[0], metatile.tiles[1],
                           metatile.tiles[2], metatile.tiles[3]}},
            });
        }
        j["metatiles"] = jMetatiles;
    }

    json jLayers = json::array();
    for (const auto& layer : layers_) {
        json jl;
        jl["name"] = layer.name;
        jl["parallaxX"] = layer.parallaxX;
        jl["parallaxY"] = layer.parallaxY;
        if (!layer.previewPath.empty()) jl["previewPath"] = layer.previewPath;
        if (layer.previewOffsetX != 0) jl["previewOffsetX"] = layer.previewOffsetX;
        if (layer.previewOffsetY != 0) jl["previewOffsetY"] = layer.previewOffsetY;
        if (layer.repeatPreviewX) jl["repeatPreviewX"] = true;
        if (layer.repeatPreviewY) jl["repeatPreviewY"] = true;
        if (layer.drawAfterEntities) jl["drawAfterEntities"] = true;
        if (!layer.cameraSectionIds.empty()) jl["cameraSectionIds"] = layer.cameraSectionIds;
        if (!layer.visualSectionIds.empty()) jl["visualSectionIds"] = layer.visualSectionIds;
        if (layer.visualPhasePeriod > 0) jl["visualPhasePeriod"] = layer.visualPhasePeriod;
        if (!layer.visualPhaseIds.empty()) jl["visualPhaseIds"] = layer.visualPhaseIds;
        if (!layer.data.empty()) jl["data"] = layer.data;
        jLayers.push_back(jl);
    }
    j["layers"] = jLayers;
    // B10: preserve the priority nibbles verbatim so an editor save cannot
    // strip them (the variantsJsonDump_ pattern).
    if (!tilePriority_.empty()) {
        json jPriority = json::array();
        for (uint8_t v : tilePriority_) jPriority.push_back(static_cast<int>(v));
        j["priority"] = jPriority;
    }

    // Collision
    std::vector<int> collInts;
    collInts.reserve(collision_.size());
    for (auto t : collision_) collInts.push_back(static_cast<int>(t));
    j["collision"] = collInts;

    // Spawns
    json jSpawns = json::array();
    for (const auto& sp : spawns_) {
        json js;
        js["type"] = sp.type;
        js["id"] = sp.id;
        js["x"] = sp.x;
        js["y"] = sp.y;
        if (sp.activation == SpawnActivation::SourceHorizontalCameraBucket) {
            js["activation"] = "source-horizontal-camera-bucket";
        }
        if (!sp.oid.empty()) js["oid"] = sp.oid;
        if (sp.hp > 0) js["hp"] = sp.hp;
        if (sp.provisional) js["provenance"] = "provisional";
        jSpawns.push_back(js);
    }
    j["spawns"] = jSpawns;

    if (!slopeData_.empty()) {
        std::vector<std::pair<int, SlopeHeight>> sortedSlopes(
            slopeData_.begin(), slopeData_.end());
        std::sort(sortedSlopes.begin(), sortedSlopes.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        json jSlopes = json::array();
        for (const auto& [idx, slope] : sortedSlopes) {
            jSlopes.push_back({idx, slope.leftY, slope.rightY});
        }
        j["slopes"] = jSlopes;
    }

    if (!rooms_.empty()) {
        json jRooms = json::array();
        for (const auto& room : rooms_) {
            json jRoom = {
                {"name", room.name},
                {"x", room.x},
                {"y", room.y},
                {"w", room.w},
                {"h", room.h},
            };
            if (room.hasTrigger) {
                jRoom["trigger"] = {
                    {"x", room.tx}, {"y", room.ty}, {"w", room.tw}, {"h", room.th},
                };
            }
            jRooms.push_back(jRoom);
        }
        j["rooms"] = jRooms;
    }

    if (!collisionPatchesJsonDump_.empty()) {
        try {
            j["collisionPatches"] = json::parse(collisionPatchesJsonDump_);
        } catch (const json::exception&) {
            TraceLog(LOG_WARNING, "Skipped invalid preserved collisionPatches while saving: %s",
                     path.c_str());
        }
    }

    if (!collisionPatchRunsJsonDump_.empty()) {
        try {
            j["collisionPatchRuns"] = json::parse(collisionPatchRunsJsonDump_);
        } catch (const json::exception&) {
            TraceLog(LOG_WARNING, "Skipped invalid preserved collisionPatchRuns while saving: %s",
                     path.c_str());
        }
    }

    if (!variantsJsonDump_.empty()) {
        try {
            j["variants"] = json::parse(variantsJsonDump_);
        } catch (const json::exception&) {
            TraceLog(LOG_WARNING, "Skipped invalid preserved variants while saving: %s", path.c_str());
        }
    }

    // U59: raw attrs + preserved passthrough keys
    if (!attrs_.empty()) j["attrs"] = attrs_;
    if (!tilesetFramePaths_.empty()) {
        j["tilesetFrames"] = tilesetFramePaths_;
        j["tilesetFramePeriod"] = tilesetFramePeriod_;
    }
    if (!attrMapJsonDump_.empty()) {
        try {
            j["attrMap"] = json::parse(attrMapJsonDump_);
        } catch (const json::exception&) {
            TraceLog(LOG_WARNING, "Skipped invalid preserved attrMap while saving: %s", path.c_str());
        }
    }
    if (!animationsJsonDump_.empty()) {
        try {
            j["animations"] = json::parse(animationsJsonDump_);
        } catch (const json::exception&) {
            TraceLog(LOG_WARNING, "Skipped invalid preserved animations while saving: %s", path.c_str());
        }
    }
    if (!cameraSectionsJsonDump_.empty()) {
        try {
            j["camera_sections"] = json::parse(cameraSectionsJsonDump_);
        } catch (const json::exception&) {
            TraceLog(LOG_WARNING, "Skipped invalid preserved camera_sections while saving: %s", path.c_str());
        }
    }
    if (!visualSectionsJsonDump_.empty()) {
        try {
            j["visual_sections"] = json::parse(visualSectionsJsonDump_);
        } catch (const json::exception&) {
            TraceLog(LOG_WARNING, "Skipped invalid preserved visual_sections while saving: %s", path.c_str());
        }
    }

    if (json_io::writeAtomically(path, j, -1)) {
        TraceLog(LOG_INFO, "Saved stage to: %s", path.c_str());
    } else {
        TraceLog(LOG_ERROR, "Failed to save stage to: %s", path.c_str());
    }
}

} // namespace mmx
