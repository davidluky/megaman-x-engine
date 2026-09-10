#pragma once

#include "app/constants.h"
#include <algorithm>

namespace mmx::display_config {

constexpr int MinWindowScale = 2;
constexpr int MaxWindowScale = 4;

inline int clampWindowScale(int scale) {
    return std::clamp(scale, MinWindowScale, MaxWindowScale);
}

inline int windowHeight(int scale) {
    return INTERNAL_HEIGHT * clampWindowScale(scale);
}

inline int windowWidth(int scale, bool aspect43) {
    const int clampedScale = clampWindowScale(scale);
    const int height = INTERNAL_HEIGHT * clampedScale;
    if (aspect43) {
        return static_cast<int>(height * 4.0f / 3.0f + 0.5f);
    }
    return INTERNAL_WIDTH * clampedScale;
}

} // namespace mmx::display_config
