// weapon.cpp - manages weapon inventory state and weapon definition loading.
// Owns: ammo bookkeeping, weapon cycling, fallback state, and JSON catalog IO.

#include "systems/weapon.h"
#include "data/kb_paths.h"
#include "data/x1_catalog.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <cstdio>

using json = nlohmann::json;

namespace mmx {

WeaponInventoryState& WeaponInventory::fallbackState() {
    static WeaponInventoryState state;
    return state;
}

WeaponInventory::WeaponInventory() : state_(&fallbackState()) {}

WeaponInventory::WeaponInventory(WeaponInventoryState& state) : state_(&state) {}

void WeaponInventory::bindState(WeaponInventoryState& state) {
    state_ = &state;
    currentIndex = 0;
}

void WeaponInventory::init() {
    state_->weapons.clear();
    state_->ammo.clear();
    state_->halfFlag.clear();
    auto buster = weapons::makeById(WeaponId::fromString("buster"));
    if (!buster) {
        std::fprintf(stderr, "Weapon: required buster KB missing or invalid\n");
        return;
    }
    state_->weapons.push_back(*buster);
    state_->ammo.push_back(0); // Buster has infinite ammo (0 = special case)
    currentIndex = 0;
}

void WeaponInventory::reset() {
    state_->weapons.clear();
    state_->ammo.clear();
    state_->halfFlag.clear();
    currentIndex = 0;
}

bool WeaponInventory::addWeapon(const Weapon& w) {
    // Don't add duplicates
    for (const auto& existing : state_->weapons) {
        if (existing.id == w.id) return false;
    }
    state_->weapons.push_back(w);
    state_->ammo.push_back(w.maxAmmo); // Start fully charged
    return true;
}

void WeaponInventory::cycleNext() {
    if (state_->weapons.size() <= 1) return;
    currentIndex = (currentIndex + 1) % static_cast<int>(state_->weapons.size());
}

void WeaponInventory::cyclePrev() {
    if (state_->weapons.size() <= 1) return;
    currentIndex--;
    if (currentIndex < 0) currentIndex = static_cast<int>(state_->weapons.size()) - 1;
}

bool WeaponInventory::empty() const {
    return state_->weapons.empty();
}

size_t WeaponInventory::weaponCount() const {
    return state_->weapons.size();
}

const Weapon& WeaponInventory::weaponAt(size_t index) const {
    return state_->weapons.at(index);
}

int WeaponInventory::ammoAt(size_t index) const {
    return state_->ammo.at(index);
}

bool WeaponInventory::setAmmo(size_t index, int value) {
    if (index >= state_->ammo.size() || index >= state_->weapons.size()) return false;
    state_->ammo[index] = std::clamp(value, 0, state_->weapons[index].maxAmmo);
    return true;
}

const Weapon& WeaponInventory::current() const {
    return state_->weapons.at(static_cast<size_t>(currentIndex));
}

int WeaponInventory::currentAmmo() const {
    return state_->ammo.at(static_cast<size_t>(currentIndex));
}

bool WeaponInventory::useAmmo(int amount) {
    if (currentIndex == 0) return true; // Buster = infinite
    if (currentIndex < 0 || currentIndex >= static_cast<int>(state_->ammo.size())) return false;
    if (state_->ammo[currentIndex] < amount) return false;
    state_->ammo[currentIndex] -= amount;
    return true;
}

bool WeaponInventory::halfFlagSet() const {
    if (currentIndex < 0 || currentIndex >= static_cast<int>(state_->halfFlag.size()))
        return false;
    return state_->halfFlag[currentIndex] != 0;
}

void WeaponInventory::toggleHalfFlag() {
    if (currentIndex < 0) return;
    if (state_->halfFlag.size() < state_->weapons.size()) {
        state_->halfFlag.resize(state_->weapons.size(), 0);
    }
    if (currentIndex >= static_cast<int>(state_->halfFlag.size())) return;
    state_->halfFlag[currentIndex] = state_->halfFlag[currentIndex] ? 0 : 1;
}

void WeaponInventory::refillAmmo(int weaponIdx, int amount) {
    if (weaponIdx < 0 || weaponIdx >= static_cast<int>(state_->ammo.size()) ||
        weaponIdx >= static_cast<int>(state_->weapons.size())) {
        return;
    }
    state_->ammo[weaponIdx] =
        std::min(state_->ammo[weaponIdx] + amount, state_->weapons[weaponIdx].maxAmmo);
}

// ============================================================================
// Pre-defined weapons
// ============================================================================

namespace weapons {

namespace {

std::optional<WeaponShotType> parseShotType(const std::string& s) {
    if (s == "Horizontal")  return WeaponShotType::Horizontal;
    if (s == "ArcShatter")  return WeaponShotType::ArcShatter;
    if (s == "Spread3")     return WeaponShotType::Spread3;
    if (s == "Homing")      return WeaponShotType::Homing;
    if (s == "Rolling")     return WeaponShotType::Rolling;
    if (s == "Boomerang")   return WeaponShotType::Boomerang;
    if (s == "WallSplit")   return WeaponShotType::WallSplit;
    if (s == "StingFan")    return WeaponShotType::StingFan;
    if (s == "PlayerState") return WeaponShotType::PlayerState;
    return std::nullopt;
}

std::optional<Color> parsePaletteColor(const json& value) {
    if (!value.is_array() || value.size() != 3) return std::nullopt;

    int channels[3] = {};
    for (int i = 0; i < 3; ++i) {
        if (!value[i].is_number_integer()) return std::nullopt;
        channels[i] = value[i].get<int>();
        if (channels[i] < 0 || channels[i] > 255) return std::nullopt;
    }
    return Color{
        static_cast<unsigned char>(channels[0]),
        static_cast<unsigned char>(channels[1]),
        static_cast<unsigned char>(channels[2]),
        255
    };
}

} // namespace

std::optional<Weapon> loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return std::nullopt;

    try {
        json j;
        file >> j;

        Weapon w;
        w.id   = j.value("weapon_id", "");
        w.name = j.value("name", "");
        if (!j.contains("normal_shot") || !j["normal_shot"].is_object()) {
            std::fprintf(stderr, "Weapon: missing normal_shot object in %s\n",
                         path.c_str());
            return std::nullopt;
        }
        const bool hasChargedShot =
            j.contains("charged_shot") && j["charged_shot"].is_object();
        const bool hasChargedSled =
            j.contains("charged_sled") && j["charged_sled"].is_object();
        if (hasChargedShot == hasChargedSled) {
            std::fprintf(stderr,
                         "Weapon: expected exactly one charged_shot or charged_sled in %s\n",
                         path.c_str());
            return std::nullopt;
        }
        if (!j.contains("ammo") || !j["ammo"].is_object()) {
            std::fprintf(stderr, "Weapon: missing ammo object in %s\n", path.c_str());
            return std::nullopt;
        }
        if (j.contains("boss_source") && j["boss_source"].is_string())
            w.bossSource = j["boss_source"].get<std::string>();

        if (j.contains("normal_shot")) {
            const auto& n = j["normal_shot"];
            const std::string normalTypeName = n.value("type", "");
            const auto normalType = parseShotType(normalTypeName);
            if (!normalType) {
                std::fprintf(stderr,
                             "Weapon: unknown normal shot type '%s' in %s\n",
                             normalTypeName.c_str(), path.c_str());
                return std::nullopt;
            }
            w.normalType = *normalType;
            w.normalSpeed  = n.value("speed", 5.0f);
            w.normalDamage = n.value("damage", 1);
            w.normalWidth  = n.value("width", 8.0f);
            w.normalHeight = n.value("height", 6.0f);
            if (n.contains("spawn_offset")) {
                w.hasMeasuredSpawn = true;
                w.spawnOffsetX = n["spawn_offset"].value("x", 0.0f);
                w.spawnOffsetY = n["spawn_offset"].value("y", 0.0f);
            }
            if (n.contains("projectile_center_correction")
                && n["projectile_center_correction"].is_array()
                && n["projectile_center_correction"].size() >= 2) {
                w.projCenterCorrX = n["projectile_center_correction"][0].get<float>();
                w.projCenterCorrY = n["projectile_center_correction"][1].get<float>();
            }
            w.normalDespawnMargin = n.value("despawn_margin_px", 24.0f);
            if (n.contains("split")) {
                const auto& s = n["split"];
                w.wallSplitSpeed = s.value("speed", 0.0f);
                w.splitChildrenPierceTerrain = s.value("children_pierce_terrain", false);
                w.splitChildrenDamage = s.value("children_damage", -1);
            }
            if (n.contains("boomerang")) {
                const auto& bm = n["boomerang"];
                w.boomerangStraightFrames = bm.value("straight_frames", 0);
                w.boomerangTurnEveryFrames = bm.value("turn_every_frames", 2);
                w.boomerangFirstTurnStep = bm.value("first_turn_step", 1);
                w.boomerangCatchRefund = bm.value("catch_refund", false);
                if (bm.contains("first_turn_route")) {
                    const auto& rt = bm["first_turn_route"];
                    w.boomerangRouteDecisionTick = rt.value("decision_tick", 0);
                    w.boomerangDownStraightFrames = rt.value("down_straight_frames", 0);
                    w.boomerangDownFirstTurnStep = rt.value("down_first_turn_step", 1);
                }
            }
            if (n.contains("sting_fan")) {
                const auto& st = n["sting_fan"];
                w.stingMuzzleLifetimeFrames = st.value("muzzle_lifetime_frames", 0);
                w.stingFanSpawnTick = st.value("fan_spawn_tick", 0);
                w.stingPierceTerrain = st.value("pierce_terrain", false);
                w.stingFireGate = st.value("fire_gate_while_volley_alive", false);
                if (st.contains("darts")) {
                    for (const auto& d : st["darts"]) {
                        Weapon::StingDartSpec s;
                        if (d.contains("claim_offset") && d["claim_offset"].size() >= 2) {
                            s.offX = d["claim_offset"][0].get<float>();
                            s.offY = d["claim_offset"][1].get<float>();
                        }
                        if (d.contains("v") && d["v"].size() >= 2) {
                            s.vx = d["v"][0].get<float>();
                            s.vy = d["v"][1].get<float>();
                        }
                        s.cell = d.value("cell", 0);
                        w.stingDarts.push_back(s);
                    }
                }
            }
            if (n.contains("trail")) {
                const auto& t = n["trail"];
                w.trailEveryFrames = t.value("every_frames", 0);
                w.trailRiseVy      = t.value("rise_vy", 0.0f);
                w.trailGravity     = t.value("gravity", 0.0f);
            }
            // Fire Wave stream (KB "normal_shot.stream" + lifetime_frames).
            if (n.contains("stream")) {
                const auto& st = n["stream"];
                w.streamCadenceFrames = st.value("cadence_frames", 0);
                w.streamSegmentLifetime = n.value("lifetime_frames", 0);
            }
            // Storm Tornado phase-gated motion + fire gate.
            w.tornadoStationaryFrames = n.value("stationary_frames", 0);
            if (n.value("fire_gate_while_alive", false))
                w.stingFireGate = true;
            // Rolling Shield ball physics.
            w.rollSpawnPauseFrames = n.value("spawn_pause_frames", 0);
            w.rollGravity = n.value("gravity_px_f2", 0.0f);
            if (n.contains("wall_bounce"))
                w.rollWallBounce = n["wall_bounce"].value("vx_flip", false);
        }

        if (j.contains("charged_shot")) {
            const auto& c = j["charged_shot"];
            // "GroundWave" (fire-wave) is a behavior flag on top of a
            // Horizontal projectile type, not a new shot type.
            const std::string ctype = c.value("type", "");
            // "GroundWave" (fire-wave), "TornadoHalves" (storm-tornado) and
            // "AbsorbShield" (rolling-shield) are behavior flags on top of
            // Horizontal, not new shot types.
            if (ctype == "GroundWave" || ctype == "TornadoHalves"
                || ctype == "AbsorbShield") {
                w.chargedType = WeaponShotType::Horizontal;
            } else {
                const auto chargedType = parseShotType(ctype);
                if (!chargedType) {
                    std::fprintf(stderr,
                                 "Weapon: unknown charged shot type '%s' in %s\n",
                                 ctype.c_str(), path.c_str());
                    return std::nullopt;
                }
                w.chargedType = *chargedType;
            }
            if (ctype == "GroundWave") {
                w.chargedGroundWave = true;
                w.waveHeadSpeed = c.value("head_speed", 0.0f);
                w.waveSegmentEveryFrames = c.value("segment_every_frames", 0);
                w.waveSegmentStepX = c.value("segment_step_x", 0.0f);
                w.waveSegmentLifetime = c.value("segment_lifetime_frames", 0);
                w.waveMaxSegments = c.value("max_segments_belt", 64);
                w.waveDamageTickFrames = c.value("damage_tick_frames", 0);
            }
            if (ctype == "TornadoHalves") {
                w.chargedTornadoHalves = true;
                w.tornadoHalfOffsetY = c.value("half_offset_y", 0.0f);
                w.tornadoHalfLifetime = c.value("lifetime_frames", 0);
            }
            if (ctype == "AbsorbShield") {
                w.chargedAbsorbShield = true;
            }
            w.chargedSpeed    = c.value("speed", 6.0f);
            w.chargedDamage   = c.value("damage", 3);
            w.chargedWidth    = c.value("width", 20.0f);
            w.chargedHeight   = c.value("height", 14.0f);
            w.chargedPiercing = c.value("piercing", false);
            w.chargedDespawnMarginFwd  = c.value("despawn_margin_px_forward", 24.0f);
            w.chargedDespawnMarginBack = c.value("despawn_margin_px_backward", 24.0f);
            if (c.contains("twin")) {
                const auto& t = c["twin"];
                w.chargedTwinBackward = true;
                w.chargedLaunchFrame = t.value("launch_frame", 0);
                w.chargedBackwardClaimFrame = t.value("backward_spawn_claim_frame", 0);
            }
            if (c.contains("four_way")) {
                const auto& fw = c["four_way"];
                w.chargedBoomerangFourWay = true;
                w.chargedBoomStraightFrames = fw.value("straight_frames", 0);
                w.chargedBoomTurnEveryFrames = fw.value("turn_every_frames", 1);
            }
            // Chameleon Sting charged: an X player-state, no projectile.
            w.chargedInvincibilityFrames = c.value("invincibility_frames", 0);
        }

        if (j.contains("arm_l3_shot")) {
            const auto& l3 = j["arm_l3_shot"];
            w.armL3Speed = l3.value("speed", w.armL3Speed);
        }

        if (j.contains("ammo")) {
            const auto& a = j["ammo"];
            // cost 0.5 = the real alternating half-unit scheme (torpedo:
            // $7E1F87 bit7 half-flag; shots cost 1, 0, 1, 0).
            const double cost = a.value("cost", 1.0);
            if (cost == 0.5) {
                w.ammoCost = 1;
                w.ammoHalfAlternating = true;
            } else {
                w.ammoCost = static_cast<int>(cost);
            }
            w.chargedAmmoCost = a.value("charged_cost", 3);
            w.maxAmmo         = a.value("max", 28);
            // Fire Wave stream sub-counter (the $7E1F8D model).
            w.streamSubUnits = a.value("stream_sub_units", 0);
            w.streamSubPerSegment = a.value("stream_sub_per_segment", 0);
        }

        // Root-level oracle keys (ice/spark/torpedo KB layout — same meaning
        // as the normal_shot-nested variants above).
        if (j.contains("spawn_offset")) {
            w.hasMeasuredSpawn = true;
            w.spawnOffsetX = j["spawn_offset"].value("x", 0.0f);
            w.spawnOffsetY = j["spawn_offset"].value("y", 0.0f);
        }
        if (j.contains("projectile_center_correction")
            && j["projectile_center_correction"].is_array()
            && j["projectile_center_correction"].size() >= 2) {
            w.projCenterCorrX = j["projectile_center_correction"][0].get<float>();
            w.projCenterCorrY = j["projectile_center_correction"][1].get<float>();
        }
        if (j.contains("despawn_margin_px"))
            w.normalDespawnMargin = j["despawn_margin_px"].get<float>();

        // Homing Torpedo steering model (oracle 2026-06-12).
        if (j.contains("homing")) {
            const auto& h = j["homing"];
            w.homingLaunchFrames    = h.value("launch_frames", 0);
            w.homingLaunchV0Raw     = h.value("launch_v0_raw", 0);
            w.homingAccelRaw        = h.value("accel_raw", 0);
            w.homingStraightCapRaw  = h.value("straight_cap_raw", 0);
            if (h.contains("table_q_raw") && h["table_q_raw"].is_array()
                && h["table_q_raw"].size() == 9) {
                for (int i = 0; i < 9; i++)
                    w.homingTableQRaw[i] = h["table_q_raw"][i].get<int>();
            }
            if (h.contains("xcap_raw") && h["xcap_raw"].is_array()
                && h["xcap_raw"].size() == 9) {
                for (int i = 0; i < 9; i++)
                    w.homingXcapRaw[i] = h["xcap_raw"][i].get<int>();
            }
            w.homingCcwZero = h.value("ccw_cardinal_zero", false);
        }
        if (j.contains("charged_fan")) {
            const auto& cf = j["charged_fan"];
            w.chargedTorpedoFan = true;
            w.torpedoFanCountdownFrames = cf.value("countdown_frames", 0);
            if (cf.contains("members")) {
                for (const auto& m : cf["members"]) {
                    Weapon::TorpedoFanMemberSpec s;
                    if (m.contains("spawn_offset") && m["spawn_offset"].size() >= 2) {
                        s.offX = m["spawn_offset"][0].get<float>();
                        s.offY = m["spawn_offset"][1].get<float>();
                    }
                    auto pair = [&m](const char* key, int& a, int& b) {
                        if (m.contains(key) && m[key].size() >= 2) {
                            a = m[key][0].get<int>();
                            b = m[key][1].get<int>();
                        }
                    };
                    pair("v0_raw", s.v0x, s.v0y);
                    pair("accel1_raw", s.a1x, s.a1y);
                    pair("accel2_raw", s.a2x, s.a2y);
                    s.capX = m.value("cap_x_raw", 0);
                    s.capY = m.value("cap_y_raw", 0);
                    s.headingAtExpiry = m.value("heading_at_expiry", 8);
                    w.torpedoFan.push_back(s);
                }
            }
        }

        // Measured APU SFX ids (U41 audit 2026-06-11). "fire": null means
        // MEASURED SILENT (fire-wave tap) — keep -1. The shield/tornado
        // blocks store charged_release as the [base, followup] pair array.
        if (j.contains("sfx")) {
            const auto& s = j["sfx"];
            auto asId = [](const json& v) -> int {
                return v.is_number_integer() ? v.get<int>() : -1;
            };
            if (s.contains("fire")) w.sfxFire = asId(s["fire"]);
            if (s.contains("fan_release")) w.sfxFanRelease = asId(s["fan_release"]);
            if (s.contains("shatter")) w.sfxShatter = asId(s["shatter"]);
            if (s.contains("charged_release")) {
                const auto& cr = s["charged_release"];
                if (cr.is_array() && cr.size() >= 2) {
                    w.sfxChargedRelease = asId(cr[0]);
                    w.sfxChargedFollowup = asId(cr[1]);
                } else {
                    w.sfxChargedRelease = asId(cr);
                }
            }
            if (s.contains("charged_release_followup"))
                w.sfxChargedFollowup = asId(s["charged_release_followup"]);
            if (s.contains("charged_release_followup_l1"))
                w.sfxChargedFollowupL1 = asId(s["charged_release_followup_l1"]);
            if (s.contains("charged_release_followup_l2"))
                w.sfxChargedFollowupL2 = asId(s["charged_release_followup_l2"]);
            if (s.contains("charged_release_followup_l3"))
                w.sfxChargedFollowupL3 = asId(s["charged_release_followup_l3"]);
            if (s.contains("shield_hum")) w.sfxShieldHum = asId(s["shield_hum"]);
            w.sfxShieldHumFirst = s.value("shield_hum_first_frames", 0);
            w.sfxShieldHumEvery = s.value("shield_hum_every_frames", 0);
            if (s.contains("stream_hold")) w.sfxStreamHold = asId(s["stream_hold"]);
            w.sfxStreamHoldFirst = s.value("stream_hold_first_frames", 0);
            w.sfxStreamHoldEvery = s.value("stream_hold_every_frames", 0);
        }

        if (!j.contains("palette") || !j["palette"].is_object()) {
            std::fprintf(stderr, "Weapon: missing palette object in %s\n", path.c_str());
            return std::nullopt;
        }
        {
            const auto& p = j["palette"];
            const auto body = p.contains("body")
                ? parsePaletteColor(p["body"]) : std::nullopt;
            const auto bodyAlt = p.contains("body_alt")
                ? parsePaletteColor(p["body_alt"]) : std::nullopt;
            const auto shot = p.contains("shot")
                ? parsePaletteColor(p["shot"]) : std::nullopt;
            if (!body || !bodyAlt || !shot) {
                std::fprintf(stderr, "Weapon: invalid required palette RGB in %s\n",
                             path.c_str());
                return std::nullopt;
            }
            w.bodyColor = *body;
            w.bodyColorAlt = *bodyAlt;
            w.shotColor = *shot;
            w.gaugeColor = w.shotColor;
            if (p.contains("gauge")) {
                const auto gauge = parsePaletteColor(p["gauge"]);
                if (!gauge) {
                    std::fprintf(stderr, "Weapon: invalid palette.gauge RGB in %s\n",
                                 path.c_str());
                    return std::nullopt;
                }
                w.gaugeColor = *gauge;
            }
        }

        if (j.contains("special_properties")) {
            const auto& sp = j["special_properties"];
            w.shattersOnWallHit = sp.value("shatters_on_wall_hit", false);
            w.shatterCount      = sp.value("shatter_count", 0);
        }

        // Oracle-measured schema (shotgun-ice campaign 2026-06-09).
        if (j.contains("shatter")) {
            const auto& sh = j["shatter"];
            w.shattersOnWallHit = true;
            w.shatterCount = sh.value("fragment_count", 0);
            if (sh.contains("fragments")) {
                for (const auto& fr : sh["fragments"]) {
                    Weapon::ShatterFragmentSpec s;
                    s.vx = fr.value("vx", 0.0f);
                    s.vy = fr.value("vy", 0.0f);
                    s.gravity = fr.value("gravity", 0.0f);
                    w.shatterFragments.push_back(s);
                }
            }
        }
        if (j.contains("charged_sled")) {
            const auto& cs = j["charged_sled"];
            w.chargedType        = WeaponShotType::ArcShatter;
            w.chargedSpeed       = cs.value("max_speed", w.chargedSpeed);
            w.chargedWidth       = cs.value("width", w.chargedWidth);
            w.chargedHeight      = cs.value("height", w.chargedHeight);
            w.chargedRideable    = cs.value("rideable", false);
            w.chargedGroundFollow = cs.value("ground_follow", false);
            w.sledLaunchFrame    = cs.value("launch_at_frame", 0);
            w.sledAccel          = cs.value("accel_px_per_frame2", 0.0f);
            w.sledMaxSpeed       = cs.value("max_speed", 0.0f);
            if (cs.contains("spawn_offset")
                && cs.contains("projectile_center_correction_right")
                && cs.contains("projectile_center_correction_left")) {
                w.hasMeasuredChargedSpawn = true;
                w.chargedSpawnOffsetX = cs["spawn_offset"].value("x", 0.0f);
                w.chargedSpawnOffsetY = cs["spawn_offset"].value("y", 0.0f);
                w.chargedCenterCorrXRight = cs["projectile_center_correction_right"][0].get<float>();
                w.chargedCenterCorrY      = cs["projectile_center_correction_right"][1].get<float>();
                w.chargedCenterCorrXLeft  = cs["projectile_center_correction_left"][0].get<float>();
            }
        }
        // Per-enemy damage tables are the real mechanism (oracle-proven); the
        // engine's single-value model uses the recorded approximation.
        if (j.contains("damage") && j["damage"].contains("engine_single_value_approximation")) {
            w.normalDamage = j["damage"]["engine_single_value_approximation"].get<int>();
        }

        if (w.id.empty()) return std::nullopt;

        std::fprintf(stderr, "Weapon: loaded '%s' from %s\n", w.name.c_str(), path.c_str());
        return w;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Weapon: failed to parse %s: %s\n", path.c_str(), e.what());
        return std::nullopt;
    }
}

std::optional<Weapon> makeById(WeaponId weaponId) {
    const std::string requested = weaponId.str();
    const std::string kbRelativePath = "mmx1/weapons/" + requested + "/weapon.json";
    auto kbPath = kb_paths::resolveKBPath(kbRelativePath);
    if (kbPath) {
        auto kbWeapon = loadFromFile(*kbPath);
        if (kbWeapon) return kbWeapon;
    }
    return std::nullopt;
}

std::optional<Weapon> awardForBoss(BossId bossId) {
    const auto* boss = x1_catalog::findMaverickByBoss(bossId);
    if (!boss) return std::nullopt;
    return makeById(boss->rewardWeapon);
}

std::optional<Weapon> awardForBoss(const std::string& bossType) {
    return awardForBoss(BossId::fromString(bossType));
}

} // namespace weapons
} // namespace mmx
