#pragma once
// OID04 parent law: oid_0x04_tall_ledge_enemy_pending/head_launch.json.
// Source: 81:C0DA (idle), 81:C139 (launch), 81:C15A (turn), 81:C17A
// (post-child recovery); animation streams from head_launch.json table AF:A87D.
// Inputs are only player coordinates (for C551) and child increments
// delivered before this parent main call. No recorded parent state is an input.

#include <array>
#include <cstdint>

namespace mmx::stretch_bird_parent {

struct Token { std::uint8_t duration, control, art; std::uint16_t pointer; };
inline constexpr std::array<Token, 4> kAnim0{{
    {1, 0x00, 128, 0xA885},
    {1, 0x00, 130, 0xA888},
    {1, 0x00, 129, 0xA88B},
    {1, 0x80, 130, 0xA88E}
}};
inline constexpr std::array<Token, 24> kAnim1{{
    {34, 0x00, 131, 0xA893},
    {1, 0x00, 132, 0xA896},
    {1, 0x00, 133, 0xA899},
    {32, 0x00, 134, 0xA89C},
    {1, 0x00, 133, 0xA89F},
    {1, 0x00, 132, 0xA8A2},
    {32, 0x00, 131, 0xA8A5},
    {1, 0x00, 132, 0xA8A8},
    {1, 0x00, 133, 0xA8AB},
    {32, 0x00, 134, 0xA8AE},
    {1, 0x00, 133, 0xA8B1},
    {1, 0x00, 132, 0xA8B4},
    {32, 0x00, 131, 0xA8B7},
    {2, 0x00, 135, 0xA8BA},
    {4, 0x00, 136, 0xA8BD},
    {2, 0x00, 135, 0xA8C0},
    {4, 0x00, 131, 0xA8C3},
    {2, 0x00, 135, 0xA8C6},
    {4, 0x00, 136, 0xA8C9},
    {2, 0x00, 135, 0xA8CC},
    {16, 0x00, 131, 0xA8CF},
    {4, 0x10, 137, 0xA8D2},
    {32, 0x10, 138, 0xA8D5},
    {4, 0x90, 137, 0xA8D8}
}};
inline constexpr std::array<Token, 21> kAnim2{{
    {32, 0x00, 139, 0xA8DD},
    {4, 0x00, 131, 0xA8E0},
    {1, 0x42, 140, 0xA8E3},
    {3, 0x02, 140, 0xA8E6},
    {4, 0x04, 141, 0xA8E9},
    {4, 0x06, 142, 0xA8EC},
    {4, 0x08, 143, 0xA8EF},
    {4, 0x0A, 144, 0xA8F2},
    {4, 0x0C, 145, 0xA8F5},
    {4, 0x0E, 146, 0xA8F8},
    {1, 0x00, 150, 0xA8FB},
    {1, 0x00, 147, 0xA8FE},
    {1, 0x00, 150, 0xA901},
    {1, 0x00, 148, 0xA904},
    {1, 0x00, 150, 0xA907},
    {1, 0x00, 147, 0xA90A},
    {1, 0x00, 150, 0xA90D},
    {1, 0x00, 148, 0xA910},
    {1, 0x00, 150, 0xA913},
    {4, 0x00, 149, 0xA916},
    {4, 0x80, 131, 0xA919}
}};
inline constexpr std::array<Token, 4> kAnim3{{
    {1, 0x00, 132, 0xA91E},
    {1, 0x00, 133, 0xA921},
    {32, 0x00, 134, 0xA924},
    {32, 0x80, 134, 0xA927}
}};

struct Animation {
    std::uint8_t stream = 0;
    std::uint8_t token = 0;
    // Fresh construction is not the f1 capture seed. Native fields: D+13 remaining and D+14/D+15 token pointer.
    std::uint8_t remaining = 0;

    constexpr Token current() const {
        switch (stream) {
        case 0: return kAnim0[token];
        case 1: return kAnim1[token];
        case 2: return kAnim2[token];
        default: return kAnim3[token];
        }
    }

    constexpr void select(std::uint8_t nextStream) {
        stream = nextStream;
        token = 0;
        remaining = current().duration;
    }

    constexpr void tick() {
        // 84:8EEA DEC D+13 then advances only when the decremented byte is zero.
        // A malformed/explicit remaining=00 wraps to FF and does not advance.
        remaining = static_cast<std::uint8_t>(remaining - 1u);
        if (remaining != 0) return;
        switch (stream) {
        case 0: token = static_cast<std::uint8_t>((token + 1) % kAnim0.size()); break;
        case 1: token = static_cast<std::uint8_t>((token + 1) % kAnim1.size()); break;
        case 2: token = static_cast<std::uint8_t>((token + 1) % kAnim2.size()); break;
        default:
            // Stream 3 terminal next pointer is AFA927 itself, not AFA91E.
            token = token == 3 ? 3 : static_cast<std::uint8_t>(token + 1);
            break;
        }
        remaining = current().duration;
    }
};

struct State {
    // Native D+02, D+03, D+34, D+35, D+33. Fresh state reaches source setup
    // through action0/do0; the captured f1 state is available only through seedV5BeforeFrame1().
    std::uint8_t action = 0, subaction = 0, arm = 0, childSignal = 0, facing = 0;
    Animation animation{};

    // v5 before_placement f1: action=0, do=1, timer=118,
    // D+13=20, D+14/15=A893, D+0F=00, D+17=83.
    static constexpr State seedV5BeforeFrame1() { return State{0, 1, 118, 0, 0, Animation{1, 0, 32}}; }
};

enum class PlayerDescriptor : std::uint8_t { Unsupported, NormalA552, ActiveDashBB38 };

struct Input {
    std::uint16_t playerX = 0, playerY = 0;
    // 84:9C0E consumes the selected Player descriptor. These two literal source
    // profiles are authenticated; all others stay unsupported rather than borrowing
    // A552 extents. parentAttr11 supplies the source target bit6 mirror selection.
    PlayerDescriptor playerDescriptor = PlayerDescriptor::Unsupported;
    std::uint8_t parentAttr11 = 0;
    std::uint8_t childSignalIncrements = 0; // authenticated 81:AB99 signals before parent main
};

struct Output {
    std::uint8_t action, subaction, arm, control, art, childSignal, facing;
    std::uint8_t remaining;
    std::uint16_t tokenPointer;
    bool spawnChild;
};

constexpr std::uint16_t absWrapped(std::uint16_t a, std::uint16_t b) {
    const std::uint16_t d = static_cast<std::uint16_t>(a - b);
    return (d & 0x8000u) ? static_cast<std::uint16_t>(0u - d) : d;
}

// 86:C551 through 84:9C0E, exact published descriptor arithmetic for this actor.
constexpr bool c551(std::uint16_t parentX, std::uint16_t parentY, const Input& input) {
    std::int16_t playerCenterY = 0;
    std::uint16_t playerHalfX = 0, playerHalfY = 0;
    switch (input.playerDescriptor) {
    case PlayerDescriptor::NormalA552: // source A552: center (0,-1), half (6,14)
        playerCenterY = -1; playerHalfX = 6; playerHalfY = 14; break;
    case PlayerDescriptor::ActiveDashBB38: // source BB38: center (0,+5), half (6,8)
        playerCenterY = 5; playerHalfX = 6; playerHalfY = 8; break;
    default: return false;
    }
    // C551: center (-51,-12), half (46,40). 84:9C0E reflects its horizontal
    // center when target D+11 bit6 is set; target-side descriptor has no Y mirror.
    const std::uint16_t tx = static_cast<std::uint16_t>(parentX + ((input.parentAttr11 & 0x40u) ? 51 : -51));
    const std::uint16_t ty = static_cast<std::uint16_t>(parentY - 12);
    const std::uint16_t px = input.playerX;
    const std::uint16_t py = static_cast<std::uint16_t>(input.playerY + playerCenterY);
    return absWrapped(tx, px) <= static_cast<std::uint16_t>(46 + playerHalfX) &&
           absWrapped(ty, py) <= static_cast<std::uint16_t>(40 + playerHalfY);
}

inline Output step(State& s, const Input& in, std::uint16_t parentX, std::uint16_t parentY) {
    s.childSignal = static_cast<std::uint8_t>(s.childSignal + in.childSignalIncrements);
    bool spawnChild = false;
    const auto tick = [&]() { s.animation.tick(); };
    const auto select = [&](std::uint8_t stream) { s.animation.select(stream); };

    switch (s.action) {
    case 0: { // 81:C0DA
        if (s.subaction == 0) {
            s.subaction = 1; s.arm = 120; select(1); break; // 84:8F07 then return
        }
        // 81:C0F8 CMP D+33 is followed by 81:C0FA: 80 07 (unconditional BRA).
        // Ordinary idle input therefore cannot select action 4 from Player X.
        if (s.arm != 0) {
            if (s.childSignal != 0) {
                s.childSignal = 0; s.action = 6; s.subaction = 0; break;
            }
            --s.arm;
            tick(); // 81:C110 -> BRA C130: armed frames do not call C551.
            break;
        }
        if (c551(parentX, parentY, in)) {
            s.action = 2; s.subaction = 0;
        }
        tick(); // C12D also ticks the old animation on the launch transition.
        break;
    }
    case 2: // 81:C139
        if (s.subaction == 0) { s.subaction = 1; select(2); break; }
        if ((s.animation.current().control & 0x40u) != 0) spawnChild = true;
        if ((s.animation.current().control & 0x80u) != 0) { s.action = 0; s.subaction = 0; }
        tick();
        break;
    case 4: // 81:C15A
        if (s.subaction == 0) { s.subaction = 1; select(3); break; }
        if ((s.animation.current().control & 0x80u) != 0) {
            s.facing ^= 0x40u; s.action = 0; s.subaction = 0;
        }
        tick();
        break;
    case 6: // 81:C17A, D+03 jump table C17F/C18E/C1A1
        if (s.subaction == 0) { s.subaction = 2; select(2); break; }
        if (s.subaction == 2 && (s.animation.current().control & 0x80u) != 0) {
            s.subaction = 4; select(2); tick(); break;
        }
        if (s.subaction == 4 && (s.animation.current().control & 0x80u) != 0) {
            s.action = 0; s.subaction = 0;
        }
        tick();
        break;
    default: break;
    }
    const Token t = s.animation.current();
    return {s.action, s.subaction, s.arm, t.control, t.art, s.childSignal,
            s.facing, s.animation.remaining, t.pointer, spawnChild};
}

} // namespace mmx::stretch_bird_parent
