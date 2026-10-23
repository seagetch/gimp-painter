# 09.004 actual buffer-write evidence

Four registered targets pass **30 TAP cases**: native precision8, coordinate7,
Surface12, ROI transfers3. The Xvfb runner verifies **1080×1920×24**, authenticated
local display. These are storage/producer tests, not new UI/screenshot features.

`standalone.json` records independent normal and ASan/UBSan/float-cast-overflow
runs of the **actual current Surface source**, seven groups each, including
4,608 native-format/partial-write operations and coalesced GEGL dirty signals.
Libraries are uninstrumented; LeakSanitizer is disabled; no full-app sanitizer
claim. `run-standalone.py` reproduces this with 60-second execution budgets.
`run-native.sh` records the shared-build lock and exact native commands.
No production code was changed: existing write behavior satisfied these tests.

`excluded-fixture-attempts/regular-g-signal-connect.log` is an **invalid initial
test fixture**, not a production regression. GEGL's installed header explicitly
requires `gegl_buffer_signal_connect` to enable changed tracking; ordinary
`g_signal_connect` receives no changes. The fixture was corrected accordingly.
No test-only failure is claimed as a fixed application defect.

The dirty result is a conservative clipped ROI/bounding box, not a promise that
every enclosed pixel changed. Independent pixel-membership guards establish
that parent halo and all disallowed pixels remain unchanged. Exact dirty box
assertions apply to the supplied full-coverage scenarios; sparse gaps are
intentionally retained inside the accumulated union.

`report.json` retains twelve relevant source hashes equal to the native build
source tree. Meson environment maps are removed entirely. Native stderr retains
normal ignored .dummy plugin diagnostics. Run `validate.py --check-source` for
current source, or omit the switch to validate archived evidence after later
changes. Whole-hunk duties are tracked separately in
`../../contracts/drawable-buffer-write.json`; no inventory duty was marked done.
