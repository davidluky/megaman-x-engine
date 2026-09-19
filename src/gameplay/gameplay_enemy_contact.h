#pragma once

#include "entities/enemy.h"
#include "entities/player.h"
#include "entities/player_anchor.h"
#include "entities/projectile.h"

#include <cmath>
#include <array>
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

inline std::optional<SourceProfile> stretchBirdHeadProfile(const Enemy& enemy) {
    if (enemy.type != "stretch_bird" || enemy.behavior != EnemyBehavior::Anchored) return std::nullopt;
    // head_launch.json: C559 + 2*(control & 3F), four bytes per descriptor.
    constexpr std::array<SourceProfile, 9> heads{{
        {-1,-22,5,14}, {-13,-12,7,8}, {-17,1,12,6},
        {-13,18,10,7}, {1,24,6,8}, {15,15,5,7},
        {16,-2,9,5}, {13,-14,7,7}, {-1,-15,7,10}
    }};
    const auto control = enemy.stretchBirdState().animation.current().control & 0x3Fu;
    if ((control & 1u) || control / 2 >= heads.size()) return std::nullopt;
    auto head = heads[control / 2];
    if (!enemy.facingRight) head.offsetX = static_cast<std::int8_t>(-head.offsetX);
    return head;
}

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

inline SourceContactResult resolveSourceLogPlayerContact(const Projectile& shot, const Player& player) {
    const auto profile = playerProfile(player);
    if (!shot.sourceAxeMaxLog || !profile) return {};
    // C0D6=(0,0,13,6); contact reads live integer anchors, not sprite edges.
    const auto box = shot.getHitbox();
    const auto px = sourceWord(player_anchor::sourceRamAnchorX(
        player.position.x, player.spriteWidth, player.facingRight) + profile->offsetX);
    const auto py = sourceWord(player.position.y + 40 + profile->offsetY);
    return {true,
        inclusiveAxisOverlap(sourceWordDistance(px, sourceWord(box.x + box.w/2)),
                             profile->halfExtentX, 13) &&
        inclusiveAxisOverlap(sourceWordDistance(py, sourceWord(box.y + box.h/2)),
                             profile->halfExtentY, 6)};
}

inline SourceContactResult resolveSourceBusterEnemyContact(const Projectile& projectile, const Enemy& enemy) {
    const auto charge = projectile.sourceChargeHitProfile();
    if (charge && projectile.sourceCollisionCenterOffset && enemy.hasSourceWalker()) {
        // charge_l1_contact_phase_2026-09-17.json: compare live integer words.
        const auto center = *projectile.sourceCollisionCenterOffset;
        const auto px = sourceWord(projectile.position.x + center.x +
            (projectile.facingRight ? charge->offsetX : -charge->offsetX));
        const auto py = sourceWord(projectile.position.y + center.y + charge->offsetY);
        return {true,
            inclusiveAxisOverlap(sourceWordDistance(px, sourceWord(enemy.position.x)),
                                 charge->halfExtentX, 12) &&
            inclusiveAxisOverlap(sourceWordDistance(py, sourceWord(enemy.position.y - 1)),
                                 charge->halfExtentY, 11)};
    }
    if (!projectile.sourceCollisionCenterOffset || !projectile.isPlayerShot ||
        projectile.weaponId != "buster" || projectile.type != ProjectileType::Normal) return {};
    const auto head = stretchBirdHeadProfile(enemy);
    if (!head) return {};
    const auto center = *projectile.sourceCollisionCenterOffset;
    const auto px = sourceWord(projectile.position.x + center.x);
    const auto py = sourceWord(projectile.position.y + center.y);
    const auto overlaps = [&](SourceProfile profile) {
        const auto x = static_cast<std::uint16_t>(sourceWord(enemy.position.x) + profile.offsetX);
        const auto y = static_cast<std::uint16_t>(sourceWord(enemy.position.y) + profile.offsetY);
        // BF68 = 00 00 07 07, independently of the normal shot's visual bounds.
        return inclusiveAxisOverlap(sourceWordDistance(px,x),7,profile.halfExtentX)
            && inclusiveAxisOverlap(sourceWordDistance(py,y),7,profile.halfExtentY);
    };
    return {true, overlaps(*head) || overlaps({static_cast<std::int8_t>(enemy.facingRight ? 1 : -1),14,5,22})};
}

inline std::optional<int> sourceBusterEnemyDamage(const Projectile& projectile, const Enemy& enemy) {
    // walker_release_2026-09-17.json: OID1 reads $86:EF7E=4. Other charges differ.
    if (enemy.hasSourceWalker() && projectile.sourceCollisionCenterOffset &&
        projectile.sourceChargeHitProfile()) return 4;
    return std::nullopt;
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
    const bool drone = enemy.type == "se_drone" &&
        enemy.behavior == EnemyBehavior::HoverPatrol;
    const bool flamingle = enemy.type == "stretch_bird" &&
        enemy.behavior == EnemyBehavior::Anchored;
    const bool walker = enemy.hasSourceWalker();
    if (!drone && !flamingle && !walker) {
        return {};
    }
    const auto playerBox = playerProfile(player);
    if (!playerBox) return {};

    const auto playerX = sourceWord(player_anchor::sourceRamAnchorX(
        player.position.x, player.spriteWidth, player.facingRight));
    const auto playerY = sourceWord(player.position.y + 40.0f);
    const auto enemyX = sourceWord(enemy.position.x);
    const auto enemyY = sourceWord(enemy.position.y);
    const auto shifted = [](std::uint16_t word, std::int8_t offset) {
        return static_cast<std::uint16_t>(static_cast<std::int32_t>(word) + offset);
    };
    const auto overlaps = [&](SourceProfile enemyBox) {
        const auto dx = sourceWordDistance(shifted(playerX, playerBox->offsetX),
                                           shifted(enemyX, enemyBox.offsetX));
        const auto dy = sourceWordDistance(shifted(playerY, playerBox->offsetY),
                                           shifted(enemyY, enemyBox.offsetY));
        return inclusiveAxisOverlap(dx, playerBox->halfExtentX, enemyBox.halfExtentX) &&
               inclusiveAxisOverlap(dy, playerBox->halfExtentY, enemyBox.halfExtentY);
    };
    if (drone) return {true, overlaps({0,-13,7,6})}; // $86:D2E8
    // walker_player_contact_2026-09-17.json: executed D3F8 vs A552 comparison.
    if (walker) return {true, overlaps({0,-1,12,11})};
    const auto head = stretchBirdHeadProfile(enemy);
    if (!head) return {};
    SourceProfile body{1,14,5,22};
    if (!enemy.facingRight) {
        body.offsetX = static_cast<std::int8_t>(-body.offsetX);
    }
    // 81:C094 evaluates the selected head, then 81:C0CD evaluates C555.
    // C551 is a separate proximity trigger; never use it as a hurtbox.
    return {true, overlaps(*head) || overlaps(body)};
}

} // namespace mmx::gameplay_enemy_contact
