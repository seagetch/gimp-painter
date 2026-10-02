# Native nonlinear RGB-u8 Surface

The extended Surface now accepts native nonlinear RGB-u8 drawables as well as
RGBA-u8. It keeps the drawable's native Babl space and channel count rather than
silently inserting an alpha channel. Direct pixel iteration uses the existing
pinned three-channel blend/sample branches. Nonincremental paint still uses an
RGBA floating stroke, then composites against the original RGB snapshot with
the same native three-channel background/eraser semantics. The drawable stays
RGB through finish, cancel, Undo and Redo. Other precisions/models continue to
return an explicit unsupported-format error.

An independent old executable captured 96 warmed scenarios and 288 complete
130x96 RGB finish/Undo/Redo images. The scene matrix crosses nonincremental,
ordinary shape, paper, offset selection and smudge with normal/eraser/actual
layer alpha lock. All 385 records match the new production session byte for
byte: 21,575,701 bytes, SHA256
`64cdb3add5ad92af01fe93f079a0378b4dc7a7caefe56406fd29c67747ee00d7`.
The original 32 RGBA scenes/129 records remain exact independently.

The old reference retains the documented warm-up/split/Undo/reset sequence.
Cold old nonincremental equivalence is not claimed. The new production trace
uses the supported cold controller. Sixteen full-lock + nonincremental scenes
are visibly unchanged in the old output but still create a native Undo entry;
that observed behavior is verified explicitly. The old undefined zero-alpha
conversion is expressed through the already tested finite compatibility branch.

The first extra RGB capture tried brush lock-alpha without setting the layer
flag. Both old and new controllers override it from the actual layer at start.
That setup finding is kept separately; only the corrected real layer-lock
capture supplies the golden reference. Its optional reproduction runner also
matched all 21,575,701 bytes against independently rebuilt old harness code.

Native production gates include ten Surface cases (the added RGB case verifies
cancel, native format retention and real Undo/Redo in both accumulation modes
with normal and eraser brushes), ten shared-model cases, eight session cases,
six hover cases, four isolated-preview cases, both old full-session oracles and
complete RGB fixture checks. These are synthetic stimuli in the actual GIMP
application. They do not establish arbitrary ICC behavior, gray/indexed/high
precision, physical tablet input, Windows or macOS acceptance.

Focused sanitizer reports use private thin archives with ASan/UBSan/
float-cast-overflow, consistent RTTI/vptr, and LeakSanitizer disabled. The RGB
trace report checks every golden record and stores a hash plus non-record
diagnostics instead of duplicating full image data. Source/executable hashes
and the exact instrumented scope are retained; unlisted dependencies remain
uninstrumented. Normal production objects are not replaced.
