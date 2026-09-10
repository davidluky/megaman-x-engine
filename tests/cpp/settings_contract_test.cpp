#include "data/settings.h"
#include "data/difficulty.h"

#include <nlohmann/json.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    namespace fs = std::filesystem;
    using json = nlohmann::json;

    mmx::DifficultySettings::current = mmx::Difficulty::Easy;
    assert(mmx::DifficultySettings::startingLives() == 5);
    assert(mmx::DifficultySettings::startingReserveLives() == 4);
    mmx::DifficultySettings::current = mmx::Difficulty::Normal;
    assert(mmx::DifficultySettings::startingLives() == 3);
    assert(mmx::DifficultySettings::startingReserveLives() == 2);
    mmx::DifficultySettings::current = mmx::Difficulty::Hard;
    assert(mmx::DifficultySettings::startingLives() == 2);
    assert(mmx::DifficultySettings::startingReserveLives() == 1);

    const fs::path settingsPath = fs::path("tmp") / "settings-contract.json";
    fs::create_directories(settingsPath.parent_path());

    {
        std::ofstream out(settingsPath);
        out << json{
            {"masterVolume", 2.0f},
            {"musicVolume", -1.0f},
            {"sfxVolume", 0.5f},
            {"windowScale", 99},
            {"fullscreen", true},
            {"borderlessFullscreen", true},
            {"vsync", false},
            {"difficulty", 99},
            {"showTimer", true},
            {"practiceMode", true},
            {"screenShake", 99.0f},
            {"language", "pt-BR"},
            {"bindings", {
                {"keyboard", {
                    {"jump", json::array({"C"})},
                    {"dash", json::array({"D", "LEFT_SHIFT"})},
                    {"cancel", json::array({"ESCAPE"})},
                }},
                {"gamepad", {
                    {"jump", json::array({"LEFT_TRIGGER"})},
                    {"dash", json::array({"FACE_LEFT"})},
                    {"cancel", json::array({"FACE_RIGHT"})},
                }},
            }},
        }.dump(2);
    }

    mmx::Settings::load(settingsPath.string());
    assert(mmx::Settings::masterVolume == 1.0f);
    assert(mmx::Settings::musicVolume == 0.0f);
    assert(mmx::Settings::sfxVolume == 0.5f);
    assert(mmx::Settings::windowScale == 4);
    assert(mmx::Settings::fullscreen);
    assert(mmx::Settings::borderlessFullscreen);
    assert(!mmx::Settings::vsync);
    assert(mmx::Settings::difficulty == 2);
    assert(mmx::DifficultySettings::current == mmx::Difficulty::Hard);
    assert(mmx::Settings::showTimer);
    assert(mmx::Settings::practiceMode);
    assert(mmx::Settings::screenShake == 2.0f);
    assert(mmx::Settings::language == mmx::Language::PortugueseBrazil);
    assert(std::string(mmx::languageLabel(mmx::Settings::language)) == "PORTUGUES (BR)");
    assert(std::string(mmx::uiText(mmx::UiText::OptionsTitle, mmx::Settings::language)) == "OPCOES");
    assert(mmx::Settings::inputBindings.keyboardBinding(mmx::InputAction::Jump).primary == mmx::KeyboardKey::C);
    assert(mmx::Settings::inputBindings.keyboardBinding(mmx::InputAction::Jump).secondary == mmx::KeyboardKey::None);
    assert(mmx::Settings::inputBindings.gamepadBinding(mmx::InputAction::Jump).primary == mmx::GamepadButton::LeftTrigger);
    assert(mmx::Settings::inputBindings.gamepadBinding(mmx::InputAction::Jump).secondary == mmx::GamepadButton::None);

    {
        std::ofstream out(settingsPath);
        out << json{
            {"masterVolume", 0.25f},
            {"fullscreen", "not-a-bool"},
            {"borderlessFullscreen", "not-a-bool"},
            {"vsync", "not-a-bool"},
            {"difficulty", "not-an-int"},
            {"practiceMode", "not-a-bool"},
        }.dump(2);
    }

    mmx::Settings::load(settingsPath.string());
    assert(mmx::Settings::masterVolume == 0.25f);
    assert(mmx::Settings::fullscreen);
    assert(mmx::Settings::borderlessFullscreen);
    assert(!mmx::Settings::vsync);
    assert(mmx::Settings::difficulty == 2);
    assert(mmx::DifficultySettings::current == mmx::Difficulty::Hard);
    assert(mmx::Settings::practiceMode);
    assert(mmx::Settings::language == mmx::Language::PortugueseBrazil);
    assert(mmx::Settings::inputBindings.keyboardBinding(mmx::InputAction::Jump).primary == mmx::KeyboardKey::C);

    {
        std::ofstream out(settingsPath);
        out << json{
            {"language", "klingon"},
        }.dump(2);
    }

    mmx::Settings::load(settingsPath.string());
    assert(mmx::Settings::language == mmx::Language::PortugueseBrazil);

    {
        std::ofstream out(settingsPath);
        out << json{
            {"bindings", {
                {"keyboard", {
                    {"cancel", json::array({"X"})},
                }},
            }},
        }.dump(2);
    }

    mmx::Settings::load(settingsPath.string());
    assert(mmx::Settings::inputBindings.keyboardBinding(mmx::InputAction::Jump).primary == mmx::KeyboardKey::C);
    assert(mmx::Settings::inputBindings.keyboardBinding(mmx::InputAction::Dash).primary == mmx::KeyboardKey::D);
    assert(mmx::Settings::inputBindings.gamepadBinding(mmx::InputAction::Jump).primary == mmx::GamepadButton::LeftTrigger);

    {
        std::ofstream out(settingsPath);
        out << json{
            {"schemaVersion", mmx::Settings::kConfigFormatVersion + 1},
            {"masterVolume", 0.10f},
        }.dump(2);
    }

    mmx::Settings::masterVolume = 0.75f;
    mmx::Settings::load(settingsPath.string());
    assert(mmx::Settings::masterVolume == 0.75f);

    {
        std::ofstream out(settingsPath);
        out << "{not valid json";
    }
    mmx::Settings::masterVolume = 0.75f;
    mmx::Settings::load(settingsPath.string());
    assert(mmx::Settings::masterVolume == 0.75f);
    assert(mmx::DifficultySettings::current == mmx::Difficulty::Hard);

    mmx::Settings::save(settingsPath.string());
    assert(fs::exists(settingsPath));
    assert(!fs::exists(settingsPath.string() + ".tmp"));
    {
        json saved;
        std::ifstream in(settingsPath);
        in >> saved;
        assert(saved["schemaVersion"] == mmx::Settings::kConfigFormatVersion);
        assert(saved["bindings"]["keyboard"]["jump"] == json::array({"C"}));
        assert(saved["bindings"]["keyboard"]["cancel"] == json::array({"ESCAPE"}));
        assert(saved["bindings"]["gamepad"]["jump"] == json::array({"LEFT_TRIGGER"}));
        assert(saved["bindings"]["gamepad"]["dash"] == json::array({"FACE_LEFT"}));
        assert(saved["language"] == "pt-BR");
        assert(saved["borderlessFullscreen"] == true);
        assert(saved["vsync"] == false);
        assert(saved["practiceMode"] == true);
    }

    fs::remove(settingsPath);
    return 0;
}
