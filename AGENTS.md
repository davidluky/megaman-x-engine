# Public repository agent guide

Read `CONTRIBUTING.md` and `docs/architecture.md` before editing. This file is
the canonical agent entry point for the public repository; keep `CLAUDE.md` as
a pointer instead of maintaining a second rule set.

Work only in the public checkout and in the agreed task scope. Preserve other
contributors' changes. Use one branch and one small, reviewable objective.

The default build and test path is the asset-independent core and its selected
contracts. Do not infer that the full game executable can build or run without
the compatible separately supplied content pack and generated runtime data. Mark a command
as not run when its dependency is unavailable.

Keep the C++17 and raylib/nlohmann_json boundaries clear. Core utilities and
contracts should not load assets just to test deterministic behavior. Do not
add tests that merely duplicate an existing contract; first read the relevant
source and current tests.

Never invent gameplay constants, frame counts, collision geometry, sprite
identity, audio provenance, or original-game fidelity. Preserve measured
numbers and units only when their public source is included with the change.
If a task needs private evidence, a ROM, a missing asset, or an unavailable
generated table, stop at that boundary and describe the required public input.

Only original art and audio may be contributed. Do not add ROM data, Capcom
assets, private captures, private measurement databases, credentials, or
generated visual/audio tables copied from a private checkout.

Use normal Git review practices: stage exact paths, do not rewrite unrelated
history, do not force-push, and do not push on behalf of the maintainer. A
human reviews every pull request. Original public source code uses MIT;
do not treat that license as covering ROMs, Capcom assets, trademarks,
music, or excluded graphic data.
