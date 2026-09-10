// x_sheet_composer.h - composes X armor-piece index maps into full sheets.
// Owns: CPU-side sheet/index buffers used before texture upload.

#pragma once

#include "data/x_palette.h"
#include "systems/raylib_resource.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace mmx {

class XSheetComposer {
public:
    bool load(const std::string& barePngPath, const std::string& indexPngPath) {
        ImageResource bare;
        if (!bare.load(barePngPath)) return false;
        bare.format(PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

        ImageResource index;
        if (!index.load(indexPngPath)) return false;
        index.format(PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

        if (bare.width() <= 0 || bare.height() <= 0) return false;
        if (bare.width() != index.width() || bare.height() != index.height()) return false;
        const Color* barePx = bare.pixels();
        const Color* indexPx = index.pixels();
        if (!barePx || !indexPx) return false;

        width_ = bare.width();
        height_ = bare.height();
        const size_t n = pixelCount();
        bareBase_.assign(barePx, barePx + n);
        bareIndex_.assign(n, 0);
        for (size_t i = 0; i < n; ++i) bareIndex_[i] = indexPx[i].r;

        deltas_.clear();
        compositeBase_.clear();
        compositeIndex_.clear();
        compositeKey_.clear();
        composed_ = false;
        loaded_ = true;
        return true;
    }

    bool loadDelta(const std::string& pieceName, const std::string& pngPath) {
        if (!loaded_) return false;
        ImageResource image;
        if (!image.load(pngPath)) return false;
        image.format(PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        if (image.width() != width_ || image.height() != height_) return false;
        const Color* px = image.pixels();
        if (!px) return false;
        std::vector<uint8_t> map(pixelCount());
        for (size_t i = 0; i < map.size(); ++i) map[i] = px[i].r;
        return loadDeltaFromMemory(pieceName, map, image.width(), image.height());
    }

    bool loadDeltaFromMemory(const std::string& pieceName,
                             const std::vector<uint8_t>& delta,
                             int width, int height) {
        if (!loaded_ || pieceName.empty()) return false;
        if (width != width_ || height != height_) return false;
        if (delta.size() != pixelCount()) return false;
        deltas_[pieceName] = delta;
        composed_ = false;
        return true;
    }

    bool compose(const XPaletteTable& palette,
                 const std::vector<std::string>& activePieceList) {
        if (!loaded_) {
            composed_ = false;
            return false;
        }
        compositeIndex_ = bareIndex_;
        compositeBase_ = bareBase_;
        compositeKey_.clear();

        bool firstKey = true;
        for (const std::string& piece : activePieceList) {
            auto it = deltas_.find(piece);
            if (it == deltas_.end()) continue;
            if (!firstKey) compositeKey_ += "+";
            compositeKey_ += piece;
            firstKey = false;
            applyDelta(palette, it->second);
        }
        composed_ = true;
        return true;
    }

    const std::vector<uint8_t>& indexData() const { return compositeIndex_; }
    const std::vector<Color>& baseData() const { return compositeBase_; }
    int width() const { return width_; }
    int height() const { return height_; }
    bool composed() const { return composed_; }
    const std::string& compositeKey() const { return compositeKey_; }

private:
    size_t pixelCount() const {
        return static_cast<size_t>(width_) * static_cast<size_t>(height_);
    }

    void applyDelta(const XPaletteTable& palette, const std::vector<uint8_t>& delta) {
        for (size_t i = 0; i < delta.size(); ++i) {
            const uint8_t v = delta[i];
            if (v == 255) continue;
            if (v == 0) {
                compositeIndex_[i] = 0;
                compositeBase_[i] = {0, 0, 0, 0};
                continue;
            }
            const int slot = v / 16;
            if (slot <= 0 || slot > 15) continue;
            compositeIndex_[i] = v;
            Color c = palette.rowColor("buster", slot);
            c.a = 255;
            compositeBase_[i] = c;
        }
    }

    bool loaded_ = false;
    bool composed_ = false;
    int width_ = 0;
    int height_ = 0;
    std::vector<uint8_t> bareIndex_;
    std::vector<Color> bareBase_;
    std::unordered_map<std::string, std::vector<uint8_t>> deltas_;
    std::vector<uint8_t> compositeIndex_;
    std::vector<Color> compositeBase_;
    std::string compositeKey_;
};

}  // namespace mmx
