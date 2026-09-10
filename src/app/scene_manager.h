#pragma once

#include <memory>
#include <vector>
#include "scene.h"
#include "app/runtime_state.h"

// ============================================================================
// scene_manager.h — Manages the scene stack
//
// Scenes are organized as a stack. The topmost scene is "active" — it receives
// input and updates. Scenes below may still render if the one above is
// transparent (e.g., a pause overlay on top of gameplay).
//
// Scene transitions are queued and applied at the start of the next frame,
// not immediately. This prevents issues where a scene modifies the stack
// during its own update (e.g., gameplay scene pushing a pause scene mid-update).
// ============================================================================

namespace mmx {

class SceneManager {
public:
    // Queue a scene to replace the entire stack (clear all, push new).
    // Use for hard transitions like title → gameplay.
    void changeScene(std::unique_ptr<Scene> scene);

    // Queue a scene to push on top of the stack.
    // Use for overlays like pause menus.
    void pushScene(std::unique_ptr<Scene> scene);

    // Queue removal of the top scene.
    // Use to dismiss overlays.
    void popScene();

    // Called once per render frame to latch input (before physics loop)
    void pollInput();

    // Apply queued transitions, then run one physics tick.
    void handleInput();
    void update(float dt);

    // Render all visible scenes (bottom-up, respecting transparency).
    void render(float alpha);

    // Tear down every active scene immediately. Used by Game shutdown so
    // scene-owned raylib resources unload before CloseWindow().
    void clear();

    // Scenes may request process shutdown, but only Game owns raylib teardown.
    void requestQuit() { quitRequested_ = true; }
    bool quitRequested() const { return quitRequested_; }

    bool isEmpty() const { return scenes_.empty(); }

    RuntimeState& runtimeState() { return runtimeState_; }
    const RuntimeState& runtimeState() const { return runtimeState_; }

    // Hard scene swaps (changeScene) fade in from black. Disabled during
    // autotest/smoke runs so captured frames stay deterministic.
    void setTransitionsEnabled(bool enabled) { transitionsEnabled_ = enabled; }

private:
    // Pending transition types
    enum class TransitionType { Change, Push, Pop };
    struct PendingTransition {
        TransitionType type;
        std::unique_ptr<Scene> scene; // null for Pop
    };

    void applyPendingTransitions();

    RuntimeState runtimeState_;
    std::vector<std::unique_ptr<Scene>> scenes_;
    std::vector<PendingTransition> pending_;
    bool quitRequested_ = false;

    // Fade-in-from-black overlay played after a hard scene swap.
    bool transitionsEnabled_ = true;
    int transitionTimer_ = 0;                 // Frames remaining in the fade
    static constexpr int kTransitionFrames = 18;
};

} // namespace mmx
