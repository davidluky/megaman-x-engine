// menu_flow.h - defines title-menu labels, actions, and scroll helpers.
// Owns: title menu ordering, extra-menu windows, and selection navigation.

#pragma once

#include "data/localization.h"

#include <array>
#include <cstddef>

namespace mmx {

enum class TitleMenuAction {
    None,
    NewGame,
    Password,
    Options,
    StageSelect,
    BossRush,
    BloodyPalace,
    Randomizer,
    Extras,
    MapEditor,
    TestStage,
    Quit,
};

inline constexpr int kTitleMenuItemCount = 11;
inline constexpr int kTitleFirstExtraMenuIndex = 3;
inline constexpr int kTitleExtraMenuCount = kTitleMenuItemCount - kTitleFirstExtraMenuIndex;
inline constexpr std::array<const char*, kTitleMenuItemCount> kTitleMenuLabels = {
    "GAME START",
    "PASS WORD",
    "OPTION MODE",
    "STAGE SELECT",
    "BOSS RUSH",
    "BLOODY PALACE",
    "RANDOMIZER",
    "EXTRAS",
    "MAP EDITOR",
    "TEST STAGE",
    "QUIT GAME",
};
inline constexpr std::array<const char*, kTitleExtraMenuCount> kTitleExtraMenuLabels = {
    "STAGE SELECT",
    "BOSS RUSH",
    "BLOODY PALACE",
    "RANDOMIZER",
    "EXTRAS",
    "MAP EDITOR",
    "TEST STAGE",
    "QUIT GAME",
};

inline TitleMenuAction titleMenuActionForSelection(int selection) {
    switch (selection) {
        case 0: return TitleMenuAction::NewGame;
        case 1: return TitleMenuAction::Password;
        case 2: return TitleMenuAction::Options;
        case 3: return TitleMenuAction::StageSelect;
        case 4: return TitleMenuAction::BossRush;
        case 5: return TitleMenuAction::BloodyPalace;
        case 6: return TitleMenuAction::Randomizer;
        case 7: return TitleMenuAction::Extras;
        case 8: return TitleMenuAction::MapEditor;
        case 9: return TitleMenuAction::TestStage;
        case 10: return TitleMenuAction::Quit;
        default: return TitleMenuAction::None;
    }
}

inline const char* titleMenuLabel(int selection) {
    if (selection < 0 || selection >= kTitleMenuItemCount) {
        return "";
    }
    return kTitleMenuLabels[static_cast<std::size_t>(selection)];
}

inline const char* titleMenuLabel(int selection, Language language) {
    switch (selection) {
        case 0: return uiText(UiText::TitleGameStart, language);
        case 1: return uiText(UiText::TitlePassword, language);
        case 2: return uiText(UiText::TitleOptionMode, language);
        case 3: return uiText(UiText::TitleStageSelect, language);
        case 4: return uiText(UiText::TitleBossRush, language);
        case 5: return uiText(UiText::TitleBloodyPalace, language);
        case 6: return uiText(UiText::TitleRandomizer, language);
        case 7: return uiText(UiText::TitleExtras, language);
        case 8: return uiText(UiText::TitleMapEditor, language);
        case 9: return uiText(UiText::TitleTestStage, language);
        case 10: return uiText(UiText::TitleQuitGame, language);
        default: return "";
    }
}

inline const char* titleExtraMenuLabel(int extraIndex) {
    if (extraIndex < 0 || extraIndex >= kTitleExtraMenuCount) {
        return "";
    }
    return kTitleExtraMenuLabels[static_cast<std::size_t>(extraIndex)];
}

inline int titleMenuSelectionAfterMove(int selection, int direction) {
    if (selection < 0) {
        selection = 0;
    } else if (selection >= kTitleMenuItemCount) {
        selection = kTitleMenuItemCount - 1;
    }

    if (direction < 0 && selection > 0) {
        return selection - 1;
    }
    if (direction > 0 && selection + 1 < kTitleMenuItemCount) {
        return selection + 1;
    }
    return selection;
}

inline constexpr int kTitleVisibleMenuRows = 3;
inline constexpr int kTitleExtraVisibleMenuRows = 3;

struct TitleMenuWindow {
    int firstIndex = 0;
    bool showUpArrow = false;
    bool showDownArrow = false;
};

inline TitleMenuWindow titleMenuWindowForSelection(int selection) {
    if (selection < 0) {
        selection = 0;
    } else if (selection >= kTitleMenuItemCount) {
        selection = kTitleMenuItemCount - 1;
    }

    int maxFirst = kTitleMenuItemCount - kTitleVisibleMenuRows;
    if (maxFirst < 0) {
        maxFirst = 0;
    }

    int first = selection;
    if (first > maxFirst) {
        first = maxFirst;
    }

    return TitleMenuWindow{
        first,
        first > 0,
        first + kTitleVisibleMenuRows < kTitleMenuItemCount,
    };
}

struct TitleExtraMenuWindow {
    int firstExtraIndex = 0;
    bool showUpArrow = false;
    bool showDownArrow = false;
};

inline TitleExtraMenuWindow titleExtraMenuWindowForSelection(int selection) {
    int selectedExtra = selection - kTitleFirstExtraMenuIndex;
    if (selectedExtra < 0) {
        selectedExtra = 0;
    }

    int maxFirst = kTitleExtraMenuCount - kTitleExtraVisibleMenuRows;
    if (maxFirst < 0) {
        maxFirst = 0;
    }

    int firstExtra = selectedExtra;
    if (firstExtra > maxFirst) {
        firstExtra = maxFirst;
    }

    return TitleExtraMenuWindow{
        firstExtra,
        firstExtra > 0,
        firstExtra + kTitleExtraVisibleMenuRows < kTitleExtraMenuCount,
    };
}

inline bool titleMenuScrollArrowBlinkVisible(int timer) {
    if (timer < 0) {
        timer = 0;
    }
    return ((timer / 8) % 2) == 0;
}

inline bool titleExtraScrollArrowBlinkVisible(int timer) {
    return titleMenuScrollArrowBlinkVisible(timer);
}

enum class EscapeConfirmChoice {
    Yes,
    No,
};

struct EscapeConfirmState {
    bool open = false;
    EscapeConfirmChoice choice = EscapeConfirmChoice::No;
};

inline void openEscapeConfirm(EscapeConfirmState& state) {
    state.open = true;
    state.choice = EscapeConfirmChoice::No;
}

inline void closeEscapeConfirm(EscapeConfirmState& state) {
    state.open = false;
    state.choice = EscapeConfirmChoice::No;
}

inline void moveEscapeConfirm(EscapeConfirmState& state, int direction) {
    if (!state.open || direction == 0) {
        return;
    }
    state.choice = state.choice == EscapeConfirmChoice::No
        ? EscapeConfirmChoice::Yes
        : EscapeConfirmChoice::No;
}

inline bool escapeConfirmWantsTitle(const EscapeConfirmState& state) {
    return state.open && state.choice == EscapeConfirmChoice::Yes;
}

} // namespace mmx
