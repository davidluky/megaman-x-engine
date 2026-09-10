// flame_mammoth_gate_close.h - Player action 1E during the measured FM gate close.
// Owns one pure fixed-point step. The scene owns applying its result to Player.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace mmx {
class Tilemap;
}

namespace mmx::flame_mammoth_gate_close {

// Source coordinates use the ROM actor anchor, before the canvas (-32,-40)
// conversion. Integer positions are views of 24-bit fixed point, never copies.
struct State {
    std::uint8_t action = 0x1E;
    std::uint8_t doState = 0;
    std::uint32_t xFixed = 0;
    std::uint32_t yFixed = 0;
    std::uint16_t xOld = 0;
    std::uint16_t yOld = 0;
    std::uint16_t xSubSpeed = 0;
    std::uint16_t ySubSpeed = 0;
    std::uint16_t weight = 0;
    std::uint8_t jumpFlag = 0; // D+2F: also the terrain selector byte
    std::uint8_t stateFlag = 0;
    std::uint8_t bd3 = 0; // D+2B
    std::uint8_t bd4 = 0; // D+2C
    std::uint8_t probeX = 0;
    std::uint8_t probeY = 0;
    std::uint8_t attrX = 0;
    std::uint8_t attrY = 0;

    std::uint16_t xPos() const { return static_cast<std::uint16_t>(xFixed >> 8); }
    std::uint16_t yPos() const { return static_cast<std::uint16_t>(yFixed >> 8); }
};

struct Dispatch {
    std::uint8_t outer1F19 = 0;
    std::uint8_t player64 = 0;
    std::uint8_t transient1F1C = 0x80;
    std::uint16_t descriptor = 0xA552;
};

struct Probe {
    std::uint16_t worldX = 0;
    std::uint16_t worldY = 0;
    int blockId = -1;
    std::uint8_t rawAttr = 0;
};

struct Step {
    State state;
    std::optional<std::uint8_t> animationCommand;
    std::uint8_t terrainResultA = 0;
    bool clearsContactGlobals = false; // Source 84994C clears 0C21/0C22.
    std::array<Probe, 6> probes{};
    std::size_t probeCount = 0;
};

// knowledge_base/mmx1/stages/flame-mammoth/gate_close_player.json.
// Only the retained downward selector branch (raw attrs 00/3B/38) is covered.
// nullopt means the entire step is unavailable: callers must not publish a
// partial action, animation command or terrain result. Source dispatch skips
// are also outside this strict terrain-owning caller. No map/state is mutated.
std::optional<Step> advance(const State& input, const Tilemap& map,
                            Dispatch dispatch = {}) noexcept;

} // namespace mmx::flame_mammoth_gate_close
