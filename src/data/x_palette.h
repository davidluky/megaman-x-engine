// x_palette.h - declares X sprite palette lookup and recolor services.
// Owns: palette rows, armor-slot metadata, and indexed sheet maps.

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "raylib.h"

namespace mmx {

// Per-game X palette table with TWO recolor mechanisms:
//
// 1. INDEXED (the endgame — knowledge_base/_indexed_sheet/REPORT.md
//    2026-06-09): content/x1/sprites/x_spritesheet_index.png stores each
//    sheet pixel's SNES palette index (pixel value = index*16, 255 = no
//    data), and content/x1/palettes/x_weapon_rows.json the per-weapon
//    16-color body rows decoded from the real game's $7E0420 WRAM shadow.
//    Recolor = row[index], full row, no slot special-casing. Oracle
//    acceptance: baked idles diff 0 vs real-game screenshots for all nine
//    weapons. This is how the real game does it; the color->color LUT can
//    never be exact because base X reuses identical RGBs in slots that
//    diverge per weapon.
//
// 2. LEGACY nearest-color LUT (x_palettes.json): kept as the fallback for
//    the 38 no-data pixels and for callers without sheet coordinates.
class XPaletteTable {
public:
    bool load(const std::string& path);
    bool valid() const { return !normal_.empty(); }

    const std::vector<Color>& normal() const { return normal_; }
    // Palette for a weapon id (legend key); falls back to Normal if unknown.
    const std::vector<Color>& palette(const std::string& weaponId) const;

    // Legacy recolor of one pixel. Pixels whose nearest Normal slot is
    // farther than kPassThroughDist stay unchanged.
    Color recolor(Color src, const std::string& weaponId) const;

    // --- Indexed path ---
    bool loadIndexed(const std::string& indexPngPath,
                     const std::string& rowsJsonPath);
    bool setIndexMap(const std::vector<uint8_t>& map, int width, int height);
    bool indexed() const { return indexedReady_; }
    int indexWidth() const { return idxW_; }
    int indexHeight() const { return idxH_; }
    // Raw stored value at (x,y): index*16, 255 = no data, 0 on out-of-bounds.
    uint8_t rawIndexValue(int x, int y) const;
    // Palette index 0..15 at (x,y); -1 = no data / out of bounds.
    int indexAt(int x, int y) const;
    // Body-row color (sheet-space) for a weapon; falls back to buster row.
    Color rowColor(const std::string& weaponId, int idx) const;
    // Full per-sheet-pixel recolor: indexed lookup where data exists,
    // legacy recolor(src) otherwise. Index 0 (transparent) returns src.
    Color recolorAt(int x, int y, Color src, const std::string& weaponId) const;

private:
    static constexpr int kPassThroughDist = 80;
    std::vector<Color> normal_;
    std::unordered_map<std::string, std::vector<Color>> weapons_;
    // Per-slot: true if this slot is recolorable armor. Non-armor slots
    // (outline, skin, white, gray) pass through unchanged so the original
    // sprite's black outline and highlights are preserved.
    std::vector<bool> armorSlot_;

    bool indexedReady_ = false;
    int idxW_ = 0, idxH_ = 0;
    std::vector<uint8_t> indexMap_;  // raw png values (index*16; 255 no data)
    std::unordered_map<std::string, std::array<Color, 16>> bodyRows_;
};

}  // namespace mmx
