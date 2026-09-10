// game_ids.h - provides typed string identifiers for stages, bosses, weapons.
// Boundary: IDs remain lightweight values; data loaders resolve their meaning.

#pragma once

#include <string>
#include <utility>

namespace mmx {

template <typename Tag>
class GameId {
public:
    GameId() = default;

    static GameId fromString(std::string value) {
        return GameId(std::move(value));
    }

    const std::string& str() const { return value_; }
    bool empty() const { return value_.empty(); }

    friend bool operator==(const GameId& lhs, const GameId& rhs) {
        return lhs.value_ == rhs.value_;
    }

    friend bool operator!=(const GameId& lhs, const GameId& rhs) {
        return !(lhs == rhs);
    }

private:
    explicit GameId(std::string value) : value_(std::move(value)) {}

    std::string value_;
};

struct StageIdTag {};
struct BossIdTag {};
struct WeaponIdTag {};

using StageId = GameId<StageIdTag>;
using BossId = GameId<BossIdTag>;
using WeaponId = GameId<WeaponIdTag>;

} // namespace mmx
