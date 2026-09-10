// map_editor_fill.h - pure bounded flood-fill helper for the map editor.

#pragma once

#include <algorithm>
#include <vector>

namespace mmx::map_editor {

template <typename T>
std::vector<int> collectConnectedRegion(const std::vector<T>& cells,
                                        int width,
                                        int height,
                                        int startIndex) {
    if (width <= 0 || height <= 0 || cells.empty()) return {};

    const int stageCellCount = width * height;
    const int cellCount = std::min(stageCellCount, static_cast<int>(cells.size()));
    if (startIndex < 0 || startIndex >= cellCount) return {};

    const T target = cells[startIndex];
    std::vector<int> region;
    std::vector<int> stack;
    std::vector<unsigned char> seen(cellCount, 0);

    auto push = [&](int x, int y) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        const int idx = y * width + x;
        if (idx < 0 || idx >= cellCount || seen[idx]) return;
        seen[idx] = 1;
        stack.push_back(idx);
    };

    seen[startIndex] = 1;
    stack.push_back(startIndex);
    while (!stack.empty()) {
        const int idx = stack.back();
        stack.pop_back();
        if (cells[idx] != target) continue;

        region.push_back(idx);
        const int x = idx % width;
        const int y = idx / width;
        push(x - 1, y);
        push(x + 1, y);
        push(x, y - 1);
        push(x, y + 1);
    }

    return region;
}

} // namespace mmx::map_editor
