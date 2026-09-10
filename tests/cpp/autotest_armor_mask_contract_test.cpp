#include "harness/autotest_armor_mask.h"

#include <cassert>
#include <cstdio>

int main() {
    using mmx::parseAutotestArmorMask;

    {
        const auto mask = parseAutotestArmorMask(nullptr);
        assert(mask.boots && mask.buster && mask.body && mask.helmet);
    }
    {
        const auto mask = parseAutotestArmorMask("");
        assert(mask.boots && mask.buster && mask.body && mask.helmet);
    }
    {
        const auto mask = parseAutotestArmorMask("bare");
        assert(!mask.boots && !mask.buster && !mask.body && !mask.helmet);
    }
    {
        const auto mask = parseAutotestArmorMask("fullset");
        assert(mask.boots && mask.buster && mask.body && mask.helmet);
    }
    {
        const auto mask = parseAutotestArmorMask("boots+buster");
        assert(mask.boots && mask.buster && !mask.body && !mask.helmet);
    }
    {
        const auto mask = parseAutotestArmorMask("boots,body,helmet");
        assert(mask.boots && !mask.buster && mask.body && mask.helmet);
    }

    std::printf("autotest armor mask contract: OK\n");
    return 0;
}
