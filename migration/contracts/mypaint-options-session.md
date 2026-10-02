# Extended painter options and native session adapter

## Ownership and editable data

`GimpPainterMybrushOptions` derives from `GimpPaintOptions` and keeps its complete
C++ implementation in one typed `BindingStore` slot. `PainterOptionsRef` is the
named, owned handle. It exposes every generated setting as a native property:
45 numeric values, 5 switches and 2 optional text references. The authoritative
`painter-settings` JSON also preserves all mappings and unsupported/raw data.
Boolean defaults match the actual legacy resource reader, rather than the
inconsistent old display metadata. Curves accept zero or two through eight
finite, nondecreasing points (equal x is permitted by the old evaluator); an
invalid editor change is refused without changing the retained resource.

Draft changes do not mutate the selected resource. Memory commit applies the
whole resource, detects external changes against its baseline and preserves a
conflicting draft. It is explicitly separate from filesystem save. Config copy,
duplicate and serialization retain the full draft, including unknown fields and
curves; no ordinary-property ordering can overwrite the final JSON model.

Selecting another brush records an edited draft in an unbounded per-options
history. Resource selection publishes the complete replacement before dropping
old references, so a finalizer can safely select a newer resource. Notifications
are revision-gated. External edits update a clean draft or flag a dirty draft's
conflict. Closing during resource commit and last-reference release during
notifications are supported. Closed options refuse executable snapshots.

Shape and paper references are bidirectional, following the old options callbacks:
when both use/specified switches are enabled, an empty name adopts the current
context resource name; an available named resource selects it; an unavailable
name is preserved with the existing context fallback. Choosing a context brush
or paper updates the corresponding name only while both switches are enabled.

## Native session

`GimpPainterSession` is a distinct `GimpObject`, with one typed slot and the named
`PainterSessionRef` handle. It wraps the already validated `MyPaint::PaintCore`,
which owns the real native paint transaction. It does not create a second outer
`GimpPaintTool` transaction or delegate custom settings to upstream libmypaint.
The adapter holds options and the controller across synchronous callbacks.

Settings and relevant context changes split the current transaction. Changes
that arrive inside motion are deferred until the current sample unwinds. Invalid
or unsupported settings finish the previous valid stroke, retain an explicit
error and refuse later painting until the draft is fixed. They never keep using
a stale brush silently. Reentrant operations are rejected during transitions;
closing cancels safely, including during native preview freeze. Controller
ownership survives a callback closing or dropping the last adapter reference.

## Verification and limits

Eight native options cases cover generated properties, draft/curve/commit and
conflict behavior, history plus config copy/duplicate/round trip, notification
last-reference release, closed/invalid-curve access, bidirectional shape/paper,
old-resource finalizer selection reentry and close during memory commit.

Six native adapter cases cover exact pixels against the existing controller for
incremental and nonincremental strokes; real Undo splitting and unsupported
settings; close during native start; options changing during motion; closed
options canceling unfinished pixels; and last-reference notification teardown.
These are synthetic lifecycle stimuli in the real GIMP application. The direct
adapter/controller pixel test is not an independent old-session oracle; the
separately sealed old oracle remains the 32 warmed scenarios / 96 snapshots /
129 records described in `mypaint-full-session-comparison.md`.

The focused sanitizer runner instruments 26 source files (including the selected
native test), with consistent C++ RTTI and private thin archives. Original build
objects are never replaced. ASan, UBSan and float-cast-overflow are enabled;
LeakSanitizer is disabled and all nonlisted dependencies remain uninstrumented.
Run reports record the exact source scope and results. Raw test stderr preserves
the known test-profile writable-path/search-path shutdown diagnostic. Retained
old evaluator code can print its existing negative-substep time warning even
when supplied positive input intervals; this checkpoint does not change that
legacy arithmetic.

This checkpoint supplies the native model and session adapter. Tool registration,
GUI editor/preview/save workflow, shared/global history equivalence across separate
options instances and ordinary-context editor routing are subsequent gates.
The existing unprofiled nonlinear RGBA-u8 oracle does not establish other
precision modes, arbitrary ICC, brush pipes, all 177 rendered scenes or real
hardware/platform behavior.
