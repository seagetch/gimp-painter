# Original WBS 07.016: construction failure through the C boundary

Earlier tests measured half-built resource cleanup and C exception containment
separately. The added regression combines them: a separately compiled C11 caller
invokes a C-linkage C++ adapter, which calls BindingStore::emplace with the existing
resource-acquiring AcquiredImpl inside boundary<gboolean>. No production defect or
production behavior change is claimed.

The C caller executes success, standard-exception and bad_alloc paths, each with
and without GError output. It checks that control returns to C, the result/error
matches, the acquired C++ object and native GObject are released once, the two
GArray members are cleared once each, and the outer owner finalizes once. On
failure the unfinished Impl's close/destructor never runs; the success control
completes, closes and destroys exactly once. Thus the counters cannot pass merely
because every constructor is rejected or the caller terminates before returning.

The same 70 cases pass in the standalone native, ASan/UBSan and actual Meson
executables. Their 25 source seals match current bytes. The native C and C++ units
are compiled separately, as recorded by commands and the three-step Meson rebuild.
Existing acquired-construction/reentry cases continue to cover previous slots,
owner loss, pending identity rollback and retry. Existing allocation native and
sanitizer reports each retain five passing cases and ten unchanged source seals,
including actual String/free tracking and store/Entry/Impl/vector failure sites.

`source-mapping.json` verifies the three assigned legacy blobs. Old placement
construction and private lookup map to owned Entry/Impl candidates, owner lease,
pending reservation and boundary translation. ScopeGuard maps to matching RAII
releases; selection-helper strings and pattern arrays map to scoped cleanup.
Selection matching and feature callers remain separate. Only the three 07.016
verification rows close; their 06.029 implementation rows are not substituted.

LeakSanitizer is disabled. These results cover recoverable C++ failures, not fatal
GLib allocator termination, system-library instrumentation, every feature's GType
initialization or full application/platform releases. Earlier reports keep their
original bytes and source seals. The new common test changes three sealed test
files; the previous four-OS snapshot is therefore historical until the automatic
native CI run for this commit is collected and verified.
