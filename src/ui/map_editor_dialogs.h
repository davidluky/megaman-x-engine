// map_editor_dialogs.h - modal prompt rendering for the map editor.

#pragma once

#include "data/localization.h"
#include "raylib.h"

#include <string>

namespace mmx::map_editor {

enum class SaveNameInputAction {
    None,
    Cancel,
    Confirm,
};

struct SaveNameInputState {
    std::string draft;
    int maxChars = 32;
};

struct SaveNameInputResult {
    SaveNameInputAction action = SaveNameInputAction::None;
    std::string draft;
};

inline SaveNameInputResult handleSaveNameInput(
    const SaveNameInputState& state) {
    SaveNameInputResult result;
    result.draft = state.draft;

    int ch = GetCharPressed();
    while (ch > 0) {
        if (ch >= 32 && ch <= 126 &&
            static_cast<int>(result.draft.size()) < state.maxChars) {
            result.draft.push_back(static_cast<char>(ch));
        }
        ch = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && !result.draft.empty()) {
        result.draft.pop_back();
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        result.action = SaveNameInputAction::Cancel;
        return result;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        result.action = SaveNameInputAction::Confirm;
    }
    return result;
}

enum class MetadataInputAction {
    None,
    Cancel,
    Confirm,
};

struct MetadataInputState {
    int field = 0;
    std::string authorDraft;
    std::string descriptionDraft;
    int maxAuthorChars = 32;
    int maxDescriptionChars = 96;
};

struct MetadataInputResult {
    MetadataInputAction action = MetadataInputAction::None;
    int field = 0;
    std::string authorDraft;
    std::string descriptionDraft;
};

inline MetadataInputResult handleMetadataInput(
    const MetadataInputState& state) {
    MetadataInputResult result;
    result.field = state.field == 0 ? 0 : 1;
    result.authorDraft = state.authorDraft;
    result.descriptionDraft = state.descriptionDraft;

    std::string& target = result.field == 0 ? result.authorDraft
                                            : result.descriptionDraft;
    const int maxChars = result.field == 0 ? state.maxAuthorChars
                                           : state.maxDescriptionChars;
    int ch = GetCharPressed();
    while (ch > 0) {
        if (ch >= 32 && ch <= 126 && static_cast<int>(target.size()) < maxChars) {
            target.push_back(static_cast<char>(ch));
        }
        ch = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && !target.empty()) {
        target.pop_back();
    }

    if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) {
        result.field = 1 - result.field;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        result.action = MetadataInputAction::Cancel;
        return result;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        result.action = MetadataInputAction::Confirm;
    }
    return result;
}

enum class ExitConfirmInputAction {
    None,
    Cancel,
    Confirm,
};

struct ExitConfirmInputResult {
    ExitConfirmInputAction action = ExitConfirmInputAction::None;
};

inline ExitConfirmInputResult handleExitConfirmInput() {
    ExitConfirmInputResult result;
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_N)) {
        result.action = ExitConfirmInputAction::Cancel;
        return result;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) ||
        IsKeyPressed(KEY_Y)) {
        result.action = ExitConfirmInputAction::Confirm;
    }
    return result;
}

inline void renderExitConfirmPrompt(int internalWidth,
                                    int internalHeight,
                                    Language language) {
    const int panelW = 214;
    const int panelH = 58;
    const int panelX = (internalWidth - panelW) / 2;
    const int panelY = (internalHeight - panelH) / 2;
    DrawRectangle(panelX, panelY, panelW, panelH, {0, 12, 48, 238});
    DrawRectangleLines(panelX, panelY, panelW, panelH, {255, 210, 96, 255});
    DrawRectangleLines(panelX + 2, panelY + 2, panelW - 4, panelH - 4,
                       {74, 148, 236, 255});

    DrawText(uiText(UiText::MapEditorUnsavedTitle, language),
             panelX + 10, panelY + 8, 8, {255, 242, 116, 255});
    DrawText(uiText(UiText::MapEditorExitWithoutSaving, language),
             panelX + 10, panelY + 25, 8, WHITE);
    DrawText(uiText(UiText::MapEditorExitStayHint, language),
             panelX + 10, panelY + 43, 7, {188, 212, 244, 255});
}

inline void renderSaveNamePrompt(int internalWidth,
                                 int internalHeight,
                                 const std::string& saveNameDraft,
                                 int timer,
                                 Language language) {
    const int panelW = 188;
    const int panelH = 58;
    const int panelX = (internalWidth - panelW) / 2;
    const int panelY = (internalHeight - panelH) / 2;
    DrawRectangle(panelX, panelY, panelW, panelH, {0, 12, 48, 238});
    DrawRectangleLines(panelX, panelY, panelW, panelH, {210, 235, 255, 255});
    DrawRectangleLines(panelX + 2, panelY + 2, panelW - 4, panelH - 4,
                       {74, 148, 236, 255});

    DrawText(uiText(UiText::MapEditorSaveMapAs, language),
             panelX + 10, panelY + 8, 8, {255, 242, 116, 255});

    std::string shown = saveNameDraft;
    if ((timer / 20) % 2 == 0) {
        shown.push_back('_');
    }
    const int inputX = panelX + 10;
    const int inputY = panelY + 24;
    const int inputW = panelW - 20;
    while (MeasureText(shown.c_str(), 8) > inputW - 6 && shown.size() > 1) {
        shown.erase(shown.begin());
    }
    DrawRectangle(inputX, inputY - 2, inputW, 12, {4, 28, 84, 255});
    DrawRectangleLines(inputX, inputY - 2, inputW, 12, {96, 174, 236, 255});
    DrawText(shown.c_str(), inputX + 3, inputY, 8, WHITE);

    DrawText(uiText(UiText::MapEditorEnterSaveCancelHint, language),
             panelX + 10, panelY + 43, 7, {188, 212, 244, 255});
}

inline void renderMetadataPrompt(int internalWidth,
                                 int internalHeight,
                                 int metadataField,
                                 const std::string& authorDraft,
                                 const std::string& descriptionDraft,
                                 int timer,
                                 Language language) {
    const int panelW = 224;
    const int panelH = 82;
    const int panelX = (internalWidth - panelW) / 2;
    const int panelY = (internalHeight - panelH) / 2;
    DrawRectangle(panelX, panelY, panelW, panelH, {0, 12, 48, 238});
    DrawRectangleLines(panelX, panelY, panelW, panelH, {210, 235, 255, 255});
    DrawRectangleLines(panelX + 2, panelY + 2, panelW - 4, panelH - 4,
                       {74, 148, 236, 255});

    DrawText(uiText(UiText::MapEditorMapInfoTitle, language),
             panelX + 10, panelY + 8, 8, {255, 242, 116, 255});

    auto drawField = [&](int field, const char* label, std::string value, int y) {
        if (field == metadataField && (timer / 20) % 2 == 0) {
            value.push_back('_');
        }
        const int labelX = panelX + 10;
        const int inputX = panelX + 64;
        const int inputW = panelW - 74;
        const bool active = field == metadataField;
        DrawText(label, labelX, y, 7, active ? Color{255, 242, 116, 255}
                                             : Color{188, 212, 244, 255});
        while (MeasureText(value.c_str(), 7) > inputW - 6 && value.size() > 1) {
            value.erase(value.begin());
        }
        DrawRectangle(inputX, y - 2, inputW, 11,
                      active ? Color{6, 40, 98, 255} : Color{4, 28, 84, 255});
        DrawRectangleLines(inputX, y - 2, inputW, 11,
                           active ? Color{255, 242, 116, 255}
                                  : Color{96, 174, 236, 255});
        DrawText(value.c_str(), inputX + 3, y, 7, WHITE);
    };

    drawField(0, uiText(UiText::MapEditorAuthorField, language),
              authorDraft, panelY + 25);
    drawField(1, uiText(UiText::MapEditorDescField, language),
              descriptionDraft, panelY + 40);

    DrawText(uiText(UiText::MapEditorMapInfoFooter, language),
             panelX + 10, panelY + panelH - 14, 7, {188, 212, 244, 255});
}

} // namespace mmx::map_editor
