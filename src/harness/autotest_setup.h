// autotest_setup.h - declares shared player setup helpers for autotests.
// Boundary: helpers mutate the supplied Player only during harness setup.

#pragma once

namespace mmx {

class Player;

namespace autotest_setup {

void grantAllWeapons(Player& player, bool grantBusterArmor = true);

} // namespace autotest_setup
} // namespace mmx
