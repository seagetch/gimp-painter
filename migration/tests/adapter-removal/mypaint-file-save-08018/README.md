# Original named brush save entry — bounded 08.018 child

Canonical row44 requires the named C operation to reach the new implementation
without Interface::cast. The restored original `gboolean (GimpData *, GError **)`
entry and typed `MypaintBrushRef::save()` delegate to one native `gimp_data_save`
transaction and its existing BrushSlot serializer. The caller's resource is
borrowed and pinned for the operation. No-file returns FALSE without error as
before; a prepopulated GError remains untouched. Closed/wrong-thread resources
are rejected before I/O and nonwritable resources fail explicitly.

The exact historical source/header blobs are in `historical-contract.json`.
The old writer ignored JSON/sidecar write failures and always returned TRUE,
except with no filename. We do not reintroduce that unsafe success report.
The native transaction checks replacement/write/close errors, retains dirty
state on failure and updates dirty/mtime on success. It embeds the preview
instead of writing a separate sidecar. Therefore row44 remains **PARTIAL** and
parent08.018 remains open. This is not historical subclass/layout ABI parity.

The real C11 cases cover saved current name and preview roundtrip; independent
loaded resource; no extra sidecar; borrowed refcount and finalization; no-file
failure; readonly rejection preserving existing bytes; missing-parent I/O
failure retaining dirty state; preexisting-error, invalid type, wrong-thread
and closed-state rejection without overwriting existing bytes. A C++14 case
checks the exact C signature, typed owner save/load and error translation.

A source-spelling absence observation is recorded, not a runtime expected-red.
The follow-on mutable-live-preview gap below is explicitly not included in the
current passing roundtrip scenario. No UI changed and no screenshot is claimed.

## Remaining semantics after the entry mechanics

| Canonical row | Remaining difference / evidence needed |
|---|---|
| 42 load | Native strict UTF-8/version/numeric/size/error handling replaces permissive old fallback; naming uses parent metadata or suffix-aware basename. File/error migration policy and format-corpus comparisons are still required for unqualified legacy acceptance. |
| 42 load | Embedded preview takes precedence; `_prev.png` fallback is native policy rather than the original unconditional Cairo sidecar load. Corrupt/missing/combined image cases need explicit compatibility disposition. |
| 44 save | Native checked I/O and clean/mtime transaction intentionally differ from old ignored failures and unchanged dirty flag. Do not silently label the old behavior restored. |
| 44 save | Embedded preview rather than a separately written `_prev.png` changes interchange with old external consumers; compatibility disposition or explicit export support remains. |
| 44 save | **Potential live-pixel gap, source review only:** old save reads the live icon surface, whereas native serializer uses preview bytes cached at `set_icon`. Mutating pixels through `ref_icon` afterward may save stale pixels. Requires a genuine failing runtime test and a shared-serializer fix, not a second serializer in this entry. |
| Both | Native resource/type/layout and unknown-setting preservation differ; these bounded entries do not establish the old subclass ABI or all-format semantic parity. |

The broader 76-row inventory and remaining families are documented in
`../../public-entry-routing/README.md`; this task does not remove those duties.

## Final verification

Ordinary full suite **38/38 PASS** in 1.72 seconds; bounded ASan/UBSan plus
float-cast-overflow **38/38 PASS** in 2.15 seconds. Both retain the original
60-second budget, with no skipped/TODO cases. All 34 earlier cases remain
registered. The authenticated Xvfb runner verifies 1080×1920×24 and rejects
empty authority. No UI mode or screenshot result is claimed for this core test.

Real C11/C++14 callers, exactly one unmangled save symbol, six exact owned
source/build-copy hashes, 411 compiled header probes (195 C11, 216 C++14) and
17 route controls pass. Sanitizers instrument 37 parser/provider/native data
transaction/test/common-store registration units; 40 additional C++ units
are RTTI-compatibility-only. Other app/dependency objects are uninstrumented,
LSan is off, no suppressions or increased timeout were used. Exact scope and
source/binary identities are retained. No sanitizer diagnostic was found.

`validate.py` verifies exact case counts, original budgets, owned-source
identity in both runs, C language/link proof, canonical PARTIAL scope and
complete recursive artifact hashes. Inherited environment dictionaries are
excluded; static credential-pattern scan is clean. This does not authorize
publication, which remains under the parent's approval gate.
