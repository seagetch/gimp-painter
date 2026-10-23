# Original 04.002: history is not a frozen current work queue

The original criterion remains mixed C/C++ Meson registration, not completion of every feature named by a Makefile. No WBS, assignment or execution ledger is changed by this maintenance.

The old checker applied the original commit's unchanged-unreviewed digest and all-reassigned-TODO assertions to today's ledger. Legitimate successor work necessarily invalidates those historical invariants. The repair separates two checks without dropping either ownership review:

1. `historical-checkpoint.tar.gz` contains 127 exact files from commit `aa8e7fad108269e2c1dd5ebcbf78865fb37d1ee9`, plus the prior work ledger from `a1912fc6b4357827fc7199a0429992d006a76327`. `historical-pin.json` binds each byte hash and archive hash. The checker extracts regular files into a private directory, makes them read-only and executes the **unmodified original checker and original assignment rules**. Thus the original unchanged/unreviewed/source-diff/TODO-at-that-checkpoint/auxiliary-union invariants all still pass exactly where they apply. The original six cpp-registration artifacts and 47-row review must also remain byte-identical to their pinned versions.
2. The live 47-source review still reconstructs all legacy Makefile payloads and regenerates assignments with the current routing rules. All 19 registration duties must remain DONE and exactly exhaust current04.002 assignments. Every one of the 47 sources must have complete current canonical source/phase/task coverage, matching source blob and payload identity. DONE duties require owner/artifact/test/result evidence; future legitimate successor completion is permitted, not forced back to TODO. The global `check_tasks.py` work-ledger gate remains unchanged and passes.

The 28 reassigned sources have not vanished. The 47 sources currently account for 187 execution duties (130 DONE,57 TODO). `current-lineage.json` records every original corrected ID and its current owner/evidence. Exactly four IDs were explicitly retired by `4efd79a1a23c411d61c95637aa4d865680810fe8`: duplicate04.013 generator duties on source-list entries. The retained, byte-pinned successor review explains why30.012/30.013 own the missing PDB implementation and34.013 owns verification. The checker rejects any unexplained missing ID or missing successor owner.

Two auxiliary rows use different scopes:001008 still records the historical file-wide aggregate (including the retired duplicate04.013), while001009 now records its exact hunk duties. Both facts are disclosed in `receipt.json`; neither is silently rewritten. The old auxiliary-union assertion is retained in the historical test. Current executable ownership is checked against authoritative hunk/work ledgers, rather than treating an obsolete descriptive aggregate as a missing implementation.

`current-build.json` is an unchanged **historical** report. `receipt.json` lists its current source drift. No fresh compile, native runtime, sanitizer, UI or platform acceptance is claimed by this repair. No screenshot is appropriate for this non-UI checker change.

Run:

```
python3 -B tools/check_cpp_registration.py
python3 -B migration/tests/cpp-registration/recovery/test_recovery.py
python3 -B tools/check_tasks.py
```

The 12 tests include nine rejection controls (missing registration/source/owner, invalid DONE/evidence, changed payload, unexpected assignment, damaged archive, wrong historical commit) and three positive controls, including legitimate successor completion and exact original checker execution. Publication remains separately permission-gated; these artifacts are local only.
