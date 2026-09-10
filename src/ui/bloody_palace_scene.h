// bloody_palace_scene.h - declares the Bloody Palace challenge scene.
// Owns: wave state, enemy counts, arena index, and scene-manager handoff.

#pragma once

#include "app/scene.h"
#include "gameplay/gameplay_scene.h"
#include "app/scene_manager.h"
#include <memory>
#include <string>
#include <vector>

namespace mmx {

class BloodyPalaceScene : public Scene {
public:
    void setSceneManager(SceneManager* mgr) { sceneManager_ = mgr; }

    void onEnter() override;
    void pollInput() override;
    void handleInput() override;
    void update(float dt) override;
    void render(float alpha) override;

private:
    enum class State { Title, Fighting, WaveTransition, Results };
    State state_ = State::Title;

    // Enemy spawn definition for wave generation
    struct EnemySpawn {
        std::string enemyId;
        float x = 0;
        float y = 0;
    };

    // Configuration for a single wave
    struct WaveConfig {
        int waveNum = 0;
        std::vector<EnemySpawn> enemies;
        bool isBossWave = false;
        std::string bossId;
    };

    // Wave tracking
    int currentWave_ = 0;
    int totalFrames_ = 0;
    int totalDamageTaken_ = 0;
    int waveFrames_ = 0;

    // Combo system
    int comboCount_ = 0;
    int comboTimer_ = 0;
    int maxCombo_ = 0;
    std::string styleRank_ = "D";   // D/C/B/A/S/SS/SSS

    // Timers
    int fadeInTimer_ = 0;
    static constexpr int FADE_IN_DURATION = 30;
    int transitionTimer_ = 0;
    int titleTimer_ = 0;
    int resultsTimer_ = 0;

    int playerMaxHP_ = 0;

    // Total waves before cycling
    static constexpr int TOTAL_WAVES = 30;

    std::unique_ptr<GameplayScene> gameplay_;
    SceneManager* sceneManager_ = nullptr;

    WaveConfig generateWave(int waveNum);
    std::string stagePathForArena();
    void startWave(int waveNum);
};

} // namespace mmx
