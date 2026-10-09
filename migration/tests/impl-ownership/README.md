# BindingStore implementation ownership — original 06.010

Each registered Entry owns its Impl through std::unique_ptr. The store owns
entries, performs close, then destroys them on native owner's qdata teardown.
Entry's destructor repeats only its idempotent close wrapper before releasing
the Impl. Production implementation is unchanged.

`/painter/store/implementation-ownership` keeps separate counters for two
registered slots. It checks three paths: teardown while constructing, active
teardown fallback, and explicit close twice. Neither close nor release of a
non-final native reference destroys either Impl. A retained native owner keeps
both alive; its final release closes and destroys each exactly once. The common
suite also includes failed construction, registration rejection, constructor
reentry and owner-loss cleanup. This proves ownership and exact destructor
counts; subsequent close/callback ordering and adapter requirements remain
separate. Explicit close is still required in native owner adapters: testing the
qdata fallback does not replace that obligation.

The rebuilt native foundation and rebuilt common-component ASan/UBSan suites
each pass 49 cases; sanitizer stderr is empty. Their 11 component C/C++ units
are instrumented, system GLib is not. LSan remains unavailable under the verified
ptrace limitation and vptr is excluded for native no-RTTI compilation. Counters
provide explicit destruction evidence, not an LSan result.

Two source obligations are completed:

- glib-cxx-impl.hpp, legacy-bb33363ce7f20440f007: NewGClass placement-constructs
  Impl in native instance-private storage and explicitly calls its destructor
  in instance_finalize. The shared store replaces that lifetime attachment
- scopeguard.hpp, legacy-6ea0a43254bd0aaf640c: CXXPointer uses ScopedPointer and
  destroy_instance/delete for a C++ allocation. Entry's unique_ptr owns the
  corresponding C++ implementation and invokes its destructor once. This does
  not claim the whole old guard/nullable helper family or every caller migrated

A separate, bounded routing correction removes two mistaken TODO assignments
from selectcase-utils.hpp: 06.010 (store-owned Impl) and 06.022 (mutex guard).
The exact 123-line added file contains neither mechanism. It implements
first-match dispatch, native GType comparisons, and duplicated GLib string and
array inputs. Its existing 06.021, 06.029, 07.013 and 07.016 obligations keep
their identities and TODO states. The helper remains KEEP_CONTRACT; no runtime
feature or source is deleted. The routing correction adds zero DONE entries.
Only the two genuine ownership obligations above become DONE through testing.

`reproduce-selection-routing.py` reproduces this one-file correction from the
public parent ea5dc194 and the archived, byte-verified source/blob/addition hunk.
It updates the same existing tables and summaries; there is no overlay format.
All other rows, source identities, acceptance text and previous execution
states are preserved. `--write` refuses to overwrite subsequent execution work.
The default check permits only this task's two independently recorded completion
rows to differ in mutable evidence fields. At this checkpoint 22,942 work rows
remain: 517 DONE and 22,425 TODO. The two deleted rows were TODO, not completed.
All 2,489 hunks, 947 paths and 402 asset assignments remain covered.

This is bounded source-ledger reproduction. The complete pinned legacy/base Git
trees are absent in this checkout, so a fresh full-tree generator pass is not
claimed. Existing source snapshots and historical seals remain unchanged.
Exact source files, hashes, test commands/output and the correction report are
included in the evidence archive.
