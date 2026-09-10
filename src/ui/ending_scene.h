// ending_scene.h - declares the ending scene.
// Owns: ending timer state and scene-manager return path.

#pragma once

#include "app/scene.h"
#include "app/constants.h"

namespace mmx {

class SceneManager;

class EndingScene : public Scene {
public:
    void setSceneManager(SceneManager* mgr) { sceneManager_ = mgr; }

    void onEnter() override;
    void pollInput() override;
    void handleInput() override;
    void update(float dt) override;
    void render(float alpha) override;

private:
    SceneManager* sceneManager_ = nullptr;
    int timer_ = 0;
    int fadeInTimer_ = 0;
    static constexpr int FADE_IN_DURATION = 60;
};

} // namespace mmx
