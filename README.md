# Mega Man X Engine

A C++17/raylib fan engine working toward **X1, X2 and X3 in one game**.
This is the public source repository for code, tests and community contributions.

**This is not a playable game download.** The default build compiles a static
engine library and runs asset-independent contracts. ROMs, artwork, music,
original dialogue and generated visual tables are not included.

[Project, roadmap and gameplay clips](https://megaman.davidluky.com/en/) ·
[Contribute with AI](https://megaman.davidluky.com/en/contribute/ai/) ·
[Português](https://megaman.davidluky.com/)

## Project status — September 19, 2026

The [feature checklist](https://megaman.davidluky.com/en/roadmap/) now breaks
X1 into **192 features and 517 acceptance steps**, with **7.92% verified** under
the revised weighting. This is a scope/acceptance index, not a measure of time
spent or a claim that the remainder has no implementation. Reusable enemy
families count once; stage-specific placement and interaction count separately.

Expand features to see available artwork and dated native-engine clips,
including movement, dash, Buster, Homing Torpedo, the drifting bat, Walker and
Axe Max. Captures retain their limitations and do not prove a whole feature.
The [machine-readable plan](https://megaman.davidluky.com/downloads/X1-COMPLETION.json)
and [readable plan](https://megaman.davidluky.com/downloads/X1-COMPLETION.md)
define the complete X1 scope. X2, X3 and extra modes are separate expansions.

**Original C++ source synchronized September 19, 2026.** The accepted engine
implementation now matches the development baseline, with two deliberate
dialogue exclusions. Assets, generated visual tables and private research remain
separate, so this is still an asset-independent contribution checkout, not a
playable download. See [source provenance and contributions](docs/project-status.md).

## Build and test

Requirements: C++17 compiler, CMake 3.20+, Ninja, raylib and nlohmann_json.
The checked Windows environment uses MSYS2 **MINGW64**, raylib 5.5 and
nlohmann_json 3.12.0. No Python, ROM or emulator is needed for these contracts.

In the MSYS2 MINGW64 terminal, install the build dependencies:

```sh
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-raylib mingw-w64-x86_64-nlohmann-json
```

Then clone and build in that terminal:

```sh
git clone https://github.com/davidluky/megaman-x-engine.git
cd megaman-x-engine
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 8
ctest --test-dir build --output-on-failure
```

From PowerShell, first add your MSYS2 MINGW64 `bin` directory to `PATH`.
For the default MSYS2 installation:

```powershell
$env:PATH = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;" + $env:PATH
```

The default target is `mmx-engine-core`; `BUILD_TESTING` is ON. The initial
snapshot includes **15 contracts** for input/settings, stage identity,
passwords, randomization and several deterministic entity models.
The native library build and 15/15 contracts passed on Windows MINGW64.
Other platforms are welcome contributions and are not claimed as tested.

`MMX_PUBLIC_BUILD_ENGINE=ON` is an advanced local option that fails explicitly
when required external reference includes are absent. It is not the newcomer
build path. See [the data boundary](docs/reference-data.md) for what the static
library and tests do and do not prove.

## Make one useful contribution

1. Read [CONTRIBUTING.md](CONTRIBUTING.md) and choose one small task.
2. Fork the repository, clone your fork and create a branch.
3. Ask your agent to read [AGENTS.md](AGENTS.md), then reproduce or investigate
   the selected case using public inputs.
4. Run the relevant tests and open a pull request with exact results and limits.

Humans review and integrate contributions. An AI-generated patch follows the
same review process. Do not invent gameplay measurements or claim tests passed
when you could not run them. Documentation, portability work, focused contracts
and original art/audio proposals are also welcome.

## Project status and vision

X1 is the current campaign priority. The public roadmap uses a checklist of
verified deliveries: **20/100 X1 points**, not a percentage inferred from code
or test counts. X2, X3 and future modes have no accepted points yet.

The longer-term vision includes shared weapons and abilities across campaigns,
Zero's X3 support extending into X1, unlockable Vile, Bloody Palace, a roguelike
mode and a community map editor. These are plans, not shipped capabilities.
The [live roadmap](https://megaman.davidluky.com/en/roadmap/) explains the scale
and shows current evidence and limits.

## License

Original public code and documentation use the [MIT license](LICENSE).
[Third-party notices](THIRD_PARTY_NOTICES.md) identify dependencies and references.
Mega Man X, its characters and related game assets belong to their respective
owners. This independent project is not endorsed by Capcom. The MIT license
does not grant rights to ROMs, original art, music, dialogue or trademarks.

**PT-BR:** o código está aberto para contribuições humanas ou com IA. Os comandos
acima compilam o núcleo e executam testes sem ROM. O jogo completo e seus assets
não são distribuídos neste repositório. Comece pelo
[guia em português](https://megaman.davidluky.com/contribuir/ia/).
