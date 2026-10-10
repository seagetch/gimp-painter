# Original WBS 06.029: construction failure cleanup

The common BindingStore already owns each construction candidate before publishing
it. These regressions verify its exception and allocation-failure paths; no
production leak or runtime behavior change is claimed.

`acquired-construction-failure` acquires a C++ object, native GObject, copied GLib
string and two-element GArray before throwing either a standard exception or
`bad_alloc`. It covers an empty or populated store and retained or abandoned
native owner. Completed resource members unwind exactly once; an incomplete
Impl's close/destructor does not run. If the owner survives, the previous slot,
generation and owner reference count remain intact, lookup rejects the failed
slot, and registration of that same identity succeeds on retry. If its caller
released the owner inside the constructor, the registration lease releases it
after unwinding without accessing the destroyed store.

The separate GNU-linker allocation harness injects recoverable C++ allocation
failures before store attachment, in Entry/Impl creation and in vector
publication. Its fixed tracking arrays observe matching delete operations; a
narrow g_free wrapper observes only fixture String allocations. The real
allocators still perform every successful allocation and release. This isolated
test instrumentation is not linked into the application or its normal tests.

The existing registration tests cover wrong owner types, duplicate slots and
invalid lifecycle states before candidate construction. Existing constructor
reentry tests cover a completed candidate rejected after its owner closes.

## Source obligations

`source-mapping.json` identifies all three assigned duties and verifies the exact
old Git blobs. The old class template placement-construction path is replaced
by explicit native ownership and typed store entries. Scope guards correspond
to matching RAII release. The selection helper's duty here concerns its owned
matcher strings and pattern arrays during failure; matching semantics and
feature-specific callers retain their separate obligations.

## Reproduction and limits

```sh
python3 tools/test_painter_foundation.py --build-dir /tmp/painter-construction --report /tmp/painter-construction.json
python3 tools/test_painter_foundation.py --sanitize --build-dir /tmp/painter-construction-asan --report /tmp/painter-construction-asan.json
python3 tools/test_store_allocation_failures.py --build-dir /tmp/painter-store-failures --report /tmp/painter-store-failures.json
python3 tools/test_store_allocation_failures.py --sanitize --build-dir /tmp/painter-store-failures-asan --report /tmp/painter-store-failures-asan.json
```

The common native and ASan/UBSan reports each contain 65 passing cases, including
C11/C++14 boundary/header checks and borrow-escape compile rejection. The Meson
target separately passes the same 65 cases. Allocation reports record their
specific injected sites. Explicit release counters and ASan/UBSan are not an
LSan result; leak sanitizer is disabled. These Linux/GNU measurements do not
claim Windows/macOS, full application, fatal GLib OOM recovery or feature GType
initialization acceptance. Historical evidence keeps its original source seals.
