// map_editor_load_browser.h - saved-map load browser drawing helpers.

#pragma once

#include "data/localization.h"
#include "systems/asset_cache.h"
#include "systems/raylib_resource.h"
#include "ui/map_editor_thumbnail.h"
#include "ui/map_editor_user_maps.h"
#include "raylib.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace mmx::map_editor {

inline constexpr int kLoadBrowserPanelWidth = 240;
inline constexpr int kLoadBrowserPanelHeight = 138;
inline constexpr int kLoadBrowserVisibleRows = 6;

inline int loadBrowserFirstVisibleIndex(int selection, int count) {
    int first = 0;
    if (selection >= kLoadBrowserVisibleRows) {
        first = selection - kLoadBrowserVisibleRows + 1;
    }
    return std::max(0, std::min(first, std::max(0, count - kLoadBrowserVisibleRows)));
}

inline Rectangle loadBrowserPanelRect(int internalWidth, int internalHeight) {
    return {
        static_cast<float>((internalWidth - kLoadBrowserPanelWidth) / 2),
        static_cast<float>((internalHeight - kLoadBrowserPanelHeight) / 2),
        static_cast<float>(kLoadBrowserPanelWidth),
        static_cast<float>(kLoadBrowserPanelHeight),
    };
}

inline const TextureResource* getLoadBrowserThumbnail(
    const UserMapEntry& entry,
    std::unordered_map<std::string, const TextureResource*>& thumbnails) {
    if (entry.thumbnailPath.empty()) return nullptr;

    const std::string key = entry.thumbnailPath.generic_string();
    auto it = thumbnails.find(key);
    if (it != thumbnails.end()) return it->second;

    const TextureResource* thumbnail = nullptr;
    if (FileExists(key.c_str())) {
        thumbnail = AssetCache::loadTexture(key);
        if (thumbnail) {
            thumbnail->setFilter(TEXTURE_FILTER_POINT);
        }
    }
    thumbnails.emplace(key, thumbnail);
    return thumbnail;
}

enum class LoadBrowserAction {
    None,
    CloseWithStatus,
    LoadSelected,
};

struct LoadBrowserInputResult {
    LoadBrowserAction action = LoadBrowserAction::None;
    int selection = 0;
    UiText status = UiText::MapEditorStatusNoSavedMaps;
    std::string loadPath;
};

struct LoadBrowserOpenResult {
    std::vector<UserMapEntry> entries;
    int selection = 0;
    bool open = false;
    bool hasStatus = false;
    UiText status = UiText::MapEditorStatusNoSavedMaps;
    bool loadLegacySlot = false;
    std::string loadPath;
};

inline LoadBrowserOpenResult openLoadBrowser() {
    LoadBrowserOpenResult result;
    result.entries = listUserMaps();
    if (result.entries.empty()) {
        if (std::filesystem::exists(legacySingleSlotPath())) {
            result.loadLegacySlot = true;
            result.loadPath = legacySingleSlotPath().generic_string();
        } else {
            result.hasStatus = true;
            result.status = UiText::MapEditorStatusNoSavedMaps;
        }
        return result;
    }
    result.open = true;
    result.hasStatus = true;
    result.status = UiText::MapEditorLoadMapTitle;
    return result;
}

inline LoadBrowserInputResult handleLoadBrowserInput(
    const std::vector<UserMapEntry>& entries,
    int selection) {
    const int count = static_cast<int>(entries.size());
    LoadBrowserInputResult result;
    result.selection = count > 0 ? std::clamp(selection, 0, count - 1) : 0;
    if (count <= 0) {
        result.action = LoadBrowserAction::CloseWithStatus;
        result.status = UiText::MapEditorStatusNoSavedMaps;
        return result;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        result.action = LoadBrowserAction::CloseWithStatus;
        result.status = UiText::MapEditorStatusLoadCancelled;
        return result;
    }

    if (IsKeyPressed(KEY_UP)) {
        result.selection = (result.selection - 1 + count) % count;
    }
    if (IsKeyPressed(KEY_DOWN)) {
        result.selection = (result.selection + 1) % count;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        result.action = LoadBrowserAction::LoadSelected;
        result.loadPath = entries[result.selection].path.generic_string();
    }
    return result;
}

inline void renderLoadBrowser(int internalWidth,
                              int internalHeight,
                              const std::vector<UserMapEntry>& entries,
                              int selection,
                              const TextureResource* selectedPreview,
                              Language language) {
    const Rectangle panelRect = loadBrowserPanelRect(internalWidth, internalHeight);
    const int panelX = static_cast<int>(panelRect.x);
    const int panelY = static_cast<int>(panelRect.y);
    DrawRectangle(panelX, panelY, kLoadBrowserPanelWidth, kLoadBrowserPanelHeight,
                  {0, 12, 48, 238});
    DrawRectangleLines(panelX, panelY, kLoadBrowserPanelWidth, kLoadBrowserPanelHeight,
                       {210, 235, 255, 255});
    DrawRectangleLines(panelX + 2, panelY + 2, kLoadBrowserPanelWidth - 4,
                       kLoadBrowserPanelHeight - 4, {74, 148, 236, 255});

    DrawText(uiText(UiText::MapEditorLoadMapTitle, language),
             panelX + 10, panelY + 8, 8, {255, 242, 116, 255});

    const int previewX = panelX + 108;
    const int previewY = panelY + 28;
    const int previewW = kUserMapThumbnailWidth;
    const int previewH = kUserMapThumbnailHeight;
    const int count = static_cast<int>(entries.size());
    const int selected = count > 0 ? std::clamp(selection, 0, count - 1) : 0;
    const int first = loadBrowserFirstVisibleIndex(selected, count);

    for (int row = 0; row < kLoadBrowserVisibleRows && first + row < count; ++row) {
        const int index = first + row;
        const UserMapEntry& entry = entries[index];
        const bool isSelected = index == selected;
        const int y = panelY + 24 + row * 11;
        const Color rowColor = isSelected ? Color{255, 230, 90, 255}
                                          : Color{220, 220, 230, 255};
        if (isSelected) {
            DrawText(">", panelX + 8, y, 7, rowColor);
        }

        std::string name = entry.name;
        while (MeasureText(name.c_str(), 7) > 62 && name.size() > 1) {
            name.pop_back();
        }
        DrawText(name.c_str(), panelX + 18, y, 7, rowColor);
        if (!entry.thumbnailPath.empty()) {
            DrawText(uiText(UiText::MapEditorImageBadge, language),
                     panelX + 82, y, 7,
                     isSelected ? Color{255, 242, 116, 255}
                                : Color{120, 150, 200, 255});
        }

        if (isSelected) {
            DrawText(entry.sizeLabel.c_str(), previewX, previewY + previewH + 5,
                     7, Color{255, 242, 116, 255});
            DrawText(entry.modifiedDate.c_str(), previewX + 38,
                     previewY + previewH + 5, 7, Color{150, 170, 200, 255});
        }
    }

    DrawRectangle(previewX - 1, previewY - 1, previewW + 2, previewH + 2,
                  {4, 18, 46, 255});
    DrawRectangleLines(previewX - 1, previewY - 1, previewW + 2, previewH + 2,
                       {96, 174, 236, 190});
    if (count > 0) {
        if (selectedPreview && selectedPreview->valid()) {
            const Rectangle src = {
                0.0f,
                0.0f,
                static_cast<float>(selectedPreview->width()),
                static_cast<float>(selectedPreview->height()),
            };
            const Rectangle dst = {
                static_cast<float>(previewX),
                static_cast<float>(previewY),
                static_cast<float>(previewW),
                static_cast<float>(previewH),
            };
            DrawTexturePro(selectedPreview->get(), src, dst, {0.0f, 0.0f}, 0.0f,
                           WHITE);
        } else {
            const char* noPreview = uiText(UiText::ExtrasNoPreview, language);
            const int labelW = MeasureText(noPreview, 7);
            DrawText(noPreview, previewX + (previewW - labelW) / 2,
                     previewY + previewH / 2 - 4, 7,
                     {150, 170, 200, 255});
        }
    }

    DrawText(uiText(UiText::MapEditorEnterLoadCancelHint, language),
             panelX + 10, panelY + kLoadBrowserPanelHeight - 13, 7,
             {188, 212, 244, 255});
}

} // namespace mmx::map_editor
