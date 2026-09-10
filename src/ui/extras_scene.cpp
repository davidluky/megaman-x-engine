// extras_scene.cpp - runs the extras menu and stage thumbnail browsing.
// Owns: extras selection, thumbnail cache, and stage/menu transitions.

#include "ui/extras_scene.h"
#include "ui/title_scene.h"
#include "app/scene_manager.h"
#include "gameplay/gameplay_scene.h"
#include "app/input.h"
#include "app/constants.h"
#include "data/localization.h"
#include "data/settings.h"
#include "systems/asset_cache.h"
#include "raylib.h"
#include <algorithm>
#include <memory>
#include <string>

namespace mmx {

void ExtrasScene::onEnter() {
    cursor_ = 0;
    inputDelay_ = 0;
    transitioning_ = false;
    transTimer_ = 0;
    fadeInTimer_ = FADE_IN_DURATION;
    customMapError_.clear();
    customMapErrorTimer_ = 0;
    customMaps_ = map_editor::listUserMaps();
}

void ExtrasScene::onExit() {
    thumbnails_.clear();
}

const TextureResource* ExtrasScene::getThumbnail(const std::string& stageId) {
    auto it = thumbnails_.find(stageId);
    if (it != thumbnails_.end()) return it->second;

    // Resolve the stage's config path, then sibling thumbnail.png.
    // If it doesn't exist, cache nullptr so we don't retry each frame.
    const TextureResource* thumbnail = nullptr;
    const StageInfo* s = contentPack_.findStage(stageId);
    if (s && !s->config.empty()) {
        std::string cfg = contentPack_.resolvePath(s->config);
        auto slash = cfg.find_last_of("/\\");
        if (slash != std::string::npos) {
            std::string thumbPath = cfg.substr(0, slash + 1) + "thumbnail.png";
            if (FileExists(thumbPath.c_str())) {
                thumbnail = AssetCache::loadTexture(thumbPath);
                if (thumbnail) {
                    thumbnail->setFilter(TEXTURE_FILTER_POINT);
                }
            }
        }
    }
    thumbnails_.emplace(stageId, thumbnail);
    return thumbnail;
}

const TextureResource* ExtrasScene::getCustomMapThumbnail(
    const map_editor::UserMapEntry& entry) {
    if (entry.thumbnailPath.empty()) return nullptr;

    const std::string key = std::string("custom:") + entry.thumbnailPath.generic_string();
    auto it = thumbnails_.find(key);
    if (it != thumbnails_.end()) return it->second;

    const TextureResource* thumbnail = nullptr;
    const std::string thumbPath = entry.thumbnailPath.generic_string();
    if (FileExists(thumbPath.c_str())) {
        thumbnail = AssetCache::loadTexture(thumbPath);
        if (thumbnail) {
            thumbnail->setFilter(TEXTURE_FILTER_POINT);
        }
    }
    thumbnails_.emplace(key, thumbnail);
    return thumbnail;
}

int ExtrasScene::customStartIndex() const {
    return static_cast<int>(contentPack_.stages.size());
}

int ExtrasScene::totalEntries() const {
    return customStartIndex() + static_cast<int>(customMaps_.size());
}

int ExtrasScene::customMapIndexForCursor() const {
    const int index = cursor_ - customStartIndex();
    if (index < 0 || index >= static_cast<int>(customMaps_.size())) return -1;
    return index;
}

bool ExtrasScene::cursorOnCustomMap() const {
    return customMapIndexForCursor() >= 0;
}

void ExtrasScene::pollInput() {}

void ExtrasScene::handleInput() {
    if (transitioning_) return;
    if (totalEntries() <= 0) return;

    const int n = totalEntries();

    if (inputDelay_ > 0) {
        inputDelay_--;
    } else {
        if (Input::isUpHeld()) {
            cursor_ = (cursor_ - 1 + n) % n;
            inputDelay_ = 10;
        } else if (Input::isDownHeld()) {
            cursor_ = (cursor_ + 1) % n;
            inputDelay_ = 10;
        }
    }

    if (Input::isConfirmPressed() || Input::isJumpPressed()) {
        Input::consumeConfirmPress();
        if (cursorOnCustomMap()) {
            const auto& customMap = customMaps_[customMapIndexForCursor()];
            customMapError_ = map_editor::userMapPlayabilityError(customMap.path);
            if (!customMapError_.empty()) {
                customMapErrorTimer_ = CUSTOM_MAP_ERROR_DURATION;
                TraceLog(LOG_WARNING, "Custom map rejected: %s", customMapError_.c_str());
                return;
            }
            GameplaySceneConfig gameplayConfig;
            gameplayConfig.stagePath = customMap.path.generic_string();
            const CharacterInfo* ci = contentPack_.findCharacter("x");
            gameplayConfig.characterPath = ci
                ? contentPack_.resolvePath(ci->config)
                : std::string("content/x1/characters/x.json");
            auto gameplay = std::make_unique<GameplayScene>(gameplayConfig);
            gameplay->setSceneManager(sceneManager_);
            sceneManager_->changeScene(std::move(gameplay));
            transitioning_ = true;
            return;
        }

        const auto& stage = contentPack_.stages[cursor_];
        if (!stage.available) return;   // Silently ignore unbuilt stages

        GameplaySceneConfig gameplayConfig;
        gameplayConfig.stagePath = contentPack_.resolvePath(stage.config);
        gameplayConfig.stageId = StageId::fromString(stage.id);
        // Characters live in the x1 pack; extras manifest references it via
        // "../x1/characters/x.json" which resolves correctly through
        // ContentPack::resolvePath. For robustness fall back to x1 absolute.
        const CharacterInfo* ci = contentPack_.findCharacter("x");
        gameplayConfig.characterPath = ci
            ? contentPack_.resolvePath(ci->config)
            : std::string("content/x1/characters/x.json");
        auto gameplay = std::make_unique<GameplayScene>(gameplayConfig);
        gameplay->setSceneManager(sceneManager_);
        sceneManager_->changeScene(std::move(gameplay));
        transitioning_ = true;
    }

    if (Input::isCancelPressed()) {
        Input::consumeCancelPress();
        if (sceneManager_) {
            auto title = std::make_unique<TitleScene>();
            title->setSceneManager(sceneManager_);
            sceneManager_->changeScene(std::move(title));
        }
    }
}

void ExtrasScene::update(float /*dt*/) {
    if (fadeInTimer_ > 0) fadeInTimer_--;
    if (customMapErrorTimer_ > 0) customMapErrorTimer_--;
    if (transitioning_) transTimer_++;
}

void ExtrasScene::render(float /*alpha*/) {
    ClearBackground({12, 16, 40, 255});

    const int pad = 6;
    const int lineHeight = 14;
    const int titleY = 14;

    DrawText(uiText(UiText::TitleExtras, Settings::language), pad, titleY, 16,
             {255, 220, 120, 255});
    DrawText(uiText(UiText::ExtrasSubtitle, Settings::language), pad, titleY + 18, 10,
             {180, 180, 200, 255});

    // Layout: list occupies left ~128px; preview panel sits on the right.
    const int listW = 128;
    const int listY = titleY + 40;

    const int total = totalEntries();
    const int visibleRows = std::max(1, (INTERNAL_HEIGHT - listY - 20) / lineHeight);
    int first = 0;
    if (cursor_ >= visibleRows) {
        first = cursor_ - visibleRows + 1;
    }
    first = std::max(0, std::min(first, std::max(0, total - visibleRows)));

    for (int row = 0; row < visibleRows && first + row < total; ++row) {
        const int i = first + row;
        bool selected = (i == cursor_);
        int y = listY + row * lineHeight;
        const bool custom = i >= customStartIndex();
        const map_editor::UserMapEntry* mapEntry = nullptr;
        const StageInfo* stageEntry = nullptr;
        if (custom) {
            mapEntry = &customMaps_[i - customStartIndex()];
        } else {
            stageEntry = &contentPack_.stages[static_cast<size_t>(i)];
        }

        const bool available = custom || (stageEntry && stageEntry->available);
        Color nameColor = available
            ? (selected ? Color{255, 230, 90, 255} : Color{220, 220, 230, 255})
            : Color{100, 100, 110, 255};

        if (selected) DrawText(">", pad, y, 10, nameColor);
        std::string name = custom ? mapEntry->name : stageEntry->name;
        while (MeasureText(name.c_str(), 10) > listW - 34 && name.size() > 1) {
            name.pop_back();
        }
        DrawText(name.c_str(), pad + 10, y, 10, nameColor);

        // In-list badge: origin tag when available, localized lock marker otherwise.
        Color badgeColor = selected ? Color{200, 200, 120, 255}
                                    : Color{120, 120, 140, 255};
        const char* badge = nullptr;
        if (custom) {
            badge = uiText(UiText::ExtrasBadgeCustom, Settings::language);
        } else {
            badge = stageEntry->available
                ? (stageEntry->origin.empty() ? nullptr : stageEntry->origin.c_str())
                : uiText(UiText::ExtrasBadgeLocked, Settings::language);
        }
        if (badge) {
            int bw = MeasureText(badge, 10);
            DrawText(badge, listW - pad - bw, y, 10, badgeColor);
        }
    }

    // Preview panel — thumbnail + origin/source for the selected stage.
    if (totalEntries() > 0) {
        const int panelX = listW + 2;
        const int panelY = titleY + 14;

        if (cursorOnCustomMap()) {
            const auto& sel = customMaps_[customMapIndexForCursor()];
            const TextureResource* t = getCustomMapThumbnail(sel);
            if (t && t->valid()) {
                DrawTexture(t->get(), panelX, panelY, WHITE);
                DrawRectangleLines(panelX, panelY, t->width(), t->height(),
                                   {80, 80, 100, 255});
            } else {
                DrawText(uiText(UiText::ExtrasNoPreview, Settings::language),
                         panelX, panelY + 24, 10,
                         {120, 120, 140, 255});
            }

            const int infoY = panelY + 64;
            DrawText(sel.name.c_str(), panelX, infoY, 10, {255, 230, 90, 255});
            DrawText(uiText(UiText::ExtrasOriginUserMaps, Settings::language),
                     panelX, infoY + 14, 10,
                     {200, 200, 220, 255});
            std::string path = sel.path.generic_string();
            while (MeasureText(path.c_str(), 10) > INTERNAL_WIDTH - panelX - 4 &&
                   path.size() > 1) {
                path.erase(path.begin());
            }
            DrawText(path.c_str(), panelX, infoY + 26, 10,
                     {160, 160, 190, 255});
            DrawText(sel.sizeLabel.c_str(), panelX, infoY + 38, 10,
                     {160, 160, 190, 255});
            DrawText(sel.modifiedDate.c_str(), panelX + 54, infoY + 38, 10,
                     {160, 160, 190, 255});
            if (customMapErrorTimer_ > 0 && !customMapError_.empty()) {
                std::string error = customMapError_;
                while (MeasureText(error.c_str(), 10) > INTERNAL_WIDTH - panelX - 4 &&
                       error.size() > 1) {
                    error.pop_back();
                }
                DrawText(error.c_str(), panelX, infoY + 50, 10,
                         {255, 120, 100, 255});
            }
        } else {
            const auto& sel = contentPack_.stages[static_cast<size_t>(cursor_)];
            if (sel.available) {
                const TextureResource* t = getThumbnail(sel.id);
                if (t && t->valid()) {
                    DrawTexture(t->get(), panelX, panelY, WHITE);
                    DrawRectangleLines(panelX, panelY, t->width(), t->height(),
                                       {80, 80, 100, 255});
                } else {
                    DrawText(uiText(UiText::ExtrasNoPreview, Settings::language),
                             panelX, panelY + 24, 10,
                             {120, 120, 140, 255});
                }
            } else {
                DrawText(uiText(UiText::ExtrasNotBuilt, Settings::language),
                         panelX, panelY + 24, 10,
                         {120, 120, 140, 255});
            }

            const int infoY = panelY + 64;
            DrawText(sel.name.c_str(), panelX, infoY, 10, {255, 230, 90, 255});
            int y = infoY + 14;
            if (!sel.origin.empty()) {
                DrawText((std::string(uiText(UiText::ExtrasOriginPrefix,
                                             Settings::language)) + sel.origin).c_str(),
                         panelX, y, 10, {200, 200, 220, 255});
                y += 12;
            }
            if (!sel.source.empty()) {
                // Wrap source tag to fit the preview panel width.
                DrawText(sel.source.c_str(), panelX, y, 10, {160, 160, 190, 255});
            }
        }
    }

    int footY = INTERNAL_HEIGHT - 16;
    std::string hint = Input::isGamepadConnected()
        ? std::string("DPAD:") + uiText(UiText::PurposeNavigate, Settings::language) +
          "  A:" + uiText(UiText::PurposeLaunch, Settings::language) +
          "  B:" + uiText(UiText::PurposeQuit, Settings::language)
        : keyboardVerticalHint(Input::bindings(), uiText(UiText::PurposeNavigate,
                                                         Settings::language)) + "  " +
          keyboardActionHint(Input::bindings(), InputAction::Confirm,
                             uiText(UiText::PurposeLaunch, Settings::language)) + "  " +
          keyboardActionHint(Input::bindings(), InputAction::Cancel,
                             uiText(UiText::PurposeQuit, Settings::language));
    DrawText(hint.c_str(), pad, footY, 10, {150, 150, 170, 255});

    // Fade-in from black on scene entry
    if (fadeInTimer_ > 0) {
        int alpha = 255 * fadeInTimer_ / FADE_IN_DURATION;
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
                      {0, 0, 0, static_cast<unsigned char>(alpha)});
    }
}

} // namespace mmx
