// player.h - declares player state, progress data, diagnostics, and API.
// Owns: player state enums, upgrade persistence, and runtime control surface.

#pragma once

#include "actor.h"
#include "systems/tilemap.h"
#include "systems/animation.h"
#include "systems/weapon.h"
#include "systems/raylib_resource.h"
#include "entities/projectile.h"
#include "app/input.h"
#include "data/x_palette.h"
#include "data/x_sheet_composer.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <optional>

// ============================================================================
// player.h — Player character (Megaman X)
//
// Movement states: Idle, Run, Jump, Fall, WallSlide, WallJump, Dash, DashJump
//
// Combat system (runs in parallel with movement states):
//   Charge shot: hold shoot → charge1 (20 frames) → charge2 (50 frames)
//   Release to fire. Max 3 normal shots on screen. 8-frame cooldown.
//   Charge levels give bigger, stronger projectiles.
//
// Key mechanics:
//   Coyote time, input buffer, variable jump, wall jump lockout, dash-jump.
// ============================================================================

namespace mmx {

enum class PlayerState {
    Idle, Run, Jump, Fall,
    WallSlide, WallJump, Dash, DashJump,
    Ladder, Hurt, Die
};

enum class SourceContactProfile : std::uint8_t {
    NormalA552,
    ActiveDashBB38
};

struct PlayerSubTankProgress {
    bool collected = false;
    int health = 0;
};

struct PlayerProgress {
    int maxHealth = 16;
    PlayerSubTankProgress subTanks[4] = {};
    std::vector<std::string> collectedPickups;
    bool persistentStateInitialized = false;
    bool armorBoots = false;
    bool armorHelmet = false;
    bool armorBody = false;
    bool armorBuster = false;
};

struct PlayerRenderDiagnostic {
    bool bodyDrawn = true;
    std::string visibilityReason = "drawn_texture";
    int frameIndex = 0;
    bool facingRight = true;
    bool visualFacingRight = true;
    Rectangle sourceRect = {0.0f, 0.0f, 0.0f, 0.0f};
    Rectangle destRect = {0.0f, 0.0f, 0.0f, 0.0f};
    float spriteWidth = 0.0f;
    float spriteHeight = 0.0f;
    int sheetWidth = 0;
    int sheetHeight = 0;
    int sheetColumns = 0;
    std::string sheetPathOrKey;
    std::string paletteVariantKey;
    int tintAlpha = 255;
    int state = 0;
    int hp = 0;
    int iframeTimer = 0;
    int deathTimer = 0;
    int stingVisualFrame = -1;
    int stingPhase = -1;
    int chargeLevel = 0;
    int chargeTimer = 0;
    std::string weaponId;
};

class Player : public Actor {
public:
    Player();
    ~Player() override = default;
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;
    Player(Player&&) = delete;
    Player& operator=(Player&&) = delete;

    bool loadFromFile(const std::string& path);

    // U66: map the persistent armor capsule flags (armorBoots/...) onto this
    // instance's ability flags. Called by loadFromFile and by the numpad
    // debug grants. CENSUS u66_dash_*: dash exists ONLY with the legs
    // upgrade (fresh save walks at 1.5 with dash held; armored dashes 3.5).
    void applyArmorState();

    void pollInput();
    void handleInput() override;
    void update(float dt) override;

    // R96C scene-owned Storm Eagle support handoff.  GameplayScene calls the
    // four methods around the existing Player::update/physics step; player.cpp
    // deliberately does not call them so Player-only targets remain isolated.
    void beginStormEagleSupportFrame(bool previousSupport);
    bool consumeStormEagleSupportForPhysics();
    void finishStormEagleSupport(bool currentSupport);
    void clearStormEagleSupport();
    bool stormEagleSupportLatched() const { return stormEagleSupportLatched_; }
    // Measured object support loss: Run changes state without moving this tick.
    void queueSourceObjectDeparture() { sourceObjectDeparturePending_ = true; }
    bool sourceObjectDepartureFrame() const { return sourceObjectDepartureFrame_; }

    void render(float alpha) override;
    void render(float alpha, Vector2 cameraOffset);
    void render(float alpha, Vector2 cameraOffset,
                bool suppressVisualGrounding, int frameOverride = -1);
    PlayerRenderDiagnostic renderDiagnostic(
        float alpha, Vector2 cameraOffset,
        bool suppressVisualGrounding = false, int frameOverride = -1);
    void setTilemap(const Tilemap* tilemap,
                    bool enableSourceGroundMovement = false) {
        tilemap_ = tilemap;
        sourceGroundMovementEnabled_ = tilemap_ != nullptr &&
                                       enableSourceGroundMovement;
        resetJumpState();
    }
    bool sourceGroundMovementEnabled() const {
        return sourceGroundMovementEnabled_;
    }
    bool sourceWalkJumpLaunching() const {
        return sourceJumpSpeed_.has_value() && onGround && state_ == PlayerState::Jump;
    }

    PlayerState state() const { return state_; }
    std::optional<SourceContactProfile> sourceContactProfile() const;
    void changeState(PlayerState newState);
    // Scripted scenes (capsule cutscenes) park X outside Player::update, so
    // the state-driven animation refresh never runs; this forces the plain
    // standing pose regardless of the animation the scene interrupted.
    void forceIdlePose();
    // Gate action 18 retains the current pose while the scene owns physics.
    void tickAnimationOnly() { anim_.tick(); }

    // CP-B1C-T1-M2 scripted boss-entry walk. The ceremonial entry walk is
    // 116/256 px per tick — ~3x slower than runSpeed — so it cannot come
    // from injected input. While active, update() only advances the run
    // animation clock; input, combat, physics, and state transitions are
    // suppressed, facing is pinned right, and the SCENE drives position.x
    // per tick from the KB entry law (boss_cp_intro_timeline).
    void beginScriptedEntryWalk();
    void endScriptedEntryWalk();
    bool scriptedEntryWalkActive() const { return scriptedEntryWalk_; }

    // Plain slide/kick cells46..50 are authored for a right-wall canvas
    // (U73/R299). Preserve that orientation while the logical kick velocity
    // and shots point away. The wall_shoot overlay uses logical facing.
    bool visualFacingRight() const;

    // Physics constants
    float runSpeed = 1.46875f;
    float jumpVelocity = -5.25f;
    // Legacy configuration field. Measured Jump/DashJump release stops ascent;
    // the wall sequence defers that release until ordinary air control resumes.
    float jumpCutVelocity = -1.5f;

    // Wall
    float wallSlideSpeed = 2.0f;  // RAM-measured wall-slide descent cap (px/frame)
    float wallJumpVelocityX = 1.46875f;
    float wallJumpVelocityY = -1363.0f / 256.0f;
    int wallJumpLockoutFrames = 7;
    float ladderClimbSpeed = 1.25f;

    // Dash
    float dashSpeed = 3.0f;
    int dashDuration = 33;  // source timer $20..$00 integrates before DEC

    // Combat
    float shotSpeed = 4.0f;
    // Buster charge thresholds — EXACT, bisected via the release-followup
    // APU byte (_buster_runs/FINDINGS.md 2026-06-11: 30 silent/31=0x04,
    // 100=0x04/101=0x02, 178=0x02/179=0x05). The 0x03 charge whirr fires
    // WITH the L1 catch. L3 equals the special full-charge threshold 179.
    int chargeTime1 = 31;   // blue L1 catch (also the charge-whirr start)
    int chargeTime2 = 101;  // green L2 (full, base armor)
    int chargeTime3 = 179;  // ARM pink L3 (buster capsule required)
    // Frames for a SPECIAL weapon's full charge (arm upgrade required).
    // Oracle bisect 2026-06-09 (runs knowledge_base/_shotgun_ice_runs/
    // thresh_*): a 178-frame hold releases a normal pellet, 179 frames
    // releases the ice sled — threshold exactly 179 held frames.
    int chargeTimeSpecial = 179;

    // Feel
    int coyoteFrames = 4;
    int jumpBufferFrames = 6;

    // hurt_entry_2026-09-19: unarmored baseline; body halves the impulse
    // and shortens the reaction by one update. Init/exit do not integrate.
    float hurtKnockbackX = 138.0f / 256.0f;
    float hurtKnockbackY = -2.0f;
    int hurtDuration = 30;       // 29 moving updates, then action exit
    int iframeDuration = 91;
    bool hasArmor = false;       // Halves all damage

    // Lives
    int lives = 3;

    // Armor upgrades (Dr. Light capsules)
    bool hasBoots = true;        // Ground dash — armor-gated via applyArmorState
                                 // (default true keeps bare test Players dashing;
                                 // loadFromFile/grants overwrite from armor state)
    bool hasAirDash = false;     // Boots capsule: dash while airborne
    bool hasHelmet = false;      // Helmet capsule: headbutt breakable ceilings
    bool hasBusterUpgrade = false; // Buster capsule: charge level 3 (spiral shot)

    // Persistent player state (survives between stages)
    using SubTank = PlayerSubTankProgress;
    static void bindProgress(PlayerProgress& progress);
    static void useFallbackProgress();
    static PlayerProgress& progress();
    static PlayerProgress captureProgress();
    static void applyProgress(const PlayerProgress& progress);
    static void resetPersistentState();
    PlayerProgress& progressState();
    const PlayerProgress& progressState() const;
    void markPickupCollectedInProgress(const std::string& id);
    bool isPickupCollectedInProgress(const std::string& id) const;
    static void markPickupCollected(const std::string& id);
    static bool isPickupCollected(const std::string& id);
    void setArmorProgress(bool boots, bool helmet, bool body, bool buster);
    void grantArmorBoots();
    void grantArmorHelmet();
    void grantArmorBody();
    void grantArmorBuster();
    bool armorBootsUnlocked() const { return progressState().armorBoots; }
    bool armorHelmetUnlocked() const { return progressState().armorHelmet; }
    bool armorBodyUnlocked() const { return progressState().armorBody; }
    bool armorBusterUnlocked() const { return progressState().armorBuster; }
    // Sprite
    float spriteWidth = 32.0f;
    float spriteHeight = 32.0f;
    Vector2 busterMuzzleIdle = {45.0f, 38.0f};
    Vector2 busterMuzzleRun = {52.0f, 36.0f};
    Vector2 busterMuzzleJump = {53.0f, 36.0f};
    Vector2 busterMuzzleDash = {59.0f, 38.0f};
    Vector2 busterMuzzleWall = {54.0f, 36.0f};
    float busterMuzzleGap = 0.0f;
    float visualGroundingOffsetY = 0.0f;

    // Projectile management — owned by gameplay scene, player spawns into this
    std::vector<Projectile>* projectiles = nullptr;

    // Weapon inventory
    WeaponInventory weaponInventory;

    static constexpr int MAX_SUB_TANKS = 4;
    static constexpr int SUB_TANK_CAPACITY = 14;

    // Charge state (read by gameplay scene for visual effects)
    int chargeLevel() const { return chargeLevel_; }
    int chargeTimer() const { return chargeTimer_; }
    void setDebugSpecialChargeUnlocked(bool enabled) { debugSpecialChargeUnlocked_ = enabled; }
    bool debugSpecialChargeUnlockedForTest() const { return debugSpecialChargeUnlocked_; }

    // Damage interface — called by gameplay scene on collision
    void takeDamage(int amount, float knockbackDirX);
    void forceDeath();
    void respawnAt(Vector2 spawn, int invulnerableFrames = 90, bool grounded = false);
    // Scene-owned physics must hold the source damage-init and recovery
    // transition rows. This is true only for the current Player::update handoff.
    bool hurtEntryFrame() const { return hurtEntryFrame_; }
    bool isInvulnerable() const {
        return iframeTimer_ > 0 || stingInvincibleFrames > 0
            || state_ == PlayerState::Die;
    }

    // Chameleon Sting charged state (oracle 2026-06-11, s3_* runs): TOTAL
    // damage immunity for the duration — hits neither cancel nor shorten it
    // and consume no iframes; firing/movement allowed; deals nothing to
    // enemies. 480f flat: walking does NOT alter it (s3_walkstop/s3_walkend
    // both 480; s3_walk's +5 was contact-at-expiry coupled). Ticked once per
    // update(); public for the headless contract test.
    int stingInvincibleFrames = 0;
    void tickStingInvincibility() {
        if (stingInvincibleFrames > 0) stingInvincibleFrames--;
    }
    // Canonical entry (fireShot + tests): also arms the visual-cycle clock.
    void beginStingInvincibility(int frames) {
        stingInvincibleFrames = frames;
        stingInvincibleTotal_ = frames;
    }
    // Texture-free animation table init (U21 hurt contract drives update()
    // headless; AnimationPlayer is not empty-safe and loadFromFile pulls
    // textures, which segfaults without a window).
    void setupAnimationsForTest() { setupAnimations(); }
    int renderFrameIndexForTest() const { return renderFrameIndex(); }
    // Headless shoot-input seam (U41 SFX contract test): feed one frame of
    // shoot input exactly as pollInput would.
    void setShootInputForTest(bool held, bool pressed, bool released) {
        inputShootHeld_ = held;
        inputShootPressed_ = pressed;
        inputShootReleased_ = released;
    }
    // U03b visual cycle (oracle s7b_cgram, per-frame CGRAM row 9 + OAM):
    // d = frames since the charged release. d1 = 1-frame white/purple flash;
    // d8+ = 8-phase rainbow cycle (gold→green→deep-green→cyan→blue→purple→
    // pink→orange), 6f per phase, 48f period, until the state ends. X's
    // sprites BLINK OUT entirely on a 3-regime cadence: hidden d4..121 every
    // 3rd, d123..361 every 2nd, d366..481 every 5th (184 hidden frames,
    // chain ends on the state's last frame). Pure fns for the contract test.
    static bool stingHiddenAtFrame(int d);
    static int stingPhaseAtFrame(int d);  // -1 normal, 0 flash, 1..8 = b..i
    // Elapsed visual frame while the state is live, else -1. Clamped to 0
    // for direct stingInvincibleFrames pokes (old tests) that bypass
    // beginStingInvincibility — those render the normal palette.
    int stingVisualFrame() const {
        if (stingInvincibleFrames <= 0) return -1;
        const int d = stingInvincibleTotal_ - stingInvincibleFrames;
        return d < 0 ? 0 : d;
    }
    int iframeTimer() const { return iframeTimer_; }
    bool isDead() const { return state_ == PlayerState::Die; }
    int deathTimer() const { return deathTimer_; }
    const std::string& armorCompositeKey() const { return armorCompositeKey_; }
    size_t weaponVariantCacheSizeForTest() const { return weaponSpriteVariants_.size(); }
    void seedVariantCacheForTest(const std::string& id) {
        weaponSpriteVariants_.emplace(id, TextureResource{});
    }

    // Death burst — iconic MMX radial "life energy" nodes that fly outward when
    // X dies. Exposed for rendering and for headless verification.
    struct DeathNode { float x, y, vx, vy; };
    int deathBurstNodeCount() const { return static_cast<int>(deathNodes_.size()); }
    const std::vector<DeathNode>& deathBurstNodes() const { return deathNodes_; }

private:
    PlayerState state_ = PlayerState::Idle;
    bool scriptedEntryWalk_ = false;  // CP-B1C-T1-M2 boss-entry walk mode
    bool cfgForceBoots_ = false;  // config "hasBoots": true override (test stages)
    const Tilemap* tilemap_ = nullptr;
    bool sourceGroundMovementEnabled_ = false;
    AnimationPlayer anim_;

    // Input
    bool inputLeft_ = false;
    bool inputRight_ = false;
    bool inputUp_ = false;
    bool inputDown_ = false;
    bool inputJumpHeld_ = false;
    bool inputDashHeld_ = false;
    bool inputShootHeld_ = false;
    bool inputShootPressed_ = false;  // Set true on frame shoot key goes down (fires normal shot)
    bool inputShootReleased_ = false; // Set true on frame shoot key is released

    // Movement timers
    int coyoteTimer_ = 0;
    int jumpBufferTimer_ = 0;
    int dashBufferTimer_ = 0;
    int dashTimer_ = 0;
    int wallJumpLockout_ = 0;
    int wallJumpDirection_ = 0;
    // wall_jump_release.json: six stationary updates before the seven kicks,
    // then one stationary handoff without consuming an extra gravity step.
    int wallJumpWindup_ = 0;
    // wall_dash_jump.json: A seen during preparation survives button release.
    bool wallJumpDashed_ = false;
    bool wallJumpResumePending_ = false;
    float wallJumpResumeY_ = 0.0f;
    int wallSide_ = 0;
    int wallDropPoseTimer_ = 0;
    bool wallDropVisualFacingRight_ = true;
    bool usedAirDash_ = false;   // Reset on landing — only one air dash per jump
    // T1.7 (Storm Eagle f2112): a dash-jump's rise ends once, and the source
    // restarts the fall from zero when it does. Armed by changeState.
    bool dashJumpRiseEnded_ = false;

    int landingAnimTimer_ = 0;   // Visual-only landing recovery pose; controls remain live.

    // R96C: previous platform support is a one-frame scene handoff.  It is
    // intentionally separate from tile onGround and never changes Y speed.
    bool stormEagleSupportLatched_ = false;
    bool stormEagleSupportFrameActive_ = false;
    bool sourceObjectDeparturePending_ = false;
    bool sourceObjectDepartureFrame_ = false;

    // Idle eye-blink. The real game shuts X's eyes for ~7 frames roughly every
    // ~70 frames while standing, with an occasional quick double-blink (measured
    // from emulator capture). Visual only — substitutes the blink sprite frame.
    int blinkCountdown_ = 60;        // frames until the next blink
    int blinkCloseTimer_ = 0;        // >0 while eyes are shut
    bool blinkPendingDouble_ = false;// queue a second quick blink
    unsigned int blinkRng_ = 0x1234567u;
    static constexpr int kIdleBlinkFrame = 62;  // sheet cell: idle, eyes shut
    int renderFrameIndex(int frameOverride = -1) const;

    // Combat state
    int chargeTimer_ = 0;    // How long shoot has been held
    int chargeLevel_ = 0;    // 0=none, 1=medium, 2=full
    int shotCooldown_ = 0;   // Frames until next shot allowed
    int shootAnimTimer_ = 0; // Frames left to hold the _shoot variant of the current state anim
    bool wasShootHeld_ = false; // Previous frame's shoot state (for release detection)

    // Fire Wave held stream (oracle 2026-06-11, _fire_wave_runs): the press
    // pays 1 whole unit and buys streamSubUnits sub-units ($7E1F8D model);
    // while Y stays held a segment re-emits every streamCadenceFrames at
    // streamSubPerSegment sub-units each, crossing into the next whole unit
    // when the counter runs dry. streamReemit_ marks a cadence emission so
    // fireShot skips the whole-unit spend.
    int streamCooldown_ = 0;
    int streamSubAmmo_ = 0;
    bool streamReemit_ = false;
    // Fire-wave held-stream flame sound: 0x61 at press+sfxStreamHoldFirst
    // then every sfxStreamHoldEvery while held (U41 measurement).
    int streamHoldSfxTimer_ = 0;
    // True while fireShot runs for a sub-full-charge RELEASE volley (ARM
    // upgrade, specials): the real SFX is the 0x17 pair with the fire id as
    // the +1f followup, not the plain fire sound (U41 subthresh_cur* runs).
    bool subThresholdRelease_ = false;
    // Manual review/debug grant: allow charged special weapons without
    // mutating persistent buster armor, so X's sprites do not change.
    bool debugSpecialChargeUnlocked_ = false;

    // Damage state
    int hurtTimer_ = 0;      // Frames remaining in hurt state
    int iframeTimer_ = 0;    // Frames of invincibility remaining
    bool hurtInitPending_ = false;
    bool hurtRecoveryPending_ = false;
    bool hurtEntryFrame_ = false;
    float hurtKnockbackDirection_ = -1.0f;
    int deathTimer_ = 0;     // Animation timer for death sequence
    std::vector<DeathNode> deathNodes_;  // Active death-burst particles

    // Movement state handlers
    void updateIdle();
    void updateRun();
    void updateJump();
    void updateFall();
    void updateWallSlide();
    void updateWallJump();
    void updateDash();
    void updateDashJump();
    void updateLadder();
    bool updateHurtTransition();
    void updateHurt();
    void updateDie();
    void spawnDeathBurst();

    // Combat
    void updateCombat();  // Runs every tick, parallel to movement
    void fireShot(ProjectileType type);
    void processWeaponCycleInput();
    Vector2 busterMuzzleOffsetForState() const;
    bool visualGroundingApplies() const;
    int countActiveShots() const;
    bool hasActiveSpecialShot() const;  // any special-weapon player shot on screen

    // Helpers
    // Source-ground selector. It samples the pre-physics support tile and
    // returns a positive magnitude; applyHorizontalInput owns the held-input
    // sign assignment. The explicit scene opt-in keeps ordinary Players on
    // the existing runSpeed path.
    std::optional<float> sourceGroundSpeed() const;
    void applyHorizontalInput();
    void applyHorizontalInputWithLockout();
    // R4.1: true while the measured walk start is still running, so the Run
    // state holds through its two zero-velocity frames instead of flickering
    // back to Idle.
    bool walkStartActive() const { return walkStartTick_ >= 0 && walkStartTick_ < kWalkStartFrames; }
    bool tryJump();
    bool tryDash();
    bool tryDashJump();
    bool tryLadder();
    void beginWallDropFall();
    // Idle/dash callers arm a delay; the guarded source Walk caller dispatches
    // ascent immediately after its horizontal speed has been selected.
    bool armGroundJump(bool dashJump);
    void fireGroundJump();
    void resetJumpState();
    bool canCoyoteJump() const;
    bool isTouchingWallToward() const;
    bool isTouchingLadder() const;
    void setupAnimations();
    const TextureResource* spriteSheetForCurrentWeapon();
    const TextureResource* buildWeaponSpriteVariant(const Weapon& weapon);
    bool buildVariantPixels(const std::string& rowKey, std::vector<Color>& out) const;
    const TextureResource* uploadVariantTexture(const std::string& key,
                                                const std::vector<Color>& pixels);
    // Recolor variant for an arbitrary x_weapon_rows key (sting cycle
    // phases). Indexed-palette path only; nullptr when unavailable.
    const TextureResource* variantForRowKey(const std::string& rowKey);
    std::string desiredArmorCompositeKey() const;
    std::vector<std::string> activeArmorPieces() const;
    bool uploadCompositeSheetTexture();

    // R4.1 -- the source's walk start, measured on Chill Penguin's opening
    // corridor with tools/oracle/lua/walk_stop_walk_probe.lua
    // (knowledge_base/mmx1/player/walk_stop_walk*.csv, X's 24-bit fixed-point
    // x): the frame a direction is first held advances 0; after a standstill of
    // at least kWalkStartRampStill frames the next four advance exactly 1 px,
    // the fifth 0, and only then does runSpeed apply. Stops of 6..9 frames skip
    // the four 1 px frames, stops of 10, 11, 12 and 235 run them.
    static constexpr int kWalkStartFrames = 6;
    static constexpr int kWalkStartRampStill = 10;
    static constexpr float kWalkStartRampSpeed = 1.0f;
    int walkStillFrames_ = 0;   // consecutive grounded frames with no direction held
    // R4.3 -- what follows a released ground dash. Measured over the 53 dash
    // runs of the two movies that end on the button's release: 51 of them are
    // followed by exactly two frames of no horizontal movement with the
    // direction still held, and then the walk speed.
    static constexpr int kDashReleaseRecoveryFrames = 2;
    // ground_dash_cap.json: one stationary frame after all 33 held advances.
    static constexpr int kDashCapRecoveryFrames = 1;
    int dashRecoveryTimer_ = 0;
    bool dashReleased_ = false;  // the running dash was cut by the button's release
    bool dashWallLimited_ = false;  // legacy wall grace shortened the timer
    // Preserve measured Idle/dash launch timing. ground_jump_launch.json
    // separates the source Walk caller, which also dispatches ascent in the
    // initialization update, from callers that return after initialization.
    static constexpr int kJumpLaunchDelayFrames = 1;
    int jumpLaunchDelay_ = 0;
    bool pendingDashJump_ = false;
    // Source D+$5C: Walk-jump or wall-dash magnitude retained in Jump/Fall.
    std::optional<float> sourceJumpSpeed_;
    int walkStartTick_ = -1;    // -1 = no direction held; else frames since it was
    bool walkStartRamping_ = false;

    int stingInvincibleTotal_ = 0;  // value set at release; render derives d
    std::string spriteSourcePath_;
    // U70: which sheet is live ("" until first refresh) — armored when the
    // full capsule set is on. Exposed for the contract test.
    std::string activeSheetPath_;
public:
    const std::string& activeSheetPath() const { return activeSheetPath_; }
    std::vector<std::string> armorPiecesForTest() const { return activeArmorPieces(); }
    void refreshArmorSheet();
private:
    std::unordered_map<std::string, TextureResource> weaponSpriteVariants_;
    std::unordered_map<std::string, std::string> armorDeltaPaths_ = {
        {"boots", "content/x1/sprites/armor/x_armor_boots_delta.png"},
        {"buster", "content/x1/sprites/armor/x_armor_buster_delta.png"},
        {"body", "content/x1/sprites/armor/x_armor_body_delta.png"},
        {"helmet", "content/x1/sprites/armor/x_armor_helmet_delta.png"},
        {"combo_01", "content/x1/sprites/armor/x_armor_combo_01_delta.png"},
        {"combo_02", "content/x1/sprites/armor/x_armor_combo_02_delta.png"},
        {"combo_03", "content/x1/sprites/armor/x_armor_combo_03_delta.png"},
        {"combo_04", "content/x1/sprites/armor/x_armor_combo_04_delta.png"},
        {"combo_05", "content/x1/sprites/armor/x_armor_combo_05_delta.png"},
        {"combo_06", "content/x1/sprites/armor/x_armor_combo_06_delta.png"},
        {"combo_07", "content/x1/sprites/armor/x_armor_combo_07_delta.png"},
        {"combo_09", "content/x1/sprites/armor/x_armor_combo_09_delta.png"},
        {"combo_0b", "content/x1/sprites/armor/x_armor_combo_0b_delta.png"},
        {"combo_0c", "content/x1/sprites/armor/x_armor_combo_0c_delta.png"},
        {"combo_0d", "content/x1/sprites/armor/x_armor_combo_0d_delta.png"},
        {"combo_0e", "content/x1/sprites/armor/x_armor_combo_0e_delta.png"},
        {"fullset", "content/x1/sprites/armor/x_armor_fullset_delta.png"},
    };
    XPaletteTable xPalette_;  // legend per-weapon recolor (collected from Spriters X sheet)
    XSheetComposer sheetComposer_;
    TextureResource compositeSheetTexture_;
    std::string armorCompositeKey_;
};

} // namespace mmx
