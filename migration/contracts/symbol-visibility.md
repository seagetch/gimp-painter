# Symbol visibility (original WBS 04.012)

Painter's C++ implementations and templates are private application ABI. C
callers keep their existing entry points; the installed libgimp SDK and GModule
entry points keep their existing exports. No app archive, app header or `.hpp`
is installed as a new public SDK.

## Declaration and compiler policy

`app/painter/gimp-painter-visibility.h` is dependency-free. In C++ with GNU
attributes, `GIMP_PAINTER_PRIVATE` marks hidden declarations and
`GIMP_PAINTER_C_ENTRY` explicitly preserves default visibility on C entries.
Both macros are empty in C, preserving the shared record declarations. Other
toolchains retain their normal explicit-export model; their ABI requires its
own platform verification.

All 67 production private headers annotate the declaration blocks for
`GimpPainter`, `GimpPainterXcf` and `gimp::painter::xcf`. Namespace attributes
cover declarations in that block, not arbitrary declarations in a later
unannotated reopening. Source-only HTTP declarations are therefore annotated
as well. Two private GType getters used only by their translation unit have
internal linkage; their registered GType names and behavior are unchanged.

Opaque save/snapshot implementations and the three shared Filter records have
private C++ type visibility. C entries using these types explicitly retain
default visibility: hidden parameter/return types can otherwise hide the
function despite its C language linkage. The patch changes neither record
fields nor function signatures.

GCC can still emit default-visible weak STL constructor aliases whose arguments
contain a hidden private class. The C++-only `-fvisibility-inlines-hidden`
switch closes this gap and is required when the compiler uses GNU argument
syntax. It does not replace declaration attributes, does not add blanket
`-fvisibility=hidden` to mixed targets, and does not change C compiler options.
No archive/source grouping or final link recipe changes.

This switch also removes some ordinary inline C++ emissions, including callback
trampolines from existing core/paint code. Those are separately listed in the
native evidence. They are app-internal implementations with no installed C++
header contract; they are not mislabeled as Painter namespace symbols or as
preserved C exports. Existing C entry points, SDK and module exports remain
strictly preserved. Exception and RTTI settings remain unchanged.

## Native acceptance

The [native report](../tests/symbol-visibility/report.json) examines real default
and HTTP-enabled GUI, console and filter-worker executables, whose unchanged
link recipes still contain `--export-dynamic`. Both readelf and nm must agree
on every defined dynamic export. The six outputs lose respectively
543/510/502 and 617/587/502 private C++ exports, leaving zero in the classified
private ABI. The classifier was reviewed against all actual C++ export names,
including private record/container derivatives outside the namespace families.

Every baseline nonmangled application export remains. All nine SDK libraries
and nine C modules per configuration are byte-identical with identical export
sets, including `gimp_module_query` and `gimp_module_register`. The 2,704-entry
install manifest is unchanged. All 1,865/1,867 C compile vectors match; six of
nine upstream `.cc` vectors add only the documented inline-visibility switch.

Eight native layout probes compile the baseline/current Filter headers as C
and C++; size, alignment and all 15 field offsets of the three records agree.
Both foundation runs pass 39 cases. GUI/console `--version` exits succeed;
the worker's expected exit 125 tests only its early invalid-argument guard.
The GUI version path creates an empty isolated XDG data directory, but no files,
GIMP profile, GUI session or listener. No new compiler warning kinds remain.

The initial missing C exports, source-only HTTP vtables and ignored attribute
warnings are preserved as diagnosed intermediate failures. A separate smoke
harness correction records harmless empty-directory creation instead of
claiming it never occurred. These do not replace the final successful checks.

This closes original 04.012, which has no canonical source-duty rows. It does
not establish Windows/macOS visibility, full feature behavior, sanitizer
coverage, or completion of the migration. Earlier source-frozen reports retain
their recorded snapshots; their hashes are not rewritten after this change.
