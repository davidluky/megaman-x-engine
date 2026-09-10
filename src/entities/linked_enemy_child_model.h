#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace mmx {

// The packet derives each axis as unsigned u16 integer * 256 + subpixel byte:
// 16.8 fixed point stored in 24 total bits. Consumers may convert at their own
// boundary; this model never accumulates decimal motion.
struct SourceFixed16x8Position {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
};

inline bool operator==(const SourceFixed16x8Position& lhs,
                       const SourceFixed16x8Position& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

inline bool operator!=(const SourceFixed16x8Position& lhs,
                       const SourceFixed16x8Position& rhs) {
    return !(lhs == rhs);
}

struct SourceActionTriple {
    std::uint8_t action1 = 0;
    std::uint8_t action2 = 0;
    std::uint8_t action3 = 0;
};

inline bool operator==(const SourceActionTriple& lhs,
                       const SourceActionTriple& rhs) {
    return lhs.action1 == rhs.action1 && lhs.action2 == rhs.action2 &&
           lhs.action3 == rhs.action3;
}

inline bool operator!=(const SourceActionTriple& lhs,
                       const SourceActionTriple& rhs) {
    return !(lhs == rhs);
}

// The caller supplies both sides of an evidence-qualified source link. This
// keeps the model from inventing a parent trigger, allocator policy, or spawn
// placement outside the closed observations.
struct LinkedChildObservation {
    std::uint64_t linkId = 0;
    std::uint32_t parentFrame = 0;
    SourceFixed16x8Position parentBirthPosition;
    std::uint32_t childFrame = 0;
    SourceFixed16x8Position childBirthPosition;
};

struct SourceObservedChildStep {
    std::uint32_t sourceFrame = 0;
    SourceActionTriple fromAction;
    SourceActionTriple toAction;
    SourceFixed16x8Position fromPosition;
    SourceFixed16x8Position toPosition;
    std::uint8_t sourceUpdateCount = 0;
};

enum class ImpactDisposition : std::uint8_t {
    Unspecified,
};

struct ObservedUnarmoredContactEvent {
    std::uint64_t linkId = 0;
    std::uint32_t sourceFrame = 0;
    std::uint8_t sourceSlot = 0;
    std::uint32_t sourceGeneration = 0;
    SourceActionTriple action;
    int oldHp = 0;
    int newHp = 0;
    int hpLoss = 0;
    ImpactDisposition impactDisposition = ImpactDisposition::Unspecified;
};

inline bool operator==(const ObservedUnarmoredContactEvent& lhs,
                       const ObservedUnarmoredContactEvent& rhs) {
    return lhs.linkId == rhs.linkId && lhs.sourceFrame == rhs.sourceFrame &&
           lhs.sourceSlot == rhs.sourceSlot &&
           lhs.sourceGeneration == rhs.sourceGeneration &&
           lhs.action == rhs.action && lhs.oldHp == rhs.oldHp &&
           lhs.newHp == rhs.newHp && lhs.hpLoss == rhs.hpLoss &&
           lhs.impactDisposition == rhs.impactDisposition;
}

struct LinkedChildSnapshot {
    std::uint64_t linkId = 0;
    std::uint8_t parentSourceOid = 0;
    std::uint8_t childSourceOid = 0;
    std::uint32_t sourceFrame = 0;
    SourceFixed16x8Position position;
    SourceActionTriple action;
};

inline bool operator==(const LinkedChildSnapshot& lhs,
                       const LinkedChildSnapshot& rhs) {
    return lhs.linkId == rhs.linkId &&
           lhs.parentSourceOid == rhs.parentSourceOid &&
           lhs.childSourceOid == rhs.childSourceOid &&
           lhs.sourceFrame == rhs.sourceFrame && lhs.position == rhs.position &&
           lhs.action == rhs.action;
}

inline bool operator!=(const LinkedChildSnapshot& lhs,
                       const LinkedChildSnapshot& rhs) {
    return !(lhs == rhs);
}

// Evidence-bounded model for the numeric source OID 0x0F -> OID 0x09 link.
// It is externally armed and externally observed: no cadence, velocity,
// collision geometry, impact lifetime, art, or shipped placement is inferred.
class LinkedEnemyChildModel {
public:
    static constexpr std::uint8_t kParentSourceOid = 0x0F;
    static constexpr std::uint8_t kChildSourceOid = 0x09;

    bool queueObservedLink(const LinkedChildObservation& observation);
    std::vector<LinkedChildSnapshot> advanceToSourceFrame(
        std::uint32_t sourceFrame);
    bool applyObservedStep(std::uint64_t linkId,
                           const SourceObservedChildStep& step);
    bool retireObservedChild(std::uint64_t linkId);
    bool recordObservedUnarmoredContact(
        const ObservedUnarmoredContactEvent& observation);

    std::optional<LinkedChildSnapshot> childSnapshot(std::uint64_t linkId) const;
    std::optional<ObservedUnarmoredContactEvent> contactObservation(
        std::uint64_t linkId, std::uint32_t sourceFrame) const;
    std::size_t childCount() const { return children_.size(); }

private:
    bool hasLink(std::uint64_t linkId) const;

    std::vector<LinkedChildObservation> pending_;
    std::vector<LinkedChildSnapshot> children_;
    std::vector<ObservedUnarmoredContactEvent> contacts_;
    std::vector<std::uint64_t> seenLinkIds_;
    bool hasAdvancedFrame_ = false;
    std::uint32_t lastAdvancedFrame_ = 0;
};

} // namespace mmx
