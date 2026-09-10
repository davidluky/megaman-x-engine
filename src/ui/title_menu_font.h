// title_menu_font.h - defines the compact title-menu glyph bitmap table.
// Owns: glyph dimensions, glyph lookup, and menu-font pixel rows.

#pragma once

namespace mmx {

struct TitleMenuGlyph {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int advance = 8;
    bool drawable = false;
};

constexpr TitleMenuGlyph titleMenuGlyphForChar(char c) {
    if (c == ' ') return TitleMenuGlyph{0, 0, 0, 0, 9, false};

    // content/x1/sprites/misc/mmx_font.png uppercase atlas:
    // row 132 starts with '@' then A..O; row 145 starts with P..Z.
    if (c >= 'A' && c <= 'O') {
        const int idx = c - 'A';
        return TitleMenuGlyph{23 + idx * 13, 132, 8, 8, 9, true};
    }
    if (c >= 'P' && c <= 'Z') {
        const int idx = c - 'P';
        return TitleMenuGlyph{10 + idx * 13, 145, 8, 8, 9, true};
    }
    return TitleMenuGlyph{0, 0, 0, 0, 8, false};
}

} // namespace mmx
