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


## Original zero/360 boundary acceptance and retained Clone refresh (25.007, 2026-10-17)

Original task `25.007` is now DONE only for its exact
`角度飛びと方向反転がない` criterion. Its title and prerequisite `25.006`
are unchanged. Only the original `25.007` checkbox and progress row are closed;
no other task or dependency status changes and no child WBS item is created.
Original `25.008` onward and section 25 as a whole remain open.

### Navigation component: tested source A

The navigation component is bound only to source A `aa175058ab6e876fdbc12333e4bf53a48dec6f18`,
tree `5ec543192cfd943fe5c6d60f1d7153040cdda0b0`. Its retained
`migration/tests/rotation-boundary-component-receipt.json` and compact
`rotation-boundary-component-evidence.tar.xz` bind native receipt SHA-256
`b38ff6991e414f46fa80654e4d086f5d7e68b45828bbfb6caab3abb27a0a8f1b`.
No navigation test is relabeled as running on source B or the documentary
closure commit. Production/test sources, retained old fixtures, procedures,
component/GUI receipts and archives, and `painter-navigation.md` remain
byte-identical to source B throughout closure.

The first accepted attempt passed the full default incremental build and
28 positive normal TAP cases across three registered navigation targets:
18 event, four UI and six arithmetic. The same 18 event registrations passed
focused ASan/UBSan with exactly five C units instrumented, LSan disabled and
no suppression files or options. The shell-reset path in
`app/display/gimpdisplayshell.c` is normally tested but that translation unit
is uninstrumented. Other GIMP, GTK and dependency units remain uninstrumented.
All four top-level component commands exited 0. All nine component seals pass,
including the 12 bound source/fixture inputs, 2,873 normal ELF entries, full
default-build freshness, compile database, registry, focused artifacts/copies
and 361 locale catalogs. Registry integrity does not mean aggregate execution.

Six new event registrations add four ordered, unmirrored crossings plus angle
persistence and ordinary/custom-policy isolation. The four crossings exercise
free and 15-degree-snapped rotation in both increasing and decreasing order.
Each event run retains 24 crossing observations and 20 signed adjacent steps.
Independent constants derive from the old source geometry with a nonzero
90-degree drag anchor, not from the production helper or a new old-runtime
capture. Free observations include zero and both sides of the ordinary
0.001-degree epsilon; snapped observations enter zero from 345 degrees and
leave toward 15 degrees, in both directions.

Real display-transform bases, lengths and determinant are checked against the
independent expected geometry. Signed steps use the observed adjacent bases;
they establish direction without treating the numerical 359-to-0 wrap as an
angular jump. Initiating button, modifier action, inherited state, anchor,
tool state and grab remain held. The first motion positions each crossing;
only subsequent adjacent steps are continuity assertions. Ordinary release
retains the final exact angle.

The two tiny inherited angles also survive release, repeated transform rebuild,
scroll, scale and horizontal flip/unflip. That flip check concerns existing
angle persistence, not mirrored-input direction acceptance for `25.008`.
Ordinary absolute/relative rotation and an explicit custom binding replace the
inherited exact-angle policy and preserve the existing represented-value
endpoint behavior. Empty-display reset clears the policy. The twelve previous
event registrations and retained historical fixtures stay unchanged.

This is finite synthetic-event evidence, not physical mouse/tablet or
interruption/platform acceptance. Ordinary icon, plugin, D-Bus and GLib
diagnostics remain in the logs; no warning-free or clean-from-empty claim is
made. No old executable was accessed or run; both historical old GUI teardown
`command_exit=-11` outcomes remain recorded. There is no new GUI run. Existing
portrait GUI evidence stays bound only to older exact source
`22708f41d368c6c7c2e425d30ea2765b9d29158b`; it does not establish the new boundary
criterion or create a GUI claim for source A, B or the closure revision.

### Retained strict Clone gate: distinct tested source B

Receipt-integrated source B is `226beebe62a27fdcdd69f5106e55ecc10ba2ec69`,
tree `8d4c430eea83d9b86fc26b12799106ff9d73edb8`. It differs from source A in exactly three publication
files: `painter-navigation.md`, the boundary component receipt and its compact
archive. The unchanged strict Clone gate ran on this exact frozen source B and
passed all three default-build/normal-test/focused-sanitizer commands:

- Run: `boundary-run-001`
- Current strict archive: `migration/tests/rotation-boundary-acceptance-evidence.tar.xz`
- Archive SHA-256: `07c463d10bfa645e954ff1666f347229659335c5eac2d1bc16291d9fbf1a6ac2`
- Receipt SHA-256: `457b446f6dc96d3f9fec0881ecd9bb58eaedd3a14e3027db02953066add33c7a`

The seven exact normal Meson targets passed: `core` 4,
`projection-update-bounds` 3, `gimp-clone-layer` 62, `painter-layer-ui` 68,
`painter-layer-tree` 35, `painter-xcf-roundtrip` 49 and `update-geometry` 5,
totaling 226 positive TAP cases. Focused Clone ASan/UBSan passed 62 cases.
Both Clone executions contain the same 216 unique source-mode/type/mask scenes.
Eight translation units are instrumented; 54 RTTI-only C++ rebuilds are not
additional sanitizer coverage. LSan remains disabled and other GIMP/dependency
units remain uninstrumented.

Strict review verified unchanged source before build, before tests and after;
all 2,879 normal ELF inventory entries; full default-build freshness;
compile database; all 164 registered target definitions; all
361 locale catalogs; and private Clone sanitizer artifacts. Sealing retained
navigation sanitizer outputs does not mean strict Clone reran them. Registry
integrity is not full aggregate execution. Historical aggregate claims retain
their original exact source scope.

The archive contains exactly the existing 17 public strict receipt members,
with every raw member hash and byte unchanged. The unchanged deterministic pack
function and bounded XZ/flat-tar helpers are reused; a second pack is byte-identical.
Original-directory, extracted-directory and archive validation produce identical
strict results with execution source/build/private paths blocked and Git as the
only permitted subprocess. Public environment-removal transformations remain
intact. All earlier component, GUI and acceptance archives remain unchanged.

The existing 44 receipt controls, 17 archive controls and 11 acceptance controls
pass, together with strict current Clone acceptance, task-ledger validation and
`git diff --check`. These closure checks are Python/Git-only and create no native
execution or native claim for this later documentary commit. Four documentary
files and one verified strict archive are the only closure changes. No production,
test, fixture, validator, component/GUI receipt/archive or navigation-contract
change is made after the freeze.

Physical devices, focus/device interruption, other platforms, original `25.008`
onward, whole section 25, broader Clone/XCF/GUI acceptance, full aggregate,
packaging and release remain open. Old Edge/string-reader runtime remains
user-approved SKIPPED and compatibility unverified. This closure accepts only
the original bounded zero/360 boundary criterion while preserving the current
strict Clone acceptance route.


## Mirrored-arrow input acceptance and retained Clone refresh (25.008/mirrored-arrow-input, 2026-10-17)

Task `25.008/mirrored-arrow-input` is now DONE only for its exact
`mirrored時のLeft/Right変換と非反転時の不変を、押下・解除・toolへの転送・回転併用で試験する`
criterion. Its title and prerequisites `25.007, 01.017` remain byte-identical.
Only this sibling checkbox and progress row change. Original `25.008` horizontal-
reflection calculation remains separately accepted; no other task status,
dependency or criterion changes and no child WBS item is created.

### Navigation component: tested source A

The component run attests only to source A `e5f73ed431f1852fdfb337fc402c5a72d16cd6d7`,
tree `33772929b55d176841d3dbd1a8a5c286ecc445a7`. The retained
`migration/tests/mirrored-arrow-component-receipt.json` and compact
`mirrored-arrow-component-evidence.tar.xz` bind native receipt SHA-256
`c37c42e0a2920624ddfa08751a0afa06b8ceb6b1c5377c1562a49164836e2cea`.
The first accepted attempt passed the default incremental build and **32 positive
normal TAP cases**: 22 event, four UI and six arithmetic. All 22 event registrations
passed focused ASan/UBSan with exactly five C units instrumented, LSan disabled
and no suppression files or options. Other GIMP, GTK and dependency units remain
uninstrumented. All four top-level component commands exited 0. All nine seals
pass, including 12 source/fixture inputs, 2,873 normal ELF entries, full default-
build freshness, compile database, registry, focused artifacts/copies and all
361 locale catalogs. Registry integrity does not mean aggregate execution.

The retained old handler supplies independent literal expectations: wants-all
callbacks run before translation; only ordinary Left/Right **presses** swap under
horizontal reflection. Normal unreflected presses are unchanged. All-consume
receives the original key once. All-fallthrough receives the original key first,
then the ordinary mapped key; the test's second callback consumes the press.
Releases do not use the ordinary arrow callback route: only wants-all receives
one unchanged release. This preserves the old asymmetric dispatch, while the
modern local event copy also keeps the original allocated input immutable.
These expectations do not call the production navigation helper.

The pure six-group target explicitly checks all **20 combinations** of Left,
Right, Up, Down and `a`, two horizontal-reflection states and press/release.
Five previously missing unreflected-release assertions were added. Four new
public-handler event registrations cover `idle-zero`, `idle-rotated`,
`rotation-free` and `rotation-snapped`. Each combines both horizontal-reflection
states, normal/all-consume/all-fallthrough recorder modes, Left/Right and
press/release. This is **96 synthetic key sends, 64 press callbacks and 32 release
callbacks per event execution**, not 96 additional registered tests. Vertical
reflection is false. The prior 18 event registrations remain, including the
24 boundary observations and 20 signed steps in each event run.

A registered manager-owned focused recording tool checks callback count/order,
phase, tool/display identity, keys and all public event fields. Original event
fields, string content and device/source identities stay unchanged. Idle angles
are 0 and 37 degrees. The active free/snapped contexts start at modern 37 degrees;
independent pinned-old-source anchors are 127 unreflected and 593 reflected.
After right-to-down motion, modern angles are 127 free and 120 snapped. Every
key preserves action, initiating button, inherited routing, start/last points,
rotation/zoom anchors, angle/provenance, scale/reflection and shell/native grabs.
Continued motion to the left ray yields 217 free or 210 snapped from the same
anchor, followed by ordinary middle release and full idle/grab cleanup. There
are four real navigation drag lifecycles across the two active contexts.

This finite matrix is source-derived. There is no retained Left/Right old-runtime
capture for it, and no old executable, capture or oracle was run or regenerated.
Both historical old GUI teardown `command_exit=-11` outcomes remain. No new GUI
run is included: the older portrait GUI receipt stays bound only to source
`22708f41d368c6c7c2e425d30ea2765b9d29158b`, without a current GUI claim for A,
B or this documentary closure. Ordinary icon, plugin, D-Bus and GLib diagnostics
remain; no warning-free application or clean-from-empty build claim is made.

### Retained strict Clone gate: distinct tested source B

Receipt-integrated source B is `25364937bf7c100e2123a4339f30de4d4a04e6fe`,
tree `2ae2d6cdb4943690a05201deb29364e429b127b3`. Its only changes from A are `painter-navigation.md`, the
mirrored-arrow component receipt and its compact archive. The unchanged strict
Clone gate ran on this exact frozen B and all three default-build/normal-test/
focused-sanitizer commands passed:

- Run: `mirrored-arrow-run-001`
- Current strict archive: `migration/tests/mirrored-arrow-acceptance-evidence.tar.xz`
- Archive SHA-256: `6338ec2a74fd851fb3a1ab577a4a066f5173e43949cf8984ad943e987db3ddad`
- Receipt SHA-256: `6a7794ae0e2d725553b4ab937ed9ccdf26a1ee3813eba5f478aefd04cca0b88d`

Seven exact normal Meson targets passed: `core` 4, `projection-update-bounds` 3,
`gimp-clone-layer` 62, `painter-layer-ui` 68, `painter-layer-tree` 35,
`painter-xcf-roundtrip` 49 and `update-geometry` 5, totaling **226 positive TAP
cases**. Focused Clone ASan/UBSan passed **62 cases**. Both Clone executions
contain the same 216 unique source-mode/type/mask scenes. Eight translation
units are instrumented; 54 RTTI-only C++ rebuilds are not additional sanitizer
coverage. LSan remains disabled. Strict Clone did not rerun the navigation suite.

Strict review verified identical source before build/before tests/after,
2,879 unchanged normal ELF entries, full default-build freshness,
compile database, all 164 registered target definitions,
361 locale catalogs and private Clone sanitizer artifact seals.
Historical aggregate evidence retains its original exact source scope.

The new archive retains exactly the existing **17 public strict members**, all
raw bytes and hashes unchanged. The unchanged deterministic pack function and
bounded XZ/flat-tar helpers are reused; repacking is byte-identical. Original-
directory, extracted-directory and archive validation give identical strict
results with execution source/build/private paths blocked and Git as the only
permitted subprocess. Public environment-removal transformations remain intact.
All previous component, GUI and acceptance archives are unchanged.

Existing **44 receipt, 17 archive and 11 acceptance controls** pass, as do strict
current Clone acceptance, the task ledger and whitespace checks. The closure
runs only Python/Git documentary checks. Its exact five files are this appended
contract, the sibling task/progress edits, the Clone current-receipt route and
one verified strict archive. Production, test, fixture, validator, procedure,
component/GUI receipt/archive and navigation-contract bytes remain identical to B.
Navigation claims remain on A, strict Clone claims on B and historical GUI claims
on the older source. No native claim transfers to this documentary commit.

Physical keyboard/tablet/mouse behavior, every tool's arrow semantics,
keyboard-controller fallback, focus/device interruptions, vertical-reflection
legacy parity, transformed pan/zoom, canvas/overlay equivalence, other platforms,
whole section 25, full aggregate, final Clone freeze, packaging, release and
whole-port acceptance remain outside this closure. Old Edge/string-reader
runtime remains user-approved **SKIPPED/unverified**.


## Bounded 25.009/common-angle-draw native checkpoint (2026-10-17)

This independent child accepts only the finite common-angle draw and upright
GTK-anchor implementation/test result. Its completed inputs are original
25.008 and the 01.017 hunk assignment. The existing original **25.009 and
25.009/drag-zoom-scroll stay TODO with byte-identical criteria and dependencies**.
No existing task or progress row changes; only this bounded child is added.
It does not accept all 196 jointly assigned historical hunks or the parent
canvas/overlay obligation. At this native checkpoint, actual-window portrait
GUI was still pending; the later independent 34.005/canvas-overlay-portrait-smoke
appendix below records only its distinct exact-source GUI result.

### Component evidence: tested source A and retained failure

The native component receipt attests only to repaired source A
`ad0dab2d49c2bafcd85da05d76f077071cd1c6e1`, tree
`a237803fe42b33f6c608fb993e45d838c4ecd88b`. The unchanged
`migration/tests/canvas-overlay-component-receipt.json` and compact
`canvas-overlay-component-evidence.tar.xz` bind native receipt SHA-256
`6be80fb0b6e1f87a5b239d1093455b3459f964ab6dbed24ffcda7fa8973a5404`.
The default incremental build and all four component commands exited 0;
**37 normal TAP cases (27 event + 4 UI + 6 arithmetic) and 27 focused event
ASan/UBSan cases passed**. Exactly six C units are instrumented, including the
changed display shell; remaining renderer, canvas items, selection, GTK overlay
container/child, core, GTK and dependencies are uninstrumented. LSan is disabled,
with no suppression files/options. All nine component integrity seals pass.

Five added event registrations perform **38 actual GTK snapshots per event
execution**: 16 raster/item pairs, three selection and three upright-widget
draws. The public event route drives free 127, Ctrl-snap 120, unsnap 127,
button release and unchanged redraw under both horizontal-reflection states.
A pre-established mirrored 1.5x view, tiny negative/zero/positive CTMs, a
separate selection boundary and an image-anchored upright GTK child are also
checked. Literal trigonometric constants and independently specified geometry
supply the expectations; production transforms do not supply the oracle.
Frozen matrix and pixel tolerances remain exactly as documented in
`painter-navigation.md`. Raster cache orientation, actual image-space line and
rectangle pixels, separate selection pixels and the stationary screen-space
control are checked. Full-surface draws do not establish damage or clipping.

The retained unsuccessful source `bfbbbeade5ba733320b4c1b98a7f3213291928d2`
passed 36/37 normal and 26/27 focused event cases, then failed the explicit
widget-anchor assertion. At 37 degrees and unchanged offsets `(-296,-97)`,
expected `(490.388929396,229.950762935)` stayed at stale `(480,189)`.
This is an observed assertion failure, not an ASan/UBSan memory error.
The bounded 13-line production repair refreshes transformed child anchors
in `gimp_display_shell_rotated()` after the display matrix update and before
the signal. It preserves upright child angle and basis. The exact same fixture
SHA-256 `b5ff043ae3518784ebb485a1634cb38806fcdf1ab8b2f8ccf3d64b68255e6568`
passes on A without changed assertions, expectations, tolerances or registrations.
Immediate and both post-draw anchors now match, visible integer placement is
`(490,229)`, and the unchanged second rotated redraw passes. Both full failed
and passed public archives remain separately retained and hash-bound by the
component evidence; neither failure nor its source identity is overwritten.

The prior 22 event registrations remain successful, including 96 mirrored-key
sends, 64 press/32 release callbacks and 24 crossing observations/20 signed
steps per event execution. Logs retain the actual draw, marker, segment and
anchor checks. The optional PNG hook was **disabled**: 38 GTK draws are not
38 additional registered tests, saved screenshots or a portrait GUI run.
The compact component archive contains 22 unchanged public originals plus a
packaging manifest; eight omitted binary copies stay hash/size-bound to the
separately retained full archive. It is not a standalone rebuild package.

### Distinct strict-tested source B and documentary descendant

Receipt-integrated source B is `4c4634ead724daa30f2b0276fc6310e575d072f2`,
tree `31d6dc34db477061fa68c3cf22b451fa0cac1fd4`. Its only changes from A are
the navigation contract, component receipt and compact component archive.
The unchanged strict Clone gate ran on this frozen B; all three default-build,
normal-test and focused-sanitizer commands passed:

- Run: `canvas-overlay-run-001`
- Current strict archive: `migration/tests/canvas-overlay-acceptance-evidence.tar.xz`
- Archive SHA-256: `c740d94bd2598f6f181d8da4600770ff5749d54bec2d6189c920cd36fd7532ee`
- Receipt SHA-256: `af617c9a318775b4ce0249e5a4a116c04a2b7191c04f7d6e65347657f24f79be`

Seven exact normal Meson targets passed: `core` 4, `projection-update-bounds` 3,
`gimp-clone-layer` 62, `painter-layer-ui` 68, `painter-layer-tree` 35,
`painter-xcf-roundtrip` 49 and `update-geometry` 5, totaling **226 positive TAP
cases**. Focused Clone ASan/UBSan passed **62 cases**. Both Clone executions
retain the same 216 unique source-mode/type/mask scenes. Eight units are
instrumented; 54 RTTI-only C++ rebuilds are not sanitizer coverage. LSan stays
disabled. Strict Clone does not rerun navigation or the actual-window GUI.

Strict review verifies equal source before build/before tests/after, unchanged
2,880 normal ELF entries, full default-build freshness, compile database,
all 164 registered target definitions, 361 locale catalogs and private Clone
sanitizer artifact seals. Registry integrity is not aggregate execution.
The archive retains exactly the existing **17 public strict members**, with
unchanged raw bytes/hashes. The unchanged deterministic pack function and
bounded XZ/flat-tar helpers produce byte-identical repacking. Original-directory,
extracted-directory and archive validation give identical strict results with
execution source/build/private paths blocked and Git as the only subprocess.
Public environment-removal transformations remain intact; private profiles,
raw environments and authentication material are excluded.

The existing **44 receipt, 17 archive and 11 acceptance controls** pass,
as do strict current Clone acceptance, task-ledger and whitespace checks.
Only this appended contract, new bounded child task/progress rows, the Clone
current-receipt route and its strict archive change. No production, test,
fixture, validator, instrumentation, procedure, component/GUI archive or
navigation-contract bytes change from B. Prior archives remain unchanged.
The closure executes only Python/Git documentary checks: component claims
remain on A, strict Clone on B, and no native claim transfers to this final
documentary descendant.

The historical portrait GUI receipt remains bound only to older source
`22708f41d368c6c7c2e425d30ea2765b9d29158b`; the later actual-window GUI
appendix below separately binds source `0f16f81557db0ab4cd1cdc1099eb8da323501c7d`.
Damage, clipping, extents, hit-testing, every overlay/selection topology,
vertical-reflection legacy parity, physical input, transformed drag-zoom/pan/
scroll, other platforms, all 196 hunks, whole section 25, full aggregate,
final Clone freeze, packaging, release and whole-port acceptance remain open.
Ordinary icon, plug-in, D-Bus and GLib diagnostics and the focused
`navigation_draw_observer_get_type` missing-declarations warning remain;
there is no warning-free application or clean-from-empty build claim.
Both historical old GUI teardown `command_exit=-11` outcomes are retained.
Old Edge/string-reader runtime remains user-approved **SKIPPED/unverified**;
no old runtime, capture or oracle was accessed, executed or regenerated.


## Installed canvas-overlay portrait GUI smoke (2026-10-17)

Independent child **34.005/canvas-overlay-portrait-smoke** uses completed
03.009/tcp-portrait-environment, 25.009/common-angle-draw and
29.001/native-canvas-tiles inputs. Original **34.005, 25.009 and
25.009/drag-zoom-scroll remain TODO with unchanged criteria and dependencies**.
Only this GUI child is newly accepted. Native renderer assertions, strict Clone
acceptance and this later documentary publication keep separate source scopes.

The actual installed GUI ran on public source
`0f16f81557db0ab4cd1cdc1099eb8da323501c7d`, tree
`f4b7c2dcebcd222a2a6c75413dd8c1919ef72756`. Full default incremental build
completed 96 steps, followed by install `--no-rebuild`; both exited 0. This is
not a clean-from-empty build. The fresh 1080x1920 depth-24 display accepted an
authenticated client over loopback TCP and rejected empty authority. Ordinary
XTest mouse/keyboard events operated the real application and its View >
Painter Canvas Controls; no batch command, debugger or internal state setter
supplied the GUI result. The archive retains 23 original full-resolution PNGs.

Two ordinary ruler guides intersect the mountain apex; a committed rectangular
selection surrounds the first three color chips. Both follow the raster during
one held middle-button drag: Shift free **28.43 degrees**, add Ctrl with motion
**30**, release Shift with motion **30**, release Ctrl with motion **28.43**,
then middle-button release and settled redraw **28.43**. Top, side and bottom
Painter panels stay upright. Released/settled frames 050/051 are byte-identical.
The longest recorded input gap while held was 8.436 seconds under the unchanged
120-second idle limit, with no timeout/error release or final cleanup input.
This GUI does not instantiate an arbitrary image-anchored GTK child; that
coverage remains only in the separate source-A native **38 draw assertions**.

Normal Save Image, close-view and Open Image dialogs produced and reopened
`canvas-overlay-portrait-smoke.xcf` (127,766 bytes, SHA-256
`aa8eb8c7a6f07acddd03947b0f30a137538ee199217f6d86e4b83bf07eaeacbb`).
Its final hash equals the immediate-after-save hash. Guides and chip selection
are visible after reopen; the expected new view angle is **0 degrees**, at 70%
zoom. Between original frames 005 and 071, the exclusive screen rectangle
`(289,606)-(791,1361)` has **0 different pixels out of 379,010**; both RGB byte
hashes are `4925f91b27039998a4b1fcf294df7349d2f11ce0a488f9cb3463df708c0448f7`.
This is a rendered artwork-rectangle comparison, not full-buffer equality.

The unsuccessful **056 save-focus attempt is retained**: after dialog resize
without explicit focus, filename keystrokes reached the main window, changing
the view to 25% and temporarily selecting all/changing tool presentation. No
save completed. In 057, explicit dialog focus/cancel and ordinary Undo restored
the chip selection; Paintbrush and 70% view were restored. In 059, explicit
focus entered the filename correctly and 060 saved. The subsequent comparison
above found no artwork-rectangle pixel change. Foreground marker and tool-group
presentation differed afterward; no complete UI-preference restoration or
flawless automation is claimed. Raw failed/recovery actions and images remain.

All **13,838 source entries** match before build, after build and after GUI;
all **2,842 installed-runtime entries** and both executable seals are unchanged.
Source and runtime original manifest SHA-256 values are, respectively,
`451600849dd5dc0814b3a32a33ee3dc024a5a034684002ae542ceae37219cd31` and
`f7980a0452b6b9a9d6963c60ca00039aa49708c3ec845e91771e273d08070f24`.
Runtime inventory covers installed bin/lib/libexec/share, not every external
dependency byte. Build/installed ELF Build IDs match
`ccb98685558dbf4093828c6830a11f5a45787091`; their 6,099,050-byte `.text` sections
both hash to `ce1df04ee90cc8b48b6fc7160e9eda10d16e92f90b31766c34a2b600aca01906`.
Install removes RUNPATH, so the separately sealed whole-file hashes differ.
Application, input driver, display, launcher and final seal check all exit 0;
no held input or cleanup error remains. Logs retain **112 unsupported fonts,
64 dconf warnings and unavailable D-Bus**. No warning-free application is claimed.

The external archive `canvas-overlay-portrait-gui-run-002-evidence.tar.xz`
contains 148 files and is 4,437,036 bytes:

- Archive SHA-256: `988f6cb2aa0299fe0b8af4e388941753560415f35fa7b31ce470567ede885c5a`
- Package-manifest SHA-256: `986232936710a79e7548ffef22844cb8c52db1ed0950adbec35f0e9103072cad`
- Public `canvas-overlay-portrait-gui-receipt.json` SHA-256: `33548ba3872a13770349c9732e49d6a95f3098250e0722c1553c0883d1d39777`
- Original local receipt SHA-256: `16145d49adc08670918105c1d151294154bbe9cce574fc7755d8a2e5c1811358`

Readback verifies every manifest member. Raw PNG/XCF bytes are unchanged;
text copies replace declared project roots and the D-Bus machine ID. Private
profiles, authentication material, raw environments and compiled executables
are excluded. Embedded checksum records identify original local text; the
package manifest maps it to normalized public bytes. Sanitized helper paths
are documentary placeholders: this is evidence, not a promised one-command
portable executable. Original evidence remains separately retained. Independent
read-only reviews verify all 147 raw/public member hashes, 74 text transformations,
25 unchanged PNG/XCF files, source/binary seals, held-input sequence, XCF hash,
conservative pixel comparison and disclosed recovery limitations.

No new full native aggregate or focused sanitizer run was performed on this
GUI source. The **37 normal/27 focused** component result remains bound to
`ad0dab2d49c2bafcd85da05d76f077071cd1c6e1`; **226 normal/62 focused** strict Clone
result remains bound to `4c4634ead724daa30f2b0276fc6310e575d072f2`.
Only tasks.md, progress.tsv and this contract change after the GUI, within the
unchanged strict receipt's documentary exceptions. Validators, current Clone
route, all prior archives/receipts, source and tests remain byte-identical.
Physical input, arbitrary GUI image-anchored child, exhaustive overlay/selection,
damage/clipping/extents/hit-testing, transformed pan/zoom/scroll, other platforms,
whole-section 25, full aggregate, final Clone freeze, package, release and
whole-port gates remain open. Both old GUI teardown -11 outcomes are historical.
Old Edge/string-reader runtime stays **SKIPPED/unverified**; no legacy runtime,
capture or oracle was accessed, run or regenerated.


## Bounded 25.009/transformed-drag-events acceptance (2026-10-17)

This independent child accepts only the frozen, finite modern event cases for
transformed radial zoom and interior middle-pan. Its completed inputs are
25.008 and 01.017. Original **25.009 and 25.009/drag-zoom-scroll remain TODO
with byte-identical criteria and dependencies**; every existing task and
progress row was preserved at this native checkpoint. Only this bounded child
was added then; portrait GUI was pending. The later independent
34.005/transformed-drag-portrait-smoke appendix below records its distinct
exact-source GUI result. This appendix records the strict result; the earlier
navigation-contract receipt-integration checkpoint
and frozen SOURCE-ONLY input manifest retain their historical meaning.

### Component-tested source A

The native component belongs only to source A
`0c8620d9064f8c912d9424266771e045c276f582`, tree
`0dbd6b2306bccb83e3d38fb97db158411e3a5ac0`. The unchanged
`migration/tests/transformed-drag-component-receipt.json` binds receipt SHA-256
`925c62539afbeaa6dca5edeca70cbb75a3f9ac766b83d068f0e9022b129d44a8`.
All four component commands exited 0: default incremental build, focused
builder, registered normal targets and focused event executable. **41 normal
TAP cases (31 event, 4 UI, 6 arithmetic) and 31 focused event ASan/UBSan cases
passed** in exact source-defined order. No production repair was needed.
The unchanged preceding 27 event registrations and their function bodies,
including 38 actual GTK draw assertions per execution, remain required and
passed. The PNG hook was disabled; these are assertions, not saved PNGs or a
new portrait GUI run.

The four new registrations are radial-transformed, radial-clamp,
pan-transformed and device-image-origin under `/navigation-events/drag/`.
Each event execution contains **16 main drags, 16 stationary reentries and
six genuine fractional primary press/release probe pairs**. These are finite
subcases, not additional GTest registrations. Radial zoom covers angles
0, 37 and 90 degrees with horizontal reflection off/on, the fixed anchor 335
and factors 1.25, 1.25, 2.45, 67/60, 1.25. Image center stays within one
observed display pixel per axis of integer viewport center. Repeated lower
requests apply 1/256 and upper requests 256; returning to start restores the
original factor without anchor replacement. **Clamp focus and offsets are
not accepted.** Interior pan uses scale 1.5 and frozen exact offset deltas,
stationary repetition and return to origin. Six fractional primary callbacks
in the two 37-degree reflection contexts match an independently calculated
image coordinate; only middle-pan endpoints narrow to integers.

The affine oracle uses literal trigonometric constants and separately
specified geometry, never production transform matrices/helpers for expected
values. All frozen tolerances stay unchanged. Logs establish one actual
**849 x 450** viewport, 30 radial motion records, six clamp records and six
primary probe records. Passed source assertions account for **32 pan/inverse
comparisons and 96 forward landmark comparisons**; they are not extra log
records. The 34 preflight states produce 68 axis records, with minimum
observed interior margin **209 pixels**, above the frozen 16-pixel threshold.
Ordinary release clears shell state immediately and native grabs after the
existing synchronized observation boundary. No test-side ungrab or state
repair is added. Retained mirrored-key, rotation-boundary, draw and anchor
checks continue to pass. Full legacy transformed-pan or radial display parity
is not established by these modern source-derived cases.

Exactly **nine C units** are instrumented: scale, scroll, transform, rotate,
navigation arithmetic, modifier manager, display shell, tool-events and the
event fixture. Zoom-model library, device-coordinate adapter, tool dispatch,
renderer/GTK and remaining GIMP/dependencies are uninstrumented. **LSan is off**,
with no suppressions. All nine component integrity seals pass, including four
equal exact-A source snapshots, three equal 2,873-entry normal ELF inventories,
focused-builder preservation, freshness, compile database, registered tests,
focused artifacts/copies and all 361 locale catalogs. Forty source/fixture
inputs and five committed procedures remain bound to A. The navigation
contract was subsequently integrated in B, and task narration is added in C;
neither documentary difference transfers the native run to a later source.

The separate full component archive
`component-run-001-passed-public.tar.xz` has **33 original public files** and
22,341,764 bytes, SHA-256
`eb119e254d43ec9036b296662fd9cbecfc48e7b34f015497e928361231b80722`.
The committed compact archive retains **22 byte-identical originals plus one
packaging manifest**, preserving the original receipt and all nested hashes.
Eleven omitted sanitizer binary/object/archive copies remain size/hash-bound
to the separately retained full archive. It is not a standalone rebuild
package. Parent and independent native audits verified the public privacy
transformations, actual registered environment reuse and nine instrumented
objects. Both default/focused build logs retain the missing-declarations
warning for `navigation_draw_observer_get_type`; ordinary runtime diagnostics
also remain. This is an incremental build, not a warning-free or
clean-from-empty application claim.

### Distinct strict-tested source B and documentary source C

Receipt-integrated public source B is
`1f911216f78d0702361bef782954b0ca14b056cf`, tree
`31497adb13fc2f292ef06ff020c64d74735bab85`. B differs from A only in the
navigation-contract appendix, compact component archive and component summary.
The unchanged strict Clone gate ran on this frozen B. Default build, normal
tests and focused sanitizer commands all exited 0:

- Run: `transformed-drag-run-001`
- Current strict archive: `migration/tests/transformed-drag-acceptance-evidence.tar.xz`
- Archive SHA-256: `2d109a884fbd03d51ead285fe13b6c9ad1a6d632a196a21e5878bedff0702180`
- Receipt SHA-256: `721126e400f176cbe315c914355fca3882981d6be4a6b5cf490fbbd4a207dd97`

Seven normal targets passed: core 4, projection-update-bounds 3,
gimp-clone-layer 62, painter-layer-ui 68, painter-layer-tree 35,
painter-xcf-roundtrip 49 and update-geometry 5, totaling **226 positive TAP
cases**. Focused Clone ASan/UBSan passed **62 cases**; both executions retain
216 unique source-mode/type/mask scenes. Eight units are instrumented, and
54 RTTI-only C++ rebuilds do not count as sanitizer coverage. LSan remains off.
Strict Clone does not rerun navigation or actual-window GUI.

Equal source snapshots before build/before tests/after, 2,883 unchanged
normal ELF entries, full default-build freshness, compile database, all 164
registered target definitions, 361 locale catalogs and sanitizer artifact
seals were verified. Registry sealing is not full aggregate execution. The
strict archive has exactly **17 unchanged public members**. The existing
unchanged deterministic packing function and bounded XZ/flat-tar extraction
helpers reproduce identical bytes. Original-directory, extracted-directory
and archive validation agree with native execution source/build/private paths
blocked and Git as the only subprocess. Public privacy transformations remain
intact; private profiles, raw environments and authentication material are
excluded.

The unchanged **44 receipt, 17 archive and 11 acceptance controls**, strict
current Clone acceptance, task-ledger and whitespace checks pass. This final
documentary C changes exactly five paths: the current-receipt route, appended
Clone contract, one new task row, one new progress row and the new strict
archive. Existing documentary exceptions and single exact storage exception
are unchanged. All production/test/fixture/validator/instrumentation/procedure
bytes, navigation contract, component evidence and older archives remain
byte-identical to B. The closure uses only Python/Git documentary checks.
Component claims remain on A and strict Clone claims remain on B; **C has no
native execution claim**.

At this native checkpoint, the latest separate portrait GUI was historical
exact source `0f16f81557db0ab4cd1cdc1099eb8da323501c7d`, not an A/B/C GUI result.
The later GUI appendix below separately binds public source
`adcb50e8c7481a86f5c15e858dffb5a6dbd930ff`.
Native anisotropy, off-center focus, fractional-pan/old-start truncation parity,
clamp focus/offsets/overshoot/reversal/old-policy equivalence, full old
transformed pan/radial display parity, new raster/overlay/damage/clipping,
focus/device interruptions, physical input and other platforms remain deferred.
No all-196-hunk, whole-section 25, full aggregate, final Clone freeze,
packaging, release or whole-port acceptance is implied. Both historical old
GUI teardown `command_exit=-11` outcomes remain preserved. Old Edge/string-reader
runtime stays **SKIPPED/unverified**; no old runtime, capture or oracle was
accessed, executed or regenerated.


## Installed transformed-drag portrait GUI smoke (2026-10-17)

Independent child **34.005/transformed-drag-portrait-smoke** uses completed
03.009/tcp-portrait-environment, 25.009/transformed-drag-events and
29.001/native-canvas-tiles inputs. Original **34.005, 25.009 and
25.009/drag-zoom-scroll remain TODO with byte-identical criteria and dependencies**.
Only this bounded GUI child is newly accepted; component, strict Clone and GUI
execution retain separate exact-source provenance.

The actual installed GUI ran on public source
`adcb50e8c7481a86f5c15e858dffb5a6dbd930ff`, tree
`0ee3cd8130ee08db075f9b805df26cbb2b55bd54`. Full default incremental build
completed 96 steps, followed by install `--no-rebuild`; both exited 0. This is
not a clean-from-empty build. The fresh 1080x1920 depth-24 display accepted an
authenticated loopback-TCP client and rejected empty authority. Ordinary XTest
mouse/keyboard events operated the real installed application and View >
Painter Canvas Controls; no batch command, debugger or internal state setter
supplied the result. The archive retains **28 original full-resolution PNGs**.

One synthetic 720x1080 u8 RGB scene was set to **37 degrees, horizontal flip,
60% base zoom**. One held middle-button drag produced **60 -> 64 -> 68 -> 64 ->
60%**: Ctrl+Alt+middle with motion, Ctrl release while Alt+middle remained held
with further motion, Alt release while middle remained held with reverse
motion, return to the start and middle release. Released/settled frames
040/041 have identical full-frame RGB bytes. A separate ordinary middle-pan
displaced the pointer by **(36,-24) device pixels**, returned it to its start,
and released; fixed Painter panels stayed upright. The largest recorded gap
while held was 10.575 seconds under the unchanged 120-second idle limit.
There was no timeout/safety release, geometry retry or held input after cleanup.

Pan baseline/return frames 045/051 differ at **24 of 818,400 pixels** in the
exclusive screen rectangle `(90,520)-(970,1450)`. Independent pixel analysis
places all 24 within 3 pixels of the visible
rotated-artwork boundary, with maximum channel delta 2; it does not establish
a renderer-level cause. A strict artwork polygon inset by 3 pixels compares
exactly on return (0/271,104); translated pan at (+36,-24), with a cursor
exclusion, also compares exactly (0/264,704). Zoom baseline/return differs at
280/818,400 canvas pixels, comprising those boundary locations plus 256 brush-
cursor-region pixels; the strict inset polygon excluding the cursor compares
exactly (0/268,459). The released/settled equality concerns only frames 040/041.
No exact pan-return rectangle, full-frame equality or full-buffer claim is made.

Explicitly focused ordinary Save Image, close-view and Open Image dialogs
saved and reopened `transformed-drag-portrait-smoke.xcf` (124,278 bytes,
SHA-256 `01a1846108e57776bed5b132928f6524de81c7d936dfe2be4b7cb60616c5c931`).
The final file is independently rehashed. Equality with the immediate-save
hash relies on the parent's observed `sha256sum`/`stat` tool output after
request 064, chunk `249d9c` at 2026-10-17 14:01:31 UTC, before request 066;
**no contemporaneous on-disk hash artifact exists**, so the independent audit
cannot separately prove that temporal observation. Frame 072 reopens in a
0-degree, unflipped view at 100%; frame 074 restores 60% and centering through
ordinary controls. Between frames 005/074 the conservative
interior artwork rectangle `(326,661)-(754,1306)` has **0 different pixels out
of 276,060**, with identical RGB hash
`72c9b7715b50ea03bb532627b080c7786a410e8b5b7123f381ba843f69f1626c`.
This establishes only rendered interior equality, not whole-image numerical
or complete UI-state equality.

All **13,843 source entries** match before build, after build and after GUI;
all **2,842 installed-runtime entries** and both executable seals are unchanged.
Original source/runtime manifest SHA-256 values are respectively
`740b78b005664c245f31c1962fe45d9ab54dc75a2496730085f3d02d064a4a1c` and
`62882c45fde0e895fad1f8dbb03c3357b5349b52e8c9b2085178241a03d4e959`.
Runtime inventory covers installed bin/lib/libexec/share, not every external
dependency byte. Build/installed ELF Build IDs both equal
`415f537e7581e6fe450ae8b6cdf26ab5e1289c26`; their 6,099,050-byte `.text`
sections both hash to
`ce1df04ee90cc8b48b6fc7160e9eda10d16e92f90b31766c34a2b600aca01906`.
The independently sealed whole-file hashes differ, and installation removes
RUNPATH; this is not a claim that RUNPATH is the only changed byte range.
Application, driver, display, launcher and final seal check all exited 0,
with no cleanup error. Logs retain **112 unsupported fonts, 49 dconf warnings
and unavailable D-Bus**; no warning-free application is claimed. Historical
preparation-script/README source labels do not identify this run; accepted-source
files and three equal source seals do.

External archive `transformed-drag-portrait-gui-run-003-evidence.tar.xz`
contains 173 files and is 5,018,356 bytes:

- Archive SHA-256: `519a89036260d073521bd2795cf2646cd0a044f025d77f6b7492a8cbc2d1c705`
- Package-manifest SHA-256: `6aa101d031902e6157f555bfbb6a245f24a4d5d410764d0df67ac4ef9cff633d`
- Public `transformed-drag-portrait-gui-receipt.json` SHA-256: `33f7c210043fdad09dbd1c1aca4ee0c54df18b1dd08303ccea280e4e1da75aae`
- Original local receipt SHA-256: `b644c9bc471b6d2f3452e4417a902f2f559c11673d0718d94f87b38555aa7ff1`

Readback verifies all 172 manifest members. Raw PNG/XCF bytes are unchanged;
88 text copies replace declared project roots and the D-Bus machine ID.
Private profiles, authentication material, raw environments and compiled
executables are excluded. Embedded checksums identify original local text;
the package manifest maps it to normalized public bytes. Sanitized helper
paths are documentary placeholders, not a promised portable executable.
Original evidence remains separately retained. The independent read-only GUI
audit verifies all 28 screenshots, 28 request/response pairs, 27 historical
process-map snapshots, source/runtime/binary seals, gesture chronology,
current XCF hash and the qualified pixel comparisons above. Its temporal
XCF-hash limitation is retained; no extra GUI or native test was executed.

No new native aggregate or focused sanitizer was run on this GUI source or
its documentary descendant. The **41 normal/31 focused** component result
remains bound to `0c8620d9064f8c912d9424266771e045c276f582`; the **226 normal/62
focused** strict Clone result remains bound to
`1f911216f78d0702361bef782954b0ca14b056cf`. Only tasks.md, progress.tsv and this
contract change after GUI, within the existing strict receipt's documentary
exceptions. Current Clone route, validators, production/test/fixture bytes,
prior archives and receipts remain unchanged. Existing strict acceptance,
task-ledger, 44 receipt/17 archive/11 acceptance controls and whitespace checks
pass. No new native execution is implied by these documentary checks.

Physical devices, native anisotropy/fractional displacement, off-center/clamp
focus, full legacy transformed-pan/radial parity, focus/device interruptions,
arbitrary GUI image-anchored GTK children, exhaustive overlay/selection,
damage/clipping/extents/hit-testing and other platforms remain outside this
bounded smoke. No all-196-hunk, whole-section 25, full aggregate, final Clone
freeze, package/release or whole-port acceptance is implied. Both old GUI
teardown -11 outcomes remain historical. Old Edge/string-reader runtime stays
**SKIPPED/unverified**; no old runtime, capture or oracle was accessed, executed
or regenerated.


## Bounded 25.009/fractional-pan-difference acceptance (2026-10-17)

Only the existing independent **25.009/fractional-pan-difference** child is
marked DONE, with completed inputs 25.008 and 01.017. Original **25.009 and
25.009/drag-zoom-scroll remain TODO with byte-identical criteria and
dependencies**; every other task and progress row is unchanged. This is a
source-backed regression for the existing modern integer-device
replacement/difference, **not approval or redefinition of exact historical
compatibility**. Old inverse-before-gint/double-offset arithmetic is not an
old applied runtime, raster or overlay oracle; those obligations remain open.

### Exact component source A

The unchanged component evidence belongs only to public source
`8e92569df4841a458202de2dd89ed6db1cb4c974`, tree
`25bec9e511414ea7dca1e8fd7ff9740d089a7b31`: **41 normal TAP cases (31 event,
4 UI, 6 pure) and 31 focused event ASan/UBSan cases**, nine instrumented C
units, all four native commands exit 0. Original receipt SHA-256 is
`459c289f3c2cc0d61c4e5f010731d3ff669fb6264dae8ad4c7748df00abf4f61`.
The committed component summary and compact archive remain byte-identical;
their historical TODO/pending-strict checkpoint is not rewritten by this
later acceptance. The compact archive preserves 23 original nonbinary files
plus its manifest; eleven omitted binaries remain hash/size-bound to the
separately retained 34-file full public archive.

One additional finite subcase inside the existing device-image-origin
registration uses unmirrored 37 degrees, scale 1.5 and floating displacement
(17.6,-10.6), which quantizes to (17,-11). Native offsets are (-236,-31),
quantized image-anchor delta is approximately zero, and raw pointer drift is
(0.47993821019281313,-0.0277565399148898), matching the independent frozen
expectation within 1e-8. Ordinary release clears state and grabs through the
existing observation boundary. No production repair or extra registration
was introduced. Per execution, 17 current main drags, 16 stationary reentries,
six primary pairs, 33 pan-helper calls, 99 forward landmark comparisons and
38 retained actual GTK snapshots pass at one 849x450 viewport. Historical
16-drag receipts/manifests remain unchanged. This adds no saved PNG, new
raster/overlay/damage/clipping or actual-window GUI acceptance. Nine component
integrity seals, 2,873 normal ELF entries and all 361 catalogs were preserved.

### Distinct strict source B and documentary C

The separate unchanged strict Clone gate belongs only to public source
`fbe7efacdec68938d5334be6ea56cd83b0d0afdf`, tree
`ef9ae3842a659bb1fabbe6631115c3faa78a03f8`. B integrates A's component evidence
and narration in exactly five files; it inherits no native component or GUI
pass. Run `fractional-pan-run-001` completed default build, normal tests and
focused sanitizer commands with exit 0:

- Current archive: `migration/tests/fractional-pan-acceptance-evidence.tar.xz`
- Archive SHA-256: `0325d237fb3cfc3597e0b49e8b58418905c59d486bb1b39c0ab3ac35e1cc5cc9`
- Receipt SHA-256: `b21f7750946d36776c0fd05ab75eb9c198555b84a9a6721effb38f03c947237b`

Seven normal targets pass **226 positive TAP cases**: core 4,
projection-update-bounds 3, gimp-clone-layer 62, painter-layer-ui 68,
painter-layer-tree 35, painter-xcf-roundtrip 49 and update-geometry 5.
Focused Clone ASan/UBSan passes **62 cases**; normal and focused Clone each
retain 216 unique source-mode/type/mask scenes. Eight units are instrumented;
54 RTTI-only C++ rebuilds are not sanitizer coverage. LSan is off. This gate
does not rerun navigation or actual-window GUI.

Source seals before build/before tests/after agree. The unchanged committed
checker reconstructs full source identity and current-checkout seals,
2,883 normal ELF entries, default-build freshness, compile database,
all 164 registered target definitions, 361 catalogs and focused artifacts.
The registry seal is not full aggregate execution. The deterministic archive
contains exactly 17 unchanged public files; original-directory, extracted
and archive checks agree with native source/build/private paths blocked and
Git as the only subprocess. Existing bounded extraction rejects unsafe,
duplicate, unexpected or nonregular members. All original and nested hashes
are verified; private profiles, raw environments and credentials are excluded.

The unchanged **44 receipt, 17 archive and 11 acceptance controls**, strict
current acceptance, task-ledger and whitespace checks pass. Final documentary
C changes exactly five paths: current_core_receipt routing in clone.json,
this appended contract, the existing fractional child task/progress rows and
the new archive. Matrix gates and documentary/storage exceptions are
unchanged. All other tracked source, test, fixture, validator, procedure,
component receipt/archive and navigation-contract bytes remain unchanged.
**Component native claims remain on A, strict Clone claims on B; C has no
native execution claim.**

Latest separately accepted actual-window GUI remains exact
`adcb50e8c7481a86f5c15e858dffb5a6dbd930ff` only, without transfer to A/B/C.
Remaining GIMP/GTK/dependencies are uninstrumented; component builds retain
the missing-declarations warning and ordinary runtime diagnostics. Native
anisotropy, off-center/clamp focus, offsets/overshoot/reversal/old-policy
equivalence, full legacy pan/radial behavior, physical-device interruption
and other platforms remain open. Both historical old GUI teardown
`command_exit=-11` outcomes remain preserved. Old Edge/string-reader runtime
remains **user-approved SKIPPED/unverified**; no old runtime/capture/oracle was
accessed or regenerated. No all-196-hunk, parent/whole-section 25, full
aggregate, final Clone freeze, package, release or whole-port acceptance.


## Ordinary Meson sanitizer workflow correction (2026-10-17)

This is a current acceptance correction, not a rewrite of the historical runs
above. The general native-build contracts were already mandatory when the
2026-10-17 Clone and navigation focused runs were performed:

- `b2a6948f49c3f3ecd30b3292a8d1484ca72b0aa4`, 2026-10-11 08:45:49 UTC,
  added the ordinary Meson sanitizer workflow in `rtti-separation.md`.
- `8121f485447917938baaf57dc4627516a109f367`, 2026-10-11 08:48:31 UTC,
  explicitly prohibited restoring individually substituted object files in
  `native-build-workflow.md`.

Later component contracts and builders prescribed the conflicting focused
per-object approach. They did not record a waiver of the general rule. The
current general rule is authoritative: use an ordinary, fresh out-of-source
Meson sanitizer configuration with the prescribed consistent RTTI, without
rewriting generated backend state or substituting individually rebuilt objects.

The recorded normal and focused passes remain real, source-bound observations.
For example, the latest strict Clone run at `fbe7efacdec68938d5334be6ea56cd83b0d0afdf`
passed 226 normal cases and 62 focused cases; its eight instrumented sources and
54 RTTI-only compatibility sources must retain those labels. The navigation
run at `8e92569df4841a458202de2dd89ed6db1cb4c974` passed 41 normal and 31 focused
cases with nine C units instrumented. Neither observation proves the required
ordinary-Meson sanitizer acceptance. The earlier all-163 normal aggregate at
`f271db84bb9cd3d99c404a43d5c727c19d96ca5f` is also not a later-source sanitizer
or aggregate result. No historical archive, log, source seal or failure is erased.

Fourteen direct task checks are reopened pending compliant acceptance:
`14.007/component-visibility-regression`,
`14.007/source-mode-projection-regression`, originals `25.001` through `25.008`,
`25.008/mirrored-arrow-input`, and `25.009/common-angle-draw`,
`25.009/transformed-drag-events`, `25.009/fractional-pan-difference`.
The three dependent portrait GUI checks under `34.005` are unchecked/BLOCKED
because their required predecessor checks are reopened. Their actual installed
GUI observations, screenshots and exact-source limitations remain valid; no GUI
failure is inferred from this workflow correction. Unrelated parent and legacy
compatibility obligations remain open as before.

All 28 current Clone matrix rows now carry an explicit ordinary-Meson pending
gap; the 23 previously COMPONENT_VERIFIED rows are PARTIAL. This changes current
acceptance status, not the recorded behavior or old completed snapshots. The
unchanged `current_core_receipt` still locates and verifies its exact historical
focused evidence. A green `check_clone_acceptance.py` at this interim checkpoint
means that evidence and source reconciliation succeeded; it is expressly not
ordinary-Meson workflow acceptance. The separate
`ordinary_meson_sanitizer_acceptance.status` in `clone.json` is PENDING and has no
observed ordinary results. No validator exception or weakened assertion was
introduced to turn that pending requirement into a pass.

The bounded corrective plan is one frozen published revision and one fresh
ordinary Meson `b_sanitize=address,undefined` build, running the four existing
registrations `gimp-clone-layer`, `painter-navigation-events`,
`painter-navigation-ui`, `painter-navigation` (62 + 31 + 4 + 6 = 103 cases).
Verify actual compile commands and linked inputs, including the prescribed
Meson RTTI policy, rather than counting configured flags alone. Do not replace
objects or archives after Meson builds them. Run the existing ten-target normal
union on the same frozen revision (the seven Clone component targets plus
those three navigation targets, 267 cases); this is a bounded component run,
not a new whole-registration aggregate. Retain exact source/tree, submodule,
configuration, compiler, source/ELF/metadata seals, all command statuses, all
stdout/stderr and the authenticated portrait TCP Xvfb checks. Leak detection
remains unavailable in the confirmed executor; ASan, UBSan and vptr checks are
not suppressed. Existing normal receipts can establish comparability but do
not replace the newly required executions. Failed attempts stay preserved.

Only after those gates pass may the explicitly reopened checks be reconsidered.
This correction makes no ordinary-sanitizer PASS, new GUI, whole-port, historical
pan-parity, platform or release-completion claim.


## Ordinary Meson acceptance restored (2026-10-17)

This section supersedes the pending current state of the dated correction above,
while retaining that correction and all historical focused results. The exact
tested source is `47202852286b654d88d3fa4f0bafd59e08030cad`, tree
`47edf23ea7f95390751c76d649acb9429d38ff61`, with pinned gimp-data
`a31e5dea4f572fb5f33b06f5c4b86db5f30643dc`. The ten complete normal registrations
passed **267** positive cases. The four complete ordinary Meson ASan/UBSan
registrations passed **103**: Clone 62, navigation events 31, navigation UI 4,
and navigation 6. No skip, TODO, timeout, crash or sanitizer diagnostic qualified
as a passing case. Both batches used fresh isolated profiles, all 361 catalogs
and authenticated TCP portrait Xvfb at 1080x1920.

The fresh ordinary `b_sanitize=address,undefined` build was created in run-002.
Structural compile/link evidence accounts for 1,373 instrumented runtime objects
and three separately proven build-machine generator objects, 33 runtime archives,
seven shared libraries and the four test links. Prescribed RTTI is retained;
no individual object substitution or generated-backend rewriting occurred.
Run-003 completed only the three declared missing menu resources. Run-004
executed zero build commands and reran the complete 267+103 cases on that exact
sealed build. Source, ELF, full freshness, metadata and resource fingerprints
remained identical through all five current phases. Current `before_build` and
`after_build` keys mean pre/post reuse verification, not a new compilation.

Failures remain explicit. Run-001 stopped at an overbroad build-generator audit;
run-002 stopped on a fatal missing-menu warning; run-003 hit the unchanged 1 GB
disk reserve during normal large-XCF testing. Run-004 restored 4 GB admission
and retained the 1 GB live reserve. Its native batches passed, but its owner
exited **1** because the public writer redacted the fixed ASAN_OPTIONS and
UBSAN_OPTIONS strings, and the offline validator correctly rejected those
redacted fields. The original public tree, receipt, policy and failed validation
are preserved byte-for-byte inside the package.

A separate reviewed policy-disclosure supplement, executed with exit **0** and
no native rerun, extracts only allowlisted settings from the four actual private
Meson result rows. Their original raw-log hash already matches the sealed
transformation manifest. Independent extraction reproduces the attestation.
The unchanged frozen validator must first reproduce the sole original failure;
a one-use in-memory adapter then supplies only those two observed policy values,
and every original assertion must pass. This is a reviewed extraction attestation:
the withheld raw environment cannot be reconstructed from its hash. No private
environment, credentials, suppression file or invented test result is published.

The canonical artifact is
`migration/tests/ordinary-meson-clone-navigation-evidence.tar.xz`, SHA256
`7b4a3ab67e7d008f3889f57df9b22d1a6ea0c9279646aab605b74398b75c3b2f`.
It contains the exact blocked run-004 archive once, including both failed
provenance origins, plus the pinned disclosure helper, attestation and validation.
The original execution receipt SHA256 is
`db0cbf6086c25eb82314ff649a8b01aa5079e8dec349c1b697377f90b854186f`.
All execution/recovery pins and both distinct terminal outcomes are recorded
in `ordinary_meson_supplement` in `migration/acceptance/clone.json`.

The authoritative current check is:

```sh
python3 -B tools/check_ordinary_meson_acceptance.py --root . --frozen-source /path/to/exact-47202852-source
```

The first publication also requires `--audit-closure`. The default gate allows
subsequent documentary progress while preserving covered acceptance facts; it
does not permanently freeze unrelated task rows. It validates the unchanged
historical focused checker against the exact frozen source and validates the
current native-input equivalence separately. Only the four existing documentary
paths and three exact hash-pinned integration additions are permitted. Production,
test, build and historical helper/archive changes still fail. The new checker and
its controls are artifact-only additions, not code executed by the native runs.

This restores precisely the fourteen directly reopened checks and three
GUI-dependency checks. The latter retain their existing exact-source observations
and screenshots; they were not rerun. Exactly the original 23 Clone rows return
to COMPONENT_VERIFIED; 14.007, 14.009, 14.018, 14.019 and 14.020 remain PARTIAL.
Original parents 14.006, 14.007, 25.009, 25.009/drag-zoom-scroll and 34.005 remain
open. Historical pan-origin differences remain unresolved. Leak detection stays
unavailable, external dependencies are uninstrumented, and these 103 cases do
not establish whole-application, platform, release or whole-port acceptance.
The earlier f271 all-163 normal aggregate remains the only claimed full aggregate.
