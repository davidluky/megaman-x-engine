// boss_rush_scene.cpp - runs sequential boss fights in boss-rush mode.
// Owns: boss order progress, stage lookup, fight setup, and result rendering.

#include "ui/boss_rush_scene.h"
#include "ui/title_scene.h"
#include "app/input.h"
#include "data/localization.h"
#include "data/settings.h"
#include "data/x1_catalog.h"
#include "systems/audio.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <cstdio>

namespace mmx {

void BossRushScene::onEnter() {
    // Canonical weakness chain order
    fights_.clear();
    for (const auto& boss : x1_catalog::maverickBosses()) {
        fights_.push_back({boss.stage.str(), boss.boss.str(), boss.displayName});
    }

    currentFight_ = 0;
    totalFrames_ = 0;
    totalDamageTaken_ = 0;
    state_ = State::Title;
    titleTimer_ = 0;
    fadeInTimer_ = FADE_IN_DURATION;
}

std::string BossRushScene::stagePathFor(const std::string& stageId) {
    namespace fs = std::filesystem;
    std::string x1 = "content/x1/stages/ripped/" + stageId + "/stage_final.json";
    if (fs::exists(x1)) return x1;
    return "content/extras/stages/" + stageId + "/stage_final.json";
}

Vector2 BossRushScene::findBossSpawn(const std::string& stagePath) {
    std::ifstream f(stagePath);
    if (!f.is_open()) return {0, 0};
    nlohmann::json j;
    f >> j;
    for (const auto& sp : j.value("spawns", nlohmann::json::array())) {
        if (sp.value("type", "") == "boss") {
            return {sp.value("x", 0.0f) - 128.0f, sp.value("y", 0.0f)};
        }
    }
    return {0, 0};
}

void BossRushScene::startFight(int index) {
    const auto& fight = fights_[index];
    std::string path = stagePathFor(fight.stageId);
    Vector2 spawn = findBossSpawn(path);

    GameplaySceneConfig gameplayConfig;
    gameplayConfig.stagePath = path;
    gameplayConfig.stageId = StageId::fromString(fight.stageId);
    gameplayConfig.hasSpawnOverride = true;
    gameplayConfig.spawnOverride = spawn;
    gameplayConfig.bossRushMode = true;
    gameplay_ = std::make_unique<GameplayScene>(gameplayConfig);
    gameplay_->onEnter();

    // Weapons carry via WeaponInventory's static storage. Each boss death
    // adds its weapon, and the next GameplayScene sees it automatically.
    playerMaxHP_ = gameplay_->getPlayer().progressState().maxHealth;
    fightFrames_ = 0;
    state_ = State::Fighting;
}

void BossRushScene::pollInput() {
    if (state_ == State::Title) {
        if (Input::isConfirmPressed()) {
            Input::consumeConfirmPress();
            startFight(0);
        }
        if (Input::isCancelPressed()) {
            Input::consumeCancelPress();
            if (sceneManager_) {
                auto title = std::make_unique<TitleScene>();
                title->setSceneManager(sceneManager_);
                sceneManager_->changeScene(std::move(title));
            }
        }
    } else if (state_ == State::Fighting && gameplay_) {
        gameplay_->pollInput();
    } else if (state_ == State::Results) {
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
}

void BossRushScene::handleInput() {
    if (state_ == State::Fighting && gameplay_) {
        gameplay_->handleInput();
    }
}

void BossRushScene::update(float dt) {
    if (fadeInTimer_ > 0) fadeInTimer_--;

    if (state_ == State::Title) {
        titleTimer_++;
        return;
    }

    if (state_ == State::Fighting && gameplay_) {
        gameplay_->update(dt);
        totalFrames_++;
        fightFrames_++;

        if (gameplay_->isStageClear() && gameplay_->getStageClearTimer() > 45) {
            int dmg = playerMaxHP_ - gameplay_->getPlayer().health;
            totalDamageTaken_ += dmg;
            fights_[currentFight_].frames = fightFrames_;
            fights_[currentFight_].damageTaken = dmg;
            state_ = State::Transition;
            transitionTimer_ = 0;
        }

        if (gameplay_->isGameOver()) {
            state_ = State::Results;
            resultsTimer_ = 0;
        }
        return;
    }

    if (state_ == State::Transition) {
        transitionTimer_++;
        if (transitionTimer_ > 120) {
            currentFight_++;
            if (currentFight_ >= static_cast<int>(fights_.size())) {
                state_ = State::Results;
                resultsTimer_ = 0;
            } else {
                startFight(currentFight_);
            }
        }
        return;
    }

    if (state_ == State::Results) {
        resultsTimer_++;
    }
}

void BossRushScene::render(float alpha) {
    ClearBackground(BLACK);

    if (state_ == State::Title) {
        int cx = 128, cy = 60;
        const char* title = uiText(UiText::TitleBossRush, Settings::language);
        DrawText(title, cx - MeasureText(title, 20) / 2, cy, 20, WHITE);
        const char* subtitle = uiText(UiText::BossRushSubtitle, Settings::language);
        DrawText(subtitle, cx - MeasureText(subtitle, 10) / 2, cy + 30, 10, GRAY);

        if ((titleTimer_ / 30) % 2 == 0) {
            const char* prompt = uiText(UiText::TitlePressStart, Settings::language);
            DrawText(prompt, cx - MeasureText(prompt, 10) / 2, cy + 70, 10, WHITE);
        }

        // Show fight order
        for (int i = 0; i < static_cast<int>(fights_.size()); i++) {
            Color c = (i < 4) ? Color{100, 200, 240, 255} : Color{200, 100, 240, 255};
            DrawText(fights_[i].displayName.c_str(), 40, 140 + i * 12, 8, c);
        }
        return;
    }

    if (state_ == State::Fighting && gameplay_) {
        gameplay_->render(alpha);

        // Overlay: boss counter
        char buf[32];
        snprintf(buf, sizeof(buf), "%d / %d", currentFight_ + 1,
                 static_cast<int>(fights_.size()));
        DrawText(buf, 210, 4, 8, {200, 200, 200, 180});
        return;
    }

    if (state_ == State::Transition) {
        const auto& prev = fights_[currentFight_];

        int cy = 80;
        DrawText(prev.displayName.c_str(),
                 128 - MeasureText(prev.displayName.c_str(), 16) / 2, cy, 16, WHITE);
        const char* defeated = uiText(UiText::BossRushDefeated, Settings::language);
        DrawText(defeated,
                 128 - MeasureText(defeated, 12) / 2, cy + 24, 12, {100, 255, 100, 255});

        char buf[64];
        snprintf(buf, sizeof(buf), "%d / %d", currentFight_ + 1,
                 static_cast<int>(fights_.size()));
        DrawText(buf, 128 - MeasureText(buf, 10) / 2, cy + 50, 10, GRAY);

        if (currentFight_ + 1 < static_cast<int>(fights_.size())) {
            const auto& next = fights_[currentFight_ + 1];
            const char* nextLabel = uiText(UiText::BossRushNextPrefix, Settings::language);
            DrawText(nextLabel, 128 - MeasureText(nextLabel, 8) / 2, cy + 72, 8, GRAY);
            DrawText(next.displayName.c_str(),
                     128 - MeasureText(next.displayName.c_str(), 10) / 2, cy + 84, 10,
                     {255, 200, 100, 255});
        }
        return;
    }

    if (state_ == State::Results) {
        int cy = 14;
        bool victory = currentFight_ >= static_cast<int>(fights_.size());

        const char* header = victory
            ? uiText(UiText::BossRushAllDefeated, Settings::language)
            : uiText(UiText::GameOverTitle, Settings::language);
        Color hc = victory ? Color{100, 255, 100, 255} : Color{255, 80, 80, 255};
        DrawText(header, 128 - MeasureText(header, 14) / 2, cy, 14, hc);

        char buf[64];

        // Summary stats
        int seconds = totalFrames_ / 60;
        int minutes = seconds / 60;
        seconds %= 60;
        snprintf(buf, sizeof(buf), "%s %d:%02d   %s %d   %s %d/%d",
                 uiText(UiText::BossRushSummaryTimePrefix, Settings::language),
                 minutes, seconds,
                 uiText(UiText::BossRushSummaryDamagePrefix, Settings::language),
                 totalDamageTaken_,
                 uiText(UiText::BossRushSummaryBossesPrefix, Settings::language),
                 currentFight_,
                 static_cast<int>(fights_.size()));
        DrawText(buf, 128 - MeasureText(buf, 7) / 2, cy + 20, 7, LIGHTGRAY);

        // Rank
        if (victory) {
            const char* rank = "D";
            Color rankColor = {150, 150, 150, 255};
            if (totalDamageTaken_ == 0 && totalFrames_ < 60 * 120) {
                rank = "S"; rankColor = {255, 215, 0, 255};
            } else if (totalDamageTaken_ <= 8 && totalFrames_ < 60 * 180) {
                rank = "A"; rankColor = {100, 200, 255, 255};
            } else if (totalDamageTaken_ <= 24 && totalFrames_ < 60 * 300) {
                rank = "B"; rankColor = {100, 255, 100, 255};
            } else if (totalDamageTaken_ <= 48) {
                rank = "C"; rankColor = {255, 200, 60, 255};
            }
            snprintf(buf, sizeof(buf), "%s %s",
                     uiText(UiText::BossRushRankPrefix, Settings::language), rank);
            DrawText(buf, 128 - MeasureText(buf, 16) / 2, cy + 32, 16, rankColor);
        }

        // Per-boss breakdown table
        int tableY = cy + 54;
        DrawText(uiText(UiText::BossRushTableBoss, Settings::language), 10, tableY, 7, GRAY);
        DrawText(uiText(UiText::BossRushTableTime, Settings::language), 150, tableY, 7, GRAY);
        DrawText(uiText(UiText::BossRushTableDamage, Settings::language), 200, tableY, 7, GRAY);
        DrawLine(10, tableY + 9, 240, tableY + 9, {60, 60, 80, 255});

        for (int i = 0; i < currentFight_ && i < static_cast<int>(fights_.size()); i++) {
            int rowY = tableY + 12 + i * 11;
            const auto& f = fights_[i];
            DrawText(f.displayName.c_str(), 10, rowY, 7, WHITE);

            int fs = f.frames / 60;
            int fm = fs / 60;
            fs %= 60;
            snprintf(buf, sizeof(buf), "%d:%02d", fm, fs);
            DrawText(buf, 150, rowY, 7, LIGHTGRAY);

            snprintf(buf, sizeof(buf), "%d", f.damageTaken);
            Color dmgColor = f.damageTaken == 0
                ? Color{100, 255, 100, 255}
                : Color{255, 200, 200, 255};
            DrawText(buf, 205, rowY, 7, dmgColor);
        }

        if (resultsTimer_ > 60 && (resultsTimer_ / 30) % 2 == 0) {
            const char* prompt = uiText(UiText::PressAnyKey, Settings::language);
            DrawText(prompt, 128 - MeasureText(prompt, 8) / 2, INTERNAL_HEIGHT - 16, 8, GRAY);
        }
    }

    if (fadeInTimer_ > 0) {
        int a = 255 * fadeInTimer_ / FADE_IN_DURATION;
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
                      {0, 0, 0, static_cast<unsigned char>(a)});
    }
}

} // namespace mmx
