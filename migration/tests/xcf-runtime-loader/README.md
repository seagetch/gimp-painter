# Relocated XCF sanitizer runtime-library selection

A clean build exposed a runner error: its relocated executable could not use the
ordinary test binary's relative RUNPATH, silently loaded the host libgimpconfig,
and exited127 for missing `gimp_config_set_xcf_version` after14 open cases.
The unmodified-source open suite had passed normally. This is not an XCF parse
failure or evidence that the paused source candidates are required.

The runner now explicitly prepends the exact build's libgimp shared-library
directories while retaining the inherited dependency path afterward. Missing
built DSOs fail closed rather than intentionally falling back to a host GIMP.
Three tests execute the actual AST path-selection statements (no GIMP build
required), covering missing libraries, precedence, deduplication and unrelated
directories. Actual relocated execution then passes provenance12 and open31;
`ldd` resolves libgimpconfig inside the same build. The receipts are selected
checkpoints, not a completed full sanitizer run or whole-branch claim.

The recorded clean base plus four explicitly hashed XCF candidate files remains
separate from this runner fix. All five paused provider paths match that committed
base. No blocked candidate is compiled or accepted as a dependency.

Run: `python3 migration/tests/test_xcf_runtime_loader.py`.
