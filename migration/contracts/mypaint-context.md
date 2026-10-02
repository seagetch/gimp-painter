# Extended painter brush context selection

A distinct GimpPainterMybrush context property supplies the native type/name/
signal lookup expected by resource selectors and shared editors. Upstream
GimpMybrush remains unchanged. `painter-mybrush` is property/mask22; existing
EXPAND bit21 is reserved and skipped by generic context-property loops. Existing
property/mask values do not shift. Internal signal and type/name tables preserve
index alignment across the reserved entry.

The property participates in inheritance, explicit definition, copy/duplicate,
name tracking, factory remove/thaw, serialization, memory accounting and teardown.
`mypaint-brush`, the exact pinned old painter key, is a read-compatible alias;
serialization writes only `painter-mybrush`. When an unavailable name falls back
to the internal standard brush, its requested name survives the canonical write.
A separately identified internal painter standard resource provides a pre-load or
empty-factory fallback; it follows the application's standard-resource lifetime
convention and does not replace the upstream MyPaint standard.

Selection publishes complete state before releasing replaced resource refs.
A context lease spans finalizers and notifications, and a monotonically increasing
selection revision suppresses stale outer notifications following nested changes.
Remove callbacks similarly release old resources outside the published state and
only choose a fallback if no nested selection superseded them. Closed contexts do
not adopt resources after disposal.

Four native cases pass: property/type/signal mapping and parent inheritance;
old-key read plus missing-name-preserving canonical round trip; a replaced
resource finalizer dropping the caller's last context ref; and nested selection
with exactly one current-resource signal. The same cases pass ASan/UBSan in a
private full-GIMP harness with12 instrumented sources and consistent C++ RTTI.
Remaining GIMP/dependencies are uninstrumented; LeakSanitizer is disabled. Raw
startup/shutdown diagnostics are retained separately from memory-safety results.

This is context/resource plumbing, not tool/editor completion. The forthcoming
options model and GimpObject session adapter use the established typed handles
and single BindingStore implementation slot. Real tool event delivery, GUI editor
behavior and hardware/ICC/precision coverage remain separate gates.
