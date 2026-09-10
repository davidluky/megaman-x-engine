// map_editor_scene.h - declares the tilemap editor scene.
// Owns: editor viewport, selected tile/layer state, and edit-mode controls.

#pragma once

#include "app/scene.h"
#include "app/constants.h"
#include "ui/map_editor_backgrounds.h"
#include "ui/map_editor_paths.h"
#include "ui/map_editor_spawns.h"
#include "ui/map_editor_user_maps.h"
#include "systems/tilemap.h"
#include "systems/raylib_resource.h"
#include "data/content_pack.h"
#include "raylib.h"
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>

// ============================================================================
// map_editor_scene.h — Tile-based map editor
//
// Lets the player create custom stages using tiles from any stage tileset.
// Features: tile palette, paint/erase, collision editing, layer switching,
// save/load, and test-play (launches GameplayScene with the edited stage).
//
// Controls:
//   Arrow keys / WASD: Scroll viewport
//   Left click: Paint tile / set collision
//   Right click: Erase tile / clear collision
//   Middle click / Alt+click: Pick tile from canvas
//   Mouse wheel: Scroll tile palette
//   1-4: Switch tool (Paint, Erase, Fill, Pick)
//   Tab: Toggle palette panel
//   Toolbar PAL / ALL / ART / PAS / SOL / TS / BG / CMD: Mouse pickers
//   G: Toggle grid overlay
//   C: Toggle collision editing mode
//   V: Toggle collision visibility
//   L: Switch active layer (BG1/BG2)
//   Shift+PgUp/PgDn: Cycle BG2 background
//   [ / ]: BG2 parallax X (Shift adjusts Y)
//   R / Shift+R: Toggle BG2 repeat X/Y
//   +/-: Resize stage width
//   Ctrl+Z: Undo edit
//   Ctrl+Y / Ctrl+Shift+Z: Redo edit
//   Ctrl+S: Save stage
//   Ctrl+Shift+S: Save stage as named map
//   Ctrl+M: Edit map info
//   Ctrl+O: Browse user maps
//   P / Shift+P: Set / delete player spawn
//   Ctrl+E: Cycle enemy spawn id
//   E / Shift+E: Place / delete selected enemy spawn
//   Ctrl+K: Cycle pickup spawn id
//   K / Shift+K: Place / delete selected pickup spawn
//   O / Shift+O: Place / delete checkpoint
//   Ctrl+B: Cycle boss spawn id
//   B / Shift+B: Place / delete selected boss spawn
//   F1: Toggle help overlay
//   F5: Test play
//   Escape: Cancel Fill / confirm unsaved exit
// ============================================================================

namespace mmx {

namespace map_editor {
struct SpawnActionResult;
} // namespace map_editor

class SceneManager;
namespace map_editor {
struct BrushEditState;
struct ToolbarRenderState;
}

class MapEditorScene : public Scene {
public:
    MapEditorScene() = default;
    ~MapEditorScene() override = default;
    MapEditorScene(const MapEditorScene&) = delete;
    MapEditorScene& operator=(const MapEditorScene&) = delete;
    MapEditorScene(MapEditorScene&&) = delete;
    MapEditorScene& operator=(MapEditorScene&&) = delete;

    void setSceneManager(SceneManager* mgr) { sceneManager_ = mgr; }
    void setContentPack(const ContentPack& cp) { contentPack_ = cp; }

    void onEnter() override;
    void onExit() override;
    void onResume() override;
    void pollInput() override;
    void handleInput() override;
    void update(float dt) override;
    void render(float alpha) override;

private:
    SceneManager* sceneManager_ = nullptr;
    ContentPack contentPack_;

    // Stage data
    int stageWidth_ = 32;    // In tiles
    int stageHeight_ = 14;   // In tiles (1 screen = 16x14)
    std::vector<int> layerData_;       // BG1 (main layer)
    std::vector<int> layerDataBG_;     // BG2 (background layer)
    bool layerDataMainEditable_ = true;
    bool layerDataBgEditable_ = true;
    struct PreviewLayer {
        std::string path;
        TextureResource texture;
        float parallaxX = 1.0f;
        float parallaxY = 1.0f;
        int offsetX = 0;
        int offsetY = 0;
        bool repeatX = false;
        bool repeatY = false;
    };
    PreviewLayer previewMain_;
    PreviewLayer previewBg_;
    std::vector<TileType> collision_;
    std::vector<EditorSpawn> editorSpawns_;
    bool spawnsDirty_ = false;
    std::vector<std::string> enemySpawnIds_;
    int selectedEnemySpawn_ = 0;
    std::vector<std::string> pickupSpawnIds_;
    int selectedPickupSpawn_ = 0;
    std::vector<std::string> bossSpawnIds_;
    int selectedBossSpawn_ = 0;
    int activeLayer_ = 0;  // 0=BG1, 1=BG2

    struct EditSnapshot {
        int stageWidth = 0;
        int stageHeight = 0;
        std::vector<int> layerData;
        std::vector<int> layerDataBG;
        bool layerDataMainEditable = true;
        bool layerDataBgEditable = true;
        std::vector<TileType> collision;
        std::vector<EditorSpawn> editorSpawns;
        bool spawnsDirty = false;
        std::unordered_map<int, std::pair<int, int>> slopeEdits;
    };
    std::vector<EditSnapshot> undoStack_;
    std::vector<EditSnapshot> redoStack_;
    static constexpr int MAX_EDIT_HISTORY = 64;

    // Tileset
    TextureResource tileset_;
    int tilesetCols_ = 0;
    int tilesetRows_ = 0;
    int rawTileCount_ = 0;
    int totalTiles_ = 0;
    std::vector<int> paletteTileIds_;
    std::vector<int> stagePaletteTileIds_;
    std::vector<int> paletteExclude_;
    std::string tilesetPath_;
    std::string tilesetId_;

    // Camera (viewport into the stage)
    float camX_ = 0, camY_ = 0;

    // Tile palette
    int selectedTile_ = 1;
    int paletteScroll_ = 0;
    bool paletteVisible_ = true;
    bool showAllPaletteTiles_ = false;
    static constexpr int PALETTE_COLS = 3;
    static constexpr int PALETTE_TILE_PX = 16;
    static constexpr int PALETTE_WIDTH = PALETTE_COLS * PALETTE_TILE_PX;  // 48px

    // Cursor
    int cursorX_ = 0, cursorY_ = 0;  // Grid position

    // Collision editing
    bool editCollision_ = false;
    bool showCollision_ = true;
    TileType collisionBrush_ = TileType::Solid;

    // Tools
    enum class Tool { Paint, Erase, Fill, Pick };
    Tool tool_ = Tool::Paint;
    enum class ObjectTool { None, Player, Enemy, Pickup, Boss };
    ObjectTool objectTool_ = ObjectTool::None;
    enum class OptionPicker { None, Tileset, Background };
    OptionPicker optionPicker_ = OptionPicker::None;
    int optionPickerSelection_ = 0;

    // UI state
    bool showGrid_ = true;
    int timer_ = 0;
    int inputDelay_ = 0;
    std::string statusMsg_;
    int statusTimer_ = 0;
    bool helpOverlayVisible_ = false;

    // Stage file management
    std::string savePath_ = map_editor::defaultUserMapPath().generic_string();
    std::string mapName_ = "Untitled Stage";
    std::string mapAuthor_;
    std::string mapDescription_;
    bool hasNamedSavePath_ = false;
    bool saveNamePromptOpen_ = false;
    std::string saveNameDraft_;
    bool metadataPromptOpen_ = false;
    int metadataField_ = 0;
    std::string metadataAuthorDraft_;
    std::string metadataDescriptionDraft_;
    bool loadBrowserOpen_ = false;
    std::vector<map_editor::UserMapEntry> userMapEntries_;
    int userMapSelection_ = 0;
    std::unordered_map<std::string, const TextureResource*> loadBrowserThumbnails_;
    std::unordered_map<std::string, const TextureResource*> objectPreviewTextures_;
    bool commandMenuOpen_ = false;
    int commandMenuSelection_ = 0;
    bool hasUnsavedChanges_ = false;
    bool confirmExitPromptOpen_ = false;
    bool confirmExitToExtras_ = false;
    static constexpr int MAX_MAP_NAME_CHARS = 32;
    static constexpr int MAX_MAP_AUTHOR_CHARS = 32;
    static constexpr int MAX_MAP_DESCRIPTION_CHARS = 96;

    // Available tilesets (for tileset picker)
    struct TilesetEntry {
        std::string name;
        std::string id;
        std::string path;
        int cols;
        std::string stageJson;  // U64: decoded stages — the attrs source
        std::vector<int> paletteExclude;
    };
    std::vector<TilesetEntry> availableTilesets_;
    int selectedTileset_ = 0;

    // Available preview backgrounds (for BG2 picker)
    std::vector<map_editor::BackgroundEntry> availableBackgrounds_;
    int selectedBackground_ = -1;

    // U64: per-blockId raw attrs of the loaded DECODED tileset (empty for
    // legacy tilesets). Painting a block auto-derives its physics from these
    // + records slope heights (paint = art + physics together).
    std::vector<int> editorAttrs_;
    std::unordered_map<int, TileType> editorAttrOverrides_;
    std::unordered_map<int, std::pair<int, int>> slopeEdits_;  // idx -> (l,r)

    // Methods
    void initBlankStage();
    void loadTileset(const std::string& path, int cols,
                     const std::string& stageJson = std::string(),
                     std::vector<int> paletteExclude = {},
                     const std::string& tilesetId = std::string());
    void applyBackground(const map_editor::BackgroundEntry& entry);
    void selectBackground(int direction);
    void selectBackgroundIndex(int index);
    void updateSelectedBackgroundFromPreview();
    void selectTileset(int direction);
    void selectTilesetIndex(int index);
    void refreshPaletteTileIds();
    int paletteVisibleRows() const;
    int paletteMaxScroll() const;
    void clampPaletteScroll();
    void pagePalette(int direction);
    void toggleAllPaletteTiles();
    void enterTilePaintMode();
    void setCollisionBrush(TileType brush);
    void setObjectTool(ObjectTool tool);
    bool handleToolbarMouse(float internalX, float internalY);
    void openTilesetPicker();
    void openBackgroundPicker();
    void handleOptionPickerInput();
    void markUnsaved();
    std::string cursorStatusText() const;
    EditSnapshot captureEditSnapshot() const;
    bool editSnapshotMatchesCurrent(const EditSnapshot& snapshot) const;
    void restoreEditSnapshot(const EditSnapshot& snapshot);
    void pushUndoSnapshot(EditSnapshot snapshot);
    void recordEditSnapshotIfChanged(const EditSnapshot& before);
    void applySpawnActionResult(const EditSnapshot& before,
                                const map_editor::SpawnActionResult& result);
    map_editor::BrushEditState brushEditState();
    bool paintTileNoHistory(int tx, int ty);
    bool eraseTileNoHistory(int tx, int ty);
    void undoEdit();
    void redoEdit();
    void paintTile(int tx, int ty);
    void eraseTile(int tx, int ty);
    void fillTile(int tx, int ty);
    void pickTile(int tx, int ty);
    void placePlayerSpawnAt(float worldX, float worldY);
    void placePlayerSpawnAtCursor();
    void deletePlayerSpawn();
    std::string selectedEnemySpawnId() const;
    void selectEnemySpawn(int direction);
    std::string selectedPickupSpawnId() const;
    void selectPickupSpawn(int direction);
    std::string selectedBossSpawnId() const;
    void selectBossSpawn(int direction);
    void placeEnemySpawnAt(float worldX, float worldY);
    void deleteEnemySpawnNear(float worldX, float worldY);
    void placePickupSpawnAt(float worldX, float worldY);
    void deletePickupSpawnNear(float worldX, float worldY);
    void placeCheckpointAt(float worldX, float worldY);
    void deleteCheckpointNear(float worldX, float worldY);
    void placeBossSpawnAt(float worldX, float worldY);
    void deleteBossSpawnNear(float worldX, float worldY);
    void resizeStage(int newWidth, int newHeight);
    void save(bool clearUnsaved = true);
    void saveOrPromptForName();
    void beginSaveAs();
    void handleSaveNameInput();
    void confirmSaveAs();
    void beginMetadataEdit();
    void handleMetadataInput();
    void confirmMetadataEdit();
    void openLoadBrowser();
    void handleLoadBrowserInput();
    const TextureResource* getLoadBrowserThumbnail(
        const map_editor::UserMapEntry& entry);
    const TextureResource* getCachedEditorTexture(const std::string& path);
    void openCommandMenu();
    void handleCommandMenuInput();
    void activateCommandMenuSelection();
    void load(const std::string& path);
    void requestExitToTitle();
    void returnToTitle();
    void openExtras();
    void handleExitConfirmInput();
    void testPlay();
    void setStatus(const char* msg);

    void renderStage();
    void renderObjectOverlay();
    void renderPalette();
    map_editor::ToolbarRenderState buildToolbarRenderState() const;
    void renderToolbar();
    void renderHelpOverlay();
    void renderLoadBrowser();
    void renderCommandMenu();
    void renderOptionPicker();

    // Coordinate helpers
    int viewportWidth() const;
    int stageAreaWidth() const;
    void worldToTile(float worldX, float worldY, int& tileX, int& tileY);
};

} // namespace mmx
