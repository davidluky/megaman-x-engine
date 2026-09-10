// animation.cpp - advances named frame animations and playback state.
// Owns: animation frame clocks, current-frame selection, and sync playback.

#include "systems/animation.h"

namespace mmx {

void AnimationPlayer::addAnimation(const std::string& name, Animation anim) {
    animations_[name] = std::move(anim);
}

void AnimationPlayer::play(const std::string& name) {
    if (currentAnim_ == name && !finished_) return; // Already playing
    currentAnim_ = name;
    frameIdx_ = 0;
    tickCount_ = 0;
    finished_ = false;
}

void AnimationPlayer::playSynced(const std::string& name) {
    // Switch animation but carry the current cycle phase over, so two parallel
    // loops (e.g. run / walk_shoot — same stride, different arm) stay in step
    // instead of snapping back to frame 0 on every toggle.
    if (currentAnim_ == name && !finished_) return;
    auto it = animations_.find(name);
    int frameCount = (it != animations_.end()) ? static_cast<int>(it->second.frames.size()) : 0;
    currentAnim_ = name;
    if (frameCount > 0) {
        frameIdx_ = frameIdx_ % frameCount;
    } else {
        frameIdx_ = 0;
        tickCount_ = 0;
    }
    finished_ = false;
}

void AnimationPlayer::tick() {
    if (currentAnim_.empty() || finished_) return;

    auto it = animations_.find(currentAnim_);
    if (it == animations_.end()) return;

    const auto& anim = it->second;
    if (anim.frames.empty()) return;

    tickCount_++;
    if (tickCount_ >= anim.frames[frameIdx_].duration) {
        tickCount_ = 0;
        frameIdx_++;
        if (frameIdx_ >= static_cast<int>(anim.frames.size())) {
            if (anim.loop) {
                frameIdx_ = 0;
            } else {
                frameIdx_ = static_cast<int>(anim.frames.size()) - 1;
                finished_ = true;
            }
        }
    }
}

int AnimationPlayer::currentFrameIndex() const {
    if (currentAnim_.empty()) return 0;
    auto it = animations_.find(currentAnim_);
    if (it == animations_.end() || it->second.frames.empty()) return 0;
    return it->second.frames[frameIdx_].index;
}

Color AnimationPlayer::getPlaceholderColor() const {
    // Each animation state gets a distinct color so you can see state transitions
    // visually even without real sprites
    if (currentAnim_ == "idle")   return {0, 120, 215, 255};   // Blue (standing)
    if (currentAnim_ == "run")    return {0, 180, 255, 255};   // Light blue (moving)
    if (currentAnim_ == "jump")   return {100, 200, 255, 255}; // Cyan (ascending)
    if (currentAnim_ == "fall")   return {60, 100, 200, 255};  // Dark cyan (descending)
    if (currentAnim_ == "dash")   return {255, 140, 0, 255};   // Orange (dashing)
    if (currentAnim_ == "wall")   return {180, 100, 255, 255}; // Purple (wall sliding)
    if (currentAnim_ == "hurt")   return {255, 60, 60, 255};   // Red (taking damage)
    return SKYBLUE;
}

} // namespace mmx
