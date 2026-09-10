// palette_swap.h - declares X armor pixel detection and recolor helpers.
// Owns: palette-swap API used by player weapon variant rendering.

#pragma once

#include "systems/weapon.h"

namespace mmx::palette_swap {

bool isLikelyXArmorPixel(Color color);
Color recolorXArmorPixel(Color source, const Weapon& weapon);

} // namespace mmx::palette_swap
