// title_scene.h - declares title-screen state and menu transition data.
// Owns: title menu selection, timers, assets, and next-scene routing.

#pragma once

#include "app/scene.h"
#include "app/constants.h"
#include "data/content_pack.h"
#include "data/save_system.h"
#include "systems/raylib_resource.h"
#include "ui/menu_flow.h"
#include "ui/password_entry.h"

#include <array>
#include <optional>

// ============================================================================
// title_scene.h — Title screen
//
// Shows the game logo and waits for START before revealing the menu:
//   - Start Game (→ stage select or directly to Intro Highway)
//   - Options (future: volume, controls)
//
// The title screen loads the content pack manifest to know what's available.
// After selecting "Start Game", it transitions to the stage select scene
// (or directly to gameplay if there's only one available stage).
//
// Visual style: dark background with the source-backed title logo and CAPCOM
// mark. The settled source title has no blinking prompt.
// ============================================================================

namespace mmx {

// Forward declare — the title scene needs to push scenes onto the manager
class SceneManager;
class TextureResource;

class TitleScene : public Scene {
public:
    // The scene manager is passed so we can push new scenes on transitions
    void setSceneManager(SceneManager* mgr) { sceneManager_ = mgr; }

    void onEnter() override;
    void pollInput() override;
    void handleInput() override;
    void update(float dt) override;
    void render(float alpha) override;

private:
    SceneManager* sceneManager_ = nullptr;
    ContentPack contentPack_;

    enum class State {
        PressStart,  // Waiting for player to press start
        Menu,        // Showing menu options
        SaveSlots,   // Choosing the save slot for New Game
        Password     // Entering the original 4 x 3 password grid
    };
    State state_ = State::PressStart;

    enum class SlotAction {
        None,
        NewGame
    };

    int menuSelection_ = 0;
    static constexpr int MENU_ITEMS = kTitleMenuItemCount;
    int slotSelection_ = 1;
    SlotAction slotAction_ = SlotAction::None;
    PasswordEntryState passwordEntry_;
    std::array<std::optional<SaveSummary>, SaveSystem::kSaveSlotCount> saveSlotSummaries_ = {};
    bool hasSaveFile_ = false;
    int saveStagesCleared_ = 0;
    int saveDeaths_ = 0;
    int savePlayMinutes_ = 0;
    int saveEnemiesDefeated_ = 0;
    int saveBossesDefeated_ = 0;
    bool saveSigmaDefeated_ = false;
    int timer_ = 0;        // General-purpose animation timer
    int inputDelay_ = 0;   // D-pad repeat delay (frames before next move)
    bool transitioning_ = false;
    int fadeInTimer_ = 0;
    static constexpr int FADE_IN_DURATION = 45; // Slower fade for title atmosphere
    const TextureResource* windowSheet_ = nullptr;
    const TextureResource* titleLogo_ = nullptr;  // U50: ripped real logo
    // U75: ripped menu rows (3 rows x 14px, cyan/orange states) + the X
    // selector sprite that stands beside the selected row.
    const TextureResource* menuRowsCyan_ = nullptr;
    const TextureResource* menuRowsOrange_ = nullptr;
    const TextureResource* selectorX_ = nullptr;
    const TextureResource* titleExtraFontCyan_ = nullptr;
    const TextureResource* passwordSheet_ = nullptr;
    TextureResource titleExtraFontOrange_;

    void loadMenuSprites();
    void refreshSaveSummaries();
    const SaveSummary* summaryForSlot(int slotIndex) const;
    bool slotHasSave(int slotIndex) const;
    int firstSaveSlotIndex() const;
    int firstEmptySlotIndex() const;
    void useSummaryForMenuStats(int slotIndex);
    void openSaveSlotSelect(SlotAction action);
    void openPasswordEntry();
    void submitPassword();
    void beginSelectedSlotAction();
    void launchIntroHighway();
    void launchStageSelect();
    void launchSelectedNewGame();
    void renderBackdrop() const;
    void renderTitleLogo() const;
    void renderTitleIdle() const;
    void renderMainMenu() const;
    void renderSaveSlots() const;
    void renderPasswordEntry() const;
};

} // namespace mmx
