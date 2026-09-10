// settings.h - declares global runtime settings persisted in config.json.
// Owns: the process-wide settings facade used by menus, audio, and gameplay.

#pragma once

#include "app/input_bindings.h"
#include "data/localization.h"

#include <string>

namespace mmx {

class Settings {
public:
    static constexpr int kConfigFormatVersion = 1;

    static void load(const std::string& path = "config.json");
    static void save(const std::string& path = "config.json");

    static float masterVolume;
    static float musicVolume;
    static float sfxVolume;
    static int windowScale;
    static bool fullscreen;
    static bool borderlessFullscreen;
    static bool vsync;
    static bool aspect43;   // true = stretch 256x224 to classic 4:3; false = native 8:7 square pixels
    static int difficulty;
    static bool showTimer;
    static bool practiceMode;
    static float screenShake;
    static Language language;
    static InputBindings inputBindings;
};

} // namespace mmx
