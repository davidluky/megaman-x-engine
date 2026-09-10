// storm_eagle_destructible_terrain.h - source-backed Storm Eagle block interactions.
//
// The coordinates and tile transitions below are derived from the measured
// glass-cascade source artifact. The source-tile ids stay alongside the
// engine ids so a future atlas revision cannot silently change the mapping.
#pragma once

#include "gameplay/destructible_terrain.h"
#include "systems/tilemap.h"

#include <array>
#include <cstddef>
#include <utility>

namespace mmx::storm_eagle_destructible_terrain {

struct SourceMutation {
    int col = 0;
    int row = 0;
    int fromSourceTileId = 0;
    int toSourceTileId = 0;
    int expectedEngineTileId = 0;
    int replacementEngineTileId = 0;
    int tickOffset = 0;
};

inline constexpr int kMutationCount = 186;
inline constexpr int kFirstWaveTick = 8;
inline constexpr int kLastWaveTick = 72;

inline const std::array<SourceMutation, kMutationCount>& mutations() {
    static const std::array<SourceMutation, kMutationCount> kMutations = {{
{103, 35, 0x5C, 0x0D, 92, 728, 8},
    {104, 35, 0x5D, 0x0C, 93, 727, 8},
    {103, 36, 0x5F, 0x10, 95, 731, 8},
    {104, 36, 0x60, 0x11, 96, 732, 8},
    {103, 37, 0x5F, 0x12, 95, 733, 8},
    {104, 37, 0x60, 0x13, 96, 734, 8},
    {103, 38, 0x62, 0x17, 98, 738, 8},
    {104, 38, 0x63, 0x18, 99, 739, 8},
    {103, 39, 0x66, 0x11, 102, 732, 8},
    {104, 39, 0x66, 0x11, 102, 732, 8},
    {103, 40, 0x69, 0x18, 105, 739, 8},
    {104, 40, 0x69, 0x18, 105, 739, 8},
    {101, 35, 0x5C, 0x0D, 92, 728, 14},
    {102, 35, 0x5D, 0x0C, 93, 727, 14},
    {105, 35, 0x5C, 0x0D, 92, 728, 14},
    {106, 35, 0x5D, 0x0C, 93, 727, 14},
    {101, 36, 0x5F, 0x10, 95, 731, 14},
    {102, 36, 0x60, 0x11, 96, 732, 14},
    {105, 36, 0x5F, 0x10, 95, 731, 14},
    {106, 36, 0x60, 0x11, 96, 732, 14},
    {101, 37, 0x5F, 0x12, 95, 733, 14},
    {102, 37, 0x60, 0x13, 96, 734, 14},
    {105, 37, 0x5F, 0x12, 95, 733, 14},
    {106, 37, 0x60, 0x13, 96, 734, 14},
    {101, 38, 0x62, 0x17, 98, 738, 14},
    {102, 38, 0x63, 0x18, 99, 739, 14},
    {105, 38, 0x62, 0x17, 98, 738, 14},
    {106, 38, 0x63, 0x18, 99, 739, 14},
    {101, 39, 0x65, 0x19, 101, 0, 14},
    {102, 39, 0x66, 0x11, 102, 732, 14},
    {105, 39, 0x65, 0x19, 101, 0, 14},
    {106, 39, 0x66, 0x11, 102, 732, 14},
    {101, 40, 0x68, 0x17, 104, 738, 14},
    {102, 40, 0x69, 0x18, 105, 739, 14},
    {105, 40, 0x68, 0x17, 104, 738, 14},
    {106, 40, 0x69, 0x18, 105, 739, 14},
    {99, 35, 0x5C, 0x0D, 92, 728, 24},
    {100, 35, 0x5D, 0x0C, 93, 727, 24},
    {107, 35, 0x5C, 0x0D, 92, 728, 24},
    {108, 35, 0x5D, 0x0C, 93, 727, 24},
    {99, 36, 0x5F, 0x10, 95, 731, 24},
    {100, 36, 0x60, 0x11, 96, 732, 24},
    {107, 36, 0x5F, 0x10, 95, 731, 24},
    {108, 36, 0x60, 0x11, 96, 732, 24},
    {99, 37, 0x5F, 0x12, 95, 733, 24},
    {100, 37, 0x60, 0x13, 96, 734, 24},
    {107, 37, 0x5F, 0x12, 95, 733, 24},
    {108, 37, 0x60, 0x13, 96, 734, 24},
    {99, 38, 0x62, 0x17, 98, 738, 24},
    {100, 38, 0x63, 0x18, 99, 739, 24},
    {107, 38, 0x62, 0x17, 98, 738, 24},
    {108, 38, 0x63, 0x18, 99, 739, 24},
    {99, 39, 0x66, 0x11, 102, 732, 24},
    {100, 39, 0x66, 0x11, 102, 732, 24},
    {107, 39, 0x66, 0x11, 102, 732, 24},
    {108, 39, 0x66, 0x11, 102, 732, 24},
    {99, 40, 0x69, 0x18, 105, 739, 24},
    {100, 40, 0x69, 0x18, 105, 739, 24},
    {107, 40, 0x69, 0x18, 105, 739, 24},
    {108, 40, 0x69, 0x18, 105, 739, 24},
    {97, 35, 0x5C, 0x0D, 92, 728, 36},
    {98, 35, 0x5D, 0x0C, 93, 727, 36},
    {109, 35, 0x5C, 0x0D, 92, 728, 36},
    {110, 35, 0x5D, 0x0C, 93, 727, 36},
    {97, 36, 0x5F, 0x10, 95, 731, 36},
    {98, 36, 0x60, 0x11, 96, 732, 36},
    {109, 36, 0x5F, 0x10, 95, 731, 36},
    {110, 36, 0x60, 0x11, 96, 732, 36},
    {97, 37, 0x5F, 0x12, 95, 733, 36},
    {98, 37, 0x60, 0x13, 96, 734, 36},
    {109, 37, 0x5F, 0x12, 95, 733, 36},
    {110, 37, 0x60, 0x13, 96, 734, 36},
    {97, 38, 0x62, 0x17, 98, 738, 36},
    {98, 38, 0x63, 0x18, 99, 739, 36},
    {109, 38, 0x62, 0x17, 98, 738, 36},
    {110, 38, 0x63, 0x18, 99, 739, 36},
    {97, 39, 0x65, 0x19, 101, 0, 36},
    {98, 39, 0x66, 0x11, 102, 732, 36},
    {109, 39, 0x65, 0x19, 101, 0, 36},
    {110, 39, 0x66, 0x11, 102, 732, 36},
    {97, 40, 0x68, 0x17, 104, 738, 36},
    {98, 40, 0x69, 0x18, 105, 739, 36},
    {109, 40, 0x68, 0x17, 104, 738, 36},
    {110, 40, 0x69, 0x18, 105, 739, 36},
    {95, 35, 0x5C, 0x0D, 92, 728, 46},
    {95, 36, 0x5F, 0x10, 95, 731, 46},
    {95, 37, 0x5F, 0x12, 95, 733, 46},
    {95, 38, 0x62, 0x17, 98, 738, 46},
    {95, 39, 0x66, 0x11, 102, 732, 46},
    {95, 40, 0x69, 0x18, 105, 739, 46},
    {96, 35, 0x5D, 0x0C, 93, 727, 46},
    {111, 35, 0x5C, 0x0D, 92, 728, 46},
    {96, 36, 0x60, 0x11, 96, 732, 46},
    {111, 36, 0x5F, 0x10, 95, 731, 46},
    {96, 37, 0x60, 0x13, 96, 734, 46},
    {111, 37, 0x5F, 0x12, 95, 733, 46},
    {96, 38, 0x63, 0x18, 99, 739, 46},
    {111, 38, 0x62, 0x17, 98, 738, 46},
    {96, 39, 0x66, 0x11, 102, 732, 46},
    {111, 39, 0x66, 0x11, 102, 732, 46},
    {96, 40, 0x69, 0x18, 105, 739, 46},
    {111, 40, 0x69, 0x18, 105, 739, 46},
    {112, 35, 0x5D, 0x0C, 93, 727, 46},
    {112, 36, 0x60, 0x11, 96, 732, 46},
    {112, 37, 0x60, 0x13, 96, 734, 46},
    {112, 38, 0x63, 0x18, 99, 739, 46},
    {112, 39, 0x66, 0x11, 102, 732, 46},
    {112, 40, 0x69, 0x18, 105, 739, 46},
    {93, 35, 0x5C, 0x0D, 92, 728, 53},
    {94, 35, 0x5D, 0x0C, 93, 727, 53},
    {93, 36, 0x5F, 0x10, 95, 731, 53},
    {94, 36, 0x60, 0x11, 96, 732, 53},
    {93, 37, 0x5F, 0x12, 95, 733, 53},
    {94, 37, 0x60, 0x13, 96, 734, 53},
    {93, 38, 0x62, 0x17, 98, 738, 53},
    {94, 38, 0x63, 0x18, 99, 739, 53},
    {93, 39, 0x65, 0x19, 101, 0, 53},
    {94, 39, 0x66, 0x11, 102, 732, 53},
    {93, 40, 0x68, 0x17, 104, 738, 53},
    {94, 40, 0x69, 0x18, 105, 739, 53},
    {113, 35, 0x5C, 0x0D, 92, 728, 53},
    {114, 35, 0x5D, 0x0C, 93, 727, 53},
    {113, 36, 0x5F, 0x10, 95, 731, 53},
    {114, 36, 0x60, 0x11, 96, 732, 53},
    {113, 37, 0x5F, 0x12, 95, 733, 53},
    {114, 37, 0x60, 0x13, 96, 734, 53},
    {113, 38, 0x62, 0x17, 98, 738, 53},
    {114, 38, 0x63, 0x18, 99, 739, 53},
    {113, 39, 0x65, 0x19, 101, 0, 53},
    {114, 39, 0x66, 0x11, 102, 732, 53},
    {113, 40, 0x68, 0x17, 104, 738, 53},
    {114, 40, 0x69, 0x18, 105, 739, 53},
    {91, 35, 0x5C, 0x0D, 92, 728, 60},
    {92, 35, 0x5D, 0x0C, 93, 727, 60},
    {91, 36, 0x5F, 0x10, 95, 731, 60},
    {92, 36, 0x60, 0x11, 96, 732, 60},
    {91, 37, 0x5F, 0x12, 95, 733, 60},
    {92, 37, 0x60, 0x13, 96, 734, 60},
    {91, 38, 0x62, 0x17, 98, 738, 60},
    {92, 38, 0x63, 0x18, 99, 739, 60},
    {91, 39, 0x66, 0x11, 102, 732, 60},
    {92, 39, 0x66, 0x11, 102, 732, 60},
    {91, 40, 0x69, 0x18, 105, 739, 60},
    {92, 40, 0x69, 0x18, 105, 739, 60},
    {115, 35, 0x5C, 0x0D, 92, 728, 60},
    {116, 35, 0x5C, 0x0D, 92, 728, 60},
    {115, 36, 0x5F, 0x10, 95, 731, 60},
    {116, 36, 0x5F, 0x19, 95, 0, 60},
    {115, 37, 0x5F, 0x12, 95, 733, 60},
    {116, 37, 0x5F, 0x2E, 95, 744, 60},
    {115, 38, 0x62, 0x17, 98, 738, 60},
    {116, 38, 0x62, 0x17, 98, 738, 60},
    {115, 39, 0x66, 0x11, 102, 732, 60},
    {116, 39, 0x65, 0x19, 101, 0, 60},
    {115, 40, 0x69, 0x18, 105, 739, 60},
    {116, 40, 0x68, 0x17, 104, 738, 60},
    {89, 35, 0x5C, 0x0B, 92, 726, 66},
    {90, 35, 0x5D, 0x0C, 93, 727, 66},
    {89, 36, 0x5F, 0x00, 95, 0, 66},
    {90, 36, 0x60, 0x0F, 96, 730, 66},
    {89, 37, 0x5F, 0x00, 95, 0, 66},
    {90, 37, 0x60, 0x0E, 96, 729, 66},
    {89, 38, 0x62, 0x15, 98, 736, 66},
    {90, 38, 0x63, 0x16, 99, 737, 66},
    {89, 39, 0x65, 0x00, 101, 0, 66},
    {90, 39, 0x66, 0x0E, 102, 729, 66},
    {89, 40, 0x68, 0x15, 104, 736, 66},
    {90, 40, 0x69, 0x16, 105, 737, 66},
    {117, 35, 0x6F, 0x29, 111, 740, 66},
    {118, 35, 0x5C, 0x2A, 92, 741, 66},
    {117, 36, 0x71, 0x2C, 113, 742, 66},
    {118, 36, 0x5F, 0x00, 95, 0, 66},
    {117, 37, 0x71, 0x2D, 113, 743, 66},
    {118, 37, 0x5F, 0x00, 95, 0, 66},
    {117, 38, 0x73, 0x2F, 115, 745, 66},
    {118, 38, 0x62, 0x15, 98, 736, 66},
    {117, 39, 0x75, 0x2D, 117, 743, 66},
    {118, 39, 0x65, 0x00, 101, 0, 66},
    {117, 40, 0x77, 0x2F, 119, 745, 66},
    {118, 40, 0x68, 0x15, 104, 736, 66},
    {88, 35, 0x5B, 0x0A, 91, 725, 72},
    {88, 36, 0x5E, 0x0E, 94, 729, 72},
    {88, 37, 0x5E, 0x0E, 94, 729, 72},
    {88, 38, 0x61, 0x14, 97, 735, 72},
    {88, 39, 0x64, 0x0E, 100, 729, 72},
    {88, 40, 0x67, 0x14, 103, 735, 72},
    }};
    return kMutations;
}

inline const SourceMutation* mutationAt(int col, int row) {
    for (const SourceMutation& mutation : mutations()) {
        if (mutation.col == col && mutation.row == row) return &mutation;
    }
    return nullptr;
}

inline mmx::destructible_terrain::CascadeSchedule measuredCascade() {
    mmx::destructible_terrain::CascadeSchedule schedule;
    schedule.sfxCommand = 0x1B;
    schedule.sfxLeadTicks = 1;
    schedule.sfxFromWaveIndex = 0;
    schedule.sfxThroughWaveIndex = 7;

    constexpr int kWaveTicks[] = {8, 14, 24, 36, 46, 53, 60, 66, 72};
    for (int tick : kWaveTicks) {
        mmx::destructible_terrain::Wave wave;
        wave.tickOffset = tick;
        for (const SourceMutation& source : mutations()) {
            if (source.tickOffset != tick) continue;
            wave.cells.push_back({source.col, source.row,
                                  source.expectedEngineTileId,
                                  source.replacementEngineTileId});
        }
        schedule.waves.push_back(std::move(wave));
    }
    return schedule;
}

// Mark only source cells that already exist in the loaded stage. This is a
// runtime binding seam, not protected-content placement: an empty match set
// leaves the stage unchanged and must remain an explicit review blocker.
inline int installBreakableFixtures(Tilemap& tilemap) {
    int installedCount = 0;
    for (const SourceMutation& mutation : mutations()) {
        if (tilemap.mainTileId(mutation.col, mutation.row) !=
            mutation.expectedEngineTileId) {
            continue;
        }
        tilemap.setCollision(mutation.col, mutation.row, TileType::Breakable);
        ++installedCount;
    }
    return installedCount;
}

inline bool applyMutation(Tilemap& tilemap, const SourceMutation& mutation) {
    return tilemap.replaceDirectMainTile(
        mutation.col, mutation.row,
        mutation.expectedEngineTileId, mutation.replacementEngineTileId,
        TileType::None);
}

}  // namespace mmx::storm_eagle_destructible_terrain
