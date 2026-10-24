# Explicit oncanvas supplement — exact 55bf production

Production: `55bfa5f1f72f33fb5a2f10b1f7a319bd773b0e94`.
This supplements, never repins, the historical exact-aggregate-55bf receipt.

Authenticated Xvfb root 1080×1920×24: **74 cases PASS** (Smudge UI26,
Save continuation13, MyPaint tool27, geometry tool8). Three fixture translation
units were privately rebuilt/relinked against the unchanged exact55bf inputs;
four test source files changed because Save includes the Fill scene. Geometry's
existing executable and documented `GIMP_TEST_CONFLICT_SCREENSHOTS` branch were
used unchanged. All 19,165 committed source inputs and all pinned link inputs
remained unchanged. This is normal native testing, not a new sanitizer run.

The private overlays explicitly set/read back `painter-canvas-ui=TRUE`, set and
assert the associated shell's visible state, and check the portrait root.
All original assertion lines remain in order. The 300ms rendering wait starts
only after the causal pending-paint Undo result assertion. Fixed monitor checks
are private evidence overlays, **not general CI requirements**. No product,
held provider, or held geometry diagnostic was changed/executed.

## Real captures

- `screenshots/smudge-pending-undo-result-oncanvas.png`: API-driven pending
  Smudge paint followed by successful Undo; original initial pixels restored.
  Captured before the original Redo assertion, which also passes.
- `screenshots/save-completed-oncanvas.png`: API-driven Save/continuation
  regression after the saved pixels are verified; red/blue marks and native
  saved-file status are rendered, with visible oncanvas controls.
- `screenshots/mypaint-pressure-undo-redo-oncanvas.png`: capture after original
  pressure/Undo/Redo/hover pixel assertions; visible oncanvas controls. Small
  fixture marks are not offered as a visual substitute for pixel assertions.
- `geometry-screenshots/{01-conflict-preserved,02-next-gesture-recovered}.png`:
  existing conflict/recovery captures, native1040×1800 windows on the portrait
  root. The other three images are actual1080×1920 root captures.

No resizing, compositing, physical-pen, or whole44-target enablement claim.
The exhaustive44-target matrix in the archive describes **pre-supplement**
fixture coverage: backend-only tests remain backend-only, standalone Options
editors do not become image-shell UI proofs, and conditional source presence
is not execution coverage. This replay addresses the three genuine tool/display
setting gaps and exercises geometry's previously unset screenshot branch.

## Preserved setup diagnostics

The first private run failed before any TAP case because its temporary build
root lacked generated `menus/image-menu.ui`. A read-only symlink to the existing
exact build menus fixed setup; its content hash is retained. A retry input-set
assertion then noticed the three newly existing private output executables
while enumerating link arguments; every original linked-input hash was still
identical. The runtime-only retry checks the original sealed input set, without
recompilation or weakening its hashes. Both failures are retained separately
from the final74-case results.

`evidence.tar.gz` contains source endpoints/patch, commands, source/link seals,
full TAP results, binary hashes, setup diagnostics,44-row matrix, and five PNGs.
`verify.py` checks its immutable hash,42 individual artifact hashes, original
assertion preservation, counts, portrait dimensions and stated scope.
`test_verifier.py`: two groups, nine semantic negative controls.
No WBS parent completion or publication is implied.
