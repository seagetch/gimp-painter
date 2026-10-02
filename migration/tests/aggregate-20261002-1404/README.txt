Frozen Linux aggregate verification, 2026-10-02

Result: NOT PASSED. Meson completed all 108 registered targets: 104 OK,
4 FAIL, 0 skipped, 0 timeout, 0 expected failures. No exclusion or baseline
waiver was used. aggregate-gate.json has all_passed=false and complete
per-target registration/command/outcome coverage.

Frozen source commit: e109524cca95639d65ec8b0f4ba0f1fbd6e45a98
Frozen source tree: d90ca1a4a8040f94f0e55f791d0a76de2c36166c
Run: 14:06:22 to 14:15:33 UTC, under the actual Linux X11 desktop session.
UI_TEST=yes, GSETTINGS_BACKEND=memory, --no-rebuild, --num-processes 1,
--timeout-multiplier 3. No initial full build was repeated. The prior full
successful default build, executable check, and freshness explanation are
included as prior-*.log / prior-executable-check.json.

Failures:
- painter-perspective-ui: SIGTRAP in /perspective-ui/add-move-remove-undo;
  gimp_image_get_resolution asserts GIMP_IS_IMAGE(image).
- layer-presets-ui: SIGTRAP in /layer-presets-ui/preferences-registration;
  gimp_prop_memsize_entry_new rejects maximum > GIMP_MAX_MEMSIZE.
- painter-mypaint-editor: SIGTRAP in /painter-editor/07-registration with
  the same memsize assertion.
- save-and-export: existing upstream-baseline NoneType.equal failure for
  imported_file. Its Script-Fu subprocess also segfaults. The same Script-Fu
  crash is explicitly recorded in all 12 Meson-OK libgimp tests. Those tests'
  assertions passed, but the runs are not classified as clean passes. The
  upstream-baseline observations do not establish a root cause.

Suite coverage: painter 53/53 OK; app 38/42 OK; libgimp 12/12 Meson OK with
Script-Fu caveats; desktop 1/1 OK. No TAP SKIP/TODO subtests were observed.

Integrity: source commit, tree, test registry, all 8,742 tracked-file
fingerprints, all 414 pre-existing ELF fingerprints, and both main app
binaries were unchanged at completion. The broad ELF inventory gained one
verified /usr/bin/python3 symlink under the failed test's private temporary
profile. This is retained explicitly in the gate and after manifest; it is
not a changed built executable. Tests used private XDG/config/cache/data/tmp
roots plus their registered build-local profiles and source fixture profile.
No user profile was used. No tracked source was edited by this verification.

The full normal Filter target passed in 51.26 seconds, including the
8193x8193 case (14.67 seconds); that case reported heartbeat p95 2.429 ms,
p99 4.018 ms, maximum 72.410 ms. This does not establish an interactive
frame-time bound or resolve the separate earlier ASan/SIGKILL limitations.
No sanitizer suite was run in this aggregate.

The packaging candidate dry-run no-work guard is overstrict: the prior
freshness explanation begins with the always-dirty PHONY git-version
custom commands, whose outputs require real execution/restat. This alone
does not establish a stale binary, and it does not waive the test failures.
No packaging, upload, staging, commit, or push was performed here.

Evidence: meson.jsonl and meson.txt contain sanitized results/diagnostics;
registered-tests.json and aggregate-gate.json map all registered targets;
source-and-binary-*.json seal source and binaries. Repository sanitization
was applied to JSON/JSONL/text. Raw inherited environments are not included.
Raw originals remain separately local and are not part of this deliverable.
