#pragma once

// OID53 / 87:DE93: oid_0x53_mad_pecker/attack_parent_law.json.
// Source law: state00 87:DEFF..DF30; state02 87:DF31..DFAE; state04
// 87:DFB0..DFCF; aim 84:A081..A10C + 86:BB5A + 86:EE37/EE39; animation
// selector 88 at AF:DAE8.  This has no renderer, collision, pool, or camera
// policy: the result exposes the one source allocation request to its caller.

#include <array>
#include <cstdint>

namespace mmx::mad_pecker_parent_source {

struct Animation {
    std::uint16_t pointer14 = 0;  // D+14/+15
    std::uint8_t remaining13 = 0; // D+13
    std::uint8_t art17 = 0;       // D+17

    constexpr void tick84_8EEA() {
        remaining13 = static_cast<std::uint8_t>(remaining13 - 1u); // source DEC wraps
        if (remaining13 != 0) return;
        switch (pointer14) {
        case 0xDAEE: load(0xDAF1, 9, 0x81); break;
        case 0xDAF1: load(0xDAF4, 8, 0x82); break;
        case 0xDAF4: load(0xDAF7, 8, 0x83); break;
        case 0xDAF7: load(0xDAFA, 5, 0x84); break;
        case 0xDAFA: load(0xDAFD, 6, 0x85); break;
        case 0xDAFD: load(0xDB00, 7, 0x86); break;
        case 0xDB00: load(0xDB03, 5, 0x87); break;
        case 0xDB03: load(0xDB06, 6, 0x88); break;
        case 0xDB06: load(0xDB09, 5, 0x89); break;
        case 0xDB09: load(0xDB0C, 6, 0x88); break;
        case 0xDB0C: load(0xDB0C, 6, 0x88); break; // FD FF loop
        case 0xDB11: load(0xDB11, 8, 0x80); break; // FD FF loop
        default: break; // source-valid stream pointer is a caller invariant
        }
    }

    constexpr void setAttackIndex0() { load(0xDAEE, 8, 0x80); }
    constexpr void setIdleIndex1() { load(0xDB11, 8, 0x80); }

private:
    constexpr void load(std::uint16_t pointer, std::uint8_t remaining,
                        std::uint8_t art) {
        pointer14 = pointer;
        remaining13 = remaining;
        art17 = art;
    }
};

struct Velocity { std::int16_t x; std::int16_t y; };

constexpr std::uint16_t absWord(std::uint16_t word) {
    return (word & 0x8000u) == 0 ? word : static_cast<std::uint16_t>(0u - word);
}

// Exact-width form of 84:A081. All shifts and each four-step sum are 16-bit
// source-A operations; do not replace them with unbounded host integers.
constexpr std::uint8_t quantizeAim84_A081(std::uint16_t parentX,
                                          std::uint16_t parentY,
                                          std::uint16_t playerX,
                                          std::uint16_t playerY) {
    std::uint8_t index = 0;
    const auto dx = static_cast<std::uint16_t>(playerX - parentX);
    const auto dy = static_cast<std::uint16_t>(parentY - playerY);
    std::uint16_t horizontal = absWord(dx);
    std::uint16_t vertical = absWord(dy);
    if ((dx & 0x8000u) != 0) index = static_cast<std::uint8_t>(index + 0x20u);
    if ((dy & 0x8000u) != 0) index = static_cast<std::uint8_t>(index + 0x10u);
    if (vertical < horizontal) {
        const auto saved = vertical;
        vertical = horizontal;
        horizontal = saved;
        index = static_cast<std::uint8_t>(index + 0x08u);
    }
    const auto minorTimes8 = static_cast<std::uint16_t>(horizontal << 3u);
    const auto majorStep = static_cast<std::uint16_t>(vertical << 1u);
    std::uint16_t major = vertical;
    for (std::uint8_t remaining = 4; remaining != 0 && major < minorTimes8;
         --remaining) {
        major = static_cast<std::uint16_t>(major + majorStep);
        ++index;
    }
    constexpr std::array<std::uint8_t, 64> kLookup{{
        0,1,2,3,4,0,0,0, 8,7,6,5,4,0,0,0,
        16,15,14,13,12,0,0,0, 8,9,10,11,12,0,0,0,
        0,31,30,29,28,0,0,0, 24,25,26,27,28,0,0,0,
        16,17,18,19,20,0,0,0, 24,23,22,21,20,0,0,0,
    }};
    return kLookup[index]; // 86:BB5A,X
}

constexpr std::array<Velocity, 32> kVelocity86_EE37_EE39{{
    {0x0000,0x0400},{0x00F8,0x03E0},{0x01C8,0x0392},{0x0266,0x0332},
    {0x02D2,0x02D2},{0x0332,0x0266},{0x0392,0x01C8},{0x03E0,0x00F8},
    {0x0400,0x0000},{0x03E0,static_cast<std::int16_t>(0xFF08)},
    {0x0392,static_cast<std::int16_t>(0xFE38)},{0x0332,static_cast<std::int16_t>(0xFD9A)},
    {0x02D2,static_cast<std::int16_t>(0xFD2E)},{0x0266,static_cast<std::int16_t>(0xFCCE)},
    {0x01C8,static_cast<std::int16_t>(0xFC6E)},{0x00F8,static_cast<std::int16_t>(0xFC20)},
    {0x0000,static_cast<std::int16_t>(0xFC00)},{static_cast<std::int16_t>(0xFF08),static_cast<std::int16_t>(0xFC20)},
    {static_cast<std::int16_t>(0xFE38),static_cast<std::int16_t>(0xFC6E)},{static_cast<std::int16_t>(0xFD9A),static_cast<std::int16_t>(0xFCCE)},
    {static_cast<std::int16_t>(0xFD2E),static_cast<std::int16_t>(0xFD2E)},{static_cast<std::int16_t>(0xFCCE),static_cast<std::int16_t>(0xFD9A)},
    {static_cast<std::int16_t>(0xFC6E),static_cast<std::int16_t>(0xFE38)},{static_cast<std::int16_t>(0xFC20),static_cast<std::int16_t>(0xFF08)},
    {static_cast<std::int16_t>(0xFC00),0x0000},{static_cast<std::int16_t>(0xFC20),0x00F8},
    {static_cast<std::int16_t>(0xFC6E),0x01C8},{static_cast<std::int16_t>(0xFCCE),0x0266},
    {static_cast<std::int16_t>(0xFD2E),0x02D2},{static_cast<std::int16_t>(0xFD9A),0x0332},
    {static_cast<std::int16_t>(0xFE38),0x0392},{static_cast<std::int16_t>(0xFF08),0x03E0},
}};

struct TickResult {
    bool attemptedAim = false;   // 87:DF5C path
    bool acceptedAim = false;    // 87:DF64..DF87 result
    bool requestChild23 = false; // 87:DFCD call; allocation outcome is external
};

struct Controller {
    std::uint16_t anchorX05 = 0;
    std::uint16_t anchorY08 = 0;
    std::uint8_t state01 = 0;
    std::uint8_t timer33 = 0;
    std::uint8_t attr11 = 0;
    std::uint8_t selector35 = 0;
    std::int16_t speedX1A = 0;
    std::int16_t speedY1C = 0;
    Animation animation{};

    constexpr TickResult tick(std::uint16_t playerX, std::uint16_t playerY) {
        TickResult result{};
        if (state01 == 2) {
            if (timer33 >= 0x15) updateFacing87_9ED4(playerX);
            if (timer33 == 0x14) {
                timer33 = static_cast<std::uint8_t>(timer33 - 1u);
                selector35 = quantizeAim84_A081(anchorX05, anchorY08, playerX, playerY);
                result.attemptedAim = true;
                const bool facesRight = (attr11 & 0x40u) != 0;
                result.acceptedAim = facesRight
                    ? selector35 >= 0x04 && selector35 < 0x0C
                    : selector35 >= 0x14 && selector35 < 0x1C;
                if (result.acceptedAim) {
                    speedX1A = kVelocity86_EE37_EE39[selector35].x;
                    speedY1C = kVelocity86_EE37_EE39[selector35].y;
                } else {
                    timer33 = 0x5A; // 87:DFAB
                }
            } else {
                timer33 = static_cast<std::uint8_t>(timer33 - 1u);
                if (timer33 == 0) {
                    state01 = 4;
                    timer33 = 0x30;
                    animation.setAttackIndex0(); // 87:DF4D
                }
            }
        } else if (state01 == 4) {
            timer33 = static_cast<std::uint8_t>(timer33 - 1u);
            if (timer33 == 0) {
                state01 = 2;
                timer33 = 0x5A;
                result.requestChild23 = true; // 87:DFCD, regardless of pool success
                animation.setIdleIndex1();
            } else {
                animation.tick84_8EEA();
            }
        }
        return result;
    }

private:
    constexpr void updateFacing87_9ED4(std::uint16_t playerX) {
        if (playerX >= anchorX05) attr11 = static_cast<std::uint8_t>(attr11 | 0x40u);
        else attr11 = static_cast<std::uint8_t>(attr11 & 0xBFu);
    }
};

// 82:827D mainEnemySpriteLoad's 86:A688/A689 selector result is runtime data:
// `88,81` selects sprite record 88 and indexes 7F:8200/8300[$81].  These two
// bytes are inputs, not portable capture literals.
struct RuntimeSpriteLoad {
    std::uint8_t graphics18 = 0; // 7F:8200[$81]
    std::uint8_t attr11 = 0;     // 7F:8300[$81]
};

struct FreshState00 {
    Controller controller{};
    std::uint8_t spriteSelector16 = 0x88;
    std::uint8_t graphics18 = 0;
    std::uint8_t attr34 = 0;
    std::uint8_t health27 = 8;
    std::uint8_t field28 = 1;
    std::uint8_t damage26 = 3;
    std::uint8_t field12 = 4;
    std::uint16_t descriptor20 = 0xD42F;
    std::int16_t weight1E = 0;
    std::uint8_t field30 = 0;
    std::uint8_t animationFlags0F = 0x80;
};

// State00 87:DEFF. `residue35` is explicit because neither 82:827D nor
// 87:DEFF writes D+35; 87:DF5C overwrites it at the first aim attempt.
constexpr FreshState00 initializeState00_87_DEFF(
    std::uint16_t parentX, std::uint16_t parentY, std::uint16_t playerX,
    RuntimeSpriteLoad sprite, std::uint8_t residue35 = 0) {
    FreshState00 out{};
    out.controller.anchorX05 = parentX;
    out.controller.anchorY08 = parentY;
    out.controller.state01 = 2; // 82:827F
    out.controller.timer33 = 0x14; // 87:DF26
    out.controller.attr11 = sprite.attr11;
    if (playerX >= parentX) out.controller.attr11 |= 0x40u;
    else out.controller.attr11 &= 0xBFu;
    out.controller.selector35 = residue35;
    out.controller.animation.setIdleIndex1(); // 87:DF2A -> AF:DB11
    out.graphics18 = sprite.graphics18;
    out.attr34 = static_cast<std::uint8_t>(sprite.attr11 & 0x0Eu); // 87:DF03
    return out;
}

} // namespace mmx::mad_pecker_parent_source
