#include "data/stage_identity.h"

#include <cassert>
#include <type_traits>
#include <string>

int main() {
    static_assert(!std::is_constructible_v<mmx::StageId, std::string>);
    static_assert(!std::is_constructible_v<mmx::BossId, std::string>);
    static_assert(!std::is_constructible_v<mmx::WeaponId, std::string>);
    static_assert(!std::is_same_v<mmx::StageId, mmx::BossId>);
    static_assert(!std::is_same_v<mmx::StageId, mmx::WeaponId>);

    using mmx::stage_identity::displayName;
    using mmx::stage_identity::fromPath;

    assert(fromPath("content/x1/stages/ripped/chill-penguin/stage_final.json").str() == "chill-penguin");
    assert(fromPath("content\\x1\\stages\\ripped\\storm-eagle\\stage_final.json").str() == "storm-eagle");
    assert(fromPath("content/x1/stages/ripped/sigma-4/stage.json").str() == "sigma-4");
    assert(fromPath("content/x1/stages/intro-highway.json").str() == "intro-highway");
    assert(fromPath("content/extras/stages/enemy-sandbox/stage_final.json").str() == "enemy-sandbox");

    assert(displayName(mmx::StageId::fromString("chill-penguin")) == "CHILL PENGUIN");
    assert(displayName(mmx::StageId::fromString("enemy_sandbox")) == "ENEMY SANDBOX");
    assert(displayName({}) == "UNKNOWN STAGE");

    return 0;
}
