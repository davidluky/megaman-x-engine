// destructible_terrain.h - SEI-3/SEI-4: how a destroyed fixture rewrites the
// terrain around it.
//
// Measured from David's Storm Eagle movie and gated by
// knowledge_base/mmx1/stages/storm_eagle/glass_shatter_cascade.json (nine
// waves, 186 cells over 72 ticks) and .../industrial_destruction.json
// (single-cell flips). Three properties of the source drove this design, and
// the engine's existing breakable path violates all three:
//
//   1. Destroying ONE fixture mutates MANY cells, spread over many ticks and
//      several screens - not the single cell under the hit.
//   2. A cell becomes a SPECIFIC new tile id (0x48 -> 0x49, 0x5C -> 0x0D).
//      Tilemap::breakTile clears to empty, which the source never does.
//   3. The mutation is NOT view-gated: the cascade rewrites cells the camera
//      never shows. There is deliberately no camera parameter anywhere here,
//      so an implementation cannot accidentally become view-gated.
//
// Shards are NOT objects. The source's debris never enters either object
// table, so this module emits tile mutations only; spawning debris entities
// (what the dead helmet path does today, with invented speeds) would be a
// different behaviour than the one measured.
//
// The Storm Eagle integration keeps those decisions in
// storm_eagle_destructible_terrain.h: it binds the measured source cells to
// the promoted post-state atlas slots and marks matching cells as breakable
// when an owner-approved placement is present in a loaded stage. It does not
// add fixtures to protected stage content. This generic header remains
// independent of that stage-specific content.
#pragma once

#include <cstddef>
#include <vector>

namespace mmx::destructible_terrain {

// One measured cell change. Applying it is fail-closed on fromTileId so a
// mis-placed schedule cannot silently corrupt unrelated terrain.
struct TileMutation {
    int col = 0;
    int row = 0;
    int fromTileId = 0;
    int toTileId = 0;
};

// A group of cells that all change on the same tick.
struct Wave {
    int tickOffset = 0;  // ticks after the trigger
    std::vector<TileMutation> cells;
};

struct CascadeSchedule {
    std::vector<Wave> waves;

    // The source fires an APU command one frame BEFORE each wave, for a
    // contiguous run of waves - in the glass cascade, command 0x1B ahead of
    // waves 0..7 but not the ninth.
    int sfxCommand = -1;          // -1 = no cue
    int sfxLeadTicks = 0;
    int sfxFromWaveIndex = 0;
    int sfxThroughWaveIndex = -1;  // inclusive; -1 = no cue
};

inline int totalCells(const CascadeSchedule& s) {
    int n = 0;
    for (const Wave& w : s.waves) n += static_cast<int>(w.cells.size());
    return n;
}

inline int lastTick(const CascadeSchedule& s) {
    int t = 0;
    for (const Wave& w : s.waves) {
        if (w.tickOffset > t) t = w.tickOffset;
    }
    return t;
}

// The source never flipped one offset twice across a whole cascade. A
// schedule that does is malformed, and the runner would apply the second
// mutation to a cell that no longer holds its expected old id.
inline bool offsetsUnique(const CascadeSchedule& s) {
    std::vector<const TileMutation*> seen;
    for (const Wave& w : s.waves) {
        for (const TileMutation& m : w.cells) {
            for (const TileMutation* p : seen) {
                if (p->col == m.col && p->row == m.row) return false;
            }
            seen.push_back(&m);
        }
    }
    return true;
}

// Rewrite one cell of a flat row-major tile-id grid, but only if it currently
// holds the measured old id and is in bounds. Returns whether it applied.
inline bool applyTo(std::vector<int>& grid, int width, const TileMutation& m) {
    // The column check also covers a degenerate width: for any width <= 0 no
    // column can satisfy 0 <= col < width, so a separate width guard would be
    // unreachable. The contract still asserts the zero-width behaviour.
    if (m.col < 0 || m.col >= width || m.row < 0) return false;
    const std::size_t idx = static_cast<std::size_t>(m.row) * static_cast<std::size_t>(width) +
                            static_cast<std::size_t>(m.col);
    if (idx >= grid.size()) return false;
    if (grid[idx] != m.fromTileId) return false;
    grid[idx] = m.toTileId;
    return true;
}

// Plays a schedule tick by tick. Holds no terrain and no camera: it reports
// which cells are due, and the caller applies them.
class CascadeRunner {
public:
    void trigger() {
        active_ = true;
        finished_ = false;
        tick_ = -1;
        due_.clear();
    }

    void reset() {
        active_ = false;
        finished_ = false;
        tick_ = -1;
        due_.clear();
    }

    bool active() const { return active_; }
    bool finished() const { return finished_; }
    int tick() const { return tick_; }

    // Advance one tick and return the cells due on it (empty if none).
    const std::vector<TileMutation>& advance(const CascadeSchedule& s) {
        due_.clear();
        if (!active_ || finished_) return due_;
        tick_++;
        for (const Wave& w : s.waves) {
            if (w.tickOffset != tick_) continue;
            due_.insert(due_.end(), w.cells.begin(), w.cells.end());
        }
        if (tick_ >= lastTick(s)) finished_ = true;
        return due_;
    }

    // Whether the cue fires on the CURRENT tick, i.e. sfxLeadTicks before one
    // of the cued waves.
    bool sfxDueThisTick(const CascadeSchedule& s) const {
        if (!active_ || s.sfxCommand < 0 || s.sfxThroughWaveIndex < 0) return false;
        const int last = s.sfxThroughWaveIndex < static_cast<int>(s.waves.size())
                             ? s.sfxThroughWaveIndex
                             : static_cast<int>(s.waves.size()) - 1;
        for (int i = s.sfxFromWaveIndex; i <= last; i++) {
            if (i < 0 || i >= static_cast<int>(s.waves.size())) continue;
            if (s.waves[i].tickOffset - s.sfxLeadTicks == tick_) return true;
        }
        return false;
    }

private:
    bool active_ = false;
    bool finished_ = false;
    int tick_ = -1;
    std::vector<TileMutation> due_;
};

}  // namespace mmx::destructible_terrain
