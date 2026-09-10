// tilemap.cpp - renders loaded tilemaps and exposes runtime map diagnostics.
// Owns: layer visibility, foreground/background drawing, and region lookups.

#include "systems/tilemap.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace {

int positiveModulo(int value, int divisor) {
    if (divisor <= 0) return 0;
    int result = value % divisor;
    return result < 0 ? result + divisor : result;
}

} // namespace

namespace mmx {

static bool idListAllows(const std::vector<std::string>& ids, std::string_view activeId) {
    if (ids.empty()) return true;
    return std::find(ids.begin(), ids.end(), activeId) != ids.end();
}

static bool phaseListAllows(const TileLayer& layer, int visualPhaseTick) {
    if (layer.visualPhaseIds.empty()) return true;
    if (layer.visualPhasePeriod <= 0) return false;
    const int activePhase = positiveModulo(visualPhaseTick, layer.visualPhasePeriod);
    return std::find(layer.visualPhaseIds.begin(), layer.visualPhaseIds.end(), activePhase)
        != layer.visualPhaseIds.end();
}

Tilemap::~Tilemap() {
    clearLoadedData();
}

void Tilemap::replaceSpawns(std::vector<SpawnPoint> spawns) {
    spawns_ = std::move(spawns);
}

bool Tilemap::layerVisibleInCameraSection(const TileLayer& layer,
                                          std::string_view activeCameraSectionId) {
    return idListAllows(layer.cameraSectionIds, activeCameraSectionId);
}

bool Tilemap::layerVisibleInSections(const TileLayer& layer,
                                     std::string_view activeCameraSectionId,
                                     std::string_view activeVisualSectionId) {
    return layerVisibleInSections(layer, activeCameraSectionId, activeVisualSectionId, 0);
}

bool Tilemap::layerVisibleInSections(const TileLayer& layer,
                                     std::string_view activeCameraSectionId,
                                     std::string_view activeVisualSectionId,
                                     int visualPhaseTick) {
    return idListAllows(layer.cameraSectionIds, activeCameraSectionId)
        && idListAllows(layer.visualSectionIds, activeVisualSectionId)
        && phaseListAllows(layer, visualPhaseTick);
}

std::vector<TileLayerRenderDiagnostic> Tilemap::collectRenderLayerDiagnostics(
    float cameraX, float cameraY,
    std::string_view activeCameraSectionId,
    std::string_view activeVisualSectionId) const {
    std::vector<TileLayerRenderDiagnostic> out;
    out.reserve(layers_.size());
    for (const auto& layer : layers_) {
        TileLayerRenderDiagnostic item;
        item.name = layer.name;
        item.drawAfterEntities = layer.drawAfterEntities;
        item.cameraSectionAllowed = idListAllows(layer.cameraSectionIds, activeCameraSectionId);
        item.visualSectionAllowed = idListAllows(layer.visualSectionIds, activeVisualSectionId);
        item.phaseAllowed = phaseListAllows(layer, animTick_);
        item.visible = item.cameraSectionAllowed && item.visualSectionAllowed && item.phaseAllowed;
        item.visualPhaseTick = animTick_;
        item.visualPhasePeriod = layer.visualPhasePeriod;
        item.activePhase = layer.visualPhasePeriod > 0
            ? positiveModulo(animTick_, layer.visualPhasePeriod)
            : 0;
        item.visualPhaseIds = layer.visualPhaseIds;
        item.cameraSectionIds = layer.cameraSectionIds;
        item.visualSectionIds = layer.visualSectionIds;
        item.parallaxX = layer.parallaxX;
        item.parallaxY = layer.parallaxY;
        item.previewOffsetX = layer.previewOffsetX;
        item.previewOffsetY = layer.previewOffsetY;
        item.scrollX = static_cast<int>(cameraX * layer.parallaxX) + layer.previewOffsetX;
        item.scrollY = static_cast<int>(cameraY * layer.parallaxY) + layer.previewOffsetY;
        item.previewPath = layer.previewPath;
        item.previewWidth = layer.previewTex.valid() ? layer.previewTex.width() : 0;
        item.previewHeight = layer.previewTex.valid() ? layer.previewTex.height() : 0;
        item.repeatPreviewX = layer.repeatPreviewX;
        item.repeatPreviewY = layer.repeatPreviewY;
        out.push_back(std::move(item));
    }
    return out;
}
void Tilemap::render(float cameraX, float cameraY) const {
    render(cameraX, cameraY, std::string_view{});
}

void Tilemap::render(float cameraX, float cameraY,
                     std::string_view activeCameraSectionId) const {
    render(cameraX, cameraY, activeCameraSectionId, std::string_view{});
}

void Tilemap::render(float cameraX, float cameraY,
                     std::string_view activeCameraSectionId,
                     std::string_view activeVisualSectionId) const {
    for (const auto& layer : layers_) {
        if (!layer.drawAfterEntities &&
            layerVisibleInSections(layer, activeCameraSectionId, activeVisualSectionId, animTick_))
            renderLayer(layer, cameraX, cameraY, activeVisualSectionId);
    }
}

const Texture2D* Tilemap::tilesetForVisualSection(
    std::string_view activeVisualSectionId) const {
    if (!activeVisualSectionId.empty()) {
        const auto it = sectionTilesets_.find(std::string(activeVisualSectionId));
        if (it != sectionTilesets_.end() && it->second.valid()) return &it->second.get();
    }
    return tileset_.valid() ? &tileset_.get() : nullptr;
}

void Tilemap::renderTilePriorityForeground(
    float cameraX, float cameraY,
    std::string_view activeCameraSectionId,
    std::string_view activeVisualSectionId) const {
    if (tilePriority_.empty() || !useDirectTiles_ || !tileset_.valid() ||
        tilesetCols_ <= 0)
        return;

    const TileLayer* main = nullptr;
    for (const auto& layer : layers_) {
        if (layer.name == "main") { main = &layer; break; }
    }
    if (!main && !layers_.empty()) main = &layers_.back();
    if (!main || main->data.size() != tilePriority_.size()) return;
    // The priority pass is part of the main layer: when a visual or camera
    // section hides that layer (a substituted backdrop, for instance), its
    // foreground quadrants must not draw either.
    if (!layerVisibleInSections(*main, activeCameraSectionId,
                                activeVisualSectionId, animTick_))
        return;

    const int scrollX = static_cast<int>(cameraX * main->parallaxX) + main->previewOffsetX;
    const int scrollY = static_cast<int>(cameraY * main->parallaxY) + main->previewOffsetY;
    const int startCol = std::max(0, scrollX / tileSize_);
    const int startRow = std::max(0, scrollY / tileSize_);
    const int endCol = std::min(width_, startCol + (INTERNAL_WIDTH / tileSize_) + 2);
    const int endRow = std::min(height_, startRow + (INTERNAL_HEIGHT / tileSize_) + 2);
    const int half = tileSize_ / 2;

    const Texture2D* tex = tilesetForVisualSection(activeVisualSectionId);
    if (!tex) return;
    if (!tilesetFrames_.empty() && tilesetFramePeriod_ > 0) {
        const int phases = static_cast<int>(tilesetFrames_.size());
        const int sub = tilesetFramePeriod_ / phases;
        int fi = (animTick_ % tilesetFramePeriod_) / (sub > 0 ? sub : 1);
        if (fi >= phases) fi = phases - 1;
        if (tilesetFrames_[fi].valid()) tex = &tilesetFrames_[fi].get();
    }

    for (int row = startRow; row < endRow; row++) {
        for (int col = startCol; col < endCol; col++) {
            const int idx = row * width_ + col;
            if (idx < 0 || idx >= static_cast<int>(tilePriority_.size())) continue;
            const uint8_t mask = tilePriority_[idx];
            if (mask == 0) continue;
            const int tileId = main->data[idx];
            if (tileId <= 0) continue;

            const int tx = (tileId % tilesetCols_) * tileSize_;
            const int ty = (tileId / tilesetCols_) * tileSize_;
            const int screenX = col * tileSize_ - scrollX;
            const int screenY = row * tileSize_ - scrollY;

            if (mask == 0x0F) {
                Rectangle src = { (float)tx, (float)ty, (float)tileSize_, (float)tileSize_ };
                Rectangle dst = { (float)screenX, (float)screenY, (float)tileSize_, (float)tileSize_ };
                DrawTexturePro(*tex, src, dst, {0, 0}, 0.0f, WHITE);
                continue;
            }
            // Quadrant order matches the decode: bit0 TL, bit1 TR, bit2 BL, bit3 BR.
            for (int q = 0; q < 4; q++) {
                if (!(mask & (1 << q))) continue;
                const int qx = (q % 2) * half;
                const int qy = (q / 2) * half;
                Rectangle src = { (float)(tx + qx), (float)(ty + qy), (float)half, (float)half };
                Rectangle dst = { (float)(screenX + qx), (float)(screenY + qy), (float)half, (float)half };
                DrawTexturePro(*tex, src, dst, {0, 0}, 0.0f, WHITE);
            }
        }
    }
}

void Tilemap::renderForeground(float cameraX, float cameraY) const {
    renderForeground(cameraX, cameraY, std::string_view{});
}

void Tilemap::renderForeground(float cameraX, float cameraY,
                               std::string_view activeCameraSectionId) const {
    renderForeground(cameraX, cameraY, activeCameraSectionId, std::string_view{});
}

void Tilemap::renderForeground(float cameraX, float cameraY,
                               std::string_view activeCameraSectionId,
                               std::string_view activeVisualSectionId) const {
    for (const auto& layer : layers_) {
        if (layer.drawAfterEntities &&
            layerVisibleInSections(layer, activeCameraSectionId, activeVisualSectionId, animTick_))
            renderLayer(layer, cameraX, cameraY, activeVisualSectionId);
    }
}

std::string Tilemap::visualSectionIdForRect(float x, float y, float w, float h,
                                            bool bossLocked) const {
    if (bossLocked && !bossLockVisualSectionId_.empty()) return bossLockVisualSectionId_;

    const float right = x + w;
    const float bottom = y + h;
    if (visualSectionSelectionMode_ == VisualSectionSelectionMode::FirstOverlap) {
        for (const auto& section : visualSections_) {
            const auto& r = section.rect;
            const bool overlaps = right > r.x && x < r.x + r.w
                && bottom > r.y && y < r.y + r.h;
            if (overlaps) return section.id;
        }
        return defaultVisualSectionId_;
    }

    bool hasBest = false;
    std::string bestId;
    float bestRectArea = 0.0f;
    float bestOverlapArea = 0.0f;
    float bestCenterDistanceSq = 0.0f;
    std::size_t bestIndex = 0;
    const float centerX = x + w * 0.5f;
    const float centerY = y + h * 0.5f;
    constexpr float EPS = 0.001f;

    for (std::size_t index = 0; index < visualSections_.size(); ++index) {
        const auto& section = visualSections_[index];
        const auto& r = section.rect;
        const float overlapW = std::max(0.0f, std::min(right, r.x + r.w) - std::max(x, r.x));
        const float overlapH = std::max(0.0f, std::min(bottom, r.y + r.h) - std::max(y, r.y));
        const float overlapArea = overlapW * overlapH;
        if (overlapArea <= 0.0f) continue;

        const float rectArea = r.w * r.h;
        const float sectionCenterX = r.x + r.w * 0.5f;
        const float sectionCenterY = r.y + r.h * 0.5f;
        const float dx = sectionCenterX - centerX;
        const float dy = sectionCenterY - centerY;
        const float centerDistanceSq = dx * dx + dy * dy;

        const bool better = !hasBest
            || rectArea < bestRectArea - EPS
            || (std::abs(rectArea - bestRectArea) <= EPS
                && overlapArea > bestOverlapArea + EPS)
            || (std::abs(rectArea - bestRectArea) <= EPS
                && std::abs(overlapArea - bestOverlapArea) <= EPS
                && centerDistanceSq < bestCenterDistanceSq - EPS)
            || (std::abs(rectArea - bestRectArea) <= EPS
                && std::abs(overlapArea - bestOverlapArea) <= EPS
                && std::abs(centerDistanceSq - bestCenterDistanceSq) <= EPS
                && index < bestIndex);
        if (!better) continue;

        hasBest = true;
        bestId = section.id;
        bestRectArea = rectArea;
        bestOverlapArea = overlapArea;
        bestCenterDistanceSq = centerDistanceSq;
        bestIndex = index;
    }
    return hasBest ? bestId : defaultVisualSectionId_;
}

void Tilemap::renderLayer(const TileLayer& layer, float cameraX, float cameraY) const {
    renderLayer(layer, cameraX, cameraY, std::string_view{});
}

void Tilemap::renderLayer(const TileLayer& layer, float cameraX, float cameraY,
                          std::string_view activeVisualSectionId) const {
    // Floor scroll to integer pixels. Fractional src.x/y combined with the
    // 4x display upscale (256x224 -> 1024x896) smears edges even under
    // TEXTURE_FILTER_POINT because the sampler rounds per-destination-pixel,
    // not per-source-pixel.
    int scrollX = static_cast<int>(cameraX * layer.parallaxX) + layer.previewOffsetX;
    int scrollY = static_cast<int>(cameraY * layer.parallaxY) + layer.previewOffsetY;

    // Fast path: if this layer has its own pre-rendered image, blit it cropped
    // to the viewport with parallax applied. Each layer carries its own
    // texture so BG and FG can scroll independently.
    if (layer.previewTex.valid()) {
        const int texW = layer.previewTex.width();
        const int texH = layer.previewTex.height();
        if (texW <= 0 || texH <= 0) return;

        const int firstSrcX = layer.repeatPreviewX ? positiveModulo(scrollX, texW)
                                                   : std::max(0, scrollX);
        const int firstSrcY = layer.repeatPreviewY ? positiveModulo(scrollY, texH)
                                                   : std::max(0, scrollY);

        // Dst offset matches any negative non-repeating scroll so the layer
        // anchors correctly when the camera is near (0,0). Repeating layers
        // always fill from the viewport origin.
        const int firstDstX = (!layer.repeatPreviewX && scrollX < 0) ? -scrollX : 0;
        const int firstDstY = (!layer.repeatPreviewY && scrollY < 0) ? -scrollY : 0;

        int dstY = firstDstY;
        int srcY = firstSrcY;
        while (dstY < INTERNAL_HEIGHT) {
            int srcH = std::min(texH - srcY, INTERNAL_HEIGHT - dstY);
            if (srcH <= 0) return;

            int dstX = firstDstX;
            int srcX = firstSrcX;
            while (dstX < INTERNAL_WIDTH) {
                int srcW = std::min(texW - srcX, INTERNAL_WIDTH - dstX);
                if (srcW <= 0) break;

                Rectangle src = { (float)srcX, (float)srcY, (float)srcW, (float)srcH };
                Rectangle dst = { (float)dstX, (float)dstY, (float)srcW, (float)srcH };
                DrawTexturePro(layer.previewTex.get(), src, dst, {0, 0}, 0.0f, WHITE);

                if (!layer.repeatPreviewX) break;
                dstX += srcW;
                srcX = 0;
            }

            if (!layer.repeatPreviewY) break;
            dstY += srcH;
            srcY = 0;
        }
        return;
    }

    // Per-tile rendering (fallback when no preview texture)
    int startCol = std::max(0, static_cast<int>(scrollX) / tileSize_);
    int startRow = std::max(0, static_cast<int>(scrollY) / tileSize_);
    int endCol = std::min(width_, startCol + (INTERNAL_WIDTH / tileSize_) + 2);
    int endRow = std::min(height_, startRow + (INTERNAL_HEIGHT / tileSize_) + 2);

    for (int row = startRow; row < endRow; row++) {
        for (int col = startCol; col < endCol; col++) {
            int idx = row * width_ + col;
            if (idx < 0 || idx >= static_cast<int>(layer.data.size())) continue;

            int tileId = layer.data[idx];

            int screenX = col * tileSize_ - static_cast<int>(scrollX);
            int screenY = row * tileSize_ - static_cast<int>(scrollY);

            if (useDirectTiles_ && tileset_.valid() && tilesetCols_ > 0) {
                // Direct 16x16 tile mode (webp-decomposed stages)
                // Tile ID -1 = empty, 0+ = valid tile in atlas
                if (tileId < 0) continue;
                int tx = (tileId % tilesetCols_) * tileSize_;
                int ty = (tileId / tilesetCols_) * tileSize_;
                Rectangle src = { (float)tx, (float)ty, (float)tileSize_, (float)tileSize_ };
                Rectangle dst = { (float)screenX, (float)screenY, (float)tileSize_, (float)tileSize_ };
                // U63: palette-cycle animation = whole-atlas frame swap on the
                // measured cadence (FM belts: 4 phases / period 40). A visual
                // section's own atlas (R3.storm-eagle.sections) wins over the
                // stage's, and the phase frames still override both.
                const Texture2D* tex = tilesetForVisualSection(activeVisualSectionId);
                if (!tex) continue;
                if (!tilesetFrames_.empty() && tilesetFramePeriod_ > 0) {
                    int phases = static_cast<int>(tilesetFrames_.size());
                    int sub = tilesetFramePeriod_ / phases;
                    int fi = (animTick_ % tilesetFramePeriod_) / (sub > 0 ? sub : 1);
                    if (fi >= phases) fi = phases - 1;
                    if (tilesetFrames_[fi].valid()) tex = &tilesetFrames_[fi].get();
                }
                DrawTexturePro(*tex, src, dst, {0, 0}, 0.0f, WHITE);
            } else if (tileset_.valid() && tileId > 0 && tileId < static_cast<int>(metatiles_.size())) {
                // Metatile mode (ROM-ripped stages)
                const auto& metatile = metatiles_[tileId];
                int tw = tileset_.width() / 8;
                if (tw == 0) tw = 1;

                for (int i = 0; i < 4; i++) {
                    int tidx = metatile.tiles[i];
                    int tx = (tidx % tw) * 8;
                    int ty = (tidx / tw) * 8;

                    Rectangle src = { (float)tx, (float)ty, 8, 8 };
                    Rectangle dst = { (float)(screenX + (i % 2) * 8), (float)(screenY + (i / 2) * 8), 8, 8 };
                    DrawTexturePro(tileset_.get(), src, dst, {0, 0}, 0.0f, WHITE);
                }
            } else if (tileId != 0) {
                Color color = getTileColor(tileId);
                DrawRectangle(screenX, screenY, tileSize_, tileSize_, color);
            }
        }
    }
}

void Tilemap::renderOverlays(float cameraX, float cameraY,
                             std::string_view activeVisualSectionId) const {
    if (overlays_.empty() && regionOverlays_.empty()) return;

    int scrollX = static_cast<int>(cameraX);
    int scrollY = static_cast<int>(cameraY);

    if (!overlays_.empty()) {
        int startCol = std::max(0, scrollX / tileSize_);
        int startRow = std::max(0, scrollY / tileSize_);
        int endCol = std::min(width_, startCol + (INTERNAL_WIDTH / tileSize_) + 2);
        int endRow = std::min(height_, startRow + (INTERNAL_HEIGHT / tileSize_) + 2);

        for (int row = startRow; row < endRow; row++) {
            for (int col = startCol; col < endCol; col++) {
                int idx = row * width_ + col;
                auto it = overlays_.find(idx);
                if (it == overlays_.end()) continue;
                int screenX = col * tileSize_ - scrollX;
                int screenY = row * tileSize_ - scrollY;
                DrawRectangle(screenX, screenY, tileSize_, tileSize_, it->second);
            }
        }
    }

    for (const auto& overlay : regionOverlays_) {
        if (!idListAllows(overlay.visualSectionIds, activeVisualSectionId)) continue;
        const float screenX = overlay.x - static_cast<float>(scrollX);
        const float screenY = overlay.y - static_cast<float>(scrollY);
        const float left = std::max(0.0f, screenX);
        const float top = std::max(0.0f, screenY);
        const float right = std::min(static_cast<float>(INTERNAL_WIDTH), screenX + overlay.w);
        const float bottom = std::min(static_cast<float>(INTERNAL_HEIGHT), screenY + overlay.h);
        if (right <= left || bottom <= top) continue;
        Rectangle screenRect{left, top, right - left, bottom - top};
        DrawRectangleRec(screenRect, overlay.color);
    }
}

void Tilemap::renderWater(float /*cameraX*/, float cameraY) const {
    if (!waterEnabled_) return;

    // Water fills the full screen width, so only the vertical scroll matters.
    int scrollY = static_cast<int>(cameraY);
    int surfaceScreenY = static_cast<int>(waterLevel_) - scrollY;

    if (surfaceScreenY >= INTERNAL_HEIGHT) return;

    int drawY = std::max(0, surfaceScreenY);
    int drawH = INTERNAL_HEIGHT - drawY;
    if (drawH <= 0) return;

    // Main water body
    DrawRectangle(0, drawY, INTERNAL_WIDTH, drawH, waterColor_);

    // Surface highlight line with subtle wave
    if (surfaceScreenY >= 0 && surfaceScreenY < INTERNAL_HEIGHT) {
        Color surfaceColor = {
            static_cast<unsigned char>(std::min(255, waterColor_.r + 60)),
            static_cast<unsigned char>(std::min(255, waterColor_.g + 60)),
            static_cast<unsigned char>(std::min(255, waterColor_.b + 40)),
            static_cast<unsigned char>(std::min(255, waterColor_.a + 40))
        };
        DrawRectangle(0, surfaceScreenY, INTERNAL_WIDTH, 2, surfaceColor);
    }
}

void Tilemap::renderCollisionDebug(float cameraX, float cameraY) const {
    int startCol = std::max(0, static_cast<int>(cameraX) / tileSize_);
    int startRow = std::max(0, static_cast<int>(cameraY) / tileSize_);
    int endCol = std::min(width_, startCol + (INTERNAL_WIDTH / tileSize_) + 2);
    int endRow = std::min(height_, startRow + (INTERNAL_HEIGHT / tileSize_) + 2);

    for (int row = startRow; row < endRow; row++) {
        for (int col = startCol; col < endCol; col++) {
            int idx = row * width_ + col;
            if (idx < 0 || idx >= static_cast<int>(collision_.size())) continue;

            TileType type = collision_[idx];
            if (type == TileType::None) continue;

            int screenX = col * tileSize_ - static_cast<int>(cameraX);
            int screenY = row * tileSize_ - static_cast<int>(cameraY);

            switch (type) {
                case TileType::Solid:
                    // Red outline for solid tiles
                    DrawRectangleLines(screenX, screenY, tileSize_, tileSize_,
                                       {255, 60, 60, 120});
                    break;
                case TileType::OneWay:
                    // Yellow line at top edge — passable from below, solid from above
                    DrawLine(screenX, screenY, screenX + tileSize_, screenY,
                             {255, 255, 0, 180});
                    DrawLine(screenX, screenY + 1, screenX + tileSize_, screenY + 1,
                             {255, 255, 0, 120});
                    break;
                case TileType::Spike:
                    // Spike triangles pointing up (danger!)
                    DrawTriangle(
                        {static_cast<float>(screenX + tileSize_ / 2), static_cast<float>(screenY)},
                        {static_cast<float>(screenX + tileSize_), static_cast<float>(screenY + tileSize_)},
                        {static_cast<float>(screenX), static_cast<float>(screenY + tileSize_)},
                        {255, 0, 0, 80}
                    );
                    DrawRectangleLines(screenX, screenY, tileSize_, tileSize_,
                                       {255, 0, 0, 120});
                    break;
                case TileType::Breakable:
                    // Cracked pattern — orange/brown with X marking
                    DrawRectangle(screenX, screenY, tileSize_, tileSize_,
                                   {180, 120, 60, 80});
                    DrawLine(screenX, screenY, screenX + tileSize_, screenY + tileSize_,
                             {255, 180, 80, 150});
                    DrawLine(screenX + tileSize_, screenY, screenX, screenY + tileSize_,
                             {255, 180, 80, 150});
                    break;
                case TileType::SlopeL:
                case TileType::SlopeR: {
                    // Cyan hypotenuse along the actual slope surface for debug clarity
                    SlopeHeight h = getSlope(col, row);
                    DrawLine(screenX, screenY + h.leftY,
                             screenX + tileSize_, screenY + h.rightY,
                             {100, 220, 255, 220});
                    break;
                }
                default:
                    break;
            }
        }
    }
}

TileType Tilemap::getTileType(int tileX, int tileY) const {
    if (tileX < 0 || tileX >= width_ || tileY < 0 || tileY >= height_) {
        return TileType::None; // Out of bounds = no collision
    }
    int idx = tileY * width_ + tileX;
    if (idx < 0 || idx >= static_cast<int>(collision_.size())) {
        return TileType::None;
    }
    return collision_[idx];
}

TileType Tilemap::getTileTypeAtPixel(float worldX, float worldY) const {
    if (tileSize_ <= 0) return TileType::None;
    int tileX = static_cast<int>(std::floor(worldX / static_cast<float>(tileSize_)));
    int tileY = static_cast<int>(std::floor(worldY / static_cast<float>(tileSize_)));
    return getTileType(tileX, tileY);
}

int Tilemap::getRawAttr(int tileX, int tileY) const {
    if (tileX < 0 || tileX >= width_ || tileY < 0 || tileY >= height_) {
        return 0;
    }
    if (attrs_.empty() || layers_.empty()) {
        return 0;
    }

    const TileLayer* main = nullptr;
    for (const auto& layer : layers_) {
        if (layer.name == "main") {
            main = &layer;
            break;
        }
    }
    if (!main) main = &layers_.back();

    int idx = tileY * width_ + tileX;
    if (idx < 0 || idx >= static_cast<int>(main->data.size())) {
        return 0;
    }

    int blockId = main->data[idx];
    if (blockId < 0 || blockId >= static_cast<int>(attrs_.size())) {
        return 0;
    }
    return attrs_[blockId];
}

int Tilemap::getRawAttrAtPixel(float worldX, float worldY) const {
    int tileX = static_cast<int>(std::floor(worldX / static_cast<float>(tileSize_)));
    int tileY = static_cast<int>(std::floor(worldY / static_cast<float>(tileSize_)));
    return getRawAttr(tileX, tileY);
}

bool Tilemap::isSolid(int tileX, int tileY) const {
    TileType type = getTileType(tileX, tileY);
    return isFullTileBlock(type);
}

SlopeHeight Tilemap::getSlope(int tileX, int tileY) const {
    if (tileX < 0 || tileX >= width_ || tileY < 0 || tileY >= height_) return {16, 16};
    auto it = slopeData_.find(tileY * width_ + tileX);
    if (it == slopeData_.end()) return {16, 16};
    return it->second;
}

bool Tilemap::hasSlope(int tileX, int tileY) const {
    if (tileX < 0 || tileX >= width_ || tileY < 0 || tileY >= height_) return false;
    return slopeData_.count(tileY * width_ + tileX) > 0;
}

void Tilemap::setSlope(int tileX, int tileY, SlopeHeight h) {
    if (tileX < 0 || tileX >= width_ || tileY < 0 || tileY >= height_) return;
    slopeData_[tileY * width_ + tileX] = h;
}

bool Tilemap::breakTile(int tileX, int tileY) {
    if (tileX < 0 || tileX >= width_ || tileY < 0 || tileY >= height_) return false;
    int idx = tileY * width_ + tileX;
    if (idx < 0 || idx >= static_cast<int>(collision_.size())) return false;
    if (collision_[idx] != TileType::Breakable) return false;

    // Clear collision
    collision_[idx] = TileType::None;

    // Clear visual tile in all layers
    for (auto& layer : layers_) {
        if (idx < static_cast<int>(layer.data.size())) {
            layer.data[idx] = 0;
        }
    }
    return true;
}

void Tilemap::setTile(int layerIdx, int tileX, int tileY, int tileId) {
    if (layerIdx < 0 || layerIdx >= static_cast<int>(layers_.size())) return;
    if (tileX < 0 || tileX >= width_ || tileY < 0 || tileY >= height_) return;
    int idx = tileY * width_ + tileX;
    auto& lyr = layers_[layerIdx];
    if (idx < static_cast<int>(lyr.data.size())) {
        lyr.data[idx] = tileId;
    }
    // Invalidate this layer's cached preview (editor edits make it stale).
    lyr.previewTex.reset();
}

void Tilemap::setCollision(int tileX, int tileY, TileType type) {
    if (tileX < 0 || tileX >= width_ || tileY < 0 || tileY >= height_) return;
    int idx = tileY * width_ + tileX;
    if (idx < static_cast<int>(collision_.size())) {
        collision_[idx] = type;
    }
}

Color Tilemap::getTileColor(int tileId) {
    // Placeholder palette until real tilesets exist.
    // Each tile ID maps to a distinct color so the level layout is visible.
    switch (tileId) {
        case 0:  return BLANK;                      // Empty
        case 1:  return {70, 70, 85, 255};          // Dark blue-gray (ground)
        case 2:  return {90, 80, 65, 255};          // Brown (platform)
        case 3:  return {45, 45, 60, 255};          // Darker blue-gray (bg decoration)
        case 4:  return {180, 50, 50, 255};          // Red-brown (spikes)
        case 5:  return {55, 75, 55, 255};          // Dark green (vegetation)
        case 10: return {40, 40, 55, 255};          // Deep dark (background fill)
        default: return {80, 80, 90, 255};          // Default gray
    }
}

} // namespace mmx
