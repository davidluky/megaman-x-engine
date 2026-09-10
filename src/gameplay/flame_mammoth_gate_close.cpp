// flame_mammoth_gate_close.cpp - source Player close action plus real map probes.
// Provenance: knowledge_base/mmx1/stages/flame-mammoth/gate_close_player.json.
#include "gameplay/flame_mammoth_gate_close.h"
#include "systems/tilemap.h"

namespace mmx::flame_mammoth_gate_close {
namespace {

int signed8(std::uint8_t value) {
    return value < 0x80 ? value : static_cast<int>(value) - 0x100;
}

int signed16(std::uint16_t value) {
    return value < 0x8000 ? value : static_cast<int>(value) - 0x10000;
}

std::uint32_t addFixed(std::uint32_t value, int delta) {
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(value) + delta)
        & 0xFFFFFFu;
}

bool readProbe(Step& step, const Tilemap& map, std::uint8_t xOffset,
               std::uint8_t yOffset, Probe& probe) {
    if (step.probeCount >= step.probes.size()) return false;
    probe.worldX = static_cast<std::uint16_t>(step.state.xPos() + signed8(xOffset));
    probe.worldY = static_cast<std::uint16_t>(step.state.yPos() + signed8(yOffset));
    const int col = probe.worldX / 16;
    const int row = probe.worldY / 16;
    if (col >= map.width() || row >= map.height()) return false;
    probe.blockId = map.mainTileId(col, row);
    if (probe.blockId < 0 || probe.blockId >= static_cast<int>(map.attrs().size()))
        return false;
    const int attr = map.attrs()[probe.blockId];
    if (attr < 0 || attr > 0xFF) return false;
    probe.rawAttr = static_cast<std::uint8_t>(attr);
    step.probes[step.probeCount++] = probe;
    return true;
}

bool terrainStep(Step& step, const Tilemap& map) {
    auto& state = step.state;
    if ((state.doState != 2 && state.doState != 4) || state.jumpFlag != 0 ||
        state.xPos() != state.xOld) return false;
    const bool downward = state.yPos() > state.yOld ||
        (state.yPos() == state.yOld && signed16(state.ySubSpeed) <= 0);
    if (!downward) return false;

    // 8491BE clears D+2B before scanning. The empty horizontal scan preserves
    // attrX; the vertical handler restores probeY=10 before dispatch.
    state.bd3 = 0;
    for (std::uint8_t xOffset : {std::uint8_t{0}, std::uint8_t{0xF9}, std::uint8_t{7}}) {
        Probe probe;
        if (!readProbe(step, map, xOffset, 0x16, probe)) return false;
        state.probeX = xOffset;
        state.probeY = 0x10;
        state.attrY = probe.rawAttr;
        if (probe.rawAttr == 0) continue;
        if (probe.rawAttr != 0x3B && probe.rawAttr != 0x38) return false;
        if (map.getTileType(probe.worldX / 16, probe.worldY / 16) == TileType::None)
            return false;

        // 8496B5: selector6 - ((probeY & 15)+1), on the integer Y word only.
        const int correction = signed8(static_cast<std::uint8_t>(
            6 - ((probe.worldY & 15) + 1)));
        state.yFixed = addFixed(state.yFixed, correction * 256);
        state.bd3 = 4;
        state.jumpFlag = 0;
        step.terrainResultA = 1;
        step.clearsContactGlobals = true;
        if (probe.rawAttr == 0x3B) return true;

        // 8498FA: conveyor +0080, followed by 8492AC only when X crosses an
        // integer. This source window proves positive, air-only post probes.
        state.xFixed = addFixed(state.xFixed, 0x80);
        const std::uint16_t delta = static_cast<std::uint16_t>(state.xPos() - state.xOld);
        if (delta == 0) return true;
        if (delta & 0x8000u) return false;
        for (std::uint8_t yOffset : {std::uint8_t{0xFF}, std::uint8_t{0xF6}, std::uint8_t{8}}) {
            if (!readProbe(step, map, 7, yOffset, probe) || probe.rawAttr != 0)
                return false;
            state.probeX = 7;
            state.probeY = yOffset;
            state.attrX = probe.rawAttr;
        }
        return true;
    }
    return true; // Known 84969C -> 84977A air leaf, A=0.
}

} // namespace

std::optional<Step> advance(const State& input, const Tilemap& map,
                            Dispatch dispatch) noexcept {
    if (dispatch.outer1F19 != 0 || dispatch.player64 != 0 ||
        dispatch.transient1F1C != 0x80 || dispatch.descriptor != 0xA552 ||
        input.action != 0x1E || input.xFixed > 0xFFFFFF || input.yFixed > 0xFFFFFF ||
        map.width() != 512 || map.height() != 64 || map.tileSize() != 16 ||
        map.collision().size() != 512u * 64u) return std::nullopt;

    Step step;
    auto& state = step.state;
    state = input;
    state.xOld = state.xPos();
    state.yOld = state.yPos();
    state.stateFlag = static_cast<std::uint8_t>(state.bd3 | state.bd4);
    // 81812E -> action1E -> 818165. Grounding is consumed on the next tick;
    // the held state preserves the residual Y speed without another gravity tick.
    if (state.doState == 0 || state.doState == 2) {
        if (state.stateFlag & 4) {
            state.doState = 4;
            state.jumpFlag = 0;
            step.animationCommand = 0x22;
        } else if (state.doState == 0) {
            state.doState = 2;
            state.xSubSpeed = 0;
            state.ySubSpeed = 0;
            state.weight = 0x40;
            step.animationCommand = 0x28;
        } else {
            state.xSubSpeed = static_cast<std::uint16_t>(state.xSubSpeed - (state.weight >> 8));
            state.ySubSpeed = static_cast<std::uint16_t>(state.ySubSpeed - (state.weight & 0xFF));
            state.xFixed = addFixed(state.xFixed, signed16(state.xSubSpeed));
            state.yFixed = addFixed(state.yFixed, -signed16(state.ySubSpeed));
        }
    } else if (state.doState == 4) {
        if (!(state.stateFlag & 4)) state.doState = 0;
    } else {
        return std::nullopt;
    }
    state.bd4 = 0;
    if (!terrainStep(step, map)) return std::nullopt;
    return step;
}

} // namespace mmx::flame_mammoth_gate_close
