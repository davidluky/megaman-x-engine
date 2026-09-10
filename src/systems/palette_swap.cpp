// palette_swap.cpp - recolors likely X armor pixels for active weapons.
// Owns: armor-pixel detection and weapon-tinted color mixing rules.

#include "systems/palette_swap.h"
#include <algorithm>

namespace mmx::palette_swap {
namespace {

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

Color scaleColor(Color color, float scale) {
    return {
        static_cast<unsigned char>(std::clamp(static_cast<int>(color.r * scale), 0, 255)),
        static_cast<unsigned char>(std::clamp(static_cast<int>(color.g * scale), 0, 255)),
        static_cast<unsigned char>(std::clamp(static_cast<int>(color.b * scale), 0, 255)),
        color.a
    };
}

Color mixColor(Color a, Color b, float t) {
    t = clamp01(t);
    const float inv = 1.0f - t;
    return {
        static_cast<unsigned char>(std::clamp(static_cast<int>(a.r * inv + b.r * t), 0, 255)),
        static_cast<unsigned char>(std::clamp(static_cast<int>(a.g * inv + b.g * t), 0, 255)),
        static_cast<unsigned char>(std::clamp(static_cast<int>(a.b * inv + b.b * t), 0, 255)),
        static_cast<unsigned char>(std::clamp(static_cast<int>(a.a * inv + b.a * t), 0, 255))
    };
}

} // namespace

bool isLikelyXArmorPixel(Color color) {
    if (color.a == 0) return false;

    const int r = color.r;
    const int g = color.g;
    const int b = color.b;
    const int maxChannel = std::max({r, g, b});
    const int minChannel = std::min({r, g, b});

    // X's base sheet uses saturated blue/cyan armor. Keep whites, grays,
    // face colors, black outlines, and projectile effects out of the remap.
    return b >= 72 &&
           b >= r + 24 &&
           g >= r - 12 &&
           maxChannel - minChannel >= 32;
}

Color recolorXArmorPixel(Color source, const Weapon& weapon) {
    if (!isLikelyXArmorPixel(source)) {
        return source;
    }

    const float value = std::max({source.r, source.g, source.b}) / 255.0f;

    // Map the source pixel's brightness onto a multi-stop ramp built from the
    // weapon's two colors (plus a derived deep shadow and a bright edge). X keeps
    // his shaded, multi-tone armor look instead of being washed in one flat hue.
    const Color shadow = scaleColor(weapon.bodyColor, 0.55f);          // deepest
    const Color mid    = weapon.bodyColor;                            // main body
    const Color light  = weapon.bodyColorAlt;                        // lit accent
    const Color edge   = weapon.bodyColorAlt;                         // bright rim = same as light, no white wash

    Color result;
    if (value < 0.42f) {
        result = mixColor(shadow, mid, value / 0.42f);
    } else if (value < 0.68f) {
        result = mixColor(mid, light, (value - 0.42f) / 0.26f);
    } else if (value < 0.86f) {
        result = light;
    } else {
        result = edge;
    }

    result.a = source.a;
    return result;
}

} // namespace mmx::palette_swap
