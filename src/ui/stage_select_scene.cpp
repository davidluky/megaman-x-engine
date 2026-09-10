// stage_select_scene.cpp - runs the stage-select map, cursor, and utilities.
// Owns: cursor routing, stage previews, randomizer tabs, and launch handoff.

#include "ui/stage_select_scene.h"
#include "ui/title_scene.h"
#include "ui/boss_intro_scene.h"
#include "app/scene_manager.h"
#include "gameplay/gameplay_scene.h"
#include "app/input.h"
#include "data/localization.h"
#include "data/settings.h"
#include "data/x1_catalog.h"
#include "systems/asset_cache.h"
#include "systems/audio.h"
#include "data/save_system.h"
#include <memory>
#include <cmath>
#include <string>
#include <algorithm>
#include <cstdio>

namespace mmx {

static bool prerequisitesMet(const StageInfo& stage) {
    for (const auto& prereq : stage.prerequisites) {
        if (!SaveSystem::isStageCompleted(StageId::fromString(prereq))) return false;
    }
    return true;
}

static Color bossColorForBoss(BossId bossId) {
    static const Color colors[] = {
        {80, 160, 220, 255},
        {100, 180, 100, 255},
        {220, 100, 40, 255},
        {255, 220, 60, 255},
        {180, 140, 100, 255},
        {60, 120, 200, 255},
        {120, 200, 80, 255},
        {160, 80, 200, 255},
    };

    const auto& bosses = x1_catalog::maverickBosses();
    constexpr size_t colorCount = sizeof(colors) / sizeof(colors[0]);
    for (size_t i = 0; i < bosses.size() && i < colorCount; ++i) {
        if (bosses[i].boss == bossId) return colors[i];
    }
    return {100, 100, 120, 255};
}

using StageSelectRouteModel = x1_catalog::StageSelectRouteModel;

static const StageSelectRouteModel* maverickRouteModelForGrid(int gridPos) {
    for (const auto& entry : x1_catalog::maverickStageSelectRoute()) {
        if (entry.gridPosition == gridPos) return &entry;
    }
    return nullptr;
}

static const StageSelectRouteModel* maverickRouteModelForOriginalCursorId(int cursorId) {
    for (const auto& entry : x1_catalog::maverickStageSelectRoute()) {
        if (entry.originalCursorId == cursorId) return &entry;
    }
    return nullptr;
}

static int originalCursorIdForGrid(int gridPos) {
    const auto* entry = maverickRouteModelForGrid(gridPos);
    return entry ? entry->originalCursorId : -1;
}

static int gridForOriginalCursorId(int cursorId) {
    const auto* entry = maverickRouteModelForOriginalCursorId(cursorId);
    return entry ? entry->gridPosition : -1;
}

constexpr int absInt(int v) {
    return v < 0 ? -v : v;
}

static int ringTransitionForInput(int cursorId, int dx, int dy) {
    if (dx == 0 && dy == 0) return cursorId;
    const StageSelectRouteModel* current = maverickRouteModelForOriginalCursorId(cursorId);
    if (!current) return cursorId;

    int bestCursor = cursorId;
    int bestScore = 999;
    for (const auto& candidate : x1_catalog::maverickStageSelectRoute()) {
        if (candidate.originalCursorId == cursorId || candidate.originalCursorId < 1) continue;

        const int primary = dx != 0
            ? (candidate.ringX - current->ringX) * dx
            : (candidate.ringY - current->ringY) * dy;
        if (primary <= 0) continue;

        const int perpendicular = dx != 0
            ? absInt(candidate.ringY - current->ringY)
            : absInt(candidate.ringX - current->ringX);
        const int score = primary * 16 + perpendicular;
        if (score < bestScore) {
            bestScore = score;
            bestCursor = candidate.originalCursorId;
        }
    }
    return bestCursor;
}

static int originalTransitionForInput(int cursorId, int dx, int dy) {
    if (cursorId < 1 || cursorId > 8) return cursorId;
    return ringTransitionForInput(cursorId, dx, dy);
}

struct StageSelectUtilityTab {
    Rectangle bounds;
    const char* label;
};

static Rectangle hotspotBoundsFor(const StageSelectRouteModel& entry) {
    return Rectangle{entry.hotspotX, entry.hotspotY, entry.hotspotW, entry.hotspotH};
}

static int stageSelectBossHotspotAt(Vector2 internalMouse) {
    // The model keeps hotspots beside the original cursor and screenshot ids,
    // so click targets cannot drift from exact-screen rendering.
    for (const auto& entry : x1_catalog::maverickStageSelectRoute()) {
        if (entry.originalCursorId < 1) continue;
        if (CheckCollisionPointRec(internalMouse, hotspotBoundsFor(entry))) {
            return entry.originalCursorId;
        }
    }
    return -1;
}

static const std::array<StageSelectUtilityTab, 4>& stageSelectUtilityTabs() {
    static const std::array<StageSelectUtilityTab, 4> tabs{{
        {Rectangle{34.0f, 17.0f, 47.0f, 47.0f}, "STAGE"},
        {Rectangle{175.0f, 17.0f, 47.0f, 47.0f}, "MAP"},
        {Rectangle{34.0f, 160.0f, 47.0f, 47.0f}, "SPEC"},
        {Rectangle{175.0f, 160.0f, 47.0f, 47.0f}, "MEGA"},
    }};
    return tabs;
}

static Vector2 stageSelectInternalMousePosition() {
    const Vector2 rawMouse = GetMousePosition();
    const float screenW = static_cast<float>(GetScreenWidth());
    const float screenH = static_cast<float>(GetScreenHeight());
    if (screenW <= 0.0f || screenH <= 0.0f) return {-1.0f, -1.0f};

    const float scale = std::min(screenW / static_cast<float>(INTERNAL_WIDTH),
                                 screenH / static_cast<float>(INTERNAL_HEIGHT));
    if (scale <= 0.0f) return {-1.0f, -1.0f};

    const float offsetX = (screenW - static_cast<float>(INTERNAL_WIDTH) * scale) * 0.5f;
    const float offsetY = (screenH - static_cast<float>(INTERNAL_HEIGHT) * scale) * 0.5f;
    return Vector2{(rawMouse.x - offsetX) / scale, (rawMouse.y - offsetY) / scale};
}

static int stageSelectUtilityTabAt(Vector2 internalMouse) {
    const auto& tabs = stageSelectUtilityTabs();
    for (int i = 0; i < static_cast<int>(tabs.size()); ++i) {
        if (CheckCollisionPointRec(internalMouse, tabs[i].bounds)) return i;
    }
    return -1;
}

static void drawStageSelectUtilityHover(int tabIndex, int timer) {
    const auto& tabs = stageSelectUtilityTabs();
    if (tabIndex < 0 || tabIndex >= static_cast<int>(tabs.size())) return;

    const Rectangle b = tabs[tabIndex].bounds;
    const int pulse = static_cast<int>(std::sin(timer * 0.12f) * 32.0f + 190.0f);
    Color line{
        247,
        static_cast<unsigned char>(std::clamp(pulse, 0, 255)),
        72,
        255
    };
    DrawRectangleRec(b, {255, 232, 96, 42});
    DrawRectangleLinesEx(b, 2.0f, line);
}

static void drawStageSelectStatusOverlay(const StageInfo& stage, bool prerequisitesOk) {
    if (stage.available && prerequisitesOk) {
        return;
    }
    const char* message = !stage.available
        ? uiText(UiText::StageUnavailable, Settings::language)
        : uiText(UiText::StageClearPrevious, Settings::language);
    DrawRectangle(35, INTERNAL_HEIGHT - 31, 186, 21, {0, 0, 0, 190});
    DrawRectangleLines(35, INTERNAL_HEIGHT - 31, 186, 21, {210, 235, 255, 255});
    const int w = MeasureText(message, 8);
    DrawText(message, (INTERNAL_WIDTH - w) / 2, INTERNAL_HEIGHT - 24, 8, {255, 210, 76, 255});
}

static void drawDynamicStageSelectBackdrop() {
    ClearBackground({68, 68, 76, 255});

    for (int y = 11; y < INTERNAL_HEIGHT; y += 16) {
        DrawRectangle(0, y, INTERNAL_WIDTH, 3, {34, 38, 48, 255});
        DrawRectangle(0, y + 3, INTERNAL_WIDTH, 1, {126, 136, 146, 255});
    }

    DrawRectangle(26, 8, 204, 208, {22, 48, 86, 255});
    DrawRectangleLines(26, 8, 204, 208, {218, 238, 255, 255});
    DrawRectangleLines(28, 10, 200, 204, {82, 154, 238, 255});
    DrawRectangle(33, 16, 190, 192, {0, 16, 52, 255});
    DrawRectangleLines(34, 17, 188, 190, {0, 100, 146, 255});

    const struct { int x; int y; const char* label; } utilityTabs[] = {
        {39, 22, "STAGE"},
        {178, 22, "MAP"},
        {39, 148, "SPEC"},
        {178, 148, "MEGA"},
    };
    for (const auto& tab : utilityTabs) {
        DrawRectangle(tab.x, tab.y, 36, 30, {0, 38, 68, 255});
        DrawRectangleLines(tab.x, tab.y, 36, 30, {188, 222, 245, 255});
        DrawText(tab.label, tab.x + 5, tab.y + 8, 7, {118, 230, 126, 255});
        DrawRectangle(tab.x + 8, tab.y + 21, 20, 3, {120, 138, 160, 255});
    }

    DrawRectangle(76, 70, 104, 75, {6, 24, 58, 255});
    DrawRectangleLines(76, 70, 104, 75, {220, 236, 255, 255});
    DrawRectangleLines(79, 73, 98, 69, {82, 154, 238, 255});
}

void StageSelectScene::setRandomizer(uint64_t seed, const RandomizerConfig& config) {
    randomizerMode_ = true;
    randomizerSeed_ = seed;
    randomizerConfig_ = config;
}

const StageInfo* StageSelectScene::getStageForGrid(int gridPos) const {
    int order = stageOrderForGrid(gridPos);
    if (order < 0) return nullptr;
    for (const auto& s : contentPack_.stages) {
        if (s.order == order) return &s;
    }
    return nullptr;
}

int StageSelectScene::stageOrderForGrid(int gridPos) const {
    if (gridPos < 0 || gridPos >= GRID_SIZE || gridPos == CENTER) return -1;

    const auto& route = (routeMode_ == RouteMode::Fortress)
        ? x1_catalog::sigmaFortressRoute()
        : x1_catalog::maverickStageSelectRoute();

    for (const auto& entry : route) {
        if (entry.gridPosition == gridPos) return entry.order;
    }
    return -1;
}

bool StageSelectScene::fortressRouteUnlocked() const {
    for (const auto& entry : x1_catalog::sigmaFortressRoute()) {
        for (const auto& stage : contentPack_.stages) {
            if (stage.order == entry.order && stage.available && prerequisitesMet(stage)) {
                return true;
            }
        }
    }
    return false;
}

bool StageSelectScene::isGridSelectable(int gridPos) const {
    return getStageForGrid(gridPos) != nullptr;
}

void StageSelectScene::refreshRouteMode() {
    routeMode_ = fortressRouteUnlocked() ? RouteMode::Fortress : RouteMode::Mavericks;
}

void StageSelectScene::placeCursorOnFirstStage() {
    // MMX1 enters stage select with Launch Octopus highlighted. Keep that as
    // the canonical starting cursor when the Maverick route is available.
    if (routeMode_ == RouteMode::Mavericks && isGridSelectable(gridForOriginalCursorId(1))) {
        cursorPos_ = gridForOriginalCursorId(1);
        return;
    }

    for (int i = 0; i < GRID_SIZE; ++i) {
        if (isGridSelectable(i)) {
            cursorPos_ = i;
            return;
        }
    }
    cursorPos_ = 0;
}

std::optional<BossId> StageSelectScene::getBossForGrid(int gridPos) const {
    if (gridPos < 0 || gridPos >= GRID_SIZE) return std::nullopt;
    return bossGrid_[gridPos];
}

void StageSelectScene::refreshBossGrid() {
    for (auto& bossId : bossGrid_) {
        bossId.reset();
    }
    if (routeMode_ == RouteMode::Fortress) return;

    std::optional<Randomizer> rng;
    if (randomizerMode_) {
        rng.emplace(randomizerSeed_);
        rng->setConfig(randomizerConfig_);
    }

    for (int gridPos = 0; gridPos < GRID_SIZE; ++gridPos) {
        if (gridPos == CENTER) continue;
        const StageInfo* stage = getStageForGrid(gridPos);
        if (!stage) continue;

        const StageId stageId = StageId::fromString(stage->id);
        if (rng.has_value()) {
            if (auto bossId = rng->bossForStage(stageId)) {
                bossGrid_[gridPos] = bossId;
                continue;
            }
        }

        if (!stage->boss.empty()) {
            bossGrid_[gridPos] = BossId::fromString(stage->boss);
        }
    }
}

const char* StageSelectScene::getBossLabel(BossId bossId) const {
    if (const auto* boss = x1_catalog::findMaverickByBoss(bossId)) {
        return boss->displayName;
    }
    return "";
}

Color StageSelectScene::getBossColorForGrid(int gridPos) const {
    if (auto bossId = getBossForGrid(gridPos)) {
        return bossColorForBoss(*bossId);
    }
    return {100, 100, 120, 255};
}

void StageSelectScene::onEnter() {
    refreshRouteMode();
    placeCursorOnFirstStage();
    timer_ = 0;
    transitioning_ = false;
    returnToTitle_ = false;
    transTimer_ = 0;
    hoveredUtilityTab_ = -1;

    refreshBossGrid();
    loadPortraits();

    AudioManager::playBGM("stage-select");

    fadeInTimer_ = FADE_IN_DURATION;
}

void StageSelectScene::onExit() {
    unloadPortraits();
}

void StageSelectScene::loadPortraits() {
    if (portraitsLoaded_) return;
    portraits_.fill(nullptr);
    stageSelectScreens_.fill(nullptr);
    stageSelectBlinkScreens_.fill(nullptr);
    xPortrait_ = nullptr;

    const char* base = "content/x1/sprites/boss_portraits/";
    for (int i = 0; i < 9; i++) {
        auto bossId = getBossForGrid(i);
        if (!bossId.has_value()) continue;
        std::string path = std::string(base) + bossId->str() + ".png";
        if (FileExists(path.c_str())) {
            portraits_[i] = AssetCache::loadTexture(path);
            if (portraits_[i]) {
                portraits_[i]->setFilter(TEXTURE_FILTER_POINT);
            }
        }
    }
    // Load X mugshot for center
    const char* xPath = "content/x1/sprites/misc/mmx_x_mugshot.png";
    if (FileExists(xPath)) {
        xPortrait_ = AssetCache::loadTexture(xPath);
        if (xPortrait_) {
            xPortrait_->setFilter(TEXTURE_FILTER_POINT);
        }
    }

    for (const auto& entry : x1_catalog::maverickStageSelectRoute()) {
        if (entry.originalCursorId < 1 || entry.screenshotId[0] == '\0') continue;
        const int gridPos = entry.gridPosition;
        if (gridPos < 0 || gridPos >= GRID_SIZE) continue;

        char path[96];
        snprintf(path, sizeof(path), "content/x1/sprites/stage_select/%s.png", entry.screenshotId);
        if (FileExists(path)) {
            stageSelectScreens_[gridPos] = AssetCache::loadTexture(path);
            if (stageSelectScreens_[gridPos]) {
                stageSelectScreens_[gridPos]->setFilter(TEXTURE_FILTER_POINT);
            }
        }
        snprintf(path, sizeof(path),
                 "content/x1/sprites/stage_select/%s_blink.png",
                 entry.screenshotId);
        if (FileExists(path)) {
            stageSelectBlinkScreens_[gridPos] = AssetCache::loadTexture(path);
            if (stageSelectBlinkScreens_[gridPos]) {
                stageSelectBlinkScreens_[gridPos]->setFilter(TEXTURE_FILTER_POINT);
            }
        }
    }
    portraitsLoaded_ = true;
}

void StageSelectScene::unloadPortraits() {
    if (!portraitsLoaded_) return;
    portraits_.fill(nullptr);
    stageSelectScreens_.fill(nullptr);
    stageSelectBlinkScreens_.fill(nullptr);
    xPortrait_ = nullptr;
    portraitsLoaded_ = false;
}

void StageSelectScene::pollInput() {
    // Handled globally
}

void StageSelectScene::confirmCurrentStage() {
    const StageInfo* stage = getStageForGrid(cursorPos_);
    if (stage && stage->available && prerequisitesMet(*stage)) {
        AudioManager::playSFX(SFX::MenuSelect);
        transitioning_ = true;
        transTimer_ = 0;
    }
}

void StageSelectScene::handleInput() {
    const Vector2 internalMouse = stageSelectInternalMousePosition();
    const bool exactMaverickScreen = routeMode_ == RouteMode::Mavericks && !randomizerMode_;
    const int hoveredBossCursorId = exactMaverickScreen
        ? stageSelectBossHotspotAt(internalMouse)
        : -1;
    hoveredUtilityTab_ = exactMaverickScreen
        ? -1
        : stageSelectUtilityTabAt(internalMouse);
    if (transitioning_) return;

    // R282/R288: a parked pointer must not undo keyboard/gamepad selection
    // on every repeat-delay frame. Moving or clicking takes mouse ownership.
    const Vector2 mouseDelta = GetMouseDelta();
    const bool pointerActive = mouseDelta.x != 0.0f || mouseDelta.y != 0.0f ||
                               IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    if (pointerActive && hoveredBossCursorId >= 1 && hoveredBossCursorId <= 8) {
        const int hoveredGrid = gridForOriginalCursorId(hoveredBossCursorId);
        if (hoveredGrid >= 0 && isGridSelectable(hoveredGrid)) {
            if (cursorPos_ != hoveredGrid) {
                cursorPos_ = hoveredGrid;
                AudioManager::playSFX(SFX::MenuMove);
            }
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                confirmCurrentStage();
                return;
            }
        }
    }

    if (hoveredUtilityTab_ >= 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        AudioManager::playSFX(SFX::MenuMove);
    }

    // Navigate grid with repeat delay
    if (inputDelay_ > 0) {
        inputDelay_--;
    } else {
        bool moved = false;
        if (Input::isLeftHeld())  { moveCursor(-1, 0); moved = true; }
        if (Input::isRightHeld()) { moveCursor(1, 0);  moved = true; }
        if (Input::isUpHeld())    { moveCursor(0, -1);  moved = true; }
        if (Input::isDownHeld())  { moveCursor(0, 1);   moved = true; }
        if (moved) {
            inputDelay_ = 10;
            AudioManager::playSFX(SFX::MenuMove);
        }
    }
    if (!Input::isLeftHeld() && !Input::isRightHeld() &&
        !Input::isUpHeld() && !Input::isDownHeld()) {
        inputDelay_ = 0;
    }

    // Confirm selection
    if (Input::isConfirmPressed() || Input::isJumpPressed()) {
        Input::consumeConfirmPress();
        Input::consumeJumpPress();

        confirmCurrentStage();
    }

    // Back to title
    if (Input::isCancelPressed()) {
        Input::consumeCancelPress();
        AudioManager::playSFX(SFX::MenuCancel);
        transitioning_ = true;
        returnToTitle_ = true;
        transTimer_ = 0;
    }
}

void StageSelectScene::moveCursor(int dx, int dy) {
    if (dx == 0 && dy == 0) return;

    if (routeMode_ == RouteMode::Mavericks) {
        const int cursorId = originalCursorIdForGrid(cursorPos_);
        const int nextCursorId = originalTransitionForInput(cursorId, dx, dy);
        const int nextGrid = gridForOriginalCursorId(nextCursorId);
        if (nextGrid >= 0 && isGridSelectable(nextGrid)) {
            cursorPos_ = nextGrid;
            return;
        }
    }

    int col = cursorPos_ % 3;
    int row = cursorPos_ / 3;

    for (int attempt = 0; attempt < GRID_SIZE; ++attempt) {
        col += dx;
        row += dy;
        if (col < 0) col = 2;
        if (col > 2) col = 0;
        if (row < 0) row = 2;
        if (row > 2) row = 0;

        int newPos = row * 3 + col;
        if (newPos != CENTER && isGridSelectable(newPos)) {
            cursorPos_ = newPos;
            return;
        }
    }
}

void StageSelectScene::update(float /*dt*/) {
    timer_++;
    blinkTick_ = (blinkTick_ + 1) % 23;   // U51 measured highlight cadence
    if (fadeInTimer_ > 0) fadeInTimer_--;

    if (transitioning_) {
        transTimer_++;
        if (transTimer_ > 30 && sceneManager_) {
            if (returnToTitle_) {
                auto title = std::make_unique<TitleScene>();
                title->setSceneManager(sceneManager_);
                sceneManager_->changeScene(std::move(title));
            } else {
                const StageInfo* stage = getStageForGrid(cursorPos_);
                if (stage) {
                    const std::string stagePath = contentPack_.resolvePath(stage->config);
                    const StageId sid = StageId::fromString(stage->id);
                    const std::string charPath = contentPack_.resolvePath("characters/x.json");

                    // Real MMX1 plays the boss-intro cutscene only for stages
                    // not yet cleared, and not in randomizer mode.
                    const bool playIntro = !randomizerMode_ &&
                                           !SaveSystem::isStageCompleted(sid);
                    if (playIntro) {
                        auto intro = std::make_unique<BossIntroScene>();
                        intro->setSceneManager(sceneManager_);
                        intro->stageId = stage->id;
                        intro->stagePath = stagePath;
                        intro->stageIdEnum = sid;
                        intro->characterPath = charPath;
                        sceneManager_->changeScene(std::move(intro));
                    } else {
                        GameplaySceneConfig gameplayConfig;
                        gameplayConfig.stagePath = stagePath;
                        gameplayConfig.stageId = sid;
                        gameplayConfig.characterPath = charPath;
                        gameplayConfig.playWarpIn = true;  // X warps into the stage
                        gameplayConfig.randomizerMode = randomizerMode_;
                        gameplayConfig.randomizerSeed = randomizerSeed_;
                        gameplayConfig.randomizerConfig = randomizerConfig_;
                        auto gameplay = std::make_unique<GameplayScene>(gameplayConfig);
                        gameplay->setSceneManager(sceneManager_);
                        sceneManager_->changeScene(std::move(gameplay));
                    }
                }
            }
        }
    }
}

void StageSelectScene::render(float /*alpha*/) {
    drawDynamicStageSelectBackdrop();

    if (routeMode_ == RouteMode::Mavericks && !randomizerMode_) {
        const TextureResource* originalScreen = stageSelectScreens_[cursorPos_];
        // U51: measured highlight blink — ON 7, OFF 4, ON 8, OFF 4 (period
        // 23). The ON phase is its own full-screen rip.
        const int bt = blinkTick_ % 23;
        const bool blinkOn = (bt < 7) || (bt >= 11 && bt < 19);
        if (blinkOn && stageSelectBlinkScreens_[cursorPos_] &&
            stageSelectBlinkScreens_[cursorPos_]->valid()) {
            originalScreen = stageSelectBlinkScreens_[cursorPos_];
        }
        if (originalScreen && originalScreen->valid()) {
            Rectangle src = {0.0f, 0.0f, static_cast<float>(originalScreen->width()), static_cast<float>(originalScreen->height())};
            Rectangle dst = {0.0f, 0.0f, static_cast<float>(INTERNAL_WIDTH), static_cast<float>(INTERNAL_HEIGHT)};
            DrawTexturePro(originalScreen->get(), src, dst, {0, 0}, 0.0f, WHITE);

            if (const StageInfo* selected = getStageForGrid(cursorPos_)) {
                drawStageSelectStatusOverlay(*selected, prerequisitesMet(*selected));
            }

            if (fadeInTimer_ > 0) {
                int fadeAlpha = 255 * fadeInTimer_ / FADE_IN_DURATION;
                DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
                              {0, 0, 0, static_cast<unsigned char>(fadeAlpha)});
            }

            if (transitioning_) {
                int fadeAlpha = std::min(255, transTimer_ * 8);
                DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
                              {0, 0, 0, static_cast<unsigned char>(fadeAlpha)});
            }
            return;
        }
    }

    // Title
    const UiText titleText = routeMode_ == RouteMode::Fortress
        ? UiText::TitleSigmaFortress
        : UiText::TitleStageSelect;
    const char* title = uiText(titleText, Settings::language);
    int titleW = MeasureText(title, 12);
    DrawText(title, (INTERNAL_WIDTH - titleW) / 2, 12, 12, {200, 200, 230, 255});

    // Grid layout
    constexpr int cellSize = 48;
    constexpr int gap = 8;
    constexpr int gridW = 3 * cellSize + 2 * gap;
    int gridX = (INTERNAL_WIDTH - gridW) / 2;
    int gridY = 34;

    for (int i = 0; i < GRID_SIZE; i++) {
        int col = i % 3;
        int row = i / 3;
        int cx = gridX + col * (cellSize + gap);
        int cy = gridY + row * (cellSize + gap);

        if (i == CENTER) {
            // Center cell: X portrait from mugshot
            DrawRectangle(cx, cy, cellSize, cellSize, {20, 20, 50, 255});
            DrawRectangleLines(cx, cy, cellSize, cellSize, {60, 120, 255, 200});

            if (xPortrait_ && xPortrait_->valid()) {
                // The mugshot asset is a strip; the stage-select center uses one
                // 32x32 portrait frame, not the full four-frame sheet.
                Rectangle src = {5.0f, 5.0f, 32.0f, 32.0f};
                float scale = std::min((float)(cellSize - 4) / src.width,
                                        (float)(cellSize - 4) / src.height);
                float w = src.width * scale;
                float h = src.height * scale;
                float dx = cx + (cellSize - w) / 2.0f;
                float dy = cy + (cellSize - h) / 2.0f;
                Rectangle dst = {dx, dy, w, h};
                DrawTexturePro(xPortrait_->get(), src, dst, {0, 0}, 0.0f, WHITE);
            }
            continue;
        }

        const StageInfo* stage = getStageForGrid(i);
        bool available = stage && stage->available;
        bool selected = (i == cursorPos_);

        // Cell background
        Color bgColor = available ? getBossColorForGrid(i) : Color{30, 30, 45, 255};
        if (!available) {
            // Locked — darken
            bgColor.r /= 3;
            bgColor.g /= 3;
            bgColor.b /= 3;
        }
        DrawRectangle(cx, cy, cellSize, cellSize, bgColor);

        if (stage) {
            Color portraitColor = available ? getBossColorForGrid(i) : Color{50, 50, 65, 255};
            DrawRectangle(cx + 2, cy + 2, cellSize - 4, cellSize - 4, portraitColor);

            const TextureResource* portrait = portraits_[i];
            bool hasPortrait = portrait && portrait->valid();
            if (hasPortrait && available) {
                float scale = std::min((float)(cellSize - 4) / portrait->width(),
                                       (float)(cellSize - 14) / portrait->height());
                float w = portrait->width() * scale;
                float h = portrait->height() * scale;
                float dx = cx + (cellSize - w) / 2.0f;
                float dy = cy + 2 + ((cellSize - 14) - h) / 2.0f;
                Rectangle src = {0, 0, (float)portrait->width(), (float)portrait->height()};
                Rectangle dst = {dx, dy, w, h};
                DrawTexturePro(portrait->get(), src, dst, {0, 0}, 0.0f, WHITE);
            }

            const char* nameStr = stage->name.c_str();
            if (auto bossId = getBossForGrid(i)) {
                const char* bossLabel = getBossLabel(*bossId);
                if (bossLabel[0] != '\0') nameStr = bossLabel;
            }
            char abbrev[20];
            snprintf(abbrev, sizeof(abbrev), "%.12s", nameStr);
            int nameW = MeasureText(abbrev, 6);
            DrawText(abbrev, cx + (cellSize - nameW) / 2, cy + cellSize - 10, 6,
                     available ? WHITE : Color{100, 100, 120, 255});

            if (!available) {
                DrawRectangle(cx + 2, cy + 2, cellSize - 4, cellSize - 4, {0, 0, 0, 100});
                DrawText("?", cx + cellSize / 2 - 3, cy + cellSize / 2 - 3, 10, {200, 200, 100, 255});
            }

            bool completed = stage && SaveSystem::isStageCompleted(StageId::fromString(stage->id));
            if (completed) {
                DrawText("*", cx + cellSize - 12, cy + 2, 10, {100, 255, 100, 255});
            }
        }

        // Selection cursor — pulsing border
        if (selected) {
            int pulse = static_cast<int>(std::sin(timer_ * 0.1f) * 40.0f + 215.0f);
            Color cursorColor = {
                static_cast<unsigned char>(pulse),
                static_cast<unsigned char>(pulse),
                255,
                255
            };
            DrawRectangleLines(cx - 2, cy - 2, cellSize + 4, cellSize + 4, cursorColor);
            DrawRectangleLines(cx - 1, cy - 1, cellSize + 2, cellSize + 2, cursorColor);
        }
    }

    const StageInfo* selected = getStageForGrid(cursorPos_);
    if (selected) {
        const char* name = selected->name.c_str();
        if (auto bossId = getBossForGrid(cursorPos_)) {
            const char* bossLabel = getBossLabel(*bossId);
            if (bossLabel[0] != '\0') name = bossLabel;
        }
        int nameW = MeasureText(name, 10);
        DrawText(name, (INTERNAL_WIDTH - nameW) / 2, INTERNAL_HEIGHT - 30, 10,
                 selected->available ? WHITE : Color{120, 120, 140, 255});

        if (!selected->available) {
            const char* locked = uiText(UiText::StageUnavailable, Settings::language);
            int lw = MeasureText(locked, 8);
            DrawText(locked, (INTERNAL_WIDTH - lw) / 2, INTERNAL_HEIGHT - 18, 8,
                     {180, 80, 80, 255});
        } else if (!prerequisitesMet(*selected)) {
            const char* locked = (routeMode_ == RouteMode::Fortress)
                ? uiText(UiText::StageClearPrevious, Settings::language)
                : uiText(UiText::StageDefeatAllMavericks, Settings::language);
            int lw = MeasureText(locked, 8);
            DrawText(locked, (INTERNAL_WIDTH - lw) / 2, INTERNAL_HEIGHT - 18, 8,
                     {200, 160, 60, 255});
        } else if (SaveSystem::isStageCompleted(StageId::fromString(selected->id))) {
            int best = SaveSystem::getBestTime(StageId::fromString(selected->id));
            if (best > 0) {
                int sec = best / 60;
                int fr = best % 60;
                char buf[48];
                snprintf(buf, sizeof(buf), "%s  %d:%02d.%02d",
                         uiText(UiText::StageCleared, Settings::language),
                         sec / 60, sec % 60, fr * 100 / 60);
                int bw = MeasureText(buf, 8);
                DrawText(buf, (INTERNAL_WIDTH - bw) / 2, INTERNAL_HEIGHT - 18, 8,
                         {100, 255, 100, 255});
            } else {
                const char* cleared = uiText(UiText::StageCleared, Settings::language);
                int cw = MeasureText(cleared, 8);
                DrawText(cleared, (INTERNAL_WIDTH - cw) / 2, INTERNAL_HEIGHT - 18, 8,
                         {100, 255, 100, 255});
            }
        }
    }

    // Input hint
    std::string hint;
    if (Input::isGamepadConnected()) {
        hint = std::string("A:") + uiText(UiText::PurposeSelect, Settings::language) +
               "  B:" + uiText(UiText::PurposeBack, Settings::language) +
               "  DPAD:" + uiText(UiText::PurposeNavigate, Settings::language);
    } else {
        hint = keyboardActionHint(Input::bindings(), InputAction::Confirm,
                                  uiText(UiText::PurposeSelect, Settings::language)) + "  " +
               keyboardActionHint(Input::bindings(), InputAction::Cancel,
                                  uiText(UiText::PurposeBack, Settings::language)) + "  " +
               keyboardDirectionalHint(Input::bindings(), uiText(UiText::PurposeNavigate,
                                                                 Settings::language));
    }
    int hintW = MeasureText(hint.c_str(), 7);
    DrawText(hint.c_str(), (INTERNAL_WIDTH - hintW) / 2, INTERNAL_HEIGHT - 8, 7, {80, 80, 110, 255});
    drawStageSelectUtilityHover(hoveredUtilityTab_, timer_);

    // Fade-in from black on scene entry
    if (fadeInTimer_ > 0) {
        int alpha = 255 * fadeInTimer_ / FADE_IN_DURATION;
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
                      {0, 0, 0, static_cast<unsigned char>(alpha)});
    }

    // Transition fade-out on exit
    if (transitioning_) {
        int fadeAlpha = std::min(255, transTimer_ * 8);
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
                      {0, 0, 0, static_cast<unsigned char>(fadeAlpha)});
    }
}


} // namespace mmx
