#pragma once

#include "entities/enemy.h"
#include "entities/player.h"
#include "entities/player_anchor.h"

#include <cmath>
#include <cstdint>
#include <optional>

// OID49 body contact: knowledge_base/mmx1/enemies/oid_0x49/player_contact.json.
// This profile is independent of the render/physics box and shot vulnerability.
namespace mmx::gameplay_enemy_contact {

struct SourceContactResult {
    bool sourceSupported = false;
    bool hit = false;
};

struct SourceProfile {
    std::int8_t offsetX;
    std::int8_t offsetY;
    std::uint8_t halfExtentX;
    std::uint8_t halfExtentY;
};

inline std::optional<SourceProfile> playerProfile(const Player& player) {
    const auto profile = player.sourceContactProfile();
    if (!profile) return std::nullopt;
    switch (*profile) {
        case SourceContactProfile::NormalA552:
            return SourceProfile{0, -1, 6, 14}; // $86:A552 = 00 FF 06 0E
        case SourceContactProfile::ActiveDashBB38:
            return SourceProfile{0, 5, 6, 8}; // $86:BB38 = 00 05 06 08
    }
    return std::nullopt;
}

inline std::uint16_t sourceWord(float value) {
    return static_cast<std::uint16_t>(static_cast<std::int64_t>(std::floor(value)));
}

inline std::uint16_t sourceWordDistance(std::uint16_t lhs, std::uint16_t rhs) {
    auto distance = static_cast<std::uint16_t>(lhs - rhs);
    if ((distance & 0x8000u) != 0) {
        distance = static_cast<std::uint16_t>(0u - distance);
    }
    return distance;
}

inline bool inclusiveAxisOverlap(std::uint16_t distance,
                                 std::uint8_t playerHalfExtent,
                                 std::uint8_t enemyHalfExtent) {
    // SEC/SBC carry survives the source's following INC: equality is contact.
    const auto extent = static_cast<std::uint8_t>(playerHalfExtent + enemyHalfExtent);
    return distance <= extent;
}

// standing_log_contact.json: C0CE corrects the Player's integer X word in
// the observed right-facing approach to the first CP remnant. Its fraction,
// velocity and the separate BD4/contact-flag lane are not written here.
inline std::optional<int> resolveSourceAxeMaxLogHorizontal(
    const Player& player, Vector2 sourceLogAnchor) {
    const auto selected = player.sourceContactProfile();
    if (!player.facingRight || !selected ||
        *selected != SourceContactProfile::NormalA552) return std::nullopt;
    const auto playerBox = playerProfile(player);
    if (!playerBox) return std::nullopt;

    constexpr SourceProfile logBox{0, -4, 10, 7}; // $86:C0CE = 00 FC 0A 07
    const auto shifted = [](std::uint16_t word, std::int8_t offset) {
        return static_cast<std::uint16_t>(static_cast<std::int32_t>(word) + offset);
    };
    const auto playerX = shifted(sourceWord(player_anchor::sourceRamAnchorX(
        player.position.x, player.spriteWidth, player.facingRight)), playerBox->offsetX);
    const auto playerY = shifted(sourceWord(player.position.y + 40.0f), playerBox->offsetY);
    const auto logX = shifted(sourceWord(sourceLogAnchor.x), logBox.offsetX);
    const auto logY = shifted(sourceWord(sourceLogAnchor.y), logBox.offsetY);
    const auto signedX = static_cast<std::uint16_t>(playerX - logX);
    // The retained contact witnesses approach from the log's left side.
    if ((signedX & 0x8000u) == 0) return std::nullopt;
    const auto dx = sourceWordDistance(playerX, logX);
    const auto dy = sourceWordDistance(playerY, logY);
    if (!inclusiveAxisOverlap(dx, playerBox->halfExtentX, logBox.halfExtentX) ||
        !inclusiveAxisOverlap(dy, playerBox->halfExtentY, logBox.halfExtentY)) {
        return std::nullopt;
    }
    const int overlapX = playerBox->halfExtentX + logBox.halfExtentX - dx + 1;
    const int overlapY = playerBox->halfExtentY + logBox.halfExtentY - dy + 1;
    if (overlapX > overlapY) return std::nullopt; // $81B2E1..E9 chooses X on a tie.
    return -overlapX;
}

inline SourceContactResult resolveSourcePlayerEnemyContact(
    const Player& player, const Enemy& enemy) {
    if (enemy.type != "se_drone" || enemy.behavior != EnemyBehavior::HoverPatrol) {
        return {};
    }
    const auto playerBox = playerProfile(player);
    if (!playerBox) return {};

    constexpr SourceProfile enemyBox{0, -13, 7, 6}; // $86:D2E8 = 00 F3 07 06
    const auto playerX = sourceWord(player_anchor::sourceRamAnchorX(
        player.position.x, player.spriteWidth, player.facingRight));
    const auto playerY = sourceWord(player.position.y + 40.0f);
    const auto enemyX = sourceWord(enemy.position.x);
    const auto enemyY = sourceWord(enemy.position.y);
    const auto shifted = [](std::uint16_t word, std::int8_t offset) {
        return static_cast<std::uint16_t>(static_cast<std::int32_t>(word) + offset);
    };
    const auto dx = sourceWordDistance(shifted(playerX, playerBox->offsetX),
                                       shifted(enemyX, enemyBox.offsetX));
    const auto dy = sourceWordDistance(shifted(playerY, playerBox->offsetY),
                                       shifted(enemyY, enemyBox.offsetY));
    return {true, inclusiveAxisOverlap(dx, playerBox->halfExtentX, enemyBox.halfExtentX) &&
                  inclusiveAxisOverlap(dy, playerBox->halfExtentY, enemyBox.halfExtentY)};
}

} // namespace mmx::gameplay_enemy_contact
