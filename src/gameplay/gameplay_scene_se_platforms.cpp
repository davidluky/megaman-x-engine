// gameplay_scene_se_platforms.cpp - R96C Storm Eagle scene seam.

#include "gameplay/gameplay_scene_se_platforms.h"

#include "entities/player.h"
#include "entities/player_anchor.h"
#include "systems/asset_cache.h"

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mmx {
namespace {

std::string normalizedStagePath(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    while (path.rfind("./", 0) == 0) path.erase(0, 2);
    return path;
}

std::int16_t signedWordDelta(std::uint16_t current,
                             std::uint16_t previous) {
    return static_cast<std::int16_t>(
        static_cast<std::uint16_t>(current - previous));
}

std::uint16_t wrappedCameraCoordinate(std::uint16_t value,
                                      std::uint16_t camera) {
    return static_cast<std::uint16_t>(value - camera + 64u);
}

bool insideSourceCull(const StormEaglePlatformState& state,
                      std::uint16_t cameraX, std::uint16_t cameraY) {
    return wrappedCameraCoordinate(state.x, cameraX) < 384u &&
           wrappedCameraCoordinate(state.y, cameraY) < 352u;
}

int sourcePlayerX(const Player& player) {
    return static_cast<int>(std::floor(player_anchor::sourceRamAnchorX(
        player.position.x, player.spriteWidth, player.facingRight)));
}

int sourcePlayerY(const Player& player) {
    return static_cast<int>(std::floor(player.position.y + 40.0f));
}

}  // namespace

bool StormEaglePlatformScene::canonicalScope(
    const StormEaglePlatformSceneConfig& config) {
    return config.activeStageId == "storm-eagle" &&
           normalizedStagePath(config.stagePath) ==
               "content/x1/stages/tiles/storm-eagle_full.json" &&
           !config.randomizerMode && !config.arenaWaveMode &&
           !config.bossRushMode;
}

void StormEaglePlatformScene::configure(
    const StormEaglePlatformSceneConfig& config) {
    config_ = config;
    enabled_ = canonicalScope(config_);
    reset();
    if (enabled_ && IsWindowReady()) {
        texture_ = AssetCache::loadTexture(
            "content/x1/sprites/objects/storm_eagle_platform.png");
    }
}

void StormEaglePlatformScene::reset() {
    platform_ = StormEaglePlatform{};
    animation_ = StormEaglePlatformAnimation::fromSource();
    previousRenderState_ = {};
    currentRenderState_ = {};
    lifecycle_ = enabled_ ? Lifecycle::Unallocated : Lifecycle::Disabled;
    hasCompletedRenderState_ = false;
    playerSupportLatched_ = false;
    recordSupportLatched_ = false;
    sourceX_ = 0;
    sourceFlag_ = 0;
    pendingRender_ = {};
    displayedRender_ = {};
    pendingRenderQueued_ = false;
    texture_ = nullptr;
}

void StormEaglePlatformScene::beginFrame(std::uint16_t sourceX,
                                         std::uint8_t sourceFlag) {
    sourceX_ = sourceX;
    if (!enabled_ || lifecycle_ != Lifecycle::Unallocated) return;
    if (sourceX < 256) return;

    sourceFlag_ = sourceFlag;
    lifecycle_ = Lifecycle::Allocated;
}

bool StormEaglePlatformScene::prepareSourceFrame(
    std::uint16_t sourceX, std::uint8_t sourceFlag) {
    // Shift exactly once per physics tick. An invalid pending record clears
    // the display only after the previous valid snapshot had its last frame.
    if (pendingRenderQueued_) {
        displayedRender_ = pendingRender_;
        pendingRender_ = {};
        pendingRenderQueued_ = false;
    }

    const bool advance = lifecycle_ == Lifecycle::Initialized ||
                         lifecycle_ == Lifecycle::Running;
    if (!enabled_) return false;
    if (lifecycle_ == Lifecycle::Unallocated) {
        beginFrame(sourceX, sourceFlag);
        return false;
    }
    if (lifecycle_ == Lifecycle::Allocated) {
        initializePending();
        return false;
    }
    return advance;
}

void StormEaglePlatformScene::initializePending() {
    if (!enabled_ || lifecycle_ != Lifecycle::Allocated) return;

    // fromSourceFlag is the measured source initializer.  This seam does not
    // add a float/fraction setter or duplicate the R92 state tables.
    platform_ = StormEaglePlatform::fromSourceFlag(sourceFlag_);
    lifecycle_ = Lifecycle::Initialized;
}

void StormEaglePlatformScene::tickMotion() {
    if (!enabled_ || (lifecycle_ != Lifecycle::Initialized &&
                     lifecycle_ != Lifecycle::Running)) {
        return;
    }

    previousRenderState_ = platform_.state();
    platform_.tickMotion();
    currentRenderState_ = platform_.state();
    hasCompletedRenderState_ = true;
    lifecycle_ = Lifecycle::Running;
}

void StormEaglePlatformScene::finishSourceFrame(std::uint16_t cameraX,
                                                std::uint16_t cameraY) {
    if (!enabled_ || lifecycle_ != Lifecycle::Running ||
        !hasCompletedRenderState_) {
        return;
    }

    pendingRender_.state = currentRenderState_;
    if (insideSourceCull(currentRenderState_, cameraX, cameraY)) {
        animation_.tick();
        pendingRender_.pose = animation_.pose();
        pendingRender_.valid = true;
    } else {
        // Motion remains source-live outside the window, but contact,
        // animation, and renderer queue validity do not.
        pendingRender_.valid = false;
    }
    pendingRenderQueued_ = true;
}

void StormEaglePlatformScene::render(float cameraX, float cameraY, float alpha) const {
    (void)alpha;
    if (!displayedRender_.valid || !texture_ || !texture_->valid()) return;
    const Rectangle source{
        static_cast<float>(displayedRender_.pose * 32u), 0.0f, 32.0f, 21.0f};
    const Rectangle destination{
        std::round(static_cast<float>(displayedRender_.state.x) - cameraX - 16.0f),
        std::round(static_cast<float>(displayedRender_.state.y) - cameraY - 16.0f),
        32.0f, 21.0f};
    DrawTexturePro(texture_->get(), source, destination,
                   {0.0f, 0.0f}, 0.0f, WHITE);
}

bool StormEaglePlatformScene::resolvePlayerContact(Player& player,
                                                    std::uint16_t cameraX,
                                                    std::uint16_t cameraY) {
    if (!enabled_ || lifecycle_ != Lifecycle::Running ||
        !hasCompletedRenderState_) {
        playerSupportLatched_ = false;
        return false;
    }

    // 82:806E culls before contact/animation/queue.  The per-record latch is
    // deliberately retained outside this window, while the aggregate Player
    // support result expires there.  This first native seam covers the
    // primary measured top contact only; secondary/wall masks and hazards are
    // not inferred from the four accepted rows.
    const bool inside = insideSourceCull(currentRenderState_, cameraX, cameraY);
    if (!inside) {
        playerSupportLatched_ = false;
        return false;
    }

    // Source D+2C carry applies before the current profile comparison.  Add
    // integer source-word deltas to native floats so the Player fraction is
    // preserved.  First contact has no previous record latch and therefore
    // receives no Y snap.
    const bool carryRecord = recordSupportLatched_ && !player.onGround;
    recordSupportLatched_ = false;
    if (carryRecord) {
        // Carry uses the physical state copied by the source motion wrapper,
        // not a renderer snapshot that may already be one phase behind.
        player.position.x += static_cast<float>(signedWordDelta(
            currentRenderState_.x, currentRenderState_.previousX));
        player.position.y += static_cast<float>(signedWordDelta(
            currentRenderState_.y, currentRenderState_.previousY));
    }

    // Accepted ROM profiles: player 00 FF 06 0E and platform 00 F3 0F 06.
    // Their signed offsets produce sums 21 (X) and 20 (Y).  The source uses
    // inclusive overlap, so distance == extent remains contact.
    constexpr int kPlayerOffsetX = 0;
    constexpr int kPlayerOffsetY = -1;
    constexpr int kPlayerWidth = 6;
    constexpr int kPlayerHeight = 14;
    constexpr int kPlatformOffsetX = 0;
    constexpr int kPlatformOffsetY = -13;
    constexpr int kPlatformWidth = 15;
    constexpr int kPlatformHeight = 6;

    const int playerCenterX = sourcePlayerX(player) + kPlayerOffsetX;
    const int playerCenterY = sourcePlayerY(player) + kPlayerOffsetY;
    const int platformCenterX = static_cast<int>(currentRenderState_.x) +
                                kPlatformOffsetX;
    const int platformCenterY = static_cast<int>(currentRenderState_.y) +
                                kPlatformOffsetY;
    const int horizontalDistance = std::abs(platformCenterX - playerCenterX);
    const int verticalDistance = std::abs(platformCenterY - playerCenterY);
    const int horizontalOverlap = kPlayerWidth + kPlatformWidth -
                                  horizontalDistance + 1;
    const int verticalOverlap = kPlayerHeight + kPlatformHeight -
                                verticalDistance + 1;
    const bool verticalAxis = horizontalOverlap >= verticalOverlap;
    const bool overlapping = horizontalOverlap >= 1 && verticalOverlap >= 1;
    const bool topContact = platformCenterY >= playerCenterY && verticalAxis;
    const bool contact = topContact && overlapping;

    // T1.7e: the same profile pair resolved the other way up. On source frame
    // 1392 of the Storm Eagle movie X's rising profile reaches overlapY 1
    // against the record at $7E16E8 (573,1488 against 560,1480) and the source
    // zeroes his rise for 1393 without moving him and without supporting him:
    // he then falls from rest. A downward velocity is left alone.
    if (overlapping && verticalAxis && platformCenterY < playerCenterY &&
        player.velocity.y < 0.0f) {
        player.velocity.y = 0.0f;
        // T1.7: and the frame after it does not move him. This seam runs in
        // the post-physics object lane, so the zero it writes is still worth
        // one accumulation on the next moveAndCollide unless it is spent
        // here; the source's f1393 dy is 0 and its f1394 dy is +64/256.
        player.gravityStepSpent = true;
    }

    // The source top-contact correction is -overlap+1, capped at -8.  The
    // measured first contact has overlap=1 and therefore keeps its native
    // fraction/position; deeper overlaps are corrected without a full snap.
    if (contact) {
        const int verticalCorrection = std::max(-verticalOverlap + 1, -8);
        player.position.y += static_cast<float>(verticalCorrection);
    }

    // This is the measured primary top-contact boundary. First contact has
    // zero correction; deeper overlap uses the bounded source correction
    // above rather than snapping to the platform.
    recordSupportLatched_ = contact;
    playerSupportLatched_ = contact;
    return contact;
}

}  // namespace mmx
