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
| Update uses projected source pixels | `:511–586`, `gimplayer-project.c:38–153` | Committed projected ROI feeds inherited `gimp:buffer-source-validate`, then the clone's own offset/mode/mask graph; `projected_pixels`, `partial_update_and_graph` |
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

- Historical core reports record 37 full-GIMP integration cases. The current
  scoped receipt below reruns all 58 registered cases in the combined Clone
  test executable (`test-gimp-clone-layer.c` and `test-gimp-clone-layout.cpp`).
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
- Indexed fixtures, genuine component-visibility runtime comparison, cross-image workflows beyond
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

## Component visibility regression scope (14.007/component-visibility-regression)

This independent verification child uses completed `14.001/core-implementation`,
`02.005`, and `09.002` under the existing WBS bounded-parallelization rule. It
does not close parent `14.007`, predecessor `14.006`, broader blend-mode, genuine
runtime, GUI or release gates. No eager-refresh feature or production change is
needed. The bounded child is accepted by the current source-bound receipt below.

The new expected bytes are derived from pinned legacy source
`afa43fae3e920210146abed514f136fd49f671b5`, not a new genuine runtime capture:

- `gimpclonelayer.cpp:277–303,511–586`: source update/frozen subscriptions, no
  image component-visibility subscription; source update projects its ROI.
- `gimplayer-project.c:95–144`: RGB/Gray without alpha selects
  INITIAL_INTENSITY, RGBA/Gray-alpha selects INITIAL_INTENSITY_ALPHA; both
  receive the image visibility array.
- `paint-funcs-generic.h:814–995`: hidden colors are zero and hidden alpha is
  zero, including generated alpha for a source without alpha.
- `paint-funcs-utils.h:22–28`: opacity 0.37 becomes byte 94; source alpha 101 and
  mask 93 yield INT_MULT(94,101)=37, INT_MULT(94,93)=34 and
  INT_MULT3(94,101,93)=14. The last result would be 13 with truncated or chained
  multiplication, making the regression sensitive to rounding changes.
- `gimplayer-project.c:49–63`: show-mask copies opaque grayscale and bypasses
  source opacity and component visibility. The existing genuine 128-mask fixture
  supports only that measured show-mask behavior, not the new toggle scenarios.

The six added nonlinear-U8 tests cover all 16 RGB/RGBA and all 4 Gray/Gray-alpha component
visibility combinations, both without and with an applied mask (80 scenes).
Every scene checks exact full-cache and GEGL source-node bytes before and after
toggle-only operations, an explicit 1x1 source update at (1,1), visibility
restoration, an update at a distinct ROI and a final refresh of the stale ROI.
Each explicit update must emit exactly one matching Clone update rectangle;
all five neighboring cached pixels must remain unchanged. Four further show-mask
scenes cover RGB/Gray with/without alpha at zero opacity with every component
hidden and mask bytes 0, 1, 93, 128, 254, 255.

Final image projection is distinct: current `gimp_image_get_graph` installs
`gimp:mask-components` with alpha 1.0. A separate RGB test demonstrates opaque
black final projection while Clone cache first retains its pre-toggle bytes,
then becomes transparent black only after source update. Gray final-projection
semantics are outside this assertion; no assumption about them is used to
derive Clone cache expectations.

### Accepted current receipt and retained attempts

`migration/tests/clone-component-visibility-evidence.tar.xz` contains the complete
strict receipt for run-004 on 2026-10-17:

- Tested source commit: `63be6dcf0aed2cf715576cfd63e723e614cd0172`
- Tested tree: `61deee83eeed8cf5a4a4a99c128beaf530694c81`
- Archive SHA-256: `421e89882a0edc1dd16864f2380854b7326d44b973ed81a4699313252528a20e`
- Receipt SHA-256: `c0f5de165fff330f541037420b586eab3d5906264d85134115029adb17f5057a`

The full default Ninja build passed before seven exact, unfiltered registered
Meson targets. The full 164-target registry supplies qualified identities and
commands; the selected targets contributed 222 positive TAP cases: `core` 4,
`projection-update-bounds` 3, `gimp-clone-layer` 58, `painter-layer-ui` 68,
`painter-layer-tree` 35, `painter-xcf-roundtrip` 49 and `update-geometry` 5.
All seven passed without skipped cases or timeouts. This uses the retained build
tree from the earlier attempts, with a complete default-build revalidation; it
is not a fresh-from-empty build claim.

Focused ASan/UBSan then passed all 58 Clone cases. Exactly eight source units
are instrumented. Another 54 linked production C++ units are rebuilt with RTTI
only to keep the mixed harness ABI coherent; they are not sanitizer coverage.
The eight instrumented objects are explicit early link anchors with original
copies removed from private direct/archive slots. The compatibility-only objects
remain archive-selected. UBSan vptr checks remain enabled; LSan remains disabled.
Upstream/dependency code outside those eight units is uninstrumented. This does
not establish whole-GIMP sanitizer coverage.

The receipt seals source, default-build freshness, compile database, full
registry, ELF/shared-library loader provenance and all 361 configured locale
catalogs before/after testing. The isolated sanitizer outputs do not modify the
normal build. Authenticated portrait Xvfb uses the existing helper. Public logs
retain test blocks and diagnostics; explicitly recorded transformations remove
inherited-environment headers from raw Meson output. The archive and original
receipt directory independently validate identically, and each archived member
matches its original bytes.

Two unsuccessful attempts remain separate failed evidence; no failed status or
diagnostic was replaced by the accepted receipt:

1. Run-001 at `bf90eab07f8043f2b3d540ca7d4cdd8f7d2b6bef` stopped in the default
   build because the evidence environment omitted `GIMP_DEPS_ROOT`, which the
   restored Python dependency environment requires. No tests ran. The narrow
   allowlist correction was separately reviewed. Retained archive
   `run-001-failed-evidence.tar.gz` has SHA-256
   `673a99d19beb06aa8eb5e942aaca8bd83d235ca91ad597038ce1427766d0686e`.
2. Run-002 at `b7ecd1ce2566fdf0b412e84bd311d00a7a0d815e` completed the full
   default build and all 222 normal cases, then failed sanitizer startup with a
   mixed-RTTI `HistoryState` shared-pointer control-block vptr diagnostic.
   Static ELF review confirmed a null typeinfo slot in a no-RTTI production
   vtable used by an instrumented template. The bounded RTTI-compatible link
   repair above retained vptr instrumentation; it did not suppress the check.
   Retained archive `run-002-failed-evidence.tar.gz` has SHA-256
   `594766b843177ea74c656aa5d6cf261ae2359e1825a313df54b04fb5b0c99c23`;
   the separate static diagnosis has SHA-256
   `32424ca7b61567f70aacf86b01e24f14084c48bfd84ed321be7a9cb96080f107`.

Run-003 at `bc68880847d4275781eb9482159441ec532f65ab` remains a native PASS
(222 normal and 58 focused sanitizer cases), retained with receipt SHA-256
`d9a8ce3178c26ea0510ebcffa8b8b4de617b294f2eee8bc1cf94bb0ee327c166` and
compact archive SHA-256
`24ac0e10a77af45683ab0873cd0238034da268738423a0ccf03bffa94d7892e4`.
It was superseded for final publication because the offline receipt reader
resolved archived loader aliases using live build-tree symlinks. The reviewed
reader correction resolves those aliases from the sealed symlink inventory,
without consulting the historical source or build filesystem. Run-004 repeats
the complete native sequence with that exact tooling source frozen; strict
absent-build validation also passes. This additional publication check does not
change the native result for run-003.

Historical core reports and their recorded source hashes remain unchanged. The
pre-existing four historical source-hash mismatches are recorded in the new
receipt's baseline evidence, rather than hidden by replacing expected hashes.
The current receipt supports only these actual current reruns. It does not
refresh older broad integration checkpoints, claim a new genuine legacy capture,
or close the parent, blend-mode, GUI, full aggregate, platform or release gates.

## Installed component-visibility portrait GUI smoke (2026-10-17)

Task `34.005/clone-components-portrait-smoke` is accepted only for this bounded
modern installed-application workflow. It is separate from the native component
matrix above. The original `14.007`, `14.006`, `34.005`, legacy UI-equivalence,
aggregate, platform, release and whole-port gates remain open. The historical
Edge runtime pilot remains **SKIPPED**; it was not resumed.

The exact tested public commit is
`1d2b621a15801601af95fd5e44cfc46f4a6ee750`, tree
`8addde80f58b198d1be02b5341cc6bcb34618cc0`. The existing official Debian dependency
environment was used for the complete incremental default target (96 steps,
exit 0), followed by Meson install `--no-rebuild` (exit 0). This was not a new
clean build or native aggregate rerun. The installed GIMP 3.0.9 process used a
fresh writable profile and authenticated loopback-only TCP Xvfb at
1080 by 1920, depth 24. A valid client connected and an empty-authority client
was rejected. Ordinary XTest mouse/keyboard input drove the actual application;
all 32 full-screen PNGs are unedited raw XGetImage captures.

### Observed bounded workflow

1. Open the 720 by 1080 synthetic u8 RGB study, enable View > Painter Canvas
   Controls, and name the ordinary visible source layer `Synthetic Source`.
   Actual Painter tile popup > New Clone Layer immediately creates the separate
   above-source `Clone Layer`, in Normal (Painter legacy) mode. Clone Source
   reports `Keep current reference: Synthetic Source (live)`. Choosing No source
   and then Cancel leaves that live reference unchanged on reopening.
2. Open Windows > Dockable Dialogs > Channels and click **only the Red eye**,
   without clicking a component row or thumbnail. On the original source, use
   Pencil (N), the solid `2. Hardness 100` brush at 140 image pixels, 100% opacity,
   dynamics disabled, and white foreground. A single local dab A is made while
   Red is hidden. Restoring Red leaves A cyan in the visible Clone; hiding only
   Clone reveals white A in the source. This also confirms that Red remained
   editable while its display eye was off.
3. The actual refreshed cache contains a **rectangular** component-masked area
   around the dab: the sun is green there and orange outside it. The local
   paint-update rectangle is wider than the round brush footprint; no
   brush-footprint-only update claim is made. Showing Clone and toggling Red
   off/on again without a source edit produces byte-identical full screenshots
   (`017-red-restored-cyan-clone-a.png` and `019-no-eager-toggle-rewrite.png`).
4. Save `clone-components-mixed.xcf`, close the view, and reopen through ordinary
   Open. Restore the same centered 66.7% view. Cyan Clone A, white source A and
   the live source reference survive. The visible rectangle `[302,625,778,1340)`
   is identical before/reopen: zero differing pixels out of 340,340. This is
   explicitly a screenshot rectangle comparison, **not full-buffer numerical
   acceptance**.
5. After reopening, with every component visible, a second white source dab B
   becomes white in Clone while A stays cyan. The fixed visible A rectangle
   `[510,705,646,841)` has zero differing pixels before/after B. Painting source A
   black and then white updates Clone correspondingly and repairs its local
   rectangular surroundings. Save to the separate `clone-components-repaired.xcf`.
   The original mixed XCF remains byte-identical. Optional reference detachment
   and reattachment were intentionally omitted.
6. Ctrl+Q exits normally with status 0. The display shuts down and the shared
   lock is available. All 13,803 tracked source/submodule inputs have identical
   pre-build, post-build and post-GUI manifests. The 278 installed bin/lib/libexec
   file-or-symlink entries and both executable hashes are unchanged across GUI
   execution; installed data assets outside that inventory are not all sealed.

### Compact receipt and external evidence

The full JSON receipt, original screenshots, synthetic input, both XCF files,
ordered actions, sanitized diagnostics and source/runtime manifests are retained
in the external archive below. Archive readback verified every package-manifest
digest. Profile, authentication files and raw environment are excluded; text
roots and the D-Bus machine ID are replaced with placeholders, with raw/published
hash mappings recorded. Screenshot and XCF bytes are unmodified. The hashes of
source/runtime manifests below identify the original local seals; the package
also records the hashes of their sanitized copies.

```json
{
  "task": "34.005/clone-components-portrait-smoke",
  "result": "PASS: bounded synthetic modern Clone GUI workflow",
  "tested_commit": "1d2b621a15801601af95fd5e44cfc46f4a6ee750",
  "tested_tree": "8addde80f58b198d1be02b5341cc6bcb34618cc0",
  "archive": "clone-portrait-gui-run-001-evidence.tar.xz",
  "archive_bytes": 4950916,
  "archive_sha256": "98d43d5dac93c867b95647c8a4c62a43e867beac2672fb75b2e78fbe19ba2d0f",
  "archive_member_files": 133,
  "full_receipt": "clone-portrait-gui-run-001/clone-portrait-gui-receipt.json",
  "full_receipt_sha256": "02d0b4903d981934c9ac4210194cc942b5030bee5ede05c5071edffe24bf831f",
  "source_manifest_before_after_sha256": "c84df0c13c5311b0bfca3cf5153ba893688ac56218b6e13d64abaa6fef61f31d",
  "runtime_manifest_before_after_sha256": "f169a1122d79436356cf2a51e53e815407d55e30d42c7ad0d029c25c3a5654d6",
  "build_executable_sha256": "b00ae9ca9293850f870aa406a9dfe8e7945366d574127c3526407ca9c6cbd54d",
  "installed_executable_sha256": "5846acf20af9fc700bd04423ce9959d30ffcba95ca9390b868acd24492a86701",
  "both_elf_build_id": "b885006f1f815a905b18d1754af974c01539a3c6",
  "both_text_section_sha256": "2208ecb22ccf65fb89adfa273d6ff39e2917fa976c1624048761b6571b3d5ed1",
  "mixed_xcf_bytes": 253740,
  "mixed_xcf_sha256": "293a1205979b967f2a87c7a47fba69c9427e57e6fca1b14494bfe585adf957f7",
  "mixed_xcf_preserved": true,
  "repaired_xcf_bytes": 263282,
  "repaired_xcf_sha256": "b63192d94f8499e593de3963659413197f765c2c224cae57ac1b7cb6e6060cd5",
  "normal_quit_exit": 0,
  "full_buffer_numerical_claim": false,
  "whole_port_or_parent_gate_acceptance": false
}
```

Install removes build RUNPATH; matching ELF Build ID and `.text` hashes were
verified without modifying either binary. The incremental build logged zero
compiler-warning lines, which does not establish a clean warning-free build.
The GUI retained 112 ignored unsupported fonts, unavailable D-Bus and 67 dconf
warnings (`dbus-launch` child exit 1); CRITICAL and segmentation-fault mentions
were zero. No warning-free application claim is made.

This evidence covers one synthetic u8 RGB modern GUI workflow, not the full
RGB/Gray/alpha/show-mask matrix, historical runtime equivalence, compound works,
other operating systems or input devices, full sanitizer/LeakSanitizer coverage,
exhaustive shutdown, or release acceptance. This later documentary publication
is not the exact executable source tested above. Native run-004 evidence and
all its input/receipt bytes remain unchanged.


## Source-mode projection regression scope (14.007/source-mode-projection-regression)

This bounded independent verification child uses completed
`14.001/core-implementation`, `14.007/component-visibility-regression`,
`10.005/normal-multiply-parity`, `02.005`, and `09.002` under the existing WBS
parallelization rule. Parent `14.007` and predecessor `14.006` remain open. This
tests-only child is accepted by the source-bound native receipt below. The
accepted component-visibility and installed portrait GUI receipts above, and
every historical source hash, remain unchanged.

Expected bytes are independently derived from pinned legacy source
`afa43fae3e920210146abed514f136fd49f671b5`; this is not a new genuine old-runtime
oracle or a broader blend-compositor parity claim:

- `app/core/gimpclonelayer.cpp:511–585` calls source `project_region` with
  `combine=FALSE` for the update ROI and then notifies the same Clone ROI.
- `app/core/gimplayer-project.c:95–144` dispatches RGB/Gray without alpha to
  `INITIAL_INTENSITY`, RGBA/Gray-alpha to `INITIAL_INTENSITY_ALPHA`, and passes
  opacity, applied mask, mode and visible components to `initial_region`.
- `app/paint-funcs/paint-funcs.c:3947–4016` only special-cases Dissolve in these
  initial-region paths. Every other mode uses the same initial intensity
  functions; the source's final blend operation is not the Clone cache oracle.
- `app/paint-funcs/paint-funcs-generic.h:814–995` copies visible color channels
  and calculates generated/source alpha using the single-rounding macros in
  `app/paint-funcs/paint-funcs-utils.h:22–28`. With opacity 0.37 (byte 94), alpha
  inputs `[0,1,101,128,254,255]` produce `[0,0,37,47,94,94]` without a mask and
  `[0,0,14,17,34,34]` with mask 93. No-alpha sources produce alpha 94 or 34.
  Replacement alpha 203 produces 75 or 27. These literal expectations are not
  calculated by the implementation under test.
- `app/core/gimplayer.c:2052–2077` emits a full-layer drawable update only when
  the stored mode changes; assigning the same mode does nothing.

The explicit 27-entry raw-to-modern test table contains raw 0, 3–21 and 23–29.
Raw 0 and 3 map to `PAINTER_NORMAL` and `PAINTER_MULTIPLY`; 4–21 map to the named
legacy enums (including the old Overlay value); 23–29 map to the seven named
Painter Erase/Replace/Anti-erase/Src-in/Dst-in/Src-out/Dst-out enums. The test
compares the mapper result against this literal table and asserts ordinary-layer
context before constructing the source. It does not cast colliding raw values
into modern enums. Current mapping is in
`app/operations/layer-modes-legacy/gimpoperationpainterlegacy.c:94–114`;
ordinary-layer admission is in `app/operations/layer-modes/gimp-layer-modes.c`.
Raw 2 (Behind) and 22 (Color erase) are excluded because their mapped contexts
do not admit ordinary layers. Dissolve is covered only by earlier fixtures;
this child does not expand its coverage.

Four nonlinear-U8 cases, `source_mode_projection_rgb`,
`source_mode_projection_rgba`, `source_mode_projection_gray` and
`source_mode_projection_gray_alpha`, each cover 27 modes × two mask states:
216 scenes total. Each scene uses a 3×2 varying-color source (varying alpha
where present), opacity 0.37, all components visible, and either no mask or an
applied mask filled with 93. The test compares every byte in the committed
Clone cache and its GEGL source node. It then changes to a distinct legal mode
and reverts, requires exactly one full 3×2 Clone notification for each change,
and requires no notification on reassigning either stored mode. Finally it
changes the source pixel at (1,1), checks one exact 1×1 notification, and checks
the full cache/source node so all five neighboring pixels remain unchanged.
All expected bytes are independent of the selected blend mode.

The current implementation's only mode-dependent initial-region branch is
Dissolve in `app/core/gimpclonelayer.cpp:353–443`. Modern stored/effective-mode
notification paths are in `app/core/gimplayer.c:1833–1842,2652–2719,2949–3007`.
Show-mask transitions need their own contract: effective Normal can suppress
an update even when the stored mode changes. They are intentionally excluded
here, together with groups, indexed/high-precision images, full image
composition, XCF admission, GUI, platforms, full aggregate and whole-port gates.
The broader source-mode gap remains open outside this exact initial-region
slice. No old Edge/runtime build or execution is part of this work.


### Accepted source-mode receipt (2026-10-17)

`migration/tests/clone-source-mode-projection-evidence.tar.xz` preserves the
complete public strict receipt for `source-modes-run-001`:

- Tested source commit: `7ab1230c1e1d77cc143f93f71e44a16aea021009`
- Tested tree: `c2785ea8a438ab6ac0c9284da6c4604520099172`
- Archive SHA-256: `65ed9e7b9343e3a745220350cda057e681c7e025a25eafb60d71e86497ff015d`
- Receipt SHA-256: `c341014c3892ed7400cbae3bcf9bebcd884eb25a32e32cab42b37dc3ebd2fa6a`
- Test C source SHA-256: `8da47900b3fac3e8c33cd94fad458ac06852800bfa64bc409a1a123c428508ae`

The unchanged reviewed harness completed the full default incremental Ninja
build, then all seven exact, unfiltered registered Meson targets: `core` 4,
`projection-update-bounds` 3, `gimp-clone-layer` 62, `painter-layer-ui` 68,
`painter-layer-tree` 35, `painter-xcf-roundtrip` 49 and `update-geometry` 5.
All 226 positive TAP cases passed without skipped cases or timeouts. The
retained configured build was revalidated; this is not a from-empty build.

Focused ASan/UBSan passed all 62 Clone cases. Both normal and sanitizer output
contain exactly 216 distinct mode/type/mask scene records for the four new
source-mode cases. Exactly eight source units are instrumented, while 54 linked
C++ units are rebuilt with RTTI only for compatibility. Those 54 units are not
sanitizer coverage. UBSan vptr checks remain enabled and LSan remains disabled;
upstream/dependency code outside the eight-unit scope is uninstrumented. No
production Clone code or evidence-tooling change was required. The full default
build and sanitizer console logs contain no compiler warning lines.

The strict receipt verifies unchanged source, default-build freshness, compile
database, full 164-target registry, ELF/shared-library loader provenance and all
361 configured locale catalogs before and after testing. Sanitizer artifacts
are isolated from normal build objects. An authenticated 1080×1920 Xvfb supports
the tests; this is not another installed GUI smoke test. The archive is distinct
from the accepted component-visibility archive, retains every public receipt
member unchanged, and independently validates against the original receipt
directory. Explicit logged transformations remove inherited-environment headers
from raw Meson output; no test or diagnostic evidence is dropped.

The independent source review verified the literal mapping, pinned-source
rounding, mask getter preconditions, mode notifications/no-ops and actual pixel
mutation checks before native execution. The unchanged strict evidence and
archive controls pass 44 and 17 cases. Source-only preflight acceptance checks
intentionally rejected the older 58-case component receipt after four test
registrations were added. Those stale-receipt and initially uninitialized
submodule checks are retained in external checkpoint records; they are not
failed native runs and do not replace the older accepted receipt. The first
native source-mode attempt passed. Historical core reports and their four
known historical source-hash mismatches remain unchanged; the new source-bound
receipt supports only its actual rerun and the bounded child described here.

The original GUI receipt still records its exact tested `1d2b621a` source and
the earlier component receipt still records `63be6dcf0a`; neither is relabeled
as a run of this source. Genuine old-runtime comparison, raw 2/22 admission,
show-mask mode transitions and the wider group/indexed/precision/compositing,
XCF, GUI/platform/full aggregate and whole-port gates remain open. The skipped
legacy Edge/runtime work is not resumed or counted as accepted.


## Saved-field contract handoff and retained Clone refresh (10.001, 2026-10-17)

Original task `10.001` is now DONE for its actual metadata/schema criterion:
every in-scope saved item has a type, default, owning type, reader/writer and
executable source assertion ID. The frozen source-only contract covers 1,378
records, 873 classified syntax candidates and the existing 16 persistence
routes. Its 32 direct negative controls and 720 independent controls (510
MyPaint, 173 configuration and 37 XCF/unified) rejected their mutations. Counts
describe the reviewed population, not completeness or runtime-parity oracles.

The authoritative source artifacts remain byte-identical to reviewed source
`522738a8fc31073878b4bcb7d6bff67fa77e526d`, tree
`d412b1cc289e8773524cf00519e81507f9dcfd96`. This includes
`migration/contracts/persistence-field-contract.md` and both source/review
receipts. Their handoff-pending statements describe the frozen source-stage
checkpoint; this appendix and the task/progress ledgers record acceptance of
that handoff. No schema, source assertions, native production code or test
registration was changed after review. Original `10.002`–`10.018`, `14.001`
and the broader Clone and port parents remain open.

Adding the tracked contract artifacts made the previous all-source-sealed Clone
receipt stale for the current tree. The same strict gate was rerun without
weakening its requirements. Its new archive is
`migration/tests/persistence-contract-acceptance-evidence.tar.xz`:

- Tested source commit: `522738a8fc31073878b4bcb7d6bff67fa77e526d`
- Tested source tree: `d412b1cc289e8773524cf00519e81507f9dcfd96`
- Archive SHA-256: `8581d325d9d62893bf3e1ff6fa7a17b81bbed32cf790eb2fbfd158e35214c40f`
- Receipt SHA-256: `774bdb01229c7af35bfae7fdbc9e92c3a1c1ce71b66e067858cb75db75780e37`
- Run: `persistence-contract-run-001`; all three build/test/sanitizer commands exited zero

The retained configured build completed a full default incremental Ninja build,
then the same seven exact registered Meson targets: `core` 4,
`projection-update-bounds` 3, `gimp-clone-layer` 62, `painter-layer-ui` 68,
`painter-layer-tree` 35, `painter-xcf-roundtrip` 49 and `update-geometry` 5.
All 226 positive normal TAP cases and all 62 focused ASan/UBSan Clone cases
passed. Each Clone run includes 216 distinct source-mode/type/mask scenes.
Exactly eight translation units are instrumented; 54 linked C++ units are
rebuilt with RTTI only and are not additional sanitizer coverage. LSan remains
disabled and other GIMP/dependency units remain uninstrumented. This retained
build is not a from-empty build.

Strict offline review verified all command statuses, unchanged source before
build/before tests/after tests, normal ELF and sanitizer artifact seals,
default-build freshness, compile database, full 164-target registry and all 361
locale catalogs. The 164-target registry was sealed, not executed as an
aggregate. The historical strict 163-target aggregate remains scoped to
`f271db84bb9cd3d99c404a43d5c727c19d96ca5f`; no latest 164-target aggregate pass
is claimed.

The new archive contains exactly the existing 17 public receipt members with
unchanged bytes. The unchanged deterministic pack function was reused; an
independent repack is byte-identical. The existing bounded decompressor and
flat-tar extractor were reused without modification. Original-directory,
extracted-directory and archive validation return identical strict results
while access to the original execution source, build and private output paths
is blocked and only Git subprocesses are allowed. Logged environment-removal
transformations remain intact. Every prior evidence archive is unchanged.

The current Clone acceptance route now uses this new sealed archive. The
44 receipt controls, 17 archive controls and 11 acceptance controls pass,
together with the source contract's 32 direct controls, strict current Clone
acceptance, task-ledger validation and `git diff --check`. These Python/Git
checks do not constitute another native execution. The closure commit changes
only the four documentary files permitted by the existing receipt contract and
the exact new archive; the native tested source remains the commit above.

This native refresh preserves the retained Clone acceptance after tracked
source artifacts were added. It does not validate runtime persistence of every
contract field or establish property order/duplicates, broad malformed-input
handling, rendering, GUI/platform equivalence, release or whole-port acceptance.
No old executable or legacy Edge runtime was built or run. All earlier Clone,
XCF and GUI receipts remain historical evidence of their exact original source.


## Original v4 header acceptance and retained Clone refresh (10.002, 2026-10-17)

Original task `10.002` is now DONE for its exact byte-layout criterion:
explain the genuine old v4 header without a precision word using byte fixtures.
The two preserved historical Clone XCF files retain their independently pinned
sizes and SHA-256 hashes. Their layout is a 14-byte `gimp xcf v004` signature
including NUL, followed by big-endian 32-bit width, height and image base type.
Image properties start at byte 26, without a precision word: the first property
is `PROP_COMPRESSION` (17), declared length 1, payload RLE (1). Layer/channel
offsets are four bytes. These are existing saved bytes; no historical capture
or old executable was rerun.

Pinned legacy source establishes that top-level modes 23–29 force version 4.
The recursive group helper checks 26–29 and custom layers, omitting 23–25;
a group containing only nested 23–25 may remain version 3 if nothing else
forces version 4. This is a source observation, not a new dialect classifier.
The `v004` signature alone does not identify the dialect.

All eight preserved-fixture Python tests passed with zero skips, including 20
embedded negative controls. The retained independent source review rejected
172 controls and its acceptance receipt passed 24 digest crosschecks. Those
controls test fixture assertions, not production-parser robustness. The
unchanged positive persistence gate was rerun and passed 1,378 records, 873
classified source candidates and 16 routes; the previous 32 expensive negative
controls were retained without a fresh-run claim.

The header test, `migration/contracts/xcf-compatibility.md` and
`migration/acceptance/xcf-v4-header-source.json` remain byte-identical to
reviewed source `8e45e61fc7aebf5a88a8456bb6841ec61a423b44`, tree
`9982e5a7b81c2e825b3ba62f3a67a65faa7c3fe1`. Their pending-handoff wording is historical
source-stage context; this appendix and the task/progress ledgers record the
completed bounded handoff. No decoder, fixture, validator, production code or
native test registration changed. Original `10.003`, `10.004`, `11.002` and all
other previously open tasks remain open. Header explanation does not establish
normal Open integration, runtime roundtrip, rendering or whole compatibility.
Old Edge execution remains user-approved SKIPPED, with compatibility unverified.

Adding the tracked source artifacts made the previous all-source-sealed Clone
receipt stale for this tree. The existing strict gate was rerun unchanged. Its
new archive is `migration/tests/xcf-v4-header-acceptance-evidence.tar.xz`:

- Tested source commit: `8e45e61fc7aebf5a88a8456bb6841ec61a423b44`
- Tested source tree: `9982e5a7b81c2e825b3ba62f3a67a65faa7c3fe1`
- Archive SHA-256: `306aa762588c5b297b2af3efe7832e2fbe136a1dbe7f741fe3775bf2c74ad2d9`
- Receipt SHA-256: `b87780e55cd01c2d518b4ae73d3f2ed91b34486a360d0cedde373dcb502160a7`
- Run: `v4-header-run-001`; all three build/test/sanitizer commands exited zero

The retained configured build completed a full default incremental Ninja build,
then the same seven exact registered Meson targets: `core` 4,
`projection-update-bounds` 3, `gimp-clone-layer` 62, `painter-layer-ui` 68,
`painter-layer-tree` 35, `painter-xcf-roundtrip` 49 and `update-geometry` 5.
All 226 positive normal TAP cases and 62 focused ASan/UBSan Clone cases passed.
Each Clone run includes 216 distinct source-mode/type/mask scenes. Exactly eight
translation units are instrumented; 54 linked C++ units have RTTI-only rebuilds
and are not additional sanitizer coverage. LSan remains disabled and other
GIMP/dependency units remain uninstrumented. This is a retained incremental
build, not a from-empty build.

Strict offline review verified all command statuses, unchanged source before
build/before tests/after tests, normal ELF and sanitizer artifact seals,
default-build freshness, compile database, the full 164-target registry and all
361 locale catalogs. Registry integrity does not mean those 164 targets were
executed as an aggregate. The historical strict 163-target aggregate remains
scoped to `f271db84bb9cd3d99c404a43d5c727c19d96ca5f`; there is no new 164-target
aggregate pass or release/platform/whole-port claim.

The archive contains exactly the existing 17 public receipt members, with
unchanged bytes. The unchanged deterministic pack function was reused and a
second pack is byte-identical. The existing bounded decompressor and flat-tar
extractor were reused without modification. Original-directory,
extracted-directory and archive validation return identical strict results
while access to the execution checkout, build and private output paths is
blocked and only Git subprocesses are allowed. Logged environment-removal
transformations remain intact. Every previous evidence archive is unchanged.

The current Clone acceptance route now uses this sealed archive. All 44 receipt
controls, 17 archive controls and 11 acceptance controls pass, alongside the
eight fixture tests, positive persistence gate, strict current Clone acceptance,
task-ledger validation and `git diff --check`. These Python/Git checks do not
constitute another native execution. Closure changes exactly the four permitted
documentary files and this exact evidence archive. The native tested source is
the commit above. This refresh preserves retained Clone acceptance after tracked
header artifacts were added; it is not a fresh execution of every header parser
test or all persistence fields. Broader Clone/XCF/GUI/compatibility gates remain
open, and all historical receipts retain their original scope.


## Original rotation-start acceptance and retained Clone refresh (25.001, 2026-10-17)

Original task `25.001` is now DONE for its exact `button・modifier・tool 状態が一致`
start criterion. Its title, original dependencies `02.015, 03.009` and criterion
are unchanged. Only its original checkbox and progress row are closed. This
accepts the finite source-defined start matrix below; it does not close those
dependency tasks, create another child WBS item, or close all of section 25.

### Navigation component: tested source A

The navigation run is bound only to source A `3634001805313a297e259aea0d3020b1f6805d04`,
tree `af466e90bbe5ac396c48360176d27fcfc3f2c932`. The unchanged
`migration/tests/rotation-start-component-receipt.json` and its compact archive
retain this identity; no navigation test is relabeled as running on source B
or on the later documentary closure commit. The navigation production/test
sources, retained fixtures, component receipt, component archive and
`migration/contracts/painter-navigation.md` are byte-identical to source B.

The three registered normal navigation targets passed 21 positive TAP cases:
11 events, four UI and six arithmetic. The same 11 event registrations passed
focused ASan/UBSan, with exactly five C units instrumented and LSan disabled.
The seven new event registrations contain 62 accepted middle starts, 16 rejected
navigation probes and two legitimate primary setups; these 80 subcases are
not 80 registered GTests. The original four event cases are also retained.

The finite matrix covers real Paintbrush and Move tool state and a recording
tool, button/modifier starts, occupied-navigation and primary-interaction
rejection snapshots, and release/reentry. All immediate shell/action/anchor/
grab-pointer cleanup assertions remain intact. The GDK no-grab observation
follows a deterministic server roundtrip and main-loop drain, preserving the
start-time grab assertion. There is no sleep, polling until success, forced
ungrab or discarded input event. The source rationale and exact registrations
remain in the unchanged navigation contract.

Retained historical positive sequences 2 (Shift+middle) and 222
(Shift+Ctrl+Alt+middle) are each exercised with both real tools. The preserved
fixture supplies the button/modifiers, handled state, exclusive rotation,
angle/anchor and grab checks. Modern tool-state expectations are explicitly
pinned-source-derived: those old rows contain neither tool identities nor
negative tool-state captures. Both historical GUI captures still record
`command_exit=-11` during teardown. No old executable or new old-runtime
capture was used.

Failed component runs 001, 002 and 003 remain failed in the unchanged component
summary and separately retained complete public archives. They respectively
record the missing declaration build failure, asynchronous GDK grab-observation
failure and incorrect 25-column fixture assertion. Accepted run 004 uses the
unchanged 24-column fixture and preserves field indices and all behavioral
assertions. None of those failures is overwritten or relabeled.

### Retained strict Clone gate: distinct tested source B

The receipt-integrated source B is `d975c1ba64895bcb61995c5a807d0b440509716b`,
tree `d3f8aa2072cf1c21c5a81fb3e58ecfaefa15b6a1`. Its only changes from source A are the
three component publication files. The unchanged strict Clone gate ran on this
exact frozen source B and passed all three build/test/sanitizer commands:

- Run: `rotation-start-run-001`
- Current strict archive: `migration/tests/rotation-start-acceptance-evidence.tar.xz`
- Archive SHA-256: `a2da52eac06c31c2af0633661e94e357ea9f1cbc037a09d8f46912fbb9a523c8`
- Receipt SHA-256: `2a997d252b2e3f4ef12c2daa589158259cfcb80aeea81eb4264834f523982e8e`

The retained full default incremental build and seven exact normal Meson
targets passed: `core` 4, `projection-update-bounds` 3, `gimp-clone-layer` 62,
`painter-layer-ui` 68, `painter-layer-tree` 35, `painter-xcf-roundtrip` 49 and
`update-geometry` 5, totaling 226 positive TAP cases. Focused Clone ASan/UBSan
passed 62 cases. Both Clone executions include the same 216 distinct
source-mode/type/mask scenes. Exactly eight translation units are instrumented;
54 RTTI-only C++ rebuilds are not additional sanitizer coverage. LSan remains
disabled and the remaining GIMP/dependency units remain uninstrumented.

Strict review verified unchanged source before build, before tests and after
tests; all 2,879 normal build ELF inventory entries; full default-build
freshness; compile database; all 164 registered target definitions; all
361 compiled locale catalogs; and unchanged private Clone sanitizer
artifacts. Inventory counts come from this exact receipt, including retained
navigation event sanitizer outputs where present. Sealing those outputs does
not claim the navigation sanitizer was rerun by the strict Clone gate. Registry
integrity is not a full aggregate execution, and the retained build is not a
clean-from-empty build. Historical aggregate receipts keep their original scope.

The strict archive contains exactly the existing 17 public receipt members with
unchanged bytes. The reviewed deterministic pack function and existing bounded
XZ/flat-tar helpers are reused without modification. A second pack is byte-for-
byte identical. Original-directory, extracted-directory and archive validation
produce identical strict results with execution source/build/private paths
blocked and Git as the only permitted subprocess. Public environment-removal
transformations remain intact. Every earlier evidence archive is unchanged.

All 44 existing receipt controls, 17 archive controls and 11 acceptance controls
pass, together with strict current Clone acceptance, task-ledger validation and
`git diff --check`. These closure checks are Python/Git-only; they do not create
another native execution or transfer source A/B native claims to this later
commit. The four documentary files and this one exact verified strict archive
are the only closure changes. No production, test, fixture, validator, component
receipt, component archive or navigation-contract change is made after freeze.

Physical mouse/tablet equivalence, focus/device interruption, other platforms,
original `25.002` onward, all of section 25, broader Clone/XCF/GUI acceptance,
full aggregate, packaging and release remain open. Old Edge/string-reader
runtime remains user-approved SKIPPED and compatibility unverified. This
closure establishes only the original finite rotation-start criterion while
preserving the current strict Clone acceptance route.


## Original mid-drag Ctrl acceptance and retained Clone refresh (25.004, 2026-10-17)

Original task `25.004` is now DONE only for its exact
`drag 継続中に旧仕様の snap へ切替` criterion. Its title and prerequisite
`25.003` are unchanged. Only the original `25.004` checkbox and progress row
are closed; no other task or dependency status changes and no child WBS item
is created. Ctrl-release evidence supports the adjacent timing obligation but
original `25.005` remains open, as do `25.006` onward and section 25 as a whole.

### Navigation component: tested source A

The navigation component is bound only to source A `edf861423cd438f275332c0e7f3b899dc3126d51`,
tree `cdb92379f0c983dbfe6c0473993e4c58ea43e9b9`. The unchanged
`migration/tests/rotation-ctrl-component-receipt.json` and compact
`rotation-ctrl-component-evidence.tar.xz` retain that identity and native receipt
SHA-256 `43e1de63ef396f2c7a9547793dfb5791426843f8470b775648b51c3ebba92ad7`.
No navigation test is relabeled as running on source B or the documentary
closure commit. The navigation production/test sources, retained fixtures,
procedure, published component and GUI receipts/archives, and
`migration/contracts/painter-navigation.md` remain byte-identical to source B.

The accepted full default incremental build and three registered normal targets
passed 22 positive TAP cases: 12 events, four UI and six arithmetic. The same
12 event registrations passed focused ASan/UBSan with exactly five C units
instrumented, LSan disabled and no suppression files or options. Other GIMP,
GTK and dependency units remain uninstrumented. All nine component seals pass;
the retained source, normal ELF, default freshness, compile database, registry,
focused artifacts/copies and 361 locale catalogs are unchanged across the run.

The single new `/navigation-events/rotation/ctrl-mid-drag` registration replays
immutable historical sequences 22–30 with real Paintbrush and Move tools.
These are two real-tool replays of one registered case. All eleven preceding
event cases and production code are unchanged. Literal old event types,
pre-event modifier masks, key/button details, coordinates and angle/anchor/
held-state expectations are checked before modern events, without deriving
expected angles from the production arithmetic helper.

The finite unmirrored sequence begins at +7.5 degrees with Shift+middle.
Ctrl press alone leaves the entire navigation snapshot unchanged; the next
stationary motion applies the 15-degree constraint. Shift release and Ctrl-only
motion retain 15. Ctrl release alone likewise preserves that snapshot; the next
stationary motion returns to +7.5. Every held step retains the initiating button,
start anchors, inherited action, scroll coordinates, scales and actual grab.
Ordinary middle release and accepted reentry exercise the existing cleanup
helpers. The key-only versus motion transition establishes the original
mid-drag Ctrl acceptance criterion without generalizing this one sequence to
all angles, mirrored states, devices or interruption paths.

Both retained old GUI captures still record teardown `command_exit=-11`.
Modern real-tool-state expectations are source-derived; the retained old rows
are not a fresh old-runtime execution. Previous failures and evidence archives
remain unchanged. Logs keep missing-icon, dummy-plugin discovery, unavailable
D-Bus and GLib environment-thread messages; empty warning-classifier arrays do
not imply a warning-free application or clean-from-empty build.

### Separate portrait GUI evidence: older exact source

The unchanged `migration/tests/rotation-portrait-gui-receipt.json` and compact
`rotation-portrait-gui-evidence.tar.xz` describe only older exact source
`22708f41d368c6c7c2e425d30ea2765b9d29158b`, tree
`0a48c7e042ae2f2b46e6cfe828926470e1ace9ab`. Its original receipt SHA-256 is
`b8d9cd3af5518d14a74d9ee3c499dfd1e46cb0dcdc40b2c243fd43da1a35e52c`.
The GUI was not rerun on source A, source B or this closure commit.

Ordinary XTest input to the directly launched installed branch executable on an
authenticated loopback-TCP 1080×1920 portrait display produced same-drag visible
angles 28.43 → 30 → 30 → 28.43 → 28.43 degrees through free rotation, Ctrl snap,
Shift release, Ctrl release and middle release. The actual default incremental
build/install passed; all 19 requests succeeded without geometry retry or idle
input release. Normal Save/Open dialogs preserved XCF SHA-256
`01a1846108e57776bed5b132928f6524de81c7d936dfe2be4b7cb60616c5c931`.
Reopening reset the view angle to zero and preserved all 379,010 compared
same-geometry visible artwork pixels. This rendered-region comparison is not
an internal GIMP-buffer proof or general XCF semantic acceptance. A short
post-reopen rotation again showed 28.43 degrees.

The application, driver, launcher and display exited 0 with held inputs cleared
and source/runtime/binary seals matched. Unsupported-font, session-bus and dconf
diagnostics remain recorded; no warning-free runtime claim is made. The compact
archive binds omitted original screenshots, input PNG and XCF by hash/size;
the separately retained full public archive contains their original bytes.

### Retained strict Clone gate: distinct tested source B

Receipt-integrated source B is `67f107eb0add92cbfee90ce344574e61fed97525`,
tree `09261d1e3da663e45160fdc64791b64eecf56f29`. It differs from source A in exactly the five
component/GUI publication files named above, including the navigation contract.
The unchanged strict Clone gate ran on this exact frozen source B and passed
all three default-build/normal-test/focused-sanitizer commands:

- Run: `ctrl-timing-run-001`
- Current strict archive: `migration/tests/rotation-ctrl-acceptance-evidence.tar.xz`
- Archive SHA-256: `ee8e999bd8ba5764d190d77462d9b567333d267c66e1020a005d7136ab0161ab`
- Receipt SHA-256: `3c66f4a4fe7648433ad1639a5f899717530a7bbc9db466cdc83b7e20b7919cf1`

The seven exact normal Meson targets passed: `core` 4,
`projection-update-bounds` 3, `gimp-clone-layer` 62, `painter-layer-ui` 68,
`painter-layer-tree` 35, `painter-xcf-roundtrip` 49 and `update-geometry` 5,
totaling 226 positive TAP cases. Focused Clone ASan/UBSan passed 62 cases.
Both Clone executions contain the same 216 unique source-mode/type/mask scenes.
Eight translation units are instrumented; 54 RTTI-only C++ rebuilds are not
additional sanitizer coverage. LSan remains disabled and other GIMP/dependency
units remain uninstrumented.

Strict review verified unchanged source before build, before tests and after;
all 2,879 normal build ELF inventory entries; full default-build freshness;
compile database; all 164 registered target definitions; all
361 locale catalogs; and private Clone sanitizer artifacts. Sealing retained
navigation sanitizer outputs does not mean strict Clone reran them. Registry
integrity is not full aggregate execution; the retained incremental build is
not a clean-from-empty build. Historical aggregate claims keep their exact scope.

The archive contains exactly the existing 17 public strict receipt members with
unchanged bytes. The unchanged deterministic pack function and bounded XZ/flat-tar
helpers are reused; a second pack is byte-identical. Original-directory,
extracted-directory and archive validation produce identical strict results
with execution source/build/private paths blocked and Git as the only permitted
subprocess. Public environment-removal transformations remain intact. Earlier
component, GUI and acceptance evidence archives remain unchanged.

The existing 44 receipt controls, 17 archive controls and 11 acceptance controls
pass, together with strict current Clone acceptance, task-ledger validation
and `git diff --check`. These closure checks are Python/Git-only; they create
no native execution or native claim for this later documentary commit. The
four documentary files and one verified strict archive are the only closure
changes. No production, test, fixture, validator, component/GUI receipt/archive
or navigation-contract change is made after the freeze.

Physical mouse/tablet equivalence, focus/device interruption, other platforms,
original `25.005` onward, whole section 25, broader Clone/XCF/GUI acceptance,
full aggregate, packaging and release remain open. Old Edge/string-reader
runtime remains user-approved SKIPPED and compatibility unverified. This
closure accepts only the original bounded mid-drag Ctrl criterion while
preserving the current strict Clone acceptance route.
