// extras_scene.h - declares the extras menu scene.
// Owns: selection index, unlocked-stage view state, and thumbnail resources.

#pragma once

#include "app/scene.h"
#include "data/content_pack.h"
#include "ui/map_editor_user_maps.h"
#include "raylib.h"
#include <string>
#include <unordered_map>

// ============================================================================
// extras_scene.h — Non-MMX1 content browser
//
// Shows a vertical list of fork/extra stages (Magma Dragoon = MMX4, Blade Man
// = MM10, Avalanche and Headquarters = axlforte originals) with a thumbnail
// preview of the currently-selected stage. Selecting one launches
// GameplayScene pointing at its config under content/extras/.
//
// Kept separate from the main stage_select_scene so the canonical MMX1 UI
// stays a 9-slot boss grid. This scene is reached via the --extras CLI flag
// and doesn't depend on the title_scene menu (which is pre-Session-14 WIP).
// ============================================================================

namespace mmx {

class SceneManager;
class TextureResource;

class ExtrasScene : public Scene {
public:
    ExtrasScene() = default;
    ~ExtrasScene() override = default;
    ExtrasScene(const ExtrasScene&) = delete;
    ExtrasScene& operator=(const ExtrasScene&) = delete;
    ExtrasScene(ExtrasScene&&) = delete;
    ExtrasScene& operator=(ExtrasScene&&) = delete;

    void setSceneManager(SceneManager* mgr) { sceneManager_ = mgr; }
    void setContentPack(const ContentPack& pack) { contentPack_ = pack; }

    void onEnter() override;
    void onExit() override;
    void pollInput() override;
    void handleInput() override;
    void update(float dt) override;
    void render(float alpha) override;

private:
    SceneManager* sceneManager_ = nullptr;
    ContentPack contentPack_;

    int cursor_ = 0;
    int inputDelay_ = 0;
    bool transitioning_ = false;
    int transTimer_ = 0;
    int fadeInTimer_ = 0;
    std::string customMapError_;
    int customMapErrorTimer_ = 0;
    static constexpr int FADE_IN_DURATION = 30;
    static constexpr int CUSTOM_MAP_ERROR_DURATION = 180;

    // Lazily resolved thumbnail borrows. Keyed by stage id or custom thumbnail
    // path so nullptr entries are a negative cache and do not retry each frame.
    std::unordered_map<std::string, const TextureResource*> thumbnails_;
    const TextureResource* getThumbnail(const std::string& stageId);
    const TextureResource* getCustomMapThumbnail(const map_editor::UserMapEntry& entry);

    std::vector<map_editor::UserMapEntry> customMaps_;

    int totalEntries() const;
    int customStartIndex() const;
    bool cursorOnCustomMap() const;
    int customMapIndexForCursor() const;
};

} // namespace mmx
