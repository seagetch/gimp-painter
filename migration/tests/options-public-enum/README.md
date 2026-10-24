# Public MyPaint enum successor

This package verifies the original public library boundary, not full Options
parity. See `migration/contracts/mypaint-public-enum.md` for the preserved
semantics and remaining obligations.

`preliminary/` is a bounded private relink, not real build acceptance.
`integration/` records the real Meson shared-library and application build,
standalone C11 consumer, both C++14 registration orders, all six previously
committed constant mode cases, native Options regression, and header-route
checks. The shared library defines the getter; libgimp references it through its
registration list. The existing generator reproduces the added registrar,
identical to the pinned original, without unrelated enum churn.

`sanitizer/` instruments seven selected units: generated enum C, Options,
binding store, new C/C++ fixtures and old mode C/C++ fixtures. Other dependencies
remain native. No whole-app, vptr/RTTI, leak detection, Windows or GIR guarantee
is claimed. Portrait authenticated Xvfb1080x1920 and enabled oncanvas preference
are model-only checks, not a new screenshot/UI-interaction claim.

Inputs are explicit overlays on the exact20b predecessor plus committed mode
and Filter rotate/matrix work. All eight held providers and four Clone paths
remain exact20b; prior overwritten files and complete source hashes are retained.
No dirty held source is used. Prior `options-mode-entry` evidence remains frozen;
its strict current-file hash check naturally detects this documented successor.

The first real integration passed all 33 normal cases and 88 header probes,
but its final source guard rejected two required generator outputs: the PDB
enum map and libgimp type-name list. This is preserved as an unsealed first
pass. The final successor includes both exact generated outputs, independently
reproduced twice, and repeats the integration with a separate compile/link no-op check. Existing always-run
git-version probes (archive source has no .git) and icon-fixture validation still
execute; the first literal no-work check rejected for that reason. Relevant
object/shared-library/test-binary hashes and all source bytes remain unchanged.

One generator setup mistake executed the existing clean Meson wrapper before
candidate synchronization. It changed timestamps but the clean generated C
remained byte-identical to20b. The stamp's previous hash was not captured and is
unknown. Exact command/affected paths are recorded; this failed setup is not
acceptance evidence. Successful private generation used explicit private output
paths twice, followed by the real integration build.

Publication remains paused pending approval. These files are local evidence.
