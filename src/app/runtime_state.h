#pragma once

#include "data/save_system.h"
#include "entities/player.h"
#include "systems/audio.h"
#include "systems/weapon.h"

namespace mmx {

struct RuntimeState {
    SaveSystemState save;
    WeaponInventoryState weaponInventory;
    PlayerProgress playerProgress;
    AudioRuntimeState audio;
};

} // namespace mmx
