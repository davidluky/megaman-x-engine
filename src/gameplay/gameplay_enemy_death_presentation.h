#pragma once

#include <array>
#include <optional>

namespace mmx::gameplay_enemy_death_presentation {

inline constexpr int kMaterialCount = 8;
inline constexpr int kFirstVisibleAge = 1;
inline constexpr int kLastVisibleAge = 23;
inline constexpr int kFirstClearAge = 24;
inline constexpr int kCellWidth = 32;
inline constexpr int kCellHeight = 32;
// The clean source replay anchors the effect at the pre-kill actor X and four
// whole pixels above its visual center after subpixel rasterization.
inline constexpr int kTargetCenterAnchorOffsetX = 0;
inline constexpr int kTargetCenterAnchorOffsetY = -4;

struct Material {
    int anchorOffsetX;
    int anchorOffsetY;
};

struct BodyDrawDecision {
    bool drawBody;
    bool drawHitFlashOverlay;
};

// The lethal contact frame retains the enemy's last normal body pose. The
// white nonlethal-hit overlay is not part of that pose. On the next age the
// body clears as flash_small begins.
inline constexpr BodyDrawDecision bodyDrawDecisionForAge(int age) {
    return {age == 0, false};
}

// Tight source composites are stored at the top-left of eight 32x32 slots.
// Offsets are measured from the stable source anchor (214,114).
inline constexpr std::array<Material, kMaterialCount> kMaterials{{
    {-8, -8},   // flash_small
    {-16, -16}, // flash_large
    {-14, -13}, // burst_0
    {-15, -15}, // burst_1
    {-16, -16}, // burst_2
    {-16, -23}, // burst_3
    {-16, -21}, // burst_4
    {-16, -18}, // burst_5
}};

inline constexpr int cellForAge(int age) {
    if (age == 1 || (age >= 4 && age <= 6)) return 0;
    if (age >= 2 && age <= 3) return 1;
    if (age >= 7 && age <= 9) return 2;
    if (age >= 10 && age <= 12) return 3;
    if (age >= 13 && age <= 15) return 4;
    if (age >= 16 && age <= 18) return 5;
    if (age >= 19 && age <= 20) return 6;
    if (age >= 21 && age <= 23) return 7;
    return -1;
}

struct Draw {
    int cell;
    int sourceX;
    int sourceY;
    int sourceWidth;
    int sourceHeight;
    float topLeftX;
    float topLeftY;
};

inline std::optional<Draw> drawForAge(int age,
                                      float anchorX,
                                      float anchorY,
                                      float cameraX,
                                      float cameraY) {
    const int cell = cellForAge(age);
    if (cell < 0) return std::nullopt;
    const Material& material = kMaterials[static_cast<std::size_t>(cell)];
    return Draw{
        cell,
        cell * kCellWidth,
        0,
        kCellWidth,
        kCellHeight,
        anchorX - cameraX + static_cast<float>(material.anchorOffsetX),
        anchorY - cameraY + static_cast<float>(material.anchorOffsetY),
    };
}

inline std::optional<Draw> drawForTargetCenter(int age,
                                               float targetCenterX,
                                               float targetCenterY,
                                               float cameraX,
                                               float cameraY) {
    return drawForAge(
        age,
        targetCenterX + static_cast<float>(kTargetCenterAnchorOffsetX),
        targetCenterY + static_cast<float>(kTargetCenterAnchorOffsetY),
        cameraX,
        cameraY);
}

} // namespace mmx::gameplay_enemy_death_presentation
