# Project status and community contributions

Reviewed September 19, 2026. The live [roadmap](https://megaman.davidluky.com/en/roadmap/)
is the current completion map; [updates](https://megaman.davidluky.com/en/updates/)
explain larger accepted gameplay advances. The former coarse percentage was
recalibrated, not carried over as a claim of equivalent completion.

## What this repository contains

This public code baseline supports an asset-independent core build and contracts.
It is not a playable download and does not contain the original game's media,
private measurements or generated visual tables. The development build has
additional accepted gameplay work, including hurt/recovery trajectory corrections,
that has not been exported into this source snapshot. A source synchronization
requires dependency and provenance review plus public-build validation.

The completion plan weights a fully accepted special weapon at 1%, each currently
identified reusable enemy family at 0.75%, and each fully integrated stage at 2%.
These are editorial scope weights, not estimates of effort. Family weights can
be revised publicly as the remaining object inventory is classified; X1 always
totals 100%. Partial features earn only their accepted steps.

## Community activity

- [MattBetancourt's pull request #1](https://github.com/davidluky/megaman-x-engine/pull/1)
  adds a screen-transform contract and fixes rounding at an exact-aspect viewport
  boundary. It remains open pending maintainer acceptance; it is not part of main.
- The same contributor has a separate [JSON I/O contract branch](https://github.com/MattBetancourt/megaman-x-engine/tree/task/json-io-contract).
  No pull request for that branch was present at review time; it is not accepted.
- Three public forks and five stars were visible at review time. Forks and stars
  signal interest, not completed contributions. The other two forks' main branches
  had no commits ahead of upstream.

Before taking a starter task, check existing pull requests and contributor branches
so work is not duplicated. The screen-transform starter already has a proposal.
Choose a bounded task from [CONTRIBUTING.md](../CONTRIBUTING.md) or use the
[AI contribution kit](https://megaman.davidluky.com/en/contribute/ai/).

## Maintainer-side verification

PR #1 was checked at `f05dc3a1484e2272983c12154fe270fb0183b10b` in an isolated
checkout on Windows/MINGW64 (GCC 16.1, raylib 5.5, nlohmann_json 3.12).
CMake Release configuration and the complete default core build succeeded;
all 16 public CTest contracts passed, including `screen-transform.contract`.
Existing compiler warnings remain. This validates the submitted PR revision,
not an integrated main branch or playable-game fidelity. No PR review/comment
was posted and no merge was performed during this status update.

Maintenance lesson: public task availability must reflect incoming proposals,
while development-build progress must not be presented as exported public code.
