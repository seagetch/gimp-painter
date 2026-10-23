# Original25.010 — registered rotation reset action

Acceptance remains **状態と UI 表示が一致**, dependency25.009. Two original
rotate.c/.h duties and all their other whole-hunk owners are retained in
`audit.json`, with full pinned legacy source and hunk payloads. Those old hunks
provide transform helpers, not a named reset action; no invented historical
reset runtime is claimed.

The existing production route is already connected:
`view-rotate-reset` enum action → absolute action callback → public
`gimp_display_shell_rotate_to(0)` → rotated signal, common transform, invalidation
and statusbar update. This checkpoint adds acceptance tests, **not a production
fix**. Tests activate the registered action via the UI manager; they do not call
the reset helper in place of the action.

## Results

- Six reset tests pass normal and ordinary ASan/UBSan builds. They cover48 actual
  UI scenes (six angles, public versus inherited fractional-pan/rotation entry,
  all H/V combinations), repeated action idempotence, and8 immediate gesture
  reentries (default and explicit custom binding, all H/V combinations).
-112 action activations per configuration. State is zero/non-exact, cached old
  origin invalidated, fractional offsets cleared, flip/zoom preserved, transforms
  and inverse correct, image landmark pixels aligned and drawable checksum
  unchanged. At zero the statusbar intentionally **hides** the rotation widget;
  retained hidden label text is not mistaken for a visible stale angle.
- Repeated reset leaves offsets unchanged and zero central-window pixel residual.
  Next gestures start without an intervening public rotate/scale/scroll repair.
  Custom binding identity remains unchanged. Reentry proves reset integration;
  it does not replace original25.011's full recorded modifier-angle acceptance.
- Original50 cases rerun on the final scoped sources:35 original +2 old rotation
  +10 old zoom +3 old Space. Total **56 normal +56 ordinary ASan/UBSan PASS**.
  Every invocation retains60 seconds, maximum36.50 seconds. Earlier196-duty
  common-matrix proof is replayed from immutable0f71f055a5, not relabeled as a
  fresh97-case run on this registration change.

All new UI fixtures enable oncanvas and use authenticated1080x1920x24 Xvfb.
`before-action.png` and `after-action.png` are actual root-window screenshots
immediately around registered reset activation, not gesture captions or mockups.
They show37°/rotated image+overlay before and0°/upright image+overlay with the
rotation status widget hidden afterward. Original historical detector settings
remain unchanged and are not relabeled oncanvas-enabled.

## Attempts and scope

`initial-fixture-api-failure.*` preserves the first test error: a modifiers getter
requires a nonnull description output pointer. The test suppliedNULL; correcting
that fixture argument is not a production defect. The intermediate four-case
PASS receipt/source is preserved separately from the final six-case proof.

`source-before-build.json` seals97 scoped production/test/oracle inputs before
compilation. `held-source-isolation.json` seals all eight held production paths:
seven private files match committedce40 baseline and the new Bucket header is
absent. No current held candidate or held test was imported/executed. Native
snapshot is ce40 plus scoped committed display/navigation changes and this test,
not the evolving whole checkout. Build/executable and projection hashes were
captured before the later independent graph-harness projection overlay; that
later work is not part of these receipts. No physical-device, old-application,
cross-platform or whole-port completion is claimed. ASan/UBSan use ordinary
Meson instrumentation with assertions enabled, O2/no debug symbols; dependency
libraries are uninstrumented and leak detection disabled.

`check-reset.py` validates source/artifact seals, exact duty/owner preservation,
registered action usage, native case/scene combinations, UI results and build
scope. `test-reset-evidence.py` rejects corrupted evidence, including coherently
rehashing a wrong registered action. `replay-common-matrix-checkpoint.py` validates
the immutable predecessor proof. Raw environment-containing Meson JSON is not
packaged; receipts include only allowlisted fields.

## Retrospective runtime-asset dependency clarification

The97-input before-build seal is explicitly scoped to code/oracle inputs. The
original35-case fixture also reads two retained TSV assets at three call sites.
`retrospective-runtime-dependencies.json` and `check-runtime-dependencies.py`
separately verify their current bytes against the literal SHA256 values asserted
**inside the sealed native fixture before parsing**. Passed registered consumers
are required in both normal and sanitizer receipts. This is a retrospective
source/data dependency audit, not a fabricated before-build asset seal or a new
old-runtime run. Native source and runtime receipt bytes are unchanged. The old
GUI trace's historical teardown limitation remains intact; the zoom TSV is old
function output, not a GUI capture.
