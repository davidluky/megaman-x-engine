// enemy_damage.h - declares enemy damage parsing and weapon lookup helpers.
// Owns: damage hit-form names and enemy definition damage entry contracts.

#pragma once

#include "entities/enemy.h"

#include <nlohmann/json_fwd.hpp>

#include <string>

// ============================================================================
// enemy_damage — per-enemy damage tables (oracle-proven real-MMX1 mechanism;
// see knowledge_base/_shotgun_ice_runs/FINDINGS.md S6).
//
// Lookup rules:
//   Normal   -> "normal" entry
//   Fragment -> "fragment" entry, else "normal" (shatter fragments share the
//               pellet's projectile OID in the real game)
//   Charged  -> "charged" entry only (a different object — no normal fallback)
//   any miss -> the projectile's own damage (engine fallback, keeps sparse
//               tables usable while oracle campaigns fill them in per game)
//
// Boss weakness multipliers (boss.cpp takeDamage) are a parallel mechanism;
// migrating bosses onto these tables is noted future work, not done here.
// ============================================================================

namespace mmx::enemy_damage {

enum class HitForm { Normal, Fragment, Charged };

// Parse the optional "damage_taken" block of one enemy_defs.json entry into
// def.damageTaken. "$"-prefixed keys are provenance and ignored.
void parseInto(Enemy::Definition& def, const nlohmann::json& enemyJson);

int damageFor(const Enemy::Definition& def, const std::string& weaponId,
              HitForm form, int fallbackDamage);

} // namespace mmx::enemy_damage
