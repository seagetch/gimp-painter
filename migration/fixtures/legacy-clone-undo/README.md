# Legacy CloneLayer source-resize Undo observations

These are observations from the real rebuilt GIMP-Painter 2.8.23 core at
`afa43fae3e920210146abed514f136fd49f671b5`, captured on 2026-10-02. The process
exited with status 0, completing both valid RGB scenes and all eight ordinary
Undo/Redo calls. They extend, and do not replace, the sealed
[`legacy-runtime` fixtures](../legacy-runtime/README.md).

## Scene and actions

Each scene starts in a fresh 64×64 RGB image, with a 16×16 RGBA source at (0,0)
filled with opaque (204,51,102,255). A normal-mode CloneLayer refers to that
source and is placed at (32,30). The second scene also receives its own white
16×16 layer mask using `gimp_layer_create_mask()` and `gimp_layer_add_mask()`.

The harness moves the source to (5,7), emits a full drawable update, and clears
the setup Undo history. It then calls the ordinary API
`gimp_item_resize(source, user_context, 20, 18, -2, -3)`, followed by Undo,
Redo, Undo, and Redo. Each snapshot only reads state: no extra update or
projection flush is inserted between the resize/Undo/Redo calls.

## Observed geometry and history

Both scenes have the same source and clone geometry:

| State | Source size / position | Clone size / position | Undo / Redo depth |
| --- | --- | --- | --- |
| Baseline | 16×16 / (5,7) | 16×16 / (32,30) | 0 / 0 |
| Resized | 20×18 / (7,10) | 20×18 / (34,33) | 1 / 0 |
| Undo 1 | 16×16 / (5,7) | 16×16 / (30,27) | 2 / 1 |
| Redo 1 | 20×18 / (7,10) | 20×18 / (34,33) | 4 / 0 |
| Undo 2 | 16×16 / (5,7) | 16×16 / (30,27) | 4 / 1 |
| Redo 2 | 20×18 / (7,10) | 20×18 / (34,33) | 6 / 0 |

The clone-owned mask stays **16×16 at (32,30)** throughout. The real internal
mask resize emits six `GIMP_IS_CONTEXT (context)` guard criticals. The harness
passes a valid user context for the source resize; the unchanged legacy
CloneLayer's automatic resize passes NULL internally. All warnings remain in
the log.

The initial resize produces one `GimpUndoStack` (undo type 22), with two
`GimpDrawableModUndo` children (type 48). In list order, which this legacy
stack uses for execution, the clone comes first, storing 16×16 at (32,30),
then the source, storing 16×16 at (5,7). Undo generates additional clone
entries. After Undo, the top Redo group lists source then clone, storing their
20×18 positions (7,10) and (34,33). The first Redo changes the saved clone
snapshot in the returned Undo group to 20×18 at (32,30). Full typed fields and
each top group's child ordering are in `observations.json` and `capture.log`.

These surprising positions, mask extents, and history depths are measured
legacy behavior. This package makes no judgment about whether a future port
should preserve or deliberately correct them.

## Provenance and reproduction

- `capture-clone-undo.inc` contains only scene construction, ordinary editing
  API calls, and observations through declared, GType-checked Undo structures
- `source-overlay.patch` is the complete tracked source overlay on the pinned
  reference, including the previously documented build compatibility changes,
  the new test entry point, and inherited FilterLayer logging instrumentation
- No CloneLayer, Undo, resize, layer-mask, XCF, or compositor implementation is
  altered for this capture; the inherited FilterLayer code is not exercised
- `capture-report.json` records the build command, source and binary hashes,
  exit codes, and reference build-report hash
- `build.log` preserves compilation warnings; `run-capture.sh` records the
  exact execution environment and 60-second process timeout
- `capture.log` preserves the full run, including two startup GValue criticals
  and a Script-Fu pipe warning at process teardown
- `observations.json` is parsed from the run's `UNDO_STATE`, `UNDO_NODE`, and
  `UNDO_ACTION` lines; no expected trace was synthesized
- `manifest.json` seals every other file in this package by size and SHA-256

Use the build dependencies and recipe in
[`migration/baseline/legacy`](../../baseline/legacy/README.md), applying this
complete overlay to the pinned source instead of applying its constituent
patches twice. The run script retains the actual capture's absolute paths.
The instrumented ELF binary's SHA-256 is
`47976bd527f7cd7f2e6b7cc5558d98fb03d7022b596fd16adf57520f6dbb24be`.

This is a headless core capture, not a GUI test or a pixel-content comparison.
Only each stack's top entry is recursively enumerated; total depths are
reported separately. The integrity test validates the sealed evidence without
running the legacy executable:

```sh
python3 -m unittest discover -s migration/tests -p 'test_legacy_clone_undo_fixtures.py' -v
```
