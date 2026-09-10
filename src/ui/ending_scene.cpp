#include <array>
// ending_scene.cpp - displays the ending screen and return-to-title input.
// Owns: ending scene timer, completion handoff, and summary rendering.

#include "ui/ending_scene.h"
#include "ui/title_scene.h"
#include "app/scene_manager.h"
#include "app/input.h"
#include "data/localization.h"
#include "data/settings.h"
#include "systems/audio.h"
#include "raylib.h"
#include <cmath>
#include <memory>

namespace mmx {

void EndingScene::onEnter() {
    timer_ = 0;
    fadeInTimer_ = FADE_IN_DURATION;
    AudioManager::playBGM("ending");
}

void EndingScene::pollInput() {}

void EndingScene::handleInput() {
    if (timer_ < 120) return;

    if (Input::isConfirmPressed() || Input::isCancelPressed()) {
        Input::consumeConfirmPress();
        Input::consumeCancelPress();
        if (sceneManager_) {
            auto title = std::make_unique<TitleScene>();
            title->setSceneManager(sceneManager_);
            sceneManager_->changeScene(std::move(title));
        }
    }
}

void EndingScene::update(float /*dt*/) {
    timer_++;
    if (fadeInTimer_ > 0) fadeInTimer_--;
}

void EndingScene::render(float /*alpha*/) {
    ClearBackground({4, 4, 16, 255});

    // Stars
    for (int i = 0; i < 50; i++) {
        int sx = (i * 73 + 17) % INTERNAL_WIDTH;
        int sy = (i * 41 + 53) % INTERNAL_HEIGHT;
        int brightness = 60 + (i * 37) % 140;
        int twinkle = ((timer_ + i * 5) / 15) % 2;
        if (twinkle) {
            DrawPixel(sx, sy, {
                static_cast<unsigned char>(brightness),
                static_cast<unsigned char>(brightness),
                static_cast<unsigned char>(std::min(255, brightness + 50)),
                255
            });
        }
    }

    int cx = INTERNAL_WIDTH / 2;

    // Title line fades in over the first 2 seconds.
    if (timer_ > 30) {
        int alpha = std::min(255, (timer_ - 30) * 6);
        float bob = std::sin(timer_ * 0.02f) * 2.0f;
        const char* congrats = uiText(UiText::EndingCongratulations, Settings::language);
        int cw = MeasureText(congrats, 16);
        DrawText(congrats, cx - cw / 2 + 1, 35 + static_cast<int>(bob) + 1, 16, {20, 20, 60, static_cast<unsigned char>(alpha)});
        DrawText(congrats, cx - cw / 2, 35 + static_cast<int>(bob), 16, {255, 215, 0, static_cast<unsigned char>(alpha)});
    }

    // Story text — scrolls in over time
    struct TextLine { int delay; const char* text; int size; Color color; };
    // Narrative text is excluded from this source distribution.
    // A separately licensed content layer may supply these lines.
    const std::array<TextLine, 0> lines{};

    for (const auto& line : lines) {
        if (timer_ > line.delay) {
            int alpha = std::min(255, (timer_ - line.delay) * 8);
            Color c = line.color;
            c.a = static_cast<unsigned char>(alpha);
            int w = MeasureText(line.text, line.size);
            int y = 70 + (line.delay - 60) * 5 / 6;
            DrawText(line.text, cx - w / 2, y, line.size, c);
        }
    }

    // Credits
    if (timer_ > 300) {
        int alpha = std::min(255, (timer_ - 300) * 4);
        Color credColor = {120, 120, 160, static_cast<unsigned char>(alpha)};

        const char* credits[] = {
            "Engine by davidluky",
            "Based on Mega Man X (Capcom, 1993)",
            "Built with raylib + C++17",
            "Sprites sourced from community rips",
        };
        int y = 170;
        for (const auto* line : credits) {
            int w = MeasureText(line, 8);
            DrawText(line, cx - w / 2, y, 8, credColor);
            y += 12;
        }
    }

    // Return prompt
    if (timer_ > 120 && (timer_ / 30) % 2 == 0) {
        const char* prompt = uiText(UiText::PressAnyKey, Settings::language);
        int pw = MeasureText(prompt, 8);
        DrawText(prompt, cx - pw / 2, INTERNAL_HEIGHT - 16, 8, {150, 150, 180, 255});
    }

    // Fade from white (victory flash)
    if (fadeInTimer_ > 0) {
        int a = 255 * fadeInTimer_ / FADE_IN_DURATION;
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
                      {255, 255, 255, static_cast<unsigned char>(a)});
    }
}

} // namespace mmx
