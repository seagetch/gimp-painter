# Exact committed integration — 0a775

Source commit: `0a775df4135052a2c851c9a5dfa37cea25887268`.
Full 1,610-step native build succeeded. Results are deliberately separate:

- **47/47 selected Meson targets PASS.**
- **515 executed non-SKIP TAP cases**, **six explicit TAP skips**.
- **Two non-TAP targets PASS:** enum-library-only and Normalize-shadow.
  Their native output is retained; exhaustive shadow iterations are not
  invented TAP cases.
- **90 selected extra executions PASS:** Filter 65 (old Edge excluded),
  Red Eye/Exchange/Colorify/Normalize XCF 14, Colorify GUI 7, Red Eye GUI 1,
  graph GUI 3. These executions can overlap prior suites; no unique-case sum.

Five Perspective primary-suite skips are covered by the exact same selectors
in separate geometry/damage targets. The sixth, event-order close-active-image,
remains explicitly unresolved. An OK Meson target does not erase this skip.
Options' 28 cases include the prior-serializer environment. The three newly
committed tool Scenes explicitly enable/assert oncanvas UI; portrait-only
assertions are opted into with `GIMP_TEST_REQUIRE_PORTRAIT=1`.

## Scope and actual images

Authenticated Xvfb root: 1080×1920×24. Thirty-one actual PNGs: 25 root captures
and six native 1040×1800 windows on the portrait root. The archive includes
Smudge pending-paint Undo, Save continuation, MyPaint tool, Colorify editor,
Filter geometry/flip, Red Eye, graph invalidation, physical gesture, named
editor, catalog and standalone Options editor captures. API-driven/native
fixture input is not a physical-pen claim. Backend/model targets and standalone
editors are not all image-shell interaction proofs; the predecessor 44-row
matrix and committed test-only enablement package retain that distinction.
No production default was changed to force the setting.

## Exact inputs and exclusions

All **19,601 tracked native source files** and **1,714 external asset files (content hashes)**
remain unchanged. External asset content is pinned separately; repository
submodule identity is not inferred solely from byte hashes. Forty-six native
executable hashes and seven held-scope exclusions are sealed. Ten previously
safe provider hashes remain identical; the eleventh, Options, has exactly the
separately approved class-default substitution `FALSE`→`s.default_value`.
The paused acquisition diagnostic is not changed or executed. The held Smudge
size-domain fixture is absent; its provider is the committed safe blob.

The independent Git-only source gates materialized **17,089 selected inputs**
(including actual app/header bytes): adapter 2535 sources/720 reviewed sites/
4 dormant findings PASS; persistence 1379 records/873 dispositions/16 routes
PASS. The first metadata invocation lacked Git object-store access for pinned
legacy reads; its setup failure is preserved. Adding the existing read-only
Git metadata link fixed setup, with native snapshot hashes unchanged. This is
not a validation waiver or a repin of field records.

## Historical count wording correction

Historical archives remain immutable. Their former `ok`-line counting included
five explicitly skipped Perspective entries:

| Receipt | Historical `ok` lines | Executed non-SKIP TAP | Skips |
|---|---:|---:|---:|
| exact503 (one failing Smudge target retained) |449|444|5|
| exact55bf (all selected targets passed) |478|473|5|

Thus the earlier exact55bf “532 completed” wording (478+54) overcounted five
skips; the proper categories are 473 executed TAP plus 54 selected extra
executions, not a unique-case total. Every skipped Perspective selector has an
independently successful matching geometry/damage target in both receipts.
`historical-tap-count-correction.json` traces exact names and results hashes;
no functional coverage or acceptance is removed, and no associated task is
reopened solely for this counting-label correction. The existing failed503
Smudge result is not rewritten.

`verify.py` verifies the immutable archive, 59 artifact pins, actual dimensions,
source/exclusion/gate seals, separated counts, and the erratum against both
unchanged predecessor archives. `test_verifier.py`: three groups and 12 semantic
negative controls. No new sanitizer, whole-port/full-suite, WBS-parent closure,
publication, or later-HEAD integration claim.
