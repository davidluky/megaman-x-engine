#pragma once

#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

namespace mmx::gameplay_buster_impact {

// Runtime adapters supply these already-resolved collision facts so this
// source-measured presentation model does not depend on Projectile or Raylib.
struct HitEligibility {
    bool chargeL1 = false;
    std::string_view weaponId;
    bool damagedEnemy = false;
    bool killedEnemy = false;
};

inline bool isEligible(const HitEligibility& hit) {
    return hit.chargeL1 && hit.weaponId == "buster" && hit.damagedEnemy &&
           !hit.killedEnemy;
}

struct NormalBusterLethalContactEligibility {
    bool normalShot = false;
    std::string_view weaponId;
    bool damagedEnemy = false;
    bool killedEnemy = false;
};

inline bool isNormalBusterLethalContactResidueEligible(
    const NormalBusterLethalContactEligibility& hit) {
    return hit.normalShot && hit.weaponId == "buster" && hit.damagedEnemy &&
           hit.killedEnemy;
}

struct NormalBusterSurvivorContactEligibility {
    bool normalShot = false;
    std::string_view weaponId;
    bool damagedEnemy = false;
    bool killedEnemy = false;
};

inline bool isNormalBusterSurvivorContactResidueEligible(
    const NormalBusterSurvivorContactEligibility& hit) {
    return hit.normalShot && hit.weaponId == "buster" && hit.damagedEnemy &&
           !hit.killedEnemy;
}

struct NormalBusterContactPreludeEligibility {
    bool normalShot = false;
    std::string_view weaponId;
    bool damagedEnemy = false;
    bool killedEnemy = false;
};

inline bool isNormalBusterContactPreludeEligible(
    const NormalBusterContactPreludeEligibility& hit) {
    return hit.normalShot && hit.weaponId == "buster" && hit.damagedEnemy;
}

struct ProjectileBounds {
    float left = 0.0f;
    float top = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    bool facingRight = true;
};

struct TargetBounds {
    float left = 0.0f;
    float top = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

enum class ImpactKind {
    ChargeL1Nonlethal,
    NormalBusterContactPrelude,
    NormalBusterLethalContactResidue,
    NormalBusterSurvivorContactResidue,
};

enum class NormalBusterContactBranch {
    Survived,
    Killed,
};

enum class PriorityOcclusionProfile {
    None,
    ChillPenguinOpeningSnowEdge,
};

inline PriorityOcclusionProfile sourcePriorityOcclusionProfile(
    std::string_view stageId,
    std::string_view visualSectionId,
    const TargetBounds& target) {
    const float deathAnchorX = target.left + target.width * 0.5f;
    const float deathAnchorY = target.top + target.height * 0.5f - 4.0f;
    if (stageId == "chill-penguin" &&
        visualSectionId == "cp_opening_backdrop" &&
        deathAnchorX == 525.0f && deathAnchorY == 1147.25f) {
        return PriorityOcclusionProfile::ChillPenguinOpeningSnowEdge;
    }
    return PriorityOcclusionProfile::None;
}

struct Impact {
    float anchorX = 0.0f;
    float anchorY = 0.0f;
    int age = 0;
    bool mirror = false;
    int sourceProjectileSerial = 0;
    int targetSerial = 0;
    ImpactKind kind = ImpactKind::ChargeL1Nonlethal;
    PriorityOcclusionProfile priorityOcclusion = PriorityOcclusionProfile::None;
    bool atContactPoint = false;
};

struct SourceSnowPriorityClip {
    int fullRowsBottomYExclusive = 0;
    int firstEdgeRightXExclusive = 0;
    int secondEdgeRightXExclusive = 0;
    bool hasSinglePixelAperture = false;
    int singlePixelApertureX = 0;
    int singlePixelApertureY = 0;
};

struct ImpactDraw {
    int cell = 0;
    int sourceX = 0;
    int sourceY = 0;
    int sourceWidth = 0;
    int sourceHeight = 0;
    float topLeftX = 0.0f;
    float topLeftY = 0.0f;
    bool mirror = false;
    std::optional<SourceSnowPriorityClip> sourceSnowPriorityClip;
};

inline Impact spawn(const ProjectileBounds& projectile,
                    int sourceProjectileSerial = 0,
                    int targetSerial = 0) {
    // +7 is source-measured for the right-facing hit. The -7 branch is the
    // same explicit symmetric inference as draw mirroring (FW7-REVIEW-D).
    constexpr float kFacingAnchorOffsetX = 7.0f;
    constexpr float kAnchorOffsetY = 4.0f;
    const float centerX = projectile.left + projectile.width * 0.5f;
    const float centerY = projectile.top + projectile.height * 0.5f;
    return {
        centerX + (projectile.facingRight ? kFacingAnchorOffsetX
                                         : -kFacingAnchorOffsetX),
        centerY + kAnchorOffsetY,
        0,
        !projectile.facingRight,
        sourceProjectileSerial,
        targetSerial,
        ImpactKind::ChargeL1Nonlethal,
    };
}

inline Impact spawnNormalBusterContactPrelude(
    const TargetBounds& target,
    NormalBusterContactBranch branch,
    int sourceProjectileSerial = 0,
    int targetSerial = 0) {
    // The authenticated prelude is target-owned. It shares the already-proven
    // survivor/lethal target anchors, then renders cell 0 at (-28,+16) for the
    // collision tick only. The consumed projectile is never retained.
    constexpr float kSurvivorAnchorOffsetX = 1.0f;
    constexpr float kAnchorOffsetY = -4.0f;
    return {
        target.left + target.width * 0.5f +
            (branch == NormalBusterContactBranch::Survived
                 ? kSurvivorAnchorOffsetX
                 : 0.0f),
        target.top + target.height * 0.5f + kAnchorOffsetY,
        0,
        false,
        sourceProjectileSerial,
        targetSerial,
        ImpactKind::NormalBusterContactPrelude,
    };
}

inline Impact spawnNormalBusterLethalContactResidue(
    const TargetBounds& target,
    int sourceProjectileSerial = 0,
    int targetSerial = 0,
    PriorityOcclusionProfile priorityOcclusion = PriorityOcclusionProfile::None) {
    // The residue is attached to the enemy death anchor: target center, four
    // pixels above its hitbox center. Unlike the moving ChargeL1 comet, this
    // contact residue is target-owned and has no directional mirror branch.
    constexpr float kDeathAnchorOffsetY = -4.0f;
    Impact impact{
        target.left + target.width * 0.5f,
        target.top + target.height * 0.5f + kDeathAnchorOffsetY,
        0,
        false,
        sourceProjectileSerial,
        targetSerial,
        ImpactKind::NormalBusterLethalContactResidue,
    };
    impact.priorityOcclusion = priorityOcclusion;
    return impact;
}

inline Impact spawnNormalBusterSurvivorContactResidue(
    const TargetBounds& target,
    int sourceProjectileSerial = 0,
    int targetSerial = 0,
    PriorityOcclusionProfile priorityOcclusion = PriorityOcclusionProfile::None) {
    // Source (+0.5,-4.25) is raster-equivalent to this route-bounded integer
    // adapter. The fixed residue is target-owned and does not retain or move
    // the consumed gameplay projectile.
    constexpr float kAnchorOffsetX = 1.0f;
    constexpr float kAnchorOffsetY = -4.0f;
    Impact impact{
        target.left + target.width * 0.5f + kAnchorOffsetX,
        target.top + target.height * 0.5f + kAnchorOffsetY,
        0,
        false,
        sourceProjectileSerial,
        targetSerial,
        ImpactKind::NormalBusterSurvivorContactResidue,
    };
    impact.priorityOcclusion = priorityOcclusion;
    return impact;
}

// R282/R286 playable review: the target-owned offsets above authenticate a
// particular source layout. Elsewhere, keep the same art/cadence at the
// resolved horizontal Buster contact, including rear and high/low hits.
inline void placeAtProjectileContact(Impact& impact,
                                     const ProjectileBounds& projectile,
                                     const TargetBounds& target) {
    impact.anchorX = projectile.facingRight ? target.left : target.left + target.width;
    impact.anchorY = std::clamp(projectile.top + projectile.height * 0.5f,
                               target.top, target.top + target.height);
    impact.mirror = !projectile.facingRight;
    impact.atContactPoint = true;
}

inline int retirementAge(const ImpactKind kind) {
    switch (kind) {
    case ImpactKind::ChargeL1Nonlethal:
        return 11;
    case ImpactKind::NormalBusterContactPrelude:
        return 1;
    case ImpactKind::NormalBusterLethalContactResidue:
    case ImpactKind::NormalBusterSurvivorContactResidue:
        return 9;
    }
    return 0;
}

// Called before collision resolution. Impacts spawned by later collision work
// therefore enter the queue at age zero and advance independently next tick.
inline void updateBeforeCollisions(std::vector<Impact>& impacts) {
    for (auto& impact : impacts) {
        ++impact.age;
    }
    impacts.erase(
        std::remove_if(impacts.begin(), impacts.end(),
                       [](const Impact& impact) {
                           return impact.age >= retirementAge(impact.kind);
                       }),
        impacts.end());
}

inline int cellForAge(int age) {
    if (age == 0) return 2;
    if (age == 1 || age == 2 || (age >= 7 && age <= 10)) return 0;
    if (age >= 3 && age <= 6) return 1;
    return -1;
}

inline int normalContactCellForAge(int age) {
    if (age == 1) return 0;
    if (age >= 2 && age <= 5) return 1;
    if (age == 6 || age == 7) return 2;
    if (age == 8) return 3;
    return -1;
}

inline int cellFor(const Impact& impact) {
    if (impact.kind == ImpactKind::ChargeL1Nonlethal) {
        return cellForAge(impact.age);
    }
    if (impact.kind == ImpactKind::NormalBusterContactPrelude) {
        return impact.age == 0 ? 0 : -1;
    }
    return normalContactCellForAge(impact.age);
}

template <typename WriteRow>
inline void writeParityTraceRows(const std::vector<Impact>& impacts,
                                 WriteRow& writeRow) {
    for (const auto& impact : impacts) {
        const int cell = cellFor(impact);
        const bool contactPrelude =
            impact.kind == ImpactKind::NormalBusterContactPrelude;
        const bool lethalContact =
            impact.kind == ImpactKind::NormalBusterLethalContactResidue;
        const bool survivorContact =
            impact.kind == ImpactKind::NormalBusterSurvivorContactResidue;
        const char* id = contactPrelude
                             ? "buster_normal_contact_prelude"
                             : (lethalContact
                                    ? "buster_normal_lethal_contact_residue"
                                    : (survivorContact
                                           ? "buster_normal_survivor_contact_residue"
                                           : "buster_l1_nonlethal_impact"));
        writeRow("fx", impact.sourceProjectileSerial,
                 id,
                 impact.age, cell,
                 impact.anchorX, impact.anchorY, 0.0f, 0.0f,
                 cell >= 0 ? 1 : 0,
                 impact.mirror ? 1 : 0,
                 impact.targetSerial, 0);
    }
}

inline std::optional<ImpactDraw> drawFor(const Impact& impact,
                                         float cameraX,
                                         float cameraY) {
    const int cell = cellFor(impact);
    if (cell < 0) {
        return std::nullopt;
    }

    const bool contactPrelude =
        impact.kind == ImpactKind::NormalBusterContactPrelude;
    if (contactPrelude) {
        return ImpactDraw{
            0,
            0,
            0,
            16,
            16,
            impact.anchorX - cameraX - (impact.atContactPoint ? 8.0f : 28.0f),
            impact.anchorY - cameraY + (impact.atContactPoint ? -8.0f : 16.0f),
            impact.mirror,
            std::nullopt,
        };
    }

    const bool lethalContact =
        impact.kind == ImpactKind::NormalBusterLethalContactResidue;
    const bool survivorContact =
        impact.kind == ImpactKind::NormalBusterSurvivorContactResidue;
    if (lethalContact || survivorContact) {
        constexpr float kLeftOffsets[] = {-22.0f, -24.0f, -25.0f, -26.0f};
        constexpr float kTopOffsets[] = {16.0f, 13.0f, 12.0f, 11.0f};
        std::optional<SourceSnowPriorityClip> priorityClip;
        if (impact.priorityOcclusion ==
            PriorityOcclusionProfile::ChillPenguinOpeningSnowEdge) {
            // The authenticated route has a three-row world-space snow edge:
            // all pixels above y=1168, then the left side of two one-pixel
            // stair rows ending at x=504 and x=502. This reproduces the
            // source-visible lethal 32/51/47/37 masks. The survivor adapter's
            // two-pixel horizontal translation needs one additional source-
            // proven aperture at world (512,1168), producing 32/51/45/36.
            // Both paths continue using the unmodified raw atlas.
            // Tilemap preview layers truncate the camera before subtracting it
            // from integer world coordinates. Match that quantization so a
            // fractional render camera cannot move the priority edge by 1px.
            const int scrollX = static_cast<int>(cameraX);
            const int scrollY = static_cast<int>(cameraY);
            priorityClip = SourceSnowPriorityClip{
                1168 - scrollY,
                504 - scrollX,
                502 - scrollX,
                survivorContact,
                survivorContact ? 512 - scrollX : 0,
                survivorContact ? 1168 - scrollY : 0,
            };
        }
        return ImpactDraw{
            cell,
            cell * 16,
            0,
            16,
            16,
            impact.anchorX - cameraX + (impact.atContactPoint ? -8.0f : kLeftOffsets[cell]),
            impact.anchorY - cameraY + (impact.atContactPoint ? -8.0f : kTopOffsets[cell]),
            impact.mirror,
            priorityClip,
        };
    }

    const bool contactComet = cell == 2;
    const int sourceX = contactComet ? 48 : cell * 24;
    const int sourceWidth = contactComet ? 43 : 24;
    // Source f447 measures the right-facing contact cell at (-31,-18) from
    // the impact anchor. The fixed strip remains at (-4,-8). Left-facing
    // origins use the explicit symmetric mirror inference until FW7-REVIEW-D
    // captures that branch directly: 43-31 = 12 for contact and 24-4 = 20
    // for fixed.
    const float leftOffset = contactComet
                                 ? (impact.mirror ? -12.0f : -31.0f)
                                 : (impact.mirror ? -20.0f : -4.0f);
    const float topOffset = contactComet ? -18.0f : -8.0f;
    return ImpactDraw{
        cell,
        sourceX,
        0,
        sourceWidth,
        24,
        impact.anchorX - cameraX + leftOffset,
        impact.anchorY - cameraY + topOffset,
        impact.mirror,
        std::nullopt,
    };
}

} // namespace mmx::gameplay_buster_impact
