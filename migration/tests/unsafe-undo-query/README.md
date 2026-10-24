# Separate cancelable pending paint from unsafe native Undo

The exact `503ade3466` aggregate failed the existing Smudge UI `undo-pending`
case: the generic pop preflight reused the Save/admission pending-paint query.
Cancelable native Smudge/Fill strokes must reach their normal Undo notifications;
paired Painter evaluator/pixel transactions must instead reject history movement.

`query-unsafe-undo` is an independent read-only, true-handled boolean signal with
FALSE/no-listener default. It adds no GimpImageClass vfunc, layout, property,
persistent value or alternate state registry. The six-provider delta uses existing
Painter transaction state: core start/end/sample/open/batch/history, operation
active, and tool busy. Paired scoped connections cover transient hover and ordinary
transaction lifetimes. All old `query-pending-paint` predicates and Save/admission
calls remain unchanged, including idle interactive Save completion policy.

## Exact evidence

Private links use exact503 compiled dependencies and the device audit's immutable
regular 254-object core archive, not the concurrently rewritten thin core archive.
Five production translation units are freshly compiled; only six product files
and two test files differ. Link input hashes/commands and predecessor archive
member hashes are recorded. This is a bounded overlay proof, not a whole-HEAD
aggregate. Native **148** cases pass: pop guard13, paired18, original SmudgeUI26,
Fill40, Session10, actual Save continuation13, MyPaint Tool27 and paired real UI1.
The half-sample Tool assertion additionally checks unsafe=true, rejected Undo/Redo,
unchanged stack depth and unsafe=false after release. Five-unit scoped ASan/UBSan
passes **32** cases (pop13, paired18, half-sample1), dependencies/fixtures otherwise
uninstrumented, leak detection disabled. Four C11/C++14 first/double include probes
use the actual internal GIO/GEGL/GdkPixbuf/core-types prerequisites.

Two linked causal mutants fail their expected assertions: restoring the old
pending-paint generic gate rejects cancelable-only paint; restoring the old core
without its unsafe connection permits an idle paired stroke to move history.
Existing preflight reject/unwind, nested groups, callbacks, last-owner disposal,
ordinary Undo/Redo and mixed pending-only/unsafe owner controls remain intact.

The initial Save replay lacked export-procedure registration (native XCF cases
passed). Its failure is retained, not suppressed. Building the configured current
file-png target (138 actions, no source changes) restored real registration; all13
cases then passed, including Save/export-back/export pixel reopen. No fake exporter
was substituted. Plugin/library hashes and build/source receipts are included.
Initial private compiler include-path setup failures are likewise retained.

## Images / limitations

`00-baseline.png` through `03-redo.png` are actual rendered application captures
from Xvfb1080x1920, portrait application1040x1800, oncanvas enabled and asserted.
The fixture drives the ordinary native APIs, not physical pointer clicks; visible
canvas bytes match baseline after Undo and painted after Redo. Existing missing
icon theme warnings remain visible and are not a new UI completeness claim.
No paused acquisition/preview/tree-popup/Smudge candidate diagnostics or providers
were invoked. No arbitrary allocator safety, old raw-pointer ABI, full section
completion, or fresh whole-application sanitizer claim is made.

## Admission

`validate.py [--commit SHA] --negative-controls` binds all eight owned sources,
predecessor archive, runtime receipts and exact saved-policy invariants. Source
bindings of older evidence remain historical; no original manifests are repinned.
The new gimpimage.c requires a separate exact persistence-source successor after
commit; original1379 fields/family files and dirty candidates are not changed here.
