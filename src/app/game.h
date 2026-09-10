#pragma once

#include "scene_manager.h"
#include "systems/raylib_resource.h"
#include "raylib.h"
#include <string>

// ============================================================================
// game.h — Main game class
//
// Owns the window, the render target, and the fixed-timestep game loop.
//
// How the loop works:
//   1. Measure real elapsed time since last frame
//   2. Add it to an accumulator
//   3. While accumulator >= PHYSICS_DT:
//        - Process input
//        - Advance physics by exactly PHYSICS_DT
//        - Subtract PHYSICS_DT from accumulator
//   4. Calculate interpolation alpha = accumulator / PHYSICS_DT
//   5. Render using alpha to blend between previous and current state
//
// This gives us deterministic, frame-rate-independent physics. On a 60Hz
// monitor, we get exactly 1 physics step per frame. On 144Hz, we get
// roughly 1 step every 2.4 frames, with interpolation filling the gaps.
// The game feels identical on both.
//
// We render to an internal RenderTexture2D at SNES resolution (256x224),
// then draw that scaled up to the window. This preserves crisp pixel art.
// ============================================================================

namespace mmx {

class Game {
public:
    explicit Game(bool hiddenWindow = false);
    ~Game();

    // No copy/move — singleton-like, owns the window
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    void run();

    SceneManager& sceneManager() { return sceneManager_; }

    // --autotest <stage-id>[:profile] — called from main before run(). Boots
    // directly into the stage (no title), runs a named profile's scripted
    // input sequence, captures screenshots at profile-defined moments, and
    // exits. Output files default to build/autotest/autotest_<stage>_<profile>_<NN>.png
    // unless MMX_AUTOTEST_SHOT_DIR is set. Profile names and default durations
    // live in autotest_profiles.
    void enableAutotest(const std::string& stageId, const std::string& profile = "walk");
    void enableFiniteSmoke(const std::string& id,
                           const std::string& profile = "boot",
                           int frames = 240);

private:
    void processFrame();
    void autotestTick();  // Drives scripted input + captures shots + exit
    void finiteSmokeTick();
    void configureFiniteSmokeFromEnv();
    void saveFramebufferShot(const char* path);
    void autotestSnap(int atFrame);  // Helper: snap a shot when frame matches
    void finiteSmokeSnap(int atFrame);

    RenderTextureResource target_;     // Internal SNES-resolution framebuffer
    SceneManager sceneManager_;
    float accumulator_ = 0.0f;
    bool running_ = true;
    bool audioDeviceInitialized_ = false;

    // Autotest state (only used when enableAutotest was called)
    bool autotestEnabled_ = false;
    std::string autotestStageId_;
    std::string autotestProfile_ = "walk";
    int autotestFrame_ = 0;
    int autotestShotsTaken_ = 0;
    int autotestTotalFrames_ = 420;

    bool finiteSmokeEnabled_ = false;
    std::string finiteSmokeId_;
    std::string finiteSmokeProfile_ = "boot";
    int finiteSmokeFrame_ = 0;
    int finiteSmokeShotsTaken_ = 0;
    int finiteSmokeTotalFrames_ = 240;
};

} // namespace mmx
