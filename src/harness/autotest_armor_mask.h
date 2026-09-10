// autotest_armor_mask.h - parses armor-selection tokens for autotest runs.
// Boundary: test setup chooses armor state; gameplay grants still use Player.

#pragma once

#include <cctype>
#include <string>

namespace mmx {

struct AutotestArmorMask {
    bool boots = true;
    bool buster = true;
    bool body = true;
    bool helmet = true;
};

inline void applyAutotestArmorToken(AutotestArmorMask& mask,
                                    const std::string& token) {
    if (token == "all" || token == "full" || token == "fullset") {
        mask.boots = mask.buster = mask.body = mask.helmet = true;
    } else if (token == "bare" || token == "none") {
        mask.boots = mask.buster = mask.body = mask.helmet = false;
    } else if (token == "boots" || token == "legs") {
        mask.boots = true;
    } else if (token == "buster" || token == "arm" || token == "arms") {
        mask.buster = true;
    } else if (token == "body" || token == "chest") {
        mask.body = true;
    } else if (token == "helmet" || token == "head") {
        mask.helmet = true;
    }
}

inline AutotestArmorMask parseAutotestArmorMask(const char* spec) {
    if (!spec || *spec == '\0') {
        return {};
    }

    AutotestArmorMask mask{false, false, false, false};
    std::string token;
    auto flush = [&]() {
        if (token.empty()) return;
        applyAutotestArmorToken(mask, token);
        token.clear();
    };

    for (const unsigned char ch : std::string(spec)) {
        if (std::isalnum(ch) || ch == '-' || ch == '_') {
            char out = static_cast<char>(std::tolower(ch));
            if (out == '-') out = '_';
            token.push_back(out);
        } else {
            flush();
        }
    }
    flush();
    return mask;
}

}  // namespace mmx
