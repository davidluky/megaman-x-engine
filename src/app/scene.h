#pragma once

// ============================================================================
// scene.h — Scene interface
//
// A Scene represents a distinct game state: title screen, gameplay, pause menu,
// game over, etc. The SceneManager holds a stack of scenes. The top scene gets
// input and updates. Scenes below it can optionally still render (for overlays
// like the pause menu drawn on top of gameplay).
//
// Each scene has three lifecycle methods:
//   update()  — called once per physics tick (fixed timestep)
//   render()  — called once per frame (variable rate, receives interpolation alpha)
//   handleInput() — called once per physics tick before update
// ============================================================================

namespace mmx {

// Forward declaration — scenes can request transitions via the manager
class SceneManager;

class Scene {
public:
    virtual ~Scene() = default;

    // Called when this scene becomes the active top of the stack.
    // Use for initialization that depends on being "on screen."
    virtual void onEnter() {}

    // Called when this scene is removed from the stack.
    virtual void onExit() {}

    // Called when another scene is pushed on top of this one.
    // The scene stays in the stack but loses focus.
    virtual void onPause() {}

    // Called when the scene above this one is popped, restoring focus.
    virtual void onResume() {}

    // Called once per render frame (before physics loop) to latch key presses.
    // On high-refresh monitors, many frames have no physics tick. Key presses
    // during those frames would be lost if only checked in handleInput().
    // Override this to capture IsKeyPressed() events at frame rate.
    virtual void pollInput() {}

    // Process input. Called once per physics tick.
    virtual void handleInput() = 0;

    // Advance game logic by one fixed timestep.
    virtual void update(float dt) = 0;

    // Draw the scene. Alpha is the interpolation factor (0.0–1.0) between
    // the previous and current physics state, used for smooth rendering.
    virtual void render(float alpha) = 0;

    // If true, the scene below this one in the stack still renders.
    // Used for overlay scenes like pause menus.
    virtual bool isTransparent() const { return false; }
};

} // namespace mmx
