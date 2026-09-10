// animation.h - declares animation frame data and playback controller state.
// Owns: animation clips, frame durations, looping, and current animation API.

#pragma once

#include "raylib.h"
#include <string>
#include <unordered_map>
#include <vector>

// ============================================================================
// animation.h — Sprite animation system
//
// Animations are defined as sequences of frames from a sprite sheet.
// Each frame has an index (position in the sheet) and a duration (in physics
// ticks). The animation player tracks the current frame and advances
// automatically each tick.
//
// Runtime rendering consumes these frame indices from loaded sprite sheets when
// art is available. Placeholder colors remain as a fallback/debug path for
// missing sheets or headless-friendly states.
//
// Sprite sheets are laid out as grids:
//   Frame index 0 = top-left cell
//   Frame index 1 = next cell to the right
//   etc., wrapping to the next row when a row is full
//
// The AnimationPlayer is owned by each entity and drives its visual state.
// State machine transitions set the animation; the player handles timing.
// ============================================================================

namespace mmx {

// One frame in an animation sequence
struct AnimFrame {
    int index;     // Frame index in the sprite sheet grid
    int duration;  // How many physics ticks this frame lasts
};

// A named animation sequence
struct Animation {
    std::string name;
    std::vector<AnimFrame> frames;
    bool loop = true; // false = play once and hold last frame
};

// Plays animations and tracks current frame state
class AnimationPlayer {
public:
    // Register an animation by name
    void addAnimation(const std::string& name, Animation anim);

    // Switch to a named animation. Resets to frame 0 unless already playing it.
    void play(const std::string& name);

    // Switch animation while preserving the current cycle phase (frame index),
    // for swapping between two parallel loops of the same length without a hitch.
    void playSynced(const std::string& name);

    // Advance the animation by one physics tick
    void tick();

    // Get the current frame index (for sprite sheet lookup)
    int currentFrameIndex() const;

    // Check which animation is playing
    const std::string& currentName() const { return currentAnim_; }
    bool isFinished() const { return finished_; }

    // Get a color for fallback/debug rendering when a sprite sheet is unavailable.
    Color getPlaceholderColor() const;

private:
    std::unordered_map<std::string, Animation> animations_;
    std::string currentAnim_;
    int frameIdx_ = 0;   // Which frame in the sequence
    int tickCount_ = 0;  // Ticks spent on current frame
    bool finished_ = false;
};

} // namespace mmx
