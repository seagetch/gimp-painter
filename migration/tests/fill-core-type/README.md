# Original WBS 08.011: native Fill brush core

The pinned legacy Brush controller derives from GimpBrushCore through the old
C++ registration adapter. It overrides paint and enables changing, transforming
and dynamically transforming brushes. Its method named start initializes the
private renderer; it is not the native GimpPaintCore start virtual method.
The native brush start, pre/post paint, interpolation, paint-buffer allocation,
Undo and finalization are inherited in that implementation.

GimpFillBrush now directly derives from the current GimpBrushCore, owns its
implementation through the common BindingStore and registers with GimpPaintInfo
under the preserved gimp-bucket-fill-brush identifier. Its admitted start calls
the real BrushCore parent start. A one-shot check_start permit protects the
asynchronous transaction before the ordinary native entrypoint overwrites
scratch state. Bare native starts remain explicitly unsupported. The supported
owned adapter and generic coordinate/path/boundary entrypoints all use the same
core and native transaction. No second paint implementation is added here.

The new registration case constructs through the actual PaintInfo, verifies the
exact native parent, instance/class sizes, options association and inherited
undo-desc property, and uses all six public type/class macros. Three conventional
class macros were added to the current public core header. This is not restoration
of an old exported core header: the old core was private to the tool CPP file.
The test checks the overridden start/paint and inherited pre/post/interpolate/
paint-buffer/Undo/finalize slots, then paints real pixels and checks Undo/Redo.

A separate case removes either the brush or dynamics resource and exercises the
real parent-start failure. It checks the propagated error, cleaned scratch and
applicator ownership, no preview freeze, no pending paint, unchanged pixels and
Undo, then restores the resource and completes a real stroke with the same core.
A four-phase lifetime case covers idle, queued, published and committed states.
Repeated disposal rolls back uncommitted work and releases pending paint. Final
unref runs the inherited native finalizer and releases the brush and dynamics;
committed Undo/Redo remains usable after the core is destroyed. The qdata sentinel
observes final destruction independently of weak notifications during dispose.

Both native set-brush and set-dynamics notifications are exercised with cancel,
dispose and release of the caller's last core reference. The owner lease keeps
the start callback stack valid and completed cleanup leaves no frozen preview,
pending work or changed pixels. The first fixture version attempted to disconnect
a signal that GObject disposal had already removed. That test-only error was
corrected with a connected-handler check; it is not counted as a production bug.
Read-only review and current executions found no further type/vfunc wiring fault.

All 40 normal cases pass: four new boundary cases and the existing 36 core,
asynchronous and owned generic stroke cases. Native report seals were collected
after the completed run; source stayed unchanged after the successful build and
throughout the execution, but this invocation had no automatic pre-run seal
comparison. The focused sanitizer run passes 10/10 cases with 32 instrumented sources.
Another 43 units are compiled for RTTI compatibility only; 41 enter link inputs,
which does not imply that every archive member was extracted. The builder
verifies 1,276 normal artifacts and 1,830 source/generated headers unchanged.
LeakSanitizer and unexecuted platform gates are not claimed by these results.

The source-specific core duty in the full tool CPP is accepted only for this
native type/parent contract. Three erroneous broad associations are removed:
the tool-only header, its include and the bounded-search API declarations.
Exact legacy blob and hunk hashes plus independent review substantiate the
correction. All other feature duties are preserved; correcting three TODO rows
adds zero completed work. The 08.012 tool/options registration, section 27
rendering/selection behavior, indexed/high-precision support and cross-platform
acceptance remain separate. Historical test seals are unchanged.
