# Native property transfer — original 06.015

The common bridge uses native GObject registration and exact C property vfuncs,
with implementation state in the shared BindingStore. The existing
property_boundary contains fallible C++ calls and retains the owner and spec.
This acceptance completes the common transfer contract; no second property
registry or bridge is introduced, and no production behavior changes here.

The new fixture is registered and dispatched in a real C translation unit. Its
C-linkage C++ callbacks use initialize during construction, with for active
writes, and read for getters. A mixed native schema verifies nine representative
families: int, boolean, double, string, object, boxed GBytes, enum, flags and
uint64. These are not claimed to be the exact nine legacy helper overloads.
Installed types, owner, IDs, flags, numeric ranges and advertised defaults are
checked. Actual default construction and an explicit construct-property value
are checked separately. Ordinary READWRITE metadata does not initialize C++
state automatically: this fixture explicitly copies its native defaults.

Public get/set retains exact double and large uint64 values, enum/flags, string
NULL versus empty, copied string input, and owning object/boxed output. Reads
into prefilled string storage remain safe. Object weak notification proves the
returned GValue owns its reference after property replacement; GBytes remains
readable after both the caller and property release it. Native int-to-double
conversion is exercised in both setter and getter directions. The old
GClassWrapper getter called g_value_transform itself; the new native dispatcher
preserves the public conversion route without a handwritten conversion table.

Automatic notify reports even repeated same-value sets. EXPLICIT_NOTIFY emits
once on a committed change and suppresses unchanged values. Freeze/thaw
coalesces repeated requests per property. Observers see committed typed values,
may perform a nested edit, and may dispose/release the caller's last reference.
Public g_object_set queues notifications until dispatch unwinds, so that final
case establishes native notification lifetime. Original06.012 separately
verifies in-flight BindingStore leases. Existing hierarchy cases in the same
suite retain base/child property routing and construction order.

The rebuilt native foundation and rebuilt ASan/UBSan common-component suites
each pass 53 cases. All 11 common C/C++ translation units are instrumented;
system libraries are not. LeakSanitizer remains unavailable under the executor's
verified ptrace limitation; vptr is excluded for the native no-RTTI build.
The first draft's string assertion incorrectly kept a borrowed pointer from a
temporary Value across a test macro statement. The final test keeps an owning
Value local. This was a fixture error, not a reproduced production defect.

Production property failure evidence from
../property-boundary/report.json is reused with all 36 source digests and the
archive digest unchanged. Its historical Options tests verify 55 properties,
including exact native metadata/defaults and notifications. Its native controls
cover retained construct-only Session objects, scalar adapters and owning
Filter argument reads. Those results also cover contained setter/getter failures
and diagnostic last-reference release. They are source-equal historical results,
not freshly rerun production tests. Failed allocating native getters leave the
initialized zero/NULL value, not necessarily the GParamSpec default. Failed
setters retain state while GObject automatic notification still invites a read.

The archived verification script checks the unchanged production source/archive
seals and all 20 original header obligation identities. The old property-boundary
checker also compares historical mutable completion fields, so it rejects later
legitimate task completions; its original report and digests are not rewritten.
Current runtime results and current source hashes are in this task's report.

Only legacy-d84cf3595b84fea6ff82 becomes DONE: pinned glib-cxx-impl.hpp
hunk 01.002/000045, including GClassWrapper's property dispatch and parent fallback.
The old blob, whole-file and addition-hunk digests are verified. Future
feature-specific adapters, all-vfunc containment, platform execution and other
remaining migration acceptance are not closed by this common property gate.
