// content_pack.h - declares manifest-backed content pack inventory records.
// Owns: pack, stage, and character metadata shared by menus and loaders.

#pragma once

#include <string>
#include <vector>

// ============================================================================
// content_pack.h — Content pack manifest loader
//
// A content pack is a directory of game data: characters, stages, enemies,
// weapons, sprites, and audio. Each pack has a manifest.json that declares
// what content it provides.
//
// The engine can load multiple packs simultaneously. Any mode can reference
// content from any loaded pack by ID. This enables cross-game mixing:
// play as Zero (X3) in X1 maps, X2 weapons in X1 stages, etc.
//
// Pack directory layout:
//   content/<pack-id>/
//     manifest.json        — Declares all content
//     characters/*.json    — Character physics/stats
//     stages/*.json        — Stage tilemaps + metadata
//     enemies/*.json       — Enemy definitions (future)
//     weapons/*.json       — Weapon definitions (future)
//     sprites/*.png        — Sprite sheets (future)
//     audio/*              — Music + SFX (future)
// ============================================================================

namespace mmx {

struct StageInfo {
    std::string id;
    std::string name;
    std::string config;  // Relative path within pack dir (empty = not yet built)
    std::string boss;    // Boss ID (empty for non-boss stages like intro)
    int order = 0;       // Display order in stage select
    bool available = false; // Has a config file (stage is playable)
    std::vector<std::string> prerequisites; // Stage IDs that must be completed first
    std::string source;  // Origin repo / provenance (e.g. "axlforte Characters fork")
    std::string origin;  // Game-of-origin tag (e.g. "MMX4", "MM10", "fork-original")
};

struct CharacterInfo {
    std::string id;
    std::string name;
    std::string config;
};

struct ContentPack {
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    std::string basePath;  // Filesystem path to the pack directory
    std::string contentRoot; // Filesystem root that pack-relative paths may not escape

    std::vector<CharacterInfo> characters;
    std::vector<StageInfo> stages;

    // Load from a manifest.json file. basePath is set to the containing directory.
    bool loadFromFile(const std::string& manifestPath);

    // Get the full filesystem path for a relative asset path. Paths are
    // resolved from basePath but must stay inside contentRoot.
    std::string resolvePath(const std::string& relativePath) const;

    // Find stage by ID (returns nullptr if not found)
    const StageInfo* findStage(const std::string& stageId) const;
    const CharacterInfo* findCharacter(const std::string& charId) const;
};

} // namespace mmx
