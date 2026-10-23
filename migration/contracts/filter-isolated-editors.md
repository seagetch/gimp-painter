# Native GTK editors for the four isolated FilterLayer routes

WBS `30.001/isolated-filter-editors` exposes the already accepted Blinds,
Small Tiles, Retinex and Convolution executors through New/Edit Filter Layer.
These are independent Painter document layers. The existing definition-edit
transaction, Undo, scheduler, process bridge and completed GEGL cache remain
authoritative; this UI introduces no filter algorithm or GEGL effect lifecycle.

## Definition editing

The literal procedure names and supported typed shapes are unchanged. The
editor borrows an immutable argument snapshot when loading these four routes.
No-op OK and Cancel perform no definition transaction. Editing the same route
uses the [Phase C immutable snapshot patch](filter-parameter-editor.md) after
checking the loaded definition revision and current provider, and patches only
the controls whose numeric bits or semantic choice actually changed. Untouched
slots, including reference provenance and huge ignored tails, remain shared. Other scalars, array elements, context IDs, raw metadata and
Convolution's ignored twelfth value remain untouched. A stale editor reports
that it must be reopened instead of overwriting a newer definition.

ASCII numeric entries use GLib's round-trip binary64 representation, accept
scientific notation, and reject incomplete, nonfinite, overflowed or out-of-range
input without closing the dialog or modifying the model/Undo/cache. Integer
parameters require complete decimal integers. Blinds retains angle0..90 and
segments1..100; Small Tiles retains factor0..6; Retinex retains scale16..256,
nscales0..8 and dynamic0..4. Convolution presents25 coefficients in five image
rows, mapped to the old x-major array index `x*5+y`, plus divisor and byte-unit
offset. Its divisor must be nonzero; only actual U8 nonlinear images apply the
old float representability limits. Other precisions retain binary64 range.
Execution still owns image-, selection- and intermediate-result validation.

Blinds direction is a Horizontal/Vertical combo; the old integer1 means vertical
and every other integer means horizontal. Transparency and the five Convolution
channels are checkboxes; any nonzero saved integer means enabled. An unchanged
semantic setting, including a toggle away and back, preserves the exact signed
integer. An explicitly changed setting writes0/1 only to that field. Distribution
and border use named combos. An unsupported saved enum is visibly labeled and
retained until the user chooses a supported value. The old Convolution alpha
weighting argument is retained; the UI explains that legacy noninteractive
execution ignores it. It does not advertise an ineffective option as a working
control.

Unknown procedures/shapes remain selected as Keep saved definition, with types,
values and raw metadata visible. Numeric-array previews show at most256 elements
through an immutable borrow, including their byte lengths; they do not copy an
entire array for the preview. Phase C subsequently adds a64 KiB total valid-UTF-8 preview bound, bounded
string/STRV/procedure-name formatting, and a512-entry traversal budget, while
retaining the4096-byte raw presentation cap. See the linked implementation
contract for copied versus shared allocation accounting; this is not a total-RSS
claim.

## Ownership, execution and limits

Every new signal uses the existing typed BindingStore and weak DialogHook. Dialog
close disconnects the hooks; retained or detached child widgets do not retain an
implementation. Validation and status updates tolerate reentrant destruction.
Existing dialog Cancel, filter status, definition Undo/Redo and running-generation
cancellation/previous-cache behavior remain unchanged.

The UI states the old Blinds/Small Tiles U8 nonlinear RGB/Gray restriction and
Retinex U8 nonlinear RGB/minimum16×16 selected-region restriction. Saved values
are not replaced by modern plugin defaults. Unsupported execution still retains
the definition and prior completed pixels. Linux native GTK tests and the built
app walkthrough are reported separately from core/algorithm acceptance in
`../tests/filter-isolated-editors/acceptance.json`; this task does not rerun or
expand the already accepted old-PDB numerical corpus. Windows/macOS, tablet
workflows, all popup contexts, owner progress forwarding and global resource/
latency acceptance remain separate gates.

## Recorded acceptance

The retained native GTK logs pass 35 groups in each of normal and focused
ASan/UBSan mode, followed by two boundary groups in each mode, all with exit0.
This is36 unique groups per mode (24 existing and12 isolated-editor groups),
not37: invalid-input is repeated. The editor implementation hash is identical
in all four runs. Comparing the full-suite test snapshot with final sources
shows only the added precision-range group and whitespace-invalid case; both
passed in the final-source boundary runs. The final36-group registration was
not rerun as a single suite. Source hashes, the test-only delta, log hashes and
final executable identities are recorded in the acceptance JSON.

The sanitizer builder instruments seven translation units. The retained compile
database independently resolves51 additional production C++ units rebuilt for
RTTI compatibility only. GTK, remaining core, filter executors and dependencies
are outside this focused instrumentation, and leak detection is disabled. The
final sanitizer build report/source archive was not recovered. All four logs
retain localization notices and the known test-profile writable data-folder
save diagnostic; passing tests do not establish warning-free execution.

The built-app walkthrough was reported to complete all four editors, the5×5
grid, named combos/checkboxes, validation, Undo/Redo and Save-close-reopen-edit
without crashes. Its raw log was lost with the previous runtime's shared
directory, so this documentation pass does not independently verify that flow
or its built-app binary identity. GTK/GDK accessibility-related critical
diagnostics were also reported; their exact cause remains unverified. This
reported manual result is separate from the preserved automated evidence.
