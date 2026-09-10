// title_scene.cpp - renders and drives the title screen and title menus.
// Owns: menu animation, title assets, selection flow, and scene transitions.

#include "ui/title_scene.h"
#include "ui/title_menu_font.h"
#include "ui/stage_select_scene.h"
#include "ui/boss_rush_scene.h"
#include "ui/bloody_palace_scene.h"
#include "ui/extras_scene.h"
#include "ui/options_scene.h"
#include "ui/map_editor_scene.h"
#include "gameplay/gameplay_scene.h"
#include "app/scene_manager.h"
#include "app/input.h"
#include "data/mmx1_password.h"
#include "data/settings.h"
#include "systems/asset_cache.h"
#include "systems/audio.h"
#include "data/save_system.h"
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <memory>

namespace mmx {

namespace {

constexpr Color kChromeLight{210, 235, 255, 255};
constexpr Color kChromeMid{74, 148, 236, 255};
constexpr Color kChromeDark{0, 25, 84, 255};
constexpr Color kPanelFill{0, 12, 48, 255};
constexpr Color kMenuDim{92, 106, 134, 255};
constexpr Color kMenuText{188, 212, 244, 255};
constexpr Color kMenuHighlight{255, 242, 116, 255};

void drawOutlinedText(const char* text, int x, int y, int size, Color fill, Color outline) {
    DrawText(text, x - 1, y, size, outline);
    DrawText(text, x + 1, y, size, outline);
    DrawText(text, x, y - 1, size, outline);
    DrawText(text, x, y + 1, size, outline);
    DrawText(text, x, y, size, fill);
}

void drawCenteredText(const char* text, int y, int size, Color fill, Color outline) {
    const int w = MeasureText(text, size);
    drawOutlinedText(text, (INTERNAL_WIDTH - w) / 2, y, size, fill, outline);
}

void drawTitleMenuText(const TextureResource& font, const char* text, int x, int y) {
    int penX = x;
    for (const char* p = text; *p; ++p) {
        const TitleMenuGlyph glyph = titleMenuGlyphForChar(*p);
        if (glyph.drawable) {
            Rectangle src{static_cast<float>(glyph.x), static_cast<float>(glyph.y),
                          static_cast<float>(glyph.width), static_cast<float>(glyph.height)};
            Rectangle dst{static_cast<float>(penX), static_cast<float>(y),
                          src.width, src.height};
            DrawTexturePro(font.get(), src, dst, {0, 0}, 0.0f, WHITE);
        }
        penX += glyph.advance;
    }
}

void drawTitleScrollArrow(int x, int y, bool up, Color color) {
    if (up) {
        DrawTriangle(Vector2{static_cast<float>(x + 4), static_cast<float>(y)},
                     Vector2{static_cast<float>(x), static_cast<float>(y + 5)},
                     Vector2{static_cast<float>(x + 8), static_cast<float>(y + 5)},
                     color);
    } else {
        DrawTriangle(Vector2{static_cast<float>(x + 4), static_cast<float>(y + 5)},
                     Vector2{static_cast<float>(x + 8), static_cast<float>(y)},
                     Vector2{static_cast<float>(x), static_cast<float>(y)},
                     color);
    }
}

bool sameRgb(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

void recolorTitleFontToOrange(ImageResource& image) {
    image.format(PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Color* pixels = image.pixels();
    if (!pixels) return;

    const Color sourceLight{208, 224, 240, 255};
    const Color sourceCyan{80, 224, 248, 255};
    const Color sourceShadow{32, 32, 32, 255};
    const Color selectedLight{247, 206, 107, 255};
    const Color selectedOrange{247, 140, 0, 255};
    const Color selectedShadow{33, 33, 33, 255};

    const int pixelCount = image.width() * image.height();
    for (int i = 0; i < pixelCount; ++i) {
        const unsigned char alpha = pixels[i].a;
        if (alpha == 0) continue;
        if (sameRgb(pixels[i], sourceLight)) {
            pixels[i] = Color{selectedLight.r, selectedLight.g, selectedLight.b, alpha};
        } else if (sameRgb(pixels[i], sourceCyan)) {
            pixels[i] = Color{selectedOrange.r, selectedOrange.g, selectedOrange.b, alpha};
        } else if (sameRgb(pixels[i], sourceShadow)) {
            pixels[i] = Color{selectedShadow.r, selectedShadow.g, selectedShadow.b, alpha};
        }
    }
}

void drawMmxWindow(int x, int y, int w, int h) {
    DrawRectangle(x, y, w, h, kChromeDark);
    DrawRectangleLines(x, y, w, h, kChromeLight);
    DrawRectangleLines(x + 1, y + 1, w - 2, h - 2, kChromeMid);
    DrawRectangle(x + 3, y + 3, w - 6, h - 6, kPanelFill);
    DrawRectangleLines(x + 4, y + 4, w - 8, h - 8, {0, 78, 124, 255});
}

void drawSelectorArrow(int x, int y, int timer) {
    const int bob = (timer / 8) % 2;
    DrawTriangle(
        Vector2{static_cast<float>(x + bob), static_cast<float>(y + 3)},
        Vector2{static_cast<float>(x + bob), static_cast<float>(y + 12)},
        Vector2{static_cast<float>(x + 8 + bob), static_cast<float>(y + 7)},
        {236, 72, 24, 255});
    DrawTriangle(
        Vector2{static_cast<float>(x + 2 + bob), static_cast<float>(y + 4)},
        Vector2{static_cast<float>(x + 2 + bob), static_cast<float>(y + 10)},
        Vector2{static_cast<float>(x + 7 + bob), static_cast<float>(y + 7)},
        kMenuHighlight);
}

} // namespace

void TitleScene::onEnter() {
    state_ = State::PressStart;
    menuSelection_ = 0;
    slotSelection_ = SaveSystem::activeSlotIndex();
    slotAction_ = SlotAction::None;
    resetPasswordEntry(passwordEntry_);
    timer_ = 0;
    transitioning_ = false;

    SaveSystem::resetRuntimeState();

    refreshSaveSummaries();
    if (hasSaveFile_) {
        if (!slotHasSave(SaveSystem::activeSlotIndex())) {
            SaveSystem::setActiveSlotIndex(firstSaveSlotIndex());
        }
        useSummaryForMenuStats(SaveSystem::activeSlotIndex());
    }
    contentPack_.loadFromFile("content/x1/manifest.json");
    loadMenuSprites();

    AudioManager::playBGM("title");

    fadeInTimer_ = FADE_IN_DURATION;
}

void TitleScene::pollInput() {
    // Input::poll() is handled globally by game loop
}

void TitleScene::loadMenuSprites() {
    windowSheet_ = nullptr;
    const char* windowPath = "content/x1/sprites/misc/mmx1_windows.gif";
    if (FileExists(windowPath)) {
        windowSheet_ = AssetCache::loadTexture(windowPath);
        if (windowSheet_) {
            windowSheet_->setFilter(TEXTURE_FILTER_POINT);
        }
    }
    // U50: the REAL title logo, ripped from the power-on VRAM dump
    // (docs/evidence/2026-06-12-U50). 223x96 at screen (16,16).
    titleLogo_ = nullptr;
    const char* logoPath = "content/x1/sprites/title/title_logo.png";
    if (FileExists(logoPath)) {
        titleLogo_ = AssetCache::loadTexture(logoPath);
        if (titleLogo_) titleLogo_->setFilter(TEXTURE_FILTER_POINT);
    }
    // U75: the EXACT menu rows (ripped per-row strips, cyan + orange
    // states) and the X selector sprite (build/u75/title captures —
    // rows y=155/171/187 x=74, X at (18,148)+16/row).
    menuRowsCyan_ = nullptr;
    menuRowsOrange_ = nullptr;
    selectorX_ = nullptr;
    if (FileExists("content/x1/sprites/title/title_menu_rows_cyan.png")) {
        menuRowsCyan_ = AssetCache::loadTexture(
            "content/x1/sprites/title/title_menu_rows_cyan.png");
        if (menuRowsCyan_) menuRowsCyan_->setFilter(TEXTURE_FILTER_POINT);
    }
    if (FileExists("content/x1/sprites/title/title_menu_rows_orange.png")) {
        menuRowsOrange_ = AssetCache::loadTexture(
            "content/x1/sprites/title/title_menu_rows_orange.png");
        if (menuRowsOrange_) menuRowsOrange_->setFilter(TEXTURE_FILTER_POINT);
    }
    if (FileExists("content/x1/sprites/title/title_selector_x.png")) {
        selectorX_ = AssetCache::loadTexture(
            "content/x1/sprites/title/title_selector_x.png");
        if (selectorX_) selectorX_->setFilter(TEXTURE_FILTER_POINT);
    }

    titleExtraFontCyan_ = nullptr;
    passwordSheet_ = nullptr;
    titleExtraFontOrange_.reset();
    const char* extraFontPath = "content/x1/sprites/misc/mmx_font.png";
    if (FileExists(extraFontPath)) {
        titleExtraFontCyan_ = AssetCache::loadTexture(extraFontPath);
        if (titleExtraFontCyan_) titleExtraFontCyan_->setFilter(TEXTURE_FILTER_POINT);

        ImageResource orangeFont;
        if (orangeFont.load(extraFontPath)) {
            recolorTitleFontToOrange(orangeFont);
            if (orangeFont.uploadTo(titleExtraFontOrange_)) {
                titleExtraFontOrange_.setFilter(TEXTURE_FILTER_POINT);
            }
        }
    }

    const char* passwordSheetPath = "content/x1/sprites/misc/mmx1_password.gif";
    if (FileExists(passwordSheetPath)) {
        passwordSheet_ = AssetCache::loadTexture(passwordSheetPath);
        if (passwordSheet_) passwordSheet_->setFilter(TEXTURE_FILTER_POINT);
    }
}

void TitleScene::refreshSaveSummaries() {
    hasSaveFile_ = false;
    saveStagesCleared_ = 0;
    saveDeaths_ = 0;
    savePlayMinutes_ = 0;
    saveEnemiesDefeated_ = 0;
    saveBossesDefeated_ = 0;
    saveSigmaDefeated_ = false;

    for (int i = 1; i <= SaveSystem::kSaveSlotCount; ++i) {
        saveSlotSummaries_[i - 1].reset();
        const auto slot = SaveSlot::numbered(i);
        if (!slot.has_value()) {
            continue;
        }

        saveSlotSummaries_[i - 1] = SaveSystem::summarize(*slot);
        if (saveSlotSummaries_[i - 1].has_value()) {
            hasSaveFile_ = true;
            saveSigmaDefeated_ = saveSigmaDefeated_ || saveSlotSummaries_[i - 1]->sigmaDefeated;
        }
    }

    const int displaySlot = slotHasSave(SaveSystem::activeSlotIndex())
        ? SaveSystem::activeSlotIndex()
        : firstSaveSlotIndex();
    if (displaySlot > 0) {
        useSummaryForMenuStats(displaySlot);
    }
}

const SaveSummary* TitleScene::summaryForSlot(int slotIndex) const {
    if (slotIndex < 1 || slotIndex > SaveSystem::kSaveSlotCount) {
        return nullptr;
    }
    const auto& summary = saveSlotSummaries_[slotIndex - 1];
    return summary.has_value() ? &(*summary) : nullptr;
}

bool TitleScene::slotHasSave(int slotIndex) const {
    return summaryForSlot(slotIndex) != nullptr;
}

int TitleScene::firstSaveSlotIndex() const {
    for (int i = 1; i <= SaveSystem::kSaveSlotCount; ++i) {
        if (slotHasSave(i)) {
            return i;
        }
    }
    return 0;
}

int TitleScene::firstEmptySlotIndex() const {
    for (int i = 1; i <= SaveSystem::kSaveSlotCount; ++i) {
        if (!slotHasSave(i)) {
            return i;
        }
    }
    return SaveSystem::activeSlotIndex();
}

void TitleScene::useSummaryForMenuStats(int slotIndex) {
    const SaveSummary* summary = summaryForSlot(slotIndex);
    if (!summary) {
        return;
    }
    saveStagesCleared_ = summary->stagesCleared;
    saveDeaths_ = summary->totalDeaths;
    savePlayMinutes_ = summary->totalPlayFrames / 3600;
    saveEnemiesDefeated_ = summary->enemiesDefeated;
    saveBossesDefeated_ = summary->bossesDefeated;
    saveSigmaDefeated_ = summary->sigmaDefeated;
}

void TitleScene::openSaveSlotSelect(SlotAction action) {
    slotAction_ = action;
    slotSelection_ = firstEmptySlotIndex();
    if (slotSelection_ <= 0 || slotSelection_ > SaveSystem::kSaveSlotCount) {
        slotSelection_ = 1;
    }
    state_ = State::SaveSlots;
    inputDelay_ = 0;
    timer_ = 0;
}

void TitleScene::openPasswordEntry() {
    resetPasswordEntry(passwordEntry_);
    state_ = State::Password;
    inputDelay_ = 0;
    timer_ = 0;
}

void TitleScene::submitPassword() {
    const std::string text = mmx1_password::formatDigits(passwordEntry_.digits, false);
    const auto decoded = mmx1_password::decode(text);
    if (!decoded.has_value()) {
        passwordEntry_.invalid = true;
        AudioManager::playSFX(SFX::MenuCancel);
        return;
    }

    SaveSystem::importData(decoded->saveData);
    AudioManager::playSFX(SFX::MenuSelect);
    transitioning_ = true;
    timer_ = 0;
}

void TitleScene::beginSelectedSlotAction() {
    if (!SaveSystem::setActiveSlotIndex(slotSelection_)) {
        AudioManager::playSFX(SFX::MenuCancel);
        return;
    }
    transitioning_ = true;
    timer_ = 0;
}

void TitleScene::launchIntroHighway() {
    GameplaySceneConfig gameplayConfig;
    gameplayConfig.stagePath =
        contentPack_.resolvePath("stages/tiles/intro-highway_full.json");
    gameplayConfig.stageId = StageId::fromString("intro-highway");
    gameplayConfig.characterPath = contentPack_.resolvePath("characters/x.json");
    auto gameplay = std::make_unique<GameplayScene>(gameplayConfig);
    gameplay->setSceneManager(sceneManager_);
    sceneManager_->changeScene(std::move(gameplay));
}

void TitleScene::launchStageSelect() {
    auto stageSelect = std::make_unique<StageSelectScene>();
    stageSelect->setSceneManager(sceneManager_);
    stageSelect->setContentPack(contentPack_);
    sceneManager_->changeScene(std::move(stageSelect));
}

void TitleScene::launchSelectedNewGame() {
    const auto slot = SaveSystem::activeSlot();
    const SaveSummary* summary = summaryForSlot(slotSelection_);
    if (summary && summary->sigmaDefeated && SaveSystem::load(slot)) {
        SaveSystem::resetProgressionForNewRun();
        SaveSystem::deleteSave(slot);
        SaveSystem::save(slot);
    } else {
        SaveSystem::resetRuntimeState();
        SaveSystem::deleteSave(slot);
    }
    launchIntroHighway();
}

void TitleScene::handleInput() {
    if (transitioning_) return;

    if (state_ == State::PressStart) {
        if (Input::isConfirmPressed() || Input::isPausePressed()) {
            Input::consumeConfirmPress();
            Input::consumePausePress();
            AudioManager::playSFX(SFX::MenuSelect);
            state_ = State::Menu;
            timer_ = 0;
        }
        return;
    }

    if (state_ == State::Password) {
        if (Input::isCancelPressed()) {
            Input::consumeCancelPress();
            AudioManager::playSFX(SFX::MenuCancel);
            state_ = State::Menu;
            inputDelay_ = 0;
            timer_ = 0;
            return;
        }

        // Source controls: Y increments, B decrements, START confirms.
        // Confirm/A deliberately has no effect on this grid.
        if (Input::isShootPressed()) {
            Input::consumeShootPress();
            cyclePasswordDigit(passwordEntry_, 1);
            AudioManager::playSFX(SFX::MenuMove);
            return;
        }
        if (Input::isJumpPressed()) {
            Input::consumeJumpPress();
            cyclePasswordDigit(passwordEntry_, -1);
            AudioManager::playSFX(SFX::MenuMove);
            return;
        }
        if (Input::isPausePressed()) {
            Input::consumePausePress();
            submitPassword();
            return;
        }

        if (inputDelay_ > 0) {
            inputDelay_--;
        } else {
            int columnDelta = 0;
            int rowDelta = 0;
            if (Input::isLeftHeld()) {
                columnDelta = -1;
            } else if (Input::isRightHeld()) {
                columnDelta = 1;
            } else if (Input::isUpHeld()) {
                rowDelta = -1;
            } else if (Input::isDownHeld()) {
                rowDelta = 1;
            }

            const int previousCursor = passwordEntry_.cursor;
            movePasswordCursor(passwordEntry_, columnDelta, rowDelta);
            if (passwordEntry_.cursor != previousCursor) {
                inputDelay_ = 10;
                AudioManager::playSFX(SFX::MenuMove);
            }
        }
        if (!Input::isLeftHeld() && !Input::isRightHeld() &&
            !Input::isUpHeld() && !Input::isDownHeld()) {
            inputDelay_ = 0;
        }
        return;
    }

    if (state_ == State::SaveSlots) {
        if (Input::isCancelPressed()) {
            Input::consumeCancelPress();
            AudioManager::playSFX(SFX::MenuCancel);
            state_ = State::Menu;
            slotAction_ = SlotAction::None;
            inputDelay_ = 0;
            timer_ = 0;
            return;
        }

        if (Input::isJumpPressed() || Input::isConfirmPressed()) {
            Input::consumeJumpPress();
            Input::consumeConfirmPress();
            AudioManager::playSFX(SFX::MenuSelect);
            beginSelectedSlotAction();
            return;
        }

        if (inputDelay_ > 0) {
            inputDelay_--;
        } else {
            if (Input::isUpHeld()) {
                slotSelection_--;
                if (slotSelection_ < 1) slotSelection_ = SaveSystem::kSaveSlotCount;
                inputDelay_ = 10;
                AudioManager::playSFX(SFX::MenuMove);
            }
            if (Input::isDownHeld()) {
                slotSelection_++;
                if (slotSelection_ > SaveSystem::kSaveSlotCount) slotSelection_ = 1;
                inputDelay_ = 10;
                AudioManager::playSFX(SFX::MenuMove);
            }
        }
        if (!Input::isUpHeld() && !Input::isDownHeld()) {
            inputDelay_ = 0;
        }
        return;
    }

    // Menu navigation
    if (Input::isJumpPressed() || Input::isConfirmPressed()) {
        Input::consumeJumpPress();
        Input::consumeConfirmPress();

        const TitleMenuAction action = titleMenuActionForSelection(menuSelection_);
        if (action == TitleMenuAction::NewGame) {
            AudioManager::playSFX(SFX::MenuSelect);
            openSaveSlotSelect(SlotAction::NewGame);
            return;
        }
        if (action == TitleMenuAction::Password) {
            AudioManager::playSFX(SFX::MenuSelect);
            openPasswordEntry();
            return;
        }

        AudioManager::playSFX(SFX::MenuSelect);
        transitioning_ = true;
        timer_ = 0;
        return;
    }

    // Cancel goes back to press-start state
    if (Input::isCancelPressed()) {
        Input::consumeCancelPress();
        state_ = State::PressStart;
        timer_ = 0;
        return;
    }

    // Move cursor with repeat delay
    if (inputDelay_ > 0) {
        inputDelay_--;
    } else {
        int direction = 0;
        if (Input::isUpHeld()) {
            direction = -1;
        } else if (Input::isDownHeld()) {
            direction = 1;
        }

        const int nextSelection = titleMenuSelectionAfterMove(menuSelection_, direction);
        if (nextSelection != menuSelection_) {
            menuSelection_ = nextSelection;
            inputDelay_ = 10; // 10 frames between repeats (~6/sec)
            AudioManager::playSFX(SFX::MenuMove);
        }
    }
    // Reset delay when no direction held
    if (!Input::isUpHeld() && !Input::isDownHeld()) {
        inputDelay_ = 0;
    }
}

void TitleScene::update(float /*dt*/) {
    timer_++;
    if (fadeInTimer_ > 0) fadeInTimer_--;

    // Transition after brief delay
    if (transitioning_ && timer_ > 10 && sceneManager_) {
        const TitleMenuAction action = titleMenuActionForSelection(menuSelection_);
        if (slotAction_ == SlotAction::NewGame) {
            launchSelectedNewGame();
            return;
        }
        if (action == TitleMenuAction::Password && state_ == State::Password) {
            launchStageSelect();
            return;
        }

        if (action == TitleMenuAction::Options) {
            // U50: OPTION MODE (the real third entry)
            auto options = std::make_unique<OptionsScene>();
            options->setSceneManager(sceneManager_);
            sceneManager_->changeScene(std::move(options));
        } else if (action == TitleMenuAction::StageSelect) {
            // Stage Select
            if (hasSaveFile_) {
                SaveSystem::load(SaveSystem::activeSlot());
            } else {
                SaveSystem::resetRuntimeState();
            }
            launchStageSelect();
        } else if (action == TitleMenuAction::BossRush) {
            // Boss Rush
            auto rush = std::make_unique<BossRushScene>();
            rush->setSceneManager(sceneManager_);
            sceneManager_->changeScene(std::move(rush));
        } else if (action == TitleMenuAction::BloodyPalace) {
            // Bloody Palace
            auto palace = std::make_unique<BloodyPalaceScene>();
            palace->setSceneManager(sceneManager_);
            sceneManager_->changeScene(std::move(palace));
        } else if (action == TitleMenuAction::Randomizer) {
            // Randomizer — launch directly into intro highway with shuffled enemies
            GameplaySceneConfig gameplayConfig;
            gameplayConfig.stagePath =
                contentPack_.resolvePath("stages/tiles/intro-highway_full.json");
            gameplayConfig.stageId = StageId::fromString("intro-highway");
            gameplayConfig.characterPath = contentPack_.resolvePath("characters/x.json");
            gameplayConfig.randomizerMode = true;
            gameplayConfig.randomizerSeed = static_cast<uint64_t>(std::time(nullptr));
            auto gameplay = std::make_unique<GameplayScene>(gameplayConfig);
            gameplay->setSceneManager(sceneManager_);
            sceneManager_->changeScene(std::move(gameplay));
        } else if (action == TitleMenuAction::Extras) {
            // Extras
            auto extras = std::make_unique<ExtrasScene>();
            ContentPack extrasPack;
            extrasPack.loadFromFile("content/extras/manifest.json");
            extras->setContentPack(extrasPack);
            extras->setSceneManager(sceneManager_);
            sceneManager_->changeScene(std::move(extras));
        } else if (action == TitleMenuAction::MapEditor) {
            // Map Editor
            auto editor = std::make_unique<MapEditorScene>();
            editor->setSceneManager(sceneManager_);
            editor->setContentPack(contentPack_);
            sceneManager_->changeScene(std::move(editor));
        } else if (action == TitleMenuAction::TestStage) {
            // Test Stage - real decoded fixture for sprite/weapon/physics checks.
            // F4 still grants all weapons; synthetic maps stay for micro-contracts.
            GameplaySceneConfig gameplayConfig;
            gameplayConfig.stagePath = "content/x1/stages/tiles/flame-mammoth_full.json";
            gameplayConfig.stageId = StageId::fromString("flame-mammoth");
            gameplayConfig.characterPath = contentPack_.resolvePath("characters/x.json");
            auto gameplay = std::make_unique<GameplayScene>(gameplayConfig);
            gameplay->setSceneManager(sceneManager_);
            gameplay->addExtraEnemySpawn({"enemy", "walker", 304.0f, 640.0f});
            sceneManager_->changeScene(std::move(gameplay));
        } else if (action == TitleMenuAction::Quit) {
            Settings::save();
            sceneManager_->requestQuit();
        }
    }
}

void TitleScene::renderBackdrop() const {
    // U75: the real title background is PURE BLACK (menu_settled capture) —
    // the old scanline/window dressing is gone per David's "exactly like
    // the original" directive.
    ClearBackground(BLACK);
}

void TitleScene::renderTitleLogo() const {
    // U50: the REAL logo (ripped from the power-on dump) at its measured
    // screen position (16,16) — the real game keeps it static between the
    // input-wait title screen and the menu.
    if (titleLogo_ && titleLogo_->valid()) {
        DrawTexture(titleLogo_->get(), 16, 16, WHITE);
    }
}

void TitleScene::renderTitleIdle() const {
    // GC3.2-T2 source trace: the settled title is invariant across every one
    // of 151 dense frames (7800..7950) and 18 additional 60-frame samples
    // through frame 8540. There is no blinking PRESS START prompt or remake
    // copyright line on this screen.
    DrawText("CAPCOM", 98, 194, 10, {255, 220, 54, 255});
}

void TitleScene::renderMainMenu() const {
    // U50: the REAL menu baseline: three visible rows (power-on dump
    // docs/evidence/2026-06-12-U50). U159 keeps the title at three total
    // visible options while the whole list scrolls through original + added
    // entries.
    const Color kCyan{82, 231, 255, 255};
    const Color kOrange{247, 140, 0, 255};
    const Color kDim{60, 110, 130, 255};

    // U159: original and added rows are one title-menu list. Only three rows
    // total are visible at a time.
    constexpr int kRowX = 74;
    constexpr int kGeneratedRowTextX = kRowX + 6;
    constexpr int kRowH = 14;
    constexpr int kRowY[3] = {155, 171, 187};
    const TitleMenuWindow menuWindow = titleMenuWindowForSelection(menuSelection_);
    const int selectedVisibleRow = menuSelection_ - menuWindow.firstIndex;

    for (int row = 0; row < kTitleVisibleMenuRows; row++) {
        const int idx = menuWindow.firstIndex + row;
        const bool selected = menuSelection_ == idx;
        const bool dimmed = false;
        const int y = kRowY[row];
        bool drewRippedRow = false;

        if (idx < kTitleFirstExtraMenuIndex &&
            menuRowsCyan_ && menuRowsCyan_->valid() &&
            menuRowsOrange_ && menuRowsOrange_->valid()) {
            const Texture2D& tex = selected
                ? menuRowsOrange_->get()
                : menuRowsCyan_->get();
            Rectangle src{0.0f, static_cast<float>(idx * kRowH),
                          static_cast<float>(tex.width),
                          static_cast<float>(kRowH)};
            Rectangle dst{static_cast<float>(kRowX),
                          static_cast<float>(y), src.width, src.height};
            DrawTexturePro(tex, src, dst, {0, 0}, 0.0f,
                           dimmed ? Color{120, 120, 120, 255} : WHITE);
            if (idx == 0 && saveSigmaDefeated_) {
                DrawText("+", 176, y + 1, 8, selected ? kOrange : kCyan);
            }
            drewRippedRow = true;
        }

        if (!drewRippedRow) {
            const char* label = titleMenuLabel(idx, Settings::language);
            if (titleExtraFontCyan_ && titleExtraFontCyan_->valid() &&
                titleExtraFontOrange_.valid()) {
                const TextureResource& font = selected ? titleExtraFontOrange_ : *titleExtraFontCyan_;
                drawTitleMenuText(font, label, kGeneratedRowTextX, y);
            } else {
                const Color col = dimmed ? kDim : (selected ? kOrange : kCyan);
                DrawText(label, kGeneratedRowTextX, 148 + row * 16, 8, col);
            }
        }
    }

    if (selectorX_ && selectorX_->valid() &&
        selectedVisibleRow >= 0 && selectedVisibleRow < kTitleVisibleMenuRows) {
        DrawTexture(selectorX_->get(), 18, 148 + selectedVisibleRow * 16, WHITE);
    }
    if (titleMenuScrollArrowBlinkVisible(timer_)) {
        if (menuWindow.showUpArrow) {
            drawTitleScrollArrow(224, 148, true, kCyan);
        }
        if (menuWindow.showDownArrow) {
            drawTitleScrollArrow(224, 190, false, kCyan);
        }
    }

    if (hasSaveFile_) {
        char stats[64];
        snprintf(stats, sizeof(stats), "%dMIN  %dK  %dD", savePlayMinutes_, saveEnemiesDefeated_, saveDeaths_);
        DrawText(stats, 160, 138, 7, {96, 132, 164, 255});
    }
}

void TitleScene::renderSaveSlots() const {
    const char* heading = uiText(UiText::TitleGameStart, Settings::language);
    drawMmxWindow(29, 80, 198, 116);
    drawCenteredText(heading, 89, 9, {108, 224, 116, 255}, {0, 42, 74, 255});

    constexpr int rowX = 54;
    constexpr int rowY = 110;
    constexpr int rowH = 25;
    for (int slotIndex = 1; slotIndex <= SaveSystem::kSaveSlotCount; ++slotIndex) {
        const SaveSummary* summary = summaryForSlot(slotIndex);
        const bool selected = slotSelection_ == slotIndex;
        const bool dimmed = false;
        const int y = rowY + (slotIndex - 1) * rowH;

        if (selected && !dimmed) {
            drawSelectorArrow(rowX - 18, y - 2, timer_);
        }

        char label[16];
        snprintf(label, sizeof(label), "%s %d", uiText(UiText::SaveFile, Settings::language), slotIndex);
        DrawText(label, rowX, y, 8, dimmed ? kMenuDim : (selected ? WHITE : kMenuText));

        if (summary) {
            char stats[48];
            snprintf(stats, sizeof(stats), "%d/8  %dMIN  %dD",
                     summary->stagesCleared,
                     summary->totalPlayFrames / 3600,
                     summary->totalDeaths);
            DrawText(stats, rowX + 54, y, 7, selected ? kMenuHighlight : Color{96, 174, 118, 255});
            if (slotAction_ == SlotAction::NewGame) {
                DrawText(uiText(UiText::SaveOverwrite, Settings::language), rowX + 54, y + 10, 7,
                         selected ? Color{255, 150, 76, 255} : Color{138, 92, 78, 255});
            } else if (summary->sigmaDefeated) {
                DrawText(uiText(UiText::SaveClear, Settings::language), rowX + 130, y + 10, 7,
                         selected ? kMenuHighlight : Color{150, 124, 64, 255});
            }
        } else {
            DrawText(uiText(UiText::SaveEmpty, Settings::language),
                     rowX + 54, y, 7, selected ? kMenuHighlight : kMenuDim);
        }
    }
}

void TitleScene::renderPasswordEntry() const {
    constexpr Color digitColors[] = {
        Color{244, 84, 72, 255},
        Color{154, 102, 224, 255},
        Color{230, 194, 72, 255},
        Color{100, 190, 218, 255},
        Color{238, 112, 76, 255},
        Color{198, 132, 218, 255},
        Color{224, 82, 72, 255},
        Color{242, 196, 80, 255},
    };
    constexpr int frameX = 25;
    constexpr int frameY = 7;
    constexpr int cellX = 33;
    constexpr int cellY = 22;
    constexpr int cellW = 42;
    constexpr int cellH = 34;
    constexpr int stepX = 49;
    constexpr int stepY = 50;
    const bool hasSourceSheet = passwordSheet_ && passwordSheet_->valid();

    ClearBackground(BLACK);
    if (hasSourceSheet) {
        DrawTexturePro(
            passwordSheet_->get(),
            Rectangle{5.0f, 0.0f, 256.0f, 224.0f},
            Rectangle{0.0f, 0.0f, 256.0f, 224.0f},
            Vector2{0.0f, 0.0f},
            0.0f,
            WHITE
        );
    } else {
        DrawRectangle(frameX, frameY, 206, 203, Color{24, 34, 42, 255});
        DrawRectangleLines(frameX, frameY, 206, 203, Color{226, 232, 244, 255});
        DrawRectangleLines(frameX + 2, frameY + 2, 202, 199, Color{38, 112, 188, 255});
        DrawRectangle(frameX + 7, 165, 192, 38, Color{8, 110, 54, 255});
    }

    constexpr Rectangle digitSources[] = {
        Rectangle{276.0f, 6.0f, 8.0f, 17.0f},
        Rectangle{273.0f, 30.0f, 13.0f, 17.0f},
        Rectangle{273.0f, 52.0f, 13.0f, 17.0f},
        Rectangle{272.0f, 75.0f, 14.0f, 17.0f},
        Rectangle{273.0f, 99.0f, 13.0f, 17.0f},
        Rectangle{273.0f, 122.0f, 13.0f, 17.0f},
        Rectangle{273.0f, 145.0f, 13.0f, 17.0f},
        Rectangle{272.0f, 168.0f, 13.0f, 17.0f},
    };
    constexpr Rectangle helmetSource{7.0f, 250.0f, 21.0f, 12.0f};

    for (int index = 0; index < mmx1_password::kPasswordDigitCount; ++index) {
        const int column = index % kPasswordGridColumns;
        const int row = index / kPasswordGridColumns;
        const int x = cellX + column * stepX;
        const int y = cellY + row * stepY;
        const bool selected = passwordEntry_.cursor == index;

        const int digit = passwordEntry_.digits[static_cast<std::size_t>(index)];
        if (hasSourceSheet) {
            const Rectangle digitSource = digitSources[digit - 1];
            DrawTexturePro(
                passwordSheet_->get(),
                digitSource,
                Rectangle{
                    static_cast<float>(x + (cellW - static_cast<int>(digitSource.width)) / 2),
                    static_cast<float>(y + 2),
                    digitSource.width,
                    digitSource.height,
                },
                Vector2{0.0f, 0.0f},
                0.0f,
                WHITE
            );
            DrawTexturePro(
                passwordSheet_->get(),
                helmetSource,
                Rectangle{
                    static_cast<float>(x + (cellW - 21) / 2),
                    static_cast<float>(y + 22),
                    21.0f,
                    12.0f,
                },
                Vector2{0.0f, 0.0f},
                0.0f,
                WHITE
            );
            if (selected && ((timer_ / 8) % 2) == 0) {
                DrawRectangleLines(x - 1, y - 1, cellW + 2, cellH + 6,
                                   Color{246, 210, 58, 255});
            }
        } else {
            DrawRectangle(x - 2, y - 2, cellW + 4, cellH + 7,
                          selected ? Color{244, 210, 62, 255} : Color{28, 100, 170, 255});
            DrawRectangle(x, y, cellW, cellH, Color{4, 84, 38, 255});
            DrawRectangle(x + 8, y + 5, cellW - 16, cellH - 13, Color{32, 42, 46, 255});
            DrawRectangle(x + 10, y + 7, cellW - 20, cellH - 17, Color{202, 208, 206, 255});

            const char text[2] = {static_cast<char>('0' + digit), '\0'};
            const int textWidth = MeasureText(text, 18);
            drawOutlinedText(
                text,
                x + (cellW - textWidth) / 2,
                y + 5,
                18,
                digitColors[digit - 1],
                BLACK
            );

            if (selected) {
                const int centerX = x + cellW / 2;
                DrawTriangle(
                    Vector2{static_cast<float>(centerX), static_cast<float>(y + cellH - 1)},
                    Vector2{static_cast<float>(centerX - 7), static_cast<float>(y + cellH + 7)},
                    Vector2{static_cast<float>(centerX + 7), static_cast<float>(y + cellH + 7)},
                    Color{246, 210, 58, 255}
                );
            }
        }
    }

    if (passwordEntry_.invalid) {
        drawCenteredText("INVALID PASSWORD", 178, 9, Color{255, 224, 84, 255}, BLACK);
    }
}

void TitleScene::render(float /*alpha*/) {
    renderBackdrop();
    if (state_ != State::Password) {
        renderTitleLogo();
    }

    if (state_ == State::PressStart) {
        renderTitleIdle();
    } else if (state_ == State::Password) {
        renderPasswordEntry();
    } else if (state_ == State::SaveSlots) {
        renderSaveSlots();
    } else {
        renderMainMenu();
    }

    if (fadeInTimer_ > 0) {
        int fadeAlpha = 255 * fadeInTimer_ / FADE_IN_DURATION;
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
                      {0, 0, 0, static_cast<unsigned char>(fadeAlpha)});
    }

    if (transitioning_) {
        int fadeAlpha = std::min(255, timer_ * 25);
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
                      {0, 0, 0, static_cast<unsigned char>(fadeAlpha)});
    }

}

} // namespace mmx
