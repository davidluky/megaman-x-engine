# Project status and community contributions

Reviewed September 21, 2026. The live [roadmap](https://megaman.davidluky.com/en/roadmap/)
is the current completion map; [updates](https://megaman.davidluky.com/en/updates/)
explain larger accepted gameplay advances. The former coarse percentage was
recalibrated, not carried over as a claim of equivalent completion.

## What this repository contains

This public code baseline supports an asset-independent core build and contracts.
It is not a playable download and does not contain the original game's media,
private measurements or generated visual tables. The September 19 code synchronization imports the accepted development C++
implementation, including hurt/recovery, projectile/contact and encounter work.
That export matched development commit
`3ea4eccd39295d897a62f38cf42d398e169f27be` except the two deliberately sanitized
dialogue adapters. The 41-file update and hashes are in [source-sync.json](source-sync.json).
The September 21 community screen-transform fix is now applied on top of that
baseline. Later private development is not implied to be part of this export.
No content, ROM, private research or generated includes were exported.
Public build/contracts validate compilation and their specific assertions; the
private gameplay acceptance suite and playable assets remain separate.

The completion plan weights a fully accepted special weapon at 1%, each currently
identified reusable enemy family at 0.75%, and each fully integrated stage at 2%.
These are editorial scope weights, not estimates of effort. Family weights can
be revised publicly as the remaining object inventory is classified; X1 always
totals 100%. Partial features earn only their accepted steps.

## Community activity

- [MattBetancourt's pull request #1](https://github.com/davidluky/megaman-x-engine/pull/1)
  adds a screen-transform contract and fixes rounding at an exact-aspect viewport
  boundary. It was reviewed and merged September 21.
- The same contributor's [JSON I/O contract](https://github.com/davidluky/megaman-x-engine/pull/2)
  was integrated from his branch, with original authorship preserved, and merged
  September 21. Maintainer corrections isolate temporary directories and cover
  replacement of an existing destination.
- Three public forks and five stars were visible at review time. Forks and stars
  signal interest, not completed contributions. The other two forks' main branches
  had no commits ahead of upstream.

Before taking a starter task, check existing pull requests and contributor branches
so work is not duplicated. Both starters above are completed and unavailable for
duplicate implementation.
Choose a bounded task from [CONTRIBUTING.md](../CONTRIBUTING.md) or use the
[AI contribution kit](https://megaman.davidluky.com/en/contribute/ai/).

## Maintainer-side verification

On Windows/MINGW64 (GCC 16.1, raylib 5.5, nlohmann_json 3.12), the screen-transform
regression failed on the old header and passed with PR #1. The integrated Release
core build and all 17 public CTest contracts passed after both contributions.
The JSON contract also passed 64 runs with eight concurrent processes, preserving
a pre-existing legacy directory and cleaning only its own acquired directories.
Existing compiler warnings remain. These checks do not claim playable-game
fidelity or filesystem crash durability.

Maintenance lesson: review test side effects as carefully as production changes;
keep contributor credit, accepted code and task availability synchronized.

## September 19 synchronization receipt

The asset-independent Release core build and 15/15 public contracts passed after
the source synchronization. Dialogue adapters and missing-generated-data checks
remain intact. No incoming contribution was overwritten or merged.

Lesson: synchronize code, the pinned kit revision and task availability together;
updating a roadmap alone leaves contributors working against an older API.
