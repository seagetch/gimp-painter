# Original nested-use brush operations

Original C entries `gimp_mypaint_brush_begin_use` / `end_use` delegate to the
same native BrushSlot used by all brush operations. Typed handle methods call
those C entries, not a second implementation. The slot owns a nonserialized
counter: increment happens before the 0→1 begin hook; decrement happens before
the 1→0 end hook. Default hooks are empty, as in the historical class, while
rebuilt native subclasses can override real class hook pointers.

A strong owner spans counter mutation and callback. The callback executes
outside the mutable slot borrow, so nested begin/end, disposal and release of
the caller's last reference are supported. Invalid types and unmatched end
retain GLib precondition checks; owner-thread/active-state admission rejects
closed, foreign-thread and preactivation operations without changing state.
Disposal clears the private counter without synthesizing a final end callback.
An overflow guard prevents unsigned wrap; exhaustive maximum nesting is not
a runtime test claim. Exceptions from rebuilt C++ hooks are contained at the
C boundary; the already-applied transition is not rolled back.

The C test subclass covers nested and independent owners, transition ordering,
underflow/null input, wrong-thread/closed admission, both directions of
reentry, disposal, last-reference release and constructor activation. A real
C++14 caller covers typed-owner copies and throwing hooks. The original
preimplementation observation records missing symbols, not a fake runtime red.

Historical runtime GType names, public object counter field and complete
object/class binary layout remain outside this source-level port. Canonical
entries stay PARTIAL and parent08.018 stays open. No UI change is involved.

Historical implicit caller still outstanding: old `get_new_preview` wrapped
pixel generation in begin/end calls. The native preview vfunc does not yet
route through these hooks; this package covers explicit C/handle operations,
not implicit preview integration. That separate behavior must remain open.

## Verified results

Normal49/49 passes in2.00s and exact restored candidate49/49 in1.96s. The
threshold mutation changes only ++count==1 to ++count==2 and is rejected at
the independent brush's actual event assertion; both source and mutated
binary identities are recorded. This is a negative control, not a claimed
preimplementation runtime failure. Bounded ASan/UBSan/float49/49 passes2.21s.
All runs retain the original60s budget and skip no case. Both native headers
compile in the existing411-probe C11/C++14 closure, with17 route controls.

The isolated build uses an exact committed archive plus only this task's
owned overlay, private source/build/prefix/profile/lock, shared read-only
dependencies, O2 and enabled assertions. Debug symbols are disabled to keep
the build small; sanitizer objects use their disclosed O1 instrumentation.
Xvfb is authenticated1080x1920x24; these core tests claim no UI rendering.

37 sources are instrumented;38 actually linked C++ sources are RTTI-only.
The former whole-application40-candidate RTTI census also lists HTTP navigation
and a standalone filter worker: neither supplies an object to this executable,
so the builder records both as excluded/unlinked rather than falsely claiming
them as linked instrumentation. Every instrumented source remains required.
All aliases in linked private archives are replaced; ordinary files are not
overwritten. Other application/dependency objects are uninstrumented; LSan
is off. No sanitizer diagnostic occurred. Exact seven owned source hashes
match ordinary and sanitizer builds. Setup-only failures are disclosed in
setup-dispositions.json and never counted as expected runtime failures.

The source-only persistence gate still checks1378 records,873 dispositions,
16 routes and46 rejected controls. Only the provider seal/derived hash changed;
field populations/defaults are unchanged. Evidence omits inherited environment
dictionaries; publication remains subject to the parent's approval gate.
