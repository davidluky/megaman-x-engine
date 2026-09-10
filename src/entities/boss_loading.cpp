// boss_loading.cpp - loads boss definitions, textures, and diagnostics.
// Owns: boss JSON parsing, texture binding, and missing-data diagnostics.

#include "entities/boss.h"
#include "entities/boss_cp_intro_timeline.h"

#include "data/difficulty.h"
#include "data/kb_paths.h"
#include "systems/asset_cache.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <optional>

using json = nlohmann::json;

namespace mmx {

// ============================================================================
// Boss texture loaders
// ============================================================================

namespace {

bool isRetiredBossRuntimeFallback(const std::string& bossType) {
    return bossType == "chill-penguin"
        || bossType == "vile"
        || bossType == "storm-eagle"
        || bossType == "spark-mandrill"
        || bossType == "sting-chameleon"
        || bossType == "magma-dragoon"
        || bossType == "boomer-kuwanger"
        || bossType == "flame-mammoth"
        || bossType == "sigma"
        || bossType == "armored-armadillo"
        || bossType == "launch-octopus";
}

bool bindCachedBossTexture(
    const TextureResource*& slot,
    int& frameWidth,
    int& frameHeight,
    int& frameCount,
    const std::string& path,
    int frameW,
    int frameH,
    const char* label
) {
    slot = nullptr;
    frameWidth = frameHeight = frameCount = 0;

    const TextureResource* cached = AssetCache::loadTexture(path);
    if (!cached || !cached->valid()) {
        TraceLog(LOG_WARNING, "Boss: failed to load %s texture '%s'", label, path.c_str());
        return false;
    }

    if (frameW <= 0 || frameH <= 0 || cached->width() < frameW || cached->height() < frameH) {
        TraceLog(LOG_WARNING,
                 "Boss: invalid %s frame metadata for '%s' (%dx%d on %dx%d texture)",
                 label, path.c_str(), frameW, frameH, cached->width(), cached->height());
        return false;
    }

    slot = cached;
    frameWidth = frameW;
    frameHeight = frameH;
    frameCount = cached->width() / frameW;
    if (frameCount <= 0) {
        slot = nullptr;
        frameWidth = frameHeight = frameCount = 0;
        TraceLog(LOG_WARNING, "Boss: no %s frames available in '%s'", label, path.c_str());
        return false;
    }

    cached->setFilter(TEXTURE_FILTER_POINT);
    return true;
}

std::optional<BossAttackType> parseAttackType(const std::string& s) {
    if (s == "Charge")         return BossAttackType::Charge;
    if (s == "ProjectileBurst") return BossAttackType::ProjectileBurst;
    if (s == "JumpAttack")     return BossAttackType::JumpAttack;
    if (s == "GroundPound")    return BossAttackType::GroundPound;
    if (s == "Slide")          return BossAttackType::Slide;
    if (s == "Special")        return BossAttackType::Special;
    return std::nullopt;
}

} // namespace

bool Boss::loadTexture(const std::string& path, int frameW, int frameH) {
    hitPaletteTexture = nullptr;
    // Headless (no GL context): GPU upload would crash; render falls back to
    // the rectangle path, so skipping is safe (contract tests run windowless).
    if (!IsWindowReady()) return false;
    return bindCachedBossTexture(texture, frameWidth, frameHeight, frameCount,
                                 path, frameW, frameH, "primary");
}

bool Boss::loadTextureAlt(const std::string& path, int frameW, int frameH) {
    if (!IsWindowReady()) return false;
    return bindCachedBossTexture(textureAlt, frameWidthAlt, frameHeightAlt, frameCountAlt,
                                 path, frameW, frameH, "alt");
}

bool Boss::loadHitPaletteTexture(const std::string& path) {
    hitPaletteTexture = nullptr;
    if (!IsWindowReady() || !texture || !texture->valid()) return false;
    const TextureResource* cached = AssetCache::loadTexture(path);
    if (!cached || !cached->valid() || cached->width() != texture->width() ||
        cached->height() != texture->height()) {
        TraceLog(LOG_WARNING, "Boss: invalid hit palette atlas '%s'", path.c_str());
        return false;
    }
    cached->setFilter(TEXTURE_FILTER_POINT);
    hitPaletteTexture = cached;
    return true;
}

// ============================================================================
// JSON-driven boss loading from KB
// ============================================================================

bool Boss::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    try {
        json j;
        file >> j;

        if (j.contains("display_name")) displayName = j["display_name"].get<std::string>();

        if (j.contains("stats")) {
            const auto& s = j["stats"];
            health    = s.value("health", 32);
            maxHealth = s.value("max_health", 32);
            contactDamage = s.value("contact_damage", 4);
            gravity       = s.value("gravity", 0.22f);
            maxFallSpeed  = s.value("max_fall_speed", 5.0f);
        }

        if (j.contains("hitbox")) {
            const auto& h = j["hitbox"];
            hitboxSize   = {static_cast<float>(h.value("width", 22)),
                            static_cast<float>(h.value("height", 28))};
            hitboxOffset = {static_cast<float>(h.value("offset_x", 4)),
                            static_cast<float>(h.value("offset_y", 4))};
        }

        if (j.contains("scripted")) isScripted_ = j["scripted"].get<bool>();

        weaknesses.clear();
        if (j.contains("weaknesses") && j["weaknesses"].is_array()) {
            for (const auto& w : j["weaknesses"]) {
                weaknesses.push_back({
                    w.value("weapon_id", ""),
                    w.value("multiplier", 1.0f)
                });
            }
        }

        if (j.contains("sprite")) {
            const auto& sp = j["sprite"];
            std::string sheetPath;
            if (sp.contains("sheet_path") && sp["sheet_path"].is_string())
                sheetPath = sp["sheet_path"].get<std::string>();
            int fw = sp.value("frame_width", 0);
            int fh = sp.value("frame_height", 0);
            if (!sheetPath.empty() && fw > 0 && fh > 0) {
                loadTexture(sheetPath, fw, fh);
            }
            if (sp.contains("hit_palette_sheet_path") &&
                sp["hit_palette_sheet_path"].is_string()) {
                loadHitPaletteTexture(sp["hit_palette_sheet_path"].get<std::string>());
            }
            std::string altPath;
            if (sp.contains("alt_sheet_path") && sp["alt_sheet_path"].is_string())
                altPath = sp["alt_sheet_path"].get<std::string>();
            int afw = sp.value("alt_frame_width", 0);
            int afh = sp.value("alt_frame_height", 0);
            if (!altPath.empty() && afw > 0 && afh > 0) {
                loadTextureAlt(altPath, afw, afh);
            }
        }

        phases.clear();
        if (j.contains("phases") && j["phases"].is_array()) {
            for (const auto& ph : j["phases"]) {
                BossPhase phase;
                phase.name = ph.value("name", "Phase");
                phase.healthThreshold = ph.value("health_threshold", 1.0f);
                phase.speedMultiplier = ph.value("speed_multiplier", 1.0f);

                if (ph.contains("attacks") && ph["attacks"].is_array()) {
                    for (const auto& a : ph["attacks"]) {
                        BossAttack atk;
                        atk.name     = a.value("name", "attack");
                        const std::string attackTypeName = a.value("type", "");
                        const auto attackType = parseAttackType(attackTypeName);
                        if (!attackType) {
                            TraceLog(LOG_ERROR,
                                     "Boss: attack '%s' in %s has unknown type '%s'",
                                     atk.name.c_str(), path.c_str(),
                                     attackTypeName.c_str());
                            return false;
                        }
                        atk.type = *attackType;
                        const int rawWeight = a.value("weight", 10);
                        if (rawWeight <= 0) {
                            TraceLog(LOG_WARNING,
                                     "Boss: attack '%s' in %s has non-positive weight %d; disabling it",
                                     atk.name.c_str(), path.c_str(), rawWeight);
                        }
                        atk.weight   = std::max(0, rawWeight);
                        atk.startup  = a.value("startup", 12);
                        atk.active   = a.value("active", 20);
                        atk.recovery = a.value("recovery", 24);
                        atk.moveSpeed        = a.value("move_speed", 3.0f);
                        atk.faceTarget       = a.value("face_target", true);
                        atk.wallBounce       = a.value("wall_bounce", false);
                        atk.invincibleDuring = a.value("invincible", false);

                        if (a.contains("projectile") && a["projectile"].is_object()) {
                            const auto& p = a["projectile"];
                            atk.projectileCount  = p.value("count", 1);
                            atk.projectileSpeed  = p.value("speed", 3.0f);
                            atk.spreadAngle      = p.value("spread_angle", 0.0f);
                            atk.projectileDamage = p.value("damage", 3);
                            atk.projectileVY     = p.value("vy", 0.0f);
                        }

                        if (a.contains("jump") && a["jump"].is_object()) {
                            const auto& jmp = a["jump"];
                            atk.jumpVelocity = jmp.value("velocity", -6.0f);
                            atk.jumpHSpeed   = jmp.value("h_speed", 2.0f);
                        }

                        phase.attacks.push_back(atk);
                    }
                }
                phases.push_back(phase);
            }
        }

        if (phases.empty()) {
            TraceLog(LOG_WARNING, "Boss: %s loaded but has no phases", path.c_str());
            return false;
        }

        TraceLog(LOG_INFO, "Boss: loaded '%s' from %s (%d phases, %d weaknesses)",
                 displayName.c_str(), path.c_str(),
                 static_cast<int>(phases.size()),
                 static_cast<int>(weaknesses.size()));
        return true;
    } catch (const std::exception& e) {
        TraceLog(LOG_WARNING, "Boss: failed to parse %s: %s", path.c_str(), e.what());
        return false;
    }
}

// ============================================================================
// Boss initialization
// ============================================================================

void Boss::init(const std::string& bossType, float x, float y) {
    type = bossType;
    position = {x, y};
    prevPosition = position;
    velocity = {0, 0};
    active = true;
    alive = true;
    dormant = true;
    isScripted_ = false;
    useTextureAlt = false;
    texture = nullptr;
    hitPaletteTexture = nullptr;
    textureAlt = nullptr;
    frameWidth = frameHeight = frameCount = 0;
    frameWidthAlt = frameHeightAlt = frameCountAlt = 0;
    phases.clear();
    weaknesses.clear();
    pendingShots.clear();
    deathBursts_.clear();
    deathWhiteFlash_ = false;
    hitFlash = 0;
    currentAttack_ = {};
    hasCurrentAttack_ = false;
    attackTimer_ = 0;
    attackFired_ = false;
    stateTimer_ = 0;
    introTimer_ = 0;
    rescueTimer_ = 0;
    deathTimer_ = 0;
    deathExplosions_ = 0;
    fightTimer = 0;
    rescueTime = 600;
    currentPhase = 0;
    arenaLeft_ = 0;
    arenaRight_ = 0;

    const std::string kbRelativePath = "mmx1/bosses/" + type + "/boss.json";
    auto kbPath = kb_paths::resolveKBPath(kbRelativePath);
    const bool loadedFromData = kbPath && loadFromFile(*kbPath);
    if (!loadedFromData) {
        if (isRetiredBossRuntimeFallback(type)) {
            const char* pathText = kbPath ? kbPath->c_str() : kbRelativePath.c_str();
            TraceLog(LOG_ERROR,
                     "Boss: retired data fallback '%s' requires %s; using diagnostic placeholder",
                     type.c_str(), pathText);
        } else {
            TraceLog(LOG_WARNING,
                     "Boss: unknown boss '%s'; using generic placeholder fallback",
                     type.c_str());
        }
        health = 32;
        maxHealth = 32;
        contactDamage = 4;
        hitboxSize = {20, 28};
        hitboxOffset = {6, 4};
        gravity = 0.22f;
        maxFallSpeed = 5.0f;
        displayName = bossType;
    }

    float hpMul = DifficultySettings::bossHPMultiplier();
    health = std::max(1, static_cast<int>(health * hpMul));
    maxHealth = std::max(1, static_cast<int>(maxHealth * hpMul));

    // U35 B5: chill-penguin runs the measured FSM regardless of whether the
    // KB JSON or the missing-data diagnostic path provided the stats.
    cpMeasured_ = (type == "chill-penguin");
    // SE-B5 E1: the death grammar generalises (SE-B8), so the death
    // table is selected independently of the CP-only FSM flag above.
    deathTimeline_ = bossDeathTimelineFor(type);
    deathPopIndex_ = 0;
    // SE-B5 E3: Storm Eagle absorbs every nonlethal hit in place
    // (fight_timeline.json). This is deliberately NOT shared with Chill
    // Penguin - SE-B3F-C proved the flinch/damage-response rule does not
    // generalise between bosses, unlike the death grammar.
    absorbsDamageInPlace_ = (type == "storm-eagle");
    // SE-B5 E6: the measured fixed fight loop, likewise SE-only. Kept as its
    // own flag rather than folded into absorbsDamageInPlace_ - they happen to
    // coincide on this boss, but one is a damage rule and the other is an FSM,
    // and SE-B3F-C is the standing warning against merging boss behaviours.
    seMeasured_ = (type == "storm-eagle");
    seStepIndex_ = 0;
    seTimer_ = 0;
    seYFp_ = 0;
    seYFpInit_ = false;
    cpState_ = CpState::None;
    cpTimer_ = 0;
    cpFirstIdle_ = true;
    cpIdleShotDone_ = false;
    cpVelFp_ = 0;
    cpStageIntroConfigured_ = false;
    cpIntroCeilingY_ = 0;
    cpDisplayHealth_ = 0;
    if (cpMeasured_) {
        contactDamage = 6;  // damage_matrix.json body contact (19 samples)
        // B7: the oracle sheet (104x72 cells, slot anchor at in-cell (44,0),
        // poses captured facing LEFT; placement pixel-verified vs the movie
        // still by test_cp_boss_sheet_placement.py). Overrides any sheet the
        // KB json bound.
        loadTexture("content/x1/sprites/bosses/chill_penguin_oracle_sheet.png",
                    104, 72);
    }

    bossState = BossState::Dormant;
    setupAnimations();
}

void Boss::setupAnimations() {
    // Hardcoded fallback for bosses without a JSON animation file
    anim_.addAnimation("idle",     {"idle",     {{0, 60}}, true});
    anim_.addAnimation("charge",   {"charge",   {{1, 4}, {2, 4}}, true});
    anim_.addAnimation("shoot",    {"shoot",    {{3, 60}}, true});
    anim_.addAnimation("slide",    {"slide",    {{8, 60}}, true});
    anim_.addAnimation("jump",     {"jump",     {{5, 60}}, true});
    anim_.addAnimation("stunned",  {"stunned",  {{4, 60}}, true});
    anim_.addAnimation("rescued",  {"rescued",  {{5, 60}}, true});
    anim_.addAnimation("dying",    {"dying",    {{4, 4}, {6, 4}}, true});
    anim_.addAnimation("intro",    {"intro",    {{0, 60}}, true});

    // Override with data-driven animations from JSON
    std::string animPath = "content/x1/characters/bosses/" + type + ".json";
    std::ifstream animFile(animPath);
    if (animFile.is_open()) {
        try {
            json j;
            animFile >> j;
            if (j.contains("animations") && j["animations"].is_object()) {
                for (auto it = j["animations"].begin(); it != j["animations"].end(); ++it) {
                    const auto& animData = it.value();
                    if (!animData.contains("frames") || !animData["frames"].is_array())
                        continue;

                    std::vector<AnimFrame> frames;
                    for (const auto& f : animData["frames"]) {
                        frames.push_back({
                            f.value("index", 0),
                            f.value("ticks", 60)
                        });
                    }
                    bool loop = animData.value("loop", true);
                    anim_.addAnimation(it.key(), {it.key(), frames, loop});
                }
            }
        } catch (const std::exception& e) {
            TraceLog(LOG_WARNING, "Boss '%s': failed to parse %s: %s",
                     type.c_str(), animPath.c_str(), e.what());
        }
    }

    anim_.play("idle");
}

bool Boss::introVisible() const {
    // C2: the source has no Storm Eagle object before the slot goes live; the
    // boss enters by flying in (boss_se_intro_timeline.h).
    if (seMeasured_) {
        if (bossState == BossState::Dormant) return false;
        if (bossState == BossState::Intro) return seIntroVisible_;
        return true;
    }
    if (!cpStageIntroConfigured_) return true;
    if (bossState == BossState::Dormant) return false;
    return bossState != BossState::Intro ||
           introSourceTick() >= boss_cp_intro_timeline::kFallTick;
}

int Boss::cpRenderFrame() const {
    if (bossState == BossState::Dying || bossState == BossState::Dead) return 10;
    if (cpStageIntroConfigured_ && bossState == BossState::Intro) {
        return boss_cp_intro_timeline::entryBossPoseFrame(introSourceTick());
    }
    const int toggle = (cpTimer_ / 2) & 1;
    switch (cpState_) {
        case CpState::IdleGate: return toggle;
        case CpState::Slide: return cpTimer_ <= 52 ? toggle : 4;
        case CpState::Volley: return cpTimer_ < 50 ? toggle : 2 + toggle;
        case CpState::BarHang: return 5;
        case CpState::Leap:
            return cpTimer_ <= 44 ? 6 : (cpTimer_ <= 94 ? 7 + toggle : 9);
        case CpState::Flinch: return 10;
        case CpState::None: return toggle;
    }
    return 0;
}

} // namespace mmx
