# TempBuf conversion — original 09.005 criterion

Action: **TempBuf の変換を実装する**. Acceptance: **stride・format・所有権を保持**.
Dependency: completed 09.004 write criterion.

The current application already has `GimpTempBuf`, `gimp_temp_buf_lock/unlock`,
`gimp_temp_buf_create_buffer`, pixbuf import/export and owned copy/scale APIs.
Tests execute those actual C implementations and the actual C++ Surface consumer;
there is no replacement model or recreated legacy TempBuf wrapper.

## Explicit conversion and ownership contract

* Native TempBuf is packed, buffer-local zero-origin storage with exact `Babl`
  format (including component precision, TRC, space and alpha). A same-format or
  null-format lock borrows its data; unlock does not free that storage. A deep
  copy preserves format, dimensions and bytes but owns independent storage.
* A different-format lock owns scratch storage. READ converts in, WRITE converts
  back on unlock, READWRITE does both. READ scratch edits never write back;
  WRITE-only scratch is unspecified until initialized. The caller must retain
  the TempBuf until unlock; the lock is not an independent owner of the TempBuf. Tests check exact byte
  to same-model uint16 expansion (`v * 257`) and reverse values, not approximate
  colorimetric agreement.
* `gimp_temp_buf_create_buffer` retains the TempBuf while exposing a linear alias.
  Nested GEGL subbuffers keep ancestor storage/stride alive. The actual Surface
  producer clips writes to the child domain; raw `gegl_buffer_set` is not claimed
  to be a new general bounds-checking API. Releasing the original TempBuf and
  ancestor handles before a child write is safe. Commit/cancel tests guard every
  parent byte and confirm exact child dirty bounds.
* Pixbuf import consumes its real rowstride and subpixbuf pointer offset into an
  independently owned packed TempBuf. Export returns an independently owned
  pixbuf and advances its actual rowstride, including RGB padding. Destroying or
  mutating either source cannot alter its copy. Alpha/no-alpha and explicit
  Gray16/RGBA16 conversions are tested with exact endpoint/alpha oracles.
* Scaling returns independent storage even for unchanged dimensions; tested
  nearest-neighbor mappings preserve format and complete native pixel bytes.
* Every test returns GimpTempBuf's total allocation counter to its baseline after
  releasing all owners. This checks actual TempBuf allocations, not dependency
  library leak-freedom. Large allocation budgets and failure admission remain
  09.009; no enormous allocation is attempted by these tests.

A TempBuf no longer stores image/item origin. Placement is represented by its
consumer and GEGL rectangle, with image/local translation specified by 09.002.
Old subwindow wrappers leaked on free; that bug is not reproduced. Current
refcounted ownership, ancestor stride and clipped producer semantics are tested
instead. Generic old PixelRegion closed-loop iterator equivalence is not inferred.

## Evidence and retained historical duties

`../tests/tempbuf-conversion/` records normal and focused sanitizer executions.
Six groups cover 144 native-format copy/borrow cases, explicit converted lock
access modes, padded/subpixbuf import/export, explicit precision/alpha conversion,
16 nested-alias Surface write/cancel scenarios, and deep-owned scaling. Actual
TempBuf and pixbuf C sources, Surface C++ source and tests are instrumented with
ASan/UBSan/float-cast-overflow. Dependencies remain uninstrumented; LeakSanitizer
is disabled. Registered native evidence is recorded separately.

There are **34**, not only six, assigned source duties: six dispatcher hunks,
21 PixelRegion hunks, one whole pixel.hpp and six TempBuf hunks. The JSON contract
retains every source ID/range/hash, pinned blob digest, original source scope,
per-hunk meaning and inherited 09.003/09.004 obligation. Inventory identity,
primary WBS ownership and unfinished whole-hunk checkpoint status are unchanged.

The original conversion criterion can complete without waiting for removal of
all old consumers. **31.017** remains responsible for the entire consumer and
representation replacement, including shallow descriptor copies, five-region
serial/parallel dispatch, first nonperiodic region bounds, periodic width/height
portions, wrapped tile/linear addressing and all old TempBuf API consumers.
**09.015** retains full scalar precision/color-space behavior beyond format-safe
storage conversion. Those obligations are explicit per source duty and reverse
linked from the existing owner tasks. No new child task hides unfinished work.


### Transitive whole-hunk completion guard

The six TempBuf rows in the current 09.003 and 09.004 resolution manifests also
retain explicit 31.017 successor links. Completing the read/write/conversion
criteria alone cannot prematurely close those ancestor whole-hunks while full
consumer migration is still open. Original text/IDs, historical task lists and
TODO checkpoints are preserved. Dedicated negative controls demonstrate that
09.004/09.005 DONE without 31.017 DONE is insufficient. Historical runtime
checkpoint evidence remains untouched; this is stronger current ownership
tracking, not a retroactive claim of a new test execution.
