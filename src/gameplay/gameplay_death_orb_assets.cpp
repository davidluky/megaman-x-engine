// gameplay_death_orb_assets.cpp - loads death-orb sprite resources for gameplay.
// Owns: death-orb texture lookup and shared asset binding for the scene.

#include "gameplay/gameplay_death_orb_assets.h"

#include "data/content_paths.h"
#include "systems/asset_cache.h"

#include "raylib.h"

#include <fstream>

#include <nlohmann/json.hpp>

namespace mmx::gameplay_death_orbs {

void loadSpriteMetadata(SpriteMetadata& meta, const std::string& metaPath) {
    std::ifstream f(metaPath);
    if (!f.is_open()) return;

    try {
        nlohmann::json j;
        f >> j;
        if (j.contains("deathOrb")) {
            const auto& d = j["deathOrb"];
            meta.sheetX = d.value("sheetX", meta.sheetX);
            meta.sheetY = d.value("sheetY", meta.sheetY);
            meta.frameWidth = d.value("frameWidth", meta.frameWidth);
            meta.frameHeight = d.value("frameHeight", meta.frameHeight);
            meta.frameCount = d.value("frameCount", meta.frameCount);
        }
    } catch (const nlohmann::json::parse_error& e) {
        TraceLog(LOG_WARNING, "death orb meta parse error: %s - using defaults", e.what());
    }
}

void loadSpriteAssets(const TextureResource*& sheet, SpriteMetadata& meta) {
    if (sheet && sheet->valid()) return;

    if (auto sheetPath = content_paths::x1SpritePath("x_spritesheet.png")) {
        sheet = AssetCache::loadTexture(*sheetPath);
    }
    if (auto metaPath = content_paths::x1SpritePath("x_spritesheet_meta.json")) {
        loadSpriteMetadata(meta, *metaPath);
    }
}

void loadSpriteAssets(State& state) {
    loadSpriteAssets(state.sheet, state.spriteMeta);
}

} // namespace mmx::gameplay_death_orbs
