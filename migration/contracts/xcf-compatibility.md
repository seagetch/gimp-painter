# Painter XCF byte-decoder contract

Status: metadata/probe groundwork, 2026-10-02. This is **not** a working legacy
Open implementation or a successful migrated save/reopen test. Tasks 10–12 must
not be marked complete as a group on this evidence.

## Sources and scope

The authoritative old implementation is `../gimp-painter-legacy` at
`afa43fae3e920210146abed514f136fd49f671b5`, notably:

- `app/xcf/xcf-private.h`: property and Filter argument wire constants
- `app/xcf/xcf.c:310–346`, `xcf-load.c:164–365`: header and load dispatch
- `app/xcf/xcf-save.c:204–330,354–447`: version choice/header/offset tables
- `app/xcf/xcf-load.c:448–1275`, `xcf-save.c:450–1325`: image/layer/channel properties
- `app/xcf/xcf-load.c:1281–1457`, `xcf-save.c:1329–1438`: Filter/Clone nested records
- `app/xcf/xcf-read.c:144–191`: original string consumption/normalization
- `app/core/gimpclonelayer.cpp:302–340`, `xcf-load.c:2503–2534`: source-name resolution

The modern comparisons are this GIMP 3.0 checkout's `app/xcf/xcf.c`,
`xcf-load.c:218–300`, `xcf-private.h`, and `libgimpbase/gimpbaseenums.h`.
The pure implementation is `app/xcf/painter-xcf-compat.{hpp,cpp}`. It has no
GObject, GIMP, GEGL, PDB, file-I/O or UI dependency and changes no reader cursor,
input byte, layer or application state. It returns absolute ranges into a
caller-owned immutable input. The caller must retain that exact input; the
result is not an independently owned copy of the payloads.

Implemented: header candidates, ordered image/layer/channel property records,
layer/channel offset tables, layer/channel metadata, bounded Filter/Clone
records, semantic identities for old modes, and raw preservation. Not implemented:
normal Open dispatch, decompression, tile/hierarchy traversal, mask object
construction, path parsing, GObjects, reference binding, Filter execution,
ordinary attribute application, a new writer, or edit/save/reopen behavior.

## 1. Header and candidate selection

All integers here are big-endian. The first fourteen bytes are
`gimp xcf file\0` (v0) or `gimp xcf vNNN\0`. Width, height, and base type are
32-bit words at offsets 14, 18, and 22.

| Candidate | Versions audited | Word at 26 | Image properties | Offset words |
|---|---|---|---|---|
| Old painter | 0–4 | First property tag | 26 | 32-bit |
| Standard | 0–3 | First property tag | 26 | 32-bit |
| Standard | 4–10 | Precision | 30 | 32-bit |
| Standard | 11–23 | Precision | 30 | 64-bit |

Standard v4 precision is 0–4, with historical meanings u8 nonlinear, u16
nonlinear, u32 linear, half linear, float linear. Standard v5/6 values are
100,150,200,250,300,350,400,450,500,550. Later values are explicit current
precision constants (including 500/550/575 half and 600/650/675 float).
No current enum is used to interpret a legacy field.

Old writer version selection forces v4 for FilterLayer, CloneLayer and modes
26–29. It does **not** force v4 merely for modes 23–25. Therefore low version
numbers cannot establish a standard semantic dialect. Old v0–3 can be ordinary
shared-format data or carry colliding semantics.

`probe()` tries both eligible candidates with separate budgets and no mutations.
It classifies:

- `legacy_candidate` / `standard_candidate`: only that structural metadata
  candidate completed; this is evidence, not proof the complete image opens
- `shared_layout`: both v0–3 candidates completed without layer property32/33
  or mode >=23; this does not identify the application that wrote the file
- `ambiguous`: both completed and their interpretations can differ
- `inconclusive`: either candidate hit a work/resource/allocation/recovery limit;
  never eliminate a candidate merely because its budget ran out
- `invalid`: neither metadata candidate completed; **not** proof the historical
  reader cannot recover the file
- `unsupported`: version exceeds both audited readers' ranges

A synthetic v004 with zero words at26–45 is structurally dual-valid: the old
candidate reads END@26 and tables@34/38; the standard candidate reads precision0
@26, END@30 and tables@38/42. Tests intentionally assert `ambiguous`. It is not
a genuine application-open fixture. Unknown properties and degenerate records
make a version/first-word heuristic unsafe.

### Required future Open/recovery behavior

1. Run the probe before creating image state, on a bounded, reusable source view
2. Keep both candidate diagnostics and all original bytes
3. Do not force a winner in ambiguous or resource-limited cases. A future
   application integration must either validate both complete object/pixel
   interpretations in isolated temporary state, or expose a reversible dialect
   choice/recovery route. It must never silently cast colliding enums
4. A candidate failure must not replace GIMP's existing standard recovery or
   prevent a known-old-readable file from reaching a compatibility recovery
   route. Current probe is not an exhaustive emulation of old salvage behavior
5. Commit one completed construction transaction; cancel/failure destroys all
   temporary objects. Never share partial layers between candidate attempts
6. Do not save over the input during detection or recovery

No branch of this Open contract has been wired into `xcf_load_image()` yet.

## 2. Records, ordering and field collisions

Properties have tag:u32, declared-length:u32, payload. `Property` stores both the
actual consumed byte range and declared length, preserving order, duplicates,
END and unknown tags. Fixed-width known properties consume their historical
reader width, even when the declared width differs; `length_mismatch` records
that discrepancy. Unknown properties use declared framing. Colormap and user
unit consumption is structural rather than a blind length skip.

| Layer tag | Old painter meaning | Standard meaning |
|---|---|---|
| 32 | Filter procedure/string plus nested arguments | Position lock, u32 |
| 33 | Clone source-name/string plus nested END | Float opacity, IEEE32 |

Context matters: an image-level unknown tag32 is not a FilterLayer. Old custom
properties are interpreted only after selecting the painter layer context.
No global property-number rewrite occurs.

Legacy reader applies properties in encounter order. Repeated scalar properties
ordinarily apply later values. However group/custom-type properties replace the
layer, transferring only selected values; consequently simply reducing all
properties to a last-value dictionary does not emulate every malformed or
reordered file. This decoder deliberately retains the full sequence and performs
no replacements. Unknown old properties were skipped and generally not re-saved;
the migration must retain them rather than copying that data loss.

`PROP_USER_UNIT` has another old writer bug: missing parentheses in its chained
`?:` length expression can declare only the first string length+5. Both old and
modern readers consume factor:f32, digits:u32, then five XCF strings (three in
standard v21+). The decoder follows the consumed fields and retains the mismatched
length; the test is explicitly synthetic, not an application fixture.

The companion `migration/inventory/xcf-wire-records.tsv` records the currently
inventoried framing. Opaque paths/parasites/metadata remain preserved byte ranges,
not fully interpreted structures. Numeric constructor defaults and all ordinary
metadata behavioral tests still require the app integration audit.

## 3. Modes 23–29: semantic identities, never enum aliases

| Raw old value | Decoder semantic identity | Conflicting standard number's meaning |
|---|---|---|
| 23 | erase | Overlay |
| 24 | replace | LCh Hue |
| 25 | anti_erase | LCh Chroma |
| 26 | src_in | LCh Color |
| 27 | dst_in | LCh Lightness |
| 28 | src_out | Normal |
| 29 | dst_out | Behind |

`decode_legacy_mode()` returns a separate `LegacyMode` enum. Values0–22 return
`common_0_to_22` alongside the caller's original raw value; values above29 return
`unknown`. This is not a GIMP mode/compositor mapping. Runtime composition and
full legacy mode interpretation are separate outstanding work.

## 4. Filter wire records, exact bugs and recovery separation

The outer PROP32 length is patched with the actual bytes written. Its payload is
an XCF string naming the procedure, then argument records through tag0/size0.
There is no explicit argument count. XCF strings are u32 byte count followed by
those bytes, usually including NUL; count0 represents NULL. Raw bytes remain
unchanged even with missing NUL, embedded NUL or invalid UTF-8. The old string
reader forcibly NUL-terminated the last byte and converted to UTF-8; a later
application adapter must reproduce name lookup behavior without destroying the
original bytes.

| Arg tag | Writer bytes after tag/size | Old reader consumption | Decoder |
|---|---|---|---|
| 0 END | none | none; stop | Retained terminator |
| 1 unsupported | none, size0 | none; **invalid GValue pointer-copy bug** | Opaque unsupported slot; no GValue |
| 2 INT32 | size4 + u32 bits | four | Exact bits |
| 3 INT16 | **size2 + four u32 bytes** | **four** | `int16_writer_32_bits`, mismatch flag |
| 4 INT8 | size1 + byte | one | Exact byte |
| 5 FLOAT | size4 + IEEE32 | four, promoted to double | IEEE32 bits, including NaN payload |
| 6 INT8ARRAY | size0, contents omitted | none | Omitted-data slot |
| 7 FLOATARRAY | size0, contents omitted | none | Omitted-data slot |
| 8 STRING | writer does not use this tag | seeks back to size word, reads XCF string | Reader-compatible string |
| 9 RGB | no supported writer branch | none | Omitted-data slot |
| 10 DRAWABLE | size0, reference omitted | none | Omitted-reference slot |
| other | no known writer | none; next header immediately | Unknown slot, raw range retained |

INT16 and INT8 reader initialize unsigned GValue types but call `g_value_set_int`,
which has incompatible type preconditions. The decoder preserves the recorded
bits; it does not claim they became the same effective value in the old runner.
Tag1 does `g_array_append_val(args, value)` with `value` a pointer and element size
`sizeof(GValue)`. That reads unrelated stack bytes. It must **never** be reproduced.

For a G_TYPE_STRING the old writer emits **tag7**, then `xcf_write_string()`:
the word occupying the size field is the string byte count. The old reader
interprets tag7 as an omitted float array and leaves its bytes at the next
argument header. Depending on content this misaligns, fails, or encounters END
at an unintended position. A successful structural parse alone cannot prove
that a string became a usable old argument.

`decode_filter(...legacy_reader)` follows the actual bounded byte consumption,
without executing the invalid GValue operations. `writer_string_recovery` is a
separate explicitly requested interpretation of nonzero-size tag7; it sets
`used_writer_string_recovery` and labels the argument `recovered_writer_string`.
It is never selected by `probe()`. Zero-size tag7 is ambiguous between omitted
float array and NULL string and sets `has_ambiguous_null_string`.
No guessed array elements, RGB values, drawable IDs, image IDs, or unsaved
original precision are invented. Full outer payload and unconsumed tail remain
available even when the nested parse fails or terminates early.

## 5. Clone source names

PROP33 contains an XCF string and nested tag0/size0. For unknown nested tags the
old Clone reader consumes the header only, ignoring the declared length; decoder
records preserve that behavior without allocating GValues. The outer payload is
retained even on failure.

Old `get_source()` searches the same image's layer tree in container index order:
visit a layer's name, then recursively visit its children, then the next sibling.
It chooses the first exact `strcmp` match, including a matching group before its
children. It does not exclude the clone itself in this search. The setter stores
a duplicate of the source name; loading invokes getters in a later recursive
post-pass. A successful match clears the pending name and installs the pointer.
An unresolved name remains pending and can be retried by later getters. The old
writer dereferences that nullable result without a guard.

This decoder does not resolve names or assign stable IDs. The future two-stage
app construction must first restore layers/order/hierarchy, then reproduce this
name search and preserve the original name even after resolution. Do not connect
a missing name to a "similar" layer. Actual duplicate-name/self-reference,
rename, delete, Undo and unresolved-save runtime coverage remains outstanding.
The real supplied clone fixtures verify sources in a group and a group source.

## 6. Bounds, ownership and unknown-data persistence

Default per-candidate/per-extension limits:

- 100,000 metadata/offset/argument records
- 16,384 referenced layer/channel objects
- 1 MiB per string
- 64 MiB per declared property/outer extension
- 256 MiB aggregate byte-work (revisited bytes also count)

Bounds checks use subtraction (`length <= end - position`) before addition.
64-bit references are compared with source size before narrowing to size_t. No
vector/string allocation is sized from an unchecked length. Allocation failures
and container limits become diagnostic results. A limit result is inconclusive,
not a permanent rejection of a legitimate large work. Callers may select a larger
explicit budget and retry against the same immutable bytes.

Unknown data is represented by original encoded/payload ranges plus owning
image/layer/channel context and sequence. An eventual persistent model must copy
those bytes or retain a read-only backing object; it must not retain pointers
into temporary I/O buffers. Preservation here proves exact in-memory access to
the original bytes, **not** that a writer already roundtrips them.

Hierarchy/mask offsets are range checked but not followed. Table sizes, strings,
properties and record work are bounded. Tile allocation/product checks, hierarchy
cycles, overlapping/aliased object ranges, compressed streams, salvage offset-table
scanning, nested paths/vectors, and recursive mask/channel construction are not
validated here. These cannot be called complete task11.023 coverage.

## 7. Genuine fixture evidence versus synthetic tests

All paths below are under `migration/fixtures/`. Provenance and captured old
application behavior belong to `legacy-runtime/manifest.json` and its README/logs.
Fixture hashes are also fixed below so a changed fixture requires explicit review.

| File | SHA-256 | Meaning |
|---|---|---|
| legacy-runtime/ordinary-layers.xcf | 9079ee063d3bf138d374560d961343b6e31d6eb3bf32ffcb80ec9caeb364b722 | Real old save/reopen success, v003 |
| legacy-runtime/clone-normal-in-group.xcf | 0fb12d0345bf6c6685562944dae91fb3cc79e84122bb2e67080164d79846d038 | Real old CloneLayer save/reopen, v004 |
| legacy-runtime/clone-group.xcf | 6953b26108bb89196c96c2d1a5d38ddd73883c43ba4918a8693d48f867add11e | Real old group-source CloneLayer save/reopen, v004 |
| legacy-runtime/filter-edge.xcf | 9e6f93077e32c239b5d7dca1424bcb513aab98561f6b68a86026a7f0fd566e2a | Real completed asynchronous edge result/save; **old reader crashes** |
| gimp3-baseline.xcf | e9622c6bb195ec5de7a4e00d1bd558527b43b728fdefae8b721d589c8cc7c9ae | Existing modern baseline, standard v011 |

Genuine byte assertions include:

- Ordinary: properties@26; layer table@375; layer records@403,661,2625;
  second layer mask@2434; opacity payload@716 is191/255 rather than exact75%
- Both clones: compression17@26 makes the standard v4 precision invalid;
  layer@395; PROP33@561, payload@569 length25, source bytes@573 length13;
  nested END@586 and hierarchy@610
- Filter negative fixture: PROP32@563, payload@571 length89; procedure
  `plug-in-edge`; argument tags2,1,10,5,2,2,0 at588,600,608,616,628,640,652.
  Unsupported image-ID and omitted drawable are retained without triggering
  the old reader's memory bug. This is additional broken-file recovery evidence,
  not part of the positive old-readable roundtrip claim
- Modern: v011 precision150@26, properties@30, 64-bit offset tables

Synthetic tests are separately labelled in
`app/xcf/tests/test-painter-xcf-compat.cpp`: v004 dual-valid ambiguity, lower-version
mode23 collision, standard32/33, duplicate/unknown properties, actual-width INT16,
reader STRING8, writer-mistagged STRING7, zero-size ambiguity, missing NUL, tails,
user-unit length bug, truncated prefixes, huge lengths/64-bit offsets, budget
exhaustion and deterministic byte mutations. These do not replace genuine
INT16/string writer and old-reader runtime fixtures, which are still needed.

Commands and results:

- Standalone C++14 `-Wall -Wextra -Werror -fno-rtti`: 144,142 checks passed
- Meson `painter-xcf-compat`: 1/1 passed; decoder compiled into libappxcf
- ASan+UBSan `-fsanitize=address,undefined`, halt-on-error: 144,142 checks passed
- LSan explicitly disabled (`ASAN_OPTIONS=detect_leaks=0`) because the host
  ptrace environment is incompatible; no leak-sanitizer pass is claimed
- Logs: `migration/tests/xcf-compatibility-meson.txt`,
  `migration/tests/xcf-compatibility-sanitizers.txt`

## 8. Remaining save-format decisions (tasks10.011–10.014,12)

No new save wire format is adopted by this change. A standard-XCF parasite or
other explicitly versioned noncolliding extension remains a design candidate.
It must preserve independent layer type, stable internal references plus original
legacy name, procedure and ordered argument records, unparsed raw records,
definition generation, cache generation/completeness, and unknown extension
versions. The numeric legacy property/mode values cannot be emitted into a modern
namespace. Designing a record alone does not implement atomic save, ownership,
execution-time snapshots, or edit/save/reopen. Those gates remain open.

### String-bound diagnostic refinement

A string length beyond the available source is `truncated` before considering
its configured string budget. Raising a budget cannot supply absent bytes;
reporting such a scalar-as-string interpretation as merely resource-limited
would conceal structural evidence. An actually present string exceeding the
budget remains `limit`. `migration/tests/xcf-string-bounds.txt` records 144,144
normal and ASan/UBSan assertions after this refinement (LSan disabled).
