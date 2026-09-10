#include "entities/linked_enemy_child_model.h"

#include <algorithm>
#include <limits>

namespace mmx {
namespace {

bool isEvidenceQualifiedLink(const LinkedChildObservation& observation) {
    constexpr std::uint32_t kMaxSourceFixed = 0xFFFFFF;
    if (observation.linkId == 0 ||
        observation.parentFrame ==
            std::numeric_limits<std::uint32_t>::max() ||
        observation.childFrame != observation.parentFrame + 1) {
        return false;
    }

    const auto& parent = observation.parentBirthPosition;
    const auto& child = observation.childBirthPosition;
    if (parent.x > kMaxSourceFixed || parent.y > kMaxSourceFixed ||
        child.x > kMaxSourceFixed || child.y > kMaxSourceFixed) {
        return false;
    }

    // The source routine copies integer X/Y and leaves the child subpixel bytes
    // zero. All six unique normalized links also fall within 255 fixed units.
    constexpr std::uint32_t kFractionMask = 0xFF;
    if ((child.x & kFractionMask) != 0 || (child.y & kFractionMask) != 0 ||
        (parent.x >> 8) != (child.x >> 8) ||
        (parent.y >> 8) != (child.y >> 8)) {
        return false;
    }

    const auto dx = child.x > parent.x ? child.x - parent.x : parent.x - child.x;
    const auto dy = child.y > parent.y ? child.y - parent.y : parent.y - child.y;
    return dx + dy <= 255;
}

bool isSourceFixed(const SourceFixed16x8Position& position) {
    constexpr std::uint32_t kMaxSourceFixed = 0xFFFFFF;
    return position.x <= kMaxSourceFixed && position.y <= kMaxSourceFixed;
}

} // namespace

bool LinkedEnemyChildModel::hasLink(std::uint64_t linkId) const {
    return std::find(seenLinkIds_.begin(), seenLinkIds_.end(), linkId) !=
           seenLinkIds_.end();
}

bool LinkedEnemyChildModel::queueObservedLink(
    const LinkedChildObservation& observation) {
    if (!isEvidenceQualifiedLink(observation) || hasLink(observation.linkId)) {
        return false;
    }
    if (hasAdvancedFrame_ && observation.parentFrame < lastAdvancedFrame_) {
        return false;
    }
    pending_.push_back(observation);
    seenLinkIds_.push_back(observation.linkId);
    return true;
}

std::vector<LinkedChildSnapshot> LinkedEnemyChildModel::advanceToSourceFrame(
    std::uint32_t sourceFrame) {
    if (hasAdvancedFrame_ && sourceFrame < lastAdvancedFrame_) {
        return {};
    }
    hasAdvancedFrame_ = true;
    lastAdvancedFrame_ = sourceFrame;

    std::vector<LinkedChildSnapshot> births;
    auto pending = pending_.begin();
    while (pending != pending_.end()) {
        if (pending->childFrame < sourceFrame) {
            // A missed source frame is not delivered retroactively: that would
            // turn the closed +1-frame onset into a late engine spawn.
            pending = pending_.erase(pending);
            continue;
        }
        if (pending->childFrame != sourceFrame) {
            ++pending;
            continue;
        }

        LinkedChildSnapshot child{
            pending->linkId,
            kParentSourceOid,
            kChildSourceOid,
            pending->childFrame,
            pending->childBirthPosition,
            {0, 0, 0},
        };
        children_.push_back(child);
        births.push_back(child);
        pending = pending_.erase(pending);
    }
    return births;
}

bool LinkedEnemyChildModel::applyObservedStep(
    std::uint64_t linkId, const SourceObservedChildStep& step) {
    auto child = std::find_if(
        children_.begin(), children_.end(),
        [linkId](const LinkedChildSnapshot& item) { return item.linkId == linkId; });
    if (child == children_.end() ||
        child->sourceFrame == std::numeric_limits<std::uint32_t>::max() ||
        step.sourceFrame != child->sourceFrame + 1 ||
        step.fromAction != child->action ||
        step.fromPosition != child->position || step.sourceUpdateCount > 1 ||
        !isSourceFixed(step.fromPosition) || !isSourceFixed(step.toPosition)) {
        return false;
    }

    if (step.sourceUpdateCount == 0 &&
        (step.toPosition != step.fromPosition ||
         step.toAction != step.fromAction)) {
        return false;
    }

    LinkedChildSnapshot updated = *child;
    updated.sourceFrame = step.sourceFrame;
    updated.position = step.toPosition;
    updated.action = step.toAction;
    *child = updated;
    return true;
}

bool LinkedEnemyChildModel::retireObservedChild(std::uint64_t linkId) {
    const auto child = std::find_if(
        children_.begin(), children_.end(),
        [linkId](const LinkedChildSnapshot& item) { return item.linkId == linkId; });
    if (child == children_.end()) {
        return false;
    }
    children_.erase(child);
    return true;
}

bool LinkedEnemyChildModel::recordObservedUnarmoredContact(
    const ObservedUnarmoredContactEvent& observation) {
    const auto child = childSnapshot(observation.linkId);
    if (!child.has_value() || observation.sourceFrame < child->sourceFrame ||
        observation.sourceSlot >= 31 || observation.sourceGeneration == 0 ||
        observation.newHp < 0 || observation.oldHp <= observation.newHp ||
        observation.hpLoss != observation.oldHp - observation.newHp ||
        observation.impactDisposition != ImpactDisposition::Unspecified) {
        return false;
    }
    const auto duplicate = std::find_if(
        contacts_.begin(), contacts_.end(),
        [&observation](const ObservedUnarmoredContactEvent& item) {
            return item.linkId == observation.linkId &&
                   item.sourceFrame == observation.sourceFrame;
        });
    if (duplicate != contacts_.end()) {
        return false;
    }
    contacts_.push_back(observation);
    return true;
}

std::optional<LinkedChildSnapshot> LinkedEnemyChildModel::childSnapshot(
    std::uint64_t linkId) const {
    const auto child = std::find_if(
        children_.begin(), children_.end(),
        [linkId](const LinkedChildSnapshot& item) { return item.linkId == linkId; });
    if (child == children_.end()) {
        return std::nullopt;
    }
    return *child;
}

std::optional<ObservedUnarmoredContactEvent>
LinkedEnemyChildModel::contactObservation(std::uint64_t linkId,
                                          std::uint32_t sourceFrame) const {
    const auto contact = std::find_if(
        contacts_.begin(), contacts_.end(),
        [linkId, sourceFrame](const ObservedUnarmoredContactEvent& item) {
            return item.linkId == linkId && item.sourceFrame == sourceFrame;
        });
    if (contact == contacts_.end()) {
        return std::nullopt;
    }
    return *contact;
}

} // namespace mmx
