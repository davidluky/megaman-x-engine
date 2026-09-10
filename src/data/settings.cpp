// settings.cpp - loads and saves player-facing configuration values.
// Owns: config.json sanitation for audio, display, difficulty, and bindings.

#include "data/settings.h"
#include "data/display_config.h"
#include "data/difficulty.h"
#include "data/json_io.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdio>
#include <fstream>

using json = nlohmann::json;

namespace mmx {

float Settings::masterVolume = 1.0f;
float Settings::musicVolume = 0.7f;
float Settings::sfxVolume = 0.8f;
int Settings::windowScale = 4;
bool Settings::fullscreen = false;
bool Settings::borderlessFullscreen = false;
bool Settings::vsync = true;
bool Settings::aspect43 = true;   // default to the classic 4:3 look
int Settings::difficulty = 1;
bool Settings::showTimer = false;
bool Settings::practiceMode = false;
float Settings::screenShake = 1.0f;
Language Settings::language = Language::English;
InputBindings Settings::inputBindings = InputBindings::defaults();

namespace {

struct SettingsSnapshot {
    float masterVolume = 1.0f;
    float musicVolume = 0.7f;
    float sfxVolume = 0.8f;
    int windowScale = 4;
    bool fullscreen = false;
    bool borderlessFullscreen = false;
    bool vsync = true;
    bool aspect43 = true;
    int difficulty = 1;
    bool showTimer = false;
    bool practiceMode = false;
    float screenShake = 1.0f;
    Language language = Language::English;
    InputBindings inputBindings = InputBindings::defaults();
};

float sanitizedFloatField(const json& root, const char* key, float fallback, float minValue, float maxValue) {
    const auto it = root.find(key);
    if (it == root.end() || !it->is_number()) {
        return fallback;
    }
    return std::clamp(it->get<float>(), minValue, maxValue);
}

int sanitizedIntField(const json& root, const char* key, int fallback, int minValue, int maxValue) {
    const auto it = root.find(key);
    if (it == root.end() || !it->is_number_integer()) {
        return fallback;
    }
    return std::clamp(it->get<int>(), minValue, maxValue);
}

bool sanitizedBoolField(const json& root, const char* key, bool fallback) {
    const auto it = root.find(key);
    if (it == root.end() || !it->is_boolean()) {
        return fallback;
    }
    return it->get<bool>();
}

bool readSupportedFormatVersion(const json& root, const char* key, int currentVersion) {
    const auto it = root.find(key);
    if (it == root.end()) {
        return true;
    }
    if (!it->is_number_integer()) {
        return false;
    }
    const int version = it->get<int>();
    return version >= 0 && version <= currentVersion;
}

Language sanitizedLanguageField(const json& root, const char* key, Language fallback) {
    const auto it = root.find(key);
    if (it == root.end() || !it->is_string()) {
        return fallback;
    }
    auto language = languageFromName(it->get<std::string>());
    return language.has_value() ? *language : fallback;
}

KeyboardBinding sanitizedKeyboardBindingField(const json& root, const char* key, KeyboardBinding fallback) {
    const auto it = root.find(key);
    if (it == root.end()) {
        return fallback;
    }

    KeyboardBinding parsed = fallback;
    auto parseKeyName = [](const json& value) -> std::optional<KeyboardKey> {
        if (!value.is_string()) {
            return std::nullopt;
        }
        return keyboardKeyFromName(value.get<std::string>());
    };

    if (it->is_string()) {
        auto keyName = parseKeyName(*it);
        if (keyName.has_value()) {
            parsed.primary = *keyName;
            parsed.secondary = KeyboardKey::None;
        }
        return parsed;
    }

    if (!it->is_array()) {
        return fallback;
    }

    KeyboardBinding arrayParsed;
    int count = 0;
    for (const auto& value : *it) {
        auto keyName = parseKeyName(value);
        if (!keyName.has_value() || *keyName == KeyboardKey::None) {
            continue;
        }
        if (count == 0) {
            arrayParsed.primary = *keyName;
        } else if (count == 1) {
            arrayParsed.secondary = *keyName;
        } else {
            break;
        }
        ++count;
    }
    return count > 0 ? arrayParsed : fallback;
}

GamepadBinding sanitizedGamepadBindingField(const json& root, const char* key, GamepadBinding fallback) {
    const auto it = root.find(key);
    if (it == root.end()) {
        return fallback;
    }

    GamepadBinding parsed = fallback;
    auto parseButtonName = [](const json& value) -> std::optional<GamepadButton> {
        if (!value.is_string()) {
            return std::nullopt;
        }
        return gamepadButtonFromName(value.get<std::string>());
    };

    if (it->is_string()) {
        auto buttonName = parseButtonName(*it);
        if (buttonName.has_value()) {
            parsed.primary = *buttonName;
            parsed.secondary = GamepadButton::None;
        }
        return parsed;
    }

    if (!it->is_array()) {
        return fallback;
    }

    GamepadBinding arrayParsed;
    int count = 0;
    for (const auto& value : *it) {
        auto buttonName = parseButtonName(value);
        if (!buttonName.has_value() || *buttonName == GamepadButton::None) {
            continue;
        }
        if (count == 0) {
            arrayParsed.primary = *buttonName;
        } else if (count == 1) {
            arrayParsed.secondary = *buttonName;
        } else {
            break;
        }
        ++count;
    }
    return count > 0 ? arrayParsed : fallback;
}

InputBindings sanitizedBindingsField(const json& root, const char* key, const InputBindings& fallback) {
    const auto it = root.find(key);
    if (it == root.end() || !it->is_object()) {
        return fallback;
    }

    InputBindings parsed = fallback;
    bool parsedAnyBindingGroup = false;

    const auto keyboardIt = it->find("keyboard");
    if (keyboardIt != it->end() && keyboardIt->is_object()) {
        parsedAnyBindingGroup = true;
        for (InputAction action : allInputActions()) {
            parsed.keyboardBinding(action) =
                sanitizedKeyboardBindingField(*keyboardIt, inputActionName(action), parsed.keyboardBinding(action));
        }
    }

    const auto gamepadIt = it->find("gamepad");
    if (gamepadIt != it->end() && gamepadIt->is_object()) {
        parsedAnyBindingGroup = true;
        for (InputAction action : allInputActions()) {
            parsed.gamepadBinding(action) =
                sanitizedGamepadBindingField(*gamepadIt, inputActionName(action), parsed.gamepadBinding(action));
        }
    }

    return parsedAnyBindingGroup && parsed.hasSafeBindings() ? parsed : fallback;
}

SettingsSnapshot currentSnapshot() {
    return {
        Settings::masterVolume,
        Settings::musicVolume,
        Settings::sfxVolume,
        Settings::windowScale,
        Settings::fullscreen,
        Settings::borderlessFullscreen,
        Settings::vsync,
        Settings::aspect43,
        Settings::difficulty,
        Settings::showTimer,
        Settings::practiceMode,
        Settings::screenShake,
        Settings::language,
        Settings::inputBindings,
    };
}

void commitSnapshot(const SettingsSnapshot& parsed) {
    Settings::masterVolume = parsed.masterVolume;
    Settings::musicVolume = parsed.musicVolume;
    Settings::sfxVolume = parsed.sfxVolume;
    Settings::windowScale = parsed.windowScale;
    Settings::fullscreen = parsed.fullscreen;
    Settings::borderlessFullscreen = parsed.borderlessFullscreen;
    Settings::vsync = parsed.vsync;
    Settings::aspect43 = parsed.aspect43;
    Settings::difficulty = parsed.difficulty;
    Settings::showTimer = parsed.showTimer;
    Settings::practiceMode = parsed.practiceMode;
    Settings::screenShake = parsed.screenShake;
    Settings::language = parsed.language;
    Settings::inputBindings = parsed.inputBindings;
    DifficultySettings::fromInt(Settings::difficulty);
}

} // namespace

void Settings::load(const std::string& path) {
    const auto read = json_io::readJsonObjectFromFile(path);
    if (!read.ok) {
        if (read.error != json_io::ReadError::Open) {
            std::fprintf(stderr, "Settings: %s\n", read.message.c_str());
        }
        return;
    }

    try {
        const json& j = read.value;
        if (!readSupportedFormatVersion(j, "schemaVersion", kConfigFormatVersion)) {
            std::fprintf(stderr, "Settings: unsupported schemaVersion in %s\n",
                         path.c_str());
            return;
        }
        SettingsSnapshot parsed = currentSnapshot();
        parsed.masterVolume = sanitizedFloatField(j, "masterVolume", parsed.masterVolume, 0.0f, 1.0f);
        parsed.musicVolume = sanitizedFloatField(j, "musicVolume", parsed.musicVolume, 0.0f, 1.0f);
        parsed.sfxVolume = sanitizedFloatField(j, "sfxVolume", parsed.sfxVolume, 0.0f, 1.0f);
        parsed.windowScale = sanitizedIntField(j, "windowScale", parsed.windowScale,
                                               display_config::MinWindowScale,
                                               display_config::MaxWindowScale);
        parsed.fullscreen = sanitizedBoolField(j, "fullscreen", parsed.fullscreen);
        parsed.borderlessFullscreen = sanitizedBoolField(j, "borderlessFullscreen", parsed.borderlessFullscreen);
        parsed.vsync = sanitizedBoolField(j, "vsync", parsed.vsync);
        parsed.aspect43 = sanitizedBoolField(j, "aspect43", parsed.aspect43);
        parsed.difficulty = sanitizedIntField(j, "difficulty", parsed.difficulty, 0, 2);
        parsed.showTimer = sanitizedBoolField(j, "showTimer", parsed.showTimer);
        parsed.practiceMode = sanitizedBoolField(j, "practiceMode", parsed.practiceMode);
        parsed.screenShake = sanitizedFloatField(j, "screenShake", parsed.screenShake, 0.0f, 2.0f);
        parsed.language = sanitizedLanguageField(j, "language", parsed.language);
        parsed.inputBindings = sanitizedBindingsField(j, "bindings", parsed.inputBindings);
        commitSnapshot(parsed);
    } catch (const json::exception& e) {
        std::fprintf(stderr, "Settings: invalid schema in %s: %s\n", path.c_str(), e.what());
        return;
    }
}

void Settings::save(const std::string& path) {
    json j;
    j["schemaVersion"] = kConfigFormatVersion;
    j["masterVolume"] = masterVolume;
    j["musicVolume"] = musicVolume;
    j["sfxVolume"] = sfxVolume;
    j["windowScale"] = windowScale;
    j["fullscreen"] = fullscreen;
    j["borderlessFullscreen"] = borderlessFullscreen;
    j["vsync"] = vsync;
    j["aspect43"] = aspect43;
    j["difficulty"] = difficulty;
    j["showTimer"] = showTimer;
    j["practiceMode"] = practiceMode;
    j["screenShake"] = screenShake;
    j["language"] = languageName(language);

    json keyboard = json::object();
    json gamepad = json::object();
    for (InputAction action : allInputActions()) {
        const auto& binding = inputBindings.keyboardBinding(action);
        json keys = json::array();
        keys.push_back(keyboardKeyName(binding.primary));
        if (binding.secondary != KeyboardKey::None) {
            keys.push_back(keyboardKeyName(binding.secondary));
        }
        keyboard[inputActionName(action)] = keys;

        const auto& padBinding = inputBindings.gamepadBinding(action);
        json buttons = json::array();
        buttons.push_back(gamepadButtonName(padBinding.primary));
        if (padBinding.secondary != GamepadButton::None) {
            buttons.push_back(gamepadButtonName(padBinding.secondary));
        }
        gamepad[inputActionName(action)] = buttons;
    }
    j["bindings"] = {{"keyboard", keyboard}, {"gamepad", gamepad}};

    json_io::writeAtomically(path, j);
}

} // namespace mmx
