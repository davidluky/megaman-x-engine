// stage_select_scene.h - declares the stage-select scene state.
// Owns: stage grid selection, cursor animation, previews, and menu mode.

#pragma once

#include "app/scene.h"
#include "app/constants.h"
#include "data/content_pack.h"
#include "data/game_ids.h"
#include "systems/randomizer.h"
#include "raylib.h"
#include <array>
#include <optional>

// ============================================================================
// stage_select_scene.h — MMX-style 8-boss stage select grid
//
// Displays a 3x3 grid (center = X portrait, 8 surrounding = bosses).
// Player navigates with d-pad/arrows, confirms with jump/confirm.
// Unavailable stages (no config file) are shown as locked/dimmed.
//
// After selecting a stage, transitions to GameplayScene with the
// selected stage path.
//
// For Phase 1, only Intro Highway is available. The other 8 slots
// show boss names but are locked. This structure is ready for Phase 2+.
// ============================================================================

namespace mmx {

class SceneManager;
class TextureResource;

class StageSelectScene : public Scene {
public:
    StageSelectScene() = default;
    ~StageSelectScene() override = default;
    StageSelectScene(const StageSelectScene&) = delete;
    StageSelectScene& operator=(const StageSelectScene&) = delete;
    StageSelectScene(StageSelectScene&&) = delete;
    StageSelectScene& operator=(StageSelectScene&&) = delete;

    void setSceneManager(SceneManager* mgr) { sceneManager_ = mgr; }
    void setContentPack(const ContentPack& pack) { contentPack_ = pack; }
    void setRandomizer(uint64_t seed, const RandomizerConfig& config = RandomizerConfig{});

    void onEnter() override;
    void onExit() override;
    void pollInput() override;
    void handleInput() override;
    void update(float dt) override;
    void render(float alpha) override;

private:
    SceneManager* sceneManager_ = nullptr;
    ContentPack contentPack_;

    enum class RouteMode { Mavericks, Fortress };

    // Grid positions: 0-8 map to the 3x3 grid. Index 4 = center (X portrait).
    // The route catalog maps selectable positions to manifest stage orders.
    int cursorPos_ = 0; // Current cursor position in grid (0-8)
    RouteMode routeMode_ = RouteMode::Mavericks;
    int timer_ = 0;
    int inputDelay_ = 0;  // D-pad repeat delay
    bool transitioning_ = false;
    bool returnToTitle_ = false;
    int transTimer_ = 0;
    int fadeInTimer_ = 0;
    static constexpr int FADE_IN_DURATION = 30;

    // Mapping from grid position to stage index in contentPack_.stages
    // Grid layout (MMX classic):
    //   0  1  2
    //   3 [X] 5
    //   6  7  8
    // Position 4 = center, not a selectable stage
    static constexpr int GRID_SIZE = 9;
    static constexpr int CENTER = 4;

    // Get the stage info for a grid position (nullptr if center or out of range)
    const StageInfo* getStageForGrid(int gridPos) const;
    int stageOrderForGrid(int gridPos) const;
    bool fortressRouteUnlocked() const;
    bool isGridSelectable(int gridPos) const;
    void refreshRouteMode();
    void placeCursorOnFirstStage();
    std::optional<BossId> getBossForGrid(int gridPos) const;
    const char* getBossLabel(BossId bossId) const;
    Color getBossColorForGrid(int gridPos) const;
    void refreshBossGrid();

    // Navigation helpers
    void moveCursor(int dx, int dy);
    void confirmCurrentStage();

    // Borrowed from AssetCache. The cache outlives active scenes and is cleared
    // by Game after scenes are destroyed.
    std::array<const TextureResource*, GRID_SIZE> portraits_{};  // Index matches grid position
    std::array<const TextureResource*, GRID_SIZE> stageSelectScreens_{}; // Exact MMX1 cursor screenshots
    // U51: ON-phase rips (selected-cell corner flash). Measured cadence
    // (u51 blink capture, 140f): ON 7, OFF 4, ON 8, OFF 4 — period 23.
    std::array<const TextureResource*, GRID_SIZE> stageSelectBlinkScreens_{};
    int blinkTick_ = 0;
    int hoveredUtilityTab_ = -1;
    const TextureResource* xPortrait_ = nullptr;
    bool portraitsLoaded_ = false;
    bool randomizerMode_ = false;
    uint64_t randomizerSeed_ = 0;
    RandomizerConfig randomizerConfig_;
    std::array<std::optional<BossId>, GRID_SIZE> bossGrid_;
    void loadPortraits();
    void unloadPortraits();
};

} // namespace mmx
