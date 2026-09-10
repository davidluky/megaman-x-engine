// storm_eagle_industrial_destruction.h - source-backed SEI-4 event data.
//
// This is a pure evidence adapter for the two measured industrial terrain
// phases. It intentionally stops at source event data and source-offset to
// stage-cell mapping: it does not choose a weapon, trigger, fixture, or
// runtime placement.
//
// Oracle: knowledge_base/mmx1/stages/storm_eagle/industrial_destruction.json
// and scene_geometry.json. The scene-definition formula is:
//   cell_row = (offset % 0x200) / 0x20
//   cell_col = (offset % 0x20) / 2
//   world cell = (screen_col * 16 + cell_col,
//                 screen_row * 16 + cell_row)
#pragma once

#include <array>
#include <cstddef>

namespace mmx::storm_eagle_industrial_destruction {

struct IndustrialCell {
    int sourceOffset = 0;
    int fromSourceTileId = 0;
    int toSourceTileId = 0;
};

struct IndustrialEvent {
    int sourceFrame = 0;
    int playerX = 0;
    int playerY = 0;
    int apuParam = 0;
    int apuFrameOffset = 0;
    int cellCount = 0;
    std::array<IndustrialCell, 3> cells{};
};

struct StageCell {
    int col = 0;
    int row = 0;
    int fromSourceTileId = 0;
    int toSourceTileId = 0;
};

inline constexpr int kDestructionCommand = 0x23;
inline constexpr int kPhaseOneScreenCol = 14;
inline constexpr int kPhaseOneScreenRow = 2;
inline constexpr int kPhaseTwoScreenCol = 15;
inline constexpr int kPhaseTwoScreenRow = 2;

inline constexpr std::array<IndustrialEvent, 4> kPhaseOneEvents = {{
    IndustrialEvent{9905, 3714, 559, 0x15, 0, 3,
                    {{IndustrialCell{0x4234, 0x7E, 0x00},
                      IndustrialCell{0x4254, 0x7F, 0x00},
                      IndustrialCell{0x4274, 0x80, 0x00}}}},
    IndustrialEvent{9911, 3720, 559, 0x15, 0, 3,
                    {{IndustrialCell{0x4236, 0x7E, 0x00},
                      IndustrialCell{0x4256, 0x7F, 0x00},
                      IndustrialCell{0x4276, 0x80, 0x00}}}},
    IndustrialEvent{9917, 3729, 559, 0x17, 0, 3,
                    {{IndustrialCell{0x4238, 0x7E, 0x00},
                      IndustrialCell{0x4258, 0x7F, 0x00},
                      IndustrialCell{0x4278, 0x80, 0x00}}}},
    IndustrialEvent{9923, 3738, 559, 0x22, 2, 3,
                    {{IndustrialCell{0x423A, 0x7E, 0x00},
                      IndustrialCell{0x425A, 0x7F, 0x00},
                      IndustrialCell{0x427A, 0x80, 0x00}}}},
}};

inline constexpr std::array<IndustrialEvent, 17> kPhaseTwoEvents = {{
    IndustrialEvent{10065, 3979, 642, 0x00, 0, 2,
                    {{IndustrialCell{0x44D0, 0x48, 0x49},
                      IndustrialCell{0x44D2, 0x48, 0x49}, IndustrialCell{}}}},
    IndustrialEvent{10073, 3979, 626, 0x00, 0, 2,
                    {{IndustrialCell{0x44B0, 0x48, 0x49},
                      IndustrialCell{0x44B2, 0x48, 0x49}, IndustrialCell{}}}},
    IndustrialEvent{10084, 3979, 610, 0x00, 0, 2,
                    {{IndustrialCell{0x4490, 0x48, 0x49},
                      IndustrialCell{0x4492, 0x48, 0x49}, IndustrialCell{}}}},
    IndustrialEvent{10150, 3975, 602, 0x00, 0, 1,
                    {{IndustrialCell{0x44CE, 0x48, 0x49}, IndustrialCell{},
                      IndustrialCell{}}}},
    IndustrialEvent{10158, 3978, 594, 0x00, 0, 2,
                    {{IndustrialCell{0x4470, 0x48, 0x49},
                      IndustrialCell{0x4472, 0x48, 0x49}, IndustrialCell{}}}},
    IndustrialEvent{10165, 3983, 578, 0x00, 0, 2,
                    {{IndustrialCell{0x4450, 0x48, 0x49},
                      IndustrialCell{0x4452, 0x48, 0x49}, IndustrialCell{}}}},
    IndustrialEvent{10183, 3975, 566, 0x00, 0, 1,
                    {{IndustrialCell{0x446E, 0x48, 0x49}, IndustrialCell{},
                      IndustrialCell{}}}},
    IndustrialEvent{10190, 3976, 562, 0x00, 0, 2,
                    {{IndustrialCell{0x4430, 0x48, 0x49},
                      IndustrialCell{0x4432, 0x48, 0x49}, IndustrialCell{}}}},
    IndustrialEvent{10235, 3975, 551, 0x00, 1, 1,
                    {{IndustrialCell{0x444E, 0x48, 0x49}, IndustrialCell{},
                      IndustrialCell{}}}},
    IndustrialEvent{10278, 3991, 573, 0x00, 0, 1,
                    {{IndustrialCell{0x4494, 0x48, 0x49}, IndustrialCell{},
                      IndustrialCell{}}}},
    IndustrialEvent{10300, 3992, 555, 0x00, 0, 1,
                    {{IndustrialCell{0x4474, 0x48, 0x49}, IndustrialCell{},
                      IndustrialCell{}}}},
    IndustrialEvent{10321, 3992, 549, 0x00, 0, 1,
                    {{IndustrialCell{0x4454, 0x48, 0x49}, IndustrialCell{},
                      IndustrialCell{}}}},
    IndustrialEvent{10372, 4008, 562, 0x00, 0, 1,
                    {{IndustrialCell{0x4434, 0x48, 0x49}, IndustrialCell{},
                      IndustrialCell{}}}},
    IndustrialEvent{10454, 4007, 642, 0x00, 0, 1,
                    {{IndustrialCell{0x44D4, 0x48, 0x49}, IndustrialCell{},
                      IndustrialCell{}}}},
    IndustrialEvent{10462, 4007, 626, 0x00, 0, 1,
                    {{IndustrialCell{0x44B4, 0x48, 0x49}, IndustrialCell{},
                      IndustrialCell{}}}},
    IndustrialEvent{10513, 3973, 626, 0x00, 0, 1,
                    {{IndustrialCell{0x44AE, 0x48, 0x49}, IndustrialCell{},
                      IndustrialCell{}}}},
    IndustrialEvent{10522, 3973, 610, 0x00, 0, 1,
                    {{IndustrialCell{0x448E, 0x48, 0x49}, IndustrialCell{},
                      IndustrialCell{}}}},
}};

template <std::size_t N>
inline int totalCells(const std::array<IndustrialEvent, N>& events) {
    int total = 0;
    for (const IndustrialEvent& event : events) total += event.cellCount;
    return total;
}

template <std::size_t N>
inline bool framesStrictlyIncrease(
    const std::array<IndustrialEvent, N>& events) {
    for (std::size_t i = 1; i < events.size(); ++i) {
        if (events[i - 1].sourceFrame >= events[i].sourceFrame) return false;
    }
    return true;
}

template <std::size_t N>
inline bool offsetsUnique(const std::array<IndustrialEvent, N>& events) {
    std::array<int, 35> seen{};
    int seenCount = 0;
    for (const IndustrialEvent& event : events) {
        for (int i = 0; i < event.cellCount; ++i) {
            const int offset = event.cells[static_cast<std::size_t>(i)].sourceOffset;
            for (int j = 0; j < seenCount; ++j) {
                if (seen[static_cast<std::size_t>(j)] == offset) return false;
            }
            seen[static_cast<std::size_t>(seenCount++)] = offset;
        }
    }
    return true;
}

inline StageCell stageCellForOffset(int screenCol,
                                    int screenRow,
                                    const IndustrialCell& source) {
    const int localOffset = source.sourceOffset % 0x200;
    const int cellCol = (source.sourceOffset % 0x20) / 2;
    const int cellRow = localOffset / 0x20;
    return {screenCol * 16 + cellCol, screenRow * 16 + cellRow,
            source.fromSourceTileId, source.toSourceTileId};
}

}  // namespace mmx::storm_eagle_industrial_destruction
