// tilemap.h - declares tilemap data, collision types, layers, and regions.
// Owns: tile metadata, spawns, camera sections, checkpoints, and map API.

#pragma once

#include "app/constants.h"
#include "systems/raylib_resource.h"
#include "raylib.h"
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <optional>

// ============================================================================
// tilemap.h â€” Tile-based stage system
//
// Stages are grids of tiles with multiple visual layers and a collision layer.
// Each visual layer can scroll at different rates (parallax) for depth effect.
// The collision layer determines what the player can stand on, pass through,
// or interact with â€” it's separate from visuals so art and gameplay are
// independently tunable.
//
// Rendering supports two modes:
//   1. Color rectangles (placeholder) â€” tile ID maps to a color
//   2. Metatile textures â€” tile ID indexes into a metatile array, each metatile
//      is 4 sub-tiles (2x2) from a tileset spritesheet (8x8 pixels each)
//   3. Direct tiles â€” tile ID indexes directly into a 16x16 tileset atlas
//      (no metatile indirection, used by webp-decomposed stages)
//
// Coordinate systems:
//   Tile coords:  (tileX, tileY) â€” integer grid position
//   Pixel coords: (worldX, worldY) â€” world space in pixels
//   Convert: tileX = worldX / tileSize, worldX = tileX * tileSize
// ============================================================================

namespace mmx {

// Collision tile types â€” what happens when the player touches this tile
enum class TileType : uint8_t {
    None      = 0,  // Empty space â€” no collision
    Solid     = 1,  // Full solid block â€” stops movement from all directions
    OneWay    = 2,  // Platform â€” solid from above, passable from below/sides
    SlopeL    = 3,  // Slope with rising-left surface (walking left climbs up)
    SlopeR    = 4,  // Slope with rising-right surface (walking right climbs up)
    Spike     = 5,  // Instant kill on contact
    Ladder    = 6,  // Climbable area (future)
    Breakable = 7,  // Breakable ceiling â€” destroyed by helmet headbutt
    Conveyor  = 8   // Belt surface â€” solid + horizontal push (U62; attrs 0x36-0x38)
};

inline std::optional<TileType> tileTypeForAttrMapName(std::string_view name) {
    if (name == "none") return TileType::None;
    if (name == "solid") return TileType::Solid;
    if (name == "oneway") return TileType::OneWay;
    if (name == "slopeL") return TileType::SlopeL;
    if (name == "slopeR") return TileType::SlopeR;
    if (name == "spike") return TileType::Spike;
    if (name == "ladder") return TileType::Ladder;
    if (name == "breakable") return TileType::Breakable;
    if (name == "conveyor") return TileType::Conveyor;
    return std::nullopt;
}

inline bool isSlope(TileType type) {
    return type == TileType::SlopeL || type == TileType::SlopeR;
}

inline bool isHazard(TileType type) {
    return type == TileType::Spike;
}

// Full collision volumes block side/head overlap. Surface-only tiles such as
// one-way platforms, slopes, and conveyors are handled by specialized surface code.
inline bool isFullTileBlock(TileType type) {
    return type == TileType::Solid || type == TileType::Breakable || isHazard(type);
}

inline bool isStandable(TileType type) {
    return type == TileType::Solid || type == TileType::OneWay ||
           type == TileType::Breakable || type == TileType::Conveyor ||
           isSlope(type);
}

inline bool supportsProjectile(TileType type) {
    return isStandable(type) || isHazard(type);
}

inline bool anchorsFireWaveSegment(TileType type) {
    return type == TileType::Solid || type == TileType::Breakable || isSlope(type);
}

// Per-tile surface heights for slope tiles. leftY and rightY are the surface Y
// offset from the tile's top edge in pixels (0 = surface at top of tile, ts = bottom).
// The walkable surface interpolates linearly between these across the tile's width,
// letting a single slope type express 1:1, 1:2, 1:4, etc. grades.
struct SlopeHeight {
    uint8_t leftY  = 16;   // Surface Y at tile's left edge, from tile top
    uint8_t rightY = 16;   // Surface Y at tile's right edge, from tile top
};

struct DecodedMainBlockReplacement {
    int tileX;
    int tileY;
    int expectedBlockId;
    int replacementBlockId;
    // The loader permits baked collision on attr0 cells. Such a source-proved
    // cell requires its exact current type here; other cells derive the guard.
    std::optional<TileType> expectedBakedCollision = std::nullopt;
};

// One visual layer of tiles
struct TileLayer {
    std::string name;
    float parallaxX = 1.0f;  // Scroll rate relative to camera (1.0 = same speed)
    float parallaxY = 1.0f;  // Values < 1.0 scroll slower (appears farther away)
    // Vertical scroll the layer performs on its own, in PIXELS PER ANIMATION
    // TICK, independent of the camera. Storm Eagle's BG2 backdrop measures
    // -0.5 px/frame outside the deck and -8 px/frame from camera x 5779 on
    // (knowledge_base/mmx1/stages/storm-eagle/bg2_autoscroll.json). The
    // accumulated offset is floored to whole pixels, which reproduces the
    // measured "alternating 0 and -1" shape of the slow rate.
    float autoScrollY = 0.0f;
    std::vector<int> data;   // Tile IDs, row-major: index = y * width + x

    // Optional pre-rendered layer image. When set, renderLayer blits this
    // texture cropped to the viewport (with parallax applied) instead of
    // drawing per-tile. Used by ROM-ripped stages where we have clean
    // bg1_only.png / bg2_only.png artifacts.
    std::string previewPath;
    TextureResource previewTex;
    int previewOffsetX = 0;      // Source-space offset before cropping preview
    int previewOffsetY = 0;
    bool repeatPreviewX = false;
    bool repeatPreviewY = false;
    // Optional section filter for generated section-composition layers.
    // Empty means global; nonempty means render only while the active camera
    // section id matches one of these ids.
    std::vector<std::string> cameraSectionIds;
    // Optional visual-section filter selected independently from camera locks.
    std::vector<std::string> visualSectionIds;
    // Optional visual phase filter. When visualPhaseIds is nonempty, the layer
    // renders only when animTick % visualPhasePeriod is one of these ids.
    int visualPhasePeriod = 0;
    std::vector<int> visualPhaseIds;
    // When true, this layer is skipped in render() and drawn by renderForeground()
    // (called after entities). Used for FG occlusion layers (e.g. snow over X's feet).
    bool drawAfterEntities = false;
};

struct TileLayerRenderDiagnostic {
    std::string name;
    bool drawAfterEntities = false;
    bool visible = false;
    bool cameraSectionAllowed = false;
    bool visualSectionAllowed = false;
    bool phaseAllowed = false;
    int visualPhaseTick = 0;
    int activePhase = 0;
    int visualPhasePeriod = 0;
    std::vector<int> visualPhaseIds;
    std::vector<std::string> cameraSectionIds;
    std::vector<std::string> visualSectionIds;
    float parallaxX = 1.0f;
    float parallaxY = 1.0f;
    float autoScrollY = 0.0f;
    int previewOffsetX = 0;
    int previewOffsetY = 0;
    int scrollX = 0;
    int scrollY = 0;
    std::string previewPath;
    int previewWidth = 0;
    int previewHeight = 0;
    bool repeatPreviewX = false;
    bool repeatPreviewY = false;
};
enum class SpawnActivation : uint8_t {
    CameraWindow,
    SourceHorizontalCameraBucket,
};

// Entity/item spawn point defined in the stage
struct SpawnPoint {
    std::string type;  // "player_spawn", "enemy", "pickup", "boss", "checkpoint"
    std::string id;    // For enemies: which enemy type (e.g., "met"); for bosses: boss ID
    float x, y;        // World position in pixels
    // Provisional census placeholders (ENDGAME waiver W2): the source OID the
    // spawn stands in for, its measured HP from spawn_census.json, and the
    // provisional flag (MMX_SKIP_PROVISIONAL=1 hides such spawns so they
    // never enter a parity number).
    std::string oid;
    int hp = 0;
    bool provisional = false;
    SpawnActivation activation = SpawnActivation::CameraWindow;
};

struct RegionOverlay {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    Color color = {0, 0, 0, 0};
    std::vector<std::string> visualSectionIds;
};

// 2x2 block of 8x8 sub-tiles from a tileset spritesheet.
// Each metatile represents one 16x16 game tile.
struct Metatile {
    int tiles[4]; // [top-left, top-right, bottom-left, bottom-right] indices into tileset
};

// Named room rectangle for camera lock-in (boss arenas)
struct Room {
    std::string name;
    float x, y, w, h;
    // Optional activation rectangle for a named (boss) room. Without it the
    // room activates once the player's x reaches the room's x (Chill Penguin's
    // one-screen arena). Storm Eagle's fight has no camera lock in x, so its
    // room spans the stage and activates only with X standing on the deck
    // (knowledge_base/mmx1/bosses/storm-eagle/arena.json, plan task C4).
    bool hasTrigger = false;
    float tx = 0.0f, ty = 0.0f, tw = 0.0f, th = 0.0f;

    bool triggeredBy(float px, float py) const {
        if (!hasTrigger) return px >= x;
        return px >= tx && px < tx + tw && py >= ty && py < ty + th;
    }
};

enum class CameraSectionMode : uint8_t {
    Lock,
    Follow,
};

struct CameraSectionRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct CameraSection {
    std::string id;
    CameraSectionRect rect;
    CameraSectionMode mode = CameraSectionMode::Lock;
    float lockCamY = 0.0f;
    float deadzoneTop = 0.0f;
    float deadzoneBottom = 0.0f;
    float lagPxPerFrame = 0.0f;
};

struct VisualSection {
    std::string id;
    CameraSectionRect rect;
    // Optional per-section block atlas. The SNES reloads CGRAM as the stage
    // scrolls, so a stage decoded from one capture carries one region's
    // colours everywhere; a section that names its own atlas draws the main
    // layer (and its priority pass) from that texture instead. Measured for
    // Storm Eagle by plan task R3.storm-eagle.sections.
    std::string tilesetPath;
};

enum class VisualSectionSelectionMode : uint8_t {
    FirstOverlap,
    SmallestRectNearestCenter,
};

class Tilemap {
public:
    Tilemap() = default;
    ~Tilemap();
    Tilemap(const Tilemap&) = delete;
    Tilemap& operator=(const Tilemap&) = delete;
    Tilemap(Tilemap&&) = delete;
    Tilemap& operator=(Tilemap&&) = delete;

    // Load stage data from a JSON file. Returns false on failure.
    bool loadFromFile(const std::string& path);

    // Apply stage variants based on which stages have been completed.
    // Reads "variants" from the parsed JSON and patches collision/slopes,
    // overlays/water, and visual layer/tileset swaps for any variant whose
    // condition is met (e.g. boss defeated).
    void applyVariants(const std::string& path);

    // Render all visual layers (bg -> main -> fg order), skipping any with
    // drawAfterEntities=true. Camera position determines visible region.
    void render(float cameraX, float cameraY) const;
    void render(float cameraX, float cameraY, std::string_view activeCameraSectionId) const;
    void render(float cameraX, float cameraY, std::string_view activeCameraSectionId,
                std::string_view activeVisualSectionId) const;
    void renderLayer(const TileLayer& layer, float cameraX, float cameraY) const;
    void renderLayer(const TileLayer& layer, float cameraX, float cameraY,
                     std::string_view activeVisualSectionId) const;
    // The block atlas the active visual section names, or the stage's own.
    const Texture2D* tilesetForVisualSection(std::string_view activeVisualSectionId) const;
    // B10: the SNES BG tile priority bit. The stage's `priority` array holds
    // one nibble per grid cell (bit0 TL, bit1 TR, bit2 BL, bit3 BR of the
    // block's four 8x8 tilemap words); a set quadrant is drawn IN FRONT of the
    // sprites. Call this after the player, enemies and projectiles: it
    // re-draws exactly those quadrants of the main layer over them. Re-drawing
    // (rather than skipping them in the background pass) is pixel-identical,
    // because SNES 4bpp tiles are opaque or fully transparent, and it leaves
    // the background pass untouched.
    void renderTilePriorityForeground(float cameraX, float cameraY,
                                      std::string_view activeCameraSectionId,
                                      std::string_view activeVisualSectionId) const;
    const std::vector<uint8_t>& tilePriority() const { return tilePriority_; }

    // Render only layers with drawAfterEntities=true. Call after entities/player.
    void renderForeground(float cameraX, float cameraY) const;
    void renderForeground(float cameraX, float cameraY, std::string_view activeCameraSectionId) const;
    void renderForeground(float cameraX, float cameraY, std::string_view activeCameraSectionId,
                          std::string_view activeVisualSectionId) const;
    static bool layerVisibleInCameraSection(const TileLayer& layer,
                                            std::string_view activeCameraSectionId);
    static bool layerVisibleInSections(const TileLayer& layer,
                                       std::string_view activeCameraSectionId,
                                       std::string_view activeVisualSectionId);
    static bool layerVisibleInSections(const TileLayer& layer,
                                       std::string_view activeCameraSectionId,
                                       std::string_view activeVisualSectionId,
                                       int visualPhaseTick);

    // T1.6b autotest-only knob (`MMX_AUTOTEST_BG2_PHASE_Y`, read in main.cpp
    // next to MMX_AUTOTEST_RENDER_CAMERA_X/Y). A movie anchor forces the
    // frame's PPU camera but not the frame's BG2 phase, so a layer that
    // scrolls on its own lands at whatever offset the harness's animTick has
    // reached. When this is set, every layer whose `autoScrollY` is nonzero
    // uses it as its whole-pixel vertical offset instead of the accumulated
    // rate; layers with `autoScrollY == 0` are unaffected. No gameplay path
    // sets it.
    static void setAutoScrollPhaseOverrideY(std::optional<int> phase);
    static std::optional<int> autoScrollPhaseOverrideY();

    std::vector<TileLayerRenderDiagnostic> collectRenderLayerDiagnostics(
        float cameraX, float cameraY,
        std::string_view activeCameraSectionId,
        std::string_view activeVisualSectionId) const;
    // Render semi-transparent color overlays on specific tiles (e.g. ice tint,
    // darkened sections). Applied by stage variants â€” call after render().
    void renderOverlays(float cameraX, float cameraY,
                        std::string_view activeVisualSectionId = {}) const;

    // Render a semi-transparent water surface from waterLevel_ to stage bottom.
    // Called after renderOverlays(). Water config is loaded from stage variants.
    void renderWater(float cameraX, float cameraY) const;
    bool hasWater() const { return waterEnabled_; }
    float waterLevel() const { return waterLevel_; }
    bool hasOverlays() const { return !overlays_.empty() || !regionOverlays_.empty(); }

    // Draw collision tile types as colored overlays (for debugging)
    void renderCollisionDebug(float cameraX, float cameraY) const;

    // --- Collision queries ---
    TileType getTileType(int tileX, int tileY) const;
    TileType getTileTypeAtPixel(float worldX, float worldY) const;
    int getRawAttr(int tileX, int tileY) const;
    int getRawAttrAtPixel(float worldX, float worldY) const;
    bool isSolid(int tileX, int tileY) const; // isFullTileBlock(getTileType(...))

    // Slope surface heights for SlopeL / SlopeR tiles. Defaults to {16,16} when
    // no slope data is stored â€” treat that as a flat floor at tile bottom so
    // anything not explicitly mapped is still walkable without snapping.
    SlopeHeight getSlope(int tileX, int tileY) const;
    bool hasSlope(int tileX, int tileY) const;
    void setSlope(int tileX, int tileY, SlopeHeight h);

    // Break a breakable tile (set to None, clear visual layer)
    bool breakTile(int tileX, int tileY);

    // Atomically replace one decoded-stage main-layer block and recompute its
    // collision/slope semantics from the loaded raw attribute table. The
    // expected block guards source-proved terrain transactions against stale
    // or misidentified cells; every validation failure is a no-op.
    bool replaceDecodedMainBlock(int tileX, int tileY,
                                 int expectedBlockId, int replacementBlockId);

    // Validate the entire batch before changing any cell. Duplicate positions,
    // empty batches and stale block/collision/slope state are rejected as no-ops.
    bool replaceDecodedMainBlocks(
        const std::vector<DecodedMainBlockReplacement>& replacements);

    // --- Dimensions ---
    int width() const { return width_; }       // Stage width in tiles
    int height() const { return height_; }     // Stage height in tiles
    int tileSize() const { return tileSize_; }
    int pixelWidth() const { return width_ * tileSize_; }
    int pixelHeight() const { return height_ * tileSize_; }
    std::optional<float> pitDeathY() const { return pitDeathY_; }

    const std::vector<SpawnPoint>& spawns() const { return spawns_; }
    void replaceSpawns(std::vector<SpawnPoint> spawns);
    const std::vector<Room>& rooms() const { return rooms_; }
    const std::vector<CameraSection>& cameraSections() const { return cameraSections_; }
    const std::vector<VisualSection>& visualSections() const { return visualSections_; }
    const std::string& defaultVisualSectionId() const { return defaultVisualSectionId_; }
    const std::string& bossLockVisualSectionId() const { return bossLockVisualSectionId_; }
    VisualSectionSelectionMode visualSectionSelectionMode() const { return visualSectionSelectionMode_; }
    std::string visualSectionIdForRect(float x, float y, float w, float h,
                                       bool bossLocked) const;
    Color backgroundColor() const { return bgColor_; }

    void addMetatile(const Metatile& m) { metatiles_.push_back(m); }

    // Editor support â€” direct data access for map editor
    bool isDirectTileMode() const { return useDirectTiles_; }
    int tilesetCols() const { return tilesetCols_; }
    const Texture2D& tileset() const { return tileset_.get(); }
    std::vector<TileLayer>& layersMut() { return layers_; }
    const std::vector<TileLayer>& layers() const { return layers_; }
    std::vector<TileType>& collisionMut() { return collision_; }
    const std::vector<TileType>& collision() const { return collision_; }
    void setTile(int layerIdx, int tileX, int tileY, int tileId);
    void setCollision(int tileX, int tileY, TileType type);
    int mainTileId(int tileX, int tileY) const;
    bool replaceDirectMainTile(int tileX, int tileY,
                               int expectedBlockId, int replacementBlockId,
                               TileType replacementCollision);
    void saveToFile(const std::string& path) const;

    // U59: raw per-blockId attribute bytes from the game's own table (decoded
    // stages). Empty for hand-made/legacy stages. attrs_[blockId] = raw byte.
    const std::vector<int>& attrs() const { return attrs_; }

    // U63: palette-cycle tileset animation — call once per gameplay update.
    void tickAnimation() { animTick_++; }
    const std::vector<std::string>& tilesetFramePaths() const { return tilesetFramePaths_; }
    int tilesetFramePeriod() const { return tilesetFramePeriod_; }

private:
    // Autotest-only auto-scroll phase override (see setAutoScrollPhaseOverrideY).
    static std::optional<int> autoScrollPhaseOverrideY_;

    std::string stageName_;
    std::string stageSource_;

    int width_ = 0;
    int height_ = 0;
    int tileSize_ = TILE_SIZE;
    std::optional<float> pitDeathY_;
    Color bgColor_ = BLACK;

    bool useDirectTiles_ = false;  // true = flat 16x16 tile IDs (webp), false = metatile mode
    int tilesetCols_ = 0;         // columns in tileset atlas (for direct tile mode)

    std::string tilesetPath_;
    TextureResource tileset_;            // Spritesheet for metatile rendering
    std::vector<Metatile> metatiles_;     // Metatile definitions from stage JSON
    std::vector<TileLayer> layers_;
    // B10: per-cell BG tile priority nibbles, parallel to the main layer's
    // data. Empty when the stage carries no `priority` array (all behind).
    std::vector<uint8_t> tilePriority_;
    std::vector<TileType> collision_;
    std::unordered_map<int, SlopeHeight> slopeData_;  // key = row * width + col
    std::vector<SpawnPoint> spawns_;
    std::vector<Room> rooms_;
    std::vector<CameraSection> cameraSections_;
    std::vector<VisualSection> visualSections_;
    // id -> the atlas that section's `tilesetPath` resolved to (only for the
    // sections that name one).
    std::unordered_map<std::string, TextureResource> sectionTilesets_;
    std::string defaultVisualSectionId_;
    std::string bossLockVisualSectionId_;
    VisualSectionSelectionMode visualSectionSelectionMode_ = VisualSectionSelectionMode::FirstOverlap;
    std::unordered_map<int, Color> overlays_;  // Per-tile color overlays from variants
    std::vector<RegionOverlay> regionOverlays_;

    // Water rendering â€” enabled by stage variants (e.g. Sting Chameleon after Launch Octopus)
    bool waterEnabled_ = false;
    float waterLevel_ = 0.0f;    // World Y where water surface starts (pixels from top)
    Color waterColor_ = {30, 80, 180, 90};  // Semi-transparent blue
    std::string collisionPatchesJsonDump_;
    std::string collisionPatchRunsJsonDump_;
    std::string variantsJsonDump_;
    // U59 raw-attr layer (decoded stages) + not-yet-editable passthrough keys
    // (the variantsJsonDump_ pattern: preserved verbatim so editor saves can't
    // strip them).
    std::vector<int> attrs_;
    std::unordered_map<int, TileType> attrOverrides_;
    bool attrOverridesFirst_ = false;
    std::string attrMapJsonDump_;
    std::vector<TextureResource> tilesetFrames_;   // U63 per-phase atlases
    std::vector<std::string> tilesetFramePaths_;
    int tilesetFramePeriod_ = 0;
    int animTick_ = 0;
    std::string animationsJsonDump_;
    std::string cameraSectionsJsonDump_;
    std::string visualSectionsJsonDump_;

    // Map tile IDs to colors (placeholder until real tilesets are loaded)
    static Color getTileColor(int tileId);
    void clearLoadedData();
    void commitLoadedDataFrom(Tilemap& loaded);
};

} // namespace mmx
