// gameplay_scene.cpp - orchestrates stage gameplay simulation and rendering.
// Owns: scene lifetime, stage runtime state, lane ordering, and handoffs.

#include "gameplay_scene.h"
#include "gameplay/parity_trace.h"
#include "gameplay/gameplay_boss.h"
#include "gameplay/gameplay_game_over.h"
#include "gameplay/gameplay_presentation.h"
#include "gameplay/gameplay_death_orb_assets.h"
#include "gameplay/gameplay_projectiles.h"
#include "gameplay/gameplay_scene_trace.h"
#include "gameplay/gameplay_stage_clear.h"
#include "gameplay/stage_parity_transitions.h"
#include "gameplay/cp_source_obj_foreground.h"
#include "gameplay/gameplay_player_bounds.h"
#include "gameplay/gameplay_pit_death.h"
#include "gameplay/gameplay_enemies.h"
#include "gameplay/gameplay_enemy_contact.h"
#include "entities/player_anchor.h"
#include "entities/boss_cp_intro_timeline.h"
#include "app/scene_manager.h"
#include "ui/title_scene.h"
#include "ui/stage_select_scene.h"
#include "ui/ending_scene.h"
#include "physics/collision.h"
#include "physics/projectile_collision.h"
#include "entities/enemy_damage.h"
#include "systems/asset_cache.h"
#include "systems/audio.h"
#include "data/save_system.h"
#include "data/difficulty.h"
#include "data/settings.h"
#include "data/stage_identity.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <memory>
#include <utility>

namespace mmx {
using gameplay_scene_trace::jsonQuoted;
using gameplay_scene_trace::writeIntArray;
using gameplay_scene_trace::writeSourceObjForegroundTrace;
using gameplay_scene_trace::writeStringArray;

namespace {

std::string pickupPersistentId(StageId stageId, size_t ordinal, const SpawnPoint& spawn) {
    return stageId.str() + ":pickup:" +
           std::to_string(ordinal) + ":" +
           spawn.id + ":" +
           std::to_string(static_cast<int>(std::lround(spawn.x))) + ":" +
           std::to_string(static_cast<int>(std::lround(spawn.y)));
}

std::string legacyPickupPersistentId(const std::string& stagePath, const SpawnPoint& spawn) {
    return stagePath + ":" +
           std::to_string(static_cast<int>(std::lround(spawn.x))) + ":" +
           std::to_string(static_cast<int>(std::lround(spawn.y)));
}

bool isProjectileFloorSupport(TileType type) {
    return type == TileType::Solid || type == TileType::OneWay ||
           type == TileType::SlopeL || type == TileType::SlopeR ||
           type == TileType::Spike || type == TileType::Breakable ||
           type == TileType::Conveyor;
}

constexpr int kCpEntryShutterMinCol = 479;
constexpr int kCpEntryShutterMaxCol = 480;
constexpr int kCpEntryShutterMinRow = 23;
constexpr int kCpEntryShutterMaxRow = 25;

constexpr std::array<std::uint8_t, 10> kStormEaglePlatformFlags = {
    137, 136, 135, 134, 5, 4, 3, 2, 1, 0};

std::uint16_t sourceCameraCoordinate(float value) {
    if (!std::isfinite(value) || value <= 0.0f) return 0;
    if (value >= 65535.0f) return 65535;
    return static_cast<std::uint16_t>(std::floor(value));
}

bool parseAutotestFloat(const char* value, float& out) {
    if (!value || *value == '\0') return false;
    char* end = nullptr;
    const float parsed = std::strtof(value, &end);
    if (end == value || *end != '\0' || !std::isfinite(parsed)) return false;
    out = parsed;
    return true;
}
} // namespace

void GameplayScene::setArenaWave(std::vector<SpawnPoint> enemies,
                                  std::optional<SpawnPoint> boss) {
    arenaWaveMode_ = true;
    arenaWaveEnemies_ = std::move(enemies);
    arenaWaveBoss_ = std::move(boss);
}

void GameplayScene::configureStormEaglePlatforms() {
    const StormEaglePlatformSceneConfig config{
        activeStageId_.str(), stagePath, randomizerMode, arenaWaveMode_,
        bossRushMode};
    for (auto& platform : stormEaglePlatforms_) {
        platform.configure(config);
    }
    stormEagleAdvanceThisFrame_.fill(false);
    stormEaglePlatformFrameActive_ = false;
}

bool GameplayScene::prepareStormEaglePlatformFrame() {
    const StormEaglePlatformSceneConfig config{
        activeStageId_.str(), stagePath, randomizerMode, arenaWaveMode_,
        bossRushMode};
    if (!StormEaglePlatformScene::canonicalScope(config)) {
        stormEagleAdvanceThisFrame_.fill(false);
        stormEaglePlatformFrameActive_ = false;
        player_.clearStormEagleSupport();
        return false;
    }

    // This pre-action seam exposes only the aggregate prior support. Source
    // allocation/initialization is deliberately deferred until the post-
    // physics object lane, so the trigger tick cannot also move the record.
    bool previousSupport = false;
    stormEagleAdvanceThisFrame_.fill(false);
    for (const auto& platform : stormEaglePlatforms_) {
        previousSupport = previousSupport ||
                          platform.previousSupport();
    }
    stormEaglePlatformFrameActive_ = true;
    player_.beginStormEagleSupportFrame(previousSupport);
    return true;
}

void GameplayScene::updateStormEaglePlatforms() {
    if (!stormEaglePlatformFrameActive_) return;

    const auto cameraX = sourceCameraCoordinate(camera_.baseX());
    const auto cameraY = sourceCameraCoordinate(camera_.baseY());
    const float sourceAnchor = player_anchor::sourceRamAnchorX(
        player_.position.x, player_.spriteWidth, player_.facingRight);
    const auto sourceX = static_cast<std::uint16_t>(std::clamp(
        static_cast<int>(std::floor(sourceAnchor)), 0, 65535));
    bool currentSupport = false;
    for (std::size_t i = 0; i < stormEaglePlatforms_.size(); ++i) {
        auto& platform = stormEaglePlatforms_[i];
        stormEagleAdvanceThisFrame_[i] = platform.prepareSourceFrame(
            sourceX, kStormEaglePlatformFlags[i]);
        if (stormEagleAdvanceThisFrame_[i]) {
            platform.tickMotion();
        }
        if (platform.lifecycle() == StormEaglePlatformScene::Lifecycle::Running) {
            const bool contact = platform.resolvePlayerContact(
                player_, cameraX, cameraY);
            currentSupport = currentSupport || contact;
            if (contact) player_.onGround = true;
            platform.finishContact(contact);
            platform.finishSourceFrame(cameraX, cameraY);
        }
    }
    player_.finishStormEagleSupport(currentSupport);
    stormEaglePlatformFrameActive_ = false;
    stormEagleAdvanceThisFrame_.fill(false);
}

void GameplayScene::renderStormEaglePlatforms(float alpha, float cameraX,
                                               float cameraY) {
    for (auto& platform : stormEaglePlatforms_) {
        platform.render(cameraX, cameraY, alpha);
    }
}

void GameplayScene::onEnter() {
    activeStageId_ = stageId.empty() ? stage_identity::fromPath(stagePath) : stageId;
    parityRoomTransitionTimer_ = 0;
    cpCapsule_.reset();
    stormEagleCapsule_.reset();
    stingChameleonCapsule_.reset();
    stormEagleCascade_.reset();
    stormEagleCascadeStarted_ = false;

    autotestSourceObjForegroundTarget_.clear();
    if (const char* sourceObjTarget = std::getenv("MMX_AUTOTEST_SOURCE_OBJ_FOREGROUND_TARGET")) {
        if (*sourceObjTarget) {
            autotestSourceObjForegroundTarget_ = sourceObjTarget;
        }
    }

    hud_.loadSprites();  // Load MMX-style HUD sprite
    gameplay_death_orbs::loadSpriteAssets(deathOrbState_);
    if (!tilemap_.loadFromFile(stagePath)) {
        TraceLog(LOG_ERROR, "GameplayScene could not load stage: %s", stagePath.c_str());
        returnToStageSelect_ = true;
        return;
    }
    tilemap_.applyVariants(stagePath);
    setupCpEntryStagePresentation();
    setupStormEagleDestructibles();
    configureStormEaglePlatforms();
    if (sceneManager_) {
        player_.weaponInventory.bindState(sceneManager_->runtimeState().weaponInventory);
    }
    if (!player_.loadFromFile(characterPath)) {
        TraceLog(LOG_ERROR, "GameplayScene could not load character: %s", characterPath.c_str());
        returnToTitle_ = true;
        return;
    }
    setupCpEntryHitboxRollout();
    // R134 source-ground policy belongs to canonical X in the authored CP
    // stage. Attaching a map to a custom Player does not opt into this law.
    player_.setTilemap(&tilemap_,
        characterPath == "content/x1/characters/x.json" &&
        stagePath == "content/x1/stages/tiles/chill-penguin_full.json" &&
        activeStageId_.str() == "chill-penguin" &&
        !randomizerMode && !arenaWaveMode_ && !bossRushMode);
    if (grantAllWeaponsForAutotest) {
        grantAllWeaponsForAutotestRun();
    }
    // Task-17 ghost trace (guarded: onEnter re-runs on restart).
    if (!projTrace_) {
        if (const char* tracePath = std::getenv("MMX_PROJTRACE")) {
            if (*tracePath) {
                projTrace_ = std::fopen(tracePath, "w");
                if (projTrace_) {
                    std::fprintf(projTrace_, "tick,kind,serial,weapon,charged,fragment,x,y,vx,vy\n");
                }
            }
        }
    }
    if (!parityTrace_) {
        if (const char* tracePath = std::getenv("MMX_PARITY_TRACE")) {
            if (*tracePath) {
                parityTrace_ = std::fopen(tracePath, "w");
                if (parityTrace_) {
                    std::fprintf(parityTrace_, "%s\n", parity_trace::kHeader);
                    parityTraceTick_ = 0;
                    parityApuLogIndex_ = AudioManager::apuLog().size();
                    paritySfxLogIndex_ = AudioManager::sfxLog().size();
                }
            }
        }
    }
    if (!visualCompositionTrace_) {
        if (const char* tracePath = std::getenv("MMX_VISUAL_COMPOSITION_TRACE")) {
            if (*tracePath) {
                visualCompositionTrace_ = std::fopen(tracePath, "w");
                visualCompositionTraceFrame_ = 0;
                visualCompositionTraceTargetFrame_ = 0;
                if (const char* frameValue = std::getenv("MMX_VISUAL_COMPOSITION_TRACE_FRAME")) {
                    char* end = nullptr;
                    const long parsed = std::strtol(frameValue, &end, 10);
                    if (end != frameValue && *end == '\0' && parsed > 0) {
                        visualCompositionTraceTargetFrame_ = parsed;
                    }
                }
            }
        }
    }
    player_.projectiles = &projectiles_;
    player_.lives = DifficultySettings::startingReserveLives();

    Enemy::loadDefinitions("content/x1/enemies/enemy_defs.json");

    defaultSpawn_ = {0, 0};
    bool foundDefaultSpawn = false;
    for (const auto& sp : tilemap_.spawns()) {
        if (sp.type == "player_spawn") {
            defaultSpawn_ = {sp.x, sp.y};
            foundDefaultSpawn = true;
            break;
        }
    }

    // Place player at spawn (or override, if set via --spawn-at)
    if (hasSpawnOverride) {
        player_.position = spawnOverride;
        player_.prevPosition = player_.position;
        checkpoint_ = player_.position;
    } else {
        if (foundDefaultSpawn) {
            player_.position = defaultSpawn_;
            player_.prevPosition = player_.position;
            checkpoint_ = player_.position;
        }
    }
    if (!foundDefaultSpawn) {
        defaultSpawn_ = player_.position;
    }

    hasAutotestPlayerPositionLock_ = false;
    float lockedPlayerX = 0.0f;
    float lockedPlayerY = 0.0f;
    const bool hasLockedPlayerX = parseAutotestFloat(
        std::getenv("MMX_AUTOTEST_PLAYER_X"), lockedPlayerX);
    const bool hasLockedPlayerY = parseAutotestFloat(
        std::getenv("MMX_AUTOTEST_PLAYER_Y"), lockedPlayerY);
    if (hasLockedPlayerX && hasLockedPlayerY) {
        hasAutotestPlayerPositionLock_ = true;
        autotestPlayerPositionLock_ = {lockedPlayerX, lockedPlayerY};
        applyAutotestPlayerPositionLock();
    }

    if (!hasSpawnOverride && foundDefaultSpawn) {
        defaultSpawn_ = resolveRespawnPoint(defaultSpawn_);
        checkpoint_ = defaultSpawn_;
        player_.respawnAt(defaultSpawn_, 0, isRespawnPointSafe(defaultSpawn_));
    }

    // Apply randomizer shuffles if enabled
    if (randomizerMode) {
        Randomizer rng(randomizerSeed);
        RandomizerConfig stageRandomizerConfig = randomizerConfig;
        if (stageRandomizerConfig.enemyTypePool.empty()) {
            stageRandomizerConfig.enemyTypePool.reserve(Enemy::definitions.size());
            for (const auto& [enemyType, _] : Enemy::definitions) {
                stageRandomizerConfig.enemyTypePool.push_back(enemyType);
            }
            std::sort(stageRandomizerConfig.enemyTypePool.begin(), stageRandomizerConfig.enemyTypePool.end());
        }
        rng.setConfig(stageRandomizerConfig);
        // Convert tilemap spawns to shuffle entries, shuffle, then replace
        // the tilemap-owned list through its explicit mutation boundary.
        auto spawns = tilemap_.spawns();
        std::vector<SpawnShuffleEntry> entries;
        for (const auto& sp : spawns) {
            entries.push_back({sp.type, sp.id, sp.x, sp.y});
        }
        rng.shuffleSpawns(entries, activeStageId_.str());
        rng.applyBossAssignment(entries, activeStageId_);
        for (size_t i = 0; i < entries.size() && i < spawns.size(); i++) {
            spawns[i].type = entries[i].type;
            spawns[i].id = entries[i].id;
            spawns[i].x = entries[i].x;
            spawns[i].y = entries[i].y;
        }
        tilemap_.replaceSpawns(std::move(spawns));
    }

    // Spawn enemies and load checkpoints from stage data. Arena waves replace
    // hostile spawns but keep checkpoints/player spawn from the loaded arena.
    spawnEnemies();

    // Load checkpoints
    for (const auto& sp : tilemap_.spawns()) {
        if (sp.type == "checkpoint") {
            checkpoints_.push_back({sp.x, {sp.x, sp.y}, false});
        }
    }

    // Spawn boss if present (any boss type in spawn data), or the arena wave's
    // generated boss. Arena bosses activate immediately because sandbox arenas
    // do not carry named boss-room trigger data.
    if (arenaWaveMode_ && arenaWaveBoss_.has_value()) {
        initBossFromSpawn(*arenaWaveBoss_);
    } else {
        for (const auto& sp : tilemap_.spawns()) {
            if (sp.type == "boss" && !sp.id.empty()) {
                initBossFromSpawn(sp);
                break;
            }
        }
    }

    seedCpBossEntryAutotest();

    camera_.setWorldBounds(
        static_cast<float>(tilemap_.pixelWidth()),
        static_cast<float>(tilemap_.pixelHeight())
    );
    // Apply the per-room camera bounds before the initial snap so the spawn
    // frame is already framed correctly (snapTo clamps to the active room).
    updateCameraRoom();
    camera_.snapTo(
        playerCameraAnchorX(),
        player_.position.y + player_.spriteHeight / 2
    );
    if (bossEntryCinematicActive_ && boss_.cpStageIntroConfigured()) {
        camera_.setClampSuppressed(true);
        camera_.setBaseXY(
            bossEntryCamLockX_ + boss_cp_intro_timeline::entryCameraOffsetPx(
                boss_cp_intro_timeline::kSourceSpawnTick),
            static_cast<float>(boss_cp_intro_timeline::kEntryCameraYPx));
    }

    if (arenaWaveMode_ && arenaWaveBoss_.has_value()) {
        activateArenaWaveBoss();
    } else if (bossLocked_) {
        AudioManager::playBGM("boss");
    } else {
        // Stage-local systems use the selected stage slot. Boss rewards use the
        // defeated boss id, which may intentionally diverge under boss shuffles.
        AudioManager::playBGM(activeStageId_.str());
    }

    stageName_ = stage_identity::displayName(activeStageId_);
    stageTimer_ = 0;
    if (const char* hpValue = std::getenv("MMX_AUTOTEST_PLAYER_HP")) {
        if (*hpValue) {
            char* end = nullptr;
            const long parsed = std::strtol(hpValue, &end, 10);
            if (end != hpValue && *end == '\0') {
                const long maxHealth = static_cast<long>(player_.progressState().maxHealth);
                const long clamped = std::clamp(parsed, 0L, maxHealth);
                player_.health = static_cast<int>(clamped);
            }
        }
    }
    if (const char* livesValue = std::getenv("MMX_AUTOTEST_PLAYER_LIVES")) {
        if (*livesValue) {
            char* end = nullptr;
            const long parsed = std::strtol(livesValue, &end, 10);
            if (end != livesValue && *end == '\0') {
                player_.lives = static_cast<int>(std::clamp(parsed, 0L, 99L));
            }
        }
    }
    hud_.update(player_.health, player_.progressState().maxHealth, player_.lives);
    hud_.syncDisplay();

    // Test hook: force the warp-in for headless verification of the materialize
    // effect (the real flow sets playWarpIn from boss-intro / stage-select).
    if (std::getenv("MEGAMAN_X_FORCE_WARP")) playWarpIn = true;
    stageStartSequence_ = playWarpIn;
    stageStartTick_ = -1;
    // The warp-in plays on the fully-visible stage (matching the real game), so
    // skip the black fade-in when warping — otherwise it darkens the materialize.
    fadeInTimer_ = (playWarpIn || skipInitialFade) ? 0 : FADE_IN_DURATION;
    warpInTimer_ = 0;
    gameOverGridSheet_ =
        AssetCache::loadTexture("content/x1/sprites/misc/mmx1_password.gif");
    if (gameOverGridSheet_) {
        gameOverGridSheet_->setFilter(TEXTURE_FILTER_POINT);
    }
    if (playWarpIn) {
        warpCapsuleTex_ = gameplay_presentation::loadPointSourceTexture(
            "content/x1/sprites/x_warp_capsule.png");
        warpMaterializeTex_ = gameplay_presentation::loadPointSourceTexture(
            "content/x1/sprites/x_warp_materialize.png");
        warpInPosition_ = player_.position;
        // The safe-respawn search accepts a flat support up to1 pixel below
        // the hitbox. Place this presentation at that surface so its final
        // canvas joins the standing player without the preparatory gap.
        // R336 source/native join; no player position or collision changes.
        const AABB hb = player_.getHitbox();
        const int ts = tilemap_.tileSize();
        if (ts > 0 && player_.onGround) {
            const int row = static_cast<int>(std::floor((hb.bottom() + 1.0f) / ts));
            const int col = static_cast<int>(std::floor((hb.left() + hb.right()) * 0.5f / ts));
            const TileType type = tilemap_.getTileType(col, row);
            const float gap = static_cast<float>(row * ts) - hb.bottom();
            if (gap >= 0.0f && gap <= 1.0f &&
                (type == TileType::Solid || type == TileType::OneWay ||
                 type == TileType::Breakable || type == TileType::Conveyor)) {
                warpInPosition_.y += gap;
            }
        }
        stageReadyTex_ = gameplay_presentation::loadPointSourceTexture(
            "content/x1/sprites/misc/stage_ready.png");
    }

    if (autotestWeaponGetSequence) startWeaponGetSequenceForCapture();
    if (skipInitialFade && bossEntryCinematicActive_ && parityTrace_) {
        // The first rendered boss-entry frame is source t84. Record that
        // initialized state before the regular update loop advances to t85,
        // keeping both the trace and f(t-83) screenshot mapping exact.
        writeParityTrace();
    }
}

void GameplayScene::applyAutotestPlayerPositionLock() {
    if (!hasAutotestPlayerPositionLock_) return;
    player_.position = autotestPlayerPositionLock_;
    player_.prevPosition = player_.position;
    player_.velocity = {0.0f, 0.0f};
}

void GameplayScene::grantAllWeaponsForAutotestRun(bool grantBusterArmor) {
    // Keep screenshot order aligned with weapon_palette_rows.json cursor order.
    static constexpr const char* kReferenceWeaponOrder[] = {
        "homing-torpedo",
        "chameleon-sting",
        "rolling-shield",
        "fire-wave",
        "storm-tornado",
        "electric-spark",
        "boomerang-cutter",
        "shotgun-ice",
    };

    player_.weaponInventory.init();
    for (const char* weaponId : kReferenceWeaponOrder) {
        auto weapon = weapons::makeById(WeaponId::fromString(weaponId));
        if (!weapon) {
            TraceLog(LOG_WARNING, "Autotest: missing weapon id %s",
                     weaponId);
            continue;
        }
        player_.weaponInventory.addWeapon(*weapon);
    }
    for (size_t i = 0; i < player_.weaponInventory.weaponCount(); ++i) {
        player_.weaponInventory.setAmmo(i, player_.weaponInventory.weaponAt(i).maxAmmo);
    }
    player_.weaponInventory.currentIndex = 0;
    if (const char* slot = std::getenv("MMX_AUTOTEST_WEAPON_SLOT")) {
        if (*slot) {
            char* end = nullptr;
            const long parsed = std::strtol(slot, &end, 10);
            if (end != slot && *end == '\0') {
                const int maxIndex = static_cast<int>(player_.weaponInventory.weaponCount()) - 1;
                player_.weaponInventory.currentIndex =
                    static_cast<int>(std::clamp<long>(parsed, 0, maxIndex));
            }
        }
    }
    // Specials can only CHARGE with the arm upgrade (oracle-verified real-game
    // Autotest profiles match the all-weapons fixture and need the real arm
    // upgrade. Manual F4 review must not mutate armor sprites, so it uses a
    // scene-local special-charge override instead.
    if (grantBusterArmor) {
        player_.grantArmorBuster();
        player_.setDebugSpecialChargeUnlocked(false);
    } else {
        player_.setDebugSpecialChargeUnlocked(true);
    }
    TraceLog(LOG_INFO, "Autotest: granted %zu weapons for weapon-cycle profile",
             player_.weaponInventory.weaponCount());
}

void GameplayScene::onExit() {
    if (parityTrace_) {
        std::fclose(parityTrace_);
        parityTrace_ = nullptr;
    }
    if (visualCompositionTrace_) {
        std::fclose(visualCompositionTrace_);
        visualCompositionTrace_ = nullptr;
    }
    gameplay_death_orbs::clearBorrowedSpriteSheet(deathOrbState_);
    gameplay_death_orbs::clearLiveOrbs(deathOrbState_);
    deathOrbState_.previousDeathTimer = -1;
    cpCapsule_.reset();
    stormEagleCapsule_.reset();
    stingChameleonCapsule_.reset();
    for (auto& platform : stormEaglePlatforms_) platform.reset();
    stormEagleAdvanceThisFrame_.fill(false);
    stormEaglePlatformFrameActive_ = false;
}

void GameplayScene::spawnEnemyFromSpawn(const SpawnPoint& sp) {
    if (sp.type != "enemy" || sp.id.empty()) return;

    Enemy e;
    e.init(sp.id, sp.x, sp.y);
    if (sp.id == "placeholder") {
        e.setPlaceholder(sp.oid, sp.hp);
    }
    // Source CP opening capture: the right-hand walker is drawn four pixels
    // higher at the snow seam than the shared body anchor.  Keep this a
    // render-only correction so its source-authored hitbox and movement stay
    // intact.  The left walker remains on the shared definition path.
    if (activeStageId_.str() == "chill-penguin" && sp.id == "walker" &&
        std::fabs(sp.x - 579.0f) < 0.1f && std::fabs(sp.y - 1167.0f) < 0.1f) {
        e.setBodyVisualOffsetY(-4.0f);
    }
    e.setTarget(&player_);
    e.setTilemap(&tilemap_);
    enemies_.push_back(std::move(e));
}

void GameplayScene::spawnStageObjectFromSpawn(const SpawnPoint& sp) {
    if (!StageObject::isStageObjectSpawn(sp)) return;

    StageObject obj;
    obj.init(sp.id, sp.x, sp.y);
    if (obj.active) {
        stageObjects_.push_back(std::move(obj));
    }
}

void GameplayScene::initBossFromSpawn(const SpawnPoint& sp) {
    if (sp.type != "boss" || sp.id.empty()) return;

    boss_.init(sp.id, sp.x, sp.y);
    // CP-B1A is an explicit canonical-stage bridge. Requiring both stage
    // identity and the authenticated full-stage spawn keeps randomizer,
    // Bloody Palace, and compact legacy instances on their own geometry.
    if (!randomizerMode && !arenaWaveMode_ && !bossRushMode &&
        activeStageId_.str() == "chill-penguin" &&
        sp.id == "chill-penguin" &&
        sp.x == 7888.0f && sp.y == 264.0f) {
        boss_.configureCpStageIntro(428.0f);
    }
    boss_.setTarget(&player_);
    bossActive_ = true;
}

void GameplayScene::activateArenaWaveBoss() {
    if (!bossActive_ || !boss_.active) return;

    bossLocked_ = true;
    boss_.activate();
    camera_.setRoom(0.0f, 0.0f,
                    static_cast<float>(tilemap_.pixelWidth()),
                    static_cast<float>(tilemap_.pixelHeight()));
    gameplay_boss::syncBossHpBar(boss_, hud_);
    AudioManager::playSFX(SFX::BossIntro);
    AudioManager::playBGM("boss");
}

void GameplayScene::completeArenaWave() {
    if (stageClear_) return;

    stageClear_ = true;
    stageClearTimer_ = 0;
    hud_.hideBossHP();
    AudioManager::stopBGM();
    AudioManager::playSFX(SFX::StageClear);
}

void GameplayScene::updateArenaWaveClear() {
    if (!arenaWaveMode_ || stageClear_ || gameOver_) return;

    for (const auto& e : enemies_) {
        if (e.active && e.enemyState != EnemyState::Dead) return;
    }

    if (bossActive_ && boss_.active && !boss_.isDead()) return;
    completeArenaWave();
}

void GameplayScene::setupStormEagleDestructibles() {
    if (activeStageId_.str() != "storm-eagle") return;
    stormEagleCascade_.reset();
    stormEagleCascadeStarted_ = false;
    const int installedFixtures =
        storm_eagle_destructible_terrain::installBreakableFixtures(tilemap_);
    if (installedFixtures == 0) {
        TraceLog(LOG_WARNING,
                 "Storm Eagle source destructible fixtures were not found in the loaded stage");
    } else {
        TraceLog(LOG_INFO,
                 "Storm Eagle source destructible cells bound at runtime: %d/%d",
                 installedFixtures,
                 storm_eagle_destructible_terrain::kMutationCount);
    }
}

void GameplayScene::updateStormEagleDestructibles() {
    if (activeStageId_.str() != "storm-eagle" || !stormEagleCascade_.active()) {
        return;
    }

    const auto& schedule = storm_eagle_destructible_terrain::measuredCascade();
    // Advance before checking the cue: the runner's current tick is the
    // source-relative tick after this update. This keeps 0x1B one update
    // before waves 0..7, while the final wave remains uncued.
    const auto& due = stormEagleCascade_.advance(schedule);
    if (stormEagleCascade_.sfxDueThisTick(schedule)) {
        AudioManager::playApu(schedule.sfxCommand);
    }
    for (const auto& mutation : due) {
        // The adapter already translated source ids to committed engine ids.
        // replaceDirectMainTile is deliberately old-id guarded, so an absent
        // or already-mutated cell cannot corrupt unrelated stage content.
        tilemap_.replaceDirectMainTile(
            mutation.col, mutation.row, mutation.fromTileId, mutation.toTileId,
            TileType::None);
    }
}

bool GameplayScene::breakStormEagleCell(int tileX, int tileY) {
    if (activeStageId_.str() != "storm-eagle") return false;
    const auto* mutation = storm_eagle_destructible_terrain::mutationAt(tileX, tileY);
    if (mutation == nullptr || tilemap_.getTileType(tileX, tileY) != TileType::Breakable) {
        return false;
    }
    // Once the source-mapped cascade owns this family of cells, consume later
    // contacts here. Returning false would fall through to the generic helmet
    // debris path, which is not the measured Storm Eagle presentation.
    if (stormEagleCascadeStarted_) return true;
    // The source transition keeps the measured post-destruction art and
    // removes the collision. It never clears the visual tile to empty.
    const bool applied = storm_eagle_destructible_terrain::applyMutation(tilemap_, *mutation);
    if (applied) {
        // The supplied movie proves a two-object source trigger, not Helmet
        // or Boots ownership. This contact path remains provisional, but it
        // now exercises the measured multi-wave mechanism once a real,
        // source-mapped contact is present.
        stormEagleCascadeStarted_ = true;
        stormEagleCascade_.trigger();
    }
    return applied;
}

void GameplayScene::checkStormEagleDashBreak() {
    if (activeStageId_.str() != "storm-eagle" || player_.isDead()) return;
    if (player_.state() != PlayerState::Dash &&
        player_.state() != PlayerState::DashJump) {
        return;
    }

    const AABB hb = player_.getHitbox();
    const int tileSize = tilemap_.tileSize();
    if (tileSize <= 0) return;
    const int direction = player_.facingRight ? 1 : -1;
    const float edge = direction > 0 ? hb.right() + 1.0f : hb.left() - 1.0f;
    const int tileX = static_cast<int>(std::floor(edge / tileSize));
    const int firstRow = static_cast<int>(hb.top() / tileSize);
    const int lastRow = static_cast<int>((hb.bottom() - 0.01f) / tileSize);
    for (int tileY = firstRow; tileY <= lastRow; ++tileY) {
        if (breakStormEagleCell(tileX, tileY)) return;
    }
}

void GameplayScene::spawnEnemies() {
    pendingSourceEnemySerials_.clear();
    if (arenaWaveMode_) {
        for (const auto& sp : arenaWaveEnemies_) {
            spawnEnemyFromSpawn(sp);
        }
        return;
    }

    size_t pickupOrdinal = 0;
    const char* skipAutotestPickups = std::getenv("MMX_AUTOTEST_SKIP_PICKUPS");
    const bool skipPickupsForAutotest =
        skipAutotestPickups && *skipAutotestPickups != '\0' &&
        std::string(skipAutotestPickups) != "0";
    // R292: provisional census markers require explicit diagnostic opt-in.
    const char* skipProvisionalEnv = std::getenv("MMX_SKIP_PROVISIONAL");
    const bool skipProvisional =
        gameplay_enemies::skipProvisionalForSetting(skipProvisionalEnv);
    for (const auto& sp : tilemap_.spawns()) {
        if (!gameplay_enemies::spawnAllowed(sp, skipProvisional)) {
            continue;
        }
        if (sp.type == "enemy") {
            const auto countBefore = enemies_.size();
            spawnEnemyFromSpawn(sp);
            if (enemies_.size() == countBefore) continue;
            if (!randomizerMode && !bossRushMode &&
                sp.activation == SpawnActivation::SourceHorizontalCameraBucket) {
                // R112 / oid_0x49/creation.json: retain serial/order, but the
                // source object does not exist before its descriptor event.
                auto& enemy = enemies_.back();
                enemy.active = false;
                enemy.cameraActivated = false;
                pendingSourceEnemySerials_.push_back(enemy.serial);
            }
            // R72 camera_return.json: only the two authenticated, authored
            // CP bodies opt in. Extra spawns and arena waves use the helper
            // without acquiring this stage-placement lifecycle.
            const bool cpAxePlacement =
                (sp.x == 501.0f && sp.y == 1146.0f) ||
                (sp.x == 1013.0f && sp.y == 1114.0f);
            if (!randomizerMode && !bossRushMode &&
                activeStageId_.str() == "chill-penguin" &&
                sp.id == "axemax" && cpAxePlacement) {
                enemies_.back().configureCpCameraReturn();
            }
        } else if (StageObject::isStageObjectSpawn(sp)) {
            spawnStageObjectFromSpawn(sp);
        } else if (sp.type == "pickup") {
            // Source replays may begin after a pickup was collected while the
            // canonical stage still declares its normal spawn. Keep this
            // diagnostic-only hook out of normal gameplay and advance the
            // ordinal so persistent-id ordering stays stable.
            if (skipPickupsForAutotest) {
                ++pickupOrdinal;
                continue;
            }
            const std::string pickupUid = pickupPersistentId(activeStageId_, pickupOrdinal++, sp);
            const std::string legacyPickupUid = legacyPickupPersistentId(stagePath, sp);
            const bool alreadyCollected = Player::isPickupCollected(pickupUid) ||
                                          Player::isPickupCollected(legacyPickupUid);
            if (sp.id == "heart-tank") {
                if (!alreadyCollected) {
                    Pickup p;
                    const auto topLeft = storm_eagle_heart_tank_placement::runtimeTopLeftFromCentre(sp.x, sp.y);
                    p.init(topLeft.x, topLeft.y, PickupType::HeartTank, pickupUid);
                    pickups_.push_back(p);
                }
            } else if (sp.id == "sub-tank") {
                if (!alreadyCollected) {
                    Pickup p;
                    p.init(sp.x, sp.y, PickupType::SubTank, pickupUid);
                    pickups_.push_back(p);
                }
            } else if (sp.id == "capsule-boots" || sp.id == "capsule-helmet" ||
                       sp.id == "capsule-body" || sp.id == "capsule-buster") {
                if (!alreadyCollected) {
                    Pickup p;
                    p.init(sp.x, sp.y, PickupType::ArmorCapsule, pickupUid);
                    // Extract the part name from the id ("capsule-boots" -> "boots")
                    p.armorPart = sp.id.substr(8); // Skip "capsule-"
                    pickups_.push_back(p);
                }
            }
        }
    }

    for (const auto& sp : extraEnemySpawns_) {
        spawnEnemyFromSpawn(sp);
    }
}

void GameplayScene::pollInput() {
    if (stageStartControlsLockedForNextFrame()) return;

    // The original has no final-stock choice menu. After the death fade it
    // shows the current password; jump/confirm dismisses that source screen.
    if (!bossRushMode && gameOver_) {
        dismissGameOverPassword();
        return;
    }

    // Stage clear returns to stage select or title.
    // In bossRushMode the parent scene handles transitions, so skip this.
    if (!bossRushMode &&
        (stageClear_ && stageClearTimer_ > 70)) {
        if (!gameplay_stage_clear::navigationInputAllowed(
                weaponGetSequenceActive())) return;
        if (Input::isConfirmPressed()) {
            Input::consumeConfirmPress();
            returnToStageSelect_ = true;
        }
        if (Input::isCancelPressed()) {
            Input::consumeCancelPress();
            returnToTitle_ = true;
        }
        return;
    }

    if (escapeConfirm_.open) {
        if (Input::isCancelPressed() || Input::isPausePressed()) {
            Input::consumeCancelPress();
            Input::consumePausePress();
            closeEscapeConfirm(escapeConfirm_);
            escapeConfirmInputDelay_ = 0;
            paused_ = false;
            AudioManager::resumeBGM();
            AudioManager::playSFX(SFX::MenuCancel);
            return;
        }

        if (escapeConfirmInputDelay_ > 0) {
            escapeConfirmInputDelay_--;
        } else {
            int direction = 0;
            if (Input::isLeftHeld() || Input::isUpHeld()) {
                direction = -1;
            } else if (Input::isRightHeld() || Input::isDownHeld()) {
                direction = 1;
            }
            if (direction != 0) {
                moveEscapeConfirm(escapeConfirm_, direction);
                escapeConfirmInputDelay_ = 8;
                AudioManager::playSFX(SFX::MenuMove);
            }
        }
        if (!Input::isLeftHeld() && !Input::isRightHeld() &&
            !Input::isUpHeld() && !Input::isDownHeld()) {
            escapeConfirmInputDelay_ = 0;
        }

        if (Input::isConfirmPressed() || Input::isJumpPressed()) {
            Input::consumeConfirmPress();
            Input::consumeJumpPress();
            const bool wantsTitle = escapeConfirmWantsTitle(escapeConfirm_);
            closeEscapeConfirm(escapeConfirm_);
            escapeConfirmInputDelay_ = 0;
            paused_ = false;
            AudioManager::playSFX(wantsTitle ? SFX::MenuSelect : SFX::MenuCancel);
            if (wantsTitle) {
                returnToTitle_ = true;
            } else {
                AudioManager::resumeBGM();
            }
            return;
        }
        return;
    }

    if (!paused_ && !gameOver_ && !stageClear_ && Input::isCancelPressed()) {
        Input::consumeCancelPress();
        openEscapeConfirm(escapeConfirm_);
        escapeConfirmInputDelay_ = 0;
        paused_ = true;
        AudioManager::playSFX(SFX::Pause);
        AudioManager::pauseBGM();
        return;
    }

    // Pause toggle
    if (Input::isPausePressed()) {
        Input::consumePausePress();
        if (!gameOver_ && !stageClear_) {
            paused_ = !paused_;
            AudioManager::playSFX(SFX::Pause);
            if (paused_) {
                AudioManager::pauseBGM();
                pauseWeaponCursor_ = player_.weaponInventory.currentIndex;
                pauseInputDelay_ = 0;
            } else {
                AudioManager::resumeBGM();
            }
        }
    }
    // Cancel also unpauses (natural "back" action)
    if (paused_ && Input::isCancelPressed()) {
        Input::consumeCancelPress();
        paused_ = false;
        AudioManager::resumeBGM();
    }

    // Weapon select while paused
    if (paused_ && !escapeConfirm_.open) {
        int weaponCount = static_cast<int>(player_.weaponInventory.weaponCount());
        if (pauseInputDelay_ > 0) {
            pauseInputDelay_--;
        } else {
            if (Input::isUpHeld()) {
                pauseWeaponCursor_--;
                if (pauseWeaponCursor_ < 0) pauseWeaponCursor_ = weaponCount - 1;
                pauseInputDelay_ = 8;
                AudioManager::playSFX(SFX::MenuMove);
            }
            if (Input::isDownHeld()) {
                pauseWeaponCursor_++;
                if (pauseWeaponCursor_ >= weaponCount) pauseWeaponCursor_ = 0;
                pauseInputDelay_ = 8;
                AudioManager::playSFX(SFX::MenuMove);
            }
        }
        if (!Input::isUpHeld() && !Input::isDownHeld()) pauseInputDelay_ = 0;

        // Confirm weapon selection
        if (Input::isConfirmPressed() || Input::isJumpPressed()) {
            Input::consumeConfirmPress();
            Input::consumeJumpPress();
            player_.weaponInventory.currentIndex = pauseWeaponCursor_;
            AudioManager::playSFX(SFX::WeaponSwitch);
            paused_ = false;
            AudioManager::resumeBGM();
        }
    }

    if (!paused_ && !gameOver_ && !stageClear_ && !capsuleControlsLocked() &&
        Input::isRestartStagePressed()) {
        Input::consumeRestartStagePress();
        restartRequested_ = true;
        return;
    }

    if (paused_ || gameOver_ || stageClear_) return;
    if (capsuleControlsLocked()) return;

    player_.pollInput();

    // Use Sub-Tank — always consume the press to prevent phantom activation
    if (Input::isSubTankPressed()) {
        Input::consumeSubTankPress();
        if (player_.health < player_.progressState().maxHealth) {
            for (int i = 0; i < Player::MAX_SUB_TANKS; i++) {
                if (player_.progressState().subTanks[i].collected && player_.progressState().subTanks[i].health > 0) {
                    int amount = std::min(player_.progressState().maxHealth - player_.health, player_.progressState().subTanks[i].health);
                    player_.health += amount;
                    player_.progressState().subTanks[i].health -= amount;
                    break;
                }
            }
        }
    }

    if (Input::isDebug1Pressed()) { Input::consumeDebug1Press(); showCollision_ = !showCollision_; }
    if (Input::isDebug2Pressed()) { Input::consumeDebug2Press(); showDebugInfo_ = !showDebugInfo_; }

    // F4: grant X-Buster + all 8 special weapons (full ammo) for testing.
    if (Input::isDebug4Pressed()) {
        Input::consumeDebug4Press();
        grantAllWeaponsForAutotestRun(false);
        TraceLog(LOG_INFO, "Debug: granted all weapons (F4, no armor mutation)");
    }

    // U66 (David 2026-06-11): numpad debug grants — one capsule upgrade per
    // key so he can check each transformation. KP1=legs(dash) KP2=body
    // KP3=helmet KP4=buster. applyArmorState makes it live immediately.
    // (Armored-X SPRITES don't exist yet — functional change + log only;
    // sprite rip filed as its own BOARD row.)
    if (Input::isUpgradeGrant1Pressed()) {
        Input::consumeUpgradeGrant1Press();
        player_.grantArmorBoots();
        TraceLog(LOG_INFO, "GRANT: legs capsule (dash unlocked)");
    }
    if (Input::isUpgradeGrant2Pressed()) {
        Input::consumeUpgradeGrant2Press();
        player_.grantArmorBody();
        TraceLog(LOG_INFO, "GRANT: body capsule (half damage)");
    }
    if (Input::isUpgradeGrant3Pressed()) {
        Input::consumeUpgradeGrant3Press();
        player_.grantArmorHelmet();
        TraceLog(LOG_INFO, "GRANT: helmet capsule (headbutt)");
    }
    if (Input::isUpgradeGrant4Pressed()) {
        Input::consumeUpgradeGrant4Press();
        player_.grantArmorBuster();
        TraceLog(LOG_INFO, "GRANT: buster capsule (charge L3)");
    }

    // F3: Sprite test mode — browse sprite frames
    if (Input::isDebug3Pressed()) {
        Input::consumeDebug3Press();
        spriteTestMode_ = !spriteTestMode_;
        spriteTestFrame_ = 0;
        spriteTestDelay_ = 0;
    }
    if (spriteTestMode_) {
        int maxFrame = 0;
        const TextureResource* playerSheet = player_.spriteSheetResource();
        if (playerSheet && playerSheet->valid()) {
            int cols = playerSheet->width() / static_cast<int>(player_.spriteWidth);
            int rows = playerSheet->height() / static_cast<int>(player_.spriteHeight);
            maxFrame = cols * rows - 1;
        }
        if (spriteTestDelay_ > 0) spriteTestDelay_--;
        if (spriteTestDelay_ == 0) {
            if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
                spriteTestFrame_ = (spriteTestFrame_ + 1) % (maxFrame + 1);
                spriteTestDelay_ = 10;
            }
            if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
                spriteTestFrame_--;
                if (spriteTestFrame_ < 0) spriteTestFrame_ = maxFrame;
                spriteTestDelay_ = 10;
            }
            // Page through 10 at a time with up/down
            if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) {
                spriteTestFrame_ = std::min(spriteTestFrame_ + 10, maxFrame);
                spriteTestDelay_ = 10;
            }
            if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) {
                spriteTestFrame_ = std::max(spriteTestFrame_ - 10, 0);
                spriteTestDelay_ = 10;
            }
        }
    }
}

bool GameplayScene::dismissGameOverPassword() {
    // Source US V1.1 traces show every face button (A/B/X/Y) and START
    // dismissing the settled grid, while D-pad input leaves it visible.
    const bool dismissPressed =
        Input::isConfirmPressed() ||
        Input::isJumpPressed() ||
        Input::isDashPressed() ||
        Input::isShootPressed() ||
        Input::isPausePressed();
    if (!gameplay_game_over::passwordVisible(gameOver_, gameOverTimer_) ||
        !dismissPressed) {
        return false;
    }
    Input::consumeConfirmPress();
    Input::consumeJumpPress();
    Input::consumeDashPress();
    Input::consumeShootPress();
    Input::consumePausePress();
    player_.lives = DifficultySettings::startingReserveLives();
    if (activeStageId_.str() == "intro-highway") {
        restartRequested_ = true;
    } else {
        returnToStageSelect_ = true;
    }
    return true;
}

void GameplayScene::handleInput() {
    // Real presses are latched in pollInput; deterministic autotest presses
    // enter during Input::update and therefore need the fixed-tick path too.
    if (!bossRushMode && gameOver_) {
        dismissGameOverPassword();
        return;
    }
    if (paused_ || stageClear_) return;
    if (stageStartControlsLockedForNextFrame()) return;
    if (capsuleControlsLocked()) return;
    player_.handleInput();
}

bool GameplayScene::stageStartControlsLockedForNextFrame() const {
    if (!stageStartSequence_) return false;
    return stage_start_timeline::evaluate(stageStartTick_ + 1).controlsLocked;
}

void GameplayScene::update(float dt) {
    // Return to title screen
    if (returnToTitle_ && sceneManager_) {
        auto title = std::make_unique<TitleScene>();
        title->setSceneManager(sceneManager_);
        sceneManager_->changeScene(std::move(title));
        return;
    }

    // Return to stage select (or ending after Sigma) after stage clear
    if (returnToStageSelect_ && sceneManager_) {
        if (bossActive_ && boss_.type == "sigma") {
            auto ending = std::make_unique<EndingScene>();
            ending->setSceneManager(sceneManager_);
            sceneManager_->changeScene(std::move(ending));
        } else {
            ContentPack pack;
            pack.loadFromFile("content/x1/manifest.json");
            auto stageSelect = std::make_unique<StageSelectScene>();
            stageSelect->setSceneManager(sceneManager_);
            stageSelect->setContentPack(pack);
            if (randomizerMode) {
                stageSelect->setRandomizer(randomizerSeed, randomizerConfig);
            }
            sceneManager_->changeScene(std::move(stageSelect));
        }
        return;
    }

    // Handle restart request (game over or stage clear)
    if (restartRequested_) {
        restartRequested_ = false;
        gameOver_ = false;
        stageClear_ = false;
        paused_ = false;
        gameOverTimer_ = 0;
        stageClearTimer_ = 0;
        awardedWeapon_.reset();
        stageClearFrozen_ = false;
        weaponGetParams_ = weapon_get_presentation::Params{};
        weaponGetText_.clear();
        parityRoomTransitionTimer_ = 0;
        bossActive_ = false;
        bossLocked_ = false;
        projectiles_.clear();
        busterImpacts_.clear();
        enemies_.clear();
        pickups_.clear();
        stageObjects_.clear();
        checkpoints_.clear();
        cpCapsule_.reset();
        stormEagleCapsule_.reset();
        stingChameleonCapsule_.reset();
        camera_.clearRoom();
        camera_.unlock();
        player_.lives = DifficultySettings::startingReserveLives();
        // A full-stage restart must revive Player before re-entering the
        // scene, and a diagnostic --spawn-at must not become the new campaign
        // start (the death autotest deliberately places X below the map).
        hasSpawnOverride = false;
        player_.respawnAt(defaultSpawn_, 0, false);
        onEnter(); // Reload everything
        return;
    }

    if (gameOver_) {
        gameOverTimer_++;
        gameplay_game_over::advanceDecorativeHelmetTicks(
            gameOverHelmetTicks_,
            []() { return std::rand() % 180; }
        );
        return;
    }
    if (stageClear_) {
        stageClearTimer_++;
        if (!stageClearFrozen_) {
            stageClearFrozen_ = true;
            player_.velocity = {0, 0};
            player_.changeState(PlayerState::Idle);
        }
        updateWeaponGetSequence();
        return;
    }
    if (paused_) return;
    if (fadeInTimer_ > 0) fadeInTimer_--;
    if (stageStartSequence_) {
        stageStartTick_++;
        warpInTimer_ = stage_start_timeline::warpTimerAt(stageStartTick_);
        if (stageStartTick_ > stage_start_timeline::kLastTick) {
            stageStartSequence_ = false;
        }
    } else if (warpInTimer_ > 0) {
        warpInTimer_--;
    }
    SaveSystem::addPlayFrames(1);
    stageTimer_++;
    if (spriteTestMode_) return; // Freeze gameplay in sprite test mode

    camera_.savePosition();

    // Keep the measured source-local release row visible for the render that
    // follows its control-release update, then retire it on the next tick.
    gameplay_storm_eagle_capsule::finishReleasedVisual(stormEagleCapsule_);
    gameplay_sting_chameleon_capsule::finishReleasedVisual(
        stingChameleonCapsule_);

    // U63: advance the palette-cycle tileset animation (measured cadence).
    tilemap_.tickAnimation();

    // A source-mapped Storm Eagle contact starts the measured cascade in the
    // collision lane; subsequent waves advance on the normal frame clock.
    updateStormEagleDestructibles();

    // READY and the pre-release warp are a presentation lock: the visible
    // stage animates, while actors and projectiles stay frozen.
    if (stage_start_timeline::evaluate(stageStartTick_).controlsLocked) return;

    if (cpCapsule_.active()) {
        updateCpCapsuleCutscene(dt);
        return;
    }
    if (stormEagleCapsule_.active()) {
        updateStormEagleCapsuleCutscene(dt);
        return;
    }
    if (stingChameleonCapsule_.active()) {
        updateStingChameleonCapsuleCutscene(dt);
        return;
    }

    // Canonical Storm Eagle support is handed to Player before its action
    // update. Allocation and initialization consume separate source ticks;
    // motion/contact are deferred until after player/tile physics below.
    const bool stormEaglePlatformActive =
        prepareStormEaglePlatformFrame();

    // Player
    player_.update(dt);
    gameplay_sting_chameleon_capsule::preparePostReleaseMotion(
        stingChameleonCapsule_, player_,
        Input::isRightHeld(), Input::isLeftHeld());
    const auto cpEntryPassable = [this](int col, int row) {
        return bossEntryShutterPassable_
            && activeStageId_.str() == "chill-penguin"
            && col >= kCpEntryShutterMinCol && col <= kCpEntryShutterMaxCol
            && row >= kCpEntryShutterMinRow && row <= kCpEntryShutterMaxRow;
    };
    // chill_penguin_ground_contact.json: CP raw-5..8 authored slopes use
    // integer quarter steps with an inclusive source edge. This owner supplies the
    // corresponding native exclusive bottom after the existing anchor bridge.
    // Its integer-only Y stores preserve the airborne integration remainder;
    // grounded continuation preserves the remainder from before synthetic gravity.
    const auto sourceBoxBeforePhysics = player_.getHitbox();
    const float sourceFeetXBeforePhysics =
        sourceBoxBeforePhysics.left() + sourceBoxBeforePhysics.w * 0.5f;
    const float sourceFeetYBeforePhysics = sourceBoxBeforePhysics.bottom();
    const physics::SlopeSurfacePolicy sourceGroundSurface =
        [this, sourceFeetYBeforePhysics](const Actor& actor, const Tilemap& tilemap,
               int tileX, int tileY, int rowOffset, float feetX,
               float /*linearSurfaceY*/) -> std::optional<float> {
            const bool grounded =
                (player_.state() == PlayerState::Idle || player_.state() == PlayerState::Run) &&
                (actor.onGround || actor.wasOnGround);
            const bool firstLanding = player_.state() == PlayerState::Fall &&
                !actor.wasOnGround && !actor.onGround && !actor.onCeiling &&
                !actor.touchingWallLeft && !actor.touchingWallRight && actor.velocity.y >= 0;
            if (&actor != static_cast<const Actor*>(&player_) ||
                !player_.sourceGroundMovementEnabled() ||
                (!grounded && !firstLanding) ||
                tilemap.tileSize() != 16 ||
                tilemap.getTileType(tileX, tileY) != TileType::SlopeR) {
                return std::nullopt;
            }
            const auto profile = player_.sourceContactProfile();
            const auto slope = tilemap.getSlope(tileX, tileY);
            const int rawAttr = tilemap.getRawAttr(tileX, tileY);
            const bool authoredCpQuarter =
                (rawAttr == 0x05 && slope.leftY == 16 && slope.rightY == 12) ||
                (rawAttr == 0x06 && slope.leftY == 12 && slope.rightY == 8) ||
                (rawAttr == 0x07 && slope.leftY == 8 && slope.rightY == 4) ||
                (rawAttr == 0x08 && slope.leftY == 4 && slope.rightY == 0);
            if (!profile || *profile != SourceContactProfile::NormalA552 ||
                !authoredCpQuarter) {
                return std::nullopt;
            }
            // The exclusive bottom of raw5 can lie exactly in the row below it.
            const bool atRaw5BottomBoundary = rowOffset == -1 && rawAttr == 0x05 &&
                actor.getHitbox().bottom() == static_cast<float>((tileY + 1) * 16);
            if (rowOffset != 0 && !atRaw5BottomBoundary) return std::nullopt;
            const int localX = static_cast<int>(std::floor(feetX)) - tileX * 16;
            if (localX < 0 || localX >= 16) return std::nullopt;
            const float fractionSource = grounded ? sourceFeetYBeforePhysics
                                                   : actor.getHitbox().bottom();
            const float fraction = fractionSource - std::floor(fractionSource);
            return static_cast<float>(tileY * 16 + slope.leftY - localX / 4) + fraction;
        };
    const bool holdStormEagleSupport = stormEaglePlatformActive
        ? player_.consumeStormEagleSupportForPhysics()
        : false;
    // ground_jump_launch.json: the Walk caller already dispatched ascent.
    // Preserve horizontal ramp clearance without adding a grounded step-up
    // to its first vertical integration (left control otherwise rises 2px extra).
    const bool allowSlopeStepUp = !player_.sourceWalkJumpLaunching();
    if (holdStormEagleSupport) {
        physics::moveAndResolveXOnly(player_, tilemap_, cpEntryPassable);
    } else if (bossEntryShutterPassable_) {
        physics::moveAndCollide(player_, tilemap_, cpEntryPassable, sourceGroundSurface, allowSlopeStepUp);
    } else {
        physics::moveAndCollide(player_, tilemap_, {}, sourceGroundSurface, allowSlopeStepUp);
    }
    // chill_penguin_ground_contact.json landingContinuationRows: the raw8
    // exit and its first raw3B flat placement both write only the integer Y.
    // The native feet column is already flat, so the slope callback cannot
    // finish this handoff. Keep the previous source lookup on adjacent raw8;
    // this does not extend fraction placement to an ordinary flat traversal.
    const auto sourceProfile = player_.sourceContactProfile();
    if (player_.sourceGroundMovementEnabled() && sourceProfile &&
        *sourceProfile == SourceContactProfile::NormalA552 &&
        (player_.state() == PlayerState::Idle || player_.state() == PlayerState::Run) &&
        player_.wasOnGround && player_.onGround && player_.velocity.y == 0 &&
        !player_.onCeiling && !player_.touchingWallLeft && !player_.touchingWallRight &&
        tilemap_.tileSize() == 16) {
        const auto box = player_.getHitbox();
        const float feetX = box.left() + box.w * 0.5f;
        const int col = static_cast<int>(std::floor(feetX / 16.0f));
        const int row = static_cast<int>(std::floor(box.bottom() / 16.0f));
        const int sourceCol = static_cast<int>(
            std::floor((sourceFeetXBeforePhysics - 1.0f) / 16.0f));
        const auto previousSlope = tilemap_.getSlope(sourceCol, row);
        if (feetX > sourceFeetXBeforePhysics && sourceCol + 1 == col &&
            std::floor(sourceFeetYBeforePhysics / 16.0f) == row &&
            box.bottom() == static_cast<float>(row * 16) &&
            tilemap_.getTileType(col, row) == TileType::Solid &&
            tilemap_.getRawAttr(col, row) == 0x3B &&
            tilemap_.getTileType(sourceCol, row) == TileType::SlopeR &&
            tilemap_.getRawAttr(sourceCol, row) == 0x08 &&
            previousSlope.leftY == 4 && previousSlope.rightY == 0) {
            player_.position.y += sourceFeetYBeforePhysics - std::floor(sourceFeetYBeforePhysics);
        }
    }
    gameplay_sting_chameleon_capsule::finishPostReleaseMotion(
        stingChameleonCapsule_, player_);
    gameplay_player_bounds::clampLeftViewportBound(player_, camera_.baseX());
    applyAutotestPlayerPositionLock();
    resolvePlayerAxeMaxStackCollision();
    checkStormEagleDashBreak();

    // Enemies
    updateEnemies();

    // Stage objects / hazards (non-enemy object lane)
    updateStageObjects();

    // Source platform motion/contact belongs after Player and tile physics,
    // in the existing stage-object lane. It updates the current aggregate
    // support latch for the following Player tick.
    updateStormEaglePlatforms();

    // Boss
    if (bossActive_) updateBoss();

    // Projectiles (player + enemy + boss shots)
    updateProjectiles();

    // Cosmetic ice trail behind Shotgun Ice pellets (after projectiles moved)
    updateIceTrail();

    // FW7: advance existing Buster impacts before collision. New age-0 impacts
    // render source f447 this tick; fixed cadence begins on the following tick.
    updateBusterImpacts();

    // Cosmetic torpedo smoke wake (after projectiles moved)
    updateTorpedoSmoke();

    // Charged Shotgun Ice: X rides the sled (after projectiles moved)
    applyRideableCarry();

    // Pickups
    updatePickups();

    // Collision checks
    checkBulletEnemyCollision();
    checkPlayerEnemyCollision();
    checkPlayerStageObjectCollision();
    checkEnemyShotPlayerCollision();
    checkPlayerPickupCollision();

    // Boss collision
    if (bossActive_) {
        checkBossPlayerCollision();
        checkBulletBossCollision();
    }

    // Flush deferred shatter/split spawns (see pendingProjectileSpawns_).
    if (!pendingProjectileSpawns_.empty()) {
        for (auto& np : pendingProjectileSpawns_) {
            projectiles_.push_back(std::move(np));
        }
        pendingProjectileSpawns_.clear();
    }

    updateArenaWaveClear();

    // Helmet headbutt — break ceiling tiles
    checkHelmetHeadbutt();

    // Checkpoints
    updateCheckpoints();

    // Handle death sequence / pit fall
    handlePlayerDeath();

    // Keep diagnostic source locks stable after carry, collisions, and death handlers.
    applyAutotestPlayerPositionLock();

    // Death orb visual effect: spawn scheduled death rings, then tick live orbs.
    const auto deathOrbUpdate = gameplay_death_orbs::updateState(
        deathOrbState_,
        gameplay_death_orbs::makeUpdateInput(
            player_.isDead(),
            player_.deathTimer(),
            playerCameraAnchorX(),
            player_.position.y + player_.spriteHeight / 2,
            player_.position.x + player_.spriteWidth / 2,
            player_.position.y + player_.spriteHeight / 2));
    if (deathOrbUpdate.cameraTransition) {
        const auto& transition = *deathOrbUpdate.cameraTransition;
        camera_.setClampSuppressed(transition.suppressClamp);
        camera_.snapTo(transition.targetX, transition.targetY);
    }
    // Boss arena lock-in: when player enters a boss room
    if (bossActive_ && !bossLocked_ && boss_.active) {
        for (const auto& room : tilemap_.rooms()) {
            // Activate when the player enters any named boss room (the
            // room's own rule: x threshold, or its trigger rectangle).
            if (!room.name.empty() &&
                room.triggeredBy(player_.position.x, player_.position.y)) {
                bossLocked_ = true;
                boss_.activate();
                camera_.setRoom(room.x, room.y, room.w, room.h);
                gameplay_boss::syncBossHpBar(boss_, hud_);
                AudioManager::playSFX(SFX::BossIntro);
                AudioManager::playBGM("boss");
                if (boss_.usesScriptedKinematics()) {
                    // CP-B1C-T1-M2: begin the KB-driven entry cinematic.
                    // Walk and pan are offsets into the engine's own settle
                    // and lock values — no absolute coordinate claim.
                    bossEntryCinematicActive_ = true;
                    bossEntryShutterPassable_ = true;
                    bossEntryCamLockX_ = room.x;
                    if (hasEntrySourceHitbox_) {
                        player_.hitboxOffset = entrySourceHitboxOffset_;
                        player_.hitboxSize = entrySourceHitboxSize_;
                    }
                    player_.beginScriptedEntryWalk();
                    player_.position.x =
                        boss_cp_intro_timeline::entryPlayerPositionX(
                            boss_cp_intro_timeline::kSourceSpawnTick);
                    player_.position.y =
                        boss_cp_intro_timeline::entryPlayerPositionY(
                            boss_cp_intro_timeline::kSourceSpawnTick);
                    player_.onGround = true;
                    player_.velocity = {0.0f, 0.0f};
                    player_.prevPosition = player_.position;
                }
                break;
            }
        }
    }

    updateCpEntryRoute();

    // Apply per-room camera bounds (vertical lock + horizontal scroll limits)
    // before following, matching the real MMX per-room camera.
    updateCameraRoom();

    // Camera follow uses the measured source/RAM anchor (position+32 facing
    // right), not the configured hitbox center, which Chill Penguin's legacy
    // rollout box puts at position+33 - one pixel right of the source. The
    // 70px render-cell center (position+35) is a third, distinct quantity.
    // CP-B1C-C1 pins all three without a tolerance (plan task B1).
    const auto previousCameraX = sourceCameraCoordinate(camera_.baseX());
    camera_.update(
        playerCameraAnchorX(),
        player_.position.y + player_.spriteHeight / 2,
        player_.facingRight
    );

    updateCpEntryCamera();
    activateSourceEnemiesAfterCamera(previousCameraX);

    // HUD
    hud_.update(player_.health, player_.progressState().maxHealth, player_.lives);
    if (!player_.weaponInventory.isBuster()) {
        const Weapon& w = player_.weaponInventory.current();
        hud_.setWeaponEnergy(player_.weaponInventory.currentAmmo(), w.maxAmmo, w.gaugeColor, w.id);
    } else {
        hud_.hideWeaponEnergy();
    }

    writeProjTrace();
    writeParityTrace();
}

void GameplayScene::writeProjTrace() {
    if (!projTrace_) return;
    ++projTraceTick_;
    std::fprintf(projTrace_, "%ld,camera,0,-,0,0,%.6f,%.6f,0,0\n",
                 projTraceTick_, camera_.x(), camera_.y());
    std::fprintf(projTrace_, "%ld,player,0,-,0,0,%.6f,%.6f,%.6f,%.6f\n",
                 projTraceTick_, player_.position.x, player_.position.y,
                 player_.velocity.x, player_.velocity.y);
    std::fprintf(projTrace_, "%ld,player_hp,0,-,%d,%d,%.6f,%.6f,%.6f,%.6f\n",
                 projTraceTick_, player_.health, player_.iframeTimer(),
                 player_.position.x, player_.position.y,
                 player_.velocity.x, player_.velocity.y);
    const auto capsuleTrace =
        gameplay_cp_capsule::traceState(cpCapsule_, player_.armorBootsUnlocked());
    if (capsuleTrace.active || capsuleTrace.grantEvent || capsuleTrace.releaseEvent) {
        const char* state = capsuleTrace.releaseEvent
            ? "release"
            : (capsuleTrace.grantEvent ? "grant" : "active");
        const int eventCode = capsuleTrace.releaseEvent
            ? 2
            : (capsuleTrace.grantEvent ? 1 : 0);
        std::fprintf(projTrace_, "%ld,cp_capsule,%d,%s,%d,%d,%.6f,%.6f,%.6f,%.6f\n",
                     projTraceTick_, capsuleTrace.sourceFrame, state,
                     capsuleTrace.armorBoots ? 1 : 0, eventCode,
                     player_.position.x, player_.position.y,
                     player_.velocity.x, player_.velocity.y);
        std::fprintf(projTrace_,
                     "%ld,cp_capsule_anim,%d,source_anim,%d,%d,%.6f,%.6f,%.6f,%.6f\n",
                     projTraceTick_, capsuleTrace.sourceFrame, capsuleTrace.animByte,
                     eventCode, player_.position.x, player_.position.y,
                     player_.velocity.x, player_.velocity.y);
    }
    const auto stormCapsuleTrace = gameplay_storm_eagle_capsule::traceState(
        stormEagleCapsule_, player_.armorHelmetUnlocked());
    if (stormCapsuleTrace.active || stormCapsuleTrace.grantEvent ||
        stormCapsuleTrace.releaseEvent) {
        const char* state = stormCapsuleTrace.releaseEvent
            ? "release"
            : (stormCapsuleTrace.grantEvent ? "grant" : "active");
        const int eventCode = stormCapsuleTrace.releaseEvent
            ? 2
            : (stormCapsuleTrace.grantEvent ? 1 : 0);
        std::fprintf(
            projTrace_,
            "%ld,storm_eagle_capsule,%d,%s,%d,%d,%.6f,%.6f,%.6f,%.6f\n",
            projTraceTick_, stormCapsuleTrace.sourceFrame, state,
            stormCapsuleTrace.armorHelmet ? 1 : 0, eventCode,
            player_.position.x, player_.position.y,
            player_.velocity.x, player_.velocity.y);
        std::fprintf(
            projTrace_,
            "%ld,storm_eagle_capsule_anim,%d,source_anim,%d,%d,%.6f,%.6f,%.6f,%.6f\n",
            projTraceTick_, stormCapsuleTrace.sourceFrame,
            stormCapsuleTrace.animByte, eventCode,
            player_.position.x, player_.position.y,
            player_.velocity.x, player_.velocity.y);
        if (stormCapsuleTrace.pickupCollected && stormCapsuleTrace.pickupId &&
            !stormCapsuleTrace.pickupId->empty()) {
            std::fprintf(
                projTrace_,
                "%ld,storm_eagle_capsule_pickup,%d,%s,1,2,%.6f,%.6f,0,0\n",
                projTraceTick_, stormCapsuleTrace.sourceFrame,
                stormCapsuleTrace.pickupId->c_str(),
                player_.position.x, player_.position.y);
        }
    }
    const auto stingCapsuleTrace = gameplay_sting_chameleon_capsule::traceState(
        stingChameleonCapsule_, player_.armorBodyUnlocked());
    gameplay_sting_chameleon_capsule::writeTraceRows(
        projTrace_, projTraceTick_, stingCapsuleTrace,
        player_.position.x, player_.position.y,
        player_.velocity.x, player_.velocity.y);
    for (const auto& p : projectiles_) {
        if (!p.active || !p.isPlayerShot) continue;
        if (p.dormantFrames > 0) continue;  // slot not yet claimed (e-spark twin)
        std::fprintf(projTrace_, "%ld,proj,%d,%s,%d,%d,%.6f,%.6f,%.6f,%.6f\n",
                     projTraceTick_, p.serial, p.weaponId.c_str(),
                     (p.type != ProjectileType::Normal) ? 1 : 0,
                     p.isShatterFragment ? 1 : 0,
                     p.position.x, p.position.y, p.vx, p.vy);
    }
    // Live enemy hitbox centers (the homing-torpedo ghost proof replays the
    // steering model against the exact target the scene fed it). charged
    // column carries target health for enemy/boss rows.
    for (size_t i = 0; i < enemies_.size(); i++) {
        const auto& e = enemies_[i];
        if (!e.active || e.enemyState == EnemyState::Dead || e.health <= 0)
            continue;
        const AABB eb = e.getHitbox();
        std::fprintf(projTrace_, "%ld,enemy,%d,%s,%d,0,%.6f,%.6f,0,0\n",
                     projTraceTick_, static_cast<int>(i), e.type.c_str(),
                     e.health, eb.x + eb.w * 0.5f, eb.y + eb.h * 0.5f);
    }
    for (size_t i = 0; i < stageObjects_.size(); i++) {
        const auto& obj = stageObjects_[i];
        if (!obj.active) continue;
        const AABB ob = obj.getHitbox();
        std::fprintf(projTrace_, "%ld,stage_object,%d,%s,%d,%d,%.6f,%.6f,0,0\n",
                     projTraceTick_, static_cast<int>(i), obj.id().c_str(),
                     obj.contactDamage(), obj.canBeDamagedByPlayerShots() ? 1 : 0,
                     ob.x + ob.w * 0.5f, ob.y + ob.h * 0.5f);
    }
    if (bossActive_ && boss_.active && boss_.bossState != BossState::Dormant &&
        boss_.bossState != BossState::Dead && boss_.health > 0) {
        const AABB bb = boss_.getHitbox();
        std::fprintf(projTrace_, "%ld,boss,0,%s,%d,0,%.6f,%.6f,0,0\n",
                     projTraceTick_, boss_.type.c_str(), boss_.health,
                     bb.x + bb.w * 0.5f, bb.y + bb.h * 0.5f);
    }
    for (const auto& b : iceTrailBits_) {
        // kind 0 = pellet trail (checked by ghost_diff S1); break debris is
        // written as "fx" which ghost_diff ignores.
        std::fprintf(projTrace_, "%ld,%s,%d,-,0,0,%.6f,%.6f,%.6f,%.6f\n",
                     projTraceTick_, b.kind == 0 ? "trail" : "fx",
                     b.serial, b.x, b.y, b.vx, b.vy);
    }
    std::fflush(projTrace_); // test-only path; survive abrupt autotest exit
}

void GameplayScene::updateParityRoomTransitionSignal() {
    if (parityRoomTransitionTimer_ > 0) {
        parityRoomTransitionTimer_++;
        return;
    }
    stage_parity_transitions::DetectorInput detector;
    detector.stageId = activeStageId_;
    detector.playerX = player_.position.x;
    detector.playerY = player_.position.y;
    detector.cameraX = camera_.x();
    detector.tilemapPixelWidth = tilemap_.pixelWidth();
    detector.stageClearActive = stageClear_;
    detector.gameOverActive = gameOver_;
    if (stage_parity_transitions::shouldSignalBossDoorTransition(detector)) {
        parityRoomTransitionTimer_ = 1;
    }
}

void GameplayScene::writeParityTrace() {
    if (!parityTrace_) return;
    ++parityTraceTick_;
    updateParityRoomTransitionSignal();

    auto writeRow = [&](const char* kind, int serial, const char* id,
                        int state, int hp, float x, float y, float vx, float vy,
                        int a, int b, int c, int d,
                        int e = 0, int f = 0, int g = 0, int h = 0) {
        std::fprintf(parityTrace_,
                     "%ld,%s,%d,%s,%d,%d,%.6f,%.6f,%.6f,%.6f,%d,%d,%d,%d,%d,%d,%d,%d\n",
                     parityTraceTick_, kind, serial, id ? id : "-",
                     state, hp, x, y, vx, vy, a, b, c, d, e, f, g, h);
    };

    writeRow("camera", 0,
             activeCameraSectionId_.empty() ? "-" : activeCameraSectionId_.c_str(),
             bossLocked_ ? 1 : 0, 0, camera_.x(), camera_.y(), 0.0f, 0.0f,
             stageClear_ ? 1 : 0, stageClearTimer_, gameOver_ ? 1 : 0, paused_ ? 1 : 0);

    const std::string playerWeapon =
        player_.weaponInventory.isBuster() ? "buster" : player_.weaponInventory.current().id;
    writeRow("player", 0, playerWeapon.c_str(),
             static_cast<int>(player_.state()), player_.health,
             player_.position.x, player_.position.y,
             player_.velocity.x, player_.velocity.y,
             player_.progressState().maxHealth, player_.iframeTimer(),
             player_.facingRight ? 1 : 0, player_.lives,
             player_.renderFrameIndexForTest(), player_.chargeLevel(),
             player_.chargeTimer(), player_.weaponInventory.currentIndex);
    writeRow("player_hp", 0, "-", player_.deathTimer(), player_.health,
             player_.position.x, player_.position.y,
             player_.velocity.x, player_.velocity.y,
             player_.progressState().maxHealth, player_.iframeTimer(), player_.isDead() ? 1 : 0, 0);

    const auto& apu = AudioManager::apuLog();
    while (parityApuLogIndex_ < apu.size()) {
        const auto& event = apu[parityApuLogIndex_++];
        writeRow("audio_apu", event.frame, "-", event.command, 0,
                 player_.position.x, player_.position.y, 0.0f, 0.0f,
                 event.command, 0, 0, 0);
    }
    const auto& sfx = AudioManager::sfxLog();
    while (paritySfxLogIndex_ < sfx.size()) {
        const auto& event = sfx[paritySfxLogIndex_++];
        writeRow("audio_sfx", event.frame, "-", event.id, 0,
                 player_.position.x, player_.position.y, 0.0f, 0.0f,
                 event.id, 0, 0, 0);
    }

    for (const auto& p : projectiles_) {
        if (!p.active || p.dormantFrames > 0) continue;
        writeRow("projectile", p.serial, p.weaponId.empty() ? "-" : p.weaponId.c_str(),
                 static_cast<int>(p.type), p.damage, p.position.x, p.position.y,
                 p.vx, p.vy, p.isPlayerShot ? 1 : 0,
                 p.isShatterFragment ? 1 : 0, p.ageFrames, p.facingRight ? 1 : 0);
    }
    gameplay_buster_impact::writeParityTraceRows(busterImpacts_, writeRow);
    for (const auto& e : enemies_) {
        const AABB eb = e.getHitbox();
        writeRow("enemy", e.serial, e.type.c_str(), static_cast<int>(e.enemyState), e.health,
                 eb.x + eb.w * 0.5f, eb.y + eb.h * 0.5f,
                 e.velocity.x, e.velocity.y, e.active ? 1 : 0,
                 e.alive ? 1 : 0, e.deathTimer, e.deathBurstNodeCount());
    }

    if (bossActive_) {
        const AABB bb = boss_.getHitbox();
        writeRow("boss", 0, boss_.type.c_str(), static_cast<int>(boss_.bossState), boss_.health,
                 bb.x + bb.w * 0.5f, bb.y + bb.h * 0.5f,
                 boss_.velocity.x, boss_.velocity.y, boss_.active ? 1 : 0,
                 boss_.isDead() ? 1 : 0, boss_.deathTimer(), bossLocked_ ? 1 : 0,
                 boss_.displayHealth(), boss_.introVisible() ? 1 : 0,
                 boss_.introSourceTick(), static_cast<int>(boss_.cpState()));
    }

    for (size_t i = 0; i < stageObjects_.size(); ++i) {
        const auto& obj = stageObjects_[i];
        const AABB ob = obj.getHitbox();
        writeRow("stage_object", static_cast<int>(i), obj.id().c_str(),
                 obj.animationFrameIndexForTest(), obj.contactDamage(),
                 ob.x + ob.w * 0.5f, ob.y + ob.h * 0.5f, 0.0f, 0.0f,
                 obj.active ? 1 : 0, obj.canBeDamagedByPlayerShots() ? 1 : 0,
                 obj.sourceOid(), 0);
    }

    for (size_t i = 0; i < pickups_.size(); ++i) {
        const auto& p = pickups_[i];
        writeRow("pickup", static_cast<int>(i),
                 p.persistentId.empty() ? "-" : p.persistentId.c_str(),
                 static_cast<int>(p.type), p.value, p.position.x, p.position.y,
                 0.0f, p.vy, p.active ? 1 : 0, p.onGround ? 1 : 0, p.lifetime, 0);
    }

    for (const auto& orb : gameplay_death_orbs::traceOrbs(deathOrbState_)) {
        writeRow("fx", 0, "death_orb", orb.animFrame, orb.lifetime,
                 orb.x, orb.y, orb.vx, orb.vy, 0, 0, 0, 0);
    }
    for (const auto& b : iceTrailBits_) {
        writeRow("fx", b.serial, b.kind == 0 ? "ice_trail" : "ice_debris",
                 b.age, b.lifetime, b.x, b.y, b.vx, b.vy,
                 b.kind, b.hflip ? 1 : 0, 0, 0);
    }
    for (const auto& puff : torpedoPuffs_) {
        writeRow("fx", puff.ownerSerial, "torpedo_puff", puff.age, 0,
                 puff.x, puff.y, 0.0f, 0.0f, 0, 0, 0, 0);
    }

    const std::string stage = activeStageId_.str();
    const int transitionState = parityRoomTransitionTimer_ > 0 ? 2 : (stageClear_ ? 1 : 0);
    const int transitionTimer = parityRoomTransitionTimer_ > 0
        ? parityRoomTransitionTimer_
        : stageClearTimer_;
    writeRow("transition", 0, stage.c_str(), transitionState, 0,
             player_.position.x, player_.position.y, 0.0f, 0.0f,
             transitionTimer, bossLocked_ ? 1 : 0, returnToStageSelect_ ? 1 : 0,
             returnToTitle_ ? 1 : 0);

    std::fflush(parityTrace_);
}

void GameplayScene::activateSourceEnemiesAfterCamera(std::uint16_t previousCameraX) {
    if (pendingSourceEnemySerials_.empty()) return;
    // The tagged stage placement uses in-range, unshaken camera coordinates.
    // Keep the existing floor conversion and native Y anchor unchanged.
    const auto cameraX = sourceCameraCoordinate(camera_.baseX());
    const auto cameraY = sourceCameraCoordinate(camera_.baseY());
    pendingSourceEnemySerials_.erase(
        std::remove_if(pendingSourceEnemySerials_.begin(), pendingSourceEnemySerials_.end(),
            [&](int serial) {
                const auto it = std::find_if(enemies_.begin(), enemies_.end(),
                    [serial](const Enemy& enemy) { return enemy.serial == serial; });
                if (it == enemies_.end()) return true;
                if (!gameplay_enemies::sourceRightwardCameraBucketReached(
                        previousCameraX, cameraX, cameraY,
                        sourceCameraCoordinate(it->position.x),
                        sourceCameraCoordinate(it->position.y))) {
                    return false;
                }
                // Enemy update, physics and contact already finished this
                // tick. Setup starts next tick, as in source frame 1159.
                it->active = true;
                it->cameraActivated = true;
                return true;
            }),
        pendingSourceEnemySerials_.end());
}

void GameplayScene::updateEnemies() {
    const float camX = camera_.x();
    for (auto& e : enemies_) {
        if (!e.active) continue;
        if (e.recycleForCameraExit(camX, camera_.y())) continue;

        // R4.6: a placed enemy stands still until the camera brings it into
        // the current activation window.
        if (!e.cameraActivated) {
            if (!gameplay_enemies::withinCameraActivationWindow(
                    e.position.x, camX, static_cast<float>(INTERNAL_WIDTH))) {
                continue;
            }
            e.cameraActivated = true;
        }

        e.update(0);
        physics::moveAndCollide(e, tilemap_);

        // Share the scene adapter with the compiled projectile contracts.
        gameplay_enemies::drainPendingShots(e, projectiles_);
    }

    enemies_.erase(
        std::remove_if(enemies_.begin(), enemies_.end(),
                       [this](const Enemy& e) {
                           return !e.active &&
                               std::find(pendingSourceEnemySerials_.begin(),
                                         pendingSourceEnemySerials_.end(), e.serial) ==
                                   pendingSourceEnemySerials_.end();
                       }),
        enemies_.end()
    );
}

void GameplayScene::updateStageObjects() {
    for (auto& obj : stageObjects_) {
        if (!obj.active) continue;
        obj.update(0);
    }

    stageObjects_.erase(
        std::remove_if(stageObjects_.begin(), stageObjects_.end(),
                       [](const StageObject& obj) { return !obj.active; }),
        stageObjects_.end());
}

void GameplayScene::updateProjectiles() {
    for (auto& p : projectiles_) {
        if (!p.active) continue;

        // Rolling Shield charged (oracle 2026-06-11): the shield object is
        // GLUED to X's anchor — re-fed every tick (it rode along x 291->451
        // in s6_absorb while X drove right).
        if (p.absorbShield && p.isPlayerShot) {
            const AABB hb = player_.getHitbox();
            p.prevPosition = p.position;
            p.position.x = (hb.x + hb.w * 0.5f) - p.hitboxSize.x * 0.5f;
            p.position.y = (hb.y + hb.h * 0.5f) - p.hitboxSize.y * 0.5f;
        }

        // Boomerang Cutter (oracle 2026-06-11): steering targets the LIVE
        // player center each frame, and the catch (X overlap after the
        // straight phase) despawns the cutter and REFUNDS the ammo.
        if (p.boomerang && p.isPlayerShot) {
            p.returnX = player_.position.x + player_.spriteWidth * 0.5f;
            p.returnY = player_.position.y + player_.spriteHeight * 0.5f;
            if (p.boomSteer && p.boomerangTimer > p.boomStraightFrames) {
                const AABB pb = player_.getHitbox();
                const AABB cb = p.getHitbox();
                if (pb.overlaps(cb)) {
                    p.active = false;
                    if (p.boomCatchRefund) {
                        auto& inv = player_.weaponInventory;
                        for (size_t i = 0; i < inv.weaponCount(); ++i) {
                            if (inv.weaponAt(i).id == p.weaponId) {
                                inv.refillAmmo(static_cast<int>(i), 1);
                                break;
                            }
                        }
                    }
                    continue;
                }
            }
        }

        // Charged torpedo fan homing gate (w70iso_REPORT 2026-06-10): each
        // member acquires the nearest LIVE enemy ONCE at release (same
        // Chebyshev rule as the normal shot) and NEVER re-acquires — the
        // projectile steers only if that exact target is still alive at
        // countdown expiry, and keeps steering only while it lives (target
        // death after steering began was never observed; the engine holds
        // heading via homingHasTarget=false). Identity by enemy serial —
        // enemies_ is compacted every frame, indices move.
        if (p.torpedoFanMember && p.isPlayerShot) {
            if (!p.fanAcquired) {
                const float px = p.position.x + p.hitboxSize.x * 0.5f;
                const float py = p.position.y + p.hitboxSize.y * 0.5f;
                // U31 acquisition gate: only ON-CAMERA targets are
                // acquirable (range_w* graded runs: every torpedo flew
                // dead straight while the cannon sat past the screen edge
                // at ANY distance — incl. 30px past — and the one steer
                // case began exactly when the camera reached it). This is
                // also w70's no-steer: that fan's target was off-camera.
                const float camL = camera_.x(), camT = camera_.y();
                auto onCam = [&](float cx, float cy) {
                    return cx >= camL && cx <= camL + INTERNAL_WIDTH
                        && cy >= camT && cy <= camT + INTERNAL_HEIGHT;
                };
                float best = 1e9f;
                auto consider = [&](float cx, float cy, int serial, bool isBoss) {
                    if (!onCam(cx, cy)) return;
                    const float adx = std::fabs(cx - px);
                    const float ady = std::fabs(cy - py);
                    const float cheb = adx > ady ? adx : ady;
                    if (cheb < best) {
                        best = cheb;
                        p.fanTargetSerial = serial;
                        p.fanTargetIsBoss = isBoss;
                    }
                };
                for (auto& e : enemies_) {
                    if (!e.active || e.enemyState == EnemyState::Dead || e.health <= 0)
                        continue;
                    const AABB eb = e.getHitbox();
                    consider(eb.x + eb.w * 0.5f, eb.y + eb.h * 0.5f, e.serial, false);
                }
                if (bossActive_ && boss_.active && boss_.bossState != BossState::Dormant &&
                    boss_.bossState != BossState::Dead) {
                    const AABB bb = boss_.getHitbox();
                    consider(bb.x + bb.w * 0.5f, bb.y + bb.h * 0.5f, -1, true);
                }
                p.fanAcquired = true;
            }
            // Resolve the acquired identity -> liveness + live center.
            bool alive = false;
            if (p.fanTargetIsBoss) {
                if (bossActive_ && boss_.active && boss_.bossState != BossState::Dormant &&
                    boss_.bossState != BossState::Dead) {
                    const AABB bb = boss_.getHitbox();
                    p.returnX = bb.x + bb.w * 0.5f;
                    p.returnY = bb.y + bb.h * 0.5f;
                    alive = true;
                }
            } else if (p.fanTargetSerial >= 0) {
                for (auto& e : enemies_) {
                    if (e.serial != p.fanTargetSerial) continue;
                    if (e.active && e.enemyState != EnemyState::Dead && e.health > 0) {
                        const AABB eb = e.getHitbox();
                        p.returnX = eb.x + eb.w * 0.5f;
                        p.returnY = eb.y + eb.h * 0.5f;
                        alive = true;
                    }
                    break;
                }
            }
            p.fanTargetAlive = alive;
            if (p.homing) p.homingHasTarget = alive;
        }

        // Homing Torpedo target selection (oracle 2026-06-12 + U31): nearest
        // LIVE enemy by CHEBYSHEV distance max(|dx|,|dy|) — but only among
        // ON-CAMERA targets (U31 range_w* graded runs: off-screen enemies
        // are invisible to homing at ANY distance, even 30px past the edge;
        // a target entering the view MID-FLIGHT is acquired then — w180
        // slot1 steered 11f after the cannon scrolled in). Rear targets
        // included; dead slots excluded (66/66 acquisition events,
        // fit_targeting.py — all with on-screen targets). The projectile's
        // own update() does the steering; the scene only feeds the live
        // target center. Converted fan members are fed above (acquired
        // identity only — no re-acquire).
        if (p.homing && p.isPlayerShot && !p.torpedoFanMember) {
            const float px = p.position.x + p.hitboxSize.x * 0.5f;
            const float py = p.position.y + p.hitboxSize.y * 0.5f;
            const float camL = camera_.x(), camT = camera_.y();
            float best = 1e9f;
            float tx = 0, ty = 0;
            bool found = false;
            auto consider = [&](float cx, float cy) {
                if (cx < camL || cx > camL + INTERNAL_WIDTH
                    || cy < camT || cy > camT + INTERNAL_HEIGHT) return;
                const float adx = std::fabs(cx - px);
                const float ady = std::fabs(cy - py);
                const float cheb = adx > ady ? adx : ady;
                if (cheb < best) {
                    best = cheb;
                    tx = cx; ty = cy;
                    found = true;
                }
            };
            for (auto& e : enemies_) {
                if (!e.active || e.enemyState == EnemyState::Dead || e.health <= 0)
                    continue;
                const AABB eb = e.getHitbox();
                consider(eb.x + eb.w * 0.5f, eb.y + eb.h * 0.5f);
            }
            if (bossActive_ && boss_.active && boss_.bossState != BossState::Dormant &&
                boss_.bossState != BossState::Dead) {
                const AABB bb = boss_.getHitbox();
                consider(bb.x + bb.w * 0.5f, bb.y + bb.h * 0.5f);
            }
            p.homingHasTarget = found;
            if (found) {
                p.returnX = tx;
                p.returnY = ty;
            }
        }

        p.update(0);

        // Chameleon Sting: the muzzle bolt spawns its 3-dart fan at the
        // measured tick of its life (claim at muzzle age 20; darts hold one
        // tick then move — the oracle claim -> velocity -> move ladder).
        if (p.stingMuzzle && !p.stingFanSpawned
            && p.ageFrames >= p.stingFanTick) {
            p.stingFanSpawned = true;
            // 0x67 — the sting's second weapon-specific event id, at the
            // dart claim (sfx_s1/s3 sf52; fires for EVERY volley incl. the
            // sub-threshold-release one, subthresh_cur2 sf172).
            AudioManager::playApu(p.sfxFanRelease);
            spawnStingFan(p);
        }

        // Rolling Shield charged hum: 0x64 first at release+47 then every
        // 16f while the shield lives (s8_charged f319+ — the first measured
        // per-weapon loop; the SPC does NOT sustain this one, the CPU
        // re-sends it).
        if (p.absorbShield && p.isPlayerShot && p.sfxShieldHum >= 0
            && p.sfxShieldHumEvery > 0 && p.ageFrames >= p.sfxShieldHumFirst
            && (p.ageFrames - p.sfxShieldHumFirst) % p.sfxShieldHumEvery == 0) {
            AudioManager::playApu(p.sfxShieldHum);
        }

        // Fire Wave charged ground wave (U29 re-measure): the MOVING head
        // drops a terrain-snapped flame at its previous-frame anchor every
        // waveEveryFrames (wakes land +12 then +15 apart: hold-tick + 1.5/f).
        // Termination = the head's own death (wall / +32 margin), not a cap.
        if (p.waveHead && p.waveSegmentsSpawned < p.waveMaxSegments
            && p.waveEveryFrames > 0
            // ageFrames hits 1 on the head's first traced tick, so +1 keeps
            // the measured head->first-segment gap (s3_long f272->f282 = 10).
            && p.ageFrames >= (p.waveSegmentsSpawned + 1) * p.waveEveryFrames + 1) {
            // 0x61 re-sent per planted flame (fwave_full sf283,293,303,...;
            // the real param byte is position-correlated panning — mono here).
            if (spawnWaveSegment(p)) {
                AudioManager::playApu(p.sfxSegmentPlant);
                p.waveSegmentsSpawned++;
            }
        }

        // Despawn buster shots the instant they leave the visible screen, like
        // the real game — this frees the 3-on-screen slot immediately so the
        // player can fire again without waiting for the bullet to reach a far
        // world bound. The STEERED boomerang is exempt — the oracle shows it
        // surviving offscreen and returning (s9_offscreen: 16px past the left
        // edge, came back and caught X). Charged cutter giants (no steer) DO
        // margin-die (~60px, oracle 55-65), as does the homing torpedo (~30px
        // past the edge: s1_air x=286.5 / s1_left x=-31 with cam_x=0). The
        // fire-wave head margins at +32 and its wake at +0 (U29: the cp head
        // died at x321 = edge+32 exact; wake stubs died 2f past edge 289) —
        // both via despawnMarginPx, no exemption.
        if (p.active && p.isPlayerShot && !(p.boomerang && p.boomSteer) &&
            (p.type == ProjectileType::Normal || p.type == ProjectileType::ChargeL1 ||
             p.type == ProjectileType::ChargeL2 || p.type == ProjectileType::ChargeL3)) {
            const float camX = camera_.x();
            const float camY = camera_.y();
            // Oracle-measured despawn margins: ice pellet ~24px past the
            // screen edge, e-spark 32, charged e-spark 32/40 (per-projectile,
            // set at fire time from the weapon KB), the wide ice sled ~64px.
            const float margin = p.rideable ? 64.0f : p.despawnMarginPx;
            if (p.position.x < camX - margin ||
                p.position.x > camX + INTERNAL_WIDTH + margin ||
                p.position.y < camY - margin ||
                p.position.y > camY + INTERNAL_HEIGHT + margin) {
                p.active = false;
            }
        }

        if (!p.active) continue;

        // Electric Spark split children pierce ALL terrain (oracle: the
        // down-slope child crossed the floor and died off-screen).
        if (p.ignoresTerrain) continue;

        // Tilemap collision — destroy on hitting solid tiles
        AABB box = p.getHitbox();
        int ts = tilemap_.tileSize();

        // Charged Fire Wave is floor-bound in the source game: when the head
        // reaches a gap, it dies instead of falling through and re-landing on
        // the far lip. Keep this on waveHead only; the ice sled still uses the
        // broader groundFollow terrain pass below.
        if (p.waveHead && p.groundFollow && p.ageFrames > p.launchDelayFrames) {
            const auto supportRange = physics::projectileGroundSupportTileRange(
                box, ts, 96.0f);
            bool hasSupport = false;
            for (int row = supportRange.startRow;
                 row <= supportRange.endRow && !hasSupport; ++row) {
                for (int col = supportRange.startCol; col <= supportRange.endCol; ++col) {
                    if (isProjectileFloorSupport(tilemap_.getTileType(col, row))) {
                        hasSupport = true;
                        break;
                    }
                }
            }
            if (!hasSupport) {
                p.active = false;
                continue;
            }
        }

        // U277 clean-wall capture: detect side bounces at the leading
        // rendered edge, but resolve as the SNES trace does: one opposite
        // step from the previous frame, y unchanged, no tile-face eject snap.
        if (p.rolling && p.bouncesOffWalls && p.wallBouncesLeft > 0
            && p.vx != 0.0f && p.visualFrameWidth > 0
            && p.ageFrames > p.launchDelayFrames) {
            const auto wallRange = physics::rollingShieldVisualWallTileRange(
                box, ts, p.vx, p.visualOffsetX,
                static_cast<float>(p.visualFrameWidth), p.visualScale);
            bool visualWallBounced = false;
            for (int row = wallRange.startRow; row <= wallRange.endRow; ++row) {
                for (int col = wallRange.startCol; col <= wallRange.endCol; ++col) {
                    if (!tilemap_.isSolid(col, row)) continue;
                    const float tileTop = static_cast<float>(row * ts);
                    const float tileBottom = static_cast<float>((row + 1) * ts);
                    const float projBottom = box.bottom();
                    if (p.vy > 0.0f && projBottom > tileTop && projBottom < tileBottom
                        && !tilemap_.isSolid(col, row - 1)) {
                        continue;
                    }
                    p.wallBouncesLeft--;
                    p.position.x = physics::rollingShieldWallSnapX(
                        p.prevPosition.x, p.vx, p.hitboxOffset.x, p.hitboxSize.x,
                        p.visualOffsetX, static_cast<float>(p.visualFrameWidth),
                        p.visualScale);
                    p.position.y = p.prevPosition.y;
                    p.vx = -p.vx;
                    p.facingRight = p.vx > 0.0f;
                    visualWallBounced = true;
                    break;
                }
                if (visualWallBounced) break;
            }
            if (visualWallBounced) continue;
        }
        // Moving shots contact terrain through their collision anchor. A full
        // footprint remains necessary for floor-riding projectile behaviors.
        const bool fullBodyProbe = p.mineHazard || p.rolling || p.groundFollow;
        const auto tileRange = physics::projectileTerrainTileRange(
            box, ts, p.vx, p.vy, fullBodyProbe);
        int startCol = tileRange.startCol;
        int startRow = tileRange.startRow;
        int endCol = tileRange.endCol;
        int endRow = tileRange.endRow;

        bool rollingLanded = false;
        for (int row = startRow; row <= endRow && p.active && !rollingLanded; row++) {
            for (int col = startCol; col <= endCol && p.active; col++) {
                if (tilemap_.isSolid(col, row)) {
                    if (p.mineHazard) {
                        // U19 bat mine: lands and STOPS dead (the real
                        // mine sat stationary 212f+ to log end); side
                        // contact also stops it (unmeasured — gentler
                        // than destroy). The per-frame re-snap keeps it
                        // stable against the gravity tick.
                        const float tileTop = static_cast<float>(row * ts);
                        if (p.vy > 0 && box.bottom() > tileTop) {
                            p.position.y = tileTop - p.hitboxSize.y;
                        }
                        p.vx = 0;
                        p.vy = 0;
                        rollingLanded = true;
                        break;
                    }
                    if (p.rolling) {
                        // Rolling Shield: land on the ground, stop on walls
                        float tileTop = static_cast<float>(row * ts);
                        float tileBottom = static_cast<float>((row + 1) * ts);
                        float projBottom = box.bottom();

                        // Landing on top of tile (falling). BREAK out of the
                        // whole tile scan: the box is stale after the snap,
                        // and the neighboring floor tile would re-enter this
                        // branch with vy==0 and wrongly take the wall-kill
                        // path (the oracle ball settles once and ROLLS —
                        // _rolling_shield_runs s1_air +2px one-tick settle).
                        if (p.vy > 0 && projBottom > tileTop && projBottom < tileBottom) {
                            p.position.y = tileTop - p.hitboxSize.y;
                            p.vy = 0;
                            rollingLanded = true;
                            break;
                        }
                        // U42/U76: a vertical wall BOUNCES the ball — exact
                        // vx flip, speed preserved, clipped flush — but only
                        // ONCE: the second wall contact destroys it (u76
                        // ball_left_long/ball_right_long, both orders, both
                        // wall types). Objects/enemies still consume it.
                        if (p.bouncesOffWalls && p.wallBouncesLeft > 0) {
                            p.wallBouncesLeft--;
                            p.position.x = physics::rollingShieldWallSnapX(
                                p.prevPosition.x, p.vx, p.hitboxOffset.x,
                                p.hitboxSize.x, p.visualOffsetX,
                                static_cast<float>(p.visualFrameWidth),
                                p.visualScale);
                            p.position.y = p.prevPosition.y;
                            p.vx = -p.vx;
                            p.facingRight = p.vx > 0.0f;
                            rollingLanded = true;   // box is stale after the snap
                            break;
                        }
                        // Hit wall from the side — destroy
                    }

                    if (p.groundFollow) {
                        // Charged Shotgun Ice sled (oracle 2026-06-09 +
                        // s6_ride_deep re-verify): rides along the ground and
                        // climbs gentle slopes, but a wall face taller than a
                        // slope step DESTROYS it on contact — silently, with
                        // NO damaging fragments (real CP run: a ~12px step
                        // killed it on-screen; the long ascent slope was
                        // climbed fine; cosmetic burst likely lives in the
                        // SNES explosion table $7E1928, not the bullet table).
                        float tileTop = static_cast<float>(row * ts);
                        float tileBottom = static_cast<float>((row + 1) * ts);
                        float projBottom = box.bottom();
                        if (p.vy > 0 && projBottom > tileTop && projBottom < tileBottom) {
                            p.position.y = tileTop - p.hitboxSize.y;
                            p.vy = 0;
                            continue;
                        }
                        // Side contact: slope-follow if the rise is small.
                        constexpr float kSledStepUpMaxPx = 6.0f;
                        const float stepRise = projBottom - tileTop;
                        if (stepRise <= kSledStepUpMaxPx && !tilemap_.isSolid(col, row - 1)) {
                            p.position.y = tileTop - p.hitboxSize.y;
                            p.vy = 0;
                            continue;
                        }
                        // U77: the ice break-up is shotgun-ice-only; the
                        // fire-wave head groundFollows too but dies with no
                        // burst (U29 sm_wave_wall).
                        if (p.sledDebrisOnWall) {
                            spawnSledDebris(p);
                            // U89 review: the clean widened APU trace has no
                            // command at the real sled wall-break frame.
                            if (p.sfxSledBreak >= 0) {
                                AudioManager::playApu(p.sfxSledBreak);
                            }
                        }
                        p.active = false;   // destroyed; no shatter children
                        continue;
                    }

                    // Shotgun Ice: shatter into fragments flying backward.
                    // NO impact burst on plain terrain (oracle s5_face_burst
                    // 2026-06-10, David's report: the 456-face shatter shows
                    // ZERO burst OAM while the log-OBJECT shatter shows the
                    // full 13-frame sequence — WP-B's "every impact" had only
                    // object samples). Burst stays on ENEMY impacts.
                    if (p.shattersOnWallHit && p.shatterCount > 0) {
                        spawnShatterFragments(p);
                    }
                    // Electric Spark: split into up/down projectiles on wall hit
                    if (p.splitsOnWallHit && p.splitCount > 0) {
                        spawnWallSplitProjectiles(p);
                    }
                    p.active = false;
                }
            }
        }
    }

    projectiles_.erase(
        std::remove_if(projectiles_.begin(), projectiles_.end(),
                       [](const Projectile& p) { return !p.active; }),
        projectiles_.end()
    );
}

void GameplayScene::checkBulletEnemyCollision() {
    for (auto& p : projectiles_) {
        if (!p.active || !p.isPlayerShot) continue;
        if (p.dormantFrames > 0) continue;  // slot not yet claimed (e-spark twin)

        AABB pBox = p.getHitbox();

        for (auto& e : enemies_) {
            if (!e.active || !e.cameraActivated ||
                e.enemyState == EnemyState::Dead) continue;

            AABB eBox = e.getHitbox();
            if (!pBox.overlaps(eBox)) continue;
            if (p.usesOneHitPerTargetGate() && p.hasHitEnemySerial(e.serial)) continue;

            // Per-enemy damage tables — the real-MMX mechanism (lookup
            // rules in enemy_damage.h; sparse tables fall back to the
            // projectile's own damage).
            auto form = enemy_damage::HitForm::Normal;
            if (p.isShatterFragment) form = enemy_damage::HitForm::Fragment;
            else if (p.type != ProjectileType::Normal) form = enemy_damage::HitForm::Charged;
            const auto defIt = Enemy::definitions.find(e.type);
            const int dmg = (defIt != Enemy::definitions.end())
                ? enemy_damage::damageFor(defIt->second, p.weaponId, form, p.damage)
                : p.damage;

            // A measured 0-damage pairing is a full PASS-THROUGH: no hit
            // flash, no projectile death (oracle 2026-06-10: e-spark crossed
            // the live walker-0x51 with no HP change and kept flying).
            if (dmg == 0) continue;

            // Continuous-damage weapons (fire-wave stream/wave, oracle
            // 2026-06-11): at most ONE decrement per enemy per
            // damageTickFrames, however many segments overlap — the enemy
            // hitFlash counter (set to 4 on hit, -1/frame) is the window:
            // tick=1 blocks only same-frame doubles, tick=2 blocks the next
            // frame too (the wave's measured 2f cadence).
            if (p.continuousDamage && e.hitFlash > 4 - p.damageTickFrames)
                continue;

            bool damagedEnemy = false;
            bool killedEnemy = false;
            if (!e.invulnerable) {
                if (p.usesOneHitPerTargetGate()) {
                    p.markHitEnemySerial(e.serial);
                }
                e.health -= dmg;
                e.hitFlash = 4; // white damage flash (real game blanks enemy white)
                damagedEnemy = true;
                if (e.health <= 0) {
                    killedEnemy = true;
                    e.enemyState = EnemyState::Dead;
                    e.alive = false;
                    e.deathTimer = 0;
                    AudioManager::playSFX(SFX::EnemyDeath);
                    SaveSystem::incrementEnemiesDefeated();
                    // Spawn drop on kill — RNG-gated like the real game
                    // (enemy_defs dropChancePct/dropTypes; axemax E5 sample)
                    if (int t = e.rollDropType(rand() % 100, rand()); t > 0) {
                        spawnDrop(e.position.x, e.position.y, t);
                    }
                    e.dropType = 0;
                    e.dropTypes.clear();
                } else {
                    // FW7-REVIEW-C: one exact palette-0 flash frame on a
                    // surviving hit (source f447 targetFlashProof).
                    e.hitFlashVisual = 1;
                    AudioManager::playApu(0x11);   // measured hit impact (R6)
                }
            }

            dispatchNormalBusterContactEffects(p, e.serial, eBox, damagedEnemy, killedEnemy);
            if (p.shouldConsumeAfterEnemyHit(killedEnemy)) {
                if (gameplay_buster_impact::isEligible({
                        p.type == ProjectileType::ChargeL1, p.weaponId,
                        damagedEnemy, killedEnemy})) {
                    spawnBusterImpact(p, e.serial);
                }
                // Shotgun Ice shatters on ENEMY impact too — even when the
                // enemy survives (oracle r5 f806: cannon at 6HP got the fan).
                if (p.shattersOnWallHit && !p.shatterSpecs.empty()) {
                    spawnShatterFragments(p);
                    spawnImpactBurst(p.position.x + p.hitboxSize.x * 0.5f,
                                     p.position.y + p.hitboxSize.y * 0.5f);
                    AudioManager::playApu(p.sfxShatter);
                }
                p.active = false;
            }
            break; // Each bullet hits one enemy per frame
        }
    }
}

bool GameplayScene::consumeAbsorbShield() {
    // Rolling Shield charged (oracle s6_absorb/s6_absorb2 2026-06-11): the
    // shield absorbs exactly ONE hit and dies; X takes no damage from it
    // (X's first HP loss came 115f after the shield died).
    for (auto& p : projectiles_) {
        if (p.active && p.isPlayerShot && p.absorbShield) {
            p.active = false;
            return true;
        }
    }
    return false;
}

void GameplayScene::checkPlayerEnemyCollision() {
    if (player_.isInvulnerable()) return;
    if (player_.isDead()) return;

    AABB playerBox = player_.getHitbox();

    const bool canonicalX = characterPath == "content/x1/characters/x.json";

    for (auto& e : enemies_) {
        if (!e.active || !e.cameraActivated ||
            e.enemyState == EnemyState::Dead) continue;

        AABB enemyBox = e.getHitbox();
        const auto source = canonicalX
            ? gameplay_enemy_contact::resolveSourcePlayerEnemyContact(player_, e)
            : gameplay_enemy_contact::SourceContactResult{};
        if (!(source.sourceSupported ? source.hit : playerBox.overlaps(enemyBox))) continue;

        if (consumeAbsorbShield()) break;   // the shield eats the hit

        // Determine knockback direction: push player away from enemy center
        float dir = (player_.position.x < e.position.x) ? -1.0f : 1.0f;
        player_.takeDamage(e.contactDamage, dir);
        AudioManager::playApu(0x09);   // measured hurt cry (R6)
        camera_.shake(3.0f, 10);
        break; // Only take damage from one enemy per frame
    }
}

void GameplayScene::checkPlayerStageObjectCollision() {
    if (player_.isInvulnerable()) return;
    if (player_.isDead()) return;

    AABB playerBox = player_.getHitbox();

    for (auto& obj : stageObjects_) {
        if (!obj.active || obj.contactDamage() <= 0) continue;

        AABB objectBox = obj.getHitbox();
        if (!playerBox.overlaps(objectBox)) continue;

        if (consumeAbsorbShield()) break;

        const float objectCenter = objectBox.x + objectBox.w * 0.5f;
        const float playerCenter = playerBox.x + playerBox.w * 0.5f;
        const float dir = (playerCenter < objectCenter) ? -1.0f : 1.0f;
        player_.takeDamage(obj.contactDamage(), dir);
        AudioManager::playApu(0x09);   // measured hurt cry (R6)
        camera_.shake(3.0f, 10);
        break;
    }
}

void GameplayScene::checkEnemyShotPlayerCollision() {
    if (player_.isInvulnerable()) return;
    if (player_.isDead()) return;

    AABB playerBox = player_.getHitbox();

    for (auto& p : projectiles_) {
        if (!p.active || p.isPlayerShot) continue; // Only check enemy shots

        AABB shotBox = p.getHitbox();
        if (!playerBox.overlaps(shotBox)) continue;

        if (consumeAbsorbShield()) {        // the shield eats the hit
            p.active = false;               // (s6_absorb: died near the
            break;                          //  firing cannon — shot absorbed)
        }

        float dir = (player_.position.x < p.position.x) ? -1.0f : 1.0f;
        player_.takeDamage(p.damage, dir);
        AudioManager::playApu(0x09);   // measured hurt cry (R6)
        camera_.shake(2.0f, 8);
        p.active = false;
        break;
    }
}

void GameplayScene::spawnShatterFragments(const Projectile& source) {
    // Oracle-measured shatter (2026-06-09, knowledge_base/_shotgun_ice_runs/):
    // ALL fragments spawn at the parent's death position 1 frame after impact,
    // fly BACKWARD in a deterministic 5-vector fan at speed 8.0, zero gravity,
    // and despawn off-screen or on terrain without re-shattering. Specs are
    // recorded for a right-fired parent; mirror vx for left shots.
    const float mirror = (source.vx >= 0) ? 1.0f : -1.0f;
    const float cx = source.position.x + source.hitboxSize.x / 2;
    const float cy = source.position.y + source.hitboxSize.y / 2;

    for (const auto& s : source.shatterSpecs) {
        Projectile frag;
        // Fragment CENTER sits exactly on the parent's death center (oracle:
        // all 5 spawn at the parent's position) — 6x6 hitbox, so -3,-3.
        frag.init(cx - 3.0f, cy - 3.0f, mirror * s.vx, s.vy, ProjectileType::Normal);
        frag.isPlayerShot = source.isPlayerShot;
        // Real game: fragments hit the SAME per-enemy damage table entry as
        // the pellet (cannon-verified 2 = source.damage approximation).
        frag.damage = source.damage;
        frag.projGravity = s.gravity;
        frag.isShatterFragment = true; // per-enemy damage: fragment form
        frag.lifetime = 120; // bounded; real fragments die off-screen/terrain
        frag.hitboxSize = {6, 6};
        frag.color = source.color;
        frag.weaponId = source.weaponId;
        // Use real ripped ice-chip sprite instead of buster placeholder
        frag.visualSpritePath = "content/x1/sprites/weapons/shotgun_ice_fragment.png";
        frag.visualFrameWidth  = 8;
        frag.visualFrameHeight = 8;
        frag.visualFrameCount  = 1;
        // Fragments don't shatter again
        frag.shattersOnWallHit = false;
        frag.shatterCount = 0;
        pendingProjectileSpawns_.push_back(frag);
    }
}

void GameplayScene::updateIceTrail() {
    // Physics + despawn FIRST so a bit's first visible tick is its spawn
    // position (matches the OAM: bit appears at the pellet center, moves
    // from the next frame on).
    for (auto& b : iceTrailBits_) {
        b.vy += b.gravity;
        b.x += b.vx;   // pellet-trail bits hold x (vx 0); sled debris arcs
        b.y += b.vy;
        b.age++;
    }
    // Despawn rules (WP-B cosmetic_layout.json):
    //   trail bits  — EXACTLY lifetime(=30) ticks, mid-air at any height
    //                 (cos_s5_apex: a timer, NOT terrain contact);
    //   break pieces — off-screen only (oracle pieces fell THROUGH floors
    //                  and died just past the screen edges; no timer).
    const float viewL = camera_.x() - 24.0f;
    const float viewR = camera_.x() + 256.0f + 24.0f;
    const float viewT = camera_.y() - 24.0f;
    const float viewB = camera_.y() + 224.0f + 24.0f;
    iceTrailBits_.erase(
        std::remove_if(iceTrailBits_.begin(), iceTrailBits_.end(),
                       [&](const IceTrailBit& b) {
                           if (b.kind == 0) return b.age >= b.lifetime;
                           return b.x < viewL || b.x > viewR
                               || b.y < viewT || b.y > viewB;
                       }),
        iceTrailBits_.end());

    // Impact bursts: 21-frame playback strip (cos_s2 records: 22f main +
    // 7f secondary composed offline by build_s7_content.py).
    for (auto& ib : iceImpactBursts_) ib.age++;
    iceImpactBursts_.erase(
        std::remove_if(iceImpactBursts_.begin(), iceImpactBursts_.end(),
                       [](const IceImpactBurst& ib) { return ib.age >= 21; }),
        iceImpactBursts_.end());

    // Charged-release flash: 8 sparkles around X for ONE frame on the tick
    // the charged shot is born (cos_s3 oam_sf0272: single-frame presence).
    // The torpedo fan release shows the SAME pattern (pair geometry
    // identical) in torpedo colors, measured rel the player RAM anchor.
    if (releaseFlashFrames_ > 0) releaseFlashFrames_--;
    for (const auto& p : projectiles_) {
        if (!p.active || !p.isPlayerShot || p.ageFrames != 1) continue;
        if (p.rideable) {
            releaseFlashX_ = player_.position.x;
            releaseFlashY_ = player_.position.y;
            releaseFlashKind_ = 0;
            releaseFlashFrames_ = 1;
        } else if (p.torpedoFanMember) {
            // WP-C player RAM anchor, mirrored within the 70px cell.
            releaseFlashX_ = player_anchor::sourceRamAnchorX(
                player_.position.x, player_.spriteWidth, player_.facingRight);
            releaseFlashY_ = player_.position.y + 37.0f;
            releaseFlashKind_ = 1;
            releaseFlashFrames_ = 1;
        } else if (p.type == ProjectileType::ChargeL3 && p.weaponId == "buster" &&
                   p.sinePhaseFrames == 0) {
            // U45/U103: the ARM L3 release has an 11-frame pink flare/crescent
            // sequence before the static orb cluster fully takes over.
            releaseFlashX_ = player_anchor::sourceRamAnchorX(
                player_.position.x, player_.spriteWidth, player_.facingRight);
            releaseFlashY_ = player_.position.y + 37.0f;
            releaseFlashKind_ = 2;
            releaseFlashFrames_ = 11;
        }
    }

    // Shed: one bit per trailEveryFrames of pellet age, centered on the
    // pellet (OAM: bit center == pellet sprite center at spawn).
    for (const auto& p : projectiles_) {
        if (!p.active || !p.isPlayerShot || p.trailEveryFrames <= 0) continue;
        if (p.ageFrames <= 0 || (p.ageFrames % p.trailEveryFrames) != 0) continue;
        IceTrailBit bit;
        bit.x = p.position.x + p.hitboxSize.x * 0.5f;
        bit.y = p.position.y + p.hitboxSize.y * 0.5f;
        bit.vx = 0.0f;   // trail bits hold x (oracle: stationary, fall only)
        bit.vy = p.trailRiseVy;
        bit.gravity = p.trailGravity;
        bit.age = 0;
        bit.serial = ++iceTrailSerial_;
        bit.kind = 0;
        bit.lifetime = 30;  // exact (effect-table episodes, all == 30)
        iceTrailBits_.push_back(bit);
    }
}

void GameplayScene::updateTorpedoSmoke() {
    // Homing Torpedo smoke (S7 2026-06-10): age + prune FIRST so a puff's
    // first visible tick is its spawn position; lifetime EXACTLY 7 frames
    // (CE,CF,CF,D0,D0,D1,D1) and the trail vanishes with its owner slot
    // (s1_vram: both trailing puffs truncated at the torpedo margin death).
    for (auto& s : torpedoPuffs_) s.age++;
    torpedoPuffs_.erase(
        std::remove_if(torpedoPuffs_.begin(), torpedoPuffs_.end(),
                       [&](const TorpedoPuff& s) {
                           if (s.age >= 7) return true;
                           for (const auto& p : projectiles_) {
                               if (p.active && p.serial == s.ownerSerial)
                                   return false;
                           }
                           return true;
                       }),
        torpedoPuffs_.end());

    // Shed: every 3 frames of age (first puff at fire+3, slot byte +0x3E
    // cycling 3,2,1), laid down IN THE WAKE — the body center at the
    // PREVIOUS puff tick, +2px nose-ward (fits the 9 s1 samples +-0.7px;
    // measured fired-right, offset mirrored for left). Both tiers emit.
    for (auto& p : projectiles_) {
        if (!p.active || !p.isPlayerShot || p.weaponId != "homing-torpedo")
            continue;
        if (!p.homing && !p.torpedoFanMember) continue;
        const float cx = p.position.x + p.hitboxSize.x * 0.5f;
        const float cy = p.position.y + p.hitboxSize.y * 0.5f;
        if (p.ageFrames <= 1) {
            // First sight: holdFirstTick kept the spawn-tick center exact.
            p.puffWakeX = cx;
            p.puffWakeY = cy;
            continue;
        }
        if (p.ageFrames % 3 == 0) {
            torpedoPuffs_.push_back(
                {p.puffWakeX + (p.facingRight ? 2.0f : -2.0f), p.puffWakeY,
                 0, p.serial});
            p.puffWakeX = cx;
            p.puffWakeY = cy;
        }
    }
}

void GameplayScene::spawnSledDebris(const Projectile& sled) {
    // MEASURED wall-break burst (WP-B cos_s6_face f1021; effect-table slots
    // $7E0C98..$7E0D38, cosmetic_layout.json): 6 pieces — 4 shards (tile 54)
    // + 2 16x8 chunks (tiles 38+39) — all spawn at ONE point (the sled RAM
    // anchor; engine approximation: hitbox center, WP-C bridges anchors
    // exactly), exact vectors, gravity 48/256, despawn off-screen only.
    // Silent and harmless (no bullet rows, no APU write). vx as measured on
    // a LEFT-moving sled; mirrored for right-moving.
    struct P { int kind; bool hflip; float vx, vyUp; };
    static const P kPieces[] = {
        {1, true,   1.0f, 4.5f}, {1, false, -2.0f, 5.0f},
        {1, true,   3.0f, 4.0f}, {1, false, -1.5f, 3.0f},
        {2, true,   2.5f, 3.5f}, {2, false, -3.5f, 2.5f},
    };
    const bool mirrored = sled.vx > 0.0f;   // measured facing = left
    for (const auto& d : kPieces) {
        IceTrailBit bit;
        bit.x = sled.position.x + sled.hitboxSize.x * 0.5f;
        bit.y = sled.position.y + sled.hitboxSize.y * 0.5f;
        bit.vx = mirrored ? -d.vx : d.vx;
        bit.vy = -d.vyUp;        // oracle convention: vy positive = UP
        bit.gravity = 0.1875f;   // raw +1E byte 0x30 = 48/256
        bit.age = 0;
        bit.serial = ++iceTrailSerial_;
        bit.kind = d.kind;
        bit.lifetime = 0;        // no timer; off-screen despawn
        bit.hflip = mirrored ? !d.hflip : d.hflip;
        iceTrailBits_.push_back(bit);
    }
}

void GameplayScene::spawnImpactBurst(float x, float y) {
    // Pellet shatter impact burst (WP-B cos_s2, $7E1928 records): composite
    // animation at the shatter point on EVERY impact (terrain AND enemies),
    // played from the offline-composed 64x64x21 strip. First 2 strip frames
    // are empty, matching the oracle's spawn->first-sprite delay.
    iceImpactBursts_.push_back({x, y, 0});
}

void GameplayScene::renderIceTrail(float camX, float camY) {
    // Trail bits (kind 0) + wall-break debris (kind 1 shard / 2 chunk).
    if (!iceTrailBits_.empty()) {
        if (!iceTrailTex_) {
            iceTrailTex_ = AssetCache::loadTexture(
                "content/x1/sprites/weapons/shotgun_ice_trail.png");
        }
        if (!iceDebrisTex_) {
            iceDebrisTex_ = AssetCache::loadTexture(
                "content/x1/sprites/weapons/shotgun_ice_debris.png");
        }
        for (const auto& b : iceTrailBits_) {
            if (b.kind == 0) {
                const float sx = b.x - camX - 4.0f;
                const float sy = b.y - camY - 4.0f;
                if (iceTrailTex_ && iceTrailTex_->valid()) {
                    // Real sparkle cycle 36->37->52->53, ~4f per sprite (S7
                    // VRAM decode; strip cells in that order; the effect
                    // table's sprite byte 0x81..0x84 wraps the same way).
                    const int frame = (b.age / 4) % 4;
                    const Rectangle src = {static_cast<float>(frame * 8), 0, 8, 8};
                    DrawTextureRec(iceTrailTex_->get(), src, {sx, sy}, WHITE);
                } else {
                    DrawRectangle(static_cast<int>(sx) + 2, static_cast<int>(sy) + 2,
                                  4, 4, Color{180, 230, 255, 220});
                }
            } else if (iceDebrisTex_ && iceDebrisTex_->valid()) {
                // Debris strip: cell 0 = 8x8 shard, cells 1-2 = 16x8 chunk.
                const float w = (b.kind == 1) ? 8.0f : 16.0f;
                Rectangle src = {(b.kind == 1) ? 0.0f : 8.0f, 0, w, 8};
                // U49: no pre-shift — raylib 5.x flips a negative-width
                // source IN PLACE (U40 law; the shift sampled the next cell).
                if (b.hflip) { src.width = -src.width; }
                DrawTextureRec(iceDebrisTex_->get(), src,
                               {b.x - camX - w * 0.5f, b.y - camY - 4.0f}, WHITE);
            }
        }
    }

    // Impact bursts: 64x64 cells centered on the shatter point.
    if (!iceImpactBursts_.empty()) {
        if (!iceImpactTex_) {
            iceImpactTex_ = AssetCache::loadTexture(
                "content/x1/sprites/weapons/shotgun_ice_impact.png");
        }
        if (iceImpactTex_ && iceImpactTex_->valid()) {
            for (const auto& ib : iceImpactBursts_) {
                const Rectangle src = {static_cast<float>(ib.age * 64), 0, 64, 64};
                DrawTextureRec(iceImpactTex_->get(), src,
                               {ib.x - camX - 32.0f, ib.y - camY - 32.0f}, WHITE);
            }
        }
    }

    // Charged-release flash: 8 one-frame sparkles around X (cos_s3 measured
    // offsets relative to X's 30x34 sprite box; strip cells 195,197,231,232).
    if (releaseFlashFrames_ > 0 && releaseFlashKind_ == 0) {
        if (!iceFlashTex_) {
            iceFlashTex_ = AssetCache::loadTexture(
                "content/x1/sprites/weapons/shotgun_ice_flash.png");
        }
        if (iceFlashTex_ && iceFlashTex_->valid()) {
            struct F { int cell; float dx, dy; };
            static const F kFlash[8] = {
                {1, -4, 10}, {1, 32, 25}, {2, 8, 34}, {2, 20, 2},
                {3, 4, 8}, {3, 24, 28}, {0, 22, 38}, {0, 6, -2},
            };
            for (const auto& f : kFlash) {
                const Rectangle src = {static_cast<float>(f.cell * 8), 0, 8, 8};
                DrawTextureRec(iceFlashTex_->get(), src,
                               {releaseFlashX_ + f.dx - camX,
                                releaseFlashY_ + f.dy - camY}, WHITE);
            }
        }
    }
    if (releaseFlashFrames_ > 0 && releaseFlashKind_ == 1) {
        // Torpedo fan release flash (S7 oam_sf272): same 8-sparkle pattern
        // as the ice release, torpedo pal-3 colors; sparkle topleft offsets
        // measured rel the player RAM anchor, fired right — x-mirrored
        // (and hflips toggled) for a left-side release.
        if (!torpedoFlashTex_) {
            torpedoFlashTex_ = AssetCache::loadTexture(
                "content/x1/sprites/weapons/homing_torpedo_flash.png");
        }
        if (torpedoFlashTex_ && torpedoFlashTex_->valid()) {
            torpedoFlashTex_->setFilter(TEXTURE_FILTER_POINT);
            struct F { int cell; bool hflip; float dx, dy; };
            static const F kFlash[8] = {
                {0, false, -10, -22}, {0, true,  +6, +18},
                {1, true,  -20, -10}, {1, true, +16,  +5},
                {2, false,  -8, +14}, {2, true,  +4, -18},
                {3, true,  -12, -12}, {3, true,  +8,  +8},
            };
            const bool right = player_.facingRight;
            for (const auto& f : kFlash) {
                Rectangle src = {static_cast<float>(f.cell * 8), 0, 8, 8};
                const bool hf = right ? f.hflip : !f.hflip;
                if (hf) {
                    src.width = -src.width;   // U49: in-place flip (U40 law)
                }
                const float dx = right ? f.dx : -f.dx - 8.0f;
                DrawTextureRec(torpedoFlashTex_->get(), src,
                               {releaseFlashX_ + dx - camX,
                                releaseFlashY_ + f.dy - camY}, WHITE);
            }
        }
    }
    if (releaseFlashFrames_ > 0 && releaseFlashKind_ == 2) {
        if (!busterL3ReleaseTex_) {
            busterL3ReleaseTex_ = AssetCache::loadTexture(
                "content/x1/sprites/weapons/buster_l3_release_flash.png");
        }
        if (busterL3ReleaseTex_ && busterL3ReleaseTex_->valid()) {
            busterL3ReleaseTex_->setFilter(TEXTURE_FILTER_POINT);
            constexpr int kFrameCount = 11;
            constexpr float kCellW = 112.0f;
            constexpr float kCellH = 80.0f;
            const int frame = std::clamp(kFrameCount - releaseFlashFrames_,
                                         0, kFrameCount - 1);
            Rectangle src = {static_cast<float>(frame) * kCellW, 0.0f, kCellW, kCellH};
            const bool right = player_.facingRight;
            const float originX = right ? 48.0f : kCellW - 48.0f;
            if (!right) src.width = -src.width;
            Rectangle dst = {releaseFlashX_ - camX, releaseFlashY_ - camY, kCellW, kCellH};
            DrawTexturePro(busterL3ReleaseTex_->get(), src, dst,
                           {originX, 40.0f}, 0.0f, WHITE);
        }
    }
}

void GameplayScene::renderSledSpray(float camX, float camY) {
    // Snow spray at the moving sled's rear-bottom — OAM-MEASURED per wheel
    // state (knowledge_base/_s7_sprites/spray_offsets.json, s3_charged_vram
    // moving-sled dumps, 0px variance across frames): the wheel byte steps
    // spr 5->6->7->8 every 3 moving frames; tile patterns per state are
    // {60} / {45,45,63}+60 / {61,61,63,63}+60 / {47,47,63}+60, the tile-60
    // rear-bottom anchor constant at (-8,+8). Offsets are relative to the
    // tile-42 body-anchor OAM top-left = sled sprite top-left + (8,0) for a
    // right-facing sled; mirrored cell-for-cell when facing left. Strip
    // cells (build_s7_content.py): 0=tile60, 1=45, 2=47, 3=61, 4=63, art at
    // true in-tile position, so whole 8x8 cells land pixel-exact.
    struct Puff { int cell; float dx, dy; };
    static const Puff kStates[4][5] = {
        // spr 5: anchor only
        {{0, -8, 8}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}},
        // spr 6: 45,45 + 63 + anchor
        {{1, 1, 8}, {1, 9, 8}, {4, -6, 7}, {0, -8, 8}, {-1, 0, 0}},
        // spr 7: 61,61 + 63,63 + anchor
        {{3, 0, 8}, {3, 8, 8}, {4, -16, 2}, {4, -8, 4}, {0, -8, 8}},
        // spr 8: 47,47 + 63 + anchor
        {{2, 0, 8}, {2, 3, 8}, {4, -25, 5}, {0, -8, 8}, {-1, 0, 0}},
    };
    for (const auto& p : projectiles_) {
        if (!p.active || !p.rideable || !p.isPlayerShot) continue;
        if (!p.sledMovingSpritePath.empty()) continue;  // OAM-ordered composite owns spray.
        if (std::fabs(p.vx) <= 0.01f) continue;  // spray only while moving
        if (!iceSprayTex_) {
            iceSprayTex_ = AssetCache::loadTexture(
                "content/x1/sprites/weapons/shotgun_ice_spray.png");
        }
        if (!iceSprayTex_ || !iceSprayTex_->valid()) return;
        const bool right = p.vx > 0;
        // Body-anchor screen position: tile 42 sits 8px in from the sled's
        // rear edge (sprite box == hitbox == 40x16).
        const float bodyX = p.position.x - camX;
        const float anchorY = p.position.y - camY;
        // Wheel phase: spr holds 2 moving frames at launch, then steps every
        // 3 (oracle s3: vx!=0 from sf362, spr 5,5,6,6,6,7,... -> ((m+1)/3)%4).
        const int m = std::max(0, p.ageFrames - p.sledLaunchFrame);
        const int state = ((m + 1) / 3) % 4;
        for (const auto& puff : kStates[state]) {
            if (puff.cell < 0) continue;
            Rectangle src{static_cast<float>(puff.cell * 8), 0, 8, 8};
            float px;
            if (right) {
                px = bodyX + 8.0f + puff.dx;
            } else {
                // Mirror the arrangement inside the 40px body box and hflip
                // the cell content (the game hflips for left-facing).
                px = bodyX + 24.0f - puff.dx;
                src.width = -src.width;       // U49: in-place flip (U40 law)
            }
            DrawTextureRec(iceSprayTex_->get(), src,
                           {px, anchorY + puff.dy}, WHITE);
        }
    }
}

void GameplayScene::applyRideableCarry() {
    // Charged Shotgun Ice ride (oracle S4 runs 2026-06-09): the sled scoops a
    // standing X as it slides into him; while riding, X locks to the sled's
    // nose and inherits its exact per-frame velocity. X is scraped off when
    // terrain blocks him; the sled continues unharmed.
    if (player_.isDead()) return;
    for (auto& p : projectiles_) {
        if (!p.active || !p.rideable || !p.isPlayerShot) continue;
        if (p.sledLaunchFrame > 0 && p.ageFrames < p.sledLaunchFrame) continue; // forming

        const AABB sledBox = p.getHitbox();
        const AABB feet = player_.getHitbox();
        const bool overX = feet.right() > sledBox.left() && feet.left() < sledBox.right();
        const float feetY = feet.bottom();
        // Riding: feet within a tolerance band of the sled's top surface and
        // not jumping upward.
        const bool onTopBand = feetY >= sledBox.top() - 3.0f
                            && feetY <= sledBox.top() + 6.0f;
        // Scooping (oracle s4 runs): a MOVING sled that slides into a grounded
        // X lifts him onto the nose — no jump needed. X standing beside the
        // sled has his feet at the sled's BOTTOM, so the top-band alone never
        // mounts him (ghost-proof finding 2026-06-09).
        const bool scoopBand = std::fabs(p.vx) > 0.01f && player_.onGround
                            && feetY > sledBox.top() + 6.0f
                            && feetY <= sledBox.bottom() + 2.0f;
        if (overX && player_.velocity.y >= 0.0f && (onTopBand || scoopBand)) {
            // Feet rest exactly on the sled top; X inherits the sled's Δx.
            player_.position.y = sledBox.top() - player_.hitboxOffset.y - player_.hitboxSize.y;
            player_.position.x += p.vx;
            // The carry runs AFTER this tick's player collision resolution,
            // so an unchecked +vx can wedge X INTO a wall and the next
            // tick's resolver pops him UPWARD — X visibly "climbs" the wall
            // (David's test-room report 2026-06-09). Clamp against solid
            // tiles instead: X is scraped off / pinned while the sled
            // slides on (oracle s4 dismount behavior).
            const int ts = tilemap_.tileSize();
            if (ts > 0) {
                const AABB box = player_.getHitbox();
                const int rowTop = static_cast<int>(box.top()) / ts;
                const int rowBot = static_cast<int>(box.bottom() - 1.0f) / ts;
                if (p.vx > 0) {
                    const int col = static_cast<int>(box.right()) / ts;
                    for (int row = rowTop; row <= rowBot; ++row) {
                        if (tilemap_.isSolid(col, row)) {
                            player_.position.x -= box.right() - static_cast<float>(col) * ts;
                            break;
                        }
                    }
                } else if (p.vx < 0) {
                    const int col = static_cast<int>(box.left()) / ts;
                    for (int row = rowTop; row <= rowBot; ++row) {
                        if (tilemap_.isSolid(col, row)) {
                            player_.position.x += static_cast<float>(col + 1) * ts - box.left();
                            break;
                        }
                    }
                }
            }
            player_.velocity.y = 0;
            player_.onGround = true;
        }
    }
}

void GameplayScene::spawnWallSplitProjectiles(const Projectile& source) {
    // Electric Spark terrain split (oracle 2026-06-10, _electric_spark_runs):
    // 2 children at the parent's death anchor (center-on-center — same
    // OID/sprite family, so the anchor->sprite offsets cancel), moving at
    // EXACTLY splitSpeed (6.0) px/f along +-the surface tangent. Engine tiles
    // are axis-aligned, so every engine impact face is the measured
    // vertical-face case (0,+-6); the 14-degree slope pair raw (+-1490,-+372)
    // lives in the KB for when slope tiles exist. Children pierce terrain,
    // never re-split, despawn off-screen; first visible frame is unmoved
    // (slot claimed at the death frame, motion 2 frames later).
    const float cx = source.position.x + source.hitboxSize.x / 2;
    const float cy = source.position.y + source.hitboxSize.y / 2;
    const float splitSpeed = source.splitSpeed > 0 ? source.splitSpeed : 6.0f;

    for (int i = 0; i < 2; i++) {
        Projectile split;
        const float svy = (i == 0) ? -splitSpeed : splitSpeed;
        split.init(0, 0, 0, svy, ProjectileType::Normal);
        split.isPlayerShot = source.isPlayerShot;
        // Children damage: oracle 2026-06-11 (cos_d3_childdmg2) — e-spark
        // children deal 0 and PASS THROUGH enemy bodies (15+ live-cannon
        // hurtbox crossings, no HP change/flash/child death). 0 damage takes
        // the measured-0 pass-through path in the enemy collision.
        split.damage = source.splitChildrenDamage >= 0 ? source.splitChildrenDamage
                                                       : source.damage;
        split.lifetime = 600;           // screen-bound; off-screen despawn rules
        split.hitboxSize = source.hitboxSize;
        split.position.x = cx - split.hitboxSize.x * 0.5f;
        split.position.y = cy - split.hitboxSize.y * 0.5f;
        split.prevPosition = split.position;
        split.color = source.color;
        split.weaponId = source.weaponId;
        split.applyWeaponVisual(source.weaponId, false);
        split.splitsOnWallHit = false;  // children never re-split (oracle)
        split.ignoresTerrain = source.splitPierce;
        split.despawnMarginPx = source.despawnMarginPx;
        split.holdFirstTick = true;     // visible unmoved +1, moves +2 (oracle)
        pendingProjectileSpawns_.push_back(split);
    }
}

void GameplayScene::spawnStingFan(const Projectile& muzzle) {
    // Chameleon Sting (oracle 2026-06-11, _chameleon_sting_runs): THREE darts
    // claim ALREADY SPREAD around the world-fixed muzzle bolt (specs are
    // muzzle-relative and pre-mirrored at fire time), fly at constant
    // velocity, pierce all terrain, and die only at the ~36px screen margin.
    // Same OID family as the bolt (0x08).
    const float mx = muzzle.position.x + muzzle.hitboxSize.x / 2;
    const float my = muzzle.position.y + muzzle.hitboxSize.y / 2;
    for (const auto& d : muzzle.stingDartSpecs) {
        Projectile dart;
        dart.init(0, 0, d.vx, d.vy, ProjectileType::Normal);
        dart.isPlayerShot = muzzle.isPlayerShot;
        dart.facingRight = muzzle.facingRight;
        dart.damage = muzzle.stingDartDamage;
        dart.lifetime = 600;            // margin-killed; no expiry observed
        dart.hitboxSize = muzzle.hitboxSize;
        dart.position.x = mx + d.offX - dart.hitboxSize.x * 0.5f;
        dart.position.y = my + d.offY - dart.hitboxSize.y * 0.5f;
        dart.prevPosition = dart.position;
        dart.color = muzzle.color;
        dart.weaponId = muzzle.weaponId;
        dart.applyWeaponVisual(muzzle.weaponId, false);
        // Fixed per-direction needle cells (S7: strip cells 12-14 after the
        // muzzle anim window; widen the modulo so the fixed index sticks).
        dart.visualFixedFrame = 12 + d.cell;
        dart.visualFrameCount = 15;
        dart.ignoresTerrain = true;     // floor-crossing down-diag (oracle)
        dart.despawnMarginPx = muzzle.despawnMarginPx;
        dart.holdFirstTick = true;      // claim -> unmoved+1 -> move+2
        pendingProjectileSpawns_.push_back(dart);
    }
}

bool GameplayScene::spawnWaveSegment(Projectile& head) {
    // Fire Wave charged chain (U29 re-measure): the wake flame drops at the
    // HEAD's previous-frame anchor (hold-tick + 1.5/f makes the wakes land
    // +12 then +15 apart, matching s3_long 156/171/186...), TERRAIN-SNAPPED
    // to the ground surface there (the wave climbed the plateau y 1191->1157
    // and descended after), anchored in place for 40f. Damage 1 per 2f tick
    // (cannon 8->2 ladder), capped via the enemy hitFlash window in
    // checkBulletEnemyCollision.
    // Ground snap: scan down from slightly above the head's height for the
    // first solid/slope surface (the real wave follows terrain both up and
    // down — start the scan a tile above to allow climbs). Snap the found
    // surface to the TILE TOP so every flat-run wake shares one exact
    // ground line regardless of the head's settle height (the 4px scan
    // step would alias it otherwise). Real garnish unmodeled: the first
    // wake sits +2px BELOW the rest in both oracle runs ($open-minor).
    return gameplay_projectiles::spawnWaveSegment(
        pendingProjectileSpawns_, head, tilemap_);
}

void GameplayScene::spawnDrop(float x, float y, int dropType) {
    if (dropType <= 0) return;
    Pickup p;
    p.init(x, y, static_cast<PickupType>(dropType));
    pickups_.push_back(p);
}

void GameplayScene::updatePickups() {
    for (auto& p : pickups_) {
        if (!p.active) continue;
        p.update(0, tilemap_);
    }

    pickups_.erase(
        std::remove_if(pickups_.begin(), pickups_.end(),
                       [](const Pickup& p) { return !p.active; }),
        pickups_.end()
    );
}

void GameplayScene::checkPlayerPickupCollision() {
    if (player_.isDead()) return;

    AABB playerBox = player_.getHitbox();

    for (auto& p : pickups_) {
        if (!p.active) continue;

        AABB pickupBox = p.getHitbox();
        if (!playerBox.overlaps(pickupBox)) continue;

        switch (p.type) {
            case PickupType::SmallHealth:
            case PickupType::LargeHealth:
                if (player_.health < player_.progressState().maxHealth) {
                    player_.health = std::min(player_.health + p.value, player_.progressState().maxHealth);
                    AudioManager::playSFX(SFX::HealthRestore);
                } else {
                    // Fill sub-tanks if health is full
                    for (int i = 0; i < Player::MAX_SUB_TANKS; i++) {
                        if (player_.progressState().subTanks[i].collected && player_.progressState().subTanks[i].health < Player::SUB_TANK_CAPACITY) {
                            player_.progressState().subTanks[i].health = std::min(player_.progressState().subTanks[i].health + p.value, Player::SUB_TANK_CAPACITY);
                            break;
                        }
                    }
                }
                break;
            case PickupType::SmallAmmo:
                // Future: weapon energy
                break;
            case PickupType::ExtraLife:
                player_.lives += p.value;
                AudioManager::playSFX(SFX::ExtraLife);
                break;
            case PickupType::HeartTank:
                // Cap at 32 HP (16 base + 8 heart tanks * 2) — matches original MMX
                if (player_.progressState().maxHealth < 32) {
                    int gain = std::min(p.value, 32 - player_.progressState().maxHealth);
                    player_.progressState().maxHealth += gain;
                    player_.health += gain;
                }
                player_.markPickupCollectedInProgress(p.persistentId);
                AudioManager::playSFX(SFX::HeartTank);
                break;
            case PickupType::SubTank:
                // Find next empty sub-tank slot
                for (int i = 0; i < Player::MAX_SUB_TANKS; i++) {
                    if (!player_.progressState().subTanks[i].collected) {
                        player_.progressState().subTanks[i].collected = true;
                        player_.progressState().subTanks[i].health = 0;
                        player_.markPickupCollectedInProgress(p.persistentId);
                        AudioManager::playSFX(SFX::SubTank);
                        break;
                    }
                }
                break;
            case PickupType::ArmorCapsule:
                if (shouldStartStingChameleonCapsuleCutscene(p)) {
                    startStingChameleonCapsuleCutscene(p);
                    continue;
                }
                if (shouldStartStormEagleCapsuleCutscene(p)) {
                    startStormEagleCapsuleCutscene(p);
                    continue;
                }
                if (shouldStartCpCapsuleCutscene(p)) {
                    startCpCapsuleCutscene(p);
                    continue;
                }
                grantArmorCapsule(p);
                player_.markPickupCollectedInProgress(p.persistentId);
                AudioManager::playSFX(SFX::HeartTank); // Reuse the big-pickup sound
                break;
        }

        p.active = false;
    }
}

bool GameplayScene::isRespawnPointSafe(Vector2 pos) const {
    const int ts = tilemap_.tileSize();
    if (ts <= 0) return false;

    AABB hb = {
        pos.x + player_.hitboxOffset.x,
        pos.y + player_.hitboxOffset.y,
        player_.hitboxSize.x,
        player_.hitboxSize.y
    };
    if (hb.left() < 0.0f || hb.right() > static_cast<float>(tilemap_.pixelWidth()) ||
        hb.top() < 0.0f || hb.bottom() > static_cast<float>(tilemap_.pixelHeight())) {
        return false;
    }

    const int startCol = static_cast<int>(std::floor(hb.left() / ts));
    const int endCol = static_cast<int>(std::floor((hb.right() - 0.01f) / ts));
    const int startRow = static_cast<int>(std::floor(hb.top() / ts));
    const int endRow = static_cast<int>(std::floor((hb.bottom() - 0.01f) / ts));
    for (int row = startRow; row <= endRow; ++row) {
        for (int col = startCol; col <= endCol; ++col) {
            TileType type = tilemap_.getTileType(col, row);
            if (type == TileType::Solid || type == TileType::Spike ||
                type == TileType::Breakable) {
                return false;
            }
        }
    }

    const int feetRow = static_cast<int>(std::floor((hb.bottom() + 1.0f) / ts));
    const int feetStartCol = static_cast<int>(std::floor((hb.left() + 1.0f) / ts));
    const int feetEndCol = static_cast<int>(std::floor((hb.right() - 1.0f) / ts));
    bool hasFloor = false;
    for (int col = feetStartCol; col <= feetEndCol; ++col) {
        TileType type = tilemap_.getTileType(col, feetRow);
        if (type == TileType::Spike) {
            return false;
        }
        if (type == TileType::Solid || type == TileType::OneWay ||
            type == TileType::Breakable || type == TileType::SlopeL ||
            type == TileType::SlopeR) {
            hasFloor = true;
        }
    }
    return hasFloor;
}

std::optional<Vector2> GameplayScene::snapRespawnToGround(Vector2 pos) const {
    constexpr int kSearchPx = 160;
    constexpr int kStepPx = 2;
    for (int distance = 0; distance <= kSearchPx; distance += kStepPx) {
        const Vector2 down = {pos.x, pos.y + static_cast<float>(distance)};
        if (isRespawnPointSafe(down)) return down;
        if (distance > 0) {
            const Vector2 up = {pos.x, pos.y - static_cast<float>(distance)};
            if (isRespawnPointSafe(up)) return up;
        }
    }
    return std::nullopt;
}

Vector2 GameplayScene::resolveRespawnPoint(Vector2 pos) const {
    if (auto snapped = snapRespawnToGround(pos)) {
        return *snapped;
    }
    if (auto fallback = snapRespawnToGround(defaultSpawn_)) {
        return *fallback;
    }
    return pos;
}

void GameplayScene::handlePlayerDeath() {
    // Spike death — instant kill on contact with spike tiles
    if (!player_.isDead()) {
        AABB hb = player_.getHitbox();
        int ts = tilemap_.tileSize();
        int startCol = static_cast<int>(hb.left()) / ts;
        int startRow = static_cast<int>(hb.top()) / ts;
        int endCol = static_cast<int>(hb.right() - 0.01f) / ts;
        int endRow = static_cast<int>(hb.bottom() - 0.01f) / ts;
        for (int row = startRow; row <= endRow && !player_.isDead(); row++) {
            for (int col = startCol; col <= endCol && !player_.isDead(); col++) {
                if (tilemap_.getTileType(col, row) == TileType::Spike) {
                    player_.forceDeath();
                    AudioManager::playSFX(SFX::PlayerDeath);
                    camera_.shake(5.0f, 15);
                }
            }
        }
    }

    // Pit death — instant kill
    if (gameplay_pit_death::shouldKillAtY(player_.position.y, tilemap_) &&
        !player_.isDead()) {
        player_.forceDeath();
        AudioManager::playSFX(SFX::PlayerDeath);
        camera_.shake(5.0f, 15);
    }

    // After death animation completes, respawn or game over.
    // 220 frames (~3.7s) fits all six DD orb bursts (t=31/32/64/98/141/184)
    // plus the full 60-frame orb lifetime of the last ring before the body
    // pops back in. DD uses 327 frames but includes screen fades we don't.
    if (player_.isDead() && player_.deathTimer() > 220) {
        if (player_.lives > 0) {
            player_.lives--;
            checkpoint_ = resolveRespawnPoint(checkpoint_);
            player_.respawnAt(checkpoint_, 90, isRespawnPointSafe(checkpoint_));

            // Respawn enemies and clear projectiles/pickups/death orbs
            enemies_.clear();
            projectiles_.clear();
            busterImpacts_.clear();
            pickups_.clear();
            gameplay_death_orbs::clearLiveOrbs(deathOrbState_);
            spawnEnemies();

            // Reset boss encounter if active (player respawns at checkpoint,
            // boss should be dormant again until player re-enters the arena.
            // Arena-wave bosses are the exception: the wave owns a closed
            // arena, so the boss must reactivate immediately after respawn.
            if (bossActive_) {
                if (arenaWaveMode_ && arenaWaveBoss_.has_value()) {
                    initBossFromSpawn(*arenaWaveBoss_);
                } else {
                    for (const auto& sp : tilemap_.spawns()) {
                        if (sp.type == "boss" && !sp.id.empty()) {
                            initBossFromSpawn(sp);
                            break;
                        }
                    }
                }
                bossLocked_ = false;
                camera_.clearRoom();
                camera_.unlock();
                hud_.hideBossHP();
            }

            camera_.snapTo(
                playerCameraAnchorX(),
                player_.position.y + player_.spriteHeight / 2
            );
            if (arenaWaveMode_ && arenaWaveBoss_.has_value()) {
                activateArenaWaveBoss();
            }
            hud_.update(player_.health, player_.progressState().maxHealth, player_.lives);
            hud_.syncDisplay();
        } else {
            gameOver_ = true;
            gameOverTimer_ = 0;
            // The source game-over screen reuses the password-grid shell, but
            // its stable digits are not accepted by the source title decoder,
            // even after a clean cold boot or from an all-weapons fixture.
            gameOverGridDigits_ = gameplay_game_over::makeDecorativeGridDigits(
                []() { return 1 + (std::rand() % 8); }
            );
            gameplay_game_over::initializeDecorativeHelmetTicks(
                gameOverHelmetTicks_,
                []() { return std::rand() % 241; }
            );
            SaveSystem::incrementDeaths();
        }
    }
}

void GameplayScene::checkBossPlayerCollision() {
    if (!boss_.active || player_.isInvulnerable() || player_.isDead()) return;
    if (boss_.bossState == BossState::Rescued || boss_.bossState == BossState::Dying ||
        boss_.bossState == BossState::Dead || boss_.bossState == BossState::Dormant ||
        boss_.bossState == BossState::Intro) return;

    AABB playerBox = player_.getHitbox();
    AABB bossBox = boss_.getHitbox();

    if (playerBox.overlaps(bossBox)) {
        float dir = (player_.position.x < boss_.position.x) ? -1.0f : 1.0f;
        player_.takeDamage(boss_.contactDamage, dir);
        AudioManager::playApu(0x09);   // measured hurt cry (R6)
        camera_.shake(4.0f, 12);
    }
}

void GameplayScene::checkBulletBossCollision() {
    if (!boss_.active || boss_.bossState == BossState::Rescued ||
        boss_.bossState == BossState::Dying || boss_.bossState == BossState::Dead ||
        boss_.bossState == BossState::Dormant || boss_.bossState == BossState::Intro) return;

    for (auto& p : projectiles_) {
        if (!p.active || !p.isPlayerShot) continue;

        AABB pBox = p.getHitbox();
        if (!boss_.overlapsPlayerShot(pBox)) continue;
        if (p.usesOneHitPerTargetGate() && p.hitBossOnce) continue;

        // Apply damage through the boss's weakness system. L3 buster keeps
        // enemy-orb damage in Projectile::damage, but bosses take the
        // oracle-proven 3 HP full-charge hit before blink invulnerability.
        const bool damaged = boss_.takeDamage(p.damageToBoss(), p.weaponId);
        if (damaged) {
            if (p.usesOneHitPerTargetGate()) {
                p.hitBossOnce = true;
            }
            AudioManager::playSFX(SFX::Hit);
        }

        if (!p.piercing) p.active = false;
        break;
    }
}

void GameplayScene::checkHelmetHeadbutt() {
    // Helmet upgrade: break breakable ceiling tiles when jumping into them
    if (!player_.hasHelmet) return;
    if (!player_.onCeiling) return;
    if (player_.isDead()) return;

    // Check tiles directly above the player's hitbox
    AABB hb = player_.getHitbox();
    int ts = tilemap_.tileSize();
    int startCol = static_cast<int>(hb.left()) / ts;
    int endCol = static_cast<int>((hb.right() - 0.01f)) / ts;
    int row = static_cast<int>((hb.top() - 1.0f)) / ts; // Row just above head

    bool genericBreak = false;
    for (int col = startCol; col <= endCol; col++) {
        if (tilemap_.getTileType(col, row) == TileType::Breakable) {
            if (activeStageId_.str() == "storm-eagle" &&
                breakStormEagleCell(col, row)) {
                continue;
            }
            tilemap_.breakTile(col, row);
            genericBreak = true;

            // Spawn debris particles as pickups won't work — use projectile-like debris
            float tx = static_cast<float>(col * ts + ts / 2);
            float ty = static_cast<float>(row * ts + ts / 2);

            // 4 debris fragments flying outward
            float speeds[] = {-1.5f, -0.5f, 0.5f, 1.5f};
            for (int i = 0; i < 4; i++) {
                Projectile debris;
                debris.init(tx, ty, speeds[i], -2.0f + (i % 2) * 0.5f, ProjectileType::Normal);
                debris.isPlayerShot = false;
                debris.damage = 0; // Visual only, no damage
                debris.lifetime = 20;
                debris.hitboxSize = {4, 4};
                debris.color = {180, 140, 80, 255}; // Brown/stone color
                debris.piercing = true; // Don't stop on tile hit
                projectiles_.push_back(debris);
            }
        }
    }

    if (genericBreak) {
        AudioManager::playSFX(SFX::EnemyDeath); // Reuse destruction sound
        camera_.shake(2.0f, 6);
    }
}

void GameplayScene::updateCheckpoints() {
    for (auto& cp : checkpoints_) {
        if (!cp.triggered && player_.position.x >= cp.triggerX) {
            cp.triggered = true;
            checkpoint_ = cp.respawn;
        }
    }
}

void GameplayScene::renderEscapeConfirm() const {
    DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT, {0, 0, 0, 170});

    constexpr int boxW = 158;
    constexpr int boxH = 64;
    constexpr int boxX = (INTERNAL_WIDTH - boxW) / 2;
    constexpr int boxY = (INTERNAL_HEIGHT - boxH) / 2;
    DrawRectangle(boxX, boxY, boxW, boxH, {0, 25, 84, 255});
    DrawRectangleLines(boxX, boxY, boxW, boxH, {210, 235, 255, 255});
    DrawRectangleLines(boxX + 1, boxY + 1, boxW - 2, boxH - 2, {74, 148, 236, 255});
    DrawRectangle(boxX + 4, boxY + 4, boxW - 8, boxH - 8, {0, 12, 48, 255});

    const char* title = "RETURN TO TITLE?";
    const int titleW = MeasureText(title, 9);
    DrawText(title, boxX + (boxW - titleW) / 2, boxY + 13, 9, {220, 236, 255, 255});

    constexpr int yesX = 79;
    constexpr int noX = 143;
    constexpr int choiceY = 122;
    const bool yes = escapeConfirm_.choice == EscapeConfirmChoice::Yes;
    DrawText("YES", yesX, choiceY, 9, yes ? Color{255, 248, 160, 255} : LIGHTGRAY);
    DrawText("NO", noX, choiceY, 9, !yes ? Color{255, 248, 160, 255} : LIGHTGRAY);
    DrawText(">", yes ? yesX - 12 : noX - 12, choiceY, 9, {255, 248, 160, 255});
}

void GameplayScene::writeVisualCompositionTrace(float alpha,
                                                float naturalCameraX,
                                                float naturalCameraY,
                                                float renderCameraX,
                                                float renderCameraY,
                                                bool renderCameraOverride) {
    if (!visualCompositionTrace_) return;
    ++visualCompositionTraceFrame_;
    if (visualCompositionTraceTargetFrame_ > 0 &&
        visualCompositionTraceFrame_ != visualCompositionTraceTargetFrame_) {
        return;
    }

    const AABB hb = player_.getHitbox();
    const auto layers = tilemap_.collectRenderLayerDiagnostics(
        renderCameraX, renderCameraY, activeCameraSectionId_, activeVisualSectionId_);
    int visibleLayerCount = 0;
    for (const auto& layer : layers) {
        if (layer.visible) ++visibleLayerCount;
    }

    const bool sceneWarpHidingX = stageStartSequence_ || warpInTimer_ > 0;
    const bool sceneCapsuleHidingX = cpCapsule_.active();
    const bool sceneRenderCalled = !sceneWarpHidingX && !sceneCapsuleHidingX;
    const bool suppressCpGrounding = suppressCpBossRoomVisualGrounding();
    const float cpPlayerLift = suppressCpGrounding
        ? boss_cp_intro_timeline::kEntryPlayerRenderLiftPx
        : 0.0f;
    const int cpPlayerPoseFrame = suppressCpGrounding
        ? boss_cp_intro_timeline::entryPlayerPoseFrame(
              boss_.introSourceTick())
        : -1;
    const Vector2 finalPlayerRenderCamera = {renderCameraX, renderCameraY};
    const PlayerRenderDiagnostic playerRender = player_.renderDiagnostic(
        alpha, {finalPlayerRenderCamera.x,
                finalPlayerRenderCamera.y + cpPlayerLift},
        suppressCpGrounding, cpPlayerPoseFrame);
    const bool playerBodyDrawn = sceneRenderCalled && playerRender.bodyDrawn;
    std::vector<std::string> hiddenReasons;
    if (sceneWarpHidingX) hiddenReasons.push_back("scene_warp_hidden");
    if (sceneCapsuleHidingX) hiddenReasons.push_back("scene_cp_capsule_hidden");
    if (!playerRender.bodyDrawn || playerRender.visibilityReason != "drawn_texture") {
        hiddenReasons.push_back(playerRender.visibilityReason);
    }

    std::string renderMode = "hidden";
    if (playerBodyDrawn && playerRender.sheetWidth > 0) {
        renderMode = "texture";
    }

    const float playerWorldX = playerRender.destRect.x + renderCameraX;
    const float playerWorldY = playerRender.destRect.y + renderCameraY;
    const float playerScreenRectX = playerRender.destRect.x;
    const float playerScreenRectY = playerRender.destRect.y;
    const float playerScreenRectW = playerRender.destRect.width;
    const float playerScreenRectH = playerRender.destRect.height;
    const float playerScreenHitboxX = hb.x - renderCameraX;
    const float playerScreenHitboxY = hb.y - renderCameraY;
    const bool playerScreenRectMatchesWorldMinusRenderCamera =
        std::fabs(playerScreenRectX - (playerWorldX - renderCameraX)) <= 0.001f &&
        std::fabs(playerScreenRectY - (playerWorldY - renderCameraY)) <= 0.001f &&
        std::fabs(playerScreenRectW - playerRender.destRect.width) <= 0.001f &&
        std::fabs(playerScreenRectH - playerRender.destRect.height) <= 0.001f;
    const bool playerScreenHitboxMatchesWorldMinusRenderCamera =
        std::fabs(playerScreenHitboxX - (hb.x - renderCameraX)) <= 0.001f &&
        std::fabs(playerScreenHitboxY - (hb.y - renderCameraY)) <= 0.001f;
    const std::string activeCameraSection = jsonQuoted(activeCameraSectionId_);
    const std::string activeVisualSection = jsonQuoted(activeVisualSectionId_);
    const std::string renderModeJson = jsonQuoted(renderMode);
    const std::string playerVisibilityReason = jsonQuoted(playerRender.visibilityReason);
    const std::string playerSheetPath = jsonQuoted(playerRender.sheetPathOrKey);
    const std::string playerPaletteVariant = jsonQuoted(playerRender.paletteVariantKey);
    const std::string playerWeaponId = jsonQuoted(playerRender.weaponId);
    const CpSourceObjForegroundRecord* sourceObjForeground =
        activeAutotestSourceObjForegroundRecord();

    std::fprintf(
        visualCompositionTrace_,
        "{\"kind\":\"render_visual_composition\","
        "\"renderFrame\":%ld,\"parityTick\":%ld,\"alpha\":%.6f,"
        "\"naturalCamera\":{\"x\":%.6f,\"y\":%.6f,\"currentX\":%.6f,\"currentY\":%.6f},"
        "\"renderCamera\":{\"x\":%.6f,\"y\":%.6f,\"override\":%s},"
        "\"activeCameraSection\":%s,\"activeVisualSection\":%s,"
        "\"bossLocked\":%s,\"visibleLayerCount\":%d,"
        "\"playerHitbox\":{\"x\":%.6f,\"y\":%.6f,\"w\":%.6f,\"h\":%.6f},"
        "\"playerRender\":{\"sceneRenderCalled\":%s,\"bodyDrawn\":%s,"
        "\"renderMode\":%s,\"visibilityReason\":%s,\"hiddenReasons\":",
        visualCompositionTraceFrame_, parityTraceTick_, alpha,
        naturalCameraX, naturalCameraY, camera_.x(), camera_.y(),
        renderCameraX, renderCameraY, renderCameraOverride ? "true" : "false",
        activeCameraSection.c_str(), activeVisualSection.c_str(),
        bossLocked_ ? "true" : "false", visibleLayerCount,
        hb.x, hb.y, hb.w, hb.h,
        sceneRenderCalled ? "true" : "false",
        playerBodyDrawn ? "true" : "false",
        renderModeJson.c_str(), playerVisibilityReason.c_str());
    writeStringArray(visualCompositionTrace_, hiddenReasons);
    std::fprintf(
        visualCompositionTrace_,
        ",\"coordinateBases\":{\"worldRect\":\"engine-world-pixels\",\"screenRect\":\"render-camera-viewport-pixels\",\"screenHitbox\":\"render-camera-viewport-pixels\",\"sprite.sourceRect\":\"runtime-spritesheet-pixels\","
        "\"sprite.destRect\":\"render-camera-viewport-pixels\"},"
        "\"derivationChecks\":{\"screenRectEqualsWorldMinusRenderCamera\":%s,"
        "\"screenHitboxEqualsWorldMinusRenderCamera\":%s}"
        ",\"position\":{\"x\":%.6f,\"y\":%.6f},"
        "\"prevPosition\":{\"x\":%.6f,\"y\":%.6f},"
        "\"interpolatedWorldPosition\":{\"x\":%.6f,\"y\":%.6f},"
        "\"worldRect\":{\"x\":%.6f,\"y\":%.6f,\"w\":%.6f,\"h\":%.6f},"
        "\"screenRect\":{\"x\":%.6f,\"y\":%.6f,\"w\":%.6f,\"h\":%.6f},"
        "\"screenHitbox\":{\"x\":%.6f,\"y\":%.6f,\"w\":%.6f,\"h\":%.6f},"
        "\"state\":%d,\"dead\":%s,\"facingRight\":%s,\"visualFacingRight\":%s,"
        "\"renderFrameIndex\":%d,"
        "\"sprite\":{\"sourceRect\":{\"x\":%.6f,\"y\":%.6f,\"w\":%.6f,\"h\":%.6f},"
        "\"destRect\":{\"x\":%.6f,\"y\":%.6f,\"w\":%.6f,\"h\":%.6f},"
        "\"spriteWidth\":%.6f,\"spriteHeight\":%.6f,"
        "\"sheetWidth\":%d,\"sheetHeight\":%d,\"sheetColumns\":%d,"
        "\"sheetPathOrKey\":%s,\"paletteVariantKey\":%s,\"tintAlpha\":%d},"
        "\"charge\":{\"level\":%d,\"timer\":%d},"
        "\"iframeTimer\":%d,\"deathTimer\":%d,\"stingVisualFrame\":%d,"
        "\"stingPhase\":%d,\"hp\":%d,\"weaponId\":%s}",
        playerScreenRectMatchesWorldMinusRenderCamera ? "true" : "false",
        playerScreenHitboxMatchesWorldMinusRenderCamera ? "true" : "false",
        player_.position.x, player_.position.y,
        player_.prevPosition.x, player_.prevPosition.y,
        playerWorldX, playerWorldY,
        playerWorldX, playerWorldY, playerRender.destRect.width, playerRender.destRect.height,
        playerScreenRectX, playerScreenRectY,
        playerScreenRectW, playerScreenRectH,
        playerScreenHitboxX, playerScreenHitboxY, hb.w, hb.h,
        playerRender.state, playerRender.deathTimer > 0 ? "true" : "false",
        playerRender.facingRight ? "true" : "false",
        playerRender.visualFacingRight ? "true" : "false",
        playerRender.frameIndex,
        playerRender.sourceRect.x, playerRender.sourceRect.y,
        playerRender.sourceRect.width, playerRender.sourceRect.height,
        playerScreenRectX, playerScreenRectY,
        playerScreenRectW, playerScreenRectH,
        playerRender.spriteWidth, playerRender.spriteHeight,
        playerRender.sheetWidth, playerRender.sheetHeight, playerRender.sheetColumns,
        playerSheetPath.c_str(), playerPaletteVariant.c_str(), playerRender.tintAlpha,
        playerRender.chargeLevel, playerRender.chargeTimer,
        playerRender.iframeTimer, playerRender.deathTimer,
        playerRender.stingVisualFrame, playerRender.stingPhase,
        playerRender.hp, playerWeaponId.c_str());

    std::fprintf(visualCompositionTrace_, ",\"sourceObjForegroundOverlay\":");
    writeSourceObjForegroundTrace(visualCompositionTrace_, sourceObjForeground);
    std::fprintf(visualCompositionTrace_, ",\"layers\":[");

    for (std::size_t i = 0; i < layers.size(); ++i) {
        const auto& layer = layers[i];
        if (i) std::fprintf(visualCompositionTrace_, ",");
        const std::string name = jsonQuoted(layer.name);
        const std::string previewPath = jsonQuoted(layer.previewPath);
        std::fprintf(
            visualCompositionTrace_,
            "{\"name\":%s,\"drawAfterEntities\":%s,\"visible\":%s,"
            "\"cameraSectionAllowed\":%s,\"visualSectionAllowed\":%s,\"phaseAllowed\":%s,"
            "\"visualPhaseTick\":%d,\"activePhase\":%d,\"visualPhasePeriod\":%d,"
            "\"cameraSectionIds\":",
            name.c_str(), layer.drawAfterEntities ? "true" : "false",
            layer.visible ? "true" : "false",
            layer.cameraSectionAllowed ? "true" : "false",
            layer.visualSectionAllowed ? "true" : "false",
            layer.phaseAllowed ? "true" : "false",
            layer.visualPhaseTick, layer.activePhase, layer.visualPhasePeriod);
        writeStringArray(visualCompositionTrace_, layer.cameraSectionIds);
        std::fprintf(visualCompositionTrace_, ",\"visualSectionIds\":");
        writeStringArray(visualCompositionTrace_, layer.visualSectionIds);
        std::fprintf(visualCompositionTrace_, ",\"visualPhaseIds\":");
        writeIntArray(visualCompositionTrace_, layer.visualPhaseIds);
        std::fprintf(
            visualCompositionTrace_,
            ",\"parallaxX\":%.6f,\"parallaxY\":%.6f,"
            "\"previewOffsetX\":%d,\"previewOffsetY\":%d,"
            "\"scrollX\":%d,\"scrollY\":%d,"
            "\"previewPath\":%s,\"previewWidth\":%d,\"previewHeight\":%d,"
            "\"repeatPreviewX\":%s,\"repeatPreviewY\":%s}",
            layer.parallaxX, layer.parallaxY,
            layer.previewOffsetX, layer.previewOffsetY,
            layer.scrollX, layer.scrollY,
            previewPath.c_str(), layer.previewWidth, layer.previewHeight,
            layer.repeatPreviewX ? "true" : "false",
            layer.repeatPreviewY ? "true" : "false");
    }
    std::fprintf(visualCompositionTrace_, "]}\n");
    std::fflush(visualCompositionTrace_);
}

void GameplayScene::render(float alpha) {
    float cx = camera_.prevX() + (camera_.x() - camera_.prevX()) * alpha;
    float cy = camera_.prevY() + (camera_.y() - camera_.prevY()) * alpha;
    const float naturalCx = cx;
    const float naturalCy = cy;
    if (hasAutotestRenderCameraOverride) {
        cx = autotestRenderCameraOverride.x;
        cy = autotestRenderCameraOverride.y;
    }
    writeVisualCompositionTrace(alpha, naturalCx, naturalCy, cx, cy,
                                hasAutotestRenderCameraOverride);

    ClearBackground(tilemap_.backgroundColor());
    tilemap_.render(cx, cy, activeCameraSectionId_, activeVisualSectionId_);
    renderCpEntrySourceArt(cx, cy);
    renderCpEntryShutter(cx, cy);
    tilemap_.renderOverlays(cx, cy, activeVisualSectionId_);
    tilemap_.renderWater(cx, cy);

    if (showCollision_) {
        tilemap_.renderCollisionDebug(cx, cy);
    }

    // Boss
    if (bossActive_ && boss_.active) {
        Vector2 sp = boss_.position;
        Vector2 spp = boss_.prevPosition;
        boss_.position.x -= cx;
        boss_.position.y -= cy;
        boss_.prevPosition.x -= cx;
        boss_.prevPosition.y -= cy;
        boss_.render(alpha);
        boss_.position = sp;
        boss_.prevPosition = spp;
    }

    // Enemies
    for (auto& e : enemies_) {
        if (!e.active) continue;
        Vector2 sp = e.position;
        Vector2 spp = e.prevPosition;
        e.position.x -= cx;
        e.position.y -= cy;
        e.prevPosition.x -= cx;
        e.prevPosition.y -= cy;
        e.render(alpha);
        e.position = sp;
        e.prevPosition = spp;
    }
    renderBusterImpacts(cx, cy);

    // Stage objects / hazards
    for (auto& obj : stageObjects_) {
        if (!obj.active) continue;
        Vector2 sp = obj.position;
        Vector2 spp = obj.prevPosition;
        obj.position.x -= cx;
        obj.position.y -= cy;
        obj.prevPosition.x -= cx;
        obj.prevPosition.y -= cy;
        obj.render(alpha);
        obj.position = sp;
        obj.prevPosition = spp;
    }

    // Storm Eagle platform sprites use the adapter's one-frame completed
    // snapshot queue. Rendering remains in the stage-object draw lane and
    // uses the interpolated camera only; physics/carry uses source state.
    renderStormEaglePlatforms(alpha, cx, cy);

    // Pickups
    for (auto& pk : pickups_) {
        if (!pk.active) continue;
        Vector2 sp = pk.position;
        Vector2 spp = pk.prevPosition;
        pk.position.x -= cx;
        pk.position.y -= cy;
        pk.prevPosition.x -= cx;
        pk.prevPosition.y -= cy;
        pk.render(alpha);
        pk.position = sp;
        pk.prevPosition = spp;
    }

    // Player
    Vector2 savedPos = player_.position;
    Vector2 savedPrev = player_.prevPosition;
    player_.position.x -= cx;
    player_.position.y -= cy;
    player_.prevPosition.x -= cx;
    player_.prevPosition.y -= cy;
    // R336: the transient OBJ sequence owns X through source941. The
    // ordinary player first appears at942, after the partial-body poses.
    const bool warpHidingX = stageStartSequence_ || warpInTimer_ > 0;
    // GC2.1b: the weapon-get warp-out rides the world pass; after it, the
    // sequence's own screens own the actor (gameplay_scene_weapon_get.cpp).
    const WeaponGetActorDraw weaponGet = weaponGetActorDraw();
    player_.position.y += static_cast<float>(weaponGet.arenaOffsetY);
    player_.prevPosition.y += static_cast<float>(weaponGet.arenaOffsetY);
    if (!warpHidingX && !weaponGet.hidePlayer && !cpCapsule_.active() &&
        !stormEagleCapsule_.visualActive() &&
        !gameplay_sting_chameleon_capsule::hidesOrdinaryPlayer(
            stingChameleonCapsule_)) {
        const bool suppressCpGrounding = suppressCpBossRoomVisualGrounding();
        const float cpPlayerLift = suppressCpGrounding
            ? boss_cp_intro_timeline::kEntryPlayerRenderLiftPx
            : 0.0f;
        const int cpPlayerPoseFrame = suppressCpGrounding
            ? boss_cp_intro_timeline::entryPlayerPoseFrame(
                  boss_.introSourceTick())
            : -1;
        player_.render(alpha, {0.0f, cpPlayerLift}, suppressCpGrounding,
                       cpPlayerPoseFrame);
    }
    player_.position = savedPos;
    player_.prevPosition = savedPrev;
    // Source warp OBJ priority2 remains behind the foreground snow/tiles.
    renderWarpIn(cx, cy);
    renderAutotestSourceObjForeground();
    renderCpCapsuleSourceObj();
    renderStormEagleCapsuleSourceObj();

    // FG occlusion layers (drawAfterEntities=true): snow strip etc drawn over X's feet
    tilemap_.renderForeground(cx, cy, activeCameraSectionId_, activeVisualSectionId_);

    // Death orbs — drawn after X so they sit on top of the (now-hidden) body
    gameplay_presentation::renderDeathOrbs(deathOrbState_, cx, cy);

    // Cosmetic ice-trail bits + sled snow spray (under the projectiles)
    renderIceTrail(cx, cy);
    renderSledSpray(cx, cy);
    renderTorpedoSmoke(cx, cy);

    // Projectiles
    for (auto& p : projectiles_) {
        if (!p.active) continue;
        Vector2 sp = p.position;
        Vector2 spp = p.prevPosition;
        p.position.x -= cx;
        p.position.y -= cy;
        p.prevPosition.x -= cx;
        p.prevPosition.y -= cy;
        p.render(alpha);
        p.position = sp;
        p.prevPosition = spp;
    }

    // B10: BG tiles the ROM marks with the SNES priority bit are drawn over
    // the sprites — the snow at X's feet at the Chill Penguin spawn is the
    // measured case (blockIds 132/133/135, mask 0b1100). Must come after the
    // player, the enemies and the projectiles.
    tilemap_.renderTilePriorityForeground(cx, cy, activeCameraSectionId_,
                                          activeVisualSectionId_);

    // Checkpoint markers (debug)
    if (showCollision_) {
        for (const auto& cp : checkpoints_) {
            gameplay_presentation::renderCheckpointDebugFlag(cp.respawn, cp.triggered, cx, cy);
        }
    }
    // Debug HUD
    if (showDebugInfo_) {
        gameplay_presentation::renderDebugHud({
            stateToString(player_.state()),
            player_.chargeLevel(),
            player_.health,
            player_.progressState().maxHealth,
            player_.lives,
            player_.iframeTimer(),
            player_.position,
            player_.velocity,
            static_cast<int>(projectiles_.size()),
            static_cast<int>(enemies_.size()),
            player_.onGround,
            player_.touchingWallLeft,
            player_.touchingWallRight,
            player_.weaponInventory.current().name.c_str(),
            gameplay_presentation::DebugHudBossData{
                bossActive_ && boss_.active,
                static_cast<int>(boss_.bossState),
                boss_.health,
                boss_.maxHealth,
                boss_.currentPhase,
                boss_.fightTimer,
            },
        });
    }
    renderCpCapsuleCutscene();
    renderStormEagleCapsuleDialog();
    renderStingChameleonCapsulePresentation();

    // HUD (always rendered on top of game world)
    hud_.render();
    gameplay_presentation::renderStageStartReady(stageStartTick_, stageReadyTex_);

    gameplay_presentation::renderSpeedrunTimerOverlay(
        Settings::showTimer,
        paused_,
        gameOver_,
        stageClear_,
        SaveSystem::totalPlayFrames(),
        stageTimer_,
        SaveSystem::getBestTime(activeStageId_));
    gameplay_presentation::renderPauseOverlay(
        paused_,
        stageName_,
        stageTimer_,
        player_,
        pauseWeaponCursor_);

    if (escapeConfirm_.open) {
        renderEscapeConfirm();
    }

    gameplay_presentation::renderGameOverOverlay(
        gameOver_,
        gameOverTimer_,
        gameOverGridDigits_,
        gameOverHelmetTicks_,
        gameOverGridSheet_);
    renderStageClearPresentation(alpha);

    // (No in-gameplay boss-name banner. The Maverick name reveal is the separate
    // emulator-exact boss-intro CUTSCENE (boss_intro_scene); the real game does not
    // overlay the name in the gameplay arena. David review pt2 caught it leaking in.)

    // Sprite test mode overlay (F3)
    const TextureResource* playerSheet = player_.spriteSheetResource();
    if (spriteTestMode_ && playerSheet && playerSheet->valid()) {
        const Texture2D& texture = playerSheet->get();
        // Dark background
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT, {0, 0, 0, 200});

        int cols = texture.width / static_cast<int>(player_.spriteWidth);
        int sw = static_cast<int>(player_.spriteWidth);
        int sh = static_cast<int>(player_.spriteHeight);
        int tx = (spriteTestFrame_ % cols) * sw;
        int ty = (spriteTestFrame_ / cols) * sh;

        // Draw current frame large (3x scale) in center
        float scale = 3.0f;
        float drawX = (INTERNAL_WIDTH - sw * scale) / 2;
        float drawY = (INTERNAL_HEIGHT - sh * scale) / 2 - 10;

        Rectangle srcRect = {static_cast<float>(tx), static_cast<float>(ty),
                             static_cast<float>(sw), static_cast<float>(sh)};
        Rectangle dstRect = {drawX, drawY, sw * scale, sh * scale};
        DrawTexturePro(texture, srcRect, dstRect, {0, 0}, 0.0f, WHITE);

        // Also draw flipped version next to it
        Rectangle srcFlip = {static_cast<float>(tx), static_cast<float>(ty),
                             -static_cast<float>(sw), static_cast<float>(sh)};
        Rectangle dstFlip = {drawX + sw * scale + 8, drawY, sw * scale, sh * scale};
        DrawTexturePro(texture, srcFlip, dstFlip, {0, 0}, 0.0f, WHITE);

        // Frame number (big)
        char buf[64];
        snprintf(buf, sizeof(buf), "FRAME %d", spriteTestFrame_);
        int textW = MeasureText(buf, 14);
        DrawText(buf, (INTERNAL_WIDTH - textW) / 2, 10, 14, {255, 255, 100, 255});

        // Source coordinates
        snprintf(buf, sizeof(buf), "src: (%d, %d)  size: %dx%d", tx, ty, sw, sh);
        int srcW = MeasureText(buf, 8);
        DrawText(buf, (INTERNAL_WIDTH - srcW) / 2, 28, 8, {180, 180, 200, 255});

        // Navigation help
        const char* helpText = "LEFT/RIGHT: browse   UP/DOWN: +/-10   F3: exit";
        int helpW = MeasureText(helpText, 7);
        DrawText(helpText, (INTERNAL_WIDTH - helpW) / 2, INTERNAL_HEIGHT - 16, 7, {120, 120, 150, 255});

        // Show neighboring frames (small) at bottom
        int thumbY = static_cast<int>(drawY + sh * scale + 16);
        int thumbScale = 1;
        int thumbSpacing = sw + 4;
        int startFrame = std::max(0, spriteTestFrame_ - 4);
        int totalFrames = cols * (texture.height / sh);
        int endFrame = std::min(totalFrames - 1, spriteTestFrame_ + 4);

        int stripW = (endFrame - startFrame + 1) * thumbSpacing;
        int stripX = (INTERNAL_WIDTH - stripW) / 2;

        for (int f = startFrame; f <= endFrame; f++) {
            int ftx = (f % cols) * sw;
            int fty = (f / cols) * sh;
            int px = stripX + (f - startFrame) * thumbSpacing;

            Rectangle fSrc = {static_cast<float>(ftx), static_cast<float>(fty),
                              static_cast<float>(sw), static_cast<float>(sh)};
            Rectangle fDst = {static_cast<float>(px), static_cast<float>(thumbY),
                              static_cast<float>(sw * thumbScale), static_cast<float>(sh * thumbScale)};

            Color tint = (f == spriteTestFrame_) ? WHITE : Color{150, 150, 150, 200};
            DrawTexturePro(texture, fSrc, fDst, {0, 0}, 0.0f, tint);

            // Frame number below thumbnail
            snprintf(buf, sizeof(buf), "%d", f);
            Color numColor = (f == spriteTestFrame_) ? Color{255, 255, 100, 255} : Color{120, 120, 150, 255};
            DrawText(buf, px + 2, thumbY + sh + 2, 7, numColor);
        }
    }

    // White flash on death — MMX1 flashes the screen white at the instant X bursts.
    // Peaks with the first orb ring (deathTimer 31), holds a few frames, then fades.
    if (player_.isDead()) {
        int t = player_.deathTimer();
        const int kFlashStart = 31;   // coincides with the first death-orb ring
        const int kFlashHold  = 3;    // frames at full white
        const int kFlashFade  = 18;   // frames fading back out
        if (t >= kFlashStart && t < kFlashStart + kFlashHold + kFlashFade) {
            float a = (t < kFlashStart + kFlashHold)
                          ? 1.0f
                          : 1.0f - static_cast<float>(t - kFlashStart - kFlashHold) /
                                       static_cast<float>(kFlashFade);
            DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
                          {255, 255, 255, static_cast<unsigned char>(255 * a)});
        }
    }

    // Fade-in from black on scene entry
    if (fadeInTimer_ > 0) {
        int alpha = 255 * fadeInTimer_ / FADE_IN_DURATION;
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
                      {0, 0, 0, static_cast<unsigned char>(alpha)});
    }
}

// R336 source-only OBJ art and discrete timing:
// knowledge_base/mmx1/story/stage_entry_warp.json. No background is in this
// atlas; the normal tile foreground pass supplies SNES priority occlusion.
void GameplayScene::renderWarpIn(float cameraX, float cameraY) const {
    if (warpInTimer_ <= 0) return;
    const float x = warpInPosition_.x - cameraX;
    const float y = warpInPosition_.y - cameraY;
    if (stageStartTick_ <= stage_start_timeline::kCapsuleLastTick &&
        warpCapsuleTex_ && warpCapsuleTex_->valid()) {
        const Texture2D& t = warpCapsuleTex_->get();
        // Source capsule x126, ordinary canvas(96,118), descent8 px/tick.
        const float top = y - 118.0f +
            static_cast<float>(stage_start_timeline::capsuleScreenTopAt(stageStartTick_));
        DrawTexturePro(t, {0, 0, static_cast<float>(t.width), static_cast<float>(t.height)},
                       {x + 30.0f, top, static_cast<float>(t.width), static_cast<float>(t.height)},
                       {0, 0}, 0.0f, WHITE);
        return;
    }
    const int cell = stage_start_timeline::materializeCellAt(stageStartTick_);
    if (cell >= 0 && warpMaterializeTex_ && warpMaterializeTex_->valid()) {
        const auto& armor = player_.progressState();
        const int mask = (armor.armorHelmet ? 1 : 0) |
                         (armor.armorBody ? 2 : 0) |
                         (armor.armorBuster ? 4 : 0) |
                         (armor.armorBoots ? 8 : 0);
        DrawTexturePro(warpMaterializeTex_->get(),
                       {static_cast<float>(cell * 70), static_cast<float>(mask * 70), 70, 70},
                       {x, y, 70, 70},
                       {0, 0}, 0.0f, WHITE);
    }
}

} // namespace mmx
