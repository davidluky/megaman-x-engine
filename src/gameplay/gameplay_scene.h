// gameplay_scene.h - declares the main stage gameplay scene and lane state.
// Owns: GameplayScene state shared by player, stage, camera, UI, and diagnostics.

#pragma once

#include "app/scene.h"
#include "app/constants.h"
#include "gameplay/cp_source_obj_foreground.h"
#include "gameplay/gameplay_buster_impact.h"
#include "gameplay/gameplay_death_orbs.h"
#include "gameplay/gameplay_cp_capsule.h"
#include "gameplay/gameplay_storm_eagle_capsule.h"
#include "gameplay/gameplay_sting_chameleon_capsule.h"
#include "gameplay/storm_eagle_destructible_terrain.h"
#include "gameplay/storm_eagle_industrial_destruction.h"
#include "gameplay/storm_eagle_heart_tank_placement.h"
#include "gameplay/gameplay_scene_se_platforms.h"
#include "gameplay/gameplay_scene_fm_gates.h"
#include "gameplay/stage_start_timeline.h"
#include "gameplay/weapon_get_presentation.h"
#include "app/input.h"
#include "systems/tilemap.h"
#include "systems/camera.h"
#include "ui/hud.h"
#include "entities/player.h"
#include "entities/projectile.h"
#include "entities/enemy.h"
#include "entities/stretch_bird_shot.h"
#include "entities/deck_turret_shot.h"
#include "entities/mad_pecker_shot.h"
#include "gameplay/deck_turret_effect.h"
#include "entities/pickup.h"
#include "entities/boss.h"
#include "entities/stage_object.h"
#include "systems/weapon.h"
#include "systems/randomizer.h"
#include "systems/raylib_resource.h"
#include "data/game_ids.h"
#include "ui/menu_flow.h"
#include "raylib.h"
#include <vector>
#include <array>
#include <string>
#include <optional>
#include <utility>
#include <cstdint>

// ============================================================================
// gameplay_scene.h — Main gameplay scene
//
// Owns tilemap, player, enemies, projectiles, and camera.
// ============================================================================

namespace mmx {

class SceneManager; // Forward declare

struct GameplaySceneConfig {
    std::string stagePath = "content/x1/stages/intro-highway.json";
    StageId stageId;
    std::string characterPath = "content/x1/characters/x.json";
    Vector2 spawnOverride = {0, 0};
    bool hasSpawnOverride = false;
    Vector2 autotestRenderCameraOverride = {0, 0};
    bool hasAutotestRenderCameraOverride = false;
    bool bossRushMode = false;
    bool suppressProgressionRewards = false;
    uint64_t randomizerSeed = 0;
    bool randomizerMode = false;
    RandomizerConfig randomizerConfig;
    bool grantAllWeaponsForAutotest = false;
    bool playWarpIn = false;
    // GC2.1b capture entry: start already in the weapon-get sequence.
    bool autotestWeaponGetSequence = false;
    // Boss-entry capture starts in an already-running room; there is no scene
    // transition fade at source t84.
    bool skipInitialFade = false;
};

class GameplayScene : public Scene {
public:
    GameplayScene() = default;
    explicit GameplayScene(const GameplaySceneConfig& config)
        : stagePath(config.stagePath),
          stageId(config.stageId),
          characterPath(config.characterPath),
          spawnOverride(config.spawnOverride),
          hasSpawnOverride(config.hasSpawnOverride),
          autotestRenderCameraOverride(config.autotestRenderCameraOverride),
          hasAutotestRenderCameraOverride(config.hasAutotestRenderCameraOverride),
          bossRushMode(config.bossRushMode),
          suppressProgressionRewards(config.suppressProgressionRewards),
          randomizerSeed(config.randomizerSeed),
          randomizerMode(config.randomizerMode),
          randomizerConfig(config.randomizerConfig),
          grantAllWeaponsForAutotest(config.grantAllWeaponsForAutotest),
          playWarpIn(config.playWarpIn),
          autotestWeaponGetSequence(config.autotestWeaponGetSequence),
          skipInitialFade(config.skipInitialFade) {}
    ~GameplayScene() override = default;
    GameplayScene(const GameplayScene&) = delete;
    GameplayScene& operator=(const GameplayScene&) = delete;
    GameplayScene(GameplayScene&&) = delete;
    GameplayScene& operator=(GameplayScene&&) = delete;

    // Set before pushing — which stage and character to load
    std::string stagePath = "content/x1/stages/intro-highway.json";
    StageId stageId; // Canonical progression slot; derived from stagePath when empty.
    std::string characterPath = "content/x1/characters/x.json";

    // Optional spawn override. When hasSpawnOverride is true, onEnter drops
    // the player at spawnOverride instead of the stage's player_spawn entry.
    // Used by --spawn-at to drop directly into specific sections for autotest.
    Vector2 spawnOverride = {0, 0};
    bool hasSpawnOverride = false;

    // Autotest-only render viewport override for source-register stage gates.
    // Normal gameplay never sets this through public launch modes.
    Vector2 autotestRenderCameraOverride = {0, 0};
    bool hasAutotestRenderCameraOverride = false;

    // Boss Rush mode: suppresses restart/title input on stage clear so the
    // parent BossRushScene can control transitions.
    bool bossRushMode = false;
    // Special modes that run disposable fights can suppress boss rewards and
    // save/stat mutations while still using the normal boss death pipeline.
    bool suppressProgressionRewards = false;

    // Randomizer: when set, shuffles enemy/pickup spawns before creating entities
    uint64_t randomizerSeed = 0;
    bool randomizerMode = false;
    RandomizerConfig randomizerConfig;
    bool grantAllWeaponsForAutotest = false;

    // When set (by the boss intro / stage-select flow), play the X warp-in
    // beam at stage start. Left false for autotest/respawn so verification
    // baselines stay deterministic.
    bool playWarpIn = false;

    // GC2.1b capture entry. No scripted fight reliably kills a Maverick, so the
    // weapon-get sequence gets a direct entry point the same way the boss-intro
    // cutscene does. Autotest only; the normal flow enters through boss death.
    bool autotestWeaponGetSequence = false;
    bool skipInitialFade = false;

    void setSceneManager(SceneManager* mgr) { sceneManager_ = mgr; }

    bool isStageClear() const { return stageClear_; }
    bool isGameOver() const { return gameOver_; }
    int getStageClearTimer() const { return stageClearTimer_; }
    Player& getPlayer() { return player_; }

    // Arena waves let parent challenge scenes supply their own hostile list
    // while reusing the stage's tilemap, camera, player, and combat systems.
    void setArenaWave(std::vector<SpawnPoint> enemies,
                      std::optional<SpawnPoint> boss = std::nullopt);
    void addExtraEnemySpawn(SpawnPoint enemy) { extraEnemySpawns_.push_back(std::move(enemy)); }

    void onEnter() override;
    void onExit() override;
    void pollInput() override;
    void handleInput() override;
    void update(float dt) override;
    void render(float alpha) override;

private:
    Tilemap tilemap_;
    Player player_;
    std::vector<Projectile> projectiles_;
    // Scoped OID03 lane; does not claim mixed-enemy SNES allocator parity.
    stretch_bird_shot::Pool stretchBirdShots_;
    deck_turret_shot::Child13Pool deckTurretShots_;
    mad_pecker_shot::PoolController madPeckerShots_;
    deck_turret_effect::Pool deckTurretEffects_{};
    std::array<int, 8> stretchBirdShotParents_{};
    std::array<unsigned, 8> stretchBirdShotAnimation_{};
    std::vector<Enemy> enemies_;
    // Stable preallocated identities awaiting a source camera-bucket event.
    std::vector<int> pendingSourceEnemySerials_;
    std::vector<Pickup> pickups_;
    std::vector<StageObject> stageObjects_;
    std::array<StormEaglePlatformScene, 10> stormEaglePlatforms_{};
    std::array<bool, 10> stormEagleAdvanceThisFrame_{};
    bool stormEaglePlatformFrameActive_ = false;
    Boss boss_;
    bool bossActive_ = false;
    bool bossLocked_ = false; // Camera locked to boss arena
    // CP-B1C-T1-M2 boss-entry cinematic (KB-driven ceremonial walk + camera
    // pan, both relative to the engine's own settle/lock values).
    bool bossEntryCinematicActive_ = false;
    flame_mammoth_gates::Lane fmGate_;
    bool fmCanonicalAssets_ = false;
    bool fmGateScope() const;
    bool moveFmGateApproach();
    bool beginFmGateContact();
    void advanceFmGate();
    void updateFmGateCamera();
    void finishFmGateFrame();
    bool bossEntryShutterPassable_ = false;
    float bossEntryCamLockX_ = 0.0f;
    bool hasEntrySourceHitbox_ = false;
    Vector2 entrySourceHitboxOffset_ = {0.0f, 0.0f};
    Vector2 entrySourceHitboxSize_ = {0.0f, 0.0f};
    const TextureResource* cpEntrySourceOverlayTex_ = nullptr;
    const TextureResource* cpEntryShutterTex_ = nullptr;
    bool arenaWaveMode_ = false;
    std::vector<SpawnPoint> arenaWaveEnemies_;
    std::optional<SpawnPoint> arenaWaveBoss_;
    std::vector<SpawnPoint> extraEnemySpawns_;

    Camera camera_;
    std::string activeCameraSectionId_;
    std::string activeVisualSectionId_;
    HUD hud_;
    Vector2 autotestPlayerPositionLock_ = {0, 0};
    bool hasAutotestPlayerPositionLock_ = false;

    // Game state
    bool paused_ = false;
    bool gameOver_ = false;
    int gameOverTimer_ = 0;
    std::array<int, 12> gameOverGridDigits_ = {};
    std::array<int, 12> gameOverHelmetTicks_ = {};
    bool stageClear_ = false;
    int stageClearTimer_ = 0;
    int fadeInTimer_ = 0;
    static constexpr int FADE_IN_DURATION = 30;
    // Source-clock stage entry: READY flashes before X descends and the held
    // control boundary opens on the final materialization frame.
    bool stageStartSequence_ = false;
    int stageStartTick_ = -1;
    int warpInTimer_ = 0;
    Vector2 warpInPosition_ = {};
    static constexpr int WARP_IN_DURATION = stage_start_timeline::kWarpDuration;
    void renderWarpIn(float cameraX, float cameraY) const;
    bool stageStartControlsLockedForNextFrame() const;
    bool dismissGameOverPassword();
    bool restartRequested_ = false;
    std::optional<Weapon> awardedWeapon_;
    bool stageClearFrozen_ = false;

    // GC2.1b: the source-timed weapon-get sequence. It only runs on a real
    // weapon award, so arena-wave clears and the Zero rescue keep the plain
    // stage-clear overlay.
    // Lane implementation: gameplay_scene_weapon_get.cpp.
    weapon_get_presentation::Params weaponGetParams_;
    std::string weaponGetText_;
    struct WeaponGetActorDraw {
        bool hidePlayer = false;
        int arenaOffsetY = 0;
    };
    void beginWeaponGetSequence();
    void startWeaponGetSequenceForCapture();
    void updateWeaponGetSequence();
    bool weaponGetSequenceActive() const;
    int weaponGetTick() const { return stageClearTimer_ - 1; }
    weapon_get_presentation::Plan weaponGetPlan() const;
    WeaponGetActorDraw weaponGetActorDraw() const;
    void renderWeaponGetActor(float alpha);
    void renderStageClearPresentation(float alpha);

    SceneManager* sceneManager_ = nullptr;

    // Debug (F1/F2/F3 toggles)
    bool showCollision_ = false;
    bool showDebugInfo_ = false;
    bool returnToTitle_ = false;
    bool returnToStageSelect_ = false;

    // Sprite test mode (F3) — browse sprite frames to map animations
    bool spriteTestMode_ = false;
    int spriteTestFrame_ = 0;
    int spriteTestDelay_ = 0;

    // Pause menu weapon select
    int pauseWeaponCursor_ = 0;
    int pauseInputDelay_ = 0;
    EscapeConfirmState escapeConfirm_;
    int escapeConfirmInputDelay_ = 0;

    // Stage timer (frames, converted to mm:ss for display)
    int stageTimer_ = 0;
    StageId activeStageId_;
    std::string stageName_;
    gameplay_cp_capsule::State cpCapsule_;
    gameplay_storm_eagle_capsule::State stormEagleCapsule_;
    gameplay_sting_chameleon_capsule::State stingChameleonCapsule_;
    mmx::destructible_terrain::CascadeRunner stormEagleCascade_;
    bool stormEagleCascadeStarted_ = false;
    std::string autotestSourceObjForegroundTarget_;

    // Death orb visual effect: rings of 16x16 orbs spawned when Player
    // enters Die state, matching DD's player_state_death orb schedule.
    gameplay_death_orbs::State deathOrbState_;
    // Warp-in materialize sprites (ripped from the real game). Borrowed from AssetCache.
    const TextureResource* warpCapsuleTex_ = nullptr;
    const TextureResource* warpMaterializeTex_ = nullptr;
    const TextureResource* stageReadyTex_ = nullptr;
    const TextureResource* gameOverGridSheet_ = nullptr;
    void spawnEnemies();
    void activateSourceEnemiesAfterCamera(std::uint16_t previousCameraX);
    void applyAutotestPlayerPositionLock();
    void grantAllWeaponsForAutotestRun(bool grantBusterArmor = true);
    // Task-17 ghost proof: when MMX_PROJTRACE is set, append one CSV row per
    // physics tick for the camera, the player, and every active player
    // projectile (tools/oracle/post/ghost_diff.py consumes it).
    void writeProjTrace();
    void writeParityTrace();
    void writeVisualCompositionTrace(float alpha, float naturalCameraX, float naturalCameraY,
                                     float renderCameraX, float renderCameraY,
                                     bool renderCameraOverride);
    FILE* projTrace_ = nullptr;
    long projTraceTick_ = 0;
    FILE* parityTrace_ = nullptr;
    long parityTraceTick_ = 0;
    FILE* visualCompositionTrace_ = nullptr;
    long visualCompositionTraceFrame_ = 0;
    long visualCompositionTraceTargetFrame_ = 0;
    size_t parityApuLogIndex_ = 0;
    size_t paritySfxLogIndex_ = 0;
    int parityRoomTransitionTimer_ = 0;
    void updateParityRoomTransitionSignal();
    // Shatter fans / wall splits spawned while projectiles_ is being
    // iterated land here and are flushed once per tick: pushing into
    // projectiles_ mid-iteration reallocates under live references (UB —
    // caught by the ghost frame-trace 2026-06-09), and the deferral also
    // matches the oracle (fragments appear 1 frame after impact).
    std::vector<Projectile> pendingProjectileSpawns_;

    // FW7 source presentation: consumed ChargeL1 and normal Buster shots leave
    // target-owned contact material without extending projectile gameplay. The
    // pure model owns the shared one-tick normal-shot prelude, fixed residue,
    // and ChargeL1 cadence/geometry; the scene owns queueing and drawing.
    std::vector<gameplay_buster_impact::Impact> busterImpacts_;
    void updateBusterImpacts();
    void spawnBusterImpact(const Projectile& projectile, int targetSerial);
    void dispatchNormalBusterContactEffects(
        const Projectile& projectile, int targetSerial, const AABB& targetBox,
        bool damagedEnemy, bool killedEnemy);
    void spawnNormalBusterLethalContactResidue(
        const Projectile& projectile, int targetSerial, const AABB& targetBox);
    void spawnNormalBusterSurvivorContactResidue(
        const Projectile& projectile, int targetSerial, const AABB& targetBox);
    void spawnNormalBusterContactPrelude(
        const Projectile& projectile, int targetSerial, const AABB& targetBox,
        bool killedEnemy);
    void spawnTableWeaponContactImpact(
        const Projectile& projectile, int targetSerial, const AABB& targetBox);
    void renderBusterImpacts(float camX, float camY);
    const TextureResource* busterImpactTex_ = nullptr; // Borrowed from AssetCache.
    const TextureResource* busterNormalContactResidueTex_ = nullptr;

    // Cosmetic Shotgun Ice effect bits (WP-B: SNES effect table $7E0BD8+,
    // measured in knowledge_base/_shotgun_ice_runs/cosmetic_layout.json).
    // kind 0 = pellet trail bit (8x8 sparkle cycle; dies at EXACTLY
    // lifetime=30 ticks mid-air — a timer, NOT terrain contact);
    // kind 1 = sled wall-break shard (tile 54), kind 2 = break chunk
    // (16x8, tiles 38+39) — break pieces despawn OFF-SCREEN only (the
    // oracle pieces fell through floors and died past the screen edges).
    // Never collides, never damages.
    struct IceTrailBit { float x, y, vx, vy, gravity; int age; int serial;
                         int kind = 0; int lifetime = 0; bool hflip = false; };
    std::vector<IceTrailBit> iceTrailBits_;
    int iceTrailSerial_ = 0;
    void updateIceTrail();
    void renderIceTrail(float camX, float camY);
    // MEASURED sled wall-break burst (WP-B cos_s6_face): 6 pieces from the
    // sled anchor, exact vectors, gravity 48/256. Silent and harmless.
    void spawnSledDebris(const Projectile& sled);
    // Pellet shatter impact burst (WP-B cos_s2): a composite animating
    // record at the shatter point on EVERY impact (terrain and enemies),
    // played back from the offline-composed frame strip.
    struct IceImpactBurst { float x, y; int age; };
    std::vector<IceImpactBurst> iceImpactBursts_;
    void spawnImpactBurst(float x, float y);
    // Charged-release flash (WP-B cos_s3): 8 one-frame sparkles around X.
    // The torpedo release shows the SAME 8-sparkle pattern (pair geometry
    // identical at oam_sf272) in torpedo pal-3 colors; kind picks the
    // texture + measured offset table.
    int releaseFlashFrames_ = 0;
    float releaseFlashX_ = 0, releaseFlashY_ = 0;
    int releaseFlashKind_ = 0;  // 0 = ice sled, 1 = torpedo fan, 2 = buster L3
    // Homing Torpedo smoke trail (S7 2026-06-10, s1/s3/s4_vram): every 3
    // frames of age a puff is laid down IN THE WAKE — at the body center
    // one spawn-interval (3 frames) earlier, +2px nose-ward (fits all 9 s1
    // samples within +-0.7px) — static in world, 7-frame life with cells
    // CE,CF,CF,D0,D0,D1,D1 (strip cols 0,1,1,2,2,3,3), and the trail
    // VANISHES with its owner slot (s1: truncated at the margin death).
    // BOTH tiers emit (all five fan members trail in s3_vram). Wisps are
    // gray (true OBJ pal 2 — the first build read CGRAM at a wrong base).
    struct TorpedoPuff { float x, y; int age; int ownerSerial; };
    std::vector<TorpedoPuff> torpedoPuffs_;
    void updateTorpedoSmoke();
    void renderTorpedoSmoke(float camX, float camY);
    const TextureResource* torpedoSmokeTex_ = nullptr;  // Borrowed from AssetCache.
    const TextureResource* torpedoFlashTex_ = nullptr;  // Borrowed from AssetCache.
    const TextureResource* busterL3ReleaseTex_ = nullptr; // Borrowed from AssetCache.
    // VRAM-decoded Shotgun Ice cosmetics (S7): trail sparkle cycle + the
    // sled's snow spray (the wheel byte animates the SPRAY, not the body).
    const TextureResource* iceTrailTex_ = nullptr;  // Borrowed from AssetCache.
    const TextureResource* iceSprayTex_ = nullptr;  // Borrowed from AssetCache.
    const TextureResource* iceDebrisTex_ = nullptr; // Borrowed from AssetCache.
    const TextureResource* iceImpactTex_ = nullptr; // Borrowed from AssetCache.
    const TextureResource* iceFlashTex_ = nullptr;  // Borrowed from AssetCache.
    void renderSledSpray(float camX, float camY);
    void spawnEnemyFromSpawn(const SpawnPoint& sp);
    void spawnStageObjectFromSpawn(const SpawnPoint& sp);
    void initBossFromSpawn(const SpawnPoint& sp);
    void requestFmBossHandover();
    void armFmGateRecord();
    void activateArenaWaveBoss();
    void updateArenaWaveClear();
    void completeArenaWave();
    void setupStormEagleDestructibles();
    void updateStormEagleDestructibles();
    void configureStormEaglePlatforms();
    bool prepareStormEaglePlatformFrame();
    void updateStormEaglePlatforms();
    void renderStormEaglePlatforms(float alpha, float cameraX, float cameraY);
    void checkStormEagleDashBreak();
    bool breakStormEagleCell(int tileX, int tileY);
    void updateEnemies();
    void updateStretchBirdShots();
    void allocateStretchBirdShot(const Enemy& enemy);
    void renderStretchBirdShots(float cameraX, float cameraY);
    void updateDeckTurretShots();
    void updateMadPeckerShots();
    void allocateMadPeckerShot(const Enemy& enemy);
    void renderMadPeckerShots(float cameraX, float cameraY);
    void allocateDeckTurretShot(const Enemy& enemy);
    void renderDeckTurretShots(float cameraX, float cameraY);
    void updateStageObjects();
    void updateProjectiles();
    void checkBulletEnemyCollision();
    void checkPlayerEnemyCollision();
    void resolvePlayerAxeMaxStackCollision();
    void checkPlayerStageObjectCollision();
    void checkEnemyShotPlayerCollision();
    void updatePickups();
    void checkPlayerPickupCollision();
    bool capsuleControlsLocked() const;
    bool shouldStartCpCapsuleCutscene(const Pickup& pickup) const;
    void startCpCapsuleCutscene(Pickup& pickup);
    void updateCpCapsuleCutscene(float dt);
    bool shouldStartStormEagleCapsuleCutscene(const Pickup& pickup) const;
    void startStormEagleCapsuleCutscene(Pickup& pickup);
    void updateStormEagleCapsuleCutscene(float dt);
    bool shouldStartStingChameleonCapsuleCutscene(const Pickup& pickup) const;
    void startStingChameleonCapsuleCutscene(Pickup& pickup);
    void updateStingChameleonCapsuleCutscene(float dt);
    const CpSourceObjForegroundRecord* activeAutotestSourceObjForegroundRecord() const;
    void renderAutotestSourceObjForeground() const;
    void renderCpCapsuleSourceObj() const;
    void renderCpCapsuleCutscene() const;
    void renderStormEagleCapsuleSourceObj() const;
    void renderStormEagleCapsuleDialog() const;
    void renderStingChameleonCapsulePresentation() const;
    void grantArmorCapsule(const Pickup& pickup);
    void spawnDrop(float x, float y, int dropType);
    void spawnShatterFragments(const Projectile& source);
    void spawnWallSplitProjectiles(const Projectile& source);
    void spawnStingFan(const Projectile& muzzle);
    bool spawnWaveSegment(Projectile& head);
    // Rolling Shield charged: consume the X-glued shield (one absorb) if
    // one is active — the caller skips the player damage when true.
    bool consumeAbsorbShield();
    void applyRideableCarry();
    void handlePlayerDeath();
    void checkHelmetHeadbutt();
    void updateCheckpoints();
    void updateCameraRoom();
    bool applyCameraSection();
    void updateActiveVisualSection();
    void renderEscapeConfirm() const;
    // X's horizontal camera anchor is the configured hitbox center (currently
    // position+33), not the right-facing WP-C source/RAM anchor at position+32.
    float playerCameraAnchorX() const;
    void setupCpEntryStagePresentation();
    void setupCpEntryHitboxRollout();
    void seedCpBossEntryAutotest();
    void updateCpEntryRoute();
    void updateCpEntryCamera();
    bool suppressCpBossRoomVisualGrounding() const;
    void renderCpEntrySourceArt(float cameraX, float cameraY) const;
    void renderCpEntryShutter(float cameraX, float cameraY) const;
    void updateBoss();
    void checkBossPlayerCollision();
    void checkBulletBossCollision();
    const char* stateToString(PlayerState s) const;
    bool isRespawnPointSafe(Vector2 pos) const;
    std::optional<Vector2> snapRespawnToGround(Vector2 pos) const;
    Vector2 resolveRespawnPoint(Vector2 pos) const;

    // Checkpoint — position where player respawns after death
    Vector2 checkpoint_ = {0, 0};
    Vector2 defaultSpawn_ = {0, 0};

    // Checkpoint triggers — X positions that update the respawn point
    struct Checkpoint {
        float triggerX;  // Player must pass this X position
        Vector2 respawn; // Where to respawn
        bool triggered;
    };
    std::vector<Checkpoint> checkpoints_;
};

} // namespace mmx
