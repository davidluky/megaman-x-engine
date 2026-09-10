// weapon.h - declares weapon definitions, inventory state, and shot metadata.
// Owns: weapon IDs, ammo limits, shot types, and inventory control API.

#pragma once

#include "data/game_ids.h"
#include "raylib.h"
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

// ============================================================================
// weapon.h — Weapon definitions and inventory
//
// Each weapon has:
//   - Normal shot behavior (type, speed, damage, size)
//   - Charged shot behavior (different per weapon)
//   - Ammo cost per shot
//   - Max ammo
//   - X's color when equipped (palette swap)
//
// The buster is always weapon 0 (infinite ammo, always available).
// Special weapons are acquired by defeating bosses.
//
// Weapon projectile types are handled by the projectile system.
// The weapon definition tells the player what type to spawn.
// ============================================================================

namespace mmx {

// How the weapon's projectile behaves
enum class WeaponShotType {
    Horizontal,      // Standard horizontal shot (buster, Shotgun Ice)
    ArcShatter,      // Shotgun Ice charged: forward arc, shatters on hit
    Spread3,         // legacy 3-way spread (pre-campaign Chameleon Sting)
    Homing,          // Homing Torpedo: tracks nearest enemy
    Rolling,         // Rolling Shield: rolls along ground with gravity
    Boomerang,       // Boomerang Cutter: arcs up then curves back
    WallSplit,       // Electric Spark: splits into 2 vertical projectiles on wall
    StingFan,        // Chameleon Sting: stationary muzzle bolt -> 3-dart fan
    PlayerState,     // no projectile — applies a player state (sting charged)
};

struct Weapon {
    std::string id;
    std::string name;
    std::string bossSource;   // Which boss drops this weapon

    // Normal shot
    WeaponShotType normalType = WeaponShotType::Horizontal;
    float normalSpeed = 5.0f;
    int normalDamage = 1;
    float normalWidth = 8.0f;
    float normalHeight = 6.0f;

    // Oracle-measured spawn offset (KB "normal_shot.spawn_offset"): SNES
    // player anchor -> pellet RAM anchor, px, right-facing (mirror x for
    // left). Applied via the WP-C anchor bridge in Player::fireShot;
    // weapons without a measured offset keep the muzzle system.
    bool hasMeasuredSpawn = false;
    float spawnOffsetX = 0.0f;
    float spawnOffsetY = 0.0f;
    // Projectile RAM-anchor -> visual/tile-center correction, right-facing
    // (mirror x for left). Measured per weapon from OAM-vs-bullets.csv:
    // ice pellet (0,+4), e-spark ball (-3,+4).
    float projCenterCorrX = 0.0f;
    float projCenterCorrY = 0.0f;

    // Charged shot
    WeaponShotType chargedType = WeaponShotType::Horizontal;
    float chargedSpeed = 6.0f;
    int chargedDamage = 3;
    float chargedWidth = 20.0f;
    float chargedHeight = 14.0f;
    bool chargedPiercing = false;

    // ARM-upgrade L3 buster helix. Kept separate from chargedSpeed because
    // the source-backed L2 shot moves at 8 px/f while the L3 orb table uses 6 px/f.
    float armL3Speed = 6.0f;

    // Ammo
    int ammoCost = 1;         // Per normal shot
    int chargedAmmoCost = 3;  // Per charged shot
    // Full weapon energy bar. Oracle-verified 28 (NOT 32): every WRAM weapon
    // slot at $7E1F88.. reads 0xDC/0x5C = bit6 acquired + low bits 0x1C = 28
    // (shotgun-ice campaign 2026-06-09, knowledge_base/_shotgun_ice_runs/).
    int maxAmmo = 28;

    // MEASURED APU SFX command ids (KB weapon.json "sfx" blocks; U41 audit
    // 2026-06-11 + the per-weapon S8 campaigns). -1 = no sound (measured
    // silent, e.g. fire-wave tap) — AudioManager::playApu(-1) is a no-op.
    // Charged release is a PAIR: the shared 0x17 at the release frame, then
    // a weapon-specific followup ONE frame later. A sub-full-charge release
    // follows 0x17 with the weapon's NORMAL FIRE id instead (measured for
    // ALL 8 weapons, _buster_runs/subthresh_cur* 2026-06-11). Buster's
    // followup is CHARGE-LEVEL-specific (l1/l2/l3 below).
    int sfxFire = -1;                 // normal-fire command (buster/ice 0x01)
    int sfxFanRelease = -1;           // sting 0x67 at the dart-fan spawn
    int sfxShatter = -1;              // Shotgun Ice interactive shatter 0x78
    int sfxChargedRelease = -1;       // shared 0x17
    int sfxChargedFollowup = -1;      // full-charge followup (specials)
    int sfxChargedFollowupL1 = -1;    // buster blue 0x04
    int sfxChargedFollowupL2 = -1;    // buster green 0x02
    int sfxChargedFollowupL3 = -1;    // buster ARM pink 0x05
    // Rolling-shield charged hum: 0x64 first at release+47 then every 16f
    // while the shield lives (S8 s8_charged f319+).
    int sfxShieldHum = -1;
    int sfxShieldHumFirst = 0;
    int sfxShieldHumEvery = 0;
    // Fire-wave flame: held stream sends 0x61 at press+12 then every 16f
    // while held; the charged ground wave re-sends it per planted segment
    // (~10f cadence, position param) (_buster_runs/fwave_full 2026-06-11 —
    // supersedes the earlier hold-silence claim; taps stay silent).
    int sfxStreamHold = -1;
    int sfxStreamHoldFirst = 0;
    int sfxStreamHoldEvery = 0;

    // X's palette when this weapon is equipped
    Color bodyColor    = {0, 120, 215, 255};   // Main body
    Color bodyColorAlt = {0, 180, 255, 255};   // Lighter accent
    Color shotColor    = {255, 255, 100, 255};  // Projectile color
    // Weapon-energy HUD gauge fill. Collected from the Spriters "Weapons and
    // Items" sheet (content/x1/sprites/reference/); NOT yet verified vs the real
    // game. Defaults to shotColor when a weapon defines no palette.gauge.
    Color gaugeColor   = {255, 255, 100, 255};  // HUD energy-bar fill

    // Special: shotgun ice shatters into fragments on impact (terrain OR
    // enemy, even when the enemy survives — oracle s2/s6 runs 2026-06-09).
    bool shattersOnWallHit = false;
    int shatterCount = 0;

    // Oracle-measured shatter fragment table (KB "shatter.fragments").
    // vx/vy in px/frame for a RIGHT-fired parent, engine y-down convention
    // (mirror vx for left); gravity in px/frame^2 (measured 0 — straight lines).
    struct ShatterFragmentSpec { float vx = 0, vy = 0, gravity = 0; };
    std::vector<ShatterFragmentSpec> shatterFragments;

    // Cosmetic ice trail shed by the normal pellet (KB "normal_shot.trail",
    // oracle s1_air OAM + David's recall 2026-06-09): one bit every
    // trailEveryFrames at the pellet's rear, rising (trailRiseVy) then
    // falling under small trailGravity until terrain contact. Cosmetic only —
    // never collides, never damages. trailEveryFrames 0 = no trail.
    int trailEveryFrames = 0;
    float trailRiseVy = 0.0f;
    float trailGravity = 0.0f;

    // Electric Spark terrain split (KB "normal_shot.split", oracle
    // 2026-06-10): on TERRAIN death the spark spawns 2 children at its death
    // anchor moving at exactly wallSplitSpeed along +-the surface tangent
    // (vertical face = straight up/down). Children pierce all terrain, never
    // re-split, despawn off-screen. 0 = no split data.
    float wallSplitSpeed = 0.0f;
    bool splitChildrenPierceTerrain = false;
    // Children damage; -1 = inherit the parent's damage (no measurement).
    // E-spark measured 0 = full pass-through on enemy bodies (2026-06-11).
    int splitChildrenDamage = -1;

    // Boomerang Cutter (oracle 2026-06-11, _boomerang_cutter_runs): constant
    // speed through the whole flight; straight phase then SNES slope-table
    // steps (one per boomerangTurnEveryFrames; the FIRST turn tick advances
    // boomerangFirstTurnStep entries) steering bang-bang toward live X;
    // despawn on X overlap with ammo refund. Charged = FOUR giants at
    // chargedSpeed with their own straight/turn cadence, monotonic rotation
    // (no steering, no catch).
    int boomerangStraightFrames = 0;
    int boomerangTurnEveryFrames = 2;
    int boomerangFirstTurnStep = 1;
    // First-turn route is POSITION FEEDBACK (s5_route_REPORT 2026-06-10,
    // 55/56 decisions): at routeDecisionTick of flight the cutter turns DOWN
    // iff X's body center has fallen below the flight line; DOWN mode flies
    // downStraightFrames straight and enters at the 14deg entry (step 1, no
    // 2-entry skip). 0 decision tick = always UP-first (charged giants).
    int boomerangRouteDecisionTick = 0;
    int boomerangDownStraightFrames = 0;
    int boomerangDownFirstTurnStep = 1;
    bool boomerangCatchRefund = false;

    // Chameleon Sting (oracle 2026-06-11, _chameleon_sting_runs): press+1
    // spawns a STATIONARY muzzle bolt at spawnOffset; at stingFanSpawnTick of
    // its 23f life THREE darts claim already spread at the measured anchor
    // offsets (x mirrored for left) and fly at constant velocity, piercing
    // terrain, killed only at the ~36px screen margin. A new volley is
    // BLOCKED (no ammo cost) while one lives. Charged = the X invincibility
    // player-state (no projectile).
    int stingMuzzleLifetimeFrames = 0;   // 23
    int stingFanSpawnTick = 0;           // 20
    struct StingDartSpec {
        float offX = 0, offY = 0;        // claim offset vs player anchor
        float vx = 0, vy = 0;            // screen coords (KB stores screen-down)
        int cell = 0;                    // fixed sprite cell (S7)
    };
    std::vector<StingDartSpec> stingDarts;
    bool stingPierceTerrain = false;
    bool stingFireGate = false;
    int chargedInvincibilityFrames = 0;  // 480 (s3_dur)
    bool chargedBoomerangFourWay = false;
    int chargedBoomStraightFrames = 0;
    int chargedBoomTurnEveryFrames = 1;

    // Measured charged-shot spawn (WP-C anchor bridge + the charged
    // projectile's own tile-anchor; ice sled measured 2026-06-11):
    // RAM spawn = player anchor + (chargedSpawnOffset, x mirrored for left);
    // visual center = RAM anchor + a PER-FACING correction (the sled's left
    // correction is NOT the mirror of the right one — both measured).
    bool hasMeasuredChargedSpawn = false;
    float chargedSpawnOffsetX = 0.0f;
    float chargedSpawnOffsetY = 0.0f;
    float chargedCenterCorrXRight = 0.0f;
    float chargedCenterCorrXLeft = 0.0f;
    float chargedCenterCorrY = 0.0f;

    // Off-screen despawn margins, px past the screen edge (oracle-observed
    // per weapon; ice pellet 24, spark 32, charged spark 32/40).
    float normalDespawnMargin = 24.0f;
    float chargedDespawnMarginFwd = 24.0f;
    float chargedDespawnMarginBack = 24.0f;

    // Electric Spark charged twin (KB "charged_shot.twin"): release spawns a
    // forward giant that sits stationary until chargedLaunchFrame, plus a
    // backward twin whose slot appears at chargedBackwardClaimFrame; both
    // first move at chargedLaunchFrame at +-chargedSpeed.
    bool chargedTwinBackward = false;
    int chargedLaunchFrame = 0;
    int chargedBackwardClaimFrame = 0;

    // Homing Torpedo (oracle 2026-06-12, _homing_torpedo_runs/FINDINGS.md):
    // 7-frame launch (raw 288 + 32/f along facing), then per-frame steering
    // accel = the magnitude-32 direction table (quarter homingTableQRaw over
    // a 32-dir circle, 0=up clockwise) at the bearing-to-target — quantized
    // by octant + eighth-slope boundaries and FOLDED across the first
    // cardinal the heading hasn't crossed. The heading byte chases the
    // bearing 1 step/frame (it is also the sprite rotation). Per-axis
    // velocity caps homingXcapRaw indexed by the heading's circular distance
    // from the cardinal that zeroes the axis; a ccw landing ON a cardinal
    // zeroes that axis (measured asymmetry). Target = nearest LIVE enemy by
    // Chebyshev distance, no range limit. homingLaunchFrames 0 = no model.
    int homingLaunchFrames = 0;
    int homingLaunchV0Raw = 0;       // raw subpx/f (288 = 1.125 px/f)
    int homingAccelRaw = 0;          // raw subpx/f^2 (32)
    int homingStraightCapRaw = 0;    // raw vx cap with no target (1536)
    int homingTableQRaw[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    int homingXcapRaw[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    bool homingCcwZero = false;
    // Ammo half-cost: the real game spends 1 unit on every OTHER shot via a
    // bit7 half-flag before the ammo byte ($7E1F87) — shots alternate
    // cost 1, 0, 1, 0 (net 0.5/shot).
    bool ammoHalfAlternating = false;
    // Charged 5-way torpedo fan (OID 16, oracle s3_charged): five scripted
    // members, NO homing; each runs accel1 until the shared countdown ends,
    // then accel2, with per-axis velocity caps (0 = uncapped axis). Raw
    // sub-units, y SCREEN-DOWN, right-facing (mirror x for left). v0 is the
    // SNES claim-row velocity; the first move uses v0 + one accel1 tick
    // (same convention as the normal shot's 288 -> first move 320).
    bool chargedTorpedoFan = false;
    int torpedoFanCountdownFrames = 0;   // 15
    struct TorpedoFanMemberSpec {
        float offX = 0, offY = 0;        // spawn offset vs player anchor
        int v0x = 0, v0y = 0;
        int a1x = 0, a1y = 0;
        int a2x = 0, a2y = 0;
        int capX = 0, capY = 0;
        // Measured heading byte at countdown expiry (w70iso_w70repro +0x37,
        // right-facing release) — the steering start value when the homing
        // gate opens. Left release = engine mirror (32-h)%32, unmeasured.
        int headingAtExpiry = 8;
    };
    std::vector<TorpedoFanMemberSpec> torpedoFan;

    // Fire Wave stream (oracle 2026-06-11, _fire_wave_runs/FINDINGS.md):
    // HOLDING fire emits one segment every streamCadenceFrames at
    // spawnOffset; segments fly at normalSpeed, live streamSegmentLifetime
    // frames, pierce terrain AND enemies (lifetime is the only despawn), and
    // deal normalDamage per overlapped FRAME per enemy (no enemy i-frames,
    // max one decrement per frame). Ammo mirrors the $7E1F8D sub-counter:
    // the press's first segment costs 1 whole unit (buying streamSubUnits);
    // each further segment spends streamSubPerSegment sub-units.
    int streamCadenceFrames = 0;     // 2; 0 = not a stream weapon
    int streamSegmentLifetime = 0;   // 10
    int streamSubUnits = 0;          // 240
    int streamSubPerSegment = 0;     // 16

    // Fire Wave charged ground wave (KB charged_shot type "GroundWave";
    // U29 re-measure cp_wave_vram/sm_wave_wall/s6_wave 2026-06-11): the
    // release spawns a MOVING head at spawnOffset — 1 frame held, then
    // exactly waveHeadSpeed px/f (fp 384 = 1.5) ground-following (climbs
    // the CP mound slope; a tall face kills it: SM corridor end at x283;
    // the off-screen margin +32 kills it too: cp x321 = edge+32 exact —
    // the old "stationary head"/"25-seg cap" were misreads: the head IS
    // the traveling fire wall, the cap was margin geometry). It drops a
    // TERRAIN-SNAPPED stationary flame at its previous-frame anchor every
    // waveSegmentEveryFrames (wakes sit +12 then +15 apart), each living
    // waveSegmentLifetime frames (±2f jitter on slopes) and dying at
    // edge+0 (2f stubs at x291/306 vs edge 289); overlapped enemies take
    // chargedDamage every waveDamageTickFrames.
    bool chargedGroundWave = false;
    float waveHeadSpeed = 0.0f;      // 1.5 (fp 384/f, after a 1f hold)
    int waveSegmentEveryFrames = 0;  // 10
    float waveSegmentStepX = 0.0f;   // 15 (derived speed*every; doc only)
    int waveSegmentLifetime = 0;     // 40
    int waveMaxSegments = 0;         // belt cap only (margins terminate)
    int waveDamageTickFrames = 0;    // 2

    // Storm Tornado (oracle 2026-06-11, _storm_tornado_runs/FINDINGS.md):
    // the column grows WORLD-FIXED for tornadoStationaryFrames, then rushes
    // at normalSpeed until the 32px off-screen margin kills it (no fixed
    // rush duration). A new press is DRY while a column lives (fireGate).
    // Damage = normalDamage per overlapped frame (continuous, like the
    // fire-wave stream). 0 = not a tornado weapon.
    int tornadoStationaryFrames = 0;     // 65
    // Charged = TWO stationary terrain-ignoring halves at X-anchor
    // +-(0, tornadoHalfOffsetY), both living tornadoHalfLifetime frames
    // (the rising tornado is the anim; halves' own damage unmeasured —
    // engine carries the normal column's value).
    bool chargedTornadoHalves = false;
    float tornadoHalfOffsetY = 0.0f;     // 96
    int tornadoHalfLifetime = 0;         // 55

    // Chameleon Sting / Storm Tornado fire gate: a new volley is BLOCKED
    // (no ammo) while any object of the previous one lives. (Field shared;
    // named sting* for the first weapon that measured it.)

    // Rolling Shield (oracle 2026-06-11, _rolling_shield_runs/FINDINGS.md):
    // the ball pauses rollSpawnPauseFrames unmoved at the muzzle, then rolls
    // at normalSpeed under rollGravity (0.25 px/f^2), ground-following
    // slopes UP, dying at the 32px screen margin. Charged = ONE object
    // GLUED to X's anchor that absorbs exactly one hit (X takes no damage
    // while it lives), persisting until absorbed.
    int rollSpawnPauseFrames = 0;    // 7
    float rollGravity = 0.0f;        // 0.25
    // U42: a vertical wall BOUNCES the ball — exact vx flip, speed
    // preserved, position clipped flush, no count limit (sm_ball_left
    // visible-wall clip x17 -> +4.0; KB normal_shot.wall_bounce).
    bool rollWallBounce = false;
    bool chargedAbsorbShield = false;

    // Charged Shotgun Ice sled (KB "charged_sled"): spawns stationary
    // (formation), launches at sledLaunchFrame frames after spawn, then
    // accelerates by sledAccel px/f^2 up to sledMaxSpeed; ground-follows;
    // X is scooped onto the nose and carried at exact sled velocity.
    bool chargedRideable = false;
    bool chargedGroundFollow = false;
    int sledLaunchFrame = 0;
    float sledAccel = 0.0f;
    float sledMaxSpeed = 0.0f;
};

struct WeaponInventoryState {
    std::vector<Weapon> weapons;   // Index 0 = buster (always present)
    std::vector<int> ammo;         // Current ammo per weapon (index matches weapons)
    std::vector<uint8_t> halfFlag; // per-weapon half-unit ammo flag
};

// Player's weapon inventory
struct WeaponInventory {
    int currentIndex = 0;          // Which weapon is selected

    WeaponInventory();
    explicit WeaponInventory(WeaponInventoryState& state);
    void bindState(WeaponInventoryState& state);
    WeaponInventoryState& state() { return *state_; }
    const WeaponInventoryState& state() const { return *state_; }

    void init();                   // Set up buster as weapon 0
    void reset();                  // Clear everything (for new game)
    bool addWeapon(const Weapon& w);
    void cycleNext();
    void cyclePrev();

    bool empty() const;
    size_t weaponCount() const;
    const Weapon& weaponAt(size_t index) const;
    int ammoAt(size_t index) const;
    bool setAmmo(size_t index, int value);
    const Weapon& current() const;
    int currentAmmo() const;
    bool useAmmo(int amount);      // Returns false if not enough ammo
    void refillAmmo(int weaponIdx, int amount);
    bool isBuster() const { return currentIndex == 0; }
    // Homing-torpedo half-unit ammo (the $7E1F87 bit7 flag): shots
    // alternate cost 1, 0, 1, 0 — the flag marks the next shot FREE.
    bool halfFlagSet() const;
    void toggleHalfFlag();

private:
    static WeaponInventoryState& fallbackState();

    WeaponInventoryState* state_ = nullptr;
};

// Pre-defined weapons
namespace weapons {
    // KB-driven loading (returns nullopt if file missing or invalid)
    std::optional<Weapon> loadFromFile(const std::string& path);

    std::optional<Weapon> makeById(WeaponId weaponId);

    // Look up which weapon a boss drops. Returns nullopt for bosses
    // that don't award weapons (e.g. Vile, Sigma).
    std::optional<Weapon> awardForBoss(BossId bossId);
    std::optional<Weapon> awardForBoss(const std::string& bossType);
}

} // namespace mmx
