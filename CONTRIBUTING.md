# Contributing to the public engine

This repository is a clean public source project for an engine core. It is not
a distribution of the original game and is not expected to be playable until a
separately reviewed public content strategy exists.

## Start with one task

Fork `https://github.com/davidluky/megaman-x-engine` on GitHub to create your own
remote. Anyone can read or clone the public repository. Clone your fork and
create a branch from `main` (replace `YOUR-USERNAME` below):

```sh
git clone https://github.com/YOUR-USERNAME/megaman-x-engine.git
cd megaman-x-engine
git switch -c task/<short-name>
```

Read `docs/architecture.md`, choose one task, and inspect the
relevant source and existing tests before changing anything. Keep the patch
small enough for a human reviewer to understand in one pass.

## Follow an existing ownership boundary

For an input or binding change, start with the action rather than a raw raylib
key. `InputBindings` owns stable action names, defaults, labels and duplicate
rejection; `Input` turns the current bindings into held and latched action
states; settings persists bindings; scenes and entities query actions. Do not
put a new physical-key policy in a caller that consumes an action.

The relevant public checks are:

```sh
cmake --build build --target input_bindings_contract_test settings_contract_test
ctest --test-dir build -R '^(input-bindings|settings)\.contract$' --output-on-failure
```

Run the settings contract when the change crosses saved configuration. These
contracts establish only their documented binding and persistence behavior;
they do not require or prove a playable content runtime.

## Public starter tasks

These tasks are deliberately bounded to deterministic code and public tests.
They do not require ROMs, original assets, a private measurement corpus, or a
full game build.

| Task | Source and test scope | Done when |
|---|---|---|
| Review the proposed [screen-transform contract (PR #1)](https://github.com/davidluky/megaman-x-engine/pull/1) before starting duplicate work | `src/app/screen_transform.h`; add `tests/cpp/screen_transform_contract_test.cpp` and register it with the public test target. | The contract covers invalid window dimensions, letterbox/pillarbox points outside the internal viewport, and the existing native/aspect-preserving mappings without opening a window or loading assets. |
| Add a JSON I/O result contract | `src/data/json_io.h` and `src/data/json_io.cpp`; add `tests/cpp/json_io_contract_test.cpp`. | Temporary files prove open failure, malformed JSON, object-schema rejection, successful object reads, and atomic write/reload behavior using `ReadError`; no content pack is needed. |
| Add an animation-player contract | `src/systems/animation.h` and `src/systems/animation.cpp`; add `tests/cpp/animation_contract_test.cpp`. | The test proves documented frame duration, looping, one-shot completion, `play` reset behavior, and phase-preserving `playSynced` behavior without a sprite sheet. |
| Add a path-containment contract | `src/data/path_utils.h`; add `tests/cpp/path_utils_contract_test.cpp`. | The test proves normalized descendants are accepted, parent escapes and sibling-prefix paths are rejected, and forward-slash conversion is stable across supported hosts. |

The existing source has indirect coverage for some of these areas. A new
contract must target the stated uncovered boundary and must not copy or rename
an existing contract just to increase test count. If inspection shows the
boundary is already covered in the public baseline, return a short no-change
report instead.

## Evidence and behavior

Measured gameplay values are part of the data contract. Keep their units and
provenance with the public artifact that supplies them. Do not turn a test
green result into a claim of original-game parity, and do not fill an unknown
attack, collision, animation, or spawn rule with a guess.

The public starter accepts code, tests, documentation, portability work, and
original art or audio. Original media must be authored or licensed for this
repository; Capcom material, ROM-derived data, private captures, and private
measurement files are out of scope.

## Validation and pull requests

Use the dependencies and platform setup in [README.md](README.md), then run:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 8
ctest --test-dir build --output-on-failure
```

The initial public selection has 15 contracts. No window, ROM or content pack
is needed. A full-engine build is optional and requires the separately supplied
inputs documented in [reference-data.md](docs/reference-data.md). Report
commands exactly as run and mark unavailable commands `not run`.

To submit after reviewing your diff, stage the named files, commit, push your
branch to your own fork and open a pull request against this repository's main
branch. Your agent may help prepare the patch; sending or publishing it requires
your own instruction. You do not have the maintainer's permissions.

Pull requests should state:

- the task and public files changed;
- the behavior or contract being changed;
- the exact checks run and their results;
- the provenance of any measured number or original media; and
- any missing input or known limitation.

Every pull request receives human review. Do not push, merge, or publish on
behalf of the maintainer. Original public source code uses MIT;
that license does not cover ROMs, Capcom assets, trademarks, music, or
excluded graphic data. See [LICENSE](LICENSE) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
