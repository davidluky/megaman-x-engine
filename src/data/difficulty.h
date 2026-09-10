// difficulty.h - declares difficulty levels and their gameplay multipliers.
// Owns: the static DifficultySettings facade used by gameplay and menus.

#pragma once

namespace mmx {

enum class Difficulty { Easy, Normal, Hard };

class DifficultySettings {
public:
    static Difficulty current;

    static float playerDamageMultiplier() {
        switch (current) {
            case Difficulty::Easy:   return 0.5f;
            case Difficulty::Hard:   return 1.5f;
            default:                 return 1.0f;
        }
    }

    static float bossHPMultiplier() {
        switch (current) {
            case Difficulty::Easy:   return 0.75f;
            case Difficulty::Hard:   return 1.5f;
            default:                 return 1.0f;
        }
    }

    static float enemyDamageMultiplier() {
        switch (current) {
            case Difficulty::Easy:   return 0.5f;
            case Difficulty::Hard:   return 1.5f;
            default:                 return 1.0f;
        }
    }

    static int startingLives() {
        switch (current) {
            case Difficulty::Easy:   return 5;
            case Difficulty::Hard:   return 2;
            default:                 return 3;
        }
    }

    // UI/source memory stores reserve lives: a visible value of 2 means the
    // current life plus two retries (three total attempts).
    static int startingReserveLives() {
        return startingLives() - 1;
    }

    static const char* name() {
        switch (current) {
            case Difficulty::Easy:   return "EASY";
            case Difficulty::Hard:   return "HARD";
            default:                 return "NORMAL";
        }
    }

    static int toInt() { return static_cast<int>(current); }
    static void fromInt(int v) {
        if (v >= 0 && v <= 2) current = static_cast<Difficulty>(v);
    }
};

} // namespace mmx
