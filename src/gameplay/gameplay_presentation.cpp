// gameplay_presentation.cpp - renders gameplay HUD, overlays, and transition chrome.
// Boundary: presentation only; simulation and rewards stay in gameplay lanes.

#include "gameplay/gameplay_presentation.h"
#include "entities/projectile.h"
#include "app/input.h"
#include "data/localization.h"
#include "data/settings.h"
#include "gameplay/stage_start_timeline.h"
#include "gameplay/weapon_get_timeline.h"
#include "ui/title_menu_font.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>

namespace mmx::gameplay_presentation {
void renderStageStartReady(int stageStartTick, const TextureResource* atlas) {
    const int cell = stage_start_timeline::readyRasterCellAt(stageStartTick);
    if (cell < 0 || !atlas || !atlas->valid()) return;
    // R297: exact source39x13 raster, including the first two glyph phases.
    DrawTextureRec(atlas->get(), {0.0f, cell * 13.0f, 39.0f, 13.0f},
                   {108.0f, 107.0f}, WHITE);
}

void drawCenteredText(const char* text, int y, int fontSize, Color color) {
    const int textW = MeasureText(text, fontSize);
    DrawText(text, centeredXForWidth(textW), y, fontSize, color);
}

int fitFontSizeForLine(const std::string& line, int preferredFontSize) {
    constexpr int kMinReadableFontSize = 5;
    constexpr int kMargin = 4;
    const int maxWidth = INTERNAL_WIDTH - kMargin * 2;
    for (int size = preferredFontSize; size >= kMinReadableFontSize; --size) {
        if (MeasureText(line.c_str(), size) <= maxWidth) {
            return size;
        }
    }
    return kMinReadableFontSize;
}

void drawCenteredFitText(const std::string& text, int y, int fontSize, Color color) {
    constexpr int kMargin = 4;
    const int fittedFontSize = fitFontSizeForLine(text, fontSize);
    const int textW = MeasureText(text.c_str(), fittedFontSize);
    DrawText(text.c_str(), std::max(kMargin, centeredXForWidth(textW)), y,
             fittedFontSize, color);
}

std::array<std::string, 2> pauseControlsSummaryLines() {
    const auto& bindings = Input::bindings();
    const char* move = uiText(UiText::PurposeMove, Settings::language);
    const char* jump = uiText(UiText::PurposeJump, Settings::language);
    const char* dash = uiText(UiText::PurposeDash, Settings::language);
    const char* shoot = uiText(UiText::PurposeShoot, Settings::language);
    const char* pause = uiText(UiText::PurposePause, Settings::language);

    if (Input::isGamepadConnected()) {
        return {
            std::string("DPAD:") + move + "  " +
                gamepadActionHint(bindings, InputAction::Jump, jump) + "  " +
                gamepadActionHint(bindings, InputAction::Dash, dash),
            gamepadActionHint(bindings, InputAction::Shoot, shoot) + "  " +
                gamepadActionHint(bindings, InputAction::Pause, pause),
        };
    }

    return {
        keyboardDirectionalHint(bindings, move) + "  " +
            keyboardActionHint(bindings, InputAction::Jump, jump) + "  " +
            keyboardActionHint(bindings, InputAction::Dash, dash),
        keyboardActionHint(bindings, InputAction::Shoot, shoot) + "  " +
            keyboardActionHint(bindings, InputAction::Pause, pause),
    };
}

void drawPauseControlsSummary(const PauseOverlayLayout& layout) {
    const auto lines = pauseControlsSummaryLines();
    const Color color = {130, 150, 190, 255};
    drawCenteredFitText(lines[0], layout.controlsSummaryY,
                        layout.controlsSummaryFontSize, color);
    drawCenteredFitText(lines[1], layout.controlsSummaryY + layout.controlsSummaryLineGap,
                        layout.controlsSummaryFontSize, color);
}

void renderGameOverOverlay(bool gameOver,
                           int gameOverTimer,
                           const std::array<int, 12>& gridDigits,
                           const std::array<int, 12>& helmetTicks,
                           const TextureResource* sourceSheet) {
    if (!gameOver) return;

    const auto layout = gameOverOverlayLayout();
    DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT, BLACK);
    if (!gameplay_game_over::passwordVisible(gameOver, gameOverTimer)) return;

    const bool hasSourceSheet = sourceSheet && sourceSheet->valid();
    if (hasSourceSheet) {
        DrawTexturePro(
            sourceSheet->get(),
            Rectangle{5.0f, 0.0f, 256.0f, 224.0f},
            Rectangle{0.0f, 0.0f, 256.0f, 224.0f},
            Vector2{0.0f, 0.0f},
            0.0f,
            WHITE
        );

        constexpr Rectangle digitSources[] = {
            Rectangle{276.0f, 6.0f, 8.0f, 17.0f},
            Rectangle{273.0f, 30.0f, 13.0f, 17.0f},
            Rectangle{273.0f, 52.0f, 13.0f, 17.0f},
            Rectangle{272.0f, 75.0f, 14.0f, 17.0f},
            Rectangle{273.0f, 99.0f, 13.0f, 17.0f},
            Rectangle{273.0f, 122.0f, 13.0f, 17.0f},
            Rectangle{273.0f, 145.0f, 13.0f, 17.0f},
            Rectangle{272.0f, 168.0f, 13.0f, 17.0f},
        };
        constexpr Rectangle helmetSources[] = {
            Rectangle{7.0f, 241.0f, 21.0f, 21.0f},
            Rectangle{31.0f, 241.0f, 21.0f, 21.0f},
            Rectangle{57.0f, 241.0f, 21.0f, 21.0f},
            Rectangle{81.0f, 241.0f, 21.0f, 21.0f},
            Rectangle{107.0f, 241.0f, 22.0f, 21.0f},
            Rectangle{135.0f, 241.0f, 21.0f, 21.0f},
            Rectangle{163.0f, 241.0f, 21.0f, 21.0f},
            Rectangle{190.0f, 241.0f, 21.0f, 21.0f},
        };
        constexpr int cellX = 33;
        constexpr int cellY = 22;
        constexpr int cellW = 42;
        constexpr int stepX = 49;
        constexpr int stepY = 50;
        constexpr int helmetX[] = {49, 97, 145, 194};
        constexpr int helmetY[] = {39, 87, 135};

        for (int index = 0; index < 12; ++index) {
            const int row = index / 4;
            const int column = index % 4;
            const int x = cellX + column * stepX;
            const int y = cellY + row * stepY;
            const int digit = std::clamp(
                gridDigits[static_cast<std::size_t>(index)], 1, 8);
            const Rectangle digitSource = digitSources[digit - 1];
            DrawTexturePro(
                sourceSheet->get(),
                digitSource,
                Rectangle{
                    static_cast<float>(
                        x + (cellW - static_cast<int>(digitSource.width)) / 2),
                    static_cast<float>(y + 2),
                    digitSource.width,
                    digitSource.height,
                },
                Vector2{0.0f, 0.0f},
                0.0f,
                WHITE
            );
            const int helmetFrame = gameplay_game_over::decorativeHelmetFrame(
                helmetTicks[static_cast<std::size_t>(index)]);
            const Rectangle helmetSource = helmetSources[helmetFrame];
            DrawTexturePro(
                sourceSheet->get(),
                helmetSource,
                Rectangle{
                    static_cast<float>(helmetX[column]),
                    static_cast<float>(helmetY[row]),
                    helmetSource.width,
                    helmetSource.height,
                },
                Vector2{0.0f, 0.0f},
                0.0f,
                WHITE
            );
        }
        return;
    }

    DrawRectangle(layout.panelX - 4, layout.panelY - 4,
                  layout.panelWidth + 8, layout.panelHeight + 8,
                  Color{18, 24, 34, 255});
    DrawRectangle(layout.panelX, layout.panelY,
                  layout.panelWidth, layout.panelHeight,
                  Color{70, 84, 92, 255});
    DrawRectangle(layout.panelX + 5, layout.panelY + 5,
                  layout.panelWidth - 10, layout.panelHeight - 10,
                  Color{10, 64, 58, 255});

    constexpr Color digitColors[] = {
        Color{244, 84, 72, 255},
        Color{154, 102, 224, 255},
        Color{230, 194, 72, 255},
        Color{100, 190, 218, 255},
        Color{238, 112, 76, 255},
        Color{198, 132, 218, 255},
        Color{224, 82, 72, 255},
        Color{242, 196, 80, 255},
    };
    for (int index = 0; index < 12; ++index) {
        const int row = index / 4;
        const int column = index % 4;
        const int x = layout.gridX + column * (layout.cellWidth + layout.cellGapX);
        const int y = layout.gridY + row * (layout.cellHeight + layout.cellGapY);
        DrawRectangle(x, y, layout.cellWidth, layout.cellHeight,
                      Color{24, 34, 42, 255});
        DrawRectangle(x + 3, y + 3, layout.cellWidth - 6, layout.cellHeight - 6,
                      Color{48, 72, 76, 255});
        const int digit = std::clamp(gridDigits[static_cast<std::size_t>(index)], 1, 8);
        const std::string text = std::to_string(digit);
        const int width = MeasureText(text.c_str(), layout.digitFontSize);
        DrawText(text.c_str(),
                 x + (layout.cellWidth - width) / 2,
                 y + (layout.cellHeight - layout.digitFontSize) / 2,
                 layout.digitFontSize,
                 digitColors[digit - 1]);
    }
}

void renderPauseOverlay(bool paused,
                               const std::string& stageName,
                               int stageTimer,
                               const Player& player,
                               int pauseWeaponCursor) {
    if (!paused) return;

    const auto layout = pauseOverlayLayout();
    DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
                  withAlpha(layout.fade.color, layout.fade.maxAlpha));

    const int stageSeconds = stageTimer / 60;
    char timerBuf[32];
    snprintf(timerBuf, sizeof(timerBuf), "%d:%02d", stageSeconds / 60, stageSeconds % 60);
    DrawText(stageName.c_str(), layout.headerStageX, layout.headerY,
             layout.headerFontSize, {150, 180, 220, 255});
    const int tw = MeasureText(timerBuf, layout.headerFontSize);
    DrawText(timerBuf, INTERNAL_WIDTH - layout.headerStageX - tw, layout.headerY,
             layout.headerFontSize, {180, 200, 150, 255});

    drawCenteredText(uiText(UiText::PauseWeaponSelectTitle, Settings::language),
                     layout.titleY, layout.titleFontSize,
                     {200, 200, 230, 255});

    const int weaponCount = static_cast<int>(player.weaponInventory.weaponCount());
    const int listX = layout.weaponListX;
    const int itemH = layout.weaponItemHeight;

    for (int i = 0; i < weaponCount; i++) {
        const Weapon& w = player.weaponInventory.weaponAt(static_cast<size_t>(i));
        const int ammo = player.weaponInventory.ammoAt(static_cast<size_t>(i));
        const int y = pauseWeaponRowY(layout, i);
        const bool selected = (i == pauseWeaponCursor);
        const bool equipped = (i == player.weaponInventory.currentIndex);

        if (selected) {
            DrawRectangle(listX - layout.selectionInsetX,
                          y - layout.selectionInsetY,
                          layout.selectionWidth,
                          itemH,
                          {40, 60, 100, 180});
        }

        if (equipped) {
            DrawText("*", listX - layout.equippedMarkerOffsetX, y + 2, 8,
                     {255, 255, 100, 255});
        }

        DrawRectangle(listX, y + layout.swatchOffsetY,
                      layout.swatchSize, layout.swatchSize, w.shotColor);
        DrawRectangleLines(listX, y + layout.swatchOffsetY,
                           layout.swatchSize, layout.swatchSize, {180, 180, 200, 200});

        const Color nameColor = selected ? WHITE : Color{160, 160, 180, 255};
        DrawText(w.name.c_str(), listX + layout.weaponNameOffsetX,
                 y + layout.weaponNameOffsetY, layout.weaponNameFontSize, nameColor);

        if (i > 0) {
            const int barX = listX + layout.ammoBarOffsetX;
            const int barY = y + layout.ammoBarOffsetY;

            DrawRectangle(barX, barY, layout.ammoBarWidth, layout.ammoBarHeight,
                          {30, 30, 50, 255});

            const float fill = (w.maxAmmo > 0) ? static_cast<float>(ammo) / w.maxAmmo : 0.0f;
            const int fillW = static_cast<int>(fill * layout.ammoBarWidth);
            const Color barColor = (fill > 0.3f) ? Color{80, 200, 80, 255}
                                 : (fill > 0.0f) ? Color{200, 200, 50, 255}
                                 : Color{200, 50, 50, 255};
            DrawRectangle(barX, barY, fillW, layout.ammoBarHeight, barColor);

            DrawRectangleLines(barX, barY, layout.ammoBarWidth, layout.ammoBarHeight,
                selected ? Color{120, 200, 255, 200} : Color{60, 60, 80, 200});
        } else {
            DrawText(uiText(UiText::PauseInfiniteAmmo, Settings::language),
                     listX + layout.infiniteAmmoOffsetX, y + 2, 7,
                     {120, 180, 120, 255});
        }
    }

    const int subY = pauseSubTankY(layout, weaponCount);
    const auto& progress = player.progressState();
    DrawText(uiText(UiText::PauseSubTanks, Settings::language), listX, subY, 8,
             {180, 180, 200, 255});
    for (int i = 0; i < Player::MAX_SUB_TANKS; i++) {
        const int sx = listX + layout.subTankOffsetX + i * layout.subTankSpacing;
        if (progress.subTanks[i].collected) {
            DrawRectangle(sx, subY, layout.subTankWidth, layout.subTankHeight,
                          {30, 30, 50, 255});
            const float fill =
                static_cast<float>(progress.subTanks[i].health) / Player::SUB_TANK_CAPACITY;
            const int fillH = static_cast<int>(fill * layout.subTankHeight);
            DrawRectangle(sx, subY + layout.subTankHeight - fillH,
                          layout.subTankWidth, fillH, {80, 200, 255, 255});
            DrawRectangleLines(sx, subY, layout.subTankWidth, layout.subTankHeight,
                               {100, 180, 255, 200});
        } else {
            DrawRectangle(sx, subY, layout.subTankWidth, layout.subTankHeight,
                          {20, 20, 30, 255});
            DrawRectangleLines(sx, subY, layout.subTankWidth, layout.subTankHeight,
                               {40, 40, 60, 200});
        }
    }

    const int armorY = subY + layout.armorGapY;
    DrawText(uiText(UiText::PauseArmor, Settings::language), listX, armorY, 8,
             {180, 180, 200, 255});
    const char* armorNames[] = {
        uiText(UiText::PauseArmorBoots, Settings::language),
        uiText(UiText::PauseArmorHelm, Settings::language),
        uiText(UiText::PauseArmorBody, Settings::language),
        uiText(UiText::PauseArmorArm, Settings::language),
    };
    const bool armorFlags[] = {
        player.armorBootsUnlocked(),
        player.armorHelmetUnlocked(),
        player.armorBodyUnlocked(),
        player.armorBusterUnlocked(),
    };
    for (int i = 0; i < 4; i++) {
        const int ax = listX + layout.armorNameOffsetX + i * layout.armorNameSpacing;
        const Color ac = armorFlags[i] ? Color{100, 255, 100, 255} : Color{60, 60, 70, 255};
        DrawText(armorNames[i], ax, armorY, 7, ac);
    }

    char livesBuf[16];
    snprintf(livesBuf, sizeof(livesBuf), "%s %d",
             uiText(UiText::PauseLivesPrefix, Settings::language), player.lives);
    DrawText(livesBuf, listX, armorY + layout.livesOffsetY, 8, {180, 200, 180, 255});

    drawPauseControlsSummary(layout);

    std::string hint = Input::isGamepadConnected()
        ? std::string("DPAD:") + uiText(UiText::PurposeSelect, Settings::language) +
          "  A:" + uiText(UiText::PurposeEquip, Settings::language) +
          "  START:" + uiText(UiText::PurposeResume, Settings::language)
        : keyboardVerticalHint(Input::bindings(), uiText(UiText::PurposeSelect,
                                                         Settings::language)) + "  " +
          keyboardActionHint(Input::bindings(), InputAction::Confirm,
                             uiText(UiText::PurposeEquip, Settings::language)) + "  " +
          keyboardActionHint(Input::bindings(), InputAction::Pause,
                             uiText(UiText::PurposeResume, Settings::language));
    const int hintW = MeasureText(hint.c_str(), layout.hintFontSize);
    DrawText(hint.c_str(), centeredXForWidth(hintW), layout.hintY,
             layout.hintFontSize, {100, 100, 130, 255});
}

void renderPauseOverlay(const gameplay_pause_menu::State& state,
                               const std::string& stageName,
                               int stageTimer,
                               const Player& player) {
    renderPauseOverlay(state.paused, stageName, stageTimer, player, state.weaponCursor);
}

void renderStageClearBanner(int stageClearTimer,
                                   int stageTimer,
                                   bool bossActive,
                                   int bestTime) {
    const auto layout = stageClearOverlayLayout();
    DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
                  withAlpha(layout.fade.color, overlayFadeAlpha(stageClearTimer, layout.fade)));

    if (stageClearTimer > layout.titleDelayFrames) {
        const int textAge = stageClearTimer - layout.titleDelayFrames;
        const int textAlpha = std::min(255, textAge * 12);

        const char* clearText = uiText(UiText::StageClearTitle, Settings::language);
        const int textW = MeasureText(clearText, layout.titleFontSize);
        const int titleX = centeredXForWidth(textW);
        const int titleY = layout.titleBaseY - stageClearTitleBobY(layout, textAge);

        DrawText(clearText,
                 titleX + layout.titleShadowOffset,
                 titleY + layout.titleShadowOffset,
                 layout.titleFontSize,
                 {0, 0, 0, static_cast<unsigned char>(textAlpha / 2)});
        DrawText(clearText, titleX, titleY, layout.titleFontSize,
                 {100, 255, 100, static_cast<unsigned char>(textAlpha)});

        const int stageSeconds = stageTimer / 60;
        const int stageFrames = stageTimer % 60;
        char timeBuf[32];
        snprintf(timeBuf, sizeof(timeBuf), "%s %d:%02d.%02d",
                 uiText(UiText::StageClearTimePrefix, Settings::language),
                 stageSeconds / 60, stageSeconds % 60, stageFrames * 100 / 60);
        const int tmW = MeasureText(timeBuf, layout.timeFontSize);
        DrawText(timeBuf, centeredXForWidth(tmW), titleY + layout.timeOffsetY,
                 layout.timeFontSize, {180, 220, 180, static_cast<unsigned char>(textAlpha)});

        if (bossActive && bestTime > 0 && bestTime != stageTimer) {
            const int bestSec = bestTime / 60;
            const int bestFr = bestTime % 60;
            char bestBuf[48];
            snprintf(bestBuf, sizeof(bestBuf), "%s %d:%02d.%02d",
                     uiText(UiText::StageClearBestPrefix, Settings::language),
                     bestSec / 60, bestSec % 60, bestFr * 100 / 60);
            const int bw = MeasureText(bestBuf, layout.bestFontSize);
            const Color bestColor = (stageTimer <= bestTime)
                ? Color{255, 220, 80, static_cast<unsigned char>(textAlpha)}
                : Color{150, 150, 170, static_cast<unsigned char>(textAlpha)};
            DrawText(bestBuf, centeredXForWidth(bw), titleY + layout.bestOffsetY,
                     layout.bestFontSize, bestColor);
            if (stageTimer <= bestTime) {
                const char* newRecord = uiText(UiText::StageClearNewRecord,
                                               Settings::language);
                const int nrw = MeasureText(newRecord, layout.bestFontSize);
                DrawText(newRecord, centeredXForWidth(nrw), titleY + layout.recordOffsetY,
                         layout.bestFontSize,
                         {255, 200, 50, static_cast<unsigned char>(textAlpha)});
            }
        }
    }
}

void renderStageClearOverlay(bool stageClear,
                                    int stageClearTimer,
                                    int stageTimer,
                                    bool bossActive,
                                    int bestTime,
                                    bool bossRescued,
                                    const std::optional<Weapon>& awardedWeapon) {
    if (!stageClear) return;

    const auto layout = stageClearOverlayLayout();
    renderStageClearBanner(stageClearTimer, stageTimer, bossActive, bestTime);

    if (stageClearTimer > layout.rewardDelayFrames) {
        const int age = stageClearTimer - layout.rewardDelayFrames;
        const int slideX = slideInOffsetX(age, layout.rewardSlideFrames);
        if (bossRescued) {
            const char* zeroText = "ZERO ARRIVED JUST IN TIME!";
            const int zw = MeasureText(zeroText, layout.zeroFontSize);
            DrawText(zeroText, centeredXForWidth(zw) + slideX, layout.rewardY,
                     layout.zeroFontSize, {200, 200, 255, 255});

        } else if (awardedWeapon.has_value()) {
            if (age >= 15 && age < 51) {
                const int phase = age - 15;
                const int inCycle = phase % 12;
                if (inCycle < 6) {
                    const int sa = 110 - inCycle * 18;
                    Color fc = awardedWeapon->bodyColorAlt;
                    DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
                                  {fc.r, fc.g, fc.b,
                                   static_cast<unsigned char>(std::max(0, sa))});
                }
            }

            const std::string reward = weaponGetText(*awardedWeapon);
            const char* weaponText = reward.c_str();
            const int ww = MeasureText(weaponText, layout.weaponFontSize);

            Color wc = awardedWeapon->shotColor;
            if (age >= 15) {
                const float pulse = std::sin((age - 15) * 0.15f) * 0.3f + 0.7f;
                wc.r = static_cast<unsigned char>(std::min(255.0f, wc.r * pulse + 60 * (1.0f - pulse)));
                wc.g = static_cast<unsigned char>(std::min(255.0f, wc.g * pulse + 60 * (1.0f - pulse)));
                wc.b = static_cast<unsigned char>(std::min(255.0f, wc.b * pulse + 60 * (1.0f - pulse)));
            }

            const int textX = centeredXForWidth(ww) + slideX;
            const int textY = layout.rewardY;

            DrawText(weaponText, textX + 1, textY + 1, layout.weaponFontSize, {0, 0, 0, 160});
            DrawText(weaponText, textX, textY, layout.weaponFontSize, wc);

            if (age >= 15) {
                const int swatchY = textY + layout.weaponSwatchOffsetY;
                const int swatchCX = INTERNAL_WIDTH / 2;
                const float swatchPulse = std::sin(age * 0.2f) * 2.0f + 6.0f;
                const int sz = static_cast<int>(swatchPulse);
                DrawRectangle(swatchCX - sz, swatchY, sz * 2, sz * 2,
                              awardedWeapon->shotColor);
                if ((age / 8) % 2 == 0) {
                    DrawRectangle(swatchCX - 1, swatchY + sz - 1, 2, 2, WHITE);
                }
            }
        }
    }

    if (stageClearTimer > layout.promptDelayFrames) {
        std::string restartText = Input::isGamepadConnected()
            ? std::string("A:") + uiText(UiText::TitleStageSelect, Settings::language) +
              "    B:" + uiText(UiText::PurposeTitle, Settings::language)
            : keyboardActionHint(Input::bindings(), InputAction::Confirm,
                                 uiText(UiText::TitleStageSelect, Settings::language)) + "    " +
              keyboardActionHint(Input::bindings(), InputAction::Cancel,
                                 uiText(UiText::PurposeTitle, Settings::language));
        const int rw = MeasureText(restartText.c_str(), layout.promptFontSize);
        if ((stageClearTimer / 30) % 2 == 0 || stageClearTimer < 100) {
            DrawText(restartText.c_str(), centeredXForWidth(rw), layout.promptY,
                     layout.promptFontSize, LIGHTGRAY);
        }
    }
}

void renderStageClearOverlay(const gameplay_stage_clear::State& state,
                                    int stageTimer,
                                    bool bossActive,
                                    int bestTime,
                                    bool bossRescued) {
    renderStageClearOverlay(
        state.active,
        state.timer,
        stageTimer,
        bossActive,
        bestTime,
        bossRescued,
        state.awardedWeapon);
}

std::string weaponGetText(const Weapon& weapon) {
    return weapon_get_presentation::sourceText(weapon.name);
}

const char* weaponGetSourceScreenPath(
    const weapon_get_presentation::Screen screen,
    const Weapon& weapon) {
    using weapon_get_presentation::Screen;
    if (weapon.id == "shotgun-ice") {
        if (screen == Screen::SpecText) {
            return "content/x1/sprites/weapon_get/shotgun_ice_spec.png";
        }
        if (screen == Screen::Demo) {
            return "content/x1/sprites/weapon_get/shotgun_ice_demo.png";
        }
    }
    if (weapon.id == "storm-tornado") {
        if (screen == Screen::SpecText) {
            return "content/x1/sprites/weapon_get/storm_tornado_spec.png";
        }
        if (screen == Screen::Demo) {
            return "content/x1/sprites/weapon_get/storm_tornado_demo.png";
        }
    }
    return nullptr;
}

bool hasWeaponGetSourceScreen(
    const weapon_get_presentation::Screen screen,
    const Weapon& weapon) {
    return loadPointSourceTexture(weaponGetSourceScreenPath(screen, weapon)) != nullptr;
}

namespace {

constexpr const char* kShotgunDenseAtlasPath =
    "content/x1/sprites/weapon_get/shotgun_ice_dense_atlas.png";
constexpr int kShotgunDenseAtlasStartTick =
    weapon_get_timeline::kSpecScreenTick;
constexpr int kShotgunDenseAtlasFrameCount = 959;
constexpr int kShotgunDenseAtlasColumns = 16;
// Rebuilt from the Storm Eagle movie's source framebuffer: victory f19908,
// atlas starts at f20080 and ends at f21054, sampled every movie frame.
// Tick 172 is the first atlas frame, so the source-backed window stays tied
// to the measured victory clock without changing the shared phase law.
constexpr const char* kStormDenseAtlasPath =
    "content/x1/sprites/weapon_get/storm_tornado_dense_atlas.png";
constexpr int kStormDenseAtlasStartTick =
    weapon_get_timeline::kSpecScreenTick;
constexpr int kStormDenseAtlasFrameCount = 975;
constexpr int kStormDenseAtlasColumns = 16;

const char* weaponGetSourceScreenPathAtTick(
    const int tick,
    const weapon_get_presentation::Screen screen,
    const Weapon& weapon,
    Rectangle* sourceRect = nullptr) {
    if (sourceRect) *sourceRect = Rectangle{0.0f, 0.0f, 0.0f, 0.0f};

    // The Chill Penguin recording contains a complete no-skip source window
    // from the first spec-screen frame through the final password-grid hold.
    // One atlas cell is one movie frame, so short projectile/flicker states
    // cannot disappear between samples. The final source frame is held for
    // the last engine tick because the recording ends at f18950.
    if (weapon.id == "shotgun-ice" &&
        tick >= kShotgunDenseAtlasStartTick &&
        tick >= weapon_get_timeline::kReturnTick &&
        tick <= weapon_get_timeline::kSourceHandoffEndTick) {
        const int frame = std::min(
            tick - kShotgunDenseAtlasStartTick,
            kShotgunDenseAtlasFrameCount - 1);
        constexpr float kFrameWidth = 256.0f;
        constexpr float kFrameHeight = 224.0f;
        const int column = frame % kShotgunDenseAtlasColumns;
        const int row = frame / kShotgunDenseAtlasColumns;
        if (sourceRect) {
            *sourceRect = Rectangle{column * kFrameWidth,
                                    row * kFrameHeight,
                                    kFrameWidth,
                                    kFrameHeight};
        }
        return kShotgunDenseAtlasPath;
    }

    // Storm Eagle's second recording has the same source-backed visual
    // window, but its 19-glyph text tail pushes the warp-in and password tail
    // later in movie time. Keep the engine's generic choreography untouched;
    // this atlas maps the recorded Storm framebuffer to the same relative
    // victory clock so every visible state follows the movie.
    if (weapon.id == "storm-tornado" &&
        tick >= kStormDenseAtlasStartTick &&
        tick <= weapon_get_timeline::kStormSourceHandoffEndTick) {
        const int frame = tick - kStormDenseAtlasStartTick;
        if (frame >= 0 && frame < kStormDenseAtlasFrameCount) {
            constexpr float kFrameWidth = 256.0f;
            constexpr float kFrameHeight = 224.0f;
            const int column = frame % kStormDenseAtlasColumns;
            const int row = frame / kStormDenseAtlasColumns;
            if (sourceRect) {
                *sourceRect = Rectangle{column * kFrameWidth,
                                        row * kFrameHeight,
                                        kFrameWidth,
                                        kFrameHeight};
            }
            return kStormDenseAtlasPath;
        }
    }

    // The source atlas covers the spec and demo portion with the same
    // one-movie-frame cadence. This branch is limited to the pre-handoff
    // screens because the handoff is handled above.
    if ((screen == weapon_get_presentation::Screen::SpecText ||
         screen == weapon_get_presentation::Screen::Demo) &&
        weapon.id == "shotgun-ice" &&
        tick >= kShotgunDenseAtlasStartTick &&
        tick < weapon_get_timeline::kReturnTick &&
        tick - kShotgunDenseAtlasStartTick < kShotgunDenseAtlasFrameCount) {
        const int frame = tick - kShotgunDenseAtlasStartTick;
        if (frame >= 0 && frame < kShotgunDenseAtlasFrameCount) {
            constexpr float kFrameWidth = 256.0f;
            constexpr float kFrameHeight = 224.0f;
            const int column = frame % kShotgunDenseAtlasColumns;
            const int row = frame / kShotgunDenseAtlasColumns;
            if (sourceRect) {
                *sourceRect = Rectangle{column * kFrameWidth,
                                        row * kFrameHeight,
                                        kFrameWidth,
                                        kFrameHeight};
            }
            return kShotgunDenseAtlasPath;
        }
    }

    // The promoted full source screen is the final typed state. Build-time
    // reveal variants keep the source panel, X, diagram, palette, and glyph
    // raster intact while exposing only the characters the measured clock has
    // reached. The space in "YOU GET" is a timed character but has no pixels.
    if (screen == weapon_get_presentation::Screen::SpecText) {
        constexpr const char* kShotgunReveal[] = {
            "content/x1/sprites/weapon_get/shotgun_ice_spec_reveal_00.png",
            "content/x1/sprites/weapon_get/shotgun_ice_spec_reveal_01.png",
            "content/x1/sprites/weapon_get/shotgun_ice_spec_reveal_02.png",
            "content/x1/sprites/weapon_get/shotgun_ice_spec_reveal_03.png",
            "content/x1/sprites/weapon_get/shotgun_ice_spec_reveal_04.png",
            "content/x1/sprites/weapon_get/shotgun_ice_spec_reveal_05.png",
            "content/x1/sprites/weapon_get/shotgun_ice_spec_reveal_06.png",
            "content/x1/sprites/weapon_get/shotgun_ice_spec_reveal_07.png",
            "content/x1/sprites/weapon_get/shotgun_ice_spec_reveal_08.png",
            "content/x1/sprites/weapon_get/shotgun_ice_spec_reveal_09.png",
            "content/x1/sprites/weapon_get/shotgun_ice_spec_reveal_10.png",
            "content/x1/sprites/weapon_get/shotgun_ice_spec_reveal_11.png",
            "content/x1/sprites/weapon_get/shotgun_ice_spec_reveal_12.png",
            "content/x1/sprites/weapon_get/shotgun_ice_spec_reveal_13.png",
            "content/x1/sprites/weapon_get/shotgun_ice_spec_reveal_14.png",
            "content/x1/sprites/weapon_get/shotgun_ice_spec_reveal_15.png",
            "content/x1/sprites/weapon_get/shotgun_ice_spec_reveal_16.png",
        };
        constexpr const char* kStormReveal[] = {
            "content/x1/sprites/weapon_get/storm_tornado_spec_reveal_00.png",
            "content/x1/sprites/weapon_get/storm_tornado_spec_reveal_01.png",
            "content/x1/sprites/weapon_get/storm_tornado_spec_reveal_02.png",
            "content/x1/sprites/weapon_get/storm_tornado_spec_reveal_03.png",
            "content/x1/sprites/weapon_get/storm_tornado_spec_reveal_04.png",
            "content/x1/sprites/weapon_get/storm_tornado_spec_reveal_05.png",
            "content/x1/sprites/weapon_get/storm_tornado_spec_reveal_06.png",
            "content/x1/sprites/weapon_get/storm_tornado_spec_reveal_07.png",
            "content/x1/sprites/weapon_get/storm_tornado_spec_reveal_08.png",
            "content/x1/sprites/weapon_get/storm_tornado_spec_reveal_09.png",
            "content/x1/sprites/weapon_get/storm_tornado_spec_reveal_10.png",
            "content/x1/sprites/weapon_get/storm_tornado_spec_reveal_11.png",
            "content/x1/sprites/weapon_get/storm_tornado_spec_reveal_12.png",
            "content/x1/sprites/weapon_get/storm_tornado_spec_reveal_13.png",
            "content/x1/sprites/weapon_get/storm_tornado_spec_reveal_14.png",
            "content/x1/sprites/weapon_get/storm_tornado_spec_reveal_15.png",
            "content/x1/sprites/weapon_get/storm_tornado_spec_reveal_16.png",
            "content/x1/sprites/weapon_get/storm_tornado_spec_reveal_17.png",
            "content/x1/sprites/weapon_get/storm_tornado_spec_reveal_18.png",
        };
        const bool shotgun = weapon.id == "shotgun-ice";
        const bool storm = weapon.id == "storm-tornado";
        if (shotgun || storm) {
            const int maxChars = shotgun ? 17 : 19;
            const int visible = tick < weapon_get_timeline::kTextStartTick
                ? 0
                : std::min(
                      maxChars,
                      (tick - weapon_get_timeline::kTextStartTick) /
                              weapon_get_timeline::kTextStepTicks +
                          1);
            if (visible < maxChars) {
                return shotgun ? kShotgunReveal[visible]
                               : kStormReveal[visible];
            }
        }
    }
    // The source packet also contains the deterministic first projectile
    // state at f18451 (the first demo shot is 40 ticks after f18411). Keep
    // this exact sample narrow; the remaining flight path still needs its
    // own source frames before it can claim continuous pixel parity.
    if (screen == weapon_get_presentation::Screen::Demo
        && weapon.id == "shotgun-ice"
        && tick == weapon_get_timeline::kDemoFirstTick + 40) {
        return "content/x1/sprites/weapon_get/shotgun_ice_demo_projectile.png";
    }
    // Storm Eagle's source capture records the first visible Storm Tornado
    // projectile on the exact warp-in-end frame. The normal demo asset is the
    // settled pose, so keep this one-frame source state explicit rather than
    // drawing the wrong X pose over the projectile.
    if (screen == weapon_get_presentation::Screen::Demo
        && weapon.id == "storm-tornado"
        && tick == weapon_get_timeline::kWarpInEndTick) {
        return "content/x1/sprites/weapon_get/storm_tornado_demo_projectile.png";
    }
    return weaponGetSourceScreenPath(screen, weapon);
}

constexpr const char* kWeaponGetOriginalFontPath =
    "content/x1/sprites/misc/mmx_font.png";

// The two recorded awards use complete source framebuffers. These constants
// are the measured geometry for the native fallback used by the other six
// awards: two SNES-style panels, the narrow center gutter, and the right-side
// technical area. Keeping this composition shared means a new weapon only
// needs a projectile profile and a diagram colour, not another screen-only
// renderer.
constexpr Color kWeaponGetFrame = {33, 66, 99, 255};
constexpr Color kWeaponGetLeftInterior = {0, 0, 41, 255};
constexpr Color kWeaponGetRightInterior = {0, 49, 16, 255};
constexpr Color kWeaponGetInk = {82, 173, 148, 255};

Color blendWeaponGetColor(Color base, int whiteAlpha) {
    const int alpha = std::clamp(whiteAlpha, 0, 255);
    const auto blend = [alpha](unsigned char value) {
        return static_cast<unsigned char>(
            (static_cast<int>(value) * (255 - alpha) + 255 * alpha) / 255);
    };
    return {blend(base.r), blend(base.g), blend(base.b), 255};
}

bool renderWeaponGetNativePanels(int tick) {
    if (!weapon_get_presentation::nativeFallbackPanelVisibleAtTick(tick)) {
        if (tick >= weapon_get_presentation::kNativeFallbackWhiteStartTick &&
            tick < weapon_get_presentation::kNativeFallbackWhiteEndTick) {
            DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT, WHITE);
        }
        return false;
    }

    int whiteAlpha = 0;
    if (tick < weapon_get_presentation::kNativeFallbackWhiteStartTick) {
        whiteAlpha = (tick - weapon_get_presentation::kNativeFallbackFadeStartTick) * 255 /
                     (weapon_get_presentation::kNativeFallbackWhiteStartTick -
                      weapon_get_presentation::kNativeFallbackFadeStartTick);
    } else if (tick < weapon_get_presentation::kNativeFallbackStableTick) {
        whiteAlpha = (weapon_get_presentation::kNativeFallbackStableTick - tick) * 255 /
                     (weapon_get_presentation::kNativeFallbackStableTick -
                      weapon_get_presentation::kNativeFallbackWhiteEndTick);
    }

    DrawRectangle(3, 3, 138, 218,
                  blendWeaponGetColor(kWeaponGetFrame, whiteAlpha));
    DrawRectangle(8, 8, 130, 208,
                  blendWeaponGetColor(kWeaponGetLeftInterior, whiteAlpha));
    DrawRectangle(147, 3, 106, 218,
                  blendWeaponGetColor(kWeaponGetFrame, whiteAlpha));
    DrawRectangle(152, 8, 96, 208,
                  blendWeaponGetColor(kWeaponGetRightInterior, whiteAlpha));

    // The source has a hard technical-panel divider rather than a generic
    // rounded card. Preserve the same square, stepped silhouette in fallback
    // mode so the panel is useful even before a new source capture exists.
    DrawRectangle(142, 3, 5, 218, blendWeaponGetColor(BLACK, whiteAlpha));
    DrawRectangle(151, 79, 98, 3,
                  blendWeaponGetColor(kWeaponGetFrame, whiteAlpha));
    DrawRectangleLines(3, 3, 138, 218,
                       blendWeaponGetColor(BLACK, whiteAlpha));
    DrawRectangleLines(147, 3, 106, 218,
                       blendWeaponGetColor(BLACK, whiteAlpha));
    return true;
}

int weaponGetNativeLineX(int lineIndex) {
    // Source placement is intentionally not centered as a modern UI label:
    // "YOU GET" and the first name word begin at x=160, while the final
    // short word is set on the lower-right side of the panel.
    return lineIndex == 2 ? 218 : 160;
}

int weaponGetNativeLineY(int lineIndex) {
    if (lineIndex == 0) return 23;
    if (lineIndex == 1) return 39;
    return 47 + (lineIndex - 2) * 8;
}

void renderWeaponGetSourceGlyphText(const std::string& typed) {
    const TextureResource* font = loadPointSourceTexture(
        kWeaponGetOriginalFontPath);
    std::size_t lineStart = 0;
    int lineIndex = 0;
    while (lineStart <= typed.size()) {
        const std::size_t lineEnd = typed.find('\n', lineStart);
        const std::string line = typed.substr(
            lineStart,
            lineEnd == std::string::npos ? std::string::npos
                                          : lineEnd - lineStart);
        int lineX = weaponGetNativeLineX(lineIndex);
        if (lineIndex == 2) {
            int lineWidth = 0;
            for (const char c : line) {
                lineWidth += titleMenuGlyphForChar(c).advance;
            }
            // Preserve the measured ICE anchor at x=218, but keep longer
            // second words inside the 152..248 right-panel interior.
            lineX = std::max(152, std::min(lineX, 248 - lineWidth));
        }
        int penX = lineX;
        const int penY = weaponGetNativeLineY(lineIndex);

        if (font) {
            for (const char c : line) {
                const TitleMenuGlyph glyph = titleMenuGlyphForChar(c);
                if (glyph.drawable) {
                    DrawTexturePro(
                        font->get(),
                        Rectangle{static_cast<float>(glyph.x),
                                  static_cast<float>(glyph.y),
                                  static_cast<float>(glyph.width),
                                  static_cast<float>(glyph.height)},
                        Rectangle{static_cast<float>(penX),
                                  static_cast<float>(penY),
                                  static_cast<float>(glyph.width),
                                  static_cast<float>(glyph.height)},
                        {0, 0}, 0.0f, WHITE);
                }
                penX += glyph.advance;
            }
        } else {
            // Keep the presentation functional if a stripped build omits the
            // source font. This is a last-resort diagnostic path, never the
            // normal artwork path.
            DrawText(line.c_str(), lineX, penY, 8, WHITE);
        }

        if (lineEnd == std::string::npos) break;
        lineStart = lineEnd + 1;
        ++lineIndex;
    }
}

void renderWeaponGetNativeDiagram(const Weapon& weapon) {
    // Parameterized technical drawing: the silhouette stays stable while the
    // live projectile profile supplies the weapon-specific sample in the
    // same way it does during gameplay and in the recorded overlays.
    DrawRectangleLines(166, 91, 69, 92, kWeaponGetInk);
    DrawLine(177, 101, 224, 101, kWeaponGetInk);
    DrawLine(177, 105, 214, 105, kWeaponGetInk);
    DrawCircleLines(200, 139, 25.0f, kWeaponGetInk);
    DrawCircleLines(200, 139, 13.0f, kWeaponGetInk);
    DrawLine(200, 114, 200, 164, kWeaponGetInk);
    DrawLine(175, 139, 225, 139, kWeaponGetInk);
    DrawLine(185, 120, 215, 158, kWeaponGetInk);
    DrawLine(215, 120, 185, 158, kWeaponGetInk);
    DrawCircle(200, 139, 4.0f, weapon.shotColor);
    renderProjectileVisualAt(weapon.id, 0, {222.0f, 139.0f}, true);
}

void renderWeaponGetDemoShots(int tick,
                              const weapon_get_presentation::Params& params,
                              const WeaponGetScreenLayout& layout,
                              const Weapon& weapon) {
    const int shots = params.timing.demoShotCount;
    const int step = params.timing.demoShotStepTicks;
    if (shots <= 0 || step <= 0) return;

    for (int i = 0; i < shots; ++i) {
        const int firedAt = weapon_get_timeline::kDemoFirstTick + i * step;
        const int age = tick - firedAt;
        if (age < 0) continue;
        const int x = layout.demoActorX + layout.demoShotOffsetX + age * layout.demoShotSpeed;
        if (x > INTERNAL_WIDTH) continue;
        const float centerX = x + layout.demoShotWidth * 0.5f;
        const float centerY = layout.demoActorY + layout.demoShotOffsetY +
                              layout.demoShotHeight * 0.5f;
        if (weapon_get_presentation::hasGameplayProjectileVisual(weapon.id)) {
            // Unrecorded rewards use the same projectile visual profile as
            // gameplay. Recorded CP/Storm screens return through the source
            // atlas path above and add their measured shared states below.
            renderProjectileVisualAt(weapon.id, age, {centerX, centerY}, true);
        } else {
            DrawRectangle(x, layout.demoActorY + layout.demoShotOffsetY,
                          layout.demoShotWidth, layout.demoShotHeight,
                          weapon.shotColor);
        }
    }
}

void renderShotgunIceWeaponGetSharedShot(int tick) {
    // CP source f17992 maps to engine tick 172. These measured visible pellet
    // windows map to the same static 16x16 sprite used by gameplay. The
    // source atlas remains underneath as the pixel oracle; this shared draw
    // is deliberately a visual no-op when the source-exact asset is used.
    struct Sample {
        int firstTick;
        int lastTick;
        float centerX;
    };
    constexpr std::array<Sample, 7> kSamples = {{
        {598, 600, 121.0f}, // source f18418-f18420, x=113
        {601, 602, 145.0f}, // source f18421-f18422, x=137
        {606, 608, 185.0f}, // source f18426-f18428, x=177
        {677, 679, 113.0f}, // source f18497-f18499, x=105
        {680, 681, 137.0f}, // source f18500-f18501, x=129
        {682, 683, 153.0f}, // source f18502-f18503, x=145
        {684, 686, 169.0f}, // source f18504-f18506, x=161
    }};
    for (const Sample sample : kSamples) {
        if (tick < sample.firstTick || tick > sample.lastTick) continue;
        renderProjectileVisualAt("shotgun-ice", 0,
                                 {sample.centerX, 125.0f}, true);
        return;
    }
}

void renderStormTornadoWeaponGetSharedShot(int tick) {
    // Storm source f20080 maps to engine tick 172. These are the measured
    // visible source cells in the two demo shots. Cell/position pairs were
    // matched directly against the same 36-cell gust strip used by
    // Projectile::render during gameplay. The source atlas already contains
    // those pixels; drawing the shared projectile on top is intentionally a
    // no-op visually, but makes these weapon-get states use the live
    // projectile renderer instead of a screen-only rectangle.
    struct Sample {
        int firstTick;
        int lastTick;
        int gustCell;
        float centerX;
    };
    constexpr std::array<Sample, 22> kSamples = {{
        {607, 609, 3, 80.0f},   // source f20515-f20517
        {613, 615, 6, 80.0f},   // source f20521-f20523
        {619, 621, 9, 80.0f},   // source f20527-f20529
        {625, 627, 12, 80.0f},  // source f20533-f20535
        {631, 633, 15, 80.0f},  // source f20539-f20541
        {637, 639, 18, 80.0f},  // source f20545-f20547
        {643, 645, 21, 80.0f},  // source f20551-f20553
        {653, 655, 26, 80.0f},  // source f20561-f20563
        {659, 661, 29, 80.0f},  // source f20567-f20569
        {665, 667, 31, 80.0f},  // source f20573-f20575
        {671, 673, 30, 120.0f}, // source f20579-f20581
        {707, 709, 3, 80.0f},    // source f20615-f20617
        {713, 715, 6, 80.0f},    // source f20621-f20623
        {719, 721, 9, 80.0f},    // source f20627-f20629
        {725, 727, 12, 80.0f},   // source f20633-f20635
        {731, 733, 15, 80.0f},   // source f20639-f20641
        {737, 739, 18, 80.0f},   // source f20645-f20647
        {743, 745, 21, 80.0f},   // source f20651-f20653
        {749, 751, 24, 80.0f},   // source f20657-f20659
        {755, 757, 27, 80.0f},   // source f20663-f20665
        {761, 763, 30, 80.0f},   // source f20669-f20671
        {773, 775, 31, 120.0f},  // source f20681-f20683
    }};
    for (const Sample sample : kSamples) {
        if (tick < sample.firstTick || tick > sample.lastTick) continue;
        // The live profile advances one gust cell every two projectile-age
        // frames. Preserve the source-selected cell while using that same
        // gameplay cadence and draw offsets.
        renderProjectileVisualAt("storm-tornado", sample.gustCell * 2,
                                 {sample.centerX, 124.0f}, true);
        return;
    }
}

}  // namespace

bool weaponGetNativeFallbackPanelVisible(int tick) {
    return weapon_get_presentation::nativeFallbackPanelVisibleAtTick(tick);
}

void renderWeaponGetScreen(int tick,
                                  const weapon_get_presentation::Params& params,
                                  const weapon_get_presentation::Plan& plan,
                                  const std::string& text,
                                  const Weapon& weapon) {
    using weapon_get_presentation::Screen;
    if (plan.screen != Screen::SpecText && plan.screen != Screen::Demo &&
        plan.screen != Screen::Handoff) {
        return;
    }

    const auto layout = weaponGetScreenLayout();

    // GC2.1d-T2: these are complete 256x224 source framebuffers captured from
    // the original ROM with frame skipping disabled. Use them as the primary
    // presentation for the two recorded weapon awards so the visible screen
    // is the original panel/X/technical-diagram composition, not a drawn
    // approximation. Unrecorded weapons intentionally retain the procedural
    // fallback below rather than borrowing another weapon's art silently.
    Rectangle sourceRect{0.0f, 0.0f, 0.0f, 0.0f};
    if (const char* sourcePath = weaponGetSourceScreenPathAtTick(
            tick, plan.screen, weapon, &sourceRect)) {
        if (const TextureResource* source = loadPointSourceTexture(sourcePath)) {
            if (sourceRect.width <= 0.0f || sourceRect.height <= 0.0f) {
                sourceRect = Rectangle{
                    0.0f,
                    0.0f,
                    static_cast<float>(source->width()),
                    static_cast<float>(source->height()),
                };
            }
            DrawTexturePro(source->get(),
                           sourceRect,
                           Rectangle{0.0f, 0.0f,
                                     static_cast<float>(INTERNAL_WIDTH),
                                     static_cast<float>(INTERNAL_HEIGHT)},
                           {0.0f, 0.0f}, 0.0f, WHITE);
            if (plan.screen == Screen::Demo) {
                if (weapon.id == "shotgun-ice") {
                    renderShotgunIceWeaponGetSharedShot(tick);
                } else if (weapon.id == "storm-tornado") {
                    renderStormTornadoWeaponGetSharedShot(tick);
                }
            }
            return;
        }
    }

    DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT, layout.background);

    // Unrecorded weapon tails remain a black handoff until the scene changes;
    // Chill Penguin's recorded password-grid tail is handled by the source
    // atlas above.
    if (plan.screen == Screen::Handoff) return;

    const bool nativePanelsVisible = renderWeaponGetNativePanels(tick);
    if (nativePanelsVisible) renderWeaponGetNativeDiagram(weapon);

    if (plan.screen == Screen::SpecText) {
        if (plan.revealedChars <= 0) return;

        const std::string typed =
            weapon_get_presentation::revealText(text, plan.revealedChars);
        renderWeaponGetSourceGlyphText(typed);

        return;
    }

    renderWeaponGetDemoShots(tick, params, layout, weapon);

}

} // namespace mmx::gameplay_presentation
