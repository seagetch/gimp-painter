# Original WBS 07.017: parent dispose reentry

The existing native C base/child GObject fixture satisfies this original gate.
The child dispose calls the C-linkage close adapter before chaining to the base;
the base closes idempotently, invokes its callback and then chains to GObject.
The callback asserts the store is closed, all existing slots have closed in
child-before-base order, and neither Impl destruction nor native finalize has
started. It reads both base and child values through the C-linkage getter and
scoped const store access, then recursively invokes native g_object_run_dispose.

The reentry test observes two parent callbacks for the outer/nested disposal,
three after another explicit disposal, and four after final unref. Generation
advances once, both close counts stay one, and both Impls survive until native
finalization. Each Impl is destroyed once. The native finalize trace is child
entry, base entry, base exit, child exit. The shutdown matrix also covers base-only
and derived owners, with and without explicit close before repeated disposal.
The finalizing-close/read test separately checks idempotent fallback close and
read rejection while the native owner can no longer be retained.

The pinned legacy helper used private Impl lookup and explicitly destroyed it
before its parent's finalize. Its one assigned 07.017 verification obligation
maps to the common store's split close/destruction lifetime and this actual native
parent-chain test. There is no claim that the old helper itself had a dispose
hook or that every feature's callback is covered. No assignments are removed.

Native, ASan/UBSan and actual Meson reports already run for the immediately
preceding task each pass 70 cases and retain 25 source seals identical to current
bytes. Those reports are reused without pretending this reconciliation reran
binaries. The current four-native-OS index provides the same three case results
and 30 matching source seals, including explicit Windows checkout conversion.
No production or test implementation change is necessary. Historical report
bytes remain unchanged; LSan, full application/platform releases and other
feature-specific shutdown gates are not claimed here.
