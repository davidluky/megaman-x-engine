# Code and reference data

The default build compiles the engine source into `mmx-engine-core` and runs
selected contracts that use in-memory values or small temporary JSON fixtures.
It does not need a ROM, a game content pack, an emulator or a graphics window.

This static library is a compilation target, not a complete playable runtime.
Four cutscene/foreground translation units require generated data and are
excluded from it. Other compiled objects can reference those adapters; linking
a full game requires the omitted translation units and their data.

## Full executable

`MMX_PUBLIC_BUILD_ENGINE=ON` is an advanced local integration option. CMake
lists each missing generated include and stops during configuration when any
is absent. It does not download, reconstruct or substitute those tables.
Runtime `content/` and `knowledge_base/` are also separate prerequisites; the
public checkout alone cannot run the complete game.

Generated visual tables and original capsule dialogue are external inputs.
For the Chill Penguin dialogue adapter, `cp_capsule_cutscene.cpp` expects
`generated/cp_capsule/cp_capsule_dialog.inc` to define `kDialogF0660`,
`kDialogF0900`, `kDialogF1200`, `kDialogF1260`, `kDialogF1290`, and `kDialogF1320`
as `CpCapsuleDialogLine` arrays. Their original text is not supplied here.
The public ending scene also keeps its narrative list empty while retaining
rendering logic and attribution. These are explicit content exclusions in the
public snapshot, not fixes or parity changes to the private playable build.
Do not copy material from a private research checkout into a public commit to
make this optional target pass. A future public content pack needs its own
reviewed provenance and a documented generation/import workflow.

## Provenance comments

Some source comments name research files, frame observations, or generated
tables that are not distributed here. Those comments preserve the origin and
units of existing logic; they are not public download links or proof that a
contributor has reproduced the measurements. Code review and public contracts
remain useful. If a proposed gameplay change needs missing evidence, identify
the exact input needed and continue with an independent task.

Passing the public contracts proves their bounded assertions. It does not prove
original-game visual/audio parity, a complete level, or campaign completion.

## Original content contributions

Artwork, sound and music must be original or explicitly licensed for inclusion.
Describe the author, license and source files in the pull request. The initial
snapshot excludes media by default; coordinate a proposed content path and
provenance before changing that boundary.
