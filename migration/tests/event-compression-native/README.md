# 23.014: native event ordering and owned compression lifecycle

Original requirement is retained verbatim in `scope.json`. The task remains
**DOING**, not complete. The separate `event-compression-foundation` proof covers
the old owned-event queue; this package is current native GTK/GDK evidence.

## Verified bounded change

Exact strokes now retain the actual native window and its original compression
setting. Release, tool/image cancellation and disposal restore only that live
window; a replacement is never altered. Repeated restoration is harmless.
Prior FALSE is deliberately preserved instead of the old unconditional TRUE.
A generation guard prevents a failed outer callback from ending a newer nested
stroke's lease, including same-window reentry and uint64 wrap.

The five provider files are captured with exact55bf baseline and candidate
source. Internal shell instance fields are appended; no class/public library
ABI is changed. The defining type translation unit and every changed consumer
are compiled with the candidate header. No subclass or independent instance-size
consumer was found. C11/C++14 probes require unmangled new C linkage without an
outer probe linkage adapter. The narrow existing ignored-qualifier compiler
exception is documented in the header receipt. Nothing is persisted; historical
persistence field inventories are unchanged.

## Results and limits

- **14 normal + 14 focused ASan/UBSan cases pass.** The fixture and three changed
  C providers are instrumented, not the entire application or shared GTK/GDK.
  LSan is disabled because this execution host does not support it under ptrace.
- Both exact55bf tool-switch and image-switch probes fail before the fix, using
  the same explicit oncanvas-enabled portrait fixture.
- Removing the generation guard fails the nested-new-stroke ownership assertion.
- Actual GDK queued motion/release/key/crossing and synthetic device/state changes
  traverse the GDK dispatch callback and GTK native canvas path. A recording tool
  observes motion/release; it does not paint pixels. Initial signal-hook counting
  wrongly counted GTK key propagation twice; that diagnostic is retained, not
  interpreted as duplicate queue ownership.
- Every final image setup explicitly enables/asserts oncanvas and asserts a
  1080×1920 Xvfb root. `oncanvas-event-order-portrait.png` is the actual rendered
  app after the queued-terminal case, not a drawing/pen accuracy demonstration.
  Missing icon/theme and session-bus warnings remain visible in receipts.
- **One known failing case is explicitly excluded**, never counted passed:
  `/event-order/close-active-image`. A live empty reused shell swallows the
  subsequent physical release and keeps `grab_seat`. Its failure is retained.
  Safe grab ownership/teardown is a separate unresolved obligation: blindly
  ungrabbing a shared seat could cancel another shell's newer grab.

Native event pointers are borrowed from GTK. The app must neither free them nor
retain them beyond dispatch; this test frees only the input copies it creates
before GDK copies/dispatches them. No ineffective interposition count is used to
claim shared-library internal event frees. Real tablet/pen history sampling,
server-side hardware motion compression, Windows behavior and full original
criterion/dependencies are **not** established here.

The registered native test explicitly skips the known case. To reproduce that
open diagnostic, run the executable with `-p /event-order/close-active-image`
without the registered skip argument. Test fixture cleanup sends outstanding
releases only to live reused/recreated windows; actual disposed-window testing
uses no invented post-destruction release. Earlier fixture cleanup/remapping
failures are retained separately and are not product failure or passing evidence.

`validate.py` verifies recorded receipts, exact source capture/current provider
identity, and the explicit exclusion. `test_validator.py` exercises rejecting
controls. Revalidation/successor evidence is required after provider changes;
do not merely reseal older receipts.
