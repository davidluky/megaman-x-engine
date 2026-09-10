// map_editor_scene_io.cpp - save/load, modal, and scene-transition methods.

#include "ui/map_editor_scene.h"
#include "ui/map_editor_dialogs.h"
#include "ui/map_editor_input.h"
#include "ui/map_editor_load_browser.h"
#include "ui/map_editor_save_load.h"
#include "ui/map_editor_stage_json.h"
#include "ui/map_editor_thumbnail.h"
#include "ui/map_editor_toolbar.h"
#include "ui/map_editor_tilesets.h"
#include "ui/extras_scene.h"
#include "ui/title_scene.h"
#include "gameplay/gameplay_scene.h"
#include "app/scene_manager.h"
#include "data/content_paths.h"
#include "data/json_io.h"
#include "data/localization.h"
#include "data/settings.h"
#include "systems/asset_cache.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <utility>

namespace mmx {

namespace {
namespace fs = std::filesystem;
}

void MapEditorScene::save(bool clearUnsaved) {
    std::filesystem::path outPath(savePath_);
    const std::filesystem::path thumbnailPath = map_editor::userMapThumbnailPath(outPath);

    nlohmann::json j;
    map_editor::readJsonFile(outPath, j);
    if (!j.is_object()) {
        j = nlohmann::json::object();
    }

    map_editor::MapSaveJsonState saveState;
    saveState.mapName = mapName_;
    saveState.mapAuthor = mapAuthor_;
    saveState.mapDescription = mapDescription_;
    saveState.tilesetPath = tilesetPath_;
    saveState.tilesetId = tilesetId_;
    saveState.thumbnailFilename = thumbnailPath.filename().generic_string();
    saveState.stageWidth = stageWidth_;
    saveState.stageHeight = stageHeight_;
    saveState.tileSize = TILE_SIZE;
    saveState.tilesetCols = tilesetCols_;
    saveState.selectedBackground = selectedBackground_;
    saveState.paletteExclude = &paletteExclude_;
    saveState.layerData = &layerData_;
    saveState.layerDataBg = &layerDataBG_;
    saveState.layerDataMainEditable = layerDataMainEditable_;
    saveState.layerDataBgEditable = layerDataBgEditable_;
    saveState.collision = &collision_;
    saveState.slopeEdits = &slopeEdits_;
    saveState.editorSpawns = &editorSpawns_;
    saveState.spawnsDirty = spawnsDirty_;
    saveState.availableBackgrounds = &availableBackgrounds_;
    saveState.previewMain = map_editor::savePreviewLayerFrom(previewMain_);
    saveState.previewBg = map_editor::savePreviewLayerFrom(previewBg_);
    j = map_editor::buildUserMapSaveJson(std::move(j), saveState);

    if (json_io::writeAtomically(outPath, j)) {
        const bool thumbnailSaved = map_editor::writeUserMapThumbnail(
            outPath,
            stageWidth_,
            stageHeight_,
            layerData_,
            layerDataBG_,
            collision_,
            editorSpawns_,
            TILE_SIZE);
        if (clearUnsaved) {
            hasUnsavedChanges_ = false;
            spawnsDirty_ = false;
        }
        setStatus(uiText(thumbnailSaved ? UiText::MapEditorStatusSaved
                                        : UiText::MapEditorStatusSavedNoPreview,
                         Settings::language));
    } else {
        setStatus(uiText(UiText::MapEditorStatusSaveFailed, Settings::language));
    }
}

void MapEditorScene::beginSaveAs() {
    saveNameDraft_ = map_editor::displayMapNameOrDefault(mapName_);
    if (!hasNamedSavePath_ && saveNameDraft_ == "Untitled Stage") {
        saveNameDraft_.clear();
    }
    saveNamePromptOpen_ = true;
    setStatus(uiText(UiText::MapEditorStatusNameMap, Settings::language));
}

void MapEditorScene::handleSaveNameInput() {
    map_editor::SaveNameInputState input;
    input.draft = saveNameDraft_;
    input.maxChars = MAX_MAP_NAME_CHARS;
    const map_editor::SaveNameInputResult result =
        map_editor::handleSaveNameInput(input);
    saveNameDraft_ = result.draft;

    if (result.action == map_editor::SaveNameInputAction::Cancel) {
        saveNamePromptOpen_ = false;
        setStatus(uiText(UiText::MapEditorStatusSaveAsCancelled, Settings::language));
        return;
    }

    if (result.action == map_editor::SaveNameInputAction::Confirm) {
        confirmSaveAs();
    }
}

void MapEditorScene::confirmSaveAs() {
    mapName_ = map_editor::displayMapNameOrDefault(saveNameDraft_);
    savePath_ = map_editor::userMapPathForName(mapName_).generic_string();
    hasNamedSavePath_ = true;
    saveNamePromptOpen_ = false;
    markUnsaved();
    save();
}

void MapEditorScene::saveOrPromptForName() {
    if (!hasNamedSavePath_) {
        beginSaveAs();
        return;
    }
    save();
}

void MapEditorScene::beginMetadataEdit() {
    metadataAuthorDraft_ = mapAuthor_;
    metadataDescriptionDraft_ = mapDescription_;
    metadataField_ = 0;
    metadataPromptOpen_ = true;
    setStatus(uiText(UiText::MapEditorStatusEditMapInfo, Settings::language));
}

void MapEditorScene::handleMetadataInput() {
    map_editor::MetadataInputState input;
    input.field = metadataField_;
    input.authorDraft = metadataAuthorDraft_;
    input.descriptionDraft = metadataDescriptionDraft_;
    input.maxAuthorChars = MAX_MAP_AUTHOR_CHARS;
    input.maxDescriptionChars = MAX_MAP_DESCRIPTION_CHARS;
    const map_editor::MetadataInputResult result =
        map_editor::handleMetadataInput(input);
    metadataField_ = result.field;
    metadataAuthorDraft_ = result.authorDraft;
    metadataDescriptionDraft_ = result.descriptionDraft;

    if (result.action == map_editor::MetadataInputAction::Cancel) {
        metadataPromptOpen_ = false;
        setStatus(uiText(UiText::MapEditorStatusMapInfoCancelled, Settings::language));
        return;
    }

    if (result.action == map_editor::MetadataInputAction::Confirm) {
        confirmMetadataEdit();
    }
}

void MapEditorScene::confirmMetadataEdit() {
    const bool changed = mapAuthor_ != metadataAuthorDraft_ ||
                         mapDescription_ != metadataDescriptionDraft_;
    mapAuthor_ = metadataAuthorDraft_;
    mapDescription_ = metadataDescriptionDraft_;
    if (changed) markUnsaved();
    metadataPromptOpen_ = false;
    setStatus(uiText(UiText::MapEditorStatusMapInfoUpdated, Settings::language));
}

void MapEditorScene::openLoadBrowser() {
    loadBrowserThumbnails_.clear();
    const map_editor::LoadBrowserOpenResult result =
        map_editor::openLoadBrowser();
    userMapEntries_ = result.entries;
    userMapSelection_ = result.selection;
    if (result.loadLegacySlot) {
        load(result.loadPath);
        return;
    }
    loadBrowserOpen_ = result.open;
    if (result.hasStatus) setStatus(uiText(result.status, Settings::language));
}

void MapEditorScene::handleLoadBrowserInput() {
    const map_editor::LoadBrowserInputResult result =
        map_editor::handleLoadBrowserInput(userMapEntries_, userMapSelection_);
    userMapSelection_ = result.selection;
    if (result.action == map_editor::LoadBrowserAction::CloseWithStatus) {
        loadBrowserOpen_ = false;
        setStatus(uiText(result.status, Settings::language));
    } else if (result.action == map_editor::LoadBrowserAction::LoadSelected) {
        savePath_ = result.loadPath;
        loadBrowserOpen_ = false;
        load(savePath_);
    }
}

const TextureResource* MapEditorScene::getLoadBrowserThumbnail(
    const map_editor::UserMapEntry& entry) {
    return map_editor::getLoadBrowserThumbnail(entry, loadBrowserThumbnails_);
}

const TextureResource* MapEditorScene::getCachedEditorTexture(
    const std::string& path) {
    if (path.empty()) return nullptr;

    auto it = objectPreviewTextures_.find(path);
    if (it != objectPreviewTextures_.end()) return it->second;

    const auto resolved = content_paths::resolveAssetPath(path);
    const std::string texturePath = resolved ? *resolved : path;
    const TextureResource* texture = nullptr;
    if (FileExists(texturePath.c_str())) {
        texture = AssetCache::loadTexture(texturePath);
        if (texture) {
            texture->setFilter(TEXTURE_FILTER_POINT);
        }
    }
    objectPreviewTextures_.emplace(path, texture);
    return texture;
}

void MapEditorScene::openCommandMenu() {
    commandMenuOpen_ = true;
    commandMenuSelection_ = 0;
    optionPicker_ = OptionPicker::None;
    setStatus(uiText(UiText::MapEditorCommandMenuTitle, Settings::language));
}

void MapEditorScene::handleCommandMenuInput() {
    const screen_transform::InternalPoint internalMouse =
        map_editor::currentInternalMouse(Settings::aspect43);

    map_editor::CommandMenuInputState input;
    input.internalWidth = INTERNAL_WIDTH;
    input.internalHeight = INTERNAL_HEIGHT;
    input.selection = commandMenuSelection_;
    input.escapePressed = IsKeyPressed(KEY_ESCAPE);
    input.upPressed = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W);
    input.downPressed = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S);
    input.confirmPressed = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
    input.mouseInside = internalMouse.inside;
    input.mouseX = internalMouse.x;
    input.mouseY = internalMouse.y;
    input.leftMousePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    input.rightMousePressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

    const auto result = map_editor::handleCommandMenuInput(input);
    commandMenuSelection_ = result.selection;
    if (result.action == map_editor::CommandMenuInputAction::Cancel) {
        commandMenuOpen_ = false;
        setStatus(uiText(UiText::MapEditorStatusCommandCancelled, Settings::language));
        return;
    }
    if (result.action == map_editor::CommandMenuInputAction::Activate) {
        activateCommandMenuSelection();
        return;
    }

    if (result.tickStatusTimer && statusTimer_ > 0) statusTimer_--;
}

void MapEditorScene::activateCommandMenuSelection() {
    const int command =
        std::clamp(commandMenuSelection_, 0, map_editor::kCommandMenuItemCount - 1);
    commandMenuOpen_ = false;
    if (command == 0) {
        saveOrPromptForName();
    } else if (command == 1) {
        openLoadBrowser();
    } else if (command == 2) {
        placePlayerSpawnAtCursor();
    } else if (command == 3) {
        testPlay();
    } else {
        openExtras();
    }
}

void MapEditorScene::openTilesetPicker() {
    if (availableTilesets_.empty()) return;
    optionPicker_ = OptionPicker::Tileset;
    optionPickerSelection_ =
        std::clamp(selectedTileset_, 0, static_cast<int>(availableTilesets_.size()) - 1);
}

void MapEditorScene::openBackgroundPicker() {
    if (availableBackgrounds_.empty()) {
        setStatus(uiText(UiText::MapEditorStatusNoBackgrounds, Settings::language));
        return;
    }
    updateSelectedBackgroundFromPreview();
    optionPicker_ = OptionPicker::Background;
    optionPickerSelection_ = selectedBackground_ >= 0 ? selectedBackground_ : 0;
}

void MapEditorScene::handleOptionPickerInput() {
    const bool pickingTileset = optionPicker_ == OptionPicker::Tileset;
    const int count = pickingTileset
        ? static_cast<int>(availableTilesets_.size())
        : static_cast<int>(availableBackgrounds_.size());
    if (count <= 0 || optionPicker_ == OptionPicker::None) {
        optionPicker_ = OptionPicker::None;
        return;
    }

    auto applySelection = [&]() {
        if (pickingTileset) {
            selectTilesetIndex(optionPickerSelection_);
        } else {
            selectBackgroundIndex(optionPickerSelection_);
        }
        optionPicker_ = OptionPicker::None;
    };

    const screen_transform::InternalPoint internalMouse =
        map_editor::currentInternalMouse(Settings::aspect43);

    map_editor::OptionPickerInputState input;
    input.internalWidth = INTERNAL_WIDTH;
    input.internalHeight = INTERNAL_HEIGHT;
    input.selection = optionPickerSelection_;
    input.count = count;
    input.escapePressed = IsKeyPressed(KEY_ESCAPE);
    input.confirmPressed = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
    input.upPressed = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W);
    input.downPressed = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S);
    input.pageUpPressed = IsKeyPressed(KEY_PAGE_UP);
    input.pageDownPressed = IsKeyPressed(KEY_PAGE_DOWN);
    input.mouseInside = internalMouse.inside;
    input.mouseX = internalMouse.x;
    input.mouseY = internalMouse.y;
    input.leftMousePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    input.rightMousePressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    input.mouseWheel = GetMouseWheelMove();

    const auto result = map_editor::handleOptionPickerInput(input);
    optionPickerSelection_ = result.selection;
    if (result.action == map_editor::OptionPickerInputAction::Cancel) {
        optionPicker_ = OptionPicker::None;
        return;
    }
    if (result.action == map_editor::OptionPickerInputAction::Apply) {
        applySelection();
        return;
    }
    if (result.tickStatusTimer && statusTimer_ > 0) statusTimer_--;
}

void MapEditorScene::load(const std::string& path) {
    const auto read = json_io::readJsonObjectFromFile(path);
    if (!read.ok) {
        if (read.error == json_io::ReadError::Open) {
            setStatus(uiText(UiText::MapEditorStatusFileNotFound, Settings::language));
            return;
        }
        std::fprintf(stderr, "MapEditor: %s\n", read.message.c_str());
        setStatus(uiText(UiText::MapEditorStatusInvalidJson, Settings::language));
        return;
    }
    const nlohmann::json& j = read.value;
    const fs::path stageConfigPath(path);
    const map_editor::LoadedMapIdentity identity =
        map_editor::loadMapIdentity(j, stageConfigPath);
    hasNamedSavePath_ = identity.hasNamedSavePath;
    savePath_ = identity.savePath;
    mapName_ = identity.mapName;

    stageWidth_ = map_editor::positiveIntMemberOrDefault(j, "width", 32);
    stageHeight_ = map_editor::positiveIntMemberOrDefault(j, "height", 14);

    mapAuthor_ = identity.mapAuthor;
    mapDescription_ = identity.mapDescription;
    editorAttrs_.clear();
    editorAttrOverrides_.clear();
    UiText loadWarningStatus = UiText::MapEditorStatusLoaded;
    const auto tilesetChoice = map_editor::resolveLoadedMapTileset(
        j, stageConfigPath, availableTilesets_, identity.tilesetId);
    if (tilesetChoice.warning == map_editor::LoadedMapTilesetWarning::MissingTilesetId) {
        std::fprintf(stderr, "MapEditor: unknown tilesetId '%s' in %s\n",
                     tilesetChoice.missingTilesetId.c_str(), path.c_str());
        loadWarningStatus = UiText::MapEditorStatusMissingTilesetId;
    }
    if (tilesetChoice.selectedTileset >= 0) {
        selectedTileset_ = tilesetChoice.selectedTileset;
    }
    if (tilesetChoice.hasTileset) {
        loadTileset(tilesetChoice.path, tilesetChoice.cols, tilesetChoice.stageJson,
                    tilesetChoice.paletteExclude, tilesetChoice.id);
    }
    if (j.contains("attrs") && j["attrs"].is_array()) {
        editorAttrs_ = map_editor::intArrayValues(j["attrs"]);
    }
    if (j.contains("attrMap") && j["attrMap"].is_object()) {
        editorAttrOverrides_ = map_editor::parseAttrCollisionOverrides(j);
    }

    const map_editor::LoadedMapLayers loadedLayers =
        map_editor::loadMapLayers(j, stageWidth_, stageHeight_);
    layerData_ = loadedLayers.main;
    layerDataBG_ = loadedLayers.bg;
    layerDataMainEditable_ = loadedLayers.mainEditable;
    layerDataBgEditable_ = loadedLayers.bgEditable;
    map_editor::applyLoadedPreviewLayer(previewMain_, loadedLayers.previewMain);
    map_editor::applyLoadedPreviewLayer(previewBg_, loadedLayers.previewBg);
    if (rawTileCount_ > 0 && j.contains("layers") && j["layers"].is_array()) {
        const std::vector<int> mapPaletteTileIds =
            map_editor::buildStagePaletteTileIds(j, rawTileCount_, paletteExclude_);
        if (!mapPaletteTileIds.empty()) {
            stagePaletteTileIds_ = mapPaletteTileIds;
            refreshPaletteTileIds();
        }
    }
    updateSelectedBackgroundFromPreview();
    if (previewBg_.path.empty() && !identity.backgroundId.empty()) {
        const int backgroundIndex =
            map_editor::backgroundIndexForId(availableBackgrounds_, identity.backgroundId);
        if (backgroundIndex >= 0) {
            selectedBackground_ = backgroundIndex;
            applyBackground(availableBackgrounds_[selectedBackground_]);
        } else {
            std::fprintf(stderr, "MapEditor: unknown backgroundId '%s' in %s\n",
                         identity.backgroundId.c_str(), path.c_str());
            loadWarningStatus = UiText::MapEditorStatusMissingBackgroundId;
        }
    }

    slopeEdits_.clear();
    collision_ = map_editor::loadMapCollision(j, stageWidth_, stageHeight_);
    const bool normalizedAttr10 = map_editor::normalizeAttr10Collision(
        collision_, layerData_, editorAttrs_, editorAttrOverrides_);
    if (normalizedAttr10) {
        std::fprintf(stderr, "MapEditor: normalized stale attr 0x10 collision in %s\n",
                     path.c_str());
    }
    editorSpawns_ = map_editor::loadEditorSpawns(j);
    spawnsDirty_ = false;

    camX_ = 0;
    camY_ = 0;
    undoStack_.clear();
    redoStack_.clear();
    hasUnsavedChanges_ = normalizedAttr10;
    objectTool_ = ObjectTool::None;
    optionPicker_ = OptionPicker::None;
    loadBrowserOpen_ = false;
    commandMenuOpen_ = false;
    confirmExitPromptOpen_ = false;
    setStatus(uiText(loadWarningStatus, Settings::language));
}

void MapEditorScene::requestExitToTitle() {
    if (hasUnsavedChanges_) {
        confirmExitPromptOpen_ = true;
        confirmExitToExtras_ = false;
        setStatus(uiText(UiText::MapEditorUnsavedTitle, Settings::language));
        return;
    }
    returnToTitle();
}

void MapEditorScene::returnToTitle() {
    if (!sceneManager_) return;
    auto title = std::make_unique<TitleScene>();
    title->setSceneManager(sceneManager_);
    sceneManager_->changeScene(std::move(title));
}

void MapEditorScene::openExtras() {
    if (!sceneManager_) return;
    if (hasUnsavedChanges_) {
        confirmExitPromptOpen_ = true;
        confirmExitToExtras_ = true;
        setStatus(uiText(UiText::MapEditorUnsavedTitle, Settings::language));
        return;
    }

    auto extras = std::make_unique<ExtrasScene>();
    ContentPack extrasPack;
    extrasPack.loadFromFile("content/extras/manifest.json");
    extras->setContentPack(extrasPack);
    extras->setSceneManager(sceneManager_);
    sceneManager_->changeScene(std::move(extras));
}

void MapEditorScene::handleExitConfirmInput() {
    const map_editor::ExitConfirmInputResult result =
        map_editor::handleExitConfirmInput();
    if (result.action == map_editor::ExitConfirmInputAction::Cancel) {
        confirmExitPromptOpen_ = false;
        confirmExitToExtras_ = false;
        setStatus(uiText(UiText::MapEditorStatusExitCancelled, Settings::language));
        return;
    }

    if (result.action == map_editor::ExitConfirmInputAction::Confirm) {
        const bool exitToExtras = confirmExitToExtras_;
        confirmExitPromptOpen_ = false;
        confirmExitToExtras_ = false;
        hasUnsavedChanges_ = false;
        if (exitToExtras) {
            openExtras();
        } else {
            returnToTitle();
        }
    }
}

void MapEditorScene::testPlay() {
    std::string tempPath = map_editor::editorTestMapPath().generic_string();
    std::string prevSavePath = savePath_;
    savePath_ = tempPath;
    save(false);
    savePath_ = prevSavePath;

    if (!sceneManager_) return;

    GameplaySceneConfig gameplayConfig;
    gameplayConfig.stagePath = tempPath;
    gameplayConfig.characterPath = contentPack_.resolvePath("characters/x.json");
    auto gameplay = std::make_unique<GameplayScene>(gameplayConfig);
    gameplay->setSceneManager(sceneManager_);
    sceneManager_->pushScene(std::move(gameplay));
}

void MapEditorScene::setStatus(const char* msg) {
    statusMsg_ = msg;
    statusTimer_ = 180; // 3 seconds at 60fps
}

} // namespace mmx
