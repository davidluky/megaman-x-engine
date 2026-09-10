#include "scene_manager.h"
#include "app/input.h"
#include "app/constants.h"
#include "raylib.h"

namespace mmx {

void SceneManager::changeScene(std::unique_ptr<Scene> scene) {
    pending_.push_back({TransitionType::Change, std::move(scene)});
}

void SceneManager::pushScene(std::unique_ptr<Scene> scene) {
    pending_.push_back({TransitionType::Push, std::move(scene)});
}

void SceneManager::popScene() {
    pending_.push_back({TransitionType::Pop, nullptr});
}

void SceneManager::applyPendingTransitions() {
    const bool applyingTransitions = !pending_.empty();
    bool didHardSwap = false;
    for (auto& t : pending_) {
        switch (t.type) {
            case TransitionType::Change:
                // Clear entire stack
                while (!scenes_.empty()) {
                    scenes_.back()->onExit();
                    scenes_.pop_back();
                }
                scenes_.push_back(std::move(t.scene));
                scenes_.back()->onEnter();
                didHardSwap = true;
                break;

            case TransitionType::Push:
                if (!scenes_.empty()) {
                    scenes_.back()->onPause();
                }
                scenes_.push_back(std::move(t.scene));
                scenes_.back()->onEnter();
                break;

            case TransitionType::Pop:
                if (!scenes_.empty()) {
                    scenes_.back()->onExit();
                    scenes_.pop_back();
                    if (!scenes_.empty()) {
                        scenes_.back()->onResume();
                    }
                }
                break;
        }
    }
    pending_.clear();
    if (didHardSwap && transitionsEnabled_) {
        transitionTimer_ = kTransitionFrames;
    }
    if (applyingTransitions) {
        // A button press that confirmed one scene should not immediately
        // activate the first item in the next scene on the same physics tick.
        Input::consumeConfirmPress();
        Input::consumeJumpPress();
        Input::consumeCancelPress();
        Input::consumePausePress();
        Input::clearScripted();
    }
}

void SceneManager::pollInput() {
    if (!scenes_.empty()) {
        scenes_.back()->pollInput();
    }
}

void SceneManager::handleInput() {
    applyPendingTransitions();
    if (!scenes_.empty()) {
        scenes_.back()->handleInput();
    }
}

void SceneManager::update(float dt) {
    if (transitionTimer_ > 0) transitionTimer_--;
    if (!scenes_.empty()) {
        scenes_.back()->update(dt);
    }
}

void SceneManager::render(float alpha) {
    if (scenes_.empty()) return;

    // Find the lowest scene that should render.
    // Walk down from the top until we find one that isn't transparent.
    int firstVisible = static_cast<int>(scenes_.size()) - 1;
    while (firstVisible > 0 && scenes_[firstVisible]->isTransparent()) {
        firstVisible--;
    }

    // Render bottom-up so overlays draw on top
    for (int i = firstVisible; i < static_cast<int>(scenes_.size()); i++) {
        scenes_[i]->render(alpha);
    }

    // Fade-in-from-black overlay after a hard scene swap.
    if (transitionTimer_ > 0) {
        const int a = 255 * transitionTimer_ / kTransitionFrames;
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
                      {0, 0, 0, static_cast<unsigned char>(a)});
    }
}

void SceneManager::clear() {
    pending_.clear();
    while (!scenes_.empty()) {
        scenes_.back()->onExit();
        scenes_.pop_back();
    }
}

} // namespace mmx
