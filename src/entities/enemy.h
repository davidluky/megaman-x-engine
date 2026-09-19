// enemy.h - declares enemy definitions, runtime state, and behavior contracts.
// Owns: enemy type metadata, movement state, and KB behavior bindings.

#pragma once

#include "actor.h"
#include "stretch_bird_parent.h"
#include "deck_turret_parent.h"
#include "mad_pecker_parent_source.h"
#include "systems/animation.h"
#include "systems/raylib_resource.h"
#include <memory>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>

// ============================================================================
// enemy.h — Enemy entity with behavior system
//
// Enemies use a simple state-based AI. Each enemy type has a behavior that
// defines what it does: patrol back and forth, hide behind a shield and shoot,
// fly in patterns, etc.
//
// Enemy definitions are loaded from JSON (content/x1/enemies/enemy_defs.json).
// Each definition sets behavior, HP, damage, sprite, hitbox, and AI parameters.
// Captured KB FSM playback is an explicit per-definition opt-in so a content
// enemy cannot accidentally change behavior just because a KB folder shares
// its id.
//
// Enemies interact with the player through collision:
//   - Contact damage: player touches enemy hitbox -> player takes damage
//   - Bullet damage: player's projectile hits enemy -> enemy takes damage
//   - Enemy bullets: some enemies shoot projectiles at the player
// ============================================================================

namespace mmx {

enum class EnemyBehavior {
    Patrol,        // Walk back and forth, turn at edges/walls
    HideAndShoot,  // Met: hide behind shield, peek, shoot, hide
    FlyPattern,    // Fly in a pattern, swoop toward player
    Stationary,    // Stand in place, face player
    Drift,         // Float in a straight line at constant velocity (no gravity, no AI)
    Turret,        // Stand in place; fire at the player when in range
    AxeMax,        // CP OID 0x0B: stationary lumberjack, 2-log volleys (oracle)
    Hover,         // Fly in place at the spawn height (no gravity) — e.g. helicopter
    Walker,        // CP OID 0x51 (U17): pure chaser, stand 55f / walk 30f
                   // at exactly 1.5 px/f toward X; fires from stands
    Bat,           // CP OID 0x19 (U19): fixed-height patrol at exactly
                   // 1.25 px/f; map-fixed hover dropping 2 ground mines
    BatDrift,      // CP OID 0x2D (plan R1.0x2D): hang at the spawn, then fly
                   // at exactly 1 px/f in a drawn direction, held 20-53
                   // frames, and repeat (enemy_bat_drift_law.h)
    Anchored,      // gravity-free and completely still: the measured law
                   // of CP OID 0x53 (the mad pecker, plan R1.0x53) and CP OID
                   // 0x04 (the stretch bird, R1.0x04) -- dx = dy = 0 on every
                   // frame of every movie instance of both. Distinct from
                   // Stationary, which carries gravity, and from Hover, which
                   // adds a bob
    HoverPatrol,   // SE OID 0x49: source setup, player-X trigger, and the
                   // measured action-2 down/hold/up path around the patrol.
    SnowballRoll,  // CP OID 0x54 (plan R1.0x54): a rolling snowball. While it
                   // is AIRBORNE it only falls, at the measured 64 fp per
                   // frame; while it is GROUNDED its horizontal velocity is
                   // re-written by exactly -12 fp every frame and its vertical
                   // one is zero. The push is a constant, not the slope's
                   // share of gravity: the source keeps accelerating on FLAT
                   // ground too, which no slope-driven model does
    EagletBurst,   // SE OID 0x56 (plan R7.eggs): the four eaglets the boss's
                   // egg hatches into. Gravity-free, they hold the velocity
                   // they were spawned with -- the measured (+/-361, +/-361)
                   // fp diagonals -- for a measured number of frames and then
                   // leave. The DIVE that follows the burst is measured in
                   // attack_kinematics.json and deliberately not wired: the
                   // movie only shows it for the two that survive
    StrideWalk     // SE OID 0x27 (plan R1.0x27): a two-legged strider that
                   // does not have a speed. Its fixed-point x holds still for
                   // twelve frames and then JUMPS, once per cell of an
                   // eight-cell walk, by a constant that belongs to the cell
                   // (6, 9, 9, 6, 6, 11, 9, 7 px = 63 px per cycle). Two
                   // frames after the spawn it hops 7 px forward and 4 px up
                   // into the walk. Direction = the side X is on at the spawn
};

std::optional<EnemyBehavior> parseEnemyBehavior(const std::string& behavior);

enum class EnemyState {
    Idle,
    Patrol,
    Hide,       // Met: hiding behind helmet (invulnerable)
    Peek,       // Met: about to shoot
    Shoot,      // Met: firing
    Fly,
    Hurt,
    Dead
};

class Player;  // Forward declare
class Tilemap; // Forward declare
class KBFSM;   // Forward declare

class Enemy : public Actor {
public:
    // JSON-loaded definition — shared across all enemies of the same type.
    // Texture is loaded once and shared by all instances of that enemy type.
    struct Definition {
        Definition() = default;
        Definition(const Definition&) = delete;
        Definition& operator=(const Definition&) = delete;
        ~Definition() = default;
        Definition(Definition&&) noexcept = default;
        Definition& operator=(Definition&&) noexcept = default;

        std::string behavior;
        std::string kbBehaviorId; // Optional knowledge_base/mmx1/enemies/<id>/behavior.json opt-in
        int hp;
        int contactDamage;
        std::string spritePath;
        TextureResource texture; // Loaded once, shared across instances
        // FW7-REVIEW-C: optional exact palette-0 flash variant of the sheet.
        // One visible frame on a surviving hit (source f447 targetFlashProof:
        // twelve OAM cells, identical geometry/tiles, OBJ palette 7 -> 0).
        std::string flashSpritePath;
        TextureResource flashTexture;
        // Per-definition sprite cell size. Defaults to 32x32 so pre-existing
        // sheets keep rendering identically. Larger enemies (rider 50x37,
        // scrapmetal, axemax) override these so they render at native size.
        int frameWidth = 32;
        int frameHeight = 32;
        int frameCount = 0;      // derived: texture.width / frameWidth, 0 before texture loads
        // Body-only render correction. Default zero preserves every existing
        // definition. It never shifts physics, hitboxes, spawns, terrain, or
        // separately rendered child/launcher/projectile compositions.
        float bodyVisualOffsetY = 0.0f;
        // Most enemies mirror their authored body cell when logical facing is
        // left. Source-authored fixed-facing bodies (FE5.7B Axe Max) can opt
        // out without changing projectile/launcher direction semantics.
        bool bodyMirrorsWithFacing = true;
        // The walker sheet is authored facing left; most legacy sheets are
        // authored facing right. This controls only the body texture mirror.
        bool bodySourceFacesRight = true;
        float hitboxWidth, hitboxHeight;
        float hitboxOffsetX, hitboxOffsetY;
        float patrolSpeed = 0.8f;
        float flySpeed = 1.5f;
        float detectionRange = 100.0f;
        int dropType = 1; // 0=nothing, 1=small health, 2=large health, 3=small ammo, 4=extra life
        // Real MMX1 drops are RNG-gated (axemax E5: 2 drops over 18 unique
        // kill frames — one small health, one small ammo). dropChancePct is
        // 0-100; dropTypes (when non-empty) is the uniform candidate pool
        // overriding dropType. Missing evidence fails closed to no drop.
        int dropChancePct = 0;
        std::vector<int> dropTypes;

        // Per-(weapon, form) damage this enemy takes — the real MMX1
        // mechanism (oracle shotgun-ice campaign 2026-06-09: ice pellet
        // deals 2 to the 8HP snow-cannon yet one-shots the 4HP walker).
        // Key = weapon id; -1 = form unspecified (lookup rules in
        // enemy_damage.h). Loaded from the def's "damage_taken" block.
        struct DamageTaken { int normal = -1; int fragment = -1; int charged = -1; };
        std::unordered_map<std::string, DamageTaken> damageTaken;

        // Optional per-def animation overrides. If a state name here matches
        // one of the hardcoded defaults (idle/walk/hide/peek/shoot/hurt/dead),
        // it replaces that default in setupAnimations. Partial overrides are
        // fine — unspecified states fall back to the hardcoded frame indices.
        std::unordered_map<std::string, Animation> animations;
    };

    Enemy();
    ~Enemy();
    Enemy(Enemy&&) noexcept;
    Enemy& operator=(Enemy&&) noexcept;
    Enemy(const Enemy&) = delete;
    Enemy& operator=(const Enemy&) = delete;

    // Shared definition registry — loaded once per stage
    static std::unordered_map<std::string, Definition> definitions;
    static void loadDefinitions(const std::string& path);
    static void unloadDefinitions();
    // Protected-content cleanup keeps old authored map IDs loadable without
    // retaining a second provisional combat definition for the same source OID.
    static std::string canonicalDefinitionId(const std::string& type);

    // Initialize from JSON definition. Type must match a key in definitions map.
    void init(const std::string& type, float x, float y);

    void update(float dt) override;
    void render(float alpha) override;
    void render(float alpha, Vector2 cameraOffset);

    // Set references for AI
    void setTarget(Player* target) { target_ = target; }
    void setTilemap(const Tilemap* tm) { tilemap_ = tm; }
    // Opt-in for the measured canonical CP Walker instance (T1.7l).
    void configureSourceWalker();
    bool hasSourceWalker() const { return sourceWalker_; }
    void finishSourceWalkerMotion(float attemptedY);

    EnemyBehavior behavior = EnemyBehavior::Patrol;
    EnemyState enemyState = EnemyState::Idle;
    std::string type;
    // Monotonic spawn id (assigned in init). Needed for stable references:
    // enemies_ is compacted every frame, so indices/pointers move (the
    // torpedo fan's acquired-target identity keys off this).
    int serial = 0;

    // Combat
    int contactDamage = 2;
    bool invulnerable = false; // Met is invulnerable while hiding
    // R4.6: false until the camera brings this enemy inside the activation
    // window. An opted-in CP replacement starts with a fresh closed latch.
    bool cameraActivated = false;
    // R72: opt-in only for authored Axe Max placements in canonical CP.
    void configureCpCameraReturn();
    std::optional<Vector2> sourceCreationOrigin() const {
        if (cpCameraReturn_) return cameraReturnSourceOrigin_;
        return std::nullopt;
    }
    bool recycleForCameraExit(float cameraX, float cameraY);
    int hitFlash = 0;          // frames remaining of the white damage flash
    int hitFlashVisual = 0;    // FW7-REVIEW-C: exact palette-0 flash frames left (1 on a surviving hit)

    // Patrol params
    float patrolSpeed = 0.8f;
    float detectionRange = 100.0f;

    // Met-specific timers
    int hideTimer = 0;
    int peekTimer = 0;
    int shootTimer = 0;

    // FlyPattern (Batton) params
    float flySpeed = 1.5f;      // Swoop speed
    float returnSpeed = 0.8f;   // Speed returning to home position

    // Death + drops
    int deathTimer = 0;
    static constexpr int DEATH_FLASH_FRAMES = 28;

    // Death pop — small radial spark burst when the enemy is destroyed.
    struct DeathNode { float x, y, vx, vy; };
    int deathBurstNodeCount() const { return static_cast<int>(deathNodes_.size()); }

    // What this enemy drops on death (loaded from JSON definition)
    // 0=nothing, 1=small health, 2=large health, 3=small ammo, 4=extra life
    int dropType = 0;
    int dropChancePct = 0;
    std::vector<int> dropTypes;

    // AxeMax standing log magazine — oracle-true visible stack (launch
    // empties the fire-line slot, restocks 70f/54f before the next volley)
    // that persists restocked + frozen after death (launcher remnant).
    int logStackCount() const { return axeStack_; }
    bool isRemnantStack() const { return remnant_; }

    // standing_log_contact.json: the observed first CP remnant's child is
    // at the retained source origin minus16Y, independent of body settling.
    // Other placements and stock/lifecycle states keep their existing path.
    std::optional<Vector2> cpRemnantLogSourceAnchor() const {
        // second_remnant_descriptors_2026-09-17.json closes the second placement.
        const bool measured =
            (cameraReturnNativeOrigin_.x == 501.0f && cameraReturnNativeOrigin_.y == 1146.0f) ||
            (cameraReturnNativeOrigin_.x == 1013.0f && cameraReturnNativeOrigin_.y == 1114.0f);
        if (!cpCameraReturn_ || !active || !remnant_ || axeStack_ != 2 || !measured) {
            return std::nullopt;
        }
        return Vector2{cameraReturnSourceOrigin_.x,
                       cameraReturnSourceOrigin_.y - 16.0f};
    }

    // The stack holds TWO stocked logs, so the source runs the $86:C0CE
    // contact against two boxes 16 px apart. Measured 2026-09-15 (T1.7
    // f1537, `build/t17-cp` objects.csv, the $7E1468 table): the lower log
    // stands at (469, 1144) and the upper one settles at (469, 1128) on
    // source frame 1545 - the retained parent origin (469, 1160) minus 32.
    std::optional<Vector2> cpRemnantUpperLogSourceAnchor() const {
        const auto lower = cpRemnantLogSourceAnchor();
        if (!lower) return std::nullopt;
        return Vector2{lower->x, lower->y - 16.0f};
    }

    // The launcher/log composition is a solid movement blocker in the
    // Chill Penguin opening.  It is intentionally separate from the enemy
    // body hitbox: the source lets the player collide with the stack while
    // shots can still damage the stack/body through the normal combat lane.
    AABB solidAxeStackHitbox() const;

    // launcher_solid_box_2026-09-15.json (T1.7g): the source contact list of
    // the measured CP remnant, in world (= RAM) pixels. Records in the
    // artifact's order - launcher $1428 at the retained source origin, lower
    // log $1468, upper log $14A8 - each walking $86:C0CE then $86:C0D2 against
    // the player's $86:A552. Boxes are inclusive source pixels widened by one
    // (x = centre + offset - halfExtent, w = 2 * halfExtent + 1), so a strict
    // AABB overlap is exactly the source's `distance <= sum` contact. Empty
    // for placements and stock states without measured record positions.
    struct AxeStackSourceBox {
        std::uint16_t descriptor;  // bank $86 address
        AABB box;
    };
    std::vector<AxeStackSourceBox> axeStackSourceContactBoxes() const;

    // Per-instance visual correction used only when a source capture proves
    // that foreground snow occludes a child sprite at a different seam than
    // the shared enemy definition.  Physics and hitboxes are unchanged.
    void setBodyVisualOffsetY(float offsetY) { bodyVisualOffsetY_ = offsetY; }

    // Provisional census placeholder (plan task A2, waiver W2): a spawn for an
    // OID without a runtime definition keeps the OID as its label and takes
    // its HP from the census (spawn_census.json observed_hp) when measured.
    void setPlaceholder(const std::string& oid, int hp);
    const std::string& placeholderLabel() const { return placeholderLabel_; }

    // The drop decision for this kill. Rolls are injected for determinism
    // (gameplay passes rand(); tests pass fixed values): roll100 in 0-99
    // gates against dropChancePct, pickRoll selects uniformly from
    // dropTypes. Returns the PickupType int, or 0 for no drop.
    int rollDropType(int roll100, int pickRoll) const {
        if (dropTypes.empty() && dropType <= 0) return 0;
        if (roll100 >= dropChancePct) return 0;
        if (dropTypes.empty()) return dropType;
        return dropTypes[static_cast<size_t>(pickRoll) % dropTypes.size()];
    }

    // Projectile spawning (enemy shots go into this vector, consumed by gameplay_scene)
    struct PendingShot {
        float x, y, vx, vy;
        // U19 bat mine: falls under gravity with inherited drift, lands,
        // STOPS, and persists as a ground hazard (the scene maps this to
        // the projectile's mineHazard flag).
        bool mine = false;
        bool sourceAxeMaxLog = false;
        // Optional visual/damage override (Axe Max log: 32x16 strip; base
        // damage 2, source trace 2026-07-05). Empty sprite = legacy orb.
        std::string sprite;
        float w = 0, h = 0;
        int damage = 2;
        bool visualMirrorsWithFacing = true;
        // Optional art dimensions independent of the collision override w/h.
        int visualWidth = 0, visualHeight = 0;
        int visualFrameCount = 1, visualFrameTicks = 1;
    };
    std::vector<PendingShot> pendingShots;

    const stretch_bird_parent::State& stretchBirdState() const { return stretchBirdState_; }
    bool consumeStretchBirdLaunch() { return std::exchange(stretchBirdLaunch_, false); }
    void signalStretchBirdChild() { ++stretchBirdState_.childSignal; }
    const deck_turret_parent::ParentFields& deckTurretState() const { return deckTurretController_.fields(); }
    bool consumeDeckTurretLaunch() { return std::exchange(deckTurretLaunch_, false); }
    const mad_pecker_parent_source::Controller& madPeckerState() const { return madPeckerController_; }
    bool consumeMadPeckerLaunch() { return std::exchange(madPeckerLaunch_, false); }

private:
    mad_pecker_parent_source::Controller madPeckerController_{};
    bool madPeckerLaunch_ = false;
    deck_turret_parent::ParentController deckTurretController_{deck_turret_parent::ParentFields{}};
    bool deckTurretLaunch_ = false;
    stretch_bird_parent::State stretchBirdState_{};
    bool stretchBirdLaunch_ = false;
    Player* target_ = nullptr;
    const Tilemap* tilemap_ = nullptr;
    AnimationPlayer anim_;

    // Per-instance copy of the definition's frame dims so render() avoids a
    // definitions-map lookup per frame. Populated in init(). Defaults keep
    // pre-existing 32x32 sheets rendering identically.
    int frameWidth_ = 32;
    int frameHeight_ = 32;
    int frameCount_ = 0;
    float bodyVisualOffsetY_ = 0.0f;
    bool bodyMirrorsWithFacing_ = true;
    bool bodySourceFacesRight_ = true;
    std::string placeholderLabel_; // set only for provisional census placeholders
    bool cpCameraReturn_ = false;
    Vector2 cameraReturnNativeOrigin_ = {0, 0};
    Vector2 cameraReturnSourceOrigin_ = {0, 0};

    void updatePatrol();
    void updateHideAndShoot();
    void updateFlyPattern();
    void updateDrift();   // Intro Highway OID 0x15 flyer: constant straight-line drift
    void updateTurret();  // Intro Highway OID 0x29 robot: stationary, fires in range
    void updateAxeMax();  // CP OID 0x0B lumberjack: 2-log volleys, alert near X
    void updateWalker();  // CP OID 0x51 chaser: stand/walk cycle, fires from stands
    void updateBat();     // CP OID 0x19: fixed-height patrol + hover mine drops
    void updateBatDrift(); // CP OID 0x2D: hang, then 1 px/f drawn-direction flight
    void updateAnchored(); // gravity-free and completely still (0x53, 0x04)
    void updateHoverPatrol(); // SE OID 0x49 source motion; physics integrates once
    void updateStrideWalk();  // SE OID 0x27: per-cell discrete steps
    void updateEagletBurst(); // SE OID 0x56: the hatch's fixed diagonal
    void updateSnowballRoll();// CP OID 0x54: -12 fp per grounded frame
    void updateHover();   // Intro Highway OID 0x0F helicopter: flies in place (gravity-free)
    void updateDead();
    void spawnDeathBurst();
    std::vector<DeathNode> deathNodes_;
    float distToTarget() const;
    float dirToTarget() const; // -1 or +1
    void setupAnimations();

    // FlyPattern (Batton) state
    Vector2 homePos_ = {0, 0};   // Where the enemy hangs/returns to
    int flyTimer_ = 0;            // Frames in current fly sub-state
    bool swooping_ = false;       // Currently diving toward player
    float swoopVX_ = 0, swoopVY_ = 0; // Swoop velocity (locked on initiation)

    // AxeMax (CP 0x0B) volley state — oracle _snow_cannon_runs/FINDINGS.md
    int axeTimer_ = 0;        // frames until the next fire
    int axeShotInVolley_ = 0; // 0 = first shot of the volley, 1 = second
    int axeSweep_ = -1;       // >=0: frames into the firing anim sweep
    int axeStack_ = 2;        // visible logs in the standing magazine (0-2)
    bool axeAlertLatched_ = false; // alert anim latched per rest cycle
    bool remnant_ = false;    // post-death frozen launcher (render-only)

    // Walker (CP 0x51, U17) — oracle _walker_runs/FINDINGS.md: 27f
    // activation stand, then stand 55f / walk 30f at exactly 1.5 px/f
    // toward X; firing stands stretch to 125f with the shot launched at
    // stand+48 (from anchor−18·facing at 2.25 px/f toward X, SFX 0x33).
    bool walkerWalking_ = false;
    bool sourceWalker_ = false;
    int sourceWalkerPhase_ = 0;
    int sourceWalkerWait_ = 1;
    int sourceWalkerCell_ = 0;
    int sourceWalkerCellTicks_ = 8;
    int sourceWalkerDrawCell_ = 0;
    float sourceWalkerSpawnX_ = 0;
    float sourceWalkerVx_ = 0;
    float sourceWalkerVy_ = -4;
    Vector2 sourceWalkerDrawPosition_{};
    bool sourceWalkerDrawFacing_ = false;
    bool sourceWalkerDrawReady_ = false;
    void updateSourceWalker();
    void pickSourceWalkerLaunch();
    int walkerTimer_ = 0;       // frames left in the current stand/walk
    int walkerStandLen_ = 0;    // current stand's total length (55 or 125)
    int walkerStandAge_ = 0;    // frames elapsed in the current stand
    bool walkerStandFires_ = false;
    int walkerStandIndex_ = 0;  // stands seen (the fire gate alternates)

    // Bat (CP 0x19, U19) — oracle _bat_runs/FINDINGS.md: patrol at the
    // spawn height at EXACTLY 1.25 px/f (the 4f cycle -1,-2,-1,-1); after
    // batHoverAfterPx_ of travel it HOVERS 118f, dropping 2 mines at
    // hover+25 and hover+49 (bat-10px below), then resumes; off-camera
    // wrap respawns it at its spawn point (engine: the scene's camera
    // cull + stage respawn handles it).
    float batTravelled_ = 0.0f;
    int batHoverTimer_ = 0;     // >0 = hovering, counts down
    bool batHoverDone_ = false; // one choreographed hover per pass
    int batMinesDropped_ = 0;

public:
    // StrideWalk (SE 0x27, plan task R1.0x27) -- measured over all four movie
    // instances (knowledge_base/mmx1/enemies/oid_0x27/ghost.csv, 32 steps):
    // the step taken when a cell is left is a constant per cell and never
    // varies, and the cadence is 12 frames (the movie's 11..44 is SNES lag,
    // which can only inflate a frame count).
    static constexpr int kStrideCells = 8;
    static constexpr int kStrideFrames = 12;
    static constexpr int kStrideSpawnFrames = 2;
    static constexpr int kStrideSpawnHopX = 7;
    static constexpr int kStrideSpawnHopY = 4;
    static constexpr int kStrideStep[kStrideCells] = {6, 9, 9, 6, 6, 11, 9, 7};
    int strideCell_ = 0;
    int strideTimer_ = 0;
    bool strideSpawning_ = true;

    // EagletBurst (SE 0x56, plan task R7.eggs). The spawner hands the eaglet
    // its measured fixed-point velocity and the number of frames the source
    // logs it for; the enemy integrates in the source's own 1/256 px so the
    // diagonal never accumulates float error, and deactivates when the count
    // runs out. `lifeFrames` 0 means "no measured lifetime": it just flies.
    void setEagletBurst(int vxFp, int vyFp, int lifeFrames);
    int eagletFramesLeft() const { return eagletLife_; }

    // BatDrift (CP 0x2D, plan task R1.0x2D) — the law is a measured
    // distribution (knowledge_base/mmx1/enemies/oid_0x2D_bat_family_pending/
    // direction_law.json, generated into src/entities/enemy_bat_drift_law.h):
    // hang, then one pixel per frame in a drawn direction for a drawn number
    // of frames, repeat. Every draw comes from this enemy-local RNG so a test
    // can seed it, and `setBatDriftChoiceSequence` replaces the draws outright
    // with a recorded sequence so a ghost can replay one source instance.
    struct BatDriftChoices {
        std::vector<int> hangFrames;
        std::vector<int> holdFrames;
        std::vector<std::pair<int, int>> directions;  // vx, vy at 1/256 px
    };
    void seedBatDriftRng(uint32_t seed);
    void setBatDriftChoiceSequence(BatDriftChoices choices);
    int batDriftHangRemaining() const { return batDriftHang_; }
    int batDriftHoldRemaining() const { return batDriftHold_; }
    int batDriftVx() const { return batDriftVx_; }
    int batDriftVy() const { return batDriftVy_; }
    int batDriftFixedX() const { return batDriftXf_; }
    int batDriftFixedY() const { return batDriftYf_; }

private:
    // SE OID 0x49 source state.  The stored fixed-point velocities remain
    // separate from the applied Actor velocity: a source boundary can cancel
    // this frame's applied step while retaining the reversed patrol speed for
    // the next update. Source position words use floor(position), matching
    // the 16-bit ROM comparisons.
    enum class HoverPatrolSourcePhase : std::uint8_t {
        SetupReference,
        SetupVelocity,
        Horizontal,
        Action2Down,
        Action2Hold,
        Action2Up,
    };
    HoverPatrolSourcePhase hoverPatrolSourcePhase_ =
        HoverPatrolSourcePhase::SetupReference;
    int hoverPatrolSourceTimer_ = 0;
    int hoverPatrolSourceVxFp_ = 0;
    int hoverPatrolSourceVyFp_ = 0;
    std::uint16_t hoverPatrolReferenceX_ = 0;
    bool hoverPatrolHorizontalEntered_ = false;

    // The 24-bit fixed-point position the source keeps (pixel * 256 + the
    // sub-pixel byte); `position` is derived from it every frame so the drift
    // never accumulates float error.
    int batDriftXf_ = 0;
    int batDriftYf_ = 0;
    int batDriftHang_ = 0;      // frames left motionless at the spawn
    int batDriftHold_ = 0;      // frames left on the current direction
    int batDriftVx_ = 0;        // 1/256 px per frame
    int batDriftVy_ = 0;        // 1/256 px per frame, positive is UP
    uint32_t batDriftRng_ = 0;

    // EagletBurst: the source's own 1/256 px, positive vy is DOWN here
    // because that is how the movie's eaglet rows integrate.
    int eagletXf_ = 0;
    int eagletYf_ = 0;
    int eagletVx_ = 0;
    int eagletVy_ = 0;
    int eagletLife_ = 0;

    // SnowballRoll: the source's own horizontal velocity in 1/256 px, which it
    // re-writes by -12 on every grounded frame and never resets.
    static constexpr int kSnowballAccelFp = -12;
    int snowballVxFp_ = 0;
    BatDriftChoices batDriftChoices_;
    int batDriftHangDraws_ = 0;
    int batDriftHoldDraws_ = 0;
    int batDriftDirectionDraws_ = 0;
    uint32_t nextBatDriftRoll();
    void drawBatDriftDirection();
    void renderBody(float drawX, float drawY, bool allowHitFlashOverlay);
    void renderAxeStack(float drawX, float drawY);
    const TextureResource* flashSheetResource_ = nullptr; // Borrowed FW7-REVIEW-C flash variant.

    // KB-driven FSM (optional — null when using legacy EnemyBehavior enum)
    std::unique_ptr<KBFSM> kbFsm_;
    void updateKBFSM();
    void applyKBState(const std::string& stateName, bool enteredState);
};

} // namespace mmx
