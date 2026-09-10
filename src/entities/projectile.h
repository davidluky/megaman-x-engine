// projectile.h - declares projectile types, visuals, and collision shape.
// Owns: projectile movement fields, owner tags, and visual style contracts.

#pragma once

#include "entity.h"
#include "raylib.h"
#include <string>
#include <vector>

// ============================================================================
// projectile.h — Projectile entity (bullets, charged shots)
//
// Projectiles are simple entities: they move in a straight line, check for
// collision with the tilemap (destroy on hit) and enemies (deal damage),
// and have a limited lifetime to prevent stray bullets from existing forever.
//
// The player can have at most MAX_PLAYER_SHOTS on screen simultaneously.
// Each shot has a cooldown (SHOT_COOLDOWN frames) before the next can fire.
//
// Charge levels:
//   0 = normal buster shot (small, 1 damage)
//   1 = charge level 1 (medium, 2 damage)
//   2 = charge level 2 (large, 3 damage, piercing)
// ============================================================================

namespace mmx {

constexpr int MAX_PLAYER_SHOTS = 3;
// Real MMX has no per-shot cooldown — fire rate is limited only by the 3-on-
// screen cap. Tapping fires as fast as the player can press.
constexpr int SHOT_COOLDOWN = 0;

enum class ProjectileType {
    Normal,     // Small lemon shot
    ChargeL1,   // Medium charged shot
    ChargeL2,   // Full charge — large, piercing
    ChargeL3    // Buster upgrade — spiral shot, huge damage
};

enum class ProjectileVisualStyle {
    BusterSprite,
    BusterCharge1,
    EnemyOrb,
    ShotgunIce,
    StormTornado,
    FireWave,
    ElectricSpark,
    RollingShield,
    HomingTorpedo,
    BoomerangCutter,
    ChameleonSting,
};

class Projectile : public Entity {
public:
    void init(float x, float y, float vx, float vy, ProjectileType type);
    void applyWeaponVisual(const std::string& weaponId, bool charged);
    void applyEnemyVisual();
    bool usesOneHitPerTargetGate() const;
    bool shouldConsumeAfterEnemyHit(bool killedEnemy) const;
    int damageToBoss() const;
    bool hasHitEnemySerial(int enemySerial) const;
    void markHitEnemySerial(int enemySerial);

    void update(float dt) override;
    void render(float alpha) override;
    void render(float alpha, Vector2 cameraOffset);

    ProjectileType type = ProjectileType::Normal;
    int serial = 0;          // Monotonic spawn id (assigned in init; trace/tests)
    void assignFreshSerial();  // for copies (e-spark twin) — see init()
    float speed = 5.0f;      // Base speed (used for player shots facing direction)
    float vx = 0, vy = 0;    // Actual velocity components (used for movement)
    int damage = 1;
    int lifetime = 180;      // Frames before auto-despawn (3 seconds)
    bool piercing = false;   // Passes through enemies (charge L2)
    int ageFrames = 0;
    // Non-continuous piercing shots (ARM L3 orbs, etc.) can pass through a
    // target, but a single projectile should not keep damaging the same
    // overlapped target every frame. Each orb keeps its own hit memory, so
    // the three L3 orbs may still all connect independently.
    bool hitBossOnce = false;
    std::vector<int> hitEnemySerials;
    // SNES bullets exist for one frame at the unmoved spawn position before
    // their mover runs (s1_air bullets.csv); set by measured-spawn weapons
    // (WP-C) to skip the spawn tick's movement step.
    bool holdFirstTick = false;
    // Fully inert + undrawn + untraced for N ticks (Electric Spark backward
    // charged twin: its bullet slot is claimed 10 frames after release).
    int dormantFrames = 0;
    // Stationary "formation" window: no movement until ageFrames exceeds
    // this (Electric Spark charged giants: both launch at release+12).
    int launchDelayFrames = 0;
    // Pierces ALL terrain (Electric Spark split children — oracle: the
    // down-slope child crossed the floor and died off-screen).
    bool ignoresTerrain = false;
    // Off-screen despawn margin, px past the screen edge (oracle-measured
    // per weapon: ice pellet 24, spark 32, charged spark 32/40).
    float despawnMarginPx = 24.0f;
    // Electric Spark terrain split (copied from Weapon at fire time).
    float splitSpeed = 0.0f;
    bool splitPierce = false;
    int splitChildrenDamage = -1;  // -1 = inherit; e-spark measured 0 (pass-through)

    // Owner tag to prevent self-damage
    bool isPlayerShot = true;

    // Weapon system
    std::string weaponId = "buster";
    Color color = {255, 255, 100, 255};  // Visual color (overridden by weapon)

    // Rendering profile. Sprite animation is intentionally independent from
    // collision so the original 16x16/charge-shot art can overhang tight MMX
    // gameplay boxes.
    ProjectileVisualStyle visualStyle = ProjectileVisualStyle::BusterSprite;
    std::string visualSpritePath;
    int visualFrameWidth = 0;
    int visualFrameStart = 0;   // first sheet column of the looping (travel) frames
    int visualFrameHeight = 0;
    int visualFrameCount = 0;
    int visualFrameTicks = 4;
    float visualScale = 1.0f;
    // Some source packets are already authored in their only gameplay
    // orientation (Axe Max's leftward log). Keep those pixels intact instead
    // of applying the generic velocity-facing mirror.
    bool visualMirrorsWithFacing = true;
    // Homing Torpedo S7: the real body re-uploads one bitmap per rotation
    // state — the strip bakes all 32 headings (flips included), so the
    // renderer indexes the sheet column by homingHeading directly (no
    // facing flip, no time animation).
    bool visualHeadingIndexed = false;
    // Fixed sheet column (>=0): charged torpedo fan members each show one
    // measured fish cell; the facing flip still mirrors left-side fire.
    int visualFixedFrame = -1;
    // Intro-then-loop (U29 fire-wave head: spr 129 x4f, 130 x4f, then 147
    // forever): the first N cells play ONCE at visualFrameTicks each, then
    // the remaining (count-N) cells loop. 0 = plain modulo loop.
    int visualIntroCells = 0;
    // U30 storm-tornado: the real object is drawn only on EVEN ageFrames
    // (the SNES alternate-frame flicker = 30Hz translucency). Render-only.
    bool flickerAlternate = false;
    // Charged Storm Tornado is a full-height periodic column. Extra vertical
    // copies prevent camera movement from exposing the finite 224px cell ends.
    int visualTileYCopies = 0;
    // U19 bat mine: falls under projGravity, lands and STOPS, persists as
    // a ground hazard (enemy-shot damage path).
    bool mineHazard = false;
    // Anchor-relative draw correction (px) added to the centered dest: strip
    // cells bake as-fired-right art at their build-time anchor convention;
    // when a later OAM bridge measures a different split (charged e-spark
    // column 70px-above/24px-below, sm_setup_REPORT 2026-06-10), the
    // correction lands here. X mirrors with the source flip.
    float visualOffsetX = 0.0f;
    float visualOffsetY = 0.0f;

    // Shotgun Ice: shatters into fragments on ANY impact (terrain or enemy,
    // even when the enemy survives) — oracle campaign 2026-06-09.
    bool shattersOnWallHit = false;
    int shatterCount = 0;
    // Oracle-measured fragment specs copied from the Weapon at spawn time
    // (right-fired convention; spawnShatterFragments mirrors vx for left).
    struct ShatterSpec { float vx = 0, vy = 0, gravity = 0; };
    std::vector<ShatterSpec> shatterSpecs;

    // True on shatter-fragment children: per-enemy damage tables treat
    // fragments as the pellet's table entry unless explicitly overridden
    // (same projectile OID in the real game — enemy_damage.h).
    bool isShatterFragment = false;

    // Cosmetic ice trail (Shotgun Ice pellet; weapon.h trail* docs). The
    // scene sheds one bit every trailEveryFrames at the pellet's rear.
    int trailEveryFrames = 0;   // 0 = no trail
    float trailRiseVy = 0.0f;
    float trailGravity = 0.0f;

    // Charged Shotgun Ice sled: spawns stationary (formation), launches at
    // sledLaunchFrame frames of age, accelerates sledAccel px/f^2 toward
    // facing up to sledMaxSpeed, ground-follows; X is scooped onto the nose.
    // U219: ARM L3 buster uses a source-table helix when l3HelixVariant is
    // set. Sine fields stay for legacy projectile paths.
    float sineAmplitude = 0.0f;
    int sinePeriodFrames = 0;
    int sinePhaseFrames = 0;
    float sineBaseY = 0.0f;
    int l3HelixVariant = -1;  // 0=main, 1=early upper trailer, 2=late lower trailer
    // Cosmetic handoff window: the scene may draw an oracle-composed
    // release strip while the live projectile slots already exist.
    int renderSuppressFrames = 0;

    bool rideable = false;
    bool groundFollow = false;
    // U77: the cosmetic ICE slab break-up on wall death is shotgun-ice
    // specific; the fire-wave head also groundFollows but dies silently
    // (U29 sm_wave_wall — no ice burst in the real game).
    bool sledDebrisOnWall = false;
    // Charged ice wall-break SFX command. The clean widened APU trace
    // s8_sfx_s6_ride_wide shows the real wall-break death is silent.
    int sfxSledBreak = -1;
    int sledLaunchFrame = 0;   // 0 = not a sled
    float sledAccel = 0.0f;
    float sledMaxSpeed = 0.0f;
    bool sledFacingRight = true;

    // Charged Shotgun Ice formation (S7): chunk strip shown while
    // ageFrames < formationFrames (phase = ageFrames/10, 5 cells of 48x16),
    // then the static body. Empty = no formation phase.
    std::string formationSpritePath;
    int formationFrames = 0;

    // Charged Shotgun Ice moving sled: body + snow spray are rendered as one
    // OAM-priority composite once the sled launches. Before launch, the
    // static body/formation paths above stay in effect.
    std::string sledMovingSpritePath;
    int sledMovingFrameWidth = 0;
    int sledMovingFrameHeight = 0;
    int sledMovingFrameCount = 0;

    // ChargeL3 spiral animation state
    int spiralTimer = 0;

    // Homing Torpedo (oracle 2026-06-12, _homing_torpedo_runs/FINDINGS.md):
    // the scene feeds the live target center into returnX/returnY and
    // homingHasTarget; update() runs the launch countdown, bearing
    // quantization (octant + eighth-slope), the fold across un-crossed
    // cardinals, the magnitude-32 table accel, per-axis caps by heading
    // distance, and the ccw cardinal-landing zero. SNES order is
    // accel -> move -> cap, so the block caps the PREVIOUS frame's
    // velocity first (moves use pre-cap values — the capped fan member
    // measurably moves at (cap+accel)/256 steadily).
    bool homing = false;
    int homingCountdown = 0;     // straight accel block-ticks before steering
    int homingHeading = 8;       // 32-dir byte convention (0=up, clockwise)
    bool homingHasTarget = false;
    int homingAccelRaw = 32;
    int homingStraightCapRaw = 1536;
    int homingTableQRaw[9] = {32, 31, 28, 25, 22, 19, 14, 7, 0};
    int homingXcapRaw[9] = {372, 372, 684, 921, 1083, 1227, 1371, 1488, 1536};
    bool homingCcwZero = true;
    // Homing Torpedo smoke wake (S7): body center recorded at the previous
    // puff tick — the next puff spawns THERE (laid down in the wake; the
    // scene owns the cadence and initializes this on first sight, while
    // holdFirstTick keeps the spawn-tick center exact).
    float puffWakeX = 0, puffWakeY = 0;
    // Charged torpedo fan member (OID 16, no homing): accel1 until the
    // shared countdown ends, then accel2; per-axis caps (0 = uncapped),
    // same accel -> move -> cap order.
    bool torpedoFanMember = false;
    int fanCountdown = 0;
    int fanA1x = 0, fanA1y = 0, fanA2x = 0, fanA2y = 0;
    int fanCapX = 0, fanCapY = 0;
    // Homing gate (w70iso_REPORT 2026-06-10): the member acquires the
    // nearest LIVE enemy ONCE at release (scene side, identity by enemy
    // serial / boss flag) and steers — standard homing model, measured
    // per-member start heading — iff that exact target is still alive at
    // countdown expiry. Dead/none = locked scripted flight (accel2). No
    // re-acquisition, no steering during the countdown.
    int fanHeading = 8;            // measured +0x37 at expiry (mirrored left)
    bool fanAcquired = false;      // scene set the target identity already
    bool fanTargetAlive = false;   // scene-refreshed each frame
    int fanTargetSerial = -1;      // acquired enemy serial (-1 = none)
    bool fanTargetIsBoss = false;

    // Rolling (gravity-affected ground projectile)
    bool rolling = false;
    float projGravity = 0.0f;
    // R7.eggs: an UNCAPPED ballistic term for boss-spawned projectiles, in px
    // per frame squared, applied after the move. Every other gravity path here
    // clamps the fall at 4 px/frame; Storm Eagle's egg is measured passing
    // 7.25 px/frame before it hatches (boss_se_egg_drop.h), so it needs a term
    // with no cap rather than a looser one.
    float ballisticGravity = 0.0f;
    // U42/U76: vertical walls BOUNCE the ball ONCE (exact vx flip, clipped
    // flush); the SECOND wall contact destroys it (u76 ball_left_long +
    // ball_right_long: both orders, both wall types — first contact reverses
    // at f67, slot freed at the second at f131). The old "no count limit"
    // claim never sampled a second contact. Objects/enemies still kill.
    bool bouncesOffWalls = false;
    int wallBouncesLeft = 0;

    // Boomerang behavior
    bool boomerang = false;
    int boomerangTimer = 0;
    int boomTurnSteps = 0;   // U46: giants curl ~30 table steps then exit straight
    float returnX = 0, returnY = 0; // LIVE player center (scene updates per frame)
    // Boomerang Cutter slope-table flight (oracle 2026-06-11): angle index
    // into the 32-entry SNES slope table, magnitude in raw subpx (1152 normal
    // / 2048 charged). straight phase, then a step every boomTurnEvery frames
    // (first turn tick advances boomFirstStep entries); steer = bang-bang
    // toward the live bearing to X; rotSense = initial/monotonic direction.
    int boomAngleIdx = 0;
    int boomMagRaw = 1152;
    int boomStraightFrames = 19;
    int boomTurnEvery = 2;
    int boomFirstStep = 2;
    bool boomSteer = true;
    int boomRotSense = -1;
    bool boomTurned = false;        // first turn tick taken
    // First-turn route decision (oracle s5_route_REPORT 2026-06-10): at
    // flight tick boomDecisionTick, turn DOWN iff X's body center (returnY,
    // scene-live) is below the cutter's flight line (its own center — still
    // in straight flight). DOWN = boomDownStraightFrames straight entering
    // at the 14deg entry (boomDownFirstStep, no skip); UP keeps the spawn
    // defaults (19f, 2-entry skip). 0 = no decision (charged giants).
    int boomDecisionTick = 0;
    int boomDownStraightFrames = 0;
    int boomDownFirstStep = 1;
    bool boomRouteDecided = false;
    bool boomCatchRefund = false;   // ammo +1 on catch (scene handles)

    // Wall split (Electric Spark)
    bool splitsOnWallHit = false;
    int splitCount = 0;

    // Chameleon Sting muzzle bolt (oracle 2026-06-11): a stationary 23f
    // entity that spawns the 3-dart fan at stingFanTick of its life
    // (scene-side, the wall-split pattern). Dart specs are copied from the
    // Weapon at fire time, MUZZLE-relative and pre-mirrored — the muzzle is
    // world-fixed, so a moving X doesn't drag the fan.
    bool stingMuzzle = false;
    int stingFanTick = 0;
    bool stingFanSpawned = false;
    int stingDartDamage = 0;
    struct StingDartSpec { float offX = 0, offY = 0, vx = 0, vy = 0; int cell = 0; };
    std::vector<StingDartSpec> stingDartSpecs;

    // Fire Wave (oracle 2026-06-11, _fire_wave_runs/FINDINGS.md): stream
    // segments pierce terrain AND enemies and deal damage CONTINUOUSLY —
    // one hit per enemy per damageTickFrames while overlapped (the scene
    // caps via the enemy hitFlash counter; no enemy i-frames measured).
    bool continuousDamage = false;
    int damageTickFrames = 1;
    // Charged GROUND WAVE head: a stationary spawner — the scene plants a
    // terrain-snapped flame segment every waveEveryFrames, the first +12px
    // from the head anchor then +15px each (measured), up to
    // waveMaxSegments; segments are anchored in place for their lifetime.
    bool waveHead = false;
    int waveEveryFrames = 0;
    float waveStepX = 0.0f;
    int waveSegLifetime = 0;
    int waveMaxSegments = 0;
    int waveSegmentsSpawned = 0;
    bool waveFacingRight = true;
    int waveSegDamage = 1;
    int waveDamageTickFrames = 2;
    // Wave chain members IGNORE the off-screen despawn rule: the oracle
    // chain kept planting/living segments 230+px past the view (s6_wave
    // x 633 with camera ~148).
    bool waveSegment = false;

    // Rolling Shield charged (oracle 2026-06-11): ONE object glued to X's
    // anchor (scene re-feeds the position every tick), absorbing exactly
    // one hit — the scene's player-damage paths consume it instead of
    // hurting X. Persists until absorbed.
    bool absorbShield = false;

    // Measured APU SFX ids carried for SCENE-side events (U41; copied from
    // the Weapon at spawn; -1 = none): the sting dart-fan release (0x67 at
    // muzzle tick 20), Shotgun Ice interactive shatter (0x78), the
    // charged-shield hum (0x64 at release+47 then every 16f), and the
    // fire-wave per-segment flame (0x61 per plant).
    int sfxFanRelease = -1;
    int sfxShatter = -1;
    int sfxShieldHum = -1;
    int sfxShieldHumFirst = 0;
    int sfxShieldHumEvery = 0;
    int sfxSegmentPlant = -1;
};

// Draw one weapon projectile through the same visual profile used by live
// gameplay. This is also used by source-backed presentation probes so a
// weapon-get demo cannot silently grow a second, screen-only projectile art
// path.
void renderProjectileVisualAt(const std::string& weaponId,
                              int ageFrames,
                              Vector2 center,
                              bool facingRight = true);

} // namespace mmx
