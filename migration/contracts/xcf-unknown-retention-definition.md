# Unknown payload retention rules (10.014)

This is the **retention policy definition**, not universal damaged-file recovery
or a fresh runtime roundtrip claim. It consolidates the v1, multipart, adversarial
recovery and byte-decoder contracts. The core rule is: readable but uninterpreted
data must not be replaced by guessed defaults or silently discarded to permit an
edit/save. Preserve its owned original representation, or explicitly refuse the
operation before destination replacement.

| Data class | Retention and interpretation rule |
| --- | --- |
| Unknown legacy/standard property records | Keep framed raw records in owner order, including repeats. `opaque-record-sequences` archives exact ordered bytes. Never reinterpret a numeric collision as a modern property without dialect selection. |
| Imported original header/property/extension | Keep immutable `original-header`, `original-properties`, `original-extension` and owner provenance separately from current edited semantics. Do not recursively archive the newly generated archive as a new original. |
| Unknown key in an otherwise valid unique-key v1 dictionary | Carry its GVariant value forward when updating known fields. Dictionary key order/padding may be canonically reserialized; this is not a promise that the whole edited capsule remains byte-identical. Byte-array payload bytes remain unchanged. |
| Duplicate key, including unknown key | Retain original capsule; refuse ambiguous rewriting rather than choosing a conflicting first/last interpretation. |
| Unknown magic/version, missing/wrong/unknown kind, invalid semantic envelope | Keep the capsule inert on its ordinary proxy. Do not instantiate executable custom semantics from it. Editing into incompatible custom semantics refuses before replacement. |
| Unknown semantic capsule requiring extra provenance | Preserve the semantic namespace untouched; use separate `gimp-painter-origin`. Refuse overwriting an unknown origin-archive version. |
| Current unreadable Filter argument model | Core definition owns opaque immutable payload separately from raw legacy definition and converted arguments. Missing, wrong-typed, null and empty representations remain distinct. Explicit replacement can clear current opaque state, but original model/procedure provenance and definition Undo semantics remain separate. |
| Invalid/incomplete/foreign/duplicated multipart set or reserved user parasite | Keep exact inner parasite framing (name length/name/flags/data length/data), duplicates and physical order before a native name-keyed list could collapse them. Never claim it as active transport. |
| Inert multipart records after owner-ID repair, copy or conversion | Emit/retain validated suppression carriers keyed to exact original record digest; they can only suppress activation, never validate data. Invalid/future carriers remain inert and are not recursively wrapped. |
| Ordinary parasites mixed with intercepted transport | Continue through native handling and write once; do not also archive them as transport duplicates. |
| Unknown reference identity | Keep descriptor/provenance, never substitute by name, path, network or old process ID. |

All retained bytes require ownership beyond a temporary parser buffer. The pure
byte decoder returns ranges into an immutable source whose caller must retain;
application provenance owns/refcounts snapshots independently. Resource bounds,
malformed framing and allocation/I/O failures must produce explicit errors or
inert recovery according to the applicable layer, not an empty success. The
policy does not promise successful recovery of unreadable/truncated bytes that
were never safely framed, nor semantic execution of an unknown payload.

The original whole image snapshot is recovery evidence, not a claim that every
future edited save is identical to the original file. Raw sequence ordering and
named dictionary ordering have deliberately different preservation rules.

## Verification scope

The accompanying source verifier checks the archive keys, base-dictionary
carry-forward, duplicate-key refusal, unknown semantic guard, separate origin
archive and raw transport capture/protection paths. These checks establish
specification correspondence only. Full malformed input, Undo, editing and save
roundtrip tests remain their separately recorded acceptance gates.
