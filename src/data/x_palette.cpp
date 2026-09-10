// x_palette.cpp - loads X palette tables and applies weapon/armor recolors.
// Owns: indexed sheet palette mapping plus legacy nearest-color fallback.

#include "data/x_palette.h"

#include <cstdlib>
#include <fstream>

#include "nlohmann/json.hpp"
#include "systems/raylib_resource.h"

namespace mmx {

using json = nlohmann::json;

static Color toColor(const json& a) {
    return {
        static_cast<unsigned char>(a[0].get<int>()),
        static_cast<unsigned char>(a[1].get<int>()),
        static_cast<unsigned char>(a[2].get<int>()),
        255,
    };
}

bool XPaletteTable::load(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;
    json j;
    try {
        f >> j;
    } catch (const std::exception&) {
        return false;
    }
    if (!j.contains("palettes")) return false;
    normal_.clear();
    weapons_.clear();
    for (auto it = j["palettes"].begin(); it != j["palettes"].end(); ++it) {
        std::vector<Color> ramp;
        for (const auto& c : it.value()) ramp.push_back(toColor(c));
        if (it.key() == "normal") {
            normal_ = ramp;
        } else {
            weapons_[it.key()] = ramp;
        }
    }
    armorSlot_.assign(normal_.size(), false);
    if (j.contains("armorSlots")) {
        for (const auto& s : j["armorSlots"]) {
            const int i = s.get<int>();
            if (i >= 0 && i < static_cast<int>(armorSlot_.size())) armorSlot_[i] = true;
        }
    }
    return !normal_.empty();
}

const std::vector<Color>& XPaletteTable::palette(const std::string& weaponId) const {
    auto it = weapons_.find(weaponId);
    return it != weapons_.end() ? it->second : normal_;
}

bool XPaletteTable::loadIndexed(const std::string& indexPngPath,
                                const std::string& rowsJsonPath) {
    // Rows first (cheap to fail).
    std::ifstream f(rowsJsonPath);
    if (!f) return false;
    json j;
    try {
        f >> j;
    } catch (const std::exception&) {
        return false;
    }
    if (!j.contains("x_body_rows")) return false;
    bodyRows_.clear();
    for (auto it = j["x_body_rows"].begin(); it != j["x_body_rows"].end(); ++it) {
        if (!it.value().contains("rgb_sheet_space")) continue;
        const auto& sheet = it.value()["rgb_sheet_space"];
        if (!sheet.is_array() || sheet.size() != 16) continue;
        std::array<Color, 16> row{};
        for (size_t i = 0; i < 16; ++i) row[i] = toColor(sheet[i]);
        bodyRows_[it.key()] = row;
    }
    if (bodyRows_.empty()) return false;

    // Index map: CPU-side image, .r channel carries index*16 (255 = no data).
    ImageResource img;
    if (!img.load(indexPngPath)) return false;
    img.format(PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    const Color* px = img.pixels();
    if (!px || img.width() <= 0 || img.height() <= 0) return false;
    idxW_ = img.width();
    idxH_ = img.height();
    indexMap_.assign(static_cast<size_t>(idxW_) * idxH_, 0);
    for (size_t i = 0; i < indexMap_.size(); ++i) indexMap_[i] = px[i].r;
    indexedReady_ = true;
    return true;
}

bool XPaletteTable::setIndexMap(const std::vector<uint8_t>& map,
                                int width, int height) {
    if (width <= 0 || height <= 0) return false;
    const auto expected = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (map.size() != expected) return false;
    idxW_ = width;
    idxH_ = height;
    indexMap_ = map;
    indexedReady_ = true;
    return true;
}

uint8_t XPaletteTable::rawIndexValue(int x, int y) const {
    if (!indexedReady_ || x < 0 || y < 0 || x >= idxW_ || y >= idxH_) return 0;
    return indexMap_[static_cast<size_t>(y) * idxW_ + x];
}

int XPaletteTable::indexAt(int x, int y) const {
    if (!indexedReady_ || x < 0 || y < 0 || x >= idxW_ || y >= idxH_) return -1;
    const uint8_t v = indexMap_[static_cast<size_t>(y) * idxW_ + x];
    if (v == 255) return -1;
    return v / 16;
}

Color XPaletteTable::rowColor(const std::string& weaponId, int idx) const {
    if (idx < 0 || idx > 15) return {0, 0, 0, 255};
    auto it = bodyRows_.find(weaponId);
    if (it == bodyRows_.end()) it = bodyRows_.find("buster");
    if (it == bodyRows_.end()) return {0, 0, 0, 255};
    return it->second[static_cast<size_t>(idx)];
}

Color XPaletteTable::recolorAt(int x, int y, Color src,
                               const std::string& weaponId) const {
    const int idx = indexAt(x, y);
    if (idx == 0) return src;                      // transparent
    if (idx > 0 && bodyRows_.count(weaponId)) {
        Color c = rowColor(weaponId, idx);
        c.a = src.a;
        return c;
    }
    return recolor(src, weaponId);                 // no data -> legacy LUT
}

Color XPaletteTable::recolor(Color src, const std::string& weaponId) const {
    if (normal_.empty()) return src;
    int best = -1;
    int bestDist = 1 << 30;
    for (int i = 0; i < static_cast<int>(normal_.size()); ++i) {
        const int d = std::abs(src.r - normal_[i].r)
                    + std::abs(src.g - normal_[i].g)
                    + std::abs(src.b - normal_[i].b);
        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    // Only recolor armor slots; outline / skin / white / gray pass through so the
    // original sprite's black outline and highlights stay intact.
    if (bestDist > kPassThroughDist || best < 0
        || best >= static_cast<int>(armorSlot_.size()) || !armorSlot_[best]) {
        return src;
    }
    const std::vector<Color>& pal = palette(weaponId);
    if (best >= static_cast<int>(pal.size())) return src;
    Color c = pal[best];
    c.a = src.a;
    return c;
}

}  // namespace mmx
