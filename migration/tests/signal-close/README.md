# Original WBS 07.007: receiver lifetime during signal close

The previous acceptance cited direct store calls. Those tests verified lease
return and unwind behavior, but did not isolate a receiver's lifetime inside an
actual native signal. The property-notify reentry test uses one object as both
emitter and receiver, and its Impl read ends before disposal. The new test closes
this evidence gap without changing production code.

`/painter/signal/close-receiver-lifetime` uses separate live GObjects for emitter
and receiver. The receiver's Impl owns the Connection; its exact notify-signature
trampoline dispatches through `boundary_void` and `BindingStore::with`. Inside
that invocation, the test closes the store, disconnects the Connection and drops
the last external receiver reference. The refcount must fall from two to one,
showing only the receiver lease remains; the signal emitter cannot keep it alive.

A scope-exit check reads Impl value 41 and requires zero Impl destruction and
zero receiver finalization while returning normally or unwinding an injected
exception. After the protected call returns, both destruction counters must be
one. The exception becomes GError inside the C-compatible trampoline. A second
native emission must not invoke the disconnected observer. Stack callback data
remains alive through both emissions; the test does not assume closure-data
finalization timing during an in-flight emission.

`native.json`, `sanitizers.json` and `meson.json` each record 67 passing common
cases with exact current source seals. C fixtures remain C11, adapters C++14.
ASan/UBSan cover the component; LSan, system GLib, whole-application and platform
coverage are not claimed. The initial test-only helper-name compile error was
corrected before these successful measurements.

Duty `legacy-5cad10c6f8c018a73309` maps old app/base/delegators.hpp hunk
`01.002/000042`, exact blob `f615f41b4fdebbebc865e17888eadc38a0c45e58`.
The full source is already preserved in gtk-binding/caller-census.tar.gz member
`source/app/base/delegators.hpp`. Lines 32–35 and 46–48 dispatch the delegator;
65–74 store/invoke a raw receiver; 128–142 disconnect; 159–177 register a native
closure and cleanup. Modern Connection retains the native connection lifecycle,
while the common store retains the receiver for each Impl invocation. The old
raw receiver pointer is not claimed to provide the new lease guarantee.

`validation.json` records the exact mapping, one changed source duty and current
acceptance. Historical report seals are preserved and are not promoted to current
passing evidence when a test source changes.
