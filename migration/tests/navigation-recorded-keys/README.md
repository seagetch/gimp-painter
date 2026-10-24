# Original25.011 — every recorded key-transition angle

Original acceptance **記録した全キー遷移の期待角に一致** and dependency25.010
are unchanged. All198 original work IDs, actions, criteria, full legacy hunks
and other whole-hunk owner IDs remain pinned in `audit.json` and `pinned-hunks/`.
No new production fix or test-only child is introduced.

## Exact recorded scope

The unchanged retained `legacy-rotation-operations/gui-captures/rotation.tsv`
contains370 rows/46 scenes. Every one of its66 key events occurs in22 stationary
sequences. All22 sequences are replayed, including their surrounding198 press,
motion, key and release events, on both paintbrush and move tools. This produces
44 scene executions,396 event executions and132 key-event executions per
configuration, in four registered tests. Other recorded button/quadrant rows
contain no key transitions; their separate prior coverage is not relabeled.

Literal recorded angles remain the expected values. The unchanged extracted old
rotation functions additionally verify those retained motion values. Modern
mirrored angles have the opposite sign because `H R(a) = R(-a) H`; comparisons
explicitly normalize that coordinate-convention conversion, not rewrite the old
trace or substitute a modern calculation for its oracle. Center-relative integer
pointer coordinates are rebased from the old1015x674 canvas to the new portrait
viewport, preserving the old integer-center bearing. This is not an old-canvas
or old-application rerun.

All recorded Ctrl presses, Shift releases and Ctrl releases keep the previous
angle until the next motion, exactly as recorded. The stationary half-step and
near-half-step values, positive/negative initial angles, horizontal reflection,
and releases are retained. State/statusbar agreement is checked after genuine
GTK idle processing. Native angles use1e-9 comparison tolerance; the old snap
thresholds and expected literal angles are not changed. No physical keyboard,
mouse/tablet hardware or cross-platform parity is claimed.

## Results and UI

Four recorded-key cases plus the unchanged6 reset and50 original navigation
cases pass in each configuration: **60 normal +60 ordinary ASan/UBSan PASS**.
All scopes retain the original60-second invocation budget, maximum39.46 seconds.
New actual UI tests explicitly enable oncanvas and use authenticated
1080x1920x24 Xvfb. `before-snap.png` shows7.5° free rotation; `after-snap.png`
shows15° after Ctrl press **and the following motion**, not an immediate-keydown
snap. These are actual root-window captures with image/overlay/statusbar visible.
Historical detector settings are unchanged, not relabeled oncanvas-enabled.

The original198 source duties map to196 unchanged common-matrix source routes
plus two additional clauses: native Meson registration replaces old Autotools
rotate.c/.h registration, and old hunk000680 is mirrored Left/Right **KEY_PRESS**
translation after the all-key early route. Native dispatch translates a copy,
preserves original event fields and leaves KEY_RELEASE unchanged; the existing
four mirrored-key cases retain96 sends/64 press/32 release callbacks. Original
other-owner obligations are not stolen. Predecessor0f71 common-matrix andc8ff
reset evidence is replayed immutably, not claimed freshly reexecuted in full.

## Source and execution boundaries

All101 scoped production/test/oracle/runtime-asset inputs are sealed before
build, including both retained TSV assets and the exact committedd2791f9
projection hot-cache dependency. The old trace bytes are unchanged and checked
by literal SHA256 before native parsing. The earlier trace's teardown failure
and historical capture limitations remain unchanged; old runtime is USER-SKIPPED.

The private source remains ce40 plus scoped committed navigation/display work,
the committed projection correction and these tests, not the entire evolving
checkout. All eight held production paths remain committedce40 baseline bytes
(or absent for the new Bucket header); no held candidate/test was imported or
executed. Ordinary Meson ASan/UBSan, assertions enabled, O2/no debug symbols;
LSan disabled and dependencies uninstrumented. No whole-port completion claim.
Allowlisted receipts omit raw environment-containing Meson JSON.

Run `check-recorded-keys.py`, `test-recorded-key-evidence.py`, and
`replay-common-matrix-and-reset.py`. Negative controls reject missing/duplicated
recorded events, altered literal/native angles, omitted runtime assets/oracles,
changed original duties/owners, and unsupported result/scope claims.

`audit.json` is the pre-execution TODO snapshot (`RUNTIME_ACCEPTANCE_PENDING`),
retained as original-assignment evidence; the final receipts and manifest above
record the completed scoped acceptance, rather than rewriting that snapshot.
