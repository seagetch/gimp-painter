# BindingStore qdata registration — original 06.007

The existing production implementation uses the one private quark
`gimp-painter-binding-store-v1`. `ensure` returns the existing store or attaches
one newly constructed store with a native qdata destroy callback. `find` and
`require` read the same key. Production store code is unchanged.

The new `/painter/store/registration` native test verifies the actual qdata
pointer equals the returned store, repeated ensure/require/find return the
same identity, and two owners have different stores under that same key.
Lookup before registration does not attach a store. Registering does not
increase the owner's strong reference count. Repeated ensure in active and
closed states keeps the original store; a closed store is not replaced or
reactivated, and its generation is unchanged by ensure. The second owner's
active state and slot value remain independent.

Both owners also carry unrelated native qdata with independent destroy
counters. Store installation and closure preserve those pointers without
running their destructors. Releasing one owner finalizes only that owner's
store/implementation and unrelated qdata; the other remains live and registered.
The final owner release destroys its remaining data once. This tests native
qdata ownership without requiring a store-to-owner strong cycle.

The rebuilt native foundation and fully rebuilt common-component ASan/UBSan
suites each pass 46 cases; sanitizer stderr is empty. The 11 component C/C++
units are instrumented, system GLib is not. LSan remains disabled for the
verified executor ptrace limitation and vptr is excluded for native no-RTTI
compilation. Explicit destruction counters are not an LSan result.

Work item `legacy-4b7c4aa5ed4415834338` maps pinned old glib-cxx-impl.hpp hunk
`01.002/000045`. Old NewGClass registers per-type instance-private storage,
placement-constructs Impl in instance_init, resolves it through
G_TYPE_INSTANCE_GET_PRIVATE, and explicitly destroys it in instance_finalize.
The new common store replaces this shared attachment mechanism with one
qdata owner and separately typed slots. This registration criterion does not
close each old vfunc, property, slot or feature adapter obligation. Exact old
Git blob, whole-file and addition-hunk digests are verified in the archive.
Only this row's mutable execution evidence becomes DONE.

The existing construction/explicit-installation and owning-thread preconditions
apply. Simultaneous first registration from competing threads and external
replacement of the private reserved key are unsupported misuse, not guarantees
made by this result. Explicit close is still required by native owner adapters;
qdata destruction is the final fallback. The later slot, close/reentry and
feature-caller acceptance items remain separate.
