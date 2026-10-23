# Current saved-field checkpoint (12.015/field-owner-checkpoint)

This execution closes the missing focused-sanitizer verification loop of the
historical field checkpoint. It does not rewrite that checkpoint's receipts or
claim every property-order permutation, recursive graph, platform or native
operation is covered. The original 185-row coverage ledger remains unchanged,
including each row's remaining scope.

The full registered suites are used, including the roundtrip suite's `-m slow`
large-record case and the open suite's existing owner-context fixture wrapper.
Budgets remain 180 seconds for open/roundtrip and 60 for upstream/provenance.
Normal execution uses authenticated 1080×1920×24 Xvfb. These are file/model tests,
not new UI screenshots. ASan/UBSan/float-cast instrumentation is explicitly
bounded by the source list; additional RTTI-only units are identified. GLib,
GEGL, other native code and external dependencies are not claimed instrumented;
LSan is disabled.

Each field ledger test reference must occur in an actual successful TAP case.
Source identities, exact archived inputs, executable hashes and receipt hashes
are checked. Selected normal results exclude inherited environment dictionaries.
The compile database contains compilation commands, not process environments.

The older document's inline-only 256 MiB refusal is historical, not the current
ordinary-save behavior: the separate completed multipart-storage task implements
chunk transport. Its existing tests run here unchanged; that does not broaden
this task into all malformed-file or cross-platform acceptance.
