// boss_intro_scene.h - declares the pre-fight boss intro scene.
// Owns: boss intro identity, animation timer, and next-stage transition data.

#pragma once

#include "app/scene.h"
#include "data/game_ids.h"
#include "systems/raylib_resource.h"
#include <string>

// ============================================================================
// boss_intro_scene.h — the MMX1 boss-intro cutscene (the "boss reveal" shown
// when you select an uncleared stage). Data-driven + swappable: the shared
// star/band backdrop and choreography are fixed; the per-stage hold image and
// boss sprite are parameters, so a future custom boss can reuse the animation.
//
// Reference: emulator-captured frame-by-frame (build/oracle_runs/sweep/<stage>),
// assets in content/x1/sprites/boss_intro/. The HOLD draws the pixel-exact
// captured frame so the 8 known bosses match the real game.
// ============================================================================

namespace mmx {

class SceneManager;

class BossIntroScene : public Scene {
public:
    // Set before pushing — which stage this intro precedes.
    std::string stageId;        // engine id, e.g. "chill-penguin"
    std::string stagePath;      // gameplay stage json
    std::string characterPath = "content/x1/characters/x.json";
    StageId stageIdEnum;

    void setSceneManager(SceneManager* mgr) { sceneManager_ = mgr; }

    void onEnter() override;
    void pollInput() override {}
    void handleInput() override;
    void update(float dt) override;
    void render(float alpha) override;

private:
    void finish();
    void drawHold(float scale, unsigned char alpha) const;
    void drawBossWalk() const;
    bool drawExactFrameSequence() const;

    int timer_ = 0;
    bool finished_ = false;

    const TextureResource* hold_ = nullptr;        // pixel-exact captured hold frame
    // Swappable layers (full-screen, boss/name at their real positions).
    const TextureResource* backdrop_ = nullptr;    // shared star + band + blue
    const TextureResource* boss_ = nullptr;        // per-stage boss sprite layer
    const TextureResource* name_ = nullptr;        // per-stage name text layer
    const TextureResource* downTri_ = nullptr;     // purple down-triangle (drops in)
    // U52 rebuild assets (the measured phases): the two lightning strikes
    // (full-screen rips) and the per-stage waddle-in cells.
    const TextureResource* starEmblem_ = nullptr;
    const TextureResource* boltA_ = nullptr;
    const TextureResource* boltA2_ = nullptr;
    const TextureResource* boltB_ = nullptr;
    const TextureResource* boltB2_ = nullptr;
    const TextureResource* walk_[4] = {nullptr, nullptr, nullptr, nullptr};
    std::string exactFrameBase_;
    int exactFrameCount_ = 0;

    SceneManager* sceneManager_ = nullptr;
};

} // namespace mmx
