# Popup renderer original operations 6–8

Public base: 6e99e97ccc1cfc1b4fdbdf50efc4f28ea78d3f97, tree
58d0d78495816f47d4f95ca46120c5ed23ef017d. Legacy reference:
afa43fae3e920210146abed514f136fd49f671b5.

The original three C signatures remain unchanged. The C factory still returns
a floating GtkCellRenderer. The named create operation sinks it; retain adds a
reference to an existing renderer, including one already owned by a column.
Lazy TypeTraits uses the existing sole C registration and no global singleton.

The public clicked C entry now requires an existing active binding on its owner
thread before acquiring renderer/tree leases, then checks the existing typed
PopupSlot after those potentially reentrant retains. Emission occurs outside
the Impl borrow. No store creation, revival, alternate implementation, or new
signal is introduced. The active property, unbound trees, invalid row paths,
and optional area do not become additional admission gates. Native provider
validation still decides whether a popup can be opened. Once admitted, the
current RUN_FIRST class closure, normal observers, after observers, nested
emissions, and subclass no-chain/chained behavior retain normal GLib semantics.
An observer closing the binding does not truncate an already-admitted emission.
Direct g_signal_emit calls bypass this public-entry admission. This is an
owner-thread API boundary, not general GTK thread safety or arbitrary-pointer
validation.

Current native callers are gimplayertreeview.c construction/column insertion,
gimpcontainertreeview.c row-event dispatch, and the renderer activate vfunc.
The current provider resolves the clicked tree row's GimpViewRenderer viewable
and retains the target layer/image. It does not substitute current selection.
Existing event-generation and stable-row safeguards are unchanged.

Compatibility is deliberately bounded. The historical C++ abstract
CellRendererPopupInterface::clicked virtual dispatch is not restored. Legacy
parent-clicked used RUN_LAST without a class closure, with its provider attached
as a normal listener during construction. Current code uses RUN_FIRST with a
real native class closure. The old C class clicked field was not connected to
that signal. Historical instance/class parent layout also differs. Row 6 stays
PARTIAL; rows 7–8 record native callable registration/construction and ownership
evidence, without claiming historical Interface, subclass/layout ABI, or render
parity. The separate type/handle catalog, all other 73 C entries, 31 hooks,
08.018 completion status, and migration counts remain unchanged.

Baseline controls captured before the production edit show both closed-store
and wrong-thread public C calls reaching a counting-only subclass closure and
external observer once. Each expected-zero control aborts with process -6,
Meson 1, and clean authenticated X server exit 0. The wrong-thread callback
bodies are atomic-only and never call GTK. Test fixture compilation initially
needed the ordinary widgets-types prerequisite; the initial unknown-target
Meson lookup required normal Ninja regeneration. These failures are retained.

No ordinary UI was added or redesigned. The change is internal admission and
ownership; integrated native layer-target tests provide the meaningful workflow
evidence without an artificial static screenshot.

## Final acceptance

57 native and 57 ASan+UBSan cases passed: all 26 existing plus 9 new actual-C11
layer-tree cases, and 22 focused C++14 handle/admission cases. Every selected
process, aggregate command and authenticated X server exits 0 without skip,
timeout or sanitizer diagnostics. Both original admission red controls fail
before the fix and pass in the complete candidate.

375 reviewed C11/C++14 header probes, both header gates, four positive and three
negative named-handle compile controls, 48 source-policy cases, and the
90-contract/76-route gate pass. Strict evidence verification and 12 corrupted-
report rejection controls pass. Independent final source review has no blocker.
The native application is incrementally relinked with one copy of each original
C provider, also verified in both focused native and ASan test executables.

Reproduce with the established native and address,undefined builds:

    meson compile -C BUILD -j2 painter-layer-tree painter-popup-handles
    meson test -C BUILD --no-rebuild --num-processes 1 --print-errorlogs painter-layer-tree painter-popup-handles

See report.json for exact source/binary/evidence hashes, baseline failures and
scope limits. No unrelated MyPaint suite was repeated.
