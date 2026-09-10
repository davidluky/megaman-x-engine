// enemy_definitions.cpp - loads and unloads the enemy definition catalog.
// Owns: enemy JSON definition registry and optional KB behavior IDs.

#include "entities/enemy.h"
#include "entities/enemy_damage.h"
#include "data/content_paths.h"
#include "data/kb_paths.h"

#include <fstream>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace mmx {
namespace {
std::unordered_map<std::string, std::string> s_definitionAliases;

bool readOptionalKBBehaviorId(const json& value,
                              Enemy::Definition& def,
                              const std::string& enemyId,
                              const std::string& sourcePath) {
    if (!value.contains("kbBehaviorId") || value["kbBehaviorId"].is_null()) {
        return true;
    }
    if (!value["kbBehaviorId"].is_string()) {
        TraceLog(LOG_ERROR, "Enemy: kbBehaviorId for '%s' in %s must be a string",
                 enemyId.c_str(), sourcePath.c_str());
        return false;
    }

    std::string behaviorId = value["kbBehaviorId"].get<std::string>();
    if (behaviorId.empty()) {
        return true;
    }
    if (!kb_paths::isSafeIdSegment(behaviorId)) {
        TraceLog(LOG_ERROR, "Enemy: rejected unsafe kbBehaviorId '%s' for '%s' in %s",
                 behaviorId.c_str(), enemyId.c_str(), sourcePath.c_str());
        return false;
    }

    def.kbBehaviorId = std::move(behaviorId);
    return true;
}

} // namespace

std::unordered_map<std::string, Enemy::Definition> Enemy::definitions;

void Enemy::loadDefinitions(const std::string& path) {
    // Skip if already loaded — definitions are shared across all stages
    if (!definitions.empty()) return;

    s_definitionAliases.clear();
    std::ifstream file(path);
    if (!file.is_open()) return;
    json j;
    try {
        file >> j;
        std::unordered_map<std::string, std::string> pendingAliases;
        for (auto& [key, value] : j.items()) {
            // U33 hardening: $-prefixed keys are notes, and a def must be an
            // object — neither may abort the file (the U17 incident: one bad
            // row silently defaulted EVERY enemy).
            if (!key.empty() && key[0] == '$') continue;
            if (!value.is_object()) {
                TraceLog(LOG_WARNING, "Enemy: skipping non-object def '%s' in %s",
                         key.c_str(), path.c_str());
                continue;
            }
            // A compatibility redirect is deliberately not a combat
            // definition. Load canonical definitions first, then admit only
            // one-hop redirects to a real canonical target.
            if (value.contains("$compatAlias")) {
                if (value["$compatAlias"].is_string()) {
                    pendingAliases.emplace(
                        key, value["$compatAlias"].get<std::string>());
                } else {
                    TraceLog(LOG_ERROR,
                             "Enemy: skipping alias '%s' with non-string target in %s",
                             key.c_str(), path.c_str());
                }
                continue;
            }
            try {
            Definition def;
            def.behavior = value.value("behavior", "");
            if (!parseEnemyBehavior(def.behavior)) {
                TraceLog(LOG_ERROR,
                         "Enemy: skipping def '%s' with unknown behavior '%s' in %s",
                         key.c_str(), def.behavior.c_str(), path.c_str());
                continue;
            }
            if (!readOptionalKBBehaviorId(value, def, key, path)) {
                unloadDefinitions();
                return;
            }
            def.hp = value.at("hp").get<int>();
            def.contactDamage = value.at("contactDamage").get<int>();
            def.spritePath = value.value("spritePath", "");
            def.flashSpritePath = value.value("flashSpritePath", "");
            // Frame dims default to 32x32 for backward compat. Larger enemies
            // override per-def so render() can draw at native size with
            // bottom-center anchoring on the hitbox.
            def.frameWidth = value.value("frameWidth", 32);
            def.frameHeight = value.value("frameHeight", 32);
            def.bodyVisualOffsetY = value.value("bodyVisualOffsetY", 0.0f);
            def.bodyMirrorsWithFacing = value.value("bodyMirrorsWithFacing", true);
            def.bodySourceFacesRight = value.value("bodySourceFacesRight", true);
            def.hitboxWidth = value.at("hitboxWidth").get<float>();
            def.hitboxHeight = value.at("hitboxHeight").get<float>();
            def.hitboxOffsetX = value.at("hitboxOffsetX").get<float>();
            def.hitboxOffsetY = value.at("hitboxOffsetY").get<float>();
            if (def.hp <= 0 || def.contactDamage < 0
                || def.hitboxWidth <= 0.0f || def.hitboxHeight <= 0.0f) {
                TraceLog(LOG_ERROR,
                         "Enemy: skipping def '%s' with invalid core combat geometry in %s",
                         key.c_str(), path.c_str());
                continue;
            }
            const bool needsPatrolSpeed = def.behavior == "Patrol"
                || def.behavior == "HoverPatrol";
            const bool needsFlySpeed =
                def.behavior == "FlyPattern" || def.behavior == "Drift";
            const bool needsDetectionRange =
                def.behavior == "HideAndShoot" || def.behavior == "FlyPattern"
                || def.behavior == "Turret" || def.behavior == "AxeMax";
            if ((needsPatrolSpeed
                 && (!value.contains("patrolSpeed")
                     || !value["patrolSpeed"].is_number()))
                || (needsFlySpeed
                    && (!value.contains("flySpeed")
                        || !value["flySpeed"].is_number()))
                || (needsDetectionRange
                    && (!value.contains("detectionRange")
                        || !value["detectionRange"].is_number()))) {
                TraceLog(LOG_ERROR,
                         "Enemy: skipping def '%s' with missing behavior parameters in %s",
                         key.c_str(), path.c_str());
                continue;
            }
            def.patrolSpeed = value.value("patrolSpeed", 0.8f);
            def.flySpeed = value.value("flySpeed", 1.5f);
            def.detectionRange = value.value("detectionRange", 100.0f);
            if ((needsPatrolSpeed && def.patrolSpeed <= 0.0f)
                || (needsFlySpeed && def.flySpeed <= 0.0f)
                || (needsDetectionRange && def.detectionRange < 0.0f)) {
                TraceLog(LOG_ERROR,
                         "Enemy: skipping def '%s' with invalid behavior parameters in %s",
                         key.c_str(), path.c_str());
                continue;
            }
            def.dropType = value.value("dropType", 1);
            def.dropChancePct = value.value("dropChancePct", 0);
            if (value.contains("dropTypes"))
                def.dropTypes = value.at("dropTypes").get<std::vector<int>>();
            enemy_damage::parseInto(def, value); // optional "damage_taken" table

            // Optional per-def animation overrides. Schema:
            //   "animations": {
            //     "walk": { "frames": [[2, 8], [3, 8]], "loop": true },
            //     "hurt": { "frames": [[7, 8]], "loop": false }
            //   }
            if (value.contains("animations") && value["animations"].is_object()) {
                for (auto& [animName, animSpec] : value["animations"].items()) {
                    // $-notes and non-object specs are not animations.
                    if (!animName.empty() && animName[0] == '$') continue;
                    if (!animSpec.is_object()) continue;
                    Animation a;
                    a.name = animName;
                    a.loop = animSpec.value("loop", true);
                    if (animSpec.contains("frames") && animSpec["frames"].is_array()) {
                        for (const auto& fr : animSpec["frames"]) {
                            if (fr.is_array() && fr.size() >= 2) {
                                a.frames.push_back({fr[0].get<int>(), fr[1].get<int>()});
                            }
                        }
                    }
                    def.animations[animName] = std::move(a);
                }
            }

            // Load texture once per definition — shared by all instances of this enemy type
            if (!def.spritePath.empty()) {
                auto resolvedSpritePath = content_paths::resolveAssetPath(def.spritePath);
                if (!resolvedSpritePath) {
                    unloadDefinitions();
                    TraceLog(LOG_ERROR, "Enemy: rejected unsafe sprite path '%s' in %s",
                             def.spritePath.c_str(), path.c_str());
                    return;
                }
                if (def.texture.load(*resolvedSpritePath) && def.frameWidth > 0) {
                    // Derive sheet cells from the configured frame dimensions so
                    // single-row and multi-row enemy sheets both render correctly.
                    int cols = def.texture.width() / def.frameWidth;
                    int rows = def.frameHeight > 0
                        ? def.texture.height() / def.frameHeight
                        : 1;
                    if (cols < 1) cols = 1;
                    if (rows < 1) rows = 1;
                    def.frameCount = cols * rows;
                } else {
                    TraceLog(LOG_WARNING, "Enemy: failed to load texture '%s'",
                             def.spritePath.c_str());
                }
            }

            // FW7-REVIEW-C: optional palette-0 flash variant, loaded once and
            // borrowed by instances exactly like the base sheet.
            if (!def.flashSpritePath.empty()) {
                auto resolvedFlashPath =
                    content_paths::resolveAssetPath(def.flashSpritePath);
                if (!resolvedFlashPath) {
                    unloadDefinitions();
                    TraceLog(LOG_ERROR,
                             "Enemy: rejected unsafe flash sprite path '%s' in %s",
                             def.flashSpritePath.c_str(), path.c_str());
                    return;
                }
                if (!def.flashTexture.load(*resolvedFlashPath)) {
                    TraceLog(LOG_WARNING, "Enemy: failed to load flash texture '%s'",
                             def.flashSpritePath.c_str());
                }
            }

            definitions.emplace(key, std::move(def));
            } catch (const json::exception& e) {
                // U33: a malformed def is skipped with a log — the rest of
                // the roster must keep its measured data.
                TraceLog(LOG_ERROR, "Enemy: skipping malformed def '%s' in %s: %s",
                         key.c_str(), path.c_str(), e.what());
            }
        }
        for (const auto& [alias, target] : pendingAliases) {
            if (alias != target && definitions.count(target) != 0
                && pendingAliases.count(target) == 0) {
                s_definitionAliases.emplace(alias, target);
            } else {
                TraceLog(LOG_ERROR,
                         "Enemy: skipping alias '%s' with non-canonical target '%s' in %s",
                         alias.c_str(), target.c_str(), path.c_str());
            }
        }
    } catch (const json::exception& e) {
        // File-level parse failure (bad JSON syntax): nothing usable.
        unloadDefinitions();
        TraceLog(LOG_ERROR, "Invalid enemy definitions in %s: %s", path.c_str(), e.what());
    }
}

void Enemy::unloadDefinitions() {
    definitions.clear();
    s_definitionAliases.clear();
}

std::string Enemy::canonicalDefinitionId(const std::string& type) {
    const auto it = s_definitionAliases.find(type);
    return it == s_definitionAliases.end() ? type : it->second;
}

} // namespace mmx
