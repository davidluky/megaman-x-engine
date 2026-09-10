// boss_intro_scene.cpp - plays the boss-intro presentation before fights.
// Owns: intro timing, title-card drawing, boss texture use, and scene exit.

#include "ui/boss_intro_scene.h"
#include "gameplay/gameplay_scene.h"
#include "app/scene_manager.h"
#include "app/input.h"
#include "app/constants.h"
#include "systems/asset_cache.h"
#include "systems/audio.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

namespace mmx {

// U52 REBUILD — the timeline below is the MEASURED CP intro (786-frame
// per-frame capture, knowledge_base/mmx1/boss_intro/cp_full — local,
// regenerable via u52_intro_capture.lua; phase map in
// docs/evidence/2026-06-12-U52/README.md). Scene t0 = capture f37 (the
// select-cell zoom + mosaic scramble f1-36 happen on the SELECT screen —
// stage-select transition follow-up, not this scene).
namespace {
constexpr Color kBossIntroBlue{24, 82, 156, 255};  // sampled boss-intro bg

// capture-frame anchors minus 36:
// R315: source81 is the first active song frame after the upload/quiet gap.
// This is08 Stage Intro; the later1F/20 mailbox events are separate SFX.
constexpr int kMusicQuietStart = 38;  // source74: previous voices are off
constexpr int kMusicStart = 45;
constexpr int kBoltA      = 77;   // f113: strike A (4f) + remnant (5f)
constexpr int kBoltAEnd   = 86;
constexpr int kBoltB      = 116;  // f152: strike B (4f) + C (5f)
constexpr int kBoltBEnd   = 125;
constexpr int kWhiteRamp  = 165;  // f201: black -> white over 40f
constexpr int kWhiteHold  = 205;  // f240: full white
constexpr int kBlueFade   = 220;  // f256: white -> blue, boss materializes
constexpr int kWalkIn     = 244;  // f280: boss waddles in place on blue
constexpr int kBannerIn   = 274;  // f310: triangle drops / star rises (35f)
constexpr int kComposed   = 309;  // f345: composition settled
constexpr int kNameStart  = 319;  // f355: name spells left-to-right
constexpr int kNameEnd    = 383;
constexpr int kHoldEnd    = 569;  // f605: fade out begins
constexpr int kFadeEnd    = 585;  // f621: scene ends (stage loads)

std::string assetKey(const std::string& engineId) {
    std::string s = engineId;
    std::replace(s.begin(), s.end(), '-', '_');
    return s;
}

const TextureResource* loadIfExists(const std::string& path) {
    if (!FileExists(path.c_str())) return nullptr;
    const TextureResource* t = AssetCache::loadTexture(path);
    if (t) t->setFilter(TEXTURE_FILTER_POINT);
    return t;
}

const TextureResource* loadStageBolt(const std::string& base,
                                     const std::string& key,
                                     const char* suffix) {
    if (const TextureResource* stageBolt =
            loadIfExists(base + key + "_" + suffix + ".png")) {
        return stageBolt;
    }
    if (key == "chill_penguin") {
        return loadIfExists(base + suffix + ".png");
    }
    return nullptr;
}

std::string exactFramePath(const std::string& base, int frame) {
    char name[16];
    std::snprintf(name, sizeof(name), "f%04d.png", frame);
    return base + name;
}

int countExactFrameSequence(const std::string& base) {
    int count = 0;
    for (int frame = 1; frame < kFadeEnd; ++frame) {
        if (!FileExists(exactFramePath(base, frame).c_str())) break;
        count = frame;
    }
    return count;
}
}

void BossIntroScene::onEnter() {
    timer_ = 0;
    finished_ = false;

    const std::string key = assetKey(stageId);
    const std::string base = "content/x1/sprites/boss_intro/";
    exactFrameBase_ = base + "frame_sequences/" + key + "/";
    exactFrameCount_ = countExactFrameSequence(exactFrameBase_);
    hold_        = loadIfExists(base + key + "_hold.png");
    backdrop_    = loadIfExists(base + "backdrop_empty.png");
    boss_        = loadIfExists(base + key + "_boss.png");
    name_        = loadIfExists(base + key + "_name.png");
    downTri_     = loadIfExists(base + "down_triangle.png");
    starEmblem_  = loadIfExists(base + "star_emblem.png");
    boltA_       = loadStageBolt(base, key, "bolt_a");
    boltA2_      = loadStageBolt(base, key, "bolt_a2");
    boltB_       = loadStageBolt(base, key, "bolt_b");
    boltB2_      = loadStageBolt(base, key, "bolt_b2");
    for (int i = 0; i < 4; ++i) {
        walk_[i] = loadIfExists(base + key + "_walk" + std::to_string(i) + ".png");
    }

    // Need at least a baked hold or the composed backdrop, else don't strand
    // the player on a black screen — go straight to gameplay.
    if (exactFrameCount_ == 0 && !hold_ && !backdrop_) finish();
}

void BossIntroScene::handleInput() {
    if (finished_) return;
    // Allow skipping once the lightning phase is past.
    if (timer_ > kBoltBEnd && (Input::isConfirmPressed() || Input::isCancelPressed())) {
        Input::consumeConfirmPress();
        finish();
    }
}

void BossIntroScene::update(float /*dt*/) {
    if (finished_) return;
    timer_++;
    if (timer_ == kMusicQuietStart) AudioManager::stopBGM();
    if (timer_ == kMusicStart) AudioManager::playBGM("stage-intro", false);
    if (timer_ >= kFadeEnd) finish();
}

void BossIntroScene::drawHold(float scale, unsigned char alpha) const {
    if (!hold_) return;
    const float w = static_cast<float>(hold_->width());
    const float h = static_cast<float>(hold_->height());
    Rectangle src = {0, 0, w, h};
    const float dw = INTERNAL_WIDTH * scale;
    const float dh = INTERNAL_HEIGHT * scale;
    Rectangle dst = {(INTERNAL_WIDTH - dw) * 0.5f, (INTERNAL_HEIGHT - dh) * 0.5f, dw, dh};
    DrawTexturePro(hold_->get(), src, dst, {0, 0}, 0.0f, {255, 255, 255, alpha});
}

void BossIntroScene::drawBossWalk() const {
    // Measured: the boss waddles IN PLACE at ~(126,114) center during the
    // walk-in window (cells ripped from f285-309; CP cell = 64x64).
    const int phase = (timer_ / 8) % 4;
    const TextureResource* cell = walk_[phase] ? walk_[phase] : walk_[0];
    if (cell && cell->valid()) {
        DrawTexture(cell->get(), 126 - cell->width() / 2,
                    114 - cell->height() / 2, WHITE);
    } else if (boss_) {
        DrawTexture(boss_->get(), 0, 0, WHITE);
    }
}

bool BossIntroScene::drawExactFrameSequence() const {
    if (exactFrameCount_ <= 0 || timer_ < 1 || timer_ > exactFrameCount_) return false;
    const TextureResource* frame = loadIfExists(exactFramePath(exactFrameBase_, timer_));
    if (!frame || !frame->valid()) return false;
    DrawTexture(frame->get(), 0, 0, WHITE);
    return true;
}

void BossIntroScene::render(float /*alpha*/) {
    ClearBackground(BLACK);
    if (drawExactFrameSequence()) return;

    const int t = timer_;

    if (t < kWhiteRamp) {
        // LONG BLACK with two lightning strikes (the real cadence: strike A
        // at t77 — 4f main + 5f remnant — and strike B at t116).
        const TextureResource* bolt = nullptr;
        if (t >= kBoltA && t < kBoltA + 4) bolt = boltA_;
        else if (t >= kBoltA + 4 && t < kBoltAEnd) bolt = boltA2_;
        else if (t >= kBoltB && t < kBoltB + 4) bolt = boltB_;
        else if (t >= kBoltB + 4 && t < kBoltBEnd) bolt = boltB2_;
        if (bolt && bolt->valid()) DrawTexture(bolt->get(), 0, 0, WHITE);
    } else if (t < kWhiteHold) {
        // black -> WHITE ramp (measured brightness 0 -> 255 over 40f).
        const float k = (t - kWhiteRamp) / static_cast<float>(kWhiteHold - kWhiteRamp);
        const unsigned char a = static_cast<unsigned char>(255 * k);
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT, {255, 255, 255, a});
    } else if (t < kBlueFade) {
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT, WHITE);
    } else if (t < kWalkIn) {
        // WHITE -> BLUE with the boss materializing inside the light.
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT, kBossIntroBlue);
        drawBossWalk();
        const float k = (t - kBlueFade) / static_cast<float>(kWalkIn - kBlueFade);
        const unsigned char a = static_cast<unsigned char>(255 * (1.0f - k));
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT, {255, 255, 255, a});
    } else if (t < kBannerIn) {
        // Boss waddling alone on the blue field.
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT, kBossIntroBlue);
        drawBossWalk();
    } else if (t < kComposed) {
        // Triangle drops from the top / star rises from the bottom over the
        // measured 35f window (linear; exact easing $open).
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT, kBossIntroBlue);
        const float k = (t - kBannerIn) / static_cast<float>(kComposed - kBannerIn);
        if (downTri_ && downTri_->valid()) {
            const int yoff = static_cast<int>(-(1.0f - k) * INTERNAL_HEIGHT);
            DrawTexture(downTri_->get(), 0, yoff, WHITE);
        }
        if (starEmblem_ && starEmblem_->valid()) {
            // rest position measured at ~(84,56) (f420 template match)
            const int restY = 56;
            const int y = restY + static_cast<int>((1.0f - k) * (INTERNAL_HEIGHT - restY));
            DrawTexture(starEmblem_->get(), 84, y, WHITE);
        }
        drawBossWalk();
    } else if (t < kHoldEnd) {
        // Composition settled; name spells left-to-right in its window.
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT, kBossIntroBlue);
        if (backdrop_) DrawTexture(backdrop_->get(), 0, 0, WHITE);
        if (boss_) DrawTexture(boss_->get(), 0, 0, WHITE);
        if (t >= kNameStart && name_) {
            const float k = std::min(1.0f, (t - kNameStart) /
                            static_cast<float>(kNameEnd - kNameStart));
            const int w = static_cast<int>(INTERNAL_WIDTH * k);
            Rectangle src = {0, 0, static_cast<float>(w),
                             static_cast<float>(name_->height())};
            DrawTextureRec(name_->get(), src, {0, 0}, WHITE);
        }
        // Prefer the exact captured HOLD frame once everything is settled.
        if (t >= kNameEnd && hold_) drawHold(1.0f, 255);
    } else {
        // Fade to black, then into the stage.
        const float k = (t - kHoldEnd) / static_cast<float>(kFadeEnd - kHoldEnd);
        if (hold_) drawHold(1.0f, 255);
        else {
            DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT, kBossIntroBlue);
            if (backdrop_) DrawTexture(backdrop_->get(), 0, 0, WHITE);
            if (boss_) DrawTexture(boss_->get(), 0, 0, WHITE);
        }
        const unsigned char a = static_cast<unsigned char>(std::min(255.0f, 255.0f * k));
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT, {0, 0, 0, a});
    }
}

void BossIntroScene::finish() {
    if (finished_) return;
    finished_ = true;
    if (!sceneManager_) return;
    GameplaySceneConfig gameplayConfig;
    gameplayConfig.stagePath = stagePath;
    gameplayConfig.stageId = stageIdEnum;
    gameplayConfig.characterPath = characterPath;
    gameplayConfig.playWarpIn = true;  // X warps into the stage after the reveal
    auto gameplay = std::make_unique<GameplayScene>(gameplayConfig);
    gameplay->setSceneManager(sceneManager_);
    sceneManager_->changeScene(std::move(gameplay));
}

} // namespace mmx
