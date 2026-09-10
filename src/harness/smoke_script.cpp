// smoke_script.cpp - drives short scripted inputs for smoke-test surfaces.
// Owns: menu, stage-select, pause, and mode-start input timings.

#include "smoke_script.h"

#include "app/input.h"

namespace mmx::smoke_script {

void runFrame(std::string_view id, std::string_view profile, int frame) {
    const int f = frame;

    Input::clearScripted();

    if (profile == "menu") {
        if (f == 30) Input::scriptedConfirmPress();
        if (f >= 55 && f < 175) Input::setScriptedDownHeld(true);
    } else if (profile == "password-valid") {
        if (f == 30) Input::scriptedConfirmPress();
        if (f == 45) Input::setScriptedDownHeld(true);
        if (f == 50) Input::scriptedConfirmPress();

        // Enter the canonical empty-progress password from the source-backed
        // all-ones initial grid. Operations are separated by a release frame
        // so edge-triggered face buttons and D-pad repeat behave naturally.
        constexpr std::string_view operations =
            ">-->--->+++v<<<"
            ">+++>->-v<<<"
            "-->-->+++>-P";
        const int operationIndex = (f - 60) / 2;
        if (f >= 60 && ((f - 60) % 2) == 0 &&
            operationIndex < static_cast<int>(operations.size())) {
            switch (operations[static_cast<std::size_t>(operationIndex)]) {
                case '>': Input::setScriptedRightHeld(true); break;
                case '<': Input::setScriptedLeftHeld(true); break;
                case 'v': Input::setScriptedDownHeld(true); break;
                case '+': Input::setScriptedShootHeld(true); break;
                case '-': Input::scriptedJumpPress(); break;
                case 'P': Input::scriptedPausePress(); break;
                default: break;
            }
        }
    } else if (profile == "stage-select") {
        if (f >= 60 && f < 95) Input::setScriptedRightHeld(true);
    } else if (profile == "start") {
        if (f == 30) Input::scriptedConfirmPress();
        if (f >= 100 && f < 180) Input::setScriptedRightHeld(true);
        if (f >= 130 && f < 134) Input::setScriptedShootHeld(true);
    } else if (profile == "pause") {
        if (f == 80) Input::scriptedPausePress();
        if (f >= 120 && f < 150) Input::setScriptedDownHeld(true);
    } else if (profile == "custom-map-menu") {
        if (f >= 35 && f < 150) Input::setScriptedDownHeld(true);
        if (f == 170) Input::scriptedConfirmPress();
        if (f >= 205 && f < 245) Input::setScriptedRightHeld(true);
    } else if (profile == "custom-map-thumbnail") {
        if (f >= 35 && f < 155) Input::setScriptedDownHeld(true);
    } else {
        if ((id == "boss-rush" || id == "bloody-palace") && f == 30) {
            Input::scriptedConfirmPress();
        }
        if (id == "extras" && f >= 45 && f < 75) {
            Input::setScriptedDownHeld(true);
        }
        if (id == "randomizer") {
            if (f >= 60 && f < 180) Input::setScriptedRightHeld(true);
            if (f >= 120 && f < 124) Input::setScriptedShootHeld(true);
            if (f == 150) Input::scriptedJumpPress();
        }
    }
}

} // namespace mmx::smoke_script
