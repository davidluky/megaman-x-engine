// Resolution against the existing Axe Max launcher/log envelope.
// The scene adapter owns stage/enemy selection; this header keeps the
// movement contract headless and directly testable.

#pragma once

#include "entities/enemy.h"
#include "entities/entity.h"
#include "entities/player.h"
#include "gameplay/gameplay_enemy_contact.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace mmx::axemax_stack_collision {

struct HorizontalResolution {
    float playerPositionX = 0.0f;
    bool touchingWallLeft = false;
    bool touchingWallRight = false;
};

struct VerticalResolution {
    float playerPositionY;
    bool onGround;
    bool onCeiling;
};

inline std::optional<VerticalResolution> resolveVertical(
    const AABB& previousBox, const AABB& playerBox, const AABB& blocker,
    float playerHitboxOffsetY, float velocityY) {
    if (blocker.w <= 0 || blocker.h <= 0 ||
        playerBox.right() <= blocker.left() || playerBox.left() >= blocker.right()) {
        return std::nullopt;
    }
    // R246: choose the crossed face at the time of contact. Requiring
    // previous overlap rejects valid corner landings; testing only the end
    // position incorrectly lifts a late side entry onto the magazine.
    const auto supportedAt = [&](float t) {
        const float x = previousBox.x + (playerBox.x - previousBox.x) * t;
        return x + playerBox.w > blocker.left() && x < blocker.right();
    };
    if (velocityY >= 0 && previousBox.bottom() <= blocker.top() &&
        playerBox.bottom() >= blocker.top()) {
        const float dy = playerBox.bottom() - previousBox.bottom();
        if (!supportedAt(dy > 0 ? (blocker.top() - previousBox.bottom()) / dy : 1.0f)) {
            return std::nullopt;
        }
        return VerticalResolution{
            blocker.top() - playerHitboxOffsetY - playerBox.h, true, false};
    }
    if (velocityY < 0 && previousBox.top() >= blocker.bottom() &&
        playerBox.top() <= blocker.bottom()) {
        const float dy = playerBox.top() - previousBox.top();
        if (!supportedAt(dy < 0 ? (blocker.bottom() - previousBox.top()) / dy : 1.0f)) {
            return std::nullopt;
        }
        return VerticalResolution{blocker.bottom() - playerHitboxOffsetY, false, true};
    }
    return std::nullopt;
}

inline std::optional<HorizontalResolution> resolveHorizontal(
    const AABB& playerBox, const AABB& blocker, float playerHitboxOffsetX) {
    if (blocker.w <= 0.0f || blocker.h <= 0.0f ||
        !playerBox.overlaps(blocker)) {
        return std::nullopt;
    }

    const float playerCenter = playerBox.x + playerBox.w * 0.5f;
    const float blockerCenter = blocker.x + blocker.w * 0.5f;
    if (playerCenter <= blockerCenter) {
        return HorizontalResolution{
            blocker.left() - playerHitboxOffsetX - playerBox.w,
            false,
            true,
        };
    }
    return HorizontalResolution{
        blocker.right() - playerHitboxOffsetX,
        true,
        false,
    };
}

// The per-enemy scene pass, headless so the contract drives the same code the
// GameplayScene adapter runs.
//
// Two blocker sources:
// - Measured (launcher_solid_box_2026-09-15.json, T1.7g): the CP remnant's
//   records each carry $86:C0CE and $86:C0D2 in the contact list, but only
//   C0CE is a solid box here. The committed source corrections prove it:
//   standing_log_contact.json f1533 moves X 454 -> 452 (C0CE overlap 2); a
//   C0D2 push (half extent 13, overlap 5) would end at 449, and the T1.7
//   f1537..f1541 rows likewise match C0CE alone. C0D2's own effect (the BD4
//   contact lane) is not a position correction and is not modelled here.
//   X's box is his $86:A552 / $86:BB38 source profile at the source anchor
//   (player_anchor::sourceRamAnchorX, position.y + 40).
// - Unmeasured placements (test map, R228 fixtures, other stock states):
//   the draw-built column and the engine hitbox, unchanged.
inline void resolvePlayerStack(Player& player, const Enemy& enemy) {
    std::vector<AABB> blockers;
    const auto sourceBoxes = enemy.axeStackSourceContactBoxes();
    for (const auto& source : sourceBoxes) {
        if (source.descriptor == 0xC0CE) blockers.push_back(source.box);
    }
    const auto sourceProfile = gameplay_enemy_contact::playerProfile(player);
    const bool sourceContact = !blockers.empty() && sourceProfile.has_value();
    if (sourceBoxes.empty()) {
        const auto envelope = enemy.solidAxeStackHitbox();
        if (envelope.w <= 0.0f || envelope.h <= 0.0f) return;
        blockers.push_back(envelope);
    }

    const auto boxAt = [&](Vector2 position) {
        if (!sourceContact) {
            return AABB{position.x + player.hitboxOffset.x,
                        position.y + player.hitboxOffset.y,
                        player.hitboxSize.x, player.hitboxSize.y};
        }
        const float anchorX = std::floor(player_anchor::sourceRamAnchorX(
            position.x, player.spriteWidth, player.facingRight));
        const float anchorY = std::floor(position.y + 40.0f);
        const auto& p = *sourceProfile;
        return AABB{anchorX + static_cast<float>(p.offsetX - p.halfExtentX),
                    anchorY + static_cast<float>(p.offsetY - p.halfExtentY),
                    static_cast<float>(2 * p.halfExtentX + 1),
                    static_cast<float>(2 * p.halfExtentY + 1)};
    };

    // Measured contacts choose the least penetration (horizontal on a tie).
    // Generic envelopes retain their crossed-face vertical resolution.
    bool vertical = false;
    for (const auto& blocker : blockers) {
        AABB playerBox = boxAt(player.position);
        // Inclusive source contact is a strict overlap of the widened boxes;
        // a touching edge is one pixel clear (CP f1557).
        const AABB previousBox = boxAt(player.prevPosition);
        const bool wasSupported = sourceContact && player.velocity.y >= 0.0f &&
            previousBox.bottom() - 1.0f == blocker.top() &&
            previousBox.right() > blocker.left() && previousBox.left() < blocker.right();
        if (wasSupported) {
            // The ground caller does not integrate synthetic tile gravity.
            player.position.y = player.prevPosition.y;
            player.velocity.y = 0.0f;
            playerBox = boxAt(player.position);
        }
        if (sourceContact && playerBox.overlaps(blocker)) {
            const float left = playerBox.right() - blocker.left();
            const float right = blocker.right() - playerBox.left();
            const float top = playerBox.bottom() - blocker.top();
            const float bottom = blocker.bottom() - playerBox.top();
            if (std::min(left, right) <= std::min(top, bottom)) {
                // edge_axis_transition_2026-09-17.json, B2E4..B313: only
                // integer X changes. Losing support dispatches Fall next tick.
                player.position.x += left <= right ? -left : right;
                player.onGround = false;
                if (wasSupported) player.queueSourceObjectDeparture();
                return;
            }
        }
        if (sourceContact && !playerBox.overlaps(blocker)) {
            // The last supported step spends no gravity even if horizontal
            // movement leaves the inclusive edge (CP f1570). It does not
            // remain grounded; the following update begins the fall.
            if (player.velocity.y >= 0.0f &&
                previousBox.bottom() - 1.0f == blocker.top() &&
                previousBox.right() > blocker.left() &&
                previousBox.left() < blocker.right()) {
                player.position.y = player.prevPosition.y;
                player.velocity.y = 0.0f;
                player.onGround = false;
                // CP2111: lost support dispatches Fall without moving that tick.
                // player/stack_nonoverlap_departure_2026-09-17.json.
                player.queueSourceObjectDeparture();
                vertical = true;
            }
            continue;
        }
        // T1.7h, standing_support_2026-09-17.json: source support is the
        // inclusive bottom pixel, not the expanded AABB's exclusive edge.
        // Preserve the landing fraction; existing support retains the previous
        // fixed-point height rather than spending tile gravity on every tick.
        if (sourceContact && player.velocity.y >= 0.0f &&
            previousBox.bottom() - 1.0f <= blocker.top() &&
            playerBox.bottom() - 1.0f >= blocker.top()) {
            player.position.y = previousBox.bottom() - 1.0f == blocker.top()
                ? player.prevPosition.y
                : player.position.y - (playerBox.bottom() - 1.0f - blocker.top());
            player.prevPosition.y = player.position.y;
            player.velocity.y = 0.0f;
            player.onGround = true;
            player.onCeiling = false;
            vertical = true;
            continue;
        }
        if (const auto resolved = resolveVertical(
                previousBox, playerBox, blocker, playerBox.y - player.position.y,
                player.velocity.y)) {
            player.position.y = resolved->playerPositionY;
            player.prevPosition.y = player.position.y;
            player.velocity.y = 0;
            player.onGround = resolved->onGround;
            player.onCeiling = resolved->onCeiling;
            vertical = true;
        }
    }
    if (vertical) return;
    if (const auto sourceLog = enemy.cpRemnantLogSourceAnchor()) {
        // Both stocked logs carry the box (T1.7 f1537, 2026-09-15): X's
        // mid-ascent leaves the lower log's box at source frame 1537 and
        // the source keeps correcting his integer x -2, -1, -2, -1 through
        // f1540 against the upper one. One correction per frame - the
        // source resolves a single contact, so the first box that reaches
        // X owns the frame.
        const std::optional<Vector2> logs[2] = {
            sourceLog, enemy.cpRemnantUpperLogSourceAnchor()};
        for (const auto& log : logs) {
            if (!log) continue;
            if (const auto correction =
                    gameplay_enemy_contact::resolveSourceAxeMaxLogHorizontal(
                        player, *log)) {
                // Source STA81B313 changes only integer X. Preserve velocity,
                // interpolation and existing wall flags.
                player.position.x += static_cast<float>(*correction);
                break;
            }
        }
    }
    for (const auto& blocker : blockers) {
        const AABB playerBox = boxAt(player.position);
        const auto resolution =
            resolveHorizontal(playerBox, blocker, playerBox.x - player.position.x);
        if (!resolution.has_value()) continue;
        player.position.x = resolution->playerPositionX;
        player.touchingWallLeft = resolution->touchingWallLeft;
        player.touchingWallRight = resolution->touchingWallRight;
        player.velocity.x = 0.0f;
        // The collision follows the normal tile pass. Keep interpolation from
        // drawing one transient frame through the stack before the snap-back.
        player.prevPosition.x = player.position.x;
    }
}

} // namespace mmx::axemax_stack_collision
