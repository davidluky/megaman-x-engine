#pragma once

// OID04 child OID03 law: head_launch.json and head_projectile_aim.json.
// Source: 81:C20A/81:AB2E, 81:AB3A/AB68/ABEC/AC01, 82:806E,
// 84:A081, 86:BB5A/C072/EEB7. Collision resolution is supplied by the scene after motion.
#include <array>
#include <cstdint>

namespace mmx::stretch_bird_shot {

constexpr std::uint32_t kFpMask = 0x00FFFFFFu;
constexpr std::uint32_t fp(std::uint16_t integer, std::uint8_t fraction) {
    return (static_cast<std::uint32_t>(integer) << 8) | fraction;
}
constexpr std::uint16_t word(std::uint32_t value) { return static_cast<std::uint16_t>((value >> 8) & 0xFFFFu); }
constexpr std::uint8_t fraction(std::uint32_t value) { return static_cast<std::uint8_t>(value & 0xFFu); }
constexpr std::uint32_t addFixed(std::uint32_t value, std::int16_t delta) {
    return static_cast<std::uint32_t>((static_cast<std::int64_t>(value) + delta) & kFpMask);
}
constexpr std::uint16_t absWord(std::uint16_t left, std::uint16_t right) {
    const auto delta = static_cast<std::uint16_t>(left - right);
    return (delta & 0x8000u) ? static_cast<std::uint16_t>(0u - delta) : delta;
}

struct Aim { std::uint8_t selector; std::int16_t vx, vy; };
inline constexpr std::array<std::uint8_t, 64> kBucketToAlpha{{0, 1, 2, 3, 4, 0, 0, 0, 8, 7, 6, 5, 4, 0, 0, 0, 16, 15, 14, 13, 12, 0, 0, 0, 8, 9, 10, 11, 12, 0, 0, 0, 0, 31, 30, 29, 28, 0, 0, 0, 24, 25, 26, 27, 28, 0, 0, 0, 16, 17, 18, 19, 20, 0, 0, 0, 24, 23, 22, 21, 20, 0, 0, 0}};
inline constexpr std::array<std::uint8_t, 64> kAlphaToSelector{{26, 26, 26, 26, 26, 26, 26, 26, 26, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 23, 24, 25, 26, 26, 26, 26, 26, 26, 6, 6, 6, 6, 6, 6, 6, 7, 8, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10}};
inline constexpr std::array<Aim, 32> kVelocity{{
    {0, 0, 512},    {1, 99, 501},    {2, 195, 472},    {3, 284, 425},    {4, 361, 361},    {5, 425, 284},    {6, 472, 195},    {7, 501, 99},    {8, 512, 0},    {9, 501, -99},    {10, 472, -195},    {11, 425, -284},    {12, 361, -361},    {13, 284, -425},    {14, 195, -472},    {15, 99, -501},    {16, 0, -512},    {17, -99, -501},    {18, -195, -472},    {19, -284, -425},    {20, -361, -361},    {21, -425, -284},    {22, -472, -195},    {23, -501, -99},    {24, -512, 0},    {25, -501, 99},    {26, -472, 195},    {27, -425, 284},    {28, -361, 361},    {29, -284, 425},    {30, -195, 472},    {31, -99, 501}
}};

// Exact 84:A081 bucket arithmetic, including source 16-bit wrap and child +11 bit6.
constexpr Aim aim84A081(std::uint16_t childX, std::uint16_t childY,
                        std::uint16_t playerX, std::uint16_t playerY,
                        std::uint8_t childAttr11) {
    std::uint16_t dx = static_cast<std::uint16_t>(playerX - childX);
    std::uint16_t bucket = 0;
    if (dx & 0x8000u) { dx = static_cast<std::uint16_t>(0u - dx); bucket += 0x20u; }
    std::uint16_t dy = static_cast<std::uint16_t>(childY - playerY);
    if (dy & 0x8000u) { dy = static_cast<std::uint16_t>(0u - dy); bucket += 0x10u; }
    if (dy < dx) { const auto held = dx; dx = dy; dy = held; bucket += 0x08u; }
    const auto scaledMinor = static_cast<std::uint16_t>(dx << 3u);
    const auto doubledMajor = static_cast<std::uint16_t>(dy << 1u);
    for (std::uint8_t retries = 4; retries != 0 && dy < scaledMinor; --retries) {
        dy = static_cast<std::uint16_t>(dy + doubledMajor); ++bucket;
    }
    const auto alpha = kBucketToAlpha[bucket & 0x3Fu];
    const auto index = static_cast<std::uint8_t>(alpha + ((childAttr11 & 0x40u) ? 0x20u : 0u));
    return kVelocity[kAlphaToSelector[index & 0x3Fu]];
}

enum class ResolvedCollision : std::uint8_t { None, ContactOrProjectileKill };

struct Parent {
    std::uint16_t x = 0, y = 0, directPage = 0;
    std::uint8_t attr11 = 0;
    bool verticalFacing = false; // parent +10 V: true means spawn X+16, false X-16
};
struct Environment {
    std::uint16_t playerX = 0, playerY = 0, cameraX = 0, cameraY = 0;
    // Authenticated only from an observed 81:AB99 signal_parent callback. This draft
    // does not evaluate player/projectile collision geometry or damage.
    ResolvedCollision resolvedCollision = ResolvedCollision::None;
};
struct Slot {
    bool active = false;
    std::uint8_t action = 0, subaction = 0; // D+01 / D+02: 0 birth, 2 live, 4 terminal
    std::uint8_t type0A = 0, attr11 = 0, savedAttr11 = 0, descriptorCenterX = 0, descriptorCenterY = 0;
    std::uint8_t descriptorHalfX = 0, descriptorHalfY = 0, animation = 0, hp = 0, damage = 0;
    std::uint16_t parentDirectPage = 0, parentX = 0;
    std::uint8_t source0E = 0, source2C = 0; // 82:83A3 clears these on terminal update
    std::uint32_t xFp = 0, yFp = 0; // source 16.8 stored in a 24-bit value
    std::int16_t vx = 0, vy = 0;
};
struct StepResult { bool culled = false, rangeExpired = false, signalledParent = false, cleared = false; };

class Pool {
public:
    std::array<Slot, 8> slots{}; // source allocator 82:8358: D=1428..<1628, stride 40.
    std::uint32_t parentSignalCount = 0; // parent D+35 mutations issued by this pool

    // The current receipt authenticates only slot1428: f157 allocation return has
    // D+04/D+07=80/80. Other free slots remain deliberately unseeded here.
    constexpr void seedObservedFreeSlotFractions(std::size_t index, std::uint8_t xFraction,
                                                 std::uint8_t yFraction) {
        slots[index].xFp = xFraction; slots[index].yFp = yFraction;
    }

    // 81:C20A success writes integer spawn coordinates and link only. Low fractional
    // bytes are deliberately preserved across both first use and slot reuse.
    int allocate(const Parent& parent) {
        for (std::size_t index = 0; index != slots.size(); ++index) {
            auto& slot = slots[index];
            if (slot.active) continue;
            const auto x = static_cast<std::uint16_t>(parent.x + (parent.verticalFacing ? 16 : -16));
            const auto y = static_cast<std::uint16_t>(parent.y - 16);
            // The measured post-allocation birth row has D+01/D+02=00/00. 81:C20A
            // itself does not contain that D+01 write; model it as the required birth
            // postcondition, not as a claim that 81:C20A clears retained art/state.
            slot.active = true; slot.action = 0; slot.subaction = 0; slot.type0A = 3;
            slot.attr11 = parent.attr11; slot.parentDirectPage = parent.directPage; slot.parentX = parent.x;
            slot.xFp = fp(x, fraction(slot.xFp)); slot.yFp = fp(y, fraction(slot.yFp));
            return static_cast<int>(index);
        }
        return -1;
    }

    static constexpr bool cameraContains(std::uint16_t x, std::uint16_t y,
                                         std::uint16_t cameraX, std::uint16_t cameraY) {
        return static_cast<std::uint16_t>(x - cameraX + 64u) < 384u &&
               static_cast<std::uint16_t>(y - cameraY + 64u) < 352u;
    }

    StepResult step(Slot& slot, const Environment& environment) {
        StepResult out{};
        if (!slot.active) return out;
        if (slot.action == 4) {
            // 81:AC01 -> 82:83A3 writes only D+00, D+02, D+0E and D+2C = 0.
            // D+01/action, integer/fraction positions, velocity, link, descriptor and
            // art bytes are retained source residue. Only active=false is operational.
            slot.active = false; slot.subaction = 0; slot.source0E = 0; slot.source2C = 0;
            out.cleared = true; return out;
        }
        if (slot.action == 0) { // 81:AB3A init, no aim/motion until the next update
            slot.savedAttr11 = slot.attr11; // 81:AB56: save presentation attributes at +3C
            slot.action = 2; slot.subaction = 0; slot.damage = 2; slot.hp = 4;
            slot.descriptorCenterX = 0; slot.descriptorCenterY = 0;
            slot.descriptorHalfX = 8; slot.descriptorHalfY = 8; // 86:C06E: 00 00 08 08
            slot.animation = 0;
            return out;
        }
        if (slot.action != 2) return out;
        slot.attr11 = slot.savedAttr11; // 81:AB68 restores after the prior hit flash
        if (slot.subaction == 0) {
            const auto selected = aim84A081(word(slot.xFp), word(slot.yFp),
                                             environment.playerX, environment.playerY, slot.attr11);
            slot.vx = selected.vx; slot.vy = selected.vy; slot.subaction = 2;
        }
        // 81:ABEC -> 82:820A. X adds velocity; Y subtracts source signed velocity.
        slot.xFp = addFixed(slot.xFp, slot.vx);
        slot.yFp = addFixed(slot.yFp, static_cast<std::int16_t>(-slot.vy));
        out.culled = !cameraContains(word(slot.xFp), word(slot.yFp), environment.cameraX, environment.cameraY);
        // 81:ABEC's camera-cull write falls through its animation return to 81:AB70;
        // f276 has the later range/contact/projectile callbacks. It is not a short circuit.
        if (out.culled) { slot.action = 4; slot.subaction = 0; }
        out.rangeExpired = absWord(slot.parentX, word(slot.xFp)) >= 0x120u;
        // In contrast, 81:AB87..AB8F returns immediately on the 288px parent-range path.
        if (out.rangeExpired) { slot.action = 4; slot.subaction = 0; return out; }
        // This is intentionally an external post-call outcome, never a locally inferred hit.
        // It may follow a camera cull, but cannot follow the range early return.
        if (environment.resolvedCollision == ResolvedCollision::ContactOrProjectileKill) {
            slot.action = 4; slot.subaction = 0; out.signalledParent = true;
        }
        return out;
    }

    std::array<StepResult, 8> stepAll(const Environment& environment) {
        std::array<StepResult, 8> result{};
        for (std::size_t i = 0; i != slots.size(); ++i) {
            result[i] = step(slots[i], environment);
            if (result[i].signalledParent) ++parentSignalCount; // 81:AB99 only
        }
        return result;
    }
};

} // namespace mmx::stretch_bird_shot
