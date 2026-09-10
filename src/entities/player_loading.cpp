// player_loading.cpp - loads player sprite definitions and animation setup.
// Owns: player JSON parsing, muzzle offsets, and animation registration.

#include "entities/player.h"

#include "data/content_paths.h"
#include "systems/asset_cache.h"

#include <fstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace mmx {

namespace {

Vector2 readMuzzleOffset(const json& offsets, const char* key, Vector2 fallback) {
    if (!offsets.contains(key) || !offsets[key].is_object()) {
        return fallback;
    }

    const auto& value = offsets[key];
    return {
        value.value("x", fallback.x),
        value.value("y", fallback.y)
    };
}

} // namespace

bool Player::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        TraceLog(LOG_ERROR, "Failed to open character file: %s", path.c_str());
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
        // Basic physics
        runSpeed        = j.value("runSpeed", 1.46875f);  // RAM: 0x178/256 px/frame
        jumpVelocity    = j.value("jumpVelocity", -5.25f);  // measured from RAM: peak ~55px
        jumpCutVelocity = j.value("jumpCutVelocity", -1.5f);
        gravity         = j.value("gravity", 0.25f);       // measured from RAM: ~0.25 px/f^2 (MMX 0x40)
        maxFallSpeed    = j.value("maxFallSpeed", 5.75f);  // RAM-measured terminal free-fall ~5.75 px/f (MMX 0x5C0)

        // Wall mechanics
        wallSlideSpeed       = j.value("wallSlideSpeed", 2.0f);  // RAM-measured: pinned wall descent caps at exactly 2.0 px/f
        // wall_jump_release.json: controlled source observations pin six
        // stationary updates, seven away steps and a stationary handoff.
        // The first moving dy is -1299/256 after the external gravity tick.
        wallJumpVelocityX    = j.value("wallJumpVelocityX", 1.46875f);
        wallJumpVelocityY    = j.value("wallJumpVelocityY", -1363.0f / 256.0f);
        wallJumpLockoutFrames = j.value("wallJumpLockoutFrames", 7);

        // Dash
        dashSpeed    = j.value("dashSpeed", 3.45703125f);  // RAM: 0x375/256 px/frame
        dashDuration = j.value("dashDuration", 33);

        // Feel
        coyoteFrames     = j.value("coyoteFrames", 4);
        jumpBufferFrames = j.value("jumpBufferFrames", 6);

        // Combat
        shotSpeed  = j.value("shotSpeed", 4.0f);
        chargeTime1 = j.value("chargeTime1", 31);  // blue L1 catch (APU 0x04)
        chargeTime2 = j.value("chargeTime2", 101); // green L2 full buster
        chargeTime3 = j.value("chargeTime3", 179); // arm-upgrade pink L3
        chargeTimeSpecial = j.value("chargeTimeSpecial", 179);  // oracle bisect, see player.h

        // Damage
        hurtKnockbackX   = j.value("hurtKnockbackX", 138.0f / 256.0f);
        hurtKnockbackY   = j.value("hurtKnockbackY", -1.0f);
        hurtDuration      = j.value("hurtDuration", 29);
        iframeDuration    = j.value("iframeDuration", 91);

        if (j.contains("armorDeltas") && j["armorDeltas"].is_object()) {
            armorDeltaPaths_.clear();
            for (auto it = j["armorDeltas"].begin(); it != j["armorDeltas"].end(); ++it) {
                if (!it.value().is_string()) continue;
                auto resolvedDeltaPath = content_paths::resolveAssetPath(it.value().get<std::string>());
                if (resolvedDeltaPath) {
                    armorDeltaPaths_[it.key()] = *resolvedDeltaPath;
                } else {
                    TraceLog(LOG_WARNING, "Rejected unsafe armor delta path for %s",
                             it.key().c_str());
                }
            }
        }

        // Upgrades — sync from persistent armor state. U66 census: dash
        // exists ONLY with the legs capsule; a config "hasBoots": true is an
        // explicit override for test/extra stages, never the default.
        cfgForceBoots_ = j.value("hasBoots", false);
        applyArmorState();

        // Hitbox
        hitboxSize.x   = j.value("hitboxWidth", 14.0f);
        hitboxSize.y   = j.value("hitboxHeight", 24.0f);
        hitboxOffset.x = j.value("hitboxOffsetX", 9.0f);
        hitboxOffset.y = j.value("hitboxOffsetY", 6.0f);

        // Sprite
        spriteWidth  = j.value("spriteWidth", 32.0f);
        spriteHeight = j.value("spriteHeight", 32.0f);
        if (j.contains("busterMuzzleOffsets") && j["busterMuzzleOffsets"].is_object()) {
            const auto& offsets = j["busterMuzzleOffsets"];
            busterMuzzleIdle = readMuzzleOffset(offsets, "idle", busterMuzzleIdle);
            busterMuzzleRun = readMuzzleOffset(offsets, "run", busterMuzzleRun);
            busterMuzzleJump = readMuzzleOffset(offsets, "jump", busterMuzzleJump);
            busterMuzzleDash = readMuzzleOffset(offsets, "dash", busterMuzzleDash);
            busterMuzzleWall = readMuzzleOffset(offsets, "wall", busterMuzzleWall);
        }
        busterMuzzleGap = j.value("busterMuzzleGap", 0.0f);
        visualGroundingOffsetY = j.value("visualGroundingOffsetY", 0.0f);
        setSpriteSheetResource(nullptr);
        spriteSourcePath_.clear();
        weaponSpriteVariants_.clear();
        compositeSheetTexture_.reset();
        sheetComposer_ = XSheetComposer{};
        armorCompositeKey_.clear();

        if (j.contains("spritePath")) {
            std::string spritePath = j["spritePath"];
            auto resolvedSpritePath = content_paths::resolveAssetPath(spritePath);
            if (!resolvedSpritePath) {
                TraceLog(LOG_ERROR, "Rejected unsafe player sprite path in %s: %s",
                         path.c_str(), spritePath.c_str());
                return false;
            }
            spriteSourcePath_ = *resolvedSpritePath;
            setSpriteSheetResource(AssetCache::loadTexture(*resolvedSpritePath));
            activeSheetPath_ = spriteSourcePath_;
            if (auto palPath = content_paths::x1PalettePath("x_palettes.json")) {
                if (!xPalette_.load(*palPath)) {
                    TraceLog(LOG_WARNING, "Player: failed to load X palette table: %s", palPath->c_str());
                } else {
                    TraceLog(LOG_INFO, "Player: loaded X palette table (%s)", palPath->c_str());
                }
            }
            // Indexed recolor (the endgame: real palette-index mechanism;
            // oracle acceptance 0-mismatch for all 9 weapons — see
            // x_palette.h). Legacy LUT above stays the no-data fallback.
            {
                auto idxPath = content_paths::x1SpritePath("x_spritesheet_index.png");
                auto rowsPath = content_paths::x1PalettePath("x_weapon_rows.json");
                if (idxPath && rowsPath) {
                    if (xPalette_.loadIndexed(*idxPath, *rowsPath)) {
                        TraceLog(LOG_INFO, "Player: indexed X recolor active");
                    } else {
                        TraceLog(LOG_WARNING, "Player: indexed X recolor unavailable, using LUT");
                    }
                    if (sheetComposer_.load(*resolvedSpritePath, *idxPath)) {
                        for (const auto& [piece, deltaPath] : armorDeltaPaths_) {
                            if (!sheetComposer_.loadDelta(piece, deltaPath)) {
                                TraceLog(LOG_WARNING, "Player: failed to load armor delta %s: %s",
                                         piece.c_str(), deltaPath.c_str());
                            }
                        }
                    } else {
                        TraceLog(LOG_WARNING, "Player: X sheet composer unavailable");
                    }
                }
            }
            refreshArmorSheet();
        }

        // Health
        auto& playerProgress = progressState();
        if (!playerProgress.persistentStateInitialized) {
            playerProgress.maxHealth = j.value("maxHealth", 16);
            playerProgress.persistentStateInitialized = true;
        }
        maxHealth = playerProgress.maxHealth;
        health = playerProgress.maxHealth;

        // Initialize weapon inventory (buster always available)
        // Only init if empty to persist weapons between stages
        if (weaponInventory.empty()) {
            weaponInventory.init();
        }

        // A Player can be reused across stage/config loads. Scene ownership
        // must opt into source ground movement again after a successful load;
        // a prior stage's explicit policy may not leak into the new one.
        sourceGroundMovementEnabled_ = false;
        resetJumpState();
        setupAnimations();

        TraceLog(LOG_INFO, "Loaded character '%s': run=%.1f jump=%.1f dash=%.1f grav=%.2f boots=%s",
                 j.value("name", "?").c_str(), runSpeed, jumpVelocity, dashSpeed, gravity,
                 hasBoots ? "yes" : "no");
        return true;
    } catch (const json::exception& e) {
        TraceLog(LOG_ERROR, "Invalid character schema in %s: %s", path.c_str(), e.what());
        return false;
    }
}


void Player::setupAnimations() {
    // DD-packed spritesheet (10 cols, 70x70 frames). Frame indices come from
    // tools/build_x_spritesheet.py — kept in sync via x_spritesheet_meta.json.
    //
    //   idle          0..5   (6)   10 ticks
    //   walk          6..16  (11)   4 ticks
    //   walk_shoot   17..27  (11)   4 ticks
    //   dash         28..29  (2)    3 ticks
    //   dash_shoot   30..31  (2)    3 ticks
    //   jump         32..34  (3)    launch, then hold the ascending pose
    //   fall         35..36  (2)    descent, then hold
    //   land         38      (1)    short recovery visual
    //   jump_shoot   39..41  (3)    shooting ascent / launch
    //   fall_shoot   42..43  (2)    shooting descent, then hold
    //   land_shoot   45      (1)    short recovery visual
    //   wall         46..48  (3)    entry, then hold the sliding pose
    //   wall_shoot   51..53  (3)    entry, then hold the shooting slide
    //   wall_kick_shoot 54..55 (2)  kick preparation / motion only
    //   crouch       56..57  (2)    4 ticks
    //   crouch_shoot 58..59  (2)    4 ticks
    //   shoot        60..61  (2)    4 ticks
    //
    // Fall holds frame 37. Frame 38 is the landing/recovery pose; keeping it
    // out of the air arc prevents X from snapping upright before he touches
    // down.
    // Hurt cells80..108 are the R244 original OAM layouts, captured after
    // each armor mask's stage-entry CHR reload. The compositor preserves the
    // recoil and impact flashes under every weapon palette.
    // Death sprite art task: DESLOPPIFY N4.

    // Real MMX idle is a STATIC pose at full HP (verified against the savestate —
    // X does not animate while standing). Frames 4-5 (mouth open) are the
    // heavy-breathing idle that only plays when health is low ("idle_low").
    anim_.addAnimation("idle",     {"idle",     {{0, 10}}, true});
    anim_.addAnimation("idle_low", {"idle_low", {{0, 10}, {1, 10}, {2, 10}, {3, 10}, {4, 10}, {5, 10}}, true});
    // Run: steady stride loops frames 7-16 with the real game's per-pose
    // durations. Frame 6 is excluded here; using it as a push-off made short
    // taps look like X slid before the walk cycle began.
    anim_.addAnimation("run",  {"run",  {{7, 2}, {8, 2}, {9, 3}, {10, 3}, {11, 3},
                                         {12, 2}, {13, 2}, {14, 3}, {15, 3}, {16, 3}}, true});
    // walk_shoot mirrors the run stride (frames 18-27 = same legs as run 7-16,
    // arm extended) with matching durations, so toggling shoot mid-stride keeps
    // the legs in phase. Frame 17 is the push-off-shoot, excluded from the loop.
    anim_.addAnimation("walk_shoot", {"walk_shoot", {{18, 2}, {19, 2}, {20, 3}, {21, 3}, {22, 3},
                                                     {23, 2}, {24, 2}, {25, 3}, {26, 3}, {27, 3}}, true});
    // R244 locomotion_visual:28 is startup,29 is the stable body, with
    // the separate source dust excluded. Preserve the existing frame clock.
    anim_.addAnimation("dash",       {"dash",       {{28, 3}, {29, 60}}, false});
    anim_.addAnimation("dash_shoot", {"dash_shoot", {{30, 3}, {31, 60}}, false});
    // R291: _xpose_runs/jump/poses.csv keeps anim196/cell34 while B is
    // held. Cell35 is anim212 after release and belongs to descent.
    anim_.addAnimation("jump",       {"jump",       {{32, 4}, {33, 4}, {34, 4}}, false});
    anim_.addAnimation("land",       {"land",       {{38, 3}}, false});
    anim_.addAnimation("jump_shoot", {"jump_shoot", {{39, 4}, {40, 4}, {41, 4}}, false});
    anim_.addAnimation("fall_shoot", {"fall_shoot", {{42, 4}, {43, 60}}, false});
    anim_.addAnimation("land_shoot", {"land_shoot", {{45, 3}}, false});
    // R299: source jump/jumpshoot f20/24 descend;37/44 are floor contact.
    anim_.addAnimation("fall",       {"fall",       {{35, 4}, {36, 60}}, false});
    // R291: _xpose_runs/wallslide/poses.csv holds anim163/cell48.
    // wallclimb f33/36 only selects cells49/50 after B starts a wall kick.
    anim_.addAnimation("wall",       {"wall",       {{46, 6}, {47, 6}, {48, 6}}, false});
    anim_.addAnimation("wall_kick",  {"wall_kick",  {{49, 4}, {50, 60}}, false});
    // R356: source176/179/182 are shooting slide poses;168/171 belong
    // only to the kick. Match the plain-wall phase when toggling the shot.
    anim_.addAnimation("wall_shoot", {"wall_shoot", {{51, 6}, {52, 6}, {53, 6}}, false});
    anim_.addAnimation("wall_kick_shoot", {"wall_kick_shoot", {{54, 4}, {55, 60}}, false});
    anim_.addAnimation("crouch",       {"crouch",       {{56, 4}, {57, 4}}, true});
    anim_.addAnimation("crouch_shoot", {"crouch_shoot", {{58, 4}, {59, 4}}, true});
    anim_.addAnimation("shoot",        {"shoot",        {{60, 4}, {61, 4}}, false});
    // See knowledge_base/mmx1/player/hurt_visual.json. These visual ticks
    // preserve the separately measured 29-tick reaction/28-step knockback.
    Animation hurt{"hurt", {}, false};
    for (int frame = 80; frame <= 108; ++frame) hurt.frames.push_back({frame, 1});
    anim_.addAnimation("hurt", std::move(hurt));
    anim_.addAnimation("die",  {"die",  {{0, 10}, {38, 10}, {0, 8}}, false});
    anim_.play("idle");
}


} // namespace mmx
