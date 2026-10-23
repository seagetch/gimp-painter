# Atomic native Fill Brush and Painter Smudge generic stroking

The existing prepared-coordinate route in `gimppaintcore-stroke.c` now dispatches
native raw Stroke, Path and Boundary to the existing typed Fill/Smudge owner
slots. There is no second renderer, implementation store or native base ABI
change. Bare unowned native start remains refused.

Each operation retains the core, original options, drawable, image and watched
resources through callbacks. It validates all fourteen numeric coordinate axes
before starting and rejects nested or already-pending image transactions without
cancelling them. One native start/snapshot spans every disconnected subpath;
FINISH/INIT resets family-local Fill projection/search or Smudge accumulation
state while native cumulative interpolation state and one original Undo snapshot
remain intact. Each accepted motion drains the existing resumable step API.
Image-space inputs retain the existing per-family offset arithmetic.

An operation-local shared cancellation record watches options and original
brush/dynamics/pattern/gradient resources through common Connection and weak
signal payloads. It neither captures an Impl pointer in a callback nor clones
live brush-pipe state. Before native commit, cancellation, close, preparation or
publication failure rolls back the whole owned transaction when the target is
still current. Independently changed raster/format/extent data is preserved
instead of receiving a stale snapshot. The public coordinate/distance bookkeeping
is restored on failed operations. There is no Undo/Redo stack surgery.

The requested push_undo policy is stored in the existing slot. Commit is sealed
after FINISH callbacks and a fresh target validity check, immediately before
native finish consumes the snapshot. Later notifications emitted while native
Undo/image-dirty dispatch is already committing cannot retroactively report a
rollback. Pending-paint queries remain true through actual commit/rollback.
Outside-canvas strokes are genuine no-ops. Existing zero-opacity native extent/
Undo behavior is retained rather than silently replaced with a new policy.

## Native preparation fixes

The native boundary sorter changes visited flags despite its const signature.
Owned dispatch now sorts a copy, preserving caller-owned arrays. Offset additions
are promoted before arithmetic, avoiding signed integer overflow.

Real Path tests exposed uninitialized view metadata in native Bezier subdivision:
coordinate arithmetic assigns paint axes but does not assign xscale, yscale,
angle or reflect. The producer now seeds each subdivided coordinate from its
source endpoint before arithmetic. This changes no paint-axis interpolation and
does not weaken validation of raw or custom-stroke coordinates.

## Independent evidence

`legacy-owned-generic` compiles the actual pinned old Stroke/Vectors/Boundary
entrypoints from unchanged archives. It has three routes for each of the existing
12 Fill and 48 Smudge scenes: raw input, two separated path subpaths, and two
boundary loops with offset and emulated dynamics. Its fixture header constructs
equivalent stored direction: old Bezier lineto prepends anchors, whereas GIMP3
appends. Using superficially identical constructor argument order produces
opposite traversal and was rejected as an invalid comparison. Initial mismatched
runs and the producer diagnostic are not described as successful evidence.

The existing family native test fixtures add actual generic API lifecycle groups:
one Undo/Redo, no-Undo with existing Redo, no-op, offset/selection and separated
Path/Boundary geometry, immutable caller arrays, nested/active admission,
first/second-subpath failure, every nonfinite axis, option/resource changes,
explicit owner close, last caller-owned core/options reference loss, FINISH
cancellation/external pixels, pending Save queries, coordinate restoration and
harmless postcommit notification.

All six independent generic comparisons pass exactly: 690 records / 14,303,286
normalized bytes across 180 scenes. The final normal run passes 36 Fill lifecycle
groups, 23 Smudge lifecycle groups, all five unchanged old family trace variants
and the MyPaint generic regression suite. See `owned-generic-native.log`,
`owned-generic-native-tests.{txt,jsonl}` and `owned-generic-native-oracle.json`
for the exact checkpoint outcomes. The focused sanitizer builder is provided but no successful
sanitizer run is claimed until its report is present. This is a native API
checkpoint, not a physical-device, standalone PDB transport or full GUI claim.

The existing nonlinear-u8, symmetry, ICC, pipe, large-dab memory/latency and legacy
geometry acceptance boundaries remain. In particular, this work does not infer
legacy geometry from a standard brush's blend mode or paper option.

## Current ordinary-Meson acceptance in progress (2026-10-18)

The historical normal checkpoint above is retained, but its source allowlist no
longer matches five current files: `gimpfillbrush.cpp`, `gimpfillbrush.h`,
`gimppaintersmudge.cpp`, `painter-native-stroking.hpp`, and
`test-gimp-fill-brush.cpp`. It is not current-source acceptance. The old
`run_owned_generic_sanitizers.py` object-substitution harness is historical and
must not be used for the required current gate.

The bounded completion adds ordinary Meson registrations for the six retained
Fill/Smudge raw, Path and Boundary comparisons, without changing their fixture
bytes or executing the old runtime. Acceptance requires a frozen current source,
a fresh ordinary normal build and unfiltered registered aggregate, plus a
separate ordinary ASan/UBSan build covering the complete Fill, Smudge and
MyPaint batch suites, all six generic comparisons and the five existing family
trace variants. Keep RTTI/vptr enabled, record any unavailable LSan coverage,
and preserve all failures and source/build/runtime seals. No current native
pass or task closure is claimed by this preparation checkpoint.

Only `27.009/owned-generic-strokes` can close after these gates. The parent
`27.009`, standalone PDB transport, full GUI/device, precision, resource/latency
and platform obligations remain open.

### First current aggregate and fixture repair in progress

The unchanged unfiltered aggregate on published
`99d398b54cf86fad3e9a6cb471373f8042db152d` completed all 171 registrations:
170 passed and `filter-entry-contracts` failed. All six generic comparisons
passed, and source, tree, ELF, registry and locale integrity checks passed.
The aggregate is rejected; it is not current acceptance for this task.

The failing destructive-entry subprocess dropped Gimp while a closed startup
plug-in's low-priority child watch still owned its restore PDB context and
stroke options. The fatal two-context teardown warning remains a failure.
The test-only repair waits for observable retirement of these unrelated
startup owners before the three destructive-entry cases, with the existing
bounded deadline. It retains the real restored Blinds provider, fatal warnings
and all entry/reentry/finalization assertions. The repaired 35-case registration
passed its complete ordinary normal prepublication check (35 cases, original
120-second budget, 8.75 seconds), with source/build/ELF/catalog seals unchanged.
Its complete ASan/UBSan validation and the new unfiltered normal aggregate remain
pending alongside the generic-stroke gate. No task closure is claimed here.

### Fresh full normal checkpoint (2026-10-18 11:22 UTC)

After executor replacement, exact published source
`cedf6968c2bfd1d7f7b0623be713b27e449ac3c2` was rebuilt with all 560 pinned
dependencies in a fresh ordinary Meson source/build pair. The complete serial,
unfiltered normal aggregate passed **171/171**, with no failures, timeouts,
registration skips, budget multipliers or baseline exemptions. All 493 declared
catalogs were built; 361 were staged. The authenticated loopback-only portrait
display, source/build/ELF/registry/catalog integrity and final freshness seals
passed. The normal package gate accepted this exact pre-cleanup build.

See `../tests/owned-generic-fresh-normal-20261018.json` for all registration
outcomes and the checksum-pinned, readback-verified private evidence checkpoint.
The initial fresh aggregate launch stopped before tests because its outer shell
had not sourced the dependency environment; that failed attempt is retained.
The successful retry sourced the unchanged environment. No source, test, warning
policy or registered budget was changed. The earlier rejected 170/171 aggregate
also remains failed historical evidence.

The separate fresh full-default ASan/UBSan build, linked-object audit and all
15 complete original-budget registrations passed, followed by the final seal.
The linked runtime closure contains 1,377 instrumented objects, 33 archives,
six test executables and seven local shared libraries; RTTI/vptr checks remain
intact. All compiler diagnostics are retained, including 842 warning occurrences.
External dependencies are identity-sealed, not claimed instrumented; LSan remains
excluded only by the reproduced host ptrace limitation.

Three legacy Fill Brush registrations explicitly set their private build-profile
directory. The original wrapper rejected that registered value after a successful
native comparison. That attempt remains failed. A separately reviewed invocation
amendment aligns only the launch GIMP3_DIRECTORY to the exact registered private
path; Meson's effective native environment and every original validator remain
unchanged. All three complete registrations then passed fresh attempts. The base
preparation/snapshot stay immutable; the receipt explicitly binds the amendment,
its negative controls and independent review, rather than claiming unchanged
invocation bytes.

See `../tests/owned-generic-ordinary-acceptance.json` and its reviewed evidence
archive. Acceptance belongs to cedf6968c2bfd1d7f7b0623be713b27e449ac3c2,
tree 8b5b431de5ec05ee1bb6bc36cbccf4e1dd2920a9, with pinned gimp-data
a31e5dea4f572fb5f33b06f5c4b86db5f30643dc. Only the owned-generic-strokes and
blinds-entry-fixture-quiescence children close. This later documentary commit
changes no native input and has no new native/GUI run attributed to itself.

After preserving the normal evidence, 2,814 reproducible compiled outputs were
permanently disposed of under an independently reviewed finite manifest,
recovering 6,520,283,136 bytes. This was not reversible parking. Source, generated
inputs, the two application binaries, registry, reports and evidence were retained,
but the remaining normal tree is not a complete runtime. Future runtime packaging
requires a full rebuild and fresh aggregate revalidation. Parents 27.009/08.018,
PDB, full GUI/device, resource/latency, precision/platform and release obligations
remain open.
