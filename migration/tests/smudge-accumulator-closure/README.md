# Original 23.002: accumulator initial sampling and updates

Original title: **Smudge の蓄積 buffer を移植する**. Dependency: **23.001**.
Unchanged acceptance: **初期採色と更新が一致**.

The 52 original source duties are preserved individually in
`migration/tests/smudge-accumulator-closure/rows.json`, with exact before/after
rows, source hashes, replacement routes, criterion-specific justification and
all other whole-hunk owners. This closes only their 23.002 roles, not every
behavior mentioned in a shared hunk. No duty is removed or reassigned.

The final production `app/paint/gimppaintersmudge.cpp` has Git blob
`40c57e21ae07ffb4cc2500977e4033b983750d59`, introduced by hard-mask correction
commit `5d455fd98c`; this review changes no production or
native test. Four sealed checkpoints supply the proof: first overlapping seed
including opacity zero; native float-mask to byte normalization; partial and
transparent alpha including hidden colors; current dynamics and neutral curve
cross-pin. Existing 26 native cases, 384 center scenes / 2016 observed updates,
four captured-old 48-scene raster routes / 193 records each, 1024 integer kernel
vectors, five model tests and 1028 neutral mapping values passed normally and
with their documented focused sanitizer units. Checker replay is **not a new
native execution**. Source-only old adapters and captured old fixtures are not
new old-runtime runs. Ten held providers remain the recorded committed private
baselines; no whole-current-HEAD aggregate is claimed.

## Ownership and crop role review

- `State` owns the GEGL accumulator. `clear`, `release_frame`, deferred `close`
  and `finish_frame` retire it on finish/cancel/dispose and exceptional paths.
  Reuse/cancel/dispose/reentry/failure/current-target checks are covered by the
  unchanged final 26 native tests and focused 18-unit sanitizer receipt. LSan
  was disabled; this is not a leak-detector result.
- The first successful overlap initializes the retained square, prefills the
  clamped sample and copies the visible drawable overlap. Entirely outside
  contact returns without allocation. Zero opacity seeds only once and does
  not update an existing accumulator. The current clipped/negative/outside
  center matrices and captured full-raster corpus test these distinct roles.
- `sampled`, `accumulated`, `shaded` and output storage are owned local vectors.
  Accumulator GEGL publication occurs before optional shade; shade reads the
  accumulator into disjoint storage. Captured multi-dab blending off/on raster
  comparisons corroborate this accumulator-isolation role. Original 23.003
  **旧 blending-output を再現** remains open for its full output contract.
- Accumulation omits only SIZE dynamics; output crop reapplies them. Direct
  GEGL publication replaces old temporary borrowed `canvas_buf` subwindow
  swapping. Bounds/current-state checks and existing size-varying raster tests
  prove this ownership/update role. Original 23.004 **蓄積 buffer の再配置が一致**
  remains open for complete transformed footprint/repositioning; this review
  does not claim arbitrary angle/aspect geometry from center-only checks.
- Typed rate/blending property defaults and native consumption are in the
  sealed production and matrix routes. Original 23.005 **値が描画と往復に反映**
  retains GUI/save/roundtrip obligations; they are not prerequisites for this
  initial read/update criterion. No UI change or new screenshot is claimed.
- Disabled branches/declaration delimiters and whitespace are explicitly
  identified, rather than reimplemented as unused APIs or deleted from scope.

## Historical evidence admission

All four prior audit snapshots remain OPEN exactly as recorded. Their test
archives, receipts, diagnostics and source seals are unchanged. Only documentary
checkers/README/manifests and existing checker-chain pins admit one exact
successor row per original ID. The admission compares every field against the
immutable prior row or reviewed closure row; unknown changes are rejected.
Cross-owner IDs/criteria remain guarded. Snapshot future-owner TODO states
record the time of closure; the checker does not require future tasks to remain
TODO forever. Negative controls exercise missing IDs, altered criteria, sources,
ownership and metadata. Source changes require a new explicit successor proof.

`check-closure.py --commit HEAD` reads the exact selected commit for the final
ledger, dependency and listed current-source pins. It executes hashed documentary
checker copies in the worktree; their historical ledger checks accept only exact
before/after rows, permitting intentionally stale worktree ledgers without
overwriting them. This does not assert every source in the commit was rebuilt.
`--preflight` uses HEAD dependency status with the current original ledger rows.
