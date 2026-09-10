# Public engine architecture

The public repository starts with an asset-independent engine core. Its code
can be reasoned about and tested without distributing a ROM, original art,
captured audio, private measurement data, or generated visual tables. A later
full-engine layer may add a compatible public content pack when the maintainer
has reviewed its provenance and build requirements.

## Runtime shape

When a complete runtime is available, the flow is:

```text
process entry
  -> application shell and scene stack
  -> UI or gameplay scene
  -> entities and reusable systems
  -> deterministic physics and collision helpers
  -> data/content services and rendering/audio adapters
```

The asset-independent core stops before any required game content is loaded.
Tests for that core should construct small values in memory or use temporary
files, so they remain useful in a clean checkout.

## Ownership boundaries

| Area | Owns | Does not own |
|---|---|---|
| Application shell | Process boot, fixed-step loop, scene routing, input facade, shared runtime state | Enemy rules, tile collision policy, or content schemas |
| UI scenes | Menus, configuration screens, and presentation of user state | Physics, entity behavior, or file path policy |
| Gameplay scenes | Scene orchestration, spawning/simulation/rendering coordination, and transitions | Generic file loading or reusable collision mathematics |
| Entities | World state, hitboxes, combat state, and per-entity updates | Scene transitions and content-pack parsing |
| Systems | Reusable services such as animation, camera, audio, resource cache, and weapon inventory | Ownership of a particular scene's policy |
| Physics | Deterministic movement, tile interaction, and projectile collision helpers | Rendering, JSON parsing, or invented tuning values |
| Data services | IDs, path containment, JSON I/O, settings, saves, and content-pack interfaces | Per-frame simulation and rendering |
| Harness and contracts | Finite scenarios and assertions for public behavior | Gameplay implementation or a substitute for source evidence |

An API crossing two areas should make the ownership boundary visible. For
example, a physics helper may set contact flags, while gameplay decides what a
contact means. A data loader may report a parse or schema error, while the
caller decides how to present it.

## Content and generated-data boundary

The public starter does not contain a playable content pack. The full-engine
layer must treat content as an explicit dependency and fail clearly when it is
absent. Generated includes that encode captured visual or audio material are
also outside this public onboarding package. They must never be copied from a
private checkout merely to make a build appear complete.

This boundary keeps source, tests, and original contributions reviewable. It
also prevents a path or filename from being mistaken for proof that an enemy,
attack, collision, or stage matches the original game.

## Testing boundary

Public contracts should prefer deterministic inputs and no window, audio
device, ROM, or asset load. A contract should state what it proves and what it
does not prove. Core examples include coordinate transforms, path containment,
JSON read/write results, and animation state transitions.

Runtime or visual checks may be added later when their public content and
environment are documented. Passing a core contract is evidence for that
contract only; it is not a parity score, a complete-game claim, or a license
for original media.

## Portability boundary

Keep platform-specific filesystem and window details behind narrow adapters.
Use C++17 standard-library types at core boundaries, and keep raylib-dependent
code isolated where a headless contract does not need it. A portability change
must state the host assumptions it tested and leave unsupported content or
generated-data requirements explicit.
