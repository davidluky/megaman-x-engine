// enemy_damage.cpp - parses enemy weapon damage data and resolves hit damage.
// Owns: enemy damage table import and weapon/form lookup rules.

#include "entities/enemy_damage.h"

#include <nlohmann/json.hpp>

namespace mmx::enemy_damage {

void parseInto(Enemy::Definition& def, const nlohmann::json& enemyJson) {
    if (!enemyJson.contains("damage_taken")) return;
    const auto& dt = enemyJson.at("damage_taken");
    if (!dt.is_object()) return;
    for (const auto& [weaponId, entry] : dt.items()) {
        if (!weaponId.empty() && weaponId[0] == '$') continue; // provenance
        if (!entry.is_object()) continue;
        Enemy::Definition::DamageTaken row;
        row.normal = entry.value("normal", -1);
        row.fragment = entry.value("fragment", -1);
        row.charged = entry.value("charged", -1);
        def.damageTaken[weaponId] = row;
    }
}

int damageFor(const Enemy::Definition& def, const std::string& weaponId,
              HitForm form, int fallbackDamage) {
    const auto it = def.damageTaken.find(weaponId);
    if (it == def.damageTaken.end()) return fallbackDamage;
    const auto& row = it->second;
    int v = -1;
    switch (form) {
        case HitForm::Normal:   v = row.normal; break;
        case HitForm::Fragment: v = (row.fragment >= 0) ? row.fragment : row.normal; break;
        case HitForm::Charged:  v = row.charged; break;
    }
    return (v >= 0) ? v : fallbackDamage;
}

} // namespace mmx::enemy_damage
