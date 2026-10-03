# Reconstructed first native PDB Filter route

This checkpoint verifies only the first allowlisted bundled Blinds route and
its process/owner boundaries. The full migration, full context adapter, UI
progress forwarding/editor coverage, relocated package and other platforms
remain separate gates.

- `native.json`: freshly built full Filter suite (94 tests), scheduler (37),
  spool (11), process transport (15) and wire validation. Source fingerprints
  are from the immediately following archived focused build; this native run
  did not independently capture a before/after fingerprint pair
- `sanitizers.json`: 22 explicitly instrumented sources and 41 RTTI-only
  compatibility sources; helper and bundled Blinds are instrumented. Four
  independent unit targets and 11 selected native Filter/cancel/exit-status
  cases passed. All 1,709 recorded source/header inputs stayed unchanged.
  Other GIMP/dependencies are ordinary; LSan is off
- `foundation-sanitizers.json`: the two existing image-close/retained-handle
  acceptance cases, rerun against the same sealed instrumented binary with
  unchanged source and binary fingerprints
- `sources.tar.gz`: exact repository/generated inputs captured before the
  focused build. External dependency headers are fingerprinted in the report
- `oracle-probe-executed.cpp`, `oracle-probe.log`, `oracle-probe.json`: the actual
  320-case native comparison against freshly captured old PDB outputs; all 320
  match. Its initial 12 odd-fan identity assumptions were wrong and are not
  passing evidence. The probe's final identity counter was not corrected;
  the metadata expressly excludes it. The registered identity smoke now uses
  even fan widths. The old corpus is `../../fixtures/legacy-blinds`
- `../filter-active-quit-pdb/report.json`: normal actual console Quit with one
  and two already-running 4096² jobs, observed only after explicit lower-source
  invalidation following observer readiness. Exit times 0.395/0.426 seconds;
  no recorded child/group/profile survivors or 5-second fallback
- `../filter-active-quit-sanitizers/report.json`: same proof with exactly the
  instrumented console/helper/Blinds binaries; 0.788/0.846 seconds, no sanitizer
  diagnostics or recorded survivors. Both proofs preserve 0/64/69 controls and
  exact executed observer copies. Native controller tests separately cover
  0/1/64/69/70/130 and rejected GUI-exit signal behavior

Earlier observer/API failures remain in `../filter-active-quit` and
`../filter-active-quit-after-batch-fix`. Their missing-worker or TypeError
outcomes are not accepted Quit evidence. Application startup diagnostics are
preserved in the final logs; this checkpoint does not assert that the entire
old/current application starts without warnings.
