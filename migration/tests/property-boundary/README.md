# Recoverable native property callbacks

Acceptance for the existing `05.013/legacy-exit-removal` obligation. This closes
the property callback subset; it does not close the all-vfunc/signal task
`38.004/all-vfunc-exception-containment` or feature/platform acceptance.

The pinned old `GClassWrapper::set_property` / `get_property` at
`afa43fae3e920210146abed514f136fd49f671b5` copied allocating dispatch state and
called `exit(1)` from their catch handlers. The current inventory has 31 native
types, 9 property owners, 7 assigned setters and 8 getters. The archive records
the old 10 property-declaring types / 15 declarations and their present routes.
Some obsolete wrappers have no current property adapter; their feature/removal
WBS obligations remain open. The original header's 20 broader ledger obligations
are unchanged; none is reassigned to make this task appear complete.

## Failure contract and corrections

Fallible native property callbacks use `property_boundary`. It contains C++
exceptions, reports a GLib warning naming the type/property/operation, and holds
the object and GParamSpec alive through reentrant diagnostics. There is no
process-exit recovery path. Warnings obey the caller's normal GLib fatal-log
configuration; a debugger/test setting that makes warnings fatal is not disabled.

Ordinary `g_object_get_property` initializes or resets the typed output before
dispatch. A failed allocating read therefore leaves the native zero/FALSE/NULL
value, which can safely be unset. This is not a promise to reproduce the
GParamSpec's advertised default or to support an arbitrary uninitialized direct
vfunc call. Failed setters retain coherent prior state and GLib's existing
automatic notification for the requested property; this notification invites a
fresh read and does not assert that the value changed. Successful edits retain
their explicit settings notifications and generation/revision cancellation.

Two defects were reproduced on real application-linked native code:

* MyPaint Options committed a draft and then copied a whole Resource while
  preparing resource synchronization. A failure in that copy made the checked
  API return FALSE with the new draft already visible, and skipped settings,
  JSON and state notifications. Synchronization now reads the required switches
  and copies only the selected name into GLib-owned storage while holding the
  read lease. Resource's borrowed `peek_text` preserves missing/null/empty/type
  semantics and must not outlive mutation of its Resource. There is no C++
  allocation between commit and the first observer in the measured paths.
  GLib's fatal allocation exhaustion is outside recoverable C++ exceptions.
* GuideUndo treated a failed non-null guide duplication as a valid NULL snapshot.
  Four of eight allocation positions created an entry that later cleared a valid
  guide. Failed construction is now terminal and implements native GInitable
  admission. `gimp_image_undo_push` checks only subclasses implementing this
  interface before deleting redo history or inserting an entry. Ordinary native
  Undo classes retain their existing path. Legitimate NULL-before snapshots still
  represent adding a guide. Rejecting an Undo entry does not roll back a tool edit
  that was already applied, native dirty notifications before construction, or
  an Undo group's earlier creation/redo clearing.

FilterLayer's owning property getter now reports its contained error at the
property boundary rather than silently discarding the inner C API error. Its
stored definition, weak argument model, partial-copy cleanup and typed NULL
failure behavior are retained. CloneLayer and mask-components' scalar handlers
do not enter throwing C++ state; they are covered as native controls.

## Evidence

`report.json` seals the current sources, initializer inventory and archive.
`tools/check_painter_property_boundaries.py` checks these seals, original ledger
ownership, the bounded runtime results, and task-state consistency.

* Options: 14 cases on both baseline and corrected code, normally and with
  ASan/UBSan. Tests exercise checked/JSON/numeric commit paths, malformed/null
  JSON, pre-commit failures, allocating JSON/text getters with fresh and prefilled
  GValues, exact class/spec metadata, inherited properties, recovery, observer
  edits/closes, and exactly-once finalization of all 14 explicitly owned Options
* GuideUndo: baseline control plus all 8 real allocation positions. Corrected
  code runs those positions plus legitimate NULL and two grouped failure cases,
  normally and with ASan/UBSan; failed admission keeps the target state/history,
  and subsequent valid Undo/Redo recovers. The native pre-existing empty group
  remains a group; failed admission adds no child
* Eight native adapter controls normally and with ASan/UBSan cover Fill, Smudge,
  Guide, Session, Clone, mask-components, Filter owning/expired argument reads,
  and a diagnostic callback dropping the caller's last reference
* Rebuilt ordinary suites: Options 10, perspective 9, Resource 8 and Session 8

The Options and GuideUndo translation units are instrumented in their focused
sanitizer variants. Native dependency archives are not fully instrumented.
LeakSanitizer is disabled because this executor's ptrace environment prevents its
supported run; UBSan vptr is excluded for the native no-RTTI build. Explicit
owner counters and weak notifications are identified as such, not LSan results.
The existing synthetic-profile writable-folder shutdown warning is retained.
No new GUI, platform, tablet or all-filter pass is claimed.

Adding GInitable produces 39 current initializers (31 class and 8 interface);
the 15 property handlers are unchanged. Earlier class-initialization, vfunc,
allocator and foundation evidence preserves its historical source hashes.
Its old snapshot checker can reject these legitimately changed files; this
checkpoint does not rewrite those old digests or call them a fresh current pass.
