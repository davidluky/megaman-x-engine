// FE5.5-RUNTIME-B contract for the source-linked numeric OID 0x0F -> 0x09
// child model. The immutable source rows are in:
//   docs/evidence/2026-07-14-fe5-oid0f-oid09-identity-link/
//   docs/evidence/2026-07-14-fe5-oid0f-oid09-collision-owner/
//   docs/evidence/2026-07-15-fe5-oid0f-oid09-damage-motion/
//
// This contract intentionally does not name OID 0x09, choose an attack cadence,
// decode velocity, infer impact lifecycle, or bind the model to shipped content.

#include "entities/linked_enemy_child_model.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <limits>

using mmx::LinkedChildObservation;
using mmx::LinkedEnemyChildModel;
using mmx::ObservedUnarmoredContactEvent;
using mmx::SourceActionTriple;
using mmx::SourceFixed16x8Position;
using mmx::SourceObservedChildStep;

namespace {

struct LinkWitness {
    std::uint64_t linkId;
    std::uint32_t parentFrame;
    std::uint32_t childFrame;
    SourceFixed16x8Position parent;
    SourceFixed16x8Position child;
};

constexpr std::array<LinkWitness, 6> kUniqueLinkWitnesses{{
    {1, 634, 635, {285839, 77824}, {285696, 77824}},
    {2, 634, 635, {280576, 77824}, {280576, 77824}},
    {3, 677, 678, {299008, 73728}, {299008, 73728}},
    {4, 699, 700, {303168, 71680}, {303104, 71680}},
    {5, 1033, 1034, {360527, 73792}, {360448, 73728}},
    {6, 1147, 1148, {380943, 72256}, {380928, 72192}},
}};

LinkedChildObservation observation(const LinkWitness& witness) {
    return {
        witness.linkId,
        witness.parentFrame,
        witness.parent,
        witness.childFrame,
        witness.child,
    };
}

} // namespace

int main() {
    // Every unique A/B-normalized link is absent at parent frame F, is emitted
    // exactly once at F+1, copies integer X/Y with zero subpixels, and starts in
    // the packet-observed action triple (0,0,0).
    for (const auto& witness : kUniqueLinkWitnesses) {
        LinkedEnemyChildModel model;
        assert(model.queueObservedLink(observation(witness)));
        assert(model.advanceToSourceFrame(witness.parentFrame).empty());

        const auto births = model.advanceToSourceFrame(witness.childFrame);
        assert(births.size() == 1);
        const auto& birth = births.front();
        assert(birth.linkId == witness.linkId);
        assert(birth.parentSourceOid == 0x0F);
        assert(birth.childSourceOid == 0x09);
        assert(birth.sourceFrame == witness.childFrame);
        assert(birth.position == witness.child);
        assert((birth.action == SourceActionTriple{0, 0, 0}));

        const auto snapshot = model.childSnapshot(witness.linkId);
        assert(snapshot.has_value());
        assert(*snapshot == birth);
        // Lifetime is caller-observed, not synthesized. Explicit retirement is
        // available without tying the child to its parent's lifetime.
        assert(model.retireObservedChild(witness.linkId));
        assert(!model.childSnapshot(witness.linkId).has_value());
        assert(model.advanceToSourceFrame(witness.childFrame + 10).empty());
        assert(!model.queueObservedLink(observation(witness)));
    }

    // Two parents born on the same source frame create two distinct children on
    // the next frame; the unsaturated model does not collapse their ownership.
    {
        LinkedEnemyChildModel model;
        assert(model.queueObservedLink(observation(kUniqueLinkWitnesses[0])));
        assert(model.queueObservedLink(observation(kUniqueLinkWitnesses[1])));
        assert(model.advanceToSourceFrame(634).empty());
        const auto births = model.advanceToSourceFrame(635);
        assert(births.size() == 2);
        assert(births[0].linkId != births[1].linkId);
        const auto firstCopy = model.childSnapshot(births[0].linkId);
        assert(firstCopy.has_value());
        assert(model.queueObservedLink(observation(kUniqueLinkWitnesses[2])));
        assert(model.advanceToSourceFrame(678).size() == 1);
        assert(firstCopy->linkId == births[0].linkId); // value copy stays valid
    }

    // Missing F+1 never produces a retroactive birth at F+2.
    {
        LinkedEnemyChildModel model;
        assert(model.queueObservedLink(observation(kUniqueLinkWitnesses[0])));
        assert(model.advanceToSourceFrame(636).empty());
        assert(!model.childSnapshot(1).has_value());
        assert(model.childCount() == 0);
    }

    // Preserve a literal packet motion prefix as fixed point plus source-entry
    // action. These are observations supplied by the caller, not a velocity or
    // cadence law synthesized by the model.
    {
        LinkedEnemyChildModel model;
        assert(model.queueObservedLink(observation(kUniqueLinkWitnesses[0])));
        assert(model.advanceToSourceFrame(635).size() == 1);

        const SourceObservedChildStep f636{
            636, {0, 0, 0}, {2, 0, 0},
            {285696, 77824}, {285696, 77824}, 1,
        };
        const SourceObservedChildStep f637{
            637, {2, 0, 0}, {2, 0, 1},
            {285696, 77824}, {285440, 77824}, 1,
        };
        const SourceObservedChildStep f638{
            638, {2, 0, 1}, {2, 0, 1},
            {285440, 77824}, {285440, 77824}, 1,
        };
        assert(model.applyObservedStep(1, f636));
        assert((model.childSnapshot(1)->position ==
                SourceFixed16x8Position{285696, 77824}));
        assert((model.childSnapshot(1)->action == SourceActionTriple{2, 0, 0}));
        assert(model.applyObservedStep(1, f637));
        assert((model.childSnapshot(1)->position ==
                SourceFixed16x8Position{285440, 77824}));
        assert((model.childSnapshot(1)->action == SourceActionTriple{2, 0, 1}));
        assert(model.applyObservedStep(1, f638));
        assert((model.childSnapshot(1)->position ==
                SourceFixed16x8Position{285440, 77824}));

        // Conversion is an explicit consumer boundary; internal storage never
        // accumulates float deltas.
        assert(model.childSnapshot(1)->position.x / 256.0 == 1115.0);
    }

    // Exercise the two unique accepted event contexts. Damage remains
    // caller-supplied observation metadata; the model exposes no class-wide
    // damage accessor, mutates no player, and chooses no impact lifecycle.
    {
        constexpr std::array<ObservedUnarmoredContactEvent, 2> contacts{{
            {4, 891, 1, 2, {2, 2, 1}, 12, 8, 4,
             mmx::ImpactDisposition::Unspecified},
            {5, 1095, 0, 2, {2, 8, 4}, 8, 4, 4,
             mmx::ImpactDisposition::Unspecified},
        }};
        for (std::size_t i = 0; i < contacts.size(); ++i) {
            LinkedEnemyChildModel model;
            const auto& link = kUniqueLinkWitnesses[i + 3];
            assert(model.queueObservedLink(observation(link)));
            assert(model.advanceToSourceFrame(link.childFrame).size() == 1);
            assert(model.recordObservedUnarmoredContact(contacts[i]));
            const auto stored =
                model.contactObservation(contacts[i].linkId,
                                         contacts[i].sourceFrame);
            assert(stored.has_value());
            assert(*stored == contacts[i]);
            assert(!model.recordObservedUnarmoredContact(contacts[i]));

            auto conflicting = contacts[i];
            conflicting.sourceGeneration += 1;
            assert(!model.recordObservedUnarmoredContact(conflicting));

            auto inconsistent = contacts[i];
            inconsistent.sourceFrame += 1;
            inconsistent.hpLoss = 3;
            assert(!model.recordObservedUnarmoredContact(inconsistent));
        }

        LinkedEnemyChildModel empty;
        assert(!empty.recordObservedUnarmoredContact(contacts[0]));
    }

    // Link-validation negatives are atomic and do not create a child.
    {
        LinkedEnemyChildModel model;
        auto sameFrame = observation(kUniqueLinkWitnesses[0]);
        sameFrame.childFrame = sameFrame.parentFrame;
        assert(!model.queueObservedLink(sameFrame));

        auto fractionalChild = observation(kUniqueLinkWitnesses[0]);
        fractionalChild.childBirthPosition.x += 1;
        assert(!model.queueObservedLink(fractionalChild));

        auto wrongInteger = observation(kUniqueLinkWitnesses[0]);
        wrongInteger.childBirthPosition.x += 256;
        assert(!model.queueObservedLink(wrongInteger));

        auto tooFar = observation(kUniqueLinkWitnesses[0]);
        tooFar.parentBirthPosition = {285951, 78079};
        assert(!model.queueObservedLink(tooFar));

        auto frameOverflow = observation(kUniqueLinkWitnesses[0]);
        frameOverflow.parentFrame = std::numeric_limits<std::uint32_t>::max();
        frameOverflow.childFrame = 0;
        assert(!model.queueObservedLink(frameOverflow));

        auto coordinateOverflow = observation(kUniqueLinkWitnesses[0]);
        coordinateOverflow.parentBirthPosition.x = 0x1000000;
        assert(!model.queueObservedLink(coordinateOverflow));
        assert(model.childCount() == 0);
    }

    // Bad observed steps reject without partially changing fixed-point/action
    // state. Zero-source-update rows are accepted only as exact no-ops.
    {
        LinkedEnemyChildModel model;
        assert(model.queueObservedLink(observation(kUniqueLinkWitnesses[0])));
        assert(model.advanceToSourceFrame(635).size() == 1);
        const auto baseline = *model.childSnapshot(1);

        assert(!model.applyObservedStep(
            1, {636, {9, 9, 9}, {2, 0, 0},
                {285696, 77824}, {285696, 77824}, 1}));
        assert(*model.childSnapshot(1) == baseline);
        assert(!model.applyObservedStep(
            1, {637, {0, 0, 0}, {2, 0, 0},
                {285696, 77824}, {285696, 77824}, 1}));
        assert(*model.childSnapshot(1) == baseline);
        assert(!model.applyObservedStep(
            1, {636, {0, 0, 0}, {2, 0, 0},
                {285696, 77824}, {285696, 77824}, 2}));
        assert(*model.childSnapshot(1) == baseline);
        assert(!model.applyObservedStep(
            1, {636, {0, 0, 0}, {0, 0, 0},
                {285696, 77824}, {285697, 77824}, 0}));
        assert(*model.childSnapshot(1) == baseline);
        assert(!model.applyObservedStep(
            1, {636, {0, 0, 0}, {2, 0, 0},
                {285440, 77824}, {285696, 77824}, 1}));
        assert(*model.childSnapshot(1) == baseline);

        assert(model.applyObservedStep(
            1, {636, {0, 0, 0}, {0, 0, 0},
                {285696, 77824}, {285696, 77824}, 0}));
        const auto afterNoUpdate = *model.childSnapshot(1);
        assert(afterNoUpdate.sourceFrame == 636);
        assert(afterNoUpdate.position == baseline.position);
        assert(afterNoUpdate.action == baseline.action);

        // Reject a coordinate outside the packet's unsigned 24-bit domain.
        assert(!model.applyObservedStep(
            1, {637, {0, 0, 0}, {2, 0, 0},
                {285696, 77824}, {0x1000000, 77824}, 1}));
        assert(*model.childSnapshot(1) == afterNoUpdate);
        assert(!model.applyObservedStep(
            999, {637, {0, 0, 0}, {2, 0, 0},
                  {285696, 77824}, {285696, 77824}, 1}));
    }

    return 0;
}
