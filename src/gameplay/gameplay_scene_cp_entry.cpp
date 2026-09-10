// gameplay_scene_cp_entry.cpp - source-timed Chill Penguin room entrance.

#include "gameplay/gameplay_scene.h"
#include "entities/boss_cp_intro_timeline.h"
#include "gameplay/gameplay_boss.h"
#include "systems/asset_cache.h"
#include "systems/audio.h"

namespace mmx {
namespace {

constexpr int kShutterMinCol = 479;
constexpr int kShutterMinRow = 23;
constexpr int kShutterWorldCol = 480;
constexpr int kEntryArtWorldX = 7616;
constexpr int kEntryArtWorldY = 257;

} // namespace

void GameplayScene::setupCpEntryStagePresentation() {
    if (activeStageId_.str() != "chill-penguin") return;
    cpEntrySourceOverlayTex_ = AssetCache::loadTexture(
        "content/x1/stages/tiles/chill-penguin_boss_entry_source_overlay.png");
    cpEntryShutterTex_ = AssetCache::loadTexture(
        "content/x1/sprites/misc/chill-penguin_entry_shutter.png");
    for (auto& layer : tilemap_.layersMut()) {
        if (layer.name != "main") continue;
        for (int row = kShutterMinRow; row < kShutterMinRow + 3; ++row) {
            for (int col = kShutterMinCol; col < kShutterMinCol + 2; ++col) {
                const int index = row * tilemap_.width() + col;
                if (index >= 0 && index < static_cast<int>(layer.data.size())) {
                    layer.data[index] = 0;
                }
            }
        }
        layer.previewTex.reset();
        return;
    }
}

void GameplayScene::setupCpEntryHitboxRollout() {
    entrySourceHitboxOffset_ = player_.hitboxOffset;
    entrySourceHitboxSize_ = player_.hitboxSize;
    hasEntrySourceHitbox_ = true;
    if (skipInitialFade) return;
    // Preserve unrelated source-pinned encounter clocks until the canonical
    // CP room transition restores the approved source box atomically.
    player_.hitboxOffset = {21.0f, 22.0f};
    player_.hitboxSize = {24.0f, 35.0f};
}

void GameplayScene::seedCpBossEntryAutotest() {
    if (!skipInitialFade || activeStageId_.str() != "chill-penguin" ||
        !bossActive_ || !boss_.active || bossLocked_) return;
    for (const auto& room : tilemap_.rooms()) {
        if (room.name.empty()) continue;
        bossLocked_ = true;
        boss_.activate();
        camera_.setRoom(room.x, room.y, room.w, room.h);
        gameplay_boss::syncBossHpBar(boss_, hud_);
        AudioManager::playSFX(SFX::BossIntro);
        bossEntryCinematicActive_ = true;
        bossEntryShutterPassable_ = true;
        bossEntryCamLockX_ = room.x;
        player_.beginScriptedEntryWalk();
        player_.position = {
            boss_cp_intro_timeline::entryPlayerPositionX(
                boss_cp_intro_timeline::kSourceSpawnTick),
            boss_cp_intro_timeline::entryPlayerPositionY(
                boss_cp_intro_timeline::kSourceSpawnTick)};
        player_.prevPosition = player_.position;
        player_.onGround = true;
        player_.velocity = {0.0f, 0.0f};
        return;
    }
}

void GameplayScene::updateCpEntryRoute() {
    if (!bossEntryCinematicActive_) return;
    if (player_.isDead()) {
        player_.endScriptedEntryWalk();
        camera_.setClampSuppressed(false);
        bossEntryCinematicActive_ = false;
        bossEntryShutterPassable_ = false;
        return;
    }
    const int tick = boss_.introSourceTick();
    bossEntryShutterPassable_ =
        boss_cp_intro_timeline::entryShutterPassable(tick);
    if (tick == boss_cp_intro_timeline::kShutterSealCueTick) {
        AudioManager::playApu(0x41);
    }
    player_.position = {
        boss_cp_intro_timeline::entryPlayerPositionX(tick),
        boss_cp_intro_timeline::entryPlayerPositionY(tick)};
    player_.onGround = tick <= 120 ||
        tick >= boss_cp_intro_timeline::kEntrySettleTick;
    if (tick == boss_cp_intro_timeline::kSourceSpawnTick) {
        player_.prevPosition = player_.position;
    }
    if (tick >= boss_cp_intro_timeline::kEntryWalkLastAdvanceTick) {
        player_.endScriptedEntryWalk();
    }
    if (tick >= boss_cp_intro_timeline::kEntrySettleTick) {
        camera_.setClampSuppressed(false);
        bossEntryCinematicActive_ = false;
        bossEntryShutterPassable_ = false;
    }
}

void GameplayScene::updateCpEntryCamera() {
    if (!bossLocked_ || !boss_.cpStageIntroConfigured() || player_.isDead()) {
        return;
    }
    float cameraX = bossEntryCamLockX_;
    if (bossEntryCinematicActive_) {
        const float offset = boss_cp_intro_timeline::entryCameraOffsetPx(
            boss_.introSourceTick());
        cameraX += offset;
        camera_.setClampSuppressed(offset < 0.0f);
    }
    camera_.setBaseXY(
        cameraX, static_cast<float>(boss_cp_intro_timeline::kEntryCameraYPx));
}

bool GameplayScene::suppressCpBossRoomVisualGrounding() const {
    // The source room places grounded X directly on the y191 snow lip. The
    // general stage-art accommodation (+8 px) sinks his boots into this exact
    // source floor, so the boss room suppresses it. The measured additional
    // 2 px render lift lives in boss_cp_intro_timeline.h and keeps the last
    // boot pixel at y191, directly above the y192 source floor strip.
    return activeStageId_.str() == "chill-penguin" && bossLocked_ &&
        boss_.cpStageIntroConfigured();
}

void GameplayScene::renderCpEntrySourceArt(float cameraX, float cameraY) const {
    if (activeStageId_.str() != "chill-penguin" ||
        !cpEntrySourceOverlayTex_ || !cpEntrySourceOverlayTex_->valid()) return;
    const Texture2D& texture = cpEntrySourceOverlayTex_->get();
    const Rectangle source = {
        0.0f, 0.0f, static_cast<float>(texture.width),
        static_cast<float>(texture.height)};
    const Rectangle destination = {
        static_cast<float>(kEntryArtWorldX) - cameraX,
        static_cast<float>(kEntryArtWorldY) - cameraY,
        static_cast<float>(texture.width), static_cast<float>(texture.height)};
    DrawTexturePro(texture, source, destination, {0, 0}, 0.0f, WHITE);
}

void GameplayScene::renderCpEntryShutter(float cameraX, float cameraY) const {
    if (activeStageId_.str() != "chill-penguin" || !bossLocked_ ||
        !boss_.cpStageIntroConfigured() || !cpEntryShutterTex_ ||
        !cpEntryShutterTex_->valid()) return;
    const int frame = boss_cp_intro_timeline::entryShutterFrame(
        boss_.introSourceTick());
    if (frame < 0) return;
    const Rectangle source = {
        static_cast<float>(frame * 16), 0.0f, 16.0f,
        static_cast<float>(boss_cp_intro_timeline::kShutterHeightPx)};
    const Rectangle destination = {
        static_cast<float>(kShutterWorldCol * tilemap_.tileSize()) - cameraX,
        static_cast<float>(kShutterMinRow * tilemap_.tileSize()) - cameraY,
        16.0f, static_cast<float>(boss_cp_intro_timeline::kShutterHeightPx)};
    DrawTexturePro(cpEntryShutterTex_->get(), source, destination,
                   {0, 0}, 0.0f, WHITE);
}

} // namespace mmx
