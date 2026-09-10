// bloody_palace_scene.cpp - runs the Bloody Palace wave challenge scene.
// Owns: wave generation, arena setup, enemy progress, and scene rendering.

#include "ui/bloody_palace_scene.h"
#include "ui/title_scene.h"
#include "app/input.h"
#include "data/localization.h"
#include "data/settings.h"
#include "systems/audio.h"
#include "app/constants.h"
#include <memory>
#include <cstdio>
#include <string>
#include <vector>
#include <filesystem>
#include <optional>
#include <utility>

namespace mmx {

// ============================================================================
// Bloody Palace - DMC-style wave combat arena
//
// 30 waves of escalating enemy encounters in a flat arena stage.
// Every 5th wave introduces miniboss-tier enemies, every 10th is a boss wave.
// Player is ranked on waves cleared, damage taken, and time.
// ============================================================================

void BloodyPalaceScene::onEnter() {
    currentWave_ = 0;
    totalFrames_ = 0;
    totalDamageTaken_ = 0;
    comboCount_ = 0;
    comboTimer_ = 0;
    maxCombo_ = 0;
    styleRank_ = "D";
    state_ = State::Title;
    titleTimer_ = 0;
    fadeInTimer_ = FADE_IN_DURATION;
}

std::string BloodyPalaceScene::stagePathForArena() {
    // The enemy-sandbox is a flat 40x14 testbed arena.
    // Try x1 path first (future-proof), fall back to extras.
    namespace fs = std::filesystem;
    std::string x1 = "content/x1/stages/ripped/enemy-sandbox/stage_final.json";
    if (fs::exists(x1)) return x1;
    return "content/extras/stages/enemy-sandbox/stage_final.json";
}

BloodyPalaceScene::WaveConfig BloodyPalaceScene::generateWave(int waveNum) {
    WaveConfig cfg;
    cfg.waveNum = waveNum;
    cfg.isBossWave = false;

    // Arena floor spans roughly tiles 2..38 horizontally, tile 11 vertically.
    // Enemy Y is placed on the floor; X is spread across the arena width.
    const float floorY = 176.0f;  // tile row 11 * 16

    // Boss waves: every 10th wave
    if (waveNum == 10) {
        cfg.isBossWave = true;
        cfg.bossId = "chill-penguin";
        return cfg;
    }
    if (waveNum == 20) {
        cfg.isBossWave = true;
        cfg.bossId = "storm-eagle";
        return cfg;
    }
    if (waveNum == 30) {
        cfg.isBossWave = true;
        cfg.bossId = "spark-mandrill";
        return cfg;
    }

    // Wave 1-5: introductory - mets and patrols
    if (waveNum >= 1 && waveNum <= 5) {
        int count = 1 + waveNum;  // 2..6, capped at 4 for early waves
        if (count > 4) count = 4;
        for (int i = 0; i < count; i++) {
            float x = 100.0f + i * 80.0f;
            const char* type = (i % 2 == 0) ? "met" : "patrol";
            cfg.enemies.push_back({type, x, floorY});
        }
        return cfg;
    }

    // Wave 6-9: add battons, increase count to 5-6
    if (waveNum >= 6 && waveNum <= 9) {
        int count = 4 + (waveNum - 5);  // 5..8, capped at 6
        if (count > 6) count = 6;
        const char* types[] = {"met", "patrol", "batton", "patrol", "batton", "met"};
        for (int i = 0; i < count; i++) {
            float x = 80.0f + i * 70.0f;
            cfg.enemies.push_back({types[i % 6], x, floorY});
        }
        return cfg;
    }

    // Wave 11-19: mix all enemy types, 6-8 per wave
    if (waveNum >= 11 && waveNum <= 19) {
        int count = 5 + (waveNum - 10);  // 6..14, capped at 8
        if (count > 8) count = 8;
        const char* types[] = {"met", "batton", "patrol", "axemax", "snowball",
                               "met", "batton", "patrol"};
        for (int i = 0; i < count; i++) {
            float x = 64.0f + i * 60.0f;
            cfg.enemies.push_back({types[i % 8], x, floorY});
        }
        return cfg;
    }

    // Wave 21-29: heavy waves, 8-10 enemies, more axemax
    if (waveNum >= 21 && waveNum <= 29) {
        int count = 7 + (waveNum - 20);  // 8..16, capped at 10
        if (count > 10) count = 10;
        const char* types[] = {"axemax", "batton", "snowball", "axemax", "patrol",
                               "met", "axemax", "snowball", "batton", "axemax"};
        for (int i = 0; i < count; i++) {
            float x = 48.0f + i * 52.0f;
            cfg.enemies.push_back({types[i % 10], x, floorY});
        }
        return cfg;
    }

    // Beyond wave 30: congratulations screen (no enemies generated)
    return cfg;
}

void BloodyPalaceScene::startWave(int waveNum) {
    WaveConfig wave = generateWave(waveNum);
    std::vector<SpawnPoint> enemies;
    enemies.reserve(wave.enemies.size());
    for (const auto& enemy : wave.enemies) {
        enemies.push_back({"enemy", enemy.enemyId, enemy.x, enemy.y});
    }

    std::optional<SpawnPoint> boss;
    if (wave.isBossWave && !wave.bossId.empty()) {
        boss = SpawnPoint{"boss", wave.bossId, 480.0f, 160.0f};
    }

    GameplaySceneConfig gameplayConfig;
    gameplayConfig.stagePath = stagePathForArena();
    gameplayConfig.stageId = StageId::fromString("enemy-sandbox");
    gameplayConfig.bossRushMode = true;  // Suppresses restart input
    gameplayConfig.suppressProgressionRewards = true;
    gameplayConfig.hasSpawnOverride = true;
    gameplayConfig.spawnOverride = {80.0f, 160.0f};
    gameplay_ = std::make_unique<GameplayScene>(gameplayConfig);
    gameplay_->setArenaWave(std::move(enemies), std::move(boss));

    gameplay_->onEnter();
    playerMaxHP_ = gameplay_->getPlayer().progressState().maxHealth;
    waveFrames_ = 0;
    state_ = State::Fighting;
}

void BloodyPalaceScene::pollInput() {
    if (state_ == State::Title) {
        if (Input::isConfirmPressed()) {
            Input::consumeConfirmPress();
            currentWave_ = 1;
            startWave(currentWave_);
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

void BloodyPalaceScene::handleInput() {
    if (state_ == State::Fighting && gameplay_) {
        gameplay_->handleInput();
    }
}

void BloodyPalaceScene::update(float dt) {
    if (fadeInTimer_ > 0) fadeInTimer_--;

    if (state_ == State::Title) {
        titleTimer_++;
        return;
    }

    if (state_ == State::Fighting && gameplay_) {
        gameplay_->update(dt);
        totalFrames_++;
        waveFrames_++;

        // Combo decay: reset combo after 180 frames (~3 seconds) of no kills
        if (comboTimer_ > 0) {
            comboTimer_--;
            if (comboTimer_ == 0) {
                comboCount_ = 0;
            }
        }

        // Update style rank based on current combo
        if (comboCount_ >= 50)      styleRank_ = "SSS";
        else if (comboCount_ >= 30) styleRank_ = "SS";
        else if (comboCount_ >= 20) styleRank_ = "S";
        else if (comboCount_ >= 12) styleRank_ = "A";
        else if (comboCount_ >= 6)  styleRank_ = "B";
        else if (comboCount_ >= 3)  styleRank_ = "C";
        else                        styleRank_ = "D";

        // Track max combo
        if (comboCount_ > maxCombo_) maxCombo_ = comboCount_;

        // Wave cleared - record damage taken and advance
        if (gameplay_->isStageClear() && gameplay_->getStageClearTimer() > 45) {
            int dmg = playerMaxHP_ - gameplay_->getPlayer().health;
            if (dmg < 0) dmg = 0;
            totalDamageTaken_ += dmg;
            state_ = State::WaveTransition;
            transitionTimer_ = 0;
        }

        // Game over - go to results
        if (gameplay_->isGameOver()) {
            state_ = State::Results;
            resultsTimer_ = 0;
        }
        return;
    }

    if (state_ == State::WaveTransition) {
        transitionTimer_++;
        if (transitionTimer_ > 90) {
            currentWave_++;
            if (currentWave_ > TOTAL_WAVES) {
                // All 30 waves complete
                state_ = State::Results;
                resultsTimer_ = 0;
            } else {
                startWave(currentWave_);
            }
        }
        return;
    }

    if (state_ == State::Results) {
        resultsTimer_++;
    }
}

void BloodyPalaceScene::render(float alpha) {
    ClearBackground(BLACK);

    // --- Title screen ---
    if (state_ == State::Title) {
        int cx = INTERNAL_WIDTH / 2;
        int cy = 50;

        // Title
        const char* title = uiText(UiText::TitleBloodyPalace, Settings::language);
        DrawText(title, cx - MeasureText(title, 20) / 2, cy, 20,
                 {200, 30, 30, 255});

        // Subtitle
        const char* subtitle = uiText(UiText::BloodyPalaceSubtitle, Settings::language);
        DrawText(subtitle, cx - MeasureText(subtitle, 10) / 2, cy + 28, 10, GRAY);

        // Blinking prompt
        if ((titleTimer_ / 30) % 2 == 0) {
            const char* prompt = uiText(UiText::TitlePressStart, Settings::language);
            DrawText(prompt, cx - MeasureText(prompt, 10) / 2, cy + 60, 10, WHITE);
        }

        // Wave rules
        const char* rules[] = {
            uiText(UiText::BloodyPalaceRuleWaves, Settings::language),
            uiText(UiText::BloodyPalaceRuleBoss, Settings::language),
            uiText(UiText::BloodyPalaceRuleContinues, Settings::language),
            uiText(UiText::BloodyPalaceRuleRank, Settings::language),
        };
        for (int i = 0; i < 4; i++) {
            DrawText(rules[i], cx - MeasureText(rules[i], 8) / 2,
                     130 + i * 14, 8, {160, 160, 180, 255});
        }

        // Fade-in overlay
        if (fadeInTimer_ > 0) {
            int a = 255 * fadeInTimer_ / FADE_IN_DURATION;
            DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
                          {0, 0, 0, static_cast<unsigned char>(a)});
        }
        return;
    }

    // --- Fighting ---
    if (state_ == State::Fighting && gameplay_) {
        gameplay_->render(alpha);

        // Overlay: wave counter in top-right
        char buf[32];
        snprintf(buf, sizeof(buf), "%s %d",
                 uiText(UiText::BloodyPalaceWavePrefix, Settings::language),
                 currentWave_);
        int tw = MeasureText(buf, 8);
        DrawText(buf, INTERNAL_WIDTH - tw - 4, 4, 8, {200, 200, 200, 180});

        // Combo counter (only show when combo > 1)
        if (comboCount_ > 1) {
            // Style rank color
            Color rankColor = {150, 150, 150, 255};  // D default
            if (styleRank_ == "SSS")     rankColor = {255, 215, 0, 255};    // gold
            else if (styleRank_ == "SS") rankColor = {200, 100, 255, 255};  // purple
            else if (styleRank_ == "S")  rankColor = {100, 200, 255, 255};  // cyan
            else if (styleRank_ == "A")  rankColor = {100, 255, 100, 255};  // green
            else if (styleRank_ == "B")  rankColor = {255, 200, 60, 255};   // yellow
            else if (styleRank_ == "C")  rankColor = {200, 200, 200, 255};  // silver

            snprintf(buf, sizeof(buf), "%d %s", comboCount_,
                     uiText(UiText::BloodyPalaceHitSuffix, Settings::language));
            DrawText(buf, INTERNAL_WIDTH - MeasureText(buf, 10) - 4, 16, 10, rankColor);

            DrawText(styleRank_.c_str(),
                     INTERNAL_WIDTH - MeasureText(styleRank_.c_str(), 14) - 4,
                     28, 14, rankColor);
        }
        return;
    }

    // --- Wave Transition ---
    if (state_ == State::WaveTransition) {
        int cy = 70;
        int cx = INTERNAL_WIDTH / 2;

        char buf[64];
        snprintf(buf, sizeof(buf), "%s %d %s",
                 uiText(UiText::BloodyPalaceWavePrefix, Settings::language),
                 currentWave_,
                 uiText(UiText::BloodyPalaceClearSuffix, Settings::language));
        DrawText(buf, cx - MeasureText(buf, 16) / 2, cy, 16,
                 {100, 255, 100, 255});

        // Show damage taken this wave
        int waveDmg = playerMaxHP_ - (gameplay_ ? gameplay_->getPlayer().health : 0);
        if (waveDmg < 0) waveDmg = 0;
        snprintf(buf, sizeof(buf), "%s %d",
                 uiText(UiText::BloodyPalaceDamagePrefix, Settings::language),
                 waveDmg);
        DrawText(buf, cx - MeasureText(buf, 10) / 2, cy + 28, 10, LIGHTGRAY);

        // Next wave preview
        if (currentWave_ < TOTAL_WAVES) {
            snprintf(buf, sizeof(buf), "%s %s %d",
                     uiText(UiText::BloodyPalaceNextPrefix, Settings::language),
                     uiText(UiText::BloodyPalaceWavePrefix, Settings::language),
                     currentWave_ + 1);
            DrawText(buf, cx - MeasureText(buf, 10) / 2, cy + 52, 10,
                     {255, 200, 100, 255});

            WaveConfig next = generateWave(currentWave_ + 1);
            if (next.isBossWave) {
                // Capitalize boss name for display
                std::string bossName = next.bossId;
                for (char& ch : bossName) {
                    if (ch >= 'a' && ch <= 'z') ch -= 32;
                    if (ch == '-') ch = ' ';
                }
                snprintf(buf, sizeof(buf), "%s %s",
                         uiText(UiText::BloodyPalaceBossPrefix, Settings::language),
                         bossName.c_str());
                DrawText(buf, cx - MeasureText(buf, 8) / 2, cy + 68, 8,
                         {255, 80, 80, 255});
            } else {
                snprintf(buf, sizeof(buf), "%d %s",
                         static_cast<int>(next.enemies.size()),
                         uiText(UiText::BloodyPalaceEnemiesSuffix, Settings::language));
                DrawText(buf, cx - MeasureText(buf, 8) / 2, cy + 68, 8, GRAY);
            }
        } else {
            const char* congrats = uiText(UiText::BloodyPalaceCongratulations,
                                          Settings::language);
            DrawText(congrats, cx - MeasureText(congrats, 14) / 2, cy + 52, 14,
                     {255, 215, 0, 255});
        }
        return;
    }

    // --- Results ---
    if (state_ == State::Results) {
        int cy = 20;
        int cx = INTERNAL_WIDTH / 2;
        bool victory = currentWave_ > TOTAL_WAVES;

        // Header
        const char* header = victory
            ? uiText(UiText::BloodyPalaceAllCleared, Settings::language)
            : uiText(UiText::GameOverTitle, Settings::language);
        Color hc = victory ? Color{100, 255, 100, 255} : Color{255, 80, 80, 255};
        DrawText(header, cx - MeasureText(header, 16) / 2, cy, 16, hc);

        char buf[96];

        // Waves cleared
        int wavesCleared = victory ? TOTAL_WAVES : (currentWave_ - 1);
        if (wavesCleared < 0) wavesCleared = 0;
        snprintf(buf, sizeof(buf), "%s %d / %d",
                 uiText(UiText::BloodyPalaceWavesClearedPrefix, Settings::language),
                 wavesCleared, TOTAL_WAVES);
        DrawText(buf, cx - MeasureText(buf, 8) / 2, cy + 24, 8, LIGHTGRAY);

        // Time
        int seconds = totalFrames_ / 60;
        int minutes = seconds / 60;
        seconds %= 60;
        snprintf(buf, sizeof(buf), "%s %d:%02d",
                 uiText(UiText::BloodyPalaceTimePrefix, Settings::language),
                 minutes, seconds);
        DrawText(buf, cx - MeasureText(buf, 8) / 2, cy + 38, 8, LIGHTGRAY);

        // Damage
        snprintf(buf, sizeof(buf), "%s %d",
                 uiText(UiText::BloodyPalaceTotalDamagePrefix, Settings::language),
                 totalDamageTaken_);
        DrawText(buf, cx - MeasureText(buf, 8) / 2, cy + 52, 8, LIGHTGRAY);

        // Max combo
        snprintf(buf, sizeof(buf), "%s %d",
                 uiText(UiText::BloodyPalaceMaxComboPrefix, Settings::language),
                 maxCombo_);
        DrawText(buf, cx - MeasureText(buf, 8) / 2, cy + 66, 8, LIGHTGRAY);

        // Final rank - based on waves cleared + damage taken
        const char* finalRank = "D";
        Color rankColor = {150, 150, 150, 255};  // gray

        if (victory && totalDamageTaken_ < 50) {
            // S rank: cleared all 30, took less than 50 damage
            finalRank = "S";
            rankColor = {100, 200, 255, 255};  // cyan
        } else if (wavesCleared >= 20 && totalDamageTaken_ < 100) {
            // A rank: 20+ waves, under 100 damage
            finalRank = "A";
            rankColor = {100, 255, 100, 255};  // green
        } else if (wavesCleared >= 10) {
            // B rank: cleared at least 10 waves
            finalRank = "B";
            rankColor = {255, 200, 60, 255};  // yellow
        } else if (wavesCleared >= 5) {
            // C rank: cleared at least 5 waves
            finalRank = "C";
            rankColor = {200, 200, 200, 255};  // silver
        }
        // D rank: default (fewer than 5 waves)

        snprintf(buf, sizeof(buf), "%s %s",
                 uiText(UiText::BloodyPalaceRankPrefix, Settings::language), finalRank);
        DrawText(buf, cx - MeasureText(buf, 20) / 2, cy + 86, 20, rankColor);

        // Wave breakdown summary
        int tableY = cy + 116;
        const char* breakdown = uiText(UiText::BloodyPalaceWaveBreakdown,
                                       Settings::language);
        DrawText(breakdown, cx - MeasureText(breakdown, 8) / 2, tableY, 8, GRAY);
        DrawLine(20, tableY + 10, INTERNAL_WIDTH - 20, tableY + 10,
                 {60, 60, 80, 255});

        // Show milestone waves reached
        const UiText milestones[] = {
            UiText::BloodyPalaceMilestoneBasic,
            UiText::BloodyPalaceMilestoneChillPenguin,
            UiText::BloodyPalaceMilestoneMixed,
            UiText::BloodyPalaceMilestoneStormEagle,
            UiText::BloodyPalaceMilestoneHeavy,
            UiText::BloodyPalaceMilestoneSparkMandrill,
        };
        for (int i = 0; i < 6; i++) {
            int rowY = tableY + 14 + i * 11;
            // Highlight reached milestones in white, unreached in dark gray
            bool reached = false;
            if (i == 0) reached = wavesCleared >= 1;
            else if (i == 1) reached = wavesCleared >= 10;
            else if (i == 2) reached = wavesCleared >= 11;
            else if (i == 3) reached = wavesCleared >= 20;
            else if (i == 4) reached = wavesCleared >= 21;
            else if (i == 5) reached = wavesCleared >= 30;

            Color mc = reached ? WHITE : Color{80, 80, 80, 255};
            DrawText(uiText(milestones[i], Settings::language), 24, rowY, 7, mc);
        }

        // Blinking prompt
        if (resultsTimer_ > 60 && (resultsTimer_ / 30) % 2 == 0) {
            const char* prompt = uiText(UiText::PressAnyKey, Settings::language);
            DrawText(prompt, cx - MeasureText(prompt, 8) / 2,
                     INTERNAL_HEIGHT - 16, 8, GRAY);
        }
    }

    // Fade-in overlay (applies to all states except Title which handles its own)
    if (fadeInTimer_ > 0) {
        int a = 255 * fadeInTimer_ / FADE_IN_DURATION;
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
                      {0, 0, 0, static_cast<unsigned char>(a)});
    }
}

} // namespace mmx
