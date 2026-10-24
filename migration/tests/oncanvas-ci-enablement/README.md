# Test-only oncanvas enablement, portrait opt-in

Four test files, three Scene setups. The Smudge UI, Fill/Save continuation and
MyPaint tool fixtures now always set/read back `painter-canvas-ui=TRUE` and
set/assert their associated display shell's visible state. No product defaults
or production code change. All original assertion lines remain in order.

`GIMP_TEST_REQUIRE_PORTRAIT=1` explicitly requests 1080×1920 root assertions
(and matching capture window resize). Without that flag ordinary UI CI may use
other monitor dimensions. Optional `GIMP_TEST_ONCANVAS_SCREENSHOTS` selects
only the artifact destination; it does not enable the UI. Captures use actual
root dimensions. The save point is after original causal pixel assertions,
not an inserted wait before pending-paint Undo.

Private proof uses unchanged production 55bfa5f1f72f33fb5a2f10b1f7a319bd773b0e94:
three privately compiled fixture TUs plus unchanged geometry executable.
- **74 portrait cases PASS:** Smudge 26, Save 13, MyPaint 27, geometry 8.
- **Three 1280×800 cases PASS without the flag:** pending Smudge Undo,
  Save/export pixels, pressure/Undo/Redo. Each renders actual oncanvas UI.
- Same landscape server with the portrait flag causally rejects 1280≠1080.
- 19,165 committed source files and 1,329 link inputs unchanged; no shared
  source/build writes. Eight actual captures: three portrait roots, three
  landscape controls, two native 1040×1800 geometry windows on portrait root.

`run.py` in the archive configures the portrait flag and uses the existing
portrait runner. Its private landscape runner differs only by the resolution
literal and diagnostic. `run.sh` sets TMPDIR after the environment setup.
The earlier fixed-monitor private supplement remains immutable and is not
represented as a generic CI change. This successor admits the four current
fixture source hashes; it does not repin earlier native receipts or claim new
sanitizer coverage, physical-pen validation, all 44 fixtures rendered, or WBS
parent completion. Production-default FALSE behavior is untouched.

`verify.py [source-root]` checks 40 archive artifact hashes, source endpoints,
original-line preservation, optional current source hashes, gating, runtime
counts, negative result and PNG dimensions. `test_verifier.py`: three groups,
12 semantic/source negative controls. No staging or publication is required
by these scripts.
