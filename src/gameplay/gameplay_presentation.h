// gameplay_presentation.h - declares gameplay presentation helpers and overlay data.
// Boundary: drawing contracts only; no gameplay rule ownership.

#pragma once

#include "app/constants.h"
#include "gameplay/gameplay_cp_capsule.h"
#include "gameplay/gameplay_death_orbs.h"
#include "gameplay/gameplay_escape_confirm.h"
#include "gameplay/gameplay_fade_in.h"
#include "gameplay/gameplay_game_over.h"
#include "gameplay/gameplay_pause_menu.h"
#include "gameplay/gameplay_sprite_test.h"
#include "gameplay/gameplay_stage_clear.h"
#include "gameplay/gameplay_warp_in.h"
#include "gameplay/weapon_get_presentation.h"
#include "entities/player.h"
#include "app/input.h"
#include "data/localization.h"
#include "data/settings.h"
#include "systems/asset_cache.h"

#include "raylib.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>

namespace mmx::gameplay_presentation {

inline const TextureResource* loadPointSourceTexture(const char* path) {
    if (!path || *path == '\0') return nullptr;
    const TextureResource* texture = AssetCache::loadTexture(path);
    if (texture && texture->valid()) {
        texture->setFilter(TEXTURE_FILTER_POINT);
        return texture;
    }
    return nullptr;
}

inline void drawSourcePlaneTexture(const char* path, float x, float y) {
    const TextureResource* texture = loadPointSourceTexture(path);
    if (!texture) return;
    DrawTexture(texture->get(), static_cast<int>(x), static_cast<int>(y), WHITE);
}

inline void drawSourceAtlasTexture(const char* path, Rectangle source, float x, float y) {
    const TextureResource* texture = loadPointSourceTexture(path);
    if (!texture) return;
    DrawTextureRec(texture->get(), source, {x, y}, WHITE);
}

inline void renderCpCapsuleSourceObject(const CpCapsuleSourceObjComposite& obj) {
    const char* atlasPath = CpCapsuleCutscene::sourceObjAtlasPath(obj.atlasPage);
    if (!atlasPath || *atlasPath == '\0' || obj.width <= 0 || obj.height <= 0) {
        return;
    }
    drawSourceAtlasTexture(
        atlasPath,
        Rectangle{
            static_cast<float>(obj.atlasX),
            static_cast<float>(obj.atlasY),
            static_cast<float>(obj.width),
            static_cast<float>(obj.height),
        },
        static_cast<float>(obj.screenX),
        static_cast<float>(obj.screenY));
}

inline void renderCpCapsuleSourceObject(const gameplay_cp_capsule::State& state) {
    if (!state.active()) return;
    renderCpCapsuleSourceObject(state.sourceObjComposite());
}

inline void renderCpCapsuleSourcePlane(const CpCapsuleSourcePlane& plane) {
    drawSourcePlaneTexture(plane.dialogWindowMaskPath, 0.0f, 0.0f);
    drawSourcePlaneTexture(plane.portraitBoxPath, 176.0f, 40.0f);
    drawSourcePlaneTexture(plane.bg3TextLayerPath, 0.0f, 0.0f);
}

inline void renderCpCapsuleSourcePlane(const gameplay_cp_capsule::State& state) {
    if (!state.active()) return;
    renderCpCapsuleSourcePlane(state.sourcePlane());
}

inline std::string formatTimerFrames(int timerFrames) {
    const int frames = std::max(0, timerFrames);
    const int totalSeconds = frames / 60;
    const int centiseconds = (frames % 60) * 100 / 60;

    char timerBuf[32];
    snprintf(timerBuf, sizeof(timerBuf), "%d:%02d.%02d",
             totalSeconds / 60, totalSeconds % 60, centiseconds);
    return timerBuf;
}

inline std::string formatTimerLine(const char* label, int timerFrames) {
    return std::string(label ? label : "") + " " + formatTimerFrames(timerFrames);
}

struct SpeedrunTimerOverlayText {
    std::string run;
    std::string stage;
    std::string best;
};

inline SpeedrunTimerOverlayText speedrunTimerOverlayText(int totalPlayFrames,
                                                         int stageTimer,
                                                         int bestTime,
                                                         const char* runLabel,
                                                         const char* stageLabel,
                                                         const char* bestLabel) {
    SpeedrunTimerOverlayText text;
    text.run = formatTimerLine(runLabel, totalPlayFrames);
    text.stage = formatTimerLine(stageLabel, stageTimer);
    if (bestTime > 0) {
        text.best = formatTimerLine(bestLabel, bestTime);
    }
    return text;
}

inline void renderSpeedrunTimerOverlay(bool showTimer,
                                       bool paused,
                                       bool gameOver,
                                       bool stageClear,
                                       int totalPlayFrames,
                                       int stageTimer,
                                       int bestTime) {
    if (!showTimer || paused || gameOver || stageClear) return;

    const SpeedrunTimerOverlayText text = speedrunTimerOverlayText(
        totalPlayFrames,
        stageTimer,
        bestTime,
        uiText(UiText::SpeedrunTimerRunPrefix, Settings::language),
        uiText(UiText::SpeedrunTimerStagePrefix, Settings::language),
        uiText(UiText::SpeedrunTimerBestPrefix, Settings::language));

    auto drawLine = [](const std::string& line, int y, Color color) {
        if (line.empty()) return;
        const int tw = MeasureText(line.c_str(), 7);
        DrawText(line.c_str(), INTERNAL_WIDTH - tw - 4, y, 7, color);
    };

    drawLine(text.run, 4, {210, 218, 230, 190});
    drawLine(text.stage, 12, {180, 220, 180, 190});
    drawLine(text.best, 20, {255, 220, 90, 185});
}

inline void renderDeathFlashOverlay(const Player& player) {
    if (!player.isDead()) return;

    const int t = player.deathTimer();
    constexpr int kFlashStart = 31; // coincides with the first death-orb ring
    constexpr int kFlashHold = 3;   // frames at full white
    constexpr int kFlashFade = 18;  // frames fading back out
    if (t >= kFlashStart && t < kFlashStart + kFlashHold + kFlashFade) {
        const float a = (t < kFlashStart + kFlashHold)
            ? 1.0f
            : 1.0f - static_cast<float>(t - kFlashStart - kFlashHold) /
                         static_cast<float>(kFlashFade);
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
                      {255, 255, 255, static_cast<unsigned char>(255 * a)});
    }
}

inline void renderDeathOrb(const Texture2D& texture,
                           const gameplay_death_orbs::Orb& orb,
                           int sheetX,
                           int sheetY,
                           int frameW,
                           int frameH,
                           float camX,
                           float camY) {
    const float halfW = frameW * 0.5f;
    const float halfH = frameH * 0.5f;
    const float sx = orb.x - camX - halfW;
    const float sy = orb.y - camY - halfH;
    Rectangle src = {
        static_cast<float>(sheetX + orb.animFrame * frameW),
        static_cast<float>(sheetY),
        static_cast<float>(frameW),
        static_cast<float>(frameH),
    };
    Rectangle dst = {sx, sy,
                     static_cast<float>(frameW),
                     static_cast<float>(frameH)};
    // Fade out in the last 15 frames of lifetime so they don't pop off.
    Color tint = WHITE;
    if (orb.lifetime < 15) {
        tint.a = static_cast<unsigned char>(255 * orb.lifetime / 15);
    }
    DrawTexturePro(texture, src, dst, {0, 0}, 0.0f, tint);
}

inline void renderDeathOrbs(const gameplay_death_orbs::State& state, float camX, float camY) {
    if (!state.sheet || !state.sheet->valid() || state.orbs.empty()) return;

    for (const auto& orb : state.orbs) {
        renderDeathOrb(
            state.sheet->get(),
            orb,
            state.spriteMeta.sheetX,
            state.spriteMeta.sheetY,
            state.spriteMeta.frameWidth,
            state.spriteMeta.frameHeight,
            camX,
            camY);
    }
}

inline void renderFadeInOverlay(int fadeInTimer, int fadeInDuration) {
    if (fadeInTimer <= 0) return;

    const int alpha = 255 * fadeInTimer / fadeInDuration;
    DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT,
                  {0, 0, 0, static_cast<unsigned char>(alpha)});
}

inline void renderFadeInOverlay(const gameplay_fade_in::State& state) {
    renderFadeInOverlay(state.timer, state.duration);
}

inline void renderWarpInBeam(int warpInTimer,
                             int warpInDuration,
                             float centerX,
                             float footY,
                             const Texture2D* capsuleTexture,
                             const Texture2D* burstTexture) {
    if (warpInTimer <= 0) return;

    const float p = 1.0f - static_cast<float>(warpInTimer) / warpInDuration;
    if (p < 0.70f && capsuleTexture) {
        const Texture2D& t = *capsuleTexture;
        const float d = p / 0.70f;
        const float capBottom = d * footY;
        const float capTop = capBottom - static_cast<float>(t.height);
        DrawTexturePro(t, {0, 0, static_cast<float>(t.width), static_cast<float>(t.height)},
                       {centerX - t.width * 0.5f, capTop,
                        static_cast<float>(t.width), static_cast<float>(t.height)},
                       {0, 0}, 0.0f, WHITE);
    } else if (p >= 0.70f && p < 0.87f && burstTexture) {
        const Texture2D& t = *burstTexture;
        DrawTexturePro(t, {0, 0, static_cast<float>(t.width), static_cast<float>(t.height)},
                       {centerX - t.width * 0.5f, footY - t.height,
                        static_cast<float>(t.width), static_cast<float>(t.height)},
                       {0, 0}, 0.0f, WHITE);
    }
}

inline void renderWarpInBeam(const gameplay_warp_in::State& state,
                             float centerX,
                             float footY) {
    renderWarpInBeam(
        state.timer,
        state.duration,
        centerX,
        footY,
        (state.capsuleTexture && state.capsuleTexture->valid())
            ? &state.capsuleTexture->get()
            : nullptr,
        (state.burstTexture && state.burstTexture->valid())
            ? &state.burstTexture->get()
            : nullptr);
}

struct FullscreenFadeLayout {
    int fadeFrames = 30;
    int maxAlpha = 200;
    Color color = {0, 0, 0, 255};
};

struct GameOverOverlayLayout {
    FullscreenFadeLayout fade = {30, 255, {0, 0, 0, 255}};
    int panelX = 24;
    int panelY = 18;
    int panelWidth = INTERNAL_WIDTH - 48;
    int panelHeight = 166;
    int cellWidth = 40;
    int cellHeight = 42;
    int cellGapX = 5;
    int cellGapY = 5;
    int gridX = 40;
    int gridY = 25;
    int digitFontSize = 22;
};

struct PauseOverlayLayout {
    FullscreenFadeLayout fade = {1, 160, {0, 0, 0, 255}};
    int headerStageX = 36;
    int headerY = 6;
    int headerFontSize = 7;
    int titleY = 16;
    int titleFontSize = 10;
    int weaponListX = 32;
    int weaponListY = 34;
    int weaponItemHeight = 18;
    int selectionInsetX = 4;
    int selectionInsetY = 1;
    int selectionWidth = INTERNAL_WIDTH - 56;
    int equippedMarkerOffsetX = 10;
    int swatchOffsetY = 2;
    int swatchSize = 8;
    int weaponNameOffsetX = 14;
    int weaponNameOffsetY = 1;
    int weaponNameFontSize = 8;
    int ammoBarOffsetX = 120;
    int ammoBarOffsetY = 3;
    int ammoBarWidth = 56;
    int ammoBarHeight = 6;
    int infiniteAmmoOffsetX = 140;
    int afterWeaponListGap = 8;
    int subTankOffsetX = 80;
    int subTankSpacing = 20;
    int subTankWidth = 14;
    int subTankHeight = 12;
    int armorGapY = 20;
    int armorNameOffsetX = 50;
    int armorNameSpacing = 38;
    int livesOffsetY = 14;
    int controlsSummaryY = INTERNAL_HEIGHT - 30;
    int controlsSummaryLineGap = 8;
    int controlsSummaryFontSize = 6;
    int hintY = INTERNAL_HEIGHT - 14;
    int hintFontSize = 7;
};

struct StageClearOverlayLayout {
    FullscreenFadeLayout fade = {30, 180, {0, 0, 40, 255}};
    int titleDelayFrames = 20;
    int titleBaseY = INTERNAL_HEIGHT / 2 - 30;
    int titleFontSize = 20;
    int titleShadowOffset = 1;
    int titleBobFrames = 10;
    int timeOffsetY = 22;
    int timeFontSize = 8;
    int bestOffsetY = 32;
    int bestFontSize = 7;
    int recordOffsetY = 42;
    int rewardDelayFrames = 40;
    int rewardSlideFrames = 15;
    int rewardY = INTERNAL_HEIGHT / 2 + 4;
    int zeroFontSize = 8;
    int weaponFontSize = 10;
    int weaponSwatchOffsetY = 16;
    int promptDelayFrames = 70;
    int promptY = INTERNAL_HEIGHT / 2 + 34;
    int promptFontSize = 8;
};

// GC2.1b. The screens the source shows after the spec/demo blackout starts.
// Every TIME and the two recorded text row sets are source-measured
// (weapon_get_presentation.h). CP/Storm use promoted source framebuffers;
// native fallback screens use the original MMX glyph atlas, the measured
// two-panel geometry, and live gameplay projectile/player renderers.
struct WeaponGetScreenLayout {
    Color background = {0, 0, 0, 255};
    int nameY = INTERNAL_HEIGHT / 2 - 24;
    int nameFontSize = 10;
    int nameLineStep = 12;
    int nameShadowOffset = 1;
    // Player::render draws the sprite at position + ~(23, 23) and the idle body
    // measures ~25x17, so these place the visible actor centred on the screen
    // and start the demo shot at his buster rather than over his head. Measured
    // off the capture, not guessed: build/autotest/autotest_*_weapon-get_09.png
    // put the body at x[143,167] y[163,179] for position (120, 140).
    int demoActorX = 92;
    int demoActorY = 127;
    int demoShotOffsetX = 48;
    int demoShotOffsetY = 31;
    int demoShotWidth = 8;
    int demoShotHeight = 3;
    int demoShotSpeed = 4;
};

inline constexpr WeaponGetScreenLayout weaponGetScreenLayout() {
    return WeaponGetScreenLayout{};
}

inline constexpr GameOverOverlayLayout gameOverOverlayLayout() {
    return GameOverOverlayLayout{};
}

inline constexpr PauseOverlayLayout pauseOverlayLayout() {
    return PauseOverlayLayout{};
}

inline constexpr StageClearOverlayLayout stageClearOverlayLayout() {
    return StageClearOverlayLayout{};
}

inline int centeredXForWidth(int width) {
    return (INTERNAL_WIDTH - width) / 2;
}

inline int overlayFadeAlpha(int timer, const FullscreenFadeLayout& layout) {
    const int alpha = (timer < layout.fadeFrames)
        ? (timer * 255 / layout.fadeFrames)
        : 255;
    return std::min(alpha, layout.maxAlpha);
}

inline Color withAlpha(Color color, int alpha) {
    color.a = static_cast<unsigned char>(std::clamp(alpha, 0, 255));
    return color;
}

inline int overlayMenuItemY(int menuY, int itemHeight, int index) {
    return menuY + index * itemHeight;
}

inline int pauseWeaponRowY(const PauseOverlayLayout& layout, int index) {
    return overlayMenuItemY(layout.weaponListY, layout.weaponItemHeight, index);
}

inline int pauseSubTankY(const PauseOverlayLayout& layout, int weaponCount) {
    return layout.weaponListY + weaponCount * layout.weaponItemHeight + layout.afterWeaponListGap;
}

inline int stageClearTitleBobY(const StageClearOverlayLayout& layout, int textAge) {
    return (textAge < layout.titleBobFrames) ? (layout.titleBobFrames - textAge) : 0;
}

inline int slideInOffsetX(int age, int frames) {
    return (age < frames) ? (INTERNAL_WIDTH - INTERNAL_WIDTH * age / frames) : 0;
}

void drawCenteredText(const char* text, int y, int fontSize, Color color);
void renderStageStartReady(int stageStartTick, const TextureResource* atlas);

void renderGameOverOverlay(bool gameOver,
                           int gameOverTimer,
                           const std::array<int, 12>& gridDigits,
                           const std::array<int, 12>& helmetTicks,
                           const TextureResource* sourceSheet);

void renderPauseOverlay(bool paused,
                        const std::string& stageName,
                        int stageTimer,
                        const Player& player,
                        int pauseWeaponCursor);
void renderPauseOverlay(const gameplay_pause_menu::State& state,
                        const std::string& stageName,
                        int stageTimer,
                        const Player& player);

void renderStageClearOverlay(bool stageClear,
                             int stageClearTimer,
                             int stageTimer,
                             bool bossActive,
                             int bestTime,
                             bool bossRescued,
                             const std::optional<Weapon>& awardedWeapon);
void renderStageClearOverlay(const gameplay_stage_clear::State& state,
                             int stageTimer,
                             bool bossActive,
                             int bestTime,
                             bool bossRescued);

// The stage-clear banner on its own (fade, title, times). During the weapon-get
// sequence this is all the arena phase draws: the reward text and the "press A"
// prompt move to the sequence's own screens, and the source has no prompt at all.
void renderStageClearBanner(int stageClearTimer,
                            int stageTimer,
                            bool bossActive,
                            int bestTime);

// Source rows: "YOU GET" then one row per weapon-name word. Kept here rather
// than in the scene so the reward text stays one rule in one file.
std::string weaponGetText(const Weapon& weapon);

// Complete source-backed screens exist for the two bosses whose post-boss
// movies were captured with deterministic PPU material. Other weapons keep the
// procedural fallback until an equivalent source screen is available.
const char* weaponGetSourceScreenPath(
    const weapon_get_presentation::Screen screen,
    const Weapon& weapon);
bool hasWeaponGetSourceScreen(
    const weapon_get_presentation::Screen screen,
    const Weapon& weapon);
bool weaponGetNativeFallbackPanelVisible(int tick);

// The spec/text and demo screens. `tick` is relative to the victory cue.
void renderWeaponGetScreen(int tick,
                           const weapon_get_presentation::Params& params,
                           const weapon_get_presentation::Plan& plan,
                           const std::string& text,
                           const Weapon& weapon);
inline void renderEscapeConfirmDialog(bool yesSelected) {
    DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT, {0, 0, 0, 170});

    constexpr int boxW = 158;
    constexpr int boxH = 64;
    constexpr int boxX = (INTERNAL_WIDTH - boxW) / 2;
    constexpr int boxY = (INTERNAL_HEIGHT - boxH) / 2;
    DrawRectangle(boxX, boxY, boxW, boxH, {0, 25, 84, 255});
    DrawRectangleLines(boxX, boxY, boxW, boxH, {210, 235, 255, 255});
    DrawRectangleLines(boxX + 1, boxY + 1, boxW - 2, boxH - 2, {74, 148, 236, 255});
    DrawRectangle(boxX + 4, boxY + 4, boxW - 8, boxH - 8, {0, 12, 48, 255});

    const char* title = "RETURN TO TITLE?";
    const int titleW = MeasureText(title, 9);
    DrawText(title, boxX + (boxW - titleW) / 2, boxY + 13, 9, {220, 236, 255, 255});

    constexpr int yesX = 79;
    constexpr int noX = 143;
    constexpr int choiceY = 122;
    DrawText("YES", yesX, choiceY, 9, yesSelected ? Color{255, 248, 160, 255} : LIGHTGRAY);
    DrawText("NO", noX, choiceY, 9, !yesSelected ? Color{255, 248, 160, 255} : LIGHTGRAY);
    DrawText(">", yesSelected ? yesX - 12 : noX - 12, choiceY, 9, {255, 248, 160, 255});
}

inline void renderEscapeConfirmDialog(const gameplay_escape_confirm::State& state) {
    if (!gameplay_escape_confirm::isOpen(state)) return;
    renderEscapeConfirmDialog(gameplay_escape_confirm::yesSelected(state));
}

inline void renderCheckpointDebugFlag(Vector2 respawn, bool triggered, float camX, float camY) {
    float cpScreenX = respawn.x - camX;
    float cpScreenY = respawn.y - camY;
    Color flagColor = triggered ? Color{100, 255, 100, 200} : Color{255, 200, 50, 200};
    DrawRectangle(static_cast<int>(cpScreenX), static_cast<int>(cpScreenY - 16),
                  2, 16, flagColor);
    DrawRectangle(static_cast<int>(cpScreenX + 2), static_cast<int>(cpScreenY - 16),
                  8, 6, flagColor);
}

struct DebugHudBossData {
    bool visible;
    int state;
    int health;
    int maxHealth;
    int phase;
    int fightTimer;
};

struct DebugHudData {
    const char* playerState;
    int chargeLevel;
    int health;
    int maxHealth;
    int lives;
    int iframeTimer;
    Vector2 position;
    Vector2 velocity;
    int projectileCount;
    int enemyCount;
    bool onGround;
    bool touchingWallLeft;
    bool touchingWallRight;
    const char* weaponName;
    DebugHudBossData boss;
};

inline void renderDebugHud(const DebugHudData& data) {
    char buf[128];
    snprintf(buf, sizeof(buf), "STATE: %s  CHARGE: %d",
             data.playerState, data.chargeLevel);
    DrawText(buf, 4, 2, 8, WHITE);

    snprintf(buf, sizeof(buf), "HP: %d/%d  LIVES: %d  IFRAMES: %d",
             data.health, data.maxHealth, data.lives, data.iframeTimer);
    DrawText(buf, 4, 12, 8, LIGHTGRAY);

    snprintf(buf, sizeof(buf), "POS: %.0f,%.0f  VEL: %.1f,%.1f  SHOTS: %d  ENEMIES: %d",
             data.position.x, data.position.y,
             data.velocity.x, data.velocity.y,
             data.projectileCount,
             data.enemyCount);
    DrawText(buf, 4, 22, 8, LIGHTGRAY);

    snprintf(buf, sizeof(buf), "GROUND: %s  WALL: %s%s  WPN: %s",
             data.onGround ? "YES" : "NO",
             data.touchingWallLeft ? "L" : "",
             data.touchingWallRight ? "R" : "",
             data.weaponName);
    DrawText(buf, 4, 32, 8, LIGHTGRAY);

    if (data.boss.visible) {
        const char* bossStates[] = {
            "DORMANT", "INTRO", "IDLE", "STARTUP", "ACTIVE",
            "RECOVERY", "STUNNED", "DYING", "DEAD", "RESCUED"
        };
        snprintf(buf, sizeof(buf), "BOSS: %s  HP: %d/%d  PHASE: %d  FIGHT: %d",
                 (data.boss.state >= 0 && data.boss.state <= 9)
                    ? bossStates[data.boss.state]
                    : "?",
                 data.boss.health,
                 data.boss.maxHealth,
                 data.boss.phase,
                 data.boss.fightTimer);
        DrawText(buf, 4, 42, 8, {255, 180, 100, 255});
    }

    std::string controls = Input::isGamepadConnected()
        ? "A:JUMP B:DASH Y:SHOOT DPAD:MOVE START:PAUSE"
        : keyboardActionHint(Input::bindings(), InputAction::Jump, "JUMP") + " " +
          keyboardActionHint(Input::bindings(), InputAction::Dash, "DASH") + " " +
          keyboardActionHint(Input::bindings(), InputAction::Shoot, "SHOOT") + " " +
          keyboardDirectionalHint(Input::bindings(), "MOVE") + " " +
          keyboardActionHint(Input::bindings(), InputAction::Pause, "PAUSE");
    DrawText(controls.c_str(), 4, INTERNAL_HEIGHT - 10, 8, {150, 150, 150, 255});
}

inline void renderSpriteTestOverlay(const Texture2D& texture,
                                    int spriteTestFrame,
                                    int spriteWidth,
                                    int spriteHeight) {
    // Dark background
    DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT, {0, 0, 0, 200});

    int cols = texture.width / spriteWidth;
    int tx = (spriteTestFrame % cols) * spriteWidth;
    int ty = (spriteTestFrame / cols) * spriteHeight;

    // Draw current frame large (3x scale) in center
    float scale = 3.0f;
    float drawX = (INTERNAL_WIDTH - spriteWidth * scale) / 2;
    float drawY = (INTERNAL_HEIGHT - spriteHeight * scale) / 2 - 10;

    Rectangle srcRect = {static_cast<float>(tx), static_cast<float>(ty),
                         static_cast<float>(spriteWidth), static_cast<float>(spriteHeight)};
    Rectangle dstRect = {drawX, drawY, spriteWidth * scale, spriteHeight * scale};
    DrawTexturePro(texture, srcRect, dstRect, {0, 0}, 0.0f, WHITE);

    // Also draw flipped version next to it
    Rectangle srcFlip = {static_cast<float>(tx), static_cast<float>(ty),
                         -static_cast<float>(spriteWidth), static_cast<float>(spriteHeight)};
    Rectangle dstFlip = {drawX + spriteWidth * scale + 8, drawY,
                         spriteWidth * scale, spriteHeight * scale};
    DrawTexturePro(texture, srcFlip, dstFlip, {0, 0}, 0.0f, WHITE);

    // Frame number (big)
    char buf[64];
    snprintf(buf, sizeof(buf), "FRAME %d", spriteTestFrame);
    int textW = MeasureText(buf, 14);
    DrawText(buf, (INTERNAL_WIDTH - textW) / 2, 10, 14, {255, 255, 100, 255});

    // Source coordinates
    snprintf(buf, sizeof(buf), "src: (%d, %d)  size: %dx%d",
             tx, ty, spriteWidth, spriteHeight);
    int srcW = MeasureText(buf, 8);
    DrawText(buf, (INTERNAL_WIDTH - srcW) / 2, 28, 8, {180, 180, 200, 255});

    // Navigation help
    std::string helpText = keyboardHorizontalHint(Input::bindings(), "BROWSE") + "   " +
                           keyboardVerticalHint(Input::bindings(), "+/-10") + "   F3:EXIT";
    int helpW = MeasureText(helpText.c_str(), 7);
    DrawText(helpText.c_str(), (INTERNAL_WIDTH - helpW) / 2, INTERNAL_HEIGHT - 16, 7,
             {120, 120, 150, 255});

    // Show neighboring frames (small) at bottom
    int thumbY = static_cast<int>(drawY + spriteHeight * scale + 16);
    int thumbScale = 1;
    int thumbSpacing = spriteWidth + 4;
    int startFrame = std::max(0, spriteTestFrame - 4);
    int totalFrames = cols * (texture.height / spriteHeight);
    int endFrame = std::min(totalFrames - 1, spriteTestFrame + 4);

    int stripW = (endFrame - startFrame + 1) * thumbSpacing;
    int stripX = (INTERNAL_WIDTH - stripW) / 2;

    for (int f = startFrame; f <= endFrame; f++) {
        int ftx = (f % cols) * spriteWidth;
        int fty = (f / cols) * spriteHeight;
        int px = stripX + (f - startFrame) * thumbSpacing;

        Rectangle fSrc = {static_cast<float>(ftx), static_cast<float>(fty),
                          static_cast<float>(spriteWidth), static_cast<float>(spriteHeight)};
        Rectangle fDst = {static_cast<float>(px), static_cast<float>(thumbY),
                          static_cast<float>(spriteWidth * thumbScale),
                          static_cast<float>(spriteHeight * thumbScale)};

        Color tint = (f == spriteTestFrame) ? WHITE : Color{150, 150, 150, 200};
        DrawTexturePro(texture, fSrc, fDst, {0, 0}, 0.0f, tint);

        // Frame number below thumbnail
        snprintf(buf, sizeof(buf), "%d", f);
        Color numColor = (f == spriteTestFrame)
            ? Color{255, 255, 100, 255}
            : Color{120, 120, 150, 255};
        DrawText(buf, px + 2, thumbY + spriteHeight + 2, 7, numColor);
    }
}

inline void renderSpriteTestOverlay(const Texture2D& texture,
                                    const gameplay_sprite_test::State& state,
                                    int spriteWidth,
                                    int spriteHeight) {
    renderSpriteTestOverlay(texture, state.frame, spriteWidth, spriteHeight);
}

} // namespace mmx::gameplay_presentation
