# Pinned Red Eye scalar foundation — original16.022 progress

Normal and ASan/UBSan/float-cast-overflow each PASS234,881,024 scalar comparisons:
all16,777,216 RGB triples ×seven threshold classes ×RGB/RGBA. The exact old C
inner loop is compared with independent integer-rational equations, including
critical predicate transitions and mathematically safe signed32 endpoints.
Unsafe old overflow inputs are rejected before invocation. Padded rows, alpha,
zero extents and two causal arithmetic mutants are checked. Initial fixture-only
-Werror indentation rejection is retained; no production source changed.

This executes extracted scalar C, NOT a historical GIMP/PDB process, native
Filter route, owner merge, raster spill, UI, or high-precision extension. LSan
is off. Requirements and snapshot hashes retain all108 inventory rows/93 missing
route labels, with explicit boundary between declared INT32 API and defined
old arithmetic. Original16/17 dependencies remain open.

Reproduce: `python3 migration/tests/red-eye-foundation/run.py --report /tmp/red-eye-results.json`.
Read-only receipt check: `python3 migration/tests/red-eye-foundation/validate.py`.
