# Phase C registered FilterLayer editor acceptance

`acceptance.json` records the bounded `30.001/parameter-schema-editor` result.
`raw-evidence.tar.gz` is deterministic (`mtime=0`) and contains selected native
logs, receipts, source/binary hashes, replay scripts and the actual minimum-header
compile receipt. It excludes profiles, environment dumps and build outputs.

Use the repository's normal configured GIMP3.0.9 build and native GTK display.
In this accepted workspace, load `gimp-build-restoration/env.sh` then
`tools/linux-debian13-env.sh`. All build/native/sanitizer steps use
`/workspace/shared/gimp-painter-build.lock`; a terminal launched through the real
cloud desktop supplies the native display. Merely exporting `DISPLAY=:0` in the
shell executor is not equivalent.

Build `app/tests/painter-layer-ui`, `app/tests/gimp-filter-layer`,
`app/tests/painter-xcf-roundtrip`, `app/gimp-painter-filter-worker` and
`app/gimp-3.0` with Ninja. Run the archive's `replay/run-native.py` in a native
terminal for the full normal suite. Build the private sanitizer executable with:

```
flock /workspace/shared/gimp-painter-build.lock python3 -B \
  migration/tests/build_painter_ui_sanitizers.py build-installed-filter \
  --report /path/to/evidence/ui-sanitizer-build.json
```

Then run `replay/run-native.py --mode sanitized` in the same native environment.
The script merges the selected Meson test's explicit environment and retains
raw output, exact TAP selection, exit code and binary/source hashes. The archive
contains the exact accepted wrappers and regression selections. Paths in these
workspace recipes must be adapted together for another checkout.

The sanitizer scope is15 instrumented units plus51 production C++ units rebuilt
with RTTI only. GTK, other dependencies and remaining GIMP code are ordinary;
LeakSanitizer is disabled. `compile-minimum.py` compiles six changed production
units using the prior immutable minimum build's actual GLib2.70 include paths.
It writes only its new outputs; it is not a new full minimum build/runtime pass.

The first normal run preceded the last shared-tail admission/text adjustment.
The final full53-group normal and sanitizer runs use the final implementation
and current worker. The31 core and14 XCF regressions cover unchanged final core
sources; no core change occurred after those runs. The actual-app walkthrough
preceded only that final dialog adjustment. Its source/binary identity and exact
limits are recorded; it is not silently attributed to a later binary.

Failed initial compilation and startup attempts are retained separately. The
first app launcher lacked in-build menu/sysconf paths and exited through GIMP's
fatal handler with exit0, which was rejected as a successful launch. The corrected
launch was verified through actual windows, available route pages, invalid-input
handling, completed cache, no-op and one-field Undo/Redo. That run still recorded
missing uninstalled assets/localization, a temporary invalid gimprc property,
and GTK/GDK accessibility/popup diagnostics. The final replay wrapper removes
the invalid property. Existing AT-SPI work remains open.

The old2626-file fixture seal is unchanged; no fixture is regenerated and the
full old pixel corpus is not rerun. Algorithms/precision and execution binding
are unchanged, and four real old-context routes plus progress/cancel regressions
passed. Comprehensive Phase D/E, other platforms, OOM injection across the whole
application and global latency/RSS gates remain separate.
