# CloneLayer GIMP 3 port: scope and evidence

Implementation date: 2026-10-02. Legacy source: `afa43fae3e920210146abed514f136fd49f671b5`.
This document records the implemented feature slice. It does not mark the WBS
parents complete or claim XCF round-trip/UI integration is complete.

## Implementation and ownership

- `app/core/gimpclonelayer.{h,cpp}` registers an actual `GimpLayer` subclass with
  exact current C vfunc signatures and the `GimpPickable` interface
- One `CloneSlot` in the shared `BindingStore` owns the C++ implementation;
  no old `NewGClass`, `Interface::cast`, additional qdata store, or second binding
  implementation exists
- `CloneLayerRef` in `gimpclonelayer-handle.hpp` owns
  `ObjectRef<GimpCloneLayer>` and exposes retain/adopt/sink/create; its source
  operation returns an owning `ObjectRef<GimpLayer>`. Named methods delegate to
  the C API. `gimp-painter-type-traits.hpp` is the single guarded trait definition
- The source is weak, with scoped update/frozen/name/disconnect connections.
  Exact signal adapters acquire a weak owner lease and check its BindingStore
  generation. Closing disconnects all handlers and cancels deferred refresh
- A failed instance-init/constructed boundary is recorded by the read-only
  `binding-failed` property. A generic constructed object is inert and closed;
  the factory checks the active slot and releases a partially constructed object
- Source replacement during nested buffer/graph signals invalidates the reference
  generation. A temporary projected buffer is committed only if still current;
  a replacement occurring during refresh gets one deferred owner-weak refresh.
  Thaw/freeze/source-update reentry is generation-checked, including explicit
  close or replacement inside the preview thaw callback
- `GimpDrawable` has a C++-safe `priv` spelling for the C `private` pointer,
  without changing its position/type. A real C/C++ sizeof/offsetof comparison
  and the public handle compile/runtime test verify the layout

## Legacy source contract and current connections

| Behavior | Legacy evidence | Port and test |
|---|---|---|
| Lazy recursive pre-order, first exact-name match | `gimpclonelayer.cpp:306–334` | Parent compared before children, then next sibling; cycle-safe container visit set; `recursive_first_match` |
| Missing saved name remains pending | `:306–344` | No eager fallback; original text retained; `deferred_name` |
| Direct setter does not clear a pending name | `:277–303` plus `:431–440` | Setter emits source update; the old update-size lookup is preserved, including resolving an already-present name during that update |
| Actual source, not name, survives rename | `:277–303` | Weak identity retained; last name remembered safely; `deferred_name` |
| Update uses projected source pixels | `:477–562`, `gimplayer-project.c:38–156` | Committed projected ROI feeds inherited `gimp:buffer-source-validate`, then the clone's own offset/mode/mask graph; `projected_pixels`, `partial_update_and_graph` |
| Source opacity/applied mask are baked | `initial_inten_a_pixels`, `INT_MULT/INT_MULT3` | Original byte rounding preserved, not a double-opacity graph link |
| Show-mask bypasses opacity | `gimplayer-project.c:50–68` | Opaque grayscale mask output; measured pixel fixture |
| Source visibility ignored; blend mode ignored except Dissolve | `gimplayer-project.c`, `initial_sub_region` | Explicit initial-region projection, not final source mode graph |
| Group source includes child projection | Legacy group source fixture | Flush completed group pixels before copying; `duplicate_and_group_update` |
| Move only changes clone offset when source size changes | `gimpclonelayer.cpp:496–510` | Previous offset updated for every source update; only resize applies the delta; `offset_size_and_noop_transforms` |
| Scale/flip/rotate/transform are no-ops | `:382–425` | Exact current slots intentionally empty; same test |
| Pickable opacity is zero | `:434–438` | Current `gdouble` slot returns 0.0; pixel presence remains independent |
| Clone pixels not editable; position can move | `:453–456` | Current `is_content_locked` always true; inherited position lock and translation retained |
| Direct CloneLayer duplicate keeps original source identity | `gimpclonelayer.cpp:368–377` | Direct duplicate preserves the existing source before any caller-level remap |
| Whole-group duplicate remaps internal references | `gimpgrouplayer.c:416–527` | One original→copy map across all nested descendants, populated before remap; external identities retained; `group_duplicate_internal_reference`, `group_duplicate_complete_hierarchy` |
| Delete with Undo retains source identity | Genuine `CLONE_UNDO` capture | Undo owns removed source; weak binding survives and resumes update after undo/redo; `source_delete_undo` |

## Genuine old-runtime measurements

Source: `migration/fixtures/legacy-runtime/clone-capture.log`; the fixture worker
preserves the instrumentation patch and legacy build provenance separately.
For source RGBA `(204,51,102,128)` at opacity 0.5:

- Clone alpha 64
- Applied mask 128 gives clone alpha 32
- Show-mask gives `(128,128,128,255)`
- Hidden source and Multiply source keep `(204,51,102,32)`
- Dissolve gives 35 nonzero alpha bytes out of 256; SHA-256
  `e391af9278f0852484c254d50220eca234a3b257f98ddc489ba402ac9182f9eb`
- Same-size source move `(0,0)→(5,7)` leaves clone `(32,30)`
- Source resize `16×16→20×18`, offset `(5,7)→(7,10)` moves clone to `(34,33)`
- Scale request `13×9` at `(-5,4)` leaves clone `20×18` at `(34,33)`

The port tests compare these exact byte values and the complete Dissolve alpha
hash. Gray u8, float-linear and RGB/Gray double linear/nonlinear tests are current-platform adaptation checks,
not claims that old GIMP supported floating-point layers. Higher precision keeps
the source TRC and uses double intermediates. The double tests predeclare a
2e-14 absolute tolerance (under 100 machine eps at these sample magnitudes),
which rejects float truncation and unintended nonlinear/clipping conversion.
Row storage uses checked size_t multiplication and a 64 MiB row budget.

## Intentional safety repairs and saved-state distinction

The old borrowed source pointer and frozen connection could outlive the source.
The port detaches safely on source disposal, balances preview freezing, and
retains last-known text. Explicit detach retains cached pixels. Source state is
queryable without resolution as NONE, PENDING, LIVE, or EXPIRED, so a writer can
preserve information without converting detached/expired references into new
name lookups. `dup_source_name` never dereferences a dead pointer.

The old cycle callback was an empty TODO. Self, mutual-clone and ancestor-group
cycles retain their reference and last completed pixels but suppress recursive
updates/freeze mirroring. They are not rejected or silently rebound elsewhere.
The old CloneLayer duplicate dropped unresolved text; the port preserves pending
unresolved text (also when a live source coexists with a missing pending name),
while an explicitly detached duplicate stays detached. The legacy group caller
then remaps cloned references whose sources belong to the copied hierarchy.
The port follows that caller contract with one complete original→copy table:
normal/internal, external, nested cross-sibling, group/root-as-source and pending
records are tested. Direct clone duplication is distinct from whole-group
reference remapping. An earlier test/claim conflating them was corrected before
integration.

## Source-resize Undo parity

The separately sealed genuine capture is
[`legacy-clone-undo/capture.log`](../fixtures/legacy-clone-undo/capture.log), with
structured observations and source/binary provenance alongside it. It uses a
source 16×16 at (5,7), clone 16×16 at (32,30), then source resize to 20×18 with
offsets (-2,-3). The original operation
records clone DrawableModUndo before source DrawableModUndo in the group's
execution/list order. The clone's saved buffer is 16×16 at (32,30).

| Stage | Clone geometry | Undo/Redo stack depths |
|---|---|---|
| Baseline |16×16 at (32,30)|0/0|
| Resized |20×18 at (34,33)|1/0|
| Undo1 |16×16 at (30,27)|2/1|
| Redo1 |20×18 at (34,33)|4/0|
| Undo2 |16×16 at (30,27)|4/1|
| Redo2 |20×18 at (34,33)|6/0|

Both with/without a white clone-owned mask match these results. The mask stays
16×16 at (32,30) throughout. These non-ideal offsets and growing undo history are
real legacy behavior; the port does not silently replace them with idealized
undo. The old NULL-context mask resize emitted guard warnings; the port preserves
its no-resize result without calling that invalid API. The earlier history-free
cache resize and mask auto-resize were corrected after this runtime comparison.

## Explicit reference edits and persistence snapshot

The legacy source setters keep their original no-reference-Undo behavior. New
opt-in `set_source_with_undo` and `set_source_name_with_undo` APIs use
`GimpCloneLayerUndo`, a `GimpItemUndo` subtype whose state lives in the same
BindingStore framework. They preserve none, live, expired, pending, and
pending-plus-live states, the last source name, lookup policy, cached pixels,
geometry and previous source geometry. Weak source/image identities prevent an
Undo record from keeping a foreign image alive. Rename, source relocation while
unbound, closed foreign images, repeated replay and close-during-edit are tested.
Live replay refreshes current source pixels without opportunistically resolving
a pending name. A reference edit groups dependent clone cache Undo records as
one user operation and avoids a duplicate cache record on the edited clone.
Legacy dependent cache-resize replay may still grow history; this is not a
universal history-growth repair.

`dup_reference` returns a retained source plus copied names and state without
resolving names, updating pixels, or emitting signals. `restore_reference`
validates and installs that metadata without projection, resizing or Undo.
The `allow_name_lookup` flag lets versioned stable-ID loading preserve a missing
ID as unresolved even when an unrelated same-name layer exists. Explicit legacy
name assignment enables lookup again. Snapshot, restore, direct duplicate and
Undo all preserve this policy, including pending-plus-live and expired metadata.
The XCF codec is integrated separately using this API.

Owner and source image disconnect/relocation connections close an owner binding
or expire a closed-image source while retaining readable metadata. These guard
feature callbacks. The separately scoped GimpItem lifetime repair now tracks its
image and ID-table owner with main-thread weak pointers. The retained-handle
regressions and limits are documented in `gimp-item-lifetime.md`.

Ordinary precision conversion uses inherited image/drawable Undo and is tested
through u8-nonlinear to float-linear conversion and replay. The dormant legacy
Clone conversion Undo helper was not a live caller path and is not revived.
Current RGB-to-RGB direct/group copy and source relocation tests preserve direct
external identity and remap copied internal group references. The separately
sealed `legacy-clone-cross-image` package captures the same scene in the real
old application. `compare_clone_cross_image.py` compares all 19 records exactly:
4 reference-identity groups and 15 pixel/geometry samples, including independent
internal-source edits, live source relocation, original-image closure and later
updates. `clone-cross-image-comparison.json` binds both logs by SHA-256. No
Clone implementation change was needed for this comparison. Original startup,
transient unattached-projection and dangling-pointer teardown warnings remain in
the legacy log; the port's weak lifetime repair intentionally avoids the latter.

## Verification

- `app/tests/test-gimp-clone-layer.c`: 37 full-GIMP integration cases
- `app/tests/test-gimp-clone-layout.cpp`: C++ typed factories/return and actual
  C/C++ struct layout, included in the same executable
- `migration/tests/clone-layer-testlog.{txt,json}`: normal Meson results, including
  RESIZE_TRACE/UNDO_NODE/UNDO_BUFFER records and assertions against the separately
  captured legacy source-resize Undo behavior
- `migration/tests/clone-layer-sanitizers.json`: focused ASan/UBSan results
- `migration/tests/run_clone_layer_sanitizers.py`: reproducible instrumentation
  of CloneLayer, GimpItem lifetime, group duplication, test adapters and shared BindingStore, linked to the existing
  full GIMP test harness. Remaining upstream/dependency code is uninstrumented;
  leak detection is disabled. This is not a whole-GIMP sanitizer claim.
  Instrumenting existing group code at `-O1` emits a `mask_buffer` may-be-
  uninitialized warning in the unchanged `gimp_group_layer_resume_resize` path;
  the new remapping code has no such warning and the normal build is clean

Reproduce with the project build environment loaded, holding the shared build
lock: build `app/tests/gimp-clone-layer`, run Meson test `gimp-clone-layer` with `--logbase clone-layer`, save the matching
logs using `save_clone_test_evidence.py` (explicit environment allowlist and
trailing-whitespace normalization), then run the sanitizer script with build directory and `--report` output path.

## Integration status and remaining work

The original core slice ended before XCF and GUI integration. Those historical
limitations are now superseded by the explicitly joined evidence in
`migration/acceptance/clone.json`: the provenance checkpoint includes normal and
sanitized Save/reopen/source-edit, missing-ID non-retargeting, duplication and
external-reference tests; the common-store dialog checkpoint includes real
creation/selection, source editing and opt-in reference Undo. Ordinary legacy
setters still do not add reference Undo. Both integration reports retain their
exact source archives; they are not claims that a later combined tree has been
retested. The matrix validates all26 WBS14 obligations and preserves their original
dependency gates rather than automatically checking the parent tasks.

- The complete saved-field matrix, positive same-name internal stable-ID
  checkpoint and combined-tree aggregate remain separate acceptance work
- Indexed fixtures, component-visibility changes, cross-image workflows beyond
  the measured ordinary RGB direct/group copy and live source relocation, full
  transform/resize Undo ordering and compound saved artworks remain broader
  comparison work
- Source-driven resize now records the legacy clone DrawableModUndo and keeps the
  clone-owned mask geometry unchanged. The ordinary RGB case with/without white
  clone mask is verified for two Undo/Redo cycles, including the old non-ideal
  offset/history behavior. Arbitrary compound resize/transform histories remain
  broader fixture work, not a claim made by this slice

- Existing baseline `save-and-export` failure is unrelated and is not attributed
  to this feature; normal clone tests do not claim the app suite is entirely green
