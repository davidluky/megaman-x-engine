#pragma once
// OID 02/subtype 05: oid_0x50_pending/attack_effect_observation.json.
// 84:A4B5 allocates before 80:D287's same-frame FX pass; 81:EE97 runs it.
#include <array>
#include <cstdint>

namespace mmx::deck_turret_effect {
struct Effect {
    bool active = false;
    bool visible = false;
    int age = 0;
    std::uint16_t x = 0;
    std::uint16_t y = 0;

    int cell() const {
        if (!active || !visible) return -1;
        if (age == 1 || (age >= 4 && age <= 6)) return 0;
        if (age <= 3) return 1;
        if (age <= 9) return 2;
        if (age <= 12) return 3;
        if (age <= 15) return 4;
        if (age <= 18) return 5;
        if (age <= 20) return 6;
        if (age <= 22) return 7;
        return -1;
    }
};

// 82:82D3: 31 records, $1928..<1D08 at stride $20. This adapter shares
// capacity across turret effects; other source FX families are still separate.
using Pool = std::array<Effect,31>;
inline bool allocate(Pool& pool, std::uint16_t x, std::uint16_t y) {
    for (auto& effect : pool) if (!effect.active) {
        effect = {true,false,0,x,y};
        return true;
    }
    return false;
}

inline void advance(Pool& pool, std::uint16_t cameraX, std::uint16_t cameraY) {
    for (auto& effect : pool) if (effect.active) {
        const bool visible = static_cast<std::uint16_t>(effect.x-cameraX+32) < 320 &&
            static_cast<std::uint16_t>(effect.y-cameraY+16) < 256; // 82:80B4
        effect.visible = visible;
        // Init runs setSpriteAnim and camera only; a nonvisible birth is
        // cleared by the following live pass, not during initialization.
        if (effect.age == 0) { effect.age = 1; continue; }
        ++effect.age;
        // AF:A875 has 02 00 0D followed by 02 80 0D. Entering the second
        // token immediately triggers 81:EEC0's BMI clear at age23/birth+22.
        if (!visible || effect.age >= 23) {
            effect.active = false;
            effect.visible = false;
        }
    }
}
} // namespace mmx::deck_turret_effect
