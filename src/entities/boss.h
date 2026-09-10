// boss.h - declares boss runtime state, attacks, phases, and arena bounds.
// Owns: boss health, combat states, and phase/attack data contracts.

#pragma once

#include "actor.h"
#include "entities/boss_death_timeline.h"
#include "systems/animation.h"
#include <string>
#include <vector>
#include <functional>
#include <cstdlib>

// ============================================================================
// boss.h — Boss entity with phase-based attack system
//
// Bosses are complex enemies with:
//   - Multiple phases triggered by health thresholds
//   - Each phase has a set of attacks chosen by weighted random selection
//   - Arena lock-in (camera confines to boss room)
//   - Intro sequence (health bar fills dramatically)
//   - Death sequence (explosions, weapon drop)
//   - Weakness chart (certain weapons deal extra damage)
//
// The boss system supports two modes:
//   1. Scripted encounters (Vile): predetermined behavior, can't be killed
//   2. Full boss fights (Mavericks): phase-based AI, killable, drops weapon
//
// Attack definitions:
//   Each attack has: name, type, startup frames, active frames, recovery,
//   projectile data, movement pattern. Attacks are selected from the
//   current phase's attack list with weighted probabilities.
// ============================================================================

namespace mmx {

class Player; // Forward declare
class TextureResource;

// Types of attacks a boss can perform
enum class BossAttackType {
    Charge,         // Dash toward player (contact damage)
    ProjectileBurst,// Fire multiple projectiles in a pattern
    JumpAttack,     // Jump to a position, attack on landing
    GroundPound,    // Jump, slam ground (shockwave)
    Slide,          // Low slide across ground
    Special         // Unique per-boss (handled by boss-specific code)
};

// One attack in a boss's repertoire
struct BossAttack {
    std::string name;
    BossAttackType type = BossAttackType::Charge;
    int weight = 10;          // Selection probability weight

    // Timing (in physics frames)
    int startup = 12;         // Frames before the attack becomes active
    int active = 20;          // Frames the attack is live
    int recovery = 24;        // Frames of cooldown after attack

    // Movement during attack
    float moveSpeed = 3.0f;   // Speed during active phase
    bool faceTarget = true;   // Face player at start of attack

    // Projectile data (for ProjectileBurst)
    int projectileCount = 1;
    float projectileSpeed = 3.0f;
    float spreadAngle = 0.0f; // Degrees spread for burst patterns
    int projectileDamage = 3;
    float projectileVY = 0.0f; // Vertical velocity (for angled shots)

    // Jump data (for JumpAttack, GroundPound)
    float jumpVelocity = -6.0f;
    float jumpHSpeed = 2.0f;  // Horizontal speed during jump

    // Special flags
    bool wallBounce = false;       // Slide bounces off walls instead of stopping
    bool invincibleDuring = false; // Boss takes no damage during this attack
};

// A phase in the boss fight
struct BossPhase {
    std::string name;         // "Phase 1", "Enraged", etc.
    float healthThreshold;    // Phase activates when HP% drops below this (1.0 = start)
    std::vector<BossAttack> attacks;
    float speedMultiplier = 1.0f; // Makes boss faster in later phases

    // Select a random attack based on weights
    const BossAttack* selectAttack() const;
};

enum class BossState {
    // Values 0..2 are a committed parity-trace ABI. Keep them explicit: the
    // CP-B1A checker consumes them without importing engine code.
    Dormant = 0, // Before player enters arena
    Intro = 1,   // Health bar filling, name display
    Idle = 2,    // Between attacks, facing player
    Startup,    // Wind-up before attack
    Active,     // Attack in progress
    Recovery,   // Cooldown after attack
    Stunned,    // Hit by weakness weapon (brief stun)
    Dying,      // Death explosions
    Dead,       // Fight over, weapon dropped
    Rescued     // Vile-specific: Zero rescue sequence
};

class Boss : public Actor {
public:
    Boss() = default;
    ~Boss() override = default;
    Boss(const Boss&) = delete;
    Boss& operator=(const Boss&) = delete;
    Boss(Boss&&) = delete;
    Boss& operator=(Boss&&) = delete;

    void init(const std::string& bossType, float x, float y);

    // Load boss definition from JSON (attacks, phases, stats)
    bool loadFromFile(const std::string& path);

    void update(float dt) override;
    void render(float alpha) override;
    void render(float alpha, Vector2 cameraOffset);

    void setTarget(Player* target) { target_ = target; }

    BossState bossState = BossState::Dormant;
    std::string type;

    // Combat
    AABB getPlayerShotHitbox() const;
    bool overlapsPlayerShot(const AABB& shot) const;
    int contactDamage = 4;
    int hitFlash = 0;          // Post-hit blink/invulnerability frames

    // Sprite (Phase 1). Optional — bosses without a loaded texture fall back
    // to the colored-rectangle render. Bottom-center anchored on hitbox
    // bottom-center to match Enemy::render and accommodate sprites larger
    // than the hitbox (e.g. mech-mode Vile).
    const TextureResource* texture = nullptr;
    int frameWidth = 0;     // 0 means "no texture loaded, use rectangle path"
    int frameHeight = 0;
    int frameCount = 0;     // cached texture width / frameWidth, computed at load

    // Optional palette-only atlas with the same cells and alpha as the primary.
    const TextureResource* hitPaletteTexture = nullptr;

    // Sprite (Phase 2 alt — mech mode for Vile, unused by other bosses).
    const TextureResource* textureAlt = nullptr;
    int frameWidthAlt = 0;
    int frameHeightAlt = 0;
    int frameCountAlt = 0;
    bool useTextureAlt = false; // Switched true on Vile's phase-2 transition

    // Bind a cached sprite texture from a path. Idempotent, sets frame dims,
    // and computes frameCount from the loaded width. Returns false on failure
    // (texture stays null; render falls back to rectangle path).
    bool loadTexture(const std::string& path, int frameW, int frameH);
    bool loadTextureAlt(const std::string& path, int frameW, int frameH);
    bool loadHitPaletteTexture(const std::string& path);

    // AI timing
    int fightTimer = 0;        // Total frames since fight started
    int rescueTime = 600;      // Vile: frames until Zero arrives
    bool dormant = true;       // Inactive until player enters arena

    // Phase system
    std::vector<BossPhase> phases;
    int currentPhase = 0;

    // State queries
    bool isRescued() const { return bossState == BossState::Rescued; }
    bool isDying() const { return bossState == BossState::Dying; }
    bool isDead() const { return bossState == BossState::Dead; }
    int rescueTimer() const { return rescueTimer_; }
    int introTimer() const { return introTimer_; }
    // C2: ticks since the slot-live frame
    int introSeTick() const { return introTimer_ - 1; }
    int deathTimer() const { return deathTimer_; }

    // CP-B1A: explicitly configured by the canonical CP stage adapter. Other
    // CP instances keep the generic intro and their own room geometry.
    void configureCpStageIntro(float landingY);
    bool cpStageIntroConfigured() const { return cpStageIntroConfigured_; }
    bool usesScriptedKinematics() const {
        return cpStageIntroConfigured_ &&
               (bossState == BossState::Dormant || bossState == BossState::Intro);
    }
    int displayHealth() const {
        // C2: the SE intro drives the gauge from the measured fill table.
        if (seMeasured_ && bossState == BossState::Intro) return seIntroDisplayHealth_;
        return cpStageIntroConfigured_ &&
               (bossState == BossState::Dormant || bossState == BossState::Intro)
            ? cpDisplayHealth_
            : health;
    }
    bool introVisible() const;
    // CP-B1C-H1: whether the boss HP gauge should be on screen. Only the
    // source-timed CP intro withholds it (absent through t312, revealed at
    // t313); every other boss and every post-intro state keeps prior behavior.
    bool introHudVisible() const {
        return !(cpStageIntroConfigured_ &&
                 (bossState == BossState::Dormant ||
                  bossState == BossState::Intro)) ||
               cpHudVisible_;
    }
    int introSourceTick() const {
        if (!cpStageIntroConfigured_) return introTimer_;
        return bossState == BossState::Dormant ? 83 : 84 + introTimer_;
    }

    // Weakness system: weapon_id → damage multiplier (default 1.0)
    float getWeaknessMult(const std::string& weaponId) const;
    std::vector<std::pair<std::string, float>> weaknesses;

    // Projectile spawning (boss shots → gameplay scene's projectile list)
    struct PendingShot {
        float x, y, vx, vy;
        int damage = 3;
        // R7.eggs: an UNCAPPED ballistic term, in px per frame squared,
        // applied after the move exactly as the source's egg integrates. The
        // engine's other gravity terms clamp at 4 px/frame; the egg's drop
        // passes 7.25, so this one must not.
        float gravity = 0.0f;
        // The source's egg does not move on its first logged row (its first
        // delta is 0 on both axes), so the shot can ask for the projectile's
        // existing spawn-tick hold.
        bool holdFirstFrame = false;
        // 0 keeps the projectile's own default; a measured object says how
        // many frames the source logs it for.
        int lifetime = 0;
        // Optional measured art, the same shape Enemy::PendingShot carries.
        std::string sprite;
        float w = 0.0f, h = 0.0f;
    };
    std::vector<PendingShot> pendingShots;

    // R7.eggs: enemies the boss's own program spawns (Storm Eagle's four
    // eaglets). Drained by the scene the way pendingShots is.
    struct PendingEnemy {
        std::string id;
        float x = 0.0f, y = 0.0f;
        int vxFp = 0, vyFp = 0;   // 1/256 px per frame, positive vy is DOWN
        int lifeFrames = 0;       // 0 = no measured lifetime
    };
    std::vector<PendingEnemy> pendingEnemies;

    // Boss name for intro display
    std::string displayName;

    // Activate the boss (player entered arena)
    void activate();

    // Apply damage with weakness check. Returns false when the boss ignored
    // the hit due to state/attack invulnerability.
    bool takeDamage(int amount, const std::string& weaponId = "buster");

    // ------------------------------------------------------------------
    // U35 B5 — measured Chill Penguin FSM (oracle truth; spec in
    // knowledge_base/mmx1/bosses/chill-penguin/{fsm_observations,
    // attack_kinematics,damage_matrix}.json). Active for
    // type=="chill-penguin" ONLY; other bosses keep the generic phase AI.
    // ------------------------------------------------------------------
    // None/IdleGate values are a committed engine-telemetry ABI. They are not
    // raw ROM main/substate bytes.
    enum class CpState {
        None = 0,
        IdleGate = 1,
        Slide,
        Volley,
        BarHang,
        Leap,
        Flinch
    };
    bool cpMeasured() const { return cpMeasured_; }
    CpState cpState() const { return cpState_; }
    int cpVelFp() const { return cpVelFp_; }
    void cpForceStateForTest(CpState s) { cpEnter(s); }

    // ------------------------------------------------------------------
    // SE-B5 E6: measured Storm Eagle fight loop. SE-B2 decoded the live
    // fight as a FIXED loop over 7/7 live-player windows (only durations
    // modulate), so unlike CP there is no selector here - the order IS the
    // law. The step table lives in boss_se_fight_loop.h; the index into it
    // is exposed rather than a duplicate enum, so the generated table stays
    // the single source of truth. Active for type=="storm-eagle" ONLY.
    // ------------------------------------------------------------------
    bool seMeasured() const { return seMeasured_; }
    int seStepIndex() const { return seStepIndex_; }

    // R5, Storm Eagle's measured hover flap wind
    // (knowledge_base/mmx1/bosses/storm-eagle/attack_kinematics.json
    // `hoverWind`): while the boss hovers, a continuous wind pushes GROUNDED X
    // away from the boss's x column at `pushFpPerFrame` 512 = 2 px per frame.
    // The source gate is the boss slot's +0x03 byte, whose ~45-frame pauses
    // every ~500 frames the KB records only approximately; the engine gates on
    // the fight-loop state instead, where `gustStateWindless` is measured.
    static constexpr float kSeHoverWindPushPx = 2.0f;
    float seHoverWindPushPx() const;
    void setSeStepIndexForTest(int step) { seStepIndex_ = step; }
    void setSeTimerForTest(int frames) { seTimer_ = frames; }
    int seTimer() const { return seTimer_; }
    // Injected-RNG seam (rollDropType precedent): roll(n) -> [0, n).
    std::function<int(int)> cpRoll = [](int n) { return std::rand() % n; };

private:
    // Round25: source C019 selector for the five witnessed rows, including
    // the Flinch exit's source row 0. The caller supplies oldSub (0, 2, 4, 6,
    // or 8).
    void cpSelectSource(int sourceSub);

    Player* target_ = nullptr;
    AnimationPlayer anim_;

    // State timers
    int stateTimer_ = 0;       // Frames in current state
    int introTimer_ = 0;       // Intro sequence timer
    int rescueTimer_ = 0;      // Rescue sequence timer (Vile)
    int deathTimer_ = 0;       // Death sequence timer
    int deathExplosions_ = 0;  // Count of explosions spawned

    struct DeathBurst {
        float x, y;
        float radius;
        float maxRadius;
        int timer;
        int lifetime;
    };
    std::vector<DeathBurst> deathBursts_;
    bool deathWhiteFlash_ = false;

    // Current attack (stored by value to avoid dangling pointer on phase transition)
    BossAttack currentAttack_;
    bool hasCurrentAttack_ = false;
    int attackTimer_ = 0;       // Frames into current attack phase
    bool attackFired_ = false;  // Has the active phase fired its projectiles

    // Boss-specific behavior
    bool isScripted_ = false;   // Vile = true (can't be killed normally)
    float arenaLeft_ = 0;       // Arena bounds for movement
    float arenaRight_ = 0;

    // U35 B5: measured CP FSM state (see public CpState block).
    bool cpMeasured_ = false;
    CpState cpState_ = CpState::None;
    int cpTimer_ = 0;          // frames in the current CP state
    int cpIdleLen_ = 0;        // current idle duration target
    bool cpFirstIdle_ = true;
    bool cpIdleShotDone_ = false;
    int cpVelFp_ = 0;          // slide velocity, 1/256 px units (signed)
    int cpXFp_ = 0;            // boss x in fp during the slide (exact law)
    // CP BarHang source x ghost: independent from the slide fixed-point
    // cursor so entering/re-entering BarHang cannot inherit slide state.
    int cpHangVelFp_ = 0;
    int cpHangXFp_ = 0;
    int cpHangTargetFp_ = 0;
    // B6.round19: source entry Y phase for the 190-row BarHang curve.
    int cpHangEntryYFp_ = 0;
    float cpFloorY_ = 0;       // fight-floor y captured at activation
    float cpWallLeft_ = 0;     // slide reflect bounds (spawn-relative)
    float cpWallRight_ = 0;
    int cpBarCenterFp_ = 0;    // spawn-relative bar center (left wall + 98 px)
    // Round20/22 short Leap X: source fixed-point cursor and the signed
    // 16-bit velocity assigned on the first update. The historical 172-row
    // windows remain source evidence in death_scheduler.json; live CP uses
    // the measured 142-row path.
    int cpLeapShortXFp_ = 0;
    int cpLeapShortVxFp_ = 0;
    bool cpLeapShortTargetSampled_ = false;
    // Short Leap source Y cursor: measured entry phase in 8.8 fixed point.
    // The relative 142-row curve is generated in boss_cp_leap_short_y.h.
    int cpLeapEntryYFp_ = 0;
    // SE-B5 E6: measured SE fight loop (see public accessors above).
    bool seMeasured_ = false;
    // C2: the measured SE boss-room intro (boss_se_intro_timeline.h).
    int seIntroDisplayHealth_ = 0;
    bool seIntroVisible_ = false;
    int seStepIndex_ = 0;      // index into boss_se_fight_loop::kLoop
    // R7.eggs: where this gust's egg left the boss, so the hatch lands at the
    // measured offset from it rather than from a boss that has moved since.
    float seEggSpawnX_ = 0.0f;
    float seEggSpawnY_ = 0.0f;
    int seHatchIn_ = 0;        // frames until the egg hatches, 0 = no egg
    int seTimer_ = 0;          // frames spent in the current step
    // SE-B5 E7a: measured hold motion. y in 1/256 px (the CP slide law's
    // fixed point) so the per-frame dy series replays exactly.
    int seYFp_ = 0;
    int seXFp_ = 0;            // E7b: the Chain steps also move and teleport in x
    bool seYFpInit_ = false;
    // SE-B5 E1: death choreography is a per-boss table (null = generic
    // engine death pipeline). Selected in boss_loading.cpp by type.
    const BossDeathTimeline* deathTimeline_ = nullptr;
    // SE-B5 E3: measured damage response. fight_timeline.json records 23
    // nonlethal hits with stateChangingEvents 0, flinchStateObserved
    // false and subStatesEverEntered [0,2,4] - every hit is absorbed in
    // place. Such a boss skips the generic weakness stun AND the phase
    // ladder (whose entries are engine_extraction inventions).
    bool absorbsDamageInPlace_ = false;
    int deathPopIndex_ = 0;    // measured pop schedule cursor
    bool cpStageIntroConfigured_ = false;
    float cpIntroCeilingY_ = 0;
    int cpDisplayHealth_ = 0;
    bool cpHudVisible_ = false;
    int cpRenderFrame() const;
    void updateCpFsm();
    void updateSeFightLoop();
    void cpEnter(CpState s);

    // State handlers
    void updateDormant();
    void updateIntro();
    void updateIdle();
    void updateStartup();
    void updateActive();
    void updateRecovery();
    void updateStunned();
    void updateDying();
    void updateRescued();

    // Attack execution
    void startAttack(const BossAttack& attack);
    void executeCharge();
    void executeProjectileBurst();
    void executeJumpAttack();
    void executeSlide();

    // Helpers
    float dirToTarget() const;
    float distToTarget() const;
    void checkPhaseTransition();
    void setupAnimations();
};

} // namespace mmx
