# Owned Save/export continuation for pending painting

GUI Save and export commands retain their exact image, destination, procedure,
run mode, state-update flags and compression choice while a pending paint
transaction finishes. Save-and-close runs its close action only from successful
completion, never from admission of a queued request. Save on an otherwise
clean image still proceeds if painting is pending.

The Save/Export dialog uses the same operation and performs its native last-file,
save-a-copy, close-after-save and dialog cleanup from completion. While waiting,
its editing controls are disabled and Cancel remains available. A later programmatic
file chooser change cannot redirect the accepted request. Cancel destroys only
the pending save request; it does not discard accepted painting.

Each operation is a C-shaped GObject with one common BindingStore typed slot.
It retains the image/file/procedure and weakly tracks its UI owner and progress
handler. A common Source owns the operation until ready or closed; common
Connections cancel on widget destruction, dialog Cancel or a display's image
change. The callback keeps short owner/request leases through native reentry.
There is no separate global implementation map or private bridge.

Waiting polls the pure `gimp_image_has_pending_paint()` query on the owner main
context, without a nested main loop, paint drain, event cap or forced stroke
cancellation. The image's saving signal seals current input. Fill subscribes to
that signal itself because native BrushTool preserves state by default and the
generic tool manager intentionally skips its COMMIT handling in that mode.
A late BUSY refusal from the native save pipeline keeps the same owned request
pending and retries after painting finishes. Nested COMMIT notifications track
each native automatic HALT separately, so reentrant sealing does not accidentally
roll back the accepted stroke. The direct command leases its original GFile
before the pending query; a query callback replacing the image file cannot
redirect or free the accepted destination.

This is asynchronous waiting, not a new background file writer: after the paint
query clears, the existing native Save/export implementation runs normally.
Non-GUI/PDB/direct XCF callers retain explicit BUSY failure, before destination
replacement; they do not receive a misleading synchronous success or an
unowned deferred request. Native XCF's final pre-commit pending check remains
in place. Temporary remote preparation and export-options references are
released on late refusal as well as ordinary completion.

## Native UI lifetime and regression coverage

Disabling a file dialog can synchronously destroy it from a sensitivity callback.
GTK's response helper walks borrowed buttons, so the native FileDialog helper
retains its complete widget tree, including header-bar action widgets, until the
walk unwinds and stops its own subsequent work after disposal. Re-enabling the
waiting request's Cancel button uses a retained single widget rather than another
borrowed response-widget walk. The dialog captures its exact request resources
before these notifications and checks destruction before admitting the request.

The focused native GTK harness covers thirteen groups: query-time file replacement;
destruction while disabling OK or enabling Cancel; nested COMMIT; Save-and-close;
painting reentry at XCF final close; real dialog OK/Cancel and immutable destination;
real active-tool unreleased input sealing; Save and export-forward/backward state
flags with reopened exact pixels and two Undo records; UI owner loss; display
replacement; last owner reference loss from query; saving-signal reentry; and Save
on an otherwise clean image. Export state routes use the XCF provider as a
controlled writer; this is not an end-to-end PNG export-dialog claim.

Focused builds keep ASan, UBSan, vptr and float-cast-overflow enabled. All production
C++ units explicitly built without RTTI are compiled into private compatibility
objects with RTTI; reports list these separately from sanitizer-instrumented
units and replace every original thin-archive alias. Native display runs and
builds share `/workspace/shared/gimp-painter-build.lock`; the exec and native
surfaces do not share `/tmp`. Builds verify unchanged source hashes and each
native run identifies the exact executable. Later compatibility-only dependency
changes are reported separately rather than attributed to the tested binary.

## Native start admission

GimpPaintCoreClass has an optional `check_start` hook at the end of the class.
The normal default is NULL. It is checked after argument/attachment validation
but before any native stroke-buffer, coordinate, applicator or Undo mutation.
Fill consumes a one-shot adapter permit there and a separate one-shot armed bit
in its start vfunc before calling BrushCore's parent. Generic public starts and
direct start-vfunc calls fail explicitly. Reentrant refused public starts cannot
replace the outer transaction's native state. Generic Stroke Path integration
remains unfinished; the registered core no longer silently succeeds as a no-op.

Focused builders record gimppaintcore.h because changing a base class size is an
ABI dependency. A fresh full normal build must precede private sanitizer relinks.

## Frozen Save continuation checkpoint

The final real-display runs passed all thirteen Save groups in normal and
34-unit ASan/UBSan/vptr builds, and all twenty-six Fill UI groups in normal and
30-unit focused builds. The reports separately identify twenty and twenty-one
RTTI-only compatibility units, respectively; the remaining host/dependencies
are uninstrumented and leak detection is disabled. See `save-continuation-allowlist.json`
and the adjacent native/sanitizer logs for exact source and executable hashes.
An independently edited Tile widget changed after linking; its old linked hash
is retained and its later delta is explicit in both reports. These runs do not
claim coverage of that newer Tile implementation.

These results close owned GUI Save waiting and concrete lifecycle regressions.
They do not close Fill's total phase latency, bounded admission/frontier memory,
pipe sequencing across cloned strokes, old Shift/smoothing equivalence, or
unsupported generic Stroke Path integration.
