// screen_transform.h - maps window pixels to the internal SNES framebuffer.

#pragma once

#include "app/constants.h"

namespace mmx::screen_transform {

struct InternalViewport {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct InternalPoint {
    float x = -1.0f;
    float y = -1.0f;
    bool inside = false;
};

inline float displayAspect(bool aspect43) {
    return aspect43
        ? (4.0f / 3.0f)
        : (static_cast<float>(INTERNAL_WIDTH) / static_cast<float>(INTERNAL_HEIGHT));
}

inline InternalViewport internalViewportForWindow(float screenWidth,
                                                  float screenHeight,
                                                  bool aspect43) {
    if (screenWidth <= 0.0f || screenHeight <= 0.0f) {
        return {};
    }

    const float targetAspect = displayAspect(aspect43);
    float destW = screenWidth;
    float destH = screenHeight;
    if (screenWidth / screenHeight > targetAspect) {
        destH = screenHeight;
        destW = screenHeight * targetAspect;
    } else {
        destW = screenWidth;
        destH = screenWidth / targetAspect;
    }

    return {
        (screenWidth - destW) * 0.5f,
        (screenHeight - destH) * 0.5f,
        destW,
        destH,
    };
}

inline InternalPoint screenToInternal(float screenX,
                                      float screenY,
                                      float screenWidth,
                                      float screenHeight,
                                      bool aspect43) {
    const InternalViewport viewport =
        internalViewportForWindow(screenWidth, screenHeight, aspect43);
    if (viewport.width <= 0.0f || viewport.height <= 0.0f) {
        return {};
    }

    const float internalX =
        (screenX - viewport.x) * static_cast<float>(INTERNAL_WIDTH) / viewport.width;
    const float internalY =
        (screenY - viewport.y) * static_cast<float>(INTERNAL_HEIGHT) / viewport.height;
    return {
        internalX,
        internalY,
        internalX >= 0.0f && internalX < static_cast<float>(INTERNAL_WIDTH) &&
            internalY >= 0.0f && internalY < static_cast<float>(INTERNAL_HEIGHT),
    };
}

} // namespace mmx::screen_transform
