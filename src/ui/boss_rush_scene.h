// boss_rush_scene.h - declares the boss-rush scene state.
// Owns: boss list progress, active fight, player instance, and HUD state.

#pragma once

#include "app/scene.h"
#include "gameplay/gameplay_scene.h"
#include "app/scene_manager.h"
#include <memory>
#include <string>
#include <vector>

namespace mmx {

class BossRushScene : public Scene {
public:
    void setSceneManager(SceneManager* mgr) { sceneManager_ = mgr; }

    void onEnter() override;
    void pollInput() override;
    void handleInput() override;
    void update(float dt) override;
    void render(float alpha) override;

private:
    enum class State { Title, Fighting, Transition, Results };
    State state_ = State::Title;

    struct BossFight {
        std::string stageId;
        std::string bossId;
        std::string displayName;
        int frames = 0;       // Frames to defeat this boss
        int damageTaken = 0;  // HP lost during this fight
    };

    std::vector<BossFight> fights_;
    int currentFight_ = 0;
    int totalFrames_ = 0;
    int totalDamageTaken_ = 0;
    int fightFrames_ = 0;     // Frames spent on current fight
    int transitionTimer_ = 0;
    int titleTimer_ = 0;
    int resultsTimer_ = 0;
    int fadeInTimer_ = 0;
    static constexpr int FADE_IN_DURATION = 30;
    int playerMaxHP_ = 0;

    std::unique_ptr<GameplayScene> gameplay_;
    SceneManager* sceneManager_ = nullptr;

    void startFight(int index);
    std::string stagePathFor(const std::string& stageId);
    Vector2 findBossSpawn(const std::string& stagePath);
};

} // namespace mmx
