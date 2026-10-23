# 07.008/editor-prompt-ownership

Bounded ownership correction for Save As, Rename and Delete prompts. EditorImpl owns shared prompt records with owned response/destroy Connections. Callback leases retain the record and native resource across close/context replacement; response is disconnected before the real factory operation, and retirement removes the record and destroys the dialog. Complete new C callback bodies use the exception boundary.

Nine stale-response combinations assert actual handler removal and unchanged resources/models; ten operation-reentry scenarios cover direct and prompt operations, nested responses and Rename notify reentry; eighteen repeated Cancel/emitter-first paths check exact resource references and dialog finalization. The full registered editor suite passed all25 cases in both normal and ASan/UBSan builds. No case was skipped, timeout extended, warning suppressed, or sanitizer instrumentation reduced.

The original expected-red and120-second timeout remain in native-evidence.tar.xz. The final test-only amendment removes redundant widget construction for fixture setup/cleanup while preserving snapshot/tags, all tested operations/assertions and repetitions. Final normal31.75s; sanitizer117.06s within the original120s budget, with little headroom. report.json binds exact native source hashes and evidence. LeakSanitizer is unavailable on this host; no leak-coverage claim.

Portrait GUI screenshots show real Save As/open/cancel/reopen. That first GUI attempt is qualified: application child exit0, capture helper245 after teardown, interpreter-plugin sanitizer-order warnings and private profile search-path warning. It is not clean full-plugin startup or installed-package acceptance.

The parent07.008/untracked-signals remains DOING. Name/button control routes, other source-map entries, PageRemind and legacy UI parity remain open. Historical171/171 and15-registration acceptance belongs to the earlier exact source, not this patch.
