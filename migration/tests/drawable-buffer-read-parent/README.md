# 09.003 read-domain verification (whole-hunk duties remain open)

The original task requires negative offsets and partial reads. The completed
`bounded-surface-read` child already verifies the bounded reader itself. This
additional work exercises actual callers and storage bridges; it does **not**
silently reinterpret the 48 assigned historical duties as only that criterion.
`duty-audit.json` retains every source identity and explicitly identifies
read-scope coverage and remaining write/undo/dispatch/preview/fill semantics.
The execution-time audit/report below retain their original parent-open status.
The later criterion-level resolution is `../../contracts/drawable-buffer-read-resolution.json`: original 09.003 read acceptance is DONE, while all 48 whole-hunk inventory statuses remain TODO. No production changes are part of this work.

## New behavioral checks

* `/painter-surface/temp-buf-read-domains`: real `GimpTempBuf` → linear GEGL
  buffer → nested child, all four native byte channel layouts, ancestor rowstride,
  partial and negative requests, padded output with canaries, release original
  TempBuf and ancestor handles before reading, and independent output surviving
  mutation/finalization. This is not a hand-written TempBuf replacement.
* `/painter-surface/combined-read-domains`: actual Surface with target child,
  supplied baseline, nonincremental floating buffer, shifted selection and
  periodic texture together. Negative-domain and translated zero-domain outputs
  must agree exactly for byte and double formats. Selection holes and parent
  halo remain untouched; cancellation restores the baseline.
* `/fill/independent-read-domains`: actual `Snapshot`/`Search` across more than
  eight horizontal cache tiles and negative tile boundaries, distinct source,
  search ROI and child-mask extents. The populated mask-parent halo must not
  become readable coverage. A source-color barrier has an independent exact
  output oracle. Four channel layouts × three work budgets (1, 37, 4096) run
  after caller source/mask mutation and destruction.
* `/fill/read-domain-owner-orders`: search retains its snapshot after caller
  release; cancellation publishes no output, completing permits an explicitly
  retained result to survive search destruction; snapshot weak ownership expires.

## Reproduction and limits

Source the configured dependency environment, then run `python3
migration/tests/drawable-buffer-read-parent/run-fill.py`. This rebuilds and runs
real bounded-fill production source normally and with ASan/UBSan/
float-cast-overflow, bounded to 60 seconds per execution. Dependency libraries
are not instrumented; LeakSanitizer is disabled. This is not a full-app sanitizer
claim. The registered native tests are `gimp-painter-surface` and
`painter-bounded-fill`, run under the repository's portrait Xvfb runner. These
are buffer/ownership tests, not new UI features or screenshot assertions.

The pre-existing pinned-runtime fill trace and bounded Surface reader suites
are coverage references in the audit, **not claims of new executions** here.
Raw Meson environment maps are excluded from stored native evidence.

## Recorded result

Full registered targets passed: Surface 12/12 TAP in 1.148407 s, bounded fill
7/7 in 0.098634 s, no skips or timeouts. Standalone normal and sanitized
bounded fill each passed all seven groups. Ten relevant source files match
the native build source tree byte-for-byte; hashes are retained in `report.json`.
`python3 migration/tests/drawable-buffer-read-parent/validate.py --check-source`
validates current source; omit the switch for archived evidence after subsequent
code changes. The build retains an existing StartReentry visibility warning;
native diagnostics retain ignored .dummy plugin entries.
