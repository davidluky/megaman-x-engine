// map_editor_scene.cpp - runs the in-engine tilemap editor and JSON export.
// Owns: editor cursor, tile painting, tileset discovery, and saved map data.

#include "ui/map_editor_scene.h"
#include "ui/map_editor_brush_ops.h"
#include "ui/map_editor_canvas.h"
#include "ui/map_editor_dialogs.h"
#include "ui/map_editor_help.h"
#include "ui/map_editor_input.h"
#include "ui/map_editor_load_browser.h"
#include "ui/map_editor_object_overlay.h"
#include "ui/map_editor_palette.h"
#include "ui/map_editor_save_load.h"
#include "ui/map_editor_spawn_ops.h"
#include "ui/map_editor_stage_json.h"
#include "ui/map_editor_thumbnail.h"
#include "ui/map_editor_toolbar.h"
#include "ui/map_editor_tilesets.h"
#include "ui/extras_scene.h"
#include "ui/title_scene.h"
#include "gameplay/gameplay_scene.h"
#include "app/scene_manager.h"
#include "app/input.h"
#include "data/localization.h"
#include "data/settings.h"
#include "systems/audio.h"
#include "systems/asset_cache.h"
#include "data/content_paths.h"
#include "data/json_io.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <iterator>
#include <utility>

namespace mmx {

namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;

constexpr int kScreenTileWidth = 16;
constexpr int kScreenTileHeight = 14;
constexpr int kPaletteHeaderHeight = 10;
constexpr int kPalettePagerHeight = 12;

bool envValueEquals(const char* name, const char* expected) {
    const char* value = std::getenv(name);
    return value && std::string(value) == expected;
}

int positiveModulo(int value, int divisor) {
    if (divisor <= 0) return 0;
    int result = value % divisor;
    return result < 0 ? result + divisor : result;
}

} // namespace

// How wide the stage area is on screen (excluding palette panel)
int MapEditorScene::stageAreaWidth() const {
    return paletteVisible_ ? (INTERNAL_WIDTH - PALETTE_WIDTH) : INTERNAL_WIDTH;
}

int MapEditorScene::viewportWidth() const {
    return stageAreaWidth();
}

void MapEditorScene::worldToTile(float worldX, float worldY, int& tileX, int& tileY) {
    tileX = static_cast<int>(worldX) / TILE_SIZE;
    tileY = static_cast<int>(worldY) / TILE_SIZE;
}

std::string MapEditorScene::cursorStatusText() const {
    return map_editor::cursorStatusText(
        cursorX_, cursorY_, stageWidth_, stageHeight_, activeLayer_,
        layerData_, layerDataBG_, Settings::language);
}

MapEditorScene::EditSnapshot MapEditorScene::captureEditSnapshot() const {
    EditSnapshot snapshot;
    snapshot.stageWidth = stageWidth_;
    snapshot.stageHeight = stageHeight_;
    snapshot.layerData = layerData_;
    snapshot.layerDataBG = layerDataBG_;
    snapshot.layerDataMainEditable = layerDataMainEditable_;
    snapshot.layerDataBgEditable = layerDataBgEditable_;
    snapshot.collision = collision_;
    snapshot.editorSpawns = editorSpawns_;
    snapshot.spawnsDirty = spawnsDirty_;
    snapshot.slopeEdits = slopeEdits_;
    return snapshot;
}

bool MapEditorScene::editSnapshotMatchesCurrent(const EditSnapshot& snapshot) const {
    return snapshot.stageWidth == stageWidth_ &&
           snapshot.stageHeight == stageHeight_ &&
           snapshot.layerData == layerData_ &&
           snapshot.layerDataBG == layerDataBG_ &&
           snapshot.layerDataMainEditable == layerDataMainEditable_ &&
           snapshot.layerDataBgEditable == layerDataBgEditable_ &&
           snapshot.collision == collision_ &&
           snapshot.editorSpawns == editorSpawns_ &&
           snapshot.spawnsDirty == spawnsDirty_ &&
           snapshot.slopeEdits == slopeEdits_;
}

void MapEditorScene::restoreEditSnapshot(const EditSnapshot& snapshot) {
    stageWidth_ = snapshot.stageWidth;
    stageHeight_ = snapshot.stageHeight;
    layerData_ = snapshot.layerData;
    layerDataBG_ = snapshot.layerDataBG;
    layerDataMainEditable_ = snapshot.layerDataMainEditable;
    layerDataBgEditable_ = snapshot.layerDataBgEditable;
    collision_ = snapshot.collision;
    editorSpawns_ = snapshot.editorSpawns;
    spawnsDirty_ = snapshot.spawnsDirty;
    slopeEdits_ = snapshot.slopeEdits;

    const float maxX = std::max(0.0f, static_cast<float>(stageWidth_ * TILE_SIZE - stageAreaWidth()));
    const float maxY = std::max(0.0f, static_cast<float>(stageHeight_ * TILE_SIZE - INTERNAL_HEIGHT + 10));
    camX_ = std::clamp(camX_, 0.0f, maxX);
    camY_ = std::clamp(camY_, 0.0f, maxY);
}

void MapEditorScene::pushUndoSnapshot(EditSnapshot snapshot) {
    if (static_cast<int>(undoStack_.size()) >= MAX_EDIT_HISTORY) {
        undoStack_.erase(undoStack_.begin());
    }
    undoStack_.push_back(std::move(snapshot));
}

void MapEditorScene::recordEditSnapshotIfChanged(const EditSnapshot& before) {
    if (editSnapshotMatchesCurrent(before)) return;
    pushUndoSnapshot(before);
    redoStack_.clear();
    markUnsaved();
}

void MapEditorScene::applySpawnActionResult(
    const EditSnapshot& before,
    const map_editor::SpawnActionResult& result) {
    if (!result.handled) return;
    if (result.changed) {
        spawnsDirty_ = true;
        recordEditSnapshotIfChanged(before);
    }
    setStatus(result.status.c_str());
}

void MapEditorScene::markUnsaved() {
    hasUnsavedChanges_ = true;
}

void MapEditorScene::undoEdit() {
    if (undoStack_.empty()) {
        setStatus(uiText(UiText::MapEditorStatusUndoEmpty, Settings::language));
        return;
    }

    EditSnapshot current = captureEditSnapshot();
    EditSnapshot previous = std::move(undoStack_.back());
    undoStack_.pop_back();
    const bool spawnLayerChanged = current.editorSpawns != previous.editorSpawns;
    restoreEditSnapshot(previous);
    if (spawnLayerChanged) spawnsDirty_ = true;
    redoStack_.push_back(std::move(current));
    markUnsaved();
    setStatus(uiText(UiText::MapEditorStatusUndo, Settings::language));
}

void MapEditorScene::redoEdit() {
    if (redoStack_.empty()) {
        setStatus(uiText(UiText::MapEditorStatusRedoEmpty, Settings::language));
        return;
    }

    EditSnapshot current = captureEditSnapshot();
    EditSnapshot next = std::move(redoStack_.back());
    redoStack_.pop_back();
    const bool spawnLayerChanged = current.editorSpawns != next.editorSpawns;
    restoreEditSnapshot(next);
    if (spawnLayerChanged) spawnsDirty_ = true;
    pushUndoSnapshot(std::move(current));
    markUnsaved();
    setStatus(uiText(UiText::MapEditorStatusRedo, Settings::language));
}

map_editor::ToolbarRenderState MapEditorScene::buildToolbarRenderState() const {
    return map_editor::buildToolbarRenderState({
        INTERNAL_HEIGHT,
        stageAreaWidth(),
        static_cast<int>(tool_),
        activeLayer_,
        selectedTile_,
        editCollision_,
        collisionBrush_,
        static_cast<int>(objectTool_),
        cursorStatusText(),
        paletteVisible_,
        showAllPaletteTiles_,
        commandMenuOpen_,
        selectedTileset_,
        static_cast<int>(availableTilesets_.size()),
        selectedBackground_,
        static_cast<int>(availableBackgrounds_.size()),
        map_editor::compactEditorObjectId(selectedEnemySpawnId(), 12),
        static_cast<int>(enemySpawnIds_.size()),
        selectedPickupSpawn_,
        static_cast<int>(pickupSpawnIds_.size()),
        selectedBossSpawn_,
        static_cast<int>(bossSpawnIds_.size()),
        Settings::language,
    });
}

bool MapEditorScene::handleToolbarMouse(float internalX, float internalY) {
    const auto result = map_editor::toolbarMouseAction(
        buildToolbarRenderState(),
        internalX,
        internalY,
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
        IsMouseButtonPressed(MOUSE_BUTTON_RIGHT),
        IsMouseButtonDown(MOUSE_BUTTON_LEFT),
        IsMouseButtonDown(MOUSE_BUTTON_RIGHT));
    if (!result.consumed) return false;

    switch (result.action) {
        case map_editor::ToolbarMouseAction::None:
            break;
        case map_editor::ToolbarMouseAction::TogglePalette:
            paletteVisible_ = !paletteVisible_;
            setStatus(uiText(paletteVisible_
                             ? UiText::MapEditorStatusPaletteOpen
                             : UiText::MapEditorStatusPaletteClosed,
                             Settings::language));
            break;
        case map_editor::ToolbarMouseAction::TogglePaletteMode:
            toggleAllPaletteTiles();
            break;
        case map_editor::ToolbarMouseAction::EnterArtMode:
            enterTilePaintMode();
            break;
        case map_editor::ToolbarMouseAction::SetPassableBrush:
            setCollisionBrush(TileType::None);
            break;
        case map_editor::ToolbarMouseAction::SetSolidBrush:
            setCollisionBrush(TileType::Solid);
            break;
        case map_editor::ToolbarMouseAction::OpenTilesetPicker:
            openTilesetPicker();
            break;
        case map_editor::ToolbarMouseAction::SelectNextTileset:
            selectTileset(1);
            break;
        case map_editor::ToolbarMouseAction::OpenBackgroundPicker:
            openBackgroundPicker();
            break;
        case map_editor::ToolbarMouseAction::SelectNextBackground:
            selectBackground(1);
            break;
        case map_editor::ToolbarMouseAction::OpenCommandMenu:
            openCommandMenu();
            break;
        case map_editor::ToolbarMouseAction::SelectPlayerObject:
            setObjectTool(ObjectTool::Player);
            break;
        case map_editor::ToolbarMouseAction::SelectPreviousEnemy:
            selectEnemySpawn(-1);
            setObjectTool(ObjectTool::Enemy);
            break;
        case map_editor::ToolbarMouseAction::SelectNextEnemy:
            selectEnemySpawn(1);
            setObjectTool(ObjectTool::Enemy);
            break;
        case map_editor::ToolbarMouseAction::SelectEnemyObject:
            setObjectTool(ObjectTool::Enemy);
            break;
        case map_editor::ToolbarMouseAction::SelectPreviousPickup:
            selectPickupSpawn(-1);
            setObjectTool(ObjectTool::Pickup);
            break;
        case map_editor::ToolbarMouseAction::SelectNextPickup:
            selectPickupSpawn(1);
            setObjectTool(ObjectTool::Pickup);
            break;
        case map_editor::ToolbarMouseAction::SelectPickupObject:
            setObjectTool(ObjectTool::Pickup);
            break;
        case map_editor::ToolbarMouseAction::SelectPreviousBoss:
            selectBossSpawn(-1);
            setObjectTool(ObjectTool::Boss);
            break;
        case map_editor::ToolbarMouseAction::SelectNextBoss:
            selectBossSpawn(1);
            setObjectTool(ObjectTool::Boss);
            break;
        case map_editor::ToolbarMouseAction::SelectBossObject:
            setObjectTool(ObjectTool::Boss);
            break;
    }
    return true;
}

void MapEditorScene::onEnter() {
    timer_ = 0;

    availableTilesets_.clear();
    for (const auto& discoveredTileset : map_editor::discoverAvailableTilesets()) {
        TilesetEntry e;
        e.name = discoveredTileset.name;
        e.id = discoveredTileset.id;
        e.path = discoveredTileset.path;
        e.cols = discoveredTileset.cols;
        e.stageJson = discoveredTileset.stageJson;
        e.paletteExclude = discoveredTileset.paletteExclude;
        availableTilesets_.push_back(std::move(e));
    }
    selectedTileset_ = 0;
    availableBackgrounds_ = map_editor::discoverBackgroundsFromDecodedStages();
    selectedBackground_ = -1;
    enemySpawnIds_ = map_editor::loadEnemySpawnIds();
    selectedEnemySpawn_ = 0;
    pickupSpawnIds_ = map_editor::loadPickupSpawnIds();
    selectedPickupSpawn_ = 0;
    bossSpawnIds_ = map_editor::loadBossSpawnIds();
    selectedBossSpawn_ = 0;

    // Load default tileset (intro-highway if available)
    if (!availableTilesets_.empty()) {
        loadTileset(availableTilesets_[0].path, availableTilesets_[0].cols,
                    availableTilesets_[0].stageJson,
                    availableTilesets_[0].paletteExclude,
                    availableTilesets_[0].id);
    }

    initBlankStage();

    AudioManager::playBGM("stage-select");
    setStatus(uiText(UiText::MapEditorInitialStatus, Settings::language));
    if (envValueEquals("MEGAMAN_X_SMOKE_ID", "map-editor") &&
        envValueEquals("MEGAMAN_X_SMOKE_PROFILE", "objects")) {
        load("content/x1/stages/tiles/spark-mandrill_full.json");
        if (!editorSpawns_.empty()) {
            const auto& spawn = editorSpawns_.front();
            map_editor::focusCameraOnPoint(
                camX_, camY_, spawn.x, spawn.y, stageWidth_, stageHeight_,
                TILE_SIZE, stageAreaWidth(), INTERNAL_HEIGHT);
        }
        setStatus(uiText(UiText::MapEditorStatusObjectOverlaySmoke, Settings::language));
    } else if (envValueEquals("MEGAMAN_X_SMOKE_ID", "map-editor") &&
               envValueEquals("MEGAMAN_X_SMOKE_PROFILE", "fill")) {
        tool_ = Tool::Fill;
        fillTile(0, 0);
    } else if (envValueEquals("MEGAMAN_X_SMOKE_ID", "map-editor") &&
               envValueEquals("MEGAMAN_X_SMOKE_PROFILE", "placement")) {
        placePlayerSpawnAt(32.0f, 56.0f);
        placeEnemySpawnAt(82.0f, 80.0f);
        placePickupSpawnAt(132.0f, 56.0f);
        placeCheckpointAt(178.0f, 96.0f);
        placeBossSpawnAt(178.0f, 56.0f);
    } else if (envValueEquals("MEGAMAN_X_SMOKE_ID", "map-editor") &&
               envValueEquals("MEGAMAN_X_SMOKE_PROFILE", "help")) {
        helpOverlayVisible_ = true;
    }
}

void MapEditorScene::onExit() {
    tileset_.reset();
    previewMain_.texture.reset();
    previewBg_.texture.reset();
    loadBrowserThumbnails_.clear();
    objectPreviewTextures_.clear();
}

void MapEditorScene::onResume() {
    // Returning from test play
    setStatus(uiText(UiText::MapEditorStatusBackFromTestPlay, Settings::language));
}

void MapEditorScene::initBlankStage() {
    layerData_.assign(stageWidth_ * stageHeight_, -1);
    layerDataBG_.assign(stageWidth_ * stageHeight_, -1);
    layerDataMainEditable_ = true;
    layerDataBgEditable_ = true;
    savePath_ = map_editor::defaultUserMapPath().generic_string();
    mapName_ = "Untitled Stage";
    hasNamedSavePath_ = false;
    mapAuthor_.clear();
    mapDescription_.clear();
    previewMain_ = {};
    previewBg_ = {};
    selectedBackground_ = -1;
    collision_.assign(stageWidth_ * stageHeight_, TileType::None);
    editorSpawns_.clear();
    spawnsDirty_ = false;
    slopeEdits_.clear();
    undoStack_.clear();
    redoStack_.clear();
    objectTool_ = ObjectTool::None;
    optionPicker_ = OptionPicker::None;
    loadBrowserOpen_ = false;
    commandMenuOpen_ = false;
    hasUnsavedChanges_ = false;
    confirmExitPromptOpen_ = false;
    confirmExitToExtras_ = false;
    camX_ = 0;
    camY_ = 0;
}

void MapEditorScene::loadTileset(const std::string& path, int cols,
                                 const std::string& stageJson,
                                 std::vector<int> paletteExclude,
                                 const std::string& tilesetId) {
    tileset_.load(path);
    tileset_.setFilter(TEXTURE_FILTER_POINT);
    tilesetPath_ = path;
    tilesetId_ = tilesetId;
    if (tilesetId_.empty()) {
        for (const auto& entry : availableTilesets_) {
            if (entry.path == path) {
                tilesetId_ = entry.id;
                break;
            }
        }
    }
    tilesetCols_ = cols;
    paletteExclude_ = std::move(paletteExclude);
    paletteTileIds_.clear();
    stagePaletteTileIds_.clear();
    tilesetRows_ = 0;
    rawTileCount_ = 0;
    totalTiles_ = 0;
    editorAttrs_.clear(); editorAttrOverrides_.clear();
    if (tileset_.valid() && tilesetCols_ > 0) {
        tilesetRows_ = (tileset_.height() + TILE_SIZE - 1) / TILE_SIZE;
        rawTileCount_ = tilesetCols_ * tilesetRows_;
    }
    const auto metadata = map_editor::loadTilesetStageMetadata(
        stageJson, rawTileCount_, paletteExclude_);
    editorAttrs_ = metadata.attrs; editorAttrOverrides_ = metadata.attrOverrides;
    stagePaletteTileIds_ = metadata.stagePaletteTileIds;
    if (tileset_.valid() && tilesetCols_ > 0) {
        refreshPaletteTileIds();
    }
    selectedTile_ = paletteTileIds_.empty() ? 0 : paletteTileIds_.front();
    paletteScroll_ = 0;
}

void MapEditorScene::refreshPaletteTileIds() {
    const int previousSelectedTile = selectedTile_;
    if (!showAllPaletteTiles_ && !stagePaletteTileIds_.empty()) {
        paletteTileIds_ = stagePaletteTileIds_;
    } else {
        paletteTileIds_ = map_editor::buildPaletteTileIds(
            rawTileCount_,
            showAllPaletteTiles_ ? std::vector<int>{} : paletteExclude_);
    }
    totalTiles_ = static_cast<int>(paletteTileIds_.size());
    if (paletteTileIds_.empty()) {
        selectedTile_ = 0;
    } else if (std::find(paletteTileIds_.begin(), paletteTileIds_.end(),
                         previousSelectedTile) == paletteTileIds_.end()) {
        selectedTile_ = paletteTileIds_.front();
    }
    clampPaletteScroll();
}

int MapEditorScene::paletteVisibleRows() const {
    return map_editor::paletteVisibleRows(
        INTERNAL_HEIGHT, kPaletteHeaderHeight, kPalettePagerHeight, PALETTE_TILE_PX);
}

int MapEditorScene::paletteMaxScroll() const {
    return map_editor::paletteMaxScroll(
        totalTiles_, PALETTE_COLS, paletteVisibleRows());
}

void MapEditorScene::clampPaletteScroll() {
    paletteScroll_ =
        map_editor::clampedPaletteScroll(paletteScroll_, paletteMaxScroll());
}

void MapEditorScene::pagePalette(int direction) {
    const map_editor::PalettePageResult result =
        map_editor::pagePaletteScroll(
            paletteScroll_, direction, paletteMaxScroll(), paletteVisibleRows());
    paletteScroll_ = result.scroll;

    char buf[64];
    snprintf(buf, sizeof(buf), "%s %d/%d",
             uiText(UiText::MapEditorStatusPalettePagePrefix, Settings::language),
             result.currentPage,
             result.totalPages);
    setStatus(buf);
}

void MapEditorScene::toggleAllPaletteTiles() {
    showAllPaletteTiles_ = !showAllPaletteTiles_;
    paletteScroll_ = 0;
    refreshPaletteTileIds();
    char buf[64];
    snprintf(buf, sizeof(buf), "%s %d",
             uiText(showAllPaletteTiles_
                        ? UiText::MapEditorStatusPaletteAll
                        : UiText::MapEditorStatusPaletteStage,
                    Settings::language),
             totalTiles_);
    setStatus(buf);
}

void MapEditorScene::selectTileset(int direction) {
    if (availableTilesets_.empty()) return;
    if (selectedTileset_ < 0 ||
        selectedTileset_ >= static_cast<int>(availableTilesets_.size())) {
        selectedTileset_ = 0;
    }
    selectedTileset_ = positiveModulo(
        selectedTileset_ + direction,
        static_cast<int>(availableTilesets_.size()));

    selectTilesetIndex(selectedTileset_);
}

void MapEditorScene::selectTilesetIndex(int index) {
    if (availableTilesets_.empty()) return;

    selectedTileset_ =
        std::clamp(index, 0, static_cast<int>(availableTilesets_.size()) - 1);
    optionPickerSelection_ = selectedTileset_;
    const auto& entry = availableTilesets_[selectedTileset_];
    loadTileset(entry.path, entry.cols, entry.stageJson, entry.paletteExclude, entry.id);
    markUnsaved();

    char buf[64];
    snprintf(buf, sizeof(buf), "%s %s",
             uiText(UiText::MapEditorStatusTilesetPrefix, Settings::language),
             entry.name.c_str());
    setStatus(buf);
}

void MapEditorScene::applyBackground(const map_editor::BackgroundEntry& entry) {
    previewBg_.texture.reset();
    map_editor::applyBackgroundEntryToPreview(
        entry,
        previewBg_.path,
        previewBg_.parallaxX,
        previewBg_.parallaxY,
        previewBg_.offsetX,
        previewBg_.offsetY,
        previewBg_.repeatX,
        previewBg_.repeatY);

    auto resolvedPreview = content_paths::resolveAssetPath(previewBg_.path);
    if (resolvedPreview && previewBg_.texture.load(*resolvedPreview)) {
        previewBg_.texture.setFilter(TEXTURE_FILTER_POINT);
    }
}

void MapEditorScene::selectBackground(int direction) {
    if (availableBackgrounds_.empty()) {
        setStatus(uiText(UiText::MapEditorStatusNoBackgrounds, Settings::language));
        return;
    }

    selectedBackground_ = map_editor::nextBackgroundSelection(
        selectedBackground_, direction, static_cast<int>(availableBackgrounds_.size()));

    selectBackgroundIndex(selectedBackground_);
}

void MapEditorScene::selectBackgroundIndex(int index) {
    if (availableBackgrounds_.empty()) {
        selectedBackground_ = -1;
        setStatus(uiText(UiText::MapEditorStatusNoBackgrounds, Settings::language));
        return;
    }

    selectedBackground_ =
        std::clamp(index, 0, static_cast<int>(availableBackgrounds_.size()) - 1);
    optionPickerSelection_ = selectedBackground_;
    const auto& entry = availableBackgrounds_[selectedBackground_];
    applyBackground(entry);
    markUnsaved();

    char buf[96];
    snprintf(buf, sizeof(buf), "%s %s",
             uiText(UiText::MapEditorStatusBackgroundPrefix, Settings::language),
             entry.name.c_str());
    setStatus(buf);
}

void MapEditorScene::enterTilePaintMode() {
    editCollision_ = false;
    objectTool_ = ObjectTool::None;
    tool_ = Tool::Paint;
    setStatus(uiText(UiText::MapEditorStatusArtMode, Settings::language));
}

void MapEditorScene::setCollisionBrush(TileType brush) {
    if (editCollision_ && collisionBrush_ == brush) {
        enterTilePaintMode();
        return;
    }
    collisionBrush_ = brush;
    editCollision_ = true;
    objectTool_ = ObjectTool::None;
    showCollision_ = true;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s %s",
             uiText(UiText::MapEditorStatusBrushPrefix, Settings::language),
             map_editor::mapEditorCollisionBrushBadge(collisionBrush_, Settings::language));
    setStatus(buf);
}

void MapEditorScene::setObjectTool(ObjectTool tool) {
    if (objectTool_ == tool) {
        switch (tool) {
            case ObjectTool::Player:
                setStatus(uiText(UiText::MapEditorStatusPlayerSpawnMode,
                                 Settings::language));
                break;
            case ObjectTool::Enemy:
                selectEnemySpawn(0);
                break;
            case ObjectTool::Pickup:
                selectPickupSpawn(0);
                break;
            case ObjectTool::Boss:
                selectBossSpawn(0);
                break;
            case ObjectTool::None:
                enterTilePaintMode();
                break;
        }
        return;
    }

    objectTool_ = tool;
    editCollision_ = false;
    tool_ = Tool::Paint;
    switch (objectTool_) {
        case ObjectTool::Player:
            setStatus(uiText(UiText::MapEditorStatusPlayerSpawnMode,
                             Settings::language));
            break;
        case ObjectTool::Enemy:
            selectEnemySpawn(0);
            break;
        case ObjectTool::Pickup:
            selectPickupSpawn(0);
            break;
        case ObjectTool::Boss:
            selectBossSpawn(0);
            break;
        case ObjectTool::None:
            setStatus(uiText(UiText::MapEditorStatusArtMode, Settings::language));
            break;
    }
}

void MapEditorScene::updateSelectedBackgroundFromPreview() {
    selectedBackground_ =
        map_editor::backgroundIndexForPreviewPath(availableBackgrounds_, previewBg_.path);
}

void MapEditorScene::pollInput() {
    // Nothing needed - we read raylib directly in handleInput
}

void MapEditorScene::handleInput() {
    timer_++;

    if (map_editor::dispatchCapturedInput(
            saveNamePromptOpen_,
            metadataPromptOpen_,
            loadBrowserOpen_,
            commandMenuOpen_,
            optionPicker_ != OptionPicker::None,
            confirmExitPromptOpen_,
            [this]() { handleSaveNameInput(); },
            [this]() { handleMetadataInput(); },
            [this]() { handleLoadBrowserInput(); },
            [this]() { handleCommandMenuInput(); },
            [this]() { handleOptionPickerInput(); },
            [this]() { handleExitConfirmInput(); })) {
        return;
    }

    const bool controlHeld = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    const bool shiftHeld = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    if (controlHeld && IsKeyPressed(KEY_Z)) {
        if (shiftHeld) redoEdit();
        else undoEdit();
        return;
    }
    if (controlHeld && IsKeyPressed(KEY_Y)) {
        redoEdit();
        return;
    }
    if (IsKeyPressed(KEY_F1)) {
        helpOverlayVisible_ = !helpOverlayVisible_;
        setStatus(uiText(helpOverlayVisible_
                         ? UiText::MapEditorStatusHelpOpen
                         : UiText::MapEditorStatusHelpClosed,
                         Settings::language));
        return;
    }
    if (helpOverlayVisible_) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            helpOverlayVisible_ = false;
            setStatus(uiText(UiText::MapEditorStatusHelpClosed, Settings::language));
        }
        if (statusTimer_ > 0) statusTimer_--;
        return;
    }

    // --- Escape: cancel modal tool state / return to title ---
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (tool_ == Tool::Fill) {
            tool_ = Tool::Paint;
            setStatus(uiText(UiText::MapEditorStatusFillCancelled, Settings::language));
            return;
        }
        requestExitToTitle();
        return;
    }

    map_editor::scrollCamera(
        camX_,
        camY_,
        IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A),
        IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D),
        IsKeyDown(KEY_UP) || IsKeyDown(KEY_W),
        IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S),
        shiftHeld,
        stageWidth_,
        stageHeight_,
        TILE_SIZE,
        stageAreaWidth(),
        INTERNAL_HEIGHT);

    // --- Tool switching ---
    if (IsKeyPressed(KEY_ONE)) {
        enterTilePaintMode();
    }
    if (IsKeyPressed(KEY_TWO)) {
        tool_ = Tool::Erase;
        setStatus(uiText(UiText::MapEditorStatusToolErase, Settings::language));
    }
    if (IsKeyPressed(KEY_THREE)) {
        tool_ = Tool::Fill;
        setStatus(uiText(UiText::MapEditorStatusToolFill, Settings::language));
    }
    if (IsKeyPressed(KEY_FOUR)) {
        tool_ = Tool::Pick;
        setStatus(uiText(UiText::MapEditorStatusToolPick, Settings::language));
    }

    // --- Toggle keys ---
    if (IsKeyPressed(KEY_TAB)) paletteVisible_ = !paletteVisible_;
    if (IsKeyPressed(KEY_G))   showGrid_ = !showGrid_;
    if (IsKeyPressed(KEY_C))   {
        editCollision_ = !editCollision_;
        if (editCollision_) showCollision_ = true;
        setStatus(uiText(editCollision_
                         ? UiText::MapEditorStatusCollisionEditOn
                         : UiText::MapEditorStatusCollisionEditOff,
                         Settings::language));
    }
    if (IsKeyPressed(KEY_V))   showCollision_ = !showCollision_;
    if (IsKeyPressed(KEY_L))   {
        activeLayer_ = (activeLayer_ + 1) % 2;
        setStatus(uiText(activeLayer_ == 0
                         ? UiText::MapEditorStatusLayerMain
                         : UiText::MapEditorStatusLayerBack,
                         Settings::language));
    }

    // --- Collision brush cycling (when in collision mode) ---
    if (editCollision_ && IsKeyPressed(KEY_B)) {
        collisionBrush_ = map_editor::nextCollisionBrush(collisionBrush_);
        char buf[64];
        snprintf(buf, sizeof(buf), "%s %s",
                 uiText(UiText::MapEditorStatusBrushPrefix, Settings::language),
                 map_editor::mapEditorCollisionBrushBadge(
                     collisionBrush_, Settings::language));
        setStatus(buf);
    }

    // --- Stage resize ---
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
        resizeStage(stageWidth_ + 16, stageHeight_);
        setStatus(uiText(UiText::MapEditorStatusStageWider, Settings::language));
    }
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
        if (stageWidth_ > 16) {
            resizeStage(stageWidth_ - 16, stageHeight_);
            setStatus(uiText(UiText::MapEditorStatusStageNarrower, Settings::language));
        }
    }
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_EQUAL)) {
        resizeStage(stageWidth_, stageHeight_ + 1);
        setStatus(uiText(UiText::MapEditorStatusStageTaller, Settings::language));
    }
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_MINUS)) {
        if (stageHeight_ > 7) {
            resizeStage(stageWidth_, stageHeight_ - 1);
            setStatus(uiText(UiText::MapEditorStatusStageShorter, Settings::language));
        }
    }

    // --- Tileset switching (PageUp/PageDown) ---
    if (!availableTilesets_.empty()) {
        if (!shiftHeld && IsKeyPressed(KEY_PAGE_UP)) {
            selectTileset(-1);
        }
        if (!shiftHeld && IsKeyPressed(KEY_PAGE_DOWN)) {
            selectTileset(1);
        }
    }

    // --- Background picker and preview controls ---
    if (shiftHeld && IsKeyPressed(KEY_PAGE_UP)) {
        selectBackground(-1);
    }
    if (shiftHeld && IsKeyPressed(KEY_PAGE_DOWN)) {
        selectBackground(1);
    }
    if (!previewBg_.path.empty() &&
        (IsKeyPressed(KEY_LEFT_BRACKET) || IsKeyPressed(KEY_RIGHT_BRACKET))) {
        map_editor::adjustBackgroundParallax(
            previewBg_.parallaxX,
            previewBg_.parallaxY,
            shiftHeld,
            IsKeyPressed(KEY_RIGHT_BRACKET));
        char buf[96];
        snprintf(buf, sizeof(buf), "%s %.2f %.2f",
                 uiText(UiText::MapEditorStatusBgParallaxPrefix, Settings::language),
                 previewBg_.parallaxX, previewBg_.parallaxY);
        markUnsaved();
        setStatus(buf);
    }
    if (!previewBg_.path.empty() && IsKeyPressed(KEY_R)) {
        map_editor::toggleBackgroundRepeat(previewBg_.repeatX, previewBg_.repeatY, shiftHeld);
        char buf[96];
        snprintf(buf, sizeof(buf), "%s X:%s Y:%s",
                 uiText(UiText::MapEditorStatusBgRepeatPrefix, Settings::language),
                 uiText(previewBg_.repeatX ? UiText::ValueOn : UiText::ValueOff,
                        Settings::language),
                 uiText(previewBg_.repeatY ? UiText::ValueOn : UiText::ValueOff,
                        Settings::language));
        markUnsaved();
        setStatus(buf);
    }

    // --- Save (Ctrl+S / Ctrl+Shift+S) ---
    if (controlHeld && IsKeyPressed(KEY_S)) {
        if (shiftHeld) {
            beginSaveAs();
        } else {
            saveOrPromptForName();
        }
    }

    // --- Map metadata (Ctrl+M) ---
    if (controlHeld && IsKeyPressed(KEY_M)) {
        beginMetadataEdit();
    }

    // --- Load (Ctrl+O) ---
    if (controlHeld && IsKeyPressed(KEY_O)) {
        openLoadBrowser();
    }

    // --- Enemy spawn picker (Ctrl+E / Ctrl+Shift+E) ---
    if (controlHeld && IsKeyPressed(KEY_E)) {
        selectEnemySpawn(shiftHeld ? -1 : 1);
        return;
    }

    // --- Pickup spawn picker (Ctrl+K / Ctrl+Shift+K) ---
    if (controlHeld && IsKeyPressed(KEY_K)) {
        selectPickupSpawn(shiftHeld ? -1 : 1);
        return;
    }

    // --- Boss spawn picker (Ctrl+B / Ctrl+Shift+B) ---
    if (controlHeld && IsKeyPressed(KEY_B)) {
        selectBossSpawn(shiftHeld ? -1 : 1);
        return;
    }

    // --- Test play (F5) ---
    if (IsKeyPressed(KEY_F5)) {
        testPlay();
        return;
    }

    // --- Mouse interaction ---
    const screen_transform::InternalPoint internalMouse =
        map_editor::currentInternalMouse(Settings::aspect43);
    if (!internalMouse.inside) {
        if (statusTimer_ > 0) statusTimer_--;
        return;
    }
    const float internalX = internalMouse.x;
    const float internalY = internalMouse.y;

    if (handleToolbarMouse(internalX, internalY)) {
        if (statusTimer_ > 0) statusTimer_--;
        return;
    }

    const map_editor::PaletteMouseInputResult paletteMouse =
        map_editor::handlePaletteMouseInput(map_editor::paletteMouseInputState(
            paletteVisible_,
            internalX,
            internalY,
            stageAreaWidth(),
            INTERNAL_HEIGHT,
            paletteScroll_,
            totalTiles_,
            paletteVisibleRows(),
            PALETTE_COLS,
            PALETTE_TILE_PX,
            kPaletteHeaderHeight,
            PALETTE_WIDTH));

    if (paletteMouse.overPalette) {
        if (paletteMouse.pageDirection != 0) {
            pagePalette(paletteMouse.pageDirection);
        }
        if (paletteMouse.selectedPaletteIndex >= 0) {
            selectedTile_ = paletteTileIds_[paletteMouse.selectedPaletteIndex];
            tool_ = Tool::Paint;
            editCollision_ = false;
            objectTool_ = ObjectTool::None;
        }
        if (paletteMouse.scrollDelta != 0) {
            paletteScroll_ += paletteMouse.scrollDelta;
            clampPaletteScroll();
        }
    } else {
        // Stage interaction
        float worldX = internalX + camX_;
        float worldY = internalY + camY_;
        int tx, ty;
        worldToTile(worldX, worldY, tx, ty);
        cursorX_ = tx;
        cursorY_ = ty;

        if (!controlHeld && IsKeyPressed(KEY_P)) {
            if (shiftHeld) deletePlayerSpawn();
            else placePlayerSpawnAt(worldX, worldY);
            return;
        }
        if (!controlHeld && IsKeyPressed(KEY_E)) {
            if (shiftHeld) deleteEnemySpawnNear(worldX, worldY);
            else placeEnemySpawnAt(worldX, worldY);
            return;
        }
        if (!controlHeld && IsKeyPressed(KEY_K)) {
            if (shiftHeld) deletePickupSpawnNear(worldX, worldY);
            else placePickupSpawnAt(worldX, worldY);
            return;
        }
        if (!controlHeld && IsKeyPressed(KEY_O)) {
            if (shiftHeld) deleteCheckpointNear(worldX, worldY);
            else placeCheckpointAt(worldX, worldY);
            return;
        }
        if (!controlHeld && IsKeyPressed(KEY_B)) {
            if (shiftHeld) deleteBossSpawnNear(worldX, worldY);
            else placeBossSpawnAt(worldX, worldY);
            return;
        }

        if (objectTool_ != ObjectTool::None) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (objectTool_ == ObjectTool::Player) {
                    placePlayerSpawnAt(worldX, worldY);
                } else if (objectTool_ == ObjectTool::Enemy) {
                    placeEnemySpawnAt(worldX, worldY);
                } else if (objectTool_ == ObjectTool::Pickup) {
                    placePickupSpawnAt(worldX, worldY);
                } else if (objectTool_ == ObjectTool::Boss) {
                    placeBossSpawnAt(worldX, worldY);
                }
                return;
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                if (objectTool_ == ObjectTool::Player) {
                    deletePlayerSpawn();
                } else if (objectTool_ == ObjectTool::Enemy) {
                    deleteEnemySpawnNear(worldX, worldY);
                } else if (objectTool_ == ObjectTool::Pickup) {
                    deletePickupSpawnNear(worldX, worldY);
                } else if (objectTool_ == ObjectTool::Boss) {
                    deleteBossSpawnNear(worldX, worldY);
                }
                return;
            }
            if (statusTimer_ > 0) statusTimer_--;
            return;
        }

        // Scroll palette with mouse wheel (when not over palette)
        float wheel = GetMouseWheelMove();
        if (wheel != 0 && IsKeyDown(KEY_LEFT_CONTROL)) {
            paletteScroll_ -= static_cast<int>(wheel);
            clampPaletteScroll();
        }

        // Fill is a one-shot click; paint/erase remain continuous.
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && tool_ == Tool::Fill &&
            !IsKeyDown(KEY_LEFT_ALT)) {
            fillTile(tx, ty);
        } else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            if (tool_ == Tool::Paint) paintTile(tx, ty);
            else if (tool_ == Tool::Erase) eraseTile(tx, ty);
            else if (tool_ == Tool::Pick) pickTile(tx, ty);
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            eraseTile(tx, ty);
        }
        // Middle click = pick
        if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
            pickTile(tx, ty);
        }
        // Alt+click = pick
        if (IsKeyDown(KEY_LEFT_ALT) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            pickTile(tx, ty);
        }
    }

    // Status timer
    if (statusTimer_ > 0) statusTimer_--;
}

void MapEditorScene::update(float /*dt*/) {
    // Editor is mostly input-driven, minimal update logic
}

map_editor::BrushEditState MapEditorScene::brushEditState() {
    map_editor::BrushEditState state;
    state.stageWidth = stageWidth_;
    state.stageHeight = stageHeight_;
    state.activeLayer = activeLayer_;
    state.selectedTile = selectedTile_;
    state.editCollision = editCollision_;
    state.collisionBrush = collisionBrush_;
    state.layerData = &layerData_;
    state.layerDataBg = &layerDataBG_;
    state.layerDataMainEditable = &layerDataMainEditable_;
    state.layerDataBgEditable = &layerDataBgEditable_;
    state.collision = &collision_;
    state.editorAttrs = &editorAttrs_; state.editorAttrOverrides = &editorAttrOverrides_;
    state.slopeEdits = &slopeEdits_;
    return state;
}

bool MapEditorScene::paintTileNoHistory(int tx, int ty) {
    auto state = brushEditState();
    return map_editor::paintTileNoHistory(state, tx, ty);
}

bool MapEditorScene::eraseTileNoHistory(int tx, int ty) {
    auto state = brushEditState();
    return map_editor::eraseTileNoHistory(state, tx, ty);
}

void MapEditorScene::paintTile(int tx, int ty) {
    const EditSnapshot before = captureEditSnapshot();
    if (paintTileNoHistory(tx, ty)) {
        recordEditSnapshotIfChanged(before);
    }
}

void MapEditorScene::eraseTile(int tx, int ty) {
    const EditSnapshot before = captureEditSnapshot();
    if (eraseTileNoHistory(tx, ty)) {
        recordEditSnapshotIfChanged(before);
    }
}

void MapEditorScene::fillTile(int tx, int ty) {
    const EditSnapshot before = captureEditSnapshot();
    auto state = brushEditState();
    const auto result = map_editor::fillTileNoHistory(state, tx, ty);
    if (!result.inBounds) return;
    if (result.noChange) {
        setStatus(uiText(UiText::MapEditorStatusFillNoChange, Settings::language));
        return;
    }
    if (result.changed) {
        recordEditSnapshotIfChanged(before);
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "%s %d %s",
             uiText(UiText::MapEditorStatusFillPrefix, Settings::language),
             result.tileCount,
             uiText(UiText::MapEditorStatusFillTilesSuffix, Settings::language));
    setStatus(buf);
}

void MapEditorScene::pickTile(int tx, int ty) {
    const int pickedTile = map_editor::pickTileId(brushEditState(), tx, ty);
    if (pickedTile >= 0) {
        selectedTile_ = pickedTile;
        tool_ = Tool::Paint;
    }
}

void MapEditorScene::placePlayerSpawnAt(float worldX, float worldY) {
    const EditSnapshot before = captureEditSnapshot();
    applySpawnActionResult(
        before,
        map_editor::placePlayerSpawnWithStatus(
            editorSpawns_, worldX, worldY, stageWidth_, stageHeight_, TILE_SIZE,
            uiText(UiText::MapEditorStatusPlayerSpawnSet, Settings::language),
            uiText(UiText::MapEditorStatusPlayerSpawnUnchanged, Settings::language)));
}

void MapEditorScene::placePlayerSpawnAtCursor() {
    if (stageWidth_ <= 0 || stageHeight_ <= 0) return;

    editCollision_ = false;
    objectTool_ = ObjectTool::Player;
    tool_ = Tool::Paint;

    const int tileX = std::clamp(cursorX_, 0, stageWidth_ - 1);
    const int tileY = std::clamp(cursorY_, 0, stageHeight_ - 1);
    placePlayerSpawnAt((static_cast<float>(tileX) + 0.5f) * TILE_SIZE,
                       (static_cast<float>(tileY) + 0.5f) * TILE_SIZE);
}

void MapEditorScene::deletePlayerSpawn() {
    const EditSnapshot before = captureEditSnapshot();
    applySpawnActionResult(
        before,
        map_editor::deleteSpawnsByTypeWithStatus(
            editorSpawns_, "player_spawn",
            uiText(UiText::MapEditorStatusPlayerSpawnDeleted, Settings::language),
            uiText(UiText::MapEditorStatusNoPlayerSpawn, Settings::language)));
}

std::string MapEditorScene::selectedEnemySpawnId() const {
    return map_editor::selectedSpawnCatalogId(enemySpawnIds_, selectedEnemySpawn_);
}

void MapEditorScene::selectEnemySpawn(int direction) {
    const std::string status = map_editor::selectSpawnCatalogWithStatus(
        enemySpawnIds_, selectedEnemySpawn_, direction,
        uiText(UiText::MapEditorStatusNoEnemyDefs, Settings::language),
        uiText(UiText::MapEditorStatusEnemyPrefix, Settings::language));
    setStatus(status.c_str());
}

std::string MapEditorScene::selectedPickupSpawnId() const {
    return map_editor::selectedSpawnCatalogId(pickupSpawnIds_, selectedPickupSpawn_);
}

void MapEditorScene::selectPickupSpawn(int direction) {
    const std::string status = map_editor::selectSpawnCatalogWithStatus(
        pickupSpawnIds_, selectedPickupSpawn_, direction,
        uiText(UiText::MapEditorStatusNoPickupDefs, Settings::language),
        uiText(UiText::MapEditorStatusPickupPrefix, Settings::language));
    setStatus(status.c_str());
}

std::string MapEditorScene::selectedBossSpawnId() const {
    return map_editor::selectedSpawnCatalogId(bossSpawnIds_, selectedBossSpawn_);
}

void MapEditorScene::selectBossSpawn(int direction) {
    const std::string status = map_editor::selectSpawnCatalogWithStatus(
        bossSpawnIds_, selectedBossSpawn_, direction,
        uiText(UiText::MapEditorStatusNoBossDefs, Settings::language),
        uiText(UiText::MapEditorStatusBossPrefix, Settings::language));
    setStatus(status.c_str());
}

void MapEditorScene::placeEnemySpawnAt(float worldX, float worldY) {
    const EditSnapshot before = captureEditSnapshot();
    applySpawnActionResult(
        before,
        map_editor::placeCatalogSpawnWithStatus(
            editorSpawns_, "enemy", selectedEnemySpawnId(), worldX, worldY,
            stageWidth_, stageHeight_, TILE_SIZE, 12.0f,
            uiText(UiText::MapEditorStatusNoEnemyDefs, Settings::language),
            uiText(UiText::MapEditorStatusEnemySetPrefix, Settings::language),
            uiText(UiText::MapEditorStatusEnemyUnchanged, Settings::language)));
}

void MapEditorScene::deleteEnemySpawnNear(float worldX, float worldY) {
    const EditSnapshot before = captureEditSnapshot();
    applySpawnActionResult(
        before,
        map_editor::deleteSpawnNearWithStatus(
            editorSpawns_, "enemy", worldX, worldY, 12.0f,
            uiText(UiText::MapEditorStatusEnemyDeleted, Settings::language),
            uiText(UiText::MapEditorStatusNoEnemyHere, Settings::language)));
}

void MapEditorScene::placePickupSpawnAt(float worldX, float worldY) {
    const EditSnapshot before = captureEditSnapshot();
    applySpawnActionResult(
        before,
        map_editor::placeCatalogSpawnWithStatus(
            editorSpawns_, "pickup", selectedPickupSpawnId(), worldX, worldY,
            stageWidth_, stageHeight_, TILE_SIZE, 12.0f,
            uiText(UiText::MapEditorStatusNoPickupDefs, Settings::language),
            uiText(UiText::MapEditorStatusPickupSetPrefix, Settings::language),
            uiText(UiText::MapEditorStatusPickupUnchanged, Settings::language)));
}

void MapEditorScene::deletePickupSpawnNear(float worldX, float worldY) {
    const EditSnapshot before = captureEditSnapshot();
    applySpawnActionResult(
        before,
        map_editor::deleteSpawnNearWithStatus(
            editorSpawns_, "pickup", worldX, worldY, 12.0f,
            uiText(UiText::MapEditorStatusPickupDeleted, Settings::language),
            uiText(UiText::MapEditorStatusNoPickupHere, Settings::language)));
}

void MapEditorScene::placeCheckpointAt(float worldX, float worldY) {
    const EditSnapshot before = captureEditSnapshot();
    applySpawnActionResult(
        before,
        map_editor::placeFixedSpawnWithStatus(
            editorSpawns_, "checkpoint", worldX, worldY,
            stageWidth_, stageHeight_, TILE_SIZE, 12.0f,
            uiText(UiText::MapEditorStatusCheckpointSet, Settings::language),
            uiText(UiText::MapEditorStatusCheckpointUnchanged, Settings::language)));
}

void MapEditorScene::deleteCheckpointNear(float worldX, float worldY) {
    const EditSnapshot before = captureEditSnapshot();
    applySpawnActionResult(
        before,
        map_editor::deleteSpawnNearWithStatus(
            editorSpawns_, "checkpoint", worldX, worldY, 12.0f,
            uiText(UiText::MapEditorStatusCheckpointDeleted, Settings::language),
            uiText(UiText::MapEditorStatusNoCheckpointHere, Settings::language)));
}

void MapEditorScene::placeBossSpawnAt(float worldX, float worldY) {
    const EditSnapshot before = captureEditSnapshot();
    applySpawnActionResult(
        before,
        map_editor::placeCatalogSpawnWithStatus(
            editorSpawns_, "boss", selectedBossSpawnId(), worldX, worldY,
            stageWidth_, stageHeight_, TILE_SIZE, 12.0f,
            uiText(UiText::MapEditorStatusNoBossDefs, Settings::language),
            uiText(UiText::MapEditorStatusBossSetPrefix, Settings::language),
            uiText(UiText::MapEditorStatusBossUnchanged, Settings::language)));
}

void MapEditorScene::deleteBossSpawnNear(float worldX, float worldY) {
    const EditSnapshot before = captureEditSnapshot();
    applySpawnActionResult(
        before,
        map_editor::deleteSpawnNearWithStatus(
            editorSpawns_, "boss", worldX, worldY, 12.0f,
            uiText(UiText::MapEditorStatusBossDeleted, Settings::language),
            uiText(UiText::MapEditorStatusNoBossHere, Settings::language)));
}

void MapEditorScene::resizeStage(int newWidth, int newHeight) {
    const EditSnapshot before = captureEditSnapshot();
    map_editor::StageResizeState state;
    state.stageWidth = &stageWidth_;
    state.stageHeight = &stageHeight_;
    state.layerData = &layerData_;
    state.layerDataBg = &layerDataBG_;
    state.layerDataMainEditable = &layerDataMainEditable_;
    state.layerDataBgEditable = &layerDataBgEditable_;
    state.collision = &collision_;
    state.slopeEdits = &slopeEdits_;
    if (!map_editor::resizeStageData(state, newWidth, newHeight)) return;

    map_editor::clampCamera(
        camX_, camY_, stageWidth_, stageHeight_, TILE_SIZE,
        stageAreaWidth(), INTERNAL_HEIGHT);
    recordEditSnapshotIfChanged(before);
}

// ============================================================================
// Rendering
// ============================================================================

void MapEditorScene::render(float /*alpha*/) {
    ClearBackground({16, 16, 32, 255});

    renderStage();
    if (showGrid_) {
        map_editor::renderGrid(stageWidth_, stageHeight_, stageAreaWidth(), camX_,
                               camY_, TILE_SIZE, INTERNAL_HEIGHT, kScreenTileWidth,
                               kScreenTileHeight, Settings::language);
    }
    if (showCollision_) {
        map_editor::renderCollisionOverlay(collision_, stageWidth_, stageHeight_,
                                           stageAreaWidth(), camX_, camY_, TILE_SIZE,
                                           INTERNAL_HEIGHT);
    }
    renderObjectOverlay();
    map_editor::renderCursor(tileset_, tilesetCols_, selectedTile_, cursorX_,
                             cursorY_, camX_, camY_, stageAreaWidth(),
                             INTERNAL_HEIGHT, TILE_SIZE, editCollision_);
    if (paletteVisible_) renderPalette();
    renderToolbar();
    map_editor::renderStatus(statusMsg_, statusTimer_, stageAreaWidth());
    if (saveNamePromptOpen_) {
        map_editor::renderSaveNamePrompt(INTERNAL_WIDTH, INTERNAL_HEIGHT,
                                         saveNameDraft_, timer_, Settings::language);
    }
    if (metadataPromptOpen_) {
        map_editor::renderMetadataPrompt(INTERNAL_WIDTH, INTERNAL_HEIGHT,
                                         metadataField_, metadataAuthorDraft_,
                                         metadataDescriptionDraft_, timer_,
                                         Settings::language);
    }
    if (loadBrowserOpen_) renderLoadBrowser();
    if (commandMenuOpen_) renderCommandMenu();
    if (optionPicker_ != OptionPicker::None) renderOptionPicker();
    if (helpOverlayVisible_) renderHelpOverlay();
    if (confirmExitPromptOpen_) {
        map_editor::renderExitConfirmPrompt(INTERNAL_WIDTH, INTERNAL_HEIGHT,
                                            Settings::language);
    }
}

void MapEditorScene::renderStage() {
    int areaW = stageAreaWidth();

    // Enable scissor to clip rendering to the stage area
    BeginScissorMode(0, 0, areaW, INTERNAL_HEIGHT);

    map_editor::renderPreviewLayer(
        map_editor::previewLayerRenderState(
            previewBg_, stageAreaWidth(), INTERNAL_HEIGHT, camX_, camY_),
        WHITE);
    map_editor::renderPreviewLayer(
        map_editor::previewLayerRenderState(
            previewMain_, stageAreaWidth(), INTERNAL_HEIGHT, camX_, camY_),
        (activeLayer_ == 1) ? Color{128, 128, 128, 255} : WHITE);

    // Render BG2 first (dimmed)
    map_editor::renderTileLayer(
        tileset_, layerDataBG_, tilesetCols_, stageWidth_, stageHeight_, areaW,
        camX_, camY_, TILE_SIZE, INTERNAL_HEIGHT,
        (activeLayer_ == 0) ? Color{128, 128, 128, 255} : WHITE);

    // Render BG1 (main)
    map_editor::renderTileLayer(
        tileset_, layerData_, tilesetCols_, stageWidth_, stageHeight_, areaW,
        camX_, camY_, TILE_SIZE, INTERNAL_HEIGHT,
        (activeLayer_ == 1) ? Color{128, 128, 128, 255} : WHITE);

    EndScissorMode();
}

void MapEditorScene::renderObjectOverlay() {
    map_editor::renderObjectOverlay(
        editorSpawns_,
        stageAreaWidth(),
        INTERNAL_HEIGHT,
        camX_,
        camY_,
        [this](const std::string& path) { return getCachedEditorTexture(path); });
}

void MapEditorScene::renderPalette() {
    map_editor::PaletteRenderState paletteState;
    paletteState.tileset = &tileset_;
    paletteState.paletteTileIds = &paletteTileIds_;
    paletteState.paletteX = stageAreaWidth();
    paletteState.paletteWidth = PALETTE_WIDTH;
    paletteState.internalHeight = INTERNAL_HEIGHT;
    paletteState.headerHeight = kPaletteHeaderHeight;
    paletteState.paletteCols = PALETTE_COLS;
    paletteState.paletteTilePx = PALETTE_TILE_PX;
    paletteState.tileSize = TILE_SIZE;
    paletteState.tilesetCols = tilesetCols_;
    paletteState.totalTiles = totalTiles_;
    paletteState.selectedTile = selectedTile_;
    paletteState.paletteScroll = paletteScroll_;
    paletteState.visibleRows = paletteVisibleRows();
    paletteState.maxScroll = paletteMaxScroll();
    paletteState.showAllPaletteTiles = showAllPaletteTiles_;
    paletteState.language = Settings::language;
    map_editor::renderPalette(paletteState);
}

void MapEditorScene::renderToolbar() {
    map_editor::renderToolbar(buildToolbarRenderState());
}

void MapEditorScene::renderHelpOverlay() {
    map_editor::renderHelpOverlay(INTERNAL_WIDTH, INTERNAL_HEIGHT, Settings::language);
}

void MapEditorScene::renderLoadBrowser() {
    const int count = static_cast<int>(userMapEntries_.size());
    const int selected = count > 0 ? std::clamp(userMapSelection_, 0, count - 1) : 0;
    const TextureResource* preview =
        count > 0 ? getLoadBrowserThumbnail(userMapEntries_[selected]) : nullptr;
    map_editor::renderLoadBrowser(INTERNAL_WIDTH,
                                  INTERNAL_HEIGHT,
                                  userMapEntries_,
                                  selected,
                                  preview,
                                  Settings::language);
}

void MapEditorScene::renderCommandMenu() {
    map_editor::renderCommandMenu(
        INTERNAL_WIDTH, INTERNAL_HEIGHT, commandMenuSelection_, Settings::language);
}

void MapEditorScene::renderOptionPicker() {
    const bool pickingTileset = optionPicker_ == OptionPicker::Tileset;
    const int count = pickingTileset
        ? static_cast<int>(availableTilesets_.size())
        : static_cast<int>(availableBackgrounds_.size());
    if (count <= 0 || optionPicker_ == OptionPicker::None) return;

    const auto pickerState = map_editor::buildOptionPickerRenderState(
        pickingTileset, availableTilesets_, selectedTileset_,
        availableBackgrounds_, selectedBackground_);
    map_editor::renderOptionPicker(
        INTERNAL_WIDTH,
        INTERNAL_HEIGHT,
        pickerState.title,
        pickerState.items,
        optionPickerSelection_,
        Settings::language);
}

} // namespace mmx
